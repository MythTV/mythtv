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
    chancommfree = 0;
    chanOutputFilters = "";
    stars = 0;
    

    pathname = "";
    filesize = 0;
    hostname = "";
    programflags = 0;

    startts = QDateTime::currentDateTime();
    endts = startts;
    recstartts = startts;
    recendts = startts;
    originalAirDate = startts.date();
    lastmodified = startts;


    recstatus = rsUnknown;
    savedrecstatus = rsUnknown;
    numconflicts = 0;
    conflictpriority = -1000;
    reactivate = false;
    recordid = 0;
    rectype = kNotRecording;
    dupin = kDupsInAll;
    dupmethod = kDupCheckSubDesc;

    sourceid = 0;
    inputid = 0;
    cardid = 0;
    shareable = false;
    schedulerid = "";
    recpriority = 0;
    recgroup = QString("Default");

    hasAirDate = false;
    repeat = false;

    record = NULL;
}   
        
ProgramInfo::ProgramInfo(const ProgramInfo &other)
{           
    record = NULL;
    
    clone(other);
}


ProgramInfo &ProgramInfo::operator=(const ProgramInfo &other) 
{ 
    return clone(other); 
}

ProgramInfo &ProgramInfo::clone(const ProgramInfo &other)
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
    chancommfree = other.chancommfree;
    chanOutputFilters = other.chanOutputFilters;
    
    pathname = other.pathname;
    filesize = other.filesize;
    hostname = other.hostname;

    startts = other.startts;
    endts = other.endts;
    recstartts = other.recstartts;
    recendts = other.recendts;
    lastmodified = other.lastmodified;
    spread = other.spread;
    startCol = other.startCol;

    recstatus = other.recstatus;
    savedrecstatus = other.savedrecstatus;
    numconflicts = other.numconflicts;
    conflictpriority = other.conflictpriority;
    reactivate = other.reactivate;
    recordid = other.recordid;
    rectype = other.rectype;
    dupin = other.dupin;
    dupmethod = other.dupmethod;

    sourceid = other.sourceid;
    inputid = other.inputid;
    cardid = other.cardid;
    shareable = other.shareable;
    schedulerid = other.schedulerid;
    recpriority = other.recpriority;
    recgroup = other.recgroup;
    programflags = other.programflags;

    hasAirDate = other.hasAirDate;
    repeat = other.repeat;

    seriesid = other.seriesid;
    programid = other.programid;

    originalAirDate = other.originalAirDate;
    stars = other.stars;
    
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
    list << QString(" "); // dummy place holder
    list << QString::number(shareable);
    list << QString("0"); // dummy place holder
    list << ((hostname != "") ? hostname : QString(" "));
    list << QString::number(sourceid);
    list << QString::number(cardid);
    list << QString::number(inputid);
    list << QString::number(recpriority);
    list << QString::number(recstatus);
    list << QString::number(recordid);
    list << QString::number(rectype);
    list << QString::number(dupin);
    list << QString::number(dupmethod);
    list << recstartts.toString(Qt::ISODate);
    list << recendts.toString(Qt::ISODate);
    list << QString::number(repeat);
    list << QString::number(programflags);
    list << ((recgroup != "") ? recgroup : QString("Default"));
    list << QString::number(chancommfree);
    list << ((chanOutputFilters != "") ? chanOutputFilters : QString(" "));
    list << ((seriesid != "") ? seriesid : QString(" "));
    list << ((programid != "") ? programid : QString(" "));
    list << lastmodified.toString(Qt::ISODate);
    list << QString::number(stars);
    list << originalAirDate.toString(Qt::ISODate);
}

bool ProgramInfo::FromStringList(QStringList &list, int offset)
{
    QStringList::iterator it = list.at(offset);
    return FromStringList(list, it);
}

