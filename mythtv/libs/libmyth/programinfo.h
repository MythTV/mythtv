// -*- Mode: c++ -*-

#ifndef MYTHPROGRAM_H_
#define MYTHPROGRAM_H_

// ANSI C
#include <stdint.h> // for [u]int[32,64]_t
#include <vector> // for GetNextRecordingList

#include <QStringList>
#include <QDateTime>
#include <QHash>

// MythTV headers
#include "autodeletedeque.h"
#include "recordingtypes.h"
#include "programtypes.h"
#include "mythdbcon.h"
#include "mythexp.h"
#include "mythdate.h"

/* If NUMPROGRAMLINES gets updated following files need
   updates and code changes:
   mythweb/modules/tv/classes/Program.php (layout)
   mythtv/bindings/perl/MythTV.pm (version number)
   mythtv/bindings/perl/MythTV/Program.pm (layout)
   mythtv/bindings/php/MythBackend.php (version number)
   mythtv/bindings/php/MythTVProgram.php (layout)
   mythtv/bindings/php/MythTVRecording.php (layout)
   mythtv/bindings/python/MythTV/static.py (version number)
   mythtv/bindings/python/MythTV/mythproto.py (layout)
*/
#define NUMPROGRAMLINES 47

class ProgramInfo;
typedef AutoDeleteDeque<ProgramInfo*> ProgramList;

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

class MPUBLIC ProgramInfo
{
    friend int pginfo_init_statics(void);
  public:
    /// Null constructor
    ProgramInfo(void);
    /// Copy constructor
    ProgramInfo(const ProgramInfo &other);
    /// Constructs a ProgramInfo from data in 'recorded' table
    ProgramInfo(uint chanid, const QDateTime &recstartts);
    /// Constructs a ProgramInfo from data in 'recorded' table
    ProgramInfo(const QString &title,
                const QString &subtitle,
                const QString &description,
                uint season,
                uint episode,
                const QString &syndicatedepisode,
                const QString &category,

                uint chanid,
                const QString &channum,
                const QString &chansign,
                const QString &channame,
                const QString &chanplaybackfilters,

                const QString &recgroup,
                const QString &playgroup,

                const QString &pathname,

                const QString &hostname,
                const QString &storagegroup,

                const QString &seriesid,
                const QString &programid,
                const QString &inetref,

                int recpriority,

                uint64_t filesize,

                const QDateTime &startts,
                const QDateTime &endts,
                const QDateTime &recstartts,
                const QDateTime &recendts,

                float stars,

                uint year,
                uint partnumber,
                uint parttotal,

                const QDate &originalAirDate,
                const QDateTime &lastmodified,

                RecStatusType recstatus,

                uint recordid,

                RecordingDupInType dupin,
                RecordingDupMethodType dupmethod,

                uint findid,

                uint programflags,
                uint audioproperties,
                uint videoproperties,
                uint subtitleType);

    /// Constructs a ProgramInfo from data in 'oldrecorded' table
    ProgramInfo(const QString &title,
                const QString &subtitle,
                const QString &description,
                uint season,
                uint episode,
                const QString &category,

                uint chanid,
                const QString &channum,
                const QString &chansign,
                const QString &channame,

                const QString &seriesid,
                const QString &programid,
                const QString &inetref,

                const QDateTime &startts,
                const QDateTime &endts,
                const QDateTime &recstartts,
                const QDateTime &recendts,

                RecStatusType recstatus,

                uint recordid,

                RecordingType rectype,

                uint findid,

                bool duplicate);

