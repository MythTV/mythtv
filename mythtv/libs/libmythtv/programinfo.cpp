#include <iostream>
#include <qsocket.h>
#include <qregexp.h>
#include <qmap.h>
#include <qlayout.h>
#include <qlabel.h>

#include "programinfo.h"
#include "scheduledrecording.h"
#include "util.h"
#include "mythcontext.h"
#include "commercial_skip.h"
#include "dialogbox.h"
#include "infodialog.h"
#include "remoteutil.h"

using namespace std;

ProgramInfo::ProgramInfo(void)
{
    spread = -1;
    startCol = -1;

    chanstr = "";
    chansign = "";
    channame = "";

    pathname = "";
    filesize = 0;
    hostname = "";

    startts = QDateTime::currentDateTime();
    endts = startts;
    recstartts = startts;
    recendts = startts;

    conflicting = false;
    recording = false;
    override = 0;
    recstatus = rsUnknown;
    recordid = 0;
    rectype = kNotRecording;
    recdups = kRecordDupsNever;

    sourceid = -1;
    inputid = -1;
    cardid = -1;
    schedulerid = "";
    recpriority = "";

    repeat = false;

    record = NULL;
}   
        
ProgramInfo::ProgramInfo(const ProgramInfo &other)
{           
    title = other.title;
    subtitle = other.subtitle;
    description = other.description;
    category = other.category;
    chanid = other.chanid;
    chanstr = other.chanstr;
    chansign = other.chansign;
    channame = other.channame;
    pathname = other.pathname;
    filesize = other.filesize;
    hostname = other.hostname;

    startts = other.startts;
    endts = other.endts;
    recstartts = other.recstartts;
    recendts = other.recendts;
    spread = other.spread;
    startCol = other.startCol;
 
    conflicting = other.conflicting;
    recording = other.recording;
    override = other.override;
    recstatus = other.recstatus;
    recordid = other.recordid;
    rectype = other.rectype;
    recdups = other.recdups;

    sourceid = other.sourceid;
    inputid = other.inputid;
    cardid = other.cardid;
    schedulerid = other.schedulerid;
    recpriority = other.recpriority;
    programflags = other.programflags;

    repeat = other.repeat;

    record = NULL;
}

ProgramInfo &ProgramInfo::operator=(const ProgramInfo &other)
{
    if (record)
        delete record;

    title = other.title;
    subtitle = other.subtitle;
    description = other.description;
    category = other.category;
    chanid = other.chanid;
    chanstr = other.chanstr;
    chansign = other.chansign;
    channame = other.channame;
    pathname = other.pathname;
    filesize = other.filesize;
    hostname = other.hostname;

    startts = other.startts;
    endts = other.endts;
    recstartts = other.recstartts;
    recendts = other.recendts;
    spread = other.spread;
    startCol = other.startCol;

    conflicting = other.conflicting;
    recording = other.recording;
    override = other.override;
    recstatus = other.recstatus;
    recordid = other.recordid;
    rectype = other.rectype;
    recdups = other.recdups;

    sourceid = other.sourceid;
    inputid = other.inputid;
    cardid = other.cardid;
    schedulerid = other.schedulerid;
    recpriority = other.recpriority;
    programflags = other.programflags;

    repeat = other.repeat;

    record = NULL;

    return *this;
}

ProgramInfo::~ProgramInfo() 
{
    if (record != NULL)
        delete record;
}

QString ProgramInfo::MakeUniqueKey(void) const
{
    return title + ":" + chanid + ":" + startts.toString(Qt::ISODate);
}

void ProgramInfo::ToStringList(QStringList &list)
{
    list << ((title != "") ? title : QString(" "));
    list << ((subtitle != "") ? subtitle : QString(" "));
    list << ((description != "") ? description : QString(" "));
    list << ((category != "") ? category : QString(" "));
    list << ((chanid != "") ? chanid : QString(" "));
    list << ((chanstr != "") ? chanstr : QString(" "));
    list << ((chansign != "") ? chansign : QString(" "));
    list << ((channame != "") ? channame : QString(" "));
    list << ((pathname != "") ? pathname : QString(" "));
    encodeLongLong(list, filesize);
    list << startts.toString(Qt::ISODate);
    list << endts.toString(Qt::ISODate);
    list << QString::number(conflicting);
    list << QString::number(recording);
    list << QString::number(override);
    list << ((hostname != "") ? hostname : QString(" "));
    list << QString::number(sourceid);
    list << QString::number(cardid);
    list << QString::number(inputid);
    list << ((recpriority != "") ? recpriority : QString(" "));
    list << QString::number(recstatus);
    list << QString::number(recordid);
    list << QString::number(rectype);
    list << QString::number(recdups);
    list << recstartts.toString(Qt::ISODate);
    list << recendts.toString(Qt::ISODate);
    list << QString::number(repeat);
    list << QString::number(programflags);
}

void ProgramInfo::FromStringList(QStringList &list, int offset)
{
    QStringList::iterator it = list.at(offset);
    FromStringList(list, it);
}

void ProgramInfo::FromStringList(QStringList &list, QStringList::iterator &it)
{
    QStringList::iterator checkit = it;
    checkit += (NUMPROGRAMLINES - 1);
    if (checkit == list.end())
    {
        cerr << "Not enough items in list for ProgramInfo object" << endl;
        return;
    }

    title = *(it++);
    subtitle = *(it++);
    description = *(it++);
    category = *(it++);
    chanid = *(it++);
    chanstr = *(it++);
    chansign = *(it++);
    channame = *(it++);
    pathname = *(it++);
    filesize = decodeLongLong(list, it);
    startts = QDateTime::fromString(*(it++), Qt::ISODate);
    endts = QDateTime::fromString(*(it++), Qt::ISODate);
    conflicting = (*(it++)).toInt();
    recording = (*(it++)).toInt();
    override = (*(it++)).toInt();
    hostname = *(it++);
    sourceid = (*(it++)).toInt();
    cardid = (*(it++)).toInt();
    inputid = (*(it++)).toInt();
    recpriority = *(it++);
    recstatus = RecStatusType((*(it++)).toInt());
    recordid = (*(it++)).toInt();
    rectype = RecordingType((*(it++)).toInt());
    recdups = RecordingDupsType((*(it++)).toInt());
    recstartts = QDateTime::fromString(*(it++), Qt::ISODate);
    recendts = QDateTime::fromString(*(it++), Qt::ISODate);
    repeat = (*(it++)).toInt();
    programflags = (*(it++)).toInt();

    if (title == " ")
        title = "";
    if (subtitle == " ")
        subtitle = "";
    if (description == " ")
        description = "";
    if (category == " ")
        category = "";
    if (pathname == " ")
        pathname = "";
    if (hostname == " ")
        hostname = "";
    if (chanid == " ")
        chanid = "";
    if (chanstr == " ")
        chanstr = "";
    if (pathname == " ")
        pathname = "";
    if (channame == " ")
        channame = "";
    if (chansign == " ")
        chansign = "";
    if (recpriority == " ")
        recpriority = "";
}

void ProgramInfo::ToMap(QSqlDatabase *db, QMap<QString, QString> &progMap)
{
    QString timeFormat = gContext->GetSetting("TimeFormat", "h:mm AP");
    QString dateFormat = gContext->GetSetting("DateFormat", "ddd MMMM d");
    QString shortDateFormat = gContext->GetSetting("ShortDateFormat", "M/d");

    QDateTime timeNow = QDateTime::currentDateTime();

    QString length;
    int hours, minutes, seconds;

    progMap["title"] = title;

    if (subtitle != "(null)")
        progMap["subtitle"] = subtitle;
    else
        progMap["subtitle"] = "";

    if (description != "(null)")
        progMap["description"] = description;
    else
        progMap["description"] = "";

    progMap["category"] = category;
    progMap["callsign"] = chansign;
    progMap["starttime"] = startts.toString(timeFormat);
    progMap["startdate"] = startts.toString(shortDateFormat);
    progMap["endtime"] = endts.toString(timeFormat);
    progMap["enddate"] = endts.toString(shortDateFormat);
    progMap["recstarttime"] = recstartts.toString(timeFormat);
    progMap["recstartdate"] = recstartts.toString(shortDateFormat);
    progMap["recendtime"] = recendts.toString(timeFormat);
    progMap["recenddate"] = recendts.toString(shortDateFormat);
    progMap["channum"] = chanstr;
    progMap["chanid"] = chanid;
    progMap["iconpath"] = "";

    seconds = recstartts.secsTo(recendts);
    minutes = seconds / 60;
    progMap["lenmins"] = QString("%1").arg(minutes);
    hours   = minutes / 60;
    minutes = minutes % 60;
    length.sprintf("%d:%02d", hours, minutes);
    progMap["lentime"] = length;

    progMap["rec_type"] = RecTypeChar();
    progMap["rec_str"] = RecTypeText();
    if (rectype != kNotRecording)
    {
        progMap["rec_str"] += " - ";
        progMap["rec_str"] += RecStatusText();
    }
    progMap["recordingstatus"] = progMap["rec_str"];
    progMap["type"] = progMap["rec_str"];

    progMap["recpriority"] = recpriority;
    progMap["programflags"] = programflags;

    progMap["timedate"] = recstartts.date().toString(dateFormat) + ", " +
                          recstartts.time().toString(timeFormat) + " - " +
                          recendts.time().toString(timeFormat);

    progMap["shorttimedate"] =
                          recstartts.date().toString(shortDateFormat) + ", " +
                          recstartts.time().toString(timeFormat) + " - " +
                          recendts.time().toString(timeFormat);

    progMap["time"] = timeNow.time().toString(timeFormat);

    if (gContext->GetNumSetting("DisplayChanNum") != 0)
        progMap["channel"] = channame + " [" + chansign + "]";
    else
        progMap["channel"] = chanstr;

    QString thequery = QString("SELECT icon FROM channel WHERE chanid = %1")
                               .arg(chanid);
    QSqlQuery query = db->exec(thequery);
    if (query.isActive() && query.numRowsAffected() > 0)
        if (query.next())
            progMap["iconpath"] = query.value(0).toString();

    progMap["RECSTATUS"] = RecStatusText();

    if (repeat)
        progMap["REPEAT"] = QObject::tr("Repeat");
    else
        progMap["REPEAT"] = "";
}

