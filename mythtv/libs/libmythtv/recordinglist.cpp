// -*- Mode: c++ -*-

// MythTV headers
#include "mythcontext.h"
#include "mythdb.h"
#include "util.h"
#include "recordinginfo.h"
#include "recordinglist.h"
#include "jobqueue.h"

RecordingList::~RecordingList(void)
{
    clear();
}

RecordingInfo *RecordingList::operator[](uint index)
{
#ifdef PGLIST_USE_LINKED_LIST
    iterator it = pglist.begin();
    for (uint i = 0; i < index; i++, it++)
    {
        if (it == pglist.end())
            return NULL;
    }
    if (it == pglist.end())
        return NULL;
    return *it;
#else
    if (index < pglist.size())
        return pglist[index];
    return NULL;
#endif
}

const RecordingInfo *RecordingList::operator[](uint index) const
{
    return (*(const_cast<RecordingList*>(this)))[index];
}

bool RecordingList::operator==(const RecordingList &b) const
{
    const_iterator it_a  = pglist.begin();
    const_iterator it_b  = b.pglist.begin();
    const_iterator end_a = pglist.end();
    const_iterator end_b = b.pglist.end();

    for (; it_a != end_a && it_b != end_b; ++it_a, ++it_b)
    {
        if (*it_a != *it_b)
            return false;
    }

    return (it_a == end_a) && (it_b == end_b);
}

RecordingInfo *RecordingList::take(uint index)
{
#ifndef PGLIST_USE_LINKED_LIST
    if (pglist.begin() == pglist.end())
        return NULL;

    if (0 == index)
    {
        RecordingInfo *pginfo = pglist.front();
        pglist.pop_front();
        return pginfo;
    }

    if (pglist.size() == (index + 1))
    {
        RecordingInfo *pginfo = pglist.back();
        pglist.pop_back();
        return pginfo;
    }
#endif

    iterator it = pglist.begin();
    for (uint i = 0; i < index; i++, it++)
    {
        if (it == pglist.end())
            return NULL;
    }
    RecordingInfo *pginfo = *it;
    pglist.erase(it);
    return pginfo;
}

RecordingList::iterator RecordingList::erase(iterator it)
{
    if (autodelete)
        delete *it;
    return pglist.erase(it);
}

void RecordingList::clear(void)
{
    if (autodelete)
    {
        iterator it = pglist.begin();
        for (; it != pglist.end(); ++it)
            delete *it;
    }
    pglist.clear();
}

void RecordingList::sort(bool (&f)(const RecordingInfo*, const RecordingInfo*))
{
#ifdef PGLIST_USE_LINKED_LIST
    pglist.sort(f);
#else
    stable_sort(begin(), end(), f);
#endif
}

bool RecordingList::FromScheduler(bool &hasConflicts, QString tmptable,
                                int recordid)
{
    clear();
    hasConflicts = false;

    if (gContext->IsBackend())
        return false;

    QString query;
    if (tmptable != "")
    {
        query = QString("QUERY_GETALLPENDING %1 %2")
                        .arg(tmptable).arg(recordid);
    } else {
        query = QString("QUERY_GETALLPENDING");
    }

    QStringList slist( query );
    if (!gContext->SendReceiveStringList(slist) || slist.size() < 2)
    {
        VERBOSE(VB_IMPORTANT,
                "RecordingList::FromScheduler(): Error querying master.");
        return false;
    }

    hasConflicts = slist[0].toInt();

    bool result = true;
    QStringList::const_iterator sit = slist.begin()+2;

    while (result && sit != slist.end())
    {
        RecordingInfo *p = new RecordingInfo();
        result = p->FromStringList(sit, slist.end());
        if (result)
            pglist.push_back(p);
        else
            delete p;
    }

    if (pglist.size() != slist[1].toUInt())
    {
        VERBOSE(VB_IMPORTANT,
                "RecordingList::FromScheduler(): Length mismatch");
        clear();
        result = false;
    }

    return result;
}

