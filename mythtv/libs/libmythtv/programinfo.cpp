// POSIX headers
#include <sys/types.h>
#include <unistd.h>

// C headers
#include <cstdlib>

// C++ headers
#include <iostream>
#include <algorithm>
using namespace std;

// Qt headers
#include <QRegExp>
#include <QMap>
#include <QLayout>
#include <QLabel>
#include <QApplication>
#include <QFile>
#include <QFileInfo>

// MythTV headers
#include "programinfo.h"
#include "progdetails.h"
#include "scheduledrecording.h"
#include "util.h"
#include "mythcontext.h"
#include "dialogbox.h"
#include "remoteutil.h"
#include "jobqueue.h"
#include "mythdb.h"
#include "mythverbose.h"
#include "storagegroup.h"
#include "previewgenerator.h"
#include "channelutil.h" // for SkipTypes
#include "programlist.h"

#define LOC QString("ProgramInfo: ")
#define LOC_ERR QString("ProgramInfo, Error: ")

using namespace std;

// works only for integer divisors of 60
static const uint kUnknownProgramLength = 30;

static bool insert_program(const ProgramInfo*,
                           const ScheduledRecording*);

static int init_tr(void);

/** \fn StripHTMLTags(const QString&)
 *  \brief Returns a copy of "src" with all the HTML tags removed.
 *
 *   Three tags are respected: <br> and <p> are replaced with newlines,
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
ProgramInfo::ProgramInfo(void) :
    regExpLock(QMutex::NonRecursive), regExpSeries("0000$"),
    positionMapDBReplacement(NULL)
{
    spread = -1;
    startCol = -1;
    isVideo = false;
    lenMins = 0;

    title = "";
    subtitle = "";
    description = "";
    category = "";
    chanstr = "";
    chansign = "";
    channame = "";
    chancommfree = 0;
    chanOutputFilters = "";
    year = "";
    stars = 0;
    availableStatus = asAvailable;

    pathname = "";
    storagegroup = QString("Default");
    filesize = 0;
    hostname = "";
    programflags = 0;
    transcoder = 0;
    audioproperties = 0;
    videoproperties = 0;
    subtitleType = 0;

    startts = mythCurrentDateTime();
    endts = startts;
    recstartts = startts;
    recendts = startts;
    originalAirDate = QDate (0, 1, 1);
    lastmodified = startts;
    lastInUseTime = startts.addSecs(-4 * 60 * 60);

    recstatus = rsUnknown;
    oldrecstatus = rsUnknown;
    savedrecstatus = rsUnknown;
    prefinput = 0;
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
    playgroup = QString("Default");

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
ProgramInfo::ProgramInfo(const ProgramInfo &other) :
    record(NULL), regExpLock(QMutex::NonRecursive), regExpSeries("0000$")
{
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
    {
        record->deleteLater();
        record = NULL;
    }

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
    storagegroup = other.storagegroup;
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
    playgroup = other.playgroup;
    programflags = other.programflags;
    transcoder = other.transcoder;
    audioproperties = other.audioproperties;
    videoproperties = other.videoproperties;
    subtitleType = other.subtitleType;

    hasAirDate = other.hasAirDate;
    repeat = other.repeat;

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

    positionMapDBReplacement = other.positionMapDBReplacement;

    title.detach();
    subtitle.detach();
    description.detach();
    category.detach();

    chanid.detach();
    chanstr.detach();
    chansign.detach();
    channame.detach();

    recgroup.detach();
    playgroup.detach();

    pathname.detach();
    hostname.detach();
    storagegroup.detach();

    year.detach();

    schedulerid.detach();

    chanOutputFilters.detach();

    seriesid.detach();
    programid.detach();
    catType.detach();

    sortTitle.detach();
    inUseForWhat.detach();

    return *this;
}

/** \fn ProgramInfo::~ProgramInfo()
 *  \brief Destructor deletes "record" if it exists.
 */
ProgramInfo::~ProgramInfo()
{
    if (record)
    {
        record->deleteLater();
        record = NULL;
    }
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
#define DATE_TO_LIST(x)      STR_TO_LIST((x).toString(Qt::ISODate))

#define FLOAT_TO_LIST(x)     sprintf(tmp, "%f", (x)); list << tmp;

/** \fn ProgramInfo::ToStringList(QStringList&) const
 *  \brief Serializes ProgramInfo into a QStringList which can be passed
 *         over a socket.
 *  \sa FromStringList(QStringList::const_iterator&,
                       QStringList::const_iterator)
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
    DATE_TO_LIST(originalAirDate)
    INT_TO_LIST(hasAirDate)
    STR_TO_LIST((playgroup != "") ? playgroup : "Default")
    INT_TO_LIST(recpriority2)
    INT_TO_LIST(parentid)
    STR_TO_LIST((storagegroup != "") ? storagegroup : "Default")
    INT_TO_LIST(audioproperties)
    INT_TO_LIST(videoproperties)
    INT_TO_LIST(subtitleType)
    STR_TO_LIST(year)
/* do not forget to update the NUMPROGRAMLINES defines! */
}

/** \fn ProgramInfo::FromStringList(const QStringList&,uint)
 *  \brief Uses a QStringList to initialize this ProgramInfo instance.
 *
 *  This is a convenience method which calls FromStringList(
    QStringList::const_iterator&,QStringList::const_iterator)
 *  with an iterator created using list.at(offset).
 *
 *  \param list   QStringList containing serialized ProgramInfo.
 *  \param offset First field in list to treat as beginning of
 *                serialized ProgramInfo.
 *  \return true if it succeeds, false if it fails.
 *  \sa FromStringList(QStringList::const_iterator&,
                       QStringList::const_iterator)
 *      ToStringList(QStringList&) const
 */
bool ProgramInfo::FromStringList(const QStringList &list, uint offset)
{
    QStringList::const_iterator it = list.begin()+offset;
    return FromStringList(it, list.end());
}

#define NEXT_STR()             if (it == listend)     \
                               {                      \
                                   VERBOSE(VB_IMPORTANT, listerror); \
                                   return false;      \
                               }                      \
                               ts = *it++;            \
                               if (ts.isNull())       \
                                   ts = "";

#define INT_FROM_LIST(x)       NEXT_STR() (x) = ts.toInt();
#define ENUM_FROM_LIST(x, y)   NEXT_STR() (x) = ((y)ts.toInt());

#define DATETIME_FROM_LIST(x)  NEXT_STR() (x).setTime_t(ts.toUInt());
#define DATE_FROM_LIST(x)      NEXT_STR() (x) = \
                                   ((ts.isEmpty()) || (ts == "0000-00-00")) ?\
                                   QDate() : QDate::fromString(ts, Qt::ISODate)

#define LONGLONG_FROM_LIST(x)  INT_FROM_LIST(ti); NEXT_STR() \
                               (x) = ((long long)(ti) << 32) | \
                               ((long long)(ts.toInt()) & 0xffffffffLL);

#define STR_FROM_LIST(x)       NEXT_STR() (x) = ts;

#define FLOAT_FROM_LIST(x)     NEXT_STR() (x) = ts.toFloat();

/** \fn ProgramInfo::FromStringList(QStringList::const_iterator&,
                                    QStringList::const_iterator)
 *  \brief Uses a QStringList to initialize this ProgramInfo instance.
 *  \param beg    Iterator pointing to first item in list to treat as
 *                beginning of serialized ProgramInfo.
 *  \param end    Iterator that will stop parsing of the ProgramInfo
 *  \return true if it succeeds, false if it fails.
 *  \sa FromStringList(const QStringList&,uint)
 *      ToStringList(QStringList&) const
 */

bool ProgramInfo::FromStringList(QStringList::const_iterator &it,
                                 QStringList::const_iterator  listend)
{
    QString listerror = LOC + "FromStringList, not enough items in list.";
    QString ts;
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
    STR_FROM_LIST(playgroup)
    INT_FROM_LIST(recpriority2)
    INT_FROM_LIST(parentid)
    STR_FROM_LIST(storagegroup)
    INT_FROM_LIST(audioproperties)
    INT_FROM_LIST(videoproperties)
    INT_FROM_LIST(subtitleType)
    STR_FROM_LIST(year)

    return true;
}

/** \fn ProgramInfo::ToMap(QMap<QString,QString>&,bool) const
 *  \brief Converts ProgramInfo into QString QMap containing each field
 *         in ProgramInfo converted into localized strings.
 */
