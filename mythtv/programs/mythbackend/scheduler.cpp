#include <unistd.h>
#include <qsqldatabase.h>
#include <qsqlquery.h>
#include <qregexp.h>
#include <qstring.h>
#include <qdatetime.h>

#include <iostream>
#include <algorithm>
using namespace std;

#include <sys/stat.h>
#include <sys/vfs.h>

#include "scheduler.h"

#include "libmythtv/programinfo.h"
#include "libmythtv/scheduledrecording.h"
#include "encoderlink.h"
#include "libmyth/mythcontext.h"
#include "mainserver.h"
#include "remoteutil.h"

Scheduler::Scheduler(bool runthread, QMap<int, EncoderLink *> *tvList, 
                     QSqlDatabase *ldb)
{
    hasconflicts = false;
    db = ldb;
    m_tvList = tvList;

    m_mainServer = NULL;

    setupCards();

    threadrunning = runthread;

    recordingList_lock = new QMutex(true);
    scheduledList_lock = new QMutex(true);

    if (runthread)
    {
        pthread_t scthread;
        pthread_create(&scthread, NULL, SchedulerThread, this);
    }
}

Scheduler::~Scheduler()
{
    while (recordingList.size() > 0)
    {
        ProgramInfo *pginfo = recordingList.back();
        delete pginfo;
        recordingList.pop_back();
    }
}

void Scheduler::SetMainServer(MainServer *ms)
{
    m_mainServer = ms;
}

void Scheduler::setupCards(void)
{
    QString thequery;

    thequery = "SELECT NULL FROM capturecard;";

    QSqlQuery query = db->exec(thequery);

    numcards = -1;
    numinputs = -1;
    numsources = -1;
    numInputsPerSource.clear();
    sourceToInput.clear();
    inputToCard.clear();

    if (query.isActive())
        numcards = query.numRowsAffected();

    if (numcards <= 0)
    {
        cerr << "ERROR: no capture cards are defined in the database.\n";
        cerr << "Perhaps you should read the installation instructions?\n";
        exit(1);
    }

    thequery = "SELECT sourceid,name FROM videosource ORDER BY sourceid;";

    query = db->exec(thequery);

    numsources = -1;

    if (query.isActive())
    {
        numsources = query.numRowsAffected();

        int source = 0;

        while (query.next())
        {
            source = query.value(0).toInt();

            thequery = QString("SELECT cardinputid FROM cardinput WHERE "
                               "sourceid = %1 ORDER BY cardinputid;")
                              .arg(source);
            QSqlQuery subquery = db->exec(thequery);
            
            if (subquery.isActive() && subquery.numRowsAffected() > 0)
            {
                numInputsPerSource[source] = subquery.numRowsAffected();
 
                while (subquery.next())
                    sourceToInput[source].push_back(subquery.value(0).toInt());
            }
            else
            {
                numInputsPerSource[source] = -1;
                cerr << query.value(1).toString() << " is defined, but isn't "
                     << "attached to a cardinput.\n";
            }
        }
    }

    if (numsources <= 0)
    {
        cerr << "ERROR: No channel sources defined in the database\n";
        exit(0);
    }

    thequery = "SELECT cardid,cardinputid FROM cardinput ORDER BY cardinputid;";

    query = db->exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        numinputs = query.numRowsAffected();

        while (query.next())
        {
            inputToCard[query.value(1).toInt()] = query.value(0).toInt();
        }  
    }
}

bool Scheduler::CheckForChanges(void)
{
    return ScheduledRecording::hasChanged(db);
}

static inline bool Recording(ProgramInfo *p)
{
    return (p->recstatus == rsRecording || p->recstatus == rsWillRecord);
}

class comp_proginfo 
{
  public:
    bool operator()(ProgramInfo *a, ProgramInfo *b)
    {
        return (a->recstartts < b->recstartts);
    }
};

void Scheduler::FillEncoderFreeSpaceCache()
{
    QMap<int, EncoderLink *>::Iterator enciter = m_tvList->begin();
    for (; enciter != m_tvList->end(); ++enciter)
    {
        EncoderLink *enc = enciter.data();
        enc->cacheFreeSpace();
    }
}

bool Scheduler::FillRecordLists(bool doautoconflicts)
{
    doRecPriority = (bool)gContext->GetNumSetting("RecPriorityActive");
    doRecPriorityFirst = (bool)gContext->GetNumSetting("RecPriorityOrder");

    QMutexLocker lockit(recordingList_lock);

    QDateTime now = QDateTime::currentDateTime();

    PruneRecordList(now);

    AddFuturePrograms(now);

    if (recordingList.size() > 0)
    {
        recordingList.sort(comp_proginfo());
        PruneOverlaps();
        MarkKnownInputs();
        MarkConflicts();
        PruneList(now);
        MarkConflicts();

        if (numcards > 1 || numinputs > 1)
        {
            DoMultiCard();
            MarkConflicts();
        }

        MarkConflictsToRemove();
        if (doautoconflicts)
            GuessConflicts();
        MarkConflicts();
    }

    if ((print_verbose_messages & VB_SCHEDULE) != 0)
        PrintList();

    return hasconflicts;
}

void Scheduler::FillRecordListFromMaster(void)
{
    vector<ProgramInfo *> reclist;

    RemoteGetAllPendingRecordings(reclist);

    vector<ProgramInfo *>::iterator pgiter = reclist.begin();

    QMutexLocker lockit(recordingList_lock);

    for (; pgiter != reclist.end(); pgiter++)
    {
        ProgramInfo *pginfo = (*pgiter);
        pginfo->schedulerid = pginfo->startts.toString() + "_" + pginfo->chanid;

        recordingList.push_back(*pgiter);
    }
}

void Scheduler::PrintList(bool onlyFutureRecordings)
{
    QDateTime now = QDateTime::currentDateTime();
    QString episode;

    cout << "--- print list start ---\n";
    list<ProgramInfo *>::iterator i = recordingList.begin();
    cout << "Title - Subtitle                  Chan ChID Day Start  End   "
        "S I C  C R O N Pri" << endl;

    for (; i != recordingList.end(); i++)
    {
        ProgramInfo *first = (*i);

        if (onlyFutureRecordings &&
            (first->recendts < now ||
             (first->recstartts < now && !Recording(first))))
            continue;

        if (first->subtitle > " ")
            episode = QString("%1 - \"%2\"").arg(first->title.local8Bit())
                                            .arg(first->subtitle.local8Bit());
        else
            episode = first->title.local8Bit();

        cout << episode.leftJustify(33, ' ', true) << " "
             << first->chanstr.rightJustify(4, ' ') << " " << first->chanid 
             << first->recstartts.toString("  dd hh:mm-").local8Bit()
             << first->recendts.toString("hh:mm  ").local8Bit()
             << first->sourceid 
             << " " << first->inputid << " " << first->cardid << "  "  
             << first->conflicting << " " << first->recording << " "
             << first->override << " " << first->RecStatusChar() << " "
             << QString::number(first->recpriority).rightJustify(3, ' ')
             << endl;
    }

    cout << "---  print list end  ---\n";
}