bool RecordingList::FromProgram(const QString &sql, MSqlBindings &bindings,
                              RecordingList &schedList, bool oneChanid)
{
    clear();

    QString querystr = QString(
        "SELECT DISTINCT program.chanid, program.starttime, program.endtime, "
        "    program.title, program.subtitle, program.description, "
        "    program.category, channel.channum, channel.callsign, "
        "    channel.name, program.previouslyshown, channel.commmethod, "
        "    channel.outputfilters, program.seriesid, program.programid, "
        "    program.airdate, program.stars, program.originalairdate, "
        "    program.category_type, oldrecstatus.recordid, "
        "    oldrecstatus.rectype, oldrecstatus.recstatus, "
        "    oldrecstatus.findid "
        "FROM program "
        "LEFT JOIN channel ON program.chanid = channel.chanid "
        "LEFT JOIN oldrecorded AS oldrecstatus ON "
        "    program.title = oldrecstatus.title AND "
        "    channel.callsign = oldrecstatus.station AND "
        "    program.starttime = oldrecstatus.starttime "
        ) + sql;

    if (!sql.contains(" GROUP BY "))
        querystr += " GROUP BY program.starttime, channel.channum, "
            "  channel.callsign, program.title ";
    if (!sql.contains(" ORDER BY "))
    {
        querystr += " ORDER BY program.starttime, ";
        QString chanorder = gContext->GetSetting("ChannelOrdering", "channum");
        if (chanorder != "channum")
            querystr += chanorder + " ";
        else // approximation which the DB can handle
            querystr += "atsc_major_chan,atsc_minor_chan,channum,callsign ";
    }
    if (!sql.contains(" LIMIT "))
        querystr += " LIMIT 20000 ";

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(querystr);
    MSqlBindings::const_iterator it;
    for (it = bindings.begin(); it != bindings.end(); ++it)
        if (querystr.contains(it.key()))
            query.bindValue(it.key(), it.value());

    if (!query.exec())
    {
        MythDB::DBError("RecordingList::FromProgram", query);
        return false;
    }

    while (query.next())
    {
        RecordingInfo *p = new RecordingInfo;
        p->chanid = query.value(0).toString();
        p->startts = QDateTime::fromString(query.value(1).toString(),
                                           Qt::ISODate);
        p->endts = QDateTime::fromString(query.value(2).toString(),
                                         Qt::ISODate);
        p->recstartts = p->startts;
        p->recendts = p->endts;
        p->lastmodified = p->startts;
        p->title = query.value(3).toString();
        p->subtitle = query.value(4).toString();
        p->description = query.value(5).toString();
        p->category = query.value(6).toString();
        p->chanstr = query.value(7).toString();
        p->chansign = query.value(8).toString();
        p->channame = query.value(9).toString();
        p->repeat = query.value(10).toInt();
        p->chancommfree = COMM_DETECT_COMMFREE == query.value(11).toInt();
        p->chanOutputFilters = query.value(12).toString();
        p->seriesid = query.value(13).toString();
        p->programid = query.value(14).toString();
        p->year = query.value(15).toString();
        p->stars = query.value(16).toString().toFloat();

        if (query.value(17).isNull() || query.value(17).toString().isEmpty())
        {
            p->originalAirDate = QDate (0, 1, 1);
            p->hasAirDate = false;
        }
        else
        {
            p->originalAirDate =
                QDate::fromString(query.value(17).toString(),Qt::ISODate);

            if (p->originalAirDate > QDate(1940, 1, 1))
                p->hasAirDate = true;
            else
                p->hasAirDate = false;
        }
        p->catType = query.value(18).toString();
        p->recordid = query.value(19).toInt();
        p->rectype = RecordingType(query.value(20).toInt());
        p->recstatus = RecStatusType(query.value(21).toInt());
        p->findid = query.value(22).toInt();

        iterator it = schedList.pglist.begin();
        for (; it != schedList.pglist.end(); ++it)
        {
            RecordingInfo *s = *it;
            if (p->IsSameTimeslot(*s))
            {
                p->recordid = s->recordid;
                p->recstatus = s->recstatus;
                p->rectype = s->rectype;
                p->recpriority = s->recpriority;
                p->recstartts = s->recstartts;
                p->recendts = s->recendts;
                p->cardid = s->cardid;
                p->inputid = s->inputid;
                p->dupin = s->dupin;
                p->dupmethod = s->dupmethod;
                p->findid = s->findid;

                if (s->recstatus == rsWillRecord ||
                    s->recstatus == rsRecording)
                {
                    if (oneChanid)
                    {
                        p->chanid   = s->chanid;
                        p->chanstr  = s->chanstr;
                        p->chansign = s->chansign;
                        p->channame = s->channame;
                    }
                    else if ((p->chanid != s->chanid) &&
                             (p->chanstr != s->chanstr))
                    {
                        p->recstatus = rsOtherShowing;
                    }
                }
            }
        }

        pglist.push_back(p);
    }

    return true;
}