bool ProgramInfo::FromStringList(QStringList &list, QStringList::iterator &it)
{
    QStringList::iterator checkit = it;
    checkit += (NUMPROGRAMLINES - 1);
    if (checkit == list.end())
    {
        cerr << "Not enough items in list for ProgramInfo object" << endl;
        return false;
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
    it++; // dummy place holder
    shareable = (*(it++)).toInt();
    it++; // dummy place holder
    hostname = *(it++);
    sourceid = (*(it++)).toInt();
    cardid = (*(it++)).toInt();
    inputid = (*(it++)).toInt();
    recpriority = (*(it++)).toInt();
    recstatus = RecStatusType((*(it++)).toInt());
    recordid = (*(it++)).toInt();
    rectype = RecordingType((*(it++)).toInt());
    dupin = RecordingDupInType((*(it++)).toInt());
    dupmethod = RecordingDupMethodType((*(it++)).toInt());
    recstartts = QDateTime::fromString(*(it++), Qt::ISODate);
    recendts = QDateTime::fromString(*(it++), Qt::ISODate);
    repeat = (*(it++)).toInt();
    programflags = (*(it++)).toInt();
    recgroup = *(it++);
    chancommfree = (*(it++)).toInt();
    chanOutputFilters = *(it++);
    seriesid = *(it++);
    programid = *(it++);
    lastmodified = QDateTime::fromString(*(it++), Qt::ISODate);
    stars = (*(it++)).toFloat();
    originalAirDate = QDate::fromString(*(it++), Qt::ISODate);

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
    if (recgroup == " ")
        recgroup = QString("Default");
    if(chanOutputFilters == " ")
        chanOutputFilters = "";
    if (seriesid == " ")
        seriesid = "";
    if (programid == " ")
        programid = "";

    return true;
}

void ProgramInfo::ToMap(QSqlDatabase *db, QMap<QString, QString> &progMap)
{
    QString timeFormat = gContext->GetSetting("TimeFormat", "h:mm AP");
    QString dateFormat = gContext->GetSetting("DateFormat", "ddd MMMM d");
    QString oldDateFormat = gContext->GetSetting("OldDateFormat", "M/d/yyyy");
    QString shortDateFormat = gContext->GetSetting("ShortDateFormat", "M/d");
    QString channelFormat = 
        gContext->GetSetting("ChannelFormat", "<num> <sign>");
    QString longChannelFormat = 
        gContext->GetSetting("LongChannelFormat", "<num> <name>");

    QDateTime timeNow = QDateTime::currentDateTime();

    QString length;
    int hours, minutes, seconds;
    
    progMap["title"] = title;
    progMap["subtitle"] = subtitle;
    progMap["description"] = description;
    progMap["category"] = category;
    progMap["callsign"] = chansign;
    progMap["commfree"] = chancommfree;
    progMap["outputfilters"] = chanOutputFilters;
    progMap["starttime"] = startts.toString(timeFormat);
    progMap["startdate"] = startts.toString(shortDateFormat);
    progMap["endtime"] = endts.toString(timeFormat);
    progMap["enddate"] = endts.toString(shortDateFormat);
    progMap["recstarttime"] = recstartts.toString(timeFormat);
    progMap["recstartdate"] = recstartts.toString(shortDateFormat);
    progMap["recendtime"] = recendts.toString(timeFormat);
    progMap["recenddate"] = recendts.toString(shortDateFormat);
    progMap["lastmodifiedtime"] = lastmodified.toString(timeFormat);
    progMap["lastmodifieddate"] = lastmodified.toString(dateFormat);
    progMap["lastmodified"] = lastmodified.toString(dateFormat) + " " +
                              lastmodified.toString(timeFormat);

    progMap["channum"] = chanstr;
    progMap["chanid"] = chanid;
    progMap["channel"] = ChannelText(channelFormat);
    progMap["longchannel"] = ChannelText(longChannelFormat);
    progMap["iconpath"] = "";

    QString tmpSize;

    tmpSize.sprintf("%0.2f ", filesize / 1024.0 / 1024.0 / 1024.0);
    tmpSize += QObject::tr("GB", "GigaBytes");
    progMap["filesize_str"] = tmpSize;

    progMap["filesize"] = longLongToString(filesize);

    seconds = recstartts.secsTo(recendts);
    minutes = seconds / 60;
    progMap["lenmins"] = QString("%1 %2").
        arg(minutes).arg(QObject::tr("minutes"));
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
    progMap["recgroup"] = recgroup;
    progMap["programflags"] = programflags;

    progMap["timedate"] = recstartts.date().toString(dateFormat) + ", " +
                          recstartts.time().toString(timeFormat) + " - " +
                          recendts.time().toString(timeFormat);

    progMap["shorttimedate"] =
                          recstartts.date().toString(shortDateFormat) + ", " +
                          recstartts.time().toString(timeFormat) + " - " +
                          recendts.time().toString(timeFormat);

    progMap["time"] = timeNow.time().toString(timeFormat);

    if(db)
    {
        QString thequery = QString("SELECT icon FROM channel WHERE chanid = %1")
                                   .arg(chanid);
        QSqlQuery query = db->exec(thequery);
        if (query.isActive() && query.numRowsAffected() > 0)
            if (query.next())
                progMap["iconpath"] = query.value(0).toString();
    }

    progMap["RECSTATUS"] = RecStatusText();

    if (repeat)
    {
        progMap["REPEAT"] = QString("(%1) ").arg(QObject::tr("Repeat"));
        progMap["LONGREPEAT"] = QString("(%1 %2) ")
                                .arg(QObject::tr("Repeat"))
                                .arg(originalAirDate.toString(oldDateFormat));
    }
    else
    {
        progMap["REPEAT"] = "";
        progMap["LONGREPEAT"] = "";
    }
   
    progMap["seriesid"] = seriesid;
    progMap["programid"] = programid;
    
    if(stars)
        progMap["stars"] = QString("(%1 %2) ").arg(4.0 * stars).arg(QObject::tr("stars"));
    else
        progMap["stars"] = "";
        
    progMap["originalairdate"]= originalAirDate.toString(dateFormat);
    progMap["shortoriginalairdate"]= originalAirDate.toString(shortDateFormat);
    
}

int ProgramInfo::CalculateLength(void)
{
    return startts.secsTo(endts);
}

ProgramInfo *ProgramInfo::GetProgramAtDateTime(QSqlDatabase *db,
                                               const QString &channel, 
                                               QDateTime &dtime)
{
    ProgramList schedList;
    ProgramList progList;

    QString querystr = QString(
        "WHERE program.chanid = %1 AND starttime < %2 AND "
        "    endtime > %3 ")
        .arg(channel)
        .arg(dtime.toString("yyyyMMddhhmm50"))
        .arg(dtime.toString("yyyyMMddhhmm50"));

    schedList.FromScheduler();
    progList.FromProgram(db, querystr, schedList);

    return progList.take(0);
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
                       "channel.callsign,channel.name,channel.commfree, "
                       "channel.outputfilters,seriesid,programid,filesize, "
                       "lastmodified,stars,previouslyshown,originalairdate, "
                       "hostname "
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
        proginfo->chansign = QString::fromUtf8(query.value(7).toString());
        proginfo->channame = QString::fromUtf8(query.value(8).toString());
        proginfo->chancommfree = query.value(9).toInt();
        proginfo->chanOutputFilters = query.value(10).toString();
        proginfo->seriesid = query.value(11).toString();
        proginfo->programid = query.value(12).toString();
        proginfo->filesize = stringToLongLong(query.value(13).toString());

        proginfo->lastmodified =
                  QDateTime::fromString(query.value(14).toString(),
                                        Qt::ISODate);
        
        proginfo->stars = query.value(15).toString().toFloat();
        proginfo->repeat = query.value(16).toInt();
        
        if(query.value(17).isNull() || query.value(17).toString().isEmpty())
        {
            proginfo->originalAirDate = proginfo->startts.date();
            proginfo->hasAirDate = false;
        }
        else
        {
            proginfo->originalAirDate = QDate::fromString(query.value(17).toString(),
                                                          Qt::ISODate);
            proginfo->hasAirDate = true;
        }
        proginfo->hostname = query.value(18).toString();

        proginfo->spread = -1;

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

    QSqlQuery query(QString::null, db);

    query.prepare("SELECT count(*) FROM recorded WHERE title = :TITLE;");
    query.bindValue(":TITLE", title.utf8());

    query.exec();

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
        case kFindOneRecord:
            return gContext->GetNumSetting("FindOneRecordRecPriority", 0);
        case kOverrideRecord:
        case kDontRecord:
            return gContext->GetNumSetting("OverrideRecordRecPriority", 0);
        default:
            return 0;
    }
}