int ProgramInfo::CalculateLength(void)
{
    return startts.secsTo(endts);
}

void ProgramInfo::GetProgramListByQuery(QSqlDatabase *db,
                                        QPtrList<ProgramInfo> *proglist, 
                                        const QString &where)
{
    QString thequery;

    thequery = QString("SELECT channel.chanid,starttime,endtime,title,"
                       "subtitle,description,category,channel.channum,"
                       "channel.callsign,channel.name,previouslyshown "
                       "FROM program,channel ")
                       + where;

    QSqlQuery query = db->exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        while (query.next())
        {
            ProgramInfo *proginfo = new ProgramInfo;
            proginfo->chanid = query.value(0).toString();
            proginfo->startts = QDateTime::fromString(query.value(1).toString(),
                                                      Qt::ISODate);
            proginfo->endts = QDateTime::fromString(query.value(2).toString(),
                                                    Qt::ISODate);
            proginfo->recstartts = proginfo->startts;
            proginfo->recendts = proginfo->endts;
            proginfo->title = QString::fromUtf8(query.value(3).toString());
            proginfo->subtitle = QString::fromUtf8(query.value(4).toString());
            proginfo->description = 
                                   QString::fromUtf8(query.value(5).toString());
            proginfo->category = QString::fromUtf8(query.value(6).toString());
            proginfo->chanstr = query.value(7).toString();
            proginfo->chansign = query.value(8).toString();
            proginfo->channame = query.value(9).toString();
            proginfo->repeat = query.value(10).toInt();
            proginfo->spread = -1;

            if (proginfo->title == QString::null)
                proginfo->title = "";
            if (proginfo->subtitle == QString::null)
                proginfo->subtitle = "";
            if (proginfo->description == QString::null)
                proginfo->description = "";
            if (proginfo->category == QString::null)
                proginfo->category = "";

            proglist->append(proginfo);
        }
    }
}    

void ProgramInfo::GetProgramRangeDateTime(QSqlDatabase *db,
                                          QPtrList<ProgramInfo> *proglist,
                                          const QString &channel,
                                          const QString &ltime,
                                          const QString &rtime)
{
    QString where;

    where = QString("WHERE program.chanid = %1 AND endtime >= %1 AND "
                    "starttime <= %3 AND program.chanid = channel.chanid "
                    "ORDER BY starttime;")
                    .arg(channel).arg(ltime).arg(rtime);

    GetProgramListByQuery(db, proglist, where);
}

ProgramInfo *ProgramInfo::GetProgramAtDateTime(QSqlDatabase *db,
                                               const QString &channel, 
                                               const QString &ltime)
{ 
    QString thequery;
   
    thequery = QString("SELECT channel.chanid,starttime,endtime,title,subtitle,"
                       "description,category,channel.channum,channel.callsign, "
                       "channel.name,previouslyshown FROM program,channel "
                       "WHERE program.chanid = %1 AND starttime < %2 AND "
                       "endtime > %3 AND program.chanid = channel.chanid;")
                       .arg(channel).arg(ltime).arg(ltime);

    QSqlQuery query = db->exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        ProgramInfo *proginfo = new ProgramInfo;
        proginfo->chanid = query.value(0).toString();
        proginfo->startts = QDateTime::fromString(query.value(1).toString(),
                                                  Qt::ISODate);
        proginfo->endts = QDateTime::fromString(query.value(2).toString(),
                                                Qt::ISODate);
        proginfo->recstartts = proginfo->startts;
        proginfo->recendts = proginfo->endts;
        proginfo->title = QString::fromUtf8(query.value(3).toString());
        proginfo->subtitle = QString::fromUtf8(query.value(4).toString());
        proginfo->description = QString::fromUtf8(query.value(5).toString());
        proginfo->category = QString::fromUtf8(query.value(6).toString());
        proginfo->chanstr = query.value(7).toString();
        proginfo->chansign = query.value(8).toString();
        proginfo->channame = query.value(9).toString();
        proginfo->repeat = query.value(10).toInt();
        proginfo->spread = -1;

        if (proginfo->title == QString::null)
            proginfo->title = "";
        if (proginfo->subtitle == QString::null)
            proginfo->subtitle = "";
        if (proginfo->description == QString::null)
            proginfo->description = "";
        if (proginfo->category == QString::null)
            proginfo->category = "";

        return proginfo;
    }

    return NULL;
}

ProgramInfo *ProgramInfo::GetProgramAtDateTime(QSqlDatabase *db,
                                               const QString &channel, 
                                               QDateTime &dtime)
{
    QString sqltime = dtime.toString("yyyyMMddhhmm");
    sqltime += "50"; 

    return GetProgramAtDateTime(db, channel, sqltime);
}

ProgramInfo *ProgramInfo::GetProgramFromRecorded(QSqlDatabase *db,
                                                 const QString &channel, 
                                                 QDateTime &dtime)
{
    QString sqltime = dtime.toString("yyyyMMddhhmm");
    sqltime += "00"; 

    return GetProgramFromRecorded(db, channel, sqltime);
}

ProgramInfo *ProgramInfo::GetProgramFromRecorded(QSqlDatabase *db,
                                                 const QString &channel, 
                                                 const QString &starttime)
{
    QString thequery;
   
    thequery = QString("SELECT recorded.chanid,starttime,endtime,title, "
                       "subtitle,description,channel.channum, "
                       "channel.callsign,channel.name "
                       "FROM recorded "
                       "LEFT JOIN channel "
                       "ON recorded.chanid = channel.chanid "
                       "WHERE recorded.chanid = %1 "
                       "AND starttime = %2;")
                       .arg(channel).arg(starttime);

    QSqlQuery query = db->exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        ProgramInfo *proginfo = new ProgramInfo;
        proginfo->chanid = query.value(0).toString();
        proginfo->startts = QDateTime::fromString(query.value(1).toString(),
                                                  Qt::ISODate);
        proginfo->endts = QDateTime::fromString(query.value(2).toString(),
                                                Qt::ISODate);
        proginfo->recstartts = proginfo->startts;
        proginfo->recendts = proginfo->endts;
        proginfo->title = QString::fromUtf8(query.value(3).toString());
        proginfo->subtitle = QString::fromUtf8(query.value(4).toString());
        proginfo->description = QString::fromUtf8(query.value(5).toString());

        proginfo->chanstr = query.value(6).toString();
        proginfo->chansign = query.value(7).toString();
        proginfo->channame = query.value(8).toString();
        proginfo->spread = -1;

        if (proginfo->title == QString::null)
            proginfo->title = "";
        if (proginfo->subtitle == QString::null)
            proginfo->subtitle = "";
        if (proginfo->description == QString::null)
            proginfo->description = "";
        if (proginfo->category == QString::null)
            proginfo->category = "";

        if (proginfo->chanstr == QString::null)
            proginfo->chanstr = "";
        if (proginfo->chansign == QString::null)
            proginfo->chansign = "";
        if (proginfo->channame == QString::null)
            proginfo->channame = "";

        proginfo->programflags = proginfo->getProgramFlags(db);

        return proginfo;
    }

    return NULL;
}


