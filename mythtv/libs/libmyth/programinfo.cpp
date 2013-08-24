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
#include "programinfoupdater.h"
#include "mythcorecontext.h"
#include "mythscheduler.h"
#include "mythmiscutil.h"
#include "storagegroup.h"
#include "mythlogging.h"
#include "programinfo.h"
#include "remotefile.h"
#include "remoteutil.h"
#include "dialogbox.h"
#include "mythdate.h"
#include "mythdb.h"

#define LOC      QString("ProgramInfo(%1): ").arg(GetBasename())

//#define DEBUG_IN_USE

static int init_tr(void);

int pginfo_init_statics() { return ProgramInfo::InitStatics(); }
QMutex ProgramInfo::staticDataLock;
ProgramInfoUpdater *ProgramInfo::updater;
int dummy = pginfo_init_statics();
bool ProgramInfo::usingProgIDAuth = true;


const QString ProgramInfo::kFromRecordedQuery =
    "SELECT r.title,            r.subtitle,     r.description,     "// 0-2
    "       r.season,           r.episode,      r.category,        "// 3-5
    "       r.chanid,           c.channum,      c.callsign,        "// 6-8
    "       c.name,             c.outputfilters,r.recgroup,        "// 9-11
    "       r.playgroup,        r.storagegroup, r.basename,        "//12-14
    "       r.hostname,         r.recpriority,  r.seriesid,        "//15-17
    "       r.programid,        r.inetref,      r.filesize,        "//18-20
    "       r.progstart,        r.progend,      r.stars,           "//21-23
    "       r.starttime,        r.endtime,      p.airdate+0,       "//24-26
    "       r.originalairdate,  r.lastmodified, r.recordid,        "//27-29
    "       c.commmethod,       r.commflagged,  r.previouslyshown, "//30-32
    "       r.transcoder,       r.transcoded,   r.deletepending,   "//33-35
    "       r.preserve,         r.cutlist,      r.autoexpire,      "//36-38
    "       r.editing,          r.bookmark,     r.watched,         "//39-41
    "       p.audioprop+0,      p.videoprop+0,  p.subtitletypes+0, "//42-44
    "       r.findid,           rec.dupin,      rec.dupmethod,     "//45-47
    "       p.syndicatedepisodenumber, p.partnumber, p.parttotal   "//48-50
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

QString myth_category_type_to_string(ProgramInfo::CategoryType category_type)
{
    static int NUM_CAT_TYPES = 5;
    static const char *cattype[] =
        { "", "movie", "series", "sports", "tvshow", };

    if ((category_type > ProgramInfo::kCategoryNone) &&
        ((int)category_type < NUM_CAT_TYPES))
        return QString(cattype[category_type]);

    return "";
}

ProgramInfo::CategoryType string_to_myth_category_type(const QString &category_type)
{
    static int NUM_CAT_TYPES = 5;
    static const char *cattype[] =
        { "", "movie", "series", "sports", "tvshow", };

    for (int i = 1; i < NUM_CAT_TYPES; i++)
        if (category_type == cattype[i])
            return (ProgramInfo::CategoryType) i;
    return ProgramInfo::kCategoryNone;
}

/** \fn ProgramInfo::ProgramInfo(void)
 *  \brief Null constructor.
 */
ProgramInfo::ProgramInfo(void) :
    title(),
    subtitle(),
    description(),
    season(0),
    episode(0),
    syndicatedepisode(),
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
    inetref(),
    catType(kCategoryNone),


    filesize(0ULL),

    startts(MythDate::current(true)),
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
    partnumber(0),
    parttotal(0),

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
    season(other.season),
    episode(other.episode),
    syndicatedepisode(other.syndicatedepisode),
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
    inetref(other.inetref),
    catType(other.catType),

    filesize(other.filesize),

    startts(other.startts),
    endts(other.endts),
    recstartts(other.recstartts),
    recendts(other.recendts),

    stars(other.stars),

    originalAirDate(other.originalAirDate),
    lastmodified(other.lastmodified),
    lastInUseTime(MythDate::current().addSecs(-4 * 60 * 60)),

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
    partnumber(other.partnumber),
    parttotal(other.parttotal),

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

/** \fn ProgramInfo::ProgramInfo(uint _chanid, const QDateTime &_recstartts)
 *  \brief Constructs a ProgramInfo from data in 'recorded' table
 */
ProgramInfo::ProgramInfo(uint _chanid, const QDateTime &_recstartts) :
    chanid(0),
    positionMapDBReplacement(NULL)
{
    LoadProgramFromRecorded(_chanid, _recstartts);
}

/** \fn ProgramInfo::ProgramInfo()
 *  \brief Constructs a ProgramInfo from data in 'recorded' table
 */
ProgramInfo::ProgramInfo(
    const QString &_title,
    const QString &_subtitle,
    const QString &_description,
    uint  _season,
    uint  _episode,
    const QString &_syndicatedepisode,
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
    const QString &_inetref,

    int _recpriority,

    uint64_t _filesize,

    const QDateTime &_startts,
    const QDateTime &_endts,
    const QDateTime &_recstartts,
    const QDateTime &_recendts,

    float _stars,

    uint _year,
    uint _partnumber,
    uint _parttotal,
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
    season(_season),
    episode(_episode),
    syndicatedepisode(_syndicatedepisode),
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
    inetref(_inetref),
    catType(kCategoryNone),

    filesize(_filesize),

    startts(_startts),
    endts(_endts),
    recstartts(_recstartts),
    recendts(_recendts),

    stars(clamp(_stars, 0.0f, 1.0f)),

    originalAirDate(_originalAirDate),
    lastmodified(_lastmodified),
    lastInUseTime(MythDate::current().addSecs(-4 * 60 * 60)),

    prefinput(0),
    recpriority2(0),
    recordid(_recordid),
    parentid(0),

    sourceid(0),
    inputid(0),
    cardid(0),

    findid(_findid),

    programflags(_programflags),
    properties((_subtitleType    << kSubtitlePropertyOffset) |
               (_videoproperties << kVideoPropertyOffset)    |
               (_audioproperties << kAudioPropertyOffset)),
    year(_year),
    partnumber(_partnumber),
    parttotal(_parttotal),

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

/** \fn ProgramInfo::ProgramInfo()
 *  \brief Constructs a ProgramInfo from data in 'oldrecorded' table
 */
ProgramInfo::ProgramInfo(
    const QString &_title,
    const QString &_subtitle,
    const QString &_description,
    uint  _season,
    uint  _episode,
    const QString &_category,

    uint _chanid,
    const QString &_channum,
    const QString &_chansign,
    const QString &_channame,

    const QString &_seriesid,
    const QString &_programid,
    const QString &_inetref,

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
    season(_season),
    episode(_episode),
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
    inetref(_inetref),
    catType(kCategoryNone),

    filesize(0ULL),

    startts(_startts),
    endts(_endts),
    recstartts(_recstartts),
    recendts(_recendts),

    stars(0.0f),

    originalAirDate(),
    lastmodified(startts),
    lastInUseTime(MythDate::current().addSecs(-4 * 60 * 60)),

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
    partnumber(0),
    parttotal(0),

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

/** \fn ProgramInfo::ProgramInfo()
 *  \brief Constructs a ProgramInfo from listings data in 'program' table
 */
ProgramInfo::ProgramInfo(
    const QString &_title,
    const QString &_subtitle,
    const QString &_description,
    const QString &_syndicatedepisode,
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
    const CategoryType _catType,

    float _stars,
    uint _year,
    uint _partnumber,
    uint _parttotal,

    const QDate &_originalAirDate,
    RecStatusType _recstatus,
    uint _recordid,
    RecordingType _rectype,
    uint _findid,

    bool commfree,
    bool repeat,

    uint _videoproperties,
    uint _audioproperties,
    uint _subtitleType,

    const ProgramList &schedList) :
    title(_title),
    subtitle(_subtitle),
    description(_description),
    season(0),
    episode(0),
    syndicatedepisode(_syndicatedepisode),
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
    inetref(),
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
    properties((_subtitleType    << kSubtitlePropertyOffset) |
               (_videoproperties << kVideoPropertyOffset)    |
               (_audioproperties << kAudioPropertyOffset)),
    year(_year),
    partnumber(_partnumber),
    parttotal(_parttotal),

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

        if (chanstr != s.chanstr)
        {
            if (s.recstatus == rsWillRecord)
                recstatus = rsOtherShowing;
            else if (s.recstatus == rsRecording)
                recstatus = rsOtherRecording;
            else if (s.recstatus == rsTuning)
                recstatus = rsOtherTuning;
        }

        // Stop recording keys on chanid (and recstarts).  If the
        // recording is running, override the chanid, so we can stop
        // it from any matching program.  Remove this hack when we
        // replace chanid/recstartts as the primary key for the
        // recorded and related tables with something better.
        if (s.recstatus == rsRecording ||
            s.recstatus == rsTuning)
        {
            chanid = s.chanid;
        }
    }
}

/** \fn ProgramInfo::ProgramInfo()
 *  \brief Constructs a basic ProgramInfo (used by RecordingInfo)
 */