bool RecordingList::FromRecorded( bool bDescending, RecordingList *pSchedList )
{
    clear();

    QString     fs_db_name = "";
    QDateTime   rectime    = QDateTime::currentDateTime().addSecs(
                              -gContext->GetNumSetting("RecordOverTime"));

    QString ip        = gContext->GetSetting("BackendServerIP");
    QString port      = gContext->GetSetting("BackendServerPort");

    // ----------------------------------------------------------------------

    QMap<QString, int> inUseMap;

    QString     inUseKey;
    QString     inUseForWhat;
    QDateTime   oneHourAgo = QDateTime::currentDateTime().addSecs(-61 * 60);

    MSqlQuery   query(MSqlQuery::InitCon());

    query.prepare("SELECT DISTINCT chanid, starttime, recusage "
                  " FROM inuseprograms WHERE lastupdatetime >= :ONEHOURAGO ;");
    query.bindValue(":ONEHOURAGO", oneHourAgo);

    if (query.exec())
    {
        while (query.next())
        {
            inUseKey = query.value(0).toString() + " " +
                       query.value(1).toDateTime().toString(Qt::ISODate);
            inUseForWhat = query.value(2).toString();

            if (!inUseMap.contains(inUseKey))
                inUseMap[inUseKey] = 0;

            if ((inUseForWhat == "player") ||
                (inUseForWhat == "preview player") ||
                (inUseForWhat == "PIP player"))
                inUseMap[inUseKey] = inUseMap[inUseKey] | FL_INUSEPLAYING;
            else if (inUseForWhat == "recorder")
                inUseMap[inUseKey] = inUseMap[inUseKey] | FL_INUSERECORDING;
        }
    }

    // ----------------------------------------------------------------------

    QMap<QString,bool> is_job_running;
    query.prepare("SELECT chanid, starttime, status FROM jobqueue "
                  "WHERE type = :TYPE");
    query.bindValue(":TYPE", JOB_COMMFLAG);
    if (query.exec())
    {
        while (query.next())
        {
            uint      chanid     = query.value(0).toUInt();
            QDateTime recstartts = query.value(1).toDateTime();
            int       tmpStatus  = query.value(2).toInt();
            if ((tmpStatus != JOB_UNKNOWN) &&
                (tmpStatus != JOB_QUEUED) &&
                (!(tmpStatus & JOB_DONE)))
            {
                is_job_running[
                    QString("%1###%2")
                    .arg(chanid).arg(recstartts.toString())] = true;
            }
        }
    }

    // ----------------------------------------------------------------------

    QString thequery =
        "SELECT recorded.chanid,recorded.starttime,recorded.endtime,"
        "recorded.title,recorded.subtitle,recorded.description,"
        "recorded.hostname,channum,name,callsign,commflagged,cutlist,"
        "recorded.autoexpire,editing,bookmark,recorded.category,"
        "recorded.recgroup,record.dupin,record.dupmethod,"
        "record.recordid,outputfilters,"
        "recorded.seriesid,recorded.programid,recorded.filesize, "
        "recorded.lastmodified, recorded.findid, "
        "recorded.originalairdate, recorded.playgroup, "
        "recorded.basename, recorded.progstart, "
        "recorded.progend, recorded.stars, "
        "recordedprogram.audioprop+0, recordedprogram.videoprop+0, "
        "recordedprogram.subtitletypes+0, recorded.watched, "
        "recorded.storagegroup "
        "FROM recorded "
        "LEFT JOIN record ON recorded.recordid = record.recordid "
        "LEFT JOIN channel ON recorded.chanid = channel.chanid "
        "LEFT JOIN recordedprogram ON "
        " ( recorded.chanid    = recordedprogram.chanid AND "
        "   recorded.progstart = recordedprogram.starttime ) "
        "WHERE ( recorded.deletepending = 0 OR "
        "        recorded.lastmodified <= DATE_SUB(NOW(), INTERVAL 5 MINUTE) "
        "      ) "
        "ORDER BY recorded.starttime";

    if ( bDescending )
        thequery += " DESC";

    QString chanorder = gContext->GetSetting("ChannelOrdering", "channum");
    if (chanorder != "channum")
        thequery += ", " + chanorder;
    else // approximation which the DB can handle
        thequery += ",atsc_major_chan,atsc_minor_chan,channum,callsign";

    query.prepare(thequery);

    if (!query.exec())
    {
        MythDB::DBError("RecordingList::FromRecorded", query);
        return true;
    }

    while (query.next())
    {
        RecordingInfo *proginfo = new RecordingInfo;

        proginfo->chanid        = query.value(0).toString();
        proginfo->startts       = query.value(29).toDateTime();
        proginfo->endts         = query.value(30).toDateTime();
        proginfo->recstartts    = query.value(1).toDateTime();
        proginfo->recendts      = query.value(2).toDateTime();
        proginfo->title         = query.value(3).toString();
        proginfo->subtitle      = query.value(4).toString();
        proginfo->description   = query.value(5).toString();
        proginfo->hostname      = query.value(6).toString();

        proginfo->dupin         = RecordingDupInType(query.value(17).toInt());
        proginfo->dupmethod     = RecordingDupMethodType(query.value(18).toInt());
        proginfo->recordid      = query.value(19).toInt();
        proginfo->chanOutputFilters = query.value(20).toString();
        proginfo->seriesid      = query.value(21).toString();
        proginfo->programid     = query.value(22).toString();
        proginfo->filesize      = stringToLongLong(query.value(23).toString());
        proginfo->lastmodified  = QDateTime::fromString(query.value(24).toString(), Qt::ISODate);
        proginfo->findid        = query.value(25).toInt();

        if (query.value(26).isNull() ||
            query.value(26).toString().isEmpty())
        {
            proginfo->originalAirDate = QDate (0, 1, 1);
            proginfo->hasAirDate      = false;
        }
        else
        {
            proginfo->originalAirDate =
                QDate::fromString(query.value(26).toString(),Qt::ISODate);

            if (proginfo->originalAirDate > QDate(1940, 1, 1))
                proginfo->hasAirDate  = true;
            else
                proginfo->hasAirDate  = false;
        }

        proginfo->pathname = query.value(28).toString();


        if (proginfo->hostname.isEmpty() || proginfo->hostname.isNull())
            proginfo->hostname = gContext->GetHostName();

        if (!query.value(7).toString().isEmpty())
        {
            proginfo->chanstr  = query.value(7).toString();
            proginfo->channame = query.value(8).toString();
            proginfo->chansign = query.value(9).toString();
        }
        else
        {
            proginfo->chanstr  = "#" + proginfo->chanid;
            proginfo->channame = "#" + proginfo->chanid;
            proginfo->chansign = "#" + proginfo->chanid;
        }

        int flags = 0;

        flags |= (query.value(10).toInt() == 1)           ? FL_COMMFLAG : 0;
        flags |= (query.value(11).toInt() == 1)           ? FL_CUTLIST  : 0;
        flags |=  query.value(12).toInt()                 ? FL_AUTOEXP  : 0;
        flags |= (query.value(14).toInt() == 1)           ? FL_BOOKMARK : 0;
        flags |= (query.value(35).toInt() == 1)           ? FL_WATCHED  : 0;

        inUseKey = query.value(0).toString() + " " +
            query.value(1).toDateTime().toString(Qt::ISODate);

        if (inUseMap.contains(inUseKey))
            flags |= inUseMap[inUseKey];

        if (query.value(13).toInt())
        {
            flags |= FL_EDITING;
        }
        else if (query.value(10).toInt() == COMM_FLAG_PROCESSING)
        {
            bool running =
                is_job_running.find(
                    QString("%1###%2")
                    .arg(proginfo->chanid)
                    .arg(proginfo->recstartts.toString())) !=
                is_job_running.end();
            if (running)
                flags |= FL_EDITING;
            else
                proginfo->SetCommFlagged(COMM_FLAG_NOT_FLAGGED);
        }

        proginfo->programflags = flags;

        proginfo->audioproperties = query.value(32).toInt();
        proginfo->videoproperties = query.value(33).toInt();
        proginfo->subtitleType = query.value(34).toInt();

        proginfo->category     = query.value(15).toString();
        proginfo->recgroup     = query.value(16).toString();
        proginfo->playgroup    = query.value(27).toString();
        proginfo->storagegroup = query.value(36).toString();
        proginfo->recstatus    = rsRecorded;

        if ((pSchedList != NULL) && (proginfo->recendts > rectime))
        {
            iterator it = pSchedList->pglist.begin();
            for (; it != pSchedList->pglist.end(); ++it)
            {
                ProgramInfo *s = *it;
                if (s && s->recstatus    == rsRecording &&
                    proginfo->chanid     == s->chanid   &&
                    proginfo->recstartts == s->recstartts)
                {
                    proginfo->recstatus = rsRecording;
                    break;
                }
            }
        }

        proginfo->stars = query.value(31).toDouble();

        pglist.push_back(proginfo);
    }

    return true;
}


