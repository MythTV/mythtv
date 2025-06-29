// -*- Mode: c++ -*-

// POSIX headers
#include <unistd.h> // for getpid()

// C++ headers
#include <algorithm>

// Qt headers
#include <QMap>
#include <QUrl>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTimeZone>

// MythTV headers
#include "libmythbase/compat.h"
#include "libmythbase/mythcdrom.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdb.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythmiscutil.h"
#include "libmythbase/mythscheduler.h"
#include "libmythbase/mythsorthelper.h"
#include "libmythbase/remotefile.h"
#include "libmythbase/storagegroup.h"
#include "libmythbase/stringutil.h"

#include "programinfo.h"
#include "programinfoupdater.h"
#include "remoteutil.h"

#define LOC      QString("ProgramInfo(%1): ").arg(GetBasename())

//#define DEBUG_IN_USE

static int init_tr(void);

int pginfo_init_statics() { return ProgramInfo::InitStatics(); }
QMutex ProgramInfo::s_staticDataLock;
ProgramInfoUpdater *ProgramInfo::s_updater;
int force_init = pginfo_init_statics();
bool ProgramInfo::s_usingProgIDAuth = true;

static constexpr uint    kInvalidDateTime  { UINT_MAX  };
static constexpr int64_t kLastUpdateOffset { 61LL * 60 };

#define DEFINE_FLAGS_NAMES
#include "libmythbase/programtypeflags.h"
#undef DEFINE_FLAGS_NAMES

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
    "       p.syndicatedepisodenumber, p.partnumber, p.parttotal,  "//48-50
    "       p.season,           p.episode,      p.totalepisodes,   "//51-53
    "       p.category_type,    r.recordedid,   r.inputname,       "//54-56
    "       r.bookmarkupdate,   r.lastplay                         "//57-58
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

static QString determineURLType(const QString& url)
{
    QString result = url;

    if (!url.startsWith("dvd:") && !url.startsWith("bd:"))
    {
        if(url.endsWith(".img", Qt::CaseInsensitive) ||
           url.endsWith(".iso", Qt::CaseInsensitive))
        {
            switch (MythCDROM::inspectImage(url))
            {
                case MythCDROM::kBluray:
                    result = "bd:" + url;
                    break;

                case MythCDROM::kDVD:
                    result = "dvd:" + url;
                    break;

                case MythCDROM::kUnknown:
                    // Quiet compiler warning.
                    break;
            }
        }
        else
        {
            if (QDir(url + "/BDMV").exists())
                result = "bd:" + url;
            else if (QDir(url + "/VIDEO_TS").exists())
                result = "dvd:" + url;
        }
    }

    return result;
}

static const std::array<const QString,ProgramInfo::kNumCatTypes> kCatName
{ "", "movie", "series", "sports", "tvshow" };

QString myth_category_type_to_string(ProgramInfo::CategoryType category_type)
{
    if ((category_type > ProgramInfo::kCategoryNone) &&
        (category_type < kCatName.size()))
        return kCatName[category_type];

    return "";
}

ProgramInfo::CategoryType string_to_myth_category_type(const QString &category_type)
{
    for (size_t i = 1; i < kCatName.size(); i++)
        if (category_type == kCatName[i])
            return (ProgramInfo::CategoryType) i;
    return ProgramInfo::kCategoryNone;
}

/** \fn ProgramInfo::ProgramInfo(const ProgramInfo &other)
 *  \brief Copy constructor.
 */
ProgramInfo::ProgramInfo(const ProgramInfo &other) :
    m_title(other.m_title),
    m_sortTitle(other.m_sortTitle),
    m_subtitle(other.m_subtitle),
    m_sortSubtitle(other.m_sortSubtitle),
    m_description(other.m_description),
    m_season(other.m_season),
    m_episode(other.m_episode),
    m_totalEpisodes(other.m_totalEpisodes),
    m_syndicatedEpisode(other.m_syndicatedEpisode),
    m_category(other.m_category),
    m_director(other.m_director),

    m_recPriority(other.m_recPriority),

    m_chanId(other.m_chanId),
    m_chanStr(other.m_chanStr),
    m_chanSign(other.m_chanSign),
    m_chanName(other.m_chanName),
    m_chanPlaybackFilters(other.m_chanPlaybackFilters),

    m_recGroup(other.m_recGroup),
    m_playGroup(other.m_playGroup),

    m_pathname(other.m_pathname),

    m_hostname(other.m_hostname),
    m_storageGroup(other.m_storageGroup),

    m_seriesId(other.m_seriesId),
    m_programId(other.m_programId),
    m_inetRef(other.m_inetRef),
    m_catType(other.m_catType),

    m_fileSize(other.m_fileSize),

    m_startTs(other.m_startTs),
    m_endTs(other.m_endTs),
    m_recStartTs(other.m_recStartTs),
    m_recEndTs(other.m_recEndTs),

    m_stars(other.m_stars),

    m_originalAirDate(other.m_originalAirDate),
    m_lastModified(other.m_lastModified),
    m_lastInUseTime(MythDate::current().addSecs(-kLastInUseOffset)),

    m_recPriority2(other.m_recPriority2),
    m_recordId(other.m_recordId),
    m_parentId(other.m_parentId),

    m_sourceId(other.m_sourceId),
    m_inputId(other.m_inputId),

    m_findId(other.m_findId),
    m_programFlags(other.m_programFlags),
    m_videoProperties(other.m_videoProperties),
    m_audioProperties(other.m_audioProperties),
    m_subtitleProperties(other.m_subtitleProperties),
    m_year(other.m_year),
    m_partNumber(other.m_partNumber),
    m_partTotal(other.m_partTotal),

    m_recStatus(other.m_recStatus),
    m_recType(other.m_recType),
    m_dupIn(other.m_dupIn),
    m_dupMethod(other.m_dupMethod),

    m_recordedId(other.m_recordedId),
    m_inputName(other.m_inputName),
    m_bookmarkUpdate(other.m_bookmarkUpdate),

    // everything below this line is not serialized
    m_availableStatus(other.m_availableStatus),
    m_spread(other.m_spread),
    m_startCol(other.m_startCol),

    // Private
    m_positionMapDBReplacement(other.m_positionMapDBReplacement)
{
    ensureSortFields();
}

/** \fn ProgramInfo::ProgramInfo(uint _recordedid)
 *  \brief Constructs a ProgramInfo from data in 'recorded' table
 */
ProgramInfo::ProgramInfo(uint _recordedid)
{
    ProgramInfo::clear();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT chanid, starttime "
        "FROM recorded "
        "WHERE recordedid = :RECORDEDID");
    query.bindValue(":RECORDEDID", _recordedid);

    if (query.exec() && query.next())
    {
        uint _chanid          = query.value(0).toUInt();
        QDateTime _recstartts = MythDate::as_utc(query.value(1).toDateTime());
        LoadProgramFromRecorded(_chanid, _recstartts);
    }
    else
    {
        LOG(VB_GENERAL, LOG_CRIT, LOC +
            QString("Failed to find recorded entry for %1.")
            .arg(_recordedid));
        ProgramInfo::clear();
    }

    ensureSortFields();
}

/** \fn ProgramInfo::ProgramInfo(uint _chanid, const QDateTime &_recstartts)
 *  \brief Constructs a ProgramInfo from data in 'recorded' table
 */
ProgramInfo::ProgramInfo(uint _chanid, const QDateTime &_recstartts)
{
    ProgramInfo::clear();

    LoadProgramFromRecorded(_chanid, _recstartts);
    ensureSortFields();
}

/** \fn ProgramInfo::ProgramInfo()
 *  \brief Constructs a ProgramInfo from data in 'recorded' table
 */
ProgramInfo::ProgramInfo(
    uint  _recordedid,
    QString _title,
    QString _sortTitle,
    QString _subtitle,
    QString _sortSubtitle,
    QString _description,
    uint  _season,
    uint  _episode,
    uint  _totalepisodes,
    QString _syndicatedepisode,
    QString _category,

    uint _chanid,
    QString _channum,
    QString _chansign,
    QString _channame,
    QString _chanplaybackfilters,

    QString _recgroup,
    QString _playgroup,

    const QString &_pathname,

    QString _hostname,
    QString _storagegroup,

    QString _seriesid,
    QString _programid,
    QString _inetref,
    CategoryType  _catType,

    int _recpriority,

    uint64_t _filesize,

    QDateTime _startts,
    QDateTime _endts,
    QDateTime _recstartts,
    QDateTime _recendts,

    float _stars,

    uint _year,
    uint _partnumber,
    uint _parttotal,
    QDate _originalAirDate,
    QDateTime _lastmodified,

    RecStatus::Type _recstatus,

    uint _recordid,

    RecordingDupInType _dupin,
    RecordingDupMethodType _dupmethod,

    uint _findid,

    uint _programflags,
    uint _audioproperties,
    uint _videoproperties,
    uint _subtitleType,
    QString _inputname,
    QDateTime _bookmarkupdate) :
    m_title(std::move(_title)),
    m_sortTitle(std::move(_sortTitle)),
    m_subtitle(std::move(_subtitle)),
    m_sortSubtitle(std::move(_sortSubtitle)),
    m_description(std::move(_description)),
    m_season(_season),
    m_episode(_episode),
    m_totalEpisodes(_totalepisodes),
    m_syndicatedEpisode(std::move(_syndicatedepisode)),
    m_category(std::move(_category)),

    m_recPriority(_recpriority),

    m_chanId(_chanid),
    m_chanStr(std::move(_channum)),
    m_chanSign(std::move(_chansign)),
    m_chanName(std::move(_channame)),
    m_chanPlaybackFilters(std::move(_chanplaybackfilters)),

    m_recGroup(std::move(_recgroup)),
    m_playGroup(std::move(_playgroup)),

    m_pathname(_pathname),

    m_hostname(std::move(_hostname)),
    m_storageGroup(std::move(_storagegroup)),

    m_seriesId(std::move(_seriesid)),
    m_programId(std::move(_programid)),
    m_inetRef(std::move(_inetref)),
    m_catType(_catType),

    m_fileSize(_filesize),

    m_startTs(std::move(_startts)),
    m_endTs(std::move(_endts)),
    m_recStartTs(std::move(_recstartts)),
    m_recEndTs(std::move(_recendts)),

    m_stars(std::clamp(_stars, 0.0F, 1.0F)),

    m_originalAirDate(_originalAirDate),
    m_lastModified(std::move(_lastmodified)),
    m_lastInUseTime(MythDate::current().addSecs(-kLastInUseOffset)),

    m_recordId(_recordid),
    m_findId(_findid),

    m_programFlags(_programflags),
    m_videoProperties(_videoproperties),
    m_audioProperties(_audioproperties),
    m_subtitleProperties(_subtitleType),
    m_year(_year),
    m_partNumber(_partnumber),
    m_partTotal(_parttotal),

    m_recStatus(_recstatus),
    m_dupIn(_dupin),
    m_dupMethod(_dupmethod),

    m_recordedId(_recordedid),
    m_inputName(std::move(_inputname)),
    m_bookmarkUpdate(std::move(_bookmarkupdate))
{
    if (m_originalAirDate.isValid() && m_originalAirDate < QDate(1895, 12, 28))
        m_originalAirDate = QDate();

    SetPathname(_pathname);
    ensureSortFields();
}

/** \fn ProgramInfo::ProgramInfo()
 *  \brief Constructs a ProgramInfo from data in 'oldrecorded' table
 */
ProgramInfo::ProgramInfo(
    QString _title,
    QString _sortTitle,
    QString _subtitle,
    QString _sortSubtitle,
    QString _description,
    uint  _season,
    uint  _episode,
    QString _category,

    uint _chanid,
    QString _channum,
    QString _chansign,
    QString _channame,

    QString _seriesid,
    QString _programid,
    QString _inetref,

    QDateTime _startts,
    QDateTime _endts,
    QDateTime _recstartts,
    QDateTime _recendts,

    RecStatus::Type _recstatus,

    uint _recordid,

    RecordingType _rectype,

    uint _findid,

    bool duplicate) :
    m_title(std::move(_title)),
    m_sortTitle(std::move(_sortTitle)),
    m_subtitle(std::move(_subtitle)),
    m_sortSubtitle(std::move(_sortSubtitle)),
    m_description(std::move(_description)),
    m_season(_season),
    m_episode(_episode),
    m_category(std::move(_category)),

    m_chanId(_chanid),
    m_chanStr(std::move(_channum)),
    m_chanSign(std::move(_chansign)),
    m_chanName(std::move(_channame)),

    m_seriesId(std::move(_seriesid)),
    m_programId(std::move(_programid)),
    m_inetRef(std::move(_inetref)),

    m_startTs(std::move(_startts)),
    m_endTs(std::move(_endts)),
    m_recStartTs(std::move(_recstartts)),
    m_recEndTs(std::move(_recendts)),

    m_lastModified(m_startTs),
    m_lastInUseTime(MythDate::current().addSecs(-kLastInUseOffset)),

    m_recordId(_recordid),
    m_findId(_findid),

    m_programFlags((duplicate) ? FL_DUPLICATE : FL_NONE),

    m_recStatus(_recstatus),
    m_recType(_rectype),
    m_dupIn(kDupsUnset),
    m_dupMethod(kDupCheckUnset)
{
    ensureSortFields();
}

/** \fn ProgramInfo::ProgramInfo()
 *  \brief Constructs a ProgramInfo from listings data in 'program' table
 */
ProgramInfo::ProgramInfo(
    QString _title,
    QString _sortTitle,
    QString _subtitle,
    QString _sortSubtitle,
    QString _description,
    QString _syndicatedepisode,
    QString _category,

    uint _chanid,
    QString _channum,
    QString _chansign,
    QString _channame,
    QString _chanplaybackfilters,

    QDateTime _startts,
    QDateTime _endts,
    QDateTime _recstartts,
    QDateTime _recendts,

    QString _seriesid,
   QString _programid,
    const CategoryType _catType,

    float _stars,
    uint _year,
    uint _partnumber,
    uint _parttotal,

    QDate _originalAirDate,
    RecStatus::Type _recstatus,
    uint _recordid,
    RecordingType _rectype,
    uint _findid,

    bool commfree,
    bool repeat,

    uint _videoproperties,
    uint _audioproperties,
    uint _subtitleType,

    uint _season,
    uint _episode,
    uint _totalepisodes,

    const ProgramList &schedList) :
    m_title(std::move(_title)),
    m_sortTitle(std::move(_sortTitle)),
    m_subtitle(std::move(_subtitle)),
    m_sortSubtitle(std::move(_sortSubtitle)),
    m_description(std::move(_description)),
    m_season(_season),
    m_episode(_episode),
    m_totalEpisodes(_totalepisodes),
    m_syndicatedEpisode(std::move(_syndicatedepisode)),
    m_category(std::move(_category)),

    m_chanId(_chanid),
    m_chanStr(std::move(_channum)),
    m_chanSign(std::move(_chansign)),
    m_chanName(std::move(_channame)),
    m_chanPlaybackFilters(std::move(_chanplaybackfilters)),

    m_seriesId(std::move(_seriesid)),
    m_programId(std::move(_programid)),
    m_catType(_catType),

    m_startTs(std::move(_startts)),
    m_endTs(std::move(_endts)),
    m_recStartTs(std::move(_recstartts)),
    m_recEndTs(std::move(_recendts)),

    m_stars(std::clamp(_stars, 0.0F, 1.0F)),

    m_originalAirDate(_originalAirDate),
    m_lastModified(m_startTs),
    m_lastInUseTime(m_startTs.addSecs(-kLastInUseOffset)),

    m_recordId(_recordid),
    m_findId(_findid),

    m_videoProperties(_videoproperties),
    m_audioProperties(_audioproperties),
    m_subtitleProperties(_subtitleType),
    m_year(_year),
    m_partNumber(_partnumber),
    m_partTotal(_parttotal),

    m_recStatus(_recstatus),
    m_recType(_rectype)
{
    m_programFlags |= (commfree) ? FL_CHANCOMMFREE : FL_NONE;
    m_programFlags |= (repeat)   ? FL_REPEAT       : FL_NONE;

    if (m_originalAirDate.isValid() && m_originalAirDate < QDate(1895, 12, 28))
        m_originalAirDate = QDate();

    for (auto *it : schedList)
    {
        // If this showing is scheduled to be recorded, then we need to copy
        // some of the information from the scheduler
        //
        // This applies even if the showing may be on a different channel
        // to the one which is actually being recorded e.g. A regional or HD
        // variant of the same channel
        if (!IsSameProgramAndStartTime(*it))
            continue;

        const ProgramInfo &s = *it;
        m_recordId    = s.m_recordId;
        m_recType     = s.m_recType;
        m_recPriority = s.m_recPriority;
        m_recStartTs  = s.m_recStartTs;
        m_recEndTs    = s.m_recEndTs;
        m_inputId     = s.m_inputId;
        m_dupIn       = s.m_dupIn;
        m_dupMethod   = s.m_dupMethod;
        m_findId      = s.m_findId;
        m_recordedId  = s.m_recordedId;
        m_hostname    = s.m_hostname;
        m_inputName   = s.m_inputName;

        // This is the exact showing (same chanid or callsign)
        // which will be recorded
        if (IsSameChannel(s))
        {
            m_recStatus   = s.m_recStatus;
            break;
        }

        if (s.m_recStatus == RecStatus::WillRecord ||
            s.m_recStatus == RecStatus::Pending ||
            s.m_recStatus == RecStatus::Recording ||
            s.m_recStatus == RecStatus::Tuning ||
            s.m_recStatus == RecStatus::Failing)
        m_recStatus = s.m_recStatus;
    }
    ensureSortFields();
}

/** \fn ProgramInfo::ProgramInfo()
 *  \brief Constructs a basic ProgramInfo (used by RecordingInfo)
 */
ProgramInfo::ProgramInfo(
    QString _title,
    QString _sortTitle,
    QString _subtitle,
    QString _sortSubtitle,
    QString _description,
    uint  _season,
    uint  _episode,
    uint  _totalepisodes,
    QString _category,

    uint _chanid,
    QString _channum,
    QString _chansign,
    QString _channame,
    QString _chanplaybackfilters,

    QString _recgroup,
    QString _playgroup,

    QDateTime _startts,
    QDateTime _endts,
    QDateTime _recstartts,
    QDateTime _recendts,

    QString _seriesid,
    QString _programid,
    QString _inetref,
    QString _inputname) :
    m_title(std::move(_title)),
    m_sortTitle(std::move(_sortTitle)),
    m_subtitle(std::move(_subtitle)),
    m_sortSubtitle(std::move(_sortSubtitle)),
    m_description(std::move(_description)),
    m_season(_season),
    m_episode(_episode),
    m_totalEpisodes(_totalepisodes),
    m_category(std::move(_category)),

    m_chanId(_chanid),
    m_chanStr(std::move(_channum)),
    m_chanSign(std::move(_chansign)),
    m_chanName(std::move(_channame)),
    m_chanPlaybackFilters(std::move(_chanplaybackfilters)),

    m_recGroup(std::move(_recgroup)),
    m_playGroup(std::move(_playgroup)),

    m_seriesId(std::move(_seriesid)),
    m_programId(std::move(_programid)),
    m_inetRef(std::move(_inetref)),

    m_startTs(std::move(_startts)),
    m_endTs(std::move(_endts)),
    m_recStartTs(std::move(_recstartts)),
    m_recEndTs(std::move(_recendts)),

    m_lastModified(MythDate::current()),
    m_lastInUseTime(m_lastModified.addSecs(-kLastInUseOffset)),

    m_inputName(std::move(_inputname))
{
    ensureSortFields();
}

/** \fn ProgramInfo::ProgramInfo(const QString &_pathname)
 *  \brief Constructs a ProgramInfo for a pathname.
 */
ProgramInfo::ProgramInfo(const QString &_pathname)
{
    ProgramInfo::clear();
    if (_pathname.isEmpty())
    {
        return;
    }

    uint _chanid = 0;
    QDateTime _recstartts;
    if (!gCoreContext->IsDatabaseIgnored() &&
        QueryKeyFromPathname(_pathname, _chanid, _recstartts) &&
        LoadProgramFromRecorded(_chanid, _recstartts))
    {
        return;
    }

    ProgramInfo::clear();

    QDateTime cur = MythDate::current();
    m_recStartTs = m_startTs = cur.addSecs(-kLastInUseOffset - 1);
    m_recEndTs   = m_endTs   = cur.addSecs(-1);

    QString basename = _pathname.section('/', -1);
    if (_pathname == basename)
        SetPathname(QDir::currentPath() + '/' + _pathname);
    else if (_pathname.contains("./") && !_pathname.contains(":"))
        SetPathname(QFileInfo(_pathname).absoluteFilePath());
    else
        SetPathname(_pathname);
    ensureSortFields();
}

/** \fn ProgramInfo::ProgramInfo()
 *  \brief Constructs a ProgramInfo for a video.
 */