// -1 for no data, 0 for no, 1 for weekdaily, 2 for weekly.
int ProgramInfo::IsProgramRecurring(void)
{
    QSqlDatabase *db = QSqlDatabase::database();
    QDateTime dtime = startts;

    int weekday = dtime.date().dayOfWeek();
    if (weekday < 6)
    {
        // week day    
        int daysadd = 1;
        if (weekday == 5)
            daysadd = 3;

        QDateTime checktime = dtime.addDays(daysadd);

        ProgramInfo *nextday = GetProgramAtDateTime(db, chanid, checktime);

        if (NULL == nextday)
            return -1;

        if (nextday && nextday->title == title)
        {
            delete nextday;
            return 1;
        }
        if (nextday)
            delete nextday;
    }

    QDateTime checktime = dtime.addDays(7);
    ProgramInfo *nextweek = GetProgramAtDateTime(db, chanid, checktime);

    if (NULL == nextweek)
        return -1;

    if (nextweek && nextweek->title == title)
    {
        delete nextweek;
        return 2;
    }

    if (nextweek)
        delete nextweek;
    return 0;
}

RecordingType ProgramInfo::GetProgramRecordingStatus(QSqlDatabase *db)
{
    if (record == NULL) 
    {
        record = new ScheduledRecording();
        record->loadByProgram(db, this);
    }

    return record->getRecordingType();
}

QString ProgramInfo::GetProgramRecordingProfile(QSqlDatabase *db)
{
    if (record == NULL)
    {
        record = new ScheduledRecording();
        record->loadByProgram(db, this);
    }

    return record->getProfileName();
}

bool ProgramInfo::AllowRecordingNewEpisodes(QSqlDatabase *db)
{
    if (record == NULL) 
    {
        record = new ScheduledRecording();
        record->loadByProgram(db, this);
    }

    int maxEpisodes = record->GetMaxEpisodes();

    if (!maxEpisodes || record->GetMaxNewest())
        return true;

    QString thequery;
    QString sqltitle = title;

    sqltitle.replace(QRegExp("\""), QString("\\\""));

    thequery = QString("SELECT count(*) FROM recorded WHERE title = \"%1\";")
                       .arg(sqltitle);

    QSqlQuery query = db->exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        if (query.value(0).toInt() >= maxEpisodes)
            return false;
    }

    return true;
}

int ProgramInfo::GetChannelRecPriority(QSqlDatabase *db, const QString &chanid)
{
    QString thequery;

    thequery = QString("SELECT recpriority FROM channel WHERE chanid = %1;")
                       .arg(chanid);

    QSqlQuery query = db->exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();
        return query.value(0).toInt();
    }

    return 0;
}

int ProgramInfo::GetRecordingTypeRecPriority(RecordingType type)
{
    switch (type)
    {
        case kSingleRecord:
            return gContext->GetNumSetting("SingleRecordRecPriority", 0);
        case kTimeslotRecord:
            return gContext->GetNumSetting("TimeslotRecordRecPriority", 0);
        case kWeekslotRecord:
            return gContext->GetNumSetting("WeekslotRecordRecPriority", 0);
        case kChannelRecord:
            return gContext->GetNumSetting("ChannelRecordRecPriority", 0);
        case kAllRecord:
            return gContext->GetNumSetting("AllRecordRecPriority", 0);
        default:
            return 0;
    }
}

// newstate uses same values as return of GetProgramRecordingState
void ProgramInfo::ApplyRecordStateChange(QSqlDatabase *db, 
                                         RecordingType newstate)
{
    GetProgramRecordingStatus(db);
    record->setRecordingType(newstate);
    record->save(db);
}

void ProgramInfo::ApplyRecordTimeChange(QSqlDatabase *db,
                                        const QDateTime &newstartts, 
                                        const QDateTime &newendts)
{
    GetProgramRecordingStatus(db);
    if (record->getRecordingType() != kNotRecording) 
    {
        record->setStart(newstartts);
        record->setEnd(newendts);
    }
}

void ProgramInfo::ApplyRecordRecPriorityChange(QSqlDatabase *db,
                                        const QString &newrecpriority)
{
    GetProgramRecordingStatus(db);
    record->setRecPriority(newrecpriority);
    record->save(db);
}

void ProgramInfo::ToggleRecord(QSqlDatabase *db)
{
    RecordingType curType = GetProgramRecordingStatus(db);

    switch (curType) 
    {
        case kNotRecording:
            ApplyRecordStateChange(db, kSingleRecord);
            break;
        case kSingleRecord:
            ApplyRecordStateChange(db, kTimeslotRecord);
            break;
        case kTimeslotRecord:
            ApplyRecordStateChange(db, kWeekslotRecord);
            break;
        case kWeekslotRecord:
            ApplyRecordStateChange(db, kChannelRecord);
            break;
        case kChannelRecord:
            ApplyRecordStateChange(db, kAllRecord);
            break;
        case kAllRecord:
        default:
            ApplyRecordStateChange(db, kNotRecording);
            break;
    }
}

bool ProgramInfo::IsSameProgram(const ProgramInfo& other) const
{
    if (title == other.title &&
        subtitle.length() > 2 &&
        description.length() > 2 &&
        subtitle == other.subtitle &&
        description == other.description)
        return true;
    else
        return false;
}
 
bool ProgramInfo::IsSameTimeslot(const ProgramInfo& other) const
{
    if (chanid == other.chanid &&
        startts == other.startts &&
        endts == other.endts &&
        sourceid == other.sourceid &&
        cardid == other.cardid &&
        inputid == other.inputid)
        return true;
    else
        return false;
}

bool ProgramInfo::IsSameProgramTimeslot(const ProgramInfo &other) const
{
    if (chanid == other.chanid &&
        startts < other.endts &&
        endts > other.startts &&
        (sourceid == -1 || other.sourceid == -1 ||
         sourceid == other.sourceid))
        return true;
    return false;
}

QString ProgramInfo::GetRecordBasename(void)
{
    QString starts = recstartts.toString("yyyyMMddhhmm");
    QString ends = recendts.toString("yyyyMMddhhmm");

    starts += "00";
    ends += "00";

    QString retval = QString("%1_%2_%3.nuv").arg(chanid)
                             .arg(starts).arg(ends);
    
    return retval;
}               

QString ProgramInfo::GetRecordFilename(const QString &prefix)
{
    QString starts = recstartts.toString("yyyyMMddhhmm");
    QString ends = recendts.toString("yyyyMMddhhmm");

    starts += "00";
    ends += "00";

    QString retval = QString("%1/%2_%3_%4.nuv").arg(prefix).arg(chanid)
                             .arg(starts).arg(ends);
    
    return retval;
}               

void ProgramInfo::StartedRecording(QSqlDatabase *db)
{
    if (!db)
        return;

    if (record == NULL) {
        record = new ScheduledRecording();
        record->loadByProgram(db, this);
    }

    QString starts = recstartts.toString("yyyyMMddhhmm");
    QString ends = recendts.toString("yyyyMMddhhmm");

    starts += "00";
    ends += "00";

    QString sqltitle = title;
    QString sqlsubtitle = subtitle;
    QString sqldescription = description;
    QString sqlcategory = category;

    sqltitle.replace(QRegExp("\""), QString("\\\""));
    sqlsubtitle.replace(QRegExp("\""), QString("\\\""));
    sqldescription.replace(QRegExp("\""), QString("\\\""));
    sqlcategory.replace(QRegExp("\""), QString("\\\""));

    QString query;
    query = QString("INSERT INTO recorded (chanid,starttime,endtime,title,"
                    "subtitle,description,hostname,category,autoexpire) "
                    "VALUES(%1,\"%2\",\"%3\",\"%4\",\"%5\",\"%6\",\"%7\","
                    "\"%8\",%9);")
                    .arg(chanid).arg(starts).arg(ends).arg(sqltitle.utf8()) 
                    .arg(sqlsubtitle.utf8()).arg(sqldescription.utf8())
                    .arg(gContext->GetHostName()).arg(sqlcategory.utf8())
                    .arg(record->GetAutoExpire());

    QSqlQuery qquery = db->exec(query);
    if (!qquery.isActive())
        MythContext::DBError("WriteRecordedToDB", qquery);

    query = QString("DELETE FROM recordedmarkup WHERE chanid = %1 "
                    "AND starttime = %2;")
                    .arg(chanid).arg(starts);

    qquery = db->exec(query);
    if (!qquery.isActive())
        MythContext::DBError("Clear markup on record", qquery);
}

void ProgramInfo::FinishedRecording(QSqlDatabase* db) 
{
    GetProgramRecordingStatus(db);
    record->doneRecording(db, *this);
}

