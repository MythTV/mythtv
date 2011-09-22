#include <unistd.h>

#include <iostream>
#include <algorithm>
#include <list>
using namespace std;

#ifdef __linux__
#  include <sys/vfs.h>
#else // if !__linux__
#  include <sys/param.h>
#  ifndef USING_MINGW
#    include <sys/mount.h>
#  endif // USING_MINGW
#endif // !__linux__

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include <QStringList>
#include <QDateTime>
#include <QString>
#include <QRegExp>
#include <QMutex>
#include <QFile>
#include <QMap>

#include "scheduler.h"
#include "encoderlink.h"
#include "mainserver.h"
#include "remoteutil.h"
#include "backendutil.h"
#include "util.h"
#include "exitcodes.h"
#include "mythcontext.h"
#include "mythdb.h"
#include "compat.h"
#include "storagegroup.h"
#include "recordinginfo.h"
#include "recordingrule.h"
#include "scheduledrecording.h"
#include "cardutil.h"
#include "mythdb.h"
#include "mythsystemevent.h"
#include "mythlogging.h"

#define LOC QString("Scheduler: ")
#define LOC_WARN QString("Scheduler, Warning: ")
#define LOC_ERR QString("Scheduler, Error: ")

bool debugConflicts = false;

Scheduler::Scheduler(bool runthread, QMap<int, EncoderLink *> *tvList,
                     QString tmptable, Scheduler *master_sched) :
    MThread("Scheduler"),
    recordTable(tmptable),
    priorityTable("powerpriority"),
    schedLock(),
    reclist_changed(false),
    specsched(master_sched),
    schedMoveHigher(false),
    schedulingEnabled(true),
    m_tvList(tvList),
    m_expirer(NULL),
    doRun(runthread),
    m_mainServer(NULL),
    resetIdleTime(false),
    m_isShuttingDown(false),
    error(0),
    livetvTime(QDateTime()),
    livetvpriority(0),
    prefinputpri(0)
{
    char *debug = getenv("DEBUG_CONFLICTS");
    debugConflicts = (debug != NULL);

    if (master_sched)
        master_sched->GetAllPending(reclist);

    if (!doRun)
        dbConn = MSqlQuery::DDCon();

    if (tmptable == "powerpriority_tmp")
    {
        priorityTable = tmptable;
        recordTable = "record";
    }

    if (!VerifyCards())
    {
        error = true;
        return;
    }

    fsInfoCacheFillTime = QDateTime::currentDateTime().addSecs(-1000);

    if (doRun)
    {
        {
            QMutexLocker locker(&schedLock);
            start(QThread::LowPriority);
            while (doRun && !isRunning())
                reschedWait.wait(&schedLock);
        }
        WakeUpSlaves();
    }
}

Scheduler::~Scheduler()
{
    QMutexLocker locker(&schedLock);
    if (doRun)
    {
        doRun = false;
        reschedWait.wakeAll();
        locker.unlock();
        wait();
        locker.relock();
    }
    else
        MSqlQuery::CloseDDCon();

    while (!reclist.empty())
    {
        delete reclist.back();
        reclist.pop_back();
    }

    while (!worklist.empty())
    {
        delete worklist.back();
        worklist.pop_back();
    }

    locker.unlock();
    wait();
}

void Scheduler::Stop(void)
{
    QMutexLocker locker(&schedLock);
    doRun = false;
    reschedWait.wakeAll();
}

void Scheduler::SetMainServer(MainServer *ms)
{
    m_mainServer = ms;
}

void Scheduler::ResetIdleTime(void)
{
    resetIdleTime_lock.lock();
    resetIdleTime = true;
    resetIdleTime_lock.unlock();
}

bool Scheduler::VerifyCards(void)
{
    MSqlQuery query(MSqlQuery::InitCon());
    if (!query.exec("SELECT count(*) FROM capturecard") || !query.next())
    {
        MythDB::DBError("verifyCards() -- main query 1", query);
        error = GENERIC_EXIT_DB_ERROR;
        return false;
    }

    uint numcards = query.value(0).toUInt();
    if (!numcards)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
                "No capture cards are defined in the database.\n\t\t\t"
                "Perhaps you should re-read the installation instructions?");
        error = GENERIC_EXIT_SETUP_ERROR;
        return false;
    }

    query.prepare("SELECT sourceid,name FROM videosource ORDER BY sourceid;");

    if (!query.exec())
    {
        MythDB::DBError("verifyCards() -- main query 2", query);
        error = GENERIC_EXIT_DB_ERROR;
        return false;
    }

    uint numsources = 0;
    MSqlQuery subquery(MSqlQuery::InitCon());
    while (query.next())
    {
        subquery.prepare(
            "SELECT cardinputid "
            "FROM cardinput "
            "WHERE sourceid = :SOURCEID "
            "ORDER BY cardinputid;");
        subquery.bindValue(":SOURCEID", query.value(0).toUInt());

        if (!subquery.exec())
        {
            MythDB::DBError("verifyCards() -- sub query", subquery);
        }
        else if (!subquery.next())
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC +
                QString("Listings source '%1' is defined, "
                        "but is not attached to a card input.")
                    .arg(query.value(1).toString()));
        }
        else
        {
            numsources++;
        }
    }

    if (!numsources)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "No channel sources defined in the database");
        error = GENERIC_EXIT_SETUP_ERROR;
        return false;
    }

    return true;
}

static inline bool Recording(const RecordingInfo *p)
{
    return (p->GetRecordingStatus() == rsRecording ||
            p->GetRecordingStatus() == rsTuning ||
            p->GetRecordingStatus() == rsWillRecord);
}

static bool comp_overlap(RecordingInfo *a, RecordingInfo *b)
{
    if (a->GetScheduledStartTime() != b->GetScheduledStartTime())
        return a->GetScheduledStartTime() < b->GetScheduledStartTime();
    if (a->GetScheduledEndTime() != b->GetScheduledEndTime())
        return a->GetScheduledEndTime() < b->GetScheduledEndTime();

    // Note: the PruneOverlaps logic depends on the following
    if (a->GetTitle() != b->GetTitle())
        return a->GetTitle() < b->GetTitle();
    if (a->GetChanID() != b->GetChanID())
        return a->GetChanID() < b->GetChanID();
    if (a->GetInputID() != b->GetInputID())
        return a->GetInputID() < b->GetInputID();

    // In cases where two recording rules match the same showing, one
    // of them needs to take precedence.  Penalize any entry that
    // won't record except for those from kDontRecord rules.  This
    // will force them to yield to a rule that might record.
    // Otherwise, more specific record type beats less specific.
    int apri = RecTypePriority(a->GetRecordingRuleType());
    if (a->GetRecordingStatus() != rsUnknown &&
        a->GetRecordingStatus() != rsDontRecord)
    {
        apri += 100;
    }
    int bpri = RecTypePriority(b->GetRecordingRuleType());
    if (b->GetRecordingStatus() != rsUnknown &&
        b->GetRecordingStatus() != rsDontRecord)
    {
        bpri += 100;
    }
    if (apri != bpri)
        return apri < bpri;

    if (a->GetFindID() != b->GetFindID())
        return a->GetFindID() > b->GetFindID();
    return a->GetRecordingRuleID() < b->GetRecordingRuleID();
}

static bool comp_redundant(RecordingInfo *a, RecordingInfo *b)
{
    if (a->GetScheduledStartTime() != b->GetScheduledStartTime())
        return a->GetScheduledStartTime() < b->GetScheduledStartTime();
    if (a->GetScheduledEndTime() != b->GetScheduledEndTime())
        return a->GetScheduledEndTime() < b->GetScheduledEndTime();

    // Note: the PruneRedundants logic depends on the following
    if (a->GetTitle() != b->GetTitle())
        return a->GetTitle() < b->GetTitle();
    if (a->GetRecordingRuleID() != b->GetRecordingRuleID())
        return a->GetRecordingRuleID() < b->GetRecordingRuleID();
    if (a->GetChannelSchedulingID() != b->GetChannelSchedulingID())
        return a->GetChannelSchedulingID() < b->GetChannelSchedulingID();
    return a->GetRecordingStatus() < b->GetRecordingStatus();
}

static bool comp_recstart(RecordingInfo *a, RecordingInfo *b)
{
    if (a->GetRecordingStartTime() != b->GetRecordingStartTime())
        return a->GetRecordingStartTime() < b->GetRecordingStartTime();
    if (a->GetRecordingEndTime() != b->GetRecordingEndTime())
        return a->GetRecordingEndTime() < b->GetRecordingEndTime();
    if (a->GetChannelSchedulingID() != b->GetChannelSchedulingID())
        return a->GetChannelSchedulingID() < b->GetChannelSchedulingID();
    if (a->GetRecordingStatus() != b->GetRecordingStatus())
        return a->GetRecordingStatus() < b->GetRecordingStatus();
    if (a->GetChanNum() != b->GetChanNum())
        return a->GetChanNum() < b->GetChanNum();
    return a->GetChanID() < b->GetChanID();
}

static bool comp_priority(RecordingInfo *a, RecordingInfo *b)
{
    int arec = (a->GetRecordingStatus() != rsRecording &&
                a->GetRecordingStatus() != rsTuning);
    int brec = (b->GetRecordingStatus() != rsRecording &&
                b->GetRecordingStatus() != rsTuning);

    if (arec != brec)
        return arec < brec;

    if (a->GetRecordingPriority() != b->GetRecordingPriority())
        return a->GetRecordingPriority() > b->GetRecordingPriority();

    QDateTime pasttime = QDateTime::currentDateTime().addSecs(-30);
    int apast = (a->GetRecordingStartTime() < pasttime &&
                 !a->IsReactivated());
    int bpast = (b->GetRecordingStartTime() < pasttime &&
                 !b->IsReactivated());

    if (apast != bpast)
        return apast < bpast;

    int apri = RecTypePriority(a->GetRecordingRuleType());
    int bpri = RecTypePriority(b->GetRecordingRuleType());

    if (apri != bpri)
        return apri < bpri;

    if (a->GetRecordingStartTime() != b->GetRecordingStartTime())
    {
        if (apast)
            return a->GetRecordingStartTime() > b->GetRecordingStartTime();
        else
            return a->GetRecordingStartTime() < b->GetRecordingStartTime();
    }

    if (a->GetInputID() != b->GetInputID())
        return a->GetInputID() < b->GetInputID();

    return a->GetRecordingRuleID() < b->GetRecordingRuleID();
}

bool Scheduler::FillRecordList(void)
{
    schedMoveHigher = (bool)gCoreContext->GetNumSetting("SchedMoveHigher");
    schedTime = QDateTime::currentDateTime();

    LOG(VB_SCHEDULE, LOG_INFO, "BuildWorkList...");
    BuildWorkList();

    schedLock.unlock();

    LOG(VB_SCHEDULE, LOG_INFO, "AddNewRecords...");
    AddNewRecords();
    LOG(VB_SCHEDULE, LOG_INFO, "AddNotListed...");
    AddNotListed();

    LOG(VB_SCHEDULE, LOG_INFO, "Sort by time...");
    SORT_RECLIST(worklist, comp_overlap);
    LOG(VB_SCHEDULE, LOG_INFO, "PruneOverlaps...");
    PruneOverlaps();

    LOG(VB_SCHEDULE, LOG_INFO, "Sort by priority...");
    SORT_RECLIST(worklist, comp_priority);
    LOG(VB_SCHEDULE, LOG_INFO, "BuildListMaps...");
    BuildListMaps();
    LOG(VB_SCHEDULE, LOG_INFO, "SchedNewRecords...");
    SchedNewRecords();
    LOG(VB_SCHEDULE, LOG_INFO, "SchedPreserveLiveTV...");
    SchedPreserveLiveTV();
    LOG(VB_SCHEDULE, LOG_INFO, "ClearListMaps...");
    ClearListMaps();

    schedLock.lock();

    LOG(VB_SCHEDULE, LOG_INFO, "Sort by time...");
    SORT_RECLIST(worklist, comp_redundant);
    LOG(VB_SCHEDULE, LOG_INFO, "PruneRedundants...");
    PruneRedundants();

    LOG(VB_SCHEDULE, LOG_INFO, "Sort by time...");
    SORT_RECLIST(worklist, comp_recstart);
    LOG(VB_SCHEDULE, LOG_INFO, "ClearWorkList...");
    bool res = ClearWorkList();

    return res;
}

/** \fn Scheduler::FillRecordListFromDB(int)
 *  \param recordid Record ID of recording that has changed,
 *                  or -1 if anything might have been changed.
 */
void Scheduler::FillRecordListFromDB(int recordid)
{
    struct timeval fillstart, fillend;
    float matchTime, placeTime;

    MSqlQuery query(dbConn);
    QString thequery;
    QString where = "";

    // This will cause our temp copy of recordmatch to be empty
    if (recordid == -1)
        where = "WHERE recordid IS NULL ";

    thequery = QString("CREATE TEMPORARY TABLE recordmatch ") +
                           "SELECT * FROM recordmatch " + where + "; ";

    query.prepare(thequery);
    recordmatchLock.lock();
    bool ok = query.exec();
    recordmatchLock.unlock();
    if (!ok)
    {
        MythDB::DBError("FillRecordListFromDB", query);
        return;
    }

    thequery = "ALTER TABLE recordmatch ADD INDEX (recordid);";
    query.prepare(thequery);
    if (!query.exec())
    {
        MythDB::DBError("FillRecordListFromDB", query);
        return;
    }

    QMutexLocker locker(&schedLock);

    gettimeofday(&fillstart, NULL);
    UpdateMatches(recordid);
    gettimeofday(&fillend, NULL);
    matchTime = ((fillend.tv_sec - fillstart.tv_sec ) * 1000000 +
                 (fillend.tv_usec - fillstart.tv_usec)) / 1000000.0;

    gettimeofday(&fillstart, NULL);
    FillRecordList();
    gettimeofday(&fillend, NULL);
    placeTime = ((fillend.tv_sec - fillstart.tv_sec ) * 1000000 +
                 (fillend.tv_usec - fillstart.tv_usec)) / 1000000.0;

    MSqlQuery queryDrop(dbConn);
    queryDrop.prepare("DROP TABLE recordmatch;");
    if (!queryDrop.exec())
    {
        MythDB::DBError("FillRecordListFromDB", queryDrop);
        return;
    }

    QString msg;
    msg.sprintf("Speculative scheduled %d items in "
                "%.1f = %.2f match + %.2f place", (int)reclist.size(),
                matchTime + placeTime, matchTime, placeTime);
    LOG(VB_GENERAL, LOG_INFO, msg);
}

void Scheduler::FillRecordListFromMaster(void)
{
    RecordingList schedList(false);
    bool dummy;
    LoadFromScheduler(schedList, dummy);

    QMutexLocker lockit(&schedLock);

    RecordingList::iterator it = schedList.begin();
    for (; it != schedList.end(); ++it)
        reclist.push_back(*it);
}

void Scheduler::PrintList(RecList &list, bool onlyFutureRecordings)
{
    if (!VERBOSE_LEVEL_CHECK(VB_SCHEDULE, LOG_INFO))
        return;

    QDateTime now = QDateTime::currentDateTime();

    LOG(VB_SCHEDULE, LOG_INFO, "--- print list start ---");
    LOG(VB_SCHEDULE, LOG_INFO, "Title - Subtitle                    Ch Station "
                               "Day Start  End   S C I  T N Pri");

    RecIter i = list.begin();
    for ( ; i != list.end(); ++i)
    {
        RecordingInfo *first = (*i);

        if (onlyFutureRecordings &&
            ((first->GetRecordingEndTime() < now &&
              first->GetScheduledEndTime() < now) ||
             (first->GetRecordingStartTime() < now && !Recording(first))))
            continue;

        PrintRec(first);
    }

    LOG(VB_SCHEDULE, LOG_INFO, "---  print list end  ---");
}

void Scheduler::PrintRec(const RecordingInfo *p, const char *prefix)
{
    if (!VERBOSE_LEVEL_CHECK(VB_SCHEDULE, LOG_INFO))
        return;

    QString outstr;

    if (prefix)
        outstr = QString(prefix);

    QString episode = p->toString(ProgramInfo::kTitleSubtitle, " - ", "");
    episode = episode.leftJustified(34 - (prefix ? strlen(prefix) : 0),
                                    ' ', true);

    outstr += QString("%1 %2 %3 %4-%5  %6 %7 %8  ")
        .arg(episode)
        .arg(p->GetChanNum().rightJustified(4, ' '))
        .arg(p->GetChannelSchedulingID().leftJustified(7, ' ', true))
        .arg(p->GetRecordingStartTime().toString("dd hh:mm"))
        .arg(p->GetRecordingEndTime().toString("hh:mm"))
        .arg(p->GetSourceID())
        .arg(p->GetCardID())
        .arg(p->GetInputID());
    outstr += QString("%1 %2 %3")
        .arg(toQChar(p->GetRecordingRuleType()))
        .arg(toString(p->GetRecordingStatus(), p->GetCardID()))
        .arg(p->GetRecordingPriority());
    if (p->GetRecordingPriority2())
        outstr += QString("/%1").arg(p->GetRecordingPriority2());

    LOG(VB_SCHEDULE, LOG_INFO, outstr);
}

void Scheduler::UpdateRecStatus(RecordingInfo *pginfo)
{
    QMutexLocker lockit(&schedLock);

    RecIter dreciter = reclist.begin();
    for (; dreciter != reclist.end(); ++dreciter)
    {
        RecordingInfo *p = *dreciter;
        if (p->IsSameProgramTimeslot(*pginfo))
        {
            if (p->GetRecordingStatus() != pginfo->GetRecordingStatus())
            {
                LOG(VB_GENERAL, LOG_INFO,
                    QString("Updating status for %1 on cardid %2 (%3 => %4)")
                        .arg(p->toString(ProgramInfo::kTitleSubtitle))
                        .arg(p->GetCardID())
                        .arg(toString(p->GetRecordingStatus(),
                                      p->GetRecordingRuleType()))
                        .arg(toString(pginfo->GetRecordingStatus(),
                                      p->GetRecordingRuleType())));
                bool resched =
                    ((p->GetRecordingStatus() != rsRecording &&
                      p->GetRecordingStatus() != rsTuning) ||
                     (pginfo->GetRecordingStatus() != rsRecording &&
                      pginfo->GetRecordingStatus() != rsTuning));
                p->SetRecordingStatus(pginfo->GetRecordingStatus());
                reclist_changed = true;
                p->AddHistory(false);
                if (resched)
                {
                    reschedQueue.enqueue(0);
                    reschedWait.wakeOne();
                }
                else
                {
                    MythEvent me("SCHEDULE_CHANGE");
                    gCoreContext->dispatch(me);
                }
            }
            return;
        }
    }
}

void Scheduler::UpdateRecStatus(uint cardid, uint chanid,
                                const QDateTime &startts,
                                RecStatusType recstatus,
                                const QDateTime &recendts)
{
    QMutexLocker lockit(&schedLock);

    RecIter dreciter = reclist.begin();
    for (; dreciter != reclist.end(); ++dreciter)
    {
        RecordingInfo *p = *dreciter;
        if (p->GetCardID() == cardid && p->GetChanID() == chanid &&
            p->GetScheduledStartTime() == startts)
        {
            p->SetRecordingEndTime(recendts);

            if (p->GetRecordingStatus() != recstatus)
            {
                LOG(VB_GENERAL, LOG_INFO,
                    QString("Updating status for %1 on cardid %2 (%3 => %4)")
                        .arg(p->toString(ProgramInfo::kTitleSubtitle))
                        .arg(p->GetCardID())
                        .arg(toString(p->GetRecordingStatus(),
                                      p->GetRecordingRuleType()))
                        .arg(toString(recstatus,
                                      p->GetRecordingRuleType())));
                bool resched =
                    ((p->GetRecordingStatus() != rsRecording &&
                      p->GetRecordingStatus() != rsTuning) ||
                     (recstatus != rsRecording &&
                      recstatus != rsTuning));
                p->SetRecordingStatus(recstatus);
                reclist_changed = true;
                p->AddHistory(false);
                if (resched)
                {
                    reschedQueue.enqueue(0);
                    reschedWait.wakeOne();
                }
                else
                {
                    MythEvent me("SCHEDULE_CHANGE");
                    gCoreContext->dispatch(me);
                }
            }
            return;
        }
    }
}