void ProgramInfo::ToMap(QMap<QString, QString> &progMap,
                        bool showrerecord) const
{
    QString timeFormat = gContext->GetSetting("TimeFormat", "h:mm AP");
    QString dateFormat = gContext->GetSetting("DateFormat", "ddd MMMM d");
    QString fullDateFormat = dateFormat;
    if (!fullDateFormat.contains("yyyy"))
        fullDateFormat += " yyyy";
    QString shortDateFormat = gContext->GetSetting("ShortDateFormat", "M/d");
    QString channelFormat =
        gContext->GetSetting("ChannelFormat", "<num> <sign>");
    QString longChannelFormat =
        gContext->GetSetting("LongChannelFormat", "<num> <name>");

    QDateTime timeNow = QDateTime::currentDateTime();

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
           progMap["startdate"] = "";
           progMap["recstartdate"] = "";
        }
        else
        {
            progMap["startdate"] = startts.toString("yyyy");
            progMap["recstartdate"] = startts.toString("yyyy");
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

    progMap["lenmins"] = QObject::tr("%n minute(s)","",minutes);
    hours   = minutes / 60;
    minutes = minutes % 60;

    QString minstring = QObject::tr("%n minute(s)","",minutes);

    if (hours > 0)
    {
        progMap["lentime"] = QString("%1 %2")
                                    .arg(QObject::tr("%n hour(s)","", hours))
                                    .arg(minstring);
    }
    else
        progMap["lentime"] = minstring;

    progMap["rec_type"] = RecTypeChar();
    progMap["rec_str"] = RecTypeText();
    if (rectype != kNotRecording)
    {
        QString tmp_rec;
        if (((recendts > timeNow) && (recstatus <= rsWillRecord)) ||
            (recstatus == rsConflict) || (recstatus == rsLaterShowing))
        {
            tmp_rec += QString().sprintf(" %+d", recpriority);
            if (recpriority2)
                tmp_rec += QString().sprintf("/%+d", recpriority2);
            tmp_rec += " ";
        }
        else
        {
            tmp_rec += " -- ";
        }
        if (showrerecord && recstatus == rsRecorded && !duplicate)
            tmp_rec += QObject::tr("Re-Record");
        else
            tmp_rec += RecStatusText();
        progMap["rec_str"] += tmp_rec;
    }
    progMap["recordingstatus"] = progMap["rec_str"];
    progMap["type"] = progMap["rec_str"];

    progMap["recpriority"] = recpriority;
    progMap["recpriority2"] = recpriority2;
    progMap["recgroup"] = recgroup;
    progMap["playgroup"] = playgroup;
    progMap["programflags"] = programflags;

    progMap["audioproperties"] = audioproperties;
    progMap["videoproperties"] = videoproperties;
    progMap["subtitleType"] = subtitleType;

    progMap["timedate"] = recstartts.date().toString(dateFormat) + ", " +
                          recstartts.time().toString(timeFormat) + " - " +
                          recendts.time().toString(timeFormat);

    progMap["shorttimedate"] =
                          recstartts.date().toString(shortDateFormat) + ", " +
                          recstartts.time().toString(timeFormat) + " - " +
                          recendts.time().toString(timeFormat);

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT icon FROM channel WHERE chanid = :CHANID ;");
    query.bindValue(":CHANID", chanid);

    if (query.exec() && query.next())
        progMap["iconpath"] = query.value(0).toString();

    progMap["RECSTATUS"] = RecStatusText();

    if (repeat)
    {
        progMap["REPEAT"] = QString("(%1) ").arg(QObject::tr("Repeat"));
        progMap["LONGREPEAT"] = progMap["REPEAT"];
        if (hasAirDate)
            progMap["LONGREPEAT"] = QString("(%1 %2) ")
                                .arg(QObject::tr("Repeat"))
                                .arg(originalAirDate.toString(fullDateFormat));
    }
    else
    {
        progMap["REPEAT"] = "";
        progMap["LONGREPEAT"] = "";
    }

    progMap["seriesid"] = seriesid;
    progMap["programid"] = programid;
    progMap["catType"] = catType;

    progMap["year"] = year == "0" ? "" : year;

    if (stars)
    {
        progMap["stars"] = QObject::tr("%n star(s)", "", (int)(stars * 4));
    }
    else
        progMap["stars"] = "";

    if (stars)
    {
        QString str = QObject::tr("%n star(s)", "", (int)(stars * 4));
        progMap["yearstars"] = QString("(%1, %2) ").arg(year).arg(str);
    }
    else
        progMap["yearstars"] = "";

    if (!hasAirDate ||
        (!programid.isEmpty() && (programid.left(2) == "MV")))
    {
        progMap["originalairdate"] = "";
        progMap["shortoriginalairdate"] = "";
    }
    else
    {
        progMap["originalairdate"] = originalAirDate.toString(dateFormat);
        progMap["shortoriginalairdate"] =
                                originalAirDate.toString(shortDateFormat);
    }
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

/**
 *  \brief Returns a new ProgramInfo for the program that air at
 *         "dtime" on "channel".
 *  \param chanid  %Channel ID on which to search for program.
 *  \param dtime   Date and Time for which we desire the program.
 *  \param genUnknown Generate a full entry for live-tv if unknown
 *  \param clampHoursMax Clamp the maximum time to X hours from dtime.
 *  \return Pointer to a ProgramInfo from database if it succeeds,
 *          Pointer to an "Unknown" ProgramInfo if it does not find
 *          anything in database.
 */
ProgramInfo *ProgramInfo::GetProgramAtDateTime(uint chanid,
                                               const QDateTime &dtime,
                                               bool genUnknown,
                                               int clampHoursMax)
{
    ProgramList schedList;
    ProgramList progList;

    MSqlBindings bindings;
    QString querystr = "WHERE program.chanid = :CHANID "
                       "  AND program.starttime < :STARTTS1 "
                       "  AND program.endtime > :STARTTS2 ";
    bindings[":CHANID"] = QString::number(chanid);
    QString startts = dtime.toString("yyyy-MM-ddThh:mm:50");
    bindings[":STARTTS1"] = startts;
    bindings[":STARTTS2"] = startts;

    schedList.FromScheduler();
    progList.FromProgram(querystr, bindings, schedList);

    if (!progList.isEmpty())
    {
        ProgramInfo *pginfo = progList.take(0);

        if (clampHoursMax > 0)
        {
            if (dtime.secsTo(pginfo->endts) > clampHoursMax * 3600)
            {
                pginfo->endts = dtime.addSecs(clampHoursMax * 3600);
                pginfo->recendts = pginfo->endts;
            }
        }

        return pginfo;
    }
    ProgramInfo *p = new ProgramInfo;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT chanid, channum, callsign, name, "
                  "commmethod, outputfilters "
                  "FROM channel "
                  "WHERE chanid = :CHANID ;");
    query.bindValue(":CHANID", chanid);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError(LOC + "GetProgramAtDateTime", query);
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
    p->chansign           = query.value(2).toString();
    p->channame           = query.value(3).toString();
    p->repeat             = 0;
    p->chancommfree       = COMM_DETECT_COMMFREE == query.value(4).toInt();
    p->chanOutputFilters  = query.value(5).toString();
    p->seriesid           = "";
    p->programid          = "";
    p->year               = "";
    p->stars              = 0.0f;

    if (!genUnknown)
        return p;

    // Round endtime up to the next half-hour.
    p->endts.setTime(QTime(p->endts.time().hour(),
                           p->endts.time().minute() / kUnknownProgramLength
                           * kUnknownProgramLength));
    p->endts = p->endts.addSecs(kUnknownProgramLength * 60);

    // if under a minute, bump it up to the next half hour
    if (p->startts.secsTo(p->endts) < 60)
        p->endts = p->endts.addSecs(kUnknownProgramLength * 60);

    p->recendts = p->endts;

    // Find next program starttime
    bindings.clear();
    QDateTime nextstart = p->startts;
    querystr = "WHERE program.chanid    = :CHANID  AND "
               "      program.starttime > :STARTTS "
               "GROUP BY program.starttime ORDER BY program.starttime LIMIT 1 ";
    bindings[":CHANID"]  = QString::number(chanid);
    bindings[":STARTTS"] = dtime.toString("yyyy-MM-ddThh:mm:50");

    progList.FromProgram(querystr, bindings, schedList);

    if (!progList.isEmpty())
        nextstart = (*progList.begin())->startts;

    if (nextstart > p->startts && nextstart < p->recendts)
    {
        p->recendts = p->endts = nextstart;
    }

    return p;
}

QString ProgramInfo::toString(void) const
{
    QString str("");
    str += LOC + "channame(" + channame + ") startts(" +
        startts.toString() + ") endts(" + endts.toString() + ")\n";
    str += "             recstartts(" + recstartts.toString() +
        ") recendts(" + recendts.toString() + ")\n";
    str += "             title(" + title + ")";
    return str;
}

/**
 *  \brief Returns a new ProgramInfo for an existing recording.
 *  \return Pointer to a ProgramInfo if it succeeds, NULL otherwise.
 */
ProgramInfo *ProgramInfo::GetProgramFromBasename(const QString filename)
{
    QFileInfo inf(filename);

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT chanid, starttime FROM recorded "
                  "WHERE basename = :BASENAME;");
    query.bindValue(":BASENAME", inf.fileName());

    if (query.exec() && query.next())
    {
        return GetProgramFromRecorded(query.value(0).toString(),
                                      query.value(1).toDateTime());
    }

    return NULL;
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
                  "channel.callsign,channel.name,channel.commmethod, "
                  "channel.outputfilters,seriesid,programid,filesize, "
                  "lastmodified,stars,previouslyshown,originalairdate, "
                  "hostname,recordid,transcoder,playgroup, "
                  "recorded.recpriority,progstart,progend,basename,recgroup, "
                  "storagegroup "
                  "FROM recorded "
                  "LEFT JOIN channel "
                  "ON recorded.chanid = channel.chanid "
                  "WHERE recorded.chanid = :CHANNEL "
                  "AND starttime = :STARTTIME ;");
    query.bindValue(":CHANNEL", channel);
    query.bindValue(":STARTTIME", starttime);

    if (query.exec() && query.next())
    {
        ProgramInfo *proginfo = new ProgramInfo;
        proginfo->chanid = query.value(0).toString();
        proginfo->startts = query.value(23).toDateTime();
        proginfo->endts = query.value(24).toDateTime();
        proginfo->recstartts = query.value(1).toDateTime();
        proginfo->recendts = query.value(2).toDateTime();
        proginfo->title = query.value(3).toString();
        proginfo->subtitle = query.value(4).toString();
        proginfo->description = query.value(5).toString();

        proginfo->chanstr = query.value(6).toString();
        proginfo->chansign = query.value(7).toString();
        proginfo->channame = query.value(8).toString();
        proginfo->chancommfree = COMM_DETECT_COMMFREE == query.value(9).toInt();
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
            proginfo->originalAirDate = QDate (0, 1, 1);
            proginfo->hasAirDate = false;
        }
        else
        {
            proginfo->originalAirDate =
                QDate::fromString(query.value(17).toString(),Qt::ISODate);

            if (proginfo->originalAirDate > QDate(1940, 1, 1))
                proginfo->hasAirDate = true;
            else
                proginfo->hasAirDate = false;
        }
        proginfo->hostname = query.value(18).toString();
        proginfo->recstatus = rsRecorded;
        proginfo->recordid = query.value(19).toInt();
        proginfo->transcoder = query.value(20).toInt();

        proginfo->spread = -1;

        proginfo->programflags = proginfo->getProgramFlags();

        proginfo->getProgramProperties();

        proginfo->recgroup = query.value(26).toString();
        proginfo->storagegroup = query.value(27).toString();
        proginfo->playgroup = query.value(21).toString();
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

        ProgramInfo *nextday = GetProgramAtDateTime(
            chanid.toUInt(), checktime);

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
    ProgramInfo *nextweek = GetProgramAtDateTime(chanid.toUInt(), checktime);

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

    if (query.exec() && query.next())
    {
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

/** \fn ProgramInfo::ApplyRecordRecID(void)
 *  \brief Sets recordid to match ScheduledRecording recordid
 */
void ProgramInfo::ApplyRecordRecID(void)
{
    MSqlQuery query(MSqlQuery::InitCon());

    if (getRecordID() < 0)
    {
        VERBOSE(VB_IMPORTANT,
                "ProgInfo Error: ApplyRecordRecID(void) needs recordid");
        return;
    }

    query.prepare("UPDATE recorded "
                  "SET recordid = :RECID "
                  "WHERE chanid = :CHANID AND starttime = :START");

    if (rectype == kOverrideRecord && parentid > 0)
        query.bindValue(":RECID", parentid);
    else
        query.bindValue(":RECID",  getRecordID());
    query.bindValue(":CHANID", chanid);
    query.bindValue(":START",  recstartts);

    if (!query.exec())
        MythDB::DBError(LOC + "RecordID update", query);
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
    record->Save();
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
    record->Save();
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
    query.bindValue(":RECGROUP", newrecgroup);
    query.bindValue(":START", recstartts);
    query.bindValue(":CHANID", chanid);

    if (!query.exec())
        MythDB::DBError("RecGroup update", query);

    recgroup = newrecgroup;
}

/** \fn ProgramInfo::ApplyRecordPlayGroupChange(const QString &newplaygroup)
 *  \brief Sets the recording group, both in this ProgramInfo
 *         and in the database.
 *  \param newplaygroup New recording group.
 */
void ProgramInfo::ApplyRecordPlayGroupChange(const QString &newplaygroup)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("UPDATE recorded"
                  " SET playgroup = :PLAYGROUP"
                  " WHERE chanid = :CHANID"
                  " AND starttime = :START ;");
    query.bindValue(":PLAYGROUP", newplaygroup);
    query.bindValue(":START", recstartts);
    query.bindValue(":CHANID", chanid);

    if (!query.exec())
        MythDB::DBError("PlayGroup update", query);

    playgroup = newplaygroup;
}