ProgramInfo::ProgramInfo(
    const QString &_title,
    const QString &_subtitle,
    const QString &_description,
    uint  _season,
    uint  _episode,
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
    const QString &_programid,
    const QString &_inetref) :
    title(_title),
    subtitle(_subtitle),
    description(_description),
    season(_season),
    episode(_episode),
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
    inetref(_inetref),
    catType(kCategoryNone),

    filesize(0ULL),

    startts(_startts),
    endts(_endts),
    recstartts(_recstartts),
    recendts(_recendts),

    stars(0.0f),

    originalAirDate(),
    lastmodified(MythDate::current()),
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
    partnumber(0),
    parttotal(0),

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

/** \fn ProgramInfo::ProgramInfo(const QString &_pathname)
 *  \brief Constructs a ProgramInfo for a pathname.
 */
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
    if (!gCoreContext->IsDatabaseIgnored() &&
        QueryKeyFromPathname(_pathname, _chanid, _recstartts) &&
        LoadProgramFromRecorded(_chanid, _recstartts))
    {
        return;
    }

    clear();

    QDateTime cur = MythDate::current();
    recstartts = startts = cur.addSecs(-4 * 60 * 60 - 1);
    recendts   = endts   = cur.addSecs(-1);

    QString basename = _pathname.section('/', -1);
    if (_pathname == basename)
        SetPathname(QDir::currentPath() + '/' + _pathname);
    else if (_pathname.contains("./") && !_pathname.contains(":"))
        SetPathname(QFileInfo(_pathname).absoluteFilePath());
    else
        SetPathname(_pathname);
}

/** \fn ProgramInfo::ProgramInfo()
 *  \brief Constructs a ProgramInfo for a video.
 */
ProgramInfo::ProgramInfo(const QString &_pathname,
                         const QString &_plot,
                         const QString &_title,
                         const QString &_subtitle,
                         const QString &_director,
                         int _season, int _episode,
                         const QString &_inetref,
                         uint _length_in_minutes,
                         uint _year,
                         const QString &_programid) :
    positionMapDBReplacement(NULL)
{
    clear();

    QDateTime cur = MythDate::current();
    recstartts = cur.addSecs(((int)_length_in_minutes + 1) * -60);
    recendts   = recstartts.addSecs(_length_in_minutes * 60);
    startts    = QDateTime(QDate(year,1,1),QTime(0,0,0), Qt::UTC);
    endts      = startts.addSecs(_length_in_minutes * 60);

    QString pn = _pathname;
    if ((!_pathname.startsWith("myth://")) &&
        (_pathname.endsWith(".iso", Qt::CaseInsensitive) ||
         _pathname.endsWith(".img", Qt::CaseInsensitive) ||
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

    season = _season;
    episode = _episode;
    inetref = _inetref;
    title = _title;
    programid = _programid;
}

/** \fn ProgramInfo::ProgramInfo()
 *  \brief Constructs a manual record ProgramInfo.
 */
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

        title = QString("%1 - %2").arg(ChannelText(channelFormat))
            .arg(MythDate::toString(startts, MythDate::kTime));
    }

    description = title =
        QString("%1 (%2)").arg(title).arg(QObject::tr("Manual Record"));
}

/** \fn ProgramInfo::ProgramInfo()
 *  \brief Constructs a Dummy ProgramInfo (used by GuideGrid)
 */
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
    season = other.season;
    episode = other.episode;
    syndicatedepisode = other.syndicatedepisode;
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
    inetref = other.inetref;
    catType = other.catType;

    recpriority = other.recpriority;

    filesize = other.filesize;

    startts = other.startts;
    endts = other.endts;
    recstartts = other.recstartts;
    recendts = other.recendts;

    stars = other.stars;

    year = other.year;
    partnumber = other.partnumber;
    parttotal = other.parttotal;

    originalAirDate = other.originalAirDate;
    lastmodified = other.lastmodified;
    lastInUseTime = MythDate::current().addSecs(-4 * 60 * 60);

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
    inetref.detach();

    sortTitle.detach();
    inUseForWhat.detach();
}

void ProgramInfo::clear(void)
{
    title.clear();
    subtitle.clear();
    description.clear();
    season = 0;
    episode = 0;
    syndicatedepisode.clear();
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
    partnumber = 0;
    parttotal = 0;

    seriesid.clear();
    programid.clear();
    inetref.clear();
    catType = kCategoryNone;

    sortTitle.clear();

    recpriority = 0;

    filesize = 0ULL;

    startts = MythDate::current(true);
    endts = startts;
    recstartts = startts;
    recendts = startts;

    stars = 0.0f;

    originalAirDate = QDate();
    lastmodified = startts;
    lastInUseTime = startts.addSecs(-4 * 60 * 60);

    recstatus = rsUnknown;
    oldrecstatus = rsUnknown;

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
    recstartts = MythDate::fromString(keyParts[1]);
    return chanid && recstartts.isValid();
}

bool ProgramInfo::ExtractKeyFromPathname(
    const QString &pathname, uint &chanid, QDateTime &recstartts)
{
    QString basename = pathname.section('/', -1);
    if (basename.isEmpty())
        return false;

    QStringList lr = basename.split("_");
    if (lr.size() == 2)
    {
        chanid = lr[0].toUInt();
        QStringList ts = lr[1].split(".");
        if (chanid && !ts.empty())
        {
            recstartts = MythDate::fromString(ts[0]);
            return recstartts.isValid();
        }
    }

    return false;
}

bool ProgramInfo::QueryKeyFromPathname(
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
        recstartts = MythDate::as_utc(query.value(1).toDateTime());
        return true;
    }

    return ExtractKeyFromPathname(pathname, chanid, recstartts);
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
    INT_TO_LIST(season);       // 3
    INT_TO_LIST(episode);      // 4
    STR_TO_LIST(syndicatedepisode); // 5
    STR_TO_LIST(category);     // 6
    INT_TO_LIST(chanid);       // 7
    STR_TO_LIST(chanstr);      // 8
    STR_TO_LIST(chansign);     // 9
    STR_TO_LIST(channame);     // 10
    STR_TO_LIST(pathname);     // 11
    INT_TO_LIST(filesize);     // 12

    DATETIME_TO_LIST(startts); // 13
    DATETIME_TO_LIST(endts);   // 14
    INT_TO_LIST(findid);       // 15
    STR_TO_LIST(hostname);     // 16
    INT_TO_LIST(sourceid);     // 17
    INT_TO_LIST(cardid);       // 18
    INT_TO_LIST(inputid);      // 19
    INT_TO_LIST(recpriority);  // 20
    INT_TO_LIST(recstatus);    // 21
    INT_TO_LIST(recordid);     // 22

    INT_TO_LIST(rectype);      // 23
    INT_TO_LIST(dupin);        // 24
    INT_TO_LIST(dupmethod);    // 25
    DATETIME_TO_LIST(recstartts);//26
    DATETIME_TO_LIST(recendts);// 27
    INT_TO_LIST(programflags); // 28
    STR_TO_LIST((!recgroup.isEmpty()) ? recgroup : "Default"); // 29
    STR_TO_LIST(chanplaybackfilters); // 30
    STR_TO_LIST(seriesid);     // 31
    STR_TO_LIST(programid);    // 32
    STR_TO_LIST(inetref);      // 33

    DATETIME_TO_LIST(lastmodified); // 34
    FLOAT_TO_LIST(stars);           // 35
    DATE_TO_LIST(originalAirDate);  // 36
    STR_TO_LIST((!playgroup.isEmpty()) ? playgroup : "Default"); // 37
    INT_TO_LIST(recpriority2);      // 38
    INT_TO_LIST(parentid);          // 39
    STR_TO_LIST((!storagegroup.isEmpty()) ? storagegroup : "Default"); // 40
    INT_TO_LIST(GetAudioProperties()); // 41
    INT_TO_LIST(GetVideoProperties()); // 42
    INT_TO_LIST(GetSubtitleType());    // 43

    INT_TO_LIST(year);              // 44
    INT_TO_LIST(partnumber);   // 45
    INT_TO_LIST(parttotal);    // 46
/* do not forget to update the NUMPROGRAMLINES defines! */
}

// QStringList::const_iterator it = list.begin()+offset;

#define NEXT_STR()        do { if (it == listend)                    \
                               {                                     \
                                   LOG(VB_GENERAL, LOG_ERR, listerror); \
                                   clear();                          \
                                   return false;                     \
                               }                                     \
                               ts = *it++; } while (0)

#define INT_FROM_LIST(x)     do { NEXT_STR(); (x) = ts.toLongLong(); } while (0)
#define ENUM_FROM_LIST(x, y) do { NEXT_STR(); (x) = ((y)ts.toInt()); } while (0)

