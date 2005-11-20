#include <iostream>
#include <qsocket.h>
#include <qregexp.h>
#include <qmap.h>
#include <qlayout.h>
#include <qlabel.h>
#include <qapplication.h>
#include <qfile.h>

#include <sys/types.h>
#include <unistd.h>

#include "programinfo.h"
#include "scheduledrecording.h"
#include "util.h"
#include "mythcontext.h"
#include "dialogbox.h"
#include "remoteutil.h"
#include "jobqueue.h"
#include "mythdbcon.h"

using namespace std;

static bool insert_program(MSqlQuery&,
                           const ProgramInfo*,
                           const ScheduledRecording*);

/** \fn StripHTMLTags(const QString&)
 *  \brief Returns a copy of "src" with all the HTML tags removed.
 *
 *   Three tags a respected: <br> and <p> are replaced with newlines,
 *   and <li> is replaced with a newline followed by "- ".
 *  \param src String to be processed.
 *  \return Stripped string.
 */
static QString StripHTMLTags(const QString& src)
{
    QString dst(src);

    // First replace some tags with some ASCII formatting
    dst.replace( QRegExp("<br[^>]*>"), "\n" );
    dst.replace( QRegExp("<p[^>]*>"),  "\n" );
    dst.replace( QRegExp("<li[^>]*>"), "\n- " );
    // And finally remve any remaining tags
    dst.replace( QRegExp("<[^>]*>"), "" );

    return dst;
}

/** \class ProgramInfo
 *  \brief Holds information on a %TV Program one might wish to record.
 *
 *  ProgramInfo can also contain partial information for a program we wish to
 *  find in the schedule, and may also contain information on a video we
 *  wish to view. This class is serializable from frontend to backend and
 *  back and is the basic unit of information on anything we may wish to
 *  view or record.
 */

/** \fn ProgramInfo::ProgramInfo(void)
 *  \brief Null constructor.
 */
ProgramInfo::ProgramInfo(void)
{
    spread = -1;
    startCol = -1;
    isVideo = false;
    lenMins = 0;
    chanstr = "";
    chansign = "";
    channame = "";
    chancommfree = 0;
    chanOutputFilters = "";
    year = "";
    stars = 0;
    availableStatus = asAvailable;    
    timestretch = 1.0;

    pathname = "";
    filesize = 0;
    hostname = "";
    programflags = 0;
    transcoder = 0;

    startts = QDateTime::currentDateTime();
    endts = startts;
    recstartts = startts;
    recendts = startts;
    originalAirDate = startts.date();
    lastmodified = startts;
    lastInUseTime = QDateTime::currentDateTime().addSecs(-4 * 60 * 60);

    recstatus = rsUnknown;
    oldrecstatus = rsUnknown;
    savedrecstatus = rsUnknown;
    recpriority2 = 0;
    reactivate = false;
    recordid = 0;
    parentid = 0;
    rectype = kNotRecording;
    dupin = kDupsInAll;
    dupmethod = kDupCheckSubDesc;

    sourceid = 0;
    inputid = 0;
    cardid = 0;
    shareable = false;
    duplicate = false;
    schedulerid = "";
    findid = 0;
    recpriority = 0;
    recgroup = QString("Default");

    hasAirDate = false;
    repeat = false;

    seriesid = "";
    programid = "";
    ignoreBookmark = false;
    catType = "";

    sortTitle = "";

    inUseForWhat = "";

    record = NULL;
}   

/** \fn ProgramInfo::ProgramInfo(const ProgramInfo &other) 
 *  \brief Copy constructor.
 */
ProgramInfo::ProgramInfo(const ProgramInfo &other)
{           
    record = NULL;
    
    clone(other);
}

/** \fn ProgramInfo::operator=(const ProgramInfo &other) 
 *  \brief Copies important fields from other ProgramInfo.
 */
ProgramInfo &ProgramInfo::operator=(const ProgramInfo &other) 
{ 
    return clone(other); 
}

/** \fn ProgramInfo::clone(const ProgramInfo &other) 
 *  \brief Copies important fields from other ProgramInfo.
 */
ProgramInfo &ProgramInfo::clone(const ProgramInfo &other)
{
    if (record)
        delete record;
    
    isVideo = other.isVideo;
    lenMins = other.lenMins;
    
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

    availableStatus = other.availableStatus;

    recstatus = other.recstatus;
    oldrecstatus = other.oldrecstatus;
    savedrecstatus = other.savedrecstatus;
    recpriority2 = other.recpriority2;
    reactivate = other.reactivate;
    recordid = other.recordid;
    parentid = other.parentid;
    rectype = other.rectype;
    dupin = other.dupin;
    dupmethod = other.dupmethod;

    sourceid = other.sourceid;
    inputid = other.inputid;
    cardid = other.cardid;
    shareable = other.shareable;
    duplicate = other.duplicate;
    schedulerid = other.schedulerid;
    findid = other.findid;
    recpriority = other.recpriority;
    recgroup = other.recgroup;
    programflags = other.programflags;
    transcoder = other.transcoder;

    hasAirDate = other.hasAirDate;
    repeat = other.repeat;

    timestretch = other.timestretch;

    seriesid = other.seriesid;
    programid = other.programid;
    catType = other.catType;

    sortTitle = other.sortTitle;

    originalAirDate = other.originalAirDate;
    stars = other.stars;
    year = other.year;
    ignoreBookmark = other.ignoreBookmark; 
   
    inUseForWhat = other.inUseForWhat;
    lastInUseTime = other.lastInUseTime;
    record = NULL;

    return *this;
}

/** \fn ProgramInfo::~ProgramInfo() 
 *  \brief Destructor deletes "record" if it exists.
 */
ProgramInfo::~ProgramInfo() 
{
    if (record != NULL)
        delete record;
}

/** \fn ProgramInfo::MakeUniqueKey(void) const
 *  \brief Creates a unique string that can be used to identify a recording.
 */
QString ProgramInfo::MakeUniqueKey(void) const
{
    return chanid + "_" + recstartts.toString(Qt::ISODate);
}

#define INT_TO_LIST(x)       sprintf(tmp, "%i", (x)); list << tmp;

#define DATETIME_TO_LIST(x)  INT_TO_LIST((x).toTime_t())

#define LONGLONG_TO_LIST(x)  INT_TO_LIST((int)((x) >> 32))  \
                             INT_TO_LIST((int)((x) & 0xffffffffLL))

#define STR_TO_LIST(x)       if ((x).isNull()) list << ""; else list << (x);

#define FLOAT_TO_LIST(x)     sprintf(tmp, "%f", (x)); list << tmp;

/** \fn ProgramInfo::ToStringList(QStringList&) const
 *  \brief Serializes ProgramInfo into a QStringList which can be passed
 *         over a socket.
 *  \sa FromStringList(QStringList&,int),
 *      FromStringList(QStringList&,QStringList::iterator&)
 */
void ProgramInfo::ToStringList(QStringList &list) const
{
    char tmp[64];

    STR_TO_LIST(title)
    STR_TO_LIST(subtitle)
    STR_TO_LIST(description)
    STR_TO_LIST(category)
    STR_TO_LIST(chanid)
    STR_TO_LIST(chanstr)
    STR_TO_LIST(chansign)
    STR_TO_LIST(channame)
    STR_TO_LIST(pathname)
    LONGLONG_TO_LIST(filesize)

    DATETIME_TO_LIST(startts)
    DATETIME_TO_LIST(endts)
    INT_TO_LIST(duplicate)
    INT_TO_LIST(shareable)
    INT_TO_LIST(findid);
    STR_TO_LIST(hostname)
    INT_TO_LIST(sourceid)
    INT_TO_LIST(cardid)
    INT_TO_LIST(inputid)
    INT_TO_LIST(recpriority)
    INT_TO_LIST(recstatus)
    INT_TO_LIST(recordid)
    INT_TO_LIST(rectype)
    INT_TO_LIST(dupin)
    INT_TO_LIST(dupmethod)
    DATETIME_TO_LIST(recstartts)
    DATETIME_TO_LIST(recendts)
    INT_TO_LIST(repeat)
    INT_TO_LIST(programflags)
    STR_TO_LIST((recgroup != "") ? recgroup : "Default")
    INT_TO_LIST(chancommfree)
    STR_TO_LIST(chanOutputFilters)
    STR_TO_LIST(seriesid)
    STR_TO_LIST(programid)
    DATETIME_TO_LIST(lastmodified)
    FLOAT_TO_LIST(stars)
    DATETIME_TO_LIST(QDateTime(originalAirDate))
    INT_TO_LIST(hasAirDate)     
    FLOAT_TO_LIST(timestretch);
}

/** \fn ProgramInfo::FromStringList(QStringList&,int)
 *  \brief Uses a QStringList to initialize this ProgramInfo instance.
 *  \param list   QStringList containing serialized ProgramInfo.
 *  \param offset First field in list to treat as beginning of
 *                serialized ProgramInfo.
 *  \return true if it succeeds, false if it fails.
 *  \sa ToStringList(QStringList&),
 *      FromStringList(QStringList&,QStringList::iterator&)
 */
bool ProgramInfo::FromStringList(QStringList &list, int offset)
{
    QStringList::iterator it = list.at(offset);
    return FromStringList(list, it);
}

#define NEXT_STR()             if (it == listend)     \
                               {                      \
                                   VERBOSE(VB_IMPORTANT, listerror); \
                                   return false;      \
                               }                      \
                               ts = *it++;            \
                               if (ts.isNull())       \
                                   ts = "";           
                               
#define INT_FROM_LIST(x)       NEXT_STR() (x) = atoi(ts.ascii());
#define ENUM_FROM_LIST(x, y)   NEXT_STR() (x) = (y)atoi(ts.ascii());

#define DATETIME_FROM_LIST(x)  NEXT_STR() (x).setTime_t((uint)atoi(ts.ascii()));
#define DATE_FROM_LIST(x)      DATETIME_FROM_LIST(td); (x) = td.date();

#define LONGLONG_FROM_LIST(x)  INT_FROM_LIST(ti); NEXT_STR() \
                               (x) = ((long long)(ti) << 32) | \
                               ((long long)(atoi(ts.ascii())) & 0xffffffffLL);

#define STR_FROM_LIST(x)       NEXT_STR() (x) = ts;

#define FLOAT_FROM_LIST(x)     NEXT_STR() (x) = atof(ts.ascii());

/** \fn ProgramInfo::FromStringList(QStringList&,QStringList::iterator&)
 *  \brief Uses a QStringList to initialize this ProgramInfo instance.
 *  \param list   QStringList containing serialized ProgramInfo.
 *  \param it     Iterator pointing to first field in list to treat as
 *                beginning of serialized ProgramInfo.
 *  \return true if it succeeds, false if it fails.
 *  \sa ToStringList(QStringList&), FromStringList(QStringList&,int)
 */
bool ProgramInfo::FromStringList(QStringList &list, QStringList::iterator &it)
{
    const char* listerror = "ProgramInfo::FromStringList, not enough items"
                            " in list. \n"; 
    QStringList::iterator listend = list.end();
    QString ts;
    QDateTime td;
    int ti;

    STR_FROM_LIST(title)
    STR_FROM_LIST(subtitle)
    STR_FROM_LIST(description)
    STR_FROM_LIST(category)
    STR_FROM_LIST(chanid)
    STR_FROM_LIST(chanstr)
    STR_FROM_LIST(chansign)
    STR_FROM_LIST(channame)
    STR_FROM_LIST(pathname)
    LONGLONG_FROM_LIST(filesize)

    DATETIME_FROM_LIST(startts)
    DATETIME_FROM_LIST(endts)
    NEXT_STR() // dummy place holder
    INT_FROM_LIST(shareable)
    INT_FROM_LIST(findid)
    STR_FROM_LIST(hostname)
    INT_FROM_LIST(sourceid)
    INT_FROM_LIST(cardid)
    INT_FROM_LIST(inputid)
    INT_FROM_LIST(recpriority)
    ENUM_FROM_LIST(recstatus, RecStatusType)
    INT_FROM_LIST(recordid)
    ENUM_FROM_LIST(rectype, RecordingType)
    ENUM_FROM_LIST(dupin, RecordingDupInType)
    ENUM_FROM_LIST(dupmethod, RecordingDupMethodType)
    DATETIME_FROM_LIST(recstartts)
    DATETIME_FROM_LIST(recendts)
    INT_FROM_LIST(repeat)
    INT_FROM_LIST(programflags)
    STR_FROM_LIST(recgroup)
    INT_FROM_LIST(chancommfree)
    STR_FROM_LIST(chanOutputFilters)
    STR_FROM_LIST(seriesid)
    STR_FROM_LIST(programid)
    DATETIME_FROM_LIST(lastmodified)
    FLOAT_FROM_LIST(stars)
    DATE_FROM_LIST(originalAirDate);
    INT_FROM_LIST(hasAirDate);
    FLOAT_FROM_LIST(timestretch);

    return true;
}

/** \fn ProgramInfo::ToMap(QMap<QString, QString> &progMap,
 *                         bool showrerecord) const
 *  \brief Converts ProgramInfo into QString QMap containing each field
 *         in ProgramInfo converted into lockalized strings.
 */
