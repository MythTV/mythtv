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

    pathname = "";
    filesize = 0;
    hostname = "";

    startts = QDateTime::currentDateTime();
    endts = startts;
    recstartts = startts;
    recendts = startts;

    override = 0;
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
    chancommfree = other.chancommfree;
    pathname = other.pathname;
    filesize = other.filesize;
    hostname = other.hostname;

    startts = other.startts;
    endts = other.endts;
    recstartts = other.recstartts;
    recendts = other.recendts;
    spread = other.spread;
    startCol = other.startCol;
 
    override = other.override;
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
    chancommfree = other.chancommfree;
    pathname = other.pathname;
    filesize = other.filesize;
    hostname = other.hostname;

    startts = other.startts;
    endts = other.endts;
    recstartts = other.recstartts;
    recendts = other.recendts;
    spread = other.spread;
    startCol = other.startCol;

    override = other.override;
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
    list << QString(" "); // dummy place holder
    list << QString::number(shareable);
    list << QString::number(override);
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
    it++; // dummy place holder
    shareable = (*(it++)).toInt();
    override = (*(it++)).toInt();
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
    progMap["commfree"] = chancommfree;
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

    if (gContext->GetNumSetting("DisplayChanNum") != 0)
    {
        if (channame != chansign)
            progMap["channel"] = channame + " [" + chansign + "]";
        else
            progMap["channel"] = channame;
    }
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
                       "channel.callsign,channel.name,previouslyshown,"
                       "channel.commfree "
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
            proginfo->chancommfree = query.value(11).toInt();

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

    where = QString("WHERE program.chanid = %1 AND endtime >= %2 AND "
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
                       "channel.name,previouslyshown,channel.commfree "
                       "FROM program,channel "
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
        proginfo->chancommfree = query.value(11).toInt();
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
                       "channel.callsign,channel.name,channel.commfree "
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
        proginfo->chancommfree = query.value(9).toInt();
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
        case kFindOneRecord:
            return gContext->GetNumSetting("FindOneRecordRecPriority", 0);
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

    QString querystr = QString("UPDATE recorded SET recgroup = '%1', "
                               "starttime = '%2' WHERE chanid = '%3' AND "
                               "starttime = '%4';").arg(newrecgroup).arg(starts)
                                                   .arg(chanid).arg(starts);
    QSqlQuery query = db->exec(querystr);
    if (!query.isActive())
        MythContext::DBError("RecGroup update", querystr);

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

    QString sqltitle = title;
    QString sqlsubtitle = subtitle;
    QString sqldescription = description;
    QString sqlcategory = category;
    QString sqlrecgroup = recgroup;

    sqltitle.replace(QRegExp("\""), QString("\\\""));
    sqlsubtitle.replace(QRegExp("\""), QString("\\\""));
    sqldescription.replace(QRegExp("\""), QString("\\\""));
    sqlcategory.replace(QRegExp("\""), QString("\\\""));
    sqlrecgroup.replace(QRegExp("\""), QString("\\\""));

    QString query;
    query = QString("INSERT INTO recorded (chanid,starttime,endtime,title,"
                    "subtitle,description,hostname,category,recgroup,"
                    "autoexpire,recordid) "
                    "VALUES(%1,\"%2\",\"%3\",\"%4\",\"%5\",\"%6\",\"%7\","
                        "\"%8\",\"%9\"")
                    .arg(chanid).arg(starts).arg(ends).arg(sqltitle.utf8()) 
                    .arg(sqlsubtitle.utf8()).arg(sqldescription.utf8())
                    .arg(gContext->GetHostName()).arg(sqlcategory.utf8())
                    .arg(sqlrecgroup.utf8());
    query += QString(",%1,%2);")
                    .arg(record->GetAutoExpire()).arg(recordid);

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

void ProgramInfo::FinishedRecording(QSqlDatabase* db, bool prematurestop) 
{
    GetProgramRecordingStatus(db);
    if (!prematurestop)
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
       "recordid = %1 AND station = \"%2\" AND starttime = %3 AND endtime = %4 AND "
       "title = \"%5\" AND subtitle = \"%6\" AND description = \"%7\";")
        .arg(recordid).arg(chansign.utf8())
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
            "title = \"%6\", subtitle = \"%7\", description = \"%8\", " 
            "station = \"%9\";")
            .arg(override).arg(recordid).arg(chanid)
            .arg(startts.toString("yyyyMMddhhmmss").ascii())
            .arg(endts.toString("yyyyMMddhhmmss").ascii())
            .arg(sqltitle.utf8()).arg(sqlsubtitle.utf8())
            .arg(sqldescription.utf8()).arg(chansign.utf8());
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
        recstring = QObject::tr("S");
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
    case kFindOneRecord:
        recstring = QObject::tr("F");
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
    case kFindOneRecord:
        recstring = QObject::tr("Find One Recording");
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
        return "R";
    case rsEarlierShowing:
        return "E";
    case rsTooManyRecordings:
        return "T";
    case rsCancelled:
        return "N";
    case rsConflict:
        return "C";
    case rsLaterShowing:
        return "L";
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
        case rsEarlierShowing:
            message += QObject::tr("this episode will be recorded at an "
                                   "earlier time instead.");
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
                                   "will be recorded instead.");
            break;
        case rsLaterShowing:
            message += QObject::tr("this episode will be recorded at a "
                                   "later time instead.");
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
        override = found->override;
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
    else if (recstatus <= rsWillRecord)
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
    QDateTime now = QDateTime::currentDateTime();

    QString message = title;

    if (subtitle != "")
        message += QString(" - \"%1\"").arg(subtitle);

    message += "\n\n";
    message += RecStatusDesc();

    DialogBox diag(gContext->GetMainWindow(), message);
    int button = 1, ok = -1, react = -1, addov = -1, forget = -1, clearov = -1;

    diag.AddButton(QObject::tr("OK"));
    ok = button++;

    if (recendts > now &&
        (recstatus == rsStopped ||
         recstatus == rsDeleted))
    {
        diag.AddButton(QObject::tr("Reactivate it"));
        react = button++;
    }

    if (recstartts > now && recstatus == rsWillRecord)
    {
        diag.AddButton(QObject::tr("Don't record it"));
        addov = button++;
        if ((dupmethod & kDupCheckNone) ||
            ((dupmethod & kDupCheckSub) &&
             (subtitle != "")) ||
            ((dupmethod & kDupCheckDesc) &&
             (description != "")))
        {
            diag.AddButton(QObject::tr("Never record this episode"));
            forget = button++;
        }
    }

    if (recendts > now && override != 0)
    {
        diag.AddButton(QObject::tr("Clear Override"));
        clearov = button++;
    }

    int ret = diag.exec();

    if (ret == react)
        RemoteReactivateRecording(this);
    else if (ret == addov)
        setOverride(db, 2);
    else if (ret == forget)
    {
        setOverride(db, 2);
        if (record == NULL) 
        {
            record = new ScheduledRecording();
            record->loadByProgram(db, this);
        }
        record->addHistory(db, *this);
    }
    if (ret == clearov)
        setOverride(db, 0);

    return;
}