bool Scheduler::ChangeRecordingEnd(RecordingInfo *oldp, RecordingInfo *newp)
{
    QMutexLocker lockit(&schedLock);

    if (reclist_changed)
        return false;

    RecordingType oldrectype = oldp->GetRecordingRuleType();
    int oldrecordid = oldp->GetRecordingRuleID();
    QDateTime oldrecendts = oldp->GetRecordingEndTime();

    oldp->SetRecordingRuleType(newp->GetRecordingRuleType());
    oldp->SetRecordingRuleID(newp->GetRecordingRuleID());
    oldp->SetRecordingEndTime(newp->GetRecordingEndTime());

    if (specsched)
    {
        if (newp->GetRecordingEndTime() < QDateTime::currentDateTime())
        {
            oldp->SetRecordingStatus(rsRecorded);
            newp->SetRecordingStatus(rsRecorded);
            return false;
        }
        else
            return true;
    }

    EncoderLink *tv = (*m_tvList)[oldp->GetCardID()];
    RecStatusType rs = tv->StartRecording(oldp);
    if (rs != rsRecording)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Failed to change end time on card %1 to %2")
                .arg(oldp->GetCardID())
                .arg(newp->GetRecordingEndTime(ISODate)));
        oldp->SetRecordingRuleType(oldrectype);
        oldp->SetRecordingRuleID(oldrecordid);
        oldp->SetRecordingEndTime(oldrecendts);
    }
    else
    {
        RecIter i = reclist.begin();
        for (; i != reclist.end(); ++i)
        {
            RecordingInfo *recp = *i;
            if (recp->IsSameTimeslot(*oldp))
            {
                *recp = *oldp;
                break;
            }
        }
    }

    return rs == rsRecording;
}

void Scheduler::SlaveConnected(RecordingList &slavelist)
{
    QMutexLocker lockit(&schedLock);

    RecordingList::iterator it = slavelist.begin();
    for (; it != slavelist.end(); ++it)
    {
        RecordingInfo *sp = *it;
        bool found = false;

        RecIter ri = reclist.begin();
        for ( ; ri != reclist.end(); ++ri)
        {
            RecordingInfo *rp = *ri;

            if (sp->GetInputID() &&
                sp->GetScheduledStartTime() == rp->GetScheduledStartTime() &&
                sp->GetChannelSchedulingID() == rp->GetChannelSchedulingID() &&
                sp->GetTitle() == rp->GetTitle())
            {
                if (sp->GetCardID() == rp->GetCardID())
                {
                    found = true;
                    rp->SetRecordingStatus(sp->GetRecordingStatus());
                    reclist_changed = true;
                    rp->AddHistory(false);
                    LOG(VB_GENERAL, LOG_INFO,
                        QString("setting %1/%2/\"%3\" as %4")
                            .arg(sp->GetCardID())
                            .arg(sp->GetChannelSchedulingID())
                            .arg(sp->GetTitle())
                            .arg(toUIState(sp->GetRecordingStatus())));
                }
                else
                {
                    LOG(VB_GENERAL, LOG_NOTICE,
                        QString("%1/%2/\"%3\" is already recording on card %4")
                            .arg(sp->GetCardID())
                            .arg(sp->GetChannelSchedulingID())
                            .arg(sp->GetTitle())
                            .arg(rp->GetCardID()));
                }
            }
            else if (sp->GetCardID() == rp->GetCardID() &&
                     (rp->GetRecordingStatus() == rsRecording ||
                      rp->GetRecordingStatus() == rsTuning))
            {
                rp->SetRecordingStatus(rsAborted);
                reclist_changed = true;
                rp->AddHistory(false);
                LOG(VB_GENERAL, LOG_INFO, 
                    QString("setting %1/%2/\"%3\" as aborted")
                        .arg(rp->GetCardID())
                        .arg(rp->GetChannelSchedulingID())
                        .arg(rp->GetTitle()));
            }
        }

        if (sp->GetInputID() && !found)
        {
            reclist.push_back(new RecordingInfo(*sp));
            reclist_changed = true;
            sp->AddHistory(false);
            LOG(VB_GENERAL, LOG_INFO,
                QString("adding %1/%2/\"%3\" as recording")
                    .arg(sp->GetCardID())
                    .arg(sp->GetChannelSchedulingID())
                    .arg(sp->GetTitle()));
        }
    }
}

void Scheduler::SlaveDisconnected(uint cardid)
{
    QMutexLocker lockit(&schedLock);

    RecIter ri = reclist.begin();
    for ( ; ri != reclist.end(); ++ri)
    {
        RecordingInfo *rp = *ri;

        if (rp->GetCardID() == cardid &&
            (rp->GetRecordingStatus() == rsRecording ||
             rp->GetRecordingStatus() == rsTuning))
        {
            rp->SetRecordingStatus(rsAborted);
            reclist_changed = true;
            rp->AddHistory(false);
            LOG(VB_GENERAL, LOG_INFO, QString("setting %1/%2/\"%3\" as aborted")
                    .arg(rp->GetCardID()).arg(rp->GetChannelSchedulingID())
                    .arg(rp->GetTitle()));
        }
    }
}

void Scheduler::BuildWorkList(void)
{
    reclist_changed = false;

    RecIter i = reclist.begin();
    for (; i != reclist.end(); ++i)
    {
        RecordingInfo *p = *i;
        if (p->GetRecordingStatus() == rsRecording ||
            p->GetRecordingStatus() == rsTuning)
            worklist.push_back(new RecordingInfo(*p));
    }
}

bool Scheduler::ClearWorkList(void)
{
    RecordingInfo *p;

    if (reclist_changed)
    {
        while (!worklist.empty())
        {
            p = worklist.front();
            delete p;
            worklist.pop_front();
        }

        return false;
    }

    while (!reclist.empty())
    {
        p = reclist.front();
        delete p;
        reclist.pop_front();
    }

    while (!worklist.empty())
    {
        p = worklist.front();
        reclist.push_back(p);
        worklist.pop_front();
    }

    return true;
}

static void erase_nulls(RecList &reclist)
{
    RecIter it = reclist.begin();
    uint dst = 0;
    for (it = reclist.begin(); it != reclist.end(); ++it)
    {
        if (*it)
        {
            reclist[dst] = *it;
            dst++;
        }
    }
    reclist.resize(dst);
}

void Scheduler::PruneOverlaps(void)
{
    RecordingInfo *lastp = NULL;

    RecIter dreciter = worklist.begin();
    while (dreciter != worklist.end())
    {
        RecordingInfo *p = *dreciter;
        if (!lastp || lastp->GetRecordingRuleID() == p->GetRecordingRuleID() ||
            !lastp->IsSameTimeslot(*p))
        {
            lastp = p;
            ++dreciter;
        }
        else
        {
            delete p;
            *(dreciter++) = NULL;
        }
    }

    erase_nulls(worklist);
}

void Scheduler::BuildListMaps(void)
{
    RecIter i = worklist.begin();
    for ( ; i != worklist.end(); ++i)
    {
        RecordingInfo *p = *i;
        if (p->GetRecordingStatus() == rsRecording ||
            p->GetRecordingStatus() == rsTuning ||
            p->GetRecordingStatus() == rsWillRecord ||
            p->GetRecordingStatus() == rsUnknown)
        {
            cardlistmap[p->GetCardID()].push_back(p);
            titlelistmap[p->GetTitle()].push_back(p);
            recordidlistmap[p->GetRecordingRuleID()].push_back(p);
        }
    }
}

void Scheduler::ClearListMaps(void)
{
    cardlistmap.clear();
    titlelistmap.clear();
    recordidlistmap.clear();
    cache_is_same_program.clear();
}

bool Scheduler::IsSameProgram(
    const RecordingInfo *a, const RecordingInfo *b) const
{
    IsSameKey X(a,b);
    IsSameCacheType::const_iterator it = cache_is_same_program.find(X);
    if (it != cache_is_same_program.end())
        return *it;

    IsSameKey Y(b,a);
    it = cache_is_same_program.find(Y);
    if (it != cache_is_same_program.end())
        return *it;

    return cache_is_same_program[X] = a->IsSameProgram(*b);
}

bool Scheduler::FindNextConflict(
    const RecList     &cardlist,
    const RecordingInfo *p,
    RecConstIter      &j,
    int               openEnd) const
{
    for ( ; j != cardlist.end(); ++j)
    {
        const RecordingInfo *q = *j;
        QString msg;

        if (p == q)
            continue;

        if (!Recording(q))
            continue;

        if (debugConflicts)
            msg = QString("comparing with '%1' ").arg(q->GetTitle());

        if (p->GetCardID() != 0 && (p->GetCardID() != q->GetCardID()) &&
            !igrp.GetSharedInputGroup(p->GetInputID(), q->GetInputID()))
        {
            if (debugConflicts)
                msg += "  cardid== ";
            continue;
        }

        if (openEnd == 2 || (openEnd == 1 && p->GetChanID() != q->GetChanID()))
        {
            if (p->GetRecordingEndTime() < q->GetRecordingStartTime() ||
                p->GetRecordingStartTime() > q->GetRecordingEndTime())
            {
                if (debugConflicts)
                    msg += "  no-overlap ";
                continue;
            }
        }
        else
        {
            if (p->GetRecordingEndTime() <= q->GetRecordingStartTime() ||
                p->GetRecordingStartTime() >= q->GetRecordingEndTime())
            {
                if (debugConflicts)
                    msg += "  no-overlap ";
                continue;
            }
        }

        if (debugConflicts)
        {
            LOG(VB_SCHEDULE, LOG_INFO, msg);
            LOG(VB_SCHEDULE, LOG_INFO, 
                QString("  cardid's: %1, %2 Shared input group: %3 "
                        "mplexid's: %4, %5")
                     .arg(p->GetCardID()).arg(q->GetCardID())
                     .arg(igrp.GetSharedInputGroup(
                              p->GetInputID(), q->GetInputID()))
                     .arg(p->QueryMplexID()).arg(q->QueryMplexID()));
        }

        // if two inputs are in the same input group we have a conflict
        // unless the programs are on the same multiplex.
        if (p->GetCardID() && (p->GetCardID() != q->GetCardID()) &&
            igrp.GetSharedInputGroup(p->GetInputID(), q->GetInputID()))
        {
            uint p_mplexid = p->QueryMplexID();
            if (p_mplexid && (p_mplexid == q->QueryMplexID()))
                continue;
        }

        if (debugConflicts)
            LOG(VB_SCHEDULE, LOG_INFO, "Found conflict");

        return true;
    }

    if (debugConflicts)
        LOG(VB_SCHEDULE, LOG_INFO, "No conflict");

    return false;
}

const RecordingInfo *Scheduler::FindConflict(
    const QMap<int, RecList> &reclists,
    const RecordingInfo        *p,
    int openend) const
{
    QMap<int, RecList>::const_iterator it = reclists.begin();
    for (; it != reclists.end(); ++it)
    {
        if (debugConflicts)
            LOG(VB_SCHEDULE, LOG_INFO,
                QString("Checking '%1' for conflicts on cardid %2")
                    .arg(p->GetTitle()).arg(it.key()));

        const RecList &cardlist = *it;
        RecConstIter k = cardlist.begin();
        if (FindNextConflict(cardlist, p, k, openend))
        {
            return *k;
        }
    }

    return NULL;
}

void Scheduler::MarkOtherShowings(RecordingInfo *p)
{
    RecList *showinglist = &titlelistmap[p->GetTitle()];

    MarkShowingsList(*showinglist, p);

    if (p->GetRecordingRuleType() == kFindOneRecord ||
        p->GetRecordingRuleType() == kFindDailyRecord ||
        p->GetRecordingRuleType() == kFindWeeklyRecord)
    {
        showinglist = &recordidlistmap[p->GetRecordingRuleID()];
        MarkShowingsList(*showinglist, p);
    }

    if (p->GetRecordingRuleType() == kOverrideRecord && p->GetFindID())
    {
        showinglist = &recordidlistmap[p->GetParentRecordingRuleID()];
        MarkShowingsList(*showinglist, p);
    }
}

void Scheduler::MarkShowingsList(RecList &showinglist, RecordingInfo *p)
{
    RecIter i = showinglist.begin();
    for ( ; i != showinglist.end(); ++i)
    {
        RecordingInfo *q = *i;
        if (q == p)
            continue;
        if (q->GetRecordingStatus() != rsUnknown &&
            q->GetRecordingStatus() != rsWillRecord &&
            q->GetRecordingStatus() != rsEarlierShowing &&
            q->GetRecordingStatus() != rsLaterShowing)
            continue;
        if (q->IsSameTimeslot(*p))
            q->SetRecordingStatus(rsLaterShowing);
        else if (q->GetRecordingRuleType() != kSingleRecord &&
                 q->GetRecordingRuleType() != kOverrideRecord &&
                 IsSameProgram(q,p))
        {
            if (q->GetRecordingStartTime() < p->GetRecordingStartTime())
                q->SetRecordingStatus(rsLaterShowing);
            else
                q->SetRecordingStatus(rsEarlierShowing);
        }
    }
}

void Scheduler::BackupRecStatus(void)
{
    RecIter i = worklist.begin();
    for ( ; i != worklist.end(); ++i)
    {
        RecordingInfo *p = *i;
        p->savedrecstatus = p->GetRecordingStatus();
    }
}

void Scheduler::RestoreRecStatus(void)
{
    RecIter i = worklist.begin();
    for ( ; i != worklist.end(); ++i)
    {
        RecordingInfo *p = *i;
        p->SetRecordingStatus(p->savedrecstatus);
    }
}

bool Scheduler::TryAnotherShowing(RecordingInfo *p, bool samePriority,
                                   bool preserveLive)
{
    PrintRec(p, "     >");

    if (p->GetRecordingStatus() == rsRecording ||
        p->GetRecordingStatus() == rsTuning)
        return false;

    RecList *showinglist = &titlelistmap[p->GetTitle()];

    if (p->GetRecordingRuleType() == kFindOneRecord ||
        p->GetRecordingRuleType() == kFindDailyRecord ||
        p->GetRecordingRuleType() == kFindWeeklyRecord)
        showinglist = &recordidlistmap[p->GetRecordingRuleID()];

    RecStatusType oldstatus = p->GetRecordingStatus();
    p->SetRecordingStatus(rsLaterShowing);

    bool hasLaterShowing = false;

    RecIter j = showinglist->begin();
    for ( ; j != showinglist->end(); ++j)
    {
        RecordingInfo *q = *j;
        if (q == p)
            continue;

        if (samePriority &&
            (q->GetRecordingPriority() < p->GetRecordingPriority()))
        {
            continue;
        }

        hasLaterShowing = false;

        if (q->GetRecordingStatus() != rsEarlierShowing &&
            q->GetRecordingStatus() != rsLaterShowing &&
            q->GetRecordingStatus() != rsUnknown)
        {
            continue;
        }

        if (!p->IsSameTimeslot(*q))
        {
            if (!IsSameProgram(p,q))
                continue;
            if ((p->GetRecordingRuleType() == kSingleRecord ||
                 p->GetRecordingRuleType() == kOverrideRecord))
                continue;
            if (q->GetRecordingStartTime() < schedTime &&
                p->GetRecordingStartTime() >= schedTime)
                continue;

            hasLaterShowing |= preserveLive;
        }

        if (samePriority)
            PrintRec(q, "     %");
        else
            PrintRec(q, "     $");

        bool failedLiveCheck = false;
        if (preserveLive)
        {
            failedLiveCheck |=
                (!livetvpriority ||
                 p->GetRecordingPriority() - prefinputpri >
                 q->GetRecordingPriority());

            // It is pointless to preempt another livetv session.
            // (the retrylist contains dummy livetv pginfo's)
            RecConstIter k = retrylist.begin();
            if (FindNextConflict(retrylist, q, k))
            {
                PrintRec(*k, "       L!");
                continue;
            }
        }

        const RecordingInfo *conflict = FindConflict(cardlistmap, q);
        if (conflict)
        {
            PrintRec(conflict, "        !");
            continue;
        }

        if (hasLaterShowing)
        {
            QString id = p->MakeUniqueSchedulerKey();
            hasLaterList[id] = true;
            continue;
        }

        if (failedLiveCheck)
        {
            // Failed the priority check or "Move scheduled shows to
            // avoid LiveTV feature" is turned off.
            // However, there is no conflict so if this alternate showing
            // is on an equivalent virtual card, allow the move.
            bool equiv = (p->GetSourceID() == q->GetSourceID() &&
                          igrp.GetSharedInputGroup(
                              p->GetInputID(), q->GetInputID()));

            if (!equiv)
                continue;
        }

        if (preserveLive)
        {
            QString msg = QString(
                "Moved \"%1\" on chanid: %2 from card: %3 to %4 "
                "to avoid LiveTV conflict")
                .arg(p->GetTitle()).arg(p->GetChanID())
                .arg(p->GetCardID()).arg(q->GetCardID());
            LOG(VB_SCHEDULE, LOG_INFO, msg);
        }

        q->SetRecordingStatus(rsWillRecord);
        MarkOtherShowings(q);
        PrintRec(p, "     -");
        PrintRec(q, "     +");
        return true;
    }

    p->SetRecordingStatus(oldstatus);
    return false;
}

void Scheduler::SchedNewRecords(void)
{
    LOG(VB_SCHEDULE, LOG_INFO, "Scheduling:");

    if (VERBOSE_LEVEL_CHECK(VB_SCHEDULE, LOG_INFO))
    {
        LOG(VB_SCHEDULE, LOG_INFO,
            "+ = schedule this showing to be recorded");
        LOG(VB_SCHEDULE, LOG_INFO,
            "# = could not schedule this showing, retry later");
        LOG(VB_SCHEDULE, LOG_INFO,
            "! = conflict caused by this showing");
        LOG(VB_SCHEDULE, LOG_INFO,
            "/ = retry this showing, same priority pass");
        LOG(VB_SCHEDULE, LOG_INFO,
            "? = retry this showing, lower priority pass");
        LOG(VB_SCHEDULE, LOG_INFO,
            "> = try another showing for this program");
        LOG(VB_SCHEDULE, LOG_INFO,
            "% = found another showing, same priority required");
        LOG(VB_SCHEDULE, LOG_INFO,
            "$ = found another showing, lower priority allowed");
        LOG(VB_SCHEDULE, LOG_INFO,
            "- = unschedule a showing in favor of another one");
    }

    int openEnd = gCoreContext->GetNumSetting("SchedOpenEnd", 0);

    RecIter i = worklist.begin();
    while (i != worklist.end())
    {
        RecordingInfo *p = *i;
        if (p->GetRecordingStatus() == rsRecording ||
            p->GetRecordingStatus() == rsTuning)
            MarkOtherShowings(p);
        else if (p->GetRecordingStatus() == rsUnknown)
        {
            const RecordingInfo *conflict = FindConflict(cardlistmap, p,
                                                         openEnd);
            if (!conflict)
            {
                p->SetRecordingStatus(rsWillRecord);

                if (p->GetRecordingStartTime() < schedTime.addSecs(90))
                {
                    QString id = p->MakeUniqueSchedulerKey();
                    if (!recPendingList.contains(id))
                        recPendingList[id] = false;

                    livetvTime = (livetvTime < schedTime) ?
                        schedTime : livetvTime;
                }

                MarkOtherShowings(p);
                PrintRec(p, "  +");
            }
            else
            {
                retrylist.push_front(p);
                PrintRec(p, "  #");
                PrintRec(conflict, "     !");
            }
        }

        int lastpri = p->GetRecordingPriority();
        ++i;
        if (i == worklist.end() || lastpri != (*i)->GetRecordingPriority())
        {
            MoveHigherRecords();
            retrylist.clear();
        }
    }
}