ProgramInfo::ProgramInfo(const QString &_pathname,
                         const QString &_plot,
                         const QString &_title,
                         const QString &_sortTitle,
                         const QString &_subtitle,
                         const QString &_sortSubtitle,
                         const QString &_director,
                         int _season, int _episode,
                         const QString &_inetref,
                         std::chrono::minutes length_in_minutes,
                         uint _year,
                         const QString &_programid)
{
    ProgramInfo::clear();

    //NOLINTBEGIN(cppcoreguidelines-prefer-member-initializer)
    // These have to remain after the call to ::clear().
    m_title = _title;
    m_sortTitle = _sortTitle;
    m_subtitle = _subtitle;
    m_sortSubtitle = _sortSubtitle;
    m_description = _plot;
    m_season = _season;
    m_episode = _episode;
    m_director = _director;
    m_programId = _programid;
    m_inetRef = _inetref;
    m_year = _year;
    //NOLINTEND(cppcoreguidelines-prefer-member-initializer)

    QDateTime cur = MythDate::current();
    int64_t minutes = length_in_minutes.count();
    m_recStartTs = cur.addSecs((minutes + 1) * -60);
    m_recEndTs   = m_recStartTs.addSecs(minutes * 60);
#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
    m_startTs    = QDateTime(QDate(m_year,1,1),QTime(0,0,0), Qt::UTC);
#else
    m_startTs    = QDateTime(QDate(m_year,1,1),QTime(0,0,0),
                             QTimeZone(QTimeZone::UTC));
#endif
    m_endTs      = m_startTs.addSecs(minutes * 60);

    QString pn = _pathname;
    if (!_pathname.startsWith("myth://"))
        pn = determineURLType(_pathname);

    SetPathname(pn);
    ensureSortFields();
}

/** \fn ProgramInfo::ProgramInfo()
 *  \brief Constructs a manual record ProgramInfo.
 */
ProgramInfo::ProgramInfo(const QString &_title, uint _chanid,
                         const QDateTime &_startts,
                         const QDateTime &_endts)
{
    ProgramInfo::clear();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT chanid, channum, callsign, name, outputfilters, commmethod "
        "FROM channel "
        "WHERE chanid=:CHANID");
    query.bindValue(":CHANID", _chanid);
    if (query.exec() && query.next())
    {
        m_chanStr  = query.value(1).toString();
        m_chanSign = query.value(2).toString();
        m_chanName = query.value(3).toString();
        m_chanPlaybackFilters = query.value(4).toString();
        set_flag(m_programFlags, FL_CHANCOMMFREE,
                 query.value(5).toInt() == COMM_DETECT_COMMFREE);
    }

    m_chanId  = _chanid;
    m_startTs = _startts;
    m_endTs   = _endts;

    m_title = _title;
    if (m_title.isEmpty())
    {
        QString channelFormat =
            gCoreContext->GetSetting("ChannelFormat", "<num> <sign>");

        m_title = QString("%1 - %2").arg(ChannelText(channelFormat),
            MythDate::toString(m_startTs, MythDate::kTime));
    }

    m_description = m_title =
        QString("%1 (%2)").arg(m_title, QObject::tr("Manual Record"));
    ensureSortFields();
}

/** \fn ProgramInfo::operator=(const ProgramInfo &other)
 *  \brief Copies important fields from other ProgramInfo.
 */
ProgramInfo &ProgramInfo::operator=(const ProgramInfo &other)
{
    if (this == &other)
        return *this;

    clone(other);
    return *this;
}

/// \brief Copies important fields from other ProgramInfo.
void ProgramInfo::clone(const ProgramInfo &other,
                        bool ignore_non_serialized_data)
{
    bool is_same =
        ((m_chanId != 0U) && m_recStartTs.isValid() && m_startTs.isValid() &&
         m_chanId == other.m_chanId && m_recStartTs == other.m_recStartTs &&
         m_startTs == other.m_startTs);

    m_title = other.m_title;
    m_sortTitle = other.m_sortTitle;
    m_subtitle = other.m_subtitle;
    m_sortSubtitle = other.m_sortSubtitle;
    m_description = other.m_description;
    m_season = other.m_season;
    m_episode = other.m_episode;
    m_totalEpisodes = other.m_totalEpisodes;
    m_syndicatedEpisode = other.m_syndicatedEpisode;
    m_category = other.m_category;
    m_director = other.m_director;

    m_chanId = other.m_chanId;
    m_chanStr = other.m_chanStr;
    m_chanSign = other.m_chanSign;
    m_chanName = other.m_chanName;
    m_chanPlaybackFilters = other.m_chanPlaybackFilters;

    m_recGroup = other.m_recGroup;
    m_playGroup = other.m_playGroup;

    if (!ignore_non_serialized_data || !is_same ||
        (GetBasename() != other.GetBasename()))
    {
        m_pathname = other.m_pathname;
    }

    m_hostname = other.m_hostname;
    m_storageGroup = other.m_storageGroup;

    m_seriesId = other.m_seriesId;
    m_programId = other.m_programId;
    m_inetRef = other.m_inetRef;
    m_catType = other.m_catType;

    m_recPriority = other.m_recPriority;

    m_fileSize = other.m_fileSize;

    m_startTs = other.m_startTs;
    m_endTs = other.m_endTs;
    m_recStartTs = other.m_recStartTs;
    m_recEndTs = other.m_recEndTs;

    m_stars = other.m_stars;

    m_year = other.m_year;
    m_partNumber = other.m_partNumber;
    m_partTotal = other.m_partTotal;

    m_originalAirDate = other.m_originalAirDate;
    m_lastModified = other.m_lastModified;
    m_lastInUseTime = MythDate::current().addSecs(-kLastInUseOffset);

    m_recStatus = other.m_recStatus;

    m_recPriority2 = other.m_recPriority2;
    m_recordId = other.m_recordId;
    m_parentId = other.m_parentId;

    m_recType = other.m_recType;
    m_dupIn = other.m_dupIn;
    m_dupMethod = other.m_dupMethod;

    m_recordedId = other.m_recordedId;
    m_inputName = other.m_inputName;
    m_bookmarkUpdate = other.m_bookmarkUpdate;

    m_sourceId = other.m_sourceId;
    m_inputId = other.m_inputId;

    m_findId = other.m_findId;
    m_programFlags = other.m_programFlags;
    m_videoProperties = other.m_videoProperties;
    m_audioProperties = other.m_audioProperties;
    m_subtitleProperties= other.m_subtitleProperties;

    if (!ignore_non_serialized_data)
    {
        m_spread = other.m_spread;
        m_startCol = other.m_startCol;
        m_availableStatus = other.m_availableStatus;

        m_inUseForWhat = other.m_inUseForWhat;
        m_positionMapDBReplacement = other.m_positionMapDBReplacement;
    }
}

void ProgramInfo::clear(void)
{
    m_title.clear();
    m_sortTitle.clear();
    m_subtitle.clear();
    m_sortSubtitle.clear();
    m_description.clear();
    m_season = 0;
    m_episode = 0;
    m_totalEpisodes = 0;
    m_syndicatedEpisode.clear();
    m_category.clear();
    m_director.clear();

    m_chanId = 0;
    m_chanStr.clear();
    m_chanSign.clear();
    m_chanName.clear();
    m_chanPlaybackFilters.clear();

    m_recGroup = "Default";
    m_playGroup = "Default";

    m_pathname.clear();

    m_hostname.clear();
    m_storageGroup = "Default";

    m_year = 0;
    m_partNumber = 0;
    m_partTotal = 0;

    m_seriesId.clear();
    m_programId.clear();
    m_inetRef.clear();
    m_catType = kCategoryNone;

    m_recPriority = 0;

    m_fileSize = 0ULL;

    m_startTs = MythDate::current(true);
    m_endTs = m_startTs;
    m_recStartTs = m_startTs;
    m_recEndTs = m_startTs;

    m_stars = 0.0F;

    m_originalAirDate = QDate();
    m_lastModified = m_startTs;
    m_lastInUseTime = m_startTs.addSecs(-kLastInUseOffset);

    m_recStatus = RecStatus::Unknown;

    m_recPriority2 = 0;
    m_recordId = 0;
    m_parentId = 0;

    m_recType = kNotRecording;
    m_dupIn = kDupsInAll;
    m_dupMethod = kDupCheckSubThenDesc;

    m_recordedId = 0;
    m_inputName.clear();
    m_bookmarkUpdate = QDateTime();

    m_sourceId = 0;
    m_inputId = 0;

    m_findId = 0;

    m_programFlags = FL_NONE;
    m_videoProperties = VID_UNKNOWN;
    m_audioProperties = AUD_UNKNOWN;
    m_subtitleProperties = SUB_UNKNOWN;

    // everything below this line is not serialized
    m_spread = -1;
    m_startCol = -1;
    m_availableStatus = asAvailable;

    // Private
    m_inUseForWhat.clear();
    m_positionMapDBReplacement = nullptr;
}

/*!
 *  Compare two QStrings when they can either be initialized to
 *  "Default" or to the empty string.
 */
bool qstringEqualOrDefault(const QString& a, const QString& b);
bool qstringEqualOrDefault(const QString& a, const QString& b)
{
    if (a == b)
        return true;
    if (a.isEmpty() and (b == "Default"))
        return true;
    if ((a == "Default") and b.isEmpty())
        return true;
    return false;
}

/*!
 *  Compare two ProgramInfo instances to see if they are equal.  Equal
 *  is defined as all parameters that are serialized and passed across
 *  the myth protocol are the same.
 *
 *  \param rhs The ProgramInfo instance to compare to the current
 *  instance.
 *
 *  \return True if all the serialized fields match, False otherwise.
 */
bool ProgramInfo::operator==(const ProgramInfo& rhs)
{
    if ((m_title != rhs.m_title) ||
        (m_subtitle != rhs.m_subtitle) ||
        (m_description != rhs.m_description) ||
        (m_season != rhs.m_season) ||
        (m_episode != rhs.m_episode) ||
        (m_totalEpisodes != rhs.m_totalEpisodes) ||
        (m_syndicatedEpisode != rhs.m_syndicatedEpisode) ||
        (m_category != rhs.m_category)
#if 0
        || (m_director != rhs.m_director)
#endif
        )
        return false;

    if (m_recPriority != rhs.m_recPriority)
        return false;

    if ((m_chanId != rhs.m_chanId) ||
        (m_chanStr != rhs.m_chanStr) ||
        (m_chanSign != rhs.m_chanSign) ||
        (m_chanName != rhs.m_chanName) ||
        (m_chanPlaybackFilters != rhs.m_chanPlaybackFilters))
        return false;

    if (!qstringEqualOrDefault(m_recGroup, rhs.m_recGroup) ||
        !qstringEqualOrDefault(m_playGroup, rhs.m_playGroup))
        return false;

    if (m_pathname != rhs.m_pathname)
        return false;

    if ((m_hostname != rhs.m_hostname) ||
        !qstringEqualOrDefault(m_storageGroup, rhs.m_storageGroup))
        return false;

    if ((m_seriesId != rhs.m_seriesId) ||
        (m_programId != rhs.m_programId) ||
        (m_inetRef != rhs.m_inetRef) ||
        (m_catType != rhs.m_catType))
        return false;

    if (m_fileSize != rhs.m_fileSize)
        return false;

    if ((m_startTs != rhs.m_startTs) ||
        (m_endTs != rhs.m_endTs) ||
        (m_recStartTs != rhs.m_recStartTs) ||
        (m_recEndTs != rhs.m_recEndTs))
        return false;

    if ((m_stars != rhs.m_stars) ||
        (m_originalAirDate != rhs.m_originalAirDate) ||
        (m_lastModified != rhs.m_lastModified)
#if 0
        || (m_lastInUseTime != rhs.m_lastInUseTime)
#endif
        )
        return false;

    if (m_recPriority2 != rhs.m_recPriority2)
        return false;

    if ((m_recordId != rhs.m_recordId) ||
        (m_parentId != rhs.m_parentId))
        return false;

    if ((m_sourceId != rhs.m_sourceId) ||
        (m_inputId != rhs.m_inputId) ||
        (m_findId != rhs.m_findId))
        return false;

    if ((m_programFlags != rhs.m_programFlags) ||
        (m_videoProperties != rhs.m_videoProperties) ||
        (m_audioProperties != rhs.m_audioProperties) ||
        (m_subtitleProperties != rhs.m_subtitleProperties) ||
        (m_year != rhs.m_year) ||
        (m_partNumber != rhs.m_partNumber) ||
        (m_partTotal != rhs.m_partTotal))
        return false;

    if ((m_recStatus != rhs.m_recStatus) ||
        (m_recType != rhs.m_recType) ||
        (m_dupIn != rhs.m_dupIn) ||
        (m_dupMethod != rhs.m_dupMethod))
        return false;

    if ((m_recordedId != rhs.m_recordedId) ||
        (m_inputName != rhs.m_inputName) ||
        (m_bookmarkUpdate != rhs.m_bookmarkUpdate))
        return false;

    return true;
}

/**
 *  \brief Ensure that the sortTitle and sortSubtitle fields are set.
 */
void ProgramInfo::ensureSortFields(void)
{
    std::shared_ptr<MythSortHelper>sh = getMythSortHelper();

    if (m_sortTitle.isEmpty() and not m_title.isEmpty())
        m_sortTitle = sh->doTitle(m_title);
    if (m_sortSubtitle.isEmpty() and not m_subtitle.isEmpty())
        m_sortSubtitle = sh->doTitle(m_subtitle);
}

void ProgramInfo::SetTitle(const QString &t, const QString &st)
{
    m_title = t;
    m_sortTitle = st;
    ensureSortFields();
}

void ProgramInfo::SetSubtitle(const QString &st, const QString &sst)
{
    m_subtitle = st;
    m_sortSubtitle = sst;
    ensureSortFields();
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
    return (chanid != 0U) && recstartts.isValid();
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

bool ProgramInfo::QueryRecordedIdFromPathname(const QString &pathname,
                                              uint &recordedid)
{
    QString basename = pathname.section('/', -1);
    if (basename.isEmpty())
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT recordedid "
        "FROM recorded "
        "WHERE basename = :BASENAME");
    query.bindValue(":BASENAME", basename);
    if (query.exec() && query.next())
    {
        recordedid = query.value(0).toUInt();
        return true;
    }

    return false;
}

static inline QString DateTimeToListInt(const QDateTime& x) {
    if (x.isValid())
        return QString::number(x.toSecsSinceEpoch());
    return QString::number(kInvalidDateTime);
}

/** \fn ProgramInfo::ToStringList(QStringList&) const
 *  \brief Serializes ProgramInfo into a QStringList which can be passed
 *         over a socket.
 *  \sa FromStringList(QStringList::const_iterator&,
                       QStringList::const_iterator)
 */
void ProgramInfo::ToStringList(QStringList &list) const
{
    list << m_title;                          // 0
    list << m_subtitle;                       // 1
    list << m_description;                    // 2
    list << QString::number(m_season );       // 3
    list << QString::number(m_episode );      // 4
    list << QString::number(m_totalEpisodes); // 5
    list << m_syndicatedEpisode;              // 6
    list << m_category;                       // 7
    list << QString::number(m_chanId);        // 8
    list << m_chanStr;                        // 9
    list << m_chanSign;                       // 10
    list << m_chanName;                       // 11
    list << m_pathname;                       // 12
    list << QString::number(m_fileSize);      // 13

    list << DateTimeToListInt(m_startTs);     // 14
    list << DateTimeToListInt(m_endTs);       // 15
    list << QString::number(m_findId);        // 16
    list << m_hostname;                       // 17
    list << QString::number(m_sourceId);      // 18
    list << QString::number(m_inputId);       // 19 (m_formerly cardid)
    list << QString::number(m_inputId);       // 20
    list << QString::number(m_recPriority);   // 21
    list << QString::number(m_recStatus);     // 22
    list << QString::number(m_recordId);      // 23

    list << QString::number(m_recType);       // 24
    list << QString::number(m_dupIn);         // 25
    list << QString::number(m_dupMethod);     // 26
    list << DateTimeToListInt(m_recStartTs);  // 27
    list << DateTimeToListInt(m_recEndTs);    // 28
    list << QString::number(m_programFlags);  // 29
    list << (!m_recGroup.isEmpty() ? m_recGroup : "Default"); // 30
    list << m_chanPlaybackFilters;            // 31
    list << m_seriesId;                       // 32
    list << m_programId;                      // 33
    list << m_inetRef;                        // 34

    list << DateTimeToListInt(m_lastModified);       // 35
    list << QString("%1").arg(m_stars);              // 36
    list << m_originalAirDate.toString(Qt::ISODate); // 37
    list << (!m_playGroup.isEmpty() ? m_playGroup : "Default"); // 38
    list << QString::number(m_recPriority2);         // 39
    list << QString::number(m_parentId);             // 40
    list << (!m_storageGroup.isEmpty() ? m_storageGroup : "Default"); // 41
    list << QString::number(m_audioProperties);      // 42
    list << QString::number(m_videoProperties);      // 43
    list << QString::number(m_subtitleProperties);   // 44

    list << QString::number(m_year);                 // 45
    list << QString::number(m_partNumber);           // 46
    list << QString::number(m_partTotal);            // 47
    list << QString::number(m_catType);              // 48

    list << QString::number(m_recordedId);           // 49
    list << m_inputName;                             // 50
    list << DateTimeToListInt(m_bookmarkUpdate);     // 51
/* do not forget to update the NUMPROGRAMLINES defines! */
}

// QStringList::const_iterator it = list.begin()+offset;

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define NEXT_STR()        do { if (it == listend)                    \
                               {                                     \
                                   LOG(VB_GENERAL, LOG_ERR, listerror); \
                                   clear();                          \
                                   return false;                     \
                               }                                     \
                               ts = *it++; } while (false)

static inline QDateTime DateTimeFromListItem(const QString& str)
{
    if (str.isEmpty() or (str.toUInt() == kInvalidDateTime))
        return {};
    return MythDate::fromSecsSinceEpoch(str.toLongLong());
}

static inline QDate DateFromListItem(const QString& str)
{
     if (str.isEmpty() || (str == "0000-00-00"))
         return {};
     return QDate::fromString(str, Qt::ISODate);
}


/** \fn ProgramInfo::FromStringList(QStringList::const_iterator&,
                                    QStringList::const_iterator)
 *  \brief Uses a QStringList to initialize this ProgramInfo instance.
 *  \param it     Iterator pointing to first item in list to treat as
 *                beginning of serialized ProgramInfo.
 *  \param end    Iterator that will stop parsing of the ProgramInfo
 *  \return true if it succeeds, false if it fails.
 *  \sa FromStringList(const QStringList&,uint)
 *      ToStringList(QStringList&) const
 */

bool ProgramInfo::FromStringList(QStringList::const_iterator &it,
                                 const QStringList::const_iterator&  listend)
{
    QString listerror = LOC + "FromStringList, not enough items in list.";
    QString ts;

    uint      origChanid     = m_chanId;
    QDateTime origRecstartts = m_recStartTs;

    NEXT_STR(); m_title = ts;                                // 0
    NEXT_STR(); m_subtitle = ts;                             // 1
    NEXT_STR(); m_description = ts;                          // 2
    NEXT_STR(); m_season = ts.toLongLong();                  // 3
    NEXT_STR(); m_episode = ts.toLongLong();                 // 4
    NEXT_STR(); m_totalEpisodes = ts.toLongLong();           // 5
    NEXT_STR(); m_syndicatedEpisode = ts;                    // 6
    NEXT_STR(); m_category = ts;                             // 7
    NEXT_STR(); m_chanId = ts.toLongLong();                  // 8
    NEXT_STR(); m_chanStr = ts;                              // 9
    NEXT_STR(); m_chanSign = ts;                             // 10
    NEXT_STR(); m_chanName = ts;                             // 11
    NEXT_STR(); m_pathname = ts;                             // 12
    NEXT_STR(); m_fileSize = ts.toLongLong();                // 13

    NEXT_STR(); m_startTs = DateTimeFromListItem(ts);        // 14
    NEXT_STR(); m_endTs = DateTimeFromListItem(ts);          // 15
    NEXT_STR(); m_findId = ts.toLongLong();                  // 16
    NEXT_STR(); m_hostname = ts;                             // 17
    NEXT_STR(); m_sourceId = ts.toLongLong();                // 18
    NEXT_STR();                                              // 19 (formerly cardid)
    NEXT_STR(); m_inputId = ts.toLongLong();                 // 20
    NEXT_STR(); m_recPriority = ts.toLongLong();             // 21
    NEXT_STR(); m_recStatus = (RecStatus::Type)ts.toInt();   // 22
    NEXT_STR(); m_recordId = ts.toLongLong();                // 23

    NEXT_STR(); m_recType = (RecordingType)ts.toInt();       // 24
    NEXT_STR(); m_dupIn = (RecordingDupInType)ts.toInt();    // 25
    NEXT_STR(); m_dupMethod = (RecordingDupMethodType)ts.toInt(); // 26
    NEXT_STR(); m_recStartTs = DateTimeFromListItem(ts);     // 27
    NEXT_STR(); m_recEndTs = DateTimeFromListItem(ts);       // 28
    NEXT_STR(); m_programFlags = ts.toLongLong();            // 29
    NEXT_STR(); m_recGroup = ts;                             // 30
    NEXT_STR(); m_chanPlaybackFilters = ts;                  // 31
    NEXT_STR(); m_seriesId = ts;                             // 32
    NEXT_STR(); m_programId = ts;                            // 33
    NEXT_STR(); m_inetRef = ts;                              // 34

    NEXT_STR(); m_lastModified = DateTimeFromListItem(ts);   // 35
    NEXT_STR(); (m_stars) = ts.toFloat();                    // 36
    NEXT_STR(); m_originalAirDate = DateFromListItem(ts);    // 37
    NEXT_STR(); m_playGroup = ts;                            // 38
    NEXT_STR(); m_recPriority2 = ts.toLongLong();            // 39
    NEXT_STR(); m_parentId = ts.toLongLong();                // 40
    NEXT_STR(); m_storageGroup = ts;                         // 41
    NEXT_STR(); m_audioProperties = ts.toLongLong();         // 42
    NEXT_STR(); m_videoProperties = ts.toLongLong();         // 43
    NEXT_STR(); m_subtitleProperties = ts.toLongLong();      // 44

    NEXT_STR(); m_year = ts.toLongLong();                    // 45
    NEXT_STR(); m_partNumber = ts.toLongLong();              // 46
    NEXT_STR(); m_partTotal = ts.toLongLong();               // 47
    NEXT_STR(); m_catType = (CategoryType)ts.toInt();        // 48

    NEXT_STR(); m_recordedId = ts.toLongLong();              // 49
    NEXT_STR(); m_inputName = ts;                            // 50
    NEXT_STR(); m_bookmarkUpdate = DateTimeFromListItem(ts); // 51

    if (!origChanid || !origRecstartts.isValid() ||
        (origChanid != m_chanId) || (origRecstartts != m_recStartTs))
    {
        m_availableStatus = asAvailable;
        m_spread = -1;
        m_startCol = -1;
        m_inUseForWhat = QString();
        m_positionMapDBReplacement = nullptr;
    }

    ensureSortFields();

    return true;
}