    /// Constructs a ProgramInfo from listings data in 'program' table
    ProgramInfo(const QString &title,
                const QString &subtitle,
                const QString &description,
                const QString &syndicatedepisode,
                const QString &category,

                uint chanid,
                const QString &channum,
                const QString &chansign,
                const QString &channame,
                const QString &chanplaybackfilters,

                const QDateTime &startts,
                const QDateTime &endts,
                const QDateTime &recstartts,
                const QDateTime &recendts,

                const QString &seriesid,
                const QString &programid,
                const QString &catType,

                float stars,
                uint year,
                uint partnumber,
                uint parttotal,
                const QDate &originalAirDate,
                RecStatusType recstatus,
                uint recordid,
                RecordingType rectype,
                uint findid,

                bool commfree,
                bool repeat,

                uint videoprops,
                uint audioprops,
                uint subtitletype,

                const ProgramList &schedList);
    /// Constructs a basic ProgramInfo (used by RecordingInfo)
    ProgramInfo(const QString &title,
                const QString &subtitle,
                const QString &description,
                uint season,
                uint episode,
                const QString &category,

                uint chanid,
                const QString &channum,
                const QString &chansign,
                const QString &channame,
                const QString &chanplaybackfilters,

                const QString &recgroup,
                const QString &playgroup,

                const QDateTime &startts,
                const QDateTime &endts,
                const QDateTime &recstartts,
                const QDateTime &recendts,

                const QString &seriesid,
                const QString &programid,
                const QString &inetref);
    /// Constructs a ProgramInfo for a pathname.
    ProgramInfo(const QString &pathname);
    /// Constructs a ProgramInfo for a video.
    ProgramInfo(const QString &pathname,
                const QString &plot,
                const QString &title,
                const QString &subtitle,
                const QString &director,
                int season, int episode,
                const QString &inetref,
                uint length_in_minutes,
                uint year,
                const QString &programid);
    /// Constructs a manual record ProgramInfo.
    ProgramInfo(const QString &_title, uint _chanid,
                const QDateTime &_startts, const QDateTime &_endts);
    /// Constructs a Dummy ProgramInfo (used by GuideGrid)
    ProgramInfo(const QString   &_title,   const QString   &_category,
                const QDateTime &_startts, const QDateTime &_endts);
    ProgramInfo(QStringList::const_iterator &it,
                QStringList::const_iterator  end) :
        chanid(0),
        positionMapDBReplacement(NULL)
    {
        if (!FromStringList(it, end))
            clear();
    }
    ProgramInfo(const QStringList &list) :
        chanid(0),
        positionMapDBReplacement(NULL)
    {
        QStringList::const_iterator it = list.begin();
        if (!FromStringList(it, list.end()))
            clear();
    }

    ProgramInfo &operator=(const ProgramInfo &other);
    virtual void clone(const ProgramInfo &other,
                       bool ignore_non_serialized_data = false);

    virtual void clear(void);

    // Destructor
    virtual ~ProgramInfo();

    // Serializers
    void ToStringList(QStringList &list) const;
    virtual void ToMap(QHash<QString, QString> &progMap,
                       bool showrerecord = false,
                       uint star_range = 10) const;
    virtual void SubstituteMatches(QString &str);

    // Used for scheduling recordings
    bool IsSameProgram(const ProgramInfo &other) const;
    bool IsSameTimeslot(const ProgramInfo &other) const;
    bool IsSameProgramTimeslot(const ProgramInfo &other) const;//sched only
    static int GetRecordingTypeRecPriority(RecordingType type);//sched only
    static bool UsingProgramIDAuthority(void)
    {
        return usingProgIDAuth;
    };
    static void CheckProgramIDAuthorities(void);//sched only

    // Used for extending scheduled recordings
    bool IsSameProgramWeakCheck(const ProgramInfo &other) const;
    bool IsSameRecording(const ProgramInfo &other) const
        { return chanid == other.chanid && recstartts == other.recstartts; }

    // Quick gets
    /// Creates a unique string that can be used to identify an
    /// existing recording.
    QString MakeUniqueKey(void) const
        { return MakeUniqueKey(chanid, recstartts); }
    uint GetSecondsInRecording(void) const;
    QString ChannelText(const QString&) const;
    QString GetPathname(void) const { return pathname; }
    QString GetBasename(void) const { return pathname.section('/', -1); }
    bool IsVideoFile(void) const
        { return GetProgramInfoType() == kProgramInfoTypeVideoFile; }
    bool IsVideoDVD(void) const
        { return GetProgramInfoType() == kProgramInfoTypeVideoDVD; }
    bool IsVideoBD(void) const
        { return GetProgramInfoType() == kProgramInfoTypeVideoBD; }
    bool IsLocal(void) const { return pathname.left(1) == "/"
#ifdef _WIN32
        || pathname.at(1) == ':'
#endif
            ; }
    bool IsMythStream(void) const { return pathname.left(7) == "myth://"; }
    bool IsPathSet(void) const { return GetBasename() != pathname; }
    bool HasPathname(void) const { return !GetPathname().isEmpty(); }
    bool IsFileReadable(void) const;