/** \fn ProgramInfo::ApplyStorageGroupChange(const QString &newstoragegroup)
 *  \brief Sets the storage group, both in this ProgramInfo
 *         and in the database.
 *  \param newstoragegroup New storage group.
 */
void ProgramInfo::ApplyStorageGroupChange(const QString &newstoragegroup)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("UPDATE recorded"
                  " SET storagegroup = :STORAGEGROUP"
                  " WHERE chanid = :CHANID"
                  " AND starttime = :START ;");
    query.bindValue(":STORAGEGROUP", newstoragegroup);
    query.bindValue(":START", recstartts);
    query.bindValue(":CHANID", chanid);

    if (!query.exec())
        MythDB::DBError("StorageGroup update", query);

    storagegroup = newstoragegroup;
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
    query.bindValue(":TITLE", newTitle);
    query.bindValue(":SUBTITLE", newSubtitle);
    query.bindValue(":CHANID", chanid);
    query.bindValue(":START", recstartts.toString("yyyyMMddhhmmss"));

    if (!query.exec())
        MythDB::DBError("RecTitle update", query);

    title = newTitle;
    subtitle = newSubtitle;
}

/* \fn ProgramInfo::ApplyTranscoderProfileChange(QString profile)
 * \brief Sets the transcoder profile for a recording
 * \param profile Descriptive name of the profile. ie: Autodetect
 */
void ProgramInfo::ApplyTranscoderProfileChange(QString profile)
{
    if(profile == "Default") // use whatever is already in the transcoder
        return;

    MSqlQuery query(MSqlQuery::InitCon());

    if(profile == "Autodetect")
    {
        query.prepare("UPDATE recorded "
                      "SET transcoder = 0 "
                      "WHERE chanid = :CHANID "
                      "AND starttime = :START");
        query.bindValue(":CHANID",  chanid);
        query.bindValue(":START",  recstartts);

        if (!query.exec())
            MythDB::DBError(LOC + "unable to update transcoder "
                                  "in recorded table", query);
    }
    else
    {
        MSqlQuery pidquery(MSqlQuery::InitCon());
        pidquery.prepare("SELECT r.id "
                         "FROM recordingprofiles r, profilegroups p "
                         "WHERE r.profilegroup = p.id "
                             "AND p.name = 'Transcoders' "
                             "AND r.name = :PROFILE ");
        pidquery.bindValue(":PROFILE",  profile);

        if (pidquery.exec() && pidquery.isActive() && pidquery.next())
        {
            query.prepare("UPDATE recorded "
                          "SET transcoder = :TRANSCODER "
                          "WHERE chanid = :CHANID "
                              "AND starttime = :START");
            query.bindValue(":TRANSCODER", pidquery.value(0).toInt());
            query.bindValue(":CHANID",  chanid);
            query.bindValue(":START",  recstartts);

            if (!query.exec())
                MythDB::DBError(LOC + "unable to update transcoder "
                                     "in recorded table", query);
        }
        else
            MythDB::DBError("PlaybackBox: unable to query transcoder "
                                 "profile ID", query);
    }
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
            ApplyRecordStateChange(kAllRecord);
            break;
        case kAllRecord:
            ApplyRecordStateChange(kSingleRecord);
            break;

        case kOverrideRecord:
            ApplyRecordStateChange(kDontRecord);
            break;
        case kDontRecord:
            ApplyRecordStateChange(kOverrideRecord);
            break;

        default:
            ApplyRecordStateChange(kAllRecord);
            break;
/*
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
*/
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

    if (title.toLower() != other.title.toLower())
        return false;

    if (findid && findid == other.findid)
        return true;

    if (dupmethod & kDupCheckNone)
        return false;

    if (catType == "series")
    {
        QMutexLocker locker(&regExpLock);
        if (programid.contains(regExpSeries))
            return false;
    }

    if (!programid.isEmpty() && !other.programid.isEmpty())
        return programid == other.programid;

    if ((dupmethod & kDupCheckSub) &&
        ((subtitle.isEmpty()) ||
         (subtitle.toLower() != other.subtitle.toLower())))
        return false;

    if ((dupmethod & kDupCheckDesc) &&
        ((description.isEmpty()) ||
         (description.toLower() != other.description.toLower())))
        return false;

    if ((dupmethod & kDupCheckSubThenDesc) &&
        ((subtitle.isEmpty() && other.subtitle.isEmpty() &&
          description.toLower() != other.description.toLower()) ||
         (subtitle.toLower() != other.subtitle.toLower()) ||
         (description.isEmpty() && subtitle.isEmpty())))
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

/** \fn ProgramInfo::SetRecordBasename(QString)
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
        MythDB::DBError("SetRecordBasename", query);
        return false;
    }

    return true;
}

/**
 *  \brief Returns a filename for a recording based on the
 *         recording channel and date.
 */
QString ProgramInfo::GetRecordBasename(bool fromDB) const
{
    QString retval = "";

    if (!fromDB && !pathname.isEmpty())
        retval = pathname.section('/', -1);
    else
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT basename FROM recorded "
                      "WHERE chanid = :CHANID AND "
                      "      starttime = :STARTTIME;");
        query.bindValue(":CHANID", chanid);
        query.bindValue(":STARTTIME", recstartts);

        if (!query.exec())
        {
            MythDB::DBError("GetRecordBasename", query);
        }
        else if (query.next())
        {
            retval = query.value(0).toString();
        }
        else
            VERBOSE(VB_IMPORTANT, QString("GetRecordBasename found no entry "
                    "for %1 @ %2")
                    .arg(chanid).arg(recstartts.toString(Qt::ISODate)));
    }

    return retval;
}

/**
 *  \brief Returns filename or URL to be used to play back this recording.
 *         If the file is accessible locally, the filename will be returned,
 *         otherwise a myth:// URL will be returned.
 */
QString ProgramInfo::GetPlaybackURL(bool checkMaster, bool forceCheckLocal)
{
    QString tmpURL;
    QString basename = GetRecordBasename(true);

    if (basename.isEmpty())
        return "";

    bool alwaysStream = gContext->GetNumSetting("AlwaysStreamFiles", 0);

    if ((!alwaysStream) ||
        (forceCheckLocal) ||
        (hostname == gContext->GetHostName()))
    {
        // Check to see if the file exists locally
        StorageGroup sgroup(storagegroup);
        //VERBOSE(VB_FILE, LOC +QString("GetPlaybackURL: CHECKING SG : %1 : ").arg(tmpURL));
        tmpURL = sgroup.FindRecordingFile(basename);

        if (tmpURL != "")
        {
            VERBOSE(VB_FILE, LOC +
                    QString("GetPlaybackURL: File is local: '%1'").arg(tmpURL));
            return tmpURL;
        }
        else if (hostname == gContext->GetHostName())
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("GetPlaybackURL: '%1' should be local, but it can "
                            "not be found.").arg(basename));
            return QString("/GetPlaybackURL/UNABLE/TO/FIND/LOCAL/FILE/ON/%1/%2")
                           .arg(hostname).arg(basename);
        }
    }

    // Check to see if we should stream from the master backend
    if ((checkMaster) &&
        (gContext->GetNumSetting("MasterBackendOverride", 0)) &&
        (RemoteCheckFile(this, false)))
    {
        tmpURL = QString("myth://") +
                 gContext->GetSetting("MasterServerIP") + ":" +
                 gContext->GetSetting("MasterServerPort") + "/" + basename;
        VERBOSE(VB_FILE, LOC +
                QString("GetPlaybackURL: Found @ '%1'").arg(tmpURL));
        return tmpURL;
    }

    // Fallback to streaming from the backend the recording was created on
    tmpURL = QString("myth://") +
             gContext->GetSettingOnHost("BackendServerIP", hostname) + ":" +
             gContext->GetSettingOnHost("BackendServerPort", hostname) + "/" +
             basename;

    VERBOSE(VB_FILE, LOC + QString("GetPlaybackURL: Using default of: '%1'")
                                   .arg(tmpURL));

    return tmpURL;
}

/**
 *  \brief Inserts this ProgramInfo into the database as an existing recording.
 *
 *  This method, of course, only works if a recording has been scheduled
 *  and started.
 *
 *  \param ext    File extension for recording
 */
