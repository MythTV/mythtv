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

Scheduler::Scheduler(bool runthread, QMap<int, EncoderLink *> *tvList, 
                     QSqlDatabase *ldb)
{
    hasconflicts = false;
    db = ldb;
    m_tvList = tvList;

    pthread_mutex_init(&schedulerLock, NULL);

    setupCards();

    threadrunning = runthread;

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
        exit(0);
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
        return (a->startts < b->startts);
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
    doRank = (bool)gContext->GetNumSetting("RankingActive");
    doRankFirst = (bool)gContext->GetNumSetting("RankingOrder");

    if (rankMap.size() > 0)
        rankMap.clear();
    if (channelRankMap.size() > 0)
        channelRankMap.clear();
    if (recTypeRankMap.size() > 0)
        recTypeRankMap.clear();

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

    // PrintList();

    return hasconflicts;
}

void Scheduler::PrintList(void)
{
    QString totrank;

    cout << "--- print list start ---\n";
    list<ProgramInfo *>::iterator i = recordingList.begin();
    cout << "Title                 Chan  ChID  StartTime       S I C "
            "-- C R D S Rank Total" << endl;
    for (; i != recordingList.end(); i++)
    {
        ProgramInfo *first = (*i);

        totrank = QString::number(totalRank(first));

        cout << first->title.leftJustify(22, ' ', true)
             << first->chanstr.rightJustify(4, ' ') << "  " << first->chanid 
             << first->startts.toString("  MMM dd hh:mmap  ")
             << first->sourceid 
             << " " << first->inputid << " " << first->cardid << " -- "  
             << first->conflicting << " " << first->recording << " "
             << first->duplicate << " " << first->suppressed << " "
             << first->rank.rightJustify(4, ' ') << " "
             << totrank.rightJustify(4, ' ')
             << endl;
    }

    cout << "---  print list end  ---\n";
}

void Scheduler::AddToDontRecord(ProgramInfo *pginfo)
{
    bool found = false;

    QValueList<ProgramInfo>::Iterator dreciter = dontRecordList.begin();
    for (; dreciter != dontRecordList.end(); ++dreciter)
    {
        if ((*dreciter).IsSameProgramTimeslot(*pginfo))
        {
            found = true;
            break;
        }
    }

    if (!found)
        dontRecordList.append(*pginfo);
}

void Scheduler::PruneDontRecords(void)
{
    QDateTime curtime = QDateTime::currentDateTime();

    QValueList<ProgramInfo>::Iterator dreciter = dontRecordList.begin();
    while (dreciter != dontRecordList.end())
    {
        if ((*dreciter).endts < curtime)
            dreciter = dontRecordList.remove(dreciter);
        else
            dreciter++;
    }
}

