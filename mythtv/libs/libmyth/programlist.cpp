// -*- Mode: c++ -*-

// MythTV headers
#include "mythcorecontext.h"
#include "mythdb.h"
#include "util.h"
#include "programlist.h"
#include "programinfo.h"

ProgramList::~ProgramList(void)
{
    clear();
}

ProgramInfo *ProgramList::operator[](uint index)
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

const ProgramInfo *ProgramList::operator[](uint index) const
{
    return (*(const_cast<ProgramList*>(this)))[index];
}

ProgramInfo *ProgramList::take(uint index)
{
#ifndef PGLIST_USE_LINKED_LIST
    if (pglist.begin() == pglist.end())
        return NULL;

    if (0 == index)
    {
        ProgramInfo *pginfo = pglist.front();
        pglist.pop_front();
        return pginfo;
    }

    if (pglist.size() == (index + 1))
    {
        ProgramInfo *pginfo = pglist.back();
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
    ProgramInfo *pginfo = *it;
    pglist.erase(it);
    return pginfo;
}

ProgramList::iterator ProgramList::erase(iterator it)
{
    if (autodelete)
        delete *it;
    return pglist.erase(it);
}

void ProgramList::clear(void)
{
    if (autodelete)
    {
        iterator it = pglist.begin();
        for (; it != pglist.end(); ++it)
            delete *it;
    }
    pglist.clear();
}

void ProgramList::sort(bool (&f)(const ProgramInfo*, const ProgramInfo*))
{
#ifdef PGLIST_USE_LINKED_LIST
    pglist.sort(f);
#else
    stable_sort(begin(), end(), f);
#endif
}

//////////////////////////////////////////////////////////////////////

static bool FromProgramQuery(
    const QString &sql, const MSqlBindings &bindings, MSqlQuery &query)
{
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
        QString chanorder = gCoreContext->GetSetting("ChannelOrdering", "channum");
        if (chanorder != "channum")
            querystr += chanorder + " ";
        else // approximation which the DB can handle
            querystr += "atsc_major_chan,atsc_minor_chan,channum,callsign ";
    }
    if (!sql.contains(" LIMIT "))
        querystr += " LIMIT 20000 ";

    query.prepare(querystr);
    MSqlBindings::const_iterator it;
    for (it = bindings.begin(); it != bindings.end(); ++it)
    {
        if (querystr.contains(it.key()))
            query.bindValue(it.key(), it.value());
    }

    if (!query.exec())
    {
        MythDB::DBError("LoadFromProgramQuery", query);
        return false;
    }

    return true;
}

bool LoadFromProgram(
    ProgramList &destination,
    const QString &sql, const MSqlBindings &bindings,
    const ProgramList &schedList, bool oneChanid)
{
    destination.clear();

    MSqlQuery query(MSqlQuery::InitCon());
    if (!FromProgramQuery(sql, bindings, query))
        return false;

    while (query.next())
        destination.push_back(new ProgramInfo(query, schedList, oneChanid));

    return true;
}

bool LoadFromOldRecorded(
    ProgramList &destination, const QString &sql, const MSqlBindings &bindings)
{
    destination.clear();

    MSqlQuery query(MSqlQuery::InitCon());
    
    QString querystr =
        "SELECT oldrecorded.chanid, starttime, endtime, "
        "       title, subtitle, description, category, seriesid, "
        "       programid, channel.channum, channel.callsign, "
        "       channel.name, findid, rectype, recstatus, recordid, "
        "       duplicate "
        " FROM oldrecorded "
        " LEFT JOIN channel ON oldrecorded.chanid = channel.chanid "
        + sql;

    query.prepare(querystr);
    MSqlBindings::const_iterator it;
    for (it = bindings.begin(); it != bindings.end(); ++it)
    {
        if (querystr.contains(it.key()))
            query.bindValue(it.key(), it.value());
    }

    if (!query.exec())
    {
        MythDB::DBError("LoadFromOldRecorded", query);
        return false;
    }

    while (query.next())
    {
        ProgramInfo *p = new ProgramInfo;
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

        destination.push_back(p);
    }

    return true;
}