void ProgramInfo::ToMap(QMap<QString, QString> &progMap, 
                        bool showrerecord) const
{
    QString timeFormat = gContext->GetSetting("TimeFormat", "h:mm AP");
    QString dateFormat = gContext->GetSetting("DateFormat", "ddd MMMM d");
    QString fullDateFormat = dateFormat;
    if (fullDateFormat.find(QRegExp("yyyy")) < 0)
        fullDateFormat += " yyyy";
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
    progMap["description"] = StripHTMLTags(description);
    progMap["category"] = category;
    progMap["callsign"] = chansign;
    progMap["commfree"] = chancommfree;
    progMap["outputfilters"] = chanOutputFilters;
    if (isVideo)
    {
        progMap["starttime"] = "";
        progMap["startdate"] = "";
        progMap["endtime"] = "";
        progMap["enddate"] = "";
        progMap["recstarttime"] = "";
        progMap["recstartdate"] = "";
        progMap["recendtime"] = "";
        progMap["recenddate"] = "";
        
        if (startts.date().year() == 1895)
        {
           progMap["startdate"] = "?";
           progMap["recstartdate"] = "?";
        }
        else
        {
            progMap["starttime"] = startts.toString("yyyy");
            progMap["recstarttime"] = startts.toString("yyyy");
        }
        
    }
    else
    {
        progMap["starttime"] = startts.toString(timeFormat);
        progMap["startdate"] = startts.toString(shortDateFormat);
        progMap["endtime"] = endts.toString(timeFormat);
        progMap["enddate"] = endts.toString(shortDateFormat);
        progMap["recstarttime"] = recstartts.toString(timeFormat);
        progMap["recstartdate"] = recstartts.toString(shortDateFormat);
        progMap["recendtime"] = recendts.toString(timeFormat);
        progMap["recenddate"] = recendts.toString(shortDateFormat);
    }
    
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

    if (isVideo)
    {
        minutes = lenMins;
        seconds = lenMins * 60;
    }
    else
    {
        seconds = recstartts.secsTo(recendts);
        minutes = seconds / 60;
    }
    
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
        if (recendts > timeNow && recstatus <= rsWillRecord || 
            recstatus == rsConflict || recstatus == rsLaterShowing)
        {
            if (recpriority >= 0)       
                progMap["rec_str"] += QString(" +%1 ").arg(recpriority);
            else
                progMap["rec_str"] += QString(" %1 ").arg(recpriority);
        }
        else
        {
            progMap["rec_str"] += " -- ";
        }
        if (showrerecord && recstatus == rsRecorded && !duplicate)
            progMap["rec_str"] += QObject::tr("Re-Record");
        else
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

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT icon FROM channel WHERE chanid = :CHANID ;");
    query.bindValue(":CHANID", chanid);
        
    if (query.exec() && query.isActive() && query.size() > 0)
        if (query.next())
            progMap["iconpath"] = query.value(0).toString();

    progMap["RECSTATUS"] = RecStatusText();

    if (repeat)
    {
        progMap["REPEAT"] = QString("(%1) ").arg(QObject::tr("Repeat"));
        progMap["LONGREPEAT"] = QString("(%1 %2) ")
                                .arg(QObject::tr("Repeat"))
                                .arg(originalAirDate.toString(fullDateFormat));
    }
    else
    {
        progMap["REPEAT"] = "";
        progMap["LONGREPEAT"] = "";
    }

    progMap["timestretch"] = QString::number(timestretch, 'f', 2);
   
    progMap["seriesid"] = seriesid;
    progMap["programid"] = programid;
    progMap["catType"] = catType;

    progMap["year"] = year;
    
    if (stars)
    {
        QString str = QObject::tr("stars");
        if (stars > 0 && stars <= 0.25)
            str = QObject::tr("star");

        if (year != "")
            progMap["stars"] = QString("(%1, %2 %3) ")
                                       .arg(year).arg(4.0 * stars).arg(str);
        else
            progMap["stars"] = QString("(%1 %2) ").arg(4.0 * stars).arg(str);
    }
    else
        progMap["stars"] = "";
        
    progMap["originalairdate"]= originalAirDate.toString(dateFormat);
    progMap["shortoriginalairdate"]= originalAirDate.toString(shortDateFormat);
    
}

/** \fn ProgramInfo::CalculateLength(void) const
 *  \brief Returns length of program/recording in seconds.
 */
int ProgramInfo::CalculateLength(void) const
{
    if (isVideo)
        return lenMins * 60;
    else
        return startts.secsTo(endts);
}

/** \fn ProgramInfo::SecsTillStart() const
 *  \brief Returns number of seconds until the program starts,
 *         including a negative number if program was in the past.
 */
int ProgramInfo::SecsTillStart(void) const
{
    return QDateTime::currentDateTime().secsTo(startts);
}


/** \fn ProgramInfo::GetProgramAtDateTime(const QString&, const QDateTime&)
 *  \brief Returns a new ProgramInfo for the program that air at
 *         "dtime" on "channel".
 *  \param channel %Channel ID on which to search for program.
 *  \param dtime   Date and Time for which we desire the program.
 *  \param genUnknown Generate a full entry for live-tv if unknown
 *  \return Pointer to a ProgramInfo from database if it succeeds,
 *          Pointer to an "Unknown" ProgramInfo if it does not find
 *          anything in database.
 */
ProgramInfo *ProgramInfo::GetProgramAtDateTime(const QString &channel, 
                                               const QDateTime &dtime,
                                               bool genUnknown)
{
    ProgramList schedList;
    ProgramList progList;

    MSqlBindings bindings;
    QString querystr = "WHERE program.chanid = :CHANID "
                       "  AND program.starttime < :STARTTS "
                       "  AND program.endtime > :STARTTS ";
    bindings[":CHANID"] = channel;
    bindings[":STARTTS"] = dtime.toString("yyyy-MM-ddThh:mm:50");

    schedList.FromScheduler();
    progList.FromProgram(querystr, bindings, schedList);

    if (!progList.isEmpty())
        return progList.take(0);

    ProgramInfo *p = new ProgramInfo;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT chanid, channum, callsign, name, "
                  "commfree, outputfilters "
                  "FROM channel "
                  "WHERE chanid = :CHANID ;");
    query.bindValue(":CHANID", channel);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("ProgramInfo::GetProgramAtDateTime", 
                             query);
        return p;
    }

    if (!query.next())
        return p;

    p->chanid             = query.value(0).toString();
    p->startts            = dtime;
    p->endts              = dtime;
    p->recstartts         = p->startts;
    p->recendts           = p->endts;
    p->lastmodified       = p->startts;
    p->title              = gContext->GetSetting("UnknownTitle");
    p->subtitle           = "";
    p->description        = "";
    p->category           = "";
    p->chanstr            = query.value(1).toString();
    p->chansign           = QString::fromUtf8(query.value(2).toString());
    p->channame           = QString::fromUtf8(query.value(3).toString());
    p->repeat             = 0;
    p->chancommfree       = query.value(4).toInt();
    p->chanOutputFilters  = query.value(5).toString();
    p->seriesid           = "";
    p->programid          = "";
    p->year               = "";
    p->stars              = 0.0f;

    if (!genUnknown)
        return p;

    // Round endtime up to the next half-hour.
    if (p->endts.time().minute() < 30)
        p->endts.setTime(QTime(p->endts.time().hour(), 30));
    else
    {
        if (p->endts.time().hour() == 23)
        {
            p->endts = p->endts.addDays(1);
            p->endts.setTime(QTime(0, 0));
        }
        else
            p->endts.setTime(QTime(p->endts.time().hour() + 1, 0));
    }

    // if under a minute, bump it up to the next half hour
    if (p->startts.secsTo(p->endts) < 60)
        p->endts = p->endts.addSecs(30 * 60);

    p->recendts = p->endts;

    // Find next program starttime
    QDateTime nextstart = p->startts;
    querystr = "WHERE program.chanid    = :CHANID  AND "
               "      program.starttime > :STARTTS "
               "GROUP BY program.starttime ORDER BY program.starttime LIMIT 1 ";
    bindings[":CHANID"]  = channel;
    bindings[":STARTTS"] = dtime.toString("yyyy-MM-ddThh:mm:50");

    progList.FromProgram(querystr, bindings, schedList);

    if (!progList.isEmpty())
        nextstart = progList.at(0)->startts;

    if (nextstart > p->startts && nextstart < p->recendts)
    {
        p->recendts = p->endts = nextstart;
    }

    return p;
}

QString ProgramInfo::toString(void) const
{
    QString str("");
    str += "ProgramInfo: channame(" + channame + ") startts(" +
        startts.toString() + ") endts(" + endts.toString() + ")\n";
    str += "             recstartts(" + recstartts.toString() +
        ") recendts(" + recendts.toString() + ")\n";
    str += "             title(" + title + ")";
    return str;
}

/** \fn ProgramInfo::GetProgramFromRecorded(const QString&, const QDateTime&)
 *  \brief Returns a new ProgramInfo for an existing recording.
 *  \return Pointer to a ProgramInfo if it succeeds, NULL otherwise.
 */
ProgramInfo *ProgramInfo::GetProgramFromRecorded(const QString &channel, 
                                                 const QDateTime &dtime)
{
    return GetProgramFromRecorded(channel, dtime.toString(Qt::ISODate));
}

/** \fn ProgramInfo::GetProgramFromRecorded(const QString&, const QString&)
 *  \brief Returns a new ProgramInfo for an existing recording.
 *  \return Pointer to a ProgramInfo if it succeeds, NULL otherwise.
 */
ProgramInfo *ProgramInfo::GetProgramFromRecorded(const QString &channel, 
                                                 const QString &starttime)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT recorded.chanid,starttime,endtime,title, "
                  "subtitle,description,channel.channum, "
                  "channel.callsign,channel.name,channel.commfree, "
                  "channel.outputfilters,seriesid,programid,filesize, "
                  "lastmodified,stars,previouslyshown,originalairdate, "
                  "hostname,recordid,transcoder,timestretch, "
                  "recorded.recpriority,progstart,progend,basename "
                  "FROM recorded "
                  "LEFT JOIN channel "
                  "ON recorded.chanid = channel.chanid "
                  "WHERE recorded.chanid = :CHANNEL "
                  "AND starttime = :STARTTIME ;");
    query.bindValue(":CHANNEL", channel);
    query.bindValue(":STARTTIME", starttime);
    
    if (query.exec() && query.isActive() && query.size() > 0)
    {
        query.next();

        ProgramInfo *proginfo = new ProgramInfo;
        proginfo->chanid = query.value(0).toString();
        proginfo->startts = query.value(23).toDateTime();
        proginfo->endts = query.value(24).toDateTime();
        proginfo->recstartts = query.value(1).toDateTime();
        proginfo->recendts = query.value(2).toDateTime();
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
        
        proginfo->stars = query.value(15).toDouble();
        proginfo->repeat = query.value(16).toInt();
        
        if (query.value(17).isNull() || query.value(17).toString().isEmpty())
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
        proginfo->recordid = query.value(19).toInt();
        proginfo->transcoder = query.value(20).toInt();

        proginfo->spread = -1;

        proginfo->programflags = proginfo->getProgramFlags();

        proginfo->timestretch = query.value(21).toString().toFloat();
        proginfo->recpriority = query.value(22).toInt();

        proginfo->pathname = query.value(25).toString();

        return proginfo;
    }

    return NULL;
}

/** \fn ProgramInfo::IsFindApplicable(void) const
 *  \brief Returns true if a search should be employed to find a matcing program.
 */
bool ProgramInfo::IsFindApplicable(void) const
{
    return rectype == kFindDailyRecord ||
           rectype == kFindWeeklyRecord;
}

/** \fn ProgramInfo::IsProgramRecurring(void) const
 *  \brief Returns 0 if program is not recurring, 1 if recurring daily on weekdays,
 *         2 if recurring weekly, and -1 when there is insufficient data.
 */
int ProgramInfo::IsProgramRecurring(void) const
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

/** \fn ProgramInfo::GetProgramRecordingStatus()
 *  \brief Returns the recording type for this ProgramInfo, creating
 *         "record" field if necessary.
 *  \sa RecordingType, ScheduledRecording
 */
RecordingType ProgramInfo::GetProgramRecordingStatus(void)
{
    if (record == NULL) 
    {
        record = new ScheduledRecording();
        record->loadByProgram(this);
    }

    return record->getRecordingType();
}

/** \fn ProgramInfo::GetProgramRecordingProfile()
 *  \brief Returns recording profile name that will be, or was used,
 *         for this program, creating "record" field if necessary.
 *  \sa ScheduledRecording
 */
QString ProgramInfo::GetProgramRecordingProfile(void)
{
    if (record == NULL)
    {
        record = new ScheduledRecording();
        record->loadByProgram(this);
    }

    return record->getProfileName();
}

/** \fn ProgramInfo::GetAutoRunJobs()
 *  \brief Returns a bitmap of which jobs are attached to this ProgramInfo.
 *  \sa JobTypes, getProgramFlags()
 */
int ProgramInfo::GetAutoRunJobs(void) const
{
    if (record == NULL) 
    {
        record = new ScheduledRecording();
        record->loadByProgram(this);
    }

    return record->GetAutoRunJobs();
}

/** \fn ProgramInfo::GetChannelRecPriority(const QString&)
 *  \brief Returns Recording Priority of channel.
 *  \param channel %Channel ID of channel whose priority we desire.
 */