void ProgramInfo::SetBookmark(long long pos, QSqlDatabase *db)
{
    MythContext::KickDatabase(db);

    char position[128];
    sprintf(position, "%lld", pos);

    QString starts = recstartts.toString("yyyyMMddhhmm");
    starts += "00";

    QString querystr;

    if (pos > 0)
        querystr = QString("UPDATE recorded SET bookmark = '%1', "
                           "starttime = '%2' WHERE chanid = '%3' AND "
                           "starttime = '%4';").arg(position).arg(starts)
                                               .arg(chanid).arg(starts);
    else
        querystr = QString("UPDATE recorded SET bookmark = NULL, "
                           "starttime = '%1' WHERE chanid = '%2' AND "
                           "starttime = '%3';").arg(starts).arg(chanid)
                                               .arg(starts);

    QSqlQuery query = db->exec(querystr);
    if (!query.isActive())
        MythContext::DBError("Save position update", querystr);
}

long long ProgramInfo::GetBookmark(QSqlDatabase *db)
{
    MythContext::KickDatabase(db);

    long long pos = 0;

    QString starts = recstartts.toString("yyyyMMddhhmm");
    starts += "00";

    QString querystr = QString("SELECT bookmark FROM recorded WHERE "
                               "chanid = '%1' AND starttime = '%2';")
                              .arg(chanid).arg(starts);

    QSqlQuery query = db->exec(querystr);
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        QString result = query.value(0).toString();
        if (result != QString::null)
        {
            sscanf(result.ascii(), "%lld", &pos);
        }
    }

    return pos;
}

bool ProgramInfo::IsEditing(QSqlDatabase *db)
{
    MythContext::KickDatabase(db);

    bool result = false;

    QString starts = recstartts.toString("yyyyMMddhhmm");
    starts += "00";

    QString querystr = QString("SELECT editing FROM recorded WHERE "
                               "chanid = '%1' AND starttime = '%2';")
                              .arg(chanid).arg(starts);

    QSqlQuery query = db->exec(querystr);
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        result = query.value(0).toInt();
    }

    return result;
}

void ProgramInfo::SetEditing(bool edit, QSqlDatabase *db)
{
    MythContext::KickDatabase(db);

    QString starts = recstartts.toString("yyyyMMddhhmm");
    starts += "00";

    QString querystr = QString("UPDATE recorded SET editing = '%1', "
                               "starttime = '%2' WHERE chanid = '%3' AND "
                               "starttime = '%4';").arg(edit).arg(starts)
                                                   .arg(chanid).arg(starts);
    QSqlQuery query = db->exec(querystr);
    if (!query.isActive())
        MythContext::DBError("Edit status update", querystr);
}

bool ProgramInfo::IsCommFlagged(QSqlDatabase *db)
{
    MythContext::KickDatabase(db);

    bool result = false;

    QString starts = recstartts.toString("yyyyMMddhhmm");
    starts += "00";

    QString querystr = QString("SELECT commflagged FROM recorded WHERE "
                               "chanid = '%1' AND starttime = '%2';")
                              .arg(chanid).arg(starts);

    QSqlQuery query = db->exec(querystr);
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        result = (query.value(0).toInt() == 1);
    }

    return result;
}

void ProgramInfo::SetCommFlagged(int flag, QSqlDatabase *db)
{
    MythContext::KickDatabase(db);

    QString starts = recstartts.toString("yyyyMMddhhmm");
    starts += "00";

    QString querystr = QString("UPDATE recorded SET commflagged = '%1', "
                               "starttime = '%2' WHERE chanid = '%3' AND "
                               "starttime = '%4';").arg(flag).arg(starts)
                                                   .arg(chanid).arg(starts);
    QSqlQuery query = db->exec(querystr);
    if (!query.isActive())
        MythContext::DBError("Commercial Flagged status update", querystr);
}

bool ProgramInfo::IsCommProcessing(QSqlDatabase *db)
{
    MythContext::KickDatabase(db);

    bool result = false;

    QString starts = recstartts.toString("yyyyMMddhhmm");
    starts += "00";

    QString querystr = QString("SELECT commflagged FROM recorded WHERE "
                               "chanid = '%1' AND starttime = '%2';")
                              .arg(chanid).arg(starts);

    QSqlQuery query = db->exec(querystr);
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        result = (query.value(0).toInt() == 2);
    }

    return result;
}

void ProgramInfo::SetAutoExpire(bool autoExpire, QSqlDatabase *db)
{
    MythContext::KickDatabase(db);

    QString starts = recstartts.toString("yyyyMMddhhmm");
    starts += "00";

    QString querystr = QString("UPDATE recorded SET autoexpire = '%1', "
                               "starttime = '%2' WHERE chanid = '%3' AND "
                               "starttime = '%4';").arg(autoExpire).arg(starts)
                                                   .arg(chanid).arg(starts);
    QSqlQuery query = db->exec(querystr);
    if (!query.isActive())
        MythContext::DBError("AutoExpire update", querystr);
}

bool ProgramInfo::GetAutoExpireFromRecorded(QSqlDatabase *db)
{
    MythContext::KickDatabase(db);

    QString starts = recstartts.toString("yyyyMMddhhmm");
    starts += "00";

    QString querystr = QString("SELECT autoexpire FROM recorded WHERE "
                               "chanid = '%1' AND starttime = '%2';")
                              .arg(chanid).arg(starts);

    bool result = false;
    QSqlQuery query = db->exec(querystr);
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        result = query.value(0).toInt();
    }

    return(result);
}

void ProgramInfo::GetCutList(QMap<long long, int> &delMap, QSqlDatabase *db)
{
//    GetMarkupMap(delMap, db, MARK_CUT_START);
//    GetMarkupMap(delMap, db, MARK_CUT_END, true);

    delMap.clear();

    MythContext::KickDatabase(db);

    QString starts = recstartts.toString("yyyyMMddhhmm");
    starts += "00";

    QString querystr = QString("SELECT cutlist FROM recorded WHERE "
                               "chanid = '%1' AND starttime = '%2';")
                              .arg(chanid).arg(starts);

    QSqlQuery query = db->exec(querystr);
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        QString cutlist = query.value(0).toString();

        if (cutlist.length() > 1)
        {
            QStringList strlist = QStringList::split("\n", cutlist);
            QStringList::Iterator i;

            for (i = strlist.begin(); i != strlist.end(); ++i)
            {
                long long start = 0, end = 0;
                if (sscanf((*i).ascii(), "%lld - %lld", &start, &end) != 2)
                {
                    cerr << "Malformed cutlist list: " << *i << endl;
                }
                else
                {
                    delMap[start] = 1;
                    delMap[end] = 0;
                }
            }
        }
    }
}

void ProgramInfo::SetCutList(QMap<long long, int> &delMap, QSqlDatabase *db)
{
//    ClearMarkupMap(db, MARK_CUT_START);
//    ClearMarkupMap(db, MARK_CUT_END);
//    SetMarkupMap(delMap, db);

    QString cutdata;
    char tempc[256];

    QMap<long long, int>::Iterator i;

    for (i = delMap.begin(); i != delMap.end(); ++i)
    {
        long long frame = i.key();
        int direction = i.data();

        if (direction == 1)
        {
            sprintf(tempc, "%lld - ", frame);
            cutdata += tempc;
        }
        else if (direction == 0)
        {
            sprintf(tempc, "%lld\n", frame);
            cutdata += tempc;
        }
    }

    MythContext::KickDatabase(db);

    QString starts = recstartts.toString("yyyyMMddhhmm");
    starts += "00";

    QString querystr = QString("UPDATE recorded SET cutlist = \"%1\", "
                               "starttime = '%2' WHERE chanid = '%3' AND "
                               "starttime = '%4';").arg(cutdata).arg(starts)
                                                   .arg(chanid).arg(starts);
    QSqlQuery query = db->exec(querystr);
    if (!query.isActive())
        MythContext::DBError("cutlist update", querystr);
}

void ProgramInfo::SetBlankFrameList(QMap<long long, int> &frames,
                                    QSqlDatabase *db, long long min_frame, 
                                    long long max_frame)
{
    ClearMarkupMap(db, MARK_BLANK_FRAME, min_frame, max_frame);
    SetMarkupMap(frames, db, MARK_BLANK_FRAME, min_frame, max_frame);
}

void ProgramInfo::GetBlankFrameList(QMap<long long, int> &frames,
                                    QSqlDatabase *db)
{
    GetMarkupMap(frames, db, MARK_BLANK_FRAME);
}

void ProgramInfo::SetCommBreakList(QMap<long long, int> &frames,
                                   QSqlDatabase *db)
{
    ClearMarkupMap(db, MARK_COMM_START);
    ClearMarkupMap(db, MARK_COMM_END);
    SetMarkupMap(frames, db);
}