void Scheduler::UpdateRecStatus(ProgramInfo *pginfo)
{
    QMutexLocker lockit(recordingList_lock);

    list<ProgramInfo *>::iterator dreciter = recordingList.begin();
    for (; dreciter != recordingList.end(); ++dreciter)
    {
        ProgramInfo *p = *dreciter;
        if (p->IsSameProgramTimeslot(*pginfo))
        {
            p->recstatus = pginfo->recstatus;
            return;
        }
    }
}

void Scheduler::PruneRecordList(const QDateTime &now)
{
    QDateTime deltime = now.addDays(-1);

    list<ProgramInfo *>::iterator dreciter = recordingList.begin();
    while (dreciter != recordingList.end())
    {
        ProgramInfo *p = *dreciter;
        if (p->recendts < deltime ||
            (p->recstatus >= rsWillRecord && p->recstatus != rsCancelled &&
             p->recstatus != rsLowDiskSpace && p->recstatus != rsTunerBusy))
        {
            delete p;
            dreciter = recordingList.erase(dreciter);
        }
        else
        {
            if (p->recstatus == rsRecording && p->recendts < now)
                p->recstatus = rsRecorded;
            dreciter++;
        }
    }
}

bool Scheduler::Conflict(ProgramInfo *a, ProgramInfo *b)
{
    if (a->cardid > 0 && b->cardid > 0)
    {
        if (a->cardid != b->cardid)
            return false;
    }

    return (a->recstartts < b->recendts && a->recendts > b->recstartts);
}

void Scheduler::MarkKnownInputs(void)
{
    list<ProgramInfo *>::iterator i = recordingList.begin();
    for (; i != recordingList.end(); i++)
    {
        ProgramInfo *first = (*i);
        if (first->inputid <= 0 && first->recstatus == rsWillRecord)
        {
            if (numInputsPerSource[first->sourceid] == 1)
            {
                first->inputid = sourceToInput[first->sourceid][0];
                first->cardid = inputToCard[first->inputid];
            }
        }
    }
}
 
void Scheduler::MarkConflicts(list<ProgramInfo *> *uselist)
{
    list<ProgramInfo *> *curList = &recordingList;
    if (uselist)
        curList = uselist;

    hasconflicts = false;
    list<ProgramInfo *>::iterator i = curList->begin();
    for (; i != curList->end(); i++)
    {
        ProgramInfo *first = (*i);
        first->conflicting = false;
    }

    for (i = curList->begin(); i != curList->end(); i++)
    {
        list<ProgramInfo *>::iterator j = i;
        j++;
        for (; j != curList->end(); j++)
        {
            ProgramInfo *first = (*i);
            ProgramInfo *second = (*j);

            if (!Recording(first) || !Recording(second))
                continue;

            if (Conflict(first, second))
            {
                first->conflicting = true;
                second->conflicting = true;
                hasconflicts = true;
            }
        }
    }
}

void Scheduler::PruneOverlaps(void)
{
    list<ProgramInfo *>::iterator i;
    for (i = recordingList.begin(); i != recordingList.end(); i++)
    {
        ProgramInfo *first = (*i);
        if (first->recstatus == rsOverlap)
            continue;

        list<ProgramInfo *>::iterator j = i;
        for (j++; j != recordingList.end(); j++)
        {
            ProgramInfo *second = (*j);
            if (second->recstatus == rsOverlap)
                continue;

            if (first->IsSameTimeslot(*second))
            {
                int pfirst = RecTypePriority(first->rectype);
                int psecond = RecTypePriority(second->rectype);
                if (pfirst < psecond || 
                    (pfirst == psecond && first->recordid < second->recordid))
                {
                    second->recording = false;
                    second->recstatus = rsOverlap;
                }
                else
                {
                    first->recording = false;
                    first->recstatus = rsOverlap;
                    break;
                }
            }
        }
    }    
}

void Scheduler::PruneList(const QDateTime &now)
{
    list<ProgramInfo *>::reverse_iterator i;
    list<ProgramInfo *>::iterator q;

    q = recordingList.begin();
    for ( ; q != recordingList.end(); q++)
    {
        ProgramInfo *rec = (*q);

        if (rec->recstatus == rsWillRecord &&
            !rec->AllowRecordingNewEpisodes(db))
        {
            rec->recording = false;
            rec->recstatus = rsTooManyRecordings;
        }
    }

    q = recordingList.begin();
    for ( ; q != recordingList.end(); q++)
    {
        ProgramInfo *rec = (*q);

        if (rec->recstartts > now)
            break;

        QMap<int, EncoderLink *>::Iterator enciter = m_tvList->begin();
        for (; enciter != m_tvList->end(); ++enciter)
        {
            EncoderLink *elink = enciter.data();
            if (elink->isRecording(rec))
            {
                rec->recording = true;
                rec->recstatus = rsRecording;
                break;
            }
        }
    }

    for (i = recordingList.rbegin(); i != recordingList.rend(); i++)
    {
        ProgramInfo *first = (*i);

        if (!Recording(first) || first->rectype == kSingleRecord ||
            (!first->rectype == kFindOneRecord &&
            first->subtitle.length() <= 2 && first->description.length() <= 2))
            continue;

        list<ProgramInfo *>::reverse_iterator j = i;

        for (j++; j != recordingList.rend(); j++)
        {
            ProgramInfo *second = (*j);

            if (!Recording(second))
                continue;

            if (first->IsSameProgram(*second))
            {
                if (((second->conflicting && !first->conflicting) ||
                     second->recstartts < now.addSecs(-15) || first->override == 1) 
                    && second->override != 1 
                    && ~second->dupmethod & kDupCheckNone
                    && second->recstatus != rsRecording)
                {
                    second->recording = false;
                    second->recstatus = rsOtherShowing;
                }
                else if (first->override != 1
                         && ~first->dupmethod & kDupCheckNone
                         && first->recstatus != rsRecording)
                {
                    first->recording = false;
                    first->recstatus = rsOtherShowing;
                }
            }
        }
    }    
}