void ProgramInfo::StartedRecording(QString ext)
{
    QString dirname = pathname;

    if (!record)
    {
        record = new ScheduledRecording();
        record->loadByProgram(this);
    }

    hostname = gContext->GetHostName();
    pathname = CreateRecordBasename(ext);

    int count = 0;
    while (!insert_program(this, record) && count < 50)
    {
        recstartts = recstartts.addSecs(1);
        pathname = CreateRecordBasename(ext);
        count++;
    }

    if (count >= 50)
    {
        VERBOSE(VB_IMPORTANT, "Couldn't insert program");
        return;
    }

    pathname = dirname + "/" + pathname;

    VERBOSE(VB_FILE, QString(LOC + "StartedRecording: Recording to '%1'")
                             .arg(pathname));


    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("DELETE FROM recordedseek WHERE chanid = :CHANID"
                  " AND starttime = :START;");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":START", recstartts);

    if (!query.exec() || !query.isActive())
        MythDB::DBError("Clear seek info on record", query);

    query.prepare("DELETE FROM recordedmarkup WHERE chanid = :CHANID"
                  " AND starttime = :START;");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":START", recstartts);

    if (!query.exec() || !query.isActive())
        MythDB::DBError("Clear markup on record", query);

    query.prepare("REPLACE INTO recordedcredits"
                 " SELECT * FROM credits"
                 " WHERE chanid = :CHANID AND starttime = :START;");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":START", startts);
    if (!query.exec() || !query.isActive())
        MythDB::DBError("Copy program credits on record", query);

    query.prepare("REPLACE INTO recordedprogram"
                 " SELECT * from program"
                 " WHERE chanid = :CHANID AND starttime = :START"
                 " AND title = :TITLE;");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":START", startts);
    query.bindValue(":TITLE", title);
    if (!query.exec() || !query.isActive())
        MythDB::DBError("Copy program data on record", query);

    query.prepare("REPLACE INTO recordedrating"
                 " SELECT * from programrating"
                 " WHERE chanid = :CHANID AND starttime = :START;");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":START", startts);
    if (!query.exec() || !query.isActive())
        MythDB::DBError("Copy program ratings on record", query);
}

static bool insert_program(const ProgramInfo        *pg,
                           const ScheduledRecording *schd)
{
    MSqlQuery query(MSqlQuery::InitCon());

    //query.prepare("LOCK TABLES recorded WRITE");
    if (!query.exec("LOCK TABLES recorded WRITE"))
    {
        MythDB::DBError("insert_program -- lock", query);
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
            MythDB::DBError("insert_program -- select", query);
        else
            VERBOSE(VB_IMPORTANT, "recording already exists...");

        //query.prepare("UNLOCK TABLES");
        query.exec("UNLOCK TABLES");
        return false;
    }

    query.prepare(
        "INSERT INTO recorded "
        "   (chanid,    starttime,   endtime,         title,            "
        "    subtitle,  description, hostname,        category,         "
        "    recgroup,  autoexpire,  recordid,        seriesid,         "
        "    programid, stars,       previouslyshown, originalairdate,  "
        "    findid,    transcoder,  playgroup,       recpriority,      "
        "    basename,  progstart,   progend,         profile,          "
        "    duplicate, storagegroup) "
        "VALUES"
        "  (:CHANID,   :STARTS,     :ENDS,           :TITLE,            "
        "   :SUBTITLE, :DESC,       :HOSTNAME,       :CATEGORY,         "
        "   :RECGROUP, :AUTOEXP,    :RECORDID,       :SERIESID,         "
        "   :PROGRAMID,:STARS,      :REPEAT,         :ORIGAIRDATE,      "
        "   :FINDID,   :TRANSCODER, :PLAYGROUP,      :RECPRIORITY,      "
        "   :BASENAME, :PROGSTART,  :PROGEND,        :PROFILE,          "
        "   0,         :STORGROUP) "
        );

    if (pg->rectype == kOverrideRecord)
        query.bindValue(":RECORDID",    pg->parentid);
    else
        query.bindValue(":RECORDID",    pg->recordid);

    if (pg->hasAirDate)
        query.bindValue(":ORIGAIRDATE", pg->originalAirDate);
    else
        query.bindValue(":ORIGAIRDATE", "0000-00-00");

    query.bindValue(":CHANID",      pg->chanid);
    query.bindValue(":STARTS",      pg->recstartts);
    query.bindValue(":ENDS",        pg->recendts);
    query.bindValue(":TITLE",       pg->title);
    query.bindValue(":SUBTITLE",    pg->subtitle);
    query.bindValue(":DESC",        pg->description);
    query.bindValue(":HOSTNAME",    pg->hostname);
    query.bindValue(":CATEGORY",    pg->category);
    query.bindValue(":RECGROUP",    pg->recgroup);
    query.bindValue(":AUTOEXP",     schd->GetAutoExpire());
    query.bindValue(":SERIESID",    pg->seriesid);
    query.bindValue(":PROGRAMID",   pg->programid);
    query.bindValue(":FINDID",      pg->findid);
    query.bindValue(":STARS",       pg->stars);
    query.bindValue(":REPEAT",      pg->repeat);
    query.bindValue(":TRANSCODER",  schd->GetTranscoder());
    query.bindValue(":PLAYGROUP",   pg->playgroup);
    query.bindValue(":RECPRIORITY", schd->getRecPriority());
    query.bindValue(":BASENAME",    pg->pathname);
    query.bindValue(":STORGROUP",   pg->storagegroup);
    query.bindValue(":PROGSTART",   pg->startts);
    query.bindValue(":PROGEND",     pg->endts);
    query.bindValue(":PROFILE",     schd->getProfileName());

    bool ok = query.exec() && (query.numRowsAffected() > 0);
    bool active = query.isActive();

    //query.prepare("UNLOCK TABLES");
    query.exec("UNLOCK TABLES");

    if (!ok && !active)
        MythDB::DBError("insert_program -- insert", query);

    else if (pg->recordid > 0)
    {
        query.prepare("UPDATE channel SET last_record = NOW() "
                      "WHERE chanid = :CHANID");
        query.bindValue(":CHANID", pg->chanid);
        query.exec();

        query.prepare("UPDATE record SET last_record = NOW() "
                      "WHERE recordid = :RECORDID");
        query.bindValue(":RECORDID", pg->recordid);
        query.exec();

        if (pg->rectype == kOverrideRecord && pg->parentid > 0)
        {
            query.prepare("UPDATE record SET last_record = NOW() "
                          "WHERE recordid = :PARENTID");
            query.bindValue(":PARENTID", pg->parentid);
            query.exec();
        }
    }

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
    query.prepare("UPDATE recorded SET endtime = :ENDTIME, "
                  "       duplicate = 1 "
                  "WHERE chanid = :CHANID AND "
                  "    starttime = :STARTTIME ");
    query.bindValue(":ENDTIME", recendts);
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);

    query.exec();

    if (!query.isActive())
        MythDB::DBError("FinishedRecording update", query);

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
    query.prepare("UPDATE recorded SET endtime = :ENDTIME "
                  "WHERE chanid = :CHANID AND "
                  "    starttime = :STARTTIME ");
    query.bindValue(":ENDTIME", recendts);

    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);

    query.exec();

    if (!query.isActive())
        MythDB::DBError("FinishedRecording update", query);
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
        MythDB::DBError("File size update", query);
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

    if (query.exec() && query.next())
    {
        filesize = stringToLongLong(query.value(0).toString());
    }
    else
        filesize = 0;

    return filesize;
}

/** \fn ProgramInfo::GetMplexID(void) const
 *  \brief Gets multiplex any recording would be made on, zero if unknown.
 */
int ProgramInfo::GetMplexID(void) const
{
    int ret = 0;
    if (!chanid.isEmpty())
    {
        MSqlQuery query(MSqlQuery::InitCon());

        query.prepare("SELECT mplexid FROM channel "
                      "WHERE chanid = :CHANID");
        query.bindValue(":CHANID", chanid);

        if (!query.exec())
            MythDB::DBError("GetMplexID", query);
        else if (query.next())
            ret = query.value(0).toUInt();

        // clear out bogus mplexid's
        ret = (32767 == ret) ? 0 : ret;
    }

    return ret;
}

/** \fn ProgramInfo::SetBookmark(long long pos) const
 *  \brief Sets a bookmark position in database.
 *
 */
void ProgramInfo::SetBookmark(long long pos) const
{
    ClearMarkupMap(MARK_BOOKMARK);
    frm_dir_map_t bookmarkmap;
    bookmarkmap[pos] = MARK_BOOKMARK;
    SetMarkupMap(bookmarkmap);

    if (!isVideo)
    {
        MSqlQuery query(MSqlQuery::InitCon());

        // For the time being, note whether a bookmark
        // exists in the recorded table
        query.prepare("UPDATE recorded"
                      " SET bookmark = :BOOKMARKFLAG"
                      " WHERE chanid = :CHANID"
                      " AND starttime = :STARTTIME ;");

        query.bindValue(":BOOKMARKFLAG", pos == 0 ? 0 : 1);
        query.bindValue(":CHANID", chanid);
        query.bindValue(":STARTTIME", recstartts);

        if (!query.exec() || !query.isActive())
            MythDB::DBError("bookmark flag update", query);
    }
}

/** \fn ProgramInfo::GetBookmark(void) const
 *  \brief Gets any bookmark position in database,
 *         unless "ignoreBookmark" is set.
 *
 *  \return Bookmark position in bytes if the query is executed and succeeds,
 *          zero otherwise.
 */
long long ProgramInfo::GetBookmark(void) const
{
    QMap<long long, int>::Iterator i;
    long long pos = 0;

    if (ignoreBookmark)
        return pos;

    frm_dir_map_t bookmarkmap;
    GetMarkupMap(bookmarkmap, MARK_BOOKMARK);

    if (bookmarkmap.isEmpty())
        return pos;

    i = bookmarkmap.begin();
    pos = i.key();

    return pos;
}

/** \brief Queries "dvdbookmark" table for bookmarking DVD
 * serial number. Deletes old dvd bookmarks if "delete" is true;
 *
 * \return list containing title, audio track, subtitle, framenum
 */
QStringList ProgramInfo::GetDVDBookmark(QString serialid, bool delbookmark) const
{
    QStringList fields = QStringList();
    MSqlQuery query(MSqlQuery::InitCon());

    if (!ignoreBookmark)
    {
        query.prepare(" SELECT title, framenum, audionum, subtitlenum "
                        " FROM dvdbookmark "
                        " WHERE serialid = ? ");
        query.addBindValue(serialid);

        if (query.exec() && query.next())
        {
            for(int i = 0; i < 4; i++)
                fields.append(query.value(i).toString());
        }
    }

    if (delbookmark)
    {
        int days = -(gContext->GetNumSetting("DVDBookmarkDays", 10));
        QDateTime removedate = mythCurrentDateTime().addDays(days);
        query.prepare(" DELETE from dvdbookmark "
                        " WHERE timestamp < ? ");
        query.addBindValue(removedate.toString(Qt::ISODate));

        if (!query.exec() || !query.isActive())
            MythDB::DBError("GetDVDBookmark deleting old entries", query);
    }

    return fields;
}