void Scheduler::MoveHigherRecords(bool move_this)
{
    RecIter i = retrylist.begin();
    for ( ; move_this && i != retrylist.end(); ++i)
    {
        RecordingInfo *p = *i;
        if (p->GetRecordingStatus() != rsUnknown)
            continue;

        PrintRec(p, "  /");

        BackupRecStatus();
        p->SetRecordingStatus(rsWillRecord);
        MarkOtherShowings(p);

        RecList cardlist;
        QMap<int, RecList>::const_iterator it = cardlistmap.begin();
        for (; it != cardlistmap.end(); ++it)
        {
            RecConstIter it2 = (*it).begin();
            for (; it2 != (*it).end(); ++it2)
                cardlist.push_back(*it2);
        }
        RecConstIter k = cardlist.begin();
        for ( ; FindNextConflict(cardlist, p, k ); ++k)
        {
            if (!TryAnotherShowing(*k, true))
            {
                RestoreRecStatus();
                break;
            }
        }

        if (p->GetRecordingStatus() == rsWillRecord)
            PrintRec(p, "  +");
    }

    i = retrylist.begin();
    for ( ; i != retrylist.end(); ++i)
    {
        RecordingInfo *p = *i;
        if (p->GetRecordingStatus() != rsUnknown)
            continue;

        PrintRec(p, "  ?");

        if (move_this && TryAnotherShowing(p, false))
            continue;

        BackupRecStatus();
        p->SetRecordingStatus(rsWillRecord);
        if (move_this)
            MarkOtherShowings(p);

        RecList cardlist;
        QMap<int, RecList>::const_iterator it = cardlistmap.begin();
        for (; it != cardlistmap.end(); ++it)
        {
            RecConstIter it2 = (*it).begin();
            for (; it2 != (*it).end(); ++it2)
                cardlist.push_back(*it2);
        }

        RecConstIter k = cardlist.begin();
        for ( ; FindNextConflict(cardlist, p, k); ++k)
        {
            if ((p->GetRecordingPriority() < (*k)->GetRecordingPriority() &&
                 !schedMoveHigher && move_this) ||
                !TryAnotherShowing(*k, false, !move_this))
            {
                RestoreRecStatus();
                break;
            }
        }

        if (move_this && p->GetRecordingStatus() == rsWillRecord)
            PrintRec(p, "  +");
    }
}

void Scheduler::PruneRedundants(void)
{
    RecordingInfo *lastp = NULL;

    RecIter i = worklist.begin();
    while (i != worklist.end())
    {
        RecordingInfo *p = *i;

        // Delete anything that has already passed since we can't
        // change history, can we?
        if (p->GetRecordingStatus() != rsRecording &&
            p->GetRecordingStatus() != rsTuning &&
            p->GetRecordingStatus() != rsMissedFuture &&
            p->GetScheduledEndTime() < schedTime &&
            p->GetRecordingEndTime() < schedTime)
        {
            delete p;
            *(i++) = NULL;
            continue;
        }

        // Check for rsConflict
        if (p->GetRecordingStatus() == rsUnknown)
            p->SetRecordingStatus(rsConflict);

        // Restore the old status for some selected cases.
        if (p->GetRecordingStatus() == rsMissedFuture ||
            (p->GetRecordingStatus() == rsMissed &&
             p->oldrecstatus != rsUnknown) ||
            (p->GetRecordingStatus() == rsCurrentRecording &&
             p->oldrecstatus == rsPreviousRecording && !p->future) ||
            (p->GetRecordingStatus() != rsWillRecord &&
             p->oldrecstatus == rsAborted))
        {
            RecStatusType rs = p->GetRecordingStatus();
            p->SetRecordingStatus(p->oldrecstatus);
            // Re-mark rsMissedFuture entries so non-future history
            // will be saved in the scheduler thread.
            if (rs == rsMissedFuture)
                p->oldrecstatus = rsMissedFuture;
        }

        if (!Recording(p))
        {
            p->SetCardID(0);
            p->SetInputID(0);
        }

        // Check for redundant against last non-deleted
        if (!lastp || lastp->GetRecordingRuleID() != p->GetRecordingRuleID() ||
            !lastp->IsSameTimeslot(*p))
        {
            lastp = p;
            ++i;
        }
        else
        {
            // Flag lower priority showings that will recorded so we
            // can warn the user about them
            if (lastp->GetRecordingStatus() == rsWillRecord &&
                p->GetRecordingPriority() >
                lastp->GetRecordingPriority() - lastp->GetRecordingPriority2())
            {
                lastp->SetRecordingPriority2(
                    lastp->GetRecordingPriority() - p->GetRecordingPriority());
            }
            delete p;
            *(i++) = NULL;
        }
    }

    erase_nulls(worklist);
}

void Scheduler::UpdateNextRecord(void)
{
    if (specsched)
        return;

    QMap<int, QDateTime> nextRecMap;

    RecIter i = reclist.begin();
    while (i != reclist.end())
    {
        RecordingInfo *p = *i;
        if (p->GetRecordingStatus() == rsWillRecord &&
            nextRecMap[p->GetRecordingRuleID()].isNull())
        {
            nextRecMap[p->GetRecordingRuleID()] = p->GetRecordingStartTime();
        }

        if (p->GetRecordingRuleType() == kOverrideRecord &&
            p->GetParentRecordingRuleID() > 0 &&
            p->GetRecordingStatus() == rsWillRecord &&
            nextRecMap[p->GetParentRecordingRuleID()].isNull())
        {
            nextRecMap[p->GetParentRecordingRuleID()] =
                p->GetRecordingStartTime();
        }
        ++i;
    }

    MSqlQuery query(dbConn);
    query.prepare("SELECT recordid, next_record FROM record;");

    if (query.exec() && query.isActive())
    {
        MSqlQuery subquery(dbConn);

        while (query.next())
        {
            int recid = query.value(0).toInt();
            QDateTime next_record = query.value(1).toDateTime();

            if (next_record == nextRecMap[recid])
                continue;

            if (nextRecMap[recid].isNull() || !next_record.isValid())
            {
                subquery.prepare("UPDATE record "
                                 "SET next_record = '0000-00-00 00:00:00' "
                                 "WHERE recordid = :RECORDID;");
                subquery.bindValue(":RECORDID", recid);

            }
            else
            {
                subquery.prepare("UPDATE record SET next_record = :NEXTREC "
                                 "WHERE recordid = :RECORDID;");
                subquery.bindValue(":RECORDID", recid);
                subquery.bindValue(":NEXTREC", nextRecMap[recid]);
            }
            if (!subquery.exec())
                MythDB::DBError("Update next_record", subquery);
            else
                LOG(VB_SCHEDULE, LOG_INFO, LOC +
                    QString("Update next_record for %1").arg(recid));
        }
    }
}

void Scheduler::getConflicting(RecordingInfo *pginfo, QStringList &strlist)
{
    RecList retlist;
    getConflicting(pginfo, &retlist);

    strlist << QString::number(retlist.size());

    while (!retlist.empty())
    {
        RecordingInfo *p = retlist.front();
        p->ToStringList(strlist);
        delete p;
        retlist.pop_front();
    }
}

void Scheduler::getConflicting(RecordingInfo *pginfo, RecList *retlist)
{
    QMutexLocker lockit(&schedLock);

    RecConstIter i = reclist.begin();
    for (; FindNextConflict(reclist, pginfo, i); ++i)
    {
        const RecordingInfo *p = *i;
        retlist->push_back(new RecordingInfo(*p));
    }
}

bool Scheduler::GetAllPending(RecList &retList) const
{
    QMutexLocker lockit(&schedLock);

    bool hasconflicts = false;

    RecConstIter it = reclist.begin();
    for (; it != reclist.end(); ++it)
    {
        if ((*it)->GetRecordingStatus() == rsConflict)
            hasconflicts = true;
        retList.push_back(new RecordingInfo(**it));
    }

    return hasconflicts;
}

QMap<QString,ProgramInfo*> Scheduler::GetRecording(void) const
{
    QMutexLocker lockit(&schedLock);

    QMap<QString,ProgramInfo*> recMap;
    RecConstIter it = reclist.begin();
    for (; it != reclist.end(); ++it)
    {
        if (rsRecording == (*it)->GetRecordingStatus() ||
            rsTuning == (*it)->GetRecordingStatus())
            recMap[(*it)->MakeUniqueKey()] = new ProgramInfo(**it);
    }

    return recMap;
}

RecStatusType Scheduler::GetRecStatus(const ProgramInfo &pginfo)
{
    QMutexLocker lockit(&schedLock);

    for (RecConstIter it = reclist.begin(); it != reclist.end(); ++it)
    {
        if (pginfo.IsSameRecording(**it))
        {
            return (rsRecording == (**it).GetRecordingStatus() ||
                    rsTuning == (**it).GetRecordingStatus()) ?
                (**it).GetRecordingStatus() : pginfo.GetRecordingStatus();
        }
    }

    return pginfo.GetRecordingStatus();
}

void Scheduler::GetAllPending(QStringList &strList) const
{
    RecList retlist;
    bool hasconflicts = GetAllPending(retlist);

    strList << QString::number(hasconflicts);
    strList << QString::number(retlist.size());

    while (!retlist.empty())
    {
        RecordingInfo *p = retlist.front();
        p->ToStringList(strList);
        delete p;
        retlist.pop_front();
    }
}

void Scheduler::getAllScheduled(QStringList &strList)
{
    RecList schedlist;

    findAllScheduledPrograms(schedlist);

    strList << QString::number(schedlist.size());

    while (!schedlist.empty())
    {
        RecordingInfo *pginfo = schedlist.front();
        pginfo->ToStringList(strList);
        delete pginfo;
        schedlist.pop_front();
    }
}

void Scheduler::Reschedule(int recordid)
{
    QMutexLocker locker(&schedLock);

    if (recordid == -1)
        reschedQueue.clear();

    if (recordid != 0 || reschedQueue.empty())
        reschedQueue.enqueue(recordid);

    reschedWait.wakeOne();
}

void Scheduler::AddRecording(const RecordingInfo &pi)
{
    QMutexLocker lockit(&schedLock);

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("AddRecording() recid: %1")
            .arg(pi.GetRecordingRuleID()));

    for (RecIter it = reclist.begin(); it != reclist.end(); ++it)
    {
        RecordingInfo *p = *it;
        if (p->GetRecordingStatus() == rsRecording &&
            p->IsSameProgramTimeslot(pi))
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + "Not adding recording, " +
                QString("'%1' is already in reclist.")
                    .arg(pi.GetTitle()));
            return;
        }
    }

    LOG(VB_SCHEDULE, LOG_INFO, LOC +
        QString("Adding '%1' to reclist.").arg(pi.GetTitle()));

    RecordingInfo * new_pi = new RecordingInfo(pi);
    reclist.push_back(new_pi);
    reclist_changed = true;

    // Save rsRecording recstatus to DB
    // This allows recordings to resume on backend restart
    new_pi->AddHistory(false);

    // Make sure we have a ScheduledRecording instance
    new_pi->GetRecordingRule();

    // Trigger reschedule..
    reschedQueue.enqueue(pi.GetRecordingRuleID());
    reschedWait.wakeOne();
}

bool Scheduler::IsBusyRecording(const RecordingInfo *rcinfo)
{
    if (!m_tvList || !rcinfo)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "IsBusyRecording() -> true, no tvList or no rcinfo");

        return true;
    }

    EncoderLink *rctv = (*m_tvList)[rcinfo->GetCardID()];
    // first check the card we will be recording on...
    if (!rctv || rctv->IsBusyRecording())
        return true;

    // now check other cards in the same input group as the recording.
    TunedInputInfo busy_input;
    uint inputid = rcinfo->GetInputID();
    vector<uint> cardids = CardUtil::GetConflictingCards(
        inputid, rcinfo->GetCardID());
    for (uint i = 0; i < cardids.size(); i++)
    {
        rctv = (*m_tvList)[cardids[i]];
        if (!rctv)
        {
#if 0
            LOG(VB_SCHEDULE, LOG_ERR, LOC +
                QString("IsBusyRecording() -> true, rctv(NULL) for card %2")
                    .arg(cardids[i]));
#endif
            return true;
        }

        if (rctv->IsBusy(&busy_input, -1) &&
            igrp.GetSharedInputGroup(busy_input.inputid, inputid))
        {
            return true;
        }
    }

    return false;
}

void Scheduler::OldRecordedFixups(void)
{
    MSqlQuery query(dbConn);

    // Mark anything that was recording as aborted.
    query.prepare("UPDATE oldrecorded SET recstatus = :RSABORTED "
                  "  WHERE recstatus = :RSRECORDING OR recstatus = :RSTUNING");
    query.bindValue(":RSABORTED", rsAborted);
    query.bindValue(":RSRECORDING", rsRecording);
    query.bindValue(":RSTUNING", rsTuning);
    if (!query.exec())
        MythDB::DBError("UpdateAborted", query);

    // Mark anything that was going to record as missed.
    query.prepare("UPDATE oldrecorded SET recstatus = :RSMISSED "
                  "WHERE recstatus = :RSWILLRECORD");
    query.bindValue(":RSMISSED", rsMissed);
    query.bindValue(":RSWILLRECORD", rsWillRecord);
    if (!query.exec())
        MythDB::DBError("UpdateMissed", query);

    // Mark anything that was set to rsCurrentRecording as
    // rsPreviousRecording.
    query.prepare("UPDATE oldrecorded SET recstatus = :RSPREVIOUS "
                  "WHERE recstatus = :RSCURRENT");
    query.bindValue(":RSPREVIOUS", rsPreviousRecording);
    query.bindValue(":RSCURRENT", rsCurrentRecording);
    if (!query.exec())
        MythDB::DBError("UpdateCurrent", query);

    // Clear the "future" status of anything older than the maximum
    // endoffset.  Anything more recent will bee handled elsewhere
    // during normal processing.
    query.prepare("UPDATE oldrecorded SET future = 0 "
                  "WHERE future > 0 AND "
                  "      endtime < (NOW() - INTERVAL 8 HOUR)");
    if (!query.exec())
        MythDB::DBError("UpdateFuture", query);
}

void Scheduler::run(void)
{
    RunProlog();

    dbConn = MSqlQuery::SchedCon();

    // Notify constructor that we're actually running
    {
        QMutexLocker lockit(&schedLock);
        reschedWait.wakeAll();
    }

    OldRecordedFixups();

    // wait for slaves to connect
    sleep(3);

    QMutexLocker lockit(&schedLock);

    reschedQueue.clear();
    reschedQueue.enqueue(-1);

    int       prerollseconds  = 0;
    int       wakeThreshold   = 300;
    int       idleTimeoutSecs = 0;
    int       idleWaitForRecordingTime = 15; // in minutes
    bool      blockShutdown   =
        gCoreContext->GetNumSetting("blockSDWUwithoutClient", 1);
    bool      firstRun        = true;
    QDateTime lastSleepCheck  = QDateTime::currentDateTime().addDays(-1);
    RecIter   startIter       = reclist.begin();
    QDateTime idleSince       = QDateTime();
    int       maxSleep        = 60000; // maximum sleep time in milliseconds
    int       schedRunTime    = 30; // max scheduler run time in seconds

    while (doRun)
    {
        QDateTime curtime = QDateTime::currentDateTime();
        bool statuschanged = false;
        int secs_to_next = (startIter != reclist.end()) ?
            curtime.secsTo((*startIter)->GetRecordingStartTime()) : 60*60;

        // If we're about to start a recording don't do any reschedules...
        // instead sleep for a bit
        if (secs_to_next < (schedRunTime + 2))
        {
            int msecs = CalcTimeToNextHandleRecordingEvent(
                curtime, startIter, reclist, prerollseconds, maxSleep);
            LOG(VB_SCHEDULE, LOG_INFO,
                QString("sleeping for %1 ms (s2n: %2 sr: %3)")
                    .arg(msecs).arg(secs_to_next).arg(schedRunTime));
            if (msecs < 100)
                (void) ::usleep(msecs * 1000);
            else
                reschedWait.wait(&schedLock, msecs);
        }
        else
        {
            if (reschedQueue.empty())
            {
                int sched_sleep = (secs_to_next - schedRunTime - 1) * 1000;
                sched_sleep = min(sched_sleep, maxSleep);
                if (secs_to_next < prerollseconds + (maxSleep/1000))
                    sched_sleep = min(sched_sleep, 5000);
                LOG(VB_SCHEDULE, LOG_INFO,
                    QString("sleeping for %1 ms (interuptable)")
                        .arg(sched_sleep));
                reschedWait.wait(&schedLock, sched_sleep);
                if (!doRun)
                    break;
            }

            QTime t; t.start();
            if (!reschedQueue.empty() && HandleReschedule())
            {
                statuschanged = true;
                startIter = reclist.begin();

                // The master backend is a long lived program, so
                // we reload some key settings on each reschedule.
                prerollseconds  =
                    gCoreContext->GetNumSetting("RecordPreRoll", 0);
                wakeThreshold =
                    gCoreContext->GetNumSetting("WakeUpThreshold", 300);
                idleTimeoutSecs =
                    gCoreContext->GetNumSetting("idleTimeoutSecs", 0);
                idleWaitForRecordingTime =
                    gCoreContext->GetNumSetting("idleWaitForRecordingTime", 15);
            }
            int e = t.elapsed();
            if (e > 0)
            {
                schedRunTime = (firstRun) ? 0 : schedRunTime;
                schedRunTime =
                    max((int)(((e + 999) / 1000) * 1.5f), schedRunTime);
            }

            if (firstRun)
            {
                blockShutdown &= HandleRunSchedulerStartup(
                    prerollseconds, idleWaitForRecordingTime);
                firstRun = false;

                // HandleRunSchedulerStartup releases the schedLock so the
                // reclist may have changed. If it has go to top of loop
                // and update secs_to_next...
                if (reclist_changed)
                    continue;
            }

            // Unless a recording is about to start, check for slaves
            // that can be put to sleep if it has been at least five
            // minutes since we last put slaves to sleep.
            curtime = QDateTime::currentDateTime();
            secs_to_next = (startIter != reclist.end()) ?
                curtime.secsTo((*startIter)->GetRecordingStartTime()) : 60*60;
            if ((secs_to_next > schedRunTime * 1.5f) &&
                (lastSleepCheck.secsTo(curtime) > 300))
            {
                PutInactiveSlavesToSleep();
                lastSleepCheck = QDateTime::currentDateTime();
            }
        }

        // Skip past recordings that are already history
        // (i.e. AddHistory() has been called setting oldrecstatus)
        for ( ; startIter != reclist.end(); ++startIter)
        {
            if ((*startIter)->GetRecordingStatus() !=
                (*startIter)->oldrecstatus)
            {
                break;
            }
        }

        // Start any recordings that are due to be started
        // & call RecordPending for recordings due to start in 30 seconds
        // & handle rsTuning updates
        bool done = false;
        for (RecIter it = startIter; it != reclist.end() && !done; ++it)
            done = HandleRecording(**it, statuschanged, prerollseconds);

        /// Wake any slave backends that need waking
        curtime = QDateTime::currentDateTime();
        for (RecIter it = startIter; it != reclist.end(); ++it)
        {
            int secsleft = curtime.secsTo((*it)->GetRecordingStartTime());
            if ((secsleft - prerollseconds) <= wakeThreshold)
                HandleWakeSlave(**it, prerollseconds);
            else
                break;
        }

        if (statuschanged)
        {
            MythEvent me("SCHEDULE_CHANGE");
            gCoreContext->dispatch(me);
            idleSince = QDateTime();
        }

        // if idletimeout is 0, the user disabled the auto-shutdown feature
        if ((idleTimeoutSecs > 0) && (m_mainServer != NULL))
        {
            HandleIdleShutdown(blockShutdown, idleSince, prerollseconds,
                               idleTimeoutSecs, idleWaitForRecordingTime,
                               statuschanged);
        }
    }

    RunEpilog();
}

int Scheduler::CalcTimeToNextHandleRecordingEvent(
    const QDateTime &curtime,
    RecConstIter startIter, const RecList &reclist,
    int prerollseconds, int max_sleep /*ms*/)
{
    if (startIter == reclist.end())
        return max_sleep;

    int msecs = max_sleep;
    for (RecConstIter i = startIter; i != reclist.end() && (msecs > 0); ++i)
    {
        // Check on recordings that we've told to start, but have
        // not yet started every second or so.
        if ((*i)->GetRecordingStatus() == rsTuning)
        {
            msecs = min(msecs, 1000);
            continue;
        }

        // These recordings have already been handled..
        if ((*i)->GetRecordingStatus() == (*i)->oldrecstatus)
            continue;

        int secs_to_next = curtime.secsTo((*i)->GetRecordingStartTime());

        if (!recPendingList[(*i)->MakeUniqueSchedulerKey()])
            secs_to_next -= 30;

        if (secs_to_next < 0)
        {
            msecs = 0;
            break;
        }

        // This is what normally breaks us out of the loop...
        if (secs_to_next > max_sleep)
        {
            msecs = min(msecs, max_sleep);
            break;
        }

        if (secs_to_next > 31)
        {
            msecs = min(msecs, 30 * 1000);
            continue;
        }

        if ((secs_to_next-1) * 1000 > msecs)
            continue;

        if (secs_to_next < 15)
        {
            QTime st = (*i)->GetRecordingStartTime().time();
            int tmp = curtime.time().msecsTo(st);
            tmp = (tmp < 0) ? tmp + 86400000 : tmp;
            msecs = (tmp > 15*1000) ? 0 : min(msecs, tmp);
        }
        else
        {
            msecs = min(msecs, (secs_to_next-1) * 1000);
        }
    }

    return min(msecs, max_sleep);
}