int ProgramInfo::GetChannelRecPriority(const QString &channel)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT recpriority FROM channel WHERE chanid = :CHANID ;");
    query.bindValue(":CHANID", channel);
    
    if (query.exec() && query.isActive() && query.size() > 0)
    {
        query.next();
        return query.value(0).toInt();
    }

    return 0;
}

/** \fn ProgramInfo::GetRecordingTypeRecPriority(RecordingType)
 *  \brief Returns recording priority change needed due to RecordingType.
 */
int ProgramInfo::GetRecordingTypeRecPriority(RecordingType type)
{
    switch (type)
    {
        case kSingleRecord:
            return gContext->GetNumSetting("SingleRecordRecPriority", 1);
        case kTimeslotRecord:
            return gContext->GetNumSetting("TimeslotRecordRecPriority", 0);
        case kWeekslotRecord:
            return gContext->GetNumSetting("WeekslotRecordRecPriority", 0);
        case kChannelRecord:
            return gContext->GetNumSetting("ChannelRecordRecPriority", 0);
        case kAllRecord:
            return gContext->GetNumSetting("AllRecordRecPriority", 0);
        case kFindOneRecord:
        case kFindDailyRecord:
        case kFindWeeklyRecord:
            return gContext->GetNumSetting("FindOneRecordRecPriority", -1);
        case kOverrideRecord:
        case kDontRecord:
            return gContext->GetNumSetting("OverrideRecordRecPriority", 0);
        default:
            return 0;
    }
}

/** \fn ProgramInfo::ApplyRecordStateChange(RecordingType)
 *  \brief Sets RecordingType of "record", creating "record" if it
 *         does not exist.
 *  \param newstate State to apply to "record" RecordingType.
 */
// newstate uses same values as return of GetProgramRecordingState
void ProgramInfo::ApplyRecordStateChange(RecordingType newstate)
{
    GetProgramRecordingStatus();
    if (newstate == kOverrideRecord || newstate == kDontRecord)
        record->makeOverride();
    record->setRecordingType(newstate);
    record->save();
}

/** \fn ProgramInfo::ApplyRecordRecPriorityChange(int)
 *  \brief Sets recording priority of "record", creating "record" if it
 *         does not exist.
 *  \param newrecpriority New recording priority.
 */
void ProgramInfo::ApplyRecordRecPriorityChange(int newrecpriority)
{
    GetProgramRecordingStatus();
    record->setRecPriority(newrecpriority);
    record->save();
}

/** \fn ProgramInfo::ApplyRecordRecGroupChange(const QString &newrecgroup)
 *  \brief Sets the recording group, both in this ProgramInfo
 *         and in the database.
 *  \param newrecgroup New recording group.
 */
void ProgramInfo::ApplyRecordRecGroupChange(const QString &newrecgroup)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("UPDATE recorded"
                  " SET recgroup = :RECGROUP"
                  " WHERE chanid = :CHANID"
                  " AND starttime = :START ;");
    query.bindValue(":RECGROUP", newrecgroup.utf8());
    query.bindValue(":START", recstartts);
    query.bindValue(":CHANID", chanid);

    if (!query.exec())
        MythContext::DBError("RecGroup update", query);

    recgroup = newrecgroup;
}

/** \fn ProgramInfo::ApplyRecordRecTitleChange(const QString &newTitle, const QString &newSubtitle)
 *  \brief Sets the recording title and subtitle, both in this ProgramInfo
 *         and in the database.
 *  \param newTitle New recording title.
 *  \param newSubtitle New recording subtitle
 */
void ProgramInfo::ApplyRecordRecTitleChange(const QString &newTitle, const QString &newSubtitle)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("UPDATE recorded"
                  " SET title = :TITLE, subtitle = :SUBTITLE"
                  " WHERE chanid = :CHANID"
                  " AND starttime = :START ;");
    query.bindValue(":TITLE", newTitle.utf8());
    query.bindValue(":SUBTITLE", newSubtitle.utf8());
    query.bindValue(":CHANID", chanid);
    query.bindValue(":START", recstartts.toString("yyyyMMddhhmm00"));

    if (!query.exec())
        MythContext::DBError("RecTitle update", query);

    title = newTitle;
    subtitle = newSubtitle;
}


/** \fn ProgramInfo::ToggleRecord(void)
 *  \brief Cycles through recording types.
 *
 *   If the program recording status is kNotRecording, 
 *   ApplyRecordStateChange(kSingleRecord) is called.
 *   If the program recording status is kSingleRecording, 
 *   ApplyRecordStateChange(kFindOneRecord) is called.
 *   <br>etc...
 *
 *   The states in order are: kNotRecording, kSingleRecord, kFindOneRecord,
 *     kWeekslotRecord, kFindWeeklyRecord, kTimeslotRecord, kFindDailyRecord,
 *     kChannelRecord, kAllRecord.<br>
 *   And: kOverrideRecord, kDontRecord.
 *
 *   That is if you the recording is in any of the first set of states,
 *   we cycle through those, if not we toggle between kOverrideRecord and
 *   kDontRecord.
 */
void ProgramInfo::ToggleRecord(void)
{
    RecordingType curType = GetProgramRecordingStatus();

    switch (curType) 
    {
        case kNotRecording:
            ApplyRecordStateChange(kSingleRecord);
            break;
        case kSingleRecord:
            ApplyRecordStateChange(kFindOneRecord);
            break;
        case kFindOneRecord:
            ApplyRecordStateChange(kWeekslotRecord);
            break;
        case kWeekslotRecord:
            ApplyRecordStateChange(kFindWeeklyRecord);
            break;
        case kFindWeeklyRecord:
            ApplyRecordStateChange(kTimeslotRecord);
            break;
        case kTimeslotRecord:
            ApplyRecordStateChange(kFindDailyRecord);
            break;
        case kFindDailyRecord:
            ApplyRecordStateChange(kChannelRecord);
            break;
        case kChannelRecord:
            ApplyRecordStateChange(kAllRecord);
            break;
        case kAllRecord:
        default:
            ApplyRecordStateChange(kNotRecording);
            break;
        case kOverrideRecord:
            ApplyRecordStateChange(kDontRecord);
            break;
        case kDontRecord:
            ApplyRecordStateChange(kOverrideRecord);
            break;
    }
}

/** \fn ProgramInfo::GetScheduledRecording(void)
 *  \brief Returns the "record" field, creating it if necessary.
 */
ScheduledRecording* ProgramInfo::GetScheduledRecording(void)
{
    GetProgramRecordingStatus();
    return record;
}

/** \fn ProgramInfo::getRecordID(void)
 *  \brief Returns a record id, creating "record" it if necessary.
 */
int ProgramInfo::getRecordID(void)
{
    GetProgramRecordingStatus();
    recordid = record->getRecordID();
    return recordid;
}

/** \fn ProgramInfo::IsSameProgram(const ProgramInfo&) const
 *  \brief Checks for duplicates according to dupmethod.
 *  \param other ProgramInfo to compare this one with.
 */
bool ProgramInfo::IsSameProgram(const ProgramInfo& other) const
{
    if (rectype == kFindOneRecord)
        return recordid == other.recordid;

    if (findid && findid == other.findid &&
        (recordid == other.recordid || recordid == other.parentid))
           return true;

    if (title != other.title)
        return false;

    if (findid && findid == other.findid)
        return true;

    if (dupmethod & kDupCheckNone)
        return false;

    if (catType == "series" && programid.contains(QRegExp("0000$")))
        return false;

    if (programid != "" && other.programid != "")
        return programid == other.programid;

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

/** \fn ProgramInfo::IsSameTimeslot(const ProgramInfo&) const
 *  \brief Checks chanid, start/end times for equality.
 *  \param other ProgramInfo to compare this one with.
 *  \return true if this program shares same time slot as "other" program.
 */
bool ProgramInfo::IsSameTimeslot(const ProgramInfo& other) const
{
    if (title != other.title)
        return false;
    if (startts == other.startts && endts == other.endts &&
        (chanid == other.chanid || 
         (chansign != "" && chansign == other.chansign)))
        return true;

    return false;
}

/** \fn ProgramInfo::IsSameProgramTimeslot(const ProgramInfo&) const
 *  \brief Checks chanid or chansign, start/end times,
 *         cardid, inputid for fully inclusive overlap.
 *  \param other ProgramInfo to compare this one with.
 *  \return true if this program is contained in time slot of "other" program.
 */
bool ProgramInfo::IsSameProgramTimeslot(const ProgramInfo &other) const
{
    if (title != other.title)
        return false;
    if ((chanid == other.chanid ||
         (chansign != "" && chansign == other.chansign)) &&
        startts < other.endts &&
        endts > other.startts)
        return true;

    return false;
}

/** \fn ProgramInfo::CreateRecordBasename(const QString &ext) const
 *  \brief Returns a filename for a recording based on the
 *         recording channel and date.
 */
QString ProgramInfo::CreateRecordBasename(const QString &ext) const
{
    QString starts = recstartts.toString("yyyyMMddhhmmss");

    QString retval = QString("%1_%2.%3").arg(chanid)
                             .arg(starts).arg(ext);
    
    return retval;
}               

/** \fn ProgramInfo::SetRecordBasename(QString basename) const
 *  \brief Sets a recording's basename in the database.
 */
bool ProgramInfo::SetRecordBasename(QString basename)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE recorded "
                  "SET basename = :BASENAME "
                  "WHERE chanid = :CHANID AND "
                  "      starttime = :STARTTIME;");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);
    query.bindValue(":BASENAME", basename);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("SetRecordBasename", query);
        return false;
    }
    
    return true;
}               

/** \fn ProgramInfo::GetRecordBasename(void) const
 *  \brief Returns a filename for a recording based on the
 *         recording channel and date.
 */
QString ProgramInfo::GetRecordBasename(void) const
{
    QString retval = "";

    if (!pathname.isEmpty())
        retval = pathname.section('/', -1);
    else
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT basename FROM recorded "
                      "WHERE chanid = :CHANID AND "
                      "      starttime = :STARTTIME;");
        query.bindValue(":CHANID", chanid);
        query.bindValue(":STARTTIME", recstartts);

        if (!query.exec() || !query.isActive())
            MythContext::DBError("GetRecordBasename", query);
        else if (query.size() < 1)
            VERBOSE(VB_ALL, QString("GetRecordBasename found no entry"));
        else
        {
            query.next();
            retval = query.value(0).toString();
        }
    }

    return retval;
}               

/** \fn ProgramInfo::GetRecordFilename(const QString&) const
 *  \brief Returns prefix+"/"+GetRecordBasename()
 *  \param prefix Prefix to apply to GetRecordBasename().
 */
QString ProgramInfo::GetRecordFilename(const QString &prefix) const
{
    return QString("%1/%2").arg(prefix).arg(GetRecordBasename());
}               

/** \fn ProgramInfo::GetPlaybackURL(QString) const
 *  \brief Returns URL to where %MythTV would stream the
 *         this program from, were it to be streamed.
 */
QString ProgramInfo::GetPlaybackURL(QString playbackHost) const
{
    QString tmpURL;
    QString m_hostname = gContext->GetHostName();

    if (playbackHost == "")
        playbackHost = m_hostname;

    tmpURL = GetRecordFilename(gContext->GetSettingOnHost("RecordFilePrefix",
                                                          hostname));

    if (playbackHost == hostname)
        return tmpURL;

    if (playbackHost == m_hostname)
    {
        QFile checkFile(tmpURL);

        if (checkFile.exists())
            return tmpURL;

        tmpURL = GetRecordFilename(gContext->GetSettingOnHost(
                                               "RecordFilePrefix", m_hostname));
        checkFile.setName(tmpURL);

        if (checkFile.exists())
            return tmpURL;
    }

    tmpURL = QString("myth://") +
             gContext->GetSettingOnHost("BackendServerIP", hostname) + ":" +
             gContext->GetSettingOnHost("BackendServerPort", hostname) + "/" +
             GetRecordBasename();

    return tmpURL;
}

/** \fn ProgramInfo::StartedRecording(QString, QString)
 *  \brief Inserts this ProgramInfo into the database as an existing recording.
 *  
 *  This method, of course, only works if a recording has been scheduled
 *  and started.
 *
 *  \param prefix Directory recording should be placed in
 *  \param ext    File extension for recording
 */
void ProgramInfo::StartedRecording(QString prefix, QString ext)
{
    MSqlQuery query(MSqlQuery::InitCon());

    if (!record)
    {
        record = new ScheduledRecording();
        record->loadByProgram(this);
    }

    hostname = gContext->GetHostName();
    pathname = CreateRecordBasename(ext);
    while (!insert_program(query, this, record))
    {
        recstartts.addSecs(1);
        pathname = CreateRecordBasename(ext);
    }
    pathname = prefix + "/" + pathname;

    query.prepare("DELETE FROM recordedmarkup WHERE chanid = :CHANID"
                  " AND starttime = :START;");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":START", recstartts);

    if (!query.exec() || !query.isActive())
        MythContext::DBError("Clear markup on record", query);

    query.prepare("REPLACE INTO recordedcredits"
                 " SELECT * FROM credits"
                 " WHERE chanid = :CHANID AND starttime = :START;");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":START", startts);
    if (!query.exec() || !query.isActive())
        MythContext::DBError("Copy program credits on record", query);

    query.prepare("REPLACE INTO recordedprogram"
                 " SELECT * from program"
                 " WHERE chanid = :CHANID AND starttime = :START;");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":START", startts);
    if (!query.exec() || !query.isActive())
        MythContext::DBError("Copy program data on record", query);

    query.prepare("REPLACE INTO recordedrating"
                 " SELECT * from programrating"
                 " WHERE chanid = :CHANID AND starttime = :START;");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":START", startts);
    if (!query.exec() || !query.isActive())
        MythContext::DBError("Copy program ratings on record", query);    
}