#define DATETIME_FROM_LIST(x) \
    do { NEXT_STR(); x = MythDate::fromTime_t(ts.toUInt()); } while (0)
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
    INT_FROM_LIST(season);           // 3
    INT_FROM_LIST(episode);          // 4
    STR_FROM_LIST(syndicatedepisode); // 5
    STR_FROM_LIST(category);         // 6
    INT_FROM_LIST(chanid);           // 7
    STR_FROM_LIST(chanstr);          // 8
    STR_FROM_LIST(chansign);         // 9
    STR_FROM_LIST(channame);         // 10
    STR_FROM_LIST(pathname);         // 11
    INT_FROM_LIST(filesize);         // 12

    DATETIME_FROM_LIST(startts);     // 13
    DATETIME_FROM_LIST(endts);       // 14
    INT_FROM_LIST(findid);           // 15
    STR_FROM_LIST(hostname);         // 16
    INT_FROM_LIST(sourceid);         // 17
    INT_FROM_LIST(cardid);           // 18
    INT_FROM_LIST(inputid);          // 19
    INT_FROM_LIST(recpriority);      // 20
    ENUM_FROM_LIST(recstatus, RecStatusType); // 21
    INT_FROM_LIST(recordid);         // 22

    ENUM_FROM_LIST(rectype, RecordingType);            // 23
    ENUM_FROM_LIST(dupin, RecordingDupInType);         // 24
    ENUM_FROM_LIST(dupmethod, RecordingDupMethodType); // 25
    DATETIME_FROM_LIST(recstartts);   // 26
    DATETIME_FROM_LIST(recendts);     // 27
    INT_FROM_LIST(programflags);      // 28
    STR_FROM_LIST(recgroup);          // 29
    STR_FROM_LIST(chanplaybackfilters);//30
    STR_FROM_LIST(seriesid);          // 31
    STR_FROM_LIST(programid);         // 32
    STR_FROM_LIST(inetref);           // 33

    DATETIME_FROM_LIST(lastmodified); // 34
    FLOAT_FROM_LIST(stars);           // 35
    DATE_FROM_LIST(originalAirDate);; // 36
    STR_FROM_LIST(playgroup);         // 37
    INT_FROM_LIST(recpriority2);      // 38
    INT_FROM_LIST(parentid);          // 39
    STR_FROM_LIST(storagegroup);      // 40
    uint audioproperties, videoproperties, subtitleType;
    INT_FROM_LIST(audioproperties);   // 41
    INT_FROM_LIST(videoproperties);   // 42
    INT_FROM_LIST(subtitleType);      // 43
    properties = ((subtitleType    << kSubtitlePropertyOffset) |
                  (videoproperties << kVideoPropertyOffset)    |
                  (audioproperties << kAudioPropertyOffset));

    INT_FROM_LIST(year);              // 44
    INT_FROM_LIST(partnumber);        // 45
    INT_FROM_LIST(parttotal);         // 46

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
    QLocale locale = gCoreContext->GetQLocale();
    // NOTE: Format changes and relevant additions made here should be
    //       reflected in RecordingRule
    QString channelFormat =
        gCoreContext->GetSetting("ChannelFormat", "<num> <sign>");
    QString longChannelFormat =
        gCoreContext->GetSetting("LongChannelFormat", "<num> <name>");

    QDateTime timeNow = MythDate::current();

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

    if (season > 0 || episode > 0)
    {
        progMap["season"] = format_season_and_episode(season, 1);
        progMap["episode"] = format_season_and_episode(episode, 1);
        progMap["s00e00"] = QString("s%1e%2")
            .arg(format_season_and_episode(GetSeason(), 2))
            .arg(format_season_and_episode(GetEpisode(), 2));
        progMap["00x00"] = QString("%1x%2")
            .arg(format_season_and_episode(GetSeason(), 1))
            .arg(format_season_and_episode(GetEpisode(), 2));
    }
    else
    {
        progMap["season"] = progMap["episode"] = "";
        progMap["s00e00"] = progMap["00x00"] = "";
    }
    progMap["syndicatedepisode"] = syndicatedepisode;

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
            progMap["startdate"] = startts.toLocalTime().toString("yyyy");
            progMap["recstartdate"] = startts.toLocalTime().toString("yyyy");
        }
    }
    else // if (IsRecording())
    {
        using namespace MythDate;
        progMap["starttime"] = MythDate::toString(startts, kTime);
        progMap["startdate"] =
            MythDate::toString(startts, kDateFull | kSimplify);
        progMap["shortstartdate"] = MythDate::toString(startts, kDateShort);
        progMap["endtime"] = MythDate::toString(endts, kTime);
        progMap["enddate"] = MythDate::toString(endts, kDateFull | kSimplify);
        progMap["shortenddate"] = MythDate::toString(endts, kDateShort);
        progMap["recstarttime"] = MythDate::toString(recstartts, kTime);
        progMap["recstartdate"] = MythDate::toString(recstartts, kDateShort);
        progMap["recendtime"] = MythDate::toString(recendts, kTime);
        progMap["recenddate"] = MythDate::toString(recendts, kDateShort);
        progMap["startts"] = QString::number(startts.toTime_t());
        progMap["endts"]   = QString::number(endts.toTime_t());
        if (timeNow.toLocalTime().date().year() !=
            startts.toLocalTime().date().year())
            progMap["startyear"] = startts.toLocalTime().toString("yyyy");
        if (timeNow.toLocalTime().date().year() !=
            endts.toLocalTime().date().year())
            progMap["endyear"] = endts.toLocalTime().toString("yyyy");
    }

    using namespace MythDate;
    progMap["timedate"] =
        MythDate::toString(recstartts, kDateTimeFull | kSimplify) + " - " +
        MythDate::toString(recendts, kTime);

    progMap["shorttimedate"] =
        MythDate::toString(recstartts, kDateTimeShort | kSimplify) + " - " +
        MythDate::toString(recendts, kTime);

    progMap["starttimedate"] =
        MythDate::toString(recstartts, kDateTimeFull | kSimplify);

    progMap["shortstarttimedate"] =
        MythDate::toString(recstartts, kDateTimeShort | kSimplify);

    progMap["lastmodifiedtime"] = MythDate::toString(lastmodified, kTime);
    progMap["lastmodifieddate"] =
        MythDate::toString(lastmodified, kDateFull | kSimplify);
    progMap["lastmodified"] =
        MythDate::toString(lastmodified, kDateTimeFull | kSimplify);

    progMap["channum"] = chanstr;
    progMap["chanid"] = chanid;
    progMap["channame"] = channame;
    progMap["channel"] = ChannelText(channelFormat);
    progMap["longchannel"] = ChannelText(longChannelFormat);

    QString tmpSize = locale.toString(filesize * (1.0 / (1024.0 * 1024.0 * 1024.0)), 'f', 2);
    progMap["filesize_str"] = QObject::tr("%1 GB", "GigaBytes").arg(tmpSize);

    progMap["filesize"] = locale.toString((quint64)filesize);

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

    progMap["card"] = ::toString(GetRecordingStatus(), cardid);
    progMap["input"] = ::toString(GetRecordingStatus(), inputid);
    progMap["inputname"] = QueryInputDisplayName();

    progMap["recpriority"] = recpriority;
    progMap["recpriority2"] = recpriority2;
    progMap["recordinggroup"] = (recgroup == "Default")
        ? QObject::tr("Default") : recgroup;
    progMap["playgroup"] = playgroup;

    if (storagegroup == "Default")
        progMap["storagegroup"] = QObject::tr("Default");
    else if (StorageGroup::kSpecialGroups.contains(storagegroup))
        progMap["storagegroup"] = QObject::tr(storagegroup.toUtf8().constData());
    else
        progMap["storagegroup"] = storagegroup;

    progMap["programflags"] = programflags;

    progMap["audioproperties"] = GetAudioProperties();
    progMap["videoproperties"] = GetVideoProperties();
    progMap["subtitleType"] = GetSubtitleType();

    progMap["recstatus"] = ::toString(GetRecordingStatus(),
                                      GetRecordingRuleType());
    progMap["recstatuslong"] = ::toDescription(GetRecordingStatus(),
                                               GetRecordingRuleType(),
                                               GetRecordingStartTime());

    if (IsRepeat())
    {
        progMap["repeat"] = QString("(%1) ").arg(QObject::tr("Repeat"));
        progMap["longrepeat"] = progMap["repeat"];
        if (originalAirDate.isValid())
        {
            progMap["longrepeat"] = QString("(%1 %2) ")
                .arg(QObject::tr("Repeat"))
                .arg(MythDate::toString(
                         originalAirDate,
                         MythDate::kDateFull | MythDate::kAddYear));
        }
    }
    else
    {
        progMap["repeat"] = "";
        progMap["longrepeat"] = "";
    }

    progMap["seriesid"] = seriesid;
    progMap["programid"] = programid;
    progMap["inetref"] = inetref;
    progMap["catType"] = myth_category_type_to_string(catType);

    progMap["year"] = year ? QString::number(year) : "";

    progMap["partnumber"] = partnumber ? QString::number(partnumber) : "";
    progMap["parttotal"] = parttotal ? QString::number(parttotal) : "";

    QString star_str = (stars != 0.0f) ?
        QObject::tr("%n star(s)", "", GetStars(star_range)) : "";
    progMap["stars"] = star_str;
    progMap["numstars"] = QString::number(GetStars(star_range));

    if (stars != 0.0f && year)
        progMap["yearstars"] = QString("(%1, %2)").arg(year).arg(star_str);
    else if (stars != 0.0f)
        progMap["yearstars"] = QString("(%1)").arg(star_str);
    else if (year)
        progMap["yearstars"] = QString("(%1)").arg(year);
    else
        progMap["yearstars"] = "";

    if (!originalAirDate.isValid() ||
        (!programid.isEmpty() && programid.startsWith("MV")))
    {
        progMap["originalairdate"] = "";
        progMap["shortoriginalairdate"] = "";
    }
    else
    {
        progMap["originalairdate"] = MythDate::toString(
            originalAirDate, MythDate::kDateFull);
        progMap["shortoriginalairdate"] = MythDate::toString(
            originalAirDate, MythDate::kDateShort);
    }

    // 'mediatype' for a statetype, so untranslated
    // 'mediatypestring' for textarea, so translated
    // TODO Move to a dedicated ToState() method?
    QString mediaType;
    QString mediaTypeString;
    switch (GetProgramInfoType())
    {
        case kProgramInfoTypeVideoFile :
            mediaType = "video";
            mediaTypeString = QObject::tr("Video");
            break;
        case kProgramInfoTypeVideoDVD :
            mediaType = "dvd";
            mediaTypeString = QObject::tr("DVD");
            break;
        case kProgramInfoTypeVideoStreamingHTML :
            mediaType = "httpstream";
            mediaTypeString = QObject::tr("HTTP Streaming");
            break;
        case kProgramInfoTypeVideoStreamingRTSP :
            mediaType = "rtspstream";
            mediaTypeString = QObject::tr("RTSP Streaming");
            break;
        case kProgramInfoTypeVideoBD :
            mediaType = "bluraydisc";
            mediaTypeString = QObject::tr("Blu-ray Disc");
            break;
        case kProgramInfoTypeRecording : // Fall through
        default :
            mediaType = "recording";
            mediaTypeString = QObject::tr("Recording",
                                          "Recorded file, object not action");
    }
    progMap["mediatype"] = mediaType;
    progMap["mediatypestring"] = mediaTypeString;
}