void Scheduler::getAllPending(list<ProgramInfo *> *retList)
{
    QMutexLocker lockit(recordingList_lock);

    while (retList->size() > 0)
    {
        ProgramInfo *pginfo = retList->back();
        delete pginfo;
        retList->pop_back();
    }

    QDateTime now = QDateTime::currentDateTime();

    list<ProgramInfo *>::iterator i = recordingList.begin();
    for (; i != recordingList.end(); i++)
    {
        ProgramInfo *p = *i;
        if (p->recstatus == rsRecording && p->recendts < now)
            p->recstatus = rsRecorded;
        retList->push_back(new ProgramInfo(*p));
    }
}

void Scheduler::getAllPending(QStringList *strList)
{
    QMutexLocker lockit(recordingList_lock);

    (*strList) << QString::number(hasconflicts);
    (*strList) << QString::number(recordingList.size());

    QDateTime now = QDateTime::currentDateTime();

    list<ProgramInfo *>::iterator i = recordingList.begin();
    for (; i != recordingList.end(); i++)
    {
        ProgramInfo *p = *i;
        if (p->recstatus == rsRecording && p->recendts < now)
            p->recstatus = rsRecorded;
        p->ToStringList(*strList);
    }
}

list<ProgramInfo *> *Scheduler::getAllScheduled(void)
{
    while (scheduledList.size() > 0)
    {
        ProgramInfo *pginfo = scheduledList.back();
        delete pginfo;
        scheduledList.pop_back();
    }

    findAllScheduledPrograms(scheduledList);

    return &scheduledList;
}

void Scheduler::getAllScheduled(QStringList *strList)
{
    QMutexLocker lockit(scheduledList_lock);

    getAllScheduled();

    (*strList) << QString::number(scheduledList.size());

    list<ProgramInfo *>::iterator i = scheduledList.begin();
    for (; i != scheduledList.end(); i++)
        (*i)->ToStringList(*strList);
}

void Scheduler::getConflicting(ProgramInfo *pginfo,
                               bool removenonplaying,
                               QStringList *strlist)
{
    QMutexLocker lockit(recordingList_lock);

    list<ProgramInfo *> *curList = getConflicting(pginfo, removenonplaying);

    (*strlist) << QString::number(curList->size());

    list<ProgramInfo *>::iterator i = curList->begin();
    for (; i != curList->end(); i++)
        (*i)->ToStringList(*strlist);

    delete curList;
}

list<ProgramInfo *> *Scheduler::getConflicting(ProgramInfo *pginfo,
                                               bool removenonplaying,
                                               list<ProgramInfo *> *uselist)
{
    QMutexLocker lockit(recordingList_lock);

    if (!pginfo->conflicting && removenonplaying)
        return NULL;

    list<ProgramInfo *> *curList = &recordingList;
    if (uselist)
        curList = uselist;

    list<ProgramInfo *> *retlist = new list<ProgramInfo *>;

    list<ProgramInfo *>::iterator i = curList->begin();
    for (; i != curList->end(); i++)
    {
        ProgramInfo *second = (*i);

        if (second->title == pginfo->title && 
            second->startts == pginfo->startts &&
            second->chanid == pginfo->chanid)
            continue;

        if (removenonplaying && (!Recording(pginfo) || !Recording(second)))
            continue;
        if (Conflict(pginfo, second))
            retlist->push_back(second);
    }

    return retlist;
}

void Scheduler::CheckRecPriority(ProgramInfo *info,
                          list<ProgramInfo *> *conflictList)
{
    int resolved = 0;

    list<ProgramInfo *>::iterator i = conflictList->begin();
    for (; i != conflictList->end(); i++)
    {
        ProgramInfo *second = (*i);

        if (info->recpriority == second->recpriority)
            continue;

        if (info->recpriority > second->recpriority
            && second->recstatus != rsRecording)
        {
            second->recording = false;
            second->recstatus = rsLowerRecPriority;
            resolved++;
        }
    }

    if (resolved == (int)conflictList->size())
        info->conflicting = false;
}

void Scheduler::CheckOverride(ProgramInfo *info,
                              list<ProgramInfo *> *conflictList)
{
    int resolved = 0;

    QString thequery;

    QString starts = info->startts.toString("yyyyMMddhhmm");
    starts += "00";
    QString ends = info->endts.toString("yyyyMMddhhmm");
    ends += "00";

    thequery = QString("SELECT NULL FROM conflictresolutionoverride WHERE "
                       "chanid = %1 AND starttime = %2 AND "
                       "endtime = %3;").arg(info->chanid).arg(starts)
                       .arg(ends);

    QSqlQuery query = db->exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();
        
        list<ProgramInfo *>::iterator i = conflictList->begin();
        for (; i != conflictList->end(); i++)
        {
            ProgramInfo *del = (*i);

            if (del->recstatus == rsRecording)
                continue;

            del->recording = false;
            del->recstatus = rsManualConflict;
            resolved++;
        }

        if (resolved == (int)conflictList->size())
            info->conflicting = false;
    }
}

void Scheduler::MarkSingleConflict(ProgramInfo *info,
                                   list<ProgramInfo *> *conflictList)
{
    QString thequery;

    list<ProgramInfo *>::iterator i;

    QString starts = info->startts.toString("yyyyMMddhhmm");
    starts += "00";
    QString ends = info->endts.toString("yyyyMMddhhmm");
    ends += "00";
  
    thequery = QString("SELECT dislikechanid,dislikestarttime,dislikeendtime "
                       "FROM conflictresolutionsingle WHERE "
                       "preferchanid = %1 AND preferstarttime = %2 AND "
                       "preferendtime = %3;").arg(info->chanid)
                       .arg(starts).arg(ends);
 
    QSqlQuery query = db->exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        while (query.next())
        {
            QString badchannum = query.value(0).toString();
            QDateTime badst = QDateTime::fromString(query.value(1).toString(),
                                                    Qt::ISODate);
            QDateTime badend = QDateTime::fromString(query.value(2).toString(),
                                                     Qt::ISODate);

            i = conflictList->begin();
            for (; i != conflictList->end(); i++)
            {
                ProgramInfo *test = (*i);
                if (test->chanid == badchannum && test->startts == badst && 
                    test->endts == badend && test->recstatus != rsRecording)
                {
                    test->recording = false;
                    test->recstatus = rsManualConflict;
                }
            }
        }
    }

    bool conflictsleft = false;
    i = conflictList->begin();
    for (; i != conflictList->end(); i++)
    {
        ProgramInfo *test = (*i);
        if (Recording(test) == true)
            conflictsleft = true;
    }

    if (!conflictsleft)
    {
        info->conflicting = false;
        return;
    }

    thequery = QString("SELECT disliketitle FROM conflictresolutionany WHERE "
                       "prefertitle = \"%1\";").arg(info->title.utf8());

    query = db->exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        while (query.next())
        {
            QString badtitle = QString::fromUtf8(query.value(0).toString());

            i = conflictList->begin();
            for (; i != conflictList->end(); i++)
            {
                ProgramInfo *test = (*i);
                if (test->title == badtitle && test->recstatus != rsRecording)
                {
                    test->recording = false;
                    test->recstatus = rsManualConflict;
                }
            }
        }
    }
    
    conflictsleft = false;
    i = conflictList->begin();
    for (; i != conflictList->end(); i++)
    {
        ProgramInfo *test = (*i);
        if (Recording(test) == true)
            conflictsleft = true;
    }

    if (!conflictsleft)
        info->conflicting = false;
}

