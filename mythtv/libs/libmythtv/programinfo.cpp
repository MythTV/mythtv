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

    conflicting = false;
    recording = true;
    duplicate = false;

    sourceid = -1;
    inputid = -1;
    cardid = -1;
    schedulerid = "";
    rank = "";

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
    spread = other.spread;
    startCol = other.startCol;
 
    conflicting = other.conflicting;
    recording = other.recording;
    duplicate = other.duplicate;

    sourceid = other.sourceid;
    inputid = other.inputid;
    cardid = other.cardid;
    schedulerid = other.schedulerid;
    rank = other.rank;

    record = NULL;
}

ProgramInfo::~ProgramInfo() {
    if (record != NULL)
        delete record;
}

void ProgramInfo::ToStringList(QStringList &list)
{
    if (title == "")
        title = " ";
    if (subtitle == "")
        subtitle = " ";
    if (description == "")
        description = " ";
    if (category == "")
        category = " ";
    if (pathname == "")
        pathname = " ";
    if (hostname == "")
        hostname = " ";
    if (chanid == "")
        chanid = " ";
    if (chanstr == "")
        chanstr = " ";
    if (chansign == "")
        chansign = " ";
    if (channame == "")
        channame = " ";
    if (pathname == "")
        pathname = " ";
    if (rank == "")
        rank = " ";

    list << title;
    list << subtitle;
    list << description;
    list << category;
    list << chanid;
    list << chanstr;
    list << chansign;
    list << channame;
    list << pathname;
    encodeLongLong(list, filesize);
    list << startts.toString(Qt::ISODate);
    list << endts.toString(Qt::ISODate);
    list << QString::number(conflicting);
    list << QString::number(recording);
    list << QString::number(duplicate);
    list << hostname;
    list << QString::number(sourceid);
    list << QString::number(cardid);
    list << QString::number(inputid);
    list << rank;
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
    duplicate = list[offset + 15].toInt();
    hostname = list[offset + 16];
    sourceid = list[offset + 17].toInt();
    cardid = list[offset + 18].toInt();
    inputid = list[offset + 19].toInt();
    rank = list[offset + 20];

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
    if (rank == " ")
        rank = "";
}

void ProgramInfo::ToMap(QSqlDatabase *db, QMap<QString, QString> &progMap)
{
    QString tmFmt = gContext->GetSetting("TimeFormat");
    QString dtFmt = gContext->GetSetting("ShortDateFormat");
    QString length;
    int hours, minutes, seconds;
    ScheduledRecording::RecordingType recordtype;

    progMap["title"] = title;
    progMap["subtitle"] = subtitle;
    progMap["description"] = description;
    progMap["category"] = category;
    progMap["callsign"] = chansign;
    progMap["starttime"] = startts.toString(tmFmt);
    progMap["startdate"] = startts.toString(dtFmt);
    progMap["endtime"] = endts.toString(tmFmt);
    progMap["enddate"] = endts.toString(dtFmt);
    progMap["channum"] = chanstr;
    progMap["chanid"] = chanid;
    progMap["iconpath"] = "";

    seconds = startts.secsTo(endts);
    minutes = seconds / 60;
    progMap["lenmins"] = QString("%1").arg(minutes);
    hours   = minutes / 60;
    minutes = minutes % 60;
    length.sprintf("%d:%02d", hours, minutes);
    progMap["lentime"] = length;

    recordtype = GetProgramRecordingStatus(db);
    progMap["rec_str"] = "";
    progMap["rec_type"] = "";
    switch (recordtype) {
    case ScheduledRecording::NotRecording:
             progMap["rec_str"] = QObject::tr("Not Recording");
             progMap["rec_type"] = "";
             break;
    case ScheduledRecording::SingleRecord:
             progMap["rec_str"] = QObject::tr("Recording Once");
             progMap["rec_type"] = "R";
             break;
    case ScheduledRecording::TimeslotRecord:
             progMap["rec_str"] = QObject::tr("Timeslot Recording");
             progMap["rec_type"] = "T";
             break;
    case ScheduledRecording::WeekslotRecord:
             progMap["rec_str"] = QObject::tr("Weekslot Recording");
             progMap["rec_type"] = "W";
             break;
    case ScheduledRecording::ChannelRecord:
             progMap["rec_str"] = QObject::tr("Channel Recording");
             progMap["rec_type"] = "C";
             break;
    case ScheduledRecording::AllRecord:
             progMap["rec_str"] = QObject::tr("All Recording");
             progMap["rec_type"] = "A";
             break;
    }
    progMap["rank"] = rank;

    QSqlQuery query;
    QString thequery;
    thequery = QString("SELECT icon FROM channel WHERE chanid = %1")
                       .arg(chanid);
    query.exec(thequery);
    if (query.isActive() && query.numRowsAffected() > 0)
        if (query.next())
            progMap["iconpath"] = query.value(0).toString();
}