bool Scheduler::HandleReschedule(void)
{
    // We might have been inactive for a long time, so make
    // sure our DB connection is fresh before continuing.
    dbConn = MSqlQuery::SchedCon();

    struct timeval fillstart;
    gettimeofday(&fillstart, NULL);
    QString msg;
    bool deleteFuture = false;

    while (!reschedQueue.empty())
    {
        int recordid = reschedQueue.dequeue();

        LOG(VB_GENERAL, LOG_INFO, QString("Reschedule requested for id %1.")
                .arg(recordid));

        if (recordid != 0)
        {
            if (recordid == -1)
                reschedQueue.clear();

            deleteFuture = true;
            schedLock.unlock();
            recordmatchLock.lock();
            UpdateMatches(recordid);
            recordmatchLock.unlock();
            schedLock.lock();
        }
    }

    // Delete future oldrecorded entries that no longer
    // match any potential recordings.
    if (deleteFuture)
    {
        MSqlQuery query(dbConn);
        query.prepare("DELETE oldrecorded FROM oldrecorded "
                      "LEFT JOIN recordmatch ON "
                      "    recordmatch.chanid    = oldrecorded.chanid    AND "
                      "    recordmatch.starttime = oldrecorded.starttime     "
                      "WHERE oldrecorded.future > 0 AND "
                      "    recordmatch.recordid IS NULL");
        if (!query.exec())
            MythDB::DBError("DeleteFuture", query);
    }

    struct timeval fillend;
    gettimeofday(&fillend, NULL);

    float matchTime = ((fillend.tv_sec - fillstart.tv_sec ) * 1000000 +
                       (fillend.tv_usec - fillstart.tv_usec)) / 1000000.0;

    gettimeofday(&fillstart, NULL);
    bool worklistused = FillRecordList();
    gettimeofday(&fillend, NULL);
    if (worklistused)
    {
        UpdateNextRecord();
        PrintList();
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, "Reschedule interrupted, will retry");
        reschedQueue.enqueue(0);
        return false;
    }

    float placeTime = ((fillend.tv_sec - fillstart.tv_sec ) * 1000000 +
                       (fillend.tv_usec - fillstart.tv_usec)) / 1000000.0;

    msg.sprintf("Scheduled %d items in %.1f = %.2f match + %.2f place",
                (int)reclist.size(), matchTime + placeTime, matchTime,
                placeTime);
    LOG(VB_GENERAL, LOG_INFO, msg);

    fsInfoCacheFillTime = QDateTime::currentDateTime().addSecs(-1000);

    // Write changed entries to oldrecorded.
    RecIter it = reclist.begin();
    for ( ; it != reclist.end(); ++it)
    {
        RecordingInfo *p = *it;
        if (p->GetRecordingStatus() != p->oldrecstatus)
        {
            if (p->GetRecordingEndTime() < schedTime)
                p->AddHistory(false, false, false);
            else if (p->GetRecordingStartTime() < schedTime &&
                     p->GetRecordingStatus() != rsWillRecord)
                p->AddHistory(false, false, false);
            else
                p->AddHistory(false, false, true);
        }
        else if (p->future)
        {
            // Force a non-future, oldrecorded entry to
            // get written when the time comes.
            p->oldrecstatus = rsUnknown;
        }
        p->future = false;
    }

    SendMythSystemEvent("SCHEDULER_RAN");

    return true;
}

bool Scheduler::HandleRunSchedulerStartup(
    int prerollseconds, int idleWaitForRecordingTime)
{
    bool blockShutdown = true;

    // The parameter given to the startup_cmd. "user" means a user
    // probably started the backend process, "auto" means it was
    // started probably automatically.
    QString startupParam = "user";

    // find the first recording that WILL be recorded
    RecIter firstRunIter = reclist.begin();
    for ( ; firstRunIter != reclist.end(); ++firstRunIter)
    {
        if ((*firstRunIter)->GetRecordingStatus() == rsWillRecord)
            break;
    }

    // have we been started automatically?
    QDateTime curtime = QDateTime::currentDateTime();
    if (WasStartedAutomatically() ||
        ((firstRunIter != reclist.end()) &&
         ((curtime.secsTo((*firstRunIter)->GetRecordingStartTime()) -
           prerollseconds) < (idleWaitForRecordingTime * 60))))
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "AUTO-Startup assumed");
        startupParam = "auto";

        // Since we've started automatically, don't wait for
        // client to connect before allowing shutdown.
        blockShutdown = false;
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "Seem to be woken up by USER");
    }

    QString startupCommand = gCoreContext->GetSetting("startupCommand", "");
    if (!startupCommand.isEmpty())
    {
        startupCommand.replace("$status", startupParam);
        schedLock.unlock();
        myth_system(startupCommand);
        schedLock.lock();
    }

    return blockShutdown;
}

// If a recording is about to start on a backend in a few minutes, wake it...
void Scheduler::HandleWakeSlave(RecordingInfo &ri, int prerollseconds)
{
    static const int sysEventSecs[5] = { 120, 90, 60, 30, 0 };

    QDateTime curtime = QDateTime::currentDateTime();
    QDateTime nextrectime = ri.GetRecordingStartTime();
    int secsleft = curtime.secsTo(nextrectime);

    QMap<int, EncoderLink*>::iterator tvit = m_tvList->find(ri.GetCardID());
    if (tvit == m_tvList->end())
        return;

    QString sysEventKey = ri.MakeUniqueKey();

    int i = 0;
    bool pendingEventSent = false;
    while (sysEventSecs[i] != 0)
    {
        if ((secsleft <= sysEventSecs[i]) &&
            (!sysEvents[i].contains(sysEventKey)))
        {
            if (!pendingEventSent)
            {
                SendMythSystemRecEvent(
                    QString("REC_PENDING SECS %1").arg(secsleft), &ri);
            }

            sysEvents[i].insert(sysEventKey);
            pendingEventSent = true;
        }
        i++;
    }

    // cleanup old sysEvents once in a while
    QSet<QString> keys;
    for (i = 0; sysEventSecs[i] != 0; i++)
    {
        if (sysEvents[i].size() < 20)
            continue;

        if (keys.empty())
        {
            RecConstIter it = reclist.begin();
            for ( ; it != reclist.end(); ++it)
                keys.insert((*it)->MakeUniqueKey());
            keys.insert("something");
        }

        QSet<QString>::iterator sit = sysEvents[i].begin();
        while (sit != sysEvents[i].end())
        {
            if (!keys.contains(*sit))
                sit = sysEvents[i].erase(sit);
            else
                ++sit;
        }
    }

    EncoderLink *nexttv = *tvit;

    if (nexttv->IsAsleep() && !nexttv->IsWaking())
    {
        LOG(VB_SCHEDULE, LOG_INFO, LOC +
            QString("Slave Backend %1 is being awakened to record: %2")
                .arg(nexttv->GetHostName()).arg(ri.GetTitle()));

        if (!WakeUpSlave(nexttv->GetHostName()))
            reschedQueue.enqueue(0);
    }
    else if ((nexttv->IsWaking()) &&
             ((secsleft - prerollseconds) < 210) &&
             (nexttv->GetSleepStatusTime().secsTo(curtime) < 300) &&
             (nexttv->GetLastWakeTime().secsTo(curtime) > 10))
    {
        LOG(VB_SCHEDULE, LOG_INFO, LOC +
            QString("Slave Backend %1 not available yet, "
                    "trying to wake it up again.")
                .arg(nexttv->GetHostName()));

        if (!WakeUpSlave(nexttv->GetHostName(), false))
            reschedQueue.enqueue(0);
    }
    else if ((nexttv->IsWaking()) &&
             ((secsleft - prerollseconds) < 150) &&
             (nexttv->GetSleepStatusTime().secsTo(curtime) < 300))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("Slave Backend %1 has NOT come "
                    "back from sleep yet in 150 seconds. Setting "
                    "slave status to unknown and attempting "
                    "to reschedule around its tuners.")
                .arg(nexttv->GetHostName()));

        QMap<int, EncoderLink*>::iterator it = m_tvList->begin();
        for (; it != m_tvList->end(); ++it)
        {
            if ((*it)->GetHostName() == nexttv->GetHostName())
                (*it)->SetSleepStatus(sStatus_Undefined);
        }

        reschedQueue.enqueue(0);
    }
}

bool Scheduler::HandleRecording(
    RecordingInfo &ri, bool &statuschanged, int prerollseconds)
{
    if (ri.GetRecordingStatus() == rsTuning)
    {
        HandleTuning(ri, statuschanged);
        return false;
    }

    if (ri.GetRecordingStatus() != rsWillRecord)
    {
        if (ri.GetRecordingStatus() != ri.oldrecstatus &&
            ri.GetRecordingStartTime() <= QDateTime::currentDateTime())
        {
            ri.AddHistory(false);
        }
        return false;
    }

    QDateTime nextrectime = ri.GetRecordingStartTime();
    QDateTime curtime     = QDateTime::currentDateTime();
    int       secsleft    = curtime.secsTo(nextrectime);
    QString   schedid     = ri.MakeUniqueSchedulerKey();

    if (secsleft - prerollseconds < 60)
    {
        if (!recPendingList.contains(schedid))
        {
            recPendingList[schedid] = false;

            livetvTime = (livetvTime < nextrectime) ?
                nextrectime : livetvTime;

            reschedQueue.enqueue(0);
        }
    }

    if (secsleft - prerollseconds > 35)
        return true;

    QMap<int, EncoderLink*>::iterator tvit = m_tvList->find(ri.GetCardID());
    if (tvit == m_tvList->end())
    {
        QString msg = QString("Invalid cardid (%1) for %2")
            .arg(ri.GetCardID()).arg(ri.GetTitle());
        LOG(VB_GENERAL, LOG_ERR, LOC + msg);

        ri.SetRecordingStatus(rsTunerBusy);
        ri.AddHistory(true);
        statuschanged = true;
        return false;
    }

    EncoderLink *nexttv = *tvit;

    if (nexttv->IsTunerLocked())
    {
        QString msg = QString("SUPPRESSED recording \"%1\" on channel: "
                              "%2 on cardid: %3, sourceid %4. Tuner "
                              "is locked by an external application.")
            .arg(ri.GetTitle())
            .arg(ri.GetChanID())
            .arg(ri.GetCardID())
            .arg(ri.GetSourceID());
        LOG(VB_GENERAL, LOG_NOTICE, msg);

        ri.SetRecordingStatus(rsTunerBusy);
        ri.AddHistory(true);
        statuschanged = true;
        return false;
    }

    if ((prerollseconds > 0) && !IsBusyRecording(&ri))
    {
        // Will use pre-roll settings only if no other
        // program is currently being recorded
        secsleft -= prerollseconds;
    }

#if 0
    LOG(VB_GENERAL, LOG_DEBUG, QString("%1 seconds until %2").
            .arg(secsleft) .arg(ri.GetTitle()));
#endif

    if (secsleft > 30)
        return false;

    if (nexttv->IsWaking())
    {
        if (secsleft > 0)
        {
            LOG(VB_SCHEDULE, LOG_WARNING,
                QString("WARNING: Slave Backend %1 has NOT come "
                        "back from sleep yet.  Recording can "
                        "not begin yet for: %2")
                    .arg(nexttv->GetHostName())
                    .arg(ri.GetTitle()));
        }
        else if (nexttv->GetLastWakeTime().secsTo(curtime) > 300)
        {
            LOG(VB_SCHEDULE, LOG_WARNING,
                QString("WARNING: Slave Backend %1 has NOT come "
                        "back from sleep yet. Setting slave "
                        "status to unknown and attempting "
                        "to reschedule around its tuners.")
                    .arg(nexttv->GetHostName()));

            QMap<int, EncoderLink *>::Iterator enciter =
                m_tvList->begin();
            for (; enciter != m_tvList->end(); ++enciter)
            {
                EncoderLink *enc = *enciter;
                if (enc->GetHostName() == nexttv->GetHostName())
                    enc->SetSleepStatus(sStatus_Undefined);
            }

            reschedQueue.enqueue(0);
        }

        return false;
    }

    int fsID = -1;
    if (ri.GetPathname().isEmpty())
    {
        QString recording_dir;
        fsID = FillRecordingDir(ri.GetTitle(),
                                ri.GetHostname(),
                                ri.GetStorageGroup(),
                                ri.GetRecordingStartTime(),
                                ri.GetRecordingEndTime(),
                                ri.GetCardID(),
                                recording_dir,
                                reclist);
        ri.SetPathname(recording_dir);
    }

    if (!recPendingList[schedid])
    {
        nexttv->RecordPending(&ri, max(secsleft, 0),
                              hasLaterList.contains(schedid));
        recPendingList[schedid] = true;
    }

    if (secsleft > 0)
        return false;

    QDateTime recstartts = mythCurrentDateTime().addSecs(30);
    recstartts.setTime(
        QTime(recstartts.time().hour(), recstartts.time().minute()));
    ri.SetRecordingStartTime(recstartts);

    QString details = QString("%1: channel %2 on cardid %3, sourceid %4")
        .arg(ri.toString(ProgramInfo::kTitleSubtitle))
        .arg(ri.GetChanID())
        .arg(ri.GetCardID())
        .arg(ri.GetSourceID());

    RecStatusTypes recStatus = rsOffLine;
    if (schedulingEnabled && nexttv->IsConnected())
    {
        if (ri.GetRecordingStatus() == rsWillRecord)
        {
            recStatus = nexttv->StartRecording(&ri);
            ri.AddHistory(false);

            // activate auto expirer
            if (m_expirer)
                m_expirer->Update(ri.GetCardID(), fsID, true);
        }
    }

    HandleRecordingStatusChange(ri, recStatus, details);
    statuschanged = true;

    return false;
}

void Scheduler::HandleRecordingStatusChange(
    RecordingInfo &ri, RecStatusTypes recStatus, const QString &details)
{
    if (ri.GetRecordingStatus() == recStatus)
        return;

    ri.SetRecordingStatus(recStatus);

    if (rsTuning != recStatus)
    {
        bool doSchedAfterStart =
            ((rsRecording != recStatus) && (rsTuning != recStatus)) ||
            schedAfterStartMap[ri.GetRecordingRuleID()] ||
            (ri.GetParentRecordingRuleID() &&
             schedAfterStartMap[ri.GetParentRecordingRuleID()]);
        ri.AddHistory(doSchedAfterStart);
    }

    QString msg = (rsRecording == recStatus) ?
        QString("Started recording") :
        ((rsTuning == recStatus) ?
         QString("Tuning recording") :
         QString("Canceled recording (%1)")
         .arg(toString(ri.GetRecordingStatus(), ri.GetRecordingRuleType())));

    LOG(VB_GENERAL, LOG_INFO, QString("%1: %2").arg(msg).arg(details));

    if ((rsRecording == recStatus) || (rsTuning == recStatus))
    {
        UpdateNextRecord();
    }
    else if (rsFailed == recStatus)
    {
        MythEvent me(QString("FORCE_DELETE_RECORDING %1 %2")
                     .arg(ri.GetChanID())
                     .arg(ri.GetRecordingStartTime(ISODate)));
        gCoreContext->dispatch(me);
    }
}

void Scheduler::HandleTuning(RecordingInfo &ri, bool &statuschanged)
{
    if (rsTuning != ri.GetRecordingStatus())
        return;

    // Determine current recording status
    QMap<int, EncoderLink*>::iterator tvit = m_tvList->find(ri.GetCardID());
    RecStatusTypes recStatus = rsTunerBusy;
    if (tvit == m_tvList->end())
    {
        QString msg = QString("Invalid cardid (%1) for %2")
            .arg(ri.GetCardID()).arg(ri.GetTitle());
        LOG(VB_GENERAL, LOG_ERR, LOC + msg);
    }
    else
    {
        recStatus = (*tvit)->GetRecordingStatus();
        if (rsTuning == recStatus)
        {
            // If tuning is still taking place this long after we
            // started give up on it so the scheduler can try to
            // find another broadcast of the same material.
            QDateTime curtime = QDateTime::currentDateTime();
            if ((ri.GetRecordingStartTime().secsTo(curtime) > 180) &&
                (ri.GetScheduledStartTime().secsTo(curtime) > 180))
            {
                recStatus = rsFailed;
            }
        }
    }

    // If the status has changed, handle it
    if (rsTuning != recStatus)
    {
        QString details = QString("%1: channel %2 on cardid %3, sourceid %4")
            .arg(ri.toString(ProgramInfo::kTitleSubtitle))
            .arg(ri.GetChanID()).arg(ri.GetCardID()).arg(ri.GetSourceID());
        HandleRecordingStatusChange(ri, recStatus, details);
        statuschanged = true;
    }
}

void Scheduler::HandleIdleShutdown(
    bool &blockShutdown, QDateTime &idleSince,
    int prerollseconds, int idleTimeoutSecs, int idleWaitForRecordingTime,
    bool &statuschanged)
{
    if ((idleTimeoutSecs <= 0) || (m_mainServer == NULL))
        return;

    // we release the block when a client connects
    if (blockShutdown)
        blockShutdown &= !m_mainServer->isClientConnected();
    else
    {
        QDateTime curtime = QDateTime::currentDateTime();

        // find out, if we are currently recording (or LiveTV)
        bool recording = false;
        QMap<int, EncoderLink *>::Iterator it;
        for (it = m_tvList->begin(); (it != m_tvList->end()) &&
                 !recording; ++it)
        {
            if ((*it)->IsBusy())
                recording = true;
        }

        if (!(m_mainServer->isClientConnected()) && !recording)
        {
            // have we received a RESET_IDLETIME message?
            resetIdleTime_lock.lock();
            if (resetIdleTime)
            {
                // yes - so reset the idleSince time
                idleSince = QDateTime();
                resetIdleTime = false;
            }
            resetIdleTime_lock.unlock();

            if (statuschanged || !idleSince.isValid())
            {
                if (!idleSince.isValid()) 
                    idleSince = curtime;
    
                RecIter idleIter = reclist.begin();
                for ( ; idleIter != reclist.end(); ++idleIter)
                    if ((*idleIter)->GetRecordingStatus() ==
                        rsWillRecord)
                        break;

                if (idleIter != reclist.end())
                {
                    if (curtime.secsTo
                        ((*idleIter)->GetRecordingStartTime()) -
                        prerollseconds <
                        (idleWaitForRecordingTime * 60) +
                        idleTimeoutSecs)
                    {
                        idleSince = QDateTime();
                    }
                }
            }
            
            if (idleSince.isValid())
            {
                // is the machine already idling the timeout time?
                if (idleSince.addSecs(idleTimeoutSecs) < curtime)
                {
                    // are we waiting for shutdown?
                    if (m_isShuttingDown)
                    {
                        // if we have been waiting more that 60secs then assume
                        // something went wrong so reset and try again
                        if (idleSince.addSecs(idleTimeoutSecs + 60) <
                            curtime)
                        {
                            LOG(VB_GENERAL, LOG_WARNING,
                                "Waited more than 60"
                                " seconds for shutdown to complete"
                                " - resetting idle time");
                            idleSince = QDateTime();
                            m_isShuttingDown = false;
                        }
                    }
                    else if (!m_isShuttingDown &&
                             CheckShutdownServer(prerollseconds,
                                                 idleSince,
                                                 blockShutdown))
                    {
                        ShutdownServer(prerollseconds, idleSince);
                    }
                }
                else
                {
                    int itime = idleSince.secsTo(curtime);
                    QString msg;
                    if (itime == 1)
                    {
                        msg = QString("I\'m idle now... shutdown will "
                                      "occur in %1 seconds.")
                            .arg(idleTimeoutSecs);
                        LOG(VB_GENERAL, LOG_NOTICE, msg);
                        MythEvent me(QString("SHUTDOWN_COUNTDOWN %1")
                                     .arg(idleTimeoutSecs));
                        gCoreContext->dispatch(me);
                    }
                    else if (itime % 10 == 0)
                    {
                        msg = QString("%1 secs left to system shutdown!")
                            .arg(idleTimeoutSecs - itime);
                        LOG(VB_IDLE, LOG_NOTICE, msg);
                        MythEvent me(QString("SHUTDOWN_COUNTDOWN %1")
                                     .arg(idleTimeoutSecs - itime));
                        gCoreContext->dispatch(me);
                    }
                }
            }
        }
        else
        {
            // not idle, make the time invalid
            if (idleSince.isValid())
            {
                MythEvent me(QString("SHUTDOWN_COUNTDOWN -1"));
                gCoreContext->dispatch(me);
            }
            idleSince = QDateTime();
        }
    }
}