void Scheduler::RemoveRecording(ProgramInfo *pginfo)
{
    recPendingList.remove(pginfo->schedulerid);

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

    if ((a->startts <= b->startts && b->startts < a->endts) ||
        (a->startts <  b->endts   && b->endts   < a->endts) ||
        (b->startts <= a->startts && a->startts < b->endts) ||
        (b->startts <  a->endts   && a->endts   < b->endts))
        return true;
    return false;
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

void Scheduler::PruneList(void)
{
    list<ProgramInfo *>::reverse_iterator i;
    list<ProgramInfo *>::iterator deliter, q;

    QDateTime now = QDateTime::currentDateTime();

    q = recordingList.begin();
    while (q != recordingList.end())
    {
        ProgramInfo *rec = (*q);

        if (!rec->AllowRecordingNewEpisodes(db))
        {
            rec->suppressed = true;
            rec->recording = false;
            rec->reasonsuppressed = tr("the maximum number of episodes have "
                                       "already been recorded.");
        }

        q++;
    }

    q = recordingList.begin();
    while (q != recordingList.end())
    {
        ProgramInfo *rec = (*q);

        if (rec->startts > now)
        {
            break;
        }

        bool removed = false;

        // check the "don't record" list (deleted programs, already started
        // conflicting programs, etc.

        QValueList<ProgramInfo>::Iterator dreciter = dontRecordList.begin();
        for (; dreciter != dontRecordList.end(); ++dreciter)
        {
            if ((*dreciter).IsSameProgramTimeslot(*rec))
            {
                rec->recording = false;
            }
        }

        QMap<int, EncoderLink *>::Iterator enciter = m_tvList->begin();
        for (; enciter != m_tvList->end(); ++enciter)
        {
            EncoderLink *elink = enciter.data();
            if (elink->isRecording(rec))
            {
                delete rec;
                rec = NULL;
                recordingList.erase(q++);
                removed = true;
                break;
            }
        }

        if (!removed)
            q++;
    }

    i = recordingList.rbegin();
    while (i != recordingList.rend())
    {
        list<ProgramInfo *>::reverse_iterator j = i;
        j++;

        ProgramInfo *first = (*i);

        if (first->GetProgramRecordingStatus(db) > kSingleRecord &&
            (first->subtitle.length() > 2 && first->description.length() > 2))
        {
            if (first->duplicate)
            {
                first->recording = false;
            }
            else
            {
                while (j != recordingList.rend())
                {
                    ProgramInfo *second = (*j);
                    if (first->IsSameTimeslot(*second)) 
                    {
                        delete second;
                        deliter = j.base();
                        deliter--;
                        recordingList.erase(deliter);
                    }
                    else if (first->IsSameProgram(*second))
                    {
                        if ((second->conflicting && !first->conflicting) ||
                            second->startts < now.addSecs(-15))
                        {
                            delete second;
                            deliter = j.base();
                            deliter--;
                            recordingList.erase(deliter);
                        }
                        else
                        {
                            delete first;
                            deliter = i.base();
                            deliter--;
                            recordingList.erase(deliter);
                            break;
                        }
                    }
                    else
                    {
                        j++;
                    }
                }
            }
        }
        i++;
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

    ScheduledRecording::findAllScheduledPrograms(db, scheduledList);

    return &scheduledList;
}

list<ProgramInfo *> *Scheduler::getConflicting(ProgramInfo *pginfo,
                                               bool removenonplaying,
                                               list<ProgramInfo *> *uselist)
{
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

int Scheduler::totalRank(ProgramInfo *info)
{
    int rank = 0;
    RecordingType rectype;

    if (rankMap.contains(info->schedulerid))
        return rankMap[info->schedulerid];

    if (!channelRankMap.contains(info->chanid))
        channelRankMap[info->chanid] = info->GetChannelRank(db, info->chanid);

    rectype = info->GetProgramRecordingStatus(db);
    if (!recTypeRankMap.contains(rectype))
        recTypeRankMap[rectype] = info->GetRecordingTypeRank(rectype);

    rank = info->rank.toInt();
    rank += channelRankMap[info->chanid];
    rank += recTypeRankMap[rectype];
    rankMap[info->schedulerid] = rank;

    return rank;
}

void Scheduler::CheckRank(ProgramInfo *info,
                          list<ProgramInfo *> *conflictList)
{
    int rank, srank, resolved = 0;

    rank = totalRank(info);

    list<ProgramInfo *>::iterator i = conflictList->begin();
    for (; i != conflictList->end(); i++)
    {
        ProgramInfo *second = (*i);

        srank = totalRank(second);

        if (rank == srank)
            continue;

        if (rank > srank)
        {
            second->recording = false;
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

    if (doRank && doRankFirst) 
    {
        i = recordingList.begin();
        for (; i != recordingList.end(); i++)
        {
            ProgramInfo *first = (*i);

            if (first->conflicting && first->recording)
            {
                list<ProgramInfo *> *conflictList = getConflicting(first);
                CheckRank(first, conflictList);
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

    if (doRank && !doRankFirst) 
    {
        i = recordingList.begin();
        for (; i != recordingList.end(); i++)
        {
            ProgramInfo *first = (*i);

            if (first->conflicting && first->recording)
            {
                list<ProgramInfo *> *conflictList = getConflicting(first);
                CheckRank(first, conflictList);
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
            if (test->startts < info->startts)
            {
                best = test;
                break;
            }
            if (test->startts.secsTo(test->endts) >
                info->startts.secsTo(info->endts))
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
        }
        best->conflicting = false;
    }
    else
    {
        info->recording = false;
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
                    cerr << "missing: " << second->cardid << " in tvList\n";
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
                second->inputid = sourceToInput[first->sourceid][0];
                second->cardid = inputToCard[second->inputid];
            }
        }

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

            if (doRank && totalRank(second) > totalRank(first))
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
                        cerr << "Missing: " << lower->cardid << " in tvList\n";
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
                        cerr << "Missing: " << higher->cardid << " in tvList\n";
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

    // wait for slaves to connect
    sleep(2);

    while (1)
    {
        curtime = QDateTime::currentDateTime();

        pthread_mutex_lock(&schedulerLock);

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

            nextrectime = nextRecording->startts;
            secsleft = curtime.secsTo(nextrectime);

            if (secsleft - prerollseconds > 35)
                break;

            if (recording && nexttv->isLowOnFreeSpace())
            {
                QString msg = QString("SUPPRESSED recording \"%1\" on channel: "
                                      "%2 on cardid: %3, sourceid %4.  Only "
                                      "%5 Megs of disk space available.")
                                      .arg(nextRecording->title)
                                      .arg(nextRecording->chanid)
                                      .arg(nextRecording->cardid)
                                      .arg(nextRecording->sourceid)
                                      .arg(nexttv->getFreeSpace());

                VERBOSE(VB_GENERAL, msg);

                RemoveRecording(nextRecording);
                nextRecording = NULL;
                recIter = recordingList.begin();
                continue;
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

//            VERBOSE(VB_GENERAL, secsleft << " seconds until " << nextRecording->title);

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
                if (nexttv->StartRecording(nextRecording))
                    msg = "Started recording";
                else
                    msg = "Canceled recording"; 

                msg += QString(" \"%1\" on channel: %2 on cardid: %3, "
                               "sourceid %4").arg(nextRecording->title)
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

        pthread_mutex_unlock(&schedulerLock);

        sleep(1);
    }
} 

void *Scheduler::SchedulerThread(void *param)
{
    Scheduler *sched = (Scheduler *)param;
    sched->RunScheduler();
 
    return NULL;
}