int ProgramInfo::CalculateLength(void)
{
    return startts.secsTo(endts);
}

void ProgramInfo::GetProgramRangeDateTime(QPtrList<ProgramInfo> *proglist, QString channel, 
                                          const QString &ltime, const QString &rtime)
{
    QSqlQuery query;
    QString thequery;

    thequery = QString("SELECT channel.chanid,starttime,endtime,title,subtitle,"
                       "description,category,channel.channum,channel.callsign, "
                       "channel.name FROM program,channel "
                       "WHERE program.chanid = %1 AND endtime >= %1 AND "
                       "starttime <= %3 AND program.chanid = channel.chanid "
                       "ORDER BY starttime;")
                       .arg(channel).arg(ltime).arg(rtime);
    // allowed to use default connection
    query.exec(thequery);

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
            proginfo->title = QString::fromUtf8(query.value(3).toString());
            proginfo->subtitle = QString::fromUtf8(query.value(4).toString());
            proginfo->description = 
                                   QString::fromUtf8(query.value(5).toString());
            proginfo->category = QString::fromUtf8(query.value(6).toString());
            proginfo->chanstr = query.value(7).toString();
            proginfo->chansign = query.value(8).toString();
            proginfo->channame = query.value(9).toString();
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

ProgramInfo *ProgramInfo::GetProgramAtDateTime(QString channel, const QString &ltime)
{
    QSqlQuery query;
    QString thequery;
   
    thequery = QString("SELECT channel.chanid,starttime,endtime,title,subtitle,"
                       "description,category,channel.channum,channel.callsign, "
                       "channel.name FROM program,channel WHERE "
                       "program.chanid = %1 AND starttime < %2 AND "
                       "endtime > %3 AND program.chanid = channel.chanid;")
                       .arg(channel).arg(ltime).arg(ltime);

    query.exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        ProgramInfo *proginfo = new ProgramInfo;
        proginfo->chanid = query.value(0).toString();
        proginfo->startts = QDateTime::fromString(query.value(1).toString(),
                                                  Qt::ISODate);
        proginfo->endts = QDateTime::fromString(query.value(2).toString(),
                                                Qt::ISODate);
        proginfo->title = QString::fromUtf8(query.value(3).toString());
        proginfo->subtitle = QString::fromUtf8(query.value(4).toString());
        proginfo->description = QString::fromUtf8(query.value(5).toString());
        proginfo->category = QString::fromUtf8(query.value(6).toString());
        proginfo->chanstr = query.value(7).toString();
        proginfo->chansign = query.value(8).toString();
        proginfo->channame = query.value(9).toString();
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

ProgramInfo *ProgramInfo::GetProgramAtDateTime(QString channel, 
                                               QDateTime &dtime)
{
    QString sqltime = dtime.toString("yyyyMMddhhmm");
    sqltime += "50"; 

    return GetProgramAtDateTime(channel, sqltime);
}

ProgramInfo *ProgramInfo::GetProgramFromRecorded(QString channel, 
                                                 QDateTime &dtime)
{
    QString sqltime = dtime.toString("yyyyMMddhhmm");
    sqltime += "00"; 

    return GetProgramFromRecorded(channel, sqltime);
}

ProgramInfo *ProgramInfo::GetProgramFromRecorded(QString channel, 
                                                 QString starttime)
{
    QSqlQuery query;
    QString thequery;
   
    thequery = QString("SELECT recorded.chanid,starttime,endtime,title,subtitle,"
                       "description,channel.channum,channel.callsign, "
                       "channel.name FROM recorded,channel WHERE "
                       "recorded.chanid = %1 AND starttime = %2 AND "
                       "recorded.chanid = channel.chanid;")
                       .arg(channel).arg(starttime);

    query.exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        ProgramInfo *proginfo = new ProgramInfo;
        proginfo->chanid = query.value(0).toString();
        proginfo->startts = QDateTime::fromString(query.value(1).toString(),
                                                  Qt::ISODate);
        proginfo->endts = QDateTime::fromString(query.value(2).toString(),
                                                Qt::ISODate);
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

        return proginfo;
    }

    return NULL;
}


// -1 for no data, 0 for no, 1 for weekdaily, 2 for weekly.
int ProgramInfo::IsProgramRecurring(void)
{
    QDateTime dtime = startts;

    int weekday = dtime.date().dayOfWeek();
    if (weekday < 6)
    {
        // week day    
        int daysadd = 1;
        if (weekday == 5)
            daysadd = 3;

        QDateTime checktime = dtime.addDays(daysadd);

        ProgramInfo *nextday = GetProgramAtDateTime(chanid, checktime);

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
    ProgramInfo *nextweek = GetProgramAtDateTime(chanid, checktime);

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

ScheduledRecording::RecordingType ProgramInfo::GetProgramRecordingStatus(QSqlDatabase *db)
{
    if (record == NULL) {
        record = new ScheduledRecording();
        record->loadByProgram(db, this);
    }

    return record->getRecordingType();
}

int ProgramInfo::GetChannelRank(QString chanid, QSqlDatabase *db)
{
    QString thequery;

    thequery = QString("SELECT rank FROM channel WHERE chanid = %1;")
                       .arg(chanid);

    QSqlQuery query = db->exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();
        return query.value(0).toInt();
    }
    return 0;
}

int ProgramInfo::GetRecordingTypeRank(ScheduledRecording::RecordingType type)
{
    switch(type)
    {
        case ScheduledRecording::SingleRecord:
            return gContext->GetNumSetting("SingleRecordRank", 0);
        case ScheduledRecording::TimeslotRecord:
            return gContext->GetNumSetting("TimeslotRecordRank", 0);
        case ScheduledRecording::WeekslotRecord:
            return gContext->GetNumSetting("WeekslotRecordRank", 0);
        case ScheduledRecording::ChannelRecord:
            return gContext->GetNumSetting("ChannelRecordRank", 0);
        case ScheduledRecording::AllRecord:
            return gContext->GetNumSetting("AllRecordRank", 0);
        default:
            return 0;
    }
}

// newstate uses same values as return of GetProgramRecordingState
void ProgramInfo::ApplyRecordStateChange(QSqlDatabase *db, 
                                         ScheduledRecording::RecordingType newstate)
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
    if (record->getRecordingType() != ScheduledRecording::NotRecording) {
        record->setStart(newstartts);
        record->setEnd(newendts);
    }
}

void ProgramInfo::ApplyRecordRankChange(QSqlDatabase *db,
                                        const QString &newrank)
{
    GetProgramRecordingStatus(db);
    record->setRank(newrank);
    record->save(db);
}

void ProgramInfo::ToggleRecord(QSqlDatabase *db)
{
    ScheduledRecording::RecordingType curType = GetProgramRecordingStatus(db);

    switch (curType) {
    case ScheduledRecording::NotRecording:
             ApplyRecordStateChange(db, ScheduledRecording::SingleRecord);
             break;
    case ScheduledRecording::SingleRecord:
             ApplyRecordStateChange(db, ScheduledRecording::TimeslotRecord);
             break;
    case ScheduledRecording::TimeslotRecord:
             ApplyRecordStateChange(db, ScheduledRecording::WeekslotRecord);
             break;
    case ScheduledRecording::WeekslotRecord:
             ApplyRecordStateChange(db, ScheduledRecording::ChannelRecord);
             break;
    case ScheduledRecording::ChannelRecord:
             ApplyRecordStateChange(db, ScheduledRecording::AllRecord);
             break;
    case ScheduledRecording::AllRecord:
    default:
             ApplyRecordStateChange(db, ScheduledRecording::NotRecording);
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
    QString starts = startts.toString("yyyyMMddhhmm");
    QString ends = endts.toString("yyyyMMddhhmm");

    starts += "00";
    ends += "00";

    QString retval = QString("%1_%2_%3.nuv").arg(chanid)
                             .arg(starts).arg(ends);
    
    return retval;
}               

QString ProgramInfo::GetRecordFilename(const QString &prefix)
{
    QString starts = startts.toString("yyyyMMddhhmm");
    QString ends = endts.toString("yyyyMMddhhmm");

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

    QString starts = startts.toString("yyyyMMddhhmm");
    QString ends = endts.toString("yyyyMMddhhmm");

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
                    "subtitle,description,hostname,category) "
                    "VALUES(%1,\"%2\",\"%3\",\"%4\",\"%5\",\"%6\",\"%7\","
                    "\"%8\");")
                    .arg(chanid).arg(starts).arg(ends).arg(sqltitle.utf8()) 
                    .arg(sqlsubtitle.utf8()).arg(sqldescription.utf8())
                    .arg(gContext->GetHostName()).arg(sqlcategory.utf8());

    QSqlQuery qquery = db->exec(query);
    if (!qquery.isActive())
        MythContext::DBError("WriteRecordedToDB", qquery);

}

void ProgramInfo::FinishedRecording(QSqlDatabase* db) {
    GetProgramRecordingStatus(db);
    record->doneRecording(db, *this);
}

void ProgramInfo::SetBookmark(long long pos, QSqlDatabase *db)
{
    MythContext::KickDatabase(db);

    char position[128];
    sprintf(position, "%lld", pos);

    QString starts = startts.toString("yyyyMMddhhmm");
    starts += "00";

    QString querystr = QString("UPDATE recorded SET bookmark = '%1', "
                               "starttime = '%2' WHERE chanid = '%3' AND "
                               "starttime = '%4';").arg(position).arg(starts)
                                                   .arg(chanid).arg(starts);
    QSqlQuery query = db->exec(querystr);
    if (!query.isActive())
        MythContext::DBError("Save position update", querystr);
}

long long ProgramInfo::GetBookmark(QSqlDatabase *db)
{
    MythContext::KickDatabase(db);

    long long pos = 0;

    QString starts = startts.toString("yyyyMMddhhmm");
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

    QString starts = startts.toString("yyyyMMddhhmm");
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

    QString starts = startts.toString("yyyyMMddhhmm");
    starts += "00";

    QString querystr = QString("UPDATE recorded SET editing = '%1', "
                               "starttime = '%2' WHERE chanid = '%3' AND "
                               "starttime = '%4';").arg(edit).arg(starts)
                                                   .arg(chanid).arg(starts);
    QSqlQuery query = db->exec(querystr);
    if (!query.isActive())
        MythContext::DBError("Edit status update", querystr);
}

void ProgramInfo::GetCutList(QMap<long long, int> &delMap, QSqlDatabase *db)
{
    delMap.clear();

    MythContext::KickDatabase(db);

    QString starts = startts.toString("yyyyMMddhhmm");
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

    QString starts = startts.toString("yyyyMMddhhmm");
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
        QSqlDatabase *db, long long min_frame, long long max_frame)
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
    QString starts = startts.toString("yyyyMMddhhmm");
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
                  int type, long long min_frame, long long max_frame)
{
    QMap<long long, int>::Iterator i;

    QString starts = startts.toString("yyyyMMddhhmm");
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

        querystr = QString("INSERT recordedmarkup (chanid, starttime, "
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

    QString starts = startts.toString("yyyyMMddhhmm");
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

    QString starts = startts.toString("yyyyMMddhhmm");
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

    QString starts = startts.toString("yyyyMMddhhmm");
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

        querystr = QString("INSERT recordedmarkup (chanid, starttime, "
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

void ProgramInfo::Save(void)
{
    QSqlQuery query;
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

    query.exec(querystr.utf8().data());
}

QGridLayout* ProgramInfo::DisplayWidget(ScheduledRecording* rec,
                                        QWidget *parent)
{
    int row = 0;
    float wmult, hmult;
    int screenwidth, screenheight;
    gContext->GetScreenSettings(screenwidth, wmult, screenheight, hmult);

    int bigfont = gContext->GetBigFontSize();

    QGridLayout *grid = new QGridLayout(4, 2, (int)(10*wmult));
    
    QLabel *titlefield = new QLabel(title, parent);
    titlefield->setBackgroundOrigin(QWidget::WindowOrigin);
    titlefield->setFont(QFont("Arial", (int)(bigfont * hmult), QFont::Bold));

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
        case ScheduledRecording::SingleRecord:
            recDateText += startts.toString(dateFormat + " " + timeFormat);
            break;
        case ScheduledRecording::WeekslotRecord:
            recDateText += startts.toString("dddd")+"s ";
        case ScheduledRecording::TimeslotRecord:
            recDateText += QString("%1 - %2")
                .arg(startts.toString(timeFormat))
                .arg(endts.toString(timeFormat));
            break;
        case ScheduledRecording::ChannelRecord:
        case ScheduledRecording::AllRecord:
        case ScheduledRecording::NotRecording:
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
