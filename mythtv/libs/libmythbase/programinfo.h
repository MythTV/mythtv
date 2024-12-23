// -*- Mode: c++ -*-

#ifndef MYTHPROGRAM_H_
#define MYTHPROGRAM_H_

// ANSI C
#include <cstdint> // for [u]int[32,64]_t
#include <utility>
#include <vector> // for GetNextRecordingList

#include <QStringList>
#include <QDateTime>

// MythTV headers
#include "libmythbase/autodeletedeque.h"
#include "libmythbase/mythbaseexp.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythdbcon.h"
#include "libmythbase/mythtypes.h"
#include "libmythbase/programtypes.h"
#include "libmythbase/recordingstatus.h"
#include "libmythbase/recordingtypes.h"

/* If NUMPROGRAMLINES gets updated, then MYTH_PROTO_VERSION and MYTH_PROTO_TOKEN
   in mythversion.h need to be bumped, and also follow the instructions in
   myth_version.h about other changes needed when the network protocol changes.
*/
static constexpr int8_t NUMPROGRAMLINES { 52 };

class ProgramInfo;
using ProgramList = AutoDeleteDeque<ProgramInfo*>;

/** \class ProgramInfo
 *  \brief Holds information on recordings and videos.
 *
 *  ProgramInfo can also contain partial information for a program we wish to
 *  find in the schedule, and may also contain information on a video we
 *  wish to view. This class is serializable from frontend to backend and
 *  back and is the basic unit of information on anything we may wish to
 *  view or record.
 *
 *  Any method that begins with "Is", "Get" or "Set" will run quickly
 *  and the results from "Is" or "Get" do not need to be cached.
 *
 *  Any method beginning with "Make" or "Extract" will run quickly, but it
 *  may be prudent to cache the results if they are to be used frequently.
 *
 *  Any method that begins with "Query", "Save" or "Update" will touch
 *  the database and so may take some time to complete.
 *
 *  Any method beginning with another verb needs to be examined to
 *  determine its expected run time.
 *
 *  There is one exception to this set of rules, GetPlaybackURL() is a very
 *  complex beast which not only touches the database but also may query
 *  remote backends about the files visible to them. It blocks until this
 *  task is complete and may need to wake up remote hosts that are currently
 *  powered off to complete it's task. It should not be called from the
 *  UI thread and its result should be cached.
 *
 */

class MSqlQuery;
class ProgramInfoUpdater;
class PMapDBReplacement;

class MBASE_PUBLIC ProgramInfo
{
    friend int pginfo_init_statics(void);
    friend class TestRecordingExtender;
  private:
    // Must match the number of items in CategoryType below
    static const std::array<const QString,5> kCatName;
  public:
    static constexpr int kNumCatTypes = 5;
    enum CategoryType : std::uint8_t
                      { kCategoryNone, kCategoryMovie, kCategorySeries,
                        kCategorySports, kCategoryTVShow };

    /// Null constructor
    ProgramInfo(void) { ensureSortFields(); }
    /// Copy constructor
    ProgramInfo(const ProgramInfo &other);
    /// Constructs a ProgramInfo from data in 'recorded' table
    explicit ProgramInfo(uint recordedid);
    /// Constructs a ProgramInfo from data in 'recorded' table
    ProgramInfo(uint chanid, const QDateTime &recstartts);
    /// Constructs a ProgramInfo from data in 'recorded' table
    ProgramInfo(uint recordedid,
                QString title,
                QString sortTitle,
                QString subtitle,
                QString sortSubtitle,
                QString description,
                uint season,
                uint episode,
                uint totalepisodes,
                QString syndicatedepisode,
                QString category,

                uint chanid,
                QString channum,
                QString chansign,
                QString channame,
                QString chanplaybackfilters,

                QString recgroup,
                QString playgroup,

                const QString &pathname,

                QString hostname,
                QString storagegroup,

                QString seriesid,
                QString programid,
                QString inetref,
                CategoryType  catType,

                int recpriority,

                uint64_t filesize,

                QDateTime startts,
                QDateTime endts,
                QDateTime recstartts,
                QDateTime recendts,

                float stars,

                uint year,
                uint partnumber,
                uint parttotal,

                QDate originalAirDate,
                QDateTime lastmodified,

                RecStatus::Type recstatus,

                uint recordid,

                RecordingDupInType dupin,
                RecordingDupMethodType dupmethod,

                uint findid,

                uint programflags,
                uint audioproperties,
                uint videoproperties,
                uint subtitleType,
                QString inputname,
                QDateTime bookmarkupdate);

    /// Constructs a ProgramInfo from data in 'oldrecorded' table
    ProgramInfo(QString title,
                QString sortTitle,
                QString subtitle,
                QString sortSubtitle,
                QString description,
                uint season,
                uint episode,
                QString category,

                uint chanid,
                QString channum,
                QString chansign,
                QString channame,

                QString seriesid,
                QString programid,
                QString inetref,

                QDateTime startts,
                QDateTime endts,
                QDateTime recstartts,
                QDateTime recendts,

                RecStatus::Type recstatus,

                uint recordid,

                RecordingType rectype,

                uint findid,

                bool duplicate);