/// \brief Returns length of program/recording in seconds.
uint ProgramInfo::GetSecondsInRecording(void) const
{
    int64_t recsecs = recstartts.secsTo(endts);
    int64_t duration = startts.secsTo(endts);
    return (uint) ((recsecs>0) ? recsecs : max(duration,int64_t(0)));
}

/// \brief Returns catType as a string
QString ProgramInfo::GetCategoryTypeString(void) const
{
    return myth_category_type_to_string(catType); 
}

/// \brief Returns last frame in position map or 0
uint64_t ProgramInfo::QueryLastFrameInPosMap(void) const
{
    uint64_t last_frame = 0;
    frm_pos_map_t posMap;
    QueryPositionMap(posMap, MARK_GOP_BYFRAME);
    if (posMap.empty())
    {
        QueryPositionMap(posMap, MARK_GOP_START);
        if (posMap.empty())
            QueryPositionMap(posMap, MARK_KEYFRAME);
    }
    if (!posMap.empty())
    {
        frm_pos_map_t::const_iterator it = posMap.constEnd();
        --it;
        last_frame = it.key();
    }
    return last_frame;
}

bool ProgramInfo::IsGeneric(void) const
{
    return
        (programid.isEmpty() && subtitle.isEmpty() &&
         description.isEmpty()) ||
        (!programid.isEmpty() && programid.endsWith("0000")
         && catType == kCategorySeries);
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
                .arg(GetChanID()).arg(GetRecordingStartTime(MythDate::ISODate));
            break;
        case kSchedulingKey:
            str = QString("%1 @ %2")
                .arg(GetChanID()).arg(GetScheduledStartTime(MythDate::ISODate));
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
        catType = kCategoryNone;
        lastInUseTime = MythDate::current().addSecs(-4 * 60 * 60);
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
    season       = query.value(3).toUInt();
    episode      = query.value(4).toUInt();
    syndicatedepisode = query.value(48).toString();
    category     = query.value(5).toString();

    chanid       = _chanid;
    chanstr      = QString("#%1").arg(chanid);
    chansign     = chanstr;
    channame     = chanstr;
    chanplaybackfilters.clear();
    if (!query.value(7).toString().isEmpty())
    {
        chanstr  = query.value(7).toString();
        chansign = query.value(8).toString();
        channame = query.value(9).toString();
        chanplaybackfilters = query.value(10).toString();
    }

    recgroup     = query.value(11).toString();
    playgroup    = query.value(12).toString();

    // We don't want to update the pathname if the basename is
    // the same as we may have already expanded pathname from
    // a simple basename to a localized path.
    QString new_basename = query.value(14).toString();
    if ((GetBasename() != new_basename) || !is_reload)
    {
        if (is_reload)
        {
            LOG(VB_FILE, LOG_INFO, LOC +
                QString("Updated pathname '%1':'%2' -> '%3'")
                    .arg(pathname).arg(GetBasename()).arg(new_basename));
        }
        SetPathname(new_basename);
    }

    hostname     = query.value(15).toString();
    storagegroup = query.value(13).toString();

    seriesid     = query.value(17).toString();
    programid    = query.value(18).toString();
    inetref      = query.value(19).toString();
    /**///catType;

    recpriority  = query.value(16).toInt();

    filesize     = query.value(20).toULongLong();

    startts      = MythDate::as_utc(query.value(21).toDateTime());
    endts        = MythDate::as_utc(query.value(22).toDateTime());
    recstartts   = MythDate::as_utc(query.value(24).toDateTime());
    recendts     = MythDate::as_utc(query.value(25).toDateTime());

    stars        = clamp((float)query.value(23).toDouble(), 0.0f, 1.0f);

    year         = query.value(26).toUInt();
    partnumber   = query.value(49).toUInt();
    parttotal    = query.value(50).toUInt();

    originalAirDate = query.value(27).toDate();
    lastmodified = MythDate::as_utc(query.value(28).toDateTime());
    /**///lastInUseTime;

    recstatus    = rsRecorded;
    /**///oldrecstatus;

    /**///prefinput;
    /**///recpriority2;

    recordid     = query.value(29).toUInt();
    /**///parentid;

    /**///sourcid;
    /**///inputid;
    /**///cardid;
    findid       = query.value(45).toUInt();

    /**///rectype;
    dupin        = RecordingDupInType(query.value(46).toInt());
    dupmethod    = RecordingDupMethodType(query.value(47).toInt());

    // ancillary data -- begin
    set_flag(programflags, FL_CHANCOMMFREE,
             query.value(30).toInt() == COMM_DETECT_COMMFREE);
    set_flag(programflags, FL_COMMFLAG,
             query.value(31).toInt() == COMM_FLAG_DONE);
    set_flag(programflags, FL_COMMPROCESSING ,
             query.value(31).toInt() == COMM_FLAG_PROCESSING);
    set_flag(programflags, FL_REPEAT,        query.value(32).toBool());
    set_flag(programflags, FL_TRANSCODED,
             query.value(34).toInt() == TRANSCODING_COMPLETE);
    set_flag(programflags, FL_DELETEPENDING, query.value(35).toBool());
    set_flag(programflags, FL_PRESERVED,     query.value(36).toBool());
    set_flag(programflags, FL_CUTLIST,       query.value(37).toBool());
    set_flag(programflags, FL_AUTOEXP,       query.value(38).toBool());
    set_flag(programflags, FL_REALLYEDITING, query.value(39).toBool());
    set_flag(programflags, FL_BOOKMARK,      query.value(40).toBool());
    set_flag(programflags, FL_WATCHED,       query.value(41).toBool());
    set_flag(programflags, FL_EDITING,
             (programflags & FL_REALLYEDITING) ||
             (programflags & FL_COMMPROCESSING));

    properties = ((query.value(44).toUInt() << kSubtitlePropertyOffset) |
                  (query.value(43).toUInt() << kVideoPropertyOffset)    |
                  (query.value(42).toUInt() << kAudioPropertyOffset));
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
    if (GetRecordingRuleType() == kOneRecord)
        return recordid == other.recordid;

    if (findid && findid == other.findid &&
        (recordid == other.recordid || recordid == other.parentid))
           return true;

    if (dupmethod & kDupCheckNone)
        return false;

    if (title.compare(other.title, Qt::CaseInsensitive) != 0)
        return false;

    if (catType == kCategorySeries)
    {
        if (programid.endsWith("0000"))
            return false;
    }

    if (!programid.isEmpty() && !other.programid.isEmpty())
    {
        if (usingProgIDAuth)
        {
            int index = programid.indexOf('/');
            int oindex = other.programid.indexOf('/');
            if (index == oindex && (index < 0 ||
                programid.leftRef(index) == other.programid.leftRef(oindex)))
                return programid == other.programid;
        }
        else
        {
            return programid == other.programid;
        }
    }

    if ((dupmethod & kDupCheckSub) &&
        ((subtitle.isEmpty()) ||
         (subtitle.compare(other.subtitle, Qt::CaseInsensitive) != 0)))
        return false;

    if ((dupmethod & kDupCheckDesc) &&
        ((description.isEmpty()) ||
         (description.compare(other.description, Qt::CaseInsensitive) != 0)))
        return false;

    if ((dupmethod & kDupCheckSubThenDesc) &&
        ((subtitle.isEmpty() &&
          ((!other.subtitle.isEmpty() &&
            description.compare(other.subtitle, Qt::CaseInsensitive) != 0) ||
           (other.subtitle.isEmpty() &&
            description.compare(other.description, Qt::CaseInsensitive) != 0))) ||
         (!subtitle.isEmpty() &&
          ((other.subtitle.isEmpty() &&
            subtitle.compare(other.description, Qt::CaseInsensitive) != 0) ||
           (!other.subtitle.isEmpty() &&
            subtitle.compare(other.subtitle, Qt::CaseInsensitive) != 0)))))
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
    if (title.compare(other.title, Qt::CaseInsensitive) != 0)
        return false;
    if (startts == other.startts &&
        (chanid == other.chanid ||
         (!chansign.isEmpty() &&
          chansign.compare(other.chansign, Qt::CaseInsensitive) == 0)))
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
    if (title.compare(other.title, Qt::CaseInsensitive) != 0)
        return false;
    if ((chanid == other.chanid ||
         (!chansign.isEmpty() &&
          chansign.compare(other.chansign, Qt::CaseInsensitive) == 0)) &&
        startts < other.endts &&
        endts > other.startts)
        return true;

    return false;
}

