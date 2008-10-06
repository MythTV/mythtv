#ifndef PROGRAMINFO_H_
#define PROGRAMINFO_H_

#include "recordingtypes.h"
#include "mythdbcon.h"

#include <qstring.h>
#include <qdatetime.h>
#include <qmap.h>
#include <QStringList>

#include <qregexp.h>

#include <vector>
#include <list>
using namespace std;

typedef QMap<long long, long long> frm_pos_map_t;
typedef QMap<long long, int> frm_dir_map_t;

/* If NUMPROGRAMLINES gets updated following files need
   updates and code changes:
   mythplugins/mythweb/includes/mythbackend.php
   mythplugins/mythweb/objects/MythTV.php
   mythtv/bindings/perl/MythTV.pm
   mythtv/bindings/perl/MythTV/Program.pm
   mythtv/bindings/python/MythTV/MythTV.py
*/
#define NUMPROGRAMLINES 47

typedef enum {
    MARK_UNSET = -10,
    MARK_UPDATED_CUT = -3,
    MARK_EDIT_MODE = -2,
    MARK_CUT_END = 0,
    MARK_CUT_START = 1,
    MARK_BOOKMARK = 2,
    MARK_BLANK_FRAME = 3,
    MARK_COMM_START = 4,
    MARK_COMM_END = 5,
    MARK_GOP_START = 6,
    MARK_KEYFRAME = 7,
    MARK_SCENE_CHANGE = 8,
    MARK_GOP_BYFRAME = 9,
    MARK_ASPECT_1_1 = 10,
    MARK_ASPECT_4_3 = 11,
    MARK_ASPECT_16_9 = 12,
    MARK_ASPECT_21_1_1 = 13
} MarkTypes;
MPUBLIC QString toString(MarkTypes type);

enum CommFlagStatuses {
    COMM_FLAG_NOT_FLAGGED = 0,
    COMM_FLAG_DONE = 1,
    COMM_FLAG_PROCESSING = 2,
    COMM_FLAG_COMMFREE = 3
};

enum TranscodingStatuses {
    TRANSCODING_NOT_TRANSCODED = 0,
    TRANSCODING_COMPLETE       = 1,
    TRANSCODING_RUNNING        = 2
};

enum FlagMask {
    FL_COMMFLAG       = 0x0001,
    FL_CUTLIST        = 0x0002,
    FL_AUTOEXP        = 0x0004,
    FL_EDITING        = 0x0008,
    FL_BOOKMARK       = 0x0010,
    FL_INUSERECORDING = 0x0020,
    FL_INUSEPLAYING   = 0x0040,
    FL_TRANSCODED     = 0x0400,
    FL_WATCHED        = 0x0800,
    FL_PRESERVED      = 0x1000,
};

// if AudioProps changes, the audioprop column in program and
// recordedprogram has to changed accordingly
enum AudioProps {
    AUD_UNKNOWN       = 0x00, // For backwards compatibility do not change 0 or 1
    AUD_STEREO        = 0x01,
    AUD_MONO          = 0x02,
    AUD_SURROUND      = 0x04,
    AUD_DOLBY         = 0x08,
    AUD_HARDHEAR      = 0x10,
    AUD_VISUALIMPAIR  = 0x20,
};

// if VideoProps changes, the audioprop column in program and
// recordedprogram has to changed accordingly
enum VideoProps {
    VID_UNKNOWN       = 0x00, // For backwards compatibility do not change 0 or 1
    VID_HDTV          = 0x01,
    VID_WIDESCREEN    = 0x02,
    VID_AVC           = 0x04,
};

// if SubtitleTypes changes, the audioprop column in program and
// recordedprogram has to changed accordingly
enum SubtitleTypes {
    SUB_UNKNOWN       = 0x00, // For backwards compatibility do not change 0 or 1
    SUB_HARDHEAR      = 0x01,
    SUB_NORMAL        = 0x02,
    SUB_ONSCREEN      = 0x04,
    SUB_SIGNED        = 0x08
};