void ProgramInfo::SetDVDBookmark(QStringList fields) const
{
    QStringList::Iterator it = fields.begin();
    MSqlQuery query(MSqlQuery::InitCon());

    QString serialid    = *(it);
    QString name        = *(++it);
    QString title       = *(++it);
    QString audionum    = *(++it);
    QString subtitlenum = *(++it);
    QString frame       = *(++it);

    query.prepare("INSERT IGNORE INTO dvdbookmark "
                    " (serialid, name)"
                    " VALUES ( :SERIALID, :NAME );");
    query.bindValue(":SERIALID", serialid);
    query.bindValue(":NAME", name);

    if (!query.exec() || !query.isActive())
        MythDB::DBError("SetDVDBookmark inserting", query);

    query.prepare(" UPDATE dvdbookmark "
                    " SET title       = ? , "
                    "     audionum    = ? , "
                    "     subtitlenum = ? , "
                    "     framenum    = ? , "
                    "     timestamp   = NOW() "
                    " WHERE serialid = ? ;");
    query.addBindValue(title);
    query.addBindValue(audionum);
    query.addBindValue(subtitlenum);
    query.addBindValue(frame);
    query.addBindValue(serialid);

    if (!query.exec() || !query.isActive())
        MythDB::DBError("SetDVDBookmark updating", query);
}
/** \fn ProgramInfo::SetWatchedFlag(bool) const
 *  \brief Set "watched" field in "recorded" table to "watchedFlag".
 *  \param watchedFlag value to set watched field to.
 */
void ProgramInfo::SetWatchedFlag(bool watchedFlag) const
{

    if (!isVideo)
    {
        MSqlQuery query(MSqlQuery::InitCon());

        query.prepare("UPDATE recorded"
                    " SET watched = :WATCHEDFLAG"
                    " WHERE chanid = :CHANID"
                    " AND starttime = :STARTTIME ;");
        query.bindValue(":CHANID", chanid);
        query.bindValue(":STARTTIME", recstartts);

        if (watchedFlag)
            query.bindValue(":WATCHEDFLAG", 1);
        else
            query.bindValue(":WATCHEDFLAG", 0);

        if (!query.exec() || !query.isActive())
            MythDB::DBError("Set watched flag", query);
        else
            UpdateLastDelete(watchedFlag);
    }
}

/** \fn ProgramInfo::IsEditing(void) const
 *  \brief Queries "recorded" table for its "editing" field
 *         and returns true if it is set to true.
 *  \return true if we have started, but not finished, editing.
 */
bool ProgramInfo::IsEditing(void) const
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT editing FROM recorded"
                  " WHERE chanid = :CHANID"
                  " AND starttime = :STARTTIME ;");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);

    if (query.exec() && query.next())
    {
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
        MythDB::DBError("Edit status update", query);
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
        MythDB::DBError("Set delete flag", query);
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

    if (query.exec() && query.next())
    {
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
    if (isVideo)
        return false;

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
    if (query.exec() && query.size() > 0)
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
            else if (recusage == "pipplayer")
                usageStr = QObject::tr("PIP");
            else if (recusage == "pbpplayer")
                usageStr = QObject::tr("PBP");

            byWho += query.value(0).toString() + " (" + usageStr + ")\n";
        }

        return true;
    }

    return false;
}

/** \fn ProgramInfo::GetTranscodedStatus(void) const
 *  \brief Returns the "transcoded" field in "recorded" table.
 */
int ProgramInfo::GetTranscodedStatus(void) const
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT transcoded FROM recorded"
                 " WHERE chanid = :CHANID"
                 " AND starttime = :STARTTIME ;");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);

    if (query.exec() && query.next())
    {
        return query.value(0).toInt();
    }

    return false;
}

/** \fn ProgramInfo::SetTranscoded(int transFlag) const
 *  \brief Set "transcoded" field in "recorded" table to "transFlag".
 *  \param transFlag value to set transcoded field to.
 */
void ProgramInfo::SetTranscoded(int transFlag) const
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("UPDATE recorded"
                 " SET transcoded = :FLAG"
                 " WHERE chanid = :CHANID"
                 " AND starttime = :STARTTIME ;");
    query.bindValue(":FLAG", transFlag);
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);

    if(!query.exec() || !query.isActive())
        MythDB::DBError("Transcoded status update", query);
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
        MythDB::DBError("Commercial Flagged status update", query);
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
        MythDB::DBError("PreserveEpisode update", query);
    else
        UpdateLastDelete(false);
}

/**
 *  \brief Set "autoexpire" field in "recorded" table to "autoExpire".
 *  \param autoExpire value to set auto expire field to.
 */
void ProgramInfo::SetAutoExpire(int autoExpire, bool updateDelete) const
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
        MythDB::DBError("AutoExpire update", query);
    else if (updateDelete)
        UpdateLastDelete(true);
}

/** \fn ProgramInfo::UpdateLastDelete(bool) const
 *  \brief Set or unset the record.last_delete field.
 *  \param setTime to set or clear the time stamp.
 */
void ProgramInfo::UpdateLastDelete(bool setTime) const
{
    MSqlQuery query(MSqlQuery::InitCon());

    if (setTime)
    {
        QDateTime timeNow = QDateTime::currentDateTime();
        int delay = recstartts.secsTo(timeNow) / 3600;

        if (delay > 200)
            delay = 200;
        else if (delay < 1)
            delay = 1;

        query.prepare("UPDATE record SET last_delete = :TIME, "
                      "avg_delay = (avg_delay * 3 + :DELAY) / 4 "
                      "WHERE recordid = :RECORDID");
        query.bindValue(":TIME", timeNow);
        query.bindValue(":DELAY", delay);
        query.bindValue(":RECORDID", recordid);
    }
    else
        query.prepare("UPDATE record SET last_delete = '0000-00-00T00:00:00' "
                      "WHERE recordid = :RECORDID");
        query.bindValue(":RECORDID", recordid);

    if (!query.exec() || !query.isActive())
        MythDB::DBError("Update last_delete", query);
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

    if (query.exec() && query.next())
    {
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

    if (query.exec() && query.next())
    {
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

    if (query.exec() && query.next())
    {
        return query.value(0).toInt();
    }

    return false;
}

void ProgramInfo::GetCutList(frm_dir_map_t &delMap) const
{
    GetMarkupMap(delMap, MARK_CUT_START);
    GetMarkupMap(delMap, MARK_CUT_END, true);
}

void ProgramInfo::SetCutList(frm_dir_map_t &delMap) const
{
    ClearMarkupMap(MARK_CUT_START);
    ClearMarkupMap(MARK_CUT_END);
    SetMarkupMap(delMap);

    if (!isVideo)
    {
        MSqlQuery query(MSqlQuery::InitCon());

        // Flag the existence of a cutlist
        query.prepare("UPDATE recorded"
                      " SET cutlist = :CUTLIST"
                      " WHERE chanid = :CHANID"
                      " AND starttime = :STARTTIME ;");

        query.bindValue(":CUTLIST", delMap.isEmpty() ? 0 : 1);
        query.bindValue(":CHANID", chanid);
        query.bindValue(":STARTTIME", recstartts);

        if (!query.exec() || !query.isActive())
            MythDB::DBError("cutlist flag update", query);
    }
}

void ProgramInfo::SetCommBreakList(frm_dir_map_t &frames) const
{
    ClearMarkupMap(MARK_COMM_START);
    ClearMarkupMap(MARK_COMM_END);
    SetMarkupMap(frames);
}

void ProgramInfo::GetCommBreakList(frm_dir_map_t &frames) const
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
        MythDB::DBError("ClearMarkupMap deleting", query);
}

void ProgramInfo::SetMarkupMap(frm_dir_map_t &marks,
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

        if (!query.exec())
            MythDB::DBError("SetMarkupMap checking record table", query);

        if (!query.next())
            return;
    }

    for (i = marks.begin(); i != marks.end(); ++i)
    {
        long long frame = i.key();
        int mark_type;
        QString querystr;

        if ((min_frame >= 0) && (frame < min_frame))
            continue;

        if ((max_frame >= 0) && (frame > max_frame))
            continue;

        if (type != -100)
            mark_type = type;
        else
            mark_type = *i;

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
        query.bindValue(":MARK", frame);
        query.bindValue(":TYPE", mark_type);

        if (!query.exec() || !query.isActive())
            MythDB::DBError("SetMarkupMap inserting", query);
    }
}

void ProgramInfo::GetMarkupMap(frm_dir_map_t &marks,
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

    if (query.exec() && query.size() > 0)
    {
        while(query.next())
            marks[query.value(0).toLongLong()] = query.value(1).toInt();
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

QString toString(MarkTypes type)
{
    switch (type)
    {
        case MARK_UNSET:        return "UNSET";
        case MARK_UPDATED_CUT:  return "UPDATED_CUT";
        case MARK_EDIT_MODE:    return "EDIT_MODE";
        case MARK_CUT_END:      return "CUT_END";
        case MARK_CUT_START:    return "CUT_START";
        case MARK_BOOKMARK:     return "BOOKMARK";
        case MARK_BLANK_FRAME:  return "BLANK_FRAME";
        case MARK_COMM_START:   return "COMM_START";
        case MARK_COMM_END:     return "COMM_END";
        case MARK_GOP_START:    return "GOP_START";
        case MARK_KEYFRAME:     return "KEYFRAME";
        case MARK_SCENE_CHANGE: return "SCENE_CHANGE";
        case MARK_GOP_BYFRAME:  return "GOP_BYFRAME";
        default:                return "unknown";
    }
}

void ProgramInfo::GetPositionMap(frm_pos_map_t &posMap,
                                 int type) const
{
    if (positionMapDBReplacement)
    {
        QMutexLocker locker(positionMapDBReplacement->lock);
        posMap = positionMapDBReplacement->map[(MarkTypes)type];

        return;
    }

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
        query.prepare("SELECT mark, offset FROM recordedseek"
                      " WHERE chanid = :CHANID"
                      " AND starttime = :STARTTIME"
                      " AND type = :TYPE ;");
        query.bindValue(":CHANID", chanid);
        query.bindValue(":STARTTIME", recstartts);
    }
    query.bindValue(":TYPE", type);

    if (query.exec() && query.size() > 0)
    {
        while (query.next())
            posMap[query.value(0).toLongLong()] = query.value(1).toLongLong();
    }
}

void ProgramInfo::ClearPositionMap(int type) const
{
    if (positionMapDBReplacement)
    {
        QMutexLocker locker(positionMapDBReplacement->lock);
        positionMapDBReplacement->map.clear();
        return;
    }

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
        query.prepare("DELETE FROM recordedseek"
                      " WHERE chanid = :CHANID"
                      " AND starttime = :STARTTIME"
                      " AND type = :TYPE ;");
        query.bindValue(":CHANID", chanid);
        query.bindValue(":STARTTIME", recstartts);
    }
    query.bindValue(":TYPE", type);

    if (!query.exec() || !query.isActive())
        MythDB::DBError("clear position map", query);
}

