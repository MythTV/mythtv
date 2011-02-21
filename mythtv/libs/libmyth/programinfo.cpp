// -*- Mode: c++ -*-

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
#include <QUrl>
#include <QFile>
#include <QFileInfo>
#include <QDir>

// MythTV headers
#include "programinfo.h"
#include "util.h"
#include "mythcorecontext.h"
#include "dialogbox.h"
#include "remoteutil.h"
#include "mythdb.h"
#include "mythverbose.h"
#include "storagegroup.h"
#include "programinfoupdater.h"

#define LOC      QString("ProgramInfo(%1): ").arg(GetBasename())
#define LOC_WARN QString("ProgramInfo(%1), Warning: ").arg(GetBasename())
#define LOC_ERR  QString("ProgramInfo(%1), Error: ").arg(GetBasename())

//#define DEBUG_IN_USE

static int init_tr(void);

int pginfo_init_statics() { return ProgramInfo::InitStatics(); }
QMutex ProgramInfo::staticDataLock;
ProgramInfoUpdater *ProgramInfo::updater;
int dummy = pginfo_init_statics();


const QString ProgramInfo::kFromRecordedQuery =
    "SELECT r.title,            r.subtitle,     r.description,     "// 0-2
    "       r.category,         r.chanid,       c.channum,         "// 3-5
    "       c.callsign,         c.name,         c.outputfilters,   "// 6-8
    "       r.recgroup,         r.playgroup,    r.storagegroup,    "// 9-11
    "       r.basename,         r.hostname,     r.recpriority,     "//12-14
    "       r.seriesid,         r.programid,    r.filesize,        "//15-17
    "       r.progstart,        r.progend,      r.stars,           "//18-20
    "       r.starttime,        r.endtime,      p.airdate+0,       "//21-23
    "       r.originalairdate,  r.lastmodified, r.recordid,        "//24-26
    "       c.commmethod,       r.commflagged,  r.previouslyshown, "//27-29
    "       r.transcoder,       r.transcoded,   r.deletepending,   "//30-32
    "       r.preserve,         r.cutlist,      r.autoexpire,      "//33-35
    "       r.editing,          r.bookmark,     r.watched,         "//36-38
    "       p.audioprop+0,      p.videoprop+0,  p.subtitletypes+0, "//39-41
    "       r.findid,           rec.dupin,      rec.dupmethod      "//42-44
    "FROM recorded AS r "
    "LEFT JOIN channel AS c "
    "ON (r.chanid    = c.chanid) "
    "LEFT JOIN recordedprogram AS p "
    "ON (r.chanid    = p.chanid AND "
    "    r.progstart = p.starttime) "
    "LEFT JOIN record AS rec "
    "ON (r.recordid = rec.recordid) ";

static void set_flag(uint32_t &flags, int flag_to_set, bool is_set)
{
    flags &= ~flag_to_set;
    if (is_set)
        flags |= flag_to_set;
}

/** \fn ProgramInfo::ProgramInfo(void)
 *  \brief Null constructor.
 */
ProgramInfo::ProgramInfo(void) :
    title(),
    subtitle(),
    description(),
    category(),

    recpriority(0),

    chanid(0),
    chanstr(),
    chansign(),
    channame(),
    chanplaybackfilters(),

    recgroup("Default"),
    playgroup("Default"),

    pathname(),

    hostname(),
    storagegroup("Default"),

    seriesid(),
    programid(),
    catType(),


    filesize(0ULL),

    startts(mythCurrentDateTime()),
    endts(startts),
    recstartts(startts),
    recendts(startts),

    stars(0.0f),

    originalAirDate(),
    lastmodified(startts),
    lastInUseTime(startts.addSecs(-4 * 60 * 60)),

    prefinput(0),
    recpriority2(0),
    recordid(0),
    parentid(0),

    sourceid(0),
    inputid(0),
    cardid(0),

    findid(0),

    programflags(FL_NONE),
    properties(0),
    year(0),

    recstatus(rsUnknown),
    oldrecstatus(rsUnknown),
    rectype(kNotRecording),
    dupin(kDupsInAll),
    dupmethod(kDupCheckSubDesc),

    // everything below this line is not serialized
    availableStatus(asAvailable),
    spread(-1),
    startCol(-1),
    sortTitle(),

    // Private
    inUseForWhat(),
    positionMapDBReplacement(NULL)
{
}

/** \fn ProgramInfo::ProgramInfo(const ProgramInfo &other)
 *  \brief Copy constructor.
 */
ProgramInfo::ProgramInfo(const ProgramInfo &other) :
    title(other.title),
    subtitle(other.subtitle),
    description(other.description),
    category(other.category),

    recpriority(other.recpriority),

    chanid(other.chanid),
    chanstr(other.chanstr),
    chansign(other.chansign),
    channame(other.channame),
    chanplaybackfilters(other.chanplaybackfilters),

    recgroup(other.recgroup),
    playgroup(other.playgroup),

    pathname(other.pathname),

    hostname(other.hostname),
    storagegroup(other.storagegroup),

    seriesid(other.seriesid),
    programid(other.programid),
    catType(other.catType),

    filesize(other.filesize),

    startts(other.startts),
    endts(other.endts),
    recstartts(other.recstartts),
    recendts(other.recendts),

    stars(other.stars),

    originalAirDate(other.originalAirDate),
    lastmodified(other.lastmodified),
    lastInUseTime(QDateTime::currentDateTime().addSecs(-4 * 60 * 60)),

    prefinput(other.prefinput),
    recpriority2(other.recpriority2),
    recordid(other.recordid),
    parentid(other.parentid),

    sourceid(other.sourceid),
    inputid(other.inputid),
    cardid(other.cardid),

    findid(other.findid),
    programflags(other.programflags),
    properties(other.properties),
    year(other.year),

    recstatus(other.recstatus),
    oldrecstatus(other.oldrecstatus),
    rectype(other.rectype),
    dupin(other.dupin),
    dupmethod(other.dupmethod),

    // everything below this line is not serialized
    availableStatus(other.availableStatus),
    spread(other.spread),
    startCol(other.startCol),
    sortTitle(other.sortTitle),

    // Private
    inUseForWhat(),
    positionMapDBReplacement(other.positionMapDBReplacement)
{
}

ProgramInfo::ProgramInfo(uint _chanid, const QDateTime &_recstartts) :
    chanid(0),
    positionMapDBReplacement(NULL)
{
    LoadProgramFromRecorded(_chanid, _recstartts);
}

ProgramInfo::ProgramInfo(
    const QString &_title,
    const QString &_subtitle,
    const QString &_description,
    const QString &_category,

    uint _chanid,
    const QString &_channum,
    const QString &_chansign,
    const QString &_channame,
    const QString &_chanplaybackfilters,

    const QString &_recgroup,
    const QString &_playgroup,

    const QString &_pathname,

    const QString &_hostname,
    const QString &_storagegroup,

    const QString &_seriesid,
    const QString &_programid,

    int _recpriority,

    uint64_t _filesize,

    const QDateTime &_startts,
    const QDateTime &_endts,
    const QDateTime &_recstartts,
    const QDateTime &_recendts,

    float _stars,

    uint _year,
    const QDate &_originalAirDate,
    const QDateTime &_lastmodified,

    RecStatusType _recstatus,

    uint _recordid,

    RecordingDupInType _dupin,
    RecordingDupMethodType _dupmethod,

    uint _findid,

    uint _programflags,
    uint _audioproperties,
    uint _videoproperties,
    uint _subtitleType) :
    title(_title),
    subtitle(_subtitle),
    description(_description),
    category(_category),

    recpriority(_recpriority),

    chanid(_chanid),
    chanstr(_channum),
    chansign(_chansign),
    channame(_channame),
    chanplaybackfilters(_chanplaybackfilters),

    recgroup(_recgroup),
    playgroup(_playgroup),

    pathname(_pathname),

    hostname(_hostname),
    storagegroup(_storagegroup),

    seriesid(_seriesid),
    programid(_programid),
    catType(),

    filesize(_filesize),

    startts(_startts),
    endts(_endts),
    recstartts(_recstartts),
    recendts(_recendts),

    stars(clamp(_stars, 0.0f, 1.0f)),

    originalAirDate(_originalAirDate),
    lastmodified(_lastmodified),
    lastInUseTime(QDateTime::currentDateTime().addSecs(-4 * 60 * 60)),

    prefinput(0),
    recpriority2(0),
    recordid(_recordid),
    parentid(0),

    sourceid(0),
    inputid(0),
    cardid(0),

    findid(_findid),

    programflags(_programflags),
    properties((_subtitleType<<11) | (_videoproperties<<6) | _audioproperties),
    year(_year),

    recstatus(_recstatus),
    oldrecstatus(rsUnknown),
    rectype(kNotRecording),
    dupin(_dupin),
    dupmethod(_dupmethod),

    // everything below this line is not serialized
    availableStatus(asAvailable),
    spread(-1),
    startCol(-1),
    sortTitle(),

    // Private
    inUseForWhat(),
    positionMapDBReplacement(NULL)
{
    if (originalAirDate.isValid() && originalAirDate < QDate(1940, 1, 1))
        originalAirDate = QDate();

    SetPathname(_pathname);
}

ProgramInfo::ProgramInfo(
    const QString &_title,
    const QString &_subtitle,
    const QString &_description,
    const QString &_category,

    uint _chanid,
    const QString &_channum,
    const QString &_chansign,
    const QString &_channame,

    const QString &_seriesid,
    const QString &_programid,

    const QDateTime &_startts,
    const QDateTime &_endts,
    const QDateTime &_recstartts,
    const QDateTime &_recendts,

    RecStatusType _recstatus,

    uint _recordid,

    RecordingType _rectype,

    uint _findid,

    bool duplicate) :
    title(_title),
    subtitle(_subtitle),
    description(_description),
    category(_category),

    recpriority(0),

    chanid(_chanid),
    chanstr(_channum),
    chansign(_chansign),
    channame(_channame),
    chanplaybackfilters(),

    recgroup("Default"),
    playgroup("Default"),

    pathname(),

    hostname(),
    storagegroup("Default"),

    seriesid(_seriesid),
    programid(_programid),
    catType(),

    filesize(0ULL),

    startts(_startts),
    endts(_endts),
    recstartts(_recstartts),
    recendts(_recendts),

    stars(0.0f),

    originalAirDate(),
    lastmodified(startts),
    lastInUseTime(QDateTime::currentDateTime().addSecs(-4 * 60 * 60)),

    prefinput(0),
    recpriority2(0),
    recordid(_recordid),
    parentid(0),

    sourceid(0),
    inputid(0),
    cardid(0),

    findid(_findid),

    programflags((duplicate) ? FL_DUPLICATE : 0),
    properties(0),
    year(0),

    recstatus(_recstatus),
    oldrecstatus(rsUnknown),
    rectype(_rectype),
    dupin(kDupsInAll),
    dupmethod(kDupCheckSubDesc),

    // everything below this line is not serialized
    availableStatus(asAvailable),
    spread(-1),
    startCol(-1),
    sortTitle(),

    // Private
    inUseForWhat(),
    positionMapDBReplacement(NULL)
{
}

ProgramInfo::ProgramInfo(
    const QString &_title,
    const QString &_subtitle,
    const QString &_description,
    const QString &_category,

    uint _chanid,
    const QString &_channum,
    const QString &_chansign,
    const QString &_channame,
    const QString &_chanplaybackfilters,

    const QDateTime &_startts,
    const QDateTime &_endts,
    const QDateTime &_recstartts,
    const QDateTime &_recendts,

    const QString &_seriesid,
    const QString &_programid,
    const QString &_catType,

    float _stars,
    uint _year,
    const QDate &_originalAirDate,
    RecStatusType _recstatus,
    uint _recordid,
    RecordingType _rectype,
    uint _findid,

    bool commfree,
    bool repeat,

    const ProgramList &schedList, bool oneChanid) :
    title(_title),
    subtitle(_subtitle),
    description(_description),
    category(_category),

    recpriority(0),

    chanid(_chanid),
    chanstr(_channum),
    chansign(_chansign),
    channame(_channame),
    chanplaybackfilters(_chanplaybackfilters),

    recgroup("Default"),
    playgroup("Default"),

    pathname(),

    hostname(),
    storagegroup("Default"),

    seriesid(_seriesid),
    programid(_programid),
    catType(_catType),

    filesize(0ULL),

    startts(_startts),
    endts(_endts),
    recstartts(_recstartts),
    recendts(_recendts),

    stars(clamp(_stars, 0.0f, 1.0f)),

    originalAirDate(_originalAirDate),
    lastmodified(startts),
    lastInUseTime(startts.addSecs(-4 * 60 * 60)),

    prefinput(0),
    recpriority2(0),
    recordid(_recordid),
    parentid(0),

    sourceid(0),
    inputid(0),
    cardid(0),

    findid(_findid),

    programflags(FL_NONE),
    properties(0),
    year(_year),

    recstatus(_recstatus),
    oldrecstatus(rsUnknown),
    rectype(_rectype),
    dupin(kDupsInAll),
    dupmethod(kDupCheckSubDesc),

    // everything below this line is not serialized
    availableStatus(asAvailable),
    spread(-1),
    startCol(-1),
    sortTitle(),

    // Private
    inUseForWhat(),
    positionMapDBReplacement(NULL)
{
    programflags |= (commfree) ? FL_CHANCOMMFREE : 0;
    programflags |= (repeat)   ? FL_REPEAT       : 0;

    if (originalAirDate.isValid() && originalAirDate < QDate(1940, 1, 1))
        originalAirDate = QDate();

    ProgramList::const_iterator it = schedList.begin();
    for (; it != schedList.end(); ++it)
    {
        if (!IsSameTimeslot(**it))
            continue;

        const ProgramInfo &s = **it;
        recordid    = s.recordid;
        recstatus   = s.recstatus;
        rectype     = s.rectype;
        recpriority = s.recpriority;
        recstartts  = s.recstartts;
        recendts    = s.recendts;
        cardid      = s.cardid;
        inputid     = s.inputid;
        dupin       = s.dupin;
        dupmethod   = s.dupmethod;
        findid      = s.findid;

        if (s.recstatus == rsWillRecord || s.recstatus == rsRecording)
        {
            if (oneChanid)
            {
                chanid   = s.chanid;
                chanstr  = s.chanstr;
                chansign = s.chansign;
                channame = s.channame;
            }
            else if ((chanid != s.chanid) && (chanstr != s.chanstr))
            {
                recstatus = rsOtherShowing;
            }
        }
    }
}