bool RecordingList::FromOldRecorded(const QString &sql, MSqlBindings &bindings)
{
    clear();
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT oldrecorded.chanid, starttime, endtime, "
                  " title, subtitle, description, category, seriesid, "
                  " programid, channel.channum, channel.callsign, "
                  " channel.name, findid, rectype, recstatus, recordid, "
                  " duplicate "
                  " FROM oldrecorded "
                  " LEFT JOIN channel ON oldrecorded.chanid = channel.chanid "
                  + sql);
    query.bindValues(bindings);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("RecordingList::FromOldRecorded", query);
        return false;
    }

    while (query.next())
    {
        RecordingInfo *p = new RecordingInfo;
        p->chanid = query.value(0).toString();
        p->startts = QDateTime::fromString(query.value(1).toString(),
                                           Qt::ISODate);
        p->endts = QDateTime::fromString(query.value(2).toString(),
                                         Qt::ISODate);
        p->recstartts = p->startts;
        p->recendts = p->endts;
        p->lastmodified = p->startts;
        p->title = query.value(3).toString();
        p->subtitle = query.value(4).toString();
        p->description = query.value(5).toString();
        p->category = query.value(6).toString();
        p->seriesid = query.value(7).toString();
        p->programid = query.value(8).toString();
        p->chanstr = query.value(9).toString();
        p->chansign = query.value(10).toString();
        p->channame = query.value(11).toString();
        p->findid = query.value(12).toInt();
        p->rectype = RecordingType(query.value(13).toInt());
        p->recstatus = RecStatusType(query.value(14).toInt());
        p->recordid = query.value(15).toInt();
        p->duplicate = query.value(16).toInt();

        pglist.push_back(p);
    }

    return true;
}