    QString GetTitle(void)        const { return title; }
    QString GetSubtitle(void)     const { return subtitle; }
    QString GetDescription(void)  const { return description; }
    uint    GetSeason(void)       const { return season; }
    uint    GetEpisode(void)      const { return episode; }
    QString GetCategory(void)     const { return category; }
    /// This is the unique key used in the database to locate tuning
    /// information. [1..2^32] are valid keys, 0 is not.
    uint    GetChanID(void)       const { return chanid; }
    /// This is the channel "number", in the form 1, 1_2, 1-2, 1#1, etc.
    /// i.e. this is what the user enters to tune to the channel.
    /// This is purely for use in the user interface.
    QString GetChanNum(void)      const { return chanstr; }
    /// This is the unique programming identifier of a channel.
    /// For example "BBC1 Crystal Palace". The channel may be broadcast
    /// over satelite, cable and terrestrially -- but will almost always
    /// contain the same programming. This is used when determining if
    /// two channels can be treated as the same channel in recording
    /// rules. In the DB this is called 'callsign' for historic reasons.
    QString GetChannelSchedulingID(void) const { return chansign; }
    /// This is the channel name in the local market, i.e. BBC1.
    /// This is purely for use in the user interface.
    QString GetChannelName(void)  const { return channame; }
    QString GetChannelPlaybackFilters(void) const
        { return chanplaybackfilters; }
    /// The scheduled start time of program.
    QDateTime GetScheduledStartTime(void) const { return startts; }
    /// The scheduled start time of program (with formatting).
    QString GetScheduledStartTime(MythDate::Format fmt) const
    {
        return MythDate::toString(startts, fmt);
    }
    /// The scheduled end time of the program.
    QDateTime GetScheduledEndTime(void)   const { return endts; }
    /// The scheduled end time of the program (with formatting).
    QString GetScheduledEndTime(MythDate::Format fmt) const
    {
        return MythDate::toString(endts, fmt);
    }
    /// Approximate time the recording started.
    QDateTime GetRecordingStartTime(void) const { return recstartts; }
    /// Approximate time the recording started (with formatting).
    QString GetRecordingStartTime(MythDate::Format fmt) const
    {
        return MythDate::toString(recstartts, fmt);
    }
    /// Approximate time the recording should have ended, did end,
    /// or is intended to end.
    QDateTime GetRecordingEndTime(void)   const { return recendts; }
    /// Approximate time the recording should have ended, did end,
    /// or is intended to end (with formatting).
    QString GetRecordingEndTime(MythDate::Format fmt) const
    {
        return MythDate::toString(recendts, fmt);
    }
    QString GetRecordingGroup(void)       const { return recgroup;     }
    QString GetPlaybackGroup(void)        const { return playgroup;    }
    QString GetHostname(void)             const { return hostname;     }
    QString GetStorageGroup(void)         const { return storagegroup; }
    uint    GetYearOfInitialRelease(void) const
    {
        return ((year) ? year :
                (originalAirDate.isValid()) ? originalAirDate.year() : 0);
    }
    QDate   GetOriginalAirDate(void)      const { return originalAirDate; }
    QDateTime GetLastModifiedTime(void)   const { return lastmodified; }
    QString GetLastModifiedTime(MythDate::Format fmt) const
    {
        return MythDate::toString(lastmodified, fmt);
    }
    uint64_t GetFilesize(void)            const { return filesize;     }
    QString GetSeriesID(void)             const { return seriesid;     }
    QString GetProgramID(void)            const { return programid;    }
    QString GetInetRef(void)              const { return inetref;      }
    QString GetCategoryType(void)         const { return catType;      }
    int     GetRecordingPriority(void)    const { return recpriority;  }
    int     GetRecordingPriority2(void)   const { return recpriority2; }
    float   GetStars(void)                const { return stars;        }
    uint    GetStars(uint range_max)      const
        { return (int)(stars * range_max + 0.5f); }