void ProgramInfo::CheckProgramIDAuthorities(void)
{
    QMap<QString, int> authMap;
    QString tables[] = { "program", "recorded", "oldrecorded", "" };
    MSqlQuery query(MSqlQuery::InitCon());

    int tableIndex = 0;
    QString table = tables[tableIndex];
    while (!table.isEmpty())
    {
        query.prepare(QString(
            "SELECT DISTINCT LEFT(programid, LOCATE('/', programid)) "
            "FROM %1 WHERE programid <> ''").arg(table));
        if (!query.exec())
            MythDB::DBError("CheckProgramIDAuthorities", query);
        else
        {
            while (query.next())
                authMap[query.value(0).toString()] = 1;
        }
        ++tableIndex;
        table = tables[tableIndex];
    }

    int numAuths = authMap.count();
    LOG(VB_GENERAL, LOG_INFO,
        QString("Found %1 distinct programid authorities").arg(numAuths));

    usingProgIDAuth = (numAuths > 1);
}

/** \fn ProgramInfo::CreateRecordBasename(const QString &ext) const
 *  \brief Returns a filename for a recording based on the
 *         recording channel and date.
 */
QString ProgramInfo::CreateRecordBasename(const QString &ext) const
{
    QString starts = MythDate::toString(recstartts, MythDate::kFilename);

    QString retval = QString("%1_%2.%3").arg(chanid)
                             .arg(starts).arg(ext);

    return retval;
}

static ProgramInfoType discover_program_info_type(
    uint chanid, const QString &pathname, bool use_remote)
{
    QString fn_lower = pathname.toLower();
    ProgramInfoType pit = kProgramInfoTypeVideoFile;
    if (chanid)
        pit = kProgramInfoTypeRecording;
    else if (fn_lower.startsWith("http:"))
        pit = kProgramInfoTypeVideoStreamingHTML;
    else if (fn_lower.startsWith("rtsp:"))
        pit = kProgramInfoTypeVideoStreamingRTSP;
    else
    {
        if (fn_lower.startsWith("dvd:") ||
            fn_lower.endsWith(".iso") ||
            fn_lower.endsWith(".img") ||
            ((pathname.startsWith("/")) &&
             QDir(pathname + "/VIDEO_TS").exists()))
        {
            pit = kProgramInfoTypeVideoDVD;
        }
        else if (fn_lower.startsWith("bd:") ||
                 ((pathname.startsWith("/")) &&
                  QDir(pathname + "/BDMV").exists()))
        {
            pit = kProgramInfoTypeVideoBD;
        }
        else if (use_remote && fn_lower.startsWith("myth://"))
        {
            QString tmpFileDVD = pathname + "/VIDEO_TS";
            QString tmpFileBD = pathname + "/BDMV";
            if (RemoteFile::Exists(tmpFileDVD))
                pit = kProgramInfoTypeVideoDVD;
            else if (RemoteFile::Exists(tmpFileBD))
                pit = kProgramInfoTypeVideoBD;
        }
    }
    return pit;
}

void ProgramInfo::SetPathname(const QString &pn) const
{
    pathname = pn;
    pathname.detach();

    ProgramInfoType pit = discover_program_info_type(chanid, pathname, false);
    const_cast<ProgramInfo*>(this)->SetProgramInfoType(pit);
}

ProgramInfoType ProgramInfo::DiscoverProgramInfoType(void) const
{
    return discover_program_info_type(chanid, pathname, true);
}

