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
    norecord = nrUnknown;
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
    norecord = other.norecord;
    recordid = other.recordid;
    rectype = other.rectype;
    recdups = other.recdups;

    sourceid = other.sourceid;
    inputid = other.inputid;
    cardid = other.cardid;
    schedulerid = other.schedulerid;
    recpriority = other.recpriority;

    repeat = other.repeat;

    record = NULL;
}

ProgramInfo::~ProgramInfo() 
{
    if (record != NULL)
        delete record;
}

QString ProgramInfo::MakeUniqueKey(void)
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
    list << QString::number(norecord);
    list << QString::number(recordid);
    list << QString::number(rectype);
    list << QString::number(recdups);
    list << recstartts.toString(Qt::ISODate);
    list << recendts.toString(Qt::ISODate);
    list << QString::number(repeat);
}

void ProgramInfo::FromStringList(QStringList &list, int offset)
{
    if (offset + NUMPROGRAMLINES > (int)list.size())
    {
        cerr << "offset is: " << offset << " but size is " << list.size() 
             << endl;
        return;
    }

    title = list[offset];
    subtitle = list[offset + 1];
    description = list[offset + 2];
    category = list[offset + 3];
    chanid = list[offset + 4];
    chanstr = list[offset + 5];
    chansign = list[offset + 6];
    channame = list[offset + 7];
    pathname = list[offset + 8];
    filesize = decodeLongLong(list, offset + 9);
    startts = QDateTime::fromString(list[offset + 11], Qt::ISODate);
    endts = QDateTime::fromString(list[offset + 12], Qt::ISODate);
    conflicting = list[offset + 13].toInt();
    recording = list[offset + 14].toInt();
    override = list[offset + 15].toInt();
    hostname = list[offset + 16];
    sourceid = list[offset + 17].toInt();
    cardid = list[offset + 18].toInt();
    inputid = list[offset + 19].toInt();
    recpriority = list[offset + 20];
    norecord = NoRecordType(list[offset + 21].toInt());
    recordid = list[offset + 22].toInt();
    rectype = RecordingType(list[offset + 23].toInt());
    recdups = RecordingDupsType(list[offset + 24].toInt());
    recstartts = QDateTime::fromString(list[offset + 25], Qt::ISODate);
    recendts = QDateTime::fromString(list[offset + 26], Qt::ISODate);
    repeat = list[offset + 27].toInt();

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

    progMap["rec_type"] = RecordingChar();
    progMap["rec_str"] = RecordingText();
    progMap["recordingstatus"] = progMap["rec_str"];
    progMap["type"] = progMap["rec_str"];

    progMap["recpriority"] = recpriority;

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

    QString recstatus = "";
    if (recording)
    {
        recstatus = QObject::tr("Recording on cardid: ") +
                    QString::number(cardid);
    }
    else
    {
        recstatus = QObject::tr("Not Recording: ");
        switch (norecord)
        {
            case nrManualOverride:
                    recstatus += QObject::tr("Manual Override");
                    break;
            case nrPreviousRecording:
                    recstatus += QObject::tr("Previously Recorded");
                    break;
            case nrCurrentRecording:
                    recstatus += QObject::tr("Currently Recording");
                    break;
            case nrOtherShowing:
                    recstatus += QObject::tr("Other Showing");
                    break;
            case nrTooManyRecordings:
                    recstatus += QObject::tr("Too Many Recordings");
                    break;
            case nrDontRecordList:
                    recstatus += QObject::tr("Don't Record List");
                    break;
            case nrLowerRecPriority:
                    recstatus += QObject::tr("Lower Rec Priority");
                    break;
            case nrManualConflict:
                    recstatus += QObject::tr("Manual Conflict");
                    break;
            case nrAutoConflict:
                    recstatus += QObject::tr("Auto Conflict");
                    break;
            case nrOverlap:
                    recstatus += QObject::tr("Overlap");
                    break;
            case nrUnknown:
                    recstatus += QObject::tr("Reason Unknown");
                    break;
        }
    }
    progMap["RECSTATUS"] = recstatus;

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

    thequery = QString("SELECT channel.chanid,starttime,endtime,title,subtitle,"
                       "description,category,channel.channum,channel.callsign, "
                       "channel.name,previouslyshown FROM program,channel ")
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
        startts == other.startts &&
        endts == other.endts &&
        sourceid == other.sourceid)
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

QString ProgramInfo::NoRecordText(void)
{
    switch (norecord)
    {
    case nrManualOverride:
        return "it was manually set to not record";
    case nrPreviousRecording:
        return "this episode was previously recorded according to the duplicate policy chosen for this title";
    case nrCurrentRecording:
        return "this episode was previously recorded and is still available in the list of recordings";
    case nrOtherShowing:
        return "this episode will be recorded at another time instead";
    case nrTooManyRecordings:
        return "too many recordings of this program have already been recorded";
    case nrDontRecordList:
        return "it is currently being recorded or was manually canceled";
    case nrLowerRecPriority:
        return "another program with a higher recording priority will be recorded";
    case nrManualConflict:
        return "another program was manually chosen to be recorded instead";
    case nrAutoConflict:
        return "another program was automatically chosen to be recorded instead";
    case nrOverlap:
        return "it is covered by another scheduled recording for the same program";
    case nrUnknown:
    default:
        return "you should never see this";
    }
}