bool RecordingList::GetProgramDetailList(
    QDateTime &nextRecordingStart, bool *hasConflicts, ProgramDetailList *list)
{
    nextRecordingStart = QDateTime();

    bool dummy;
    bool *conflicts = (hasConflicts) ? hasConflicts : &dummy;

    RecordingList progList;
    if (!progList.FromScheduler(*conflicts))
        return false;

    // find the earliest scheduled recording
    RecordingList::const_iterator it = progList.begin();
    for (; it != progList.end(); ++it)
    {
        if (((*it)->recstatus == rsWillRecord) &&
            (nextRecordingStart.isNull() ||
             nextRecordingStart > (*it)->recstartts))
        {
            nextRecordingStart = (*it)->recstartts;
        }
    }

    if (!list)
        return true;

    // save the details of the earliest recording(s)
    for (it = progList.begin(); it != progList.end(); ++it)
    {
        if (((*it)->recstatus  == rsWillRecord) &&
            ((*it)->recstartts == nextRecordingStart))
        {
            ProgramDetail prog;
            prog.channame  = (*it)->channame;
            prog.title     = (*it)->title;
            prog.subtitle  = (*it)->subtitle;
            prog.startTime = (*it)->recstartts;
            prog.endTime   = (*it)->recendts;
            list->push_back(prog);
        }
    }

    return true;
}
