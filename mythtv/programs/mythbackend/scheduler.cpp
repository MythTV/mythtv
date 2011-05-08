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
#include "scheduledrecording.h"
#include "cardutil.h"
#include "mythdb.h"
#include "mythsystemevent.h"

#define LOC QString("Scheduler: ")
#define LOC_WARN QString("Scheduler, Warning: ")
#define LOC_ERR QString("Scheduler, Error: ")

Scheduler::Scheduler(bool runthread, QMap<int, EncoderLink *> *tvList,
                     QString tmptable, Scheduler *master_sched) :
    recordTable(tmptable),
    priorityTable("powerpriority"),
    schedLock(),
    reclist_changed(false),
    specsched(master_sched),
    schedMoveHigher(false),
    schedulingEnabled(true),
    m_tvList(tvList),
    expirer(NULL),
    threadrunning(false),
    m_mainServer(NULL),
    resetIdleTime(false),
    m_isShuttingDown(false),
    error(0),
    livetvTime(QDateTime()),
    livetvpriority(0),
    prefinputpri(0)
{
    if (master_sched)
        master_sched->getAllPending(&reclist);

    // Only the master scheduler should use SchedCon()
    if (runthread)
        dbConn = MSqlQuery::SchedCon();
    else
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

    threadrunning = runthread;

    fsInfoCacheFillTime = QDateTime::currentDateTime().addSecs(-1000);

    if (runthread)
    {
        int err = pthread_create(&schedThread, NULL, SchedulerThread, this);
        if (err != 0)
        {
            VERBOSE(VB_IMPORTANT,
                    QString("Failed to start scheduler thread: error %1")
                    .arg(err));
            threadrunning = false;
        }

        WakeUpSlaves();
    }
}

Scheduler::~Scheduler()
{
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

    if (threadrunning)
    {
        pthread_cancel(schedThread);
        pthread_join(schedThread, NULL);
    }
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
    MSqlQuery query(dbConn);
    if (!query.exec("SELECT count(*) FROM capturecard") || !query.next())
    {
        MythDB::DBError("verifyCards() -- main query 1", query);
        error = BACKEND_EXIT_NO_CAP_CARD;
        return false;
    }

    uint numcards = query.value(0).toUInt();
    if (!numcards)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "No capture cards are defined in the database.\n\t\t\t"
                "Perhaps you should re-read the installation instructions?");
        error = BACKEND_EXIT_NO_CAP_CARD;
        return false;
    }

    query.prepare("SELECT sourceid,name FROM videosource ORDER BY sourceid;");

    if (!query.exec())
    {
        MythDB::DBError("verifyCards() -- main query 2", query);
        error = BACKEND_EXIT_NO_CHAN_DATA;
        return false;
    }

    uint numsources = 0;
    MSqlQuery subquery(dbConn);
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
            VERBOSE(VB_IMPORTANT, LOC_WARN +
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
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "No channel sources defined in the database");
        error = BACKEND_EXIT_NO_CHAN_DATA;
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
    return a->GetRecordingStatus() < b->GetRecordingStatus();
}