void ProgramInfo::SetAvailableStatus(
    AvailableStatusType status, const QString &where)
{
    if (status != availableStatus)
    {
        LOG(VB_GUI, LOG_INFO,
                 toString(kTitleSubtitle) + QString(": %1 -> %2")
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
        LOG(VB_GENERAL, LOG_INFO,
                 QString("QueryBasename found no entry for %1 @ %2")
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
        // return the original path if BD or DVD URI
    if (IsVideoBD() || IsVideoDVD())
        return GetPathname();

    QString basename = QueryBasename();
    if (basename.isEmpty())
        return "";

    bool checklocal = !gCoreContext->GetNumSetting("AlwaysStreamFiles", 0) ||
                      forceCheckLocal;

    if (IsVideo())
    {
        QString fullpath = GetPathname();
        if (!fullpath.startsWith("myth://", Qt::CaseInsensitive) || !checklocal)
            return fullpath;

        QUrl    url  = QUrl(fullpath);
        QString path = url.path();
        QString host = url.toString(QUrl::RemovePath).mid(7);
        QStringList list = host.split(":", QString::SkipEmptyParts);
        if (list.size())
        {
            host = list[0];
            list = host.split("@", QString::SkipEmptyParts);
            QString group;
            if (list.size() > 0 && list.size() < 3)
            {
                host  = list.size() == 1 ? list[0]   : list[1];
                group = list.size() == 1 ? QString() : list[0];
                StorageGroup sg = StorageGroup(group, host);
                QString local = sg.FindFile(path);
                if (!local.isEmpty() && sg.FileExists(local))
                    return local;
            }
        }
        return fullpath;
    }

    QString tmpURL;
    if (checklocal)
    {
        // Check to see if the file exists locally
        StorageGroup sgroup(storagegroup);
#if 0
        LOG(VB_FILE, LOG_DEBUG, LOC +
            QString("GetPlaybackURL: CHECKING SG : %1 : ").arg(tmpURL));
#endif
        tmpURL = sgroup.FindFile(basename);

        if (!tmpURL.isEmpty())
        {
            LOG(VB_FILE, LOG_INFO, LOC +
                QString("GetPlaybackURL: File is local: '%1'") .arg(tmpURL));
            return tmpURL;
        }
        else if (hostname == gCoreContext->GetHostName())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
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
        tmpURL = gCoreContext->GenMythURL(gCoreContext->GetSetting("MasterServerIP"),
                                          gCoreContext->GetSetting("MasterServerPort").toInt(),
                                          basename);

        LOG(VB_FILE, LOG_INFO, LOC +
            QString("GetPlaybackURL: Found @ '%1'").arg(tmpURL));
        return tmpURL;
        }

    // Fallback to streaming from the backend the recording was created on
    tmpURL = gCoreContext->GenMythURL(gCoreContext->GetBackendServerIP(hostname),
                                      gCoreContext->GetSettingOnHost("BackendServerPort", hostname).toInt(),
                                      basename);

    LOG(VB_FILE, LOG_INFO, LOC +
        QString("GetPlaybackURL: Using default of: '%1'") .arg(tmpURL));

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
        ts = MythDate::as_utc(query.value(0).toDateTime());

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
    const QString &serialid) const
{
    QStringList fields = QStringList();
    MSqlQuery query(MSqlQuery::InitCon());

    if (!(programflags & FL_IGNOREBOOKMARK))
    {
        query.prepare(" SELECT dvdstate, title, framenum, audionum, subtitlenum "
                        " FROM dvdbookmark "
                        " WHERE serialid = :SERIALID ");
        query.bindValue(":SERIALID", serialid);

        if (query.exec() && query.next())
        {
            QString dvdstate = query.value(0).toString();

            if (!dvdstate.isEmpty())
            {
                fields.append(dvdstate);
            }
            else
            {
                // Legacy bookmark
                for(int i = 1; i < 5; i++)
                    fields.append(query.value(i).toString());
            }
        }
    }

    return fields;
}

void ProgramInfo::SaveDVDBookmark(const QStringList &fields) const
{
    QStringList::const_iterator it = fields.begin();
    MSqlQuery query(MSqlQuery::InitCon());

    QString serialid    = *(it);
    QString name        = *(++it);

    if( fields.count() == 3 )
    {
        // We have a state field, so update/create the bookmark
        QString state = *(++it);

        query.prepare("INSERT IGNORE INTO dvdbookmark "
                        " (serialid, name)"
                        " VALUES ( :SERIALID, :NAME );");
        query.bindValue(":SERIALID", serialid);
        query.bindValue(":NAME", name);

        if (!query.exec())
            MythDB::DBError("SetDVDBookmark inserting", query);

        query.prepare(" UPDATE dvdbookmark "
                        " SET dvdstate    = :STATE , "
                        "     timestamp   = NOW() "
                        " WHERE serialid = :SERIALID");
        query.bindValue(":STATE",state);
        query.bindValue(":SERIALID",serialid);
    }
    else
    {
        // No state field, delete the bookmark
        query.prepare("DELETE FROM dvdbookmark "
                        "WHERE serialid = :SERIALID");
        query.bindValue(":SERIALID",serialid);
    }

    if (!query.exec())
        MythDB::DBError("SetDVDBookmark updating", query);
}

/** \brief Queries recordedprogram to get the category_type of the
 *         recording.
 *
 *  \return string category_type
 */
ProgramInfo::CategoryType ProgramInfo::QueryCategoryType(void) const
{
    CategoryType ret = kCategoryNone;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(" SELECT category_type "
                  " FROM recordedprogram "
                  " WHERE chanid = :CHANID "
                  " AND starttime = :STARTTIME;");

    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", startts);

    if (query.exec() && query.next())
    {
        ret = string_to_myth_category_type(query.value(0).toString());
    }

    return ret;
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
                  " SET deletepending = :DELETEFLAG, "
                  "     duplicate = 0 "
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

    QDateTime oneHourAgo = MythDate::current().addSecs(-61 * 60);
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

    // gCoreContext->GetNumSetting("AutoExpireInsteadOfDelete", 0) &&
    if (GetRecordingGroup() != "Deleted" && GetRecordingGroup() != "LiveTV")
        return true;

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
        QDateTime timeNow = MythDate::current();
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

bool ProgramInfo::QueryCutList(frm_dir_map_t &delMap, bool loadAutoSave) const
{
    if (loadAutoSave)
    {
        frm_dir_map_t autosaveMap;
        QueryMarkupMap(autosaveMap, MARK_TMP_CUT_START);
        QueryMarkupMap(autosaveMap, MARK_TMP_CUT_END, true);
        QueryMarkupMap(autosaveMap, MARK_PLACEHOLDER, true);
        // Convert the temporary marks into regular marks.
        delMap.clear();
        frm_dir_map_t::const_iterator i = autosaveMap.constBegin();
        for (; i != autosaveMap.constEnd(); ++i)
        {
            uint64_t frame = i.key();
            MarkTypes mark = i.value();
            if (mark == MARK_TMP_CUT_START)
                mark = MARK_CUT_START;
            else if (mark == MARK_TMP_CUT_END)
                mark = MARK_CUT_END;
            delMap[frame] = mark;
        }
    }
    else
    {
        QueryMarkupMap(delMap, MARK_CUT_START);
        QueryMarkupMap(delMap, MARK_CUT_END, true);
        QueryMarkupMap(delMap, MARK_PLACEHOLDER, true);
    }

    return !delMap.isEmpty();
}

void ProgramInfo::SaveCutList(frm_dir_map_t &delMap, bool isAutoSave) const
{
    if (!isAutoSave)
    {
        ClearMarkupMap(MARK_CUT_START);
        ClearMarkupMap(MARK_CUT_END);
    }
    ClearMarkupMap(MARK_PLACEHOLDER);
    ClearMarkupMap(MARK_TMP_CUT_START);
    ClearMarkupMap(MARK_TMP_CUT_END);

    frm_dir_map_t tmpDelMap;
    frm_dir_map_t::const_iterator i = delMap.constBegin();
    for (; i != delMap.constEnd(); ++i)
    {
        uint64_t frame = i.key();
        MarkTypes mark = i.value();
        if (isAutoSave)
        {
            if (mark == MARK_CUT_START)
                mark = MARK_TMP_CUT_START;
            else if (mark == MARK_CUT_END)
                mark = MARK_TMP_CUT_END;
        }
        tmpDelMap[frame] = mark;
    }
    SaveMarkupMap(tmpDelMap);

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


/// \brief Store the Total Duration at frame 0 in the recordedmarkup table
void ProgramInfo::SaveTotalDuration(int64_t duration)
{
    if (!IsRecording())
        return;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("DELETE FROM recordedmarkup "
                  " WHERE chanid=:CHANID "
                  " AND starttime=:STARTTIME "
                  " AND type=:TYPE");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);
    query.bindValue(":TYPE", MARK_DURATION_MS);

    if (!query.exec())
        MythDB::DBError("Duration delete", query);

    query.prepare("INSERT INTO recordedmarkup"
                  "    (chanid, starttime, mark, type, data)"
                  "    VALUES"
                  " ( :CHANID, :STARTTIME, 0, :TYPE, :DATA);");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);
    query.bindValue(":TYPE", MARK_DURATION_MS);
    query.bindValue(":DATA", (uint)(duration / 1000));

    if (!query.exec())
        MythDB::DBError("Duration insert", query);
}

/// \brief Store the Total Frames at frame 0 in the recordedmarkup table
void ProgramInfo::SaveTotalFrames(int64_t frames)
{
    if (!IsRecording())
        return;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("DELETE FROM recordedmarkup "
                  " WHERE chanid=:CHANID "
                  " AND starttime=:STARTTIME "
                  " AND type=:TYPE");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);
    query.bindValue(":TYPE", MARK_TOTAL_FRAMES);

    if (!query.exec())
        MythDB::DBError("Frames delete", query);

    query.prepare("INSERT INTO recordedmarkup"
                  "    (chanid, starttime, mark, type, data)"
                  "    VALUES"
                  " ( :CHANID, :STARTTIME, 0, :TYPE, :DATA);");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);
    query.bindValue(":TYPE", MARK_TOTAL_FRAMES);
    query.bindValue(":DATA", (uint)(frames));

    if (!query.exec())
        MythDB::DBError("Total Frames insert", query);
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

MarkTypes ProgramInfo::QueryAverageAspectRatio(void ) const
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT recordedmarkup.type "
                "FROM recordedmarkup "
                "WHERE recordedmarkup.chanid    = :CHANID    AND "
                "      recordedmarkup.starttime = :STARTTIME AND "
                "      recordedmarkup.type      >= :ASPECTSTART AND "
                "      recordedmarkup.type      <= :ASPECTEND "
                "GROUP BY recordedmarkup.type "
                "ORDER BY SUM( ( SELECT IFNULL(rm.mark, recordedmarkup.mark)"
                "                FROM recordedmarkup AS rm "
                "                WHERE rm.chanid    = recordedmarkup.chanid    AND "
                "                      rm.starttime = recordedmarkup.starttime AND "
                "                      rm.type      = recordedmarkup.type      AND "
                "                      rm.mark      > recordedmarkup.mark "
                "                ORDER BY rm.mark ASC LIMIT 1 "
                "              ) - recordedmarkup.mark "
                "            ) DESC "
                "LIMIT 1");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);
    query.bindValue(":ASPECTSTART", MARK_ASPECT_4_3); // 11
    query.bindValue(":ASPECTEND", MARK_ASPECT_CUSTOM); // 14

    if (!query.exec())
    {
        MythDB::DBError("QueryAverageAspectRatio", query);
        return MARK_UNSET;
    }

    if (!query.next())
        return MARK_UNSET;

    return static_cast<MarkTypes>(query.value(0).toInt());
}

/** \brief If present in recording this loads total duration of the
 *         main video stream from database's stream markup table.
 */
int64_t ProgramInfo::QueryTotalDuration(void) const
{
    if (gCoreContext->IsDatabaseIgnored())
        return 0LL;
    int64_t msec = load_markup_datum(MARK_DURATION_MS, chanid, recstartts);
    return msec * 1000;
}

/** \brief If present in recording this loads total frames of the
 *         main video stream from database's stream markup table.
 */
int64_t ProgramInfo::QueryTotalFrames(void) const
{
    int64_t frames = load_markup_datum(MARK_TOTAL_FRAMES, chanid, recstartts);
    return frames;
}

void ProgramInfo::SaveVideoProperties(uint mask, uint vid_flags)
{
    MSqlQuery query(MSqlQuery::InitCon());

    LOG(VB_RECORD, LOG_INFO,
        QString("SaveVideoProperties(0x%1, 0x%2)")
        .arg(mask,2,16,QChar('0')).arg(vid_flags,2,16,QChar('0')));

    query.prepare(
        "UPDATE recordedprogram "
        "SET videoprop = ((videoprop+0) & :OTHERFLAGS) | :FLAGS "
        "WHERE chanid = :CHANID AND starttime = :STARTTIME");

    query.bindValue(":OTHERFLAGS", ~mask);
    query.bindValue(":FLAGS",      vid_flags);
    query.bindValue(":CHANID",     chanid);
    query.bindValue(":STARTTIME",  startts);
    if (!query.exec())
    {
        MythDB::DBError("SaveVideoProperties", query);
        return;
    }

    uint videoproperties = GetVideoProperties();
    videoproperties &= ~mask;
    videoproperties |= vid_flags;
    properties &= ~kVideoPropertyMask;
    properties |= videoproperties << kVideoPropertyOffset;

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
    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("UpdateInUseMark(%1) '%2'")
             .arg(force?"force":"no force").arg(inUseForWhat));
#endif

    if (!IsRecording())
        return;

    if (inUseForWhat.isEmpty())
        return;

    if (force || lastInUseTime.secsTo(MythDate::current()) > 15 * 60)
        MarkAsInUse(true);
}

void ProgramInfo::SaveSeasonEpisode(uint seas, uint ep)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(
        "UPDATE recorded "
        "SET season = :SEASON, episode = :EPISODE "
        "WHERE chanid = :CHANID AND starttime = :STARTTIME "
        "AND recordid = :RECORDID");

    query.bindValue(":SEASON",     seas);
    query.bindValue(":EPISODE",    ep);
    query.bindValue(":CHANID",     chanid);
    query.bindValue(":STARTTIME",  recstartts);
    query.bindValue(":RECORDID",   recordid);
    if (!query.exec())
    {
        MythDB::DBError("SaveSeasonEpisode", query);
        return;
    }

    SendUpdateEvent();
}