void ProgramInfo::SetPositionMap(frm_pos_map_t &posMap, int type,
                                 long long min_frame, long long max_frame) const
{
    if (positionMapDBReplacement)
    {
        QMutexLocker locker(positionMapDBReplacement->lock);

        if ((min_frame >= 0) || (max_frame >= 0))
        {
            frm_pos_map_t::const_iterator it, it_end;
            it     = positionMapDBReplacement->map[(MarkTypes)type].begin();
            it_end = positionMapDBReplacement->map[(MarkTypes)type].end();

            frm_pos_map_t new_map;
            for (; it != it_end; ++it)
            {
                long long frame = it.key();
                if ((min_frame >= 0) && (frame >= min_frame))
                    continue;
                if ((min_frame >= 0) && (frame <= max_frame))
                    continue;
                new_map.insert(it.key(), *it);
            }
            positionMapDBReplacement->map[(MarkTypes)type] = new_map;
        }
        else
        {
            positionMapDBReplacement->map[(MarkTypes)type].clear();
        }

        frm_pos_map_t::const_iterator it     = posMap.begin();
        frm_pos_map_t::const_iterator it_end = posMap.end();
        for (; it != it_end; ++it)
        {
            long long frame = it.key();
            if ((min_frame >= 0) && (frame >= min_frame))
                continue;
            if ((min_frame >= 0) && (frame <= max_frame))
                continue;

            positionMapDBReplacement->map[(MarkTypes)type]
                .insert(frame, *it);
        }

        return;
    }

    QMap<long long, long long>::Iterator i;
    MSqlQuery query(MSqlQuery::InitCon());
    QString comp = "";

    if (min_frame >= 0)
        comp += " AND mark >= :MIN_FRAME ";
    if (max_frame >= 0)
        comp += " AND mark <= :MAX_FRAME ";

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
        query.prepare("DELETE FROM recordedseek"
                      " WHERE chanid = :CHANID"
                      " AND starttime = :STARTTIME"
                      " AND type = :TYPE"
                      + comp + ";");
        query.bindValue(":CHANID", chanid);
        query.bindValue(":STARTTIME", recstartts);
    }
    query.bindValue(":TYPE", type);
    if (min_frame >= 0)
        query.bindValue(":MIN_FRAME", min_frame);
    if (max_frame >= 0)
        query.bindValue(":MAX_FRAME", max_frame);

    if (!query.exec() || !query.isActive())
        MythDB::DBError("position map clear", query);

    for (i = posMap.begin(); i != posMap.end(); ++i)
    {
        long long frame = i.key();

        if ((min_frame >= 0) && (frame < min_frame))
            continue;

        if ((max_frame >= 0) && (frame > max_frame))
            continue;

        long long offset = *i;

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
            query.prepare("INSERT INTO recordedseek"
                          " (chanid, starttime, mark, type, offset)"
                          " VALUES"
                          " ( :CHANID , :STARTTIME , :MARK , :TYPE , :OFFSET );");
            query.bindValue(":CHANID", chanid);
            query.bindValue(":STARTTIME", recstartts);
        }
        query.bindValue(":MARK", frame);
        query.bindValue(":TYPE", type);
        query.bindValue(":OFFSET", offset);

        if (!query.exec() || !query.isActive())
            MythDB::DBError("position map insert", query);
    }
}

void ProgramInfo::SetPositionMapDelta(frm_pos_map_t &posMap,
                                      int type) const
{
    if (positionMapDBReplacement)
    {
        QMutexLocker locker(positionMapDBReplacement->lock);

        frm_pos_map_t::const_iterator it     = posMap.begin();
        frm_pos_map_t::const_iterator it_end = posMap.end();
        for (; it != it_end; ++it)
        {
            positionMapDBReplacement->map[(MarkTypes)type]
                .insert(it.key(), *it);
        }

        return;
    }

    QMap<long long, long long>::Iterator i;
    MSqlQuery query(MSqlQuery::InitCon());

    for (i = posMap.begin(); i != posMap.end(); ++i)
    {
        long long frame = i.key();
        long long offset = *i;

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
            query.prepare("INSERT INTO recordedseek"
                          " (chanid, starttime, mark, type, offset)"
                          " VALUES"
                          " ( :CHANID , :STARTTIME , :MARK , :TYPE , :OFFSET );");
            query.bindValue(":CHANID", chanid);
            query.bindValue(":STARTTIME", recstartts);
        }
        query.bindValue(":MARK", frame);
        query.bindValue(":TYPE", type);
        query.bindValue(":OFFSET", offset);

        if (!query.exec() || !query.isActive())
            MythDB::DBError("delta position map insert", query);
    }
}

/**
 *  \brief Store a change in aspect ratio in the recordedmark table
 */
void ProgramInfo::SetAspectChange(MarkTypes type, long long frame,
                                  uint customAspect)
{
    if (isVideo)
        return;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("INSERT INTO recordedmarkup"
                    " (chanid, starttime, mark, type, data)"
                    " VALUES"
                    " ( :CHANID, :STARTTIME, :MARK, :TYPE, :DATA);");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);

    query.bindValue(":MARK", frame);
    query.bindValue(":TYPE", type);

    if (type == MARK_ASPECT_CUSTOM)
        query.bindValue(":DATA", customAspect);
    else
        query.bindValue(":DATA", QVariant::UInt);


    if (!query.exec() || !query.isActive())
        MythDB::DBError("aspect ratio change insert", query);
}

/** \fn ProgramInfo::SetResolution(uint width, uint height, long long frame)
 *  \brief Store the Resolution at frame in the recordedmarkup table
 */
void ProgramInfo::SetResolution(uint width, uint height, long long frame)
{
    if (isVideo)
        return;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("INSERT INTO recordedmarkup"
                  "    (chanid, starttime, mark, type, data)"
                  "    VALUES"
                  " ( :CHANID, :STARTTIME, :MARK, :TYPE, :DATA);");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);
    query.bindValue(":MARK", frame);
    query.bindValue(":TYPE", MARK_VIDEO_WIDTH);
    query.bindValue(":DATA", width);

    if (!query.exec() || !query.isActive())
        MythDB::DBError("Resolution insert", query);

    query.prepare("INSERT INTO recordedmarkup"
                  "    (chanid, starttime, mark, type, data)"
                  "    VALUES"
                  " ( :CHANID, :STARTTIME, :MARK, :TYPE, :DATA);");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);
    query.bindValue(":MARK", frame);
    query.bindValue(":TYPE", MARK_VIDEO_HEIGHT);
    query.bindValue(":DATA", height);

    if (!query.exec() || !query.isActive())
        MythDB::DBError("Resolution insert", query);
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
    result.bindValue(":TITLE", title);
    result.bindValue(":STATION", chansign);

    result.exec();
    if (!result.isActive())
        MythDB::DBError("ReactivateRecording", result);

    ScheduledRecording::signalChange(0);
}

/**
 *  \brief Adds recording history, creating "record" it if necessary.
 */