// newstate uses same values as return of GetProgramRecordingState
void ProgramInfo::ApplyRecordStateChange(QSqlDatabase *db, 
                                         RecordingType newstate)
{
    GetProgramRecordingStatus(db);
    if (newstate == kOverrideRecord || newstate == kDontRecord)
        record->makeOverride();
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
                                        int newrecpriority)
{
    GetProgramRecordingStatus(db);
    record->setRecPriority(newrecpriority);
    record->save(db);
}

void ProgramInfo::ApplyRecordRecGroupChange(QSqlDatabase *db,
                                            const QString &newrecgroup)
{
    MythContext::KickDatabase(db);

    QString starts = recstartts.toString("yyyyMMddhhmm");
    starts += "00";

    QSqlQuery query(QString::null, db);
    query.prepare("UPDATE recorded SET recgroup = :RECGROUP, "
                  "starttime = :START WHERE chanid = :CHANID AND "
                  "starttime = :START2;");
    query.bindValue(":RECGROUP", newrecgroup.utf8());
    query.bindValue(":START", starts);
    query.bindValue(":CHANID", chanid);
    query.bindValue(":START2", starts);

    if (!query.exec())
        MythContext::DBError("RecGroup update", query);

    recgroup = newrecgroup;
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
            ApplyRecordStateChange(db, kFindOneRecord);
            break;
        case kFindOneRecord:
            ApplyRecordStateChange(db, kWeekslotRecord);
            break;
        case kWeekslotRecord:
            ApplyRecordStateChange(db, kTimeslotRecord);
            break;
        case kTimeslotRecord:
            ApplyRecordStateChange(db, kChannelRecord);
            break;
        case kChannelRecord:
            ApplyRecordStateChange(db, kAllRecord);
            break;
        case kAllRecord:
        default:
            ApplyRecordStateChange(db, kNotRecording);
            break;
        case kOverrideRecord:
            ApplyRecordStateChange(db, kDontRecord);
            break;
        case kDontRecord:
            ApplyRecordStateChange(db, kOverrideRecord);
            break;
    }
}

bool ProgramInfo::IsSameProgram(const ProgramInfo& other) const
{
    if (title != other.title)
        return false;

    if (rectype == kFindOneRecord)
        return true;

    if (dupmethod & kDupCheckNone)
        return false;

    if (programid.contains(QRegExp("^SH.*0000$")))
        return false;

    if (programid != "" && other.programid != "")
    {
        if (programid == other.programid)
            return true;
        else
            return false;
    }

    if ((dupmethod & kDupCheckSub) &&
        ((subtitle == "") ||
         (subtitle != other.subtitle)))
        return false;

    if ((dupmethod & kDupCheckDesc) &&
        ((description == "") ||
         (description != other.description)))
        return false;

    return true;
}
 
bool ProgramInfo::IsSameTimeslot(const ProgramInfo& other) const
{
    if (startts == other.startts && endts == other.endts &&
        (chanid == other.chanid || 
         (chansign != "" && chansign == other.chansign)))
        return true;

    return false;
}

bool ProgramInfo::IsSameProgramTimeslot(const ProgramInfo &other) const
{
    if ((chanid == other.chanid ||
         (chansign != "" && chansign == other.chansign)) &&
        startts < other.endts &&
        endts > other.startts)
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

    QSqlQuery query(QString::null, db);

    query.prepare("INSERT INTO recorded (chanid,starttime,endtime,title,"
                  "subtitle,description,hostname,category,recgroup,"
                  "autoexpire,recordid,seriesid,programid,stars,"
                  "previouslyshown,originalairdate) "
                  "VALUES(:CHANID,:STARTS,:ENDS,:TITLE,:SUBTITLE,:DESC,"
                         ":HOSTNAME,:CATEGORY,:RECGROUP,:AUTOEXP,:RECORDID,"
                         ":SERIESID,:PROGRAMID,:STARS,:REPEAT,:ORIGAIRDATE);");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTS", starts);
    query.bindValue(":ENDS", ends);
    query.bindValue(":TITLE", title.utf8());
    query.bindValue(":SUBTITLE", subtitle.utf8());
    query.bindValue(":DESC", description.utf8());
    query.bindValue(":HOSTNAME", gContext->GetHostName());
    query.bindValue(":CATEGORY", category.utf8());
    query.bindValue(":RECGROUP", recgroup.utf8());
    query.bindValue(":AUTOEXP", record->GetAutoExpire());
    query.bindValue(":RECORDID", recordid);
    query.bindValue(":SERIESID", seriesid);
    query.bindValue(":PROGRAMID", programid);
    query.bindValue(":STARS", stars);
    query.bindValue(":REPEAT", repeat);
    query.bindValue(":ORIGAIRDATE", originalAirDate.toString());

    if (!query.exec() || !query.isActive())
        MythContext::DBError("WriteRecordedToDB", query);

    query.prepare("DELETE FROM recordedmarkup WHERE chanid = :CHANID "
                  "AND starttime = :START;");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":START", starts);

    if (!query.exec() || !query.isActive())
        MythContext::DBError("Clear markup on record", query);
}