void ProgramInfo::GetCommBreakList(QMap<long long, int> &frames,
                                   QSqlDatabase *db)
{
    GetMarkupMap(frames, db, MARK_COMM_START);
    GetMarkupMap(frames, db, MARK_COMM_END, true);
}

void ProgramInfo::ClearMarkupMap(QSqlDatabase *db, int type,
                                 long long min_frame, long long max_frame)
{
    QString starts = recstartts.toString("yyyyMMddhhmm");
    starts += "00";

    QString min_comp = " ";
    QString max_comp = " ";

    if (min_frame >= 0)
    {
        char tempc[128];
        sprintf(tempc, "AND mark >= %lld", min_frame);
        min_comp += tempc;
    }

    if (max_frame >= 0)
    {
        char tempc[128];
        sprintf(tempc, "AND mark <= %lld", max_frame);
        max_comp += tempc;
    }

    QString querystr = QString("DELETE FROM recordedmarkup "
                       "WHERE chanid = '%1' AND starttime = '%2' %3 %4 ")
                       .arg(chanid).arg(starts).arg(min_comp).arg(max_comp);

    if (type != -100)
        querystr += QString("AND type = %1;").arg(type);
    else
        querystr += QString(";");

    QSqlQuery query = db->exec(querystr);
    if (!query.isActive())
        MythContext::DBError("ClearMarkupMap deleting", querystr);
}

void ProgramInfo::SetMarkupMap(QMap<long long, int> &marks, QSqlDatabase *db,
                               int type, long long min_frame, 
                               long long max_frame)
{
    QMap<long long, int>::Iterator i;

    QString starts = recstartts.toString("yyyyMMddhhmm");
    starts += "00";

    QString querystr;

    // check to make sure the show still exists before saving markups
    querystr = QString("SELECT starttime FROM recorded "
                       "WHERE chanid = '%1' AND starttime = '%2';")
                       .arg(chanid).arg(starts);

    QSqlQuery check_query = db->exec(querystr);
    if (!check_query.isActive())
        MythContext::DBError("SetMarkupMap checking record table",
            querystr);

    if (check_query.isActive())
        if ((check_query.numRowsAffected() == 0) ||
            (!check_query.next()))
            return;

    for (i = marks.begin(); i != marks.end(); ++i)
    {
        long long frame = i.key();
        int mark_type;
        QString querystr;
        QString frame_str;
        char tempc[128];
        sprintf(tempc, "%lld", frame );
        frame_str += tempc;
       
        if ((min_frame >= 0) && (frame < min_frame))
            continue;

        if ((max_frame >= 0) && (frame > max_frame))
            continue;

        if (type != -100)
            mark_type = type;
        else
            mark_type = i.data();

        querystr = QString("INSERT INTO recordedmarkup (chanid, starttime, "
                           "mark, type) values ( '%1', '%2', %3, %4);")
                           .arg(chanid).arg(starts)
                           .arg(frame_str).arg(mark_type);
        QSqlQuery query = db->exec(querystr);
        if (!query.isActive())
            MythContext::DBError("SetMarkupMap inserting", querystr);
    }
}

void ProgramInfo::GetMarkupMap(QMap<long long, int> &marks, QSqlDatabase *db,
                               int type, bool mergeIntoMap)
{
    if (!mergeIntoMap)
        marks.clear();

    MythContext::KickDatabase(db);

    QString starts = recstartts.toString("yyyyMMddhhmm");
    starts += "00";

    QString querystr = QString("SELECT mark, type FROM recordedmarkup WHERE "
                               "chanid = '%1' AND starttime = '%2' "
                               "AND type = %3 "
                               "ORDER BY mark;")
                               .arg(chanid).arg(starts)
                               .arg(type);

    QSqlQuery query = db->exec(querystr);
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        while(query.next())
            marks[query.value(0).toInt()] = query.value(1).toInt();
    }
}

bool ProgramInfo::CheckMarkupFlag(int type, QSqlDatabase *db)
{
    QMap<long long, int> flagMap;

    GetMarkupMap(flagMap, db, type);

    return(flagMap.contains(0));
}

void ProgramInfo::SetMarkupFlag(int type, bool flag, QSqlDatabase *db)
{
    ClearMarkupMap(db, type);

    if (flag)
    {
        QMap<long long, int> flagMap;

        flagMap[0] = type;
        SetMarkupMap(flagMap, db, type);
    }
}

void ProgramInfo::GetPositionMap(QMap<long long, long long> &posMap, int type,
                                 QSqlDatabase *db)
{
    posMap.clear();

    MythContext::KickDatabase(db);

    QString starts = recstartts.toString("yyyyMMddhhmm");
    starts += "00";

    QString querystr = QString("SELECT mark, offset FROM recordedmarkup "
                               "WHERE chanid = '%1' AND starttime = '%2' "
                               "AND type = %3;")
                               .arg(chanid).arg(starts).arg(type);

    QSqlQuery query = db->exec(querystr);
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        while (query.next())
        {
            QString val = query.value(1).toString();
            if (val != QString::null)
            {
                long long offset = 0;
                sscanf(val.ascii(), "%lld", &offset);

                posMap[query.value(0).toInt()] = offset;
            }
        }
    }
}

void ProgramInfo::SetPositionMap(QMap<long long, long long> &posMap, int type,
                                 QSqlDatabase *db, long long min_frame,
                                 long long max_frame)
{
    QMap<long long, long long>::Iterator i;

    QString starts = recstartts.toString("yyyyMMddhhmm");
    starts += "00";

    QString min_comp = " ";
    QString max_comp = " ";

    if (min_frame >= 0)
    {
        char tempc[128];
        sprintf(tempc, "AND mark >= %lld", min_frame);
        min_comp += tempc;
    }

    if (max_frame >= 0)
    {
        char tempc[128];
        sprintf(tempc, "AND mark <= %lld", max_frame);
        max_comp += tempc;
    }

    QString querystr = QString("DELETE FROM recordedmarkup "
                               "WHERE chanid = '%1' AND starttime = '%2' "
                               "AND type = %3 %4 %5;")
                               .arg(chanid).arg(starts).arg(type)
                               .arg(min_comp).arg(max_comp);
    QSqlQuery query = db->exec(querystr);
    if (!query.isActive())
        MythContext::DBError("position map clear", querystr);

    for (i = posMap.begin(); i != posMap.end(); ++i)
    {
        long long frame = i.key();
        char tempc[128];
        sprintf(tempc, "%lld", frame);

        if ((min_frame >= 0) && (frame < min_frame))
            continue;

        if ((max_frame >= 0) && (frame > max_frame))
            continue;

        QString frame_str = tempc;

        long long offset = i.data();
        sprintf(tempc, "%lld", offset);
       
        QString offset_str = tempc;

        querystr = QString("INSERT INTO recordedmarkup (chanid, starttime, "
                           "mark, type, offset) values "
                           "( '%1', '%2', %3, %4, \"%5\");")
                           .arg(chanid).arg(starts)
                           .arg(frame_str).arg(type)
                           .arg(offset_str);

        QSqlQuery subquery = db->exec(querystr);
        if (!subquery.isActive())
            MythContext::DBError("position map insert", querystr);
    }
}

void ProgramInfo::DeleteHistory(QSqlDatabase *db)
{
    GetProgramRecordingStatus(db);
    record->forgetHistory(db, *this);
}

void ProgramInfo::setOverride(QSqlDatabase *db, int override)
{
    QString sqltitle = title;
    sqltitle.replace(QRegExp("\'"), "\\'");
    sqltitle.replace(QRegExp("\""), "\\\"");
    QString sqlsubtitle = subtitle;
    sqlsubtitle.replace(QRegExp("\'"), "\\'");
    sqlsubtitle.replace(QRegExp("\""), "\\\"");
    QString sqldescription = description;
    sqldescription.replace(QRegExp("\'"), "\\'");
    sqldescription.replace(QRegExp("\""), "\\\"");

    QString thequery;
    QSqlQuery query;

    thequery = QString("DELETE FROM recordoverride WHERE "
       "recordid = %2 AND chanid = %3 AND starttime = %4 AND endtime = %5 AND "
       "title = \"%6\" AND subtitle = \"%7\" AND description = \"%8\";")
        .arg(recordid).arg(chanid)
        .arg(startts.toString("yyyyMMddhhmmss").ascii())
        .arg(endts.toString("yyyyMMddhhmmss").ascii())
        .arg(sqltitle.utf8()).arg(sqlsubtitle.utf8())
        .arg(sqldescription.utf8());
    query = db->exec(thequery);

    if (!query.isActive())
        MythContext::DBError("record override update", thequery);
    else
    {
        thequery = QString("INSERT INTO recordoverride SET type = %1, "
            "recordid = %2, chanid = %3, starttime = %4, endtime = %5, "
            "title = \"%6\", subtitle = \"%7\", description = \"%8\";")
            .arg(override).arg(recordid).arg(chanid)
            .arg(startts.toString("yyyyMMddhhmmss").ascii())
            .arg(endts.toString("yyyyMMddhhmmss").ascii())
            .arg(sqltitle.utf8()).arg(sqlsubtitle.utf8())
            .arg(sqldescription.utf8());
        query = db->exec(thequery);
        if (!query.isActive())
            MythContext::DBError("record override update", thequery);
    }
}