static bool insert_program(MSqlQuery                &query,
                           const ProgramInfo        *pg,
                           const ScheduledRecording *schd)
{
    query.prepare("LOCK TABLES recorded WRITE");
    if (!query.exec())
    {
        MythContext::DBError("insert_program -- lock", query);
        return false;
    }

    query.prepare(
        "SELECT recordid "
        "    FROM recorded "
        "    WHERE chanid    = :CHANID AND "
        "          starttime = :STARTS");
    query.bindValue(":CHANID", pg->chanid);
    query.bindValue(":STARTS", pg->recstartts);

    if (!query.exec() || query.size())
    {
        if (!query.isActive())
            MythContext::DBError("insert_program -- select", query);
        else
            VERBOSE(VB_IMPORTANT, "recording already exists...");

        query.prepare("UNLOCK TABLES");
        query.exec();
        return false;
    }

    query.prepare(    
        "INSERT INTO recorded "
        "   (chanid,    starttime,   endtime,         title,            "
        "    subtitle,  description, hostname,        category,         "
        "    recgroup,  autoexpire,  recordid,        seriesid,         "
        "    programid, stars,       previouslyshown, originalairdate,  "
        "    findid,    transcoder,  timestretch,     recpriority,      "
        "    basename,  progstart,   progend) "
        "VALUES"
        "  (:CHANID,   :STARTS,     :ENDS,           :TITLE,            "
        "   :SUBTITLE, :DESC,       :HOSTNAME,       :CATEGORY,         "
        "   :RECGROUP, :AUTOEXP,    :RECORDID,       :SERIESID,         "
        "   :PROGRAMID,:STARS,      :REPEAT,         :ORIGAIRDATE,      "
        "   :FINDID,   :TRANSCODER, :TIMESTRETCH,    :RECPRIORITY,      "
        "    :BASENAME, :PROGSTART,  :PROGEND) "
        );

    query.bindValue(":CHANID",      pg->chanid);
    query.bindValue(":STARTS",      pg->recstartts);
    query.bindValue(":ENDS",        pg->recendts);
    query.bindValue(":TITLE",       pg->title.utf8());
    query.bindValue(":SUBTITLE",    pg->subtitle.utf8());
    query.bindValue(":DESC",        pg->description.utf8());
    query.bindValue(":HOSTNAME",    pg->hostname);
    query.bindValue(":CATEGORY",    pg->category.utf8());
    query.bindValue(":RECGROUP",    pg->recgroup.utf8());
    query.bindValue(":AUTOEXP",     schd->GetAutoExpire());
    query.bindValue(":RECORDID",    pg->recordid);
    query.bindValue(":SERIESID",    pg->seriesid.utf8());
    query.bindValue(":PROGRAMID",   pg->programid.utf8());
    query.bindValue(":FINDID",      pg->findid);
    query.bindValue(":STARS",       pg->stars);
    query.bindValue(":REPEAT",      pg->repeat);
    query.bindValue(":ORIGAIRDATE", pg->originalAirDate);
    query.bindValue(":TRANSCODER",  schd->GetTranscoder());
    query.bindValue(":TIMESTRETCH", pg->timestretch);
    query.bindValue(":RECPRIORITY", schd->getRecPriority());
    query.bindValue(":BASENAME",    pg->pathname);
    query.bindValue(":PROGSTART",   pg->startts);
    query.bindValue(":PROGEND",     pg->endts);

    bool ok = query.exec() && (query.numRowsAffected() > 0);
    if (!ok && !query.isActive())
        MythContext::DBError("insert_program -- insert", query);

    query.prepare("UNLOCK TABLES");
    query.exec();

    return ok;
}

/** \fn ProgramInfo::FinishedRecording(bool prematurestop) 
 *  \brief If not a premature stop, adds program to history of recorded 
 *         programs.
 *  \param prematurestop If true, we only fetch the recording status.
 */
void ProgramInfo::FinishedRecording(bool prematurestop)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE recorded SET endtime = :ENDTIME "
                  "WHERE chanid = :CHANID AND "
                  "    starttime = :STARTTIME ");
    query.bindValue(":ENDTIME", recendts);
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);

    query.exec();

    if (!query.isActive())
        MythContext::DBError("FinishedRecording update", query);

    GetProgramRecordingStatus();
    if (!prematurestop)
        record->doneRecording(*this);
}

/** \fn ProgramInfo::UpdateRecordingEnd(void) 
 *  \brief Update information in the recorded table when the end-time
 *  of a recording is changed.
 */
void ProgramInfo::UpdateRecordingEnd(void)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE recorded SET endtime = :ENDTIME, "
                  "    recordid = :RECORDID "
                  "WHERE chanid = :CHANID AND "
                  "    starttime = :STARTTIME ");
    query.bindValue(":ENDTIME", recendts);
    query.bindValue(":RECORDID", recordid);
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);

    query.exec();

    if (!query.isActive())
        MythContext::DBError("FinishedRecording update", query);
}


/** \fn ProgramInfo::SetFilesize(long long)
 *  \brief Sets recording file size in database, and sets "filesize" field.
 */
void ProgramInfo::SetFilesize(long long fsize)
{
    filesize = fsize;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE recorded SET filesize = :FILESIZE"
                  " WHERE chanid = :CHANID"
                  " AND starttime = :STARTTIME ;");
    query.bindValue(":FILESIZE", longLongToString(fsize));
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);
    
    if (!query.exec() || !query.isActive())
        MythContext::DBError("File size update", 
                             query);
}

/** \fn ProgramInfo::GetFilesize(void)
 *  \brief Gets recording file size from database, and sets "filesize" field.
 */
long long ProgramInfo::GetFilesize(void)
{
    MSqlQuery query(MSqlQuery::InitCon());
    
    query.prepare("SELECT filesize FROM recorded"
                  " WHERE chanid = :CHANID"
                  " AND starttime = :STARTTIME ;");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);
    
    if (query.exec() && query.isActive() && query.size() > 0)
    {
        query.next();
        filesize = stringToLongLong(query.value(0).toString());
    }
    else
        filesize = 0;

    return filesize;
}

/** \fn ProgramInfo::SetBookmark(long long pos) const
 *  \brief Sets a bookmark position in database.
 *
 *   If this is a %MythTV recording the bookmark is put in the "recorded"
 *   table, if not it is put in the "videobookmarks" table.
 */
void ProgramInfo::SetBookmark(long long pos) const
{
    MSqlQuery query(MSqlQuery::InitCon());

    // When using mythtv to play files not recorded by myth the title is empty
    // so bookmark is saved to videobookmarks rather than recorded
    if (isVideo) 
    {
        //delete any existing bookmark for this file
        query.prepare("DELETE from videobookmarks"                      
                      " WHERE filename = :FILENAME ;");                   
        query.bindValue(":FILENAME", pathname);        
        if (!query.exec() || !query.isActive())
        MythContext::DBError("Save position update",
                             query);
        //insert new bookmark
        query.prepare("INSERT into videobookmarks (filename, bookmark)"
                      "VALUES (:FILENAME , :BOOKMARK);");
        query.bindValue(":FILENAME", pathname);                                    
    }
    else
    {
        query.prepare("UPDATE recorded"
                    " SET bookmark = :BOOKMARK"
                    " WHERE chanid = :CHANID"
                    " AND starttime = :STARTTIME ;");
        query.bindValue(":CHANID", chanid);
        query.bindValue(":STARTTIME", recstartts);
    }
        
    if (pos > 0)
    {
        char posstr[128];
        sprintf(posstr, "%lld", pos);
        query.bindValue(":BOOKMARK", posstr);
    }
    else
        query.bindValue(":BOOKMARK", QString::null);
    
    if (!query.exec() || !query.isActive())
        MythContext::DBError("Save position update",
                             query);
}

/** \fn ProgramInfo::GetBookmark(void)
 *  \brief Gets any bookmark position in database,
 *         unless "ignoreBookmark" is set.
 *
 *   If this is a %MythTV recording the bookmark is queried from the "recorded"
 *   table, if not it is queried from the "videobookmarks" table.
 *  \return Bookmark position in bytes if the query is executed and succeeds,
 *          zero otherwise.
 */
long long ProgramInfo::GetBookmark(void) const
{
    MSqlQuery query(MSqlQuery::InitCon());
    long long pos = 0;

    if (ignoreBookmark)
        return pos;
    
    // When using mythtv to play files not recorded by myth the title is empty
    // so bookmark comes from videobookmarks rather than recorded
    if (isVideo) 
    {
        query.prepare("SELECT bookmark FROM videobookmarks"
                      " WHERE filename = :FILENAME;");                  
        query.bindValue(":FILENAME", pathname);        
    } 
    else 
    {
        query.prepare("SELECT bookmark FROM recorded"
                    " WHERE chanid = :CHANID"
                    " AND starttime = :STARTTIME ;");
        query.bindValue(":CHANID", chanid);
        query.bindValue(":STARTTIME", recstartts);
    }
    if (query.exec() && query.isActive() && query.size() > 0)
    {
        query.next();
        QString result = query.value(0).toString();
        if (!result.isEmpty())
            sscanf(result.ascii(), "%lld", &pos);
    }
    
    return pos;
}

/** \fn ProgramInfo::IsEditing(void) const
 *  \brief Queries "recorded" table for its "editing" field
 *         and returns true if it is set to true.
 *  \return true iff we have started, but not finished, editing.
 */
bool ProgramInfo::IsEditing(void) const
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT editing FROM recorded"
                  " WHERE chanid = :CHANID"
                  " AND starttime = :STARTTIME ;");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        query.next();
        return query.value(0).toBool();
    }

    return false;
}

/** \fn ProgramInfo::SetEditing(bool) const
 *  \brief Sets "editing" field in "recorded" table to "edit"
 *  \param edit Editing state to set.
 */
void ProgramInfo::SetEditing(bool edit) const
{
    MSqlQuery query(MSqlQuery::InitCon());
    
    query.prepare("UPDATE recorded"
                  " SET editing = :EDIT"
                  " WHERE chanid = :CHANID"
                  " AND starttime = :STARTTIME ;");
    query.bindValue(":EDIT", edit);
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);
   
    if (!query.exec() || !query.isActive())
        MythContext::DBError("Edit status update", 
                             query);
}

/** \fn ProgramInfo::SetDeleteFlag(bool) const
 *  \brief Set "deletepending" field in "recorded" table to "deleteFlag".
 *  \param deleteFlag value to set delete pending field to.
 */
void ProgramInfo::SetDeleteFlag(bool deleteFlag) const
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("UPDATE recorded"
                  " SET deletepending = :DELETEFLAG"
                  " WHERE chanid = :CHANID"
                  " AND starttime = :STARTTIME ;");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);

    if (deleteFlag)
        query.bindValue(":DELETEFLAG", 1);
    else
        query.bindValue(":DELETEFLAG", 0);
    
    if (!query.exec() || !query.isActive())
        MythContext::DBError("Set delete flag", query);
}

/** \fn ProgramInfo::IsCommFlagged(void) const
 *  \brief Returns true if commercial flagging has been started
 *         according to "commflagged" field in "recorded" table.
 */
bool ProgramInfo::IsCommFlagged(void) const
{
    MSqlQuery query(MSqlQuery::InitCon());
    
    query.prepare("SELECT commflagged FROM recorded"
                  " WHERE chanid = :CHANID"
                  " AND starttime = :STARTTIME ;");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        query.next();
        return query.value(0).toBool();
    }

    return false;
}

/** \fn ProgramInfo::IsInUse(QString &byWho) const
 *  \brief Returns true if Program is in use.  This is determined by
 *         the inuseprograms table which is updated automatically by
 *         NuppelVideoPlayer.
 */
bool ProgramInfo::IsInUse(QString &byWho) const
{
    QDateTime oneHourAgo = QDateTime::currentDateTime().addSecs(-61 * 60);
    MSqlQuery query(MSqlQuery::InitCon());
    
    query.prepare("SELECT hostname, recusage FROM inuseprograms "
                  " WHERE chanid = :CHANID"
                  " AND starttime = :STARTTIME "
                  " AND lastupdatetime > :ONEHOURAGO ;");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);
    query.bindValue(":ONEHOURAGO", oneHourAgo);

    byWho = "";
    if (query.exec() && query.isActive() && query.size() > 0)
    {
        QString usageStr, recusage;
        while(query.next())
        {
            usageStr = QObject::tr("Unknown");
            recusage = query.value(1).toString();

            if (recusage == "player")
                usageStr = QObject::tr("Playing");
            else if (recusage == "recorder")
                usageStr = QObject::tr("Recording");
            else if (recusage == "flagger")
                usageStr = QObject::tr("Commercial Flagging");
            else if (recusage == "transcoder")
                usageStr = QObject::tr("Transcoding");
            else if (recusage == "PIP player")
                usageStr = QObject::tr("PIP");

            byWho += query.value(0).toString() + " (" + usageStr + ")\n";
        }

        return true;
    }

    return false;
}

/** \fn ProgramInfo::SetCommFlagged(int) const
 *  \brief Set "commflagged" field in "recorded" table to "flag".
 *  \param flag value to set commercial flagging field to.
 */