void Scheduler::MarkConflictsToRemove(void)
{
    list<ProgramInfo *>::iterator i;

    if (doRecPriority && doRecPriorityFirst) 
    {
        i = recordingList.begin();
        for (; i != recordingList.end(); i++)
        {
            ProgramInfo *first = (*i);

            if (first->conflicting && Recording(first) 
                && first->recstatus != rsRecording)
            {
                list<ProgramInfo *> *conflictList = getConflicting(first);
                CheckRecPriority(first, conflictList);
                delete conflictList;
            }
        }
    }

    i = recordingList.begin();
    for (; i != recordingList.end(); i++)
    {
        ProgramInfo *first = (*i);
    
        if (first->conflicting && Recording(first)
            && first->recstatus != rsRecording)
        {
            list<ProgramInfo *> *conflictList = getConflicting(first);
            CheckOverride(first, conflictList); 
            delete conflictList;
        }
    }

    i = recordingList.begin();
    for (; i != recordingList.end(); i++)
    {
        ProgramInfo *first = (*i);
  
        if (first->conflicting && Recording(first)
            && first->recstatus != rsRecording)
        {
            list<ProgramInfo *> *conflictList = getConflicting(first);
            MarkSingleConflict(first, conflictList);
            delete conflictList;
        }
        else if (!Recording(first))
            first->conflicting = false;
    }

    if (doRecPriority && !doRecPriorityFirst) 
    {
        i = recordingList.begin();
        for (; i != recordingList.end(); i++)
        {
            ProgramInfo *first = (*i);

            if (first->conflicting && Recording(first)
                && first->recstatus != rsRecording)
            {
                list<ProgramInfo *> *conflictList = getConflicting(first);
                CheckRecPriority(first, conflictList);
                delete conflictList;
            }
        }
    }
}

ProgramInfo *Scheduler::GetBest(ProgramInfo *info, 
                                list<ProgramInfo *> *conflictList)
{
    RecordingType type = info->GetProgramRecordingStatus(db);
    ProgramInfo *best = info;

    list<ProgramInfo *>::iterator i;
    for (i = conflictList->begin(); i != conflictList->end(); i++)
    {
        ProgramInfo *test = (*i);
        RecordingType testtype = test->GetProgramRecordingStatus(db);
        if (testtype < type || test->recstatus == rsRecording)
        {
            best = test;
            type = testtype;
            break;
        }
        else if (testtype == type)
        {
            if (test->recstartts < info->recstartts)
            {
                best = test;
                break;
            }
            if (test->recstartts.secsTo(test->recendts) >
                info->recstartts.secsTo(info->recendts))
            {
                best = test;
                break;
            }
            if (test->chanid.toInt() < info->chanid.toInt())
            {
                best = test;
                break;
            }
        }
    }

    return best;
}

void Scheduler::GuessSingle(ProgramInfo *info, 
                            list<ProgramInfo *> *conflictList)
{
    ProgramInfo *best = info;
    list<ProgramInfo *>::iterator i;
 
    if (conflictList->size() == 0)
    {
        info->conflicting = false;
        return;
    }

    best = GetBest(info, conflictList);

    if (best == info)
    {
        for (i = conflictList->begin(); i != conflictList->end(); i++)
        {
            ProgramInfo *pginfo = (*i);
            pginfo->recording = false;
            pginfo->recstatus = rsAutoConflict;
        }
        best->conflicting = false;
    }
    else
    {
        info->recording = false;
        info->recstatus = rsAutoConflict;
    } 
}

void Scheduler::GuessConflicts(void)
{
    list<ProgramInfo *>::iterator i = recordingList.begin();
    for (; i != recordingList.end(); i++)
    {
        ProgramInfo *first = (*i);
        if (Recording(first) && first->conflicting
            && first->recstatus != rsRecording)
        {
            list<ProgramInfo *> *conflictList = getConflicting(first, true);
            GuessSingle(first, conflictList);
            delete conflictList;
        }
    }
}

list<ProgramInfo *> *Scheduler::CopyList(list<ProgramInfo *> *sourcelist)
{
    list<ProgramInfo *> *retlist = new list<ProgramInfo *>;

    list<ProgramInfo *>::iterator i = sourcelist->begin();
    for (; i != sourcelist->end(); i++)
    {
        ProgramInfo *first = (*i);
        ProgramInfo *second = new ProgramInfo(*first);

        second->conflictfixed = false;

        if (second->cardid <= 0 && second->recstatus == rsWillRecord)
        {
            int numinputs = numInputsPerSource[first->sourceid];
            bool placed = false;

            for (int z = 0; z < numinputs; z++)
            {
                second->inputid = sourceToInput[first->sourceid][z];
                second->cardid = inputToCard[second->inputid];

                if (!m_tvList->contains(second->cardid))
                {
                    if (m_tvList->size())
                    {
                        cerr << "Missing: " << second->cardid << " in tvList\n";
                    }
                    continue;
                }

                EncoderLink *enc = (*m_tvList)[second->cardid];
                if (enc->WouldConflict(second))
                    continue;
                if (enc->isLowOnFreeSpace())
                    continue;
                if (enc->isTunerLocked())
                    continue;
                placed = true;
                break;
            }

            if (!placed)
            {
                if (numinputs > 0)
                {
                    second->inputid = sourceToInput[first->sourceid][0];
                    second->cardid = inputToCard[second->inputid];
                }
                else
                {
                    cerr << "Source: " << first->sourceid << " is not attached"
                         << " to an input on a capture card.\n";
                    cerr << "Unable to schedule:\n";
                    cerr << first->title.leftJustify(22, ' ', true)
                         << first->chanstr.rightJustify(4, ' ') << "  " 
                         << first->chanid 
                         << first->recstartts.toString("  MMM dd hh:mmap  ")
                         << endl;
                    cerr << " for recording.  It will be removed.";
                    delete second;
                    second = NULL;
                }
            }
        }

        if (second)
            retlist->push_back(second);
    }

    return retlist;
}
        