void ProgramInfo::handleNotRecording(QSqlDatabase *db)
{
    QString message = title;

    if (subtitle != "")
        message += QString(" - \"%1\"").arg(subtitle);

    message += "\n\n";
    message += RecStatusDesc();

    if (recstatus == rsConflict || recstatus == rsLaterShowing)
    {
        vector<ProgramInfo *> *confList = RemoteGetConflictList(this);

        if (confList->size())
        {
            message += "\n\n";
            message += QObject::tr("The following programs will be recorded "
                                   "instead:\n");
        }

        while (confList->begin() != confList->end())
        {
            ProgramInfo *p = *confList->begin();
            message += p->title;
            if (p->subtitle != "")
                message += QString(" - \"%1\"").arg(p->subtitle);
            message += QString(", %1 %2, %3 - %4\n")
                .arg(p->chanstr).arg(p->chansign)
                .arg(p->recstartts.toString("hh:mm"))
                .arg(p->recendts.toString("hh:mm"));
            delete p;
            confList->erase(confList->begin());
        }
        delete confList;
    }

    DialogBox diag(gContext->GetMainWindow(), message);
    int button = 1, ok = -1, react = -1, addov = -1, clearov = -1;

    diag.AddButton(QObject::tr("OK"));
    ok = button++;

    QDateTime now = QDateTime::currentDateTime();

    if (recstartts < now && recendts > now &&
        recstatus != rsManualOverride)
    {
        diag.AddButton(QObject::tr("Reactivate it"));
        react = button++;
    }

    if (recendts > now &&
        override != 1 &&
        (recstatus == rsManualOverride ||
         recstatus == rsPreviousRecording ||
         recstatus == rsCurrentRecording ||
         recstatus == rsEarlierShowing ||
         recstatus == rsConflict ||
         recstatus == rsLaterShowing))
    {
        diag.AddButton(QObject::tr("Record it anyway"));
        addov = button++;
    }

    if (recendts > now && override != 0)
    {
        diag.AddButton(QObject::tr("Clear Override"));
        clearov = button++;
    }

    int ret = diag.exec();

    if (ret == react)
        RemoteReactivateRecording(this);
    else if (ret == addov)
    {
        setOverride(db, 1);
        if (recstartts < now)
            RemoteReactivateRecording(this);
    }
    else if (ret == clearov)
        setOverride(db, 0);

    return;
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