template <typename T>
QString propsValueToString (const QString& name, QMap<T,QString> propNames,
                            T props)
{
    if (props == 0)
        return propNames[0];

    QStringList result;
    for (uint i = 0; i < (sizeof(T)*8) - 1; ++i)
    {
        uint bit = 1<<i;
        if ((props & bit) == 0)
            continue;
        if (propNames.contains(bit))
        {
            result += propNames[bit];
            continue;
        }
        QString tmp = QString("0x%1").arg(bit, sizeof(T)*2,16,QChar('0'));
        LOG(VB_GENERAL, LOG_ERR, QString("Unknown name for %1 flag 0x%2.")
            .arg(name, tmp));
        result += tmp;
    }
    return result.join('|');
}

template <typename T>
uint propsValueFromString (const QString& name, const QMap<T,QString>& propNames,
                           const QString& props)
{
    if (props.isEmpty())
        return 0;

    uint result = 0;

    QStringList names = props.split('|');
    for ( const auto& n : std::as_const(names)  )
    {
        uint bit = propNames.key(n, 0);
        if (bit == 0)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Unknown flag for %1 %2")
                .arg(name, n));
        }
        else
        {
            result |= bit;
        }
    }
    return result;
}

QString ProgramInfo::GetProgramFlagNames(void) const
{
    return propsValueToString("program", ProgramFlagNames, m_programFlags);
}


QString ProgramInfo::GetRecTypeStatus(bool showrerecord) const
{
    QString tmp_rec = ::toString(GetRecordingRuleType());
    if (GetRecordingRuleType() != kNotRecording)
    {
        QDateTime timeNow = MythDate::current();
        if (((m_recEndTs > timeNow) && (m_recStatus <= RecStatus::WillRecord)) ||
            (m_recStatus == RecStatus::Conflict) || (m_recStatus == RecStatus::LaterShowing))
        {
            tmp_rec += QString(" %1%2").arg(m_recPriority > 0 ? "+" : "").arg(m_recPriority);
            if (m_recPriority2)
                tmp_rec += QString("/%1%2").arg(m_recPriority2 > 0 ? "+" : "").arg(m_recPriority2);
            tmp_rec += " ";
        }
        else
        {
            tmp_rec += " -- ";
        }
        if (showrerecord && (GetRecordingStatus() == RecStatus::Recorded) &&
            !IsDuplicate())
        {
            tmp_rec += QObject::tr("Re-Record");
        }
        else
        {
            tmp_rec += RecStatus::toString(GetRecordingStatus(), GetRecordingRuleType());
        }
    }
    return tmp_rec;
}




QString ProgramInfo::GetSubtitleTypeNames(void) const
{
    return propsValueToString("subtitle", SubtitlePropsNames,
                              m_subtitleProperties);
}

QString ProgramInfo::GetVideoPropertyNames(void) const
{
    return propsValueToString("video", VideoPropsNames, m_videoProperties);
}

QString ProgramInfo::GetAudioPropertyNames(void) const
{
    return propsValueToString("audio", AudioPropsNames, m_audioProperties);
}

uint ProgramInfo::SubtitleTypesFromNames(const QString & names)
{
    return propsValueFromString("subtitle", SubtitlePropsNames, names);
}

uint ProgramInfo::VideoPropertiesFromNames(const QString & names)
{
    return propsValueFromString("video", VideoPropsNames, names);
}

uint ProgramInfo::AudioPropertiesFromNames(const QString & names)
{
    return propsValueFromString("audio", AudioPropsNames, names);
}

void ProgramInfo::ProgramFlagsFromNames(const QString & names)
{
    m_programFlags = propsValueFromString("program", ProgramFlagNames, names);
}

/** \brief Converts ProgramInfo into QString QHash containing each field
 *         in ProgramInfo converted into localized strings.
 */
void ProgramInfo::ToMap(InfoMap &progMap,
                        bool showrerecord,
                        uint star_range,
                        uint date_format) const
{
    QLocale locale = gCoreContext->GetQLocale();
    // NOTE: Format changes and relevant additions made here should be
    //       reflected in RecordingRule
    QString channelFormat =
        gCoreContext->GetSetting("ChannelFormat", "<num> <sign>");
    QString longChannelFormat =
        gCoreContext->GetSetting("LongChannelFormat", "<num> <name>");

    QDateTime timeNow = MythDate::current();

    progMap["title"] = m_title;
    progMap["subtitle"] = m_subtitle;
    progMap["sorttitle"] = m_sortTitle;
    progMap["sortsubtitle"] = m_sortSubtitle;

    QString tempSubTitle = m_title;
    QString tempSortSubtitle = m_sortTitle;
    if (!m_subtitle.trimmed().isEmpty())
    {
        tempSubTitle = QString("%1 - \"%2\"")
            .arg(tempSubTitle, m_subtitle);
        tempSortSubtitle = QString("%1 - \"%2\"")
            .arg(tempSortSubtitle, m_sortSubtitle);
    }

    progMap["titlesubtitle"] = tempSubTitle;
    progMap["sorttitlesubtitle"] = tempSortSubtitle;

    progMap["description"] = progMap["description0"] = m_description;

    if (m_season > 0 || m_episode > 0)
    {
        progMap["season"] = StringUtil::intToPaddedString(m_season, 1);
        progMap["episode"] = StringUtil::intToPaddedString(m_episode, 1);
        progMap["totalepisodes"] = StringUtil::intToPaddedString(m_totalEpisodes, 1);
        progMap["s00e00"] = QString("s%1e%2")
            .arg(StringUtil::intToPaddedString(GetSeason(), 2),
                 StringUtil::intToPaddedString(GetEpisode(), 2));
        progMap["00x00"] = QString("%1x%2")
            .arg(StringUtil::intToPaddedString(GetSeason(), 1),
                 StringUtil::intToPaddedString(GetEpisode(), 2));
    }
    else
    {
        progMap["season"] = progMap["episode"] = "";
        progMap["totalepisodes"] = "";
        progMap["s00e00"] = progMap["00x00"] = "";
    }
    progMap["syndicatedepisode"] = m_syndicatedEpisode;

    progMap["category"] = m_category;
    progMap["director"] = m_director;

    progMap["callsign"] = m_chanSign;
    progMap["commfree"] = (m_programFlags & FL_CHANCOMMFREE) ? "1" : "0";
    progMap["outputfilters"] = m_chanPlaybackFilters;
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

        if (m_startTs.date().year() == 1895)
        {
           progMap["startdate"] = "";
           progMap["recstartdate"] = "";
        }
        else
        {
            progMap["startdate"] = m_startTs.toLocalTime().toString("yyyy");
            progMap["recstartdate"] = m_startTs.toLocalTime().toString("yyyy");
        }
    }
    else // if (IsRecording())
    {
        using namespace MythDate;
        progMap["starttime"] = MythDate::toString(m_startTs, date_format | kTime);
        progMap["startdate"] =
            MythDate::toString(m_startTs, date_format | kDateFull | kSimplify);
        progMap["shortstartdate"] = MythDate::toString(m_startTs, date_format | kDateShort);
        progMap["endtime"] = MythDate::toString(m_endTs, date_format | kTime);
        progMap["enddate"] = MythDate::toString(m_endTs, date_format | kDateFull | kSimplify);
        progMap["shortenddate"] = MythDate::toString(m_endTs, date_format | kDateShort);
        progMap["recstarttime"] = MythDate::toString(m_recStartTs, date_format | kTime);
        progMap["recstartdate"] = MythDate::toString(m_recStartTs, date_format | kDateShort);
        progMap["recendtime"] = MythDate::toString(m_recEndTs, date_format | kTime);
        progMap["recenddate"] = MythDate::toString(m_recEndTs, date_format | kDateShort);
        progMap["startts"] = QString::number(m_startTs.toSecsSinceEpoch());
        progMap["endts"]   = QString::number(m_endTs.toSecsSinceEpoch());
        if (timeNow.toLocalTime().date().year() !=
            m_startTs.toLocalTime().date().year())
            progMap["startyear"] = m_startTs.toLocalTime().toString("yyyy");
        if (timeNow.toLocalTime().date().year() !=
            m_endTs.toLocalTime().date().year())
            progMap["endyear"] = m_endTs.toLocalTime().toString("yyyy");
    }

    using namespace MythDate;
    progMap["timedate"] =
        MythDate::toString(m_recStartTs, date_format | kDateTimeFull | kSimplify) + " - " +
        MythDate::toString(m_recEndTs, date_format | kTime);

    progMap["shorttimedate"] =
        MythDate::toString(m_recStartTs, date_format | kDateTimeShort | kSimplify) + " - " +
        MythDate::toString(m_recEndTs, date_format | kTime);

    progMap["starttimedate"] =
        MythDate::toString(m_recStartTs, date_format | kDateTimeFull | kSimplify);

    progMap["shortstarttimedate"] =
        MythDate::toString(m_recStartTs, date_format | kDateTimeShort | kSimplify);

    progMap["lastmodifiedtime"] = MythDate::toString(m_lastModified, date_format | kTime);
    progMap["lastmodifieddate"] =
        MythDate::toString(m_lastModified, date_format | kDateFull | kSimplify);
    progMap["lastmodified"] =
        MythDate::toString(m_lastModified, date_format | kDateTimeFull | kSimplify);

    if (m_recordedId)
        progMap["recordedid"] = QString::number(m_recordedId);

    progMap["channum"] = m_chanStr;
    progMap["chanid"] = QString::number(m_chanId);
    progMap["channame"] = m_chanName;
    progMap["channel"] = ChannelText(channelFormat);
    progMap["longchannel"] = ChannelText(longChannelFormat);

    QString tmpSize = locale.toString(m_fileSize * (1.0 / (1024.0 * 1024.0 * 1024.0)), 'f', 2);
    progMap["filesize_str"] = QObject::tr("%1 GB", "GigaBytes").arg(tmpSize);

    progMap["filesize"] = locale.toString((quint64)m_fileSize);

    int seconds = m_recStartTs.secsTo(m_recEndTs);
    int minutes = seconds / 60;

    QString min_str = QObject::tr("%n minute(s)","",minutes);

    progMap["lenmins"] = min_str;
    int hours   = minutes / 60;
    minutes = minutes % 60;

    progMap["lentime"] = min_str;
    if (hours > 0 && minutes > 0)
    {
        min_str = QObject::tr("%n minute(s)","",minutes);
        progMap["lentime"] = QString("%1 %2")
            .arg(QObject::tr("%n hour(s)","", hours), min_str);
    }
    else if (hours > 0)
    {
        progMap["lentime"] = QObject::tr("%n hour(s)","", hours);
    }

    progMap["recordedpercent"] =
        (m_recordedPercent >= 0)
        ?  QString::number(m_recordedPercent) : QString();
    progMap["watchedpercent"] =
        ((m_watchedPercent > 0) && !IsWatched())
        ? QString::number(m_watchedPercent) : QString();

    // This is calling toChar from recordingtypes.cpp, not the QChar
    // constructor.
    progMap["rectypechar"] = toQChar(GetRecordingRuleType());
    progMap["rectype"] = ::toString(GetRecordingRuleType());
    progMap["rectypestatus"] = GetRecTypeStatus(showrerecord);

    progMap["card"] = RecStatus::toString(GetRecordingStatus(),
					  GetShortInputName());
    progMap["input"] = RecStatus::toString(GetRecordingStatus(), m_inputId);
    progMap["inputname"] = m_inputName;
    // Don't add bookmarkupdate to progMap, for now.

    progMap["recpriority"]  = QString::number(m_recPriority);
    progMap["recpriority2"] = QString::number(m_recPriority2);
    progMap["recordinggroup"] = (m_recGroup == "Default")
        ? QObject::tr("Default") : m_recGroup;
    progMap["playgroup"] = m_playGroup;

    if (m_storageGroup == "Default")
        progMap["storagegroup"] = QObject::tr("Default");
    else if (StorageGroup::kSpecialGroups.contains(m_storageGroup))
    {
        // This relies upon the translation established in the
        // definition of StorageGroup::kSpecialGroups.
        // clazy:exclude=tr-non-literal
        progMap["storagegroup"] = QObject::tr(m_storageGroup.toUtf8().constData());
    }
    else
    {
        progMap["storagegroup"] = m_storageGroup;
    }

    progMap["programflags"]    = QString::number(m_programFlags);
    progMap["audioproperties"] = QString::number(m_audioProperties);
    progMap["videoproperties"] = QString::number(m_videoProperties);
    progMap["subtitleType"]    = QString::number(m_subtitleProperties);
    progMap["programflags_names"]    = GetProgramFlagNames();
    progMap["audioproperties_names"] = GetAudioPropertyNames();
    progMap["videoproperties_names"] = GetVideoPropertyNames();
    progMap["subtitleType_names"]    = GetSubtitleTypeNames();

    progMap["recstatus"] = RecStatus::toString(GetRecordingStatus(),
                                      GetRecordingRuleType());
    progMap["recstatuslong"] = RecStatus::toDescription(GetRecordingStatus(),
                                               GetRecordingRuleType(),
                                               GetRecordingStartTime());

    if (IsRepeat())
    {
        progMap["repeat"] = QString("(%1) ").arg(QObject::tr("Repeat"));
        progMap["longrepeat"] = progMap["repeat"];
        if (m_originalAirDate.isValid())
        {
            progMap["longrepeat"] = QString("(%1 %2) ")
                .arg(QObject::tr("Repeat"),
                     MythDate::toString(
                         m_originalAirDate,
                         date_format | MythDate::kDateFull | MythDate::kAddYear));
        }
    }
    else
    {
        progMap["repeat"] = "";
        progMap["longrepeat"] = "";
    }

    progMap["seriesid"] = m_seriesId;
    progMap["programid"] = m_programId;
    progMap["inetref"] = m_inetRef;
    progMap["catType"] = myth_category_type_to_string(m_catType);

    progMap["year"] = m_year > 1895 ? QString::number(m_year) : "";

    progMap["partnumber"] = m_partNumber ? QString::number(m_partNumber) : "";
    progMap["parttotal"] = m_partTotal ? QString::number(m_partTotal) : "";

    QString star_str = (m_stars != 0.0F) ?
        QObject::tr("%n star(s)", "", GetStars(star_range)) : "";
    progMap["stars"] = star_str;
    progMap["numstars"] = QString::number(GetStars(star_range));

    if (m_stars != 0.0F && m_year)
        progMap["yearstars"] = QString("(%1, %2)").arg(m_year).arg(star_str);
    else if (m_stars != 0.0F)
        progMap["yearstars"] = QString("(%1)").arg(star_str);
    else if (m_year)
        progMap["yearstars"] = QString("(%1)").arg(m_year);
    else
        progMap["yearstars"] = "";

    if (!m_originalAirDate.isValid() ||
        (!m_programId.isEmpty() && m_programId.startsWith("MV")))
    {
        progMap["originalairdate"] = "";
        progMap["shortoriginalairdate"] = "";
    }
    else
    {
        progMap["originalairdate"] = MythDate::toString(
            m_originalAirDate, date_format | MythDate::kDateFull);
        progMap["shortoriginalairdate"] = MythDate::toString(
            m_originalAirDate, date_format | MythDate::kDateShort);
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
std::chrono::seconds ProgramInfo::GetSecondsInRecording(void) const
{
    auto recsecs  = std::chrono::seconds(m_recStartTs.secsTo(m_endTs));
    auto duration = std::chrono::seconds(m_startTs.secsTo(m_endTs));
    return (recsecs > 0s) ? recsecs : std::max(duration, 0s);
}

/// \brief Returns catType as a string
QString ProgramInfo::GetCategoryTypeString(void) const
{
    return myth_category_type_to_string(m_catType);
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

QString ProgramInfo::GetShortInputName(void) const
{
    if (qsizetype idx = m_inputName.indexOf('/'); idx >= 0)
    {
        return m_inputName.isRightToLeft() ?
               m_inputName.left(idx) : m_inputName.right(idx);
    }

    return m_inputName.isRightToLeft() ?
           m_inputName.left(2) : m_inputName.right(2);
}

bool ProgramInfo::IsGeneric(void) const
{
    return
        (m_programId.isEmpty() && m_subtitle.isEmpty() &&
         m_description.isEmpty()) ||
        (!m_programId.isEmpty() && m_programId.endsWith("0000")
         && m_catType == kCategorySeries);
}

QString ProgramInfo::toString(const Verbosity v, const QString& sep, const QString& grp)
    const
{
    QString str;
    switch (v)
    {
        case kLongDescription:
            str = LOC + "channame(" + m_chanName + ")\n";
            str += "             startts(" +
                m_startTs.toString() + ") endts(" + m_endTs.toString() + ")\n";
            str += "             recstartts(" + m_recStartTs.toString() +
                ") recendts(" + m_recEndTs.toString() + ")\n";
            str += "             title(" + m_title + ")";
            break;
        case kTitleSubtitle:
            str = m_title.contains(' ') ?
                QString("%1%2%3").arg(grp, m_title, grp) : m_title;
            if (!m_subtitle.isEmpty())
            {
                str += m_subtitle.contains(' ') ?
                    QString("%1%2%3%4").arg(sep, grp, m_subtitle, grp) :
                    QString("%1%2").arg(sep, m_subtitle);
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
    ProgramInfo test(m_chanId, m_recStartTs);
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
        ProgramInfo::clear();
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
        ProgramInfo::clear();
        return false;
    }

    if (!query.next())
    {
        ProgramInfo::clear();
        return false;
    }

    bool is_reload = (m_chanId == _chanid) && (m_recStartTs == _recstartts);
    if (!is_reload)
    {
        // These items are not initialized below so they need to be cleared
        // if we're loading in a different program into this ProgramInfo
        m_catType = kCategoryNone;
        m_lastInUseTime = MythDate::current().addSecs(-kLastInUseOffset);
        m_recType = kNotRecording;
        m_recPriority2 = 0;
        m_parentId = 0;
        m_sourceId = 0;
        m_inputId = 0;

        // everything below this line (in context) is not serialized
        m_spread = m_startCol = -1;
        m_availableStatus = asAvailable;
        m_inUseForWhat.clear();
        m_positionMapDBReplacement = nullptr;
    }

    m_title        = query.value(0).toString();
    m_subtitle     = query.value(1).toString();
    m_description  = query.value(2).toString();
    m_season       = query.value(3).toUInt();
    if (m_season == 0)
        m_season   = query.value(51).toUInt();
    m_episode      = query.value(4).toUInt();
    if (m_episode == 0)
        m_episode  = query.value(52).toUInt();
    m_totalEpisodes = query.value(53).toUInt();
    m_syndicatedEpisode = query.value(48).toString();
    m_category     = query.value(5).toString();

    m_chanId       = _chanid;
    m_chanStr      = QString("#%1").arg(m_chanId);
    m_chanSign     = m_chanStr;
    m_chanName     = m_chanStr;
    m_chanPlaybackFilters.clear();
    if (!query.value(7).toString().isEmpty())
    {
        m_chanStr  = query.value(7).toString();
        m_chanSign = query.value(8).toString();
        m_chanName = query.value(9).toString();
        m_chanPlaybackFilters = query.value(10).toString();
    }

    m_recGroup     = query.value(11).toString();
    m_playGroup    = query.value(12).toString();

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
                    .arg(m_pathname, GetBasename(), new_basename));
        }
        SetPathname(new_basename);
    }

    m_hostname     = query.value(15).toString();
    m_storageGroup = query.value(13).toString();

    m_seriesId     = query.value(17).toString();
    m_programId    = query.value(18).toString();
    m_inetRef      = query.value(19).toString();
    m_catType      = string_to_myth_category_type(query.value(54).toString());

    m_recPriority  = query.value(16).toInt();

    m_fileSize     = query.value(20).toULongLong();

    m_startTs      = MythDate::as_utc(query.value(21).toDateTime());
    m_endTs        = MythDate::as_utc(query.value(22).toDateTime());
    m_recStartTs   = MythDate::as_utc(query.value(24).toDateTime());
    m_recEndTs     = MythDate::as_utc(query.value(25).toDateTime());

    m_stars        = std::clamp((float)query.value(23).toDouble(), 0.0F, 1.0F);

    m_year         = query.value(26).toUInt();
    m_partNumber   = query.value(49).toUInt();
    m_partTotal    = query.value(50).toUInt();

    m_originalAirDate = query.value(27).toDate();
    m_lastModified = MythDate::as_utc(query.value(28).toDateTime());
    /**///m_lastInUseTime;

    m_recStatus    = RecStatus::Recorded;

    /**///m_recPriority2;

    m_recordId     = query.value(29).toUInt();
    /**///m_parentId;

    /**///m_sourcid;
    /**///m_inputId;
    /**///m_cardid;
    m_findId       = query.value(45).toUInt();

    /**///m_recType;
    m_dupIn        = RecordingDupInType(query.value(46).toInt());
    m_dupMethod    = RecordingDupMethodType(query.value(47).toInt());

    m_recordedId   = query.value(55).toUInt();
    m_inputName    = query.value(56).toString();
    m_bookmarkUpdate = MythDate::as_utc(query.value(57).toDateTime());

    // ancillary data -- begin
    m_programFlags = FL_NONE;
    set_flag(m_programFlags, FL_CHANCOMMFREE,
             query.value(30).toInt() == COMM_DETECT_COMMFREE);
    set_flag(m_programFlags, FL_COMMFLAG,
             query.value(31).toInt() == COMM_FLAG_DONE);
    set_flag(m_programFlags, FL_COMMPROCESSING ,
             query.value(31).toInt() == COMM_FLAG_PROCESSING);
    set_flag(m_programFlags, FL_REPEAT,        query.value(32).toBool());
    set_flag(m_programFlags, FL_TRANSCODED,
             query.value(34).toInt() == TRANSCODING_COMPLETE);
    set_flag(m_programFlags, FL_DELETEPENDING, query.value(35).toBool());
    set_flag(m_programFlags, FL_PRESERVED,     query.value(36).toBool());
    set_flag(m_programFlags, FL_CUTLIST,       query.value(37).toBool());
    set_flag(m_programFlags, FL_AUTOEXP,       query.value(38).toBool());
    set_flag(m_programFlags, FL_REALLYEDITING, query.value(39).toBool());
    set_flag(m_programFlags, FL_BOOKMARK,      query.value(40).toBool());
    set_flag(m_programFlags, FL_WATCHED,       query.value(41).toBool());
    set_flag(m_programFlags, FL_LASTPLAYPOS,   query.value(58).toBool());
    set_flag(m_programFlags, FL_EDITING,
             ((m_programFlags & FL_REALLYEDITING) != 0U) ||
             ((m_programFlags & FL_COMMPROCESSING) != 0U));

    m_audioProperties    = query.value(42).toUInt();
    m_videoProperties    = query.value(43).toUInt();
    m_subtitleProperties = query.value(44).toUInt();
    // ancillary data -- end

    if (m_originalAirDate.isValid() && m_originalAirDate < QDate(1895, 12, 28))
        m_originalAirDate = QDate();

    // Extra stuff which is not serialized and may get lost.
    /**/// m_spread
    /**/// m_startCol
    /**/// m_availableStatus
    /**/// m_inUseForWhat
    /**/// m_postitionMapDBReplacement

    return true;
}