//returns true, if the shutdown is not blocked
bool Scheduler::CheckShutdownServer(int prerollseconds, QDateTime &idleSince,
                                    bool &blockShutdown)
{
    (void)prerollseconds;
    bool retval = false;
    QString preSDWUCheckCommand = gCoreContext->GetSetting("preSDWUCheckCommand",
                                                       "");
    if (!preSDWUCheckCommand.isEmpty())
    {
        uint state = myth_system(preSDWUCheckCommand);

        if (state != GENERIC_EXIT_NOT_OK)
        {
            retval = false;
            switch(state)
            {
                case 0:
                    LOG(VB_GENERAL, LOG_INFO,
                        "CheckShutdownServer returned - OK to shutdown");
                    retval = true;
                    break;
                case 1:
                    LOG(VB_IDLE, LOG_NOTICE,
                        "CheckShutdownServer returned - Not OK to shutdown");
                    // just reset idle'ing on retval == 1
                    idleSince = QDateTime();
                    break;
                case 2:
                    LOG(VB_IDLE, LOG_NOTICE,
                        "CheckShutdownServer returned - Not OK to shutdown, "
                        "need reconnect");
                    // reset shutdown status on retval = 2
                    // (needs a clientconnection again,
                    // before shutdown is executed)
                    blockShutdown =
                        gCoreContext->GetNumSetting("blockSDWUwithoutClient",
                                                    1);
                    idleSince = QDateTime();
                    break;
#if 0
                case 3:
                    //disable shutdown routine generally
                    m_noAutoShutdown = true;
                    break;
#endif
                default:
                    break;
            }
        }
    }
    else
        retval = true; // allow shutdown if now command is set.

    return retval;
}

void Scheduler::ShutdownServer(int prerollseconds, QDateTime &idleSince)
{
    m_isShuttingDown = true;

    RecIter recIter = reclist.begin();
    for ( ; recIter != reclist.end(); ++recIter)
        if ((*recIter)->GetRecordingStatus() == rsWillRecord)
            break;

    // set the wakeuptime if needed
    if (recIter != reclist.end())
    {
        RecordingInfo *nextRecording = (*recIter);
        QDateTime restarttime = nextRecording->GetRecordingStartTime()
            .addSecs((-1) * prerollseconds);

        int add = gCoreContext->GetNumSetting("StartupSecsBeforeRecording", 240);
        if (add)
            restarttime = restarttime.addSecs((-1) * add);

        QString wakeup_timeformat = gCoreContext->GetSetting("WakeupTimeFormat",
                                                         "hh:mm yyyy-MM-dd");
        QString setwakeup_cmd = gCoreContext->GetSetting("SetWakeuptimeCommand",
                                                     "echo \'Wakeuptime would "
                                                     "be $time if command "
                                                     "set.\'");

        if (setwakeup_cmd.isEmpty())
        {
            LOG(VB_GENERAL, LOG_NOTICE,
                "SetWakeuptimeCommand is empty, shutdown aborted");
            idleSince = QDateTime();
            m_isShuttingDown = false;
            return;
        }
        if (wakeup_timeformat == "time_t")
        {
            QString time_ts;
            setwakeup_cmd.replace("$time",
                                  time_ts.setNum(restarttime.toTime_t()));
        }
        else
            setwakeup_cmd.replace("$time",
                                  restarttime.toString(wakeup_timeformat));

        LOG(VB_GENERAL, LOG_NOTICE,
            QString("Running the command to set the next "
                    "scheduled wakeup time :-\n\t\t\t\t\t\t") + setwakeup_cmd);

        // now run the command to set the wakeup time
        if (myth_system(setwakeup_cmd) != GENERIC_EXIT_OK)
        {
            LOG(VB_GENERAL, LOG_ERR,
                "SetWakeuptimeCommand failed, shutdown aborted");
            idleSince = QDateTime();
            m_isShuttingDown = false;
            return;
        }
    }

    // tell anyone who is listening the master server is going down now
    MythEvent me(QString("SHUTDOWN_NOW"));
    gCoreContext->dispatch(me);

    QString halt_cmd = gCoreContext->GetSetting("ServerHaltCommand",
                                            "sudo /sbin/halt -p");

    if (!halt_cmd.isEmpty())
    {
        // now we shut the slave backends down...
        m_mainServer->ShutSlaveBackendsDown(halt_cmd);

        LOG(VB_GENERAL, LOG_NOTICE,
            QString("Running the command to shutdown "
                    "this computer :-\n\t\t\t\t\t\t") + halt_cmd);

        // and now shutdown myself
        schedLock.unlock();
        uint res = myth_system(halt_cmd);
        schedLock.lock();
        if (res == GENERIC_EXIT_OK)
            return;

        LOG(VB_GENERAL, LOG_ERR, "ServerHaltCommand failed, shutdown aborted");
    }

    // If we make it here then either the shutdown failed
    // OR we suspended or hibernated the OS instead
    idleSince = QDateTime();
    m_isShuttingDown = false;
}

void Scheduler::PutInactiveSlavesToSleep(void)
{
    int prerollseconds = 0;
    int secsleft = 0;
    EncoderLink *enc = NULL;

    bool someSlavesCanSleep = false;
    QMap<int, EncoderLink *>::Iterator enciter = m_tvList->begin();
    for (; enciter != m_tvList->end(); ++enciter)
    {
        EncoderLink *enc = *enciter;

        if (enc->CanSleep())
            someSlavesCanSleep = true;
    }

    if (!someSlavesCanSleep)
        return;

    LOG(VB_SCHEDULE, LOG_INFO,
        "Scheduler, Checking for slaves that can be shut down");

    int sleepThreshold =
        gCoreContext->GetNumSetting( "SleepThreshold", 60 * 45);

    LOG(VB_SCHEDULE, LOG_DEBUG,
        QString("  Getting list of slaves that will be active in the "
                "next %1 minutes.") .arg(sleepThreshold / 60));

    LOG(VB_SCHEDULE, LOG_DEBUG, "Checking scheduler's reclist");
    RecIter recIter = reclist.begin();
    QDateTime curtime = QDateTime::currentDateTime();
    QStringList SlavesInUse;
    for ( ; recIter != reclist.end(); ++recIter)
    {
        RecordingInfo *pginfo = *recIter;

        if ((pginfo->GetRecordingStatus() != rsRecording) &&
            (pginfo->GetRecordingStatus() != rsTuning) &&
            (pginfo->GetRecordingStatus() != rsWillRecord))
            continue;

        secsleft = curtime.secsTo(
            pginfo->GetRecordingStartTime()) - prerollseconds;
        if (secsleft > sleepThreshold)
            continue;

        if (m_tvList->find(pginfo->GetCardID()) != m_tvList->end())
        {
            enc = (*m_tvList)[pginfo->GetCardID()];
            if ((!enc->IsLocal()) &&
                (!SlavesInUse.contains(enc->GetHostName())))
            {
                if (pginfo->GetRecordingStatus() == rsWillRecord)
                    LOG(VB_SCHEDULE, LOG_DEBUG,
                        QString("    Slave %1 will be in use in %2 minutes")
                            .arg(enc->GetHostName()) .arg(secsleft / 60));
                else
                    LOG(VB_SCHEDULE, LOG_DEBUG,
                        QString("    Slave %1 is in use currently "
                                "recording '%1'")
                            .arg(enc->GetHostName()).arg(pginfo->GetTitle()));
                SlavesInUse << enc->GetHostName();
            }
        }
    }

    LOG(VB_SCHEDULE, LOG_DEBUG, "  Checking inuseprograms table:");
    QDateTime oneHourAgo = QDateTime::currentDateTime().addSecs(-61 * 60);
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT DISTINCT hostname, recusage FROM inuseprograms "
                    "WHERE lastupdatetime > :ONEHOURAGO ;");
    query.bindValue(":ONEHOURAGO", oneHourAgo);
    if (query.exec())
    {
        while(query.next()) {
            SlavesInUse << query.value(0).toString();
            LOG(VB_SCHEDULE, LOG_DEBUG,
                QString("    Slave %1 is marked as in use by a %2")
                    .arg(query.value(0).toString())
                    .arg(query.value(1).toString()));
        }
    }

    LOG(VB_SCHEDULE, LOG_DEBUG, QString("  Shutting down slaves which will "
        "be inactive for the next %1 minutes and can be put to sleep.")
            .arg(sleepThreshold / 60));

    enciter = m_tvList->begin();
    for (; enciter != m_tvList->end(); ++enciter)
    {
        enc = *enciter;

        if ((!enc->IsLocal()) &&
            (enc->IsAwake()) &&
            (!SlavesInUse.contains(enc->GetHostName())) &&
            (!enc->IsFallingAsleep()))
        {
            QString sleepCommand =
                gCoreContext->GetSettingOnHost("SleepCommand",
                                               enc->GetHostName());
            QString wakeUpCommand =
                gCoreContext->GetSettingOnHost("WakeUpCommand",
                                               enc->GetHostName());

            if (!sleepCommand.isEmpty() && !wakeUpCommand.isEmpty())
            {
                QString thisHost = enc->GetHostName();

                LOG(VB_SCHEDULE, LOG_DEBUG,
                    QString("    Commanding %1 to go to sleep.")
                        .arg(thisHost));

                if (enc->GoToSleep())
                {
                    QMap<int, EncoderLink *>::Iterator slviter =
                        m_tvList->begin();
                    for (; slviter != m_tvList->end(); ++slviter)
                    {
                        EncoderLink *slv = *slviter;
                        if (slv->GetHostName() == thisHost)
                        {
                            LOG(VB_SCHEDULE, LOG_DEBUG,
                                QString("    Marking card %1 on slave %2 "
                                        "as falling asleep.")
                                    .arg(slv->GetCardID())
                                    .arg(slv->GetHostName()));
                            slv->SetSleepStatus(sStatus_FallingAsleep);
                        }
                    }
                }
                else
                {
                    LOG(VB_GENERAL, LOG_ERR, LOC +
                        QString("Unable to shutdown %1 slave backend, setting "
                                "sleep status to undefined.").arg(thisHost));
                    QMap<int, EncoderLink *>::Iterator slviter =
                        m_tvList->begin();
                    for (; slviter != m_tvList->end(); ++slviter)
                    {
                        EncoderLink *slv = *slviter;
                        if (slv->GetHostName() == thisHost)
                            slv->SetSleepStatus(sStatus_Undefined);
                    }
                }
            }
        }
    }
}

bool Scheduler::WakeUpSlave(QString slaveHostname, bool setWakingStatus)
{
    if (slaveHostname == gCoreContext->GetHostName())
    {
        LOG(VB_GENERAL, LOG_NOTICE,
            QString("Tried to Wake Up %1, but this is the "
                    "master backend and it is not asleep.")
                .arg(slaveHostname));
        return false;
    }

    QString wakeUpCommand = gCoreContext->GetSettingOnHost( "WakeUpCommand",
        slaveHostname);

    if (wakeUpCommand.isEmpty()) {
        LOG(VB_GENERAL, LOG_NOTICE,
            QString("Trying to Wake Up %1, but this slave "
                    "does not have a WakeUpCommand set.").arg(slaveHostname));

        QMap<int, EncoderLink *>::Iterator enciter = m_tvList->begin();
        for (; enciter != m_tvList->end(); ++enciter)
        {
            EncoderLink *enc = *enciter;
            if (enc->GetHostName() == slaveHostname)
                enc->SetSleepStatus(sStatus_Undefined);
        }

        return false;
    }

    QDateTime curtime = QDateTime::currentDateTime();
    QMap<int, EncoderLink *>::Iterator enciter = m_tvList->begin();
    for (; enciter != m_tvList->end(); ++enciter)
    {
        EncoderLink *enc = *enciter;
        if (setWakingStatus && (enc->GetHostName() == slaveHostname))
            enc->SetSleepStatus(sStatus_Waking);
        enc->SetLastWakeTime(curtime);
    }

    if (!IsMACAddress(wakeUpCommand))
    {
        LOG(VB_SCHEDULE, LOG_NOTICE, QString("Executing '%1' to wake up slave.")
                .arg(wakeUpCommand));
        myth_system(wakeUpCommand);
        return true;
    }

    return WakeOnLAN(wakeUpCommand);
}

void Scheduler::WakeUpSlaves(void)
{
    QStringList SlavesThatCanWake;
    QString thisSlave;
    QMap<int, EncoderLink *>::Iterator enciter = m_tvList->begin();
    for (; enciter != m_tvList->end(); ++enciter)
    {
        EncoderLink *enc = *enciter;

        if (enc->IsLocal())
            continue;

        thisSlave = enc->GetHostName();

        if ((!gCoreContext->GetSettingOnHost("WakeUpCommand", thisSlave)
                .isEmpty()) &&
            (!SlavesThatCanWake.contains(thisSlave)))
            SlavesThatCanWake << thisSlave;
    }

    int slave = 0;
    for (; slave < SlavesThatCanWake.count(); slave++)
    {
        thisSlave = SlavesThatCanWake[slave];
        LOG(VB_SCHEDULE, LOG_NOTICE,
            QString("Scheduler, Sending wakeup command to slave: %1")
                .arg(thisSlave));
        WakeUpSlave(thisSlave, false);
    }
}

void Scheduler::UpdateManuals(int recordid)
{
    MSqlQuery query(dbConn);

    query.prepare(QString("SELECT type,title,station,startdate,starttime, "
                  " enddate,endtime "
                  "FROM %1 WHERE recordid = :RECORDID").arg(recordTable));
    query.bindValue(":RECORDID", recordid);
    if (!query.exec() || query.size() != 1)
    {
        MythDB::DBError("UpdateManuals", query);
        return;
    }

    query.next();
    RecordingType rectype = RecordingType(query.value(0).toInt());
    QString title = query.value(1).toString();
    QString station = query.value(2).toString() ;
    QDateTime startdt = QDateTime(query.value(3).toDate(),
                                  query.value(4).toTime());
    int duration = startdt.secsTo(QDateTime(query.value(5).toDate(),
                                            query.value(6).toTime())) / 60;

    query.prepare("SELECT chanid from channel "
                  "WHERE callsign = :STATION");
    query.bindValue(":STATION", station);
    if (!query.exec())
    {
        MythDB::DBError("UpdateManuals", query);
        return;
    }

    vector<uint> chanidlist;
    while (query.next())
        chanidlist.push_back(query.value(0).toUInt());

    int progcount;
    int skipdays;
    bool weekday;
    int weeksoff;

    switch (rectype)
    {
    case kSingleRecord:
    case kOverrideRecord:
    case kDontRecord:
        progcount = 1;
        skipdays = 1;
        weekday = false;
        break;
    case kTimeslotRecord:
        progcount = 13;
        skipdays = 1;
        if (startdt.date().dayOfWeek() < 6)
            weekday = true;
        else
            weekday = false;
        startdt.setDate(QDate::currentDate());
        break;
    case kWeekslotRecord:
        progcount = 2;
        skipdays = 7;
        weekday = false;
        weeksoff = (startdt.date().daysTo(QDate::currentDate()) + 6) / 7;
        startdt = startdt.addDays(weeksoff * 7);
        break;
    default:
        LOG(VB_GENERAL, LOG_ERR,
            QString("Invalid rectype for manual recordid %1").arg(recordid));
        return;
    }

    while (progcount--)
    {
        for (int i = 0; i < (int)chanidlist.size(); i++)
        {
            if (weekday && startdt.date().dayOfWeek() >= 6)
                continue;

            query.prepare("REPLACE INTO program (chanid,starttime,endtime,"
                          " title,subtitle,manualid) "
                          "VALUES (:CHANID,:STARTTIME,:ENDTIME,:TITLE,"
                          " :SUBTITLE,:RECORDID)");
            query.bindValue(":CHANID", chanidlist[i]);
            query.bindValue(":STARTTIME", startdt);
            query.bindValue(":ENDTIME", startdt.addSecs(duration * 60));
            query.bindValue(":TITLE", title);
            query.bindValue(":SUBTITLE", startdt.toString());
            query.bindValue(":RECORDID", recordid);
            if (!query.exec())
            {
                MythDB::DBError("UpdateManuals", query);
                return;
            }
        }
        startdt = startdt.addDays(skipdays);
    }
}

void Scheduler::BuildNewRecordsQueries(int recordid, QStringList &from,
                                       QStringList &where,
                                       MSqlBindings &bindings)
{
    MSqlQuery result(dbConn);
    QString query;
    QString qphrase;

    query = QString("SELECT recordid,search,subtitle,description "
                    "FROM %1 WHERE search <> %2 AND "
                    "(recordid = %3 OR %4 = -1) ")
        .arg(recordTable).arg(kNoSearch).arg(recordid).arg(recordid);

    result.prepare(query);

    if (!result.exec() || !result.isActive())
    {
        MythDB::DBError("BuildNewRecordsQueries", result);
        return;
    }

    int count = 0;
    while (result.next())
    {
        QString prefix = QString(":NR%1").arg(count);
        qphrase = result.value(3).toString();

        RecSearchType searchtype = RecSearchType(result.value(1).toInt());

        if (qphrase.isEmpty() && searchtype != kManualSearch)
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("Invalid search key in recordid %1")
                    .arg(result.value(0).toString()));
            continue;
        }

        QString bindrecid = prefix + "RECID";
        QString bindphrase = prefix + "PHRASE";
        QString bindlikephrase1 = prefix + "LIKEPHRASE1";
        QString bindlikephrase2 = prefix + "LIKEPHRASE2";
        QString bindlikephrase3 = prefix + "LIKEPHRASE3";

        bindings[bindrecid] = result.value(0).toString();

        switch (searchtype)
        {
        case kPowerSearch:
            qphrase.remove(QRegExp("^\\s*AND\\s+", Qt::CaseInsensitive));
            qphrase.remove(';');
            from << result.value(2).toString();
            where << (QString("%1.recordid = ").arg(recordTable) + bindrecid +
                      QString(" AND program.manualid = 0 AND ( %2 )")
                      .arg(qphrase));
            break;
        case kTitleSearch:
            bindings[bindlikephrase1] = QString(QString("%") + qphrase + "%");
            from << "";
            where << (QString("%1.recordid = ").arg(recordTable) + bindrecid + " AND "
                      "program.manualid = 0 AND "
                      "program.title LIKE " + bindlikephrase1);
            break;
        case kKeywordSearch:
            bindings[bindlikephrase1] = QString(QString("%") + qphrase + "%");
            bindings[bindlikephrase2] = QString(QString("%") + qphrase + "%");
            bindings[bindlikephrase3] = QString(QString("%") + qphrase + "%");
            from << "";
            where << (QString("%1.recordid = ").arg(recordTable) + bindrecid +
                      " AND program.manualid = 0"
                      " AND (program.title LIKE " + bindlikephrase1 +
                      " OR program.subtitle LIKE " + bindlikephrase2 +
                      " OR program.description LIKE " + bindlikephrase3 + ")");
            break;
        case kPeopleSearch:
            bindings[bindphrase] = qphrase;
            from << ", people, credits";
            where << (QString("%1.recordid = ").arg(recordTable) + bindrecid + " AND "
                      "program.manualid = 0 AND "
                      "people.name LIKE " + bindphrase + " AND "
                      "credits.person = people.person AND "
                      "program.chanid = credits.chanid AND "
                      "program.starttime = credits.starttime");
            break;
        case kManualSearch:
            UpdateManuals(result.value(0).toInt());
            from << "";
            where << (QString("%1.recordid = ").arg(recordTable) + bindrecid +
                      " AND " +
                      QString("program.manualid = %1.recordid ")
                          .arg(recordTable));
            break;
        default:
            LOG(VB_GENERAL, LOG_ERR,
                QString("Unknown RecSearchType (%1) for recordid %2")
                    .arg(result.value(1).toInt())
                    .arg(result.value(0).toString()));
            bindings.remove(bindrecid);
            break;
        }

        count++;
    }

    if (recordid == -1 || from.count() == 0)
    {
        QString recidmatch = "";
        if (recordid != -1)
            recidmatch = "RECTABLE.recordid = :NRRECORDID AND ";
        QString s = recidmatch +
            "RECTABLE.search = :NRST AND "
            "program.manualid = 0 AND "
            "program.title = RECTABLE.title ";

        while (1)
        {
            int i = s.indexOf("RECTABLE");
            if (i == -1) break;
            s = s.replace(i, strlen("RECTABLE"), recordTable);
        }

        from << "";
        where << s;
        bindings[":NRST"] = kNoSearch;
        if (recordid != -1)
            bindings[":NRRECORDID"] = recordid;
    }
}