void Scheduler::DoMultiCard(void)
{
    bool highermove, lowermove;
    ProgramInfo *higher, *lower;

    list<ProgramInfo *> *copylist = CopyList(&recordingList);

    MarkConflicts(copylist);

    int numconflicts = 0;

    list<ProgramInfo *> allConflictList;
    list<bool> canMoveList;

    list<ProgramInfo *>::iterator i;
    for (i = copylist->begin(); i != copylist->end(); i++)
    {
        ProgramInfo *first = (*i);
        if (Recording(first) && first->conflicting)
        {
            numconflicts++;
            allConflictList.push_back(first);
            if (numInputsPerSource[first->sourceid] == 1
                || first->recstatus == rsRecording) 
                canMoveList.push_back(false);
            else
                canMoveList.push_back(true);
        }
    }

    list<ProgramInfo *> fixedList;
    
    list<bool>::iterator biter;
    for (biter = canMoveList.begin(), i = allConflictList.begin(); 
         biter != canMoveList.end(); biter++, i++)
    {
        ProgramInfo *first = (*i);

        list<ProgramInfo *> *conflictList = getConflicting(first, true, 
                                                           copylist);

        bool firstmove = *biter;

        list<ProgramInfo *>::iterator j = conflictList->begin();
        for (; j != conflictList->end(); j++)
        {
            ProgramInfo *second = (*j);

            bool secondmove = 
                (numInputsPerSource[second->sourceid] > 1 && 
                 second->recstatus != rsRecording);

            if (second->conflictfixed)
                secondmove = false;

            if (doRecPriority && 
                second->recpriority > first->recpriority)
            {
                highermove = secondmove;
                higher = second;
                lowermove = firstmove;
                lower = first;
            }
            else
            {
                highermove = firstmove;
                higher = first;
                lowermove = secondmove;
                lower = second;
            }

            bool fixed = false;
            if (lowermove)
            {
                int storeinput = lower->inputid;
                int numinputs = numInputsPerSource[lower->sourceid];

                for (int z = 0; z < numinputs; z++)
                {
                    lower->inputid = sourceToInput[lower->sourceid][z];
                    lower->cardid = inputToCard[lower->inputid];

                    if (!m_tvList->contains(lower->cardid))
                    {
                        if (m_tvList->size())
                        {
                            cerr << "Missing: " << lower->cardid <<
                                    " in tvList\n";
                        }
                        continue;
                    }

                    EncoderLink *this_encoder = (*m_tvList)[lower->cardid];
                    if ((this_encoder->WouldConflict(lower)) ||
                        (this_encoder->isLowOnFreeSpace()) ||
                        (this_encoder->isTunerLocked()))
                        continue;

                    if (!Conflict(higher, lower))
                    {
                        bool allclear = true;
                        list<ProgramInfo *>::iterator k = fixedList.begin();
                        for (; k != fixedList.end(); k++)
                        {
                            ProgramInfo *test = (*k);
                            if (Conflict(test, lower))
                            {
                                allclear = false;
                                break;
                            }
                        }

                        if (allclear)
                        {
                            fixed = true;
                            break;
                        }
                    }
                }
                if (!fixed)
                {
                    lower->inputid = storeinput;
                    lower->cardid = inputToCard[lower->inputid];
                }
            }

            if (!fixed && highermove)
            {
                int storeinput = higher->inputid;
                int numinputs = numInputsPerSource[higher->sourceid];

                for (int z = 0; z < numinputs; z++)
                {
                    higher->inputid = sourceToInput[higher->sourceid][z];
                    higher->cardid = inputToCard[higher->inputid];

                    if (!m_tvList->contains(higher->cardid))
                    {
                        if (m_tvList->size())
                        {
                            cerr << "Missing: " << higher->cardid <<
                                    " in tvList\n";
                        }
                        continue;
                    }

                    EncoderLink *this_encoder = (*m_tvList)[higher->cardid];
                    if ((this_encoder->WouldConflict(higher)) ||
                        (this_encoder->isLowOnFreeSpace()) ||
                        (this_encoder->isTunerLocked()))
                        continue;

                    if (!Conflict(higher, second))
                    {
                        bool allclear = true;
                        list<ProgramInfo *>::iterator k = fixedList.begin();
                        for (; k != fixedList.end(); k++)
                        {
                            ProgramInfo *test = (*k);
                            if (Conflict(test, second))
                            {
                                allclear = false;
                                break;
                            }
                        }

                        if (allclear)
                        {
                            fixed = true;
                            break;
                        }
                    }
                }
                if (!fixed)
                {
                    higher->inputid = storeinput;
                    higher->cardid = inputToCard[higher->inputid];
                }
            }
        }

        delete conflictList;
        conflictList = getConflicting(first, true, copylist);
        if (!conflictList || conflictList->size() == 0)
        {
            first->conflictfixed = true;
            fixedList.push_back(first);
        }

        delete conflictList;
    }


    for (i = recordingList.begin(); i != recordingList.end(); i++)
    {
        ProgramInfo *first = (*i);
        delete first;
    }

    recordingList.clear();

    for (i = copylist->begin(); i != copylist->end(); i++)
    {
        ProgramInfo *first = (*i);
        recordingList.push_back(first);
    }

    delete copylist;
}