/** \fn ProgramInfo::IsSameProgramWeakCheck(const ProgramInfo&) const
 *  \brief Checks for duplicate using only title, chanid and startts.
 *  \param other ProgramInfo to compare this one with.
 */
bool ProgramInfo::IsSameProgramWeakCheck(const ProgramInfo &other) const
{
    return (m_title == other.m_title &&
            m_chanId == other.m_chanId &&
            m_startTs == other.m_startTs);
}

/**
 *  \brief Checks for duplicates according to dupmethod.
 *  \param other ProgramInfo to compare this one with.
 */
bool ProgramInfo::IsDuplicateProgram(const ProgramInfo& other) const
{
    if (GetRecordingRuleType() == kOneRecord)
        return m_recordId == other.m_recordId;

    if (m_findId && m_findId == other.m_findId &&
        (m_recordId == other.m_recordId || m_recordId == other.m_parentId))
           return true;

    if (m_dupMethod & kDupCheckNone)
        return false;

    if (m_title.compare(other.m_title, Qt::CaseInsensitive) != 0)
        return false;

    if (m_catType == kCategorySeries)
    {
        if (m_programId.endsWith("0000"))
            return false;
    }

    if (!m_programId.isEmpty() && !other.m_programId.isEmpty())
    {
        if (s_usingProgIDAuth)
        {
            int index = m_programId.indexOf('/');
            int oindex = other.m_programId.indexOf('/');
            if (index == oindex && (index < 0 ||
                m_programId.left(index) == other.m_programId.left(oindex)))
                return m_programId == other.m_programId;
        }
        else
        {
            return m_programId == other.m_programId;
        }
    }

    if ((m_dupMethod & kDupCheckSub) &&
        ((m_subtitle.isEmpty()) ||
         (m_subtitle.compare(other.m_subtitle, Qt::CaseInsensitive) != 0)))
        return false;

    if ((m_dupMethod & kDupCheckDesc) &&
        ((m_description.isEmpty()) ||
         (m_description.compare(other.m_description, Qt::CaseInsensitive) != 0)))
        return false;

    if ((m_dupMethod & kDupCheckSubThenDesc) &&
        ((m_subtitle.isEmpty() &&
          ((!other.m_subtitle.isEmpty() &&
            m_description.compare(other.m_subtitle, Qt::CaseInsensitive) != 0) ||
           (other.m_subtitle.isEmpty() &&
            m_description.compare(other.m_description, Qt::CaseInsensitive) != 0))) ||
         (!m_subtitle.isEmpty() &&
          ((other.m_subtitle.isEmpty() &&
            m_subtitle.compare(other.m_description, Qt::CaseInsensitive) != 0) ||
           (!other.m_subtitle.isEmpty() &&
            m_subtitle.compare(other.m_subtitle, Qt::CaseInsensitive) != 0)))))
        return false;

    return true;
}

/**
 *  \brief Checks whether this is the same program as "other", which may or may
 *         not be a repeat or on another channel. Matches based on programid
 *         with a fallback to dupmethod
 *  \param other ProgramInfo to compare this one with.
 */
bool ProgramInfo::IsSameProgram(const ProgramInfo& other) const
{
    if (m_title.compare(other.m_title, Qt::CaseInsensitive) != 0)
        return false;

    if (!m_programId.isEmpty() && !other.m_programId.isEmpty())
    {
        if (m_catType == kCategorySeries)
        {
            if (m_programId.endsWith("0000"))
                return false;
        }

        if (s_usingProgIDAuth)
        {
            int index = m_programId.indexOf('/');
            int oindex = other.m_programId.indexOf('/');
            if (index == oindex && (index < 0 ||
                m_programId.left(index) == other.m_programId.left(oindex)))
                return m_programId == other.m_programId;
        }
        else
        {
            return m_programId == other.m_programId;
        }
    }

    if ((m_dupMethod & kDupCheckSub) &&
        ((m_subtitle.isEmpty()) ||
         (m_subtitle.compare(other.m_subtitle, Qt::CaseInsensitive) != 0)))
        return false;

    if ((m_dupMethod & kDupCheckDesc) &&
        ((m_description.isEmpty()) ||
         (m_description.compare(other.m_description, Qt::CaseInsensitive) != 0)))
        return false;

    if ((m_dupMethod & kDupCheckSubThenDesc) &&
        ((m_subtitle.isEmpty() &&
          ((!other.m_subtitle.isEmpty() &&
            m_description.compare(other.m_subtitle, Qt::CaseInsensitive) != 0) ||
           (other.m_subtitle.isEmpty() &&
            m_description.compare(other.m_description, Qt::CaseInsensitive) != 0))) ||
         (!m_subtitle.isEmpty() &&
          ((other.m_subtitle.isEmpty() &&
            m_subtitle.compare(other.m_description, Qt::CaseInsensitive) != 0) ||
           (!other.m_subtitle.isEmpty() &&
            m_subtitle.compare(other.m_subtitle, Qt::CaseInsensitive) != 0)))))
        return false;

    return true;
}

/**
 *  \brief Match same program, with same starttime (channel may be different)
 *  \param other ProgramInfo to compare this one with.
 *  \return true if this program is the same and shares same start time as "other" program.
 */
bool ProgramInfo::IsSameProgramAndStartTime(const ProgramInfo& other) const
{
    if (m_startTs != other.m_startTs)
        return false;
    if (IsSameChannel(other))
        return true;
    if (!IsSameProgram(other))
        return false;
    return true;
}

/**
 *  \brief Checks title, chanid or callsign and start times for equality.
 *  \param other ProgramInfo to compare this one with.
 *  \return true if this program shares same time slot as "other" program.
 */
bool ProgramInfo::IsSameTitleStartTimeAndChannel(const ProgramInfo& other) const
{
    if (m_title.compare(other.m_title, Qt::CaseInsensitive) != 0)
        return false;
    return m_startTs == other.m_startTs &&
        IsSameChannel(other);
}

/**
 *  \brief Checks title, chanid or chansign, start/end times,
 *         cardid, inputid for fully inclusive overlap.
 *  \param other ProgramInfo to compare this one with.
 *  \return true if this program is contained in time slot of "other" program.
 */
bool ProgramInfo::IsSameTitleTimeslotAndChannel(const ProgramInfo &other) const
{
    if (m_title.compare(other.m_title, Qt::CaseInsensitive) != 0)
        return false;
    return IsSameChannel(other) &&
        m_startTs < other.m_endTs &&
        m_endTs > other.m_startTs;
}

/**
 *  \brief Checks whether channel id or callsign are identical
 *  \param other ProgramInfo to compare this one with.
 *  \return true if this program's channel is likely to be the same
 *          as the "other" program's channel
 */
bool ProgramInfo::IsSameChannel(const ProgramInfo& other) const
{
    return m_chanId == other.m_chanId ||
         (!m_chanSign.isEmpty() &&
          m_chanSign.compare(other.m_chanSign, Qt::CaseInsensitive) == 0);
}

void ProgramInfo::CheckProgramIDAuthorities(void)
{
    QMap<QString, int> authMap;
    std::array<QString,3> tables { "program", "recorded", "oldrecorded" };
    MSqlQuery query(MSqlQuery::InitCon());

    for (const QString& table : tables)
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
    }

    int numAuths = authMap.count();
    LOG(VB_GENERAL, LOG_INFO,
        QString("Found %1 distinct programid authorities").arg(numAuths));

    s_usingProgIDAuth = (numAuths > 1);
}

/** \fn ProgramInfo::CreateRecordBasename(const QString &ext) const
 *  \brief Returns a filename for a recording based on the
 *         recording channel and date.
 */
QString ProgramInfo::CreateRecordBasename(const QString &ext) const
{
    QString starts = MythDate::toString(m_recStartTs, MythDate::kFilename);

    QString retval = QString("%1_%2.%3")
        .arg(QString::number(m_chanId), starts, ext);

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
        fn_lower = determineURLType(pathname);

        if (fn_lower.startsWith("dvd:"))
        {
            pit = kProgramInfoTypeVideoDVD;
        }
        else if (fn_lower.startsWith("bd:"))
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

void ProgramInfo::SetPathname(const QString &pn)
{
    m_pathname = pn;

    ProgramInfoType pit = discover_program_info_type(m_chanId, m_pathname, false);
    SetProgramInfoType(pit);
}

ProgramInfoType ProgramInfo::DiscoverProgramInfoType(void) const
{
    return discover_program_info_type(m_chanId, m_pathname, true);
}

void ProgramInfo::SetAvailableStatus(
    AvailableStatusType status, const QString &where)
{
    if (status != m_availableStatus)
    {
        LOG(VB_GUI, LOG_INFO,
            toString(kTitleSubtitle) + QString(": %1 -> %2 in %3")
            .arg(::toString((AvailableStatusType)m_availableStatus),
                 ::toString(status),
                 where));
    }
    m_availableStatus = status;
}

/** \fn ProgramInfo::SaveBasename(const QString&)
 *  \brief Sets a recording's basename in the database.
 */
bool ProgramInfo::SaveBasename(const QString &basename)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE recorded "
                  "SET basename = :BASENAME "
                  "WHERE recordedid = :RECORDEDID;");
    query.bindValue(":RECORDEDID", m_recordedId);
    query.bindValue(":BASENAME", basename);

    if (!query.exec())
    {
        MythDB::DBError("SetRecordBasename", query);
        return false;
    }

    query.prepare("UPDATE recordedfile "
                  "SET basename = :BASENAME "
                  "WHERE recordedid = :RECORDEDID;");
    query.bindValue(":RECORDEDID", m_recordedId);
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
 *  recordedid
 */
QString ProgramInfo::QueryBasename(void) const
{
    QString bn = GetBasename();
    if (!bn.isEmpty())
        return bn;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT basename "
        "FROM recordedfile "
        "WHERE recordedid = :RECORDEDID;");
    query.bindValue(":RECORDEDID", m_recordedId);

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
                 QString("QueryBasename found no entry for recording ID %1")
                     .arg(m_recordedId));
    }

    return {};
}

/** \brief Returns filename or URL to be used to play back this recording.
 *         If the file is accessible locally, the filename will be returned,
 *         otherwise a myth:// URL will be returned.
 *
 *  \note This method sometimes initiates a QUERY_CHECKFILE MythProto
 *        call and so should not be called from the UI thread.
 */
QString ProgramInfo::GetPlaybackURL(
    bool checkMaster, bool forceCheckLocal)
{
        // return the original path if BD or DVD URI
    if (IsVideoBD() || IsVideoDVD())
        return GetPathname();

    QString basename = QueryBasename();
    if (basename.isEmpty())
        return "";

    bool checklocal = !gCoreContext->GetBoolSetting("AlwaysStreamFiles", false) ||
                      forceCheckLocal;

    if (IsVideo())
    {
        QString fullpath = GetPathname();
        if (!fullpath.startsWith("myth://", Qt::CaseInsensitive) || !checklocal)
            return fullpath;

        QUrl    url  = QUrl(fullpath);
        QString path = url.path();
        QString host = url.toString(QUrl::RemovePath).mid(7);
        QStringList list = host.split(":", Qt::SkipEmptyParts);
        if (!list.empty())
        {
            host = list[0];
            list = host.split("@", Qt::SkipEmptyParts);
            QString group;
            if (!list.empty() && list.size() < 3)
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
        StorageGroup sgroup(m_storageGroup);
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
        if (m_hostname == gCoreContext->GetHostName())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("GetPlaybackURL: '%1' should be local, but it can "
                        "not be found.").arg(basename));
            // Note do not preceed with "/" that will cause existing code
            // to look for a local file with this name...
            return QString("GetPlaybackURL/UNABLE/TO/FIND/LOCAL/FILE/ON/%1/%2")
                           .arg(m_hostname, basename);
        }
    }

    // Check to see if we should stream from the master backend
    if ((checkMaster) &&
        (gCoreContext->GetBoolSetting("MasterBackendOverride", false)) &&
        (RemoteCheckFile(this, false)))
    {
        tmpURL = MythCoreContext::GenMythURL(gCoreContext->GetMasterHostName(),
                                             MythCoreContext::GetMasterServerPort(),
                                             basename);

        LOG(VB_FILE, LOG_INFO, LOC +
            QString("GetPlaybackURL: Found @ '%1'").arg(tmpURL));
        return tmpURL;
    }

    // Fallback to streaming from the backend the recording was created on
    tmpURL = MythCoreContext::GenMythURL(m_hostname,
                                         gCoreContext->GetBackendServerPort(m_hostname),
                                         basename);

    LOG(VB_FILE, LOG_INFO, LOC +
        QString("GetPlaybackURL: Using default of: '%1'") .arg(tmpURL));

    return tmpURL;
}

/** \brief Queries multiplex any recording would be made on, zero if unknown.
 */
uint ProgramInfo::QueryMplexID(void) const
{
    uint ret = 0U;
    if (m_chanId)
    {
        MSqlQuery query(MSqlQuery::InitCon());

        query.prepare("SELECT mplexid FROM channel "
                      "WHERE chanid = :CHANID");
        query.bindValue(":CHANID", m_chanId);

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

    set_flag(m_programFlags, FL_BOOKMARK, is_valid);

    UpdateMarkTimeStamp(is_valid);
    SendUpdateEvent();
}

void ProgramInfo::UpdateMarkTimeStamp(bool bookmarked) const
{
    if (IsRecording())
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare(
            "UPDATE recorded "
            "SET bookmarkupdate = CURRENT_TIMESTAMP, "
            "    bookmark       = :BOOKMARKFLAG "
            "WHERE recordedid = :RECORDEDID");

        query.bindValue(":BOOKMARKFLAG", bookmarked);
        query.bindValue(":RECORDEDID",   m_recordedId);

        if (!query.exec())
            MythDB::DBError("bookmark flag update", query);
    }
}

void ProgramInfo::SaveLastPlayPos(uint64_t frame)
{
    ClearMarkupMap(MARK_UTIL_LASTPLAYPOS);

    bool isValid = frame > 0;
    if (isValid)
    {
        frm_dir_map_t lastPlayPosMap;
        lastPlayPosMap[frame] = MARK_UTIL_LASTPLAYPOS;
        SaveMarkupMap(lastPlayPosMap, MARK_UTIL_LASTPLAYPOS);
    }

    set_flag(m_programFlags, FL_LASTPLAYPOS, isValid);

    UpdateLastPlayTimeStamp(isValid);
    SendUpdateEvent();
}

// This function overloads the 'bookmarkupdate' field to force the UI
// to update when the last play timestamp is updated. The alternative
// is adding another field to the database and to the programinfo
// serialization.
void ProgramInfo::UpdateLastPlayTimeStamp(bool hasLastPlay) const
{
    if (IsRecording())
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare(
            "UPDATE recorded "
            "SET bookmarkupdate = CURRENT_TIMESTAMP, "
            "    lastplay       = :LASTPLAYFLAG "
            "WHERE recordedid = :RECORDEDID");

        query.bindValue(":LASTPLAYFLAG", hasLastPlay);
        query.bindValue(":RECORDEDID",   m_recordedId);

        if (!query.exec())
            MythDB::DBError("lastplay flag update", query);
    }
}

void ProgramInfo::SendUpdateEvent(void) const
{
    if (IsRecording())
        s_updater->insert(m_recordedId, kPIUpdate);
}

void ProgramInfo::SendAddedEvent(void) const
{
    s_updater->insert(m_recordedId, kPIAdd);
}