ProgramInfo::ProgramInfo(
    const QString &_title,
    const QString &_subtitle,
    const QString &_description,
    const QString &_category,

    uint _chanid,
    const QString &_channum,
    const QString &_chansign,
    const QString &_channame,
    const QString &_chanplaybackfilters,

    const QString &_recgroup,
    const QString &_playgroup,

    const QDateTime &_startts,
    const QDateTime &_endts,
    const QDateTime &_recstartts,
    const QDateTime &_recendts,

    const QString &_seriesid,
    const QString &_programid) :
    title(_title),
    subtitle(_subtitle),
    description(_description),
    category(_category),

    recpriority(0),

    chanid(_chanid),
    chanstr(_channum),
    chansign(_chansign),
    channame(_channame),
    chanplaybackfilters(_chanplaybackfilters),

    recgroup(_recgroup),
    playgroup(_playgroup),

    pathname(),

    hostname(),
    storagegroup("Default"),

    seriesid(_seriesid),
    programid(_programid),
    catType(),

    filesize(0ULL),

    startts(_startts),
    endts(_endts),
    recstartts(_recstartts),
    recendts(_recendts),

    stars(0.0f),

    originalAirDate(),
    lastmodified(QDateTime::currentDateTime()),
    lastInUseTime(lastmodified.addSecs(-4 * 60 * 60)),

    prefinput(0),
    recpriority2(0),
    recordid(0),
    parentid(0),

    sourceid(0),
    inputid(0),
    cardid(0),

    findid(0),

    programflags(FL_NONE),
    properties(0),
    year(0),

    recstatus(rsUnknown),
    oldrecstatus(rsUnknown),
    rectype(kNotRecording),
    dupin(kDupsInAll),
    dupmethod(kDupCheckSubDesc),

    // everything below this line is not serialized
    availableStatus(asAvailable),
    spread(-1),
    startCol(-1),
    sortTitle(),

    // Private
    inUseForWhat(),
    positionMapDBReplacement(NULL)
{
}

ProgramInfo::ProgramInfo(const QString &_pathname) :
    chanid(0),
    positionMapDBReplacement(NULL)
{
    if (_pathname.isEmpty())
    {
        clear();
        return;
    }

    uint _chanid;
    QDateTime _recstartts;
    if (ExtractKeyFromPathname(_pathname, _chanid, _recstartts) &&
        LoadProgramFromRecorded(_chanid, _recstartts))
    {
        return;
    }

    clear();

    QDateTime cur = QDateTime::currentDateTime();
    recstartts = startts = cur.addSecs(-4 * 60 * 60 - 1);
    recendts   = endts   = cur.addSecs(-1);

    QString basename = _pathname.section('/', -1);
    if (_pathname == basename)
        SetPathname(QDir::currentPath() + '/' + _pathname);
    else if (_pathname.contains("./"))
        SetPathname(QFileInfo(_pathname).absoluteFilePath());
    else
        SetPathname(_pathname);
}

ProgramInfo::ProgramInfo(const QString &_pathname,
                         const QString &_plot,
                         const QString &_title,
                         const QString &_subtitle,
                         const QString &_director,
                         int _season, int _episode,
                         uint _length_in_minutes,
                         uint _year) :
    positionMapDBReplacement(NULL)
{
    clear();

    QDateTime cur = QDateTime::currentDateTime();
    recstartts = cur.addSecs((_length_in_minutes + 1) * -60);
    recendts   = recstartts.addSecs(_length_in_minutes * 60);
    startts.setDate(
        QDate::fromString(QString("%1-01-01").arg(year), Qt::ISODate));
    startts.setTime(QTime(0,0,0));
    endts      = startts.addSecs(_length_in_minutes * 60);

    QString pn = _pathname;
    if ((!_pathname.startsWith("myth://")) &&
        (_pathname.right(4).toLower() == ".iso" ||
         _pathname.right(4).toLower() == ".img" ||
         QDir(_pathname + "/VIDEO_TS").exists()))
    {
        pn = QString("dvd:%1").arg(_pathname);
    }
    else if (QDir(_pathname+"/BDMV").exists())
    {
        pn = QString("bd:%1").arg(_pathname);
    }
    SetPathname(pn);

    if (!_director.isEmpty())
    {
        description = QString("%1: %2.  ")
            .arg(QObject::tr("Directed By")).arg(_director);
    }

    description += _plot;

    if (!_subtitle.isEmpty())
        subtitle = _subtitle;

    if ((_season > 0) || (_episode > 0))
    {
        chanstr = QString("%1x%2")
            .arg(_season).arg(_episode,2,10,QChar('0'));
    }

    title = _title;
}

ProgramInfo::ProgramInfo(const QString &_title, uint _chanid,
                         const QDateTime &_startts,
                         const QDateTime &_endts) :
    positionMapDBReplacement(NULL)
{
    clear();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT chanid, channum, callsign, name, outputfilters, commmethod "
        "FROM channel "
        "WHERE chanid=:CHANID");
    query.bindValue(":CHANID", _chanid);
    if (query.exec() && query.next())
    {
        chanstr  = query.value(1).toString();
        chansign = query.value(2).toString();
        channame = query.value(3).toString();
        chanplaybackfilters = query.value(4).toString();
        set_flag(programflags, FL_CHANCOMMFREE,
                 query.value(5).toInt() == COMM_DETECT_COMMFREE);
    }

    chanid  = _chanid;
    startts = _startts;
    endts   = _endts;

    title = _title;
    if (title.isEmpty())
    {
        QString channelFormat =
            gCoreContext->GetSetting("ChannelFormat", "<num> <sign>");
        QString timeformat =
            gCoreContext->GetSetting("TimeFormat", "h:mm AP");

        title = QString("%1 - %2").arg(ChannelText(channelFormat))
            .arg(startts.toString(timeformat));
    }

    description = title =
        QString("%1 (%2)").arg(title).arg(QObject::tr("Manual Record"));
}

ProgramInfo::ProgramInfo(
    const QString   &_title,   const QString   &_category,
    const QDateTime &_startts, const QDateTime &_endts) :
    positionMapDBReplacement(NULL)
{
    clear();
    title    = _title;
    category = _category;
    startts  = _startts;
    endts    = _endts;
}


/** \fn ProgramInfo::operator=(const ProgramInfo &other)
 *  \brief Copies important fields from other ProgramInfo.
 */
ProgramInfo &ProgramInfo::operator=(const ProgramInfo &other)
{
    clone(other);
    return *this;
}

/// \brief Copies important fields from other ProgramInfo.
void ProgramInfo::clone(const ProgramInfo &other,
                        bool ignore_non_serialized_data)
{
    bool is_same =
        (chanid && recstartts.isValid() && startts.isValid() &&
         chanid == other.chanid && recstartts == other.recstartts &&
         startts == other.startts);

    title = other.title;
    subtitle = other.subtitle;
    description = other.description;
    category = other.category;

    chanid = other.chanid;
    chanstr = other.chanstr;
    chansign = other.chansign;
    channame = other.channame;
    chanplaybackfilters = other.chanplaybackfilters;

    recgroup = other.recgroup;
    playgroup = other.playgroup;

    if (!ignore_non_serialized_data || !is_same ||
        (GetBasename() != other.GetBasename()))
    {
        pathname = other.pathname;
    }

    hostname = other.hostname;
    storagegroup = other.storagegroup;

    seriesid = other.seriesid;
    programid = other.programid;
    catType = other.catType;

    recpriority = other.recpriority;

    filesize = other.filesize;

    startts = other.startts;
    endts = other.endts;
    recstartts = other.recstartts;
    recendts = other.recendts;

    stars = other.stars;

    year = other.year;
    originalAirDate = other.originalAirDate;
    lastmodified = other.lastmodified;
    lastInUseTime = QDateTime::currentDateTime().addSecs(-4 * 60 * 60);

    recstatus = other.recstatus;

    prefinput = other.prefinput;
    recpriority2 = other.recpriority2;
    recordid = other.recordid;
    parentid = other.parentid;

    rectype = other.rectype;
    dupin = other.dupin;
    dupmethod = other.dupmethod;

    sourceid = other.sourceid;
    inputid = other.inputid;
    cardid = other.cardid;

    findid = other.findid;
    programflags = other.programflags;
    properties = other.properties;

    if (!ignore_non_serialized_data)
    {
        spread = other.spread;
        startCol = other.startCol;
        sortTitle = other.sortTitle;
        availableStatus = other.availableStatus;

        inUseForWhat = other.inUseForWhat;
        positionMapDBReplacement = other.positionMapDBReplacement;
    }

    title.detach();
    subtitle.detach();
    description.detach();
    category.detach();

    chanstr.detach();
    chansign.detach();
    channame.detach();
    chanplaybackfilters.detach();

    recgroup.detach();
    playgroup.detach();

    pathname.detach();
    hostname.detach();
    storagegroup.detach();

    seriesid.detach();
    programid.detach();
    catType.detach();

    sortTitle.detach();
    inUseForWhat.detach();
}

void ProgramInfo::clear(void)
{
    title.clear();
    subtitle.clear();
    description.clear();
    category.clear();

    chanid = 0;
    chanstr.clear();
    chansign.clear();
    channame.clear();
    chanplaybackfilters.clear();

    recgroup = "Default";
    playgroup = "Default";

    pathname.clear();

    hostname.clear();
    storagegroup = "Default";

    year = 0;

    seriesid.clear();
    programid.clear();
    catType.clear();

    sortTitle.clear();

    recpriority = 0;

    filesize = 0ULL;

    startts = mythCurrentDateTime();
    endts = startts;
    recstartts = startts;
    recendts = startts;

    stars = 0.0f;

    originalAirDate = QDate();
    lastmodified = startts;
    lastInUseTime = startts.addSecs(-4 * 60 * 60);

    recstatus = rsUnknown;

    prefinput = 0;
    recpriority2 = 0;
    recordid = 0;
    parentid = 0;

    rectype = kNotRecording;
    dupin = kDupsInAll;
    dupmethod = kDupCheckSubDesc;

    sourceid = 0;
    inputid = 0;
    cardid = 0;

    findid = 0;

    programflags = FL_NONE;
    properties = 0;

    // everything below this line is not serialized
    spread = -1;
    startCol = -1;
    availableStatus = asAvailable;

    // Private
    inUseForWhat.clear();
    positionMapDBReplacement = NULL;
}

/** \fn ProgramInfo::~ProgramInfo()
 *  \brief Destructor deletes "record" if it exists.
 */
ProgramInfo::~ProgramInfo()
{
}

/// \brief Creates a unique string that can be used to identify a recording.
QString ProgramInfo::MakeUniqueKey(
    uint chanid, const QDateTime &recstartts)
{
    return QString("%1_%2").arg(chanid).arg(recstartts.toString(Qt::ISODate));
}

/// \brief Extracts chanid and recstartts from a unique key generated by
///        \ref MakeUniqueKey().
/// \return true iff a valid chanid and recstartts have been extracted.
bool ProgramInfo::ExtractKey(const QString &uniquekey,
                             uint &chanid, QDateTime &recstartts)
{
    QStringList keyParts = uniquekey.split('_');
    if (keyParts.size() != 2)
        return false;
    chanid     = keyParts[0].toUInt();
    recstartts = QDateTime::fromString(keyParts[1], Qt::ISODate);
    return chanid && recstartts.isValid();
}

bool ProgramInfo::ExtractKeyFromPathname(
    const QString &pathname, uint &chanid, QDateTime &recstartts)
{
    QString basename = pathname.section('/', -1);
    if (basename.isEmpty())
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT chanid, starttime "
        "FROM recorded "
        "WHERE basename = :BASENAME");
    query.bindValue(":BASENAME", basename);
    if (query.exec() && query.next())
    {
        chanid     = query.value(0).toUInt();
        recstartts = query.value(1).toDateTime();
        return true;
    }

    QStringList lr = basename.split("_");
    if (lr.size() == 2)
    {
        chanid = lr[0].toUInt();
        QStringList ts = lr[1].split(".");
        if (chanid && !ts.empty())
        {
            recstartts = myth_dt_from_string(ts[0]);
            return recstartts.isValid();
        }
    }

    return false;
}


#define INT_TO_LIST(x)       do { list << QString::number(x); } while (0)

#define DATETIME_TO_LIST(x)  INT_TO_LIST((x).toTime_t())

#define LONGLONG_TO_LIST(x)  do { list << QString::number(x); } while (0)

#define STR_TO_LIST(x)       do { list << (x); } while (0)
#define DATE_TO_LIST(x)      do { list << (x).toString(Qt::ISODate); } while (0)

#define FLOAT_TO_LIST(x)     do { list << QString("%1").arg(x); } while (0)

/** \fn ProgramInfo::ToStringList(QStringList&) const
 *  \brief Serializes ProgramInfo into a QStringList which can be passed
 *         over a socket.
 *  \sa FromStringList(QStringList::const_iterator&,
                       QStringList::const_iterator)
 */
void ProgramInfo::ToStringList(QStringList &list) const
{
    STR_TO_LIST(title);        // 0
    STR_TO_LIST(subtitle);     // 1
    STR_TO_LIST(description);  // 2
    STR_TO_LIST(category);     // 3
    INT_TO_LIST(chanid);       // 4
    STR_TO_LIST(chanstr);      // 5
    STR_TO_LIST(chansign);     // 6
    STR_TO_LIST(channame);     // 7
    STR_TO_LIST(pathname);     // 8
    INT_TO_LIST(filesize);     // 9

    DATETIME_TO_LIST(startts); // 10
    DATETIME_TO_LIST(endts);   // 11
    INT_TO_LIST(findid);       // 12
    STR_TO_LIST(hostname);     // 13
    INT_TO_LIST(sourceid);     // 14
    INT_TO_LIST(cardid);       // 15
    INT_TO_LIST(inputid);      // 16
    INT_TO_LIST(recpriority);  // 17
    INT_TO_LIST(recstatus);    // 18
    INT_TO_LIST(recordid);     // 19

    INT_TO_LIST(rectype);      // 20
    INT_TO_LIST(dupin);        // 21
    INT_TO_LIST(dupmethod);    // 22
    DATETIME_TO_LIST(recstartts);//23
    DATETIME_TO_LIST(recendts);// 24
    INT_TO_LIST(programflags); // 25
    STR_TO_LIST((!recgroup.isEmpty()) ? recgroup : "Default"); // 26
    STR_TO_LIST(chanplaybackfilters); // 27
    STR_TO_LIST(seriesid);     // 28
    STR_TO_LIST(programid);    // 29

    DATETIME_TO_LIST(lastmodified); // 30
    FLOAT_TO_LIST(stars);           // 31
    DATE_TO_LIST(originalAirDate);  // 32
    STR_TO_LIST((!playgroup.isEmpty()) ? playgroup : "Default"); // 33
    INT_TO_LIST(recpriority2);      // 34
    INT_TO_LIST(parentid);          // 35
    STR_TO_LIST((!storagegroup.isEmpty()) ? storagegroup : "Default"); // 36
    INT_TO_LIST(GetAudioProperties()); // 37
    INT_TO_LIST(GetVideoProperties()); // 38
    INT_TO_LIST(GetSubtitleType());    // 39

    INT_TO_LIST(year);              // 40
/* do not forget to update the NUMPROGRAMLINES defines! */
}