void ProgramInfo::SetCommFlagged(int flag) const
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("UPDATE recorded"
                  " SET commflagged = :FLAG"
                  " WHERE chanid = :CHANID"
                  " AND starttime = :STARTTIME ;");
    query.bindValue(":FLAG", flag);
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);
    
    if (!query.exec() || !query.isActive())
        MythContext::DBError("Commercial Flagged status update",
                             query);
}

/** \fn ProgramInfo::SetPreserveEpisode(bool) const
 *  \brief Set "preserve" field in "recorded" table to "preserveEpisode".
 *  \param preserveEpisode value to set preserve field to.
 */
void ProgramInfo::SetPreserveEpisode(bool preserveEpisode) const
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("UPDATE recorded"
                  " SET preserve = :PRESERVE"
                  " WHERE chanid = :CHANID"
                  " AND starttime = :STARTTIME ;");
    query.bindValue(":PRESERVE", preserveEpisode);
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);

    if (!query.exec() || !query.isActive())
        MythContext::DBError("PreserveEpisode update", query);
}

/** \fn ProgramInfo::SetAutoExpire(int) const
 *  \brief Set "autoexpire" field in "recorded" table to "autoExpire".
 *  \param autoExpire value to set auto expire field to.
 */
void ProgramInfo::SetAutoExpire(int autoExpire) const
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("UPDATE recorded"
                  " SET autoexpire = :AUTOEXPIRE"
                  " WHERE chanid = :CHANID"
                  " AND starttime = :STARTTIME ;");
    query.bindValue(":AUTOEXPIRE", autoExpire);
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);

    if (!query.exec() || !query.isActive())
        MythContext::DBError("AutoExpire update",
                             query);
}

/** \fn ProgramInfo::GetAutoExpireFromRecorded(void) const
 *  \brief Returns "autoexpire" field from "recorded" table.
 */
int ProgramInfo::GetAutoExpireFromRecorded(void) const
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT autoexpire FROM recorded"
                  " WHERE chanid = :CHANID"
                  " AND starttime = :STARTTIME ;");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        query.next();
        return query.value(0).toInt();
    }

    return false;
}

/** \fn ProgramInfo::GetPreserveEpisodeFromRecorded(void) const
 *  \brief Returns "preserve" field from "recorded" table.
 */
bool ProgramInfo::GetPreserveEpisodeFromRecorded(void) const
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT preserve FROM recorded"
                  " WHERE chanid = :CHANID"
                  " AND starttime = :STARTTIME ;");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        query.next();
        return query.value(0).toBool();
    }

    return false;
}

/** \fn ProgramInfo::UsesMaxEpisodes(void) const
 *  \brief Returns "maxepisodes" field from "record" table.
 */
bool ProgramInfo::UsesMaxEpisodes(void) const
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT maxepisodes FROM record WHERE "
                  "recordid = :RECID ;");
    query.bindValue(":RECID", recordid);

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        query.next();
        return query.value(0).toInt();
    }

    return false;
}

void ProgramInfo::GetCutList(QMap<long long, int> &delMap) const
{
//    GetMarkupMap(delMap, db, MARK_CUT_START);
//    GetMarkupMap(delMap, db, MARK_CUT_END, true);

    delMap.clear();
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT cutlist FROM recorded"
                  " WHERE chanid = :CHANID"
                  " AND starttime = :STARTTIME ;");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);

    if (query.exec() && query.isActive() && query.size() > 0)
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
                    VERBOSE(VB_IMPORTANT,
                            QString("Malformed cutlist list: %1").arg(*i));
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

void ProgramInfo::SetCutList(QMap<long long, int> &delMap) const
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

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("UPDATE recorded"
                  " SET cutlist = :CUTLIST"
                  " WHERE chanid = :CHANID"
                  " AND starttime = :STARTTIME ;");
    query.bindValue(":CUTLIST", cutdata);
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);

    if (!query.exec() || !query.isActive())
        MythContext::DBError("cutlist update", 
                             query);
}

void ProgramInfo::SetCommBreakList(QMap<long long, int> &frames) const
{
    ClearMarkupMap(MARK_COMM_START);
    ClearMarkupMap(MARK_COMM_END);
    SetMarkupMap(frames);
}

void ProgramInfo::GetCommBreakList(QMap<long long, int> &frames) const
{
    GetMarkupMap(frames, MARK_COMM_START);
    GetMarkupMap(frames, MARK_COMM_END, true);
}

void ProgramInfo::ClearMarkupMap(int type, long long min_frame, 
                                           long long max_frame) const
{
    MSqlQuery query(MSqlQuery::InitCon());
    QString comp = "";

    if (min_frame >= 0)
    {
        char tempc[128];
        sprintf(tempc, " AND mark >= %lld ", min_frame);
        comp += tempc;
    }

    if (max_frame >= 0)
    {
        char tempc[128];
        sprintf(tempc, " AND mark <= %lld ", max_frame);
        comp += tempc;
    }

    if (type != -100)
        comp += QString(" AND type = :TYPE ");
    
    if (isVideo)
    {
        query.prepare("DELETE FROM filemarkup"
                      " WHERE filename = :PATH "
                      + comp + ";");
        query.bindValue(":PATH", pathname);
    }
    else
    {
        query.prepare("DELETE FROM recordedmarkup"
                      " WHERE chanid = :CHANID"
                      " AND STARTTIME = :STARTTIME"
                      + comp + ";");
        query.bindValue(":CHANID", chanid);
        query.bindValue(":STARTTIME", recstartts);
    }
    query.bindValue(":TYPE", type);
    
    if (!query.exec() || !query.isActive())
        MythContext::DBError("ClearMarkupMap deleting", 
                             query);
}

void ProgramInfo::SetMarkupMap(QMap<long long, int> &marks,
                               int type, long long min_frame, 
                               long long max_frame) const
{
    QMap<long long, int>::Iterator i;
    MSqlQuery query(MSqlQuery::InitCon());
    
    if (!isVideo)
    {
        // check to make sure the show still exists before saving markups
        query.prepare("SELECT starttime FROM recorded"
                      " WHERE chanid = :CHANID"
                      " AND starttime = :STARTTIME ;");
        query.bindValue(":CHANID", chanid);
        query.bindValue(":STARTTIME", recstartts);

        if (!query.exec() || !query.isActive())
            MythContext::DBError("SetMarkupMap checking record table",
                                 query);

        if (query.size() < 1 || !query.next())
            return;
    }
 
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

        if (isVideo)
        {
            query.prepare("INSERT INTO filemarkup (filename, mark, type)"
                          " VALUES ( :PATH , :MARK , :TYPE );");
            query.bindValue(":PATH", pathname);
        }
        else
        {
            query.prepare("INSERT INTO recordedmarkup"
                          " (chanid, starttime, mark, type)"
                          " VALUES ( :CHANID , :STARTTIME , :MARK , :TYPE );");
            query.bindValue(":CHANID", chanid);
            query.bindValue(":STARTTIME", recstartts);
        }
        query.bindValue(":MARK", frame_str);
        query.bindValue(":TYPE", mark_type);
       
        if (!query.exec() || !query.isActive())
            MythContext::DBError("SetMarkupMap inserting", 
                                 query);
    }
}

void ProgramInfo::GetMarkupMap(QMap<long long, int> &marks,
                               int type, bool mergeIntoMap) const
{
    if (!mergeIntoMap)
        marks.clear();

    MSqlQuery query(MSqlQuery::InitCon());
    
    if (isVideo)
    {
        query.prepare("SELECT mark, type FROM filemarkup"
                      " WHERE filename = :PATH"
                      " AND type = :TYPE"
                      " ORDER BY mark;");
        query.bindValue(":PATH", pathname);
    }
    else
    {
        query.prepare("SELECT mark, type FROM recordedmarkup"
                      " WHERE chanid = :CHANID"
                      " AND starttime = :STARTTIME"
                      " AND type = :TYPE"
                      " ORDER BY mark;");
        query.bindValue(":CHANID", chanid);
        query.bindValue(":STARTTIME", recstartts);
    }
    query.bindValue(":TYPE", type);

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        while(query.next())
            marks[stringToLongLong(query.value(0).toString())] =
                  query.value(1).toInt();
    }
}

bool ProgramInfo::CheckMarkupFlag(int type) const
{
    QMap<long long, int> flagMap;

    GetMarkupMap(flagMap, type);

    return(flagMap.contains(0));
}

void ProgramInfo::SetMarkupFlag(int type, bool flag) const
{
    ClearMarkupMap(type);

    if (flag)
    {
        QMap<long long, int> flagMap;

        flagMap[0] = type;
        SetMarkupMap(flagMap, type);
    }
}

void ProgramInfo::GetPositionMap(QMap<long long, long long> &posMap,
                                 int type) const
{
    posMap.clear();
    MSqlQuery query(MSqlQuery::InitCon());

    if (isVideo)
    {
        query.prepare("SELECT mark, offset FROM filemarkup"
                      " WHERE filename = :PATH"
                      " AND type = :TYPE ;");
        query.bindValue(":PATH", pathname);
    }
    else
    {
        query.prepare("SELECT mark, offset FROM recordedmarkup"
                      " WHERE chanid = :CHANID"
                      " AND starttime = :STARTTIME"
                      " AND type = :TYPE ;");
        query.bindValue(":CHANID", chanid);
        query.bindValue(":STARTTIME", recstartts);
    }
    query.bindValue(":TYPE", type);

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        while (query.next())
            posMap[stringToLongLong(query.value(0).toString())] =
                   stringToLongLong(query.value(1).toString());
    }
}

void ProgramInfo::ClearPositionMap(int type) const
{
    MSqlQuery query(MSqlQuery::InitCon());
  
    if (isVideo)
    {
        query.prepare("DELETE FROM filemarkup"
                      " WHERE filename = :PATH"
                      " AND type = :TYPE ;");
        query.bindValue(":PATH", pathname);
    }
    else
    {
        query.prepare("DELETE FROM recordedmarkup"
                      " WHERE chanid = :CHANID"
                      " AND starttime = :STARTTIME"
                      " AND type = :TYPE ;");
        query.bindValue(":CHANID", chanid);
        query.bindValue(":STARTTIME", recstartts);
    }
    query.bindValue(":TYPE", type);
                               
    if (!query.exec() || !query.isActive())
        MythContext::DBError("clear position map", 
                             query);
}

void ProgramInfo::SetPositionMap(QMap<long long, long long> &posMap, int type,
                                 long long min_frame, long long max_frame) const
{
    QMap<long long, long long>::Iterator i;
    MSqlQuery query(MSqlQuery::InitCon());
    QString comp = "";

    if (min_frame >= 0)
    {
        char tempc[128];
        sprintf(tempc, " AND mark >= %lld ", min_frame);
        comp += tempc;
    }

    if (max_frame >= 0)
    {
        char tempc[128];
        sprintf(tempc, " AND mark <= %lld ", max_frame);
        comp += tempc;
    }

    if (isVideo)
    {
        query.prepare("DELETE FROM filemarkup"
                      " WHERE filename = :PATH"
                      " AND type = :TYPE"
                      + comp + ";");
        query.bindValue(":PATH", pathname);
    }
    else
    {
        query.prepare("DELETE FROM recordedmarkup"
                      " WHERE chanid = :CHANID"
                      " AND starttime = :STARTTIME"
                      " AND type = :TYPE"
                      + comp + ";");
        query.bindValue(":CHANID", chanid);
        query.bindValue(":STARTTIME", recstartts);
    }
    query.bindValue(":TYPE", type);
    
    if (!query.exec() || !query.isActive())
        MythContext::DBError("position map clear", 
                             query);

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

        if (isVideo)
        {
            query.prepare("INSERT INTO filemarkup"
                          " (filename, mark, type, offset)"
                          " VALUES"
                          " ( :PATH , :MARK , :TYPE , :OFFSET );");
            query.bindValue(":PATH", pathname);
        }
        else
        {        
            query.prepare("INSERT INTO recordedmarkup"
                          " (chanid, starttime, mark, type, offset)"
                          " VALUES"
                          " ( :CHANID , :STARTTIME , :MARK , :TYPE , :OFFSET );");
            query.bindValue(":CHANID", chanid);
            query.bindValue(":STARTTIME", recstartts);
        }
        query.bindValue(":MARK", frame_str);
        query.bindValue(":TYPE", type);
        query.bindValue(":OFFSET", offset_str);
        
        if (!query.exec() || !query.isActive())
            MythContext::DBError("position map insert", 
                                 query);
    }
}

void ProgramInfo::SetPositionMapDelta(QMap<long long, long long> &posMap,
                                      int type) const
{
    QMap<long long, long long>::Iterator i;
    MSqlQuery query(MSqlQuery::InitCon());

    for (i = posMap.begin(); i != posMap.end(); ++i)
    {
        long long frame = i.key();
        char tempc[128];
        sprintf(tempc, "%lld", frame);

        QString frame_str = tempc;

        long long offset = i.data();
        sprintf(tempc, "%lld", offset);
       
        QString offset_str = tempc;

        if (isVideo)
        {
            query.prepare("INSERT INTO filemarkup"
                          " (filename, mark, type, offset)"
                          " VALUES"
                          " ( :PATH , :MARK , :TYPE , :OFFSET );");
            query.bindValue(":PATH", pathname);
        }
        else
        {
            query.prepare("INSERT INTO recordedmarkup"
                          " (chanid, starttime, mark, type, offset)"
                          " VALUES"
                          " ( :CHANID , :STARTTIME , :MARK , :TYPE , :OFFSET );");
            query.bindValue(":CHANID", chanid);
            query.bindValue(":STARTTIME", recstartts);
        }
        query.bindValue(":MARK", frame_str);
        query.bindValue(":TYPE", type);
        query.bindValue(":OFFSET", offset_str);
        
        if (!query.exec() || !query.isActive())
            MythContext::DBError("delta position map insert", 
                                 query);
    }
}