enum RecStatusType {
    rsFailed = -9,
    rsTunerBusy = -8,
    rsLowDiskSpace = -7,
    rsCancelled = -6,
    rsMissed = -5,
    rsAborted = -4,
    rsRecorded = -3,
    rsRecording = -2,
    rsWillRecord = -1,
    rsUnknown = 0,
    rsDontRecord = 1,
    rsPreviousRecording = 2,
    rsCurrentRecording = 3,
    rsEarlierShowing = 4,
    rsTooManyRecordings = 5,
    rsNotListed = 6,
    rsConflict = 7,
    rsLaterShowing = 8,
    rsRepeat = 9,
    rsInactive = 10,
    rsNeverRecord = 11,
    rsOffLine = 12,
    rsOtherShowing = 13
};

enum AvailableStatusType {
    asAvailable = 0,
    asNotYetAvailable,
    asPendingDelete,
    asFileNotFound,
    asZeroByte,
    asDeleted
};

enum WatchListStatus {
    wlDeleted = -4,
    wlEarlier = -3,
    wlWatched = -2,
    wlExpireOff = -1
};

enum AutoExpireType {
    kDisableAutoExpire = 0,
    kNormalAutoExpire  = 1,
    kDeletedAutoExpire = 9999,
    kLiveTVAutoExpire = 10000
};

class PMapDBReplacement
{
  public:
    PMapDBReplacement() : lock(new QMutex()) { }
   ~PMapDBReplacement() { delete lock; }
    QMutex       *lock;
    QMap<MarkTypes,frm_pos_map_t> map;
};

class ScheduledRecording;

class MPUBLIC ProgramInfo
{
  public:
    // Constructors and bulk set methods.
    ProgramInfo(void);
    ProgramInfo(const ProgramInfo &other);
    static ProgramInfo *GetProgramAtDateTime(const QString &channel,
                                             const QDateTime &dtime,
                                             bool genUnknown = false,
                                             int clampHoursMax = 0);
    static ProgramInfo *GetProgramFromBasename(const QString filename);
    static ProgramInfo *GetProgramFromRecorded(const QString &channel,
                                               const QString &starttime);
    static ProgramInfo *GetProgramFromRecorded(const QString &channel,
                                               const QDateTime &dtime);

    ProgramInfo& operator=(const ProgramInfo &other);
    ProgramInfo& clone(const ProgramInfo &other);
    bool FromStringList(QStringList::const_iterator &it,
                        QStringList::const_iterator  end);
    bool FromStringList(const QStringList &list, uint offset);

    bool FillInRecordInfo(const vector<ProgramInfo *> &reclist);

    // Destructor
    ~ProgramInfo();

    // Serializers
    void Save() const;
    void ToStringList(QStringList &list) const;
    void ToMap(QMap<QString, QString> &progMap,
               bool showrerecord = false) const;

    // Used for scheduling recordings
    int IsProgramRecurring(void) const;
    bool IsSameProgram(const ProgramInfo& other) const;
    bool IsSameTimeslot(const ProgramInfo& other) const;
    bool IsSameProgramTimeslot(const ProgramInfo& other) const;
    static int GetChannelRecPriority(const QString &channel);
    static int GetRecordingTypeRecPriority(RecordingType type);

    // Used to query and set ScheduledRecording info
    ScheduledRecording* GetScheduledRecording(void);
    int getRecordID(void);
    int GetAutoRunJobs(void) const;
    RecordingType GetProgramRecordingStatus(void);
    QString GetProgramRecordingProfile(void);
    void ApplyRecordStateChange(RecordingType newstate);
    void ApplyRecordRecPriorityChange(int);
    void ToggleRecord(void);
    void ReactivateRecording(void);
    void AddHistory(bool resched = true, bool forcedup = false);
    void DeleteHistory(void);
    void ForgetHistory(void);
    void SetDupHistory(void);

    // Used to update database with recording info
    void StartedRecording(QString ext);
    void FinishedRecording(bool prematurestop);
    void UpdateRecordingEnd(void);
    void ApplyRecordRecID(void);
    void ApplyRecordRecGroupChange(const QString &newrecgroup);
    void ApplyRecordPlayGroupChange(const QString &newrecgroup);
    void ApplyRecordRecTitleChange(const QString &newTitle,
                                   const QString &newSubtitle);
    void ApplyTranscoderProfileChange(QString);