// QStringList::const_iterator it = list.begin()+offset;

#define NEXT_STR()        do { if (it == listend)                    \
                               {                                     \
                                   VERBOSE(VB_IMPORTANT, listerror); \
                                   clear();                          \
                                   return false;                     \
                               }                                     \
                               ts = *it++; } while (0)

#define INT_FROM_LIST(x)     do { NEXT_STR(); (x) = ts.toLongLong(); } while (0)
#define ENUM_FROM_LIST(x, y) do { NEXT_STR(); (x) = ((y)ts.toInt()); } while (0)

#define DATETIME_FROM_LIST(x) \
    do { NEXT_STR(); (x).setTime_t(ts.toUInt()); } while (0)
#define DATE_FROM_LIST(x) \
    do { NEXT_STR(); (x) = ((ts.isEmpty()) || (ts == "0000-00-00")) ? \
                         QDate() : QDate::fromString(ts, Qt::ISODate); \
    } while (0)

#define STR_FROM_LIST(x)     do { NEXT_STR(); (x) = ts; } while (0)

#define FLOAT_FROM_LIST(x)   do { NEXT_STR(); (x) = ts.toFloat(); } while (0)

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

    uint      origChanid     = chanid;
    QDateTime origRecstartts = recstartts;

    STR_FROM_LIST(title);            // 0
    STR_FROM_LIST(subtitle);         // 1
    STR_FROM_LIST(description);      // 2
    STR_FROM_LIST(category);         // 3
    INT_FROM_LIST(chanid);           // 4
    STR_FROM_LIST(chanstr);          // 5
    STR_FROM_LIST(chansign);         // 6
    STR_FROM_LIST(channame);         // 7
    STR_FROM_LIST(pathname);         // 8
    INT_FROM_LIST(filesize);         // 9

    DATETIME_FROM_LIST(startts);     // 10
    DATETIME_FROM_LIST(endts);       // 11
    INT_FROM_LIST(findid);           // 12
    STR_FROM_LIST(hostname);         // 13
    INT_FROM_LIST(sourceid);         // 14
    INT_FROM_LIST(cardid);           // 15
    INT_FROM_LIST(inputid);          // 16
    INT_FROM_LIST(recpriority);      // 17
    ENUM_FROM_LIST(recstatus, RecStatusType); // 18
    INT_FROM_LIST(recordid);         // 19

    ENUM_FROM_LIST(rectype, RecordingType);            // 20
    ENUM_FROM_LIST(dupin, RecordingDupInType);         // 21
    ENUM_FROM_LIST(dupmethod, RecordingDupMethodType); // 22
    DATETIME_FROM_LIST(recstartts);   // 23
    DATETIME_FROM_LIST(recendts);     // 24
    INT_FROM_LIST(programflags);      // 25
    STR_FROM_LIST(recgroup);          // 26
    STR_FROM_LIST(chanplaybackfilters);//27
    STR_FROM_LIST(seriesid);          // 28
    STR_FROM_LIST(programid);         // 29

    DATETIME_FROM_LIST(lastmodified); // 30
    FLOAT_FROM_LIST(stars);           // 31
    DATE_FROM_LIST(originalAirDate);; // 32
    STR_FROM_LIST(playgroup);         // 33
    INT_FROM_LIST(recpriority2);      // 34
    INT_FROM_LIST(parentid);          // 35
    STR_FROM_LIST(storagegroup);      // 36
    uint audioproperties, videoproperties, subtitleType;
    INT_FROM_LIST(audioproperties);   // 37
    INT_FROM_LIST(videoproperties);   // 38
    INT_FROM_LIST(subtitleType);      // 39
    properties = (subtitleType<<11) | (videoproperties<<6) | audioproperties;

    INT_FROM_LIST(year);              // 40

    if (!origChanid || !origRecstartts.isValid() ||
        (origChanid != chanid) || (origRecstartts != recstartts))
    {
        availableStatus = asAvailable;
        spread = -1;
        startCol = -1;
        sortTitle = QString();
        inUseForWhat = QString();
        positionMapDBReplacement = NULL;
    }

    return true;
}

/** \brief Converts ProgramInfo into QString QHash containing each field
 *         in ProgramInfo converted into localized strings.
 */
void ProgramInfo::ToMap(InfoMap &progMap,
                        bool showrerecord,
                        uint star_range) const
{
    // NOTE: Format changes and relevant additions made here should be
    //       reflected in RecordingRule
    QString timeFormat = gCoreContext->GetSetting("TimeFormat", "h:mm AP");
    QString dateFormat = gCoreContext->GetSetting("DateFormat", "ddd MMMM d");
    QString fullDateFormat = dateFormat;
    if (!fullDateFormat.contains("yyyy"))
        fullDateFormat += " yyyy";
    QString shortDateFormat = gCoreContext->GetSetting("ShortDateFormat", "M/d");
    QString channelFormat =
        gCoreContext->GetSetting("ChannelFormat", "<num> <sign>");
    QString longChannelFormat =
        gCoreContext->GetSetting("LongChannelFormat", "<num> <name>");

    QDateTime timeNow = QDateTime::currentDateTime();

    int hours, minutes, seconds;

    progMap["title"] = title;
    progMap["subtitle"] = subtitle;

    QString tempSubTitle = title;
    if (!subtitle.trimmed().isEmpty())
    {
        tempSubTitle = QString("%1 - \"%2\"")
            .arg(tempSubTitle).arg(subtitle);
    }

    progMap["titlesubtitle"] = tempSubTitle;

    progMap["description"] = description;
    progMap["category"] = category;
    progMap["callsign"] = chansign;
    progMap["commfree"] = (programflags & FL_CHANCOMMFREE) ? 1 : 0;
    progMap["outputfilters"] = chanplaybackfilters;
    if (IsVideo())
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
    else // if (IsRecording())
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

    progMap["timedate"] = recstartts.date().toString(dateFormat) + ", " +
                          recstartts.time().toString(timeFormat) + " - " +
                          recendts.time().toString(timeFormat);

    progMap["shorttimedate"] =
                          recstartts.date().toString(shortDateFormat) + ", " +
                          recstartts.time().toString(timeFormat) + " - " +
                          recendts.time().toString(timeFormat);

    progMap["starttimedate"] =
                          recstartts.date().toString(dateFormat) + ", " +
                          recstartts.time().toString(timeFormat);

    progMap["shortstarttimedate"] =
                          recstartts.date().toString(shortDateFormat) + " " +
                          recstartts.time().toString(timeFormat);

    progMap["lastmodifiedtime"] = lastmodified.toString(timeFormat);
    progMap["lastmodifieddate"] = lastmodified.toString(dateFormat);
    progMap["lastmodified"] = lastmodified.toString(dateFormat) + " " +
                              lastmodified.toString(timeFormat);

    progMap["channum"] = chanstr;
    progMap["chanid"] = chanid;
    progMap["channame"] = channame;
    progMap["channel"] = ChannelText(channelFormat);
    progMap["longchannel"] = ChannelText(longChannelFormat);

    QString tmpSize;

    tmpSize.sprintf("%0.2f ", filesize * (1.0 / (1024.0 * 1024.0 * 1024.0)));
    tmpSize += QObject::tr("GB", "GigaBytes");
    progMap["filesize_str"] = tmpSize;

    progMap["filesize"] = QString::number(filesize);

    seconds = recstartts.secsTo(recendts);
    minutes = seconds / 60;

    QString min_str = QObject::tr("%n minute(s)","",minutes);

    progMap["lenmins"] = min_str;
    hours   = minutes / 60;
    minutes = minutes % 60;

    progMap["lentime"] = min_str;
    if (hours > 0 && minutes > 0)
    {
        min_str = QObject::tr("%n minute(s)","",minutes);
        progMap["lentime"] = QString("%1 %2")
            .arg(QObject::tr("%n hour(s)","", hours))
            .arg(min_str);
    }
    else if (hours > 0)
    {
        progMap["lentime"] = QObject::tr("%n hour(s)","", hours);
    }

    progMap["rectypechar"] = toQChar(GetRecordingRuleType());
    progMap["rectype"] = ::toString(GetRecordingRuleType());
    QString tmp_rec = progMap["rectype"];
    if (GetRecordingRuleType() != kNotRecording)
    {
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
        if (showrerecord && (GetRecordingStatus() == rsRecorded) &&
            !IsDuplicate())
        {
            tmp_rec += QObject::tr("Re-Record");
        }
        else
        {
            tmp_rec += ::toString(GetRecordingStatus(), GetRecordingRuleType());
        }
    }
    progMap["rectypestatus"] = tmp_rec;

    progMap["card"] = toQChar(GetRecordingStatus(), cardid);

    progMap["recpriority"] = recpriority;
    progMap["recpriority2"] = recpriority2;
    progMap["recgroup"] = recgroup;
    progMap["playgroup"] = playgroup;
    progMap["storagegroup"] = storagegroup;
    progMap["programflags"] = programflags;

    progMap["audioproperties"] = GetAudioProperties();
    progMap["videoproperties"] = GetVideoProperties();
    progMap["subtitleType"] = GetSubtitleType();

    progMap["recstatus"] = ::toString(GetRecordingStatus(),
                                      GetRecordingRuleType());

    if (IsRepeat())
    {
        progMap["repeat"] = QString("(%1) ").arg(QObject::tr("Repeat"));
        progMap["longrepeat"] = progMap["repeat"];
        if (originalAirDate.isValid())
        {
            progMap["longrepeat"] = QString("(%1 %2) ")
                .arg(QObject::tr("Repeat"))
                .arg(originalAirDate.toString(fullDateFormat));
        }
    }
    else
    {
        progMap["repeat"] = "";
        progMap["longrepeat"] = "";
    }

    progMap["seriesid"] = seriesid;
    progMap["programid"] = programid;
    progMap["catType"] = catType;

    progMap["year"] = year ? QString::number(year) : "";

    QString star_str = (stars != 0.0f) ?
        QObject::tr("%n star(s)", "", GetStars(star_range)) : "";
    progMap["stars"] = star_str;
    progMap["numstars"] = QString().number(GetStars(star_range));

    if (stars != 0.0f && year)
        progMap["yearstars"] = QString("(%1, %2)").arg(year).arg(star_str);
    else if (stars != 0.0f)
        progMap["yearstars"] = QString("(%1)").arg(star_str);
    else if (year)
        progMap["yearstars"] = QString("(%1)").arg(year);
    else
        progMap["yearstars"] = "";

    if (!originalAirDate.isValid() ||
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

/// \brief Returns length of program/recording in seconds.
uint ProgramInfo::GetSecondsInRecording(void) const
{
    int recsecs = recstartts.secsTo(endts);
    return (uint) ((recsecs>0) ? recsecs : max(startts.secsTo(endts),0));
}


QString ProgramInfo::toString(const Verbosity v, QString sep, QString grp)
    const
{
    QString str;
    switch (v)
    {
        case kLongDescription:
            str = LOC + "channame(" + channame + ") startts(" +
                startts.toString() + ") endts(" + endts.toString() + ")\n";
            str += "             recstartts(" + recstartts.toString() +
                ") recendts(" + recendts.toString() + ")\n";
            str += "             title(" + title + ")";
            break;
        case kTitleSubtitle:
            str = title.contains(' ') ?
                QString("%1%2%3").arg(grp).arg(title).arg(grp) : title;
            if (!subtitle.isEmpty())
            {
                str += subtitle.contains(' ') ?
                    QString("%1%2%3%4").arg(sep)
                        .arg(grp).arg(subtitle).arg(grp) :
                    QString("%1%2").arg(sep).arg(subtitle);
            }
            break;
        case kRecordingKey:
            str = QString("%1 at %2")
                .arg(GetChanID()).arg(GetRecordingStartTime(ISODate));
            break;
        case kSchedulingKey:
            str = QString("%1 @ %2")
                .arg(GetChanID()).arg(GetScheduledStartTime(ISODate));
            break;
    }

    return str;
}

bool ProgramInfo::Reload(void)
{
    ProgramInfo test(chanid, recstartts);
    if (test.GetChanID())
    {
        clone(test, true);
        return true;
    }
    return false;
}

/** \brief Loads ProgramInfo for an existing recording.
 *  \return true iff sucessful
 */
bool ProgramInfo::LoadProgramFromRecorded(
    const uint _chanid, const QDateTime &_recstartts)
{
    if (!_chanid || !_recstartts.isValid())
    {
        clear();
        return false;
    }

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        kFromRecordedQuery +
        "WHERE r.chanid    = :CHANID AND "
        "      r.starttime = :RECSTARTTS");
    query.bindValue(":CHANID",     _chanid);
    query.bindValue(":RECSTARTTS", _recstartts);

    if (!query.exec())
    {
        MythDB::DBError("LoadProgramFromRecorded", query);
        clear();
        return false;
    }

    if (!query.next())
    {
        clear();
        return false;
    }

    bool is_reload = (chanid == _chanid) && (recstartts == _recstartts);
    if (!is_reload)
    {
        // These items are not initialized below so they need to be cleared
        // if we're loading in a different program into this ProgramInfo
        catType.clear();
        lastInUseTime = QDateTime::currentDateTime().addSecs(-4 * 60 * 60);
        rectype = kNotRecording;
        oldrecstatus = rsUnknown;
        prefinput = 0;
        recpriority2 = 0;
        parentid = 0;
        sourceid = 0;
        inputid = 0;
        cardid = 0;

        // everything below this line (in context) is not serialized
        spread = startCol = -1;
        sortTitle.clear();
        availableStatus = asAvailable;
        inUseForWhat.clear();
        positionMapDBReplacement = NULL;
    }

    title        = query.value(0).toString();
    subtitle     = query.value(1).toString();
    description  = query.value(2).toString();
    category     = query.value(3).toString();

    chanid       = _chanid;
    chanstr      = QString("#%1").arg(chanid);
    chansign     = chanstr;
    channame     = chanstr;
    chanplaybackfilters.clear();
    if (!query.value(5).toString().isEmpty())
    {
        chanstr  = query.value(5).toString();
        chansign = query.value(6).toString();
        channame = query.value(7).toString();
        chanplaybackfilters = query.value(8).toString();
    }

    recgroup     = query.value(9).toString();
    playgroup    = query.value(10).toString();

    // We don't want to update the pathname if the basename is
    // the same as we may have already expanded pathname from
    // a simple basename to a localized path.
    QString new_basename = query.value(12).toString();
    if ((GetBasename() != new_basename) || !is_reload)
    {
        if (is_reload)
        {
            VERBOSE(VB_FILE, LOC +
                    QString("Updated pathname '%1':'%2' -> '%3'")
                    .arg(pathname).arg(GetBasename()).arg(new_basename));
        }
        SetPathname(new_basename);
    }

    hostname     = query.value(13).toString();
    storagegroup = query.value(11).toString();

    seriesid     = query.value(15).toString();
    programid    = query.value(16).toString();
    /**///catType;

    recpriority  = query.value(14).toInt();

    filesize     = query.value(17).toULongLong();

    startts      = query.value(18).toDateTime();
    endts        = query.value(19).toDateTime();
    recstartts   = query.value(21).toDateTime();
    recendts     = query.value(22).toDateTime();

    stars        = clamp((float)query.value(20).toDouble(), 0.0f, 1.0f);

    year         = query.value(23).toUInt();
    originalAirDate = query.value(24).toDate();
    lastmodified = query.value(25).toDateTime();
    /**///lastInUseTime;

    recstatus    = rsRecorded;
    /**///oldrecstatus;

    /**///prefinput;
    /**///recpriority2;

    recordid     = query.value(26).toUInt();
    /**///parentid;

    /**///sourcid;
    /**///inputid;
    /**///cardid;
    findid       = query.value(42).toUInt();

    /**///rectype;
    dupin        = RecordingDupInType(query.value(43).toInt());
    dupmethod    = RecordingDupMethodType(query.value(44).toInt());

    // ancillary data -- begin
    set_flag(programflags, FL_CHANCOMMFREE,
             query.value(27).toInt() == COMM_DETECT_COMMFREE);
    set_flag(programflags, FL_COMMFLAG,
             query.value(28).toInt() == COMM_FLAG_DONE);
    set_flag(programflags, FL_COMMPROCESSING ,
             query.value(28).toInt() == COMM_FLAG_PROCESSING);
    set_flag(programflags, FL_REPEAT,        query.value(29).toBool());
    set_flag(programflags, FL_TRANSCODED,
             query.value(31).toInt() == TRANSCODING_COMPLETE);
    set_flag(programflags, FL_DELETEPENDING, query.value(32).toBool());
    set_flag(programflags, FL_PRESERVED,     query.value(33).toBool());
    set_flag(programflags, FL_CUTLIST,       query.value(34).toBool());
    set_flag(programflags, FL_AUTOEXP,       query.value(35).toBool());
    set_flag(programflags, FL_REALLYEDITING, query.value(36).toBool());
    set_flag(programflags, FL_BOOKMARK,      query.value(37).toBool());
    set_flag(programflags, FL_WATCHED,       query.value(38).toBool());
    set_flag(programflags, FL_EDITING,
             (programflags & FL_REALLYEDITING) ||
             (programflags & FL_COMMPROCESSING));

    properties = ((query.value(41).toUInt()<<11) |
                  (query.value(40).toUInt()<<6) |
                  query.value(39).toUInt());
    // ancillary data -- end

    if (originalAirDate.isValid() && originalAirDate < QDate(1940, 1, 1))
        originalAirDate = QDate();

    // Extra stuff which is not serialized and may get lost.
    /**/// spread
    /**/// startCol
    /**/// sortTitle
    /**/// availableStatus
    /**/// inUseForWhat
    /**/// postitionMapDBReplacement

    return true;
}

/** \fn ProgramInfo::GetRecordingTypeRecPriority(RecordingType)
 *  \brief Returns recording priority change needed due to RecordingType.
 */
int ProgramInfo::GetRecordingTypeRecPriority(RecordingType type)
{
    switch (type)
    {
        case kSingleRecord:
            return gCoreContext->GetNumSetting("SingleRecordRecPriority", 1);
        case kTimeslotRecord:
            return gCoreContext->GetNumSetting("TimeslotRecordRecPriority", 0);
        case kWeekslotRecord:
            return gCoreContext->GetNumSetting("WeekslotRecordRecPriority", 0);
        case kChannelRecord:
            return gCoreContext->GetNumSetting("ChannelRecordRecPriority", 0);
        case kAllRecord:
            return gCoreContext->GetNumSetting("AllRecordRecPriority", 0);
        case kFindOneRecord:
        case kFindDailyRecord:
        case kFindWeeklyRecord:
            return gCoreContext->GetNumSetting("FindOneRecordRecPriority", -1);
        case kOverrideRecord:
        case kDontRecord:
            return gCoreContext->GetNumSetting("OverrideRecordRecPriority", 0);
        default:
            return 0;
    }
}

/** \fn ProgramInfo::IsSameProgramWeakCheck(const ProgramInfo&) const
 *  \brief Checks for duplicate using only title, chanid and startts.
 *  \param other ProgramInfo to compare this one with.
 */
bool ProgramInfo::IsSameProgramWeakCheck(const ProgramInfo &other) const
{
    return (title == other.title &&
            chanid == other.chanid &&
            startts == other.startts);
}

/** \fn ProgramInfo::IsSameProgram(const ProgramInfo&) const
 *  \brief Checks for duplicates according to dupmethod.
 *  \param other ProgramInfo to compare this one with.
 */
bool ProgramInfo::IsSameProgram(const ProgramInfo& other) const
{
    if (GetRecordingRuleType() == kFindOneRecord)
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
        if (programid.endsWith("0000"))
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
    if (startts == other.startts &&
        (chanid == other.chanid ||
         (!chansign.isEmpty() && chansign == other.chansign)))
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
         (!chansign.isEmpty() && chansign == other.chansign)) &&
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

void ProgramInfo::SetPathname(const QString &pn) const
{
    pathname = pn;
    pathname.detach();

    ProgramInfoType pit = kProgramInfoTypeVideoFile;
    if (chanid)
        pit = kProgramInfoTypeRecording;
    else if (myth_FileIsDVD(pathname))
        pit = kProgramInfoTypeVideoDVD;
    else if (pathname.toLower().startsWith("http:"))
        pit = kProgramInfoTypeVideoStreamingHTML;
    else if (pathname.toLower().startsWith("rtsp:"))
        pit = kProgramInfoTypeVideoStreamingRTSP;
    else if (myth_FileIsBD(pathname))
        pit = kProgramInfoTypeVideoBD;
    const_cast<ProgramInfo*>(this)->SetProgramInfoType(pit);
}

void ProgramInfo::SetAvailableStatus(
    AvailableStatusType status, const QString &where)
{
    if (status != availableStatus)
    {
        VERBOSE(VB_GUI, toString(kTitleSubtitle) +
                QString(": %1 -> %2")
                .arg(::toString((AvailableStatusType)availableStatus))
                .arg(::toString(status)));
    }
    availableStatus = status;
}

/** \fn ProgramInfo::SaveBasename(const QString&)
 *  \brief Sets a recording's basename in the database.
 */
bool ProgramInfo::SaveBasename(const QString &basename)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE recorded "
                  "SET basename = :BASENAME "
                  "WHERE chanid = :CHANID AND "
                  "      starttime = :STARTTIME;");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);
    query.bindValue(":BASENAME", basename);

    if (!query.exec())
    {
        MythDB::DBError("SetRecordBasename", query);
        return false;
    }

    SetPathname(basename);

    SendUpdateEvent();
    return true;
}