void Scheduler::RunScheduler(void)
{
    int prerollseconds = 0;

    int secsleft;
    EncoderLink *nexttv = NULL;

    ProgramInfo *nextRecording = NULL;
    QDateTime nextrectime;

    QDateTime curtime;
    QDateTime lastupdate = QDateTime::currentDateTime().addDays(-1);

    QString recordfileprefix = gContext->GetFilePrefix();

    list<ProgramInfo *>::iterator startIter = recordingList.begin();

    bool blockShutdown = gContext->GetNumSetting("blockSDWUwithoutClient", 1);
    QDateTime idleSince = QDateTime();
    int idleTimeoutSecs = 0;
    int idleWaitForRecordingTime = 0;
    bool firstRun = true;

    // wait for slaves to connect
    sleep(2);

    while (1)
    {
        curtime = QDateTime::currentDateTime();

        if (CheckForChanges() ||
            (lastupdate.date().day() != curtime.date().day()))
        {
            VERBOSE(VB_GENERAL, "Found changes in the todo list.");
            FillEncoderFreeSpaceCache();
            FillRecordLists();
            lastupdate = curtime;
            startIter = recordingList.begin();

            MythEvent me("SCHEDULE_CHANGE");
            gContext->dispatch(me);

            // Determine if the user wants us to start recording early
            // and by how many seconds
            prerollseconds = gContext->GetNumSetting("RecordPreRoll");

            idleTimeoutSecs = gContext->GetNumSetting("idleTimeoutSecs", 0);
            idleWaitForRecordingTime =
                       gContext->GetNumSetting("idleWaitForRecordingTime", 15);

            // check once on startup, if a recording starts within the
            // idleWaitForRecordingTime. If no, block the shutdown,
            // because system seems to be waken up by the user and not by a
            // wakeup call
            if (firstRun && blockShutdown)
            {
                if (startIter != recordingList.end() &&
                    curtime.secsTo((*startIter)->startts) - prerollseconds
                    < idleWaitForRecordingTime * 60)
                    blockShutdown = false;
            }
            firstRun = false;
        }

        for ( ; startIter != recordingList.end(); startIter++)
            if ((*startIter)->recstatus == rsWillRecord)
                break;

        list<ProgramInfo *>::iterator recIter = startIter;
        for ( ; recIter != recordingList.end(); recIter++)
        {
            QString msg;

            nextRecording = *recIter;

            nextrectime = nextRecording->recstartts;
            secsleft = curtime.secsTo(nextrectime);

            if (secsleft - prerollseconds > 35)
                break;

            if (nextRecording->recstatus != rsWillRecord)
                continue;

            if (m_tvList->find(nextRecording->cardid) == m_tvList->end())
            {
                msg = QString("invalid cardid (%1) for %2")
                    .arg(nextRecording->cardid)
                    .arg(nextRecording->title);
                VERBOSE(VB_GENERAL, msg);

                QMutexLocker lockit(recordingList_lock);
                nextRecording->conflicting = false;
                nextRecording->recording = false;
                nextRecording->recstatus = rsTunerBusy;
                continue;
            }

            nexttv = (*m_tvList)[nextRecording->cardid];
            // cerr << "nexttv = " << nextRecording->cardid;
            // cerr << " title: " << nextRecording->title << endl;

            if (nexttv->isLowOnFreeSpace())
            {
                FillEncoderFreeSpaceCache();
                if (nexttv->isLowOnFreeSpace())
                {
                    msg = QString("SUPPRESSED recording '%1' on channel"
                                  " %2 on cardid %3, sourceid %4.  Only"
                                  " %5 Megs of disk space available.")
                        .arg(nextRecording->title.utf8())
                        .arg(nextRecording->chanid)
                        .arg(nextRecording->cardid)
                        .arg(nextRecording->sourceid)
                        .arg(nexttv->getFreeSpace());
                    VERBOSE(VB_GENERAL, msg);

                    QMutexLocker lockit(recordingList_lock);
                    nextRecording->conflicting = false;
                    nextRecording->recording = false;
                    nextRecording->recstatus = rsLowDiskSpace;
                    continue;
                }
            }

            if (nexttv->isTunerLocked())
            {
                msg = QString("SUPPRESSED recording \"%1\" on channel: "
                              "%2 on cardid: %3, sourceid %4. Tuner "
                              "is locked by an external application.")
                    .arg(nextRecording->title.utf8())
                    .arg(nextRecording->chanid)
                    .arg(nextRecording->cardid)
                    .arg(nextRecording->sourceid);
                VERBOSE(VB_GENERAL, msg);

                QMutexLocker lockit(recordingList_lock);
                nextRecording->conflicting = false;
                nextRecording->recording = false;
                nextRecording->recstatus = rsTunerBusy;
                continue;
            }

            if (!nexttv->IsBusyRecording())
            {
                // Will use pre-roll settings only if no other
                // program is currently being recorded
                secsleft -= prerollseconds;
            }

            //VERBOSE(VB_GENERAL, secsleft << " seconds until " << nextRecording->title);

            if (secsleft > 30)
                continue;

            if (secsleft > 2)
            {
                QString id = nextRecording->schedulerid;
                if (!recPendingList.contains(id))
                    recPendingList[id] = false;
                if (recPendingList[id] == false)
                {
                    nexttv->RecordPending(nextRecording, secsleft);
                    recPendingList[id] = true;
                }
            }

            if (secsleft > -2)
                continue;

            nextRecording->recstartts = 
                QDateTime::currentDateTime().addSecs(30);
            nextRecording->recstartts.setTime(QTime(
                nextRecording->recstartts.time().hour(),
                nextRecording->recstartts.time().minute()));

            QMutexLocker lockit(recordingList_lock);

            int retval = nexttv->StartRecording(nextRecording);
            if (retval > 0)
            {
                msg = "Started recording";
                nextRecording->conflicting = false;
                nextRecording->recstatus = rsRecording;
            }
            else
            {
                msg = "Canceled recording"; 
                nextRecording->conflicting = false;
                nextRecording->recording = false;
                if (retval < 0)
                    nextRecording->recstatus = rsTunerBusy;
                else
                    nextRecording->recstatus = rsCancelled;
            }

            msg += QString(" \"%1\" on channel: %2 on cardid: %3, "
                           "sourceid %4").arg(nextRecording->title.utf8())
                .arg(nextRecording->chanid)
                .arg(nextRecording->cardid)
                .arg(nextRecording->sourceid);
            VERBOSE(VB_GENERAL, msg);
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
                    if (it.data()->IsBusy())
                        recording = true;
                }
                
                if (!(m_mainServer->isClientConnected()) && !recording)
                {
                    if (!idleSince.isValid())
                    {
                        if ((recIter = recordingList.begin()) != 
                            recordingList.end())
                        {
                            if (curtime.secsTo((*recIter)->startts) - 
                                prerollseconds > idleWaitForRecordingTime * 60)
                            {
                                idleSince = curtime;
                            }
                        }
                        else
                            idleSince = curtime;
                    } 
                    else 
                    {
                        // is the machine already ideling the timeout time?
                        if (idleSince.addSecs(idleTimeoutSecs) < curtime)
                        {
                            CheckShutdownServer(prerollseconds, idleSince,
                                                blockShutdown);
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
                                VERBOSE(VB_ALL, msg);
                            }
                            else if (itime % 10 == 0)
                            {
                                msg = QString("%1 secs left to system "
                                              "shutdown!")
                                             .arg(idleTimeoutSecs - itime);
                                VERBOSE(VB_ALL, msg);
                            }
                        }
                    }
                }
                else
                    // not idle, make the time invalid
                    idleSince = QDateTime();
            }
        }

        sleep(1);
    }
} 