QString ProgramInfo::RecTypeChar(void)
{
    QString recstring = "";

    switch (rectype)
    {
    case kSingleRecord:
        recstring = QObject::tr("R");
        break;
    case kTimeslotRecord:
        recstring = QObject::tr("T");
        break;
    case kWeekslotRecord:
        recstring = QObject::tr("W");
        break;
    case kChannelRecord:
        recstring = QObject::tr("C");
        break;
    case kAllRecord:
        recstring = QObject::tr("A");
        break;
    case kNotRecording:
    default:
        recstring = QObject::tr("");
        break;
    }

    return recstring;
}

QString ProgramInfo::RecTypeText(void)
{
    QString recstring;

    switch (rectype)
    {
    case kSingleRecord:
        recstring = QObject::tr("Single Recording");
        break;
    case kTimeslotRecord:
        recstring = QObject::tr("Daily Recording");
        break;
    case kWeekslotRecord:
        recstring = QObject::tr("Weekly Recording");
        break;
    case kChannelRecord:
        recstring = QObject::tr("Channel Recording");
        break;
    case kAllRecord:
        recstring = QObject::tr("All Recording");
        break;
    default:
        recstring = QObject::tr("Not Recording");
        break;
    }

    return recstring;
}

QString ProgramInfo::RecStatusChar(void)
{
    switch (recstatus)
    {
    case rsDeleted:
        return "D";
    case rsStopped:
        return "S";
    case rsWillRecord:
        return QString::number(cardid);
    case rsRecording:
        return QString::number(cardid);
    case rsManualOverride:
        return "X";
    case rsPreviousRecording:
        return "P";
    case rsCurrentRecording:
        return "C";
    case rsOtherShowing:
        return "O";
    case rsTooManyRecordings:
        return "T";
    case rsCancelled:
        return "N";
    case rsLowerRecPriority:
        return "R";
    case rsManualConflict:
        return "M";
    case rsAutoConflict:
        return "A";
    case rsOverlap:
        return "V";
    case rsLowDiskSpace:
        return "K";
    case rsTunerBusy:
        return "B";
    default:
        return "-";
    }
}

QString ProgramInfo::RecStatusText(void)
{
    if (rectype == kNotRecording)
        return QObject::tr("Not Recording");
    else if (conflicting)
        return QObject::tr("Conflicting");
    else
    {
        switch (recstatus)
        {
        case rsDeleted:
            return QObject::tr("Deleted");
        case rsStopped:
            return QObject::tr("Stopped");
        case rsRecorded:
            return QObject::tr("Recorded");
        case rsRecording:
            return QObject::tr("Recording");
        case rsWillRecord:
            return QObject::tr("Will Record");
        case rsManualOverride:
            return QObject::tr("Manual Override");
        case rsPreviousRecording:
            return QObject::tr("Previous Recording");
        case rsCurrentRecording:
            return QObject::tr("Current Recording");
        case rsOtherShowing:
            return QObject::tr("Other Showing");
        case rsTooManyRecordings:
            return QObject::tr("Max Recordings");
        case rsCancelled:
            return QObject::tr("Manual Cancel");
        case rsLowerRecPriority:
            return QObject::tr("Low Priority");
        case rsManualConflict:
            return QObject::tr("Manual Conflict");
        case rsAutoConflict:
            return QObject::tr("Auto Conflict");
        case rsOverlap:
            return QObject::tr("Overlap");
        case rsLowDiskSpace:
            return QObject::tr("Low Disk Space");
        case rsTunerBusy:
            return QObject::tr("Tuner Busy");
        default:
            return QObject::tr("Unknown");
        }
    }

    return QObject::tr("Unknown");;
}

QString ProgramInfo::RecStatusDesc(void)
{
    QString message;
    QDateTime now = QDateTime::currentDateTime();

    if (conflicting)
        message += QObject::tr("This showing conflicts with one or more other "
                               "scheduled programs.");
    else if (recording)
    {
        switch (recstatus)
        {
        case rsWillRecord:
            message = QObject::tr("This showing will be recorded.");
            break;
        case rsRecording:
            message = QObject::tr("This showing is being recorded.");
            break;
        case rsRecorded:
            message = QObject::tr("This showing was recorded.");
            break;
        case rsStopped:
            message = QObject::tr("This showing was recorded but was stopped "
                                   "before recording was completed.");
            break;
        case rsDeleted:
            message = QObject::tr("This showing was recorded but was deleted "
                                   "before recording was completed.");
            break;
        default:
            message = QObject::tr("The status of this showing is unknown.");
            break;
        }
    }
    else
    {
        if (recstartts > now)
            message = QObject::tr("This showing will not be recorded because ");
        else
            message = QObject::tr("This showing was not recorded because ");

        switch (recstatus)
        {
        case rsManualOverride:
            message += QObject::tr("it was manually set to not record.");
            break;
        case rsPreviousRecording:
            message += QObject::tr("this episode was previously recorded "
                                   "according to the duplicate policy chosen "
                                   "for this title.");
            break;
        case rsCurrentRecording:
            message += QObject::tr("this episode was previously recorded and "
                                   "is still available in the list of "
                                   "recordings.");
            break;
        case rsOtherShowing:
            message += QObject::tr("this episode will be recorded at another "
                                   "time instead.");
            break;
        case rsTooManyRecordings:
            message += QObject::tr("too many recordings of this program have "
                                   "already been recorded.");
            break;
        case rsCancelled:
            message += QObject::tr("it was manually cancelled.");
            break;
        case rsLowerRecPriority:
            message += QObject::tr("another program with a higher recording "
                                   "priority will be recorded.");
            break;
        case rsManualConflict:
            message += QObject::tr("another program was manually chosen to be "
                                    "recorded instead.");
            break;
        case rsAutoConflict:
            message += QObject::tr("another program was automatically chosen "
                                   "to be recorded instead.");
            break;
        case rsOverlap:
            message += QObject::tr("it is covered by another scheduled "
                                   "recording for the same program.");
            break;
        case rsLowDiskSpace:
            message += QObject::tr("there wasn't enough disk space available.");
            break;
        case rsTunerBusy:
            message += QObject::tr("the tuner card was already being used.");
            break;
        default:
            message += QObject::tr("you should never see this.");
            break;
        }
    }

    return message;
}

void ProgramInfo::FillInRecordInfo(vector<ProgramInfo *> &reclist)
{
    vector<ProgramInfo *>::iterator i;
    ProgramInfo *found = NULL;
    int pfound = 0;

    for (i = reclist.begin(); i != reclist.end(); i++)
    {
        ProgramInfo *p = (*i);
        if (chanid == p->chanid && 
            startts == p->startts &&
            endts == p->endts && 
            title == p->title &&
            subtitle == p->subtitle && 
            description == p->description)
        {
            int pp = RecTypePriority(p->rectype);
            if (!found || pp < pfound || 
                (pp == pfound && p->recordid < found->recordid))
            {
                found = p;
                pfound = pp;
            }
        }
    }
                
    if (found)
    {
        conflicting = found->conflicting;
        recording = found->recording;
        override = found->override;
        recstatus = found->recstatus;
        recordid = found->recordid;
        rectype = found->rectype;
        recdups = found->recdups;
        recstartts = found->recstartts;
        recendts = found->recendts;
    }
}

void ProgramInfo::Save(QSqlDatabase *db)
{
    QSqlQuery query(QString::null, db);

    query.prepare("REPLACE INTO program (chanid,starttime,endtime,"
                  "title,subtitle,description,category,airdate,"
                  "stars) VALUES(:CHANID,:STARTTIME,:ENDTIME,:TITLE,"
                  ":SUBTITLE,:DESCRIPTION,:CATEGORY,:AIRDATE,:STARS);");
    query.bindValue(":CHANID", chanid.toInt());
    query.bindValue(":STARTTIME", startts.toString("yyyyMMddhhmmss"));
    query.bindValue(":ENDTIME", endts.toString("yyyyMMddhhmmss"));
    query.bindValue(":TITLE", title.utf8());
    query.bindValue(":SUBTITLE", subtitle.utf8());
    query.bindValue(":DESCRIPTION", description.utf8());
    query.bindValue(":CATEGORY", category.utf8());
    query.bindValue(":AIRDATE", "0");
    query.bindValue(":STARS", "0");

    if (!query.exec())
        MythContext::DBError("Saving program", query);
}