    // Quick gets
    bool SetRecordBasename(QString basename);
    QString GetRecordBasename(bool fromDB = false) const;
    QString GetPlaybackURL(bool checkMaster = false,
                           bool forceCheckLocal = false);
    QString MakeUniqueKey(void) const;
    int CalculateLength(void) const;
    int SecsTillStart() const;
    QString ChannelText(const QString&) const;
    QString RecTypeChar(void) const;
    QString RecTypeText(void) const;
    QString RecStatusChar(void) const;
    QString RecStatusText(void) const;
    QString RecStatusDesc(void) const;
    void UpdateInUseMark(bool force = false);
    bool PathnameExists(void);

    // Quick sets
    /// \brief If "ignore" is true GetBookmark() will return 0, otherwise
    ///        GetBookmark() will return the bookmark position if it exists.
    void setIgnoreBookmark(bool ignore) { ignoreBookmark = ignore; }

    // DB gets
    long long GetFilesize(void);
    int GetMplexID(void) const;
    long long GetBookmark(void) const;
    QStringList GetDVDBookmark(QString serialid, bool delbookmark) const;
    bool IsEditing(void) const;
    bool IsCommFlagged(void) const;
    bool IsInUse(QString &byWho) const;
    int GetAutoExpireFromRecorded(void) const;
    int GetTranscodedStatus(void) const;
    bool GetPreserveEpisodeFromRecorded(void) const;
    bool UsesMaxEpisodes(void) const;
    int getProgramFlags(void) const;
    void getProgramProperties(void);
    bool GetChannel(QString &channum, QString &input) const;
    QString GetFileName(void) const { return pathname; }
    QString toString(void) const;

    // DB sets
    void SetFilesize(long long fsize);
    void SetBookmark(long long pos) const;
    void SetDVDBookmark(QStringList fields) const;
    void SetEditing(bool edit) const;
    void SetTranscoded(int transFlag) const;
    void SetWatchedFlag(bool watchedFlag) const;
    void SetDeleteFlag(bool deleteFlag) const;
    void SetCommFlagged(int flag) const; // 1 = flagged, 2 = processing
    void SetAutoExpire(int autoExpire, bool updateDelete = false) const;
    void SetPreserveEpisode(bool preserveEpisode) const;
    void UpdateLastDelete(bool setTime) const;

    // Commercial/Edit flagging maps
    void GetCutList(frm_dir_map_t &) const;
    void GetCommBreakList(frm_dir_map_t &) const;

    void SetCutList(frm_dir_map_t &) const;
    void SetCommBreakList(frm_dir_map_t &) const;

    // Flagging map support methods
    bool CheckMarkupFlag(int type) const;
    void GetMarkupMap(frm_dir_map_t&, int type, bool merge = false) const;
    void SetMarkupFlag(int type, bool processing) const;
    void SetMarkupMap(frm_dir_map_t &, int type = -100,
                      long long min_frm = -1, long long max_frm = -1) const;
    void ClearMarkupMap(int type = -100,
                        long long min_frm = -1, long long max_frm = -1) const;

    // Keyframe positions Map
    void GetPositionMap(frm_pos_map_t &, int type) const;
    void ClearPositionMap(int type) const;
    void SetPositionMap(frm_pos_map_t &, int type,
                        long long min_frm = -1, long long max_frm = -1) const;
    void SetPositionMapDelta(frm_pos_map_t &, int type) const;
    void SetPositionMapDBReplacement(PMapDBReplacement *pmap)
        { positionMapDBReplacement = pmap; }

    // Aspect Ratio map
    void SetAspectChange(MarkTypes type, long long frame);

    // GUI stuff
    void showDetails(void) const;
    void EditRecording(void);
    void EditScheduled(void);

    // In-use, autodeletion prevention stuff
    void MarkAsInUse(bool inuse, QString usedFor = "");

    // Rec Group
    static QString GetRecGroupPassword(QString group);
    void   UpdateRecGroup(void);

  private:
    // GUI helper functions
    bool IsFindApplicable(void) const;
    void ShowRecordingDialog(void);
    void ShowNotRecordingDialog(void);

    // Creates a basename from the start and end times
    QString CreateRecordBasename(const QString &ext) const;

  public:
    // data
    QString title;
    QString subtitle;
    QString description;
    QString category;

    QString chanid;
    QString chanstr;
    QString chansign;
    QString channame;


    int recpriority;
    QString recgroup;
    QString playgroup;
    int chancommfree;

