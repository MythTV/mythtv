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

    if (recpriorityMap.size() > 0)
        recpriorityMap.clear();
    if (channelRecPriorityMap.size() > 0)
        channelRecPriorityMap.clear();
    if (recTypeRecPriorityMap.size() > 0)
        recTypeRecPriorityMap.clear();

    QMutexLocker lockit(recordingList_lock);

    while (recordingList.size() > 0)
    {
        ProgramInfo *pginfo = recordingList.back();
        delete pginfo;
        recordingList.pop_back();
    }

    PruneDontRecords();

    ScheduledRecording::findAllProgramsToRecord(db, recordingList);

    QMap<QString, bool> foundlist;

    list<ProgramInfo *>::iterator iter = recordingList.begin();
    for (; iter != recordingList.end(); iter++)
    {
        ProgramInfo *pginfo = (*iter);
        pginfo->schedulerid = pginfo->startts.toString() + "_" + pginfo->chanid;

        foundlist[pginfo->schedulerid] = true;
    }

    if (recordingList.size() > 0)
    {
        recordingList.sort(comp_proginfo());
        MarkKnownInputs();
        PruneOverlaps();
        MarkConflicts();
        PruneList();
        MarkConflicts();

        if (numcards > 1 || numinputs > 1)
        {
            DoMultiCard();
            MarkConflicts();
        }

        MarkConflictsToRemove();
        if (doautoconflicts)
        {
            //RemoveConflicts();
            GuessConflicts();
            //RemoveConflicts();
        }
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

void Scheduler::PrintList(void)
{
    QString totrecpriority;

    cout << "--- print list start ---\n";
    list<ProgramInfo *>::iterator i = recordingList.begin();
    cout << "Title                 Chan  ChID  StartTime       S I C "
            " C R O N Priority Total" << endl;
    for (; i != recordingList.end(); i++)
    {
        ProgramInfo *first = (*i);

        totrecpriority = QString::number(totalRecPriority(first));

        cout << first->title.local8Bit().leftJustify(22, ' ', true)
             << first->chanstr.rightJustify(4, ' ') << "  " << first->chanid 
             << first->recstartts.toString("  MMM dd hh:mmap  ")
             << first->sourceid 
             << " " << first->inputid << " " << first->cardid << "  "  
             << first->conflicting << " " << first->recording << " "
             << first->override << " " << first->RecStatusChar() << " "
             << first->recpriority.rightJustify(8, ' ') << " "
             << totrecpriority.rightJustify(4, ' ')
             << endl;
    }

    cout << "---  print list end  ---\n";
}

void Scheduler::AddToDontRecord(ProgramInfo *pginfo)
{
    QMutexLocker lockit(recordingList_lock);

    QValueList<ProgramInfo>::Iterator dreciter = dontRecordList.begin();
    for (; dreciter != dontRecordList.end(); ++dreciter)
    {
        if ((*dreciter).IsSameProgramTimeslot(*pginfo))
        {
            (*dreciter).recstatus = pginfo->recstatus;
            return;
        }
    }

    dontRecordList.append(*pginfo);
}

void Scheduler::PruneDontRecords(void)
{
    QDateTime deltime = QDateTime::currentDateTime().addDays(-1);

    QValueList<ProgramInfo>::Iterator dreciter = dontRecordList.begin();
    while (dreciter != dontRecordList.end())
    {
        if ((*dreciter).recendts < deltime)
            dreciter = dontRecordList.remove(dreciter);
        else
            dreciter++;
    }
}

void Scheduler::RemoveRecording(ProgramInfo *pginfo)
{
    recPendingList.remove(pginfo->schedulerid);

    QMutexLocker lockit(recordingList_lock);

    list<ProgramInfo *>::iterator i = recordingList.begin();
    for (; i != recordingList.end(); i++)
    {
        ProgramInfo *rec = (*i);
        if (rec == pginfo)
        {
            recordingList.erase(i);
            break;
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
        if (first->inputid == -1)
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

            if (!first->recording || !second->recording)
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

void Scheduler::PruneList(void)
{
    list<ProgramInfo *>::reverse_iterator i;
    list<ProgramInfo *>::iterator q;

    QDateTime now = QDateTime::currentDateTime();

    q = recordingList.begin();
    while (q != recordingList.end())
    {
        ProgramInfo *rec = (*q);

        if (!rec->AllowRecordingNewEpisodes(db))
        {
            rec->recording = false;
            rec->recstatus = rsTooManyRecordings;
        }

        q++;
    }

    q = recordingList.begin();
    while (q != recordingList.end())
    {
        ProgramInfo *rec = (*q);

        if (rec->recstartts > now)
            break;

        // check the "don't record" list (deleted programs, already started
        // conflicting programs, etc.

        QValueList<ProgramInfo>::Iterator dreciter = dontRecordList.begin();
        for (; dreciter != dontRecordList.end(); ++dreciter)
        {
            if ((*dreciter).IsSameProgramTimeslot(*rec))
            {
                delete rec;
                rec = NULL;
                recordingList.erase(q++);
                break;
            }
        }
        if (rec == NULL)
            continue;

        QMap<int, EncoderLink *>::Iterator enciter = m_tvList->begin();
        for (; enciter != m_tvList->end(); ++enciter)
        {
            EncoderLink *elink = enciter.data();
            if (elink->isRecording(rec))
            {
                rec->conflicting = false;
                rec->recording = true;
                rec->recstatus = rsRecording;
                AddToDontRecord(rec);
                delete rec;
                rec = NULL;
                recordingList.erase(q++);
                break;
            }
        }
        if (rec == NULL)
            continue;

        q++;
    }

    for (i = recordingList.rbegin(); i != recordingList.rend(); i++)
    {
        ProgramInfo *first = (*i);

        if (!first->recording || first->rectype == kSingleRecord ||
            first->subtitle.length() <= 2 && first->description.length() <= 2)
            continue;

        list<ProgramInfo *>::reverse_iterator j = i;

        for (j++; j != recordingList.rend(); j++)
        {
            ProgramInfo *second = (*j);

            if (!second->recording)
                continue;

            if (first->IsSameProgram(*second))
            {
                if (((second->conflicting && !first->conflicting) ||
                     second->recstartts < now.addSecs(-15) || first->override == 1) 
                    && second->override != 1 
                    && second->recdups != kRecordDupsAlways)
                {
                    second->recording = false;
                    second->recstatus = rsOtherShowing;
                }
                else if (first->override != 1
                         && first->recdups != kRecordDupsAlways)
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

    QValueList<ProgramInfo>::Iterator dreciter = dontRecordList.begin();
    for (; dreciter != dontRecordList.end(); ++dreciter)
    {
        if ((*dreciter).recstatus == rsRecording && (*dreciter).recendts < now)
            (*dreciter).recstatus = rsRecorded;
        ProgramInfo *newInfo = new ProgramInfo(*dreciter);
        retList->push_back(newInfo);
    }

    list<ProgramInfo *>::iterator i = recordingList.begin();
    for (; i != recordingList.end(); i++)
    {
        ProgramInfo *newInfo = new ProgramInfo(*(*i));
        retList->push_back(newInfo);
    }
}

void Scheduler::getAllPending(QStringList *strList)
{
    QMutexLocker lockit(recordingList_lock);

    (*strList) << QString::number(hasconflicts);
    (*strList) << QString::number(recordingList.size() + dontRecordList.size());

    QDateTime now = QDateTime::currentDateTime();

    QValueList<ProgramInfo>::Iterator dreciter = dontRecordList.begin();
    for (; dreciter != dontRecordList.end(); ++dreciter)
    {
        if ((*dreciter).recstatus == rsRecording && (*dreciter).recendts < now)
            (*dreciter).recstatus = rsRecorded;
        (*dreciter).ToStringList(*strList);
    }

    list<ProgramInfo *>::iterator i = recordingList.begin();
    for (; i != recordingList.end(); i++)
        (*i)->ToStringList(*strList);
}

list<ProgramInfo *> *Scheduler::getAllScheduled(void)
{
    while (scheduledList.size() > 0)
    {
        ProgramInfo *pginfo = scheduledList.back();
        delete pginfo;
        scheduledList.pop_back();
    }

    ScheduledRecording::findAllScheduledPrograms(db, scheduledList);

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

        if (removenonplaying && (!pginfo->recording || !second->recording))
            continue;
        if (Conflict(pginfo, second))
            retlist->push_back(second);
    }

    return retlist;
}

int Scheduler::totalRecPriority(ProgramInfo *info)
{
    int recpriority = 0;
    RecordingType rectype;

    if (recpriorityMap.contains(info->schedulerid))
        return recpriorityMap[info->schedulerid];

    if (!channelRecPriorityMap.contains(info->chanid))
    {
        channelRecPriorityMap[info->chanid] = 
                                 info->GetChannelRecPriority(db, info->chanid);
    }
    rectype = info->GetProgramRecordingStatus(db);
    if (!recTypeRecPriorityMap.contains(rectype))
    {
        recTypeRecPriorityMap[rectype] = 
                                    info->GetRecordingTypeRecPriority(rectype);
    }

    recpriority = info->recpriority.toInt();
    recpriority += channelRecPriorityMap[info->chanid];
    recpriority += recTypeRecPriorityMap[rectype];
    recpriorityMap[info->schedulerid] = recpriority;

    return recpriority;
}

void Scheduler::CheckRecPriority(ProgramInfo *info,
                          list<ProgramInfo *> *conflictList)
{
    int recpriority, srecpriority, resolved = 0;

    recpriority = totalRecPriority(info);

    list<ProgramInfo *>::iterator i = conflictList->begin();
    for (; i != conflictList->end(); i++)
    {
        ProgramInfo *second = (*i);

        srecpriority = totalRecPriority(second);

        if (recpriority == srecpriority)
            continue;

        if (recpriority > srecpriority)
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

            del->recording = false;
            del->recstatus = rsManualConflict;
        }
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
                    test->endts == badend)
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
        if (test->recording == true)
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
                if (test->title == badtitle)
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
        if (test->recording == true)
            conflictsleft = true;
    }

    if (!conflictsleft)
    {
        info->conflicting = false;
    }
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

            if (first->conflicting && first->recording)
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
    
        if (first->conflicting && first->recording)
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
  
        if (first->conflicting && first->recording)
        {
            list<ProgramInfo *> *conflictList = getConflicting(first);
            MarkSingleConflict(first, conflictList);
            delete conflictList;
        }
        else if (!first->recording)
            first->conflicting = false;
    }

    if (doRecPriority && !doRecPriorityFirst) 
    {
        i = recordingList.begin();
        for (; i != recordingList.end(); i++)
        {
            ProgramInfo *first = (*i);

            if (first->conflicting && first->recording)
            {
                list<ProgramInfo *> *conflictList = getConflicting(first);
                CheckRecPriority(first, conflictList);
                delete conflictList;
            }
        }
    }
}

void Scheduler::RemoveConflicts(void)
{
    list<ProgramInfo *>::iterator del;
    list<ProgramInfo *>::iterator i = recordingList.begin();
    while (i != recordingList.end())
    {
        ProgramInfo *first = (*i);

        del = i;
        i++;

        if (!first->recording)
        {
            delete first;
            recordingList.erase(del);
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
        if (testtype < type)
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
        if (first->recording && first->conflicting)
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

        if (second->cardid <= 0)
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
        if (first->recording && first->conflicting)
        {
            numconflicts++;
            allConflictList.push_back(first);
            if (numInputsPerSource[first->sourceid] == 1) 
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

            bool secondmove = (numInputsPerSource[second->sourceid] > 1);

            if (second->conflictfixed)
                secondmove = false;

            if (doRecPriority && 
                totalRecPriority(second) > totalRecPriority(first))
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
    bool resetIter = false;
    EncoderLink *nexttv = NULL;

    ProgramInfo *nextRecording = NULL;
    QDateTime nextrectime;

    QDateTime curtime;
    QDateTime lastupdate = QDateTime::currentDateTime().addDays(-1);

    QString recordfileprefix = gContext->GetFilePrefix();

    list<ProgramInfo *>::iterator recIter;

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
            FillEncoderFreeSpaceCache();
            FillRecordLists();
            lastupdate = curtime;
            VERBOSE(VB_GENERAL, "Found changes in the todo list.");

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
                if ((recIter = recordingList.begin()) != recordingList.end())
                {
                    if (curtime.secsTo((*recIter)->startts) - prerollseconds
                        < idleWaitForRecordingTime * 60)
                    {
                        blockShutdown = false;
                    }
                }
            }
            firstRun = false;
        }

        recIter = recordingList.begin();
        while (recIter != recordingList.end())
        {
            nextRecording = (*recIter);

            if (m_tvList->find(nextRecording->cardid) == m_tvList->end())
            {
                cerr << "invalid cardid " << nextRecording->cardid << endl;
                exit(0);
            }
            nexttv = (*m_tvList)[nextRecording->cardid];
            // cerr << "nexttv = " << nextRecording->cardid;
            // cerr << " title: " << nextRecording->title << endl;

            bool recording = nextRecording->recording;

            nextrectime = nextRecording->recstartts;
            secsleft = curtime.secsTo(nextrectime);

            if (secsleft - prerollseconds > 35)
                break;

            if (recording && nexttv->isLowOnFreeSpace())
            {
                FillEncoderFreeSpaceCache();
                if (nexttv->isLowOnFreeSpace())
                {
                    QString msg = QString("SUPPRESSED recording '%1' on channel"
                                          " %2 on cardid %3, sourceid %4.  Only"
                                          " %5 Megs of disk space available.")
                                          .arg(nextRecording->title)
                                          .arg(nextRecording->chanid)
                                          .arg(nextRecording->cardid)
                                          .arg(nextRecording->sourceid)
                                          .arg(nexttv->getFreeSpace());

                    VERBOSE(VB_GENERAL, msg);

                    QMutexLocker lockit(recordingList_lock);

                    nextRecording->conflicting = false;
                    nextRecording->recording = false;
                    nextRecording->recstatus = rsLowDiskSpace;
                    AddToDontRecord(nextRecording);

                    RemoveRecording(nextRecording);
                    nextRecording = NULL;
                    recIter = recordingList.begin();
                    continue;
                }
            }

            if (recording && nexttv->isTunerLocked())
            {
                QString msg = QString("SUPPRESSED recording \"%1\" on channel: "
                                      "%2 on cardid: %3, sourceid %4. Tuner "
                                      "is locked by an external application.")
                                      .arg(nextRecording->title)
                                      .arg(nextRecording->chanid)
                                      .arg(nextRecording->cardid)
                                      .arg(nextRecording->sourceid);

                VERBOSE(VB_GENERAL, msg);

                QMutexLocker lockit(recordingList_lock);

                nextRecording->conflicting = false;
                nextRecording->recording = false;
                nextRecording->recstatus = rsTunerBusy;
                AddToDontRecord(nextRecording);

                RemoveRecording(nextRecording);
                nextRecording = NULL;
                recIter = recordingList.begin();
                continue;
            }

            if ((recording && !nexttv->IsBusyRecording()) || !recording)
            {
                // Will use pre-roll settings only if no other
                // program is currently being recorded
                secsleft -= prerollseconds;
            }

            //VERBOSE(VB_GENERAL, secsleft << " seconds until " << nextRecording->title);

            if (secsleft > 30)
                break;

            if (recording && secsleft > 2)
            {
                QString id = nextRecording->schedulerid;

                if (!recPendingList.contains(id))
                    recPendingList[id] = false;

                if (recPendingList.contains(id) && recPendingList[id] == false)
                {
                    nexttv->RecordPending(nextRecording, secsleft);
                    recPendingList[id] = true;
                }
            }

            if (recording && secsleft <= -2)
            {
                QString msg;

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

                AddToDontRecord(nextRecording);

                RemoveRecording(nextRecording);
                nextRecording = NULL;
                recIter = recordingList.begin();
                resetIter = true;
            }
 
            if (!recording && secsleft <= -2)
            {
                nextRecording->conflicting = false;
                AddToDontRecord(nextRecording);

                RemoveRecording(nextRecording);
                nextRecording = NULL;
                recIter = recordingList.begin();
                resetIter = true;
            }

            if (resetIter)
                resetIter = false;
            else
                recIter++;
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