void ProgramInfo::SendDeletedEvent(void) const
{
    s_updater->insert(m_recordedId, kPIDelete);
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
    query.bindValue(":CHANID",    m_chanId);
    query.bindValue(":STARTTIME", m_recStartTs);

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
    if (m_programFlags & FL_IGNOREBOOKMARK)
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

/** \brief Gets any lastplaypos position in database,
 *         unless the ignore lastplaypos flag is set.
 *
 *  \return LastPlayPos position in frames if the query is executed
 *          and succeeds, zero otherwise.
 */
uint64_t ProgramInfo::QueryLastPlayPos() const
{
    if (m_programFlags & FL_IGNORELASTPLAYPOS)
        return 0;

    frm_dir_map_t bookmarkmap;
    QueryMarkupMap(bookmarkmap, MARK_UTIL_LASTPLAYPOS);

    return (bookmarkmap.isEmpty()) ? 0 : bookmarkmap.begin().key();
}

/** \brief Gets any progstart position in database,
 *         unless the ignore progstart flag is set.
 *
 *  \return Progstart position in frames if the query is executed
 *          and succeeds, zero otherwise.
 */
uint64_t ProgramInfo::QueryProgStart(void) const
{
    if (m_programFlags & FL_IGNOREPROGSTART)
        return 0;

    frm_dir_map_t bookmarkmap;
    QueryMarkupMap(bookmarkmap, MARK_UTIL_PROGSTART);

    return (bookmarkmap.isEmpty()) ? 0 : bookmarkmap.begin().key();
}

uint64_t ProgramInfo::QueryStartMark(void) const
{
    uint64_t start = QueryLastPlayPos();
    if (start > 0)
    {
        LOG(VB_PLAYBACK, LOG_INFO, QString("Using last position @ %1").arg(start));
        return start;
    }

    start = QueryBookmark();
    if (start > 0)
    {
        LOG(VB_PLAYBACK, LOG_INFO, QString("Using bookmark @ %1").arg(start));
        return start;
    }

    if (HasCutlist())
    {
        // Disable progstart if the program has a cutlist.
        LOG(VB_PLAYBACK, LOG_INFO, "Ignoring progstart as cutlist exists");
        return 0;
    }

    start = QueryProgStart();
    if (start > 0)
    {
        LOG(VB_PLAYBACK, LOG_INFO, QString("Using progstart @ %1").arg(start));
        return start;
    }

    LOG(VB_PLAYBACK, LOG_INFO, "Using file start");
    return 0;
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

    if (!(m_programFlags & FL_IGNOREBOOKMARK))
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

void ProgramInfo::SaveDVDBookmark(const QStringList &fields)
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

/** \brief Queries "bdbookmark" table for bookmarking BD serial number.
 *  \return BD state string
 */
QStringList ProgramInfo::QueryBDBookmark(const QString &serialid) const
{
    QStringList fields = QStringList();
    MSqlQuery query(MSqlQuery::InitCon());

    if (!(m_programFlags & FL_IGNOREBOOKMARK))
    {
        query.prepare(" SELECT bdstate FROM bdbookmark "
                        " WHERE serialid = :SERIALID ");
        query.bindValue(":SERIALID", serialid);

        if (query.exec() && query.next())
            fields.append(query.value(0).toString());
    }

    return fields;
}

void ProgramInfo::SaveBDBookmark(const QStringList &fields)
{
    QStringList::const_iterator it = fields.begin();
    MSqlQuery query(MSqlQuery::InitCon());

    QString serialid    = *(it);
    QString name        = *(++it);

    if( fields.count() == 3 )
    {
        // We have a state field, so update/create the bookmark
        QString state = *(++it);

        query.prepare("INSERT IGNORE INTO bdbookmark "
                        " (serialid, name)"
                        " VALUES ( :SERIALID, :NAME );");
        query.bindValue(":SERIALID", serialid);
        query.bindValue(":NAME", name);

        if (!query.exec())
            MythDB::DBError("SetBDBookmark inserting", query);

        query.prepare(" UPDATE bdbookmark "
                        " SET bdstate    = :STATE , "
                        "     timestamp  = NOW() "
                        " WHERE serialid = :SERIALID");
        query.bindValue(":STATE",state);
        query.bindValue(":SERIALID",serialid);
    }
    else
    {
        // No state field, delete the bookmark
        query.prepare("DELETE FROM bdbookmark "
                        "WHERE serialid = :SERIALID");
        query.bindValue(":SERIALID",serialid);
    }

    if (!query.exec())
        MythDB::DBError("SetBDBookmark updating", query);
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

    query.bindValue(":CHANID", m_chanId);
    query.bindValue(":STARTTIME", m_startTs);

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
        query.bindValue(":CHANID", m_chanId);
        query.bindValue(":STARTTIME", m_recStartTs);
        query.bindValue(":WATCHEDFLAG", watched);

        if (!query.exec())
            MythDB::DBError("Set watched flag", query);
        else
            UpdateLastDelete(watched);

        SendUpdateEvent();
    }
    else if (IsVideoFile())
    {
        QString url = m_pathname;
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
        query.bindValue(":TITLE", m_title);
        query.bindValue(":SUBTITLE", m_subtitle);
        query.bindValue(":FILENAME", url);
        query.bindValue(":WATCHEDFLAG", watched);

        if (!query.exec())
            MythDB::DBError("Set watched flag", query);
    }

    set_flag(m_programFlags, FL_WATCHED, watched);
}

/** \brief Queries "recorded" table for its "editing" field
 *         and returns true if it is set to true.
 *  \return true if we have started, but not finished, editing.
 */
bool ProgramInfo::QueryIsEditing(void) const
{
    bool editing = (m_programFlags & FL_REALLYEDITING) != 0U;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT editing FROM recorded"
                  " WHERE chanid = :CHANID"
                  " AND starttime = :STARTTIME ;");
    query.bindValue(":CHANID", m_chanId);
    query.bindValue(":STARTTIME", m_recStartTs);

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
    query.bindValue(":CHANID", m_chanId);
    query.bindValue(":STARTTIME", m_recStartTs);

    if (!query.exec())
        MythDB::DBError("Edit status update", query);

    set_flag(m_programFlags, FL_REALLYEDITING, edit);
    set_flag(m_programFlags, FL_EDITING, (((m_programFlags & FL_REALLYEDITING) != 0U) ||
                                          ((m_programFlags & COMM_FLAG_PROCESSING) != 0U)));

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
    query.bindValue(":CHANID", m_chanId);
    query.bindValue(":STARTTIME", m_recStartTs);
    query.bindValue(":DELETEFLAG", deleteFlag);

    if (!query.exec())
        MythDB::DBError("SaveDeletePendingFlag", query);

    set_flag(m_programFlags, FL_DELETEPENDING, deleteFlag);

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

    QDateTime oneHourAgo = MythDate::current().addSecs(-kLastUpdateOffset);
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT hostname, recusage FROM inuseprograms "
                  " WHERE chanid = :CHANID"
                  " AND starttime = :STARTTIME "
                  " AND lastupdatetime > :ONEHOURAGO ;");
    query.bindValue(":CHANID", m_chanId);
    query.bindValue(":STARTTIME", m_recStartTs);
    query.bindValue(":ONEHOURAGO", oneHourAgo);

    byWho.clear();
    if (query.exec() && query.size() > 0)
    {
        QString usageStr;
        QString recusage;
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
    for (int i = 0; i+2 < users.size(); i+=3)
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
        uint play_cnt = 0;
        uint ft_cnt = 0;
        uint jq_cnt = 0;
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
    query.bindValue(":CHANID", m_chanId);
    query.bindValue(":STARTTIME", m_recStartTs);

    if (query.exec() && query.next())
        return (TranscodingStatus) query.value(0).toUInt();
    return TRANSCODING_NOT_TRANSCODED;
}

/** \brief Set "transcoded" field in "recorded" table to "trans".
 *  \note Also sets the FL_TRANSCODED flag if the status is
 *        TRASCODING_COMPLETE and clears it otherwise.
 *  \param trans value to set transcoded field to.
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
    query.bindValue(":CHANID", m_chanId);
    query.bindValue(":STARTTIME", m_recStartTs);

    if (!query.exec())
        MythDB::DBError("Transcoded status update", query);

    set_flag(m_programFlags, FL_TRANSCODED, TRANSCODING_COMPLETE == trans);
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
    query.bindValue(":CHANID", m_chanId);
    query.bindValue(":STARTTIME", m_recStartTs);

    if (!query.exec())
        MythDB::DBError("Commercial Flagged status update", query);

    set_flag(m_programFlags, FL_COMMFLAG,       COMM_FLAG_DONE == flag);
    set_flag(m_programFlags, FL_COMMPROCESSING, COMM_FLAG_PROCESSING == flag);
    set_flag(m_programFlags, FL_EDITING, (((m_programFlags & FL_REALLYEDITING) != 0U) ||
                                          ((m_programFlags & COMM_FLAG_PROCESSING) != 0U)));
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
    query.bindValue(":CHANID", m_chanId);
    query.bindValue(":STARTTIME", m_recStartTs);

    if (!query.exec())
        MythDB::DBError("PreserveEpisode update", query);
    else
        UpdateLastDelete(false);

    set_flag(m_programFlags, FL_PRESERVED, preserveEpisode);

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
    query.bindValue(":CHANID", m_chanId);
    query.bindValue(":STARTTIME", m_recStartTs);

    if (!query.exec())
        MythDB::DBError("AutoExpire update", query);
    else if (updateDelete)
        UpdateLastDelete(true);

    set_flag(m_programFlags, FL_AUTOEXP, autoExpire != kDisableAutoExpire);

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
        auto delay_secs = std::chrono::seconds(m_recStartTs.secsTo(timeNow));
        auto delay = duration_cast<std::chrono::hours>(delay_secs);
        delay = std::clamp(delay, 1h, 200h);

        query.prepare("UPDATE record SET last_delete = :TIME, "
                      "avg_delay = (avg_delay * 3 + :DELAY) / 4 "
                      "WHERE recordid = :RECORDID");
        query.bindValue(":TIME", timeNow);
        query.bindValue(":DELAY", static_cast<qint64>(delay.count()));
    }
    else
    {
        query.prepare("UPDATE record SET last_delete = NULL "
                      "WHERE recordid = :RECORDID");
    }
    query.bindValue(":RECORDID", m_recordId);

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
    query.bindValue(":CHANID", m_chanId);
    query.bindValue(":STARTTIME", m_recStartTs);

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
        // NOLINTNEXTLINE(modernize-loop-convert)
        for (auto i = autosaveMap.constBegin(); i != autosaveMap.constEnd(); ++i)
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
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (auto i = delMap.constBegin(); i != delMap.constEnd(); ++i)
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
        query.bindValue(":CHANID", m_chanId);
        query.bindValue(":STARTTIME", m_recStartTs);

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
        query.bindValue(":PATH", StorageGroup::GetRelativePathname(m_pathname));
    }
    else if (IsRecording())
    {
        query.prepare("DELETE FROM recordedmarkup"
                      " WHERE chanid = :CHANID"
                      " AND STARTTIME = :STARTTIME"
                      + comp + ';');
        query.bindValue(":CHANID", m_chanId);
        query.bindValue(":STARTTIME", m_recStartTs);
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
       videoPath = StorageGroup::GetRelativePathname(m_pathname);
    }
    else if (IsRecording())
    {
        // check to make sure the show still exists before saving markups
        query.prepare("SELECT starttime FROM recorded"
                      " WHERE chanid = :CHANID"
                      " AND starttime = :STARTTIME ;");
        query.bindValue(":CHANID", m_chanId);
        query.bindValue(":STARTTIME", m_recStartTs);

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

        if ((min_frame >= 0) && (frame < (uint64_t)min_frame))
            continue;

        if ((max_frame >= 0) && (frame > (uint64_t)max_frame))
            continue;

        int mark_type = (type != MARK_ALL) ? type : *it;

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
            query.bindValue(":CHANID", m_chanId);
            query.bindValue(":STARTTIME", m_recStartTs);
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
        QueryMarkupMap(StorageGroup::GetRelativePathname(m_pathname),
                       marks, type, merge);
    }
    else if (IsRecording())
    {
        QueryMarkupMap(m_chanId, m_recStartTs, marks, type, merge);
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

    query.prepare("SELECT mark, type, `offset` "
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
        // marks[query.value(0).toLongLong()] =
        //     (MarkTypes) query.value(1).toInt();
        int entryType = query.value(1).toInt();
        if (entryType == MARK_VIDEO_RATE)
            marks[query.value(2).toLongLong()] = (MarkTypes) entryType;
        else
            marks[query.value(0).toLongLong()] = (MarkTypes) entryType;

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
    query.prepare("SELECT mark, type, data "
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
        // marks[query.value(0).toULongLong()] =
        //     (MarkTypes) query.value(1).toInt();
        int entryType = query.value(1).toInt();
        if (entryType == MARK_VIDEO_RATE)
            marks[query.value(2).toULongLong()] = (MarkTypes) entryType;
        else
            marks[query.value(0).toULongLong()] = (MarkTypes) entryType;
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
    if (m_positionMapDBReplacement)
    {
        QMutexLocker locker(m_positionMapDBReplacement->lock);
        posMap = m_positionMapDBReplacement->map[type];

        return;
    }

    posMap.clear();
    MSqlQuery query(MSqlQuery::InitCon());

    if (IsVideo())
    {
        query.prepare("SELECT mark, `offset` FROM filemarkup"
                      " WHERE filename = :PATH"
                      " AND type = :TYPE ;");
        query.bindValue(":PATH", StorageGroup::GetRelativePathname(m_pathname));
    }
    else if (IsRecording())
    {
        query.prepare("SELECT mark, `offset` FROM recordedseek"
                      " WHERE chanid = :CHANID"
                      " AND starttime = :STARTTIME"
                      " AND type = :TYPE ;");
        query.bindValue(":CHANID", m_chanId);
        query.bindValue(":STARTTIME", m_recStartTs);
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
    if (m_positionMapDBReplacement)
    {
        QMutexLocker locker(m_positionMapDBReplacement->lock);
        m_positionMapDBReplacement->map.clear();
        return;
    }

    MSqlQuery query(MSqlQuery::InitCon());

    if (IsVideo())
    {
        query.prepare("DELETE FROM filemarkup"
                      " WHERE filename = :PATH"
                      " AND type = :TYPE ;");
        query.bindValue(":PATH", StorageGroup::GetRelativePathname(m_pathname));
    }
    else if (IsRecording())
    {
        query.prepare("DELETE FROM recordedseek"
                      " WHERE chanid = :CHANID"
                      " AND starttime = :STARTTIME"
                      " AND type = :TYPE ;");
        query.bindValue(":CHANID", m_chanId);
        query.bindValue(":STARTTIME", m_recStartTs);
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
    if (m_positionMapDBReplacement)
    {
        QMutexLocker locker(m_positionMapDBReplacement->lock);

        if ((min_frame >= 0) || (max_frame >= 0))
        {
            frm_pos_map_t new_map;
            // NOLINTNEXTLINE(modernize-loop-convert)
            for (auto it = m_positionMapDBReplacement->map[type].begin();
                 it != m_positionMapDBReplacement->map[type].end();
                 ++it)
            {
                uint64_t frame = it.key();
                if ((min_frame >= 0) && (frame >= (uint64_t)min_frame))
                    continue;
                if ((min_frame >= 0) && (frame <= (uint64_t)max_frame))
                    continue;
                new_map.insert(it.key(), *it);
            }
            m_positionMapDBReplacement->map[type] = new_map;
        }
        else
        {
            m_positionMapDBReplacement->map[type].clear();
        }

        // NOLINTNEXTLINE(modernize-loop-convert)
        for (auto it = posMap.cbegin(); it != posMap.cend(); ++it)
        {
            uint64_t frame = it.key();
            if ((min_frame >= 0) && (frame >= (uint64_t)min_frame))
                continue;
            if ((min_frame >= 0) && (frame <= (uint64_t)max_frame))
                continue;

            m_positionMapDBReplacement->map[type]
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
        videoPath = StorageGroup::GetRelativePathname(m_pathname);

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
        query.bindValue(":CHANID", m_chanId);
        query.bindValue(":STARTTIME", m_recStartTs);
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

    if (posMap.isEmpty())
        return;

    // Use the multi-value insert syntax to reduce database I/O
    QStringList q("INSERT INTO ");
    QString qfields;
    if (IsVideo())
    {
        q << "filemarkup (filename, type, mark, `offset`)";
        qfields = QString("('%1',%2,") .
            // ideally, this should be escaped
            arg(videoPath) .
            arg(type);
    }
    else // if (IsRecording())
    {
        q << "recordedseek (chanid, starttime, type, mark, `offset`)";
        qfields = QString("(%1,'%2',%3,") .
            arg(m_chanId) .
            arg(m_recStartTs.toString(Qt::ISODate)) .
            arg(type);
    }
    q << " VALUES ";

    bool add_comma = false;
    frm_pos_map_t::iterator it;
    for (it = posMap.begin(); it != posMap.end(); ++it)
    {
        uint64_t frame = it.key();

        if ((min_frame >= 0) && (frame < (uint64_t)min_frame))
            continue;

        if ((max_frame >= 0) && (frame > (uint64_t)max_frame))
            continue;

        uint64_t offset = *it;

        if (add_comma)
        {
            q << ",";
        }
        else
        {
            add_comma = true;
        }
        q << qfields << QString("%1,%2)").arg(frame).arg(offset);
    }
    query.prepare(q.join(""));
    if (!query.exec())
    {
        MythDB::DBError("position map insert", query);
    }
}

void ProgramInfo::SavePositionMapDelta(
    frm_pos_map_t &posMap, MarkTypes type) const
{
    if (posMap.isEmpty())
        return;

    if (m_positionMapDBReplacement)
    {
        QMutexLocker locker(m_positionMapDBReplacement->lock);

        for (auto it = posMap.cbegin(); it != posMap.cend(); ++it)
            m_positionMapDBReplacement->map[type].insert(it.key(), *it);

        return;
    }

    // Use the multi-value insert syntax to reduce database I/O
    QStringList q("INSERT INTO ");
    QString qfields;
    if (IsVideo())
    {
        q << "filemarkup (filename, type, mark, `offset`)";
        qfields = QString("('%1',%2,") .
            // ideally, this should be escaped
            arg(StorageGroup::GetRelativePathname(m_pathname)) .
            arg(type);
    }
    else if (IsRecording())
    {
        q << "recordedseek (chanid, starttime, type, mark, `offset`)";
        qfields = QString("(%1,'%2',%3,") .
            arg(m_chanId) .
            arg(m_recStartTs.toString(Qt::ISODate)) .
            arg(type);
    }
    else
    {
        return;
    }
    q << " VALUES ";

    bool add_comma = false;
    frm_pos_map_t::iterator it;
    for (it = posMap.begin(); it != posMap.end(); ++it)
    {
        uint64_t frame  = it.key();
        uint64_t offset = *it;

        if (add_comma)
        {
            q << ",";
        }
        else
        {
            add_comma = true;
        }
        q << qfields << QString("%1,%2)").arg(frame).arg(offset);
    }

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(q.join(""));
    if (!query.exec())
    {
        MythDB::DBError("delta position map insert", query);
    }
}

static const char *from_filemarkup_offset_asc =
    "SELECT mark, `offset` FROM filemarkup"
    " WHERE filename = :PATH"
    " AND type = :TYPE"
    " AND mark >= :QUERY_ARG"
    " ORDER BY filename ASC, type ASC, mark ASC LIMIT 1;";
static const char *from_filemarkup_offset_desc =
    "SELECT mark, `offset` FROM filemarkup"
    " WHERE filename = :PATH"
    " AND type = :TYPE"
    " AND mark <= :QUERY_ARG"
    " ORDER BY filename DESC, type DESC, mark DESC LIMIT 1;";
static const char *from_recordedseek_offset_asc =
    "SELECT mark, `offset` FROM recordedseek"
    " WHERE chanid = :CHANID"
    " AND starttime = :STARTTIME"
    " AND type = :TYPE"
    " AND mark >= :QUERY_ARG"
    " ORDER BY chanid ASC, starttime ASC, type ASC, mark ASC LIMIT 1;";
static const char *from_recordedseek_offset_desc =
    "SELECT mark, `offset` FROM recordedseek"
    " WHERE chanid = :CHANID"
    " AND starttime = :STARTTIME"
    " AND type = :TYPE"
    " AND mark <= :QUERY_ARG"
    " ORDER BY chanid DESC, starttime DESC, type DESC, mark DESC LIMIT 1;";
static const char *from_filemarkup_mark_asc =
    "SELECT `offset`,mark FROM filemarkup"
    " WHERE filename = :PATH"
    " AND type = :TYPE"
    " AND `offset` >= :QUERY_ARG"
    " ORDER BY filename ASC, type ASC, mark ASC LIMIT 1;";
static const char *from_filemarkup_mark_desc =
    "SELECT `offset`,mark FROM filemarkup"
    " WHERE filename = :PATH"
    " AND type = :TYPE"
    " AND `offset` <= :QUERY_ARG"
    " ORDER BY filename DESC, type DESC, mark DESC LIMIT 1;";
static const char *from_recordedseek_mark_asc =
    "SELECT `offset`,mark FROM recordedseek"
    " WHERE chanid = :CHANID"
    " AND starttime = :STARTTIME"
    " AND type = :TYPE"
    " AND `offset` >= :QUERY_ARG"
    " ORDER BY chanid ASC, starttime ASC, type ASC, mark ASC LIMIT 1;";
static const char *from_recordedseek_mark_desc =
    "SELECT `offset`,mark FROM recordedseek"
    " WHERE chanid = :CHANID"
    " AND starttime = :STARTTIME"
    " AND type = :TYPE"
    " AND `offset` <= :QUERY_ARG"
    " ORDER BY chanid DESC, starttime DESC, type DESC, mark DESC LIMIT 1;";

bool ProgramInfo::QueryKeyFrameInfo(uint64_t * result,
                                    uint64_t position_or_keyframe,
                                    bool backwards,
                                    MarkTypes type,
                                    const char *from_filemarkup_asc,
                                    const char *from_filemarkup_desc,
                                    const char *from_recordedseek_asc,
                                    const char *from_recordedseek_desc) const
{
    MSqlQuery query(MSqlQuery::InitCon());

    if (IsVideo())
    {
        if (backwards)
            query.prepare(from_filemarkup_desc);
        else
            query.prepare(from_filemarkup_asc);
        query.bindValue(":PATH", StorageGroup::GetRelativePathname(m_pathname));
    }
    else if (IsRecording())
    {
        if (backwards)
            query.prepare(from_recordedseek_desc);
        else
            query.prepare(from_recordedseek_asc);
        query.bindValue(":CHANID", m_chanId);
        query.bindValue(":STARTTIME", m_recStartTs);
    }
    query.bindValue(":TYPE", type);
    query.bindValue(":QUERY_ARG", (unsigned long long)position_or_keyframe);

    if (!query.exec())
    {
        MythDB::DBError("QueryKeyFrameInfo", query);
        return false;
    }

    if (query.next())
    {
        *result = query.value(1).toULongLong();
        return true;
    }

    if (IsVideo())
    {
        if (backwards)
            query.prepare(from_filemarkup_asc);
        else
            query.prepare(from_filemarkup_desc);
        query.bindValue(":PATH", StorageGroup::GetRelativePathname(m_pathname));
    }
    else if (IsRecording())
    {
        if (backwards)
            query.prepare(from_recordedseek_asc);
        else
            query.prepare(from_recordedseek_desc);
        query.bindValue(":CHANID", m_chanId);
        query.bindValue(":STARTTIME", m_recStartTs);
    }
    query.bindValue(":TYPE", type);
    query.bindValue(":QUERY_ARG", (unsigned long long)position_or_keyframe);

    if (!query.exec())
    {
        MythDB::DBError("QueryKeyFrameInfo", query);
        return false;
    }

    if (query.next())
    {
        *result = query.value(1).toULongLong();
        return true;
    }

    return false;

}

bool ProgramInfo::QueryPositionKeyFrame(uint64_t *keyframe, uint64_t position,
                                        bool backwards) const
{
   return QueryKeyFrameInfo(keyframe, position, backwards, MARK_GOP_BYFRAME,
                            from_filemarkup_mark_asc,
                            from_filemarkup_mark_desc,
                            from_recordedseek_mark_asc,
                            from_recordedseek_mark_desc);
}
bool ProgramInfo::QueryKeyFramePosition(uint64_t *position, uint64_t keyframe,
                                        bool backwards) const
{
   return QueryKeyFrameInfo(position, keyframe, backwards, MARK_GOP_BYFRAME,
                            from_filemarkup_offset_asc,
                            from_filemarkup_offset_desc,
                            from_recordedseek_offset_asc,
                            from_recordedseek_offset_desc);
}
bool ProgramInfo::QueryDurationKeyFrame(uint64_t *keyframe, uint64_t duration,
                                        bool backwards) const
{
   return QueryKeyFrameInfo(keyframe, duration, backwards, MARK_DURATION_MS,
                            from_filemarkup_mark_asc,
                            from_filemarkup_mark_desc,
                            from_recordedseek_mark_asc,
                            from_recordedseek_mark_desc);
}
bool ProgramInfo::QueryKeyFrameDuration(uint64_t *duration, uint64_t keyframe,
                                        bool backwards) const
{
   return QueryKeyFrameInfo(duration, keyframe, backwards, MARK_DURATION_MS,
                            from_filemarkup_offset_asc,
                            from_filemarkup_offset_desc,
                            from_recordedseek_offset_asc,
                            from_recordedseek_offset_desc);
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
    query.bindValue(":CHANID", m_chanId);
    query.bindValue(":STARTTIME", m_recStartTs);

    query.bindValue(":MARK", (quint64)frame);
    query.bindValue(":TYPE", type);

    if (type == MARK_ASPECT_CUSTOM)
        query.bindValue(":DATA", customAspect);
    else
    {
        // create NULL value
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
        query.bindValue(":DATA", QVariant(QVariant::UInt));
#else
        query.bindValue(":DATA", QVariant(QMetaType(QMetaType::UInt)));
#endif
    }

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
    query.bindValue(":CHANID", m_chanId);
    query.bindValue(":STARTTIME", m_recStartTs);
    query.bindValue(":MARK", (quint64)frame);
    query.bindValue(":TYPE", MARK_VIDEO_RATE);
    query.bindValue(":DATA", framerate);

    if (!query.exec())
        MythDB::DBError("Frame rate insert", query);
}


/// \brief Store the Progressive/Interlaced state in the recordedmarkup table
/// \note  All frames until the next one with a stored Scan type
///        are assumed to have the same scan type
void ProgramInfo::SaveVideoScanType(uint64_t frame, bool progressive)
{
    if (!IsRecording())
        return;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("INSERT INTO recordedmarkup"
                  "    (chanid, starttime, mark, type, data)"
                  "    VALUES"
                  " ( :CHANID, :STARTTIME, :MARK, :TYPE, :DATA);");
    query.bindValue(":CHANID", m_chanId);
    query.bindValue(":STARTTIME", m_recStartTs);
    query.bindValue(":MARK", (quint64)frame);
    query.bindValue(":TYPE", MARK_SCAN_PROGRESSIVE);
    query.bindValue(":DATA", progressive);

    if (!query.exec())
        MythDB::DBError("Video scan type insert", query);
}


static void delete_markup_datum(
    MarkTypes type, uint chanid, const QDateTime &recstartts)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("DELETE FROM recordedmarkup "
                " WHERE chanid=:CHANID "
                " AND starttime=:STARTTIME "
                " AND type=:TYPE");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);
    query.bindValue(":TYPE", type);

    if (!query.exec())
        MythDB::DBError("delete_markup_datum", query);
}

static void delete_markup_datum(
    MarkTypes type, const QString &videoPath)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("DELETE FROM filemarkup"
                " WHERE filename = :PATH "
                " AND type = :TYPE ;");
    query.bindValue(":PATH", videoPath);
    query.bindValue(":TYPE", type);

    if (!query.exec())
        MythDB::DBError("delete_markup_datum", query);
}

static void insert_markup_datum(
    MarkTypes type, uint mark, uint offset, const QString &videoPath)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("INSERT INTO filemarkup"
                "   (filename, mark, type, `offset`)"
                " VALUES"
                "   ( :PATH , :MARK , :TYPE, :OFFSET );");
    query.bindValue(":PATH", videoPath);
    query.bindValue(":OFFSET", offset);
    query.bindValue(":TYPE", type);
    query.bindValue(":MARK", mark);

    if (!query.exec())
        MythDB::DBError("insert_markup_datum", query);
}

static void insert_markup_datum(
    MarkTypes type, uint mark, uint data, uint chanid, const QDateTime &recstartts)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("INSERT INTO recordedmarkup"
                "    (chanid, starttime, mark, type, data)"
                "    VALUES"
                " ( :CHANID, :STARTTIME, :MARK, :TYPE, :DATA);");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);
    query.bindValue(":DATA", data);
    query.bindValue(":TYPE", type);
    query.bindValue(":MARK", mark);

    if (!query.exec())
        MythDB::DBError("insert_markup_datum", query);
}


/// \brief Store the Total Duration at frame 0 in the recordedmarkup table
void ProgramInfo::SaveTotalDuration(std::chrono::milliseconds duration)
{
    if (IsVideo())
    {
        auto videoPath = StorageGroup::GetRelativePathname(m_pathname);
        delete_markup_datum(MARK_DURATION_MS, videoPath);
        insert_markup_datum(MARK_DURATION_MS, 0, duration.count(), videoPath);
    }
    else if (IsRecording())
    {
        delete_markup_datum(MARK_DURATION_MS, m_chanId, m_recStartTs);
        insert_markup_datum(MARK_DURATION_MS, 0, duration.count(), m_chanId, m_recStartTs);
    }
}

/// \brief Store the Total Frames at frame 0 in the recordedmarkup table
void ProgramInfo::SaveTotalFrames(int64_t frames)
{
    if (IsVideo())
    {
        auto videoPath = StorageGroup::GetRelativePathname(m_pathname);
        delete_markup_datum(MARK_TOTAL_FRAMES, videoPath);
        insert_markup_datum(MARK_TOTAL_FRAMES, 0, frames, videoPath);
    }
    else if (IsRecording())
    {
        delete_markup_datum(MARK_TOTAL_FRAMES, m_chanId, m_recStartTs);
        insert_markup_datum(MARK_TOTAL_FRAMES, 0, frames, m_chanId, m_recStartTs);
    }
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
    query.bindValue(":CHANID", m_chanId);
    query.bindValue(":STARTTIME", m_recStartTs);
    query.bindValue(":MARK", (quint64)frame);
    query.bindValue(":TYPE", MARK_VIDEO_WIDTH);
    query.bindValue(":DATA", width);

    if (!query.exec())
        MythDB::DBError("Resolution insert", query);

    query.prepare("INSERT INTO recordedmarkup"
                  "    (chanid, starttime, mark, type, data)"
                  "    VALUES"
                  " ( :CHANID, :STARTTIME, :MARK, :TYPE, :DATA);");
    query.bindValue(":CHANID", m_chanId);
    query.bindValue(":STARTTIME", m_recStartTs);
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
        "      recordedmarkup.type      = :TYPE "
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
        "LIMIT 1");

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(qstr);
    query.bindValue(":TYPE", (int)type);
    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", recstartts);

    if (!query.exec())
    {
        MythDB::DBError("load_markup_datum", query);
        return 0;
    }

    return (query.next()) ? query.value(0).toUInt() : 0;
}

static uint load_markup_datum(
    MarkTypes type, const QString &videoPath)
{
    QString qstr = QString(
        "SELECT filemarkup.`offset` "
        "FROM filemarkup "
        "WHERE filemarkup.filename  = :PATH    AND "
        "      filemarkup.type      = :TYPE "
        "GROUP BY filemarkup.`offset` "
        "ORDER BY SUM( ( SELECT IFNULL(fm.mark, filemarkup.mark)"
        "                FROM filemarkup AS fm "
        "                WHERE fm.filename  = filemarkup.filename  AND "
        "                      fm.type      = filemarkup.type      AND "
        "                      fm.mark      > filemarkup.mark "
        "                ORDER BY fm.mark ASC LIMIT 1 "
        "              ) - filemarkup.mark "
        "            ) DESC "
        "LIMIT 1");

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(qstr);
    query.bindValue(":TYPE", (int)type);
    query.bindValue(":PATH", videoPath);

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
    return load_markup_datum(MARK_VIDEO_HEIGHT, m_chanId, m_recStartTs);
}

/** \brief If present in recording this loads average width of the
 *         main video stream from database's stream markup table.
 *  \note Saves loaded value for future reference by GetWidth().
 */