    RecStatusType GetRecordingStatus(void)    const
        { return (RecStatusType)recstatus; }
    RecStatusType GetOldRecordingStatus(void) const
        { return (RecStatusType)oldrecstatus; }
    uint    GetPreferedInputID(void)          const { return prefinput; }
    uint    GetRecordingRuleID(void)          const { return recordid;  }
    uint    GetParentRecordingRuleID(void)    const { return parentid;  }
    RecordingType GetRecordingRuleType(void)  const
        { return (RecordingType)rectype;   }

    /// Where should we check for duplicates?
    RecordingDupInType GetDuplicateCheckSource(void) const
        { return (RecordingDupInType)dupin; }

    /// What should be compared to determine if two programs are the same?
    RecordingDupMethodType GetDuplicateCheckMethod(void) const
        { return (RecordingDupMethodType)dupmethod; }

    uint    GetSourceID(void)             const { return sourceid;     }
    uint    GetInputID(void)              const { return inputid;      }
    uint    GetCardID(void)               const { return cardid;       }

    uint    GetFindID(void)               const { return findid;       }

    uint32_t GetProgramFlags(void)        const { return programflags; }
    ProgramInfoType GetProgramInfoType(void) const
        { return (ProgramInfoType)((programflags & FL_TYPEMASK) >> 16); }
    bool IsInUsePlaying(void)   const { return programflags & FL_INUSEPLAYING;}
    bool IsCommercialFree(void) const { return programflags & FL_CHANCOMMFREE;}
    bool HasCutlist(void)       const { return programflags & FL_CUTLIST;     }
    bool IsBookmarkSet(void)    const { return programflags & FL_BOOKMARK;    }
    bool IsWatched(void)        const { return programflags & FL_WATCHED;     }
    bool IsAutoExpirable(void)  const { return programflags & FL_AUTOEXP;     }
    bool IsPreserved(void)      const { return programflags & FL_PRESERVED;   }
    bool IsVideo(void)          const { return programflags & FL_TYPEMASK;    }
    bool IsRecording(void)      const { return !IsVideo();                    }
    bool IsRepeat(void)         const { return programflags & FL_REPEAT;      }
    bool IsDuplicate(void)      const { return programflags & FL_DUPLICATE;   }
    bool IsReactivated(void)    const { return programflags & FL_REACTIVATE;  }
    bool IsDeletePending(void)  const
        { return programflags & FL_DELETEPENDING; }

    uint GetSubtitleType(void)    const
        { return (properties&kSubtitlePropertyMask)>>kSubtitlePropertyOffset; }
    uint GetVideoProperties(void) const
        { return (properties & kVideoPropertyMask) >> kVideoPropertyOffset; }
    uint GetAudioProperties(void) const
        { return (properties & kAudioPropertyMask) >> kAudioPropertyOffset; }

    typedef enum
    {
        kLongDescription,
        kTitleSubtitle,
        kRecordingKey,
        kSchedulingKey,
    } Verbosity;
    QString toString(Verbosity v = kLongDescription, QString sep = ":",
                     QString grp = "\"") const;

