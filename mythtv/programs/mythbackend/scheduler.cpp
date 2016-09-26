#include <unistd.h>

#include <iostream>
#include <algorithm>
#include <list>
using namespace std;

#ifdef __linux__
#  include <sys/vfs.h>
#else // if !__linux__
#  include <sys/param.h>
#  ifndef _WIN32
#    include <sys/mount.h>
#  endif // _WIN32
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

#include "mythmiscutil.h"
#include "mythsystemlegacy.h"
#include "scheduler.h"
#include "encoderlink.h"
#include "mainserver.h"
#include "remoteutil.h"
#include "backendutil.h"
#include "mythdate.h"
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
    schedulingEnabled(true),
    m_tvList(tvList),
    m_expirer(NULL),
    doRun(runthread),
    m_mainServer(NULL),
    resetIdleTime(false),
    m_isShuttingDown(false),
    error(0),
    livetvTime(QDateTime()),
    lastPrepareTime(QDateTime()),
    m_openEnd(openEndNever)
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

    InitInputInfoMap();

    fsInfoCacheFillTime = MythDate::current().addSecs(-1000);

    if (doRun)
    {
        ProgramInfo::CheckProgramIDAuthorities();
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

    while (!conflictlists.empty())
    {
        delete conflictlists.back();
        conflictlists.pop_back();
    }

    sinputinfomap.clear();

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
            "SELECT cardid "
            "FROM capturecard "
            "WHERE sourceid = :SOURCEID "
            "ORDER BY cardid;");
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
    return (p->GetRecordingStatus() == RecStatus::Recording ||
            p->GetRecordingStatus() == RecStatus::Tuning ||
            p->GetRecordingStatus() == RecStatus::Failing ||
            p->GetRecordingStatus() == RecStatus::WillRecord ||
            p->GetRecordingStatus() == RecStatus::Pending);
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
    int aprec = RecTypePrecedence(a->GetRecordingRuleType());
    if (a->GetRecordingStatus() != RecStatus::Unknown &&
        a->GetRecordingStatus() != RecStatus::DontRecord)
    {
        aprec += 100;
    }
    int bprec = RecTypePrecedence(b->GetRecordingRuleType());
    if (b->GetRecordingStatus() != RecStatus::Unknown &&
        b->GetRecordingStatus() != RecStatus::DontRecord)
    {
        bprec += 100;
    }
    if (aprec != bprec)
        return aprec < bprec;

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
    int cmp = a->GetTitle().compare(b->GetTitle(), Qt::CaseInsensitive);
    if (cmp != 0)
        return cmp < 0;
    if (a->GetRecordingRuleID() != b->GetRecordingRuleID())
        return a->GetRecordingRuleID() < b->GetRecordingRuleID();
    cmp = a->GetChannelSchedulingID().compare(b->GetChannelSchedulingID(),
                                              Qt::CaseInsensitive);
    if (cmp != 0)
        return cmp < 0;
    if (a->GetRecordingStatus() != b->GetRecordingStatus())
        return a->GetRecordingStatus() < b->GetRecordingStatus();
    cmp = a->GetChanNum().compare(b->GetChanNum(), Qt::CaseInsensitive);
    return cmp < 0;
}

static bool comp_recstart(RecordingInfo *a, RecordingInfo *b)
{
    if (a->GetRecordingStartTime() != b->GetRecordingStartTime())
        return a->GetRecordingStartTime() < b->GetRecordingStartTime();
    int cmp = a->GetChannelSchedulingID().compare(b->GetChannelSchedulingID(),
                                                  Qt::CaseInsensitive);
    if (cmp != 0)
        return cmp < 0;
    if (a->GetRecordingEndTime() != b->GetRecordingEndTime())
        return a->GetRecordingEndTime() < b->GetRecordingEndTime();
    if (a->GetRecordingStatus() != b->GetRecordingStatus())
        return a->GetRecordingStatus() < b->GetRecordingStatus();
    if (a->GetChanNum() != b->GetChanNum())
        return a->GetChanNum() < b->GetChanNum();
    return a->GetChanID() < b->GetChanID();
}

static bool comp_priority(RecordingInfo *a, RecordingInfo *b)
{
    int arec = (a->GetRecordingStatus() != RecStatus::Recording &&
                a->GetRecordingStatus() != RecStatus::Tuning &&
                a->GetRecordingStatus() != RecStatus::Failing &&
                a->GetRecordingStatus() != RecStatus::Pending);
    int brec = (b->GetRecordingStatus() != RecStatus::Recording &&
                b->GetRecordingStatus() != RecStatus::Tuning &&
                b->GetRecordingStatus() != RecStatus::Failing &&
                b->GetRecordingStatus() != RecStatus::Pending);

    if (arec != brec)
        return arec < brec;

    if (a->GetRecordingPriority() != b->GetRecordingPriority())
        return a->GetRecordingPriority() > b->GetRecordingPriority();

    if (a->GetRecordingPriority2() != b->GetRecordingPriority2())
        return a->GetRecordingPriority2() > b->GetRecordingPriority2();

    int atype = (a->GetRecordingRuleType() == kOverrideRecord ||
                 a->GetRecordingRuleType() == kSingleRecord);
    int btype = (b->GetRecordingRuleType() == kOverrideRecord ||
                 b->GetRecordingRuleType() == kSingleRecord);
    if (atype != btype)
        return atype > btype;

    QDateTime pasttime = MythDate::current().addSecs(-30);
    int apast = (a->GetRecordingStartTime() < pasttime && !a->IsReactivated());
    int bpast = (b->GetRecordingStartTime() < pasttime && !b->IsReactivated());
    if (apast != bpast)
        return apast < bpast;

    if (a->GetRecordingStartTime() != b->GetRecordingStartTime())
        return a->GetRecordingStartTime() < b->GetRecordingStartTime();

    if (a->GetRecordingRuleID() != b->GetRecordingRuleID())
        return a->GetRecordingRuleID() < b->GetRecordingRuleID();

    if (a->GetTitle() != b->GetTitle())
        return a->GetTitle() < b->GetTitle();

    if (a->GetProgramID() != b->GetProgramID())
        return a->GetProgramID() < b->GetProgramID();

    if (a->GetSubtitle() != b->GetSubtitle())
        return a->GetSubtitle() < b->GetSubtitle();

    if (a->GetDescription() != b->GetDescription())
        return a->GetDescription() < b->GetDescription();

    if (a->schedorder != b->schedorder)
        return a->schedorder < b->schedorder;

    if (a->GetInputID() != b->GetInputID())
        return a->GetInputID() < b->GetInputID();

    return a->GetChanID() < b->GetChanID();
}

static bool comp_retry(RecordingInfo *a, RecordingInfo *b)
{
    int arec = (a->GetRecordingStatus() != RecStatus::Recording &&
                a->GetRecordingStatus() != RecStatus::Tuning);
    int brec = (b->GetRecordingStatus() != RecStatus::Recording &&
                b->GetRecordingStatus() != RecStatus::Tuning);

    if (arec != brec)
        return arec < brec;

    if (a->GetRecordingPriority() != b->GetRecordingPriority())
        return a->GetRecordingPriority() > b->GetRecordingPriority();

    if (a->GetRecordingPriority2() != b->GetRecordingPriority2())
        return a->GetRecordingPriority2() > b->GetRecordingPriority2();

    int atype = (a->GetRecordingRuleType() == kOverrideRecord ||
                 a->GetRecordingRuleType() == kSingleRecord);
    int btype = (b->GetRecordingRuleType() == kOverrideRecord ||
                 b->GetRecordingRuleType() == kSingleRecord);
    if (atype != btype)
        return atype > btype;

    QDateTime pasttime = MythDate::current().addSecs(-30);
    int apast = (a->GetRecordingStartTime() < pasttime && !a->IsReactivated());
    int bpast = (b->GetRecordingStartTime() < pasttime && !b->IsReactivated());
    if (apast != bpast)
        return apast < bpast;

    if (a->GetRecordingStartTime() != b->GetRecordingStartTime())
        return a->GetRecordingStartTime() > b->GetRecordingStartTime();

    if (a->GetRecordingRuleID() != b->GetRecordingRuleID())
        return a->GetRecordingRuleID() < b->GetRecordingRuleID();

    if (a->GetTitle() != b->GetTitle())
        return a->GetTitle() < b->GetTitle();

    if (a->GetProgramID() != b->GetProgramID())
        return a->GetProgramID() < b->GetProgramID();

    if (a->GetSubtitle() != b->GetSubtitle())
        return a->GetSubtitle() < b->GetSubtitle();

    if (a->GetDescription() != b->GetDescription())
        return a->GetDescription() < b->GetDescription();

    if (a->schedorder != b->schedorder)
        return a->schedorder > b->schedorder;

    if (a->GetInputID() != b->GetInputID())
        return a->GetInputID() > b->GetInputID();

    return a->GetChanID() > b->GetChanID();
}