void ProgramInfo::FinishedRecording(QSqlDatabase* db, bool prematurestop) 
{
    GetProgramRecordingStatus(db);
    if (!prematurestop)
        record->doneRecording(db, *this);
}

void ProgramInfo::SetFilesize(long long fsize, QSqlDatabase *db)
{
    MythContext::KickDatabase(db);

    filesize = fsize;
    QString size(longLongToString(filesize));

    QString starts = recstartts.toString("yyyyMMddhhmm");
    starts += "00";

    QString querystr;

    querystr = QString("UPDATE recorded SET filesize = %1 "
                       "WHERE chanid = '%2' AND starttime = '%3';")
                       .arg(size)
                       .arg(chanid)
                       .arg(starts);

    QSqlQuery query = db->exec(querystr);
    if (!query.isActive())
        MythContext::DBError("File size update", querystr);
}

long long ProgramInfo::GetFilesize(QSqlDatabase *db)
{
    MythContext::KickDatabase(db);

    long long size = 0;

    QString starts = recstartts.toString("yyyyMMddhhmm");
    starts += "00";

    QString querystr = QString("SELECT filesize FROM recorded WHERE "
                               "chanid = '%1' AND starttime = '%2';")
                              .arg(chanid).arg(starts);

    QSqlQuery query = db->exec(querystr);
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        size = stringToLongLong(query.value(0).toString());
    }

    filesize = size;
    return size;
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

        result = (query.value(0).toInt() == COMM_FLAG_DONE);
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

        result = (query.value(0).toInt() == COMM_FLAG_PROCESSING);
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
            marks[stringToLongLong(query.value(0).toString())] =
                  query.value(1).toInt();
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
            posMap[stringToLongLong(query.value(0).toString())] =
                   stringToLongLong(query.value(1).toString());
    }
}