    // Quick sets
    void SetTitle(const QString &t) { title = t; title.detach(); }
    void SetProgramInfoType(ProgramInfoType t)
        { programflags &= ~FL_TYPEMASK; programflags |= ((uint32_t)t<<16); }
    void SetPathname(const QString&) const;
    void SetChanID(uint _chanid) { chanid = _chanid; }
    void SetScheduledStartTime(const QDateTime &dt) { startts      = dt;    }
    void SetScheduledEndTime(  const QDateTime &dt) { endts        = dt;    }
    void SetRecordingStartTime(const QDateTime &dt) { recstartts   = dt;    }
    void SetRecordingEndTime(  const QDateTime &dt) { recendts     = dt;    }
    void SetRecordingGroup(const QString &group)    { recgroup     = group; }
    void SetPlaybackGroup( const QString &group)    { playgroup    = group; }
    void SetHostname(      const QString &host)     { hostname     = host;  }
    void SetStorageGroup(  const QString &group)    { storagegroup = group; }
    void SetFilesize(      uint64_t       sz)       { filesize     = sz;    }
    void SetSeriesID(      const QString &id)       { seriesid     = id;    }
    void SetProgramID(     const QString &id)       { programid    = id;    }
    void SetCategory(      const QString &cat)      { category     = cat;   }
    void SetCategoryType(  const QString &type)     { catType      = type;  }
    void SetRecordingPriority(int priority)       { recpriority = priority; }
    void SetRecordingPriority2(int priority)     { recpriority2 = priority; }
    void SetRecordingRuleID(uint id)                { recordid     = id;    }
    void SetSourceID(uint id)                       { sourceid     = id;    }
    void SetInputID( uint id)                       { inputid      = id;    }
    void SetCardID(  uint id)                       { cardid       = id;    }
    void SetReactivated(bool reactivate)
    {
        programflags &= ~FL_REACTIVATE;
        programflags |= (reactivate) ? FL_REACTIVATE : 0;
    }
    void SetEditing(bool editing)
    {
        programflags &= ~FL_EDITING;
        programflags |= (editing) ? FL_EDITING : 0;
    }
    void SetFlagging(bool flagging)
    {
        programflags &= ~FL_COMMFLAG;
        programflags |= (flagging) ? FL_COMMFLAG : 0;
    }
    /// \brief If "ignore" is true GetBookmark() will return 0, otherwise
    ///        GetBookmark() will return the bookmark position if it exists.
    void SetIgnoreBookmark(bool ignore)
    {
        programflags &= ~FL_IGNOREBOOKMARK;
        programflags |= (ignore) ? FL_IGNOREBOOKMARK : 0;
    }
    void SetRecordingStatus(RecStatusType status) { recstatus = status; }
    void SetRecordingRuleType(RecordingType type) { rectype   = type;   }
    void SetPositionMapDBReplacement(PMapDBReplacement *pmap)
        { positionMapDBReplacement = pmap; }

    // Slow DB gets
    QString     QueryBasename(void) const;
    uint64_t    QueryFilesize(void) const;
    uint        QueryMplexID(void) const;
    QDateTime   QueryBookmarkTimeStamp(void) const;
    uint64_t    QueryBookmark(void) const;
    QString     QueryCategoryType(void) const;
    QStringList QueryDVDBookmark(const QString &serialid) const;
    bool        QueryIsEditing(void) const;
    bool        QueryIsInUse(QStringList &byWho) const;
    bool        QueryIsInUse(QString &byWho) const;
    bool        QueryIsDeleteCandidate(bool one_player_allowed = false) const;
    AutoExpireType QueryAutoExpire(void) const;
    TranscodingStatus QueryTranscodeStatus(void) const;
    bool        QueryTuningInfo(QString &channum, QString &input) const;
    QString     QueryInputDisplayName(void) const;
    uint        QueryAverageWidth(void) const;
    uint        QueryAverageHeight(void) const;
    uint        QueryAverageFrameRate(void) const;
    int64_t     QueryTotalDuration(void) const;
    int64_t     QueryTotalFrames(void) const;
    QString     QueryRecordingGroup(void) const;
    bool        QueryMarkupFlag(MarkTypes type) const;
    uint        QueryTranscoderID(void) const;
    uint64_t    QueryLastFrameInPosMap(void) const;
    bool        Reload(void);

    // Slow DB sets
    void SaveFilesize(uint64_t fsize);
    void SaveBookmark(uint64_t frame);
    void SaveDVDBookmark(const QStringList &fields) const;
    void SaveEditing(bool edit);
    void SaveTranscodeStatus(TranscodingStatus transFlag);
    void SaveWatched(bool watchedFlag);
    void SaveDeletePendingFlag(bool deleteFlag);
    void SaveCommFlagged(CommFlagStatus flag); // 1 = flagged, 2 = processing
    void SaveAutoExpire(AutoExpireType autoExpire, bool updateDelete = false);
    void SavePreserve(bool preserveEpisode);
    bool SaveBasename(const QString &basename);
    void SaveAspect(uint64_t frame, MarkTypes type, uint customAspect);
    void SaveResolution(uint64_t frame, uint width, uint height);
    void SaveFrameRate(uint64_t frame, uint framerate);
    void SaveTotalDuration(int64_t duration);
    void SaveTotalFrames(int64_t frames);
    void SaveVideoProperties(uint mask, uint video_property_flags);
    void SaveMarkupFlag(MarkTypes type) const;
    void ClearMarkupFlag(MarkTypes type) const { ClearMarkupMap(type); }
    void UpdateLastDelete(bool setTime) const;
    void MarkAsInUse(bool inuse, QString usedFor = "");
    void UpdateInUseMark(bool force = false);
    void SaveSeasonEpisode(uint seas, uint ep);
    void SaveInetRef(const QString &inet);