static QDateTime schedTime;

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

    int apast = (a->GetRecordingStartTime() < schedTime.addSecs(-30) &&
                 !a->IsReactivated());
    int bpast = (b->GetRecordingStartTime() < schedTime.addSecs(-30) &&
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

static bool comp_timechannel(RecordingInfo *a, RecordingInfo *b)
{
    if (a->GetRecordingStartTime() != b->GetRecordingStartTime())
        return a->GetRecordingStartTime() < b->GetRecordingStartTime();
    if (a->GetChanNum() == b->GetChanNum())
        return a->GetChanID() < b->GetChanID();
    if (a->GetChanNum().toInt() > 0 && b->GetChanNum().toInt() > 0)
        return a->GetChanNum().toInt() < b->GetChanNum().toInt();
    return a->GetChanNum() < b->GetChanNum();
}

bool Scheduler::FillRecordList(bool doLock)
{
    schedMoveHigher = (bool)gCoreContext->GetNumSetting("SchedMoveHigher");
    schedTime = QDateTime::currentDateTime();

    VERBOSE(VB_SCHEDULE, "BuildWorkList...");
    BuildWorkList();
    if (doLock)
        schedLock.unlock();
    VERBOSE(VB_SCHEDULE, "AddNewRecords...");
    AddNewRecords();
    VERBOSE(VB_SCHEDULE, "AddNotListed...");
    AddNotListed();

    VERBOSE(VB_SCHEDULE, "Sort by time...");
    SORT_RECLIST(worklist, comp_overlap);
    VERBOSE(VB_SCHEDULE, "PruneOverlaps...");
    PruneOverlaps();

    VERBOSE(VB_SCHEDULE, "Sort by priority...");
    SORT_RECLIST(worklist, comp_priority);
    VERBOSE(VB_SCHEDULE, "BuildListMaps...");
    BuildListMaps();
    VERBOSE(VB_SCHEDULE, "SchedNewRecords...");
    SchedNewRecords();
    VERBOSE(VB_SCHEDULE, "SchedPreserveLiveTV...");
    SchedPreserveLiveTV();
    VERBOSE(VB_SCHEDULE, "ClearListMaps...");
    ClearListMaps();
    if (doLock)
        schedLock.lock();

    VERBOSE(VB_SCHEDULE, "Sort by time...");
    SORT_RECLIST(worklist, comp_redundant);
    VERBOSE(VB_SCHEDULE, "PruneRedundants...");
    PruneRedundants();

    VERBOSE(VB_SCHEDULE, "Sort by time...");
    SORT_RECLIST(worklist, comp_recstart);
    VERBOSE(VB_SCHEDULE, "ClearWorkList...");
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

    gettimeofday(&fillstart, NULL);
    UpdateMatches(recordid);
    gettimeofday(&fillend, NULL);
    matchTime = ((fillend.tv_sec - fillstart.tv_sec ) * 1000000 +
                 (fillend.tv_usec - fillstart.tv_usec)) / 1000000.0;

    gettimeofday(&fillstart, NULL);
    FillRecordList(false);
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
    VERBOSE(VB_GENERAL, msg);
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
    if (!VERBOSE_LEVEL_CHECK(VB_SCHEDULE))
        return;

    QDateTime now = QDateTime::currentDateTime();

    cout << "--- print list start ---\n";
    cout << "Title - Subtitle                    Ch Station "
        "Day Start  End   S C I  T N Pri" << endl;

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

    cout << "---  print list end  ---\n";
}

void Scheduler::PrintRec(const RecordingInfo *p, const char *prefix)
{
    if (!VERBOSE_LEVEL_CHECK(VB_SCHEDULE))
        return;

    if (prefix)
        cout << prefix;

    QString episode = p->toString(ProgramInfo::kTitleSubtitle, " - ", "");
    episode = episode.leftJustified(34 - (prefix ? strlen(prefix) : 0),
                                    ' ', true);

    QString outstr = QString("%1 %2 %3 %4-%5  %6 %7 %8  ")
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
        .arg(toQChar(p->GetRecordingStatus(), p->GetCardID()))
        .arg(p->GetRecordingPriority());
    if (p->GetRecordingPriority2())
        outstr += QString("/%1").arg(p->GetRecordingPriority2());

    QByteArray out = outstr.toLocal8Bit();

    cout << out.constData() << endl;
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
                VERBOSE(VB_IMPORTANT, 
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
                VERBOSE(VB_IMPORTANT, 
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
        VERBOSE(VB_IMPORTANT, QString(
                    "Failed to change end time on card %1 to %2")
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
                    VERBOSE(VB_IMPORTANT, QString("setting %1/%2/\"%3\" as %4")
                            .arg(sp->GetCardID())
                            .arg(sp->GetChannelSchedulingID())
                            .arg(sp->GetTitle())
                            .arg(toUIState(sp->GetRecordingStatus())));
                }
                else
                {
                    VERBOSE(VB_IMPORTANT, QString("%1/%2/\"%3\" is already "
                                                  "recording on card %4")
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
                VERBOSE(VB_IMPORTANT, QString("setting %1/%2/\"%3\" as aborted")
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
            VERBOSE(VB_IMPORTANT, QString("adding %1/%2/\"%3\" as recording")
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
            VERBOSE(VB_IMPORTANT, QString("setting %1/%2/\"%3\" as aborted")
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
    bool is_conflict_dbg = false;

    for ( ; j != cardlist.end(); ++j)
    {
        const RecordingInfo *q = *j;

        if (p == q)
            continue;

        if (!Recording(q))
            continue;

        if (is_conflict_dbg)
            cout << QString("\n  comparing with '%1' ").arg(q->GetTitle())
                .toLocal8Bit().constData();

        if (p->GetCardID() != 0 && (p->GetCardID() != q->GetCardID()) &&
            !igrp.GetSharedInputGroup(p->GetInputID(), q->GetInputID()))
        {
            if (is_conflict_dbg)
                cout << "  cardid== ";
            continue;
        }

        if (openEnd == 2 || (openEnd == 1 && p->GetChanID() != q->GetChanID()))
        {
            if (p->GetRecordingEndTime() < q->GetRecordingStartTime() ||
                p->GetRecordingStartTime() > q->GetRecordingEndTime())
            {
                if (is_conflict_dbg)
                    cout << "  no-overlap ";
                continue;
            }
        }
        else
        {
            if (p->GetRecordingEndTime() <= q->GetRecordingStartTime() ||
                p->GetRecordingStartTime() >= q->GetRecordingEndTime())
            {
                if (is_conflict_dbg)
                    cout << "  no-overlap ";
                continue;
            }
        }

        if (is_conflict_dbg)
            cout << "\n" <<
                (QString("  cardid's: %1, %2 ")
                 .arg(p->GetCardID()).arg(q->GetCardID()) +
                 QString("Shared input group: %1 ")
                 .arg(igrp.GetSharedInputGroup(
                          p->GetInputID(), q->GetInputID())) +
                 QString("mplexid's: %1, %2")
                 .arg(p->QueryMplexID()).arg(q->QueryMplexID()))
                .toLocal8Bit().constData();

        // if two inputs are in the same input group we have a conflict
        // unless the programs are on the same multiplex.
        if (p->GetCardID() && (p->GetCardID() != q->GetCardID()) &&
            igrp.GetSharedInputGroup(p->GetInputID(), q->GetInputID()))
        {
            uint p_mplexid = p->QueryMplexID();
            if (p_mplexid && (p_mplexid == q->QueryMplexID()))
                continue;
        }

        if (is_conflict_dbg)
            cout << "\n  Found conflict" << endl;

        return true;
    }

    if (is_conflict_dbg)
        cout << "\n  No conflict" << endl;

    return false;
}

const RecordingInfo *Scheduler::FindConflict(
    const QMap<int, RecList> &reclists,
    const RecordingInfo        *p,
    int openend) const
{
    bool is_conflict_dbg = false;

    QMap<int, RecList>::const_iterator it = reclists.begin();
    for (; it != reclists.end(); ++it)
    {
        if (is_conflict_dbg)
        {
            cout << QString("Checking '%1' for conflicts on cardid %2")
                .arg(p->GetTitle()).arg(it.key()).toLocal8Bit().constData();
        }

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
            (q->GetRecordingPriority() != p->GetRecordingPriority()))
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
            QByteArray amsg = msg.toLocal8Bit();
            VERBOSE(VB_SCHEDULE, amsg.constData());
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
    VERBOSE(VB_SCHEDULE, "Scheduling:");

    if (VERBOSE_LEVEL_CHECK(VB_SCHEDULE))
    {
        cout << "+ = schedule this showing to be recorded" << endl;
        cout << "# = could not schedule this showing, retry later" << endl;
        cout << "! = conflict caused by this showing" << endl;
        cout << "/ = retry this showing, same priority pass" << endl;
        cout << "? = retry this showing, lower priority pass" << endl;
        cout << "> = try another showing for this program" << endl;
        cout << "% = found another showing, same priority required" << endl;
        cout << "$ = found another showing, lower priority allowed" << endl;
        cout << "- = unschedule a showing in favor of another one" << endl;
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
            const RecordingInfo *conflict = FindConflict(cardlistmap, p, openEnd);
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
            if (p->GetRecordingPriority() != (*k)->GetRecordingPriority() ||
                !TryAnotherShowing(*k, true))
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

        // Restore the old status for some select cases that won't record.
        if (p->GetRecordingStatus() != rsWillRecord &&
            p->oldrecstatus != rsUnknown &&
            p->oldrecstatus != rsNotListed &&
            !p->IsReactivated())
        {
            p->SetRecordingStatus(p->oldrecstatus);
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
                VERBOSE(VB_SCHEDULE, LOC +
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

bool Scheduler::getAllPending(RecList *retList)
{
    QMutexLocker lockit(&schedLock);

    bool hasconflicts = false;

    RecConstIter it = reclist.begin();
    for (; it != reclist.end(); ++it)
    {
        if ((*it)->GetRecordingStatus() == rsConflict)
            hasconflicts = true;
        retList->push_back(new RecordingInfo(**it));
    }

    SORT_RECLIST(*retList, comp_timechannel);

    return hasconflicts;
}

QMap<QString,ProgramInfo*> Scheduler::GetRecording(void)
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

void Scheduler::getAllPending(QStringList &strList)
{
    RecList retlist;
    bool hasconflicts = getAllPending(&retlist);

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

    VERBOSE(VB_GENERAL, LOC +
            QString("AddRecording() recid: %1")
            .arg(pi.GetRecordingRuleID()));

    for (RecIter it = reclist.begin(); it != reclist.end(); ++it)
    {
        RecordingInfo *p = *it;
        if (p->GetRecordingStatus() == rsRecording &&
            p->IsSameProgramTimeslot(pi))
        {
            VERBOSE(VB_IMPORTANT, LOC + "Not adding recording, " +
                    QString("'%1' is already in reclist.")
                    .arg(pi.GetTitle()));
            return;
        }
    }

    VERBOSE(VB_SCHEDULE, LOC +
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
        VERBOSE(VB_IMPORTANT, LOC_ERR + "IsBusyRecording() -> true, "
                "no tvList or no rcinfo");

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
            VERBOSE(VB_SCHEDULE, LOC_ERR + "IsBusyRecording() -> true, "
                    "rctv("<<rctv<<"==NULL) for card "<<cardids[i]);

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

void Scheduler::RunScheduler(void)
{
    int prerollseconds = 0;
    int wakeThreshold = gCoreContext->GetNumSetting("WakeUpThreshold", 300);
    int secsleft;
    EncoderLink *nexttv = NULL;

    RecordingInfo *nextRecording = NULL;
    QDateTime nextrectime;
    QString schedid;

    QDateTime curtime;
    QDateTime lastupdate = QDateTime::currentDateTime().addDays(-1);
    QDateTime lastSleepCheck = QDateTime::currentDateTime().addDays(-1);

    bool blockShutdown = gCoreContext->GetNumSetting("blockSDWUwithoutClient", 1);
    QDateTime idleSince = QDateTime();
    int idleTimeoutSecs = 0;
    int idleWaitForRecordingTime = 0;
    bool firstRun = true;

    QString sysEventKey;
    int sysEventSecs[5] = { 120, 90, 60, 30, 0 };
    QList<QString>sysEvents[4];

    struct timeval fillstart, fillend;
    float matchTime, placeTime;

    // Mark anything that was recording as aborted.  We'll fix it up.
    // if possible, after the slaves connect and we start scheduling.
    MSqlQuery query(dbConn);
    query.prepare("UPDATE oldrecorded SET recstatus = :RSABORTED "
                  "  WHERE recstatus = :RSRECORDING OR recstatus = :RSTUNING");
    query.bindValue(":RSABORTED", rsAborted);
    query.bindValue(":RSRECORDING", rsRecording);
    query.bindValue(":RSTUNING", rsTuning);
    if (!query.exec())
        MythDB::DBError("UpdateAborted", query);

    // wait for slaves to connect
    sleep(3);

    QMutexLocker lockit(&schedLock);

    reschedQueue.clear();
    reschedQueue.enqueue(-1);

    RecIter startIter = reclist.begin();

    while (1)
    {
        curtime = QDateTime::currentDateTime();
        bool statuschanged = false;

        if ((startIter != reclist.end() &&
             curtime.secsTo((*startIter)->GetRecordingStartTime()) < 30))
        {
            schedLock.unlock();
            sleep(1);
            schedLock.lock();
        }
        else
        {
            if (reschedQueue.empty())
                reschedWait.wait(&schedLock, 1000);

            if (!reschedQueue.empty())
            {
                // We might have been inactive for a long time, so make
                // sure our DB connection is fresh before continuing.
                dbConn = MSqlQuery::SchedCon();

                gettimeofday(&fillstart, NULL);
                QString msg;

                while (!reschedQueue.empty())
                {
                    int recordid = reschedQueue.dequeue();

                    VERBOSE(VB_GENERAL,
                            QString("Reschedule requested for id %1.")
                            .arg(recordid));

                    if (recordid != 0)
                    {
                        if (recordid == -1)
                            reschedQueue.clear();

                        schedLock.unlock();
                        recordmatchLock.lock();
                        UpdateMatches(recordid);
                        recordmatchLock.unlock();
                        schedLock.lock();
                    }
                }

                gettimeofday(&fillend, NULL);

                matchTime = ((fillend.tv_sec - fillstart.tv_sec ) * 1000000 +
                             (fillend.tv_usec - fillstart.tv_usec)) / 1000000.0;

                gettimeofday(&fillstart, NULL);
                bool worklistused = FillRecordList(true);
                gettimeofday(&fillend, NULL);
                if (worklistused)
                {
                    UpdateNextRecord();
                    PrintList();
                }
                else
                {
                    VERBOSE(VB_GENERAL, "Reschedule interrupted, will retry");
                    reschedQueue.enqueue(0);;
                    continue;
                }

                placeTime = ((fillend.tv_sec - fillstart.tv_sec ) * 1000000 +
                             (fillend.tv_usec - fillstart.tv_usec)) / 1000000.0;

                msg.sprintf("Scheduled %d items in "
                            "%.1f = %.2f match + %.2f place",
                            (int)reclist.size(),
                            matchTime + placeTime, matchTime, placeTime);

                VERBOSE(VB_GENERAL, msg);
                gCoreContext->LogEntry("scheduler", LP_INFO,
                                       "Scheduled items", msg);

                fsInfoCacheFillTime =
                    QDateTime::currentDateTime().addSecs(-1000);

                lastupdate = curtime;
                startIter = reclist.begin();
                statuschanged = true;

                // Determine if the user wants us to start recording early
                // and by how many seconds
                prerollseconds = gCoreContext->GetNumSetting("RecordPreRoll");

                idleTimeoutSecs =
                    gCoreContext->GetNumSetting("idleTimeoutSecs", 0);
                idleWaitForRecordingTime =
                    gCoreContext->GetNumSetting("idleWaitForRecordingTime", 15);

                if (firstRun)
                {
                    firstRun = false;

                    //the parameter given to the startup_cmd. "user" means a user
                    // started the BE, 'auto' means it was started automatically
                    QString startupParam = "user";

                    // find the first recording that WILL be recorded
                    RecIter firstRunIter = reclist.begin();
                    for ( ; firstRunIter != reclist.end(); ++firstRunIter)
                        if ((*firstRunIter)->GetRecordingStatus() == rsWillRecord)
                            break;

                    // have we been started automatically?
                    if (WasStartedAutomatically() ||
                        ((firstRunIter != reclist.end()) &&
                         ((curtime.secsTo((*firstRunIter)->GetRecordingStartTime()) - prerollseconds)
                          < (idleWaitForRecordingTime * 60))))
                    {
                        VERBOSE(VB_IMPORTANT, "AUTO-Startup assumed");
                        startupParam = "auto";

                        // Since we've started automatically, don't wait for
                        // client to connect before allowing shutdown.
                        blockShutdown = false;
                    }
                    else
                    {
                        VERBOSE(VB_IMPORTANT, "Seem to be woken up by USER");
                    }

                    QString startupCommand =
                        gCoreContext->GetSetting("startupCommand", "");
                    if (!startupCommand.isEmpty())
                    {
                        startupCommand.replace("$status", startupParam);
                        schedLock.unlock();
                        myth_system(startupCommand);
                        schedLock.lock();
                        if (reclist_changed)
                            continue;
                    }
                }

                PutInactiveSlavesToSleep();
                lastSleepCheck = QDateTime::currentDateTime();

                SendMythSystemEvent("SCHEDULER_RAN");
            }
        }

        for ( ; startIter != reclist.end(); ++startIter)
            if ((*startIter)->GetRecordingStatus() !=
                (*startIter)->oldrecstatus)
                break;

        curtime = QDateTime::currentDateTime();

        // About every 5 minutes check for slaves that can be put to sleep
        if (lastSleepCheck.secsTo(curtime) > 300)
        {
            PutInactiveSlavesToSleep();
            lastSleepCheck = QDateTime::currentDateTime();
        }

        // Go through the list of recordings starting in the next few minutes
        // and wakeup any slaves that are asleep
        RecIter recIter = startIter;
        for ( ; schedulingEnabled && recIter != reclist.end(); ++recIter)
        {
            nextRecording = *recIter;
            nextrectime = nextRecording->GetRecordingStartTime();
            secsleft = curtime.secsTo(nextrectime);

            if ((secsleft - prerollseconds) > wakeThreshold)
                break;

            if (m_tvList->find(nextRecording->GetCardID()) == m_tvList->end())
                continue;

            sysEventKey = QString("%1:%2").arg(nextRecording->GetChanID())
                          .arg(nextrectime.toString(Qt::ISODate));
            int i = 0;
            bool pendingEventSent = false;
            while (sysEventSecs[i] != 0)
            {
                if ((secsleft <= sysEventSecs[i]) &&
                    (!sysEvents[i].contains(sysEventKey)))
                {
                    if (!pendingEventSent)
                        SendMythSystemRecEvent
                            (QString("REC_PENDING SECS %1").arg(secsleft),
                             nextRecording);

                    sysEvents[i].append(sysEventKey);
                    pendingEventSent = true;
                }
                i++;
            }

            nexttv = (*m_tvList)[nextRecording->GetCardID()];

            if (nexttv->IsAsleep() && !nexttv->IsWaking())
            {
                VERBOSE(VB_SCHEDULE, QString("Slave Backend %1 is being "
                                             "awakened to record: %2")
                        .arg(nexttv->GetHostName())
                        .arg(nextRecording->GetTitle()));

                if (!WakeUpSlave(nexttv->GetHostName()))
                {
                    reschedQueue.enqueue(0);;
                    continue;
                }
            }
            else if ((nexttv->IsWaking()) &&
                     ((secsleft - prerollseconds) < 210) &&
                     (nexttv->GetSleepStatusTime().secsTo(curtime) < 300) &&
                     (nexttv->GetLastWakeTime().secsTo(curtime) > 10))
            {
                VERBOSE(VB_SCHEDULE, QString("Slave Backend %1 not "
                                             "available yet, trying to wake it up again.")
                        .arg(nexttv->GetHostName()));
                if (!WakeUpSlave(nexttv->GetHostName(), false))
                {
                    reschedQueue.enqueue(0);;
                    continue;
                }
            }
            else if ((nexttv->IsWaking()) &&
                     ((secsleft - prerollseconds) < 150) &&
                     (nexttv->GetSleepStatusTime().secsTo(curtime) < 300))
            {
                VERBOSE(VB_SCHEDULE,
                        QString("WARNING: Slave Backend %1 has NOT come "
                                "back from sleep yet in 150 seconds. Setting "
                                "slave status to unknown and attempting "
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

                reschedQueue.enqueue(0);;
            }
        }

        for ( recIter = startIter ; recIter != reclist.end(); ++recIter)
        {
            QString msg, details;
            int fsID = -1;

            nextRecording = *recIter;

            if (nextRecording->GetRecordingStatus() != rsWillRecord)
            {
                if (nextRecording->GetRecordingStatus() !=
                    nextRecording->oldrecstatus &&
                    nextRecording->GetRecordingStartTime() <= curtime)
                    nextRecording->AddHistory(false);
                continue;
            }

            nextrectime = nextRecording->GetRecordingStartTime();
            secsleft = curtime.secsTo(nextrectime);
            schedid = nextRecording->MakeUniqueSchedulerKey();

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
                break;

            if (m_tvList->find(nextRecording->GetCardID()) == m_tvList->end())
            {
                msg = QString("invalid cardid (%1) for %2")
                      .arg(nextRecording->GetCardID())
                      .arg(nextRecording->GetTitle());
                VERBOSE(VB_GENERAL, msg);

                nextRecording->SetRecordingStatus(rsTunerBusy);
                nextRecording->AddHistory(false);
                reschedQueue.enqueue(0);
                statuschanged = true;
                continue;
            }

            nexttv = (*m_tvList)[nextRecording->GetCardID()];
            // cerr << "nexttv = " << nextRecording->GetCardID();
            // cerr << " title: " << nextRecording->GetTitle() << endl;

            if (nexttv->IsTunerLocked())
            {
                msg = QString("SUPPRESSED recording \"%1\" on channel: "
                              "%2 on cardid: %3, sourceid %4. Tuner "
                              "is locked by an external application.")
                      .arg(nextRecording->GetTitle())
                      .arg(nextRecording->GetChanID())
                      .arg(nextRecording->GetCardID())
                      .arg(nextRecording->GetSourceID());
                QByteArray amsg = msg.toLocal8Bit();
                VERBOSE(VB_GENERAL, msg.constData());

                nextRecording->SetRecordingStatus(rsTunerBusy);
                nextRecording->AddHistory(false);
                reschedQueue.enqueue(0);
                statuschanged = true;
                continue;
            }

            if (!IsBusyRecording(nextRecording))
            {
                // Will use pre-roll settings only if no other
                // program is currently being recorded
                secsleft -= prerollseconds;
            }

            //VERBOSE(VB_GENERAL, secsleft << " seconds until " << nextRecording->GetTitle());

            if (secsleft > 30)
                continue;

            if (nexttv->IsWaking())
            {
                if (secsleft > 0)
                {
                    VERBOSE(VB_SCHEDULE,
                            QString("WARNING: Slave Backend %1 has NOT come "
                                    "back from sleep yet.  Recording can "
                                    "not begin yet for: %2")
                            .arg(nexttv->GetHostName())
                            .arg(nextRecording->GetTitle()));
                }
                else if (nexttv->GetLastWakeTime().secsTo(curtime) > 300)
                {
                    VERBOSE(VB_SCHEDULE,
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

                continue;
            }

            if (nextRecording->GetPathname().isEmpty())
            {
                QString recording_dir;
                fsID = FillRecordingDir(
                                        nextRecording->GetTitle(),
                                        nextRecording->GetHostname(),
                                        nextRecording->GetStorageGroup(),
                                        nextRecording->GetRecordingStartTime(),
                                        nextRecording->GetRecordingEndTime(),
                                        nextRecording->GetCardID(),
                                        recording_dir,
                                        reclist);
                nextRecording->SetPathname(recording_dir);
            }

            if (!recPendingList[schedid])
            {
                nexttv->RecordPending(nextRecording, max(secsleft, 0),
                                      hasLaterList.contains(schedid));
                recPendingList[schedid] = true;
            }

            if (secsleft > -2)
                continue;

            QDateTime recstartts = mythCurrentDateTime().addSecs(30);
            recstartts.setTime
                (QTime(recstartts.time().hour(), recstartts.time().minute()));
            nextRecording->SetRecordingStartTime(recstartts);

            details = QString("%1: channel %2 on cardid %3, sourceid %4")
                      .arg(nextRecording->toString(ProgramInfo::kTitleSubtitle))
                      .arg(nextRecording->GetChanID())
                      .arg(nextRecording->GetCardID())
                      .arg(nextRecording->GetSourceID());

            if (schedulingEnabled && nexttv->IsConnected())
            {
                nextRecording->SetRecordingStatus
                    (nexttv->StartRecording(nextRecording));
                nextRecording->SetReactivated(false);

                nextRecording->AddHistory(false);
                if (expirer)
                {
                    // activate auto expirer
                    expirer->Update(nextRecording->GetCardID(), fsID, true);
                }
            }
            else
                nextRecording->SetRecordingStatus(rsOffLine);
            bool doSchedAfterStart =
                (nextRecording->GetRecordingStatus() != rsRecording &&
                 nextRecording->GetRecordingStatus() != rsTuning) ||
                schedAfterStartMap[nextRecording->GetRecordingRuleID()] ||
                (nextRecording->GetParentRecordingRuleID() &&
                 schedAfterStartMap[nextRecording->GetParentRecordingRuleID()]);
            nextRecording->AddHistory(false);
            if (doSchedAfterStart)
                reschedQueue.enqueue(0);

            statuschanged = true;

            bool is_rec = (nextRecording->GetRecordingStatus() == rsRecording ||
                           nextRecording->GetRecordingStatus() == rsTuning);
            msg = is_rec ?
                  QString("Started recording") :
                  QString("Canceled recording (%1)")
                  .arg(toString(nextRecording->GetRecordingStatus(),
                                nextRecording->GetRecordingRuleType()));

            VERBOSE(VB_GENERAL, QString("%1: %2").arg(msg).arg(details));
            gCoreContext->LogEntry("scheduler", LP_NOTICE, msg, details);

            if (is_rec)
                UpdateNextRecord();

            if (nextRecording->GetRecordingStatus() == rsFailed)
            {
                MythEvent me(QString("FORCE_DELETE_RECORDING %1 %2")
                             .arg(nextRecording->GetChanID())
                             .arg(nextRecording->GetRecordingStartTime(ISODate)));
                gCoreContext->dispatch(me);
            }
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
            // we release the block when a client connects
            if (blockShutdown)
                blockShutdown &= !m_mainServer->isClientConnected();
            else
            {
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

                    if (!idleSince.isValid())
                    {
                        RecIter idleIter = reclist.begin();
                        for ( ; idleIter != reclist.end(); ++idleIter)
                            if ((*idleIter)->GetRecordingStatus() ==
                                rsWillRecord)
                                break;

                        if (idleIter != reclist.end())
                        {
                            if (curtime.secsTo
                                ((*idleIter)->GetRecordingStartTime()) -
                                prerollseconds >
                                (idleWaitForRecordingTime * 60) +
                                idleTimeoutSecs)
                            {
                                idleSince = curtime;
                            }
                        }
                        else
                            idleSince = curtime;
                    }
                    else
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
                                    VERBOSE(VB_IMPORTANT, "Waited more than 60"
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
                                VERBOSE(VB_IMPORTANT, msg);
                                MythEvent me(QString("SHUTDOWN_COUNTDOWN %1")
                                             .arg(idleTimeoutSecs));
                                gCoreContext->dispatch(me);
                            }
                            else if (itime % 10 == 0)
                            {
                                msg = QString("%1 secs left to system "
                                              "shutdown!")
                                      .arg(idleTimeoutSecs - itime);
                                VERBOSE(VB_IDLE, msg);
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
        int state = myth_system(preSDWUCheckCommand);

        if (GENERIC_EXIT_NOT_OK != state)
        {
            retval = false;
            switch(state)
            {
                case 0:
                    VERBOSE(VB_GENERAL, "CheckShutdownServer returned - OK to shutdown");
                    retval = true;
                    break;
                case 1:
                    VERBOSE(VB_IDLE, "CheckShutdownServer returned - Not OK to shutdown");
                    // just reset idle'ing on retval == 1
                    idleSince = QDateTime();
                    break;
                case 2:
                    VERBOSE(VB_IDLE, "CheckShutdownServer returned - Not OK to shutdown, need reconnect");
                    // reset shutdown status on retval = 2
                    // (needs a clientconnection again,
                    // before shutdown is executed)
                    blockShutdown
                             = gCoreContext->GetNumSetting("blockSDWUwithoutClient",
                                                       1);
                    idleSince = QDateTime();
                    break;
                // case 3:
                //    //disable shutdown routine generally
                //    m_noAutoShutdown = true;
                //    break;
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
            VERBOSE(VB_IMPORTANT, "SetWakeuptimeCommand is empty, shutdown aborted");
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

        VERBOSE(VB_GENERAL, QString("Running the command to set the next "
                                    "scheduled wakeup time :-\n\t\t\t\t\t\t") +
                                    setwakeup_cmd);

        // now run the command to set the wakeup time
        if (myth_system(setwakeup_cmd))
        {
            VERBOSE(VB_IMPORTANT, "SetWakeuptimeCommand failed, "
                    "shutdown aborted");
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

        VERBOSE(VB_GENERAL, QString("Running the command to shutdown "
                                    "this computer :-\n\t\t\t\t\t\t") + halt_cmd);

        // and now shutdown myself
        schedLock.unlock();
        unsigned int res = myth_system(halt_cmd);
        schedLock.lock();
        if (!res)
            return;
        VERBOSE(VB_IMPORTANT, "ServerHaltCommand failed, shutdown aborted");
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

    VERBOSE(VB_SCHEDULE,
            "Scheduler, Checking for slaves that can be shut down");

    int sleepThreshold =
        gCoreContext->GetNumSetting( "SleepThreshold", 60 * 45);

    VERBOSE(VB_SCHEDULE+VB_EXTRA, QString("  Getting list of slaves that "
            "will be active in the next %1 minutes.")
            .arg(sleepThreshold / 60));

    VERBOSE(VB_SCHEDULE+VB_EXTRA, "Checking scheduler's reclist");
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
                    VERBOSE(VB_SCHEDULE+VB_EXTRA, QString("    Slave %1 will "
                            "be in use in %2 minutes").arg(enc->GetHostName())
                            .arg(secsleft / 60));
                else
                    VERBOSE(VB_SCHEDULE+VB_EXTRA, QString("    Slave %1 is "
                            "in use currently recording '%1'")
                            .arg(enc->GetHostName()).arg(pginfo->GetTitle()));
                SlavesInUse << enc->GetHostName();
            }
        }
    }

    VERBOSE(VB_SCHEDULE+VB_EXTRA, "  Checking inuseprograms table:");
    QDateTime oneHourAgo = QDateTime::currentDateTime().addSecs(-61 * 60);
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT DISTINCT hostname, recusage FROM inuseprograms "
                    "WHERE lastupdatetime > :ONEHOURAGO ;");
    query.bindValue(":ONEHOURAGO", oneHourAgo);
    if (query.exec())
    {
        while(query.next()) {
            SlavesInUse << query.value(0).toString();
            VERBOSE(VB_SCHEDULE+VB_EXTRA, QString("    Slave %1 is marked as "
                    "in use by a %2")
                    .arg(query.value(0).toString())
                    .arg(query.value(1).toString()));
        }
    }

    VERBOSE(VB_SCHEDULE+VB_EXTRA, QString("  Shutting down slaves which will "
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
            QString sleepCommand = gCoreContext->GetSettingOnHost("SleepCommand",
                enc->GetHostName());
            QString wakeUpCommand = gCoreContext->GetSettingOnHost("WakeUpCommand",
                enc->GetHostName());

            if (!sleepCommand.isEmpty() && !wakeUpCommand.isEmpty())
            {
                QString thisHost = enc->GetHostName();

                VERBOSE(VB_SCHEDULE+VB_EXTRA, QString("    Commanding %1 to "
                        "go to sleep.").arg(thisHost));

                if (enc->GoToSleep())
                {
                    QMap<int, EncoderLink *>::Iterator slviter =
                        m_tvList->begin();
                    for (; slviter != m_tvList->end(); ++slviter)
                    {
                        EncoderLink *slv = *slviter;
                        if (slv->GetHostName() == thisHost)
                        {
                            VERBOSE(VB_SCHEDULE+VB_EXTRA,
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
                    VERBOSE(VB_IMPORTANT, LOC_ERR + QString("Unable to "
                            "shutdown %1 slave backend, setting sleep "
                            "status to undefined.").arg(thisHost));
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
        VERBOSE(VB_IMPORTANT, QString("Tried to Wake Up %1, but this is the "
                "master backend and it is not asleep.")
                .arg(slaveHostname));
        return false;
    }

    QString wakeUpCommand = gCoreContext->GetSettingOnHost( "WakeUpCommand",
        slaveHostname);

    if (wakeUpCommand.isEmpty()) {
        VERBOSE(VB_IMPORTANT, QString("Trying to Wake Up %1, but this slave "
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
        VERBOSE(VB_SCHEDULE, QString("Executing '%1' to wake up slave.")
                .arg(wakeUpCommand));
        myth_system(wakeUpCommand);
    }
    else
        return WakeOnLAN(wakeUpCommand);

    return true;
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
        VERBOSE(VB_SCHEDULE,
                QString("Scheduler, Sending wakeup command to slave: %1")
                        .arg(thisSlave));
        WakeUpSlave(thisSlave, false);
    }
}

void *Scheduler::SchedulerThread(void *param)
{
    // Lower scheduling priority, to avoid problems with recordings.
    if (setpriority(PRIO_PROCESS, 0, 9))
        VERBOSE(VB_IMPORTANT, LOC + "Setting priority failed." + ENO);
    Scheduler *sched = static_cast<Scheduler*>(param);
    sched->RunScheduler();

    return NULL;
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
        VERBOSE(VB_IMPORTANT, QString("Invalid rectype for manual "
                                      "recordid %1").arg(recordid));
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
            VERBOSE(VB_IMPORTANT, QString("Invalid search key in recordid %1")
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
            where << ((QString("%1.recordid = ").arg(recordTable)) + bindrecid + " AND " +
                              QString("program.manualid = %1.recordid ").arg(recordTable));
            break;
        default:
            VERBOSE(VB_IMPORTANT, QString("Unknown RecSearchType "
                                         "(%1) for recordid %2")
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

    if (VERBOSE_LEVEL_CHECK(VB_SCHEDULE))
    {
        for (clause = 0; clause < fromclauses.count(); ++clause)
        {
            QString msg = QString("Query %1: %2/%3")
                .arg(clause).arg(fromclauses[clause]).arg(whereclauses[clause]);
            cout << msg.toLocal8Bit().constData() << endl;
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
    QString(" AND channel.visible = 1 AND "
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

        VERBOSE(VB_SCHEDULE, QString(" |-- Start DB Query %1...").arg(clause));

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

        VERBOSE(VB_SCHEDULE, QString(" |-- %1 results in %2 sec.")
                .arg(result.size())
                .arg(((dbend.tv_sec  - dbstart.tv_sec) * 1000000 +
                      (dbend.tv_usec - dbstart.tv_usec)) / 1000000.0));

    }

    VERBOSE(VB_SCHEDULE, " +-- Done.");
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
"       (((RECTABLE.dupmethod & 0x08) = 0) OR (program.subtitle <> '' "
"          AND program.subtitle = oldrecorded.subtitle) OR (program.subtitle = ''  "
"          AND oldrecorded.subtitle = '' AND program.description <> '' "
"          AND program.description = oldrecorded.description)) "
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
"       (((RECTABLE.dupmethod & 0x08) = 0) OR (program.subtitle <> '' "
"          AND program.subtitle = recorded.subtitle) OR (program.subtitle = ''  "
"          AND recorded.subtitle = '' AND program.description <> '' "
"          AND program.description = recorded.description)) "
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
" WHERE program.endtime >= NOW() - INTERVAL 1 DAY "
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
        "    p.programid,      p.category_type,      p.airdate,         "//27-29
        "    p.stars,          p.originalairdate,    RECTABLE.inactive, "//30-32
        "    RECTABLE.parentid, ") + progfindid + ", RECTABLE.playgroup,"//33-35
        "    oldrecstatus.recstatus, oldrecstatus.reactivate, "
        "                                            p.videoprop+0,     "//36-38
        "    p.subtitletypes+0, p.audioprop+0,   RECTABLE.storagegroup, "//39-41
        "    capturecard.hostname, recordmatch.oldrecstatus, "
        "                                           RECTABLE.avg_delay, "//42-44
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

    VERBOSE(VB_SCHEDULE, QString(" |-- Start DB Query..."));

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

    VERBOSE(VB_SCHEDULE, QString(" |-- %1 results in %2 sec. Processing...")
            .arg(result.size())
            .arg(((dbend.tv_sec  - dbstart.tv_sec) * 1000000 +
                  (dbend.tv_usec - dbstart.tv_usec)) / 1000000.0));

    while (result.next())
    {
        RecordingInfo *p = new RecordingInfo(
            result.value(4).toString(),//title
            result.value(5).toString(),//subtitle
            result.value(6).toString(),//description
            result.value(11).toString(),//category

            result.value(0).toUInt(),//chanid
            result.value(7).toString(),//channum
            result.value(8).toString(),//chansign
            result.value(9).toString(),//channame

            result.value(21).toString(),//recgroup
            result.value(35).toString(),//playgroup

            result.value(42).toString(),//hostname
            result.value(41).toString(),//storagegroup

            result.value(29).toUInt(),//year

            result.value(26).toString(),//seriesid
            result.value(27).toString(),//programid
            result.value(28).toString(),//catType

            result.value(12).toInt(),//recpriority

            result.value(2).toDateTime(),//startts
            result.value(3).toDateTime(),//endts
            result.value(18).toDateTime(),//recstartts
            result.value(19).toDateTime(),//recendts

            result.value(30).toDouble(),//stars
            (result.value(31).isNull()) ? QDate() :
            QDate::fromString(result.value(31).toString(), Qt::ISODate),
            //originalAirDate

            result.value(20).toInt(),//repeat

            RecStatusType(result.value(36).toInt()),//oldrecstatus
            result.value(37).toInt(),//reactivate

            result.value(17).toUInt(),//recordid
            result.value(33).toUInt(),//parentid
            RecordingType(result.value(16).toInt()),//rectype
            RecordingDupInType(result.value(13).toInt()),//dupin
            RecordingDupMethodType(result.value(22).toInt()),//dupmethod

            result.value(1).toUInt(),//sourceid
            result.value(25).toUInt(),//inputid
            result.value(24).toUInt(),//cardid

            result.value(34).toUInt(),//findid

            result.value(23).toInt() == COMM_DETECT_COMMFREE,//commfree
            result.value(39).toUInt(),//subtitleType
            result.value(38).toUInt(),//videoproperties
            result.value(40).toUInt());//audioproperties

        if (!recTypeRecPriorityMap.contains(p->GetRecordingRuleType()))
        {
            recTypeRecPriorityMap[p->GetRecordingRuleType()] =
                p->GetRecordingTypeRecPriority(p->GetRecordingRuleType());
        }

        p->SetRecordingPriority(
            p->GetRecordingPriority() + recTypeRecPriorityMap[p->GetRecordingRuleType()] +
            result.value(45).toInt() +
            ((autopriority) ?
             autopriority - (result.value(44).toInt() * autostrata / 200) : 0));

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
        if ((threadrunning || specsched) && !cardMap.contains(p->GetCardID()))
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
                if (result.value(43).toInt() == rsNeverRecord)
                    newrecstatus = rsNeverRecord;
                else
                    newrecstatus = rsPreviousRecording;
            }

            if ((dupin & kDupsInRecorded) && result.value(14).toInt())
                newrecstatus = rsCurrentRecording;
        }

        bool inactive = result.value(32).toInt();
        if (inactive)
            newrecstatus = rsInactive;

        // Mark anything that has already passed as missed.  If it
        // survives PruneOverlaps, it will get deleted or have its old
        // status restored in PruneRedundants.
        if (p->GetRecordingEndTime() < schedTime)
            newrecstatus = rsMissed;

        p->SetRecordingStatus(newrecstatus);

        tmpList.push_back(p);
    }

    VERBOSE(VB_SCHEDULE, " +-- Cleanup...");
    RecIter tmp = tmpList.begin();
    for ( ; tmp != tmpList.end(); ++tmp)
        worklist.push_back(*tmp);

    if (schedTmpRecord == "sched_temp_record")
    {
        result.prepare("DROP TABLE IF EXISTS sched_temp_record;");
        if (!result.exec())
            MythDB::DBError("AddNewRecords sched_temp_record", query);
    }

    result.prepare("DROP TABLE IF EXISTS sched_temp_recorded;");
    if (!result.exec())
        MythDB::DBError("AddNewRecords drop table", query);
}

void Scheduler::AddNotListed(void) {

    struct timeval dbstart, dbend;
    RecList tmpList;

    QString query = QString(
        "SELECT RECTABLE.title,       RECTABLE.subtitle,    " //  0,1
        "       RECTABLE.description, RECTABLE.category,    " //  2,3
        "       RECTABLE.chanid,      channel.channum,      " //  4,5
        "       RECTABLE.station,     channel.name,         " //  6,7
        "       RECTABLE.recgroup,    RECTABLE.playgroup,   " //  8,9
        "       RECTABLE.seriesid,    RECTABLE.programid,   " // 10,11
        "       RECTABLE.recpriority,                       " // 12
        "       RECTABLE.startdate,   RECTABLE.starttime,   " // 13,14
        "       RECTABLE.enddate,     RECTABLE.endtime,     " // 15,16
        "       RECTABLE.recordid,    RECTABLE.type,        " // 17,18
        "       RECTABLE.dupin,       RECTABLE.dupmethod,   " // 19,20
        "       RECTABLE.findid,                            " // 21
        "       RECTABLE.startoffset, RECTABLE.endoffset,   " // 22,23
        "       channel.commmethod                          " // 24
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

    VERBOSE(VB_SCHEDULE, QString(" |-- Start DB Query..."));

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

    VERBOSE(VB_SCHEDULE, QString(" |-- %1 results in %2 sec. Processing...")
            .arg(result.size())
            .arg(((dbend.tv_sec  - dbstart.tv_sec) * 1000000 +
                  (dbend.tv_usec - dbstart.tv_usec)) / 1000000.0));

    QDateTime now = QDateTime::currentDateTime();

    while (result.next())
    {
        RecordingType rectype = RecordingType(result.value(18).toInt());
        QDateTime startts(result.value(13).toDate(), result.value(14).toTime());
        QDateTime endts(  result.value(15).toDate(), result.value(16).toTime());

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

        QDateTime recstartts = startts.addSecs(result.value(22).toInt() * -60);
        QDateTime recendts   = endts.addSecs(  result.value(23).toInt() * +60);

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
            result.value(0).toString(),
            (sor) ? result.value(1).toString() : QString(),
            (sor) ? result.value(2).toString() : QString(),
            QString(),

            result.value(4).toUInt(),
            result.value(5).toString(),
            result.value(6).toString(),
            result.value(7).toString(),

            result.value(8).toString(),  result.value(9).toString(),

            result.value(10).toString(), result.value(11).toString(),

            result.value(12).toInt(),

            startts,                     endts,
            recstartts,                  recendts,

            rsNotListed,

            result.value(17).toUInt(),   RecordingType(result.value(18).toInt()),

            RecordingDupInType(result.value(19).toInt()),
            RecordingDupMethodType(result.value(20).toInt()),

            result.value(21).toUInt(),

            result.value(24).toInt() == COMM_DETECT_COMMFREE);

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
        "       RECTABLE.description, RECTABLE.category,    " //  2,3
        "       RECTABLE.chanid,      channel.channum,      " //  4,5
        "       RECTABLE.station,     channel.name,         " //  6,7
        "       RECTABLE.recgroup,    RECTABLE.playgroup,   " //  8,9
        "       RECTABLE.seriesid,    RECTABLE.programid,   " // 10,11
        "       RECTABLE.recpriority,                       " // 12
        "       RECTABLE.startdate,   RECTABLE.starttime,   " // 13,14
        "       RECTABLE.enddate,     RECTABLE.endtime,     " // 15,16
        "       RECTABLE.recordid,    RECTABLE.type,        " // 17,18
        "       RECTABLE.dupin,       RECTABLE.dupmethod,   " // 19,20
        "       RECTABLE.findid,                            " // 21
        "       channel.commmethod                          " // 22
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
        RecordingType rectype = RecordingType(result.value(18).toInt());
        QDateTime startts;
        QDateTime endts;
        if (rectype == kSingleRecord   ||
            rectype == kDontRecord     ||
            rectype == kOverrideRecord ||
            rectype == kTimeslotRecord ||
            rectype == kWeekslotRecord)
        {
            startts = QDateTime(result.value(13).toDate(),
                                result.value(14).toTime());
            endts = QDateTime(result.value(15).toDate(),
                              result.value(16).toTime());
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
            result.value(2).toString(),  result.value(3).toString(),

            result.value(4).toUInt(),    result.value(5).toString(),
            result.value(6).toString(),  result.value(7).toString(),

            result.value(8).toString(),  result.value(9).toString(),

            result.value(10).toString(), result.value(11).toString(),

            result.value(12).toInt(),

            startts, endts,
            startts, endts,

            rsUnknown,

            result.value(17).toUInt(),   rectype,
            RecordingDupInType(result.value(19).toInt()),
            RecordingDupMethodType(result.value(20).toInt()),

            result.value(21).toUInt(),

            result.value(22).toInt() == COMM_DETECT_COMMFREE));
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
    if (a->isLocal && !b->isLocal)
    {
        if (a->weight <= b->weight)
        {
            return true;
        }
    }
    else if (a->isLocal == b->isLocal)
    {
        if (a->weight < b->weight)
        {
            return true;
        }
        else if (a->weight > b->weight)
        {
            return false;
        }
        else if (a->freeSpaceKB > b->freeSpaceKB)
        {
            return true;
        }
    }
    else if (!a->isLocal && b->isLocal)
    {
        if (a->weight < b->weight)
        {
            return true;
        }
    }

    return false;
}

// prefer dirs with more free space over dirs with less
static bool comp_storage_free_space(FileSystemInfo *a, FileSystemInfo *b)
{
    if (a->freeSpaceKB > b->freeSpaceKB)
        return true;

    return false;
}

// prefer dirs with less weight (disk I/O) over dirs with more weight
static bool comp_storage_disk_io(FileSystemInfo *a, FileSystemInfo *b)
{
    if (a->weight < b->weight)
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

    VERBOSE(VB_FILE, LOC + QString("FindNextLiveTVDir: next dir is '%1'")
            .arg(recording_dir));

    if (expirer) // update auto expirer
        expirer->Update(cardid, fsID, true);
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
    VERBOSE(VB_SCHEDULE, LOC + "FillRecordingDir: Starting");

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
        VERBOSE(VB_FILE|VB_SCHEDULE, LOC + QString("FillRecordingDir: The only "
                "directory in the %1 Storage Group is %2, so it will be used "
                "by default.")
                .arg(storagegroup)
                .arg(dirlist[0]));
        recording_dir = dirlist[0];
        VERBOSE(VB_SCHEDULE, LOC + "FillRecordingDir: Finished");

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

    VERBOSE(VB_FILE|VB_SCHEDULE, LOC +
            "FillRecordingDir: Calculating initial FS Weights.");

    for (fsit = fsInfoCache.begin(); fsit != fsInfoCache.end(); ++fsit)
    {
        FileSystemInfo *fs = &(*fsit);
        int tmpWeight = 0;

        QString msg = QString("  %1:%2").arg(fs->hostname)
                              .arg(fs->directory);
        if (fs->isLocal)
        {
            tmpWeight = localStartingWeight;
            msg += " is local (" + QString::number(tmpWeight) + ")";
        }
        else
        {
            tmpWeight = remoteStartingWeight;
            msg += " is remote (+" + QString::number(tmpWeight) + ")";
        }

        fs->weight = tmpWeight;

        tmpWeight = gCoreContext->GetNumSetting(QString("SGweightPerDir:%1:%2")
                                .arg(fs->hostname).arg(fs->directory), 0);
        fs->weight += tmpWeight;

        if (tmpWeight)
            msg += ", has SGweightPerDir offset of "
                   + QString::number(tmpWeight) + ")";

        msg += ". initial dir weight = " + QString::number(fs->weight);
        VERBOSE(VB_FILE|VB_SCHEDULE, msg);

        fsInfoList.push_back(fs);
    }

    VERBOSE(VB_FILE|VB_SCHEDULE, LOC +
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
                if ((recHost == fs->hostname) &&
                    (recDir == fs->directory))
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
                        VERBOSE(VB_FILE|VB_SCHEDULE, QString(
                                "  %1 @ %2 in use by '%3' on %4:%5, FSID #%6, "
                                "FSID weightOffset +%7.")
                                .arg(recChanid)
                                .arg(recStart.toString(Qt::ISODate))
                                .arg(recUsage).arg(recHost).arg(recDir)
                                .arg(fs->fsID).arg(weightOffset));

                        // need to offset all directories on this filesystem
                        for (fsit2 = fsInfoCache.begin();
                             fsit2 != fsInfoCache.end(); ++fsit2)
                        {
                            FileSystemInfo *fs2 = &(*fsit2);
                            if (fs2->fsID == fs->fsID)
                            {
                                VERBOSE(VB_FILE|VB_SCHEDULE, QString("    "
                                        "%1:%2 => old weight %3 plus %4 = %5")
                                        .arg(fs2->hostname).arg(fs2->directory)
                                        .arg(fs2->weight).arg(weightOffset)
                                        .arg(fs2->weight + weightOffset));

                                fs2->weight += weightOffset;
                            }
                        }
                    }
                    break;
                }
            }
        }
    }

    VERBOSE(VB_FILE|VB_SCHEDULE, LOC +
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
            if ((fs->hostname == thispg->GetHostname()) &&
                (fs->directory == thispg->GetPathname()))
            {
                VERBOSE(VB_FILE|VB_SCHEDULE, QString(
                        "%1 @ %2 will record on %3:%4, FSID #%5, "
                        "weightPerRecording +%6.")
                        .arg(thispg->GetChanID())
                        .arg(thispg->GetRecordingStartTime(ISODate))
                        .arg(fs->hostname).arg(fs->directory)
                        .arg(fs->fsID).arg(weightPerRecording));

                for (fsit2 = fsInfoCache.begin();
                     fsit2 != fsInfoCache.end(); ++fsit2)
                {
                    FileSystemInfo *fs2 = &(*fsit2);
                    if (fs2->fsID == fs->fsID)
                    {
                        VERBOSE(VB_FILE|VB_SCHEDULE, QString("    "
                                "%1:%2 => old weight %3 plus %4 = %5")
                                .arg(fs2->hostname).arg(fs2->directory)
                                .arg(fs2->weight).arg(weightPerRecording)
                                .arg(fs2->weight + weightPerRecording));

                        fs2->weight += weightPerRecording;
                    }
                }
                break;
            }
        }
    }

    VERBOSE(VB_FILE|VB_SCHEDULE, QString("Using '%1' Storage Scheduler "
            "directory sorting algorithm.").arg(storageScheduler));

    if (storageScheduler == "BalancedFreeSpace")
        fsInfoList.sort(comp_storage_free_space);
    else if (storageScheduler == "BalancedDiskIO")
        fsInfoList.sort(comp_storage_disk_io);
    else // default to using original method
        fsInfoList.sort(comp_storage_combination);

    if (VERBOSE_LEVEL_CHECK(VB_FILE|VB_SCHEDULE))
    {
        cout << "--- FillRecordingDir Sorted fsInfoList start ---\n";
        for (fslistit = fsInfoList.begin();fslistit != fsInfoList.end();
             ++fslistit)
        {
            FileSystemInfo *fs = *fslistit;
            QString msg = QString(
                "%1:%2\n"
                "    Location    : %3\n"
                "    weight      : %4\n"
                "    free space  : %5")
                .arg(fs->hostname).arg(fs->directory)
                .arg((fs->isLocal) ? "local" : "remote")
                .arg(fs->weight)
                .arg(fs->freeSpaceKB);
            cout << msg.toLocal8Bit().constData() << endl;
        }
        cout << "--- FillRecordingDir Sorted fsInfoList end ---\n";
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
         (expirer) &&
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
                remainingSpaceKB[(*fslistit)->fsID] = (*fslistit)->freeSpaceKB;
            }

            // get list of expirable programs
            pginfolist_t expiring;
            expirer->GetAllExpiring(expiring);

            for(pginfolist_t::iterator it=expiring.begin();
                it != expiring.end(); ++it)
            {
                // find the filesystem its on
                FileSystemInfo *fs=NULL;
                for (fslistit = fsInfoList.begin();
                    fslistit != fsInfoList.end(); ++fslistit)
                {
                    // recording is not on this filesystem's host
                    if ((*it)->GetHostname() != (*fslistit)->hostname)
                        continue;

                    // directory is not in the Storage Group dir list
                    if (!dirlist.contains((*fslistit)->directory))
                        continue;

                    QString filename =
                        (*fslistit)->directory + "/" + (*it)->GetPathname();

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
                        if (foundSlave && programinfo->GetPathname() == filename)
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
                    VERBOSE(VB_IMPORTANT, QString("Unable to match '%1' "
                            "to any file system.  Ignoring it.")
                            .arg((*it)->GetBasename()));
                    continue;
                }

                // add this files size to the remaining free space
                remainingSpaceKB[fs->fsID] += (*it)->GetFilesize() / 1024;

                // check if we have enough space for new file
                long long desiredSpaceKB = expirer->GetDesiredSpace(fs->fsID);

                if (remainingSpaceKB[fs->fsID] > (desiredSpaceKB + maxSizeKB))
                {
                    recording_dir = fs->directory;
                    fsID = fs->fsID;

                    VERBOSE(VB_FILE, QString(
                                "pass 2: '%1' will record in '%2' "
                                "although there is only %3 MB free and the "
                                "AutoExpirer wants at least %4 MB.  This "
                                "directory has the highest priority files "
                                "to be expired from the AutoExpire list and "
                                "there are enough that the Expirer should "
                                "be able to free up space for this recording.")
                            .arg(title).arg(recording_dir)
                            .arg(fs->freeSpaceKB / 1024)
                            .arg(desiredSpaceKB / 1024));

                    foundDir = true;
                    break;
                }
            }

            expirer->ClearExpireList(expiring);
        }
        else // passes 1 & 3 (or 1 & 2 if !simulateAutoExpire)
        {
            for (fslistit = fsInfoList.begin();
                fslistit != fsInfoList.end(); ++fslistit)
            {
                long long desiredSpaceKB = 0;
                FileSystemInfo *fs = *fslistit;
                if (expirer)
                    desiredSpaceKB = expirer->GetDesiredSpace(fs->fsID);

                if ((fs->hostname == hostname) &&
                    (dirlist.contains(fs->directory)) &&
                    ((pass > 1) ||
                     (fs->freeSpaceKB > (desiredSpaceKB + maxSizeKB))))
                {
                    recording_dir = fs->directory;
                    fsID = fs->fsID;

                    if (pass == 1)
                        VERBOSE(VB_FILE, QString("pass 1: '%1' will record in "
                                "'%2' which has %3 MB free. This recording "
                                "could use a max of %4 MB and the AutoExpirer "
                                "wants to keep %5 MB free.")
                                .arg(title)
                                .arg(recording_dir)
                                .arg(fs->freeSpaceKB / 1024)
                                .arg(maxSizeKB / 1024)
                                .arg(desiredSpaceKB / 1024));
                    else
                        VERBOSE(VB_FILE, QString("pass %1: '%2' will record in "
                                "'%3' although there is only %4 MB free and "
                                "the AutoExpirer wants at least %5 MB.  "
                                "Something will have to be deleted or expired "
                                "in order for this recording to complete "
                                "successfully.")
                                .arg(pass).arg(title)
                                .arg(recording_dir)
                                .arg(fs->freeSpaceKB / 1024)
                                .arg(desiredSpaceKB / 1024));

                    foundDir = true;
                    break;
                }
            }
        }

        if (foundDir)
            break;
    }

    VERBOSE(VB_SCHEDULE, LOC + "FillRecordingDir: Finished");
    return fsID;
}

void Scheduler::FillDirectoryInfoCache(bool force)
{
    if ((!force) &&
        (fsInfoCacheFillTime > QDateTime::currentDateTime().addSecs(-180)))
        return;

    vector<FileSystemInfo> fsInfos;

    fsInfoCache.clear();

    if (m_mainServer)
        m_mainServer->GetFilesystemInfos(fsInfos);

    QMap <int, bool> fsMap;
    vector<FileSystemInfo>::iterator it1;
    for (it1 = fsInfos.begin(); it1 != fsInfos.end(); ++it1)
    {
        fsMap[it1->fsID] = true;
        fsInfoCache[it1->hostname + ":" + it1->directory] = *it1;
    }

    VERBOSE(VB_FILE, LOC + QString("FillDirectoryInfoCache: found %1 unique "
            "filesystems").arg(fsMap.size()));

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
            VERBOSE(VB_SCHEDULE,
                    "Close to auto-start time, AUTO-Startup assumed");
            autoStart = true;
        }
        else
            VERBOSE(VB_SCHEDULE+VB_EXTRA, "NOT close to auto-start time, "
                    "USER-initiated startup assumed");
    }
    else if (!s.isEmpty())
        VERBOSE(VB_IMPORTANT, LOC_ERR + QString("Invalid "
                "MythShutdownWakeupTime specified in database (%1)").arg(s));

    return autoStart;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