/** Gets the basename, from the DB if necessary.
 *
 *  If the base part of pathname is not empty this will return
 *  that value otherwise this queries the recorded table in the
 *  DB for the basename stored there for this ProgramInfo's
 *  chanid and recstartts.
 */
QString ProgramInfo::QueryBasename(void) const
{
    QString bn = GetBasename();
    if (!bn.isEmpty())
        return bn;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT basename "
        "FROM recorded "
        "WHERE chanid    = :CHANID AND "
        "      starttime = :STARTTIME");
    query.bindValue(":CHANID",    chanid);
    query.bindValue(":STARTTIME", recstartts);

    if (!query.exec())
    {
        MythDB::DBError("QueryBasename", query);
    }
    else if (query.next())
    {
        return query.value(0).toString();
    }
    else
    {
        VERBOSE(VB_IMPORTANT, QString("QueryBasename found no entry "
                                      "for %1 @ %2")
                .arg(chanid).arg(recstartts.toString(Qt::ISODate)));
    }

    return QString();
}

/** \brief Returns filename or URL to be used to play back this recording.
 *         If the file is accessible locally, the filename will be returned,
 *         otherwise a myth:// URL will be returned.
 *
 *  \note This method sometimes initiates a QUERY_CHECKFILE MythProto
 *        call and so should not be called from the UI thread.
 */
QString ProgramInfo::GetPlaybackURL(
    bool checkMaster, bool forceCheckLocal) const
{
    QString tmpURL;
    QString basename = QueryBasename();

    if (basename.isEmpty())
        return "";

    bool alwaysStream = gCoreContext->GetNumSetting("AlwaysStreamFiles", 0);

    if ((!alwaysStream) || (forceCheckLocal))
    {
        // Check to see if the file exists locally
        StorageGroup sgroup(storagegroup);
        //VERBOSE(VB_FILE, LOC +QString("GetPlaybackURL: CHECKING SG : %1 : ").arg(tmpURL));
        tmpURL = sgroup.FindRecordingFile(basename);

        if (!tmpURL.isEmpty())
        {
            VERBOSE(VB_FILE, LOC +
                    QString("GetPlaybackURL: File is local: '%1'").arg(tmpURL));
            return tmpURL;
        }
        else if (hostname == gCoreContext->GetHostName())
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("GetPlaybackURL: '%1' should be local, but it can "
                            "not be found.").arg(basename));
            // Note do not preceed with "/" that will cause existing code
            // to look for a local file with this name...
            return QString("GetPlaybackURL/UNABLE/TO/FIND/LOCAL/FILE/ON/%1/%2")
                           .arg(hostname).arg(basename);
        }
    }

    // Check to see if we should stream from the master backend
    if ((checkMaster) &&
        (gCoreContext->GetNumSetting("MasterBackendOverride", 0)) &&
        (RemoteCheckFile(this, false)))
    {
        tmpURL = QString("myth://") +
        gCoreContext->GetSetting("MasterServerIP") + ':' +
        gCoreContext->GetSetting("MasterServerPort") + '/' + basename;
        VERBOSE(VB_FILE, LOC +
                QString("GetPlaybackURL: Found @ '%1'").arg(tmpURL));
        return tmpURL;
        }

    // Fallback to streaming from the backend the recording was created on
    tmpURL = QString("myth://") +
        gCoreContext->GetSettingOnHost("BackendServerIP", hostname) + ':' +
        gCoreContext->GetSettingOnHost("BackendServerPort", hostname) + '/' +
        basename;

    VERBOSE(VB_FILE, LOC + QString("GetPlaybackURL: Using default of: '%1'")
            .arg(tmpURL));

    return tmpURL;
}

/** \fn ProgramInfo::SaveFilesize(uint64_t)
 *  \brief Sets recording file size in database, and sets "filesize" field.
 */
void ProgramInfo::SaveFilesize(uint64_t fsize)
{
    SetFilesize(fsize);

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "UPDATE recorded "
        "SET filesize = :FILESIZE "
        "WHERE chanid    = :CHANID AND "
        "      starttime = :STARTTIME");
    query.bindValue(":FILESIZE",  (quint64)fsize);
    query.bindValue(":CHANID",    chanid);
    query.bindValue(":STARTTIME", recstartts);

    if (!query.exec())
        MythDB::DBError("File size update", query);

    updater->insert(chanid, recstartts, kPIUpdateFileSize, fsize);
}

/// \brief Gets recording file size from database.
uint64_t ProgramInfo::QueryFilesize(void) const
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(
        "SELECT filesize "
        "FROM recorded "
        "WHERE chanid    = :CHANID AND "
        "      starttime = :STARTTIME");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);
    if (query.exec() && query.next())
        return query.value(0).toULongLong();

    return filesize;
}

/** \brief Queries multiplex any recording would be made on, zero if unknown.
 */
uint ProgramInfo::QueryMplexID(void) const
{
    uint ret = 0U;
    if (chanid)
    {
        MSqlQuery query(MSqlQuery::InitCon());

        query.prepare("SELECT mplexid FROM channel "
                      "WHERE chanid = :CHANID");
        query.bindValue(":CHANID", chanid);

        if (!query.exec())
            MythDB::DBError("QueryMplexID", query);
        else if (query.next())
            ret = query.value(0).toUInt();

        // clear out bogus mplexid's
        ret = (32767 == ret) ? 0 : ret;
    }

    return ret;
}

/// \brief Clears any existing bookmark in DB and if frame
///        is greater than 0 sets a new bookmark.
void ProgramInfo::SaveBookmark(uint64_t frame)
{
    ClearMarkupMap(MARK_BOOKMARK);

    bool is_valid = (frame > 0);
    if (is_valid)
    {
        frm_dir_map_t bookmarkmap;
        bookmarkmap[frame] = MARK_BOOKMARK;
        SaveMarkupMap(bookmarkmap);
    }

    if (IsRecording())
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare(
            "UPDATE recorded "
            "SET bookmarkupdate = CURRENT_TIMESTAMP, "
            "    bookmark       = :BOOKMARKFLAG "
            "WHERE chanid    = :CHANID AND "
            "      starttime = :STARTTIME");

        query.bindValue(":BOOKMARKFLAG", is_valid);
        query.bindValue(":CHANID",       chanid);
        query.bindValue(":STARTTIME",    recstartts);

        if (!query.exec())
            MythDB::DBError("bookmark flag update", query);
    }

    set_flag(programflags, FL_BOOKMARK, is_valid);

    SendUpdateEvent();
}

void ProgramInfo::SendUpdateEvent(void)
{
    updater->insert(chanid, recstartts, kPIUpdate);
}

void ProgramInfo::SendAddedEvent(void) const
{
    updater->insert(chanid, recstartts, kPIAdd);
}

void ProgramInfo::SendDeletedEvent(void) const
{
    updater->insert(chanid, recstartts, kPIDelete);
}

/** \brief Queries Latest bookmark timestamp from the database.
 *  If the timestamp has not been set this returns an invalid QDateTime.
 */
QDateTime ProgramInfo::QueryBookmarkTimeStamp(void) const
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT bookmarkupdate "
        "FROM recorded "
        "WHERE chanid    = :CHANID AND"
        "      starttime = :STARTTIME");
    query.bindValue(":CHANID",    chanid);
    query.bindValue(":STARTTIME", recstartts);

    QDateTime ts;

    if (!query.exec())
        MythDB::DBError("ProgramInfo::GetBookmarkTimeStamp()", query);
    else if (query.next())
        ts = query.value(0).toDateTime();

    return ts;
}

/** \brief Gets any bookmark position in database,
 *         unless the ignore bookmark flag is set.
 *
 *  \return Bookmark position in frames if the query is executed
 *          and succeeds, zero otherwise.
 */
uint64_t ProgramInfo::QueryBookmark(void) const
{
    if (programflags & FL_IGNOREBOOKMARK)
        return 0;

    frm_dir_map_t bookmarkmap;
    QueryMarkupMap(bookmarkmap, MARK_BOOKMARK);

    return (bookmarkmap.isEmpty()) ? 0 : bookmarkmap.begin().key();
}