/** \fn ProgramInfo::ReactivateRecording(void)
 *  \brief Asks the scheduler to restart this recording if possible.
 */
void ProgramInfo::ReactivateRecording(void)
{
    MSqlQuery result(MSqlQuery::InitCon());

    result.prepare("UPDATE oldrecorded SET reactivate = 1 "
                   "WHERE station = :STATION AND "
                   "  starttime = :STARTTIME AND "
                   "  title = :TITLE;");
    result.bindValue(":STARTTIME", startts);
    result.bindValue(":TITLE", title.utf8());
    result.bindValue(":STATION", chansign);

    result.exec();
    if (!result.isActive())
        MythContext::DBError("ReactivateRecording", result);

    ScheduledRecording::signalChange(0);
}

/** \fn ProgramInfo::AddHistory(bool resched)
 *  \brief Adds recording history, creating "record" it if necessary.
 */
void ProgramInfo::AddHistory(bool resched)
{
    bool dup = (recstatus == rsRecorded ||
                recstatus == rsNeverRecord);
    RecStatusType rs = (recstatus == rsCurrentRecording) ?
        rsPreviousRecording : recstatus;
    oldrecstatus = recstatus;
    if (dup)
        reactivate = false;

    MSqlQuery result(MSqlQuery::InitCon());

    result.prepare("REPLACE INTO oldrecorded (chanid,starttime,"
                   "endtime,title,subtitle,description,category,"
                   "seriesid,programid,findid,recordid,station,"
                   "rectype,recstatus,duplicate,reactivate) "
                   "VALUES(:CHANID,:START,:END,:TITLE,:SUBTITLE,:DESC,"
                   ":CATEGORY,:SERIESID,:PROGRAMID,:FINDID,:RECORDID,"
                   ":STATION,:RECTYPE,:RECSTATUS,:DUPLICATE,:REACTIVATE);");
    result.bindValue(":CHANID", chanid);
    result.bindValue(":START", startts.toString(Qt::ISODate));
    result.bindValue(":END", endts.toString(Qt::ISODate));
    result.bindValue(":TITLE", title.utf8());
    result.bindValue(":SUBTITLE", subtitle.utf8());
    result.bindValue(":DESC", description.utf8());
    result.bindValue(":CATEGORY", category.utf8());
    result.bindValue(":SERIESID", seriesid.utf8());
    result.bindValue(":PROGRAMID", programid.utf8());
    result.bindValue(":FINDID", findid);
    result.bindValue(":RECORDID", recordid);
    result.bindValue(":STATION", chansign);
    result.bindValue(":RECTYPE", rectype);
    result.bindValue(":RECSTATUS", rs);
    result.bindValue(":DUPLICATE", dup);
    result.bindValue(":REACTIVATE", reactivate);

    result.exec();
    if (!result.isActive())
        MythContext::DBError("addHistory", result);

    if (dup && findid)
    {
        result.prepare("REPLACE INTO oldfind (recordid, findid) "
                       "VALUES(:RECORDID,:FINDID);");
        result.bindValue(":RECORDID", recordid);
        result.bindValue(":FINDID", findid);
    
        result.exec();
        if (!result.isActive())
            MythContext::DBError("addFindHistory", result);
    }

    // The adding of an entry to oldrecorded may affect near-future
    // scheduling decisions, so recalculate if told
    if (resched)
        ScheduledRecording::signalChange(0);
}

/** \fn ProgramInfo::DeleteHistory(void)
 *  \brief Deletes recording history, creating "record" it if necessary.
 */
void ProgramInfo::DeleteHistory(void)
{
    MSqlQuery result(MSqlQuery::InitCon());

    result.prepare("DELETE FROM oldrecorded WHERE title = :TITLE AND "
                   "starttime = :START AND station = :STATION");
    result.bindValue(":TITLE", title.utf8());
    result.bindValue(":START", recstartts);
    result.bindValue(":STATION", chansign);
    
    result.exec();
    if (!result.isActive())
        MythContext::DBError("deleteHistory", result);

    if (/*duplicate &&*/ findid)
    {
        result.prepare("DELETE FROM oldfind WHERE "
                       "recordid = :RECORDID AND findid = :FINDID");
        result.bindValue(":RECORDID", recordid);
        result.bindValue(":FINDID", findid);
    
        result.exec();
        if (!result.isActive())
            MythContext::DBError("deleteFindHistory", result);
    }

    // The removal of an entry from oldrecorded may affect near-future
    // scheduling decisions, so recalculate
    ScheduledRecording::signalChange(0);
}

/** \fn ProgramInfo::ForgetHistory(void)
 *  \brief Forget the recording of a program so it will be recorded again.
 */
void ProgramInfo::ForgetHistory(void)
{
    MSqlQuery result(MSqlQuery::InitCon());

    result.prepare("UPDATE oldrecorded SET duplicate = 0 "
                   "WHERE duplicate = 1 "
                   "AND title = :TITLE AND "
                   "((subtitle = :SUBTITLE AND description = :DESC) OR "
                   "(programid <> '' AND programid = :PROGRAMID) OR "
                   "(findid <> 0 AND findid = :FINDID))");
    result.bindValue(":TITLE", title.utf8());
    result.bindValue(":SUBTITLE", subtitle.utf8());
    result.bindValue(":DESC", description.utf8());
    result.bindValue(":PROGRAMID", programid);
    result.bindValue(":FINDID", findid);
    
    result.exec();
    if (!result.isActive())
        MythContext::DBError("forgetHistory", result);

    result.prepare("DELETE FROM oldrecorded "
                   "WHERE recstatus = :NEVER AND duplicate = 0");
    result.bindValue(":NEVER", rsNeverRecord);
    
    result.exec();
    if (!result.isActive())
        MythContext::DBError("forgetNeverHisttory", result);

    if (findid)
    {
        result.prepare("DELETE FROM oldfind WHERE "
                       "recordid = :RECORDID AND findid = :FINDID");
        result.bindValue(":RECORDID", recordid);
        result.bindValue(":FINDID", findid);
    
        result.exec();
        if (!result.isActive())
            MythContext::DBError("forgetFindHistory", result);
    }

    // The removal of an entry from oldrecorded may affect near-future
    // scheduling decisions, so recalculate
    ScheduledRecording::signalChange(0);
}

/** \fn ProgramInfo::RecTypeChar(void) const
 *  \brief Converts "rectype" into a human readable character.
 */
QString ProgramInfo::RecTypeChar(void) const
{
    switch (rectype)
    {
    case kSingleRecord:
        return QObject::tr("S", "RecTypeChar kSingleRecord");
    case kTimeslotRecord:
        return QObject::tr("T", "RecTypeChar kTimeslotRecord");
    case kWeekslotRecord:
        return QObject::tr("W", "RecTypeChar kWeekslotRecord");
    case kChannelRecord:
        return QObject::tr("C", "RecTypeChar kChannelRecord");
    case kAllRecord:
        return QObject::tr("A", "RecTypeChar kAllRecord");
    case kFindOneRecord:
        return QObject::tr("F", "RecTypeChar kFindOneRecord");
    case kFindDailyRecord:
        return QObject::tr("d", "RecTypeChar kFindDailyRecord");
    case kFindWeeklyRecord:
        return QObject::tr("w", "RecTypeChar kFindWeeklyRecord");
    case kOverrideRecord:
    case kDontRecord:
        return QObject::tr("O", "RecTypeChar kOverrideRecord/kDontRecord");
    case kNotRecording:
    default:
        return " ";
    }
}

/** \fn ProgramInfo::RecTypeText(void) const
 *  \brief Converts "rectype" into a human readable description.
 */
QString ProgramInfo::RecTypeText(void) const
{
    switch (rectype)
    {
    case kSingleRecord:
        return QObject::tr("Single Record");
    case kTimeslotRecord:
        return QObject::tr("Record Daily");
    case kWeekslotRecord:
        return QObject::tr("Record Weekly");
    case kChannelRecord:
        return QObject::tr("Channel Record");
    case kAllRecord:
        return QObject::tr("Record All");
    case kFindOneRecord:
        return QObject::tr("Find One");
    case kFindDailyRecord:
        return QObject::tr("Find Daily");
    case kFindWeeklyRecord:
        return QObject::tr("Find Weekly");
    case kOverrideRecord:
    case kDontRecord:
        return QObject::tr("Override Recording");
    default:
        return QObject::tr("Not Recording");
    }
}

/** \fn ProgramInfo::RecStatusChar(void) const
 *  \brief Converts "recstatus" into a human readable character.
 */
QString ProgramInfo::RecStatusChar(void) const
{
    switch (recstatus)
    {
    case rsAborted:
        return QObject::tr("A", "RecStatusChar rsAborted");
    case rsRecorded:
        return QObject::tr("R", "RecStatusChar rsRecorded");
    case rsRecording:
        return QString::number(cardid);
    case rsWillRecord:
        return QString::number(cardid);
    case rsDontRecord:
        return QObject::tr("X", "RecStatusChar rsDontRecord");
    case rsPreviousRecording:
        return QObject::tr("P", "RecStatusChar rsPreviousRecording");
    case rsCurrentRecording:
        return QObject::tr("R", "RecStatusChar rsCurrentRecording");
    case rsEarlierShowing:
        return QObject::tr("E", "RecStatusChar rsEarlierShowing");
    case rsTooManyRecordings:
        return QObject::tr("T", "RecStatusChar rsTooManyRecordings");
    case rsCancelled:
        return QObject::tr("c", "RecStatusChar rsCancelled");
    case rsMissed:
        return QObject::tr("M", "RecStatusChar rsMissed");
    case rsConflict:
        return QObject::tr("C", "RecStatusChar rsConflict");
    case rsLaterShowing:
        return QObject::tr("L", "RecStatusChar rsLaterShowing");
    case rsRepeat:
        return QObject::tr("r", "RecStatusChar rsRepeat");    
    case rsInactive:
        return QObject::tr("x", "RecStatusChar rsInactive");
    case rsLowDiskSpace:
        return QObject::tr("K", "RecStatusChar rsLowDiskSpace");
    case rsTunerBusy:
        return QObject::tr("B", "RecStatusChar rsTunerBusy");
    case rsNotListed:
        return QObject::tr("N", "RecStatusChar rsNotListed");
    case rsNeverRecord:
        return QObject::tr("V", "RecStatusChar rsNeverRecord");
    default:
        return "-";
    }
}

/** \fn ProgramInfo::RecStatusText(void) const
 *  \brief Converts "recstatus" into a short human readable description.
 */
QString ProgramInfo::RecStatusText(void) const
{
    if (rectype == kNotRecording)
        return QObject::tr("Not Recording");
    else
    {
        switch (recstatus)
        {
        case rsAborted:
            return QObject::tr("Aborted");
        case rsRecorded:
            return QObject::tr("Recorded");
        case rsRecording:
            return QObject::tr("Recording");
        case rsWillRecord:
            return QObject::tr("Will Record");
        case rsDontRecord:
            return QObject::tr("Don't Record");
        case rsPreviousRecording:
            return QObject::tr("Previously Recorded");
        case rsCurrentRecording:
            return QObject::tr("Currently Recorded");
        case rsEarlierShowing:
            return QObject::tr("Earlier Showing");
        case rsTooManyRecordings:
            return QObject::tr("Max Recordings");
        case rsCancelled:
            return QObject::tr("Manual Cancel");
        case rsMissed:
            return QObject::tr("Missed");
        case rsConflict:
            return QObject::tr("Conflicting");
        case rsLaterShowing:
            return QObject::tr("Later Showing");
        case rsRepeat:
            return QObject::tr("Repeat");            
        case rsInactive:
            return QObject::tr("Inactive");            
        case rsLowDiskSpace:
            return QObject::tr("Low Disk Space");
        case rsTunerBusy:
            return QObject::tr("Tuner Busy");
        case rsNotListed:
            return QObject::tr("Not Listed");
        case rsNeverRecord:
            return QObject::tr("Never Record");
        default:
            return QObject::tr("Unknown");
        }
    }

    return QObject::tr("Unknown");
}

/** \fn ProgramInfo::RecStatusDesc(void) const
 *  \brief Converts "recstatus" into a long human readable description.
 */