void Scheduler::UpdateMatches(int recordid) {
    struct timeval dbstart, dbend;

    if (recordid == 0)
        return;

    MSqlQuery query(dbConn);

    if (recordid == -1)
        query.prepare("DELETE FROM recordmatch");
    else
    {
        query.prepare("DELETE FROM recordmatch WHERE recordid = :RECORDID");
        query.bindValue(":RECORDID", recordid);
    }

    if (!query.exec())
    {
        MythDB::DBError("UpdateMatches", query);
        return;
    }

    if (recordid == -1)
        query.prepare("DELETE FROM program WHERE manualid <> 0");
    else
    {
        query.prepare("DELETE FROM program WHERE manualid = :RECORDID");
        query.bindValue(":RECORDID", recordid);
    }
    if (!query.exec())
    {
        MythDB::DBError("UpdateMatches", query);
        return;
    }

    QString filterClause;
    query.prepare("SELECT filterid, clause FROM recordfilter "
                  "WHERE filterid >= 0 AND filterid < :NUMFILTERS AND "
                  "      TRIM(clause) <> ''");
    query.bindValue(":NUMFILTERS", RecordingRule::kNumFilters);
    if (!query.exec())
    {
        MythDB::DBError("UpdateMatches", query);
        return;
    }
    while (query.next())
    {
        filterClause += QString(" AND (((RECTABLE.filter & %1) = 0) OR (%2))")
            .arg(1 << query.value(0).toInt()).arg(query.value(1).toString());
    }

    // Make sure all FindOne rules have a valid findid before scheduling.
    query.prepare("SELECT NULL from record "
                  "WHERE type = :FINDONE AND findid <= 0;");
    query.bindValue(":FINDONE", kFindOneRecord);
    if (!query.exec())
    {
        MythDB::DBError("UpdateMatches", query);
        return;
    }
    else if (query.size())
    {
        QDate epoch(1970, 1, 1);
        int findtoday =  epoch.daysTo(QDate::currentDate()) + 719528;
        query.prepare("UPDATE record set findid = :FINDID "
                      "WHERE type = :FINDONE AND findid <= 0;");
        query.bindValue(":FINDID", findtoday);
        query.bindValue(":FINDONE", kFindOneRecord);
        if (!query.exec())
            MythDB::DBError("UpdateMatches", query);
    }

    int clause;
    QStringList fromclauses, whereclauses;
    MSqlBindings bindings;

    BuildNewRecordsQueries(recordid, fromclauses, whereclauses, bindings);

    if (VERBOSE_LEVEL_CHECK(VB_SCHEDULE, LOG_INFO))
    {
        for (clause = 0; clause < fromclauses.count(); ++clause)
        {
            LOG(VB_SCHEDULE, LOG_INFO, QString("Query %1: %2/%3")
                .arg(clause).arg(fromclauses[clause])
                .arg(whereclauses[clause]));
        }
    }

    for (clause = 0; clause < fromclauses.count(); ++clause)
    {
        QString query = QString(
"INSERT INTO recordmatch (recordid, chanid, starttime, manualid) "
"SELECT RECTABLE.recordid, program.chanid, program.starttime, "
" IF(search = %1, RECTABLE.recordid, 0) ").arg(kManualSearch) + QString(
"FROM (RECTABLE, program INNER JOIN channel "
"      ON channel.chanid = program.chanid) ") + fromclauses[clause] + QString(
" WHERE ") + whereclauses[clause] +
    QString(" AND (NOT ((RECTABLE.dupin & %1) AND program.previouslyshown)) "
            " AND (NOT ((RECTABLE.dupin & %2) AND program.generic > 0)) "
            " AND (NOT ((RECTABLE.dupin & %3) AND (program.previouslyshown "
            "                                      OR program.first = 0))) ")
            .arg(kDupsExRepeats).arg(kDupsExGeneric).arg(kDupsFirstNew) +
    QString(" AND channel.visible = 1 ") +
    filterClause + QString(" AND "

"((RECTABLE.type = %1 " // allrecord
"OR RECTABLE.type = %2 " // findonerecord
"OR RECTABLE.type = %3 " // finddailyrecord
"OR RECTABLE.type = %4) " // findweeklyrecord
" OR "
" ((RECTABLE.station = channel.callsign) " // channel matches
"  AND "
"  ((RECTABLE.type = %5) " // channelrecord
"   OR"
"   ((TIME_TO_SEC(RECTABLE.starttime) = TIME_TO_SEC(program.starttime)) " // timeslot matches
"    AND "
"    ((RECTABLE.type = %6) " // timeslotrecord
"     OR"
"     ((DAYOFWEEK(RECTABLE.startdate) = DAYOFWEEK(program.starttime) "
"      AND "
"      ((RECTABLE.type = %7) " // weekslotrecord
"       OR"
"       ((TO_DAYS(RECTABLE.startdate) = TO_DAYS(program.starttime)) " // date matches
"        AND (RECTABLE.type <> %8)" // single,override,don't,etc.
"        )"
"       )"
"      )"
"     )"
"    )"
"   )"
"  )"
" )"
") ")
            .arg(kAllRecord)
            .arg(kFindOneRecord)
            .arg(kFindDailyRecord)
            .arg(kFindWeeklyRecord)
            .arg(kChannelRecord)
            .arg(kTimeslotRecord)
            .arg(kWeekslotRecord)
            .arg(kNotRecording);

        while (1)
        {
            int i = query.indexOf("RECTABLE");
            if (i == -1) break;
            query = query.replace(i, strlen("RECTABLE"), recordTable);
        }

        LOG(VB_SCHEDULE, LOG_INFO, QString(" |-- Start DB Query %1...")
                .arg(clause));

        gettimeofday(&dbstart, NULL);
        MSqlQuery result(dbConn);
        result.prepare(query);

        MSqlBindings::const_iterator it;
        for (it = bindings.begin(); it != bindings.end(); ++it)
        {
            if (query.contains(it.key()))
                result.bindValue(it.key(), it.value());
        }

        bool ok = result.exec();
        gettimeofday(&dbend, NULL);

        if (!ok)
        {
            MythDB::DBError("UpdateMatches3", result);
            continue;
        }

        LOG(VB_SCHEDULE, LOG_INFO, QString(" |-- %1 results in %2 sec.")
                .arg(result.size())
                .arg(((dbend.tv_sec  - dbstart.tv_sec) * 1000000 +
                      (dbend.tv_usec - dbstart.tv_usec)) / 1000000.0));

    }

    LOG(VB_SCHEDULE, LOG_INFO, " +-- Done.");
}

void Scheduler::AddNewRecords(void)
{
    struct timeval dbstart, dbend;

    QMap<RecordingType, int> recTypeRecPriorityMap;
    RecList tmpList;

    QMap<int, bool> cardMap;
    QMap<int, EncoderLink *>::Iterator enciter = m_tvList->begin();
    for (; enciter != m_tvList->end(); ++enciter)
    {
        EncoderLink *enc = *enciter;
        if (enc->IsConnected() || enc->IsAsleep())
            cardMap[enc->GetCardID()] = true;
    }

    QMap<int, bool> tooManyMap;
    bool checkTooMany = false;
    schedAfterStartMap.clear();

    MSqlQuery rlist(dbConn);
    rlist.prepare(QString("SELECT recordid,title,maxepisodes,maxnewest FROM %1;").arg(recordTable));

    if (!rlist.exec())
    {
        MythDB::DBError("CheckTooMany", rlist);
        return;
    }

    while (rlist.next())
    {
        int recid = rlist.value(0).toInt();
        QString qtitle = rlist.value(1).toString();
        int maxEpisodes = rlist.value(2).toInt();
        int maxNewest = rlist.value(3).toInt();

        tooManyMap[recid] = false;
        schedAfterStartMap[recid] = false;

        if (maxEpisodes && !maxNewest)
        {
            MSqlQuery epicnt(dbConn);

            epicnt.prepare("SELECT DISTINCT chanid, progstart, progend "
                           "FROM recorded "
                           "WHERE recordid = :RECID AND preserve = 0 "
                               "AND recgroup NOT IN ('LiveTV','Deleted');");
            epicnt.bindValue(":RECID", recid);

            if (epicnt.exec())
            {
                if (epicnt.size() >= maxEpisodes - 1)
                {
                    schedAfterStartMap[recid] = true;
                    if (epicnt.size() >= maxEpisodes)
                    {
                        tooManyMap[recid] = true;
                        checkTooMany = true;
                    }
                }
            }
        }
    }

    prefinputpri        = gCoreContext->GetNumSetting("PrefInputPriority", 2);
    int hdtvpriority    = gCoreContext->GetNumSetting("HDTVRecPriority", 0);
    int wspriority      = gCoreContext->GetNumSetting("WSRecPriority", 0);
    int autopriority    = gCoreContext->GetNumSetting("AutoRecPriority", 0);
    int slpriority      = gCoreContext->GetNumSetting("SignLangRecPriority", 0);
    int onscrpriority   = gCoreContext->GetNumSetting("OnScrSubRecPriority", 0);
    int ccpriority      = gCoreContext->GetNumSetting("CCRecPriority", 0);
    int hhpriority      = gCoreContext->GetNumSetting("HardHearRecPriority", 0);
    int adpriority      = gCoreContext->GetNumSetting("AudioDescRecPriority", 0);

    int autostrata = autopriority * 2 + 1;

    QString pwrpri = "channel.recpriority + cardinput.recpriority";

    if (prefinputpri)
        pwrpri += QString(" + "
        "(cardinput.cardinputid = RECTABLE.prefinput) * %1").arg(prefinputpri);

    if (hdtvpriority)
        pwrpri += QString(" + (program.hdtv > 0 OR "
        "FIND_IN_SET('HDTV', program.videoprop) > 0) * %1").arg(hdtvpriority);

    if (wspriority)
        pwrpri += QString(" + "
        "(FIND_IN_SET('WIDESCREEN', program.videoprop) > 0) * %1").arg(wspriority);

    if (slpriority)
        pwrpri += QString(" + "
        "(FIND_IN_SET('SIGNED', program.subtitletypes) > 0) * %1").arg(slpriority);

    if (onscrpriority)
        pwrpri += QString(" + "
        "(FIND_IN_SET('ONSCREEN', program.subtitletypes) > 0) * %1").arg(onscrpriority);

    if (ccpriority)
        pwrpri += QString(" + "
        "(FIND_IN_SET('NORMAL', program.subtitletypes) > 0 OR "
        "program.closecaptioned > 0 OR program.subtitled > 0) * %1").arg(ccpriority);

    if (hhpriority)
        pwrpri += QString(" + "
        "(FIND_IN_SET('HARDHEAR', program.subtitletypes) > 0 OR "
        "FIND_IN_SET('HARDHEAR', program.audioprop) > 0) * %1").arg(hhpriority);

    if (adpriority)
        pwrpri += QString(" + "
        "(FIND_IN_SET('VISUALIMPAIR', program.audioprop) > 0) * %1").arg(adpriority);

    QString schedTmpRecord = recordTable;

    MSqlQuery result(dbConn);

    if (schedTmpRecord == "record")
    {
        schedTmpRecord = "sched_temp_record";

        result.prepare("DROP TABLE IF EXISTS sched_temp_record;");

        if (!result.exec())
        {
            MythDB::DBError("Dropping sched_temp_record table", result);
            return;
        }

        result.prepare("CREATE TEMPORARY TABLE sched_temp_record "
                           "LIKE record;");

        if (!result.exec())
        {
            MythDB::DBError("Creating sched_temp_record table",
                                 result);
            return;
        }

        result.prepare("INSERT sched_temp_record SELECT * from record;");

        if (!result.exec())
        {
            MythDB::DBError("Populating sched_temp_record table",
                                 result);
            return;
        }
    }

    result.prepare("DROP TABLE IF EXISTS sched_temp_recorded;");

    if (!result.exec())
    {
        MythDB::DBError("Dropping sched_temp_recorded table", result);
        return;
    }

    result.prepare("CREATE TEMPORARY TABLE sched_temp_recorded "
                       "LIKE recorded;");

    if (!result.exec())
    {
        MythDB::DBError("Creating sched_temp_recorded table", result);
        return;
    }

    result.prepare("INSERT sched_temp_recorded SELECT * from recorded;");

    if (!result.exec())
    {
        MythDB::DBError("Populating sched_temp_recorded table", result);
        return;
    }

    result.prepare(QString("SELECT recpriority, selectclause FROM %1;")
                           .arg(priorityTable));

    if (!result.exec())
    {
        MythDB::DBError("Power Priority", result);
        return;
    }

    while (result.next())
    {
        if (result.value(0).toInt())
        {
            QString sclause = result.value(1).toString();
            sclause.remove(QRegExp("^\\s*AND\\s+", Qt::CaseInsensitive));
            sclause.remove(';');
            pwrpri += QString(" + (%1) * %2").arg(sclause)
                                             .arg(result.value(0).toInt());
        }
    }
    pwrpri += QString(" AS powerpriority ");

    QString progfindid = QString(
"(CASE RECTABLE.type "
"  WHEN %1 "
"   THEN RECTABLE.findid "
"  WHEN %2 "
"   THEN to_days(date_sub(program.starttime, interval "
"                time_format(RECTABLE.findtime, '%H:%i') hour_minute)) "
"  WHEN %3 "
"   THEN floor((to_days(date_sub(program.starttime, interval "
"               time_format(RECTABLE.findtime, '%H:%i') hour_minute)) - "
"               RECTABLE.findday)/7) * 7 + RECTABLE.findday "
"  WHEN %4 "
"   THEN RECTABLE.findid "
"  ELSE 0 "
" END) ")
        .arg(kFindOneRecord)
        .arg(kFindDailyRecord)
        .arg(kFindWeeklyRecord)
        .arg(kOverrideRecord);

    QString rmquery = QString(
"UPDATE recordmatch "
" INNER JOIN RECTABLE ON (recordmatch.recordid = RECTABLE.recordid) "
" INNER JOIN program ON (recordmatch.chanid = program.chanid AND "
"                        recordmatch.starttime = program.starttime AND "
"                        recordmatch.manualid = program.manualid) "
" LEFT JOIN oldrecorded ON "
"  ( "
"    RECTABLE.dupmethod > 1 AND "
"    oldrecorded.duplicate <> 0 AND "
"    program.title = oldrecorded.title "
"     AND "
"     ( "
"      (program.programid <> '' AND program.generic = 0 "
"       AND program.programid = oldrecorded.programid) "
"      OR "
"      (oldrecorded.findid <> 0 AND "
"        oldrecorded.findid = ") + progfindid + QString(") "
"      OR "
"      ( "
"       program.generic = 0 "
"       AND "
"       (program.programid = '' OR oldrecorded.programid = '') "
"       AND "
"       (((RECTABLE.dupmethod & 0x02) = 0) OR (program.subtitle <> '' "
"          AND program.subtitle = oldrecorded.subtitle)) "
"       AND "
"       (((RECTABLE.dupmethod & 0x04) = 0) OR (program.description <> '' "
"          AND program.description = oldrecorded.description)) "
"       AND "
"       (((RECTABLE.dupmethod & 0x08) = 0) OR "
"          (program.subtitle <> '' AND "
"             (program.subtitle = oldrecorded.subtitle OR "
"              (oldrecorded.subtitle = '' AND "
"               program.subtitle = oldrecorded.description))) OR "
"          (program.subtitle = '' AND program.description <> '' AND "
"             (program.description = oldrecorded.subtitle OR "
"              (oldrecorded.subtitle = '' AND "
"               program.description = oldrecorded.description)))) "
"      ) "
"     ) "
"  ) "
" LEFT JOIN sched_temp_recorded recorded ON "
"  ( "
"    RECTABLE.dupmethod > 1 AND "
"    recorded.duplicate <> 0 AND "
"    program.title = recorded.title AND "
"    recorded.recgroup NOT IN ('LiveTV','Deleted') "
"     AND "
"     ( "
"      (program.programid <> '' AND program.generic = 0 "
"       AND program.programid = recorded.programid) "
"      OR "
"      (recorded.findid <> 0 AND "
"        recorded.findid = ") + progfindid + QString(") "
"      OR "
"      ( "
"       program.generic = 0 "
"       AND "
"       (program.programid = '' OR recorded.programid = '') "
"       AND "
"       (((RECTABLE.dupmethod & 0x02) = 0) OR (program.subtitle <> '' "
"          AND program.subtitle = recorded.subtitle)) "
"       AND "
"       (((RECTABLE.dupmethod & 0x04) = 0) OR (program.description <> '' "
"          AND program.description = recorded.description)) "
"       AND "
"       (((RECTABLE.dupmethod & 0x08) = 0) OR "
"          (program.subtitle <> '' AND "
"             (program.subtitle = recorded.subtitle OR "
"              (recorded.subtitle = '' AND "
"               program.subtitle = recorded.description))) OR "
"          (program.subtitle = '' AND program.description <> '' AND "
"             (program.description = recorded.subtitle OR "
"              (recorded.subtitle = '' AND "
"               program.description = recorded.description)))) "
"      ) "
"     ) "
"  ) "
" LEFT JOIN oldfind ON "
"  (oldfind.recordid = recordmatch.recordid AND "
"   oldfind.findid = ") + progfindid + QString(") "
"  SET oldrecduplicate = (oldrecorded.endtime IS NOT NULL), "
"      recduplicate = (recorded.endtime IS NOT NULL), "
"      findduplicate = (oldfind.findid IS NOT NULL), "
"      oldrecstatus = oldrecorded.recstatus "
" WHERE program.endtime >= NOW() - INTERVAL 9 HOUR "
);
    rmquery.replace("RECTABLE", schedTmpRecord);

    pwrpri.replace("program.","p.");
    pwrpri.replace("channel.","c.");
    progfindid.replace("program.","p.");
    progfindid.replace("channel.","c.");
    QString query = QString(
        "SELECT "
        "    c.chanid,         c.sourceid,           p.starttime,       "// 0-2
        "    p.endtime,        p.title,              p.subtitle,        "// 3-5
        "    p.description,    c.channum,            c.callsign,        "// 6-8
        "    c.name,           oldrecduplicate,      p.category,        "// 9-11
        "    RECTABLE.recpriority, RECTABLE.dupin,   recduplicate,      "//12-14
        "    findduplicate,    RECTABLE.type,        RECTABLE.recordid, "//15-17
        "    p.starttime - INTERVAL RECTABLE.startoffset "
        "    minute AS recstartts, "                                     //18
        "    p.endtime + INTERVAL RECTABLE.endoffset "
        "    minute AS recendts, "                                       //19
        "                                            p.previouslyshown, "//20
        "    RECTABLE.recgroup, RECTABLE.dupmethod,  c.commmethod,      "//21-23
        "    capturecard.cardid, cardinput.cardinputid,p.seriesid,      "//24-26
        "    p.programid,       RECTABLE.inetref,    p.category_type,   "//27-29
        "    p.airdate,         p.stars,             p.originalairdate, "//30-32
        "    RECTABLE.inactive, RECTABLE.parentid,") + progfindid + ",  "//33-35
        "    RECTABLE.playgroup, oldrecstatus.recstatus, "//36-37
        "    oldrecstatus.reactivate, p.videoprop+0,     "//38-39
        "    p.subtitletypes+0, p.audioprop+0,   RECTABLE.storagegroup, "//40-42
        "    capturecard.hostname, recordmatch.oldrecstatus, "
        "                                           RECTABLE.avg_delay, "//43-45
        "    oldrecstatus.future, "                                      //46
        + pwrpri + QString(
        "FROM recordmatch "
        "INNER JOIN RECTABLE ON (recordmatch.recordid = RECTABLE.recordid) "
        "INNER JOIN program AS p "
        "ON ( recordmatch.chanid    = p.chanid    AND "
        "     recordmatch.starttime = p.starttime AND "
        "     recordmatch.manualid  = p.manualid ) "
        "INNER JOIN channel AS c "
        "ON ( c.chanid = p.chanid ) "
        "INNER JOIN cardinput ON (c.sourceid = cardinput.sourceid) "
        "INNER JOIN capturecard ON (capturecard.cardid = cardinput.cardid) "
        "LEFT JOIN oldrecorded as oldrecstatus "
        "ON ( oldrecstatus.station   = c.callsign  AND "
        "     oldrecstatus.starttime = p.starttime AND "
        "     oldrecstatus.title     = p.title ) "
        "WHERE p.endtime >= NOW() - INTERVAL 1 DAY "
        "ORDER BY RECTABLE.recordid DESC ");
    query.replace("RECTABLE", schedTmpRecord);

    LOG(VB_SCHEDULE, LOG_INFO, QString(" |-- Start DB Query..."));

    gettimeofday(&dbstart, NULL);
    result.prepare(rmquery);
    if (!result.exec())
    {
        MythDB::DBError("AddNewRecords recordmatch", result);
        return;
    }
    result.prepare(query);
    if (!result.exec())
    {
        MythDB::DBError("AddNewRecords", result);
        return;
    }
    gettimeofday(&dbend, NULL);

    LOG(VB_SCHEDULE, LOG_INFO,
        QString(" |-- %1 results in %2 sec. Processing...")
            .arg(result.size())
            .arg(((dbend.tv_sec  - dbstart.tv_sec) * 1000000 +
                  (dbend.tv_usec - dbstart.tv_usec)) / 1000000.0));

    while (result.next())
    {
        RecordingInfo *p = new RecordingInfo(
            result.value(4).toString(),//title
            result.value(5).toString(),//subtitle
            result.value(6).toString(),//description
            0, // season
            0, // episode
            result.value(11).toString(),//category

            result.value(0).toUInt(),//chanid
            result.value(7).toString(),//channum
            result.value(8).toString(),//chansign
            result.value(9).toString(),//channame

            result.value(21).toString(),//recgroup
            result.value(36).toString(),//playgroup

            result.value(43).toString(),//hostname
            result.value(42).toString(),//storagegroup

            result.value(30).toUInt(),//year

            result.value(26).toString(),//seriesid
            result.value(27).toString(),//programid
            result.value(28).toString(),//inetref
            result.value(29).toString(),//catType

            result.value(12).toInt(),//recpriority

            result.value(2).toDateTime(),//startts
            result.value(3).toDateTime(),//endts
            result.value(18).toDateTime(),//recstartts
            result.value(19).toDateTime(),//recendts

            result.value(31).toDouble(),//stars
            (result.value(32).isNull()) ? QDate() :
            QDate::fromString(result.value(32).toString(), Qt::ISODate),
            //originalAirDate

            result.value(20).toInt(),//repeat

            RecStatusType(result.value(37).toInt()),//oldrecstatus
            result.value(38).toInt(),//reactivate

            result.value(17).toUInt(),//recordid
            result.value(34).toUInt(),//parentid
            RecordingType(result.value(16).toInt()),//rectype
            RecordingDupInType(result.value(13).toInt()),//dupin
            RecordingDupMethodType(result.value(22).toInt()),//dupmethod

            result.value(1).toUInt(),//sourceid
            result.value(25).toUInt(),//inputid
            result.value(24).toUInt(),//cardid

            result.value(35).toUInt(),//findid

            result.value(23).toInt() == COMM_DETECT_COMMFREE,//commfree
            result.value(40).toUInt(),//subtitleType
            result.value(39).toUInt(),//videoproperties
            result.value(41).toUInt(),//audioproperties
            result.value(46).toInt());//future

        if (!p->future && !p->IsReactivated() &&
            p->oldrecstatus != rsAborted &&
            p->oldrecstatus != rsNotListed)
        {
            p->SetRecordingStatus(p->oldrecstatus);
        }

        if (!recTypeRecPriorityMap.contains(p->GetRecordingRuleType()))
        {
            recTypeRecPriorityMap[p->GetRecordingRuleType()] =
                p->GetRecordingTypeRecPriority(p->GetRecordingRuleType());
        }

        p->SetRecordingPriority(
            p->GetRecordingPriority() + recTypeRecPriorityMap[p->GetRecordingRuleType()] +
            result.value(47).toInt() +
            ((autopriority) ?
             autopriority - (result.value(45).toInt() * autostrata / 200) : 0));

        // Check to see if the program is currently recording and if
        // the end time was changed.  Ideally, checking for a new end
        // time should be done after PruneOverlaps, but that would
        // complicate the list handling.  Do it here unless it becomes
        // problematic.
        RecIter rec = worklist.begin();
        for ( ; rec != worklist.end(); ++rec)
        {
            RecordingInfo *r = *rec;
            if (p->IsSameTimeslot(*r))
            {
                if (r->GetInputID() == p->GetInputID() &&
                    r->GetRecordingEndTime() != p->GetRecordingEndTime() &&
                    (r->GetRecordingRuleID() == p->GetRecordingRuleID() ||
                     p->GetRecordingRuleType() == kOverrideRecord))
                    ChangeRecordingEnd(r, p);
                delete p;
                p = NULL;
                break;
            }
        }
        if (p == NULL)
            continue;

        RecStatusType newrecstatus = p->GetRecordingStatus();
        // Check for rsOffLine
        if ((doRun || specsched) && !cardMap.contains(p->GetCardID()))
            newrecstatus = rsOffLine;

        // Check for rsTooManyRecordings
        if (checkTooMany && tooManyMap[p->GetRecordingRuleID()] &&
            !p->IsReactivated())
        {
            newrecstatus = rsTooManyRecordings;
        }

        // Check for rsCurrentRecording and rsPreviousRecording
        if (p->GetRecordingRuleType() == kDontRecord)
            newrecstatus = rsDontRecord;
        else if (result.value(15).toInt() && !p->IsReactivated())
            newrecstatus = rsPreviousRecording;
        else if (p->GetRecordingRuleType() != kSingleRecord &&
                 p->GetRecordingRuleType() != kOverrideRecord &&
                 !p->IsReactivated() &&
                 !(p->GetDuplicateCheckMethod() & kDupCheckNone))
        {
            const RecordingDupInType dupin = p->GetDuplicateCheckSource();

            if ((dupin & kDupsNewEpi) && p->IsRepeat())
                newrecstatus = rsRepeat;

            if ((dupin & kDupsInOldRecorded) && result.value(10).toInt())
            {
                if (result.value(44).toInt() == rsNeverRecord)
                    newrecstatus = rsNeverRecord;
                else
                    newrecstatus = rsPreviousRecording;
            }

            if ((dupin & kDupsInRecorded) && result.value(14).toInt())
                newrecstatus = rsCurrentRecording;
        }

        bool inactive = result.value(33).toInt();
        if (inactive)
            newrecstatus = rsInactive;

        // Mark anything that has already passed as some type of
        // missed.  If it survives PruneOverlaps, it will get deleted
        // or have its old status restored in PruneRedundants.
        if (p->GetRecordingEndTime() < schedTime)
        {
            if (p->future)
                newrecstatus = rsMissedFuture;
            else
                newrecstatus = rsMissed;
        }

        p->SetRecordingStatus(newrecstatus);

        tmpList.push_back(p);
    }

    LOG(VB_SCHEDULE, LOG_INFO, " +-- Cleanup...");
    RecIter tmp = tmpList.begin();
    for ( ; tmp != tmpList.end(); ++tmp)
        worklist.push_back(*tmp);

    if (schedTmpRecord == "sched_temp_record")
    {
        result.prepare("DROP TABLE IF EXISTS sched_temp_record;");
        if (!result.exec())
            MythDB::DBError("AddNewRecords sched_temp_record", result);
    }

    result.prepare("DROP TABLE IF EXISTS sched_temp_recorded;");
    if (!result.exec())
        MythDB::DBError("AddNewRecords drop table", result);
}