uint64_t ProgramInfo::QueryBookmark(uint chanid, const QDateTime &recstartts)
{
    frm_dir_map_t bookmarkmap;
    QueryMarkupMap(
        chanid, recstartts,
        bookmarkmap, MARK_BOOKMARK);

    return (bookmarkmap.isEmpty()) ? 0 : bookmarkmap.begin().key();
}

/** \brief Queries "dvdbookmark" table for bookmarking DVD serial
 *         number. Deletes old dvd bookmarks if "delbookmark" is set.
 *
 *  \return list containing title, audio track, subtitle, framenum
 */
QStringList ProgramInfo::QueryDVDBookmark(
    const QString &serialid, bool delbookmark) const
{
    QStringList fields = QStringList();
    MSqlQuery query(MSqlQuery::InitCon());

    if (!(programflags & FL_IGNOREBOOKMARK))
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
        int days = -(gCoreContext->GetNumSetting("DVDBookmarkDays", 10));
        QDateTime removedate = mythCurrentDateTime().addDays(days);
        query.prepare(" DELETE from dvdbookmark "
                        " WHERE timestamp < ? ");
        query.addBindValue(removedate.toString(Qt::ISODate));

        if (!query.exec())
            MythDB::DBError("GetDVDBookmark deleting old entries", query);
    }

    return fields;
}

void ProgramInfo::SaveDVDBookmark(const QStringList &fields) const
{
    QStringList::const_iterator it = fields.begin();
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

    if (!query.exec())
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

    if (!query.exec())
        MythDB::DBError("SetDVDBookmark updating", query);
}

/// \brief Set "watched" field in recorded/videometadata to "watchedFlag".
void ProgramInfo::SaveWatched(bool watched)
{
    if (IsRecording())
    {
        MSqlQuery query(MSqlQuery::InitCon());

        query.prepare("UPDATE recorded"
                    " SET watched = :WATCHEDFLAG"
                    " WHERE chanid = :CHANID"
                    " AND starttime = :STARTTIME ;");
        query.bindValue(":CHANID", chanid);
        query.bindValue(":STARTTIME", recstartts);
        query.bindValue(":WATCHEDFLAG", watched);

        if (!query.exec())
            MythDB::DBError("Set watched flag", query);
        else
            UpdateLastDelete(watched);
    }
    else if (IsVideoFile())
    {
        QString url = pathname;
        if (url.startsWith("myth://"))
        {
            url = QUrl(url).path();
            url.remove(0,1);
        }

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("UPDATE videometadata"
                    " SET watched = :WATCHEDFLAG"
                    " WHERE title = :TITLE"
                    " AND subtitle = :SUBTITLE"
                    " AND filename = :FILENAME ;");
        query.bindValue(":TITLE", title);
        query.bindValue(":SUBTITLE", subtitle);
        query.bindValue(":FILENAME", url);
        query.bindValue(":WATCHEDFLAG", watched);

        if (!query.exec())
            MythDB::DBError("Set watched flag", query);
    }

    set_flag(programflags, FL_WATCHED, watched);
    SendUpdateEvent();
}

/** \brief Queries "recorded" table for its "editing" field
 *         and returns true if it is set to true.
 *  \return true if we have started, but not finished, editing.
 */
bool ProgramInfo::QueryIsEditing(void) const
{
    bool editing = programflags & FL_REALLYEDITING;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT editing FROM recorded"
                  " WHERE chanid = :CHANID"
                  " AND starttime = :STARTTIME ;");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);

    if (query.exec() && query.next())
        editing = query.value(0).toBool();

    /*
    set_flag(programflags, FL_REALLYEDITING, editing);
    set_flag(programflags, FL_EDITING, ((programflags & FL_REALLYEDITING) ||
                                        (programflags & COMM_FLAG_PROCESSING)));
    */
    return editing;
}

/** \brief Sets "editing" field in "recorded" table to "edit"
 *  \param edit Editing state to set.
 */
void ProgramInfo::SaveEditing(bool edit)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("UPDATE recorded"
                  " SET editing = :EDIT"
                  " WHERE chanid = :CHANID"
                  " AND starttime = :STARTTIME ;");
    query.bindValue(":EDIT", edit);
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);

    if (!query.exec())
        MythDB::DBError("Edit status update", query);

    set_flag(programflags, FL_REALLYEDITING, edit);
    set_flag(programflags, FL_EDITING, ((programflags & FL_REALLYEDITING) ||
                                        (programflags & COMM_FLAG_PROCESSING)));

    SendUpdateEvent();
}

/** \brief Set "deletepending" field in "recorded" table to "deleteFlag".
 *  \param deleteFlag value to set delete pending field to.
 */
void ProgramInfo::SaveDeletePendingFlag(bool deleteFlag)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("UPDATE recorded"
                  " SET deletepending = :DELETEFLAG"
                  " WHERE chanid = :CHANID"
                  " AND starttime = :STARTTIME ;");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);
    query.bindValue(":DELETEFLAG", deleteFlag);

    if (!query.exec())
        MythDB::DBError("SaveDeletePendingFlag", query);

    set_flag(programflags, FL_DELETEPENDING, deleteFlag);

    if (!deleteFlag)
        SendAddedEvent();

    SendUpdateEvent();
}

/** \brief Returns true if Program is in use.  This is determined by
 *         the inuseprograms table which is updated automatically by
 *         NuppelVideoPlayer.
 */
bool ProgramInfo::QueryIsInUse(QStringList &byWho) const
{
    if (!IsRecording())
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

    byWho.clear();
    if (query.exec() && query.size() > 0)
    {
        QString usageStr, recusage;
        while (query.next())
        {
            usageStr = QObject::tr("Unknown");
            recusage = query.value(1).toString();

            if (recusage == kPlayerInUseID)
                usageStr = QObject::tr("Playing");
            else if (recusage == kPIPPlayerInUseID)
                usageStr = QObject::tr("PIP");
            else if (recusage == kPBPPlayerInUseID)
                usageStr = QObject::tr("PBP");
            else if ((recusage == kRecorderInUseID) ||
                     (recusage == kImportRecorderInUseID))
                usageStr = QObject::tr("Recording");
            else if (recusage == kFileTransferInUseID)
                usageStr = QObject::tr("File transfer");
            else if (recusage == kTruncatingDeleteInUseID)
                usageStr = QObject::tr("Delete");
            else if (recusage == kFlaggerInUseID)
                usageStr = QObject::tr("Commercial Detection");
            else if (recusage == kTranscoderInUseID)
                usageStr = QObject::tr("Transcoding");
            else if (recusage == kPreviewGeneratorInUseID)
                usageStr = QObject::tr("Preview Generation");
            else if (recusage == kJobQueueInUseID)
                usageStr = QObject::tr("User Job");

            byWho.push_back(recusage);
            byWho.push_back(query.value(0).toString());
            byWho.push_back(query.value(0).toString() + " (" + usageStr + ")");
        }

        return true;
    }

    return false;
}

/** \brief Returns true if Program is in use.  This is determined by
 *         the inuseprograms table which is updated automatically by
 *         NuppelVideoPlayer.
 */
bool ProgramInfo::QueryIsInUse(QString &byWho) const
{
    QStringList users;
    bool inuse = QueryIsInUse(users);
    byWho.clear();
    for (uint i = 0; i+2 < (uint)users.size(); i+=3)
        byWho += users[i+2] + "\n";
    return inuse;
}


/** \brief Returns true iff this is a recording, it is not in
 *         use (except by the recorder), and at most one player
 *         iff one_playback_allowed is set.
 *  \param one_playback_allowed iff true still returns true if there
 *         is one playback in progress and all other checks pass.
 */
bool ProgramInfo::QueryIsDeleteCandidate(bool one_playback_allowed) const
{
    if (!IsRecording())
        return false;

    bool ok = true;
    QStringList byWho;
    if (QueryIsInUse(byWho) && !byWho.isEmpty())
    {
        uint play_cnt = 0, ft_cnt = 0, jq_cnt = 0;
        for (uint i = 0; (i+2 < (uint)byWho.size()) && ok; i+=3)
        {
            play_cnt += byWho[i].contains(kPlayerInUseID) ? 1 : 0;
            ft_cnt += (byWho[i].contains(kFlaggerInUseID) ||
                       byWho[i].contains(kTranscoderInUseID)) ? 1 : 0;
            jq_cnt += (byWho[i].contains(kJobQueueInUseID)) ? 1 : 0;
            ok = ok && (byWho[i].contains(kRecorderInUseID)   ||
                        byWho[i].contains(kFlaggerInUseID)    ||
                        byWho[i].contains(kTranscoderInUseID) ||
                        byWho[i].contains(kJobQueueInUseID)   ||
                        (one_playback_allowed && (play_cnt <= 1)));
        }
        ok = ok && (ft_cnt == jq_cnt);
    }

    return ok;
}

/// \brief Returns the "transcoded" field in "recorded" table.
TranscodingStatus ProgramInfo::QueryTranscodeStatus(void) const
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT transcoded FROM recorded"
                 " WHERE chanid = :CHANID"
                 " AND starttime = :STARTTIME ;");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);

    if (query.exec() && query.next())
        return (TranscodingStatus) query.value(0).toUInt();
    return TRANSCODING_NOT_TRANSCODED;
}

/** \brief Set "transcoded" field in "recorded" table to "trans".
 *  \note Also sets the FL_TRANSCODED flag if the status is
 *        TRASCODING_COMPLETE and clears it otherwise.
 *  \param transFlag value to set transcoded field to.
 */
void ProgramInfo::SaveTranscodeStatus(TranscodingStatus trans)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(
        "UPDATE recorded "
        "SET transcoded = :VALUE "
        "WHERE chanid    = :CHANID AND"
        "      starttime = :STARTTIME");
    query.bindValue(":VALUE", (uint)trans);
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);

    if (!query.exec())
        MythDB::DBError("Transcoded status update", query);

    set_flag(programflags, FL_TRANSCODED, TRANSCODING_COMPLETE == trans);
    SendUpdateEvent();
}

/** \brief Set "commflagged" field in "recorded" table to "flag".
 *  \param flag value to set commercial flagging field to.
 */
void ProgramInfo::SaveCommFlagged(CommFlagStatus flag)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("UPDATE recorded"
                  " SET commflagged = :FLAG"
                  " WHERE chanid = :CHANID"
                  " AND starttime = :STARTTIME ;");
    query.bindValue(":FLAG", (int)flag);
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);

    if (!query.exec())
        MythDB::DBError("Commercial Flagged status update", query);

    set_flag(programflags, FL_COMMFLAG,       COMM_FLAG_DONE == flag);
    set_flag(programflags, FL_COMMPROCESSING, COMM_FLAG_PROCESSING == flag);
    set_flag(programflags, FL_EDITING, ((programflags & FL_REALLYEDITING) ||
                                        (programflags & COMM_FLAG_PROCESSING)));
    SendUpdateEvent();
}


/** \brief Set "preserve" field in "recorded" table to "preserveEpisode".
 *  \param preserveEpisode value to set preserve field to.
 */
void ProgramInfo::SavePreserve(bool preserveEpisode)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("UPDATE recorded"
                  " SET preserve = :PRESERVE"
                  " WHERE chanid = :CHANID"
                  " AND starttime = :STARTTIME ;");
    query.bindValue(":PRESERVE", preserveEpisode);
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);

    if (!query.exec())
        MythDB::DBError("PreserveEpisode update", query);
    else
        UpdateLastDelete(false);

    set_flag(programflags, FL_PRESERVED, preserveEpisode);

    SendUpdateEvent();
}

/**
 *  \brief Set "autoexpire" field in "recorded" table to "autoExpire".
 *  \param autoExpire value to set auto expire field to.
 *  \param updateDelete iff true, call UpdateLastDelete(true)
 */
void ProgramInfo::SaveAutoExpire(AutoExpireType autoExpire, bool updateDelete)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("UPDATE recorded"
                  " SET autoexpire = :AUTOEXPIRE"
                  " WHERE chanid = :CHANID"
                  " AND starttime = :STARTTIME ;");
    query.bindValue(":AUTOEXPIRE", (uint)autoExpire);
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);

    if (!query.exec())
        MythDB::DBError("AutoExpire update", query);
    else if (updateDelete)
        UpdateLastDelete(true);

    set_flag(programflags, FL_AUTOEXP, (uint)autoExpire);

    SendUpdateEvent();
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
    }
    else
    {
        query.prepare("UPDATE record SET last_delete = '0000-00-00 00:00:00' "
                      "WHERE recordid = :RECORDID");
    }
    query.bindValue(":RECORDID", recordid);

    if (!query.exec())
        MythDB::DBError("Update last_delete", query);
}

/// \brief Returns "autoexpire" field from "recorded" table.
AutoExpireType ProgramInfo::QueryAutoExpire(void) const
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT autoexpire FROM recorded"
                  " WHERE chanid = :CHANID"
                  " AND starttime = :STARTTIME ;");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);

    if (query.exec() && query.next())
        return (AutoExpireType) query.value(0).toInt();

    return kDisableAutoExpire;
}

void ProgramInfo::QueryCutList(frm_dir_map_t &delMap) const
{
    QueryMarkupMap(delMap, MARK_CUT_START);
    QueryMarkupMap(delMap, MARK_CUT_END, true);
    QueryMarkupMap(delMap, MARK_PLACEHOLDER, true);
}

void ProgramInfo::SaveCutList(frm_dir_map_t &delMap) const
{
    ClearMarkupMap(MARK_CUT_START);
    ClearMarkupMap(MARK_CUT_END);
    ClearMarkupMap(MARK_PLACEHOLDER);
    SaveMarkupMap(delMap);

    if (IsRecording())
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

        if (!query.exec())
            MythDB::DBError("cutlist flag update", query);
    }
}

void ProgramInfo::SaveCommBreakList(frm_dir_map_t &frames) const
{
    ClearMarkupMap(MARK_COMM_START);
    ClearMarkupMap(MARK_COMM_END);
    SaveMarkupMap(frames);
}

void ProgramInfo::QueryCommBreakList(frm_dir_map_t &frames) const
{
    QueryMarkupMap(frames, MARK_COMM_START);
    QueryMarkupMap(frames, MARK_COMM_END, true);
}