QString ProgramInfo::RecStatusDesc(void) const
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
        case rsAborted:
            message = QObject::tr("This showing was recorded but was aborted "
                                   "before recording was completed.");
            break;
        case rsMissed:
            message += QObject::tr("This showing was not recorded because it "
                                   "was scheduled after it would have ended.");
            break;
        case rsCancelled:
            message += QObject::tr("This showing was not recorded because it "
                                   "was manually cancelled.");
            break;
        case rsLowDiskSpace:
            message += QObject::tr("there wasn't enough disk space available.");
            break;
        case rsTunerBusy:
            message += QObject::tr("the tuner card was already being used.");
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
        case rsTooManyRecordings:
            message += QObject::tr("too many recordings of this program have "
                                   "already been recorded.");
            break;
        case rsConflict:
            message += QObject::tr("another program with a higher priority "
                                   "will be recorded.");
            break;
        case rsLaterShowing:
            message += QObject::tr("this episode will be recorded at a "
                                   "later time.");
            break;
        case rsRepeat:
            message += QObject::tr("this episode is a repeat.");
            break;            
        case rsInactive:
            message += QObject::tr("this recording schedule is inactive.");
            break;
        case rsNotListed:
            message += QObject::tr("this show does not match the current "
                                   "program listings.");
            break;            
        case rsNeverRecord:
            message += QObject::tr("it was marked to never be recorded.");
            break;            
        default:
            message += QObject::tr("you should never see this.");
            break;
        }
    }

    return message;
}

/** \fn ProgramInfo::ChannelText(const QString&) const
 *  \brief Returns channel info using "format".
 *
 *   There are three tags in "format" that will be replaced
 *   with the approriate info. These tags are "<num>", "<sign>",
 *   and "<name>", they replaced with the channel number,
 *   channel call sign, and channel name, respectively.
 *  \param format formatting string.
 *  \return formatted string.
 */
QString ProgramInfo::ChannelText(const QString &format) const
{
    QString chan(format);
    chan.replace("<num>", chanstr)
        .replace("<sign>", chansign)
        .replace("<name>", channame);
    return chan;
}

/** \fn ProgramInfo::FillInRecordInfo(const vector<ProgramInfo *>&)
 *  \brief If a ProgramInfo in "reclist" matching this program exists,
 *         it is used to set recording info in this ProgramInfo.
 *  \return true if match is found, false otherwise.
 */
bool ProgramInfo::FillInRecordInfo(const vector<ProgramInfo *> &reclist)
{
    vector<ProgramInfo *>::const_iterator i;
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
    return found;
}

/** \fn ProgramInfo::Save(void) const
 *  \brief Saves this ProgramInfo to the database, replacing any existing
 *         program in the same timeslot on the same channel.
 */
void ProgramInfo::Save(void) const
{
    MSqlQuery query(MSqlQuery::InitCon());

    // This used to be REPLACE INTO...
    // primary key of table program is chanid,starttime
    query.prepare("DELETE FROM program"
                  " WHERE chanid = :CHANID"
                  " AND starttime = :STARTTIME ;");
    query.bindValue(":CHANID", chanid.toInt());
    query.bindValue(":STARTTIME", startts);
    if (!query.exec())
        MythContext::DBError("Saving program", 
                             query);

    query.prepare("INSERT INTO program (chanid,starttime,endtime,"
                  " title,subtitle,description,category,airdate,"
                  " stars) VALUES (:CHANID,:STARTTIME,:ENDTIME,:TITLE,"
                  " :SUBTITLE,:DESCRIPTION,:CATEGORY,:AIRDATE,:STARS);");
    query.bindValue(":CHANID", chanid.toInt());
    query.bindValue(":STARTTIME", startts);
    query.bindValue(":ENDTIME", endts);
    query.bindValue(":TITLE", title.utf8());
    query.bindValue(":SUBTITLE", subtitle.utf8());
    query.bindValue(":DESCRIPTION", description.utf8());
    query.bindValue(":CATEGORY", category.utf8());
    query.bindValue(":AIRDATE", "0");
    query.bindValue(":STARS", "0");

    if (!query.exec())
        MythContext::DBError("Saving program", 
                             query);
}


/** \fn ProgramInfo::EditRecording(void)
 *  \brief Creates a dialog for editing the recording status,
 *         blocking until user leaves dialog.
 */
void ProgramInfo::EditRecording(void)
{
    if (recordid == 0)
        EditScheduled();
    else if (recstatus <= rsWillRecord)
        ShowRecordingDialog();
    else
        ShowNotRecordingDialog();
}

/** \fn ProgramInfo::EditScheduled(void)
 *  \brief Creates a dialog for editing the recording status,
 *         blocking until user leaves dialog.
 */
void ProgramInfo::EditScheduled(void)
{
    GetProgramRecordingStatus();
    record->exec();
}

/** \fn ProgramInfo::showDetails(void) const
 *  \brief Pops up a DialogBox with program info, blocking until user exits
 *         the dialog.
 */
void ProgramInfo::showDetails(void) const
{
    MSqlQuery query(MSqlQuery::InitCon());
    QString fullDateFormat = gContext->GetSetting("DateFormat", "M/d/yyyy");
    if (fullDateFormat.find(QRegExp("yyyy")) < 0)
        fullDateFormat += " yyyy";
    QString category_type, epinum, rating;
    int partnumber = 0, parttotal = 0;
    int stereo = 0, subtitled = 0, hdtv = 0, closecaptioned = 0;

    if (endts != startts)
    {
        if (filesize > 0)
            query.prepare("SELECT category_type, partnumber,"
                          " parttotal, stereo, subtitled, hdtv,"
                          " closecaptioned, syndicatedepisodenumber"
                          " FROM recordedprogram WHERE chanid = :CHANID"
                          " AND starttime = :STARTTIME ;");
        else
            query.prepare("SELECT category_type, partnumber,"
                          " parttotal, stereo, subtitled, hdtv,"
                          " closecaptioned, syndicatedepisodenumber"
                          " FROM program WHERE chanid = :CHANID"
                          " AND starttime = :STARTTIME ;");

        query.bindValue(":CHANID", chanid);
        query.bindValue(":STARTTIME", startts);

        if (query.exec() && query.isActive() && query.size() > 0)
        {
            query.next();
            category_type = query.value(0).toString();
            partnumber = query.value(1).toInt();
            parttotal = query.value(2).toInt();
            stereo = query.value(3).toInt();
            subtitled = query.value(4).toInt();
            hdtv = query.value(5).toInt();
            closecaptioned = query.value(6).toInt();
            epinum = query.value(7).toString();
        }

        if (filesize > 0)
            query.prepare("SELECT rating FROM recordedrating"
                          " WHERE chanid = :CHANID"
                          " AND starttime = :STARTTIME ;");
        else
            query.prepare("SELECT rating FROM programrating"
                          " WHERE chanid = :CHANID"
                          " AND starttime = :STARTTIME ;");

        query.bindValue(":CHANID", chanid);
        query.bindValue(":STARTTIME", startts);
        
        if (query.exec() && query.isActive() && query.size() > 0)
        {
            query.next();
            rating = query.value(0).toString();
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

    QString msg = QObject::tr("Title") + ":  " + title;

    if (subtitle != "")
        msg += " - \"" + subtitle + "\"";

    if (description  != "")
        msg += "\n" + QObject::tr("Description") + ":  " + description;

    QString attr = "";

    if (partnumber > 0)
        attr = QString(QObject::tr("Part %1 of %2, ")).arg(partnumber).arg(parttotal);

    if (rating != "" && rating != "NR")
        attr += rating + ", ";

    if (category_type == "movie")
    {
        if (year != "")
            attr += year + ", ";

        if (stars > 0.0)
        {
            QString str = QObject::tr("stars");
            if (stars > 0 && stars <= 0.25)
                str = QObject::tr("star");

            attr += QString("%1 %2, ").arg(4.0 * stars).arg(str);
        }
    }

    if (hdtv)
        attr += QObject::tr("HDTV") + ", ";
    if (closecaptioned)
        attr += QObject::tr("CC","Close Captioned") + ", ";
    if (subtitled)
        attr += QObject::tr("Subtitled") + ", ";
    if (stereo)
        attr += QObject::tr("Stereo") + ", ";
    if (repeat)
        attr += QObject::tr("Repeat") + ", ";

    if (attr != "")
    {
        attr.truncate(attr.findRev(','));
        msg += " (" + attr + ")";
    }
    msg += "\n";

    if (category != "")
    {
        msg += QObject::tr("Category") + ":  " +
               qApp->translate("Category", category);

        query.prepare("SELECT genre FROM programgenres "
                      "WHERE chanid = :CHANID AND starttime = :STARTTIME "
                      "AND relevance > 0 ORDER BY relevance;");

        query.bindValue(":CHANID", chanid);
        query.bindValue(":STARTTIME", startts);
        
        if (query.exec() && query.isActive() && query.size() > 0)
        {
            while (query.next())
                msg += ", " + query.value(0).toString();
        }
        msg += "\n";
    }

    if (category_type  != "")
    {
        msg += QObject::tr("Type","category_type") + ":  " + category_type;
        if (seriesid != "")
            msg += "  (" + seriesid + ")";
        msg += "\n";
    }

    if (epinum != "")
        msg += QObject::tr("Episode Number") + ":  " + epinum + "\n";

    if (hasAirDate && category_type != "movie")
    {
        msg += QObject::tr("Original Airdate") + ":  ";
        msg += originalAirDate.toString(fullDateFormat) + "\n";
    }
    if (programid  != "")
        msg += QObject::tr("Program ID") + ":  " + programid + "\n";

    QString role = "", pname = "";

    if (endts != startts)
    {
        if (filesize > 0)
            query.prepare("SELECT role,people.name FROM recordedcredits"
                          " AS credits"
                          " LEFT JOIN people ON credits.person = people.person"
                          " WHERE credits.chanid = :CHANID"
                          " AND credits.starttime = :STARTTIME"
                          " ORDER BY role;");
        else
            query.prepare("SELECT role,people.name FROM credits"
                          " LEFT JOIN people ON credits.person = people.person"
                          " WHERE credits.chanid = :CHANID"
                          " AND credits.starttime = :STARTTIME"
                          " ORDER BY role;");
        query.bindValue(":CHANID", chanid);
        query.bindValue(":STARTTIME", startts);

        if (query.exec() && query.isActive() && query.size() > 0)
        {
            QString rstr = "", plist = "";

            while(query.next())
            {
                role = query.value(0).toString();
                pname = query.value(1).toString();

                if (rstr == role)
                    plist += ", " + pname;
                else
                {
                    // if (rstr != "")
                    //    msg += QString("%1:  %2\n").arg(rstr).arg(plist);
                    // Only print actors, guest star and director for now.

                    if (rstr == "actor")
                        msg += QObject::tr("Actors") + ":  " + plist + "\n";
                    if (rstr == "guest_star")
                        msg += QObject::tr("Guest Star") + ":  " + plist + "\n";
                    if (rstr == "director")
                        msg += QObject::tr("Director") + ":  " + plist + "\n";

                    rstr = role;
                    plist = pname;
                }
            }
            // if (rstr != "")
            //    msg += QString("%1:  %2\n").arg(rstr).arg(plist);

            if (rstr == "actor")
                msg += QObject::tr("Actors") + ":  " + plist + "\n";
            if (rstr == "guest_star")
                msg += QObject::tr("Guest Star") + ":  " + plist + "\n";
            if (rstr == "director")
                msg += QObject::tr("Director") + ":  " + plist + "\n";
        }
    }

    QDateTime statusDate;
    if (recstatus == rsWillRecord)
        statusDate = startts;

    ProgramInfo *p = new ProgramInfo;
    p->rectype = kSingleRecord; // must be != kNotRecording
    p->recstatus = recstatus;

    if (p->recstatus == rsPreviousRecording ||  p->recstatus == rsUnknown)
    {
        query.prepare("SELECT recstatus, starttime "
                      "FROM oldrecorded WHERE duplicate > 0 AND "
                      "((programid <> '' AND programid = :PROGRAMID) OR "
                      " (title <> '' AND title = :TITLE AND "
                      "  subtitle <> '' AND subtitle = :SUBTITLE AND "
                      "  description <> '' AND description = :DECRIPTION));");

        query.bindValue(":PROGRAMID", programid);
        query.bindValue(":TITLE", title);
        query.bindValue(":SUBTITLE", subtitle);
        query.bindValue(":DECRIPTION", description);

        if (!query.exec() || !query.isActive())
            MythContext::DBError("showDetails", query);

        if (query.isActive() && query.size() > 0)
        {
            query.next();
            if (p->recstatus == rsUnknown)
                p->recstatus = RecStatusType(query.value(0).toInt());
            if (p->recstatus == rsPreviousRecording || 
                p->recstatus == rsRecorded)
                statusDate = QDateTime::fromString(query.value(1).toString(),
                                                  Qt::ISODate);
        }
    }
    if (p->recstatus == rsUnknown)
    {
        if (filesize > 0)
        {
            p->recstatus = rsRecorded;
            statusDate = startts;
        }
        else
        {
            p->rectype = rectype; // re-enable "Not Recording" status text.
        }
    }
    msg += "\nMythTV " + QObject::tr("Status: ") + p->RecStatusText();
    if (statusDate.isValid())
        msg += " " + statusDate.toString(fullDateFormat);
    msg += "\n";
    delete p;

    if (findid > 0)
    {
        QDate fdate = QDate::QDate (1970, 1, 1);
        fdate = fdate.addDays(findid - 719528);
        msg += QString("%1: %2 (%3)\n").arg(QObject::tr("Find ID"))
                       .arg(findid).arg(fdate.toString(fullDateFormat));
    }
    if (filesize > 0)
    {
        QString tmpSize;

        tmpSize.sprintf("%0.2f ", filesize / 1024.0 / 1024.0 / 1024.0);
        tmpSize += QObject::tr("GB", "GigaBytes");
    
        msg += QObject::tr("Recording Host") + ":  " + hostname + "\n";
        msg += QObject::tr("Filesize") + ":  " + tmpSize + "\n";
        msg += QObject::tr("Recording Group") + ":  " + recgroup + "\n";
    }
    DialogBox *details_dialog = new DialogBox(gContext->GetMainWindow(), msg);
    details_dialog->AddButton(QObject::tr("OK"));
    details_dialog->exec();

    delete details_dialog;
}

/** \fn ProgramInfo::getProgramFlags(void) const
 *  \brief Returns a bitmap of which jobs have been completed for recording.
 *  \sa GetAutoRunJobs(void)
 */
int ProgramInfo::getProgramFlags(void) const
{
    int flags = 0;
    MSqlQuery query(MSqlQuery::InitCon());
    
    query.prepare("SELECT commflagged, cutlist, autoexpire, "
                  "editing, bookmark FROM recorded WHERE "
                  "chanid = :CHANID AND starttime = :STARTTIME ;");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);

    if (query.exec() && query.isActive() && query.size() > 0)
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

void ProgramInfo::UpdateInUseMark(bool force)
{
    if (force || lastInUseTime.secsTo(QDateTime::currentDateTime()) > 60 * 60)
        MarkAsInUse(true);
}

void ProgramInfo::MarkAsInUse(bool inuse, QString usedFor)
{
    bool notifyOfChange = false;

    if (inuse && inUseForWhat.length() < 2)
    {
        if (usedFor != "")
            inUseForWhat = usedFor;
        else
            inUseForWhat = QObject::tr("Unknown") + " [" +
                           QString::number(getpid()) + "]";

        notifyOfChange = true;
    }

    if (!inuse && inUseForWhat.length() < 2)
        return; // can't delete if we don't have a key

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("DELETE FROM inuseprograms WHERE "
                  "chanid = :CHANID AND starttime = :STARTTIME AND "
                  "hostname = :HOSTNAME AND recusage = :RECUSAGE ;");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);
    query.bindValue(":HOSTNAME", gContext->GetHostName());
    query.bindValue(":RECUSAGE", inUseForWhat);

    query.exec();

    if (!inuse)
    {
        if (!gContext->IsBackend())
            RemoteSendMessage("RECORDING_LIST_CHANGE");
        inUseForWhat = "";
        return;
    }

    lastInUseTime = QDateTime::currentDateTime();

    query.prepare("INSERT INTO inuseprograms "
                  " (chanid, starttime, recusage, hostname, lastupdatetime) "
                  " VALUES "
                  " (:CHANID, :STARTTIME, :RECUSAGE, :HOSTNAME, :UPDATETIME);");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);
    query.bindValue(":HOSTNAME", gContext->GetHostName());
    query.bindValue(":RECUSAGE", inUseForWhat);
    query.bindValue(":UPDATETIME", lastInUseTime);

    if (!query.exec() || !query.isActive())
        MythContext::DBError("SetInUse", query);

    // Let others know we changed status
    if (notifyOfChange && !gContext->IsBackend())
        RemoteSendMessage("RECORDING_LIST_CHANGE");
}