void ProgramInfo::SaveInetRef(const QString &inet)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(
        "UPDATE recorded "
        "SET inetref = :INETREF "
        "WHERE chanid = :CHANID AND starttime = :STARTTIME "
        "AND recordid = :RECORDID");

    query.bindValue(":INETREF",    inet);
    query.bindValue(":CHANID",     chanid);
    query.bindValue(":STARTTIME",  recstartts);
    query.bindValue(":RECORDID",   recordid);
    query.exec();

    SendUpdateEvent();
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
            return "";

        QString path = GetPlaybackURL(false, true);
        if (path.startsWith("/"))
        {
            QFileInfo testFile(path);
            return testFile.path();
        }

        return "";
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

    return "";
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
                LOG(VB_GENERAL, LOG_INFO, LOC +
                    QString("MarkAsInUse(true, '%1'->'%2')")
                        .arg(inUseForWhat).arg(usedFor) +
                    " -- use has changed");
            }
            else
            {
                LOG(VB_GENERAL, LOG_INFO, LOC +
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
            LOG(VB_GENERAL, LOG_WARNING, LOC +
                QString("MarkAsInUse(true, ''->'%1')").arg(inUseForWhat) +
                " -- use was not explicitly set");
        }

        notifyOfChange = true;
    }

    if (!inuse && !inUseForWhat.isEmpty() && usedFor != inUseForWhat)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("MarkAsInUse(false, '%1'->'%2')")
                .arg(inUseForWhat).arg(usedFor) +
            " -- use has changed since first setting as in use.");
    }
#ifdef DEBUG_IN_USE
    else if (!inuse)
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("MarkAsInUse(false, '%1')")
                 .arg(inUseForWhat));
    }
#endif // DEBUG_IN_USE

    if (!inuse && inUseForWhat.isEmpty())
        inUseForWhat = usedFor;

    if (!inuse && inUseForWhat.isEmpty())
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
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
        lastInUseTime = MythDate::current(true).addSecs(-4 * 60 * 60);
        SendUpdateEvent();
        return;
    }

    if (pathname == GetBasename())
        pathname = GetPlaybackURL(false, true);

    QString recDir = DiscoverRecordingDirectory();

    QDateTime inUseTime = MythDate::current(true);

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
        LOG(VB_GENERAL, LOG_ERR, LOC + "MarkAsInUse -- select query failed");
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
        query.bindValue(":RECHOST",
                        hostname.isEmpty() ? gCoreContext->GetHostName()
                                           : hostname);
        query.bindValue(":RECDIR",     recDir);

        if (!query.exec())
            MythDB::DBError("MarkAsInUse -- insert failed", query);
        else
            lastInUseTime = inUseTime;
    }

    if (!notifyOfChange)
        return;

    // Let others know we changed status
    QDateTime oneHourAgo = MythDate::current().addSecs(-61 * 60);
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

/** \brief Returns the display name of the card input for this program.
 *  \note Ideally this would call CardUtil::GetDisplayName(), but
 *        that's in libmythtv.  Dupliacte code for now until a better
 *        solution can be found.
 */
QString ProgramInfo::QueryInputDisplayName(void) const
{
    if (!inputid)
        return QString::null;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT displayname, cardid, inputname "
                  "FROM cardinput "
                  "WHERE cardinputid = :INPUTID");
    query.bindValue(":INPUTID", inputid);

    if (!query.exec())
        MythDB::DBError("ProgramInfo::GetInputDisplayName(uint)", query);
    else if (query.next())
    {
        QString result = query.value(0).toString();
        if (result.isEmpty())
            result = QString("%1: %2").arg(query.value(1).toInt())
                                      .arg(query.value(2).toString());
        return result;
    }

    return QString::null;
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

    QString special_program_groups =
        QObject::tr("All Programs - %1",
                    "Show all programs from a specific recording group");

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
            special_program_groups.length() +
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
    if (pburl.startsWith("myth://"))
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
    str.replace(QString("%SEASON%"), QString::number(season));
    str.replace(QString("%EPISODE%"), QString::number(episode));
    str.replace(QString("%SYNDICATEDEPISODE%"), syndicatedepisode);
    str.replace(QString("%DESCRIPTION%"), description);
    str.replace(QString("%HOSTNAME%"), hostname);
    str.replace(QString("%CATEGORY%"), category);
    str.replace(QString("%RECGROUP%"), recgroup);
    str.replace(QString("%PLAYGROUP%"), playgroup);
    str.replace(QString("%CHANID%"), QString::number(chanid));
    str.replace(QString("%INETREF%"), inetref);
    str.replace(QString("%PARTNUMBER%"), QString::number(partnumber));
    str.replace(QString("%PARTTOTAL%"), QString::number(parttotal));
    str.replace(QString("%ORIGINALAIRDATE%"),
                originalAirDate.toString(Qt::ISODate));
    static const char *time_str[] =
        { "STARTTIME", "ENDTIME", "PROGSTART", "PROGEND", };
    const QDateTime *time_dtr[] =
        { &recstartts, &recendts, &startts,    &endts,    };
    for (uint i = 0; i < sizeof(time_str)/sizeof(char*); i++)
    {
        str.replace(QString("%%1%").arg(time_str[i]),
                    (time_dtr[i]->toLocalTime()).toString("yyyyMMddhhmmss"));
        str.replace(QString("%%1ISO%").arg(time_str[i]),
                    (time_dtr[i]->toLocalTime()).toString(Qt::ISODate));
        str.replace(QString("%%1UTC%").arg(time_str[i]),
                    time_dtr[i]->toString("yyyyMMddhhmmss"));
        str.replace(QString("%%1ISOUTC%").arg(time_str[i]),
                    time_dtr[i]->toString(Qt::ISODate));
    }
}

QMap<QString,uint32_t> ProgramInfo::QueryInUseMap(void)
{
    QMap<QString, uint32_t> inUseMap;
    QDateTime oneHourAgo = MythDate::current().addSecs(-61 * 60);

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT DISTINCT chanid, starttime, recusage "
                  "FROM inuseprograms WHERE lastupdatetime >= :ONEHOURAGO");
    query.bindValue(":ONEHOURAGO", oneHourAgo);

    if (!query.exec())
        return inUseMap;

    while (query.next())
    {
        QString inUseKey = ProgramInfo::MakeUniqueKey(
            query.value(0).toUInt(),
            MythDate::as_utc(query.value(1).toDateTime()));

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
        QDateTime recstartts = MythDate::as_utc(query.value(1).toDateTime());
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

    MythScheduler *sched = gCoreContext->GetScheduler();
    if (sched && tmptable.isEmpty())
    {
        sched->GetAllPending(slist);
        return slist;
    }

    if (sched)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "Called from master backend\n\t\t\t"
            "with recordid or tmptable, this is not currently supported");
        return slist;
    }

    slist.push_back(
        (tmptable.isEmpty()) ?
        QString("QUERY_GETALLPENDING") :
        QString("QUERY_GETALLPENDING %1 %2").arg(tmptable).arg(recordid));

    if (!gCoreContext->SendReceiveStringList(slist) || slist.size() < 2)
    {
        LOG(VB_GENERAL, LOG_ALERT,
                 "LoadFromScheduler(): Error querying master.");
        slist.clear();
    }

    return slist;
}