bool Scheduler::FillRecordList(void)
{
    schedTime = MythDate::current();

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
    LOG(VB_SCHEDULE, LOG_INFO, "SchedLiveTV...");
    SchedLiveTV();
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
 *                  or 0 if anything might have been changed.
 */
void Scheduler::FillRecordListFromDB(uint recordid)
{
    struct timeval fillstart, fillend;
    float matchTime, checkTime, placeTime;

    MSqlQuery query(dbConn);
    QString thequery;
    QString where = "";

    // This will cause our temp copy of recordmatch to be empty
    if (recordid == 0)
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

    thequery = "ALTER TABLE recordmatch "
               "  ADD UNIQUE INDEX (recordid, chanid, starttime); ";
    query.prepare(thequery);
    if (!query.exec())
    {
        MythDB::DBError("FillRecordListFromDB", query);
        return;
    }

    thequery = "ALTER TABLE recordmatch "
               "  ADD INDEX (chanid, starttime, manualid); ";
    query.prepare(thequery);
    if (!query.exec())
    {
        MythDB::DBError("FillRecordListFromDB", query);
        return;
    }

    QMutexLocker locker(&schedLock);

    gettimeofday(&fillstart, NULL);
    UpdateMatches(recordid, 0, 0, QDateTime());
    gettimeofday(&fillend, NULL);
    matchTime = ((fillend.tv_sec - fillstart.tv_sec ) * 1000000 +
                 (fillend.tv_usec - fillstart.tv_usec)) / 1000000.0;

    LOG(VB_SCHEDULE, LOG_INFO, "CreateTempTables...");
    CreateTempTables();

    gettimeofday(&fillstart, NULL);
    LOG(VB_SCHEDULE, LOG_INFO, "UpdateDuplicates...");
    UpdateDuplicates();
    gettimeofday(&fillend, NULL);
    checkTime = ((fillend.tv_sec - fillstart.tv_sec ) * 1000000 +
                 (fillend.tv_usec - fillstart.tv_usec)) / 1000000.0;

    gettimeofday(&fillstart, NULL);
    FillRecordList();
    gettimeofday(&fillend, NULL);
    placeTime = ((fillend.tv_sec - fillstart.tv_sec ) * 1000000 +
                 (fillend.tv_usec - fillstart.tv_usec)) / 1000000.0;

    LOG(VB_SCHEDULE, LOG_INFO, "DeleteTempTables...");
    DeleteTempTables();

    MSqlQuery queryDrop(dbConn);
    queryDrop.prepare("DROP TABLE recordmatch;");
    if (!queryDrop.exec())
    {
        MythDB::DBError("FillRecordListFromDB", queryDrop);
        return;
    }

    QString msg;
    msg.sprintf("Speculative scheduled %d items in %.1f "
                "= %.2f match + %.2f check + %.2f place",
                (int)reclist.size(),
                matchTime + checkTime + placeTime,
                matchTime, checkTime, placeTime);
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
    if (!VERBOSE_LEVEL_CHECK(VB_SCHEDULE, LOG_DEBUG))
        return;

    QDateTime now = MythDate::current();

    LOG(VB_SCHEDULE, LOG_INFO, "--- print list start ---");
    LOG(VB_SCHEDULE, LOG_INFO, "Title - Subtitle                     Ch Station "
                               "Day Start  End    G  I  T  N Pri");

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

void Scheduler::PrintRec(const RecordingInfo *p, const QString &prefix)
{
    if (!VERBOSE_LEVEL_CHECK(VB_SCHEDULE, LOG_DEBUG))
        return;

    QString outstr = prefix;

    QString episode = p->toString(ProgramInfo::kTitleSubtitle, " - ", "")
        .leftJustified(34 - prefix.length(), ' ', true);

    outstr += QString("%1 %2 %3 %4-%5  %6 %7  ")
        .arg(episode)
        .arg(p->GetChanNum().rightJustified(5, ' '))
        .arg(p->GetChannelSchedulingID().leftJustified(7, ' ', true))
        .arg(p->GetRecordingStartTime().toLocalTime().toString("dd hh:mm"))
        .arg(p->GetRecordingEndTime().toLocalTime().toString("hh:mm"))
        .arg(QString::number(p->sgroupid).rightJustified(2, ' '))
        .arg(QString::number(p->GetInputID()).rightJustified(2, ' '));
    outstr += QString("%1 %2 %3")
        .arg(toQChar(p->GetRecordingRuleType()))
        .arg(RecStatus::toString(p->GetRecordingStatus(), p->GetInputID()).rightJustified(2, ' '))
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
        if (p->IsSameTitleTimeslotAndChannel(*pginfo))
        {
            // FIXME!  If we are passed an RecStatus::Unknown recstatus, an
            // in-progress recording might be being stopped.  Try
            // to handle it sensibly until a better fix can be
            // made after the 0.25 code freeze.
            if (pginfo->GetRecordingStatus() == RecStatus::Unknown)
            {
                if (p->GetRecordingStatus() == RecStatus::Tuning ||
                    p->GetRecordingStatus() == RecStatus::Failing)
                    pginfo->SetRecordingStatus(RecStatus::Failed);
                else if (p->GetRecordingStatus() == RecStatus::Recording)
                    pginfo->SetRecordingStatus(RecStatus::Recorded);
                else
                    pginfo->SetRecordingStatus(p->GetRecordingStatus());
            }

            if (p->GetRecordingStatus() != pginfo->GetRecordingStatus())
            {
                LOG(VB_GENERAL, LOG_INFO,
                    QString("Updating status for %1 on cardid %2 (%3 => %4)")
                        .arg(p->toString(ProgramInfo::kTitleSubtitle))
                        .arg(p->GetInputID())
                        .arg(RecStatus::toString(p->GetRecordingStatus(),
                                      p->GetRecordingRuleType()))
                        .arg(RecStatus::toString(pginfo->GetRecordingStatus(),
                                      p->GetRecordingRuleType())));
                bool resched =
                    ((p->GetRecordingStatus() != RecStatus::Recording &&
                      p->GetRecordingStatus() != RecStatus::Tuning) ||
                     (pginfo->GetRecordingStatus() != RecStatus::Recording &&
                      pginfo->GetRecordingStatus() != RecStatus::Tuning));
                p->SetRecordingStatus(pginfo->GetRecordingStatus());
                reclist_changed = true;
                p->AddHistory(false);
                if (resched)
                {
                    EnqueueCheck(*p, "UpdateRecStatus1");
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
                                RecStatus::Type recstatus,
                                const QDateTime &recendts)
{
    QMutexLocker lockit(&schedLock);

    RecIter dreciter = reclist.begin();
    for (; dreciter != reclist.end(); ++dreciter)
    {
        RecordingInfo *p = *dreciter;
        if (p->GetInputID() == cardid && p->GetChanID() == chanid &&
            p->GetScheduledStartTime() == startts)
        {
            p->SetRecordingEndTime(recendts);

            if (p->GetRecordingStatus() != recstatus)
            {
                LOG(VB_GENERAL, LOG_INFO,
                    QString("Updating status for %1 on cardid %2 (%3 => %4)")
                        .arg(p->toString(ProgramInfo::kTitleSubtitle))
                        .arg(p->GetInputID())
                        .arg(RecStatus::toString(p->GetRecordingStatus(),
                                      p->GetRecordingRuleType()))
                        .arg(RecStatus::toString(recstatus,
                                      p->GetRecordingRuleType())));
                bool resched =
                    ((p->GetRecordingStatus() != RecStatus::Recording &&
                      p->GetRecordingStatus() != RecStatus::Tuning) ||
                     (recstatus != RecStatus::Recording &&
                      recstatus != RecStatus::Tuning));
                p->SetRecordingStatus(recstatus);
                reclist_changed = true;
                p->AddHistory(false);
                if (resched)
                {
                    EnqueueCheck(*p, "UpdateRecStatus2");
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
    uint oldrecordid = oldp->GetRecordingRuleID();
    QDateTime oldrecendts = oldp->GetRecordingEndTime();

    oldp->SetRecordingRuleType(newp->GetRecordingRuleType());
    oldp->SetRecordingRuleID(newp->GetRecordingRuleID());
    oldp->SetRecordingEndTime(newp->GetRecordingEndTime());

    if (specsched ||
        oldp->GetRecordingStatus() == RecStatus::Pending)
    {
        if (newp->GetRecordingEndTime() < MythDate::current())
        {
            oldp->SetRecordingStatus(RecStatus::Recorded);
            newp->SetRecordingStatus(RecStatus::Recorded);
            return false;
        }
        else
            return true;
    }

    EncoderLink *tv = (*m_tvList)[oldp->GetInputID()];
    RecordingInfo tempold(*oldp);
    lockit.unlock();
    RecStatus::Type rs = tv->StartRecording(&tempold);
    lockit.relock();
    if (rs != RecStatus::Recording)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Failed to change end time on card %1 to %2")
                .arg(oldp->GetInputID())
                .arg(newp->GetRecordingEndTime(MythDate::ISODate)));
        oldp->SetRecordingRuleType(oldrectype);
        oldp->SetRecordingRuleID(oldrecordid);
        oldp->SetRecordingEndTime(oldrecendts);
    }
    else
    {
        RecordingInfo *foundp = NULL;
        RecIter i = reclist.begin();
        for (; i != reclist.end(); ++i)
        {
            RecordingInfo *recp = *i;
            if (recp->GetInputID() == oldp->GetInputID() &&
                recp->IsSameTitleStartTimeAndChannel(*oldp))
            {
                *recp = *oldp;
                foundp = *i;
                break;
            }
        }

        // If any pending recordings are affected, set them to
        // future conflicting and force a reschedule by marking
        // reclist as changed.
        RecConstIter j = reclist.begin();
        while (FindNextConflict(reclist, foundp, j, openEndNever, NULL))
        {
            RecordingInfo *recp = *j;
            if (recp->GetRecordingStatus() == RecStatus::Pending)
            {
                QString schedid = recp->MakeUniqueSchedulerKey();
                recp->SetRecordingStatus(RecStatus::Conflict);
                recp->AddHistory(false, false, true);
                reclist_changed = true;
            }
            ++j;
        }
    }

    return rs == RecStatus::Recording;
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

            if (!sp->GetTitle().isEmpty() &&
                sp->GetScheduledStartTime() == rp->GetScheduledStartTime() &&
                sp->GetChannelSchedulingID().compare(
                    rp->GetChannelSchedulingID(), Qt::CaseInsensitive) == 0 &&
                sp->GetTitle().compare(rp->GetTitle(),
                                       Qt::CaseInsensitive) == 0)
            {
                if (sp->GetInputID() == rp->GetInputID() ||
                    sinputinfomap[sp->GetInputID()].sgroupid ==
                    rp->GetInputID())
                {
                    found = true;
                    rp->SetRecordingStatus(sp->GetRecordingStatus());
                    reclist_changed = true;
                    rp->AddHistory(false);
                    LOG(VB_GENERAL, LOG_INFO,
                        QString("setting %1/%2/\"%3\" as %4")
                            .arg(sp->GetInputID())
                            .arg(sp->GetChannelSchedulingID())
                            .arg(sp->GetTitle())
                            .arg(RecStatus::toUIState(sp->GetRecordingStatus())));
                }
                else
                {
                    LOG(VB_GENERAL, LOG_NOTICE,
                        QString("%1/%2/\"%3\" is already recording on card %4")
                            .arg(sp->GetInputID())
                            .arg(sp->GetChannelSchedulingID())
                            .arg(sp->GetTitle())
                            .arg(rp->GetInputID()));
                }
            }
            else if (sp->GetInputID() == rp->GetInputID() &&
                     (rp->GetRecordingStatus() == RecStatus::Recording ||
                      rp->GetRecordingStatus() == RecStatus::Tuning ||
                      rp->GetRecordingStatus() == RecStatus::Failing))
            {
                rp->SetRecordingStatus(RecStatus::Aborted);
                reclist_changed = true;
                rp->AddHistory(false);
                LOG(VB_GENERAL, LOG_INFO,
                    QString("setting %1/%2/\"%3\" as aborted")
                        .arg(rp->GetInputID())
                        .arg(rp->GetChannelSchedulingID())
                        .arg(rp->GetTitle()));
            }
        }

        if (sp->GetInputID() && !found)
        {
            sp->mplexid = sp->QueryMplexID();
            sp->sgroupid = sinputinfomap[sp->GetInputID()].sgroupid;
            reclist.push_back(new RecordingInfo(*sp));
            reclist_changed = true;
            sp->AddHistory(false);
            LOG(VB_GENERAL, LOG_INFO,
                QString("adding %1/%2/\"%3\" as recording")
                    .arg(sp->GetInputID())
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

        if (rp->GetInputID() == cardid &&
            (rp->GetRecordingStatus() == RecStatus::Recording ||
             rp->GetRecordingStatus() == RecStatus::Tuning ||
             rp->GetRecordingStatus() == RecStatus::Failing ||
             rp->GetRecordingStatus() == RecStatus::Pending))
        {
            if (rp->GetRecordingStatus() == RecStatus::Pending)
            {
                QString schedid = rp->MakeUniqueSchedulerKey();
                rp->SetRecordingStatus(RecStatus::Missed);
                rp->AddHistory(false, false, true);
            }
            else
            {
                rp->SetRecordingStatus(RecStatus::Aborted);
                rp->AddHistory(false);
            }
            reclist_changed = true;
            LOG(VB_GENERAL, LOG_INFO, QString("setting %1/%2/\"%3\" as aborted")
                    .arg(rp->GetInputID()).arg(rp->GetChannelSchedulingID())
                    .arg(rp->GetTitle()));
        }
    }
}

void Scheduler::BuildWorkList(void)
{
    RecIter i = reclist.begin();
    for (; i != reclist.end(); ++i)
    {
        RecordingInfo *p = *i;
        if (p->GetRecordingStatus() == RecStatus::Recording ||
            p->GetRecordingStatus() == RecStatus::Tuning ||
            p->GetRecordingStatus() == RecStatus::Failing ||
            p->GetRecordingStatus() == RecStatus::Pending)
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
            !lastp->IsSameTitleStartTimeAndChannel(*p))
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
    QMap<uint, uint> badinputs;

    RecIter i = worklist.begin();
    for ( ; i != worklist.end(); ++i)
    {
        RecordingInfo *p = *i;
        if (p->GetRecordingStatus() == RecStatus::Recording ||
            p->GetRecordingStatus() == RecStatus::Tuning ||
            p->GetRecordingStatus() == RecStatus::Failing ||
            p->GetRecordingStatus() == RecStatus::WillRecord ||
            p->GetRecordingStatus() == RecStatus::Pending ||
            p->GetRecordingStatus() == RecStatus::Unknown)
        {
            RecList *conflictlist =
                sinputinfomap[p->GetInputID()].conflictlist;
            if (!conflictlist)
            {
                ++badinputs[p->GetInputID()];
                continue;
            }
            conflictlist->push_back(p);
            titlelistmap[p->GetTitle().toLower()].push_back(p);
            recordidlistmap[p->GetRecordingRuleID()].push_back(p);
        }
    }

    QMap<uint, uint>::iterator it;
    for (it = badinputs.begin(); it != badinputs.end(); ++it)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC_WARN +
            QString("Ignored %1 entries for invalid input %2")
            .arg(badinputs[it.value()]).arg(it.key()));
    }
}

void Scheduler::ClearListMaps(void)
{
    for (uint i = 0; i < conflictlists.size(); ++i)
        conflictlists[i]->clear();
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

    return cache_is_same_program[X] = a->IsDuplicateProgram(*b);
}

bool Scheduler::FindNextConflict(
    const RecList     &cardlist,
    const RecordingInfo *p,
    RecConstIter      &j,
    OpenEndType        openEnd,
    uint              *paffinity) const
{
    uint affinity = 0;
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

        if (p->GetInputID() != q->GetInputID() &&
            !igrp.GetSharedInputGroup(p->GetInputID(), q->GetInputID()))
        {
            if (debugConflicts)
                msg += "  cardid== ";
            continue;
        }

        if (p->GetRecordingEndTime() < q->GetRecordingStartTime() ||
            p->GetRecordingStartTime() > q->GetRecordingEndTime())
        {
            if (debugConflicts)
                msg += "  no-overlap ";
            continue;
        }

        bool mplexid_ok =
            (p->sgroupid != q->sgroupid ||
             sinputinfomap[p->sgroupid].schedgroup) &&
            ((p->mplexid && p->mplexid == q->mplexid) ||
             (!p->mplexid && p->GetChanID() == q->GetChanID()));

        if (p->GetRecordingEndTime() == q->GetRecordingStartTime() ||
            p->GetRecordingStartTime() == q->GetRecordingEndTime())
        {
            if (openEnd == openEndNever ||
                (openEnd == openEndDiffChannel &&
                 p->GetChanID() == q->GetChanID()) ||
                (openEnd == openEndAlways &&
                 mplexid_ok))
            {
                if (debugConflicts)
                    msg += "  no-overlap ";
                if (mplexid_ok)
                    ++affinity;
                continue;
            }
        }

        if (debugConflicts)
        {
            LOG(VB_SCHEDULE, LOG_INFO, msg);
            LOG(VB_SCHEDULE, LOG_INFO,
                QString("  cardid's: %1, %2 Shared input group: %3 "
                        "mplexid's: %4, %5")
                     .arg(p->GetInputID()).arg(q->GetInputID())
                     .arg(igrp.GetSharedInputGroup(
                              p->GetInputID(), q->GetInputID()))
                     .arg(p->mplexid).arg(q->mplexid));
        }

        // if two inputs are in the same input group we have a conflict
        // unless the programs are on the same multiplex.
        if (mplexid_ok)
        {
            ++affinity;
            continue;
        }

        if (debugConflicts)
            LOG(VB_SCHEDULE, LOG_INFO, "Found conflict");

        if (paffinity)
            *paffinity += affinity;
        return true;
    }

    if (debugConflicts)
        LOG(VB_SCHEDULE, LOG_INFO, "No conflict");

    if (paffinity)
        *paffinity += affinity;
    return false;
}

const RecordingInfo *Scheduler::FindConflict(
    const RecordingInfo        *p,
    OpenEndType openend,
    uint *affinity,
    bool checkAll) const
{
    RecList &conflictlist = *sinputinfomap[p->GetInputID()].conflictlist;
    RecConstIter k = conflictlist.begin();
    if (FindNextConflict(conflictlist, p, k, openend, affinity))
    {
        RecordingInfo *firstConflict = *k;
        while (checkAll &&
               FindNextConflict(conflictlist, p, ++k, openend, affinity))
            ;
        return firstConflict;
    }

    return NULL;
}

void Scheduler::MarkOtherShowings(RecordingInfo *p)
{
    RecList *showinglist;

    showinglist = &titlelistmap[p->GetTitle().toLower()];
    MarkShowingsList(*showinglist, p);

    if (p->GetRecordingRuleType() == kOneRecord ||
        p->GetRecordingRuleType() == kDailyRecord ||
        p->GetRecordingRuleType() == kWeeklyRecord)
    {
        showinglist = &recordidlistmap[p->GetRecordingRuleID()];
        MarkShowingsList(*showinglist, p);
    }
    else if (p->GetRecordingRuleType() == kOverrideRecord && p->GetFindID())
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
        if (q->GetRecordingStatus() != RecStatus::Unknown &&
            q->GetRecordingStatus() != RecStatus::WillRecord &&
            q->GetRecordingStatus() != RecStatus::EarlierShowing &&
            q->GetRecordingStatus() != RecStatus::LaterShowing)
            continue;
        if (q->IsSameTitleStartTimeAndChannel(*p))
            q->SetRecordingStatus(RecStatus::LaterShowing);
        else if (q->GetRecordingRuleType() != kSingleRecord &&
                 q->GetRecordingRuleType() != kOverrideRecord &&
                 IsSameProgram(q,p))
        {
            if (q->GetRecordingStartTime() < p->GetRecordingStartTime())
                q->SetRecordingStatus(RecStatus::LaterShowing);
            else
                q->SetRecordingStatus(RecStatus::EarlierShowing);
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
                                   bool livetv)
{
    PrintRec(p, "    >");

    if (p->GetRecordingStatus() == RecStatus::Recording ||
        p->GetRecordingStatus() == RecStatus::Tuning ||
        p->GetRecordingStatus() == RecStatus::Failing ||
        p->GetRecordingStatus() == RecStatus::Pending)
        return false;

    RecList *showinglist = &recordidlistmap[p->GetRecordingRuleID()];

    RecStatus::Type oldstatus = p->GetRecordingStatus();
    p->SetRecordingStatus(RecStatus::LaterShowing);

    RecordingInfo *best = NULL;
    uint bestaffinity = 0;

    RecIter j = showinglist->begin();
    for ( ; j != showinglist->end(); ++j)
    {
        RecordingInfo *q = *j;
        if (q == p)
            continue;

        if (samePriority &&
            (q->GetRecordingPriority() < p->GetRecordingPriority() ||
             (q->GetRecordingPriority() == p->GetRecordingPriority() &&
              q->GetRecordingPriority2() < p->GetRecordingPriority2())))
        {
            continue;
        }

        if (q->GetRecordingStatus() != RecStatus::EarlierShowing &&
            q->GetRecordingStatus() != RecStatus::LaterShowing &&
            q->GetRecordingStatus() != RecStatus::Unknown)
        {
            continue;
        }

        if (!p->IsSameTitleStartTimeAndChannel(*q))
        {
            if (!IsSameProgram(p,q))
                continue;
            if ((p->GetRecordingRuleType() == kSingleRecord ||
                 p->GetRecordingRuleType() == kOverrideRecord))
                continue;
            if (q->GetRecordingStartTime() < schedTime &&
                p->GetRecordingStartTime() >= schedTime)
                continue;
        }

        uint affinity = 0;
        const RecordingInfo *conflict = FindConflict(q, openEndNever,
                                                     &affinity, false);
        if (conflict)
        {
            PrintRec(q, "    #");
            PrintRec(conflict, "      !");
            continue;
        }

        if (livetv)
        {
            // It is pointless to preempt another livetv session.
            // (the livetvlist contains dummy livetv pginfo's)
            RecConstIter k = livetvlist.begin();
            if (FindNextConflict(livetvlist, q, k))
            {
                PrintRec(q, "    #");
                PrintRec(*k, "       !");
                continue;
            }
        }

        PrintRec(q, QString("    %1:").arg(affinity));
        if (!best || affinity > bestaffinity)
        {
            best = q;
            bestaffinity = affinity;
        }
    }

    if (best)
    {
        if (livetv)
        {
            QString msg = QString(
                "Moved \"%1\" on chanid: %2 from card: %3 to %4 at %5 "
                "to avoid LiveTV conflict")
                .arg(p->GetTitle()).arg(p->GetChanID())
                .arg(p->GetInputID()).arg(best->GetInputID())
                .arg(best->GetScheduledStartTime().toLocalTime().toString());
            LOG(VB_GENERAL, LOG_INFO, msg);
        }

        best->SetRecordingStatus(RecStatus::WillRecord);
        MarkOtherShowings(best);
        if (best->GetRecordingStartTime() < livetvTime)
            livetvTime = best->GetRecordingStartTime();
        PrintRec(p, "    -");
        PrintRec(best, "    +");
        return true;
    }

    p->SetRecordingStatus(oldstatus);
    return false;
}

void Scheduler::SchedNewRecords(void)
{
    if (VERBOSE_LEVEL_CHECK(VB_SCHEDULE, LOG_DEBUG))
    {
        LOG(VB_SCHEDULE, LOG_DEBUG,
            "+ = schedule this showing to be recorded");
        LOG(VB_SCHEDULE, LOG_DEBUG,
            "n: = could schedule this showing with affinity");
        LOG(VB_SCHEDULE, LOG_DEBUG,
            "n# = could not schedule this showing, with affinity");
        LOG(VB_SCHEDULE, LOG_DEBUG,
            "! = conflict caused by this showing");
        LOG(VB_SCHEDULE, LOG_DEBUG,
            "/ = retry this showing, same priority pass");
        LOG(VB_SCHEDULE, LOG_DEBUG,
            "? = retry this showing, lower priority pass");
        LOG(VB_SCHEDULE, LOG_DEBUG,
            "> = try another showing for this program");
        LOG(VB_SCHEDULE, LOG_DEBUG,
            "- = unschedule a showing in favor of another one");
    }

    livetvTime = MythDate::current().addSecs(3600);
    m_openEnd =
        (OpenEndType)gCoreContext->GetNumSetting("SchedOpenEnd", openEndNever);

    RecIter i = worklist.begin();

    for ( ; i != worklist.end(); ++i)
    {
        if ((*i)->GetRecordingStatus() != RecStatus::Recording &&
            (*i)->GetRecordingStatus() != RecStatus::Tuning &&
            (*i)->GetRecordingStatus() != RecStatus::Pending)
            break;
        MarkOtherShowings(*i);
    }

    while (i != worklist.end())
    {
        RecIter levelStart = i;
        int recpriority = (*i)->GetRecordingPriority();

        while (i != worklist.end())
        {
            if (i == worklist.end() ||
                (*i)->GetRecordingPriority() != recpriority)
                break;

            RecIter sublevelStart = i;
            int recpriority2 = (*i)->GetRecordingPriority2();
            LOG(VB_SCHEDULE, LOG_DEBUG, QString("Trying priority %1/%2...")
                .arg(recpriority).arg(recpriority2));
            // First pass for anything in this priority sublevel.
            SchedNewFirstPass(i, worklist.end(), recpriority, recpriority2);

            LOG(VB_SCHEDULE, LOG_DEBUG, QString("Retrying priority %1/%2...")
                .arg(recpriority).arg(recpriority2));
            SchedNewRetryPass(sublevelStart, i, true);
        }

        // Retry pass for anything in this priority level.
        LOG(VB_SCHEDULE, LOG_DEBUG, QString("Retrying priority %1/*...")
            .arg(recpriority));
        SchedNewRetryPass(levelStart, i, false);
    }
}

// Perform the first pass for scheduling new recordings for programs
// in the same priority sublevel.  For each program/starttime, choose
// the first one with the highest affinity that doesn't conflict.
void Scheduler::SchedNewFirstPass(RecIter &i, RecIter end,
                                  int recpriority, int recpriority2)
{
    while (i != end)
    {
        // Find the next unscheduled program in this sublevel.
        for ( ; i != end; ++i)
        {
            if ((*i)->GetRecordingPriority() != recpriority ||
                (*i)->GetRecordingPriority2() != recpriority2 ||
                (*i)->GetRecordingStatus() == RecStatus::Unknown)
                break;
        }

        // Stop if we don't find another program to schedule.
        if (i == end ||
            (*i)->GetRecordingPriority() != recpriority ||
            (*i)->GetRecordingPriority2() != recpriority2)
            break;

        RecordingInfo *first = *i;
        RecordingInfo *best = NULL;
        uint bestaffinity = 0;

        // Try each showing of this program at this time.
        for ( ; i != end; ++i)
        {
            if ((*i)->GetRecordingPriority() != recpriority ||
                (*i)->GetRecordingPriority2() != recpriority2 ||
                (*i)->GetRecordingStartTime() !=
                first->GetRecordingStartTime() ||
                (*i)->GetRecordingRuleID() !=
                first->GetRecordingRuleID() ||
                (*i)->GetTitle() != first->GetTitle() ||
                (*i)->GetProgramID() != first->GetProgramID() ||
                (*i)->GetSubtitle() != first->GetSubtitle() ||
                (*i)->GetDescription() != first->GetDescription())
                break;

            // This shouldn't happen, but skip it just in case.
            if ((*i)->GetRecordingStatus() != RecStatus::Unknown)
                continue;

            uint affinity = 0;
            const RecordingInfo *conflict =
                FindConflict(*i, m_openEnd, &affinity, true);
            if (conflict)
            {
                PrintRec(*i, QString("  %1#").arg(affinity));
                PrintRec(conflict, "    !");
            }
            else
            {
                PrintRec(*i, QString("  %1:").arg(affinity));
                if (!best || affinity > bestaffinity)
                {
                    best = *i;
                    bestaffinity = affinity;
                }
            }
        }

        // Schedule the best one.
        if (best)
        {
            PrintRec(best, "  +");
            best->SetRecordingStatus(RecStatus::WillRecord);
            MarkOtherShowings(best);
            if (best->GetRecordingStartTime() < livetvTime)
                livetvTime = best->GetRecordingStartTime();
        }
    }
}

// Perform the retry passes for scheduling new recordings.  For each
// unscheduled program, try to move the conflicting programs to
// another time or tuner using the given constraints.
void Scheduler::SchedNewRetryPass(RecIter i, RecIter end,
                                  bool samePriority, bool livetv)
{
    RecList retry_list;
    for ( ; i != end; ++i)
    {
        if ((*i)->GetRecordingStatus() == RecStatus::Unknown)
            retry_list.push_back(*i);
    }
    SORT_RECLIST(retry_list, comp_retry);

    i = retry_list.begin();
    for ( ; i != retry_list.end(); ++i)
    {
        RecordingInfo *p = *i;
        if (p->GetRecordingStatus() != RecStatus::Unknown)
            continue;

        if (samePriority)
            PrintRec(p, "  /");
        else
            PrintRec(p, "  ?");

        // Assume we can successfully move all of the conflicts.
        BackupRecStatus();
        p->SetRecordingStatus(RecStatus::WillRecord);
        if (!livetv)
            MarkOtherShowings(p);

        // Try to move each conflict.  Restore the old status if we
        // can't.
        RecList &conflictlist = *sinputinfomap[p->GetInputID()].conflictlist;
        RecConstIter k = conflictlist.begin();
        for ( ; FindNextConflict(conflictlist, p, k); ++k)
        {
            if (!TryAnotherShowing(*k, samePriority, livetv))
            {
                RestoreRecStatus();
                break;
            }
        }

        if (!livetv && p->GetRecordingStatus() == RecStatus::WillRecord)
        {
            if (p->GetRecordingStartTime() < livetvTime)
                livetvTime = p->GetRecordingStartTime();
            PrintRec(p, "  +");
        }
    }
}

void Scheduler::PruneRedundants(void)
{
    RecordingInfo *lastp = NULL;
    int lastrecpri2 = 0;

    RecIter i = worklist.begin();
    while (i != worklist.end())
    {
        RecordingInfo *p = *i;

        // Delete anything that has already passed since we can't
        // change history, can we?
        if (p->GetRecordingStatus() != RecStatus::Recording &&
            p->GetRecordingStatus() != RecStatus::Tuning &&
            p->GetRecordingStatus() != RecStatus::Failing &&
            p->GetRecordingStatus() != RecStatus::MissedFuture &&
            p->GetScheduledEndTime() < schedTime &&
            p->GetRecordingEndTime() < schedTime)
        {
            delete p;
            *(i++) = NULL;
            continue;
        }

        // Check for RecStatus::Conflict
        if (p->GetRecordingStatus() == RecStatus::Unknown)
            p->SetRecordingStatus(RecStatus::Conflict);

        // Restore the old status for some selected cases.
        if (p->GetRecordingStatus() == RecStatus::MissedFuture ||
            (p->GetRecordingStatus() == RecStatus::Missed &&
             p->oldrecstatus != RecStatus::Unknown) ||
            (p->GetRecordingStatus() == RecStatus::CurrentRecording &&
             p->oldrecstatus == RecStatus::PreviousRecording && !p->future) ||
            (p->GetRecordingStatus() != RecStatus::WillRecord &&
             p->oldrecstatus == RecStatus::Aborted))
        {
            RecStatus::Type rs = p->GetRecordingStatus();
            p->SetRecordingStatus(p->oldrecstatus);
            // Re-mark RecStatus::MissedFuture entries so non-future history
            // will be saved in the scheduler thread.
            if (rs == RecStatus::MissedFuture)
                p->oldrecstatus = RecStatus::MissedFuture;
        }

        if (!Recording(p))
        {
            p->SetInputID(0);
            p->SetSourceID(0);
            p->sgroupid = 0;
        }

        // Check for redundant against last non-deleted
        if (!lastp || lastp->GetRecordingRuleID() != p->GetRecordingRuleID() ||
            !lastp->IsSameTitleStartTimeAndChannel(*p))
        {
            lastp = p;
            lastrecpri2 = lastp->GetRecordingPriority2();
            lastp->SetRecordingPriority2(0);
            ++i;
        }
        else
        {
            // Flag lower priority showings that will be recorded so
            // we can warn the user about them
            if (lastp->GetRecordingStatus() == RecStatus::WillRecord &&
                p->GetRecordingPriority2() >
                lastrecpri2 - lastp->GetRecordingPriority2())
            {
                lastp->SetRecordingPriority2(
                    lastrecpri2 - p->GetRecordingPriority2());
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
        if ((p->GetRecordingStatus() == RecStatus::WillRecord ||
             p->GetRecordingStatus() == RecStatus::Pending) &&
            nextRecMap[p->GetRecordingRuleID()].isNull())
        {
            nextRecMap[p->GetRecordingRuleID()] = p->GetRecordingStartTime();
        }

        if (p->GetRecordingRuleType() == kOverrideRecord &&
            p->GetParentRecordingRuleID() > 0 &&
            (p->GetRecordingStatus() == RecStatus::WillRecord ||
             p->GetRecordingStatus() == RecStatus::Pending) &&
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
            QDateTime next_record = MythDate::as_utc(query.value(1).toDateTime());

            if (next_record == nextRecMap[recid])
                continue;

            if (nextRecMap[recid].isValid())
            {
                subquery.prepare("UPDATE record SET next_record = :NEXTREC "
                                 "WHERE recordid = :RECORDID;");
                subquery.bindValue(":RECORDID", recid);
                subquery.bindValue(":NEXTREC", nextRecMap[recid]);
                if (!subquery.exec())
                    MythDB::DBError("Update next_record", subquery);
            }
            else if (next_record.isValid())
            {
                subquery.prepare("UPDATE record "
                                 "SET next_record = NULL "
                                 "WHERE recordid = :RECORDID;");
                subquery.bindValue(":RECORDID", recid);
                if (!subquery.exec())
                    MythDB::DBError("Clear next_record", subquery);
            }
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

bool Scheduler::GetAllPending(RecList &retList, int recRuleId) const
{
    QMutexLocker lockit(&schedLock);

    bool hasconflicts = false;

    RecConstIter it = reclist.begin();
    for (; it != reclist.end(); ++it)
    {
        if (recRuleId > 0 &&
            (*it)->GetRecordingRuleID() != static_cast<uint>(recRuleId))
            continue;
        if ((*it)->GetRecordingStatus() == RecStatus::Conflict)
            hasconflicts = true;
        retList.push_back(new RecordingInfo(**it));
    }

    return hasconflicts;
}

bool Scheduler::GetAllPending(ProgramList &retList, int recRuleId) const
{
    QMutexLocker lockit(&schedLock);

    bool hasconflicts = false;

    RecConstIter it = reclist.begin();
    for (; it != reclist.end(); ++it)
    {
        if (recRuleId > 0 &&
            (*it)->GetRecordingRuleID() != static_cast<uint>(recRuleId))
            continue;

        if ((*it)->GetRecordingStatus() == RecStatus::Conflict)
            hasconflicts = true;
        retList.push_back(new ProgramInfo(**it));
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
        if (RecStatus::Recording == (*it)->GetRecordingStatus() ||
            RecStatus::Tuning == (*it)->GetRecordingStatus() ||
            RecStatus::Failing == (*it)->GetRecordingStatus())
            recMap[(*it)->MakeUniqueKey()] = new ProgramInfo(**it);
    }

    return recMap;
}

RecStatus::Type Scheduler::GetRecStatus(const ProgramInfo &pginfo)
{
    QMutexLocker lockit(&schedLock);

    for (RecConstIter it = reclist.begin(); it != reclist.end(); ++it)
    {
        if (pginfo.IsSameRecording(**it))
        {
            return (RecStatus::Recording == (**it).GetRecordingStatus() ||
                    RecStatus::Tuning == (**it).GetRecordingStatus() ||
                    RecStatus::Failing == (**it).GetRecordingStatus() ||
                    RecStatus::Pending == (**it).GetRecordingStatus()) ?
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

/// Returns all scheduled programs serialized into a QStringList
void Scheduler::GetAllScheduled(QStringList &strList, SchedSortColumn sortBy,
                                bool ascending)
{
    RecList schedlist;

    GetAllScheduled(schedlist, sortBy, ascending);

    strList << QString::number(schedlist.size());

    while (!schedlist.empty())
    {
        RecordingInfo *pginfo = schedlist.front();
        pginfo->ToStringList(strList);
        delete pginfo;
        schedlist.pop_front();
    }
}

void Scheduler::Reschedule(const QStringList &request)
{
    QMutexLocker locker(&schedLock);
    reschedQueue.enqueue(request);
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
        if (p->GetRecordingStatus() == RecStatus::Recording &&
            p->IsSameTitleTimeslotAndChannel(pi))
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
    new_pi->mplexid = new_pi->QueryMplexID();
    new_pi->sgroupid = sinputinfomap[new_pi->GetInputID()].sgroupid;
    reclist.push_back(new_pi);
    reclist_changed = true;

    // Save RecStatus::Recording recstatus to DB
    // This allows recordings to resume on backend restart
    new_pi->AddHistory(false);

    // Make sure we have a ScheduledRecording instance
    new_pi->GetRecordingRule();

    // Trigger reschedule..
    EnqueueMatch(pi.GetRecordingRuleID(), 0, 0, QDateTime(),
                 QString("AddRecording %1").arg(pi.GetTitle()));
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

    if (!m_tvList->contains(rcinfo->GetInputID()))
        return true;

    InputInfo busy_input;

    EncoderLink *rctv = (*m_tvList)[rcinfo->GetInputID()];
    // first check the input we will be recording on...
    bool is_busy = rctv->IsBusy(&busy_input, -1);
    if (is_busy &&
        (rcinfo->GetRecordingStatus() == RecStatus::Pending ||
         !sinputinfomap[rcinfo->GetInputID()].schedgroup ||
         ((!busy_input.mplexid || busy_input.mplexid != rcinfo->mplexid) &&
          (busy_input.mplexid || busy_input.chanid != rcinfo->GetChanID()))))
    {
        return true;
    }

    // now check other inputs in the same input group as the recording.
    uint inputid = rcinfo->GetInputID();
    vector<uint> &inputids = sinputinfomap[inputid].conflicting_inputs;
    vector<uint> &group_inputs = sinputinfomap[inputid].group_inputs;
    for (uint i = 0; i < inputids.size(); i++)
    {
        if (!m_tvList->contains(inputids[i]))
        {
#if 0
            LOG(VB_SCHEDULE, LOG_ERR, LOC +
                QString("IsBusyRecording() -> true, rctv(NULL) for input %2")
                    .arg(inputids[i]));
#endif
            return true;
        }

        EncoderLink *rctv = (*m_tvList)[inputids[i]];
        if (rctv->IsBusy(&busy_input, -1))
        {
            if ((!busy_input.mplexid ||
                 busy_input.mplexid != rcinfo->mplexid) &&
                (busy_input.mplexid ||
                 busy_input.chanid != rcinfo->GetChanID()))
            {
                // This conflicting input is busy on a different
                // multiplex than is desired.  There is no way the
                // main input nor any of its children can be free.
                return true;
            }
            else if (!is_busy)
            {
                // This conflicting input is busy on the desired
                // multiplex and the main input is not busy.  Nothing
                // else can conflict, so the main input is free.
                return false;
            }
        }
        else if (is_busy &&
                 std::find(group_inputs.begin(), group_inputs.end(),
                           inputids[i]) != group_inputs.end())
        {
            // This conflicting input is not busy, is also a child
            // input and the main input is busy on the desired
            // multiplex.  This input is therefore considered free.
            return false;
        }
    }

    return is_busy;
}

void Scheduler::OldRecordedFixups(void)
{
    MSqlQuery query(dbConn);

    // Mark anything that was recording as aborted.
    query.prepare("UPDATE oldrecorded SET recstatus = :RSABORTED "
                  "  WHERE recstatus = :RSRECORDING OR "
                  "        recstatus = :RSTUNING OR "
                  "        recstatus = :RSFAILING");
    query.bindValue(":RSABORTED", RecStatus::Aborted);
    query.bindValue(":RSRECORDING", RecStatus::Recording);
    query.bindValue(":RSTUNING", RecStatus::Tuning);
    query.bindValue(":RSFAILING", RecStatus::Failing);
    if (!query.exec())
        MythDB::DBError("UpdateAborted", query);

    // Mark anything that was going to record as missed.
    query.prepare("UPDATE oldrecorded SET recstatus = :RSMISSED "
                  "WHERE recstatus = :RSWILLRECORD OR "
                  "      recstatus = :RSPENDING");
    query.bindValue(":RSMISSED", RecStatus::Missed);
    query.bindValue(":RSWILLRECORD", RecStatus::WillRecord);
    query.bindValue(":RSPENDING", RecStatus::Pending);
    if (!query.exec())
        MythDB::DBError("UpdateMissed", query);

    // Mark anything that was set to RecStatus::CurrentRecording as
    // RecStatus::PreviousRecording.
    query.prepare("UPDATE oldrecorded SET recstatus = :RSPREVIOUS "
                  "WHERE recstatus = :RSCURRENT");
    query.bindValue(":RSPREVIOUS", RecStatus::PreviousRecording);
    query.bindValue(":RSCURRENT", RecStatus::CurrentRecording);
    if (!query.exec())
        MythDB::DBError("UpdateCurrent", query);

    // Clear the "future" status of anything older than the maximum
    // endoffset.  Anything more recent will bee handled elsewhere
    // during normal processing.
    query.prepare("UPDATE oldrecorded SET future = 0 "
                  "WHERE future > 0 AND "
                  "      endtime < (NOW() - INTERVAL 475 MINUTE)");
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

    ClearRequestQueue();
    EnqueueMatch(0, 0, 0, QDateTime(), "SchedulerInit");

    int       prerollseconds  = 0;
    int       wakeThreshold   = 300;
    int       idleTimeoutSecs = 0;
    int       idleWaitForRecordingTime = 15; // in minutes
    bool      blockShutdown   =
        gCoreContext->GetNumSetting("blockSDWUwithoutClient", 1);
    bool      firstRun        = true;
    QDateTime lastSleepCheck  = MythDate::current().addDays(-1);
    RecIter   startIter       = reclist.begin();
    QDateTime idleSince       = QDateTime();
    int       schedRunTime    = 0; // max scheduler run time in seconds
    bool      statuschanged   = false;
    QDateTime nextStartTime   = MythDate::current().addDays(14);
    QDateTime nextWakeTime    = nextStartTime;

    while (doRun)
    {
        // If something changed, it might have short circuited a pass
        // through the list or changed the next run times.  Start a
        // new pass immediately to take care of anything that still
        // needs attention right now and reset the run times.
        if (reclist_changed)
        {
            nextStartTime = MythDate::current();
            reclist_changed = false;
        }

        nextWakeTime = min(nextWakeTime, nextStartTime);
        QDateTime curtime = MythDate::current();
        int secs_to_next = curtime.secsTo(nextStartTime);
        int sched_sleep = max(curtime.msecsTo(nextWakeTime), qint64(0));
        if (idleTimeoutSecs > 0)
            sched_sleep = min(sched_sleep, 15000);
        bool haveRequests = HaveQueuedRequests();
        int const kSleepCheck = 300;
        bool checkSlaves = lastSleepCheck.secsTo(curtime) >= kSleepCheck;

        // If we're about to start a recording don't do any reschedules...
        // instead sleep for a bit
        if ((secs_to_next > -60 && secs_to_next < schedRunTime) ||
            (!haveRequests && !checkSlaves))
        {
            if (sched_sleep)
            {
                LOG(VB_SCHEDULE, LOG_INFO,
                    QString("sleeping for %1 ms "
                            "(s2n: %2 sr: %3 qr: %4 cs: %5)")
                    .arg(sched_sleep).arg(secs_to_next).arg(schedRunTime)
                    .arg(haveRequests).arg(checkSlaves));
                if (reschedWait.wait(&schedLock, sched_sleep))
                    continue;
            }
        }
        else
        {
            if (haveRequests)
            {
                // The master backend is a long lived program, so
                // we reload some key settings on each reschedule.
                prerollseconds  =
                    gCoreContext->GetNumSetting("RecordPreRoll", 0);
                wakeThreshold =
                    gCoreContext->GetNumSetting("WakeUpThreshold", 300);
                idleTimeoutSecs =
                    gCoreContext->GetNumSetting("idleTimeoutSecs", 0);
                idleWaitForRecordingTime =
                    gCoreContext->GetNumSetting("idleWaitForRecordingTime",
                                                15);

                QTime t; t.start();
                if (HandleReschedule())
                {
                    statuschanged = true;
                    startIter = reclist.begin();
                }
                schedRunTime = max(int(((t.elapsed() + 999) / 1000) * 1.5 + 2),
                                   schedRunTime);
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

            if (checkSlaves)
            {
                // Check for slaves that can be put to sleep.
                PutInactiveSlavesToSleep();
                lastSleepCheck = MythDate::current();
            }
        }

        nextStartTime = MythDate::current().addDays(14);
        nextWakeTime = lastSleepCheck.addSecs(kSleepCheck);

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
        // & handle RecStatus::Tuning updates
        bool done = false;
        for (RecIter it = startIter; it != reclist.end() && !done; ++it)
        {
            done = HandleRecording(
                **it, statuschanged, nextStartTime, nextWakeTime,
                prerollseconds);
        }

        // HandleRecording() temporarily unlocks schedLock.  If
        // anything changed, reclist iterators could be invalidated so
        // start over.
        if (reclist_changed)
            continue;

        /// Wake any slave backends that need waking
        curtime = MythDate::current();
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
// a scheduler run has nothing to do with the idle shutdown
//            idleSince = QDateTime();
        }

        // if idletimeout is 0, the user disabled the auto-shutdown feature
        if ((idleTimeoutSecs > 0) && (m_mainServer != NULL))
        {
            HandleIdleShutdown(blockShutdown, idleSince, prerollseconds,
                               idleTimeoutSecs, idleWaitForRecordingTime,
                               statuschanged);
            if (idleSince.isValid())
            {
                nextWakeTime = MythDate::current().addSecs(
                    (idleSince.addSecs(idleTimeoutSecs - 10) <= curtime) ? 1 :
                    (idleSince.addSecs(idleTimeoutSecs - 30) <= curtime) ? 5 : 10);
            }
        }

        statuschanged = false;
    }

    RunEpilog();
}

void Scheduler::ResetDuplicates(uint recordid, uint findid,
                                const QString &title, const QString &subtitle,
                                const QString &descrip,
                                const QString &programid)
{
    MSqlQuery query(dbConn);
    QString filterClause;
    MSqlBindings bindings;

    if (!title.isEmpty())
    {
        filterClause += "AND p.title = :TITLE ";
        bindings[":TITLE"] = title;
    }

    // "**any**" is special value set in ProgLister::DeleteOldSeries()
    if (programid != "**any**")
    {
        filterClause += "AND (0 ";
        if (!subtitle.isEmpty())
        {
            // Need to check both for kDupCheckSubThenDesc
            filterClause += "OR p.subtitle = :SUBTITLE1 "
                            "OR p.description = :SUBTITLE2 ";
            bindings[":SUBTITLE1"] = subtitle;
            bindings[":SUBTITLE2"] = subtitle;
        }
        if (!descrip.isEmpty())
        {
            // Need to check both for kDupCheckSubThenDesc
            filterClause += "OR p.description = :DESCRIP1 "
                            "OR p.subtitle = :DESCRIP2 ";
            bindings[":DESCRIP1"] = descrip;
            bindings[":DESCRIP2"] = descrip;
        }
        if (!programid.isEmpty())
        {
            filterClause += "OR p.programid = :PROGRAMID ";
            bindings[":PROGRAMID"] = programid;
        }
        filterClause += ") ";
    }

    query.prepare(QString("UPDATE recordmatch rm "
                          "INNER JOIN %1 r "
                          "      ON rm.recordid = r.recordid "
                          "INNER JOIN program p "
                          "      ON rm.chanid = p.chanid "
                          "         AND rm.starttime = p.starttime "
                          "         AND rm.manualid = p.manualid "
                          "SET oldrecduplicate = -1 "
                          "WHERE p.generic = 0 "
                          "      AND r.type NOT IN (%2, %3, %4) ")
                  .arg(recordTable)
                  .arg(kSingleRecord)
                  .arg(kOverrideRecord)
                  .arg(kDontRecord)
                  + filterClause);
    MSqlBindings::const_iterator it;
    for (it = bindings.begin(); it != bindings.end(); ++it)
        query.bindValue(it.key(), it.value());
    if (!query.exec())
        MythDB::DBError("ResetDuplicates1", query);

    if (findid && programid != "**any**")
    {
        query.prepare("UPDATE recordmatch rm "
                      "SET oldrecduplicate = -1 "
                      "WHERE rm.recordid = :RECORDID "
                      "      AND rm.findid = :FINDID");
        query.bindValue("RECORDID", recordid);
        query.bindValue("FINDID", findid);
        if (!query.exec())
            MythDB::DBError("ResetDuplicates2", query);
    }
 }

bool Scheduler::HandleReschedule(void)
{
    // We might have been inactive for a long time, so make
    // sure our DB connection is fresh before continuing.
    dbConn = MSqlQuery::SchedCon();

    struct timeval fillstart, fillend;
    float matchTime, checkTime, placeTime;

    gettimeofday(&fillstart, NULL);
    QString msg;
    bool deleteFuture = false;
    bool runCheck = false;

    while (HaveQueuedRequests())
    {
        QStringList request = reschedQueue.dequeue();
        QStringList tokens;
        if (request.size() >= 1)
            tokens = request[0].split(' ', QString::SkipEmptyParts);

        if (request.size() < 1 || tokens.size() < 1)
        {
            LOG(VB_GENERAL, LOG_ERR, "Empty Reschedule request received");
            continue;
        }

        LOG(VB_GENERAL, LOG_INFO, QString("Reschedule requested for %1")
            .arg(request.join(" | ")));

        if (tokens[0] == "MATCH")
        {
            if (tokens.size() < 5)
            {
                LOG(VB_GENERAL, LOG_ERR,
                    QString("Invalid RescheduleMatch request received (%1)")
                    .arg(request[0]));
                continue;
            }

            uint recordid = tokens[1].toUInt();
            uint sourceid = tokens[2].toUInt();
            uint mplexid = tokens[3].toUInt();
            QDateTime maxstarttime = MythDate::fromString(tokens[4]);
            deleteFuture = true;
            runCheck = true;
            schedLock.unlock();
            recordmatchLock.lock();
            UpdateMatches(recordid, sourceid, mplexid, maxstarttime);
            recordmatchLock.unlock();
            schedLock.lock();
        }
        else if (tokens[0] == "CHECK")
        {
            if (tokens.size() < 4 || request.size() < 5)
            {
                LOG(VB_GENERAL, LOG_ERR,
                    QString("Invalid RescheduleCheck request received (%1)")
                    .arg(request[0]));
                continue;
            }

            uint recordid = tokens[2].toUInt();
            uint findid = tokens[3].toUInt();
            QString title = request[1];
            QString subtitle = request[2];
            QString descrip = request[3];
            QString programid = request[4];
            runCheck = true;
            schedLock.unlock();
            recordmatchLock.lock();
            ResetDuplicates(recordid, findid, title, subtitle, descrip,
                            programid);
            recordmatchLock.unlock();
            schedLock.lock();
        }
        else if (tokens[0] != "PLACE")
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("Unknown Reschedule request received (%1)")
                .arg(request[0]));
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

    gettimeofday(&fillend, NULL);
    matchTime = ((fillend.tv_sec - fillstart.tv_sec ) * 1000000 +
                 (fillend.tv_usec - fillstart.tv_usec)) / 1000000.0;

    LOG(VB_SCHEDULE, LOG_INFO, "CreateTempTables...");
    CreateTempTables();

    gettimeofday(&fillstart, NULL);
    if (runCheck)
    {
        LOG(VB_SCHEDULE, LOG_INFO, "UpdateDuplicates...");
        UpdateDuplicates();
    }
    gettimeofday(&fillend, NULL);
    checkTime = ((fillend.tv_sec - fillstart.tv_sec ) * 1000000 +
                 (fillend.tv_usec - fillstart.tv_usec)) / 1000000.0;

    gettimeofday(&fillstart, NULL);
    bool worklistused = FillRecordList();
    gettimeofday(&fillend, NULL);
    placeTime = ((fillend.tv_sec - fillstart.tv_sec ) * 1000000 +
                 (fillend.tv_usec - fillstart.tv_usec)) / 1000000.0;

    LOG(VB_SCHEDULE, LOG_INFO, "DeleteTempTables...");
    DeleteTempTables();

    if (worklistused)
    {
        UpdateNextRecord();
        PrintList();
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, "Reschedule interrupted, will retry");
        EnqueuePlace("Interrupted");
        return false;
    }

    msg.sprintf("Scheduled %d items in %.1f "
                "= %.2f match + %.2f check + %.2f place",
                (int)reclist.size(), matchTime + checkTime + placeTime,
                matchTime, checkTime, placeTime);
    LOG(VB_GENERAL, LOG_INFO, msg);

    fsInfoCacheFillTime = MythDate::current().addSecs(-1000);

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
                     p->GetRecordingStatus() != RecStatus::WillRecord &&
                     p->GetRecordingStatus() != RecStatus::Pending)
                p->AddHistory(false, false, false);
            else
                p->AddHistory(false, false, true);
        }
        else if (p->future)
        {
            // Force a non-future, oldrecorded entry to
            // get written when the time comes.
            p->oldrecstatus = RecStatus::Unknown;
        }
        p->future = false;
    }

    gCoreContext->SendSystemEvent("SCHEDULER_RAN");

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
        if ((*firstRunIter)->GetRecordingStatus() == RecStatus::WillRecord ||
            (*firstRunIter)->GetRecordingStatus() == RecStatus::Pending)
            break;
    }

    // have we been started automatically?
    QDateTime curtime = MythDate::current();
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

    QDateTime curtime = MythDate::current();
    QDateTime nextrectime = ri.GetRecordingStartTime();
    int secsleft = curtime.secsTo(nextrectime);

    QMap<int, EncoderLink*>::iterator tvit = m_tvList->find(ri.GetInputID());
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
            EnqueuePlace("HandleWakeSlave1");
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
            EnqueuePlace("HandleWakeSlave2");
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

        EnqueuePlace("HandleWakeSlave3");
    }
}

bool Scheduler::HandleRecording(
    RecordingInfo &ri, bool &statuschanged,
    QDateTime &nextStartTime, QDateTime &nextWakeTime,
    int prerollseconds)
{
    if (ri.GetRecordingStatus() == ri.oldrecstatus)
        return false;

    QDateTime curtime     = MythDate::current();
    QDateTime nextrectime = ri.GetRecordingStartTime();

    if (ri.GetRecordingStatus() != RecStatus::WillRecord &&
        ri.GetRecordingStatus() != RecStatus::Pending)
    {
        // If this recording is sufficiently after nextWakeTime,
        // nothing later can shorten nextWakeTime, so stop scanning.
        if (nextWakeTime.secsTo(nextrectime) - prerollseconds > 300)
        {
            nextStartTime = min(nextStartTime, nextrectime);
            return true;
        }

        if (curtime < nextrectime)
            nextWakeTime = min(nextWakeTime, nextrectime);
        else
            ri.AddHistory(false);
        return false;
    }

    int secsleft = curtime.secsTo(nextrectime);

    // If we haven't reached this threshold yet, nothing later can
    // shorten nextWakeTime, so stop scanning.  NOTE: this threshold
    // needs to be shorter than the related one in SchedLiveTV().
    if (secsleft - prerollseconds > 60)
    {
        nextStartTime = min(nextStartTime, nextrectime.addSecs(-30));
        nextWakeTime = min(nextWakeTime,
                            nextrectime.addSecs(-prerollseconds - 60));
        return true;
    }

    QString schedid = ri.MakeUniqueSchedulerKey();

    if (ri.GetRecordingStartTime() > lastPrepareTime)
    {
        // If we haven't rescheduled in a while, do so now to
        // accomodate LiveTV.
        if (schedTime.secsTo(curtime) > 30)
            EnqueuePlace("PrepareToRecord");
        lastPrepareTime = ri.GetRecordingStartTime();
    }

    if (secsleft - prerollseconds > 35)
    {
        nextStartTime = min(nextStartTime, nextrectime.addSecs(-30));
        nextWakeTime = min(nextWakeTime,
                            nextrectime.addSecs(-prerollseconds - 35));
        return false;
    }

    QMap<int, EncoderLink*>::iterator tvit = m_tvList->find(ri.GetInputID());
    if (tvit == m_tvList->end())
    {
        QString msg = QString("Invalid cardid (%1) for %2")
            .arg(ri.GetInputID()).arg(ri.GetTitle());
        LOG(VB_GENERAL, LOG_ERR, LOC + msg);

        ri.SetRecordingStatus(RecStatus::TunerBusy);
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
            .arg(ri.GetInputID())
            .arg(ri.GetSourceID());
        LOG(VB_GENERAL, LOG_NOTICE, msg);

        ri.SetRecordingStatus(RecStatus::TunerBusy);
        ri.AddHistory(true);
        statuschanged = true;
        return false;
    }

    // Use this temporary copy of ri when schedLock is not held.  Be
    // sure to update it as long as it is still needed whenever ri
    // changes.
    RecordingInfo tempri(ri);

    // Try to use preroll.  If we can't do so right now, try again in
    // a little while in case the recorder frees up.
    if (prerollseconds > 0)
    {
        schedLock.unlock();
        bool isBusyRecording = IsBusyRecording(&tempri);
        schedLock.lock();
        if (reclist_changed)
            return reclist_changed;

        if (isBusyRecording)
        {
            if (secsleft > 5)
                nextWakeTime = min(nextWakeTime, curtime.addSecs(5));
            prerollseconds = 0;
        }
    }

    if (secsleft - prerollseconds > 30)
    {
        nextStartTime = min(nextStartTime, nextrectime.addSecs(-30));
        nextWakeTime = min(nextWakeTime,
                            nextrectime.addSecs(-prerollseconds - 30));
        return false;
    }

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

            EnqueuePlace("SlaveNotAwake");
        }

        nextStartTime = min(nextStartTime, nextrectime);
        nextWakeTime = min(nextWakeTime, curtime.addSecs(1));
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
                                ri.GetInputID(),
                                recording_dir,
                                reclist);
        ri.SetPathname(recording_dir);
        tempri.SetPathname(recording_dir);
    }

    if (ri.GetRecordingStatus() != RecStatus::Pending)
    {
        if (sinputinfomap[ri.GetInputID()].schedgroup)
        {
            if (!AssignGroupInput(tempri))
                return reclist_changed;
            ri.SetInputID(tempri.GetInputID());
            nexttv = (*m_tvList)[ri.GetInputID()];
        }

        ri.SetRecordingStatus(RecStatus::Pending);
        tempri.SetRecordingStatus(RecStatus::Pending);
        ri.AddHistory(false, false, true);
        schedLock.unlock();
        nexttv->RecordPending(&tempri, max(secsleft, 0), false);
        schedLock.lock();
        if (reclist_changed)
            return reclist_changed;
    }

    if (secsleft - prerollseconds > 0)
    {
        nextStartTime = min(nextStartTime, nextrectime);
        nextWakeTime = min(nextWakeTime,
                            nextrectime.addSecs(-prerollseconds));
        return false;
    }

    QDateTime recstartts = MythDate::current(true).addSecs(30);
    recstartts = QDateTime(
        recstartts.date(),
        QTime(recstartts.time().hour(), recstartts.time().minute()), Qt::UTC);
    ri.SetRecordingStartTime(recstartts);
    tempri.SetRecordingStartTime(recstartts);

    QString details = QString("%1: channel %2 on cardid %3, sourceid %4")
        .arg(ri.toString(ProgramInfo::kTitleSubtitle))
        .arg(ri.GetChanID())
        .arg(ri.GetInputID())
        .arg(ri.GetSourceID());

    RecStatus::Type recStatus = RecStatus::Offline;
    if (schedulingEnabled && nexttv->IsConnected())
    {
        if (ri.GetRecordingStatus() == RecStatus::WillRecord ||
            ri.GetRecordingStatus() == RecStatus::Pending)
        {
            schedLock.unlock();
            recStatus = nexttv->StartRecording(&tempri);
            schedLock.lock();
            ri.SetRecordingID(tempri.GetRecordingID());
            ri.SetRecordingStartTime(tempri.GetRecordingStartTime());

            // activate auto expirer
            if (m_expirer && recStatus == RecStatus::Tuning)
                m_expirer->Update(ri.GetInputID(), fsID, false);
        }
    }

    HandleRecordingStatusChange(ri, recStatus, details);
    statuschanged = true;

    return reclist_changed;
}

void Scheduler::HandleRecordingStatusChange(
    RecordingInfo &ri, RecStatus::Type recStatus, const QString &details)
{
    if (ri.GetRecordingStatus() == recStatus)
        return;

    ri.SetRecordingStatus(recStatus);

    bool doSchedAfterStart =
        ((recStatus != RecStatus::Tuning &&
          recStatus != RecStatus::Recording) ||
         schedAfterStartMap[ri.GetRecordingRuleID()] ||
         (ri.GetParentRecordingRuleID() &&
          schedAfterStartMap[ri.GetParentRecordingRuleID()]));
    ri.AddHistory(doSchedAfterStart);

    QString msg = (RecStatus::Recording == recStatus) ?
        QString("Started recording") :
        ((RecStatus::Tuning == recStatus) ?
         QString("Tuning recording") :
         QString("Canceled recording (%1)")
         .arg(RecStatus::toString(ri.GetRecordingStatus(), ri.GetRecordingRuleType())));

    LOG(VB_GENERAL, LOG_INFO, QString("%1: %2").arg(msg).arg(details));

    if ((RecStatus::Recording == recStatus) || (RecStatus::Tuning == recStatus))
    {
        UpdateNextRecord();
    }
    else if (RecStatus::Failed == recStatus)
    {
        MythEvent me(QString("FORCE_DELETE_RECORDING %1 %2")
                     .arg(ri.GetChanID())
                     .arg(ri.GetRecordingStartTime(MythDate::ISODate)));
        gCoreContext->dispatch(me);
    }
}

bool Scheduler::AssignGroupInput(RecordingInfo &ri)
{
    if (!sinputinfomap[ri.GetInputID()].schedgroup)
        return true;

    LOG(VB_SCHEDULE, LOG_DEBUG,
        QString("Assigning input for %1/%2/\"%3\"")
        .arg(ri.GetInputID())
        .arg(ri.GetChannelSchedulingID())
        .arg(ri.GetTitle()));

    uint bestid = 0;
    uint betterid = 0;
    uint goodid = 0;
    QDateTime now = MythDate::current();

    // Check each child input to find the best one to use.
    vector<uint> inputs = sinputinfomap[ri.GetInputID()].group_inputs;
    for (uint i = 0; !bestid && i < inputs.size(); ++i)
    {
        uint inputid = inputs[i];
        RecordingInfo *pend = NULL;
        RecordingInfo *rec = NULL;

        // First, see if anything is already pending or still
        // recording.
        RecIter j = reclist.begin();
        for ( ; j != reclist.end(); ++j)
        {
            RecordingInfo *p = (*j);
            if (now.secsTo(p->GetRecordingStartTime()) > 300)
                break;
            if (p->GetInputID() != inputid)
                continue;
            if (p->GetRecordingStatus() == RecStatus::Pending)
            {
                pend = p;
                break;
            }
            if (p->GetRecordingStatus() == RecStatus::Recording ||
                p->GetRecordingStatus() == RecStatus::Tuning ||
                p->GetRecordingStatus() == RecStatus::Failing)
            {
                rec = p;
            }
        }

        if (pend)
        {
            LOG(VB_SCHEDULE, LOG_DEBUG,
                QString("Input %1 has a pending recording").arg(inputid));
            continue;
        }

        if (rec)
        {
            if (rec->GetRecordingEndTime() >
                ri.GetRecordingStartTime())
            {
                LOG(VB_SCHEDULE, LOG_DEBUG,
                    QString("Input %1 is recording").arg(inputid));
            }
            else if (rec->GetRecordingEndTime() <
                ri.GetRecordingStartTime())
            {
                LOG(VB_SCHEDULE, LOG_DEBUG,
                    QString("Input %1 is recording but will be free")
                    .arg(inputid));
                bestid = inputid;
            }
            else // rec->end == ri.start
            {
                if ((ri.mplexid && rec->mplexid != ri.mplexid) ||
                    (!ri.mplexid && rec->GetChanID() != ri.GetChanID()))
                {
                    LOG(VB_SCHEDULE, LOG_DEBUG,
                        QString("Input %1 is recording but has to stop")
                        .arg(inputid));
                    bestid = inputid;
                }
                else
                {
                    LOG(VB_SCHEDULE, LOG_DEBUG,
                        QString("Input %1 is recording but could be free")
                        .arg(inputid));
                    if (!betterid)
                        betterid = inputid;
                }
            }
            continue;
        }

        InputInfo busy_info;
        EncoderLink *rctv = (*m_tvList)[inputid];
        schedLock.unlock();
        bool isbusy = rctv->IsBusy(&busy_info, -1);
        schedLock.lock();
        if (reclist_changed)
            return false;
        if (!isbusy)
        {
            LOG(VB_SCHEDULE, LOG_DEBUG,
                QString("Input %1 is free").arg(inputid));
            bestid = inputid;
        }
        else if ((ri.mplexid && busy_info.mplexid != ri.mplexid) ||
                 (!ri.mplexid && busy_info.chanid != ri.GetChanID()))
        {
            LOG(VB_SCHEDULE, LOG_DEBUG,
                QString("Input %1 is on livetv but has to stop")
                .arg(inputid));
            bestid = inputid;
        }
        else
        {
            LOG(VB_SCHEDULE, LOG_DEBUG,
                QString("Input %1 is on livetv but could be free")
                .arg(inputid));
            if (!goodid)
                goodid = inputid;
        }
    }

    if (!bestid)
        bestid = (betterid ? betterid : goodid);

    if (bestid)
    {
        LOG(VB_SCHEDULE, LOG_INFO,
            QString("Assigned input %1 for %2/%3/\"%4\"")
            .arg(bestid).arg(ri.GetInputID()).arg(ri.GetChannelSchedulingID())
            .arg(ri.GetTitle()));
        ri.SetInputID(bestid);
    }
    else
    {
        LOG(VB_SCHEDULE, LOG_WARNING,
            QString("Failed to assign input for %1/%2/\"%3\"")
            .arg(ri.GetInputID()).arg(ri.GetChannelSchedulingID())
            .arg(ri.GetTitle()));
        ri.SetRecordingStatus(RecStatus::TunerBusy);
        ri.AddHistory(true);
    }

    return bestid;
}

void Scheduler::HandleIdleShutdown(
    bool &blockShutdown, QDateTime &idleSince,
    int prerollseconds, int idleTimeoutSecs, int idleWaitForRecordingTime,
    bool &statuschanged)
{
    if ((idleTimeoutSecs <= 0) || (m_mainServer == NULL))
        return;

    // we release the block when a client connects
    // Allow the presence of a non-blocking client to release this,
    // the frontend may have connected then gone idle between scheduler runs
    if (blockShutdown)
    {
        if (m_mainServer->isClientConnected())
        {
            LOG(VB_GENERAL, LOG_NOTICE, "Client is connected, removing startup block on shutdown");
            blockShutdown = false;
        }
    }
    else
    {
        QDateTime curtime = MythDate::current();

        // find out, if we are currently recording (or LiveTV)
        bool recording = false;
        QMap<int, EncoderLink *>::Iterator it;
        for (it = m_tvList->begin(); (it != m_tvList->end()) &&
                 !recording; ++it)
        {
            if ((*it)->IsBusy())
                recording = true;
        }

        // If there are BLOCKING clients, then we're not idle
        bool blocking = m_mainServer->isClientConnected(true);
        if (!blocking && !recording)
        {
            // have we received a RESET_IDLETIME message?
            resetIdleTime_lock.lock();
            if (resetIdleTime)
            {
                // yes - so reset the idleSince time
                if (idleSince.isValid())
                {
                    MythEvent me(QString("SHUTDOWN_COUNTDOWN -1"));
                    gCoreContext->dispatch(me);
                }
                idleSince = QDateTime();
                resetIdleTime = false;
            }
            resetIdleTime_lock.unlock();

            if (statuschanged || !idleSince.isValid())
            {
                bool wasValid = idleSince.isValid();
                if (!wasValid)
                    idleSince = curtime;

                RecIter idleIter = reclist.begin();
                for ( ; idleIter != reclist.end(); ++idleIter)
                    if ((*idleIter)->GetRecordingStatus() ==
                        RecStatus::WillRecord ||
                        (*idleIter)->GetRecordingStatus() ==
                        RecStatus::Pending)
                        break;

                if (idleIter != reclist.end())
                {
                    if ((curtime.secsTo((*idleIter)->GetRecordingStartTime()) -
                        prerollseconds) <
                        ((idleWaitForRecordingTime * 60) + idleTimeoutSecs))
                    {
                        LOG(VB_IDLE, LOG_NOTICE, "Blocking shutdown because "
                                                 "a recording is due to "
                                                 "start soon.");
                        idleSince = QDateTime();
                    }
                }

                // If we're due to grab guide data, then block shutdown
                if (gCoreContext->GetNumSetting("MythFillGrabberSuggestsTime") &&
                    gCoreContext->GetNumSetting("MythFillEnabled"))
                {
                    QString str = gCoreContext->GetSetting("MythFillSuggestedRunTime");
                    QDateTime guideRunTime = MythDate::fromString(str);

                    if (guideRunTime.isValid() &&
                        (guideRunTime > MythDate::current()) &&
                        (curtime.secsTo(guideRunTime) <
                        (idleWaitForRecordingTime * 60)))
                    {
                        LOG(VB_IDLE, LOG_NOTICE, "Blocking shutdown because "
                                                 "mythfilldatabase is due to "
                                                 "run soon.");
                        idleSince = QDateTime();
                    }
                }

                // Before starting countdown check shutdown is OK
                if (idleSince.isValid())
                    CheckShutdownServer(prerollseconds, idleSince, blockShutdown);

                if (wasValid && !idleSince.isValid())
                {
                    MythEvent me(QString("SHUTDOWN_COUNTDOWN -1"));
                    gCoreContext->dispatch(me);
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
                    else if (CheckShutdownServer(prerollseconds,
                                                 idleSince,
                                                 blockShutdown))
                    {
                        ShutdownServer(prerollseconds, idleSince);
                    }
                    else
                    {
                        MythEvent me(QString("SHUTDOWN_COUNTDOWN -1"));
                        gCoreContext->dispatch(me);
                    }
                }
                else
                {
                    int itime = idleSince.secsTo(curtime);
                    QString msg;
                    if (itime <= 1)
                    {
                        msg = QString("I\'m idle now... shutdown will "
                                      "occur in %1 seconds.")
                            .arg(idleTimeoutSecs);
                        LOG(VB_GENERAL, LOG_NOTICE, msg);
                        MythEvent me(QString("SHUTDOWN_COUNTDOWN %1")
                                     .arg(idleTimeoutSecs));
                        gCoreContext->dispatch(me);
                    }
                    else
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
            if (blocking)
                LOG(VB_IDLE, LOG_NOTICE, "Blocking shutdown because "
                                         "of a connected client");
            else if (recording)
                LOG(VB_IDLE, LOG_NOTICE, "Blocking shutdown because "
                                         "of an active encoder");

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

        switch(state)
        {
            case 0:
                LOG(VB_IDLE, LOG_INFO,
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
            case GENERIC_EXIT_NOT_OK:
                LOG(VB_GENERAL, LOG_NOTICE,
                    "CheckShutdownServer returned - Not OK");
                break;
            default:
                LOG(VB_GENERAL, LOG_NOTICE, QString(
                    "CheckShutdownServer returned - Error %1").arg(state));
                break;
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
        if ((*recIter)->GetRecordingStatus() == RecStatus::WillRecord ||
            (*recIter)->GetRecordingStatus() == RecStatus::Pending)
            break;

    // set the wakeuptime if needed
    if (recIter != reclist.end())
    {
        RecordingInfo *nextRecording = (*recIter);
        QDateTime restarttime = nextRecording->GetRecordingStartTime()
            .addSecs((-1) * prerollseconds);

        // Check if we need to wake up to grab guide date before the next recording
        QString str = gCoreContext->GetSetting("MythFillSuggestedRunTime");
        QDateTime guideRefreshTime = MythDate::fromString(str);

        if (gCoreContext->GetNumSetting("MythFillGrabberSuggestsTime") &&
            guideRefreshTime.isValid() &&
            (guideRefreshTime > MythDate::current()) &&
            (guideRefreshTime < restarttime))
            restarttime = guideRefreshTime;

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
            setwakeup_cmd.replace(
                "$time", restarttime.toLocalTime().toString(wakeup_timeformat));

        LOG(VB_GENERAL, LOG_NOTICE,
            QString("Running the command to set the next "
                    "scheduled wakeup time :-\n\t\t\t\t") + setwakeup_cmd);

        // now run the command to set the wakeup time
        if (myth_system(setwakeup_cmd) != GENERIC_EXIT_OK)
        {
            LOG(VB_GENERAL, LOG_ERR,
                "SetWakeuptimeCommand failed, shutdown aborted");
            idleSince = QDateTime();
            m_isShuttingDown = false;
            return;
        }

        gCoreContext->SaveSettingOnHost("MythShutdownWakeupTime",
                                        MythDate::toString(restarttime, MythDate::kDatabase),
                                        NULL);
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
                    "this computer :-\n\t\t\t\t") + halt_cmd);

        // and now shutdown myself
        schedLock.unlock();
        uint res = myth_system(halt_cmd);
        schedLock.lock();
        if (res != GENERIC_EXIT_OK)
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
    QDateTime curtime = MythDate::current();
    QStringList SlavesInUse;
    for ( ; recIter != reclist.end(); ++recIter)
    {
        RecordingInfo *pginfo = *recIter;

        if (pginfo->GetRecordingStatus() != RecStatus::Recording &&
            pginfo->GetRecordingStatus() != RecStatus::Tuning &&
            pginfo->GetRecordingStatus() != RecStatus::Failing &&
            pginfo->GetRecordingStatus() != RecStatus::WillRecord &&
            pginfo->GetRecordingStatus() != RecStatus::Pending)
            continue;

        secsleft = curtime.secsTo(
            pginfo->GetRecordingStartTime()) - prerollseconds;
        if (secsleft > sleepThreshold)
            continue;

        if (m_tvList->find(pginfo->GetInputID()) != m_tvList->end())
        {
            enc = (*m_tvList)[pginfo->GetInputID()];
            if ((!enc->IsLocal()) &&
                (!SlavesInUse.contains(enc->GetHostName())))
            {
                if (pginfo->GetRecordingStatus() == RecStatus::WillRecord ||
                    pginfo->GetRecordingStatus() == RecStatus::Pending)
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
    QDateTime oneHourAgo = MythDate::current().addSecs(-61 * 60);
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
                                    .arg(slv->GetInputID())
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

    QDateTime curtime = MythDate::current();
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

void Scheduler::UpdateManuals(uint recordid)
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

    if (!query.next())
        return;

    RecordingType rectype = RecordingType(query.value(0).toInt());
    QString title = query.value(1).toString();
    QString station = query.value(2).toString() ;
    QDateTime startdt = QDateTime(query.value(3).toDate(),
                                  query.value(4).toTime(), Qt::UTC);
    int duration = startdt.secsTo(
        QDateTime(query.value(5).toDate(),
                  query.value(6).toTime(), Qt::UTC));

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
    int daysoff;
    QDateTime lstartdt = startdt.toLocalTime();

    switch (rectype)
    {
    case kSingleRecord:
    case kOverrideRecord:
    case kDontRecord:
        progcount = 1;
        skipdays = 1;
        weekday = false;
        daysoff = 0;
        break;
    case kDailyRecord:
        progcount = 13;
        skipdays = 1;
        weekday = (lstartdt.date().dayOfWeek() < 6);
        daysoff = lstartdt.date().daysTo(
            MythDate::current().toLocalTime().date());
        startdt = QDateTime(lstartdt.date().addDays(daysoff),
                            lstartdt.time(), Qt::LocalTime).toUTC();
        break;
    case kWeeklyRecord:
        progcount = 2;
        skipdays = 7;
        weekday = false;
        daysoff = lstartdt.date().daysTo(
            MythDate::current().toLocalTime().date());
        daysoff = (daysoff + 6) / 7 * 7;
        startdt = QDateTime(lstartdt.date().addDays(daysoff),
                            lstartdt.time(), Qt::LocalTime).toUTC();
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
            if (weekday && startdt.toLocalTime().date().dayOfWeek() >= 6)
                continue;

            query.prepare("REPLACE INTO program (chanid, starttime, endtime,"
                          " title, subtitle, manualid, generic) "
                          "VALUES (:CHANID, :STARTTIME, :ENDTIME, :TITLE,"
                          " :SUBTITLE, :RECORDID, 1)");
            query.bindValue(":CHANID", chanidlist[i]);
            query.bindValue(":STARTTIME", startdt);
            query.bindValue(":ENDTIME", startdt.addSecs(duration));
            query.bindValue(":TITLE", title);
            query.bindValue(":SUBTITLE", startdt.toLocalTime());
            query.bindValue(":RECORDID", recordid);
            if (!query.exec())
            {
                MythDB::DBError("UpdateManuals", query);
                return;
            }
        }

        daysoff += skipdays;
        startdt = QDateTime(lstartdt.date().addDays(daysoff),
                            lstartdt.time(), Qt::LocalTime).toUTC();
    }
}

void Scheduler::BuildNewRecordsQueries(uint recordid, QStringList &from,
                                       QStringList &where,
                                       MSqlBindings &bindings)
{
    MSqlQuery result(dbConn);
    QString query;
    QString qphrase;

    query = QString("SELECT recordid,search,subtitle,description "
                    "FROM %1 WHERE search <> %2 AND "
                    "(recordid = %3 OR %4 = 0) ")
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

    if (recordid == 0 || from.count() == 0)
    {
        QString recidmatch = "";
        if (recordid != 0)
            recidmatch = "RECTABLE.recordid = :NRRECORDID AND ";
        QString s1 = recidmatch +
            "RECTABLE.type <> :NRTEMPLATE AND "
            "RECTABLE.search = :NRST AND "
            "program.manualid = 0 AND "
            "program.title = RECTABLE.title ";
        s1.replace("RECTABLE", recordTable);
        QString s2 = recidmatch +
            "RECTABLE.type <> :NRTEMPLATE AND "
            "RECTABLE.search = :NRST AND "
            "program.manualid = 0 AND "
            "program.seriesid <> '' AND "
            "program.seriesid = RECTABLE.seriesid ";
        s2.replace("RECTABLE", recordTable);

        from << "";
        where << s1;
        from << "";
        where << s2;
        bindings[":NRTEMPLATE"] = kTemplateRecord;
        bindings[":NRST"] = kNoSearch;
        if (recordid != 0)
            bindings[":NRRECORDID"] = recordid;
    }
}

static QString progdupinit = QString(
"(CASE "
"  WHEN RECTABLE.type IN (%1, %2, %3) THEN  0 "
"  WHEN RECTABLE.type IN (%4, %5, %6) THEN  -1 "
"  ELSE (program.generic - 1) "
" END) ")
    .arg(kSingleRecord).arg(kOverrideRecord).arg(kDontRecord)
    .arg(kOneRecord).arg(kDailyRecord).arg(kWeeklyRecord);

static QString progfindid = QString(
"(CASE RECTABLE.type "
"  WHEN %1 "
"   THEN RECTABLE.findid "
"  WHEN %2 "
"   THEN to_days(date_sub(convert_tz(program.starttime, 'UTC', 'SYSTEM'), "
"            interval time_format(RECTABLE.findtime, '%H:%i') hour_minute)) "
"  WHEN %3 "
"   THEN floor((to_days(date_sub(convert_tz(program.starttime, 'UTC', "
"            'SYSTEM'), interval time_format(RECTABLE.findtime, '%H:%i') "
"            hour_minute)) - RECTABLE.findday)/7) * 7 + RECTABLE.findday "
"  WHEN %4 "
"   THEN RECTABLE.findid "
"  ELSE 0 "
" END) ")
        .arg(kOneRecord)
        .arg(kDailyRecord)
        .arg(kWeeklyRecord)
        .arg(kOverrideRecord);

void Scheduler::UpdateMatches(uint recordid, uint sourceid, uint mplexid,
                              const QDateTime &maxstarttime)
{
    struct timeval dbstart, dbend;

    MSqlQuery query(dbConn);
    MSqlBindings bindings;
    QString deleteClause;
    QString filterClause = QString(" AND program.endtime > "
                                   "(NOW() - INTERVAL 480 MINUTE)");

    if (recordid)
    {
        deleteClause += " AND recordmatch.recordid = :RECORDID";
        bindings[":RECORDID"] = recordid;
    }
    if (sourceid)
    {
        deleteClause += " AND channel.sourceid = :SOURCEID";
        filterClause += " AND channel.sourceid = :SOURCEID";
        bindings[":SOURCEID"] = sourceid;
    }
    if (mplexid)
    {
        deleteClause += " AND channel.mplexid = :MPLEXID";
        filterClause += " AND channel.mplexid = :MPLEXID";
        bindings[":MPLEXID"] = mplexid;
    }
    if (maxstarttime.isValid())
    {
        deleteClause += " AND recordmatch.starttime <= :MAXSTARTTIME";
        filterClause += " AND program.starttime <= :MAXSTARTTIME";
        bindings[":MAXSTARTTIME"] = maxstarttime;
    }

    query.prepare(QString("DELETE recordmatch FROM recordmatch, channel "
                          "WHERE recordmatch.chanid = channel.chanid")
                  + deleteClause);
    MSqlBindings::const_iterator it;
    for (it = bindings.begin(); it != bindings.end(); ++it)
        query.bindValue(it.key(), it.value());
    if (!query.exec())
    {
        MythDB::DBError("UpdateMatches1", query);
        return;
    }
    if (recordid)
        bindings.remove(":RECORDID");

    query.prepare("SELECT filterid, clause FROM recordfilter "
                  "WHERE filterid >= 0 AND filterid < :NUMFILTERS AND "
                  "      TRIM(clause) <> ''");
    query.bindValue(":NUMFILTERS", RecordingRule::kNumFilters);
    if (!query.exec())
    {
        MythDB::DBError("UpdateMatches2", query);
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
    query.bindValue(":FINDONE", kOneRecord);
    if (!query.exec())
    {
        MythDB::DBError("UpdateMatches3", query);
        return;
    }
    else if (query.size())
    {
        QDate epoch(1970, 1, 1);
        int findtoday =
            epoch.daysTo(MythDate::current().date()) + 719528;
        query.prepare("UPDATE record set findid = :FINDID "
                      "WHERE type = :FINDONE AND findid <= 0;");
        query.bindValue(":FINDID", findtoday);
        query.bindValue(":FINDONE", kOneRecord);
        if (!query.exec())
            MythDB::DBError("UpdateMatches4", query);
    }

    int clause;
    QStringList fromclauses, whereclauses;

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
"REPLACE INTO recordmatch (recordid, chanid, starttime, manualid, "
"                          oldrecduplicate, findid) "
"SELECT RECTABLE.recordid, program.chanid, program.starttime, "
" IF(search = %1, RECTABLE.recordid, 0), ").arg(kManualSearch) +
            progdupinit + ", " + progfindid + QString(
"FROM (RECTABLE, program INNER JOIN channel "
"      ON channel.chanid = program.chanid) ") + fromclauses[clause] + QString(
" WHERE ") + whereclauses[clause] +
    QString(" AND channel.visible = 1 ") +
    filterClause + QString(" AND "

"("
" (RECTABLE.type = %1 " // all record
"  OR RECTABLE.type = %2 " // one record
"  OR RECTABLE.type = %3 " // daily record
"  OR RECTABLE.type = %4) " // weekly record
" OR "
"  ((RECTABLE.type = %6 " // single record
"   OR RECTABLE.type = %7 " // override record
"   OR RECTABLE.type = %8)" // don't record
"   AND "
"   ADDTIME(RECTABLE.startdate, RECTABLE.starttime) = program.starttime " // date/time matches
"   AND "
"   RECTABLE.station = channel.callsign) " // channel matches
") ")
            .arg(kAllRecord)
            .arg(kOneRecord)
            .arg(kDailyRecord)
            .arg(kWeeklyRecord)
            .arg(kSingleRecord)
            .arg(kOverrideRecord)
            .arg(kDontRecord);

        query.replace("RECTABLE", recordTable);

        LOG(VB_SCHEDULE, LOG_INFO, QString(" |-- Start DB Query %1...")
                .arg(clause));

        gettimeofday(&dbstart, NULL);
        MSqlQuery result(dbConn);
        result.prepare(query);

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

void Scheduler::CreateTempTables(void)
{
    MSqlQuery result(dbConn);

    if (recordTable == "record")
    {
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
            MythDB::DBError("Creating sched_temp_record table", result);
            return;
        }
        result.prepare("INSERT sched_temp_record SELECT * from record;");
        if (!result.exec())
        {
            MythDB::DBError("Populating sched_temp_record table", result);
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
}

void Scheduler::DeleteTempTables(void)
{
    MSqlQuery result(dbConn);

    if (recordTable == "record")
    {
        result.prepare("DROP TABLE IF EXISTS sched_temp_record;");
        if (!result.exec())
            MythDB::DBError("DeleteTempTables sched_temp_record", result);
    }

    result.prepare("DROP TABLE IF EXISTS sched_temp_recorded;");
    if (!result.exec())
        MythDB::DBError("DeleteTempTables drop table", result);
}

void Scheduler::UpdateDuplicates(void)
{
    QString schedTmpRecord = recordTable;
    if (schedTmpRecord == "record")
        schedTmpRecord = "sched_temp_record";

    QString rmquery = QString(
"UPDATE recordmatch "
" INNER JOIN RECTABLE ON (recordmatch.recordid = RECTABLE.recordid) "
" INNER JOIN program p ON (recordmatch.chanid = p.chanid AND "
"                          recordmatch.starttime = p.starttime AND "
"                          recordmatch.manualid = p.manualid) "
" LEFT JOIN oldrecorded ON "
"  ( "
"    RECTABLE.dupmethod > 1 AND "
"    oldrecorded.duplicate <> 0 AND "
"    p.title = oldrecorded.title AND "
"    p.generic = 0 "
"     AND "
"     ( "
"      (p.programid <> '' "
"       AND p.programid = oldrecorded.programid) "
"      OR "
"      ( ") +
        (ProgramInfo::UsingProgramIDAuthority() ?
"       (p.programid = '' OR oldrecorded.programid = '' OR "
"         LEFT(p.programid, LOCATE('/', p.programid)) <> "
"         LEFT(oldrecorded.programid, LOCATE('/', oldrecorded.programid))) " :
"       (p.programid = '' OR oldrecorded.programid = '') " )
        + QString(
"       AND "
"       (((RECTABLE.dupmethod & 0x02) = 0) OR (p.subtitle <> '' "
"          AND p.subtitle = oldrecorded.subtitle)) "
"       AND "
"       (((RECTABLE.dupmethod & 0x04) = 0) OR (p.description <> '' "
"          AND p.description = oldrecorded.description)) "
"       AND "
"       (((RECTABLE.dupmethod & 0x08) = 0) OR "
"          (p.subtitle <> '' AND "
"             (p.subtitle = oldrecorded.subtitle OR "
"              (oldrecorded.subtitle = '' AND "
"               p.subtitle = oldrecorded.description))) OR "
"          (p.subtitle = '' AND p.description <> '' AND "
"             (p.description = oldrecorded.subtitle OR "
"              (oldrecorded.subtitle = '' AND "
"               p.description = oldrecorded.description)))) "
"      ) "
"     ) "
"  ) "
" LEFT JOIN sched_temp_recorded recorded ON "
"  ( "
"    RECTABLE.dupmethod > 1 AND "
"    recorded.duplicate <> 0 AND "
"    p.title = recorded.title AND "
"    p.generic = 0 AND "
"    recorded.recgroup NOT IN ('LiveTV','Deleted') "
"     AND "
"     ( "
"      (p.programid <> '' "
"       AND p.programid = recorded.programid) "
"      OR "
"      ( ") +
        (ProgramInfo::UsingProgramIDAuthority() ?
"       (p.programid = '' OR recorded.programid = '' OR "
"         LEFT(p.programid, LOCATE('/', p.programid)) <> "
"         LEFT(recorded.programid, LOCATE('/', recorded.programid))) " :
"       (p.programid = '' OR recorded.programid = '') ")
        + QString(
"       AND "
"       (((RECTABLE.dupmethod & 0x02) = 0) OR (p.subtitle <> '' "
"          AND p.subtitle = recorded.subtitle)) "
"       AND "
"       (((RECTABLE.dupmethod & 0x04) = 0) OR (p.description <> '' "
"          AND p.description = recorded.description)) "
"       AND "
"       (((RECTABLE.dupmethod & 0x08) = 0) OR "
"          (p.subtitle <> '' AND "
"             (p.subtitle = recorded.subtitle OR "
"              (recorded.subtitle = '' AND "
"               p.subtitle = recorded.description))) OR "
"          (p.subtitle = '' AND p.description <> '' AND "
"             (p.description = recorded.subtitle OR "
"              (recorded.subtitle = '' AND "
"               p.description = recorded.description)))) "
"      ) "
"     ) "
"  ) "
" LEFT JOIN oldfind ON "
"  (oldfind.recordid = recordmatch.recordid AND "
"   oldfind.findid = recordmatch.findid) "
" SET oldrecduplicate = (oldrecorded.endtime IS NOT NULL), "
"     recduplicate = (recorded.endtime IS NOT NULL), "
"     findduplicate = (oldfind.findid IS NOT NULL), "
"     oldrecstatus = oldrecorded.recstatus "
" WHERE p.endtime >= (NOW() - INTERVAL 480 MINUTE) "
"       AND oldrecduplicate = -1 "
);
    rmquery.replace("RECTABLE", schedTmpRecord);

    MSqlQuery result(dbConn);
    result.prepare(rmquery);
    if (!result.exec())
    {
        MythDB::DBError("UpdateDuplicates", result);
        return;
    }
}

void Scheduler::AddNewRecords(void)
{
    QString schedTmpRecord = recordTable;
    if (schedTmpRecord == "record")
        schedTmpRecord = "sched_temp_record";

    struct timeval dbstart, dbend;

    RecList tmpList;

    QMap<int, bool> cardMap;
    QMap<int, EncoderLink *>::Iterator enciter = m_tvList->begin();
    for (; enciter != m_tvList->end(); ++enciter)
    {
        EncoderLink *enc = *enciter;
        if (enc->IsConnected() || enc->IsAsleep())
            cardMap[enc->GetInputID()] = true;
    }

    QMap<int, bool> tooManyMap;
    bool checkTooMany = false;
    schedAfterStartMap.clear();

    MSqlQuery rlist(dbConn);
    rlist.prepare(QString("SELECT recordid, title, maxepisodes, maxnewest "
                          "FROM %1").arg(schedTmpRecord));

    if (!rlist.exec())
    {
        MythDB::DBError("CheckTooMany", rlist);
        return;
    }

    while (rlist.next())
    {
        int recid = rlist.value(0).toInt();
        // QString qtitle = rlist.value(1).toString();
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

    int prefinputpri    = gCoreContext->GetNumSetting("PrefInputPriority", 2);
    int hdtvpriority    = gCoreContext->GetNumSetting("HDTVRecPriority", 0);
    int wspriority      = gCoreContext->GetNumSetting("WSRecPriority", 0);
    int slpriority      = gCoreContext->GetNumSetting("SignLangRecPriority", 0);
    int onscrpriority   = gCoreContext->GetNumSetting("OnScrSubRecPriority", 0);
    int ccpriority      = gCoreContext->GetNumSetting("CCRecPriority", 0);
    int hhpriority      = gCoreContext->GetNumSetting("HardHearRecPriority", 0);
    int adpriority      = gCoreContext->GetNumSetting("AudioDescRecPriority", 0);

    QString pwrpri = "channel.recpriority + capturecard.recpriority";

    if (prefinputpri)
        pwrpri += QString(" + "
        "(capturecard.cardid = RECTABLE.prefinput) * %1").arg(prefinputpri);

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

    MSqlQuery result(dbConn);

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

    pwrpri.replace("program.","p.");
    pwrpri.replace("channel.","c.");
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
        "    capturecard.cardid, 0,                  p.seriesid,      "//24-26
        "    p.programid,       RECTABLE.inetref,    p.category_type,   "//27-29
        "    p.airdate,         p.stars,             p.originalairdate, "//30-32
        "    RECTABLE.inactive, RECTABLE.parentid,   recordmatch.findid, "//33-35
        "    RECTABLE.playgroup, oldrecstatus.recstatus, "//36-37
        "    oldrecstatus.reactivate, p.videoprop+0,     "//38-39
        "    p.subtitletypes+0, p.audioprop+0,   RECTABLE.storagegroup, "//40-42
        "    capturecard.hostname, recordmatch.oldrecstatus, NULL, "//43-45
        "    oldrecstatus.future, capturecard.schedorder, " //46-47
        "    p.syndicatedepisodenumber, p.partnumber, p.parttotal, " //48-50
        "    c.mplexid, ") +                                         //51
        pwrpri + QString(
        "FROM recordmatch "
        "INNER JOIN RECTABLE ON (recordmatch.recordid = RECTABLE.recordid) "
        "INNER JOIN program AS p "
        "ON ( recordmatch.chanid    = p.chanid    AND "
        "     recordmatch.starttime = p.starttime AND "
        "     recordmatch.manualid  = p.manualid ) "
        "INNER JOIN channel AS c "
        "ON ( c.chanid = p.chanid ) "
        "INNER JOIN capturecard "
        "ON ( c.sourceid = capturecard.sourceid AND "
        "     ( capturecard.schedorder <> 0 OR "
        "       capturecard.parentid = 0 ) ) "
        "LEFT JOIN oldrecorded as oldrecstatus "
        "ON ( oldrecstatus.station   = c.callsign  AND "
        "     oldrecstatus.starttime = p.starttime AND "
        "     oldrecstatus.title     = p.title ) "
        "WHERE p.endtime > (NOW() - INTERVAL 480 MINUTE) "
        "ORDER BY RECTABLE.recordid DESC, p.starttime, p.title, c.callsign, "
        "         c.channum ");
    query.replace("RECTABLE", schedTmpRecord);

    LOG(VB_SCHEDULE, LOG_INFO, QString(" |-- Start DB Query..."));

    gettimeofday(&dbstart, NULL);
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

    RecordingInfo *lastp = NULL;

    while (result.next())
    {
        // If this is the same program we saw in the last pass and it
        // wasn't a viable candidate, then neither is this one so
        // don't bother with it.  This is essentially an early call to
        // PruneRedundants().
        uint recordid = result.value(17).toUInt();
        QDateTime startts = MythDate::as_utc(result.value(2).toDateTime());
        QString title = result.value(4).toString();
        QString callsign = result.value(8).toString();
        if (lastp && lastp->GetRecordingStatus() != RecStatus::Unknown
            && lastp->GetRecordingStatus() != RecStatus::Offline
            && lastp->GetRecordingStatus() != RecStatus::DontRecord
            && recordid == lastp->GetRecordingRuleID()
            && startts == lastp->GetScheduledStartTime()
            && title == lastp->GetTitle()
            && callsign == lastp->GetChannelSchedulingID())
            continue;

        uint mplexid = result.value(51).toUInt();
        if (mplexid == 32767)
            mplexid = 0;

        RecordingInfo *p = new RecordingInfo(
            title,
            result.value(5).toString(),//subtitle
            result.value(6).toString(),//description
            0, // season
            0, // episode
            0, // total episodes
            result.value(48).toString(),//synidcatedepisode
            result.value(11).toString(),//category

            result.value(0).toUInt(),//chanid
            result.value(7).toString(),//channum
            callsign,
            result.value(9).toString(),//channame

            result.value(21).toString(),//recgroup
            result.value(36).toString(),//playgroup

            result.value(43).toString(),//hostname
            result.value(42).toString(),//storagegroup

            result.value(30).toUInt(),//year
            result.value(49).toUInt(),//partnumber
            result.value(50).toUInt(),//parttotal

            result.value(26).toString(),//seriesid
            result.value(27).toString(),//programid
            result.value(28).toString(),//inetref
            string_to_myth_category_type(result.value(29).toString()),//catType

            result.value(12).toInt(),//recpriority

            startts,
            MythDate::as_utc(result.value(3).toDateTime()),//endts
            MythDate::as_utc(result.value(18).toDateTime()),//recstartts
            MythDate::as_utc(result.value(19).toDateTime()),//recendts

            result.value(31).toDouble(),//stars
            (result.value(32).isNull()) ? QDate() :
            QDate::fromString(result.value(32).toString(), Qt::ISODate),
            //originalAirDate

            result.value(20).toInt(),//repeat

            RecStatus::Type(result.value(37).toInt()),//oldrecstatus
            result.value(38).toInt(),//reactivate

            recordid,
            result.value(34).toUInt(),//parentid
            RecordingType(result.value(16).toInt()),//rectype
            RecordingDupInType(result.value(13).toInt()),//dupin
            RecordingDupMethodType(result.value(22).toInt()),//dupmethod

            result.value(1).toUInt(),//sourceid
            result.value(24).toUInt(),//inputid

            result.value(35).toUInt(),//findid

            result.value(23).toInt() == COMM_DETECT_COMMFREE,//commfree
            result.value(40).toUInt(),//subtitleType
            result.value(39).toUInt(),//videoproperties
            result.value(41).toUInt(),//audioproperties
            result.value(46).toInt(),//future
            result.value(47).toInt(),//schedorder
            mplexid,                 //mplexid
            result.value(24).toUInt()); //sgroupid

        if (!p->future && !p->IsReactivated() &&
            p->oldrecstatus != RecStatus::Aborted &&
            p->oldrecstatus != RecStatus::NotListed)
        {
            p->SetRecordingStatus(p->oldrecstatus);
        }

        p->SetRecordingPriority2(result.value(52).toInt());

        // Check to see if the program is currently recording and if
        // the end time was changed.  Ideally, checking for a new end
        // time should be done after PruneOverlaps, but that would
        // complicate the list handling.  Do it here unless it becomes
        // problematic.
        RecIter rec = worklist.begin();
        for ( ; rec != worklist.end(); ++rec)
        {
            RecordingInfo *r = *rec;
            if (p->IsSameTitleStartTimeAndChannel(*r))
            {
                if (r->sgroupid == p->sgroupid &&
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

        lastp = p;

        if (p->GetRecordingStatus() != RecStatus::Unknown)
        {
            tmpList.push_back(p);
            continue;
        }

        RecStatus::Type newrecstatus = RecStatus::Unknown;
        // Check for RecStatus::Offline
        if ((doRun || specsched) &&
            (!cardMap.contains(p->GetInputID()) || !p->schedorder))
        {
            newrecstatus = RecStatus::Offline;
            if (p->schedorder == 0)
            {
                LOG(VB_GENERAL, LOG_WARNING, LOC +
                    QString("Channel %1, Title %2 %3 cardinput.schedorder = %4, "
                            "it must be >0 to record from this input.")
                    .arg(p->GetChannelName()).arg(p->GetTitle())
                    .arg(p->GetScheduledStartTime().toString())
                    .arg(p->schedorder));
            }
        }

        // Check for RecStatus::TooManyRecordings
        if (checkTooMany && tooManyMap[p->GetRecordingRuleID()] &&
            !p->IsReactivated())
        {
            newrecstatus = RecStatus::TooManyRecordings;
        }

        // Check for RecStatus::CurrentRecording and RecStatus::PreviousRecording
        if (p->GetRecordingRuleType() == kDontRecord)
            newrecstatus = RecStatus::DontRecord;
        else if (result.value(15).toInt() && !p->IsReactivated())
            newrecstatus = RecStatus::PreviousRecording;
        else if (p->GetRecordingRuleType() != kSingleRecord &&
                 p->GetRecordingRuleType() != kOverrideRecord &&
                 !p->IsReactivated() &&
                 !(p->GetDuplicateCheckMethod() & kDupCheckNone))
        {
            const RecordingDupInType dupin = p->GetDuplicateCheckSource();

            if ((dupin & kDupsNewEpi) && p->IsRepeat())
                newrecstatus = RecStatus::Repeat;

            if ((dupin & kDupsInOldRecorded) && result.value(10).toInt())
            {
                if (result.value(44).toInt() == RecStatus::NeverRecord)
                    newrecstatus = RecStatus::NeverRecord;
                else
                    newrecstatus = RecStatus::PreviousRecording;
            }

            if ((dupin & kDupsInRecorded) && result.value(14).toInt())
                newrecstatus = RecStatus::CurrentRecording;
        }

        bool inactive = result.value(33).toInt();
        if (inactive)
            newrecstatus = RecStatus::Inactive;

        // Mark anything that has already passed as some type of
        // missed.  If it survives PruneOverlaps, it will get deleted
        // or have its old status restored in PruneRedundants.
        if (p->GetRecordingEndTime() < schedTime)
        {
            if (p->future)
                newrecstatus = RecStatus::MissedFuture;
            else
                newrecstatus = RecStatus::Missed;
        }

        p->SetRecordingStatus(newrecstatus);

        tmpList.push_back(p);
    }

    LOG(VB_SCHEDULE, LOG_INFO, " +-- Cleanup...");
    RecIter tmp = tmpList.begin();
    for ( ; tmp != tmpList.end(); ++tmp)
        worklist.push_back(*tmp);
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
        "WHERE (type = %1 OR type = %2) AND "
        "      recordmatch.chanid IS NULL")
        .arg(kSingleRecord)
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

    QDateTime now = MythDate::current();

    while (result.next())
    {
        RecordingType rectype = RecordingType(result.value(21).toInt());
        QDateTime startts(
            result.value(16).toDate(), result.value(17).toTime(), Qt::UTC);
        QDateTime endts(
            result.value(18).toDate(), result.value(19).toTime(), Qt::UTC);

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

            RecStatus::NotListed, // Recording Status

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

/** \brief Returns all scheduled programs
 *
 *  \note Caller is responsible for deleting the RecordingInfo's returned.
 */
void Scheduler::GetAllScheduled(RecList &proglist, SchedSortColumn sortBy,
                                bool ascending)
{
    QString sortColumn = "title";
    // Q: Why don't we use a string containing the column name instead?
    // A: It's too fragile, we'll refuse to compile if an invalid enum name is
    //    used but not if an invalid column is specified. It also means that if
    //    the column names change we only need to update one place not several
    switch (sortBy)
    {
        case kSortTitle:
            sortColumn = "record.title";
            break;
        case kSortPriority:
            sortColumn = "record.recpriority";
            break;
        case kSortLastRecorded:
            sortColumn = "record.last_record";
            break;
        case kSortNextRecording:
            // We want to shift the rules which have no upcoming recordings to
            // the back of the pack, most of the time the user won't be interested
            // in rules that aren't matching recordings at the present time.
            // We still want them available in the list however since vanishing rules
            // violates the principle of least surprise
            sortColumn = "record.next_record IS NULL, record.next_record";
            break;
        case kSortType:
            sortColumn = "record.type";
            break;
    }

    QString order = "ASC";
    if (!ascending)
        order = "DESC";

    QString query = QString(
        "SELECT record.title,       record.subtitle,    " //  0,1
        "       record.description, record.season,      " //  2,3
        "       record.episode,     record.category,    " //  4,5
        "       record.chanid,      channel.channum,    " //  6,7
        "       record.station,     channel.name,       " //  8,9
        "       record.recgroup,    record.playgroup,   " // 10,11
        "       record.seriesid,    record.programid,   " // 12,13
        "       record.inetref,     record.recpriority, " // 14,15
        "       record.startdate,   record.starttime,   " // 16,17
        "       record.enddate,     record.endtime,     " // 18,19
        "       record.recordid,    record.type,        " // 20,21
        "       record.dupin,       record.dupmethod,   " // 22,23
        "       record.findid,                          " // 24
        "       channel.commmethod                      " // 25
        "FROM record "
        "LEFT JOIN channel ON channel.callsign = record.station "
        "GROUP BY recordid "
        "ORDER BY %1 %2");

    query = query.arg(sortColumn).arg(order);

    MSqlQuery result(MSqlQuery::InitCon());
    result.prepare(query);

    if (!result.exec())
    {
        MythDB::DBError("GetAllScheduled", result);
        return;
    }

    while (result.next())
    {
        RecordingType rectype = RecordingType(result.value(21).toInt());
        QDateTime startts = QDateTime(result.value(16).toDate(),
                                      result.value(17).toTime(), Qt::UTC);
        QDateTime endts = QDateTime(result.value(18).toDate(),
                                    result.value(19).toTime(), Qt::UTC);
        // Prevent invalid date/time warnings later
        if (!startts.isValid())
            startts = QDateTime(MythDate::current().date(), QTime(0,0),
                                Qt::UTC);
        if (!endts.isValid())
            endts = startts;

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

            RecStatus::Unknown,

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

// prefer dirs with less weight (disk I/O) over dirs with more weight.
// if weights are equal, prefer dirs with more absolute free space over less
static bool comp_storage_disk_io(FileSystemInfo *a, FileSystemInfo *b)
{
    if (a->getWeight() < b->getWeight())
    {
        return true;
    }
    else if (a->getWeight() == b->getWeight())
    {
        if (a->getFreeSpace() > b->getFreeSpace())
            return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////

void Scheduler::GetNextLiveTVDir(uint cardid)
{
    QMutexLocker lockit(&schedLock);

    if (!m_tvList->contains(cardid))
        return;

    EncoderLink *tv = (*m_tvList)[cardid];

    QDateTime cur = MythDate::current(true);
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

    uint cnt = 0;
    while (!m_mainServer)
    {
        if (cnt++ % 20 == 0)
            LOG(VB_SCHEDULE, LOG_WARNING, "Waiting for main server.");
        usleep(50000);
    }

    int fsID = -1;
    MSqlQuery query(MSqlQuery::InitCon());
    QMap<QString, FileSystemInfo>::Iterator fsit;
    QMap<QString, FileSystemInfo>::Iterator fsit2;
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
            QDateTime recStart(   MythDate::as_utc(query.value(1).toDateTime()));
            QDateTime recEnd(     MythDate::as_utc(query.value(2).toDateTime()));
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
            (thispg->GetRecordingStatus() != RecStatus::WillRecord &&
             thispg->GetRecordingStatus() != RecStatus::Pending) ||
            (thispg->GetInputID() == 0) ||
            (recsCounted.contains(QString("%1:%2").arg(thispg->GetChanID())
                   .arg(thispg->GetRecordingStartTime(MythDate::ISODate)))) ||
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
                        .arg(thispg->GetRecordingStartTime(MythDate::ISODate))
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
        (fsInfoCacheFillTime > MythDate::current().addSecs(-180)))
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

    fsInfoCacheFillTime = MythDate::current();
}

void Scheduler::SchedLiveTV(void)
{
    int prerollseconds = gCoreContext->GetNumSetting("RecordPreRoll", 0);
    QDateTime curtime = MythDate::current();
    int secsleft = curtime.secsTo(livetvTime);

    // This check needs to be longer than the related one in
    // HandleRecording().
    if (secsleft - prerollseconds > 120)
        return;

    // Build a list of active livetv programs
    QMap<int, EncoderLink *>::Iterator enciter = m_tvList->begin();
    for (; enciter != m_tvList->end(); ++enciter)
    {
        EncoderLink *enc = *enciter;

        if (kState_WatchingLiveTV != enc->GetState())
            continue;

        InputInfo in;
        enc->IsBusy(&in);

        if (!in.inputid)
            continue;

        // Get the program that will be recording on this channel at
        // record start time and assume this LiveTV session continues
        // for at least another 30 minutes from now.
        RecordingInfo *dummy = new RecordingInfo(
            in.chanid, livetvTime, true, 4);
        dummy->SetRecordingStartTime(schedTime);
        if (schedTime.secsTo(dummy->GetRecordingEndTime()) < 1800)
            dummy->SetRecordingEndTime(schedTime.addSecs(1800));
        dummy->SetInputID(enc->GetInputID());
        dummy->mplexid = dummy->QueryMplexID();
        dummy->sgroupid = sinputinfomap[dummy->GetInputID()].sgroupid;
        dummy->SetRecordingStatus(RecStatus::Unknown);

        livetvlist.push_front(dummy);
    }

    if (livetvlist.empty())
        return;

    SchedNewRetryPass(livetvlist.begin(), livetvlist.end(), false, true);

    while (!livetvlist.empty())
    {
        RecordingInfo *p = livetvlist.back();
        delete p;
        livetvlist.pop_back();
    }
}

/* Determines if the system was started by the auto-wakeup process */
bool Scheduler::WasStartedAutomatically()
{
    bool autoStart = false;

    QDateTime startupTime = QDateTime();
    QString s = gCoreContext->GetSetting("MythShutdownWakeupTime", "");
    if (s.length())
        startupTime = MythDate::fromString(s);

    // if we don't have a valid startup time assume we were started manually
    if (startupTime.isValid())
    {
        int startupSecs = gCoreContext->GetNumSetting("StartupSecsBeforeRecording");
        // If we started within 'StartupSecsBeforeRecording' OR 15 minutes
        // of the saved wakeup time assume we either started automatically
        // to record, to obtain guide data or or for a
        // daily wakeup/shutdown period
        if (abs(startupTime.secsTo(MythDate::current())) <
            max(startupSecs, 15 * 60))
        {
            LOG(VB_GENERAL, LOG_INFO,
                "Close to auto-start time, AUTO-Startup assumed");

            QString str = gCoreContext->GetSetting("MythFillSuggestedRunTime");
            QDateTime guideRunTime = MythDate::fromString(str);
            if (guideRunTime.secsTo(MythDate::current()) <
                max(startupSecs, 15 * 60))
            {
                LOG(VB_GENERAL, LOG_INFO,
                    "Close to MythFillDB suggested run time, AUTO-Startup to fetch guide data?");
            }
            autoStart = true;
        }
        else
            LOG(VB_GENERAL, LOG_DEBUG,
                "NOT close to auto-start time, USER-initiated startup assumed");
    }
    else if (!s.isEmpty())
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Invalid MythShutdownWakeupTime specified in database (%1)")
                .arg(s));

    return autoStart;
}

void Scheduler::CreateConflictLists(void)
{
    // For each input, create a set containing all of the inputs
    // (including itself) that are grouped with it.
    MSqlQuery query(MSqlQuery::InitCon());
    QMap<uint, QSet<uint> > inputSets;
    query.prepare("SELECT DISTINCT ci1.cardid, ci2.cardid "
                  "FROM capturecard ci1, capturecard ci2, "
                  "     inputgroup ig1, inputgroup ig2 "
                  "WHERE ci1.cardid = ig1.cardinputid AND "
                  "      ci2.cardid = ig2.cardinputid AND"
                  "      ig1.inputgroupid = ig2.inputgroupid AND "
                  "      ci1.cardid <= ci2.cardid "
                  "ORDER BY ci1.cardid, ci2.cardid");
    if (!query.exec())
    {
        MythDB::DBError("BuildConflictLists", query);
        return;
    }
    while (query.next())
    {
        uint id0 = query.value(0).toUInt();
        uint id1 = query.value(1).toUInt();
        inputSets[id0].insert(id1);
        inputSets[id1].insert(id0);
    }

    QMap<uint, QSet<uint> >::iterator mit;
    for (mit = inputSets.begin(); mit != inputSets.end(); ++mit)
    {
        uint inputid = mit.key();
        if (sinputinfomap[inputid].conflictlist)
            continue;

        // Find the union of all inputs grouped with those already in
        // the set.  Keep doing so until no new inputs get added.
        // This might not be the most efficient way, but it's simple
        // and more than fast enough for our needs.
        QSet<uint> fullset = mit.value();
        QSet<uint> checkset;
        QSet<uint>::const_iterator sit;
        while (checkset != fullset)
        {
            checkset = fullset;
            for (sit = checkset.begin(); sit != checkset.end(); ++sit)
                fullset += inputSets[*sit];
        }

        // Create a new conflict list for the resulting set of inputs
        // and point each inputs list at it.
        RecList *conflictlist = new RecList();
        conflictlists.push_back(conflictlist);
        for (sit = checkset.begin(); sit != checkset.end(); ++sit)
        {
            LOG(VB_SCHEDULE, LOG_INFO,
                QString("Assigning input %1 to conflict set %2")
                .arg(*sit).arg(conflictlists.size()));
            sinputinfomap[*sit].conflictlist = conflictlists.back();
        }
    }
}

void Scheduler::InitInputInfoMap(void)
{
    // Cache some input related info so we don't have to keep
    // rereading it from the database.
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT cardid, parentid, schedgroup "
                  "FROM capturecard "
                  "ORDER BY cardid");
    if (!query.exec())
    {
        MythDB::DBError("InitRecLimitMap", query);
        return;
    }

    while (query.next())
    {
        uint inputid = query.value(0).toUInt();
        SchedInputInfo &siinfo = sinputinfomap[inputid];
        siinfo.inputid = inputid;
        uint parentid = query.value(1).toUInt();
        if (parentid && sinputinfomap[parentid].schedgroup)
            siinfo.sgroupid = parentid;
        else
            siinfo.sgroupid = inputid;
        siinfo.schedgroup = query.value(2).toBool();
        if (!parentid && siinfo.schedgroup)
        {
            siinfo.group_inputs = CardUtil::GetChildInputIDs(inputid);
            siinfo.group_inputs.insert(siinfo.group_inputs.begin(), inputid);
        }
        siinfo.conflicting_inputs = CardUtil::GetConflictingInputs(inputid);
        LOG(VB_SCHEDULE, LOG_INFO,
            QString("Added SchedInputInfo i=%1, g=%2, sg=%3")
            .arg(inputid).arg(siinfo.sgroupid).arg(siinfo.schedgroup));
    }

    CreateConflictLists();
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