void Scheduler::AddNotListed(void) {

    struct timeval dbstart, dbend;
    RecList tmpList;

    QString query = QString(
        "SELECT RECTABLE.title,       RECTABLE.subtitle,    " //  0,1
        "       RECTABLE.description, RECTABLE.season,       " //  2,3
        "       RECTABLE.episode,     RECTABLE.category,    " //  4,5
        "       RECTABLE.chanid,      channel.channum,      " //  6,7
        "       RECTABLE.station,     channel.name,         " //  8,9
        "       RECTABLE.recgroup,    RECTABLE.playgroup,   " // 10,11
        "       RECTABLE.seriesid,    RECTABLE.programid,   " // 12,13
        "       RECTABLE.inetref,     RECTABLE.recpriority, " // 14,15
        "       RECTABLE.startdate,   RECTABLE.starttime,   " // 16,17
        "       RECTABLE.enddate,     RECTABLE.endtime,     " // 18,19
        "       RECTABLE.recordid,    RECTABLE.type,        " // 20,21
        "       RECTABLE.dupin,       RECTABLE.dupmethod,   " // 22,23
        "       RECTABLE.findid,                            " // 24
        "       RECTABLE.startoffset, RECTABLE.endoffset,   " // 25,26
        "       channel.commmethod                          " // 27
        "FROM RECTABLE "
        "INNER JOIN channel ON (channel.chanid = RECTABLE.chanid) "
        "LEFT JOIN recordmatch on RECTABLE.recordid = recordmatch.recordid "
        "WHERE (type = %1 OR type = %2 OR type = %3 OR type = %4) AND "
        "      recordmatch.chanid IS NULL")
        .arg(kSingleRecord)
        .arg(kTimeslotRecord)
        .arg(kWeekslotRecord)
        .arg(kOverrideRecord);

    query.replace("RECTABLE", recordTable);

    LOG(VB_SCHEDULE, LOG_INFO, QString(" |-- Start DB Query..."));

    gettimeofday(&dbstart, NULL);
    MSqlQuery result(dbConn);
    result.prepare(query);
    bool ok = result.exec();
    gettimeofday(&dbend, NULL);

    if (!ok)
    {
        MythDB::DBError("AddNotListed", result);
        return;
    }

    LOG(VB_SCHEDULE, LOG_INFO,
        QString(" |-- %1 results in %2 sec. Processing...")
            .arg(result.size())
            .arg(((dbend.tv_sec  - dbstart.tv_sec) * 1000000 +
                  (dbend.tv_usec - dbstart.tv_usec)) / 1000000.0));

    QDateTime now = QDateTime::currentDateTime();

    while (result.next())
    {
        RecordingType rectype = RecordingType(result.value(21).toInt());
        QDateTime startts(result.value(16).toDate(), result.value(17).toTime());
        QDateTime endts(  result.value(18).toDate(), result.value(19).toTime());

        if (rectype == kTimeslotRecord)
        {
            int days = startts.daysTo(now);

            startts = startts.addDays(days);
            endts   = endts.addDays(days);

            if (endts < now)
            {
                startts = startts.addDays(1);
                endts   = endts.addDays(1);
            }
        }
        else if (rectype == kWeekslotRecord)
        {
            int weeks = (startts.daysTo(now) + 6) / 7;

            startts = startts.addDays(weeks * 7);
            endts   = endts.addDays(weeks * 7);

            if (endts < now)
            {
                startts = startts.addDays(7);
                endts   = endts.addDays(7);
            }
        }

        QDateTime recstartts = startts.addSecs(result.value(25).toInt() * -60);
        QDateTime recendts   = endts.addSecs(  result.value(26).toInt() * +60);

        if (recstartts >= recendts)
        {
            // start/end-offsets are invalid so ignore
            recstartts = startts;
            recendts   = endts;
        }

        // Don't bother if the end time has already passed
        if (recendts < schedTime)
            continue;

        bool sor = (kSingleRecord == rectype) || (kOverrideRecord == rectype);

        RecordingInfo *p = new RecordingInfo(
            result.value(0).toString(), // Title
            (sor) ? result.value(1).toString() : QString(), // Subtitle
            (sor) ? result.value(2).toString() : QString(), // Description
            result.value(3).toUInt(), // Season
            result.value(4).toUInt(), // Episode
            QString(), // Category

            result.value(6).toUInt(), // Chanid
            result.value(7).toString(), // Channel number
            result.value(8).toString(), // Call Sign
            result.value(9).toString(), // Channel name

            result.value(10).toString(), // Recgroup
            result.value(11).toString(), // Playgroup

            result.value(12).toString(), // Series ID
            result.value(13).toString(), // Program ID
            result.value(14).toString(), // Inetref

            result.value(15).toInt(), // Rec priority

            startts,                     endts,
            recstartts,                  recendts,

            rsNotListed, // Recording Status

            result.value(20).toUInt(), // Recording ID
            RecordingType(result.value(21).toInt()), // Recording type

            RecordingDupInType(result.value(22).toInt()), // DupIn type
            RecordingDupMethodType(result.value(23).toInt()), // Dup method

            result.value(24).toUInt(), // Find ID

            result.value(27).toInt() == COMM_DETECT_COMMFREE); // Comm Free

        tmpList.push_back(p);
    }

    RecIter tmp = tmpList.begin();
    for ( ; tmp != tmpList.end(); ++tmp)
        worklist.push_back(*tmp);
}

void Scheduler::findAllScheduledPrograms(RecList &proglist)
{
    QString query = QString(
        "SELECT RECTABLE.title,       RECTABLE.subtitle,    " //  0,1
        "       RECTABLE.description, RECTABLE.season,      " //  2,3
        "       RECTABLE.episode,     RECTABLE.category,    " //  4,5
        "       RECTABLE.chanid,      channel.channum,      " //  6,7
        "       RECTABLE.station,     channel.name,         " //  8,9
        "       RECTABLE.recgroup,    RECTABLE.playgroup,   " // 10,11
        "       RECTABLE.seriesid,    RECTABLE.programid,   " // 12,13
        "       RECTABLE.inetref,     RECTABLE.recpriority, " // 14,15
        "       RECTABLE.startdate,   RECTABLE.starttime,   " // 16,17
        "       RECTABLE.enddate,     RECTABLE.endtime,     " // 18,19
        "       RECTABLE.recordid,    RECTABLE.type,        " // 20,21
        "       RECTABLE.dupin,       RECTABLE.dupmethod,   " // 22,23
        "       RECTABLE.findid,                            " // 24
        "       channel.commmethod                          " // 25
        "FROM RECTABLE "
        "LEFT JOIN channel ON channel.callsign = RECTABLE.station "
        "GROUP BY recordid "
        "ORDER BY title ASC");
    query.replace("RECTABLE", recordTable);

    MSqlQuery result(MSqlQuery::InitCon());
    result.prepare(query);

    if (!result.exec())
    {
        MythDB::DBError("findAllScheduledPrograms", result);
        return;
    }

    while (result.next())
    {
        RecordingType rectype = RecordingType(result.value(21).toInt());
        QDateTime startts;
        QDateTime endts;
        if (rectype == kSingleRecord   ||
            rectype == kDontRecord     ||
            rectype == kOverrideRecord ||
            rectype == kTimeslotRecord ||
            rectype == kWeekslotRecord)
        {
            startts = QDateTime(result.value(16).toDate(),
                                result.value(17).toTime());
            endts = QDateTime(result.value(18).toDate(),
                              result.value(19).toTime());
        }
        else
        {
            // put currentDateTime() in time fields to prevent
            // Invalid date/time warnings later
            startts = mythCurrentDateTime();
            startts.setTime(QTime(0,0));
            endts = startts;
        }

        proglist.push_back(new RecordingInfo(
            result.value(0).toString(),  result.value(1).toString(),
            result.value(2).toString(),  result.value(3).toUInt(),
            result.value(4).toUInt(),    result.value(5).toString(),

            result.value(6).toUInt(),    result.value(7).toString(),
            result.value(8).toString(),  result.value(9).toString(),

            result.value(10).toString(),  result.value(11).toString(),

            result.value(12).toString(), result.value(13).toString(),
            result.value(14).toString(),

            result.value(15).toInt(),

            startts, endts,
            startts, endts,

            rsUnknown,

            result.value(20).toUInt(),   rectype,
            RecordingDupInType(result.value(22).toInt()),
            RecordingDupMethodType(result.value(23).toInt()),

            result.value(24).toUInt(),

            result.value(25).toInt() == COMM_DETECT_COMMFREE));
    }
}

/////////////////////////////////////////////////////////////////////////////
// Storage Scheduler sort order routines
// Sort mode-preferred to least-preferred (true == a more preferred than b)
//
// Prefer local over remote and to balance Disk I/O (weight), then free space
static bool comp_storage_combination(FileSystemInfo *a, FileSystemInfo *b)
{
    // local over remote
    if (a->isLocal() && !b->isLocal())
    {
        if (a->getWeight() <= b->getWeight())
        {
            return true;
        }
    }
    else if (a->isLocal() == b->isLocal())
    {
        if (a->getWeight() < b->getWeight())
        {
            return true;
        }
        else if (a->getWeight() > b->getWeight())
        {
            return false;
        }
        else if (a->getFreeSpace() > b->getFreeSpace())
        {
            return true;
        }
    }
    else if (!a->isLocal() && b->isLocal())
    {
        if (a->getWeight() < b->getWeight())
        {
            return true;
        }
    }

    return false;
}

// prefer dirs with more percentage free space over dirs with less
static bool comp_storage_perc_free_space(FileSystemInfo *a, FileSystemInfo *b)
{
    if (a->getTotalSpace() == 0)
        return false;

    if (b->getTotalSpace() == 0)
        return true;

    if ((a->getFreeSpace() * 100.0) / a->getTotalSpace() >
        (b->getFreeSpace() * 100.0) / b->getTotalSpace())
        return true;

    return false;
}

// prefer dirs with more absolute free space over dirs with less
static bool comp_storage_free_space(FileSystemInfo *a, FileSystemInfo *b)
{
    if (a->getFreeSpace() > b->getFreeSpace())
        return true;

    return false;
}

// prefer dirs with less weight (disk I/O) over dirs with more weight
static bool comp_storage_disk_io(FileSystemInfo *a, FileSystemInfo *b)
{
    if (a->getWeight() < b->getWeight())
        return true;

    return false;
}

/////////////////////////////////////////////////////////////////////////////

void Scheduler::GetNextLiveTVDir(uint cardid)
{
    QMutexLocker lockit(&schedLock);

    EncoderLink *tv = (*m_tvList)[cardid];

    if (!tv)
        return;

    QDateTime cur = mythCurrentDateTime();
    QString recording_dir;
    int fsID = FillRecordingDir(
        "LiveTV",
        (tv->IsLocal()) ? gCoreContext->GetHostName() : tv->GetHostName(),
        "LiveTV", cur, cur.addSecs(3600), cardid,
        recording_dir, reclist);

    tv->SetNextLiveTVDir(recording_dir);

    LOG(VB_FILE, LOG_INFO, LOC + QString("FindNextLiveTVDir: next dir is '%1'")
            .arg(recording_dir));

    if (m_expirer) // update auto expirer
        m_expirer->Update(cardid, fsID, true);
}