    /// Constructs a ProgramInfo from listings data in 'program' table
    ProgramInfo(QString title,
                QString sortTitle,
                QString subtitle,
                QString sortSubtitle,
                QString description,
                QString syndicatedepisode,
                QString category,

                uint chanid,
                QString channum,
                QString chansign,
                QString channame,
                QString chanplaybackfilters,

                QDateTime startts,
                QDateTime endts,
                QDateTime recstartts,
                QDateTime recendts,

                QString seriesid,
                QString programid,
                CategoryType catType,

                float stars,
                uint year,
                uint partnumber,
                uint parttotal,
                QDate originalAirDate,
                RecStatus::Type recstatus,
                uint recordid,
                RecordingType rectype,
                uint findid,

                bool commfree,
                bool repeat,

                uint videoproperties,
                uint audioproperties,
                uint subtitletype,

                uint season,
                uint episode,
                uint totalepisodes,

                const ProgramList &schedList);
    /// Constructs a basic ProgramInfo (used by RecordingInfo)
    ProgramInfo(QString title,
                QString sortTitle,
                QString subtitle,
                QString sortSubtitle,
                QString description,
                uint season,
                uint episode,
                uint totalepisodes,
                QString category,

                uint chanid,
                QString channum,
                QString chansign,
                QString channame,
                QString chanplaybackfilters,

                QString recgroup,
                QString playgroup,

                QDateTime startts,
                QDateTime endts,
                QDateTime recstartts,
                QDateTime recendts,

                QString seriesid,
                QString programid,
                QString inetref,
                QString inputname);
    /// Constructs a ProgramInfo for a pathname.
    explicit ProgramInfo(const QString &pathname);
    /// Constructs a ProgramInfo for a video.
    ProgramInfo(const QString &pathname,
                const QString &plot,
                const QString &title,
                const QString &sortTitle,
                const QString &subtitle,
                const QString &sortSubtitle,
                const QString &director,
                int season, int episode,
                const QString &inetref,
                std::chrono::minutes length_in_minutes,
                uint year,
                const QString &programid);
    /// Constructs a manual record ProgramInfo.
    ProgramInfo(const QString &_title, uint _chanid,
                const QDateTime &_startts, const QDateTime &_endts);
    /// Constructs a Dummy ProgramInfo (used by GuideGrid)
    ProgramInfo(QString _title,   QString _category,
                QDateTime _startts, QDateTime _endts)
        : m_title(std::move(_title)), m_category(std::move(_category)),
          m_startTs(std::move(_startts)), m_endTs(std::move(_endts))
    { ensureSortFields(); }
    ProgramInfo(QStringList::const_iterator &it,
                const QStringList::const_iterator&  end)
    {
        if (!FromStringList(it, end))
            ProgramInfo::clear();
    }
    explicit ProgramInfo(const QStringList &list)
    {
        QStringList::const_iterator it = list.begin();
        if (!FromStringList(it, list.end()))
            ProgramInfo::clear();
    }

    bool operator==(const ProgramInfo& rhs);
    ProgramInfo &operator=(const ProgramInfo &other);
    virtual void clone(const ProgramInfo &other,
                       bool ignore_non_serialized_data = false);
    void ensureSortFields(void);

    virtual void clear(void);

    // Destructor
    virtual ~ProgramInfo() = default;

    // Serializers
    void ToStringList(QStringList &list) const;
    virtual void ToMap(InfoMap &progMap,
                       bool showrerecord = false,
                       uint star_range = 10,
                       uint date_format = 0) const;
    virtual void SubstituteMatches(QString &str);

    // Used for scheduling recordings
    bool IsSameProgram(const ProgramInfo &other) const; // Exact same program
    bool IsDuplicateProgram(const ProgramInfo &other) const; // Is this program considered a duplicate according to rule type and dup method (scheduler only)
    bool IsSameProgramAndStartTime(const ProgramInfo &other) const; // Exact same program and same starttime, Any channel
    bool IsSameTitleStartTimeAndChannel(const ProgramInfo &other) const; // Same title, starttime and channel
    bool IsSameTitleTimeslotAndChannel(const ProgramInfo &other) const;//sched only - Same title, starttime, endtime and channel
    static bool UsingProgramIDAuthority(void)
    {
        return s_usingProgIDAuth;
    };
    static void CheckProgramIDAuthorities(void);//sched only

    // Used for extending scheduled recordings
    bool IsSameProgramWeakCheck(const ProgramInfo &other) const;
    bool IsSameRecording(const ProgramInfo &other) const
        { return m_chanId == other.m_chanId && m_recStartTs == other.m_recStartTs; }
    bool IsSameChannel(const ProgramInfo &other) const;