void Scheduler::CheckShutdownServer(int prerollseconds, QDateTime &idleSince, 
                                    bool &blockShutdown)
{
    bool done = false;
    QString preSDWUCheckCommand = gContext->GetSetting("preSDWUCheckCommand", 
                                                       "");
                 
    int state = 0;
    if (!preSDWUCheckCommand.isEmpty())
    {
        state = system(preSDWUCheckCommand.ascii());
                      
        if (WIFEXITED(state) && state != -1)
        {
            done = true;
            switch(WEXITSTATUS(state))
            {
                case 0:
                    ShutdownServer(prerollseconds);
                    break;
                case 1:
                    // just reset idle'ing on retval == 1
                    idleSince = QDateTime();
                    break;
                case 2:
                    // reset shutdown status on retval = 2
                    // (needs a clientconnection again,
                    // before shutdown is executed)
                    blockShutdown
                             = gContext->GetNumSetting("blockSDWUwithoutClient",
                                                       1);
                    idleSince = QDateTime();
                    break;
                // case 3:
                //    //disable shutdown routine generally
                //    m_noAutoShutdown = true;
                //    break;
                default:
                    done = false;
            }
        }
    }
                            
    if (!done)
        ShutdownServer(prerollseconds);
}

void Scheduler::ShutdownServer(int prerollseconds)
{    
    list<ProgramInfo *>::iterator recIter = recordingList.begin();

    // set the wakeuptime if needed
    if (recIter != recordingList.end())
    {
        ProgramInfo *nextRecording = (*recIter);
        QDateTime restarttime = nextRecording->startts.addSecs((-1) * 
                                                               prerollseconds);

        int add = gContext->GetNumSetting("StartupSecsBeforeRecording", 240);
        if (add)
            restarttime = restarttime.addSecs((-1) * add);

        QString wakeup_timeformat = gContext->GetSetting("WakeupTimeFormat",
                                                         "hh:mm yyyy-MM-dd");
        QString setwakeup_cmd = gContext->GetSetting("SetWakeuptimeCommand",
                                                     "echo \'Wakeuptime would "
                                                     "be $time if command "
                                                     "set.\'");

        if (wakeup_timeformat == "time_t")
        {
            QString time_ts;
            setwakeup_cmd.replace("$time", 
                                  time_ts.setNum(restarttime.toTime_t()));
        }
        else
            setwakeup_cmd.replace("$time", 
                                  restarttime.toString(wakeup_timeformat));

        // now run the command to set the wakeup time
        if (!setwakeup_cmd.isEmpty())
            system(setwakeup_cmd.ascii());
    }

    QString halt_cmd = gContext->GetSetting("ServerHaltCommand",
                                            "sudo /sbin/halt -p");

    if (!halt_cmd.isEmpty())
    {
        // now we shut the slave backends down...
        m_mainServer->ShutSlaveBackendsDown(halt_cmd);

        // and now shutdown myself
        system(halt_cmd.ascii());
    }
}

void *Scheduler::SchedulerThread(void *param)
{
    Scheduler *sched = (Scheduler *)param;
    sched->RunScheduler();
 
    return NULL;
}

void Scheduler::AddFuturePrograms(const QDateTime &now) {
    QMap<RecordingType, int> recTypeRecPriorityMap;
    list<ProgramInfo *> tmpList;

    QString query = QString(
"SELECT DISTINCT channel.chanid, channel.sourceid, "
"program.starttime, program.endtime, "
"program.title, program.subtitle, program.description, "
"channel.channum, channel.callsign, channel.name, "
"oldrecorded.starttime IS NOT NULL AS oldrecduplicate, program.category, "
"record.recpriority + channel.recpriority, record.dupin, "
"recorded.starttime IS NOT NULL as recduplicate, record.type, "
"record.recordid, recordoverride.type, "
"program.starttime - INTERVAL record.startoffset minute, "
"program.endtime + INTERVAL record.endoffset minute, "
"program.previouslyshown, record.recgroup, record.dupmethod "
"FROM record "
" INNER JOIN channel ON (channel.chanid = program.chanid) "
" INNER JOIN program ON (program.title = record.title) "
" LEFT JOIN oldrecorded ON "
"  ( "
"    oldrecorded.title IS NOT NULL AND oldrecorded.title <> '' AND program.title = oldrecorded.title "
"     AND "
"    (((record.dupmethod & 0x02) = 0) OR (oldrecorded.subtitle IS NOT NULL AND (oldrecorded.subtitle <> '' OR record.dupmethod & 0x10) AND program.subtitle = oldrecorded.subtitle)) "
"     AND "
"    (((record.dupmethod & 0x04) = 0) OR (oldrecorded.description IS NOT NULL AND (oldrecorded.description <> '' OR record.dupmethod & 0x10) AND program.description = oldrecorded.description)) "
"  ) "
" LEFT JOIN recorded ON "
"  ( "
"    recorded.title IS NOT NULL AND recorded.title <> '' AND program.title = recorded.title "
"     AND "
"    (((record.dupmethod & 0x02) = 0) OR (recorded.subtitle IS NOT NULL AND (recorded.subtitle <> '' OR record.dupmethod & 0x10) AND program.subtitle = recorded.subtitle)) "
"     AND "
"    (((record.dupmethod & 0x04) = 0) OR (recorded.description IS NOT NULL AND (recorded.description <> '' OR record.dupmethod & 0x10) AND program.description = recorded.description)) "
"  ) "
" LEFT JOIN recordoverride ON "
"  ( "
"    record.recordid = recordoverride.recordid "
"    AND program.chanid = recordoverride.chanid "
"    AND program.starttime = recordoverride.starttime "
"    AND program.endtime = recordoverride.endtime "
"    AND program.title = recordoverride.title "
"    AND program.subtitle = recordoverride.subtitle "
"    AND program.description = recordoverride.description "
"  ) "
"WHERE "
"((record.type = %1 " // allrecord
"OR record.type = %2) " // findonerecord
" OR "
" ((record.chanid = program.chanid) " // channel matches
"  AND "
"  ((record.type = %3) " // channelrecord
"   OR"
"   ((TIME_TO_SEC(record.starttime) = TIME_TO_SEC(program.starttime)) " // timeslot matches
"    AND "
"    ((record.type = %4) " // timeslotrecord
"     OR"
"     ((DAYOFWEEK(record.startdate) = DAYOFWEEK(program.starttime) "
"      AND "
"      ((record.type = %5) " // weekslotrecord
"       OR"
"       ((TO_DAYS(record.startdate) = TO_DAYS(program.starttime)) " // date matches
"        AND "
"        (TIME_TO_SEC(record.endtime) = TIME_TO_SEC(program.endtime)) "
"        AND "
"        (TO_DAYS(record.enddate) = TO_DAYS(program.endtime)) "
"        )"
"       )"
"      )"
"     )"
"    )"
"   )"
"  )"
" )"
");")
        .arg(kAllRecord)
        .arg(kFindOneRecord)
        .arg(kChannelRecord)
        .arg(kTimeslotRecord)
        .arg(kWeekslotRecord);

    QSqlQuery result = db->exec(query);

    if (!result.isActive())
    {
        MythContext::DBError("AddFuturePrograms", result);
        return;
    }

    while (result.next())
    {
        ProgramInfo *proginfo = new ProgramInfo;
        proginfo->recording = true;
        proginfo->recstatus = rsWillRecord;
        proginfo->chanid = result.value(0).toString();
        proginfo->sourceid = result.value(1).toInt();
        proginfo->startts = result.value(2).toDateTime();
        proginfo->endts = result.value(3).toDateTime();
        proginfo->title = QString::fromUtf8(result.value(4).toString());
        proginfo->subtitle = QString::fromUtf8(result.value(5).toString());
        proginfo->description = QString::fromUtf8(result.value(6).toString());
        proginfo->chanstr = result.value(7).toString();
        proginfo->chansign = result.value(8).toString();
        proginfo->channame = QString::fromUtf8(result.value(9).toString());
        proginfo->category = QString::fromUtf8(result.value(11).toString());
        proginfo->recpriority = result.value(12).toInt();
        proginfo->dupin = RecordingDupInType(result.value(13).toInt());
        proginfo->dupmethod =
            RecordingDupMethodType(result.value(22).toInt());
        proginfo->rectype = RecordingType(result.value(15).toInt());
        proginfo->recordid = result.value(16).toInt();
        proginfo->override = result.value(17).toInt();

        proginfo->recstartts = result.value(18).toDateTime();
        proginfo->recendts = result.value(19).toDateTime();

        if (!recTypeRecPriorityMap.contains(proginfo->rectype))
            recTypeRecPriorityMap[proginfo->rectype] = 
                proginfo->GetRecordingTypeRecPriority(proginfo->rectype);
        proginfo->recpriority += recTypeRecPriorityMap[proginfo->rectype];

        if (proginfo->recstartts >= proginfo->recendts)
        {
            // pre/post-roll are invalid so ignore
            proginfo->recstartts = proginfo->startts;
            proginfo->recendts = proginfo->endts;
        }

        proginfo->repeat = result.value(20).toInt();
        proginfo->recgroup = result.value(21).toString();

        if (proginfo->override == 2)
        {
            proginfo->recording = false;
            proginfo->recstatus = rsManualOverride;
        }
        else if ((proginfo->rectype != kSingleRecord) &&
                 (proginfo->override != 1) &&
                 (~proginfo->dupmethod & kDupCheckNone))
        {
            if (proginfo->dupin & kDupsInOldRecorded)
            {
                if (result.value(10).toInt())
                {
                    proginfo->recording = false;
                    proginfo->recstatus = rsPreviousRecording;
                }
            }
            else if (proginfo->dupin & kDupsInRecorded)
            {
                if (result.value(14).toInt())
                {
                    proginfo->recording = false;
                    proginfo->recstatus = rsCurrentRecording;
                }
            }
        }

        proginfo->schedulerid = 
            proginfo->startts.toString() + "_" + proginfo->chanid;

        // would save many queries to create and populate a
        // ScheduledRecording and put it in the proginfo at the
        // same time, since it will be loaded later anyway with
        // multiple queries

        if (proginfo->title == QString::null)
            proginfo->title = "";
        if (proginfo->subtitle == QString::null)
            proginfo->subtitle = "";
        if (proginfo->description == QString::null)
            proginfo->description = "";
        if (proginfo->category == QString::null)
            proginfo->category = "";

        if (proginfo->recendts < now)
            delete proginfo;
        else
        {
            list<ProgramInfo *>::iterator rec = recordingList.begin();
            for ( ; rec != recordingList.end(); rec++)
            {
                ProgramInfo *r = *rec;
                if (proginfo->IsSameProgramTimeslot(*r))
                    break;
            }
            if (rec == recordingList.end())
                tmpList.push_back(proginfo);
            else
                delete proginfo;
        }
    }

    list<ProgramInfo *>::iterator tmp = tmpList.begin();
    for ( ; tmp != tmpList.end(); tmp++)
        recordingList.push_back(*tmp);
}