void ProgramInfo::AddHistory(bool resched, bool forcedup)
{
    bool dup = (recstatus == rsRecorded || forcedup);
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
    result.bindValue(":TITLE", title);
    result.bindValue(":SUBTITLE", subtitle);
    result.bindValue(":DESC", description);
    result.bindValue(":CATEGORY", category);
    result.bindValue(":SERIESID", seriesid);
    result.bindValue(":PROGRAMID", programid);
    result.bindValue(":FINDID", findid);
    result.bindValue(":RECORDID", recordid);
    result.bindValue(":STATION", chansign);
    result.bindValue(":RECTYPE", rectype);
    result.bindValue(":RECSTATUS", rs);
    result.bindValue(":DUPLICATE", dup);
    result.bindValue(":REACTIVATE", reactivate);

    result.exec();
    if (!result.isActive())
        MythDB::DBError("addHistory", result);

    if (dup && findid)
    {
        result.prepare("REPLACE INTO oldfind (recordid, findid) "
                       "VALUES(:RECORDID,:FINDID);");
        result.bindValue(":RECORDID", recordid);
        result.bindValue(":FINDID", findid);

        result.exec();
        if (!result.isActive())
            MythDB::DBError("addFindHistory", result);
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
    result.bindValue(":TITLE", title);
    result.bindValue(":START", recstartts);
    result.bindValue(":STATION", chansign);

    result.exec();
    if (!result.isActive())
        MythDB::DBError("deleteHistory", result);

    if (/*duplicate &&*/ findid)
    {
        result.prepare("DELETE FROM oldfind WHERE "
                       "recordid = :RECORDID AND findid = :FINDID");
        result.bindValue(":RECORDID", recordid);
        result.bindValue(":FINDID", findid);

        result.exec();
        if (!result.isActive())
            MythDB::DBError("deleteFindHistory", result);
    }

    // The removal of an entry from oldrecorded may affect near-future
    // scheduling decisions, so recalculate
    ScheduledRecording::signalChange(0);
}

/** \fn ProgramInfo::ForgetHistory(void)
 *  \brief Forget the recording of a program so it will be recorded again.
 *
 * The duplicate flags in both the recorded and old recorded tables are set
 * to 0. This causes these records to be skipped in the left join in the BUSQ
 * In addition, any "Never Record" fake entries are removed from the oldrecorded
 * table and any entries in the oldfind table are removed.
 */
void ProgramInfo::ForgetHistory(void)
{
    MSqlQuery result(MSqlQuery::InitCon());

    result.prepare("UPDATE recorded SET duplicate = 0 "
                   "WHERE chanid = :CHANID "
                       "AND starttime = :STARTTIME "
                       "AND title = :TITLE;");
    result.bindValue(":STARTTIME", recstartts);
    result.bindValue(":TITLE", title);
    result.bindValue(":CHANID", chanid);

    result.exec();
    if (!result.isActive())
        MythDB::DBError("forgetRecorded", result);

    result.prepare("UPDATE oldrecorded SET duplicate = 0 "
                   "WHERE duplicate = 1 "
                   "AND title = :TITLE AND "
                   "((programid = '' AND subtitle = :SUBTITLE"
                   "  AND description = :DESC) OR "
                   " (programid <> '' AND programid = :PROGRAMID) OR "
                   " (findid <> 0 AND findid = :FINDID))");
    result.bindValue(":TITLE", title);
    result.bindValue(":SUBTITLE", subtitle);
    result.bindValue(":DESC", description);
    result.bindValue(":PROGRAMID", programid);
    result.bindValue(":FINDID", findid);

    result.exec();
    if (!result.isActive())
        MythDB::DBError("forgetHistory", result);

    result.prepare("DELETE FROM oldrecorded "
                   "WHERE recstatus = :NEVER AND duplicate = 0");
    result.bindValue(":NEVER", rsNeverRecord);

    result.exec();
    if (!result.isActive())
        MythDB::DBError("forgetNeverHisttory", result);

    if (findid)
    {
        result.prepare("DELETE FROM oldfind WHERE "
                       "recordid = :RECORDID AND findid = :FINDID");
        result.bindValue(":RECORDID", recordid);
        result.bindValue(":FINDID", findid);

        result.exec();
        if (!result.isActive())
            MythDB::DBError("forgetFindHistory", result);
    }

    // The removal of an entry from oldrecorded may affect near-future
    // scheduling decisions, so recalculate
    ScheduledRecording::signalChange(0);
}

/** \fn ProgramInfo::SetDupHistory(void)
 *  \brief Set the duplicate flag in oldrecorded.
 */
void ProgramInfo::SetDupHistory(void)
{
    MSqlQuery result(MSqlQuery::InitCon());

    result.prepare("UPDATE oldrecorded SET duplicate = 1 "
                   "WHERE duplicate = 0 "
                   "AND title = :TITLE AND "
                   "((programid = '' AND subtitle = :SUBTITLE"
                   "  AND description = :DESC) OR "
                   " (programid <> '' AND programid = :PROGRAMID) OR "
                   " (findid <> 0 AND findid = :FINDID))");
    result.bindValue(":TITLE", title);
    result.bindValue(":SUBTITLE", subtitle);
    result.bindValue(":DESC", description);
    result.bindValue(":PROGRAMID", programid);
    result.bindValue(":FINDID", findid);

    result.exec();
    if (!result.isActive())
        MythDB::DBError("setDupHistory", result);

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
        if (cardid > 0)
            return QString::number(cardid);
        else
            return QObject::tr("R", "RecStatusChar rsCurrentRecording");
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
    case rsFailed:
        return QObject::tr("f", "RecStatusChar rsFailed");
    case rsNotListed:
        return QObject::tr("N", "RecStatusChar rsNotListed");
    case rsNeverRecord:
        return QObject::tr("V", "RecStatusChar rsNeverRecord");
    case rsOffLine:
        return QObject::tr("F", "RecStatusChar rsOffLine");
    case rsOtherShowing:
        return QObject::tr("O", "RecStatusChar rsOtherShowing");
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
        case rsFailed:
            return QObject::tr("Recorder Failed");
        case rsNotListed:
            return QObject::tr("Not Listed");
        case rsNeverRecord:
            return QObject::tr("Never Record");
        case rsOffLine:
            return QObject::tr("Recorder Off-Line");
        case rsOtherShowing:
            return QObject::tr("Other Showing");
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
        case rsFailed:
            message += QObject::tr("the recorder failed to record.");
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
            message += QObject::tr("this recording rule is inactive.");
            break;
        case rsNotListed:
            message += QObject::tr("this rule does not match any showings in "
                                   "the current program listings.");
            break;
        case rsNeverRecord:
            message += QObject::tr("it was marked to never be recorded.");
            break;
        case rsOffLine:
            message += QObject::tr("the backend recorder is off-line.");
            break;
        case rsOtherShowing:
            message += QObject::tr("this episode will be recorded on a "
                                   "different channel in this time slot.");
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
        MythDB::DBError("Saving program", query);

    query.prepare("INSERT INTO program (chanid,starttime,endtime,"
                  " title,subtitle,description,category,airdate,"
                  " stars) VALUES (:CHANID,:STARTTIME,:ENDTIME,:TITLE,"
                  " :SUBTITLE,:DESCRIPTION,:CATEGORY,:AIRDATE,:STARS);");
    query.bindValue(":CHANID", chanid.toInt());
    query.bindValue(":STARTTIME", startts);
    query.bindValue(":ENDTIME", endts);
    query.bindValue(":TITLE", title);
    query.bindValue(":SUBTITLE", subtitle);
    query.bindValue(":DESCRIPTION", description);
    query.bindValue(":CATEGORY", category);
    query.bindValue(":AIRDATE", "0");
    query.bindValue(":STARS", "0");

    if (!query.exec())
        MythDB::DBError("Saving program", query);
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
 *  \brief Pops up a DialogBox with program info.
 */
void ProgramInfo::showDetails(void) const
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ProgDetails *details_dialog  = new ProgDetails(mainStack, this);

    if (!details_dialog->Create())
    {
        delete details_dialog;
        return;
    }

    mainStack->AddScreen(details_dialog);

    // HACK begin - remove when everything is using mythui
    if (GetMythMainWindow()->currentWidget())
    {
        QWidget *widget = GetMythMainWindow()->currentWidget();
        vector<QWidget *> widgetList;

        while (widget)
        {
            widgetList.push_back(widget);
            GetMythMainWindow()->detach(widget);
            widget = GetMythMainWindow()->currentWidget();
        }

        GetMythMainWindow()->GetPaintWindow()->raise();
        GetMythMainWindow()->GetPaintWindow()->setFocus();

        int screenCount = mainStack->TotalScreens();
        do
        {
            qApp->processEvents();
            usleep(5000);
        } while (mainStack->TotalScreens() >= screenCount);

        vector<QWidget*>::reverse_iterator it;
        for (it = widgetList.rbegin(); it != widgetList.rend(); ++it)
        {
            GetMythMainWindow()->attach(*it);
        }
    }
    // HACK end
}

/** \fn ProgramInfo::getProgramFlags(void) const
 *  \brief Returns a bitmap of the recorded programmes flags
 *  \sa GetAutoRunJobs(void)
 */
int ProgramInfo::getProgramFlags(void) const
{
    int flags = 0;
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT commflagged, cutlist, autoexpire, "
                  "editing, bookmark, watched, preserve "
                  "FROM recorded LEFT JOIN recordedprogram ON "
                  "(recorded.chanid = recordedprogram.chanid AND "
                  "recorded.progstart = recordedprogram.starttime) "
                  "WHERE recorded.chanid = :CHANID AND recorded.starttime = :STARTTIME ;");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);

    if (query.exec() && query.next())
    {
        flags |= (query.value(0).toInt() == COMM_FLAG_DONE) ? FL_COMMFLAG : 0;
        flags |= (query.value(1).toInt() == 1) ? FL_CUTLIST : 0;
        flags |= query.value(2).toInt() ? FL_AUTOEXP : 0;
        if ((query.value(3).toInt()) ||
            (query.value(0).toInt() == COMM_FLAG_PROCESSING))
            flags |= FL_EDITING;
        flags |= (query.value(4).toInt() == 1) ? FL_BOOKMARK : 0;
        flags |= (query.value(5).toInt() == 1) ? FL_WATCHED : 0;
        flags |= (query.value(6).toInt() == 1) ? FL_PRESERVED : 0;
    }

    return flags;
}

void ProgramInfo::getProgramProperties(void)
{

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT audioprop+0, videoprop+0, subtitletypes+0 "
                  "FROM recorded LEFT JOIN recordedprogram ON "
                  "(recorded.chanid = recordedprogram.chanid AND "
                  "recorded.progstart = recordedprogram.starttime) "
                  "WHERE recorded.chanid = :CHANID AND recorded.starttime = :STARTTIME ;");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);

    if (query.exec() && query.next())
    {
        audioproperties = query.value(0).toInt();
        videoproperties = query.value(1).toInt();
        subtitleType = query.value(2).toInt();
    }

}

void ProgramInfo::UpdateInUseMark(bool force)
{
    if (isVideo)
        return;

    if (inUseForWhat.isEmpty())
        return;

    if (force || lastInUseTime.secsTo(QDateTime::currentDateTime()) > 15 * 60)
        MarkAsInUse(true);
}

bool ProgramInfo::PathnameExists(void)
{
    if (pathname.left(7) == "myth://")
       return RemoteCheckFile(this);

    QFile checkFile(pathname);

    return checkFile.exists();
}

QString ProgramInfo::GetRecGroupPassword(QString group)
{
    QString result = QString("");

    if (group == "All Programs")
    {
        result = gContext->GetSetting("AllRecGroupPassword");
    }
    else
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT password FROM recgrouppassword "
                        "WHERE recgroup = :GROUP ;");
        query.bindValue(":GROUP", group);

        if (query.exec() && query.next())
            result = query.value(0).toString();
    }

    if (result == QString::null)
        result = QString("");

    return(result);
}

/** \brief Update Rec Group if its changed by a different programinfo instance.
 */