int Scheduler::FillRecordingDir(
    const QString &title,
    const QString &hostname,
    const QString &storagegroup,
    const QDateTime &recstartts,
    const QDateTime &recendts,
    uint cardid,
    QString &recording_dir,
    const RecList &reclist)
{
    LOG(VB_SCHEDULE, LOG_INFO, LOC + "FillRecordingDir: Starting");

    int fsID = -1;
    MSqlQuery query(MSqlQuery::InitCon());
    QMap<QString, FileSystemInfo>::Iterator fsit;
    QMap<QString, FileSystemInfo>::Iterator fsit2;
    QString dirKey;
    QStringList strlist;
    RecordingInfo *thispg;
    StorageGroup mysgroup(storagegroup, hostname);
    QStringList dirlist = mysgroup.GetDirList();
    QStringList recsCounted;
    list<FileSystemInfo *> fsInfoList;
    list<FileSystemInfo *>::iterator fslistit;

    recording_dir.clear();

    if (dirlist.size() == 1)
    {
        LOG(VB_FILE | VB_SCHEDULE, LOG_INFO, LOC +
            QString("FillRecordingDir: The only directory in the %1 Storage "
                    "Group is %2, so it will be used by default.")
                .arg(storagegroup) .arg(dirlist[0]));
        recording_dir = dirlist[0];
        LOG(VB_SCHEDULE, LOG_INFO, LOC + "FillRecordingDir: Finished");

        return -1;
    }

    int weightPerRecording =
            gCoreContext->GetNumSetting("SGweightPerRecording", 10);
    int weightPerPlayback =
            gCoreContext->GetNumSetting("SGweightPerPlayback", 5);
    int weightPerCommFlag =
            gCoreContext->GetNumSetting("SGweightPerCommFlag", 5);
    int weightPerTranscode =
            gCoreContext->GetNumSetting("SGweightPerTranscode", 5);

    QString storageScheduler =
            gCoreContext->GetSetting("StorageScheduler", "Combination");
    int localStartingWeight =
            gCoreContext->GetNumSetting("SGweightLocalStarting",
                                    (storageScheduler != "Combination") ? 0
                                        : (int)(-1.99 * weightPerRecording));
    int remoteStartingWeight =
            gCoreContext->GetNumSetting("SGweightRemoteStarting", 0);
    int maxOverlap = gCoreContext->GetNumSetting("SGmaxRecOverlapMins", 3) * 60;

    FillDirectoryInfoCache();

    LOG(VB_FILE | VB_SCHEDULE, LOG_INFO, LOC +
        "FillRecordingDir: Calculating initial FS Weights.");

    for (fsit = fsInfoCache.begin(); fsit != fsInfoCache.end(); ++fsit)
    {
        FileSystemInfo *fs = &(*fsit);
        int tmpWeight = 0;

        QString msg = QString("  %1:%2").arg(fs->getHostname())
                              .arg(fs->getPath());
        if (fs->isLocal())
        {
            tmpWeight = localStartingWeight;
            msg += " is local (" + QString::number(tmpWeight) + ")";
        }
        else
        {
            tmpWeight = remoteStartingWeight;
            msg += " is remote (+" + QString::number(tmpWeight) + ")";
        }

        fs->setWeight(tmpWeight);

        tmpWeight = gCoreContext->GetNumSetting(QString("SGweightPerDir:%1:%2")
                                .arg(fs->getHostname()).arg(fs->getPath()), 0);
        fs->setWeight(fs->getWeight() + tmpWeight);

        if (tmpWeight)
            msg += ", has SGweightPerDir offset of "
                   + QString::number(tmpWeight) + ")";

        msg += ". initial dir weight = " + QString::number(fs->getWeight());
        LOG(VB_FILE | VB_SCHEDULE, LOG_INFO, msg);

        fsInfoList.push_back(fs);
    }

    LOG(VB_FILE | VB_SCHEDULE, LOG_INFO, LOC +
        "FillRecordingDir: Adjusting FS Weights from inuseprograms.");

    MSqlQuery saveRecDir(MSqlQuery::InitCon());
    saveRecDir.prepare("UPDATE inuseprograms "
                       "SET recdir = :RECDIR "
                       "WHERE chanid    = :CHANID AND "
                       "      starttime = :STARTTIME");

    query.prepare(
        "SELECT i.chanid, i.starttime, r.endtime, recusage, rechost, recdir "
        "FROM inuseprograms i, recorded r "
        "WHERE DATE_ADD(lastupdatetime, INTERVAL 16 MINUTE) > NOW() AND "
        "      i.chanid    = r.chanid AND "
        "      i.starttime = r.starttime");

    if (!query.exec())
    {
        MythDB::DBError(LOC + "FillRecordingDir", query);
    }
    else
    {
        while (query.next())
        {
            uint      recChanid = query.value(0).toUInt();
            QDateTime recStart(   query.value(1).toDateTime());
            QDateTime recEnd(     query.value(2).toDateTime());
            QString   recUsage(   query.value(3).toString());
            QString   recHost(    query.value(4).toString());
            QString   recDir(     query.value(5).toString());

            if (recDir.isEmpty())
            {
                ProgramInfo pginfo(recChanid, recStart);
                recDir = pginfo.DiscoverRecordingDirectory();
                recDir = recDir.isEmpty() ? "_UNKNOWN_" : recDir;

                saveRecDir.bindValue(":RECDIR",    recDir);
                saveRecDir.bindValue(":CHANID",    recChanid);
                saveRecDir.bindValue(":STARTTIME", recStart);
                if (!saveRecDir.exec())
                    MythDB::DBError(LOC + "FillRecordingDir", saveRecDir);
            }
            if (recDir == "_UNKNOWN_")
                continue;

            for (fslistit = fsInfoList.begin();
                 fslistit != fsInfoList.end(); ++fslistit)
            {
                FileSystemInfo *fs = *fslistit;
                if ((recHost == fs->getHostname()) &&
                    (recDir == fs->getPath()))
                {
                    int weightOffset = 0;

                    if (recUsage == kRecorderInUseID)
                    {
                        if (recEnd > recstartts.addSecs(maxOverlap))
                        {
                            weightOffset += weightPerRecording;
                            recsCounted << QString::number(recChanid) + ":" +
                                           recStart.toString(Qt::ISODate);
                        }
                    }
                    else if (recUsage.contains(kPlayerInUseID))
                        weightOffset += weightPerPlayback;
                    else if (recUsage == kFlaggerInUseID)
                        weightOffset += weightPerCommFlag;
                    else if (recUsage == kTranscoderInUseID)
                        weightOffset += weightPerTranscode;

                    if (weightOffset)
                    {
                        LOG(VB_FILE | VB_SCHEDULE, LOG_INFO,
                            QString("  %1 @ %2 in use by '%3' on %4:%5, FSID "
                                    "#%6, FSID weightOffset +%7.")
                                .arg(recChanid)
                                .arg(recStart.toString(Qt::ISODate))
                                .arg(recUsage).arg(recHost).arg(recDir)
                                .arg(fs->getFSysID()).arg(weightOffset));

                        // need to offset all directories on this filesystem
                        for (fsit2 = fsInfoCache.begin();
                             fsit2 != fsInfoCache.end(); ++fsit2)
                        {
                            FileSystemInfo *fs2 = &(*fsit2);
                            if (fs2->getFSysID() == fs->getFSysID())
                            {
                                LOG(VB_FILE | VB_SCHEDULE, LOG_INFO,
                                    QString("    %1:%2 => old weight %3 plus "
                                            "%4 = %5")
                                        .arg(fs2->getHostname())
                                        .arg(fs2->getPath())
                                        .arg(fs2->getWeight())
                                        .arg(weightOffset)
                                        .arg(fs2->getWeight() + weightOffset));

                                fs2->setWeight(fs2->getWeight() + weightOffset);
                            }
                        }
                    }
                    break;
                }
            }
        }
    }

    LOG(VB_FILE | VB_SCHEDULE, LOG_INFO, LOC +
        "FillRecordingDir: Adjusting FS Weights from scheduler.");

    RecConstIter recIter;
    for (recIter = reclist.begin(); recIter != reclist.end(); ++recIter)
    {
        thispg = *recIter;

        if ((recendts < thispg->GetRecordingStartTime()) ||
            (recstartts > thispg->GetRecordingEndTime()) ||
                (thispg->GetRecordingStatus() != rsWillRecord) ||
                (thispg->GetCardID() == 0) ||
                (recsCounted.contains(QString("%1:%2").arg(thispg->GetChanID())
                    .arg(thispg->GetRecordingStartTime(ISODate)))) ||
                (thispg->GetPathname().isEmpty()))
            continue;

        for (fslistit = fsInfoList.begin();
             fslistit != fsInfoList.end(); ++fslistit)
        {
            FileSystemInfo *fs = *fslistit;
            if ((fs->getHostname() == thispg->GetHostname()) &&
                (fs->getPath() == thispg->GetPathname()))
            {
                LOG(VB_FILE | VB_SCHEDULE, LOG_INFO,
                    QString("%1 @ %2 will record on %3:%4, FSID #%5, "
                            "weightPerRecording +%6.")
                        .arg(thispg->GetChanID())
                        .arg(thispg->GetRecordingStartTime(ISODate))
                        .arg(fs->getHostname()).arg(fs->getPath())
                        .arg(fs->getFSysID()).arg(weightPerRecording));

                for (fsit2 = fsInfoCache.begin();
                     fsit2 != fsInfoCache.end(); ++fsit2)
                {
                    FileSystemInfo *fs2 = &(*fsit2);
                    if (fs2->getFSysID() == fs->getFSysID())
                    {
                        LOG(VB_FILE | VB_SCHEDULE, LOG_INFO,
                            QString("    %1:%2 => old weight %3 plus %4 = %5")
                                .arg(fs2->getHostname()).arg(fs2->getPath())
                                .arg(fs2->getWeight()).arg(weightPerRecording)
                                .arg(fs2->getWeight() + weightPerRecording));

                        fs2->setWeight(fs2->getWeight() + weightPerRecording);
                    }
                }
                break;
            }
        }
    }

    LOG(VB_FILE | VB_SCHEDULE, LOG_INFO,
        QString("Using '%1' Storage Scheduler directory sorting algorithm.")
            .arg(storageScheduler));

    if (storageScheduler == "BalancedFreeSpace")
        fsInfoList.sort(comp_storage_free_space);
    else if (storageScheduler == "BalancedPercFreeSpace")
        fsInfoList.sort(comp_storage_perc_free_space);
    else if (storageScheduler == "BalancedDiskIO")
        fsInfoList.sort(comp_storage_disk_io);
    else // default to using original method
        fsInfoList.sort(comp_storage_combination);

    if (VERBOSE_LEVEL_CHECK(VB_FILE | VB_SCHEDULE, LOG_INFO))
    {
        LOG(VB_FILE | VB_SCHEDULE, LOG_INFO,
            "--- FillRecordingDir Sorted fsInfoList start ---");
        for (fslistit = fsInfoList.begin();fslistit != fsInfoList.end();
             ++fslistit)
        {
            FileSystemInfo *fs = *fslistit;
            LOG(VB_FILE | VB_SCHEDULE, LOG_INFO, QString("%1:%2")
                .arg(fs->getHostname()) .arg(fs->getPath()));
            LOG(VB_FILE | VB_SCHEDULE, LOG_INFO, QString("    Location    : %1")
                .arg((fs->isLocal()) ? "local" : "remote"));
            LOG(VB_FILE | VB_SCHEDULE, LOG_INFO, QString("    weight      : %1")
                .arg(fs->getWeight()));
            LOG(VB_FILE | VB_SCHEDULE, LOG_INFO, QString("    free space  : %5")
                .arg(fs->getFreeSpace()));
        }
        LOG(VB_FILE | VB_SCHEDULE, LOG_INFO,
            "--- FillRecordingDir Sorted fsInfoList end ---");
    }

    // This code could probably be expanded to check the actual bitrate the
    // recording will record at for analog broadcasts that are encoded locally.
    // maxSizeKB is 1/3 larger than required as this is what the auto expire
    // uses
    EncoderLink *nexttv = (*m_tvList)[cardid];
    long long maxByterate = nexttv->GetMaxBitrate() / 8;
    long long maxSizeKB = (maxByterate + maxByterate/3) *
        recstartts.secsTo(recendts) / 1024;

    bool simulateAutoExpire =
       ((gCoreContext->GetSetting("StorageScheduler") == "BalancedFreeSpace") &&
        (m_expirer) &&
        (fsInfoList.size() > 1));

    // Loop though looking for a directory to put the file in.  The first time
    // through we look for directories with enough free space in them.  If we
    // can't find a directory that way we loop through and pick the first good
    // one from the list no matter how much free space it has.  We assume that
    // something will have to be expired for us to finish the recording.
    // pass 1: try to fit onto an existing file system with enough free space
    // pass 2: fit onto the file system with the lowest priority files to be
    //         expired this is used only with multiple file systems
    //         Estimates are made by simulating each expiry until one of
    //         the file  systems has enough sapce to fit the new file.
    // pass 3: fit onto the first file system that will take it with lowest
    //         priority files on this file system expired
    for (unsigned int pass = 1; pass <= 3; pass++)
    {
        bool foundDir = false;

        if ((pass == 2) && simulateAutoExpire)
        {
            // setup a container of remaining space for all the file systems
            QMap <int , long long> remainingSpaceKB;
            for (fslistit = fsInfoList.begin();
                fslistit != fsInfoList.end(); ++fslistit)
            {
                remainingSpaceKB[(*fslistit)->getFSysID()] =
                    (*fslistit)->getFreeSpace();
            }

            // get list of expirable programs
            pginfolist_t expiring;
            m_expirer->GetAllExpiring(expiring);

            for(pginfolist_t::iterator it=expiring.begin();
                it != expiring.end(); ++it)
            {
                // find the filesystem its on
                FileSystemInfo *fs = NULL;
                for (fslistit = fsInfoList.begin();
                    fslistit != fsInfoList.end(); ++fslistit)
                {
                    // recording is not on this filesystem's host
                    if ((*it)->GetHostname() != (*fslistit)->getHostname())
                        continue;

                    // directory is not in the Storage Group dir list
                    if (!dirlist.contains((*fslistit)->getPath()))
                        continue;

                    QString filename =
                        (*fslistit)->getPath() + "/" + (*it)->GetPathname();

                    // recording is local
                    if ((*it)->GetHostname() == gCoreContext->GetHostName())
                    {
                        QFile checkFile(filename);

                        if (checkFile.exists())
                        {
                            fs = *fslistit;
                            break;
                        }
                    }
                    else // recording is remote
                    {
                        QString backuppath = (*it)->GetPathname();
                        ProgramInfo *programinfo = *it;
                        bool foundSlave = false;

                        QMap<int, EncoderLink *>::Iterator enciter =
                            m_tvList->begin();
                        for (; enciter != m_tvList->end(); ++enciter)
                        {
                            if ((*enciter)->GetHostName() ==
                                programinfo->GetHostname())
                            {
                                (*enciter)->CheckFile(programinfo);
                                foundSlave = true;
                                break;
                            }
                        }
                        if (foundSlave &&
                            programinfo->GetPathname() == filename)
                        {
                            fs = *fslistit;
                            programinfo->SetPathname(backuppath);
                            break;
                        }
                        programinfo->SetPathname(backuppath);
                    }
                }

                if (!fs)
                {
                    LOG(VB_GENERAL, LOG_ERR,
                        QString("Unable to match '%1' "
                                "to any file system.  Ignoring it.")
                            .arg((*it)->GetBasename()));
                    continue;
                }

                // add this files size to the remaining free space
                remainingSpaceKB[fs->getFSysID()] +=
                    (*it)->GetFilesize() / 1024;

                // check if we have enough space for new file
                long long desiredSpaceKB =
                    m_expirer->GetDesiredSpace(fs->getFSysID());

                if (remainingSpaceKB[fs->getFSysID()] >
                        (desiredSpaceKB + maxSizeKB))
                {
                    recording_dir = fs->getPath();
                    fsID = fs->getFSysID();

                    LOG(VB_FILE, LOG_INFO,
                        QString("pass 2: '%1' will record in '%2' "
                                "although there is only %3 MB free and the "
                                "AutoExpirer wants at least %4 MB.  This "
                                "directory has the highest priority files "
                                "to be expired from the AutoExpire list and "
                                "there are enough that the Expirer should "
                                "be able to free up space for this recording.")
                            .arg(title).arg(recording_dir)
                            .arg(fs->getFreeSpace() / 1024)
                            .arg(desiredSpaceKB / 1024));

                    foundDir = true;
                    break;
                }
            }

            m_expirer->ClearExpireList(expiring);
        }
        else // passes 1 & 3 (or 1 & 2 if !simulateAutoExpire)
        {
            for (fslistit = fsInfoList.begin();
                fslistit != fsInfoList.end(); ++fslistit)
            {
                long long desiredSpaceKB = 0;
                FileSystemInfo *fs = *fslistit;
                if (m_expirer)
                    desiredSpaceKB =
                        m_expirer->GetDesiredSpace(fs->getFSysID());

                if ((fs->getHostname() == hostname) &&
                    (dirlist.contains(fs->getPath())) &&
                    ((pass > 1) ||
                     (fs->getFreeSpace() > (desiredSpaceKB + maxSizeKB))))
                {
                    recording_dir = fs->getPath();
                    fsID = fs->getFSysID();

                    if (pass == 1)
                        LOG(VB_FILE, LOG_INFO,
                            QString("pass 1: '%1' will record in "
                                    "'%2' which has %3 MB free. This recording "
                                    "could use a max of %4 MB and the "
                                    "AutoExpirer wants to keep %5 MB free.")
                                .arg(title)
                                .arg(recording_dir)
                                .arg(fs->getFreeSpace() / 1024)
                                .arg(maxSizeKB / 1024)
                                .arg(desiredSpaceKB / 1024));
                    else
                        LOG(VB_FILE, LOG_INFO,
                            QString("pass %1: '%2' will record in "
                                "'%3' although there is only %4 MB free and "
                                "the AutoExpirer wants at least %5 MB.  "
                                "Something will have to be deleted or expired "
                                "in order for this recording to complete "
                                "successfully.")
                                .arg(pass).arg(title)
                                .arg(recording_dir)
                                .arg(fs->getFreeSpace() / 1024)
                                .arg(desiredSpaceKB / 1024));

                    foundDir = true;
                    break;
                }
            }
        }

        if (foundDir)
            break;
    }

    LOG(VB_SCHEDULE, LOG_INFO, LOC + "FillRecordingDir: Finished");
    return fsID;
}

void Scheduler::FillDirectoryInfoCache(bool force)
{
    if ((!force) &&
        (fsInfoCacheFillTime > QDateTime::currentDateTime().addSecs(-180)))
        return;

    QList<FileSystemInfo> fsInfos;

    fsInfoCache.clear();

    if (m_mainServer)
        m_mainServer->GetFilesystemInfos(fsInfos);

    QMap <int, bool> fsMap;
    QList<FileSystemInfo>::iterator it1;
    for (it1 = fsInfos.begin(); it1 != fsInfos.end(); ++it1)
    {
        fsMap[it1->getFSysID()] = true;
        fsInfoCache[it1->getHostname() + ":" + it1->getPath()] = *it1;
    }

    LOG(VB_FILE, LOG_INFO, LOC +
        QString("FillDirectoryInfoCache: found %1 unique filesystems")
            .arg(fsMap.size()));

    fsInfoCacheFillTime = QDateTime::currentDateTime();
}

void Scheduler::SchedPreserveLiveTV(void)
{
    if (!livetvTime.isValid())
        return;

    if (livetvTime < schedTime)
    {
        livetvTime = QDateTime();
        return;
    }

    livetvpriority = gCoreContext->GetNumSetting("LiveTVPriority", 0);

    // Build a list of active livetv programs
    QMap<int, EncoderLink *>::Iterator enciter = m_tvList->begin();
    for (; enciter != m_tvList->end(); ++enciter)
    {
        EncoderLink *enc = *enciter;

        if (kState_WatchingLiveTV != enc->GetState())
            continue;

        TunedInputInfo in;
        enc->IsBusy(&in);

        if (!in.inputid)
            continue;

        // Get the program that will be recording on this channel
        // at record start time, if this LiveTV session continues.
        RecordingInfo *dummy = new RecordingInfo(
            in.chanid, livetvTime, true, 4);

        dummy->SetCardID(enc->GetCardID());
        dummy->SetInputID(in.inputid);
        dummy->SetRecordingStatus(rsUnknown);

        retrylist.push_front(dummy);
    }

    if (retrylist.empty())
        return;

    MoveHigherRecords(false);

    while (!retrylist.empty())
    {
        RecordingInfo *p = retrylist.back();
        delete p;
        retrylist.pop_back();
    }
}

/* Determines if the system was started by the auto-wakeup process */
bool Scheduler::WasStartedAutomatically()
{
    bool autoStart = false;

    QDateTime startupTime = QDateTime();
    QString s = gCoreContext->GetSetting("MythShutdownWakeupTime", "");
    if (s.length())
        startupTime = QDateTime::fromString(s, Qt::ISODate);

    // if we don't have a valid startup time assume we were started manually
    if (startupTime.isValid())
    {
        // if we started within 15mins of the saved wakeup time assume we
        // started automatically to record or for a daily wakeup/shutdown period

        if (abs(startupTime.secsTo(QDateTime::currentDateTime())) < (15 * 60))
        {
            LOG(VB_SCHEDULE, LOG_INFO,
                "Close to auto-start time, AUTO-Startup assumed");
            autoStart = true;
        }
        else
            LOG(VB_SCHEDULE, LOG_DEBUG,
                "NOT close to auto-start time, USER-initiated startup assumed");
    }
    else if (!s.isEmpty())
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Invalid MythShutdownWakeupTime specified in database (%1)")
                .arg(s));

    return autoStart;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