void Scheduler::findAllScheduledPrograms(list<ProgramInfo*>& proglist)
{
    QString temptime, tempdate;
    QString query = QString("SELECT record.chanid, record.starttime, "
"record.startdate, record.endtime, record.enddate, record.title, "
"record.subtitle, record.description, record.recpriority, record.type, "
"channel.name, record.recordid, record.recgroup, record.dupin, "
"record.dupmethod FROM record "
"LEFT JOIN channel ON (channel.chanid = record.chanid) "
"ORDER BY title ASC;");

    QSqlQuery result = db->exec(query);

    if (result.isActive() && result.numRowsAffected() > 0)
        while (result.next()) 
        {
            ProgramInfo *proginfo = new ProgramInfo;
            proginfo->chanid = result.value(0).toString();
            proginfo->rectype = RecordingType(result.value(9).toInt());
            proginfo->recordid = result.value(11).toInt();

            if (proginfo->rectype == kSingleRecord || 
                proginfo->rectype == kTimeslotRecord ||
                proginfo->rectype == kWeekslotRecord) 
            {
                temptime = result.value(1).toString();
                tempdate = result.value(2).toString();
                proginfo->startts = QDateTime::fromString(tempdate+":"+temptime,
                                                          Qt::ISODate);
                temptime = result.value(3).toString();
                tempdate = result.value(4).toString();
                proginfo->endts = QDateTime::fromString(tempdate+":"+temptime,
                                                        Qt::ISODate);
            }
            else 
            {
                // put currentDateTime() in time fields to prevent
                // Invalid date/time warnings later
                proginfo->startts = QDateTime::currentDateTime();
                proginfo->startts.setTime(QTime(0,0));
                proginfo->endts = QDateTime::currentDateTime();
                proginfo->endts.setTime(QTime(0,0));
            }

            proginfo->title = QString::fromUtf8(result.value(5).toString());

            if (proginfo->rectype == kSingleRecord)
                proginfo->subtitle =
                    QString::fromUtf8(result.value(6).toString());

            proginfo->description =
                QString::fromUtf8(result.value(7).toString());
            proginfo->recpriority = result.value(8).toInt();
            proginfo->channame = QString::fromUtf8(result.value(10).toString());
            proginfo->recgroup = result.value(12).toString();
            proginfo->dupin = RecordingDupInType(result.value(13).toInt());
            proginfo->dupmethod =
                RecordingDupMethodType(result.value(14).toInt());

            proginfo->recstartts = proginfo->startts;
            proginfo->recendts = proginfo->endts;

            if (proginfo->title == QString::null)
                proginfo->title = "";
            if (proginfo->subtitle == QString::null)
                proginfo->subtitle = "";
            if (proginfo->description == QString::null)
                proginfo->description = "";

            proglist.push_back(proginfo);
        }
}