/** \fn ProgramInfo::GetChannel(const ProgramInfo*,QString&,QString&) const
 *  \brief Returns the channel and input needed to record the program.
 *  \return true on success, false on failure
 */
bool ProgramInfo::GetChannel(QString &channum, QString &input) const
{
    channum = input = QString::null;
    MSqlQuery query(MSqlQuery::InitCon());   

    query.prepare("SELECT channel.channum, cardinput.inputname "
                  "FROM channel, capturecard, cardinput "
                  "WHERE channel.chanid     = :CHANID            AND "
                  "      cardinput.cardid   = capturecard.cardid AND "
                  "      cardinput.sourceid = :SOURCEID          AND "
                  "      capturecard.cardid = :CARDID");
    query.bindValue(":CHANID",   chanid);
    query.bindValue(":SOURCEID", sourceid);
    query.bindValue(":CARDID",   cardid);

    if (query.exec() && query.isActive() && query.next())
    {
        channum = query.value(0).toString();
        input   = query.value(1).toString();
        return true;
    } 
    else 
    {
        MythContext::DBError("GetChannel(ProgInfo...)", query);
        return false;
    }
}

void ProgramInfo::ShowRecordingDialog(void)
{
    QDateTime now = QDateTime::currentDateTime();

    QString message = title;

    if (subtitle != "")
        message += QString(" - \"%1\"").arg(subtitle);

    message += "\n\n";
    message += RecStatusDesc();

    DialogBox diag(gContext->GetMainWindow(), message);
    int button = 1, ok = -1, react = -1, stop = -1, addov = -1, forget = -1,
        clearov = -1, edend = -1, ednorm = -1, edcust = -1;

    diag.AddButton(QObject::tr("OK"));
    ok = button++;

    if (recstartts < now && recendts > now)
    {
        if (recstatus != rsRecording)
        {
            diag.AddButton(QObject::tr("Reactivate"));
            react = button++;
        }
        else
        {
            diag.AddButton(QObject::tr("Stop recording"));
            stop = button++;
        }
    }
    if (recendts > now)
    {
        if (rectype != kSingleRecord && rectype != kOverrideRecord)
        {
            if (recstartts > now)
            {
                diag.AddButton(QObject::tr("Don't record"));
                addov = button++;
            }
            if (recstatus != rsRecording && rectype != kFindOneRecord &&
                !((findid == 0 || !IsFindApplicable()) &&
                  catType == "series" &&
                  programid.contains(QRegExp("0000$"))) &&
                ((!(dupmethod & kDupCheckNone) && programid != "" && 
                  (findid != 0 || !IsFindApplicable())) ||
                 ((dupmethod & kDupCheckSub) && subtitle != "") ||
                 ((dupmethod & kDupCheckDesc) && description != "")))
            {
                diag.AddButton(QObject::tr("Never record"));
                forget = button++;
            }
        }

        if (rectype != kOverrideRecord && rectype != kDontRecord)
        {
            if (recstatus == rsRecording)
            {
                diag.AddButton(QObject::tr("Change Ending Time"));
                edend = button++;
            }
            else
            {
                diag.AddButton(QObject::tr("Edit Options"));
                ednorm = button++;

                if (rectype != kSingleRecord && rectype != kFindOneRecord)
                {
                    diag.AddButton(QObject::tr("Add Override"));
                    edcust = button++;
                }
            }
        }

        if (rectype == kOverrideRecord || rectype == kDontRecord)
        {
            diag.AddButton(QObject::tr("Edit Override"));
            ednorm = button++;
            if (recstatus != rsRecording)
            {
                diag.AddButton(QObject::tr("Clear Override"));
                clearov = button++;
            }
        }
    }

    int ret = diag.exec();

    if (ret == react)
        ReactivateRecording();
    else if (ret == stop)
    {
        ProgramInfo *p = GetProgramFromRecorded(chanid, recstartts);
        if (p)
        {
            RemoteStopRecording(p);
            delete p;
        }
    }
    else if (ret == addov)
        ApplyRecordStateChange(kDontRecord);
    else if (ret == forget)
    {
        recstatus = rsNeverRecord;
        startts = QDateTime::currentDateTime();
        endts = recstartts;
        AddHistory();
    }
    else if (ret == clearov)
        ApplyRecordStateChange(kNotRecording);
    else if (ret == edend)
    {
        GetProgramRecordingStatus();
        if (rectype != kSingleRecord && rectype != kOverrideRecord &&
            rectype != kFindOneRecord)
        {
            record->makeOverride();
            record->setRecordingType(kOverrideRecord);
        }
        record->exec();
    }
    else if (ret == ednorm)
    {
        GetProgramRecordingStatus();
        record->exec();
    }
    else if (ret == edcust)
    {
        GetProgramRecordingStatus();
        record->makeOverride();
        record->exec();
    }

    return;
}

void ProgramInfo::ShowNotRecordingDialog(void)
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
        ednorm = -1, edcust = -1, forget = -1, addov1 = -1, forget1 = -1;

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
             rectype != kOverrideRecord) &&
            (recstatus == rsDontRecord ||
             recstatus == rsPreviousRecording ||
             recstatus == rsCurrentRecording ||
             recstatus == rsEarlierShowing ||
             recstatus == rsRepeat ||
             recstatus == rsInactive ||
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
            if (rectype != kSingleRecord &&
                recstatus != rsPreviousRecording &&
                recstatus != rsCurrentRecording)
            {
                if (recstartts > now)
                {
                    diag.AddButton(QObject::tr("Absolutely don't record"));
                    addov1 = button++;
                }
                if (rectype != kFindOneRecord &&
                    !((findid == 0 || !IsFindApplicable()) &&
                      catType == "series" &&
                      programid.contains(QRegExp("0000$"))) &&
                    ((!(dupmethod & kDupCheckNone) && programid != "" && 
                      (findid != 0 || !IsFindApplicable())) ||
                     ((dupmethod & kDupCheckSub) && subtitle != "") ||
                     ((dupmethod & kDupCheckDesc) && description != "")))
                {
                    diag.AddButton(QObject::tr("Never record"));
                    forget1 = button++;
                }
            }

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
        ReactivateRecording();
    else if (ret == addov)
    {
        ApplyRecordStateChange(kOverrideRecord);
        if (recstartts < now)
            ReactivateRecording();
    }
    else if (ret == forget)
        ForgetHistory();
    else if (ret == addov1)
        ApplyRecordStateChange(kDontRecord);
    else if (ret == forget1)
    {
        recstatus = rsNeverRecord;
        startts = QDateTime::currentDateTime();
        endts = recstartts;
        AddHistory();
    }
    else if (ret == clearov)
        ApplyRecordStateChange(kNotRecording);
    else if (ret == ednorm)
    {
        GetProgramRecordingStatus();
        record->exec();
    }
    else if (ret == edcust)
    {
        GetProgramRecordingStatus();
        record->makeOverride();
        record->exec();
    }

    return;
}

/* ************************************************************************* *
 *                                                                           *
 *        Below this comment various ProgramList functions are defined.      *
 *                                                                           *
 * ************************************************************************* */

bool ProgramList::FromScheduler(bool &hasConflicts, QString rectable,
                                int recordid)
{
    clear();
    hasConflicts = false;

    if (gContext->IsBackend())
        return false;

    QString query;
    if (rectable != "")
    {
        query = QString("QUERY_GETALLPENDING %1 %2")
                        .arg(rectable).arg(recordid);
    } else {
        query = QString("QUERY_GETALLPENDING");
    }

    QStringList slist = query;
    if (!gContext->SendReceiveStringList(slist) || slist.size() < 2)
    {
        VERBOSE(VB_IMPORTANT,
                "ProgramList::FromScheduler(): Error querying master.");
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
        VERBOSE(VB_IMPORTANT,
                "ProgramList::FromScheduler(): Length mismatch");
        clear();
        result = false;
    }

    return result;
}

bool ProgramList::FromProgram(const QString &sql, MSqlBindings &bindings,
                              ProgramList &schedList)
{
    clear();

    QString querystr = QString(
        "SELECT DISTINCT program.chanid, program.starttime, program.endtime, "
        "    program.title, program.subtitle, program.description, "
        "    program.category, channel.channum, channel.callsign, "
        "    channel.name, program.previouslyshown, channel.commfree, "
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
        querystr += QString(" ORDER BY program.starttime, ") + 
                    gContext->GetSetting("ChannelOrdering", "channum+0") + " ";
    if (!sql.contains(" LIMIT "))
        querystr += " LIMIT 1000 ";

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(querystr);
    query.bindValues(bindings);
    
    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("ProgramList::FromProgram", 
                             query);
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
        p->year = query.value(15).toString();
        p->stars = query.value(16).toString().toFloat();
        
        if (query.value(17).isNull() || query.value(17).toString().isEmpty())
        {
            p->originalAirDate = p->startts.date();
            p->hasAirDate = false;
        }
        else
        {
            p->originalAirDate = QDate::fromString(query.value(17).toString(),
                                                   Qt::ISODate);
            p->hasAirDate = true;
        }
        p->catType = query.value(18).toString();
        p->recordid = query.value(19).toInt();
        p->rectype = RecordingType(query.value(20).toInt());
        p->recstatus = RecStatusType(query.value(21).toInt());
        p->findid = query.value(22).toInt();

        ProgramInfo *s;
        for (s = schedList.first(); s; s = schedList.next())
        {
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
            }
        }

        append(p);
    }

    return true;
}

bool ProgramList::FromOldRecorded(const QString &sql, MSqlBindings &bindings)
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
        MythContext::DBError("ProgramList::FromOldRecorded", 
                             query);
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
        p->seriesid = query.value(7).toString();
        p->programid = query.value(8).toString();
        p->chanstr = query.value(9).toString();
        p->chansign = QString::fromUtf8(query.value(10).toString());
        p->channame = QString::fromUtf8(query.value(11).toString());
        p->findid = query.value(12).toInt();
        p->rectype = RecordingType(query.value(13).toInt());
        p->recstatus = RecStatusType(query.value(14).toInt());
        p->recordid = query.value(15).toInt();
        p->duplicate = query.value(16).toInt();

        append(p);
    }

    return true;
}

int ProgramList::compareItems(QPtrCollection::Item item1,
                              QPtrCollection::Item item2)
{
    if (compareFunc)
        return compareFunc(reinterpret_cast<ProgramInfo *>(item1),
                           reinterpret_cast<ProgramInfo *>(item2));
    else
        return 0;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