    // Quick gets
    /// Creates a unique string that can be used to identify an
    /// existing recording.
    QString MakeUniqueKey(void) const
        { return MakeUniqueKey(m_chanId, m_recStartTs); }
    std::chrono::seconds GetSecondsInRecording(void) const;
    QString ChannelText(const QString& format) const;
    QString GetPathname(void) const { return m_pathname; }
    QString GetBasename(void) const { return m_pathname.section('/', -1); }
    bool IsVideoFile(void) const
        { return GetProgramInfoType() == kProgramInfoTypeVideoFile; }
    bool IsVideoDVD(void) const
        { return GetProgramInfoType() == kProgramInfoTypeVideoDVD; }
    bool IsVideoBD(void) const
        { return GetProgramInfoType() == kProgramInfoTypeVideoBD; }
    bool IsLocal(void) const { return m_pathname.startsWith("/")
#ifdef _WIN32
        || m_pathname.at(1) == ':'
#endif
            ; }
    bool IsMythStream(void) const { return m_pathname.startsWith("myth://"); }
    bool IsPathSet(void) const { return GetBasename() != m_pathname; }
    bool HasPathname(void) const { return !GetPathname().isEmpty(); }
    bool IsFileReadable(void);

    QString GetTitle(void)        const { return m_title; }
    QString GetSortTitle(void)    const { return m_sortTitle; }
    QString GetSubtitle(void)     const { return m_subtitle; }
    QString GetSortSubtitle(void) const { return m_sortSubtitle; }
    QString GetDescription(void)  const { return m_description; }
    uint    GetSeason(void)       const { return m_season; }
    uint    GetEpisode(void)      const { return m_episode; }
    uint    GetEpisodeTotal(void) const { return m_totalEpisodes; }
    QString GetCategory(void)     const { return m_category; }
    /// This is the unique key used in the database to locate tuning
    /// information. [1..2^32] are valid keys, 0 is not.
    uint    GetChanID(void)       const { return m_chanId; }
    /// This is the channel "number", in the form 1, 1_2, 1-2, 1#1, etc.
    /// i.e. this is what the user enters to tune to the channel.
    /// This is purely for use in the user interface.
    QString GetChanNum(void)      const { return m_chanStr; }
    /// This is the unique programming identifier of a channel.
    /// For example "BBC1 Crystal Palace". The channel may be broadcast
    /// over satelite, cable and terrestrially -- but will almost always
    /// contain the same programming. This is used when determining if
    /// two channels can be treated as the same channel in recording
    /// rules. In the DB this is called 'callsign' for historic reasons.
    QString GetChannelSchedulingID(void) const { return m_chanSign; }
    /// This is the channel name in the local market, i.e. BBC1.
    /// This is purely for use in the user interface.
    QString GetChannelName(void)  const { return m_chanName; }
    QString GetChannelPlaybackFilters(void) const
        { return m_chanPlaybackFilters; }
    /// The scheduled start time of program.
    QDateTime GetScheduledStartTime(void) const { return m_startTs; }
    /// The scheduled start time of program (with formatting).
    QString GetScheduledStartTime(MythDate::Format fmt) const
    {
        return MythDate::toString(m_startTs, fmt);
    }
    /// The scheduled end time of the program.
    QDateTime GetScheduledEndTime(void)   const { return m_endTs; }
    /// The scheduled end time of the program (with formatting).
    QString GetScheduledEndTime(MythDate::Format fmt) const
    {
        return MythDate::toString(m_endTs, fmt);
    }
    /// Approximate time the recording started.
    QDateTime GetRecordingStartTime(void) const { return m_recStartTs; }
    /// Approximate time the recording started (with formatting).
    QString GetRecordingStartTime(MythDate::Format fmt) const
    {
        return MythDate::toString(m_recStartTs, fmt);
    }
    /// Approximate time the recording should have ended, did end,
    /// or is intended to end.
    QDateTime GetRecordingEndTime(void)   const { return m_recEndTs; }
    /// Approximate time the recording should have ended, did end,
    /// or is intended to end (with formatting).
    QString GetRecordingEndTime(MythDate::Format fmt) const
    {
        return MythDate::toString(m_recEndTs, fmt);
    }
    QString GetRecordingGroup(void)       const { return m_recGroup;     }
    QString GetPlaybackGroup(void)        const { return m_playGroup;    }
    QString GetHostname(void)             const { return m_hostname;     }
    QString GetStorageGroup(void)         const { return m_storageGroup; }
    uint    GetYearOfInitialRelease(void) const
    {
        if (m_year > 0)
            return m_year;
        if (m_originalAirDate.isValid())
            return m_originalAirDate.year();
        return 0;
    }
    QDate   GetOriginalAirDate(void)      const { return m_originalAirDate; }
    QDateTime GetLastModifiedTime(void)   const { return m_lastModified; }
    QString GetLastModifiedTime(MythDate::Format fmt) const
    {
        return MythDate::toString(m_lastModified, fmt);
    }
    virtual uint64_t GetFilesize(void)    const; // TODO Remove
    QString GetSeriesID(void)             const { return m_seriesId;     }
    QString GetProgramID(void)            const { return m_programId;    }
    QString GetInetRef(void)              const { return m_inetRef;      }
    CategoryType GetCategoryType(void)    const { return m_catType;      }
    QString GetCategoryTypeString(void)   const;
    int     GetRecordingPriority(void)    const { return m_recPriority;  }
    int     GetRecordingPriority2(void)   const { return m_recPriority2; }
    float   GetStars(void)                const { return m_stars;        }
    uint    GetStars(uint range_max)      const
        { return lroundf(m_stars * range_max); }

