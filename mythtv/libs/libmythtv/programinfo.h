#ifndef PROGRAMINFO_H_
#define PROGRAMINFO_H_

#include "recordingtypes.h"
#include "mythdbcon.h"

#include <qstring.h>
#include <qdatetime.h>
#include <qmap.h>
#include <vector>

using namespace std;
typedef QMap<long long, long long> frm_pos_map_t;
typedef QMap<long long, int> frm_dir_map_t;

#define NUMPROGRAMLINES 41

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
    MARK_GOP_BYFRAME = 9
} MarkTypes;

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
    FL_COMMFLAG       = 0x001,
    FL_CUTLIST        = 0x002,
    FL_AUTOEXP        = 0x004,
    FL_EDITING        = 0x008,
    FL_BOOKMARK       = 0x010,
    FL_INUSERECORDING = 0x020,
    FL_INUSEPLAYING   = 0x040,
    FL_STEREO         = 0x080,
    FL_CC             = 0x100,
    FL_HDTV           = 0x200,
    FL_TRANSCODED     = 0x400
};

enum RecStatusType {
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
    rsNeverRecord = 11
};

enum AvailableStatusType {
    asAvailable = 0,
    asPendingDelete,
    asFileNotFound
};

class ScheduledRecording;
class QGridLayout;

class ProgramInfo
{
  public:
    // Constructors and bulk set methods.
    ProgramInfo(void);
    ProgramInfo(const ProgramInfo &other);
    static ProgramInfo *GetProgramAtDateTime(const QString &channel, 
                                             const QDateTime &dtime, 
                                             bool genUnknown = false);
    static ProgramInfo *GetProgramFromRecorded(const QString &channel, 
                                               const QString &starttime);
    static ProgramInfo *GetProgramFromRecorded(const QString &channel, 
                                               const QDateTime &dtime);

    ProgramInfo& operator=(const ProgramInfo &other);
    ProgramInfo& clone(const ProgramInfo &other);
    bool FromStringList(QStringList &list, int offset);
    bool FromStringList(QStringList &list, QStringList::iterator &it);
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
    void AddHistory(bool resched = true);
    void DeleteHistory(void);
    void ForgetHistory(void);
    void SetDupHistory(void);

    // Used to update database with recording info
    void StartedRecording(QString prefix, QString ext);
    void FinishedRecording(bool prematurestop);
    void UpdateRecordingEnd(void);
    void ApplyRecordRecID(void);
    void ApplyRecordRecGroupChange(const QString &newrecgroup);
    void ApplyRecordPlayGroupChange(const QString &newrecgroup);
    void ApplyRecordRecTitleChange(const QString &newTitle,
                                   const QString &newSubtitle);

    // Quick gets
    bool SetRecordBasename(QString basename);
    QString GetRecordBasename(void) const;
    QString GetRecordFilename(const QString &prefix) const;
    QString GetPlaybackURL(QString playbackHost = "") const;
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

    // Quick sets
    /// \brief If "ignore" is true GetBookmark() will return 0, otherwise
    ///        GetBookmark() will return the bookmark position if it exists.
    void setIgnoreBookmark(bool ignore) { ignoreBookmark = ignore; }

    // DB gets
    long long GetFilesize(void);
    long long GetBookmark(void) const;
    bool IsEditing(void) const;
    bool IsCommFlagged(void) const;
    bool IsInUse(QString &byWho) const;
    int GetAutoExpireFromRecorded(void) const;
    bool GetPreserveEpisodeFromRecorded(void) const;
    bool UsesMaxEpisodes(void) const;
    int getProgramFlags(void) const;
    bool GetChannel(QString &channum, QString &input) const;
    QString GetFileName(void) const { return pathname; }
    QString toString(void) const;

    // DB sets
    void SetFilesize(long long fsize);
    void SetBookmark(long long pos) const;
    void SetEditing(bool edit) const;
    void SetTranscoded(int transFlag) const;
    void SetDeleteFlag(bool deleteFlag) const;
    void SetCommFlagged(int flag) const; // 1 = flagged, 2 = processing
    void SetAutoExpire(int autoExpire) const;
    void SetPreserveEpisode(bool preserveEpisode) const;

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


    // GUI stuff
    void showDetails(void) const;
    void EditRecording(void);
    void EditScheduled(void);

    // In-use, autodeletion prevention stuff
    void MarkAsInUse(bool inuse, QString usedFor = "");

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
    int transcoder;
    QString chanOutputFilters;
    
    QString seriesid;
    QString programid;
    QString catType;

    QString sortTitle;

  private:
    bool ignoreBookmark;
    mutable class ScheduledRecording* record;

    QString inUseForWhat;
};

/** \class ProgramList
 *  \brief QPtrList of ProgramInfo instances, with helper functions.
 */
class ProgramList: public QPtrList<ProgramInfo> {
 public:
    ProgramList(bool autoDelete = true) {
        setAutoDelete(autoDelete);
        compareFunc = NULL;
    };
    ~ProgramList(void) { };

    ProgramInfo * operator[](uint index) {
        return at(index);
    };

    bool FromScheduler(bool &hasConflicts, QString altTable = "", int recordid=-1);
    bool FromScheduler(void) {
        bool dummyConflicts;
        return FromScheduler(dummyConflicts);
    };

    bool FromProgram(const QString &sql, MSqlBindings &bindings,
                     ProgramList &schedList);
    bool FromProgram(const QString &sql, MSqlBindings &bindings) {
        ProgramList dummySched;
        return FromProgram(sql, bindings, dummySched);
    }

    bool FromOldRecorded(const QString &sql, MSqlBindings &bindings);

    typedef int (*CompareFunc)(ProgramInfo *p1, ProgramInfo *p2);
    void Sort(CompareFunc func) {
        compareFunc = func;
        sort();
    };

 protected:
    virtual int compareItems(QPtrCollection::Item item1,
                             QPtrCollection::Item item2);

 private:
    CompareFunc compareFunc;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