bool LoadFromRecorded(
    ProgramList &destination,
    bool orderDescending,
    bool possiblyInProgressTecordingsOnly,
    const ProgramList &schedList)
{
    destination.clear();

    QString     fs_db_name = "";
    QDateTime   rectime    = QDateTime::currentDateTime().addSecs(
                              -gCoreContext->GetNumSetting("RecordOverTime"));

    QString ip        = gCoreContext->GetSetting("BackendServerIP");
    QString port      = gCoreContext->GetSetting("BackendServerPort");

    // ----------------------------------------------------------------------

    QMap<QString, int> inUseMap;
    QString   inUseKey;
    QString   inUseForWhat;
    QDateTime oneHourAgo = QDateTime::currentDateTime().addSecs(-61 * 60);

    MSqlQuery query(MSqlQuery::InitCon());

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

            if (inUseForWhat.contains(kPlayerInUseID))
                inUseMap[inUseKey] |= FL_INUSEPLAYING;
            else if (inUseForWhat == kRecorderInUseID)
                inUseMap[inUseKey] |= FL_INUSERECORDING;
        }
    }

    // ----------------------------------------------------------------------

    QMap<QString,bool> is_job_running;
    query.prepare("SELECT chanid, starttime, status FROM jobqueue "
                  "WHERE type = :TYPE");
    query.bindValue(":TYPE", /*JOB_COMMFLAG*/ 0x0002);
    if (query.exec())
    {
        while (query.next())
        {
            uint      chanid     = query.value(0).toUInt();
            QDateTime recstartts = query.value(1).toDateTime();
            int       tmpStatus  = query.value(2).toInt();
            if ((tmpStatus != /*JOB_UNKNOWN*/ 0x0000) &&
                (tmpStatus != /*JOB_QUEUED*/ 0x0001) &&
                (!(tmpStatus & /*JOB_DONE*/ 0x0100)))
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
        "recorded.recordid,channel.outputfilters,"
        "recorded.seriesid,recorded.programid,recorded.filesize, "
        "recorded.lastmodified, recorded.findid, "
        "recorded.originalairdate, recorded.playgroup, "
        "recorded.basename, recorded.progstart, "
        "recorded.progend, recorded.stars, "
        "recordedprogram.audioprop+0, recordedprogram.videoprop+0, "
        "recordedprogram.subtitletypes+0, recorded.watched, "
        "recorded.storagegroup, "
        "recorded.transcoded, recorded.recpriority, "
        "recorded.preserve, recordedprogram.airdate "
        "FROM recorded "
        "LEFT JOIN record ON recorded.recordid = record.recordid "
        "LEFT JOIN channel ON recorded.chanid = channel.chanid "
        "LEFT JOIN recordedprogram ON "
        " ( recorded.chanid    = recordedprogram.chanid AND "
        "   recorded.progstart = recordedprogram.starttime ) "
        "WHERE ( recorded.deletepending = 0 OR "
        "        DATE_ADD(recorded.lastmodified, INTERVAL 5 MINUTE) <= NOW() "
        "      ) ";

    if (possiblyInProgressTecordingsOnly)
    {
        thequery +=
            " AND recorded.endtime   >= NOW() "
            " AND recorded.starttime <= NOW() ";
    }

    thequery += "ORDER BY recorded.starttime";
    thequery += (orderDescending) ? " DESC " : "";

    QString chanorder = gCoreContext->GetSetting("ChannelOrdering", "channum");
    if (chanorder != "channum")
        thequery += ", " + chanorder;
    else // approximation which the DB can handle
        thequery += ",atsc_major_chan,atsc_minor_chan,channum,callsign";

    query.prepare(thequery);

    if (!query.exec())
    {
        MythDB::DBError("ProgramList::FromRecorded", query);
        return true;
    }

    while (query.next())
    {
        ProgramInfo *proginfo = new ProgramInfo;

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
        proginfo->filesize      = query.value(23).toULongLong();
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
            QDate oad = QDate::fromString(
                query.value(26).toString(), Qt::ISODate);
            proginfo->originalAirDate = oad;
            proginfo->hasAirDate = oad.isValid() && (oad > QDate(1940, 1, 1));
        }

        proginfo->pathname = query.value(28).toString();

        if (proginfo->hostname.isEmpty())
            proginfo->hostname = gCoreContext->GetHostName();

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
        flags |= (query.value(37).toInt() == TRANSCODING_COMPLETE) ?
            FL_TRANSCODED : 0;
        flags |= (query.value(39).toInt() == 1)           ? FL_PRESERVED: 0;

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
        proginfo->recpriority  = query.value(38).toInt();
        proginfo->year         = query.value(40).toString();

        if (proginfo->recendts > rectime)
        {
            ProgramList::const_iterator it = schedList.begin();
            for (; it != schedList.end(); ++it)
            {
                const ProgramInfo &s = **it;
                if (s.recstatus          == rsRecording &&
                    proginfo->chanid     == s.chanid    &&
                    proginfo->recstartts == s.recstartts)
                {
                    proginfo->recstatus = rsRecording;
                    break;
                }
            }
        }

        proginfo->stars = query.value(31).toDouble();

        destination.push_back(proginfo);
    }

    return true;
}

bool LoadFromScheduler(
    ProgramList &destination, bool &hasConflicts,
    QString tmptable, int recordid)
{
    destination.clear();
    hasConflicts = false;

    if (gCoreContext->IsBackend())
        return false;

    QString query;
    if (!tmptable.isEmpty())
    {
        query = QString("QUERY_GETALLPENDING %1 %2")
            .arg(tmptable).arg(recordid);
    }
    else
    {
        query = QString("QUERY_GETALLPENDING");
    }

    QStringList slist( query );
    if (!gCoreContext->SendReceiveStringList(slist) || slist.size() < 2)
    {
        VERBOSE(VB_IMPORTANT,
                "LoadFromScheduler(): Error querying master.");
        return false;
    }

    hasConflicts = slist[0].toInt();

    bool result = true;
    QStringList::const_iterator sit = slist.begin()+2;

    while (result && sit != slist.end())
    {
        ProgramInfo *p = new ProgramInfo();
        result = p->FromStringList(sit, slist.end());
        if (result)
            destination.push_back(p);
        else
            delete p;
    }

    if (destination.size() != slist[1].toUInt())
    {
        VERBOSE(VB_IMPORTANT,
                "LoadFromScheduler(): Length mismatch.");
        destination.clear();
        result = false;
    }

    return result;
}

bool LoadFromScheduler(ProgramList &destination)
{
    bool dummyConflicts;
    return LoadFromScheduler(destination, dummyConflicts, "", -1);
}