void ProgramInfo::UpdateRecGroup(void)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT recgroup FROM recorded"
                    "WHERE chanid = :CHANID"
                    "AND starttime = :START ;");
    query.bindValue(":START", recstartts);
    query.bindValue(":CHANID", chanid);
    if (query.exec() && query.next())
    {
        recgroup = query.value(0).toString();
    }
}
void ProgramInfo::MarkAsInUse(bool inuse, QString usedFor)
{
    if (isVideo)
        return;

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

    if (pathname.left(7) == "myth://")
        pathname = GetPlaybackURL();

    if (pathname.right(1) == "/")
        pathname.remove(pathname.length() - 1, 1);

    QString recDir = "";
    if (hostname == gContext->GetHostName())
    {
        // we may be recording this file and it may not exist yet so we need
        // to do some checking to see what is in pathname
        QFileInfo testFile(pathname);
        if (testFile.exists())
        {
            while (testFile.isSymLink())
                testFile.setFile(testFile.readLink());

            if (testFile.isFile())
                recDir = testFile.path();
            else if (testFile.isDir())
                recDir = testFile.filePath();
        }
        else
        {
            testFile.setFile(testFile.absolutePath());
            if (testFile.exists())
            {
                uint i = 0;
                for (; (i < 256) && testFile.isSymLink(); i++)
                    testFile.setFile(testFile.readLink());

                if ((256 != i) && testFile.isDir())
                    recDir = testFile.filePath();
            }
        }
    }
    else if (inUseForWhat == PreviewGenerator::kInUseID)
    {
        recDir = "";
    }
    else if (!gContext->IsBackend() &&
              RemoteCheckFile(this) && pathname.left(1) == "/")
    {
        recDir = pathname.section("/", 0, -2);
    }

    lastInUseTime = mythCurrentDateTime();

    query.prepare("INSERT INTO inuseprograms "
                  " (chanid, starttime, recusage, hostname, lastupdatetime, "
                      " rechost, recdir ) "
                  " VALUES "
                  " (:CHANID, :STARTTIME, :RECUSAGE, :HOSTNAME, :UPDATETIME, "
                      " :RECHOST, :RECDIR);");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);
    query.bindValue(":HOSTNAME", gContext->GetHostName());
    query.bindValue(":RECUSAGE", inUseForWhat);
    query.bindValue(":UPDATETIME", lastInUseTime);
    query.bindValue(":RECHOST", hostname);
    query.bindValue(":RECDIR", recDir);

    if (!query.exec() || !query.isActive())
        MythDB::DBError("SetInUse", query);

    // Let others know we changed status
    if (notifyOfChange && !gContext->IsBackend())
        RemoteSendMessage("RECORDING_LIST_CHANGE");
}

/** \fn ProgramInfo::GetChannel(QString&,QString&) const
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
        MythDB::DBError("GetChannel(ProgInfo...)", query);
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

    DialogBox *dlg = new DialogBox(gContext->GetMainWindow(), message);
    int button = 0, ok = -1, react = -1, stop = -1, addov = -1, forget = -1,
        clearov = -1, edend = -1, ednorm = -1, edcust = -1;

    dlg->AddButton(QObject::tr("OK"));
    ok = button++;

    if (recstartts < now && recendts > now)
    {
        if (recstatus != rsRecording)
        {
            dlg->AddButton(QObject::tr("Reactivate"));
            react = button++;
        }
        else
        {
            dlg->AddButton(QObject::tr("Stop recording"));
            stop = button++;
        }
    }
    if (recendts > now)
    {
        if (rectype != kSingleRecord && rectype != kOverrideRecord)
        {
            if (recstartts > now)
            {
                dlg->AddButton(QObject::tr("Don't record"));
                addov = button++;
            }
            if (recstatus != rsRecording && rectype != kFindOneRecord &&
                !((findid == 0 || !IsFindApplicable()) &&
                  catType == "series" &&
                  programid.contains(QRegExp("0000$"))) &&
                ((!(dupmethod & kDupCheckNone) && !programid.isEmpty() &&
                  (findid != 0 || !IsFindApplicable())) ||
                 ((dupmethod & kDupCheckSub) && !subtitle.isEmpty()) ||
                 ((dupmethod & kDupCheckDesc) && !description.isEmpty()) ||
                 ((dupmethod & kDupCheckSubThenDesc) && (!subtitle.isEmpty() || !description.isEmpty())) ))
            {
                dlg->AddButton(QObject::tr("Never record"));
                forget = button++;
            }
        }

        if (rectype != kOverrideRecord && rectype != kDontRecord)
        {
            if (recstatus == rsRecording)
            {
                dlg->AddButton(QObject::tr("Change Ending Time"));
                edend = button++;
            }
            else
            {
                dlg->AddButton(QObject::tr("Edit Options"));
                ednorm = button++;

                if (rectype != kSingleRecord && rectype != kFindOneRecord)
                {
                    dlg->AddButton(QObject::tr("Add Override"));
                    edcust = button++;
                }
            }
        }

        if (rectype == kOverrideRecord || rectype == kDontRecord)
        {
            if (recstatus == rsRecording)
            {
                dlg->AddButton(QObject::tr("Change Ending Time"));
                edend = button++;
            }
            else
            {
                dlg->AddButton(QObject::tr("Edit Override"));
                ednorm = button++;
                dlg->AddButton(QObject::tr("Clear Override"));
                clearov = button++;
            }
        }
    }

    DialogCode code = dlg->exec();
    int ret = MythDialog::CalcItemIndex(code);
    dlg->deleteLater();
    dlg = NULL;

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
        AddHistory(true, true);
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

    DialogBox *dlg = new DialogBox(gContext->GetMainWindow(), message);
    int button = 0, ok = -1, react = -1, addov = -1, clearov = -1,
        ednorm = -1, edcust = -1, forget = -1, addov1 = -1, forget1 = -1;

    dlg->AddButton(QObject::tr("OK"));
    ok = button++;

    QDateTime now = QDateTime::currentDateTime();

    if ((recstartts < now) && (recendts > now) &&
        (recstatus != rsDontRecord) && (recstatus != rsNotListed))
    {
        dlg->AddButton(QObject::tr("Reactivate"));
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
             recstatus == rsOtherShowing ||
             recstatus == rsNeverRecord ||
             recstatus == rsRepeat ||
             recstatus == rsInactive ||
             recstatus == rsLaterShowing))
        {
            dlg->AddButton(QObject::tr("Record anyway"));
            addov = button++;
            if (recstatus == rsPreviousRecording || recstatus == rsNeverRecord)
            {
                dlg->AddButton(QObject::tr("Forget Previous"));
                forget = button++;
            }
        }

        if (rectype != kOverrideRecord && rectype != kDontRecord)
        {
            if (rectype != kSingleRecord &&
                recstatus != rsPreviousRecording &&
                recstatus != rsCurrentRecording &&
                recstatus != rsNeverRecord &&
                recstatus != rsNotListed)
            {
                if (recstartts > now)
                {
                    dlg->AddButton(QObject::tr("Don't record"));
                    addov1 = button++;
                }
                if (rectype != kFindOneRecord &&
                    !((findid == 0 || !IsFindApplicable()) &&
                      catType == "series" &&
                      programid.contains(QRegExp("0000$"))) &&
                    ((!(dupmethod & kDupCheckNone) && !programid.isEmpty() &&
                      (findid != 0 || !IsFindApplicable())) ||
                     ((dupmethod & kDupCheckSub) && !subtitle.isEmpty()) ||
                     ((dupmethod & kDupCheckDesc) && !description.isEmpty())))
                {
                    dlg->AddButton(QObject::tr("Never record"));
                    forget1 = button++;
                }
            }

            dlg->AddButton(QObject::tr("Edit Options"));
            ednorm = button++;

            if (rectype != kSingleRecord && rectype != kFindOneRecord &&
                recstatus != rsNotListed)
            {
                dlg->AddButton(QObject::tr("Add Override"));
                edcust = button++;
            }
        }

        if (rectype == kOverrideRecord || rectype == kDontRecord)
        {
            dlg->AddButton(QObject::tr("Edit Override"));
            ednorm = button++;

            dlg->AddButton(QObject::tr("Clear Override"));
            clearov = button++;
        }
    }

    DialogCode code = dlg->exec();
    int ret = MythDialog::CalcItemIndex(code);
    dlg->deleteLater();
    dlg = NULL;

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
        AddHistory(true, true);
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

static int init_tr(void)
{
    static bool done = false;
    static QMutex init_tr_lock;
    QMutexLocker locker(&init_tr_lock);
    if (done)
        return 1;

    QString rec_profile_names =
        QObject::tr("Default",        "Recording Profile Default") +
        QObject::tr("High Quality",   "Recording Profile High Quality") +
        QObject::tr("Live TV",        "Recording Profile Live TV") +
        QObject::tr("Low Quality",    "Recording Profile Low Quality") +
        QObject::tr("Medium Quality", "Recording Profile Medium Quality") +
        QObject::tr("MPEG2",          "Recording Profile MPEG2") +
        QObject::tr("RTjpeg/MPEG4",   "Recording Profile RTjpeg/MPEG4");


    QString rec_profile_groups =
        QObject::tr("CRC IP Recorders",
                    "Recording Profile Group Name") +
        QObject::tr("DBOX2 Input",
                    "Recording Profile Group Name") +
        QObject::tr("FireWire Input",
                    "Recording Profile Group Name") +
        QObject::tr("Freebox Input",
                    "Recording Profile Group Name") +
        QObject::tr("Hardware DVB Encoders",
                    "Recording Profile Group Name") +
        QObject::tr("Hardware HDTV",
                    "Recording Profile Group Name") +
        QObject::tr("Hardware MJPEG Encoders (Matrox G200-TV, Miro DC10, etc)",
                    "Recording Profile Group Name") +
        QObject::tr("HD-PVR Recorders",
                    "Recording Profile Group Name") +
        QObject::tr("HDHomeRun Recorders",
                    "Recording Profile Group Name") +
        QObject::tr("MPEG-2 Encoders (PVR-x50, PVR-500)",
                    "Recording Profile Group Name") +
        QObject::tr("Software Encoders (v4l based)",
                    "Recording Profile Group Name") +
        QObject::tr("Transcoders",
                    "Recording Profile Group Name") +
        QObject::tr("USB Mpeg-4 Encoder (Plextor ConvertX, etc)",
                    "Recording Profile Group Name");

    QString display_rec_groups =
        QObject::tr("All Programs",   "Recording Group All Programs") +
        QObject::tr("LiveTV",         "Recording Group LiveTV") +
        QObject::tr("Default",        "Recording Group Default") +
        QObject::tr("Deleted",        "Recording Group Deleted");

    QString storage_groups =
        QObject::tr("Default",        "Storage Group Name") +
        QObject::tr("LiveTV",         "Storage Group Name") +
        QObject::tr("Thumbnails",     "Storage Group Name") +
        QObject::tr("DB Backups",     "Storage Group Name");

    QString play_groups =
        QObject::tr("Default",        "Playback Group Name");

    done = true;
    return (rec_profile_names.length() +
            rec_profile_groups.length() +
            display_rec_groups.length() +
            storage_groups.length() +
            play_groups.length());
}

QString ProgramInfo::i18n(const QString &msg)
{
    init_tr();
    QByteArray msg_arr = msg.toLatin1();
    QString msg_i18n = QObject::tr(msg_arr.constData());
    QByteArray msg_i18n_arr = msg_i18n.toLatin1();
    return (msg_arr == msg_i18n_arr) ? msg_i18n : msg;
}
/* vim: set expandtab tabstop=4 shiftwidth=4: */