QGridLayout* ProgramInfo::DisplayWidget(QWidget *parent)
{
    int row = 0;
    float wmult, hmult;
    int screenwidth, screenheight;
    gContext->GetScreenSettings(screenwidth, wmult, screenheight, hmult);

    QGridLayout *grid = new QGridLayout(4, 2, (int)(10*wmult));
    
    QLabel *titlefield = new QLabel(title, parent);
    titlefield->setBackgroundOrigin(QWidget::WindowOrigin);
    titlefield->setFont(gContext->GetBigFont());

    QString dateFormat = gContext->GetSetting("DateFormat", "ddd MMMM d");
    QString timeFormat = gContext->GetSetting("TimeFormat", "h:mm AP");
    
    QString airDateText = startts.date().toString(dateFormat) + QString(", ") +
                          startts.time().toString(timeFormat) + QString(" - ") +
                          endts.time().toString(timeFormat);
    QLabel *airDateLabel = new QLabel(QObject::tr("Air Date:"), parent);
    airDateLabel->setBackgroundOrigin(QWidget::WindowOrigin);
    QLabel* airDateField = new QLabel(airDateText,parent);
    airDateField->setBackgroundOrigin(QWidget::WindowOrigin);
    QLabel *subtitlelabel = new QLabel(QObject::tr("Episode:"), parent);
    subtitlelabel->setBackgroundOrigin(QWidget::WindowOrigin);
    QLabel *subtitlefield = new QLabel(subtitle, parent);
    subtitlefield->setBackgroundOrigin(QWidget::WindowOrigin);
    subtitlefield->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    QLabel *descriptionlabel = new QLabel(QObject::tr("Description:"), parent);
    descriptionlabel->setBackgroundOrigin(QWidget::WindowOrigin);
    QLabel *descriptionfield = new QLabel(description, parent);
    descriptionfield->setBackgroundOrigin(QWidget::WindowOrigin);
    descriptionfield->setAlignment(Qt::WordBreak | Qt::AlignLeft | 
                                   Qt::AlignTop);

    descriptionlabel->polish();

    int descwidth = (int)(740 * wmult) - descriptionlabel->width();
    int titlewidth = (int)(760 * wmult);

    titlefield->setMinimumWidth(titlewidth);
    titlefield->setMaximumWidth(titlewidth);

    subtitlefield->setMinimumWidth(descwidth);
    subtitlefield->setMaximumWidth(descwidth);

    descriptionfield->setMinimumWidth(descwidth);
    descriptionfield->setMaximumWidth(descwidth);

    grid->addMultiCellWidget(titlefield, row, 0, 0, 1, Qt::AlignLeft);
    row++;
    grid->addWidget(airDateLabel, row, 0, Qt::AlignLeft | Qt::AlignTop);
    grid->addWidget(airDateField, row, 1, Qt::AlignLeft);
    row++;
    grid->addWidget(subtitlelabel, row, 0, Qt::AlignLeft | Qt::AlignTop);
    grid->addWidget(subtitlefield, row, 1, Qt::AlignLeft | Qt::AlignTop);
    row++;
    grid->addWidget(descriptionlabel, row, 0, Qt::AlignLeft | Qt::AlignTop);
    grid->addWidget(descriptionfield, row, 1, Qt::AlignLeft | Qt::AlignTop);

    grid->setColStretch(1, 2);
    grid->setRowStretch(row, 1);

    return grid;
}

void ProgramInfo::EditRecording(QSqlDatabase *db)
{
    MythContext::KickDatabase(db);

    if (recordid == 0)
        EditScheduled(db);
    else if (conflicting)
        handleConflicting(db);
    else if (recording)
        handleRecording(db);
    else
        handleNotRecording(db);

    ScheduledRecording::signalChange(db);
}

void ProgramInfo::EditScheduled(QSqlDatabase *db)
{
    MythContext::KickDatabase(db);

    if (gContext->GetNumSetting("AdvancedRecord", 0))
    {
        GetProgramRecordingStatus(db);
        record->loadByProgram(db, this);
        record->exec(db);
    }
    else
    {
        InfoDialog diag(this, gContext->GetMainWindow(), "Program Info");
        diag.exec();
    }

    ScheduledRecording::signalChange(db);
}

int ProgramInfo::getProgramFlags(QSqlDatabase *db)
{
    int flags = 0;

    MythContext::KickDatabase(db);
        
    QString starts = recstartts.toString("yyyyMMddhhmm");
    starts += "00";
    
    QString querystr = QString("SELECT commflagged, cutlist, autoexpire, "
                               "editing, bookmark FROM recorded WHERE "
                               "chanid = '%1' AND starttime = '%2';")
                              .arg(chanid).arg(starts);
    
    QSqlQuery query = db->exec(querystr);
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        flags |= (query.value(0).toInt() == 1) ? FL_COMMFLAG : 0;
        flags |= query.value(1).toString().length() > 1 ? FL_CUTLIST : 0;
        flags |= query.value(2).toInt() ? FL_AUTOEXP : 0;
        if (query.value(3).toInt() || (query.value(0).toInt() == 2))
            flags |= FL_EDITING;
        flags |= query.value(4).toString().length() > 1 ? FL_BOOKMARK : 0;
    }

    return flags;
}

void ProgramInfo::handleRecording(QSqlDatabase *db)
{
    QString message = title;

    if (subtitle != "")
        message += QString(" - \"%1\"").arg(subtitle);

    message += "\n\n";
    message += RecStatusDesc();

    DialogBox diag(gContext->GetMainWindow(), message);
    diag.AddButton(QObject::tr("OK"));

    if (recstatus == rsWillRecord)
    {
        diag.AddButton(QObject::tr("Don't record it"));
        if (subtitle != "" && description != "")
            diag.AddButton(QObject::tr("Never record this episode"));
    }

    int ret = diag.exec();

    if (ret <= 1)
      return;

    setOverride(db, 2);

    if (ret <= 2)
      return;

    if (record == NULL) 
    {
        record = new ScheduledRecording();
        record->loadByProgram(db, this);
    }
    record->doneRecording(db, *this);
}

void ProgramInfo::handleNotRecording(QSqlDatabase *db)
{
    QString message = title;

    if (subtitle != "")
        message += QString(" - \"%1\"").arg(subtitle);

    message += "\n\n";
    message += RecStatusDesc();

    DialogBox diag(gContext->GetMainWindow(), QObject::tr(message));
    diag.AddButton(QObject::tr("OK"));

    QDateTime now = QDateTime::currentDateTime();

    if (recstartts > now &&
        recstatus != rsTooManyRecordings &&
        recstatus != rsCancelled &&
        recstatus != rsLowerRecPriority &&
        recstatus != rsOverlap &&
        recstatus != rsLowDiskSpace &&
        recstatus != rsTunerBusy)
        diag.AddButton(QObject::tr("Record it anyway"));

    int ret = diag.exec();

    if (ret <= 1)
        return;

    if (recstatus != rsManualConflict &&
        recstatus != rsAutoConflict)
    {
        setOverride(db, 1);
        return;
    }

    QString pstart = startts.toString("yyyyMMddhhmm00");
    QString pend = endts.toString("yyyyMMddhhmm00");

    QString thequery;
    thequery = QString("INSERT INTO conflictresolutionoverride (chanid,"
                       "starttime, endtime) VALUES (%1, %2, %3);")
                       .arg(chanid).arg(pstart).arg(pend);

    QSqlQuery qquery = db->exec(thequery);
    if (!qquery.isActive())
    {
        cerr << "DB Error: conflict resolution insertion failed, SQL query "
             << "was:" << endl;
        cerr << thequery << endl;
    }

    vector<ProgramInfo *> *conflictlist = RemoteGetConflictList(this, false);

    if (!conflictlist)
        return;

    QString dstart, dend;
    vector<ProgramInfo *>::iterator i;
    for (i = conflictlist->begin(); i != conflictlist->end(); i++)
    {
        dstart = (*i)->startts.toString("yyyyMMddhhmm00");
        dend = (*i)->endts.toString("yyyyMMddhhmm00");

        thequery = QString("DELETE FROM conflictresolutionoverride WHERE "
                           "chanid = %1 AND starttime = %2 AND "
                           "endtime = %3;")
                           .arg((*i)->chanid).arg(dstart).arg(dend);

        qquery = db->exec(thequery);
        if (!qquery.isActive())
        {
            cerr << "DB Error: conflict resolution deletion failed, SQL query "
                 << "was:" << endl;
            cerr << thequery << endl;
        }
    }

    vector<ProgramInfo *>::iterator iter = conflictlist->begin();
    for (; iter != conflictlist->end(); iter++)
        delete (*iter);
    delete conflictlist;
}