uint ProgramInfo::QueryAverageWidth(void) const
{
    return load_markup_datum(MARK_VIDEO_WIDTH, m_chanId, m_recStartTs);
}

/** \brief If present in recording this loads average frame rate of the
 *         main video stream from database's stream markup table.
 *  \note Saves loaded value for future reference by GetFrameRate().
 */
uint ProgramInfo::QueryAverageFrameRate(void) const
{
    return load_markup_datum(MARK_VIDEO_RATE, m_chanId, m_recStartTs);
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
                "ORDER BY SUM( ( SELECT IFNULL(rm.mark, ( "
                "                  SELECT MAX(rmmax.mark) "
                "                  FROM recordedmarkup AS rmmax "
                "                  WHERE rmmax.chanid    = recordedmarkup.chanid "
                "                    AND rmmax.starttime = recordedmarkup.starttime "
                "                ) ) "
                "                FROM recordedmarkup AS rm "
                "                WHERE rm.chanid    = recordedmarkup.chanid    AND "
                "                      rm.starttime = recordedmarkup.starttime AND "
                "                      rm.type      >= :ASPECTSTART2           AND "
                "                      rm.type      <= :ASPECTEND2             AND "
                "                      rm.mark      > recordedmarkup.mark "
                "                ORDER BY rm.mark ASC LIMIT 1 "
                "              ) - recordedmarkup.mark "
                "            ) DESC "
                "LIMIT 1");
    query.bindValue(":CHANID", m_chanId);
    query.bindValue(":STARTTIME", m_recStartTs);
    query.bindValue(":ASPECTSTART", MARK_ASPECT_4_3); // 11
    query.bindValue(":ASPECTEND", MARK_ASPECT_CUSTOM); // 14
    query.bindValue(":ASPECTSTART2", MARK_ASPECT_4_3); // 11
    query.bindValue(":ASPECTEND2", MARK_ASPECT_CUSTOM); // 14

    if (!query.exec())
    {
        MythDB::DBError("QueryAverageAspectRatio", query);
        return MARK_UNSET;
    }

    if (!query.next())
        return MARK_UNSET;

    return static_cast<MarkTypes>(query.value(0).toInt());
}

/** \brief If present in recording this loads average video scan type of the
 *         main video stream from database's stream markup table.
 *  \note Saves loaded value for future reference by
 *        QueryAverageScanProgressive().
 */
bool ProgramInfo::QueryAverageScanProgressive(void) const
{
    return static_cast<bool>(load_markup_datum(MARK_SCAN_PROGRESSIVE,
                                               m_chanId, m_recStartTs));
}

/** \brief If present this loads the total duration in milliseconds
 *         of the main video stream from recordedmarkup table in the database
 *
 *  \returns Duration in milliseconds
 */
std::chrono::milliseconds ProgramInfo::QueryTotalDuration(void) const
{
    if (gCoreContext->IsDatabaseIgnored())
        return 0ms;

    auto msec = std::chrono::milliseconds(load_markup_datum(MARK_DURATION_MS, m_chanId, m_recStartTs));

// Impossible condition, load_markup_datum returns an unsigned int
//     if (msec < 0ms)
//         return 0ms;

    return msec;
}

/** \brief If present in recording this loads total frames of the
 *         main video stream from database's stream markup table.
 */
int64_t ProgramInfo::QueryTotalFrames(void) const
{
    if (IsVideo())
    {
        int64_t frames = load_markup_datum(MARK_TOTAL_FRAMES, StorageGroup::GetRelativePathname(m_pathname));
        return frames;
    }
    if (IsRecording())
    {
        int64_t frames = load_markup_datum(MARK_TOTAL_FRAMES, m_chanId, m_recStartTs);
        return frames;
    }
    return 0;
}

void ProgramInfo::QueryMarkup(QVector<MarkupEntry> &mapMark,
                              QVector<MarkupEntry> &mapSeek) const
{
    MSqlQuery query(MSqlQuery::InitCon());
    // Get the markup
    if (IsVideo())
    {
        query.prepare("SELECT type, mark, `offset` FROM filemarkup"
                      " WHERE filename = :PATH"
                      " AND type NOT IN (:KEYFRAME,:DURATION)"
                      " ORDER BY mark, type;");
        query.bindValue(":PATH", StorageGroup::GetRelativePathname(m_pathname));
        query.bindValue(":KEYFRAME", MARK_GOP_BYFRAME);
        query.bindValue(":DURATION", MARK_DURATION_MS);
    }
    else if (IsRecording())
    {
        query.prepare("SELECT type, mark, data FROM recordedmarkup"
                      " WHERE chanid = :CHANID"
                      " AND STARTTIME = :STARTTIME"
                      " ORDER BY mark, type");
        query.bindValue(":CHANID", m_chanId);
        query.bindValue(":STARTTIME", m_recStartTs);
    }
    else
    {
        return;
    }
    if (!query.exec())
    {
        MythDB::DBError("QueryMarkup markup data", query);
        return;
    }
    while (query.next())
    {
        int type = query.value(0).toInt();
        uint64_t frame = query.value(1).toLongLong();
        uint64_t data = 0;
        bool isDataNull = query.value(2).isNull();
        if (!isDataNull)
            data = query.value(2).toLongLong();
        mapMark.append(MarkupEntry(type, frame, data, isDataNull));
    }

    // Get the seektable
    if (IsVideo())
    {
        query.prepare("SELECT type, mark, `offset` FROM filemarkup"
                      " WHERE filename = :PATH"
                      " AND type IN (:KEYFRAME,:DURATION)"
                      " ORDER BY mark, type;");
        query.bindValue(":PATH", StorageGroup::GetRelativePathname(m_pathname));
        query.bindValue(":KEYFRAME", MARK_GOP_BYFRAME);
        query.bindValue(":DURATION", MARK_DURATION_MS);
    }
    else if (IsRecording())
    {
        query.prepare("SELECT type, mark, `offset` FROM recordedseek"
                      " WHERE chanid = :CHANID"
                      " AND STARTTIME = :STARTTIME"
                      " ORDER BY mark, type");
        query.bindValue(":CHANID", m_chanId);
        query.bindValue(":STARTTIME", m_recStartTs);
    }
    if (!query.exec())
    {
        MythDB::DBError("QueryMarkup seektable data", query);
        return;
    }
    while (query.next())
    {
        int type = query.value(0).toInt();
        uint64_t frame = query.value(1).toLongLong();
        uint64_t data = 0;
        bool isDataNull = query.value(2).isNull();
        if (!isDataNull)
            data = query.value(2).toLongLong();
        mapSeek.append(MarkupEntry(type, frame, data, isDataNull));
    }
}

void ProgramInfo::SaveMarkup(const QVector<MarkupEntry> &mapMark,
                             const QVector<MarkupEntry> &mapSeek) const
{
    MSqlQuery query(MSqlQuery::InitCon());
    if (IsVideo())
    {
        QString path = StorageGroup::GetRelativePathname(m_pathname);
        if (mapMark.isEmpty())
        {
            LOG(VB_GENERAL, LOG_INFO,
                QString("No mark entries in input, "
                        "not removing marks from DB"));
        }
        else
        {
            query.prepare("DELETE FROM filemarkup"
                          " WHERE filename = :PATH"
                          " AND type NOT IN (:KEYFRAME,:DURATION)");
            query.bindValue(":PATH", path);
            query.bindValue(":KEYFRAME", MARK_GOP_BYFRAME);
            query.bindValue(":DURATION", MARK_DURATION_MS);
            if (!query.exec())
            {
                MythDB::DBError("SaveMarkup seektable data", query);
                return;
            }
            for (const auto& entry : std::as_const(mapMark))
            {
                if (entry.type == MARK_DURATION_MS)
                    continue;
                if (entry.isDataNull)
                {
                    query.prepare("INSERT INTO filemarkup"
                                  " (filename,type,mark)"
                                  " VALUES (:PATH,:TYPE,:MARK)");
                }
                else
                {
                    query.prepare("INSERT INTO filemarkup"
                                  " (filename,type,mark,`offset`)"
                                  " VALUES (:PATH,:TYPE,:MARK,:OFFSET)");
                    query.bindValue(":OFFSET", (quint64)entry.data);
                }
                query.bindValue(":PATH", path);
                query.bindValue(":TYPE", entry.type);
                query.bindValue(":MARK", (quint64)entry.frame);
                if (!query.exec())
                {
                    MythDB::DBError("SaveMarkup seektable data", query);
                    return;
                }
            }
        }
        if (mapSeek.isEmpty())
        {
            LOG(VB_GENERAL, LOG_INFO,
                QString("No seek entries in input, "
                        "not removing marks from DB"));
        }
        else
        {
            query.prepare("DELETE FROM filemarkup"
                          " WHERE filename = :PATH"
                          " AND type IN (:KEYFRAME,:DURATION)");
            query.bindValue(":PATH", path);
            query.bindValue(":KEYFRAME", MARK_GOP_BYFRAME);
            query.bindValue(":DURATION", MARK_DURATION_MS);
            if (!query.exec())
            {
                MythDB::DBError("SaveMarkup seektable data", query);
                return;
            }
            for (int i = 0; i < mapSeek.size(); ++i)
            {
                if (i > 0 && (i % 1000 == 0))
                {
                    LOG(VB_GENERAL, LOG_INFO,
                        QString("Inserted %1 of %2 records")
                        .arg(i).arg(mapSeek.size()));
                }
                const MarkupEntry &entry = mapSeek[i];
                query.prepare("INSERT INTO filemarkup"
                              " (filename,type,mark,`offset`)"
                              " VALUES (:PATH,:TYPE,:MARK,:OFFSET)");
                query.bindValue(":PATH", path);
                query.bindValue(":TYPE", entry.type);
                query.bindValue(":MARK", (quint64)entry.frame);
                query.bindValue(":OFFSET", (quint64)entry.data);
                if (!query.exec())
                {
                    MythDB::DBError("SaveMarkup seektable data", query);
                    return;
                }
            }
        }
    }
    else if (IsRecording())
    {
        if (mapMark.isEmpty())
        {
            LOG(VB_GENERAL, LOG_INFO,
                QString("No mark entries in input, "
                        "not removing marks from DB"));
        }
        else
        {
            query.prepare("DELETE FROM recordedmarkup"
                          " WHERE chanid = :CHANID"
                          " AND starttime = :STARTTIME");
            query.bindValue(":CHANID", m_chanId);
            query.bindValue(":STARTTIME", m_recStartTs);
            if (!query.exec())
            {
                MythDB::DBError("SaveMarkup seektable data", query);
                return;
            }
            for (const auto& entry : std::as_const(mapMark))
            {
                if (entry.isDataNull)
                {
                    query.prepare("INSERT INTO recordedmarkup"
                                  " (chanid,starttime,type,mark)"
                                  " VALUES (:CHANID,:STARTTIME,:TYPE,:MARK)");
                }
                else
                {
                    query.prepare("INSERT INTO recordedmarkup"
                                  " (chanid,starttime,type,mark,data)"
                                  " VALUES (:CHANID,:STARTTIME,"
                                  "         :TYPE,:MARK,:OFFSET)");
                    query.bindValue(":OFFSET", (quint64)entry.data);
                }
                query.bindValue(":CHANID", m_chanId);
                query.bindValue(":STARTTIME", m_recStartTs);
                query.bindValue(":TYPE", entry.type);
                query.bindValue(":MARK", (quint64)entry.frame);
                if (!query.exec())
                {
                    MythDB::DBError("SaveMarkup seektable data", query);
                    return;
                }
            }
        }
        if (mapSeek.isEmpty())
        {
            LOG(VB_GENERAL, LOG_INFO,
                QString("No seek entries in input, "
                        "not removing marks from DB"));
        }
        else
        {
            query.prepare("DELETE FROM recordedseek"
                          " WHERE chanid = :CHANID"
                          " AND starttime = :STARTTIME");
            query.bindValue(":CHANID", m_chanId);
            query.bindValue(":STARTTIME", m_recStartTs);
            if (!query.exec())
            {
                MythDB::DBError("SaveMarkup seektable data", query);
                return;
            }
            for (int i = 0; i < mapSeek.size(); ++i)
            {
                if (i > 0 && (i % 1000 == 0))
                {
                    LOG(VB_GENERAL, LOG_INFO,
                        QString("Inserted %1 of %2 records")
                        .arg(i).arg(mapSeek.size()));
                }
                const MarkupEntry &entry = mapSeek[i];
                query.prepare("INSERT INTO recordedseek"
                              " (chanid,starttime,type,mark,`offset`)"
                              " VALUES (:CHANID,:STARTTIME,"
                              "         :TYPE,:MARK,:OFFSET)");
                query.bindValue(":CHANID", m_chanId);
                query.bindValue(":STARTTIME", m_recStartTs);
                query.bindValue(":TYPE", entry.type);
                query.bindValue(":MARK", (quint64)entry.frame);
                query.bindValue(":OFFSET", (quint64)entry.data);
                if (!query.exec())
                {
                    MythDB::DBError("SaveMarkup seektable data", query);
                    return;
                }
            }
        }
    }
}