    // Extremely slow functions that cannot be called from the UI thread.
    QString DiscoverRecordingDirectory(void) const;
    QString GetPlaybackURL(bool checkMaster = false,
                           bool forceCheckLocal = false) const;
    ProgramInfoType DiscoverProgramInfoType(void) const;

    // Edit flagging map
    bool QueryCutList(frm_dir_map_t &, bool loadAutosave=false) const;
    void SaveCutList(frm_dir_map_t &, bool isAutoSave=false) const;

    // Commercial flagging map
    void QueryCommBreakList(frm_dir_map_t &) const;
    void SaveCommBreakList(frm_dir_map_t &) const;

    // Keyframe positions map
    void QueryPositionMap(frm_pos_map_t &, MarkTypes type) const;
    void ClearPositionMap(MarkTypes type) const;
    void SavePositionMap(frm_pos_map_t &, MarkTypes type,
                         int64_t min_frm = -1, int64_t max_frm = -1) const;
    void SavePositionMapDelta(frm_pos_map_t &, MarkTypes type) const;

    /// Sends event out that the ProgramInfo should be reloaded.
    void SendUpdateEvent(void);
    /// Sends event out that the ProgramInfo should be added to lists.
    void SendAddedEvent(void) const;
    /// Sends event out that the ProgramInfo should be delete from lists.
    void SendDeletedEvent(void) const;

    /// Translations for play,recording, & storage groups +
    static QString i18n(const QString&);

    static QString MakeUniqueKey(uint chanid, const QDateTime &recstartts);
    static bool ExtractKey(const QString &uniquekey,
                           uint &chanid, QDateTime &recstartts);
    static bool ExtractKeyFromPathname(
        const QString &pathname, uint &chanid, QDateTime &recstartts);
    static bool QueryKeyFromPathname(
        const QString &pathname, uint &chanid, QDateTime &recstartts);

    static QString  QueryRecordingGroupPassword(const QString &group);
    static uint64_t QueryBookmark(uint chanid, const QDateTime &recstartts);
    static QMap<QString,uint32_t> QueryInUseMap(void);
    static QMap<QString,bool> QueryJobsRunning(int type);
    static QStringList LoadFromScheduler(const QString &altTable, int recordid);

  protected:
    // Flagging map support methods
    void QueryMarkupMap(frm_dir_map_t&, MarkTypes type,
                        bool merge = false) const;
    void SaveMarkupMap(const frm_dir_map_t &, MarkTypes type = MARK_ALL,
                       int64_t min_frm = -1, int64_t max_frm = -1) const;
    void ClearMarkupMap(MarkTypes type = MARK_ALL,
                        int64_t min_frm = -1, int64_t max_frm = -1) const;

    // Creates a basename from the start and end times
    QString CreateRecordBasename(const QString &ext) const;

    bool  LoadProgramFromRecorded(
        const uint chanid, const QDateTime &recstarttime);

    bool FromStringList(QStringList::const_iterator &it,
                        QStringList::const_iterator  end);

    static void QueryMarkupMap(
        const QString &video_pathname,
        frm_dir_map_t&, MarkTypes type, bool merge = false);
    static void QueryMarkupMap(
        uint chanid, const QDateTime &recstartts,
        frm_dir_map_t&, MarkTypes type, bool merge = false);

    static int InitStatics(void);

  protected:
    QString title;
    QString subtitle;
    QString description;
    uint    season;
    uint    episode;
    QString syndicatedepisode;
    QString category;