void ProgramInfo::ClearMarkupMap(
    MarkTypes type, int64_t min_frame, int64_t max_frame) const
{
    MSqlQuery query(MSqlQuery::InitCon());
    QString comp;

    if (min_frame >= 0)
        comp += QString(" AND mark >= %1 ").arg(min_frame);

    if (max_frame >= 0)
        comp += QString(" AND mark <= %1 ").arg(max_frame);

    if (type != MARK_ALL)
        comp += QString(" AND type = :TYPE ");

    if (IsVideo())
    {
        query.prepare("DELETE FROM filemarkup"
                      " WHERE filename = :PATH "
                      + comp + ";");
        query.bindValue(":PATH", StorageGroup::GetRelativePathname(pathname));
    }
    else if (IsRecording())
    {
        query.prepare("DELETE FROM recordedmarkup"
                      " WHERE chanid = :CHANID"
                      " AND STARTTIME = :STARTTIME"
                      + comp + ';');
        query.bindValue(":CHANID", chanid);
        query.bindValue(":STARTTIME", recstartts);
    }
    else
    {
        return;
    }
    query.bindValue(":TYPE", type);

    if (!query.exec())
        MythDB::DBError("ClearMarkupMap deleting", query);
}

void ProgramInfo::SaveMarkupMap(
    const frm_dir_map_t &marks, MarkTypes type,
    int64_t min_frame, int64_t max_frame) const
{
    MSqlQuery query(MSqlQuery::InitCon());
    QString videoPath;

    if (IsVideo())
    {
       videoPath = StorageGroup::GetRelativePathname(pathname);
    }
    else if (IsRecording())
    {
        // check to make sure the show still exists before saving markups
        query.prepare("SELECT starttime FROM recorded"
                      " WHERE chanid = :CHANID"
                      " AND starttime = :STARTTIME ;");
        query.bindValue(":CHANID", chanid);
        query.bindValue(":STARTTIME", recstartts);

        if (!query.exec())
            MythDB::DBError("SaveMarkupMap checking record table", query);

        if (!query.next())
            return;
    }
    else
    {
        return;
    }

    frm_dir_map_t::const_iterator it;
    for (it = marks.begin(); it != marks.end(); ++it)
    {
        uint64_t frame = it.key();
        int mark_type;
        QString querystr;

        if ((min_frame >= 0) && (frame < (uint64_t)min_frame))
            continue;

        if ((max_frame >= 0) && (frame > (uint64_t)max_frame))
            continue;

        mark_type = (type != MARK_ALL) ? type : *it;

        if (IsVideo())
        {
            query.prepare("INSERT INTO filemarkup (filename, mark, type)"
                          " VALUES ( :PATH , :MARK , :TYPE );");
            query.bindValue(":PATH", videoPath);
        }
        else // if (IsRecording())
        {
            query.prepare("INSERT INTO recordedmarkup"
                          " (chanid, starttime, mark, type)"
                          " VALUES ( :CHANID , :STARTTIME , :MARK , :TYPE );");
            query.bindValue(":CHANID", chanid);
            query.bindValue(":STARTTIME", recstartts);
        }
        query.bindValue(":MARK", (quint64)frame);
        query.bindValue(":TYPE", mark_type);

        if (!query.exec())
            MythDB::DBError("SaveMarkupMap inserting", query);
    }
}

void ProgramInfo::QueryMarkupMap(
    frm_dir_map_t &marks, MarkTypes type, bool merge) const
{
    if (!merge)
        marks.clear();

    if (IsVideo())
    {
        QueryMarkupMap(StorageGroup::GetRelativePathname(pathname),
                       marks, type, merge);
    }
    else if (IsRecording())
    {
        QueryMarkupMap(chanid, recstartts, marks, type, merge);
    }
}

void ProgramInfo::QueryMarkupMap(
    const QString &video_pathname,
    frm_dir_map_t &marks,
    MarkTypes type, bool mergeIntoMap)
{
    if (!mergeIntoMap)
        marks.clear();

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT mark, type "
                  "FROM filemarkup "
                  "WHERE filename = :PATH AND "
                  "      type     = :TYPE "
                  "ORDER BY mark");
    query.bindValue(":PATH", video_pathname);
    query.bindValue(":TYPE", type);

    if (!query.exec())
    {
        MythDB::DBError("QueryMarkupMap", query);
        return;
    }

    while (query.next())
    {
        marks[query.value(0).toLongLong()] =
            (MarkTypes) query.value(1).toInt();
    }
}

void ProgramInfo::QueryMarkupMap(
    uint chanid, const QDateTime &recstartts,
    frm_dir_map_t &marks,
    MarkTypes type, bool mergeIntoMap)
{
    if (!mergeIntoMap)
        marks.clear();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT mark, type "
                  "FROM recordedmarkup "
                  "WHERE chanid    = :CHANID AND "
                  "      starttime = :STARTTIME AND"
                  "      type      = :TYPE "
                  "ORDER BY mark");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);
    query.bindValue(":TYPE", type);

    if (!query.exec())
    {
        MythDB::DBError("QueryMarkupMap", query);
        return;
    }

    while (query.next())
    {
        marks[query.value(0).toULongLong()] =
            (MarkTypes) query.value(1).toInt();
    }
}

/// Returns true iff the speficied mark type is set on frame 0
bool ProgramInfo::QueryMarkupFlag(MarkTypes type) const
{
    frm_dir_map_t flagMap;

    QueryMarkupMap(flagMap, type);

    return flagMap.contains(0);
}

/// Clears the specified flag, then if sets it
void ProgramInfo::SaveMarkupFlag(MarkTypes type) const
{
    ClearMarkupMap(type);
    frm_dir_map_t flagMap;
    flagMap[0] = type;
    SaveMarkupMap(flagMap, type);
}

void ProgramInfo::QueryPositionMap(
    frm_pos_map_t &posMap, MarkTypes type) const
{
    if (positionMapDBReplacement)
    {
        QMutexLocker locker(positionMapDBReplacement->lock);
        posMap = positionMapDBReplacement->map[(MarkTypes)type];

        return;
    }

    posMap.clear();
    MSqlQuery query(MSqlQuery::InitCon());

    if (IsVideo())
    {
        query.prepare("SELECT mark, offset FROM filemarkup"
                      " WHERE filename = :PATH"
                      " AND type = :TYPE ;");
        query.bindValue(":PATH", StorageGroup::GetRelativePathname(pathname));
    }
    else if (IsRecording())
    {
        query.prepare("SELECT mark, offset FROM recordedseek"
                      " WHERE chanid = :CHANID"
                      " AND starttime = :STARTTIME"
                      " AND type = :TYPE ;");
        query.bindValue(":CHANID", chanid);
        query.bindValue(":STARTTIME", recstartts);
    }
    else
    {
        return;
    }
    query.bindValue(":TYPE", type);

    if (!query.exec())
    {
        MythDB::DBError("QueryPositionMap", query);
        return;
    }

    while (query.next())
        posMap[query.value(0).toULongLong()] = query.value(1).toULongLong();
}

void ProgramInfo::ClearPositionMap(MarkTypes type) const
{
    if (positionMapDBReplacement)
    {
        QMutexLocker locker(positionMapDBReplacement->lock);
        positionMapDBReplacement->map.clear();
        return;
    }

    MSqlQuery query(MSqlQuery::InitCon());

    if (IsVideo())
    {
        query.prepare("DELETE FROM filemarkup"
                      " WHERE filename = :PATH"
                      " AND type = :TYPE ;");
        query.bindValue(":PATH", StorageGroup::GetRelativePathname(pathname));
    }
    else if (IsRecording())
    {
        query.prepare("DELETE FROM recordedseek"
                      " WHERE chanid = :CHANID"
                      " AND starttime = :STARTTIME"
                      " AND type = :TYPE ;");
        query.bindValue(":CHANID", chanid);
        query.bindValue(":STARTTIME", recstartts);
    }
    else
    {
        return;
    }

    query.bindValue(":TYPE", type);

    if (!query.exec())
        MythDB::DBError("clear position map", query);
}

void ProgramInfo::SavePositionMap(
    frm_pos_map_t &posMap, MarkTypes type,
    int64_t min_frame, int64_t max_frame) const
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
                uint64_t frame = it.key();
                if ((min_frame >= 0) && (frame >= (uint64_t)min_frame))
                    continue;
                if ((min_frame >= 0) && (frame <= (uint64_t)max_frame))
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
            uint64_t frame = it.key();
            if ((min_frame >= 0) && (frame >= (uint64_t)min_frame))
                continue;
            if ((min_frame >= 0) && (frame <= (uint64_t)max_frame))
                continue;

            positionMapDBReplacement->map[(MarkTypes)type]
                .insert(frame, *it);
        }

        return;
    }

    MSqlQuery query(MSqlQuery::InitCon());
    QString comp;

    if (min_frame >= 0)
        comp += " AND mark >= :MIN_FRAME ";
    if (max_frame >= 0)
        comp += " AND mark <= :MAX_FRAME ";

    QString videoPath;
    if (IsVideo())
    {
        videoPath = StorageGroup::GetRelativePathname(pathname);

        query.prepare("DELETE FROM filemarkup"
                      " WHERE filename = :PATH"
                      " AND type = :TYPE"
                      + comp + ';');
        query.bindValue(":PATH", videoPath);
    }
    else if (IsRecording())
    {
        query.prepare("DELETE FROM recordedseek"
                      " WHERE chanid = :CHANID"
                      " AND starttime = :STARTTIME"
                      " AND type = :TYPE"
                      + comp + ';');
        query.bindValue(":CHANID", chanid);
        query.bindValue(":STARTTIME", recstartts);
    }
    else
    {
        return;
    }

    query.bindValue(":TYPE", type);
    if (min_frame >= 0)
        query.bindValue(":MIN_FRAME", (quint64)min_frame);
    if (max_frame >= 0)
        query.bindValue(":MAX_FRAME", (quint64)max_frame);

    if (!query.exec())
        MythDB::DBError("position map clear", query);

    if (IsVideo())
    {
        query.prepare(
            "INSERT INTO "
            "filemarkup (filename, mark, type, offset) "
            "VALUES ( :PATH , :MARK , :TYPE , :OFFSET )");
        query.bindValue(":PATH", videoPath);
    }
    else // if (IsRecording())
    {
        query.prepare(
            "INSERT INTO "
            "recordedseek (chanid, starttime, mark, type, offset) "
            " VALUES ( :CHANID , :STARTTIME , :MARK , :TYPE , :OFFSET )");
        query.bindValue(":CHANID",    chanid);
        query.bindValue(":STARTTIME", recstartts);
    }
    query.bindValue(":TYPE", type);

    frm_pos_map_t::iterator it;
    for (it = posMap.begin(); it != posMap.end(); ++it)
    {
        uint64_t frame = it.key();

        if ((min_frame >= 0) && (frame < (uint64_t)min_frame))
            continue;

        if ((max_frame >= 0) && (frame > (uint64_t)max_frame))
            continue;

        uint64_t offset = *it;

        query.bindValue(":MARK",   (quint64)frame);
        query.bindValue(":OFFSET", (quint64)offset);

        if (!query.exec())
        {
            MythDB::DBError("position map insert", query);
            break;
        }
    }
}

void ProgramInfo::SavePositionMapDelta(
    frm_pos_map_t &posMap, MarkTypes type) const
{
    if (positionMapDBReplacement)
    {
        QMutexLocker locker(positionMapDBReplacement->lock);

        frm_pos_map_t::const_iterator it     = posMap.begin();
        frm_pos_map_t::const_iterator it_end = posMap.end();
        for (; it != it_end; ++it)
            positionMapDBReplacement->map[type].insert(it.key(), *it);

        return;
    }

    MSqlQuery query(MSqlQuery::InitCon());

    if (IsVideo())
    {
        query.prepare(
            "INSERT INTO "
            "filemarkup (filename, mark, type, offset) "
            "VALUES ( :PATH , :MARK , :TYPE , :OFFSET )");
        query.bindValue(":PATH", StorageGroup::GetRelativePathname(pathname));
    }
    else if (IsRecording())
    {
        query.prepare(
            "INSERT INTO "
            "recordedseek (chanid, starttime, mark, type, offset) "
            " VALUES ( :CHANID , :STARTTIME , :MARK , :TYPE , :OFFSET )");
        query.bindValue(":CHANID",    chanid);
        query.bindValue(":STARTTIME", recstartts);
    }
    else
    {
        return;
    }
    query.bindValue(":TYPE", type);

    frm_pos_map_t::iterator it;
    for (it = posMap.begin(); it != posMap.end(); ++it)
    {
        uint64_t frame  = it.key();
        uint64_t offset = *it;

        query.bindValue(":MARK",   (quint64)frame);
        query.bindValue(":OFFSET", (quint64)offset);

        if (!query.exec())
        {
            MythDB::DBError("delta position map insert", query);
            break;
        }
    }
}

/// \brief Store aspect ratio of a frame in the recordedmark table
/// \note  All frames until the next one with a stored aspect ratio
///        are assumed to have the same aspect ratio
void ProgramInfo::SaveAspect(
    uint64_t frame, MarkTypes type, uint customAspect)
{
    if (!IsRecording())
        return;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("INSERT INTO recordedmarkup"
                    " (chanid, starttime, mark, type, data)"
                    " VALUES"
                    " ( :CHANID, :STARTTIME, :MARK, :TYPE, :DATA);");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);

    query.bindValue(":MARK", (quint64)frame);
    query.bindValue(":TYPE", type);

    if (type == MARK_ASPECT_CUSTOM)
        query.bindValue(":DATA", customAspect);
    else
        query.bindValue(":DATA", QVariant::UInt);

    if (!query.exec())
        MythDB::DBError("aspect ratio change insert", query);
}

/// \brief Store the Frame Rate at frame in the recordedmarkup table
/// \note  All frames until the next one with a stored frame rate
///        are assumed to have the same frame rate
void ProgramInfo::SaveFrameRate(uint64_t frame, uint framerate)
{
    if (!IsRecording())
        return;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("INSERT INTO recordedmarkup"
                  "    (chanid, starttime, mark, type, data)"
                  "    VALUES"
                  " ( :CHANID, :STARTTIME, :MARK, :TYPE, :DATA);");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);
    query.bindValue(":MARK", (quint64)frame);
    query.bindValue(":TYPE", MARK_VIDEO_RATE);
    query.bindValue(":DATA", framerate);

    if (!query.exec())
        MythDB::DBError("Frame rate insert", query);
}


/// \brief Store the Resolution at frame in the recordedmarkup table
/// \note  All frames until the next one with a stored resolution
///        are assumed to have the same resolution
void ProgramInfo::SaveResolution(uint64_t frame, uint width, uint height)
{
    if (!IsRecording())
        return;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("INSERT INTO recordedmarkup"
                  "    (chanid, starttime, mark, type, data)"
                  "    VALUES"
                  " ( :CHANID, :STARTTIME, :MARK, :TYPE, :DATA);");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);
    query.bindValue(":MARK", (quint64)frame);
    query.bindValue(":TYPE", MARK_VIDEO_WIDTH);
    query.bindValue(":DATA", width);

    if (!query.exec())
        MythDB::DBError("Resolution insert", query);

    query.prepare("INSERT INTO recordedmarkup"
                  "    (chanid, starttime, mark, type, data)"
                  "    VALUES"
                  " ( :CHANID, :STARTTIME, :MARK, :TYPE, :DATA);");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);
    query.bindValue(":MARK", (quint64)frame);
    query.bindValue(":TYPE", MARK_VIDEO_HEIGHT);
    query.bindValue(":DATA", height);

    if (!query.exec())
        MythDB::DBError("Resolution insert", query);
}