void ProgramInfo::SaveVideoProperties(uint mask, uint video_property_flags)
{
    MSqlQuery query(MSqlQuery::InitCon());

    LOG(VB_RECORD, LOG_INFO,
        QString("SaveVideoProperties(0x%1, 0x%2)")
        .arg(mask,2,16,QChar('0')).arg(video_property_flags,2,16,QChar('0')));

    query.prepare(
        "UPDATE recordedprogram "
        "SET videoprop = ((videoprop+0) & :OTHERFLAGS) | :FLAGS "
        "WHERE chanid = :CHANID AND starttime = :STARTTIME");

    query.bindValue(":OTHERFLAGS", ~mask);
    query.bindValue(":FLAGS",      video_property_flags);
    query.bindValue(":CHANID",     m_chanId);
    query.bindValue(":STARTTIME",  m_startTs);
    if (!query.exec())
    {
        MythDB::DBError("SaveVideoProperties", query);
        return;
    }

    uint videoproperties = m_videoProperties;
    videoproperties &= ~mask;
    videoproperties |= video_property_flags;
    m_videoProperties = videoproperties;

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
    chan.replace("<num>", m_chanStr)
        .replace("<sign>", m_chanSign)
        .replace("<name>", m_chanName);
    return chan;
}

void ProgramInfo::UpdateInUseMark(bool force)
{
#ifdef DEBUG_IN_USE
    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("UpdateInUseMark(%1) '%2'")
             .arg(force?"force":"no force").arg(m_inUseForWhat));
#endif

    if (!IsRecording())
        return;

    if (m_inUseForWhat.isEmpty())
        return;

    if (force || MythDate::secsInPast(m_lastInUseTime) > 15min)
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
    query.bindValue(":CHANID",     m_chanId);
    query.bindValue(":STARTTIME",  m_recStartTs);
    query.bindValue(":RECORDID",   m_recordId);
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
    query.bindValue(":CHANID",     m_chanId);
    query.bindValue(":STARTTIME",  m_recStartTs);
    query.bindValue(":RECORDID",   m_recordId);
    query.exec();

    SendUpdateEvent();
}

/** \brief Attempts to ascertain if the main file for this ProgramInfo
 *         is readable.
 *  \note This method often initiates a QUERY_CHECKFILE MythProto
 *        call and so should not be called from the UI thread.
 *  \return true iff file is readable
 */
bool ProgramInfo::IsFileReadable(void)
{
    if (IsLocal() && QFileInfo(m_pathname).isReadable())
        return true;

    if (!IsMythStream())
        m_pathname = GetPlaybackURL(true, false);

    if (IsMythStream())
        return RemoteCheckFile(this);

    if (IsLocal())
        return QFileInfo(m_pathname).isReadable();

    return false;
}

QString ProgramInfo::QueryRecordingGroupPassword(const QString &group)
{
    QString result;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT password FROM recgroups "
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
    query.bindValue(":START", m_recStartTs);
    query.bindValue(":CHANID", m_chanId);

    QString grp = m_recGroup;
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
    query.bindValue(":CHANID", m_chanId);
    query.bindValue(":START",  m_recStartTs);

    if (query.exec() && query.next())
        return query.value(0).toUInt();

    return 0;
}

/**
 *
 *  \note This method sometimes initiates a QUERY_CHECKFILE MythProto
 *        call and so should not be called from the UI thread.
 */
QString ProgramInfo::DiscoverRecordingDirectory(void)
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

    QFileInfo testFile(m_pathname);
    if (testFile.exists() || (gCoreContext->GetHostName() == m_hostname))
    {
        // we may be recording this file and it may not exist yet so we need
        // to do some checking to see what is in pathname
        if (testFile.exists())
        {
            if (testFile.isSymLink())
                testFile.setFile(getSymlinkTarget(m_pathname));

            if (testFile.isFile())
                return testFile.path();
            if (testFile.isDir())
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

/** Tracks a recording's in use status, to prevent deletion and to
 *  allow the storage scheduler to perform IO load balancing.
 *
 *  \note This method sometimes initiates a QUERY_CHECKFILE MythProto
 *        call and so should not be called from the UI thread.
 */
void ProgramInfo::MarkAsInUse(bool inuse, const QString& usedFor)
{
    if (!IsRecording())
        return;

    bool notifyOfChange = false;

    if (inuse &&
        (m_inUseForWhat.isEmpty() ||
         (!usedFor.isEmpty() && usedFor != m_inUseForWhat)))
    {
        if (!usedFor.isEmpty())
        {

#ifdef DEBUG_IN_USE
            if (!m_inUseForWhat.isEmpty())
            {
                LOG(VB_GENERAL, LOG_INFO, LOC +
                    QString("MarkAsInUse(true, '%1'->'%2')")
                        .arg(m_inUseForWhat).arg(usedFor) +
                    " -- use has changed");
            }
            else
            {
                LOG(VB_GENERAL, LOG_INFO, LOC +
                    QString("MarkAsInUse(true, ''->'%1')").arg(usedFor));
            }
#endif // DEBUG_IN_USE

            m_inUseForWhat = usedFor;
        }
        else if (m_inUseForWhat.isEmpty())
        {
            m_inUseForWhat = QString("%1 [%2]")
                .arg(QObject::tr("Unknown")).arg(getpid());
            LOG(VB_GENERAL, LOG_WARNING, LOC +
                QString("MarkAsInUse(true, ''->'%1')").arg(m_inUseForWhat) +
                " -- use was not explicitly set");
        }

        notifyOfChange = true;
    }

    if (!inuse && !m_inUseForWhat.isEmpty() && usedFor != m_inUseForWhat)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("MarkAsInUse(false, '%1'->'%2')")
                .arg(m_inUseForWhat, usedFor) +
            " -- use has changed since first setting as in use.");
    }
#ifdef DEBUG_IN_USE
    else if (!inuse)
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("MarkAsInUse(false, '%1')")
                 .arg(m_inUseForWhat));
    }
#endif // DEBUG_IN_USE

    if (!inuse && m_inUseForWhat.isEmpty())
        m_inUseForWhat = usedFor;

    if (!inuse && m_inUseForWhat.isEmpty())
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
        query.bindValue(":CHANID",    m_chanId);
        query.bindValue(":STARTTIME", m_recStartTs);
        query.bindValue(":HOSTNAME",  gCoreContext->GetHostName());
        query.bindValue(":RECUSAGE",  m_inUseForWhat);

        if (!query.exec())
            MythDB::DBError("MarkAsInUse -- delete", query);

        m_inUseForWhat.clear();
        m_lastInUseTime = MythDate::current(true).addSecs(-kLastInUseOffset);
        SendUpdateEvent();
        return;
    }

    if (m_pathname == GetBasename())
        m_pathname = GetPlaybackURL(false, true);

    QString recDir = DiscoverRecordingDirectory();

    QDateTime inUseTime = MythDate::current(true);

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT count(*) "
        "FROM inuseprograms "
        "WHERE chanid   = :CHANID   AND starttime = :STARTTIME AND "
        "      hostname = :HOSTNAME AND recusage  = :RECUSAGE");
    query.bindValue(":CHANID",    m_chanId);
    query.bindValue(":STARTTIME", m_recStartTs);
    query.bindValue(":HOSTNAME",  gCoreContext->GetHostName());
    query.bindValue(":RECUSAGE",  m_inUseForWhat);

    if (!query.exec())
    {
        MythDB::DBError("MarkAsInUse -- select", query);
    }
    else if (!query.next())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "MarkAsInUse -- select query failed");
    }
    else if (query.value(0).toBool())
    {
        query.prepare(
            "UPDATE inuseprograms "
            "SET lastupdatetime = :UPDATETIME "
            "WHERE chanid   = :CHANID   AND starttime = :STARTTIME AND "
            "      hostname = :HOSTNAME AND recusage  = :RECUSAGE");
        query.bindValue(":CHANID",     m_chanId);
        query.bindValue(":STARTTIME",  m_recStartTs);
        query.bindValue(":HOSTNAME",   gCoreContext->GetHostName());
        query.bindValue(":RECUSAGE",   m_inUseForWhat);
        query.bindValue(":UPDATETIME", inUseTime);

        if (!query.exec())
            MythDB::DBError("MarkAsInUse -- update failed", query);
        else
            m_lastInUseTime = inUseTime;
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
        query.bindValue(":CHANID",     m_chanId);
        query.bindValue(":STARTTIME",  m_recStartTs);
        query.bindValue(":HOSTNAME",   gCoreContext->GetHostName());
        query.bindValue(":RECUSAGE",   m_inUseForWhat);
        query.bindValue(":UPDATETIME", inUseTime);
        query.bindValue(":RECHOST",
                        m_hostname.isEmpty() ? gCoreContext->GetHostName()
                                             : m_hostname);
        query.bindValue(":RECDIR",     recDir);

        if (!query.exec())
            MythDB::DBError("MarkAsInUse -- insert failed", query);
        else
            m_lastInUseTime = inUseTime;
    }

    if (!notifyOfChange)
        return;

    // Let others know we changed status
    QDateTime oneHourAgo = MythDate::current().addSecs(-kLastUpdateOffset);
    query.prepare("SELECT DISTINCT recusage "
                  "FROM inuseprograms "
                  "WHERE lastupdatetime >= :ONEHOURAGO AND "
                  "      chanid          = :CHANID     AND "
                  "      starttime       = :STARTTIME");
    query.bindValue(":CHANID",     m_chanId);
    query.bindValue(":STARTTIME",  m_recStartTs);
    query.bindValue(":ONEHOURAGO", oneHourAgo);
    if (!query.exec())
        return; // not safe to send update event...

    m_programFlags &= ~(FL_INUSEPLAYING | FL_INUSERECORDING | FL_INUSEOTHER);
    while (query.next())
    {
        QString inUseForWhat = query.value(0).toString();
        if (inUseForWhat.contains(kPlayerInUseID))
            m_programFlags |= FL_INUSEPLAYING;
        else if (inUseForWhat == kRecorderInUseID)
            m_programFlags |= FL_INUSERECORDING;
        else
            m_programFlags |= FL_INUSEOTHER;
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

    query.prepare("SELECT channel.channum, capturecard.inputname "
                  "FROM channel, capturecard "
                  "WHERE channel.chanid       = :CHANID            AND "
                  "      capturecard.sourceid = :SOURCEID          AND "
                  "      capturecard.cardid   = :INPUTID");
    query.bindValue(":CHANID",   m_chanId);
    query.bindValue(":SOURCEID", m_sourceId);
    query.bindValue(":INPUTID",  m_inputId);

    if (query.exec() && query.next())
    {
        channum = query.value(0).toString();
        input   = query.value(1).toString();
        return true;
    }
    MythDB::DBError("GetChannel(ProgInfo...)", query);
    return false;
}

static int init_tr(void)
{
    static bool s_done = false;
    static QMutex s_initTrLock;
    QMutexLocker locker(&s_initTrLock);
    if (s_done)
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
                    "Recording Profile Group Name") +
        QObject::tr("V4L2 Encoders",
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

    s_done = true;
    return (rec_profile_names.length() +
            rec_profile_groups.length() +
            display_rec_groups.length() +
            special_program_groups.length() +
            storage_groups.length() +
            play_groups.length());
}

int ProgramInfo::InitStatics(void)
{
    QMutexLocker locker(&s_staticDataLock);
    if (!s_updater)
        s_updater = new ProgramInfoUpdater();
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

    // N.B. Contents of %MATCH% string variables get parsed into a single
    // QStringLists. Quotes in strings will cause lost/truncated output.
    // Replace QUOTATION MARK (U+0022)j with MODIFIER LETTER DOUBLE PRIME (U+02BA).

    str.replace(QString("%FILE%"), GetBasename());
    str.replace(QString("%TITLE%"), m_title.replace("\"", "ʺ"));
    str.replace(QString("%SUBTITLE%"), m_subtitle.replace("\"", "ʺ"));
    str.replace(QString("%SEASON%"), QString::number(m_season));
    str.replace(QString("%EPISODE%"), QString::number(m_episode));
    str.replace(QString("%TOTALEPISODES%"), QString::number(m_totalEpisodes));
    str.replace(QString("%SYNDICATEDEPISODE%"), m_syndicatedEpisode);
    str.replace(QString("%DESCRIPTION%"), m_description.replace("\"", "ʺ"));
    str.replace(QString("%HOSTNAME%"), m_hostname);
    str.replace(QString("%CATEGORY%"), m_category);
    str.replace(QString("%RECGROUP%"), m_recGroup);
    str.replace(QString("%PLAYGROUP%"), m_playGroup);
    str.replace(QString("%CHANID%"), QString::number(m_chanId));
    str.replace(QString("%INETREF%"), m_inetRef);
    str.replace(QString("%PARTNUMBER%"), QString::number(m_partNumber));
    str.replace(QString("%PARTTOTAL%"), QString::number(m_partTotal));
    str.replace(QString("%ORIGINALAIRDATE%"),
                m_originalAirDate.toString(Qt::ISODate));
    static const std::array<const QString,4> s_timeStr
        { "STARTTIME", "ENDTIME", "PROGSTART", "PROGEND", };
    const std::array<const QDateTime *,4> time_dtr
        { &m_recStartTs, &m_recEndTs, &m_startTs, &m_endTs, };
    for (size_t i = 0; i < s_timeStr.size(); i++)
    {
        str.replace(QString("%%1%").arg(s_timeStr[i]),
                    (time_dtr[i]->toLocalTime()).toString("yyyyMMddhhmmss"));
        str.replace(QString("%%1ISO%").arg(s_timeStr[i]),
                    (time_dtr[i]->toLocalTime()).toString(Qt::ISODate));
        str.replace(QString("%%1UTC%").arg(s_timeStr[i]),
                    time_dtr[i]->toString("yyyyMMddhhmmss"));
        str.replace(QString("%%1ISOUTC%").arg(s_timeStr[i]),
                    time_dtr[i]->toString(Qt::ISODate));
    }
    str.replace(QString("%RECORDEDID%"), QString::number(m_recordedId));
}