void ProgramInfo::ClearPositionMap(int type, QSqlDatabase *db)
{
    QString starts = recstartts.toString("yyyyMMddhhmm");
    starts += "00";

    QString querystr = QString("DELETE FROM recordedmarkup "
                               "WHERE chanid = '%1' AND starttime = '%2' "
                               "AND type = %3;")
                               .arg(chanid).arg(starts).arg(type);
    QSqlQuery query = db->exec(querystr);
    if (!query.isActive())
        MythContext::DBError("clear position map", querystr);
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

void ProgramInfo::SetPositionMapDelta(QMap<long long, long long> &posMap,
                                      int type, QSqlDatabase *db)
{
    QMap<long long, long long>::Iterator i;

    QString starts = recstartts.toString("yyyyMMddhhmm");
    starts += "00";

    for (i = posMap.begin(); i != posMap.end(); ++i)
    {
        long long frame = i.key();
        char tempc[128];
        sprintf(tempc, "%lld", frame);

        QString frame_str = tempc;

        long long offset = i.data();
        sprintf(tempc, "%lld", offset);
       
        QString offset_str = tempc;

        QString querystr = QString("INSERT INTO recordedmarkup "
                           "(chanid, starttime, mark, type, offset) VALUES "
                           "( '%1', '%2', %3, %4, \"%5\");")
                           .arg(chanid).arg(starts)
                           .arg(frame_str).arg(type)
                           .arg(offset_str);

        QSqlQuery subquery = db->exec(querystr);
        if (!subquery.isActive())
            MythContext::DBError("delta position map insert", querystr);
    }
}

void ProgramInfo::DeleteHistory(QSqlDatabase *db)
{
    GetProgramRecordingStatus(db);
    record->forgetHistory(db, *this);
}

QString ProgramInfo::RecTypeChar(void)
{
    switch (rectype)
    {
    case kSingleRecord:
        return QObject::tr("S", "RecTypeChar");
    case kTimeslotRecord:
        return QObject::tr("T", "RecTypeChar");
    case kWeekslotRecord:
        return QObject::tr("W", "RecTypeChar");
    case kChannelRecord:
        return QObject::tr("C", "RecTypeChar");
    case kAllRecord:
        return QObject::tr("A", "RecTypeChar");
    case kFindOneRecord:
        return QObject::tr("F", "RecTypeChar");
    case kOverrideRecord:
    case kDontRecord:
        return QObject::tr("O", "RecTypeChar");
    case kNotRecording:
    default:
        return " ";
    }
}

QString ProgramInfo::RecTypeText(void)
{
    switch (rectype)
    {
    case kSingleRecord:
        return QObject::tr("Single Recording");
    case kTimeslotRecord:
        return QObject::tr("Daily Recording");
    case kWeekslotRecord:
        return QObject::tr("Weekly Recording");
    case kChannelRecord:
        return QObject::tr("Channel Recording");
    case kAllRecord:
        return QObject::tr("All Recording");
    case kFindOneRecord:
        return QObject::tr("Find One Recording");
    case kOverrideRecord:
    case kDontRecord:
        return QObject::tr("Override Recording");
    default:
        return QObject::tr("Not Recording");
    }
}

QString ProgramInfo::RecStatusChar(void)
{
    switch (recstatus)
    {
    case rsDeleted:
        return QObject::tr("D", "RecStatusChar");
    case rsStopped:
        return QObject::tr("S", "RecStatusChar");
    case rsRecorded:
        return QObject::tr("R", "RecStatusChar");
    case rsRecording:
        return QString::number(cardid);
    case rsWillRecord:
        return QString::number(cardid);
    case rsDontRecord:
        return QObject::tr("X", "RecStatusChar");
    case rsPreviousRecording:
        return QObject::tr("P", "RecStatusChar");
    case rsCurrentRecording:
        return QObject::tr("R", "RecStatusChar");
    case rsRepeat:
        return QObject::tr("r", "RecStatusChar");    
    case rsEarlierShowing:
        return QObject::tr("E", "RecStatusChar");
    case rsTooManyRecordings:
        return QObject::tr("T", "RecStatusChar");
    case rsCancelled:
        return QObject::tr("N", "RecStatusChar");
    case rsConflict:
        return QObject::tr("C", "RecStatusChar");
    case rsLaterShowing:
        return QObject::tr("L", "RecStatusChar");
    case rsLowDiskSpace:
        return QObject::tr("K", "RecStatusChar");
    case rsTunerBusy:
        return QObject::tr("B", "RecStatusChar");
    default:
        return "-";
    }
}

QString ProgramInfo::RecStatusText(void)
{
    if (rectype == kNotRecording)
        return QObject::tr("Not Recording");
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
        case rsDontRecord:
            return QObject::tr("Don't Record");
        case rsPreviousRecording:
            return QObject::tr("Previous Recording");
        case rsCurrentRecording:
            return QObject::tr("Current Recording");
        case rsEarlierShowing:
            return QObject::tr("Earlier Showing");
        case rsTooManyRecordings:
            return QObject::tr("Max Recordings");
        case rsCancelled:
            return QObject::tr("Manual Cancel");
        case rsConflict:
            return QObject::tr("Conflicting");
        case rsLaterShowing:
            return QObject::tr("Later Showing");
        case rsLowDiskSpace:
            return QObject::tr("Low Disk Space");
        case rsTunerBusy:
            return QObject::tr("Tuner Busy");
        case rsRepeat:
            return QObject::tr("Repeat");            
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

    if (recstatus <= rsWillRecord)
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
        case rsDontRecord:
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
        case rsEarlierShowing:
            message += QObject::tr("this episode will be recorded at an "
                                   "earlier time instead.");
            break;
        case rsRepeat:
            message += QObject::tr("this episode is a repeat.");
            break;            
        case rsTooManyRecordings:
            message += QObject::tr("too many recordings of this program have "
                                   "already been recorded.");
            break;
        case rsCancelled:
            message += QObject::tr("it was manually cancelled.");
            break;
        case rsConflict:
            message += QObject::tr("another program with a higher priority "
                                   "will be recorded.");
            break;
        case rsLaterShowing:
            message += QObject::tr("this episode will be recorded at a "
                                   "later time.");
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

QString ProgramInfo::ChannelText(QString format)
{
    format.replace("<num>", chanstr)
        .replace("<sign>", chansign)
        .replace("<name>", channame);

    return format;
}

void ProgramInfo::FillInRecordInfo(vector<ProgramInfo *> &reclist)
{
    vector<ProgramInfo *>::iterator i;
    ProgramInfo *found = NULL;
    int pfound = 0;

    for (i = reclist.begin(); i != reclist.end(); i++)
    {
        ProgramInfo *p = *i;
        if (IsSameTimeslot(*p))
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
        recstatus = found->recstatus;
        recordid = found->recordid;
        rectype = found->rectype;
        dupin = found->dupin;
        dupmethod = found->dupmethod;
        recstartts = found->recstartts;
        recendts = found->recendts;
        cardid = found->cardid;
        inputid = found->inputid;
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

QGridLayout* ProgramInfo::DisplayWidget(QWidget *parent, QString searchtitle)
{
    int row = 0;
    float wmult, hmult;
    int screenwidth, screenheight;
    gContext->GetScreenSettings(screenwidth, wmult, screenheight, hmult);

    QGridLayout *grid = new QGridLayout(4, 2, (int)(10*wmult));
    
    QLabel *titlefield = NULL;
    QString episode;

    if (searchtitle != "")
    {
        titlefield = new QLabel(searchtitle, parent);

        if (subtitle == "")
            episode = title;
        else
            episode = QString("%1 - \"%2\"").arg(title).arg(subtitle);
    }
    else
    {
        titlefield = new QLabel(title, parent);
        episode = subtitle;
    }

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
    QLabel *subtitlefield = new QLabel(episode, parent);
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
    else if (recstatus <= rsWillRecord)
        handleRecording(db);
    else
        handleNotRecording(db);
}

void ProgramInfo::EditScheduled(QSqlDatabase *db)
{
    MythContext::KickDatabase(db);

    if (gContext->GetNumSetting("AdvancedRecord", 0))
    {
        GetProgramRecordingStatus(db);
        record->exec(db);
    }
    else
    {
        InfoDialog diag(this, gContext->GetMainWindow(), "Program Info");
        diag.exec();
    }
}

void ProgramInfo::showDetails(QSqlDatabase *db)
{
    MythContext::KickDatabase(db);

    QString oldDateFormat = gContext->GetSetting("OldDateFormat", "M/d/yyyy");

    QString querytext;
    QString category_type, airdate, epinum, rating;
    int partnumber = 0, parttotal = 0;
    int stereo = 0, subtitled = 0, hdtv = 0, closecaptioned = 0;

    if (endts != startts)
    {
        querytext = QString("SELECT category_type, airdate, partnumber, "
                            "parttotal, stereo, subtitled, hdtv, "
                            "closecaptioned, syndicatedepisodenumber "
                            "FROM program WHERE chanid = %1 "
                            "AND starttime=\'%2\';")
                            .arg(chanid).arg(startts.toString(Qt::ISODate));

        QSqlQuery query = db->exec(querytext);

        if (query.isActive() && query.numRowsAffected())
        {
            query.next();
            category_type = query.value(0).toString();
            airdate = query.value(1).toString();
            partnumber = query.value(2).toInt();
            parttotal = query.value(3).toInt();
            stereo = query.value(4).toInt();
            subtitled = query.value(5).toInt();
            hdtv = query.value(6).toInt();
            closecaptioned = query.value(7).toInt();
            epinum = query.value(8).toString();
        }

        querytext = QString("SELECT rating FROM programrating "
                            "WHERE chanid = %1 AND starttime=\'%2\';")
                            .arg(chanid).arg(startts.toString(Qt::ISODate));
        
        QSqlQuery rquery = db->exec(querytext);
        
        if (rquery.isActive() && rquery.numRowsAffected())
        {
            rquery.next();
            rating = rquery.value(0).toString();
        }
    }
    if (category_type == "" && programid != "")
    {
        QString prefix = programid.left(2);

        if (prefix == "MV")
           category_type = "movie";
        else if (prefix == "EP")
           category_type = "series";
        else if (prefix == "SP")
           category_type = "sports";
        else if (prefix == "SH")
           category_type = "tvshow";
    }

    QString msg = title;

    if (subtitle != "")
        msg += " - \"" + subtitle + "\"";

    msg += "\n";

    if (description  != "")
        msg += "    " + description;

    QString attr = "";

    if(partnumber > 0)
        attr = QString("Part %1 of %2, ").arg(partnumber).arg(parttotal);

    if (rating != "" && rating != "NR")
        attr += rating + ", ";

    if (category_type == "movie")
    {
        if (airdate != "")
            attr += airdate + ", ";
        if (stars > 0)
            attr +=  QString("%1 %2, ").arg(4.0 * stars)
                                           .arg(QObject::tr("stars"));
    }

    if (hdtv)
        attr += "HDTV, ";
    if (closecaptioned)
        attr += "CC, ";
    if (subtitled)
        attr += "Subtitled, ";
    if (stereo)
        attr += "Stereo, ";
    if (repeat)
        attr += "Repeat, ";

    if (attr != "")
    {
        attr.truncate(attr.findRev(','));
        msg += " (" + attr + ")";
    }
    msg += "\n\n";

    if(category != "")
        msg += QObject::tr("Category:  ") + category + "\n";

    if (category_type  != "")
    {
        msg += QObject::tr("Type:  ") + category_type;
        if (seriesid != "")
            msg += "  (" + seriesid + ")";
        msg += "\n";
    }

    if(epinum != "")
        msg += QObject::tr("Episode Number:  ") + epinum + "\n";

    if (hasAirDate && category_type != "movie")
    {
        msg += QObject::tr("Original Airdate:  ");
        msg += originalAirDate.toString(oldDateFormat) + "\n";
    }
    if(programid  != "")
        msg += QObject::tr("Program ID:  ") + programid + "\n";

    QString role = "", pname = "";

    if (endts != startts)
    {
        querytext = QString("SELECT role,people.name from credits "
                    "LEFT JOIN people ON credits.person = people.person "
                    "LEFT JOIN program ON credits.chanid = program.chanid "
                    "AND credits.starttime = program.starttime "
                    "WHERE program.chanid = %1 "
                    "AND program.starttime=\'%2\' ORDER BY role;")
                    .arg(chanid).arg(startts.toString(Qt::ISODate));

        QSqlQuery pquery = db->exec(querytext);

        if (pquery.isActive() && pquery.numRowsAffected())
        {
            QString rstr = "", plist = "";

            while(pquery.next())
            {
                role = pquery.value(0).toString();
                pname = pquery.value(1).toString();

                if (rstr == role)
                    plist += ", " + pname;
                else
                {
                    // if (rstr != "")
                    //    msg += QString("%1:  %2\n").arg(rstr).arg(plist);
                    // Only print actors, guest star and director for now.

                    if (rstr == "actor")
                        msg += "Actors:  " + plist + "\n";
                    if (rstr == "guest_star")
                        msg += "Guest Star:  " + plist + "\n";
                    if (rstr == "director")
                        msg += "Director:  " + plist + "\n";

                    rstr = role;
                    plist = pname;
                }
            }
            // if (rstr != "")
            //    msg += QString("%1:  %2\n").arg(rstr).arg(plist);

            if (rstr == "actor")
                msg += "Actors:  " + plist + "\n";
            if (rstr == "guest_star")
                msg += "Guest Star:  " + plist + "\n";
            if (rstr == "director")
                msg += "Director:  " + plist + "\n";
        }
    }
    DialogBox *details_dialog = new DialogBox(gContext->GetMainWindow(), msg);
    details_dialog->AddButton(QObject::tr("OK"));
    details_dialog->exec();

    delete details_dialog;
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

        flags |= (query.value(0).toInt() == COMM_FLAG_DONE) ? FL_COMMFLAG : 0;
        flags |= query.value(1).toString().length() > 1 ? FL_CUTLIST : 0;
        flags |= query.value(2).toInt() ? FL_AUTOEXP : 0;
        if ((query.value(3).toInt()) ||
            (query.value(0).toInt() == COMM_FLAG_PROCESSING))
            flags |= FL_EDITING;
        flags |= query.value(4).toString().length() > 1 ? FL_BOOKMARK : 0;
    }

    return flags;
}

void ProgramInfo::handleRecording(QSqlDatabase *db)
{
    QDateTime now = QDateTime::currentDateTime();

    QString message = title;

    if (subtitle != "")
        message += QString(" - \"%1\"").arg(subtitle);

    message += "\n\n";
    message += RecStatusDesc();

    DialogBox diag(gContext->GetMainWindow(), message);
    int button = 1, ok = -1, react = -1, addov = -1, forget = -1, clearov = -1,
        ednorm = -1, edcust = -1;

    diag.AddButton(QObject::tr("OK"));
    ok = button++;

    if (recendts > now &&
        (recstatus == rsStopped ||
         recstatus == rsDeleted))
    {
        diag.AddButton(QObject::tr("Reactivate"));
        react = button++;
    }

    if (recendts > now)
    {
        if (rectype != kSingleRecord && rectype != kOverrideRecord)
        {
            diag.AddButton(QObject::tr("Don't record"));
            addov = button++;
            if (rectype != kFindOneRecord &&
                !programid.contains(QRegExp("^SH.*0000$")) &&
                ((!(dupmethod & kDupCheckNone) && programid != "") ||
                 ((dupmethod & kDupCheckSub) && subtitle != "") ||
                 ((dupmethod & kDupCheckDesc) && description != "")))
            {
                diag.AddButton(QObject::tr("Never record"));
                forget = button++;
            }
        }

        if (rectype != kOverrideRecord && rectype != kDontRecord)
        {
            diag.AddButton(QObject::tr("Edit Options"));
            ednorm = button++;

            if (rectype != kSingleRecord && rectype != kFindOneRecord)
            {
                diag.AddButton(QObject::tr("Add Override"));
                edcust = button++;
            }
        }

        if (rectype == kOverrideRecord || rectype == kDontRecord)
        {
            diag.AddButton(QObject::tr("Edit Override"));
            ednorm = button++;

            diag.AddButton(QObject::tr("Clear Override"));
            clearov = button++;
        }
    }

    int ret = diag.exec();

    if (ret == react)
        RemoteReactivateRecording(this);
    else if (ret == addov)
        ApplyRecordStateChange(db, kDontRecord);
    else if (ret == forget)
    {
        GetProgramRecordingStatus(db);
        record->addHistory(db, *this);
    }
    else if (ret == clearov)
        ApplyRecordStateChange(db, kNotRecording);
    else if (ret == ednorm)
    {
        GetProgramRecordingStatus(db);
        record->exec(db);
    }
    else if (ret == edcust)
    {
        GetProgramRecordingStatus(db);
        record->makeOverride();
        record->exec(db);
    }

    return;
}

void ProgramInfo::handleNotRecording(QSqlDatabase *db)
{
    QString timeFormat = gContext->GetSetting("TimeFormat", "h:mm AP");

    QString message = title;

    if (subtitle != "")
        message += QString(" - \"%1\"").arg(subtitle);

    message += "\n\n";
    message += RecStatusDesc();

    if (recstatus == rsConflict || recstatus == rsLaterShowing)
    {
        vector<ProgramInfo *> *confList = RemoteGetConflictList(this);

        if (confList->size())
            message += QObject::tr(" The following programs will be recorded "
                                   "instead:\n");

        for (int maxi = 0; confList->begin() != confList->end() &&
             maxi < 4; maxi++)
        {
            ProgramInfo *p = *confList->begin();
            message += QString("%1 - %2  %3")
                .arg(p->recstartts.toString(timeFormat))
                .arg(p->recendts.toString(timeFormat)).arg(p->title);
            if (p->subtitle != "")
                message += QString(" - \"%1\"").arg(p->subtitle);
            message += "\n";
            delete p;
            confList->erase(confList->begin());
        }
        message += "\n";
        delete confList;
    }

    DialogBox diag(gContext->GetMainWindow(), message);
    int button = 1, ok = -1, react = -1, addov = -1, clearov = -1,
        ednorm = -1, edcust = -1, forget = -1;

    diag.AddButton(QObject::tr("OK"));
    ok = button++;

    QDateTime now = QDateTime::currentDateTime();

    if (recstartts < now && recendts > now &&
        recstatus != rsDontRecord)
    {
        diag.AddButton(QObject::tr("Reactivate"));
        react = button++;
    }

    if (recendts > now)
    {
        if ((rectype != kSingleRecord && 
             rectype != kFindOneRecord &&
             rectype != kOverrideRecord) &&
            (recstatus == rsDontRecord ||
             recstatus == rsPreviousRecording ||
             recstatus == rsCurrentRecording ||
             recstatus == rsEarlierShowing ||
             recstatus == rsRepeat ||
             recstatus == rsLaterShowing))
        {
            diag.AddButton(QObject::tr("Record anyway"));
            addov = button++;
            if (recstatus == rsPreviousRecording)
            {
                diag.AddButton(QObject::tr("Forget Previous"));
                forget = button++;
            }
        }

        if (rectype != kOverrideRecord && rectype != kDontRecord)
        {
            diag.AddButton(QObject::tr("Edit Options"));
            ednorm = button++;

            if (rectype != kSingleRecord && rectype != kFindOneRecord)
            {
                diag.AddButton(QObject::tr("Add Override"));
                edcust = button++;
            }
        }

        if (rectype == kOverrideRecord || rectype == kDontRecord)
        {
            diag.AddButton(QObject::tr("Edit Override"));
            ednorm = button++;

            diag.AddButton(QObject::tr("Clear Override"));
            clearov = button++;
        }
    }

    int ret = diag.exec();

    if (ret == react)
        RemoteReactivateRecording(this);
    else if (ret == addov)
    {
        ApplyRecordStateChange(db, kOverrideRecord);
        if (recstartts < now)
            RemoteReactivateRecording(this);
    }
    else if (ret == forget)
    {
        GetProgramRecordingStatus(db);
        record->forgetHistory(db, *this);
    }
    else if (ret == clearov)
        ApplyRecordStateChange(db, kNotRecording);
    else if (ret == ednorm)
    {
        GetProgramRecordingStatus(db);
        record->exec(db);
    }
    else if (ret == edcust)
    {
        GetProgramRecordingStatus(db);
        record->makeOverride();
        record->exec(db);
    }

    return;
}

bool ProgramList::FromScheduler(bool &hasConflicts)
{
    clear();
    hasConflicts = false;

    QStringList slist = QString("QUERY_GETALLPENDING");
    if (!gContext->SendReceiveStringList(slist) || slist.size() < 2)
    {
        cerr << "error querying master in ProgramList::FromScheduler" << endl;
        return false;
    }

    hasConflicts = slist[0].toInt();

    bool result = true;
    QStringList::Iterator sit = slist.at(2);

    while (result && sit != slist.end())
    {
        ProgramInfo *p = new ProgramInfo();
        result = p->FromStringList(slist, sit);
        if (result)
            append(p);
        else
            delete p;
    }

    if (count() != slist[1].toUInt())
    {
        cerr << "length mismatch in ProgramList::FromScheduler" << endl;
        result = false;
    }

    return result;
}

bool ProgramList::FromProgram(QSqlDatabase *db, const QString sql,
                              ProgramList &schedList)
{
    clear();

    QString querystr = QString(
        "SELECT program.chanid, program.starttime, program.endtime, "
        "    program.title, program.subtitle, program.description, "
        "    program.category, channel.channum, channel.callsign, "
        "    channel.name, program.previouslyshown, channel.commfree, "
        "    channel.outputfilters, program.seriesid, program.programid, "
        "    program.stars, program.originalairdate "
        "FROM program "
        "LEFT JOIN channel ON program.chanid = channel.chanid "
        "%1 ").arg(sql);

            

                
    if (!sql.contains(" GROUP BY "))
        querystr += "GROUP BY program.starttime, channel.channum, "
            "  channel.callsign ";
    if (!sql.contains(" ORDER BY "))
        querystr += QString("ORDER BY program.starttime, %2 ")
            .arg(gContext->GetSetting("ChannelOrdering", "channum+0"));
    if (!sql.contains(" LIMIT "))
        querystr += "LIMIT 1000 ";

    QSqlQuery query = db->exec(querystr);
    if (!query.isActive())
    {
        MythContext::DBError("ProgramList::FromProgram", querystr);
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
        p->title = QString::fromUtf8(query.value(3).toString());
        p->subtitle = QString::fromUtf8(query.value(4).toString());
        p->description = QString::fromUtf8(query.value(5).toString());
        p->category = QString::fromUtf8(query.value(6).toString());
        p->chanstr = query.value(7).toString();
        p->chansign = QString::fromUtf8(query.value(8).toString());
        p->channame = QString::fromUtf8(query.value(9).toString());
        p->repeat = query.value(10).toInt();
        p->chancommfree = query.value(11).toInt();
        p->chanOutputFilters = query.value(12).toString();
        p->seriesid = query.value(13).toString();
        p->programid = query.value(14).toString();

        p->stars = query.value(15).toString().toFloat();
        
        
        
        if(query.value(16).isNull() || query.value(16).toString().isEmpty())
        {
            p->originalAirDate = p->startts.date();
            p->hasAirDate = false;
        }
        else
        {
            p->originalAirDate = QDate::fromString(query.value(16).toString(),
                                                   Qt::ISODate);
            p->hasAirDate = true;
        }

        ProgramInfo *s;
        for (s = schedList.first(); s; s = schedList.next())
        {
            if (p->IsSameTimeslot(*s))
            {
                p->recordid = s->recordid;
                p->recstatus = s->recstatus;
                p->rectype = s->rectype;
                p->recstartts = s->recstartts;
                p->recendts = s->recendts;
                p->cardid = s->cardid;
                p->inputid = s->inputid;
                p->dupin = s->dupin;
                p->dupmethod = s->dupmethod;
            }
        }

        append(p);
    }

    return true;
}

bool ProgramList::FromRecorded(QSqlDatabase *, const QString, ProgramList&)
{
    clear();
    return false;
}

bool ProgramList::FromRecord(QSqlDatabase *, const QString)
{
    clear();
    return false;
}

int ProgramList::compareItems(ProgramInfo *p1, ProgramInfo *p2)
{
    if (compareFunc)
        return compareFunc(p1, p2);
    else
        return 0;
}