    uint    GetRecordingID(void)              const { return m_recordedId; }
    RecStatus::Type GetRecordingStatus(void)    const
        { return (RecStatus::Type)m_recStatus; }
    uint    GetRecordingRuleID(void)          const { return m_recordId;  }
    uint    GetParentRecordingRuleID(void)    const { return m_parentId;  }
    RecordingType GetRecordingRuleType(void)  const
        { return (RecordingType)m_recType;   }

    /// Where should we check for duplicates?
    RecordingDupInType GetDuplicateCheckSource(void) const
        { return (RecordingDupInType)m_dupIn; }

    /// What should be compared to determine if two programs are the same?
    RecordingDupMethodType GetDuplicateCheckMethod(void) const
        { return (RecordingDupMethodType)m_dupMethod; }

    uint    GetSourceID(void)             const { return m_sourceId;     }
    uint    GetInputID(void)              const { return m_inputId;      }
    QString GetInputName(void)            const { return m_inputName;    }
    QString GetShortInputName(void) const;
    void    ClearInputName(void)          { m_inputName.clear(); }

    uint    GetFindID(void)               const { return m_findId;       }

    uint32_t GetProgramFlags(void)        const { return m_programFlags; }
    QString GetProgramFlagNames(void)     const;
    ProgramInfoType GetProgramInfoType(void) const
        { return (ProgramInfoType)((m_programFlags & FL_TYPEMASK) >> 20); }
    QDateTime GetBookmarkUpdate(void) const { return m_bookmarkUpdate; }
    QString GetRecTypeStatus(bool showrerecord) const;
    bool IsGeneric(void) const;
    bool IsInUsePlaying(void)   const { return (m_programFlags & FL_INUSEPLAYING) != 0U;}
    bool IsCommercialFree(void) const { return (m_programFlags & FL_CHANCOMMFREE) != 0U;}
    bool IsCommercialFlagged(void) const { return (m_programFlags & FL_COMMFLAG) != 0U;}
    bool HasCutlist(void)       const { return (m_programFlags & FL_CUTLIST) != 0U;     }
    bool IsBookmarkSet(void)    const { return (m_programFlags & FL_BOOKMARK) != 0U;    }
    bool IsLastPlaySet(void)    const { return (m_programFlags & FL_LASTPLAYPOS) != 0U; }
    bool IsWatched(void)        const { return (m_programFlags & FL_WATCHED) != 0U;     }
    bool IsAutoExpirable(void)  const { return (m_programFlags & FL_AUTOEXP) != 0U;     }
    bool IsPreserved(void)      const { return (m_programFlags & FL_PRESERVED) != 0U;   }
    bool IsVideo(void)          const { return (m_programFlags & FL_TYPEMASK) != 0U;    }
    bool IsRecording(void)      const { return !IsVideo();                    }
    bool IsRepeat(void)         const { return (m_programFlags & FL_REPEAT) != 0U;      }
    bool IsDuplicate(void)      const { return (m_programFlags & FL_DUPLICATE) != 0U;   }
    bool IsReactivated(void)    const { return (m_programFlags & FL_REACTIVATE) != 0U;  }
    bool IsDeletePending(void)  const
        { return (m_programFlags & FL_DELETEPENDING) != 0U; }

    uint GetSubtitleType(void)    const { return m_subtitleProperties; }
    QString GetSubtitleTypeNames(void) const;
    uint GetVideoProperties(void) const { return m_videoProperties; }
    QString GetVideoPropertyNames(void) const;
    uint GetAudioProperties(void) const { return m_audioProperties; }
    QString GetAudioPropertyNames(void) const;

    static uint SubtitleTypesFromNames(const QString & names);
    static uint VideoPropertiesFromNames(const QString & names);
    static uint AudioPropertiesFromNames(const QString & names);

    void ProgramFlagsFromNames(const QString & names);

    enum Verbosity : std::uint8_t
    {
        kLongDescription,
        kTitleSubtitle,
        kRecordingKey,
        kSchedulingKey,
    };
    QString toString(Verbosity v = kLongDescription, const QString& sep = ":",
                     const QString& grp = "\"") const;