QString ProgramInfo::NoRecordChar(void)
{
    switch (norecord)
    {
    case nrManualOverride:
        return "X";
    case nrPreviousRecording:
        return "P";
    case nrCurrentRecording:
        return "C";
    case nrOtherShowing:
        return "O";
    case nrTooManyRecordings:
        return "T";
    case nrDontRecordList:
        return "D";
    case nrLowerRecPriority:
        return "R";
    case nrManualConflict:
        return "M";
    case nrAutoConflict:
        return "A";
    case nrOverlap:
        return "V";
    case nrUnknown:
    default:
        return "-";
    }
}

QString ProgramInfo::RecordingChar(void)
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

QString ProgramInfo::RecordingText(void)
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
    case kNotRecording:
    default:
        recstring = QObject::tr("Not Recording");
        break;
    }

    if (rectype != kNotRecording)
    {
        recstring += " - ";
        if (conflicting)
            recstring += QObject::tr("Conflicting");
        else if (recording)
            recstring += QObject::tr("Will Record");
        else
            recstring += QObject::tr("Won't Record") +
                " (" + NoRecordChar() + ")";
    }

    return recstring;
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
        norecord = found->norecord;
        recordid = found->recordid;
        rectype = found->rectype;
        recdups = found->recdups;
        recstartts = found->recstartts;
        recendts = found->recendts;
    }
}

void ProgramInfo::Save(QSqlDatabase *db)
{
    QString querystr;
    QString escaped_title;
    QString escaped_subtitle;
    QString escaped_description;
    QString escaped_category;

    escaped_title = title;
    escaped_title.replace(QRegExp("\""), QString("\\\""));

    escaped_subtitle = subtitle;
    escaped_subtitle.replace(QRegExp("\""), QString("\\\""));

    escaped_description = description;
    escaped_description.replace(QRegExp("\""), QString("\\\""));

    escaped_category = category;
    escaped_category.replace(QRegExp("\""), QString("\\\""));

    querystr.sprintf("INSERT INTO program (chanid,starttime,endtime,"
                    "title,subtitle,description,category,airdate,"
                    "stars) VALUES(%d,\"%s\",\"%s\",\"%s\",\"%s\", "
                    "\"%s\",\"%s\",\"%s\",\"%s\");",
                    chanid.toInt(),
                    startts.toString("yyyyMMddhhmmss").ascii(),
                    endts.toString("yyyyMMddhhmmss").ascii(),
                    escaped_title.utf8().data(),
                    escaped_subtitle.utf8().data(),
                    escaped_description.utf8().data(),
                    escaped_category.utf8().data(),
                    "0",
                    "0");

    QSqlQuery query = db->exec(querystr.utf8().data());
}

QGridLayout* ProgramInfo::DisplayWidget(ScheduledRecording* rec,
                                        QWidget *parent)
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

    QString recDateText = "";
    QLabel *recDateLabel = NULL;
    QLabel *recDateField = NULL;
    if (rec)
    {
        switch (rec->getRecordingType()) {
        case kSingleRecord:
            recDateText += startts.toString(dateFormat + " " + timeFormat);
            break;
        case kWeekslotRecord:
            recDateText += startts.toString("dddd")+"s ";
        case kTimeslotRecord:
            recDateText += QString("%1 - %2")
                .arg(startts.toString(timeFormat))
                .arg(endts.toString(timeFormat));
            break;
        case kChannelRecord:
        case kAllRecord:
        case kNotRecording:
            recDateText += "(any)";
            break;
        }

        recDateLabel = new QLabel(QObject::tr("Record Schedule:"), parent);
        recDateLabel->setBackgroundOrigin(QWidget::WindowOrigin);
        recDateField = new QLabel(recDateText,parent);
        recDateField->setBackgroundOrigin(QWidget::WindowOrigin);
    }

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

    int descwidth = (int)(700 * wmult) - descriptionlabel->width();
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
    if (rec)
    {
        grid->addWidget(recDateLabel, row, 0, Qt::AlignLeft | Qt::AlignTop);
        grid->addWidget(recDateField, row, 1, Qt::AlignLeft);
        row++;
    }
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

void ProgramInfo::handleRecording(QSqlDatabase *db)
{
    QString message = "This showing will be recorded.";

    DialogBox diag(gContext->GetMainWindow(), QObject::tr(message));
    diag.AddButton(QObject::tr("OK"));
    diag.AddButton(QObject::tr("Don't record it"));

    int ret = diag.exec();

    if (ret <= 1)
      return;

    setOverride(db, 2);
}

void ProgramInfo::handleNotRecording(QSqlDatabase *db)
{
    QString message = QString("This showing will not be recorded because %1.")
        .arg(NoRecordText());

    DialogBox diag(gContext->GetMainWindow(), QObject::tr(message));
    diag.AddButton(QObject::tr("OK"));

    if (norecord != nrTooManyRecordings &&
        norecord != nrDontRecordList &&
        norecord != nrLowerRecPriority &&
        norecord != nrOverlap)
        diag.AddButton(QObject::tr("Record it anyway"));

    int ret = diag.exec();

    if (ret <= 1)
        return;

    if (norecord != nrManualConflict &&
        norecord != nrAutoConflict)
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