QMap<QString,uint32_t> ProgramInfo::QueryInUseMap(void)
{
    QMap<QString, uint32_t> inUseMap;
    QDateTime oneHourAgo = MythDate::current().addSecs(-kLastUpdateOffset);

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

// NOTE: This may look ugly, and in some ways it is, but it's designed this
//       way for with two purposes in mind - Consistent behaviour and speed
//       So if you do make changes then carefully test that it doesn't result
//       in any regressions in total speed of execution or adversely affect the
//       results returned for any of it's users.
static bool FromProgramQuery(const QString &sql, const MSqlBindings &bindings,
                             MSqlQuery &query, const uint start,
                             const uint limit, uint &count,
                             ProgGroupBy::Type groupBy)
{
    count = 0;

    QString columns = QString(
        "program.chanid,          program.starttime,       program.endtime,         " //  0-2
        "program.title,           program.subtitle,        program.description,     " //  3-5
        "program.category,        channel.channum,         channel.callsign,        " //  6-8
        "channel.name,            program.previouslyshown, channel.commmethod,      " //  9-11
        "channel.outputfilters,   program.seriesid,        program.programid,       " // 12-14
        "program.airdate,         program.stars,           program.originalairdate, " // 15-17
        "program.category_type,   oldrecstatus.recordid,                            " // 18-19
        "oldrecstatus.rectype,    oldrecstatus.recstatus,                           " // 20-21
        "oldrecstatus.findid,     program.videoprop+0,     program.audioprop+0,     " // 22-24
        "program.subtitletypes+0, program.syndicatedepisodenumber,                  " // 25-26
        "program.partnumber,      program.parttotal,                                " // 27-28
        "program.season,          program.episode,         program.totalepisodes ");  // 29-31

    QString querystr = QString(
	"SELECT %1 FROM ( "
        "    SELECT %2 "
        "    FROM program "
        "    LEFT JOIN channel ON program.chanid = channel.chanid "
        "    LEFT JOIN oldrecorded AS oldrecstatus ON "
        "        oldrecstatus.future = 0 AND "
        "        program.title = oldrecstatus.title AND "
        "        channel.callsign = oldrecstatus.station AND "
        "        program.starttime = oldrecstatus.starttime "
        ) + sql +
	") groupsq ";

    // If a ProgGroupBy option is specified, wrap the query in an outer
    // query using row_number() and select only rows with value 1.  We
    // do this instead of relying on MySQL's liberal support for group
    // by on non-aggregated columns because it is deterministic.
    if (groupBy != ProgGroupBy::None)
    {
        columns +=
            ", row_number() over ( "
            "      partition by ";
        if (groupBy == ProgGroupBy::ChanNum)
            columns += "channel.channum, "
		"       channel.callsign, ";
        else if (groupBy == ProgGroupBy::CallSign)
            columns += "channel.callsign, ";
        else if (groupBy == ProgGroupBy::ProgramId)
            columns += "program.programid, ";
        columns +=
            "           program.title,     "
            "           program.starttime  "
            "      order by channel.recpriority desc, "
	    "               channel.sourceid, "
            "               channel.channum+0 "
	    ") grouprn ";
        querystr += "WHERE grouprn = 1 ";
    }

    // If a limit arg was given then append the LIMIT, otherwise set a hard
    // limit of 20000.
    if (limit > 0)
        querystr += QString("LIMIT %1 ").arg(limit);
    else if (!querystr.contains(" LIMIT "))
        querystr += " LIMIT 20000 "; // For performance reasons we have to have an upper limit

    MSqlBindings::const_iterator it;
    // Some end users need to know the total number of matching records,
    // irrespective of any LIMIT clause
    //
    // SQL_CALC_FOUND_ROWS is better than COUNT(*), as COUNT(*) won't work
    // with any GROUP BY clauses. COUNT is marginally faster but not enough to
    // matter
    //
    // It's considerably faster in my tests to do a separate query which returns
    // no data using SQL_CALC_FOUND_ROWS than it is to use SQL_CALC_FOUND_ROWS
    // with the full query. Unfortunate but true.
    //
    // e.g. Fetching all programs for the next 14 days with a LIMIT of 10 - 220ms
    //      Same query with SQL_CALC_FOUND_ROWS - 1920ms
    //      Same query but only one column and with SQL_CALC_FOUND_ROWS - 370ms
    //      Total to fetch both the count and the data = 590ms vs 1920ms
    //      Therefore two queries is 1.4 seconds faster than one query.
    if (start > 0 || limit > 0)
    {
        QString countStr = querystr
	    .arg("SQL_CALC_FOUND_ROWS chanid", columns);
        query.prepare(countStr);
        for (it = bindings.begin(); it != bindings.end(); ++it)
        {
            if (countStr.contains(it.key()))
                query.bindValue(it.key(), it.value());
        }

        if (!query.exec())
        {
            MythDB::DBError("LoadFromProgramQuery", query);
            return false;
        }

        if (query.exec("SELECT FOUND_ROWS()") && query.next())
            count = query.value(0).toUInt();
    }

    if (start > 0)
        querystr += QString("OFFSET %1 ").arg(start);

    querystr = querystr.arg("*", columns);
    query.prepare(querystr);
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

bool LoadFromProgram(ProgramList &destination, const QString &where,
                     const QString &groupBy, const QString &orderBy,
                     const MSqlBindings &bindings, const ProgramList &schedList)
{
    uint count = 0;

    QString queryStr = "";

    if (!where.isEmpty())
        queryStr.append(QString("WHERE %1 ").arg(where));

    if (!groupBy.isEmpty())
        queryStr.append(QString("GROUP BY %1 ").arg(groupBy));

    if (!orderBy.isEmpty())
        queryStr.append(QString("ORDER BY %1 ").arg(orderBy));

    // ------------------------------------------------------------------------

    return LoadFromProgram(destination, queryStr, bindings, schedList, 0, 0,
                           count);
}

bool LoadFromProgram(ProgramList &destination,
                     const QString &sql, const MSqlBindings &bindings,
                     const ProgramList &schedList, ProgGroupBy::Type groupBy)
{
    uint count = 0;

    QString queryStr = sql;
    // ------------------------------------------------------------------------
    // FIXME: Remove the following. These all make assumptions about the content
    //        of the sql passed in, they can end up breaking that query by
    //        inserting a WHERE clause after a GROUP BY, or a GROUP BY after
    //        an ORDER BY. These should be part of the sql passed in otherwise
    //        the caller isn't getting what they asked for and to fix that they
    //        are forced to include a GROUP BY, ORDER BY or WHERE that they
    //        do not want
    //
    // See the new version of this method above. The plan is to convert existing
    // users of this method and have them supply all of their own data for the
    // queries (no defaults.)

    if (!queryStr.contains("WHERE"))
        queryStr += " WHERE deleted IS NULL AND visible > 0 ";

    if (!queryStr.contains("ORDER BY"))
    {
        queryStr += " ORDER BY program.starttime, ";
        QString chanorder =
            gCoreContext->GetSetting("ChannelOrdering", "channum");
        if (chanorder != "channum")
            queryStr += chanorder + " ";
        else // approximation which the DB can handle
            queryStr += "atsc_major_chan,atsc_minor_chan,channum,callsign ";
    }

    // ------------------------------------------------------------------------

    return LoadFromProgram(destination, queryStr, bindings, schedList, 0, 0,
                           count, groupBy);
}

bool LoadFromProgram( ProgramList &destination,
                      const QString &sql, const MSqlBindings &bindings,
                      const ProgramList &schedList,
                      const uint start, const uint limit, uint &count,
                      ProgGroupBy::Type groupBy)
{
    destination.clear();

    if (sql.contains(" OFFSET", Qt::CaseInsensitive))
    {
        LOG(VB_GENERAL, LOG_WARNING, "LoadFromProgram(): SQL contains OFFSET "
                                     "clause, caller should be updated to use "
                                     "start parameter instead");
    }

    if (sql.contains(" LIMIT", Qt::CaseInsensitive))
    {
        LOG(VB_GENERAL, LOG_WARNING, "LoadFromProgram(): SQL contains LIMIT "
                                     "clause, caller should be updated to use "
                                     "limit parameter instead");
    }

    MSqlQuery query(MSqlQuery::InitCon());
    query.setForwardOnly(true);
    if (!FromProgramQuery(sql, bindings, query, start, limit, count, groupBy))
        return false;

    if (count == 0)
        count = query.size();

    while (query.next())
    {
        destination.push_back(
            new ProgramInfo(
                query.value(3).toString(), // title
                QString(),                 // sortTitle
                query.value(4).toString(), // subtitle
                QString(),                 // sortSubtitle
                query.value(5).toString(), // description
                query.value(26).toString(), // syndicatedepisodenumber
                query.value(6).toString(), // category

                query.value(0).toUInt(), // chanid
                query.value(7).toString(), // channum
                query.value(8).toString(), // chansign
                query.value(9).toString(), // channame
                query.value(12).toString(), // chanplaybackfilters

                MythDate::as_utc(query.value(1).toDateTime()), // startts
                MythDate::as_utc(query.value(2).toDateTime()), // endts
                MythDate::as_utc(query.value(1).toDateTime()), // recstartts
                MythDate::as_utc(query.value(2).toDateTime()), // recendts

                query.value(13).toString(), // seriesid
                query.value(14).toString(), // programid
                string_to_myth_category_type(query.value(18).toString()), // catType

                query.value(16).toFloat(), // stars
                query.value(15).toUInt(), // year
                query.value(27).toUInt(), // partnumber
                query.value(28).toUInt(), // parttotal
                query.value(17).toDate(), // originalAirDate
                RecStatus::Type(query.value(21).toInt()), // recstatus
                query.value(19).toUInt(), // recordid
                RecordingType(query.value(20).toInt()), // rectype
                query.value(22).toUInt(), // findid

                query.value(11).toInt() == COMM_DETECT_COMMFREE, // commfree
                query.value(10).toBool(), // repeat
                query.value(23).toInt(), // videoprop
                query.value(24).toInt(), // audioprop
                query.value(25).toInt(), // subtitletypes
                query.value(29).toUInt(), // season
                query.value(30).toUInt(), // episode
                query.value(31).toUInt(), // totalepisodes

                schedList));
    }

    return true;
}

ProgramInfo* LoadProgramFromProgram(const uint chanid,
                                   const QDateTime& starttime)
{
    ProgramInfo *progInfo = nullptr;

    // Build add'l SQL statement for Program Listing

    MSqlBindings bindings;
    QString      sSQL = "WHERE program.chanid = :ChanId "
                          "AND program.starttime = :StartTime ";

    bindings[":ChanId"   ] = chanid;
    bindings[":StartTime"] = starttime;

    // Get all Pending Scheduled Programs

    ProgramList  schedList;
    bool hasConflicts = false;
    LoadFromScheduler(schedList, hasConflicts);

    // ----------------------------------------------------------------------

    ProgramList progList;

    LoadFromProgram( progList, sSQL, bindings, schedList );

    if (progList.empty())
        return progInfo;

    // progList is an Auto-delete deque, the object will be deleted with the
    // list, so we need to make a copy
    progInfo = new ProgramInfo(*(progList[0]));

    return progInfo;
}

bool LoadFromOldRecorded(
    ProgramList &destination, const QString &sql, const MSqlBindings &bindings)
{
    uint count = 0;
    return LoadFromOldRecorded(destination, sql, bindings, 0, 0, count);
}

bool LoadFromOldRecorded(ProgramList &destination, const QString &sql,
                         const MSqlBindings &bindings,
                         const uint start, const uint limit, uint &count)
{
    destination.clear();

    MSqlQuery query(MSqlQuery::InitCon());
    query.setForwardOnly(true);

    QString columns =
        "oldrecorded.chanid, starttime, endtime, "
        "title, subtitle, description, season, episode, category, seriesid, "
        "programid, inetref, channel.channum, channel.callsign, "
        "channel.name, findid, rectype, recstatus, recordid, duplicate ";

    QString querystr =
        "SELECT %1 "
        "FROM oldrecorded "
        "LEFT JOIN channel ON oldrecorded.chanid = channel.chanid "
        "WHERE oldrecorded.future = 0 "
        + sql;

    bool hasLimit = querystr.contains(" LIMIT ",Qt::CaseInsensitive);

    // make sure the most recent rows are retrieved first in case
    // there are more than the limit to be set below
    if (!hasLimit && !querystr.contains(" ORDER ",Qt::CaseInsensitive))
        querystr += " ORDER BY starttime DESC ";

    // If a limit arg was given then append the LIMIT, otherwise set a hard
    // limit of 20000, which can be overridden by a setting
    if (limit > 0)
        querystr += QString("LIMIT %1 ").arg(limit);
    else if (!hasLimit)
    {
        // For performance reasons we have to have an upper limit
        int nLimit = gCoreContext->GetNumSetting("PrevRecLimit", 20000);
        // For sanity sake at least 100
        nLimit = std::max(nLimit, 100);
        querystr += QString("LIMIT %1 ").arg(nLimit);
    }

    MSqlBindings::const_iterator it;
    // If count is non-zero then also return total number of matching records,
    // irrespective of any LIMIT clause
    //
    // SQL_CALC_FOUND_ROWS is better than COUNT(*), as COUNT(*) won't work
    // with any GROUP BY clauses. COUNT is marginally faster but not enough to
    // matter
    //
    // It's considerably faster in my tests to do a separate query which returns
    // no data using SQL_CALC_FOUND_ROWS than it is to use SQL_CALC_FOUND_ROWS
    // with the full query. Unfortunate but true.
    //
    // e.g. Fetching all programs for the next 14 days with a LIMIT of 10 - 220ms
    //      Same query with SQL_CALC_FOUND_ROWS - 1920ms
    //      Same query but only one column and with SQL_CALC_FOUND_ROWS - 370ms
    //      Total to fetch both the count and the data = 590ms vs 1920ms
    //      Therefore two queries is 1.4 seconds faster than one query.
    if (count > 0 && (start > 0 || limit > 0))
    {
        QString countStr = querystr.arg("SQL_CALC_FOUND_ROWS oldrecorded.chanid");
        query.prepare(countStr);
        for (it = bindings.begin(); it != bindings.end(); ++it)
        {
            if (countStr.contains(it.key()))
                query.bindValue(it.key(), it.value());
        }

        if (!query.exec())
        {
            MythDB::DBError("LoadFromOldRecorded", query);
            return false;
        }

        if (query.exec("SELECT FOUND_ROWS()") && query.next())
            count = query.value(0).toUInt();
    }

    if (start > 0)
        querystr += QString("OFFSET %1 ").arg(start);

    querystr = querystr.arg(columns);
    query.prepare(querystr);
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
            QString(),
            query.value(4).toString(),
            QString(),
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

            RecStatus::Type(query.value(17).toInt()),
            query.value(18).toUInt(),
            RecordingType(query.value(16).toInt()),
            query.value(15).toUInt(),

            query.value(19).toBool()));
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
 *  \param sortBy          comma separated list of fields to sort by
 *  \param ignoreLiveTV    don't return LiveTV recordings
 *  \param ignoreDeleted   don't return deleted recordings
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
    int sort,
    const QString &sortBy,
    bool ignoreLiveTV,
    bool ignoreDeleted)
{
    destination.clear();

    QDateTime   rectime    = MythDate::current().addSecs(
        -gCoreContext->GetNumSetting("RecordOverTime"));

    // ----------------------------------------------------------------------

    QString thequery = ProgramInfo::kFromRecordedQuery;
    if (possiblyInProgressRecordingsOnly || ignoreLiveTV || ignoreDeleted)
    {
        thequery += "WHERE ";
        if (possiblyInProgressRecordingsOnly)
        {
            thequery += "(r.endtime >= NOW() AND r.starttime <= NOW()) ";
        }
        if (ignoreLiveTV)
        {
            thequery += QString("%1 r.recgroup != 'LiveTV' ")
                                .arg(possiblyInProgressRecordingsOnly ? "AND" : "");
        }
        if (ignoreDeleted)
        {
            thequery += QString("%1 r.recgroup != 'Deleted' ")
                            .arg((possiblyInProgressRecordingsOnly || ignoreLiveTV)
                            ? "AND" : "");
        }
    }

    if (sortBy.isEmpty())
    {
        if (sort)
            thequery += "ORDER BY r.starttime ";
        if (sort < 0)
            thequery += "DESC ";
    }
    else
    {
        QStringList sortByFields;
        sortByFields << "starttime" <<  "title" <<  "subtitle" << "season" << "episode" << "category"
                     <<  "watched" << "stars" << "originalairdate" << "recgroup" << "storagegroup"
                     <<  "channum" << "callsign" << "name" << "filesize" << "duration";
        // sanity check the fields are one of the above fields
        QString sSortBy;
        QStringList fields = sortBy.split(",");
        for (const QString& oneField : std::as_const(fields))
        {
            bool ascending = true;
            QString field = oneField.simplified().toLower();

            if (field.endsWith("desc"))
            {
                ascending = false;
                field = field.remove("desc");
            }

            if (field.endsWith("asc"))
            {
                ascending = true;
                field = field.remove("asc");
            }

            field = field.simplified();

            if (field == "channelname")
                field = "name";

            if (sortByFields.contains(field))
            {
                QString table;
                if (field == "channum" || field == "callsign" || field == "name")
                    table = "c.";
                else
                    table = "r.";

                if (field == "channum")
                {
                    // this is to sort numerically rather than alphabetically
                    field = "channum*1000-ifnull(regexp_substr(c.channum,'-.*'),0)";
                }

                else if (field == "duration")
                {
                    field = "timestampdiff(second,r.starttime,r.endtime)";
                    table = "";
                }

                else if (field == "season")
                {
                    field = "if(r.season,r.season,p.season)";
                    table = "";
                }

                else if (field == "episode")
                {
                    field = "if(r.episode,r.episode,p.episode)";
                    table = "";
                }

                if (sSortBy.isEmpty())
                    sSortBy = QString("%1%2 %3").arg(table, field, ascending ? "ASC" : "DESC");
                else
                    sSortBy += QString(",%1%2 %3").arg(table, field, ascending ? "ASC" : "DESC");
            }
            else
            {
                LOG(VB_GENERAL, LOG_WARNING, QString("ProgramInfo::LoadFromRecorded() got an unknown sort field '%1' - ignoring").arg(oneField));
            }
        }

        thequery += "ORDER BY " + sSortBy;
    }

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

        RecStatus::Type recstatus = RecStatus::Recorded;
        QDateTime recstartts = MythDate::as_utc(query.value(24).toDateTime());

        QString key = ProgramInfo::MakeUniqueKey(chanid, recstartts);
        if (MythDate::as_utc(query.value(25).toDateTime()) > rectime &&
            recMap.contains(key))
        {
            recstatus = RecStatus::Recording;
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
        set_flag(flags, FL_LASTPLAYPOS,   query.value(58).toBool());

        if (inUseMap.contains(key))
            flags |= inUseMap[key];

        if (((flags & FL_COMMPROCESSING) != 0U) &&
            (!isJobRunning.contains(key)))
        {
            flags &= ~FL_COMMPROCESSING;
            save_not_commflagged = true;
        }

        set_flag(flags, FL_EDITING,
                 ((flags & FL_REALLYEDITING) != 0U) ||
                 ((flags & COMM_FLAG_PROCESSING) != 0U));

        // User/metadata defined season from recorded
        uint season = query.value(3).toUInt();
        if (season == 0)
            season = query.value(51).toUInt(); // Guide defined season from recordedprogram

        // User/metadata defined episode from recorded
        uint episode = query.value(4).toUInt();
        if (episode == 0)
            episode  = query.value(52).toUInt();  // Guide defined episode from recordedprogram

        // Guide defined total episodes from recordedprogram
        uint totalepisodes = query.value(53).toUInt();

        destination.push_back(
            new ProgramInfo(
                query.value(55).toUInt(),
                query.value(0).toString(),
                QString(),
                query.value(1).toString(),
                QString(),
                query.value(2).toString(),
                season,
                episode,
                totalepisodes,
                query.value(48).toString(), // syndicatedepisode
                query.value(5).toString(), // category

                chanid, channum, chansign, channame, chanfilt,

                query.value(11).toString(), query.value(12).toString(),

                query.value(14).toString(), // pathname

                hostname, query.value(13).toString(),

                query.value(17).toString(), query.value(18).toString(),
                query.value(19).toString(), // inetref
                string_to_myth_category_type(query.value(54).toString()), // category_type

                query.value(16).toInt(),  // recpriority

                query.value(20).toULongLong(),  // filesize

                MythDate::as_utc(query.value(21).toDateTime()), //startts
                MythDate::as_utc(query.value(22).toDateTime()), // endts
                MythDate::as_utc(query.value(24).toDateTime()), // recstartts
                MythDate::as_utc(query.value(25).toDateTime()), // recendts

                query.value(23).toFloat(), // stars

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
                query.value(44).toUInt(), // subtitleType
                query.value(56).toString(), // inputname
                MythDate::as_utc(query.value(57)
                                 .toDateTime()))); // bookmarkupdate

        if (save_not_commflagged)
            destination.back()->SaveCommFlagged(COMM_FLAG_NOT_FLAGGED);
    }

    return true;
}

bool GetNextRecordingList(QDateTime &nextRecordingStart,
                          bool *hasConflicts,
                          std::vector<ProgramInfo> *list)
{
    nextRecordingStart = QDateTime();

    bool dummy = false;
    bool *conflicts = (hasConflicts) ? hasConflicts : &dummy;

    ProgramList progList;
    if (!LoadFromScheduler(progList, *conflicts))
        return false;

    // find the earliest scheduled recording
    for (auto *prog : progList)
    {
        if ((prog->GetRecordingStatus() == RecStatus::WillRecord ||
             prog->GetRecordingStatus() == RecStatus::Pending) &&
            (nextRecordingStart.isNull() ||
             nextRecordingStart > prog->GetRecordingStartTime()))
        {
            nextRecordingStart = prog->GetRecordingStartTime();
        }
    }

    if (!list)
        return true;

    // save the details of the earliest recording(s)
    for (auto & prog : progList)
    {
        if ((prog->GetRecordingStatus()    == RecStatus::WillRecord ||
             prog->GetRecordingStatus()    == RecStatus::Pending) &&
            (prog->GetRecordingStartTime() == nextRecordingStart))
        {
            list->push_back(*prog);
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

// ---------------------------------------------------------------------------
// DEPRECATED CODE FOLLOWS
// ---------------------------------------------------------------------------

void ProgramInfo::SetFilesize(uint64_t sz)
{
    //LOG(VB_GENERAL, LOG_DEBUG, "FIXME: ProgramInfo::SetFilesize() called instead of RecordingInfo::SetFilesize()");
    m_fileSize   = sz;
}


/** \fn ProgramInfo::SaveFilesize(uint64_t)
 *  \brief Sets recording file size in database, and sets "filesize" field.
 */
void ProgramInfo::SaveFilesize(uint64_t fsize)
{
    //LOG(VB_GENERAL, LOG_DEBUG, "FIXME: ProgramInfo::SaveFilesize() called instead of RecordingInfo::SaveFilesize()");
    SetFilesize(fsize);

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "UPDATE recorded "
        "SET filesize = :FILESIZE "
        "WHERE chanid    = :CHANID AND "
        "      starttime = :STARTTIME");
    query.bindValue(":FILESIZE",  (quint64)fsize);
    query.bindValue(":CHANID",    m_chanId);
    query.bindValue(":STARTTIME", m_recStartTs);

    if (!query.exec())
        MythDB::DBError("File size update", query);

    s_updater->insert(m_recordedId, kPIUpdateFileSize, fsize);
}

//uint64_t ProgramInfo::GetFilesize(void) const
//{
    //LOG(VB_GENERAL, LOG_DEBUG, "FIXME: ProgramInfo::GetFilesize() called instead of RecordingInfo::GetFilesize()");
//    return m_fileSize;
//}

// Restore the original query. When a recording finishes, a
// check is made on the filesize and it's 0 at times. If
// less than 1000, commflag isn't queued. See #12290.

uint64_t ProgramInfo::GetFilesize(void) const
{

    if (m_fileSize)
        return m_fileSize;

    // Always query in the 0 case because we can't
    // tell if the filesize was 'lost'. Other than
    // the 0 case, tests showed the DB and RI sizes
    // were equal.

    uint64_t db_filesize = 0;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(
        "SELECT filesize "
        "FROM recorded "
        "WHERE chanid    = :CHANID AND "
        "      starttime = :STARTTIME");
    query.bindValue(":CHANID",    m_chanId);
    query.bindValue(":STARTTIME", m_recStartTs);
    if (query.exec() && query.next())
        db_filesize = query.value(0).toULongLong();

    if (db_filesize)
        LOG(VB_RECORD, LOG_INFO, LOC + QString("RI Filesize=0, DB Filesize=%1")
            .arg(db_filesize));

    return db_filesize;
}

void ProgramInfo::CalculateRecordedProgress()
{
    m_recordedPercent = -1;
    if (m_recStatus != RecStatus::Recording)
        return;

    QDateTime startTime = m_recStartTs;
    QDateTime now = MythDate::current();
    if (now < startTime)
        return;

    QDateTime endTime = m_recEndTs;
    int current = startTime.secsTo(now);
    int duration = startTime.secsTo(endTime);
    if (duration < 1)
        return;

    m_recordedPercent = std::clamp(current * 100 / duration, 0, 100);
    LOG(VB_GUI, LOG_DEBUG, QString("%1 recorded percent %2/%3 = %4%")
        .arg(m_title).arg(current).arg(duration).arg(m_recordedPercent));
}

void ProgramInfo::CalculateWatchedProgress(uint64_t pos)
{
    if (pos == 0)
    {
        m_watchedPercent = -1;
        return;
    }

    uint64_t total = 0;
    if (IsVideo())
    {
        total = std::max((int64_t)0, QueryTotalFrames());
    }
    else if (IsRecording())
    {
        switch (m_recStatus)
        {
        case RecStatus::Recorded:
            total = std::max((int64_t)0, QueryTotalFrames());
            break;
        case RecStatus::Recording:
            {
                // Compute expected total frames based on frame rate.
                int64_t rate1000 = QueryAverageFrameRate();
                int64_t duration = m_recStartTs.secsTo(m_recEndTs);
                total = rate1000 * duration / 1000;
            }
            break;
        default:
            break;
        }
    }

    if (total == 0)
    {
        if (IsVideo())
        {
            LOG(VB_GUI, LOG_DEBUG,
                QString("%1 %2 no frame count. Please rebuild seek table for this video.")
                .arg(m_pathname, m_title));
        }
        else if (IsRecording())
        {
            LOG(VB_GUI, LOG_DEBUG,
                QString("%1 %2 no frame count. Please rebuild seek table for this recording.")
                .arg(m_recordedId).arg(m_title));
        }
        m_watchedPercent = 0;
        return;
    }

    m_watchedPercent = std::clamp(100 * pos / total, (uint64_t)0, (uint64_t)100);
    LOG(VB_GUI, LOG_DEBUG, QString("%1 %2 watched percent %3/%4 = %5%")
        .arg(m_recordedId).arg(m_title)
        .arg(pos).arg(total).arg(m_watchedPercent));
}

void ProgramInfo::CalculateProgress(uint64_t pos)
{
    CalculateRecordedProgress();
    CalculateWatchedProgress(pos);
}

QString ProgGroupBy::toString(ProgGroupBy::Type groupBy)
{
    switch (groupBy)
    {
        case ProgGroupBy::None:
	    return tr("None");
        case ProgGroupBy::ChanNum:
	    return tr("Channel Number");
	case ProgGroupBy::CallSign:
	    return tr("CallSign");
	case ProgGroupBy::ProgramId:
	    return tr("ProgramId");
	default:
	    return tr("Unknown");
    }
}


/* vim: set expandtab tabstop=4 shiftwidth=4: */