void ProgramInfo::handleConflicting(QSqlDatabase *db)
{
    vector<ProgramInfo *> *conflictlist = RemoteGetConflictList(this, true);

    if (!conflictlist)
        return;

    QString message = QObject::tr("The following scheduled recordings conflict "
                         "with each other.  Which would you like to record?");

    DialogBox diag(gContext->GetMainWindow(), message, 
                   QObject::tr("Remember this choice and use it automatically "
                      "in the future"));
 
    QString dateformat = gContext->GetSetting("DateFormat", "ddd MMMM d");
    QString timeformat = gContext->GetSetting("TimeFormat", "h:mm AP");

    QString button; 
    button = title + QString("\n");
    button += recstartts.toString(dateformat + " " + timeformat);
    if (gContext->GetNumSetting("DisplayChanNum") != 0)
        button += " on " + channame + " [" + chansign + "]";
    else
        button += QString(" on channel ") + chanstr;

    diag.AddButton(button);

    vector<ProgramInfo *>::iterator i = conflictlist->begin();
    for (; i != conflictlist->end(); i++)
    {
        ProgramInfo *info = (*i);

        button = info->title + QString("\n");
        button += info->recstartts.toString(dateformat + " " + timeformat);
        if (gContext->GetNumSetting("DisplayChanNum") != 0)
            button += " on " + info->channame + " [" + info->chansign + "]";
        else
            button += QString(" on channel ") + info->chanstr;

        diag.AddButton(button);
    }

    int ret = diag.exec();
    int boxstatus = diag.getCheckBoxState();

    if (ret == 0)
    {
        vector<ProgramInfo *>::iterator iter = conflictlist->begin();
        for (; iter != conflictlist->end(); iter++)
        {
            delete (*iter);
        }

        delete conflictlist;
        return;
    }

    ProgramInfo *prefer = NULL;
    vector<ProgramInfo *> *dislike = new vector<ProgramInfo *>;
    if (ret == 2)
    {
        prefer = this;
        for (i = conflictlist->begin(); i != conflictlist->end(); i++)
            dislike->push_back(*i);
    }
    else
    {
        dislike->push_back(this);
        int counter = 3;
        for (i = conflictlist->begin(); i != conflictlist->end(); i++) 
        {
            if (counter == ret)
                prefer = (*i);
            else
                dislike->push_back(*i);
            counter++;
        }
    }

    if (!prefer)
    {
        printf("Ack, no preferred recording\n");
        delete dislike;
        vector<ProgramInfo *>::iterator iter = conflictlist->begin();
        for (; iter != conflictlist->end(); iter++)
        {
            delete (*iter);
        }

        delete conflictlist;
        return;
    }

    QString thequery;

    if (boxstatus == 1)
    {
        for (i = dislike->begin(); i != dislike->end(); i++)
        {
            QString sqltitle1 = prefer->title;
            QString sqltitle2 = (*i)->title;

            sqltitle1.replace(QRegExp("\""), QString("\\\""));
            sqltitle2.replace(QRegExp("\""), QString("\\\""));

            thequery = QString("INSERT INTO conflictresolutionany "
                               "(prefertitle, disliketitle) VALUES "
                               "(\"%1\", \"%2\");").arg(sqltitle1.utf8())
                               .arg(sqltitle2.utf8());
            QSqlQuery qquery = db->exec(thequery);
            if (!qquery.isActive())
            {
                cerr << "DB Error: conflict resolution insertion failed, SQL "
                     << "query was:" << endl;
                cerr << thequery << endl;
            }
        }
    } 
    else
    {
        QString pstart = prefer->startts.toString("yyyyMMddhhmm00");
        QString pend = prefer->endts.toString("yyyyMMddhhmm00");

        QString dstart, dend;

        for (i = dislike->begin(); i != dislike->end(); i++)
        {
            dstart = (*i)->startts.toString("yyyyMMddhhmm00");
            dend = (*i)->endts.toString("yyyyMMddhhmm00");

            thequery = QString("INSERT INTO conflictresolutionsingle "
                               "(preferchanid, preferstarttime, "
                               "preferendtime, dislikechanid, "
                               "dislikestarttime, dislikeendtime) VALUES "
                               "(%1, %2, %3, %4, %5, %6);") 
                               .arg(prefer->chanid).arg(pstart).arg(pend)
                               .arg((*i)->chanid).arg(dstart).arg(dend);

            QSqlQuery qquery = db->exec(thequery);
            if (!qquery.isActive())
            {
                cerr << "DB Error: conflict resolution insertion failed, SQL "
                     << "query was:" << endl;
                cerr << thequery << endl;
            }
        }
    }  

    QString dstart, dend;
    for (i = dislike->begin(); i != dislike->end(); i++)
    {
        dstart = (*i)->startts.toString("yyyyMMddhhmm00");
        dend = (*i)->endts.toString("yyyyMMddhhmm00");

        thequery = QString("DELETE FROM conflictresolutionoverride WHERE "
                           "chanid = %1 AND starttime = %2 AND "
                           "endtime = %3;").arg((*i)->chanid).arg(dstart)
                           .arg(dend);

        QSqlQuery qquery = db->exec(thequery);
        if (!qquery.isActive())
        {
            cerr << "DB Error: conflict resolution deletion failed, SQL query "
                 << "was:" << endl;
            cerr << thequery << endl;
        }
    }

    delete dislike;
    vector<ProgramInfo *>::iterator iter = conflictlist->begin();
    for (; iter != conflictlist->end(); iter++)
        delete (*iter);
    delete conflictlist;
}

PGInfoCon::PGInfoCon()
{
    init();
}

PGInfoCon::~PGInfoCon() 
{
    init();
}

void PGInfoCon::updateAll(const vector<ProgramInfo *> &pginfoList)
{
    ProgramInfo *tmp = allData.at(selectedPos);
    ProgramInfo org;

    if (tmp)
        org = *tmp;
    else
        selectedPos = -1;

    allData.clear();
    vector<ProgramInfo *>::const_iterator i;
    for (i = pginfoList.begin(); i != pginfoList.end(); i++)
        allData.append(*i);

    if (selectedPos >= 0)
    {
        tmp = allData.at(selectedPos);
        if (tmp && tmp->MakeUniqueKey() == org.MakeUniqueKey())
            ;
        else
        {
            tmp = find(org);
            if (!tmp)
                tmp = findNearest(org);

            if (tmp)
                selectedPos = allData.find(tmp);
        }
    }

    checkSetIndex();
}

bool PGInfoCon::update(const ProgramInfo &pginfo)
{
    ProgramInfo *oldpginfo = find(pginfo);

    if (oldpginfo)
    {
        *oldpginfo = pginfo;
        return true;
    }    

    return false;    
}

int PGInfoCon::count()
{
    return allData.count();
}

void PGInfoCon::clear()
{
    init();
}

PGInfoCon::PGInfoConIt PGInfoCon::iterator()
{
    PGInfoConIt it(allData);
    return it;
}

int PGInfoCon::selectedIndex()
{
    return selectedPos;
}

void PGInfoCon::setSelected(int delta)
{
    selectedPos += delta;
    checkSetIndex();        
}

bool PGInfoCon::getSelected(ProgramInfo &pginfo)
{
    ProgramInfo *current = allData.at(selectedPos);
    if (current)
    {
        pginfo = *current;
        return true;
    }    

    return false;    
}

void PGInfoCon::init()
{
    selectedPos = -1;
    allData.setAutoDelete(true);
    allData.clear();
}

void PGInfoCon::checkSetIndex()
{
    if (selectedPos < 0)
        selectedPos = 0;
    else if (selectedPos > count() - 1)
        selectedPos = count() - 1;

    if (count() == 0)
        selectedPos = -1;
}

ProgramInfo *PGInfoCon::find(const ProgramInfo &pginfo)
{
    QString key(pginfo.MakeUniqueKey());
    return findFromKey(key);
}

ProgramInfo *PGInfoCon::findFromKey(const QString &key)
{
    if (key == "")
        return NULL;

    ProgramInfo *pginfo;
    for (PGInfoConIt it(allData); (pginfo = it.current()) != 0; ++it)
    {
        if (pginfo->MakeUniqueKey() == key)
            return pginfo;
    }        

    return NULL;                
}

ProgramInfo *PGInfoCon::findNearest(const ProgramInfo &pginfo)
{
    ProgramInfo *tmppginfo;
    for (PGInfoConIt it(allData); (tmppginfo = it.current()) != 0; ++it)
    {
        if (tmppginfo->recstartts >= pginfo.recstartts)
            return tmppginfo;
    }
    return allData.last();
}