    // Quick sets
    void SetTitle(const QString &t, const QString &st = nullptr);
    void SetSubtitle(const QString &st, const QString &sst = nullptr);
    void SetProgramInfoType(ProgramInfoType t)
        { m_programFlags &= ~FL_TYPEMASK; m_programFlags |= ((uint32_t)t<<20); }
    void SetPathname(const QString& pn);
    void SetChanID(uint _chanid)                  { m_chanId       = _chanid; }
    void SetScheduledStartTime(const QDateTime &dt) { m_startTs      = dt;    }
    void SetScheduledEndTime(  const QDateTime &dt) { m_endTs        = dt;    }
    void SetRecordingStartTime(const QDateTime &dt) { m_recStartTs   = dt;    }
    void SetRecordingEndTime(  const QDateTime &dt) { m_recEndTs     = dt;    }
    void SetRecordingGroup(const QString &group)    { m_recGroup     = group; }
    void SetPlaybackGroup( const QString &group)    { m_playGroup    = group; }
    void SetHostname(      const QString &host)     { m_hostname     = host;  }
    void SetStorageGroup(  const QString &group)    { m_storageGroup = group; }
    virtual void SetFilesize( uint64_t       sz); /// TODO Move to RecordingInfo
    void SetSeriesID(      const QString &id)       { m_seriesId     = id;    }
    void SetProgramID(     const QString &id)       { m_programId    = id;    }
    void SetCategory(      const QString &cat)      { m_category     = cat;   }
    void SetCategoryType(  const CategoryType type) { m_catType      = type;  }
    void SetRecordingPriority(int priority)      { m_recPriority  = priority; }
    void SetRecordingPriority2(int priority)     { m_recPriority2 = priority; }
    void SetRecordingRuleID(uint id)                { m_recordId     = id;    }
    void SetSourceID(uint id)                       { m_sourceId     = id;    }
    void SetInputID(uint id)                        { m_inputId      = id;    }
    void SetReactivated(bool reactivate)
    {
        m_programFlags &= ~FL_REACTIVATE;
        m_programFlags |= (reactivate) ? FL_REACTIVATE : FL_NONE;
    }
    void SetEditing(bool editing)
    {
        m_programFlags &= ~FL_EDITING;
        m_programFlags |= (editing) ? FL_EDITING : FL_NONE;
    }
    void SetFlagging(bool flagging)
    {
        m_programFlags &= ~FL_COMMFLAG;
        m_programFlags |= (flagging) ? FL_COMMFLAG : FL_NONE;
    }
    /// \brief If "ignore" is true GetBookmark() will return 0, otherwise
    ///        GetBookmark() will return the bookmark position if it exists.
    void SetIgnoreBookmark(bool ignore)
    {
        m_programFlags &= ~FL_IGNOREBOOKMARK;
        m_programFlags |= (ignore) ? FL_IGNOREBOOKMARK : FL_NONE;
    }
    /// \brief If "ignore" is true QueryProgStart() will return 0, otherwise
    ///        QueryProgStart() will return the progstart position if it exists.
    void SetIgnoreProgStart(bool ignore)
    {
        m_programFlags &= ~FL_IGNOREPROGSTART;
        m_programFlags |= (ignore) ? FL_IGNOREPROGSTART : FL_NONE;
    }
    /// \brief If "ignore" is true QueryLastPlayPos() will return 0, otherwise
    ///        QueryLastPlayPos() will return the last playback position
    ///        if it exists.
    void SetIgnoreLastPlayPos(bool ignore)
    {
        m_programFlags &= ~FL_IGNORELASTPLAYPOS;
        m_programFlags |= (ignore) ? FL_IGNORELASTPLAYPOS : FL_NONE;
    }
    virtual void SetRecordingID(uint _recordedid)
        { m_recordedId = _recordedid; }
    void SetRecordingStatus(RecStatus::Type status) { m_recStatus = status; }
    void SetRecordingRuleType(RecordingType type)   { m_recType   = type;   }
    void SetPositionMapDBReplacement(PMapDBReplacement *pmap)
        { m_positionMapDBReplacement = pmap; }

    void CalculateRecordedProgress();
    uint GetRecordedPercent() const       { return m_recordedPercent; }
    void CalculateWatchedProgress(uint64_t pos);
    uint GetWatchedPercent() const        { return m_watchedPercent; }
    void SetWatchedPercent(uint progress) { m_watchedPercent = progress; }
    void CalculateProgress(uint64_t pos);

    // Slow DB gets
    QString     QueryBasename(void) const;
//  uint64_t    QueryFilesize(void) const; // TODO Remove
    uint        QueryMplexID(void) const;
    QDateTime   QueryBookmarkTimeStamp(void) const;
    uint64_t    QueryBookmark(void) const;
    uint64_t    QueryProgStart(void) const;
    uint64_t    QueryLastPlayPos(void) const;
    uint64_t    QueryStartMark(void) const;
    CategoryType QueryCategoryType(void) const;
    QStringList QueryDVDBookmark(const QString &serialid) const;
    QStringList QueryBDBookmark(const QString &serialid) const;
    bool        QueryIsEditing(void) const;
    bool        QueryIsInUse(QStringList &byWho) const;
    bool        QueryIsInUse(QString &byWho) const;
    bool        QueryIsDeleteCandidate(bool one_playback_allowed = false) const;
    AutoExpireType QueryAutoExpire(void) const;
    TranscodingStatus QueryTranscodeStatus(void) const;
    bool        QueryTuningInfo(QString &channum, QString &input) const;
    QString     QueryInputDisplayName(void) const;
    uint        QueryAverageWidth(void) const;
    uint        QueryAverageHeight(void) const;
    uint        QueryAverageFrameRate(void) const;
    MarkTypes   QueryAverageAspectRatio(void) const;
    bool        QueryAverageScanProgressive(void) const;
    std::chrono::milliseconds  QueryTotalDuration(void) const;
    int64_t     QueryTotalFrames(void) const;
    QString     QueryRecordingGroup(void) const;
    bool        QueryMarkupFlag(MarkTypes type) const;
    uint        QueryTranscoderID(void) const;
    uint64_t    QueryLastFrameInPosMap(void) const;
    bool        Reload(void);