    QString pathname;
    long long filesize;
    QString hostname;
    QString storagegroup;

    QDateTime startts;
    QDateTime endts;
    QDateTime recstartts;
    QDateTime recendts;

    AvailableStatusType availableStatus;

    bool isVideo;
    int lenMins;

    QString year;
    float stars;

    QDate originalAirDate;
    QDateTime lastmodified;
    QDateTime lastInUseTime;

    bool hasAirDate;
    bool repeat;

    int spread;
    int startCol;

    RecStatusType recstatus;
    RecStatusType oldrecstatus;
    RecStatusType savedrecstatus;
    int prefinput;
    int recpriority2;
    int reactivate;

    int recordid;
    int parentid;
    RecordingType rectype;
    RecordingDupInType dupin;
    RecordingDupMethodType dupmethod;

    int sourceid;
    int inputid;
    int cardid;
    bool shareable;
    bool duplicate;

    QString schedulerid;
    int findid;

    int programflags;
    int subtitleType;
    int videoproperties;
    int audioproperties;
    int transcoder;
    QString chanOutputFilters;

    QString seriesid;
    QString programid;
    QString catType;

    QString sortTitle;

  private:
    bool ignoreBookmark;
    mutable class ScheduledRecording* record;
    mutable QMutex regExpLock;
    QRegExp regExpSeries;

    QString inUseForWhat;
    PMapDBReplacement *positionMapDBReplacement;
};

Q_DECLARE_METATYPE(ProgramInfo*)

class MPUBLIC ProgramDetail
{
  public:
    QString   channame;
    QString   title;
    QString   subtitle;
    QDateTime startTime;
    QDateTime endTime;
};
typedef vector<ProgramDetail> ProgramDetailList;

/** \class ProgramList
 *  \brief QPtrList of ProgramInfo instances, with helper functions.
 */
class MPUBLIC ProgramList
{
  public:
    ProgramList(bool auto_delete = true) : autodelete(auto_delete) {}
    ~ProgramList();

    typedef list<ProgramInfo*>::iterator iterator;
    typedef list<ProgramInfo*>::const_iterator const_iterator;

    ProgramInfo *operator[](uint index);
    bool operator==(const ProgramList &b) const;

    bool FromScheduler(bool &hasConflicts, QString altTable = "", int recordid=-1);
    bool FromScheduler(void) {
        bool dummyConflicts;
        return FromScheduler(dummyConflicts);
    };

    bool FromProgram(const QString &sql, MSqlBindings &bindings,
                     ProgramList &schedList, bool oneChanid = false);
    bool FromProgram(const QString &sql, MSqlBindings &bindings) {
        ProgramList dummySched;
        return FromProgram(sql, bindings, dummySched);
    }

    bool FromRecorded( bool bDescending, ProgramList *pSchedList );

    bool FromOldRecorded(const QString &sql, MSqlBindings &bindings);

    static bool GetProgramDetailList(
        QDateTime         &nextRecordingStart,
        bool              *hasConflicts = NULL,
        ProgramDetailList *list = NULL);

    ProgramInfo *take(uint i);
    iterator erase(iterator it);
    void clear(void);

    iterator begin(void)             { return pglist.begin(); }
    iterator end(void)               { return pglist.end();   }
    const_iterator begin(void) const { return pglist.begin(); }
    const_iterator end(void)   const { return pglist.end();   }

    void sort(bool (&f)(const ProgramInfo*, const ProgramInfo*))
        { pglist.sort(f); }
    bool empty(void) const { return pglist.empty(); }
    size_t size(void) const { return pglist.size(); }
    void push_front(ProgramInfo *pginfo) { pglist.push_front(pginfo); }
    void push_back(ProgramInfo *pginfo) { pglist.push_back(pginfo); }

    // compatibility with old Q3PtrList
    bool isEmpty(void) const { return empty(); }
    size_t count(void) const { return size(); }
    ProgramInfo *at(uint index) { return (*this)[index]; }
    void prepend(ProgramInfo *pginfo) { push_front(pginfo); }
    void append(ProgramInfo *pginfo) { push_back(pginfo); }
    void setAutoDelete(bool auto_delete) { autodelete = auto_delete; }

  protected:
    list<ProgramInfo*> pglist;
    bool autodelete;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