static uint load_markup_datum(
    MarkTypes type, uint chanid, const QDateTime &recstartts)
{
    QString qstr = QString(
        "SELECT recordedmarkup.data "
        "FROM recordedmarkup "
        "WHERE recordedmarkup.chanid    = :CHANID    AND "
        "      recordedmarkup.starttime = :STARTTIME AND "
        "      recordedmarkup.type      = %1 "
        "GROUP BY recordedmarkup.data "
        "ORDER BY SUM( ( SELECT IFNULL(rm.mark, recordedmarkup.mark)"
        "                FROM recordedmarkup AS rm "
        "                WHERE rm.chanid    = recordedmarkup.chanid    AND "
        "                      rm.starttime = recordedmarkup.starttime AND "
        "                      rm.type      = recordedmarkup.type      AND "
        "                      rm.mark      > recordedmarkup.mark "
        "                ORDER BY rm.mark ASC LIMIT 1 "
        "              ) - recordedmarkup.mark "
        "            ) DESC "
        "LIMIT 1").arg((int)type);

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(qstr);
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);

    if (!query.exec())
    {
        MythDB::DBError("load_markup_datum", query);
        return 0;
    }

    return (query.next()) ? query.value(0).toUInt() : 0;
}

/** \brief If present in recording this loads average height of the
 *         main video stream from database's stream markup table.
 *  \note Saves loaded value for future reference by GetHeight().
 */
uint ProgramInfo::QueryAverageHeight(void) const
{
    return load_markup_datum(MARK_VIDEO_HEIGHT, chanid, recstartts);
}

/** \brief If present in recording this loads average width of the
 *         main video stream from database's stream markup table.
 *  \note Saves loaded value for future reference by GetWidth().
 */
uint ProgramInfo::QueryAverageWidth(void) const
{
    return load_markup_datum(MARK_VIDEO_WIDTH, chanid, recstartts);
}

/** \brief If present in recording this loads average frame rate of the
 *         main video stream from database's stream markup table.
 *  \note Saves loaded value for future reference by GetFrameRate().
 */
uint ProgramInfo::QueryAverageFrameRate(void) const
{
    return load_markup_datum(MARK_VIDEO_RATE, chanid, recstartts);
}

void ProgramInfo::SaveResolutionProperty(VideoProperty vid_flags)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(
        "UPDATE recordedprogram "
        "SET videoprop = ((videoprop+0) & :OTHERFLAGS) | :FLAGS "
        "WHERE chanid = :CHANID AND starttime = :STARTTIME");

    query.bindValue(":OTHERFLAGS", ~(VID_1080|VID_720));
    query.bindValue(":FLAGS",      vid_flags);
    query.bindValue(":CHANID",     chanid);
    query.bindValue(":STARTTIME",  startts);
    query.exec();

    uint videoproperties = GetVideoProperties();
    videoproperties &= (uint16_t) ~(VID_1080|VID_720);
    videoproperties |= (uint16_t) vid_flags;
    properties &= ~(0x1F<<6);
    properties |= videoproperties<<6;

    SendUpdateEvent();
}

/** \fn ProgramInfo::ChannelText(const QString&) const
 *  \brief Returns channel info using "format".
 *
 *   There are three tags in "format" that will be replaced
 *   with the appropriate info. These tags are "<num>", "<sign>",
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

void ProgramInfo::UpdateInUseMark(bool force)
{
#ifdef DEBUG_IN_USE
    VERBOSE(VB_IMPORTANT, LOC + QString("UpdateInUseMark(%1) '%2'")
            .arg(force?"force":"no force").arg(inUseForWhat));
#endif

    if (!IsRecording())
        return;

    if (inUseForWhat.isEmpty())
        return;

    if (force || lastInUseTime.secsTo(QDateTime::currentDateTime()) > 15 * 60)
        MarkAsInUse(true);
}

/** \brief Attempts to ascertain if the main file for this ProgramInfo
 *         is readable.
 *  \note This method often initiates a QUERY_CHECKFILE MythProto
 *        call and so should not be called from the UI thread.
 *  \return true iff file is readable
 */
bool ProgramInfo::IsFileReadable(void) const
{
    if (IsLocal() && QFileInfo(pathname).isReadable())
        return true;

    if (!IsMythStream())
        pathname = GetPlaybackURL(true, false);

    if (IsMythStream())
        return RemoteCheckFile(this);

    if (IsLocal())
        return QFileInfo(pathname).isReadable();

    return false;
}

QString ProgramInfo::QueryRecordingGroupPassword(const QString &group)
{
    QString result;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT password FROM recgrouppassword "
                    "WHERE recgroup = :GROUP");
    query.bindValue(":GROUP", group);

    if (query.exec() && query.next())
        result = query.value(0).toString();

    return(result);
}

/** \brief Query recgroup from recorded
 */
QString ProgramInfo::QueryRecordingGroup(void) const
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT recgroup FROM recorded "
                  "WHERE chanid    = :CHANID AND "
                  "      starttime = :START");
    query.bindValue(":START", recstartts);
    query.bindValue(":CHANID", chanid);

    QString grp = recgroup;
    if (query.exec() && query.next())
        grp = query.value(0).toString();

    return grp;
}

uint ProgramInfo::QueryTranscoderID(void) const
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT transcoder FROM recorded "
                  "WHERE chanid    = :CHANID AND "
                  "      starttime = :START");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":START",  recstartts);

    if (query.exec() && query.next())
        return query.value(0).toUInt();

    return 0;
}

/**
 *
 *  \note This method sometimes initiates a QUERY_CHECKFILE MythProto
 *        call and so should not be called from the UI thread.
 */
QString ProgramInfo::DiscoverRecordingDirectory(void) const
{
    if (!IsLocal())
    {
        if (!gCoreContext->IsBackend())
            return QString();

        QString path = GetPlaybackURL(false, true);
        if (path.left(1) == "/")
        {
            QFileInfo testFile(path);
            return testFile.path();
        }

        return QString();
    }

    QFileInfo testFile(pathname);
    if (testFile.exists() || (gCoreContext->GetHostName() == hostname))
    {
        // we may be recording this file and it may not exist yet so we need
        // to do some checking to see what is in pathname
        if (testFile.exists())
        {
            if (testFile.isSymLink())
                testFile.setFile(getSymlinkTarget(pathname));

            if (testFile.isFile())
                return testFile.path();
            else if (testFile.isDir())
                return testFile.filePath();
        }
        else
        {
            testFile.setFile(testFile.absolutePath());
            if (testFile.exists())
            {
                if (testFile.isSymLink())
                    testFile.setFile(getSymlinkTarget(testFile.path()));

                if (testFile.isDir())
                    return testFile.filePath();
            }
        }
    }

    return QString();
}

#include <cassert>
/** Tracks a recording's in use status, to prevent deletion and to
 *  allow the storage scheduler to perform IO load balancing.
 *
 *  \note This method sometimes initiates a QUERY_CHECKFILE MythProto
 *        call and so should not be called from the UI thread.
 */
void ProgramInfo::MarkAsInUse(bool inuse, QString usedFor)
{
    if (!IsRecording())
        return;

    bool notifyOfChange = false;

    if (inuse &&
        (inUseForWhat.isEmpty() ||
         (!usedFor.isEmpty() && usedFor != inUseForWhat)))
    {
        if (!usedFor.isEmpty())
        {

#ifdef DEBUG_IN_USE
            if (!inUseForWhat.isEmpty())
            {
                VERBOSE(VB_IMPORTANT,
                        LOC + QString("MarkAsInUse(true, '%1'->'%2')")
                        .arg(inUseForWhat).arg(usedFor) +
                        " -- use has changed");
            }
            else
            {
                VERBOSE(VB_IMPORTANT, LOC +
                        QString("MarkAsInUse(true, ''->'%1')").arg(usedFor));
            }
#endif // DEBUG_IN_USE

            inUseForWhat = usedFor;
        }
        else if (inUseForWhat.isEmpty())
        {
            QString oldInUseForWhat = inUseForWhat;
            inUseForWhat = QString("%1 [%2]")
                .arg(QObject::tr("Unknown")).arg(getpid());
            VERBOSE(VB_IMPORTANT, LOC_WARN +
                    QString("MarkAsInUse(true, ''->'%1')").arg(inUseForWhat) +
                    " -- use was not explicitly set");
        }

        notifyOfChange = true;
    }

    if (!inuse && !inUseForWhat.isEmpty() && usedFor != inUseForWhat)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                QString("MarkAsInUse(false, '%1'->'%2')")
                .arg(inUseForWhat).arg(usedFor) +
                " -- use has changed since first setting as in use.");
    }
#ifdef DEBUG_IN_USE
    else if (!inuse)
    {
        VERBOSE(VB_IMPORTANT, LOC + QString("MarkAsInUse(false, '%1')")
                .arg(inUseForWhat));
    }
#endif // DEBUG_IN_USE

    if (!inuse && inUseForWhat.isEmpty())
        inUseForWhat = usedFor;

    if (!inuse && inUseForWhat.isEmpty())
    {
        VERBOSE(VB_GENERAL, LOC_WARN +
                "MarkAsInUse requires a key to delete in use mark");
        return; // can't delete if we don't have a key
    }

    if (!inuse)
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare(
            "DELETE FROM inuseprograms "
            "WHERE chanid   = :CHANID   AND starttime = :STARTTIME AND "
            "      hostname = :HOSTNAME AND recusage  = :RECUSAGE");
        query.bindValue(":CHANID",    chanid);
        query.bindValue(":STARTTIME", recstartts);
        query.bindValue(":HOSTNAME",  gCoreContext->GetHostName());
        query.bindValue(":RECUSAGE",  inUseForWhat);

        if (!query.exec())
            MythDB::DBError("MarkAsInUse -- delete", query);

        inUseForWhat.clear();
        lastInUseTime = mythCurrentDateTime().addSecs(-4 * 60 * 60);
        SendUpdateEvent();
        return;
    }

    if (pathname == GetBasename())
        pathname = GetPlaybackURL(false, true);

    QString recDir = DiscoverRecordingDirectory();

    QDateTime inUseTime = mythCurrentDateTime();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT count(*) "
        "FROM inuseprograms "
        "WHERE chanid   = :CHANID   AND starttime = :STARTTIME AND "
        "      hostname = :HOSTNAME AND recusage  = :RECUSAGE");
    query.bindValue(":CHANID",    chanid);
    query.bindValue(":STARTTIME", recstartts);
    query.bindValue(":HOSTNAME",  gCoreContext->GetHostName());
    query.bindValue(":RECUSAGE",  inUseForWhat);

    if (!query.exec())
    {
        MythDB::DBError("MarkAsInUse -- select", query);
    }
    else if (!query.next())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "MarkAsInUse -- select query failed");
    }
    else if (query.value(0).toUInt())
    {
        query.prepare(
            "UPDATE inuseprograms "
            "SET lastupdatetime = :UPDATETIME "
            "WHERE chanid   = :CHANID   AND starttime = :STARTTIME AND "
            "      hostname = :HOSTNAME AND recusage  = :RECUSAGE");
        query.bindValue(":CHANID",     chanid);
        query.bindValue(":STARTTIME",  recstartts);
        query.bindValue(":HOSTNAME",   gCoreContext->GetHostName());
        query.bindValue(":RECUSAGE",   inUseForWhat);
        query.bindValue(":UPDATETIME", inUseTime);

        if (!query.exec())
            MythDB::DBError("MarkAsInUse -- update failed", query);
        else
            lastInUseTime = inUseTime;
    }
    else // if (!query.value(0).toUInt())
    {
        query.prepare(
            "INSERT INTO inuseprograms "
            " (chanid,         starttime,  recusage,  hostname, "
            "  lastupdatetime, rechost,    recdir) "
            "VALUES "
            " (:CHANID,       :STARTTIME, :RECUSAGE, :HOSTNAME, "
            "  :UPDATETIME,   :RECHOST,   :RECDIR)");
        query.bindValue(":CHANID",     chanid);
        query.bindValue(":STARTTIME",  recstartts);
        query.bindValue(":HOSTNAME",   gCoreContext->GetHostName());
        query.bindValue(":RECUSAGE",   inUseForWhat);
        query.bindValue(":UPDATETIME", inUseTime);
        query.bindValue(":RECHOST",    hostname);
        query.bindValue(":RECDIR",     recDir);

        if (!query.exec())
            MythDB::DBError("MarkAsInUse -- insert failed", query);
        else
            lastInUseTime = inUseTime;
    }

    if (!notifyOfChange)
        return;

    // Let others know we changed status
    QDateTime oneHourAgo = QDateTime::currentDateTime().addSecs(-61 * 60);
    query.prepare("SELECT DISTINCT recusage "
                  "FROM inuseprograms "
                  "WHERE lastupdatetime >= :ONEHOURAGO AND "
                  "      chanid          = :CHANID     AND "
                  "      starttime       = :STARTTIME");
    query.bindValue(":CHANID",     chanid);
    query.bindValue(":STARTTIME",  recstartts);
    query.bindValue(":ONEHOURAGO", oneHourAgo);
    if (!query.exec())
        return; // not safe to send update event...

    programflags &= ~(FL_INUSEPLAYING | FL_INUSERECORDING | FL_INUSEOTHER);
    while (query.next())
    {
        QString inUseForWhat = query.value(0).toString();
        if (inUseForWhat.contains(kPlayerInUseID))
            programflags |= FL_INUSEPLAYING;
        else if (inUseForWhat == kRecorderInUseID)
            programflags |= FL_INUSERECORDING;
        else
            programflags |= FL_INUSEOTHER;
    }
    SendUpdateEvent();
}

/** \brief Returns the channel and input needed to record the program.
 *  \note Ideally this would return a the chanid & input, since we
 *        do not enforce a uniqueness constraint on channum in the DB.
 *  \return true on success, false on failure
 */