    // Slow DB sets
    virtual void SaveFilesize(uint64_t fsize); /// TODO Move to RecordingInfo
    void SaveLastPlayPos(uint64_t frame);
    void SaveBookmark(uint64_t frame);
    static void SaveDVDBookmark(const QStringList &fields) ;
    static void SaveBDBookmark(const QStringList &fields) ;
    void SaveEditing(bool edit);
    void SaveTranscodeStatus(TranscodingStatus trans);
    void SaveWatched(bool watchedFlag);
    void SaveDeletePendingFlag(bool deleteFlag);
    void SaveCommFlagged(CommFlagStatus flag); // 1 = flagged, 2 = processing
    void SaveAutoExpire(AutoExpireType autoExpire, bool updateDelete = false);
    void SavePreserve(bool preserveEpisode);
    bool SaveBasename(const QString &basename);
    void SaveAspect(uint64_t frame, MarkTypes type, uint customAspect);
    void SaveResolution(uint64_t frame, uint width, uint height);
    void SaveFrameRate(uint64_t frame, uint framerate);
    void SaveVideoScanType(uint64_t frame, bool progressive);
    void SaveTotalDuration(std::chrono::milliseconds duration);
    void SaveTotalFrames(int64_t frames);
    void SaveVideoProperties(uint mask, uint video_property_flags);
    void SaveMarkupFlag(MarkTypes type) const;
    void ClearMarkupFlag(MarkTypes type) const { ClearMarkupMap(type); }
    void UpdateLastDelete(bool setTime) const;
    void MarkAsInUse(bool inuse, const QString& usedFor = "");
    void UpdateInUseMark(bool force = false);
    void SaveSeasonEpisode(uint seas, uint ep);
    void SaveInetRef(const QString &inet);

    // Extremely slow functions that cannot be called from the UI thread.
    QString DiscoverRecordingDirectory(void);
    QString GetPlaybackURL(bool checkMaster = false,
                           bool forceCheckLocal = false);
    ProgramInfoType DiscoverProgramInfoType(void) const;

    // Edit flagging map
    bool QueryCutList(frm_dir_map_t &delMap, bool loadAutosave=false) const;
    void SaveCutList(frm_dir_map_t &delMap, bool isAutoSave=false) const;

    // Commercial flagging map
    void QueryCommBreakList(frm_dir_map_t &frames) const;
    void SaveCommBreakList(frm_dir_map_t &frames) const;

    // Keyframe positions map
    void QueryPositionMap(frm_pos_map_t &posMap, MarkTypes type) const;
    void ClearPositionMap(MarkTypes type) const;
    void SavePositionMap(frm_pos_map_t &posMap, MarkTypes type,
                         int64_t min_frame = -1, int64_t max_frame = -1) const;
    void SavePositionMapDelta(frm_pos_map_t &posMap, MarkTypes type) const;

    // Get position/duration for keyframe and vice versa
    bool QueryKeyFrameInfo(uint64_t *result, uint64_t position_or_keyframe,
                           bool backwards,
                           MarkTypes type, const char *from_filemarkup_asc,
                           const char *from_filemarkup_desc,
                           const char *from_recordedseek_asc,
                           const char *from_recordedseek_desc) const;
    bool QueryKeyFramePosition(uint64_t *position, uint64_t keyframe,
                               bool backwards) const;
    bool QueryPositionKeyFrame(uint64_t *keyframe, uint64_t position,
                               bool backwards) const;
    bool QueryKeyFrameDuration(uint64_t *duration, uint64_t keyframe,
                               bool backwards) const;
    bool QueryDurationKeyFrame(uint64_t *keyframe, uint64_t duration,
                               bool backwards) const;

    // Get/set all markup
    struct MarkupEntry
    {
        int      type       {   -1 }; // MarkTypes
        uint64_t frame      {    0 };
        uint64_t data       {    0 };
        bool     isDataNull { true };
        MarkupEntry(int t, uint64_t f, uint64_t d, bool n)
            : type(t), frame(f), data(d), isDataNull(n) {}
        MarkupEntry(void) = default;
    };
    void QueryMarkup(QVector<MarkupEntry> &mapMark,
                     QVector<MarkupEntry> &mapSeek) const;
    void SaveMarkup(const QVector<MarkupEntry> &mapMark,
                    const QVector<MarkupEntry> &mapSeek) const;