static bool FromProgramQuery(
    const QString &sql, const MSqlBindings &bindings, MSqlQuery &query)
{
    QString querystr = QString(
        "SELECT program.description, sub.* FROM program, ("
        "SELECT DISTINCT program.chanid, program.starttime, program.endtime, "
        "    program.title, program.subtitle, "
        "    program.category, channel.channum, channel.callsign, "
        "    channel.name, program.previouslyshown, channel.commmethod, "
        "    channel.outputfilters, program.seriesid, program.programid, "
        "    program.airdate, program.stars, program.originalairdate, "
        "    program.category_type, oldrecstatus.recordid, "
        "    oldrecstatus.rectype, oldrecstatus.recstatus, "
        "    oldrecstatus.findid, program.videoprop+0, program.audioprop+0, "
        "    program.subtitletypes+0, program.syndicatedepisodenumber, "
        "    program.partnumber, program.parttotal "
        "FROM program "
        "LEFT JOIN channel ON program.chanid = channel.chanid "
        "LEFT JOIN oldrecorded AS oldrecstatus ON "
        "    oldrecstatus.future = 0 AND "
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

    querystr += " ) AS sub WHERE program.chanid=sub.chanid AND program.starttime=sub.starttime";
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
    const ProgramList &schedList)
{
    destination.clear();

    MSqlQuery query(MSqlQuery::InitCon());
    if (!FromProgramQuery(sql, bindings, query))
        return false;

    while (query.next())
    {
        destination.push_back(
            new ProgramInfo(
                query.value(4).toString(), // title
                query.value(5).toString(), // subtitle
                query.value(0).toString(), // description
                query.value(26).toString(), // syndicatedepisodenumber
                query.value(6).toString(), // category

                query.value(1).toUInt(), // chanid
                query.value(7).toString(), // channum
                query.value(8).toString(), // chansign
                query.value(9).toString(), // channame
                query.value(12).toString(), // chanplaybackfilters

                MythDate::as_utc(query.value(2).toDateTime()), // startts
                MythDate::as_utc(query.value(3).toDateTime()), // endts
                MythDate::as_utc(query.value(2).toDateTime()), // recstartts
                MythDate::as_utc(query.value(3).toDateTime()), // recendts

                query.value(13).toString(), // seriesid
                query.value(14).toString(), // programid
                string_to_myth_category_type(query.value(18).toString()), // catType

                query.value(16).toDouble(), // stars
                query.value(15).toUInt(), // year
                query.value(27).toUInt(), // partnumber
                query.value(28).toUInt(), // parttotal
                query.value(17).toDate(), // originalAirDate
                RecStatusType(query.value(21).toInt()), // recstatus
                query.value(19).toUInt(), // recordid
                RecordingType(query.value(20).toInt()), // rectype
                query.value(22).toUInt(), // findid

                query.value(11).toInt() == COMM_DETECT_COMMFREE, // commfree
                query.value(10).toInt(), // repeat
                query.value(23).toInt(), // videoprop
                query.value(24).toInt(), // audioprop
                query.value(25).toInt(), // subtitletypes

                schedList));
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
        "       title, subtitle, description, season, episode, category, seriesid, "
        "       programid, inetref, channel.channum, channel.callsign, "
        "       channel.name, findid, rectype, recstatus, recordid, "
        "       duplicate "
        " FROM oldrecorded "
        " LEFT JOIN channel ON oldrecorded.chanid = channel.chanid "
        " WHERE oldrecorded.future = 0 "
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
        if (!query.value(12).toString().isEmpty())
        {
            channum  = query.value(12).toString();
            chansign = query.value(13).toString();
            channame = query.value(14).toString();
        }

        destination.push_back(new ProgramInfo(
            query.value(3).toString(),
            query.value(4).toString(),
            query.value(5).toString(),
            query.value(6).toUInt(),
            query.value(7).toUInt(),
            query.value(8).toString(),

            chanid, channum, chansign, channame,

            query.value(9).toString(), query.value(10).toString(),
            query.value(11).toString(),

            MythDate::as_utc(query.value(1).toDateTime()),
            MythDate::as_utc(query.value(2).toDateTime()),
            MythDate::as_utc(query.value(1).toDateTime()),
            MythDate::as_utc(query.value(2).toDateTime()),

            RecStatusType(query.value(17).toInt()),
            query.value(18).toUInt(),
            RecordingType(query.value(16).toInt()),
            query.value(15).toUInt(),

            query.value(19).toInt()));
    }

    return true;
}

/** \fn ProgramInfo::LoadFromRecorded(void)
 *  \brief Load a ProgramList from the recorded table.
 *  \param destination     ProgramList to fill
 *  \param possiblyInProgressRecordingsOnly  return only in-progress
 *                                           recordings or empty list
 *  \param inUseMap        in-use programs map
 *  \param isJobRunning    job map
 *  \param recMap          recording map
 *  \param sort            sort order, negative for descending, 0 for
 *                         unsorted, positive for ascending
 *  \return true if it succeeds, false if it fails.
 *  \sa QueryInUseMap(void)
 *      QueryJobsRunning(int)
 *      Scheduler::GetRecording()
 */
bool LoadFromRecorded(
    ProgramList &destination,
    bool possiblyInProgressRecordingsOnly,
    const QMap<QString,uint32_t> &inUseMap,
    const QMap<QString,bool> &isJobRunning,
    const QMap<QString, ProgramInfo*> &recMap,
    int sort)
{
    destination.clear();

    QString     fs_db_name = "";
    QDateTime   rectime    = MythDate::current().addSecs(
        -gCoreContext->GetNumSetting("RecordOverTime"));

    // ----------------------------------------------------------------------

    QString thequery = ProgramInfo::kFromRecordedQuery;
    if (possiblyInProgressRecordingsOnly)
        thequery += "WHERE r.endtime >= NOW() AND r.starttime <= NOW() ";

    if (sort)
        thequery += "ORDER BY r.starttime ";
    if (sort < 0)
        thequery += "DESC ";

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(thequery);

    if (!query.exec())
    {
        MythDB::DBError("ProgramList::FromRecorded", query);
        return true;
    }

    while (query.next())
    {
        const uint chanid = query.value(6).toUInt();
        QString channum  = QString("#%1").arg(chanid);
        QString chansign = channum;
        QString channame = channum;
        QString chanfilt;
        if (!query.value(7).toString().isEmpty())
        {
            channum  = query.value(7).toString();
            chansign = query.value(8).toString();
            channame = query.value(9).toString();
            chanfilt = query.value(10).toString();
        }

        QString hostname = query.value(15).toString();
        if (hostname.isEmpty())
            hostname = gCoreContext->GetHostName();

        RecStatusType recstatus = rsRecorded;
        QDateTime recstartts = MythDate::as_utc(query.value(24).toDateTime());

        QString key = ProgramInfo::MakeUniqueKey(chanid, recstartts);
        if (MythDate::as_utc(query.value(25).toDateTime()) > rectime &&
            recMap.contains(key))
        {
            recstatus = rsRecording;
        }

        bool save_not_commflagged = false;
        uint flags = 0;

        set_flag(flags, FL_CHANCOMMFREE,
                 query.value(30).toInt() == COMM_DETECT_COMMFREE);
        set_flag(flags, FL_COMMFLAG,
                 query.value(31).toInt() == COMM_FLAG_DONE);
        set_flag(flags, FL_COMMPROCESSING ,
                 query.value(31).toInt() == COMM_FLAG_PROCESSING);
        set_flag(flags, FL_REPEAT,        query.value(32).toBool());
        set_flag(flags, FL_TRANSCODED,
                 query.value(34).toInt() == TRANSCODING_COMPLETE);
        set_flag(flags, FL_DELETEPENDING, query.value(35).toBool());
        set_flag(flags, FL_PRESERVED,     query.value(36).toBool());
        set_flag(flags, FL_CUTLIST,       query.value(37).toBool());
        set_flag(flags, FL_AUTOEXP,       query.value(38).toBool());
        set_flag(flags, FL_REALLYEDITING, query.value(39).toBool());
        set_flag(flags, FL_BOOKMARK,      query.value(40).toBool());
        set_flag(flags, FL_WATCHED,       query.value(41).toBool());

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
                query.value(3).toUInt(),
                query.value(4).toUInt(),
                query.value(48).toString(), // syndicatedepisode
                query.value(5).toString(),

                chanid, channum, chansign, channame, chanfilt,

                query.value(11).toString(), query.value(12).toString(),

                query.value(14).toString(), // pathname

                hostname, query.value(13).toString(),

                query.value(17).toString(), query.value(18).toString(),
                query.value(19).toString(), // inetref

                query.value(16).toInt(),  // recpriority

                query.value(20).toULongLong(),  // filesize

                MythDate::as_utc(query.value(21).toDateTime()), //startts
                MythDate::as_utc(query.value(22).toDateTime()), // endts
                MythDate::as_utc(query.value(24).toDateTime()), // recstartts
                MythDate::as_utc(query.value(25).toDateTime()), // recendts

                query.value(23).toDouble(), // stars

                query.value(26).toUInt(), // year
                query.value(49).toUInt(), // partnumber
                query.value(50).toUInt(), // parttotal
                query.value(27).toDate(), // originalAirdate
                MythDate::as_utc(query.value(28).toDateTime()), // lastmodified

                recstatus,

                query.value(29).toUInt(), // recordid

                RecordingDupInType(query.value(46).toInt()),
                RecordingDupMethodType(query.value(47).toInt()),

                query.value(45).toUInt(), // findid

                flags,
                query.value(42).toUInt(), // audioproperties
                query.value(43).toUInt(), // videoproperties
                query.value(44).toUInt())); // subtitleType

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

bool GetNextRecordingList(QDateTime &nextRecordingStart,
                          bool *hasConflicts,
                          vector<ProgramInfo> *list)
{
    nextRecordingStart = QDateTime();

    bool dummy;
    bool *conflicts = (hasConflicts) ? hasConflicts : &dummy;

    ProgramList progList;
    if (!LoadFromScheduler(progList, *conflicts))
        return false;

    // find the earliest scheduled recording
    ProgramList::const_iterator it = progList.begin();
    for (; it != progList.end(); ++it)
    {
        if (((*it)->GetRecordingStatus() == rsWillRecord) &&
            (nextRecordingStart.isNull() ||
             nextRecordingStart > (*it)->GetRecordingStartTime()))
        {
            nextRecordingStart = (*it)->GetRecordingStartTime();
        }
    }

    if (!list)
        return true;

    // save the details of the earliest recording(s)
    for (it = progList.begin(); it != progList.end(); ++it)
    {
        if (((*it)->GetRecordingStatus()    == rsWillRecord) &&
            ((*it)->GetRecordingStartTime() == nextRecordingStart))
        {
            list->push_back(ProgramInfo(**it));
        }
    }

    return true;
}

PMapDBReplacement::PMapDBReplacement() : lock(new QMutex())
{
}

PMapDBReplacement::~PMapDBReplacement()
{
    delete lock;
}

MPUBLIC QString format_season_and_episode(int seasEp, int digits)
{
    QString seasEpNum;

    if (seasEp > -1)
    {
        seasEpNum = QString::number(seasEp);

        if (digits == 2 && seasEpNum.size() < 2)
            seasEpNum.prepend("0");
    }

    return seasEpNum;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