    int32_t recpriority;

    uint32_t chanid;
    QString chanstr;
    QString chansign;
    QString channame;
    QString chanplaybackfilters;

    QString recgroup;
    QString playgroup;

    mutable QString pathname;

    QString hostname;
    QString storagegroup;

    QString seriesid;
    QString programid;
    QString inetref;
    QString catType;

    uint64_t filesize;

    QDateTime startts;
    QDateTime endts;
    QDateTime recstartts;
    QDateTime recendts;

    float stars; ///< Rating, range [0..1]
    QDate originalAirDate;
    QDateTime lastmodified;
    QDateTime lastInUseTime;

    uint32_t prefinput;
    int32_t recpriority2;

    uint32_t recordid;
    uint32_t parentid;

    uint32_t sourceid;
    uint32_t inputid;
    uint32_t cardid;
    uint32_t findid;

    uint32_t programflags;    ///< ProgramFlag
    uint16_t properties;      ///< SubtitleType,VideoProperty,AudioProperty
    uint16_t year;
    uint16_t partnumber;
    uint16_t parttotal;

    int8_t recstatus;
    int8_t oldrecstatus;
    uint8_t rectype;
    uint8_t dupin;
    uint8_t dupmethod;

// everything below this line is not serialized
    uint8_t availableStatus; // only used for playbackbox.cpp
  public:
    void SetAvailableStatus(AvailableStatusType status, const QString &where);
    AvailableStatusType GetAvailableStatus(void) const
        { return (AvailableStatusType) availableStatus; }
    int8_t spread;   // only used in guidegrid.cpp
    int8_t startCol; // only used in guidegrid.cpp
    QString sortTitle; // only use for sorting in frontend

    static const QString kFromRecordedQuery;

  protected:
    QString inUseForWhat;
    PMapDBReplacement *positionMapDBReplacement;

    static QMutex staticDataLock;
    static ProgramInfoUpdater *updater;
    static bool usingProgIDAuth;
};


MPUBLIC bool LoadFromProgram(
    ProgramList        &destination,
    const QString      &sql,
    const MSqlBindings &bindings,
    const ProgramList  &schedList);

MPUBLIC bool LoadFromOldRecorded(
    ProgramList        &destination,
    const QString      &sql,
    const MSqlBindings &bindings);

MPUBLIC bool LoadFromRecorded(
    ProgramList        &destination,
    bool                possiblyInProgressRecordingsOnly,
    const QMap<QString,uint32_t> &inUseMap,
    const QMap<QString,bool> &isJobRunning,
    const QMap<QString, ProgramInfo*> &recMap,
    int                 sort = 0);

template<typename TYPE>
bool LoadFromScheduler(
    AutoDeleteDeque<TYPE*> &destination,
    bool               &hasConflicts,
    QString             altTable = "",
    int                 recordid = -1)
{
    destination.clear();
    hasConflicts = false;

    QStringList slist = ProgramInfo::LoadFromScheduler(altTable, recordid);
    if (slist.empty())
        return false;

    hasConflicts = slist[0].toInt();

    QStringList::const_iterator sit = slist.begin()+2;
    while (sit != slist.end())
    {
        TYPE *p = new TYPE(sit, slist.end());
        destination.push_back(p);
        if (!p->HasPathname() && !p->GetChanID())
        {
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
    bool dummyConflicts;
    return LoadFromScheduler(destination, dummyConflicts, "", -1);
}

// Moved from programdetails.cpp/h, used by MythWelcome/MythShutdown
// could be factored out
MPUBLIC bool GetNextRecordingList(QDateTime &nextRecordingStart,
                                  bool *hasConflicts = NULL,
                                  vector<ProgramInfo> *list = NULL);

class QMutex;
class MPUBLIC PMapDBReplacement
{
  public:
    PMapDBReplacement();
   ~PMapDBReplacement();
    QMutex *lock;
    QMap<MarkTypes,frm_pos_map_t> map;
};

Q_DECLARE_METATYPE(ProgramInfo*)

#endif // MYTHPROGRAM_H_

/* vim: set expandtab tabstop=4 shiftwidth=4: */