    /// Sends event out that the ProgramInfo should be reloaded.
    void SendUpdateEvent(void) const;
    /// Sends event out that the ProgramInfo should be added to lists.
    void SendAddedEvent(void) const;
    /// Sends event out that the ProgramInfo should be delete from lists.
    void SendDeletedEvent(void) const;

    /// Translations for play,recording, & storage groups +
    static QString i18n(const QString &msg);

    static QString MakeUniqueKey(uint chanid, const QDateTime &recstartts);
    static bool ExtractKey(const QString &uniquekey,
                           uint &chanid, QDateTime &recstartts);
    static bool ExtractKeyFromPathname(
        const QString &pathname, uint &chanid, QDateTime &recstartts);
    static bool QueryKeyFromPathname(
        const QString &pathname, uint &chanid, QDateTime &recstartts);
    static bool QueryRecordedIdFromPathname(const QString &pathname,
        uint &recordedid);

    static QString  QueryRecordingGroupPassword(const QString &group);
    static uint64_t QueryBookmark(uint chanid, const QDateTime &recstartts);
    static QMap<QString,uint32_t> QueryInUseMap(void);
    static QMap<QString,bool> QueryJobsRunning(int type);
    static QStringList LoadFromScheduler(const QString &tmptable, int recordid);

    // Flagging map support methods
    void UpdateMarkTimeStamp(bool bookmarked) const;
    void UpdateLastPlayTimeStamp(bool lastplay) const;
    void QueryMarkupMap(frm_dir_map_t&marks, MarkTypes type,
                        bool merge = false) const;
    void SaveMarkupMap(const frm_dir_map_t &marks, MarkTypes type = MARK_ALL,
                       int64_t min_frame = -1, int64_t max_frame = -1) const;
    void ClearMarkupMap(MarkTypes type = MARK_ALL,
                        int64_t min_frame = -1, int64_t max_frame = -1) const;

  protected:
    // Creates a basename from the start and end times
    QString CreateRecordBasename(const QString &ext) const;

    bool  LoadProgramFromRecorded(
        uint chanid, const QDateTime &recstartts);

    bool FromStringList(QStringList::const_iterator &it,
                        const QStringList::const_iterator&  end);

    static void QueryMarkupMap(
        const QString &video_pathname,
        frm_dir_map_t &marks, MarkTypes type, bool merge = false);
    static void QueryMarkupMap(
        uint chanid, const QDateTime &recstartts,
        frm_dir_map_t &marks, MarkTypes type, bool merge = false);

    static int InitStatics(void);

  protected:
    QString         m_title;
    QString         m_sortTitle;
    QString         m_subtitle;
    QString         m_sortSubtitle;
    QString         m_description;
    uint            m_season            {0};
    uint            m_episode           {0};
    uint            m_totalEpisodes     {0};
    QString         m_syndicatedEpisode;
    QString         m_category;
    QString         m_director;

    int32_t         m_recPriority       {0};

    uint32_t        m_chanId            {0};
    QString         m_chanStr;  // Channum
    QString         m_chanSign; // Callsign
    QString         m_chanName;
    QString         m_chanPlaybackFilters;

    QString         m_recGroup          {"Default"};
    QString         m_playGroup         {"Default"};

    mutable QString m_pathname;

    QString         m_hostname;
    QString         m_storageGroup      {"Default"};

    QString         m_seriesId;
    QString         m_programId;
    QString         m_inetRef;
    CategoryType    m_catType           {kCategoryNone};

    uint64_t        m_fileSize          {0ULL};

    QDateTime       m_startTs           {MythDate::current(true)};
    QDateTime       m_endTs             {m_startTs};
    QDateTime       m_recStartTs        {m_startTs};
    QDateTime       m_recEndTs          {m_startTs};

    float           m_stars             {0.0F}; ///< Rating, range [0..1]
    QDate           m_originalAirDate;
    QDateTime       m_lastModified      {m_startTs};
    static constexpr int64_t kLastInUseOffset {4LL * 60 * 60};
    QDateTime       m_lastInUseTime     {m_startTs.addSecs(-kLastInUseOffset)};

    int32_t         m_recPriority2      {0};

    uint32_t        m_recordId          {0};
    uint32_t        m_parentId          {0};

    uint32_t        m_sourceId          {0};
    uint32_t        m_inputId           {0};
    uint32_t        m_findId            {0};

    uint32_t        m_programFlags      {FL_NONE}; ///< ProgramFlag
    VideoPropsType    m_videoProperties   {VID_UNKNOWN};
    AudioPropsType    m_audioProperties   {AUD_UNKNOWN};
    SubtitlePropsType m_subtitleProperties {SUB_UNKNOWN};
    uint16_t        m_year              {0};
    uint16_t        m_partNumber        {0};
    uint16_t        m_partTotal         {0};