bool ProgramInfo::QueryTuningInfo(QString &channum, QString &input) const
{
    channum.clear();
    input.clear();
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

    if (query.exec() && query.next())
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
        QObject::tr("MPEG-2",         "Recording Profile MPEG-2") +
        QObject::tr("RTjpeg/MPEG-4",  "Recording Profile RTjpeg/MPEG-4");


    QString rec_profile_groups =
        QObject::tr("CRC IP Recorders",
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
        QObject::tr("Software Encoders (V4L based)",
                    "Recording Profile Group Name") +
        QObject::tr("Transcoders",
                    "Recording Profile Group Name") +
        QObject::tr("USB MPEG-4 Encoder (Plextor ConvertX, etc)",
                    "Recording Profile Group Name");

    QString display_rec_groups =
        QObject::tr("All Programs",   "Recording Group All Programs") +
        QObject::tr("All", "Recording Group All Programs -- short form") +
        QObject::tr("Live TV",        "Recording Group Live TV") +
        QObject::tr("Default",        "Recording Group Default") +
        QObject::tr("Deleted",        "Recording Group Deleted");

    QString storage_groups =
        QObject::tr("Default",        "Storage Group Name") +
        QObject::tr("Live TV",        "Storage Group Name") +
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

int ProgramInfo::InitStatics(void)
{
    QMutexLocker locker(&staticDataLock);
    if (!updater)
        updater = new ProgramInfoUpdater();
    return 1;
}

/// Translations for play,recording, & storage groups
QString ProgramInfo::i18n(const QString &msg)
{
    init_tr();
    QByteArray msg_arr = msg.toLatin1();
    QString msg_i18n = QObject::tr(msg_arr.constData());
    QByteArray msg_i18n_arr = msg_i18n.toLatin1();
    return (msg_arr == msg_i18n_arr) ? msg : msg_i18n;
}

/** \fn ProgramInfo::SubstituteMatches(QString &str)
 *  \brief Subsitute %MATCH% type variable names in the given string
 *  \param str QString to substitute matches in
 *  \note This method sometimes initiates a QUERY_CHECKFILE MythProto
 *        call and so should not be called from the UI thread.
 */
void ProgramInfo::SubstituteMatches(QString &str)
{
    QString pburl = GetPlaybackURL(false, true);
    if (pburl.left(7) == "myth://")
    {
        str.replace(QString("%DIR%"), pburl);
    }
    else
    {
        QFileInfo dirInfo(pburl);
        str.replace(QString("%DIR%"), dirInfo.path());
    }

    str.replace(QString("%FILE%"), GetBasename());
    str.replace(QString("%TITLE%"), title);
    str.replace(QString("%SUBTITLE%"), subtitle);
    str.replace(QString("%DESCRIPTION%"), description);
    str.replace(QString("%HOSTNAME%"), hostname);
    str.replace(QString("%CATEGORY%"), category);
    str.replace(QString("%RECGROUP%"), recgroup);
    str.replace(QString("%PLAYGROUP%"), playgroup);
    str.replace(QString("%CHANID%"), QString::number(chanid));
    static const char *time_str[] =
        { "STARTTIME", "ENDTIME", "PROGSTART", "PROGEND", };
    const QDateTime *time_dtr[] =
        { &recstartts, &recendts, &startts,    &endts,    };
    for (uint i = 0; i < sizeof(time_str)/sizeof(char*); i++)
    {
        str.replace(QString("%%1%").arg(time_str[i]),
                    time_dtr[i]->toString("yyyyMMddhhmmss"));
        str.replace(QString("%%1ISO%").arg(time_str[i]),
                    time_dtr[i]->toString(Qt::ISODate));
        str.replace(QString("%%1UTC%").arg(time_str[i]),
                    (time_dtr[i]->toUTC()).toString("yyyyMMddhhmmss"));
        str.replace(QString("%%1ISOUTC%").arg(time_str[i]),
                    (time_dtr[i]->toUTC()).toString(Qt::ISODate));
    }
}

QMap<QString,uint32_t> ProgramInfo::QueryInUseMap(void)
{
    QMap<QString, uint32_t> inUseMap;
    QDateTime oneHourAgo = QDateTime::currentDateTime().addSecs(-61 * 60);

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT DISTINCT chanid, starttime, recusage "
                  "FROM inuseprograms WHERE lastupdatetime >= :ONEHOURAGO");
    query.bindValue(":ONEHOURAGO", oneHourAgo);

    if (!query.exec())
        return inUseMap;

    while (query.next())
    {
        QString inUseKey = ProgramInfo::MakeUniqueKey(
            query.value(0).toUInt(), query.value(1).toDateTime());

        QString inUseForWhat = query.value(2).toString();

        if (!inUseMap.contains(inUseKey))
            inUseMap[inUseKey] = 0;

        if (inUseForWhat.contains(kPlayerInUseID))
            inUseMap[inUseKey] |= FL_INUSEPLAYING;
        else if (inUseForWhat == kRecorderInUseID)
            inUseMap[inUseKey] |= FL_INUSERECORDING;
        else
            inUseMap[inUseKey] |= FL_INUSEOTHER;
    }

    return inUseMap;
}

QMap<QString,bool> ProgramInfo::QueryJobsRunning(int type)
{
    QMap<QString,bool> is_job_running;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT chanid, starttime, status FROM jobqueue "
                  "WHERE type = :TYPE");
    query.bindValue(":TYPE", type);
    if (!query.exec())
        return is_job_running;

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
                ProgramInfo::MakeUniqueKey(chanid,recstartts)] = true;
        }
    }

    return is_job_running;
}

QStringList ProgramInfo::LoadFromScheduler(
    const QString &tmptable, int recordid)
{
    QStringList slist;
    if (gCoreContext->IsBackend())
    {
        VERBOSE(VB_IMPORTANT,
                "LoadFromScheduler(): Error, called from backend.");
        return slist;
    }

    slist.push_back(
        (tmptable.isEmpty()) ?
        QString("QUERY_GETALLPENDING") :
        QString("QUERY_GETALLPENDING %1 %2").arg(tmptable).arg(recordid));

    if (!gCoreContext->SendReceiveStringList(slist) || slist.size() < 2)
    {
        VERBOSE(VB_IMPORTANT,
                "LoadFromScheduler(): Error querying master.");
        slist.clear();
    }

    return slist;
}

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
        QString chanorder =
            gCoreContext->GetSetting("ChannelOrdering", "channum");
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
    {
        destination.push_back(
            new ProgramInfo(
                query.value(3).toString(), // title
                query.value(4).toString(), // subtitle
                query.value(5).toString(), // description
                query.value(6).toString(), // category

                query.value(0).toUInt(), // chanid
                query.value(7).toString(), // channum
                query.value(8).toString(), // chansign
                query.value(9).toString(), // channame
                query.value(12).toString(), // chanplaybackfilters

                query.value(1).toDateTime(), // startts
                query.value(2).toDateTime(), // endts
                query.value(1).toDateTime(), // recstartts
                query.value(2).toDateTime(), // recendts

                query.value(13).toString(), // seriesid
                query.value(14).toString(), // programid
                query.value(18).toString(), // catType

                query.value(16).toDouble(), // stars
                query.value(15).toUInt(), // year
                query.value(17).toDate(), // originalAirDate
                RecStatusType(query.value(21).toInt()), // recstatus
                query.value(19).toUInt(), // recordid
                RecordingType(query.value(20).toInt()), // rectype
                query.value(22).toUInt(), // findid

                query.value(11).toInt() == COMM_DETECT_COMMFREE, // commfree
                query.value(10).toInt(), // repeat

                schedList, oneChanid));
    }

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
        uint chanid = query.value(0).toUInt();
        QString channum  = QString("#%1").arg(chanid);
        QString chansign = channum;
        QString channame = channum;
        if (!query.value(9).toString().isEmpty())
        {
            channum  = query.value(9).toString();
            chansign = query.value(10).toString();
            channame = query.value(11).toString();
        }

        destination.push_back(new ProgramInfo(
            query.value(3).toString(),
            query.value(4).toString(),
            query.value(5).toString(),
            query.value(6).toString(),

            chanid, channum, chansign, channame,

            query.value(7).toString(), query.value(8).toString(),

            query.value(1).toDateTime(), query.value(2).toDateTime(),
            query.value(1).toDateTime(), query.value(2).toDateTime(),

            RecStatusType(query.value(14).toInt()),
            query.value(15).toUInt(),
            RecordingType(query.value(13).toInt()),
            query.value(12).toUInt(),

            query.value(16).toInt()));
    }

    return true;
}

bool LoadFromRecorded(
    ProgramList &destination,
    bool possiblyInProgressRecordingsOnly,
    const QMap<QString,uint32_t> &inUseMap,
    const QMap<QString,bool> &isJobRunning,
    const QMap<QString, ProgramInfo*> &recMap)
{
    destination.clear();

    QString     fs_db_name = "";
    QDateTime   rectime    = QDateTime::currentDateTime().addSecs(
        -gCoreContext->GetNumSetting("RecordOverTime"));

    // ----------------------------------------------------------------------

    QString thequery = ProgramInfo::kFromRecordedQuery;
    if (possiblyInProgressRecordingsOnly)
        thequery += "WHERE r.endtime >= NOW() AND r.starttime <= NOW() ";

    thequery += "ORDER BY r.starttime DESC ";

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(thequery);

    if (!query.exec())
    {
        MythDB::DBError("ProgramList::FromRecorded", query);
        return true;
    }

    while (query.next())
    {
        const uint chanid = query.value(4).toUInt();
        QString channum  = QString("#%1").arg(chanid);
        QString chansign = channum;
        QString channame = channum;
        QString chanfilt;
        if (!query.value(5).toString().isEmpty())
        {
            channum  = query.value(5).toString();
            chansign = query.value(6).toString();
            channame = query.value(7).toString();
            chanfilt = query.value(8).toString();
        }

        QString hostname = query.value(13).toString();
        if (hostname.isEmpty())
            hostname = gCoreContext->GetHostName();

        RecStatusType recstatus = rsRecorded;
        QDateTime recstartts = query.value(21).toDateTime();

        QString key = ProgramInfo::MakeUniqueKey(chanid, recstartts);
        if (query.value(22).toDateTime() > rectime && recMap.contains(key))
            recstatus = rsRecording;

        bool save_not_commflagged = false;
        uint flags = 0;

        set_flag(flags, FL_CHANCOMMFREE,
                 query.value(27).toInt() == COMM_DETECT_COMMFREE);
        set_flag(flags, FL_COMMFLAG,
                 query.value(28).toInt() == COMM_FLAG_DONE);
        set_flag(flags, FL_COMMPROCESSING ,
                 query.value(28).toInt() == COMM_FLAG_PROCESSING);
        set_flag(flags, FL_REPEAT,        query.value(29).toBool());
        set_flag(flags, FL_TRANSCODED,
                 query.value(31).toInt() == TRANSCODING_COMPLETE);
        set_flag(flags, FL_DELETEPENDING, query.value(32).toBool());
        set_flag(flags, FL_PRESERVED,     query.value(33).toBool());
        set_flag(flags, FL_CUTLIST,       query.value(34).toBool());
        set_flag(flags, FL_AUTOEXP,       query.value(35).toBool());
        set_flag(flags, FL_REALLYEDITING, query.value(36).toBool());
        set_flag(flags, FL_BOOKMARK,      query.value(37).toBool());
        set_flag(flags, FL_WATCHED,       query.value(38).toBool());

        if (inUseMap.contains(key))
            flags |= inUseMap[key];

        if (flags & FL_COMMPROCESSING &&
            (isJobRunning.find(key) == isJobRunning.end()))
        {
            flags &= ~FL_COMMPROCESSING;
            save_not_commflagged = true;
        }

        set_flag(flags, FL_EDITING,
                 (flags & FL_REALLYEDITING) ||
                 (flags & COMM_FLAG_PROCESSING));

        destination.push_back(
            new ProgramInfo(
                query.value(0).toString(),
                query.value(1).toString(),
                query.value(2).toString(),
                query.value(3).toString(),

                chanid, channum, chansign, channame, chanfilt,

                query.value(9).toString(), query.value(10).toString(),

                query.value(12).toString(),

                hostname, query.value(11).toString(),

                query.value(15).toString(), query.value(16).toString(),

                query.value(14).toInt(),

                query.value(17).toULongLong(),

                query.value(18).toDateTime(), query.value(19).toDateTime(),
                query.value(21).toDateTime(), query.value(22).toDateTime(),

                query.value(20).toDouble(),

                query.value(23).toUInt(),
                query.value(24).toDate(),
                query.value(25).toDateTime(),

                recstatus,

                query.value(26).toUInt(),

                RecordingDupInType(query.value(43).toInt()),
                RecordingDupMethodType(query.value(44).toInt()),

                query.value(42).toUInt(),

                flags,
                query.value(39).toUInt(),
                query.value(40).toUInt(),
                query.value(41).toUInt()));

        if (save_not_commflagged)
            destination.back()->SaveCommFlagged(COMM_FLAG_NOT_FLAGGED);
    }

    return true;
}

QString SkipTypeToString(int flags)
{
    if (COMM_DETECT_COMMFREE == flags)
        return QObject::tr("Commercial Free");
    if (COMM_DETECT_UNINIT == flags)
        return QObject::tr("Use Global Setting");

    QChar chr = '0';
    QString ret = QString("0x%1").arg(flags,3,16,chr);
    bool blank = COMM_DETECT_BLANK & flags;
    bool scene = COMM_DETECT_SCENE & flags;
    bool logo  = COMM_DETECT_LOGO  & flags;
    bool exp   = COMM_DETECT_2     & flags;
    bool prePst= COMM_DETECT_PREPOSTROLL & flags;

    if (blank && scene && logo)
        ret = QObject::tr("All Available Methods");
    else if (blank && scene && !logo)
        ret = QObject::tr("Blank Frame + Scene Change");
    else if (blank && !scene && logo)
        ret = QObject::tr("Blank Frame + Logo Detection");
    else if (!blank && scene && logo)
        ret = QObject::tr("Scene Change + Logo Detection");
    else if (blank && !scene && !logo)
        ret = QObject::tr("Blank Frame Detection");
    else if (!blank && scene && !logo)
        ret = QObject::tr("Scene Change Detection");
    else if (!blank && !scene && logo)
        ret = QObject::tr("Logo Detection");

    if (exp)
        ret = QObject::tr("Experimental") + ": " + ret;
    else if(prePst)
        ret = QObject::tr("Pre & Post Roll") + ": " + ret;

    return ret;
}

deque<int> GetPreferredSkipTypeCombinations(void)
{
    deque<int> tmp;
    tmp.push_back(COMM_DETECT_BLANK | COMM_DETECT_SCENE | COMM_DETECT_LOGO);
    tmp.push_back(COMM_DETECT_BLANK);
    tmp.push_back(COMM_DETECT_BLANK | COMM_DETECT_SCENE);
    tmp.push_back(COMM_DETECT_SCENE);
    tmp.push_back(COMM_DETECT_LOGO);
    tmp.push_back(COMM_DETECT_2 | COMM_DETECT_BLANK | COMM_DETECT_LOGO);
    tmp.push_back(COMM_DETECT_PREPOSTROLL | COMM_DETECT_BLANK |
                  COMM_DETECT_SCENE);
    return tmp;
}

PMapDBReplacement::PMapDBReplacement() : lock(new QMutex())
{
}

PMapDBReplacement::~PMapDBReplacement()
{
    delete lock;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