    int8_t          m_recStatus         {RecStatus::Unknown};
    uint8_t         m_recType           {kNotRecording};
    uint8_t         m_dupIn             {kDupsInAll};
    uint8_t         m_dupMethod         {kDupCheckSubThenDesc};

    uint            m_recordedId        {0};
    QString         m_inputName;
    QDateTime       m_bookmarkUpdate;

// everything below this line is not serialized
    uint8_t         m_availableStatus {asAvailable}; // only used for playbackbox.cpp
    int8_t          m_recordedPercent {-1};          // only used by UI
    int8_t          m_watchedPercent  {-1};          // only used by UI

  public:
    void SetAvailableStatus(AvailableStatusType status, const QString &where);
    AvailableStatusType GetAvailableStatus(void) const
        { return (AvailableStatusType) m_availableStatus; }
    int8_t    m_spread          {-1}; // only used in guidegrid.cpp
    int8_t    m_startCol        {-1}; // only used in guidegrid.cpp

    static const QString kFromRecordedQuery;

  protected:
    QString            m_inUseForWhat;
    PMapDBReplacement *m_positionMapDBReplacement {nullptr};

    static QMutex              s_staticDataLock;
    static ProgramInfoUpdater *s_updater;
    static bool s_usingProgIDAuth;

  public:
    QDateTime       m_previewUpdate;
};

MBASE_PUBLIC bool LoadFromProgram(
    ProgramList        &destination,
    const QString      &where,
    const QString      &groupBy,
    const QString      &orderBy,
    const MSqlBindings &bindings,
    const ProgramList  &schedList);

MBASE_PUBLIC bool LoadFromProgram(
    ProgramList        &destination,
    const QString      &sql,
    const MSqlBindings &bindings,
    const ProgramList  &schedList);

MBASE_PUBLIC bool LoadFromProgram(
    ProgramList        &destination,
    const QString      &sql,
    const MSqlBindings &bindings,
    const ProgramList  &schedList,
    uint               start,
    uint               limit,
    uint               &count);

MBASE_PUBLIC ProgramInfo*  LoadProgramFromProgram(
        uint chanid, const QDateTime &starttime);

MBASE_PUBLIC bool LoadFromOldRecorded(
    ProgramList        &destination,
    const QString      &sql,
    const MSqlBindings &bindings);

MBASE_PUBLIC bool LoadFromOldRecorded(
    ProgramList        &destination,
    const QString      &sql,
    const MSqlBindings &bindings,
    uint               start,
    uint               limit,
    uint               &count);

MBASE_PUBLIC bool LoadFromRecorded(
    ProgramList        &destination,
    bool                possiblyInProgressRecordingsOnly,
    const QMap<QString,uint32_t> &inUseMap,
    const QMap<QString,bool> &isJobRunning,
    const QMap<QString, ProgramInfo*> &recMap,
    int                 sort = 0,
    const QString      &sortBy = "",
    bool                ignoreLiveTV = false,
    bool                ignoreDeleted = false);

template<typename TYPE>
bool LoadFromScheduler(
    AutoDeleteDeque<TYPE*> &destination,
    bool               &hasConflicts,
    const QString&      altTable = "",
    int                 recordid = -1)
{
    destination.clear();
    hasConflicts = false;

    QStringList slist = ProgramInfo::LoadFromScheduler(altTable, recordid);
    if (slist.empty())
        return false;

    hasConflicts = slist[0].toInt();

    QStringList::const_iterator sit = slist.cbegin()+2;
    while (sit != slist.cend())
    {
        TYPE *p = new TYPE(sit, slist.cend());
        destination.push_back(p);

        if (!p->HasPathname() && !p->GetChanID())
        {
            delete p;
            destination.clear();
            return false;
        }
    }

    if (destination.size() != slist[1].toUInt())
    {
        destination.clear();
        return false;
    }

    return true;
}

template<typename T>
bool LoadFromScheduler(AutoDeleteDeque<T> &destination)
{
    bool dummyConflicts = false;
    return LoadFromScheduler(destination, dummyConflicts, "", -1);
}

// Moved from programdetails.cpp/h, used by MythWelcome/MythShutdown
// could be factored out
MBASE_PUBLIC bool GetNextRecordingList(QDateTime &nextRecordingStart,
                                  bool *hasConflicts = nullptr,
                                  std::vector<ProgramInfo> *list = nullptr);

class QMutex;
class MBASE_PUBLIC PMapDBReplacement
{
  public:
    PMapDBReplacement();
   ~PMapDBReplacement();
    QMutex *lock;
    QMap<MarkTypes,frm_pos_map_t> map;
};

MBASE_PUBLIC QString myth_category_type_to_string(ProgramInfo::CategoryType category_type);
MBASE_PUBLIC ProgramInfo::CategoryType string_to_myth_category_type(const QString &type);

Q_DECLARE_METATYPE(ProgramInfo*)
Q_DECLARE_METATYPE(ProgramInfo::CategoryType)

#endif // MYTHPROGRAM_H_

/* vim: set expandtab tabstop=4 shiftwidth=4: */
