#ifndef PROGRAMINFO_H_
#define PROGRAMINFO_H_

#include "recordingtypes.h"

#include <qsqldatabase.h>
#include <qstring.h>
#include <qdatetime.h>
#include <qmap.h>
#include <vector>

using namespace std;

#define NUMPROGRAMLINES 38

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

enum TranscoderStatus {
    TRANSCODE_QUEUED      = 0x01,
    TRANSCODE_RETRY       = 0x02,
    TRANSCODE_FAILED      = 0x03,
    TRANSCODE_LAUNCHED    = 0x04,
    TRANSCODE_STARTED     = 0x05,
    TRANSCODE_FINISHED    = 0x06,
    TRANSCODE_USE_CUTLIST = 0x10,
    TRANSCODE_STOP        = 0x20,
    TRANSCODE_FLAGS       = 0xF0
};

enum FlagMask {
    FL_COMMFLAG  = 0x01,
    FL_CUTLIST   = 0x02,
    FL_AUTOEXP   = 0x04,
    FL_EDITING   = 0x08,
    FL_BOOKMARK  = 0x10
};

enum RecStatusType {
    rsDeleted = -5,
    rsStopped = -4,
    rsRecorded = -3,
    rsRecording = -2,
    rsWillRecord = -1,
    rsUnknown = 0,
    rsDontRecord = 1,
    rsPreviousRecording = 2,
    rsCurrentRecording = 3,
    rsEarlierShowing = 4,
    rsTooManyRecordings = 5,
    rsCancelled = 6,
    rsConflict = 7,
    rsLaterShowing = 8,
    rsRepeat = 9,
    rsInactive = 10,
    rsLowDiskSpace = 11,
    rsTunerBusy = 12
};

class ScheduledRecording;
class QGridLayout;

class ProgramInfo
{
  public:
    ProgramInfo();
    ProgramInfo(const ProgramInfo &other);
    
    ~ProgramInfo();

    ProgramInfo& operator=(const ProgramInfo &other);
    ProgramInfo& clone(const ProgramInfo &other);

    bool IsFindApplicable(void) const;
    
    // returns 0 for one-time, 1 for weekdaily, 2 for weekly
    int IsProgramRecurring(void);

    // checks for duplicates according to dupmethod
    bool IsSameProgram(const ProgramInfo& other) const;
    // checks chanid, start/end times, sourceid, cardid, inputid.
    bool IsSameTimeslot(const ProgramInfo& other) const;
    // checks chanid, start/end times, sourceid
    bool IsSameProgramTimeslot(const ProgramInfo& other) const;

    void Save();

    RecordingType GetProgramRecordingStatus();
    QString GetProgramRecordingProfile();

    int GetChannelRecPriority(const QString &chanid);
    int GetRecordingTypeRecPriority(RecordingType type);

    void ApplyRecordStateChange(RecordingType newstate);
    void ApplyRecordTimeChange(const QDateTime &newstartts,
                               const QDateTime &newendts);
    void ApplyRecordRecPriorityChange(int);
    void ApplyRecordRecGroupChange(const QString &newrecgroup);
    void ToggleRecord();

    ScheduledRecording* GetScheduledRecording();

    int getRecordID();

    void StartedRecording();
    void FinishedRecording(bool prematurestop);

    QGridLayout* DisplayWidget(QWidget *parent = NULL,
                               QString searchtitle = "");

    void showDetails();

    QString GetRecordBasename(void);
    QString GetRecordFilename(const QString &prefix);
    QString GetPlaybackURL(QString playbackHost = "");

    QString MakeUniqueKey(void) const;

    int CalculateLength(void);

    void ToStringList(QStringList &list);
    bool FromStringList(QStringList &list, int offset);
    bool FromStringList(QStringList &list, QStringList::iterator &it);
    void ToMap(QMap<QString, QString> &progMap);

    void SetFilesize(long long fsize);
    long long GetFilesize();
    void SetBookmark(long long pos);
    long long GetBookmark();
    bool IsEditing();
    void SetEditing(bool edit);
    bool IsCommFlagged();
    void SetDeleteFlag(bool deleteFlag);
    // 1 = flagged, 2 = processing
    void SetCommFlagged(int flag);
    bool IsCommProcessing();
    void SetAutoExpire(bool autoExpire);
    void SetPreserveEpisode(bool preserveEpisode);
    bool GetAutoExpireFromRecorded();
    bool GetPreserveEpisodeFromRecorded();

    bool UsesMaxEpisodes();

    int GetAutoRunJobs();

    void GetCutList(QMap<long long, int> &delMap);
    void SetCutList(QMap<long long, int> &delMap);

    void SetBlankFrameList(QMap<long long, int> &frames,
                           long long min_frame = -1, long long max_frame = -1);
    void GetBlankFrameList(QMap<long long, int> &frames);

    void SetCommBreakList(QMap<long long, int> &frames);
    void GetCommBreakList(QMap<long long, int> &frames);

    void ClearMarkupMap(int type = -100,
                      long long min_frame = -1, long long max_frame = -1);
    void SetMarkupMap(QMap<long long, int> &marks,
                      int type = -100,
                      long long min_frame = -1, long long max_frame = -1);
    void GetMarkupMap(QMap<long long, int> &marks,
                      int type, bool mergeIntoMap = false);
    bool CheckMarkupFlag(int type);
    void SetMarkupFlag(int type, bool processing);
    void GetPositionMap(QMap<long long, long long> &posMap, int type);
    void ClearPositionMap(int type);
    void SetPositionMap(QMap<long long, long long> &posMap, int type,
                        long long min_frame = -1, long long max_frame = -1);
    void SetPositionMapDelta(QMap<long long, long long> &posMap, int type);

    void DeleteHistory();
    void setIgnoreBookmark(bool ignore) { ignoreBookmark = ignore; }
    QString RecTypeChar(void);
    QString RecTypeText(void);
    QString RecStatusChar(void);
    QString RecStatusText(void);
    QString RecStatusDesc(void);
    void FillInRecordInfo(vector<ProgramInfo *> &reclist);
    void EditScheduled();
    void EditRecording();
    QString ChannelText(QString);

    int getProgramFlags();

    static ProgramInfo *GetProgramAtDateTime(const QString &channel, 
                                             QDateTime &dtime);
    static ProgramInfo *GetProgramFromRecorded(const QString &channel, 
                                               const QString &starttime);
    static ProgramInfo *GetProgramFromRecorded(const QString &channel, 
                                               QDateTime &dtime);
    int SecsTillStart() const { return QDateTime::currentDateTime().secsTo(startts); }

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
    int chancommfree;

    QString pathname;
    long long filesize;
    QString hostname;

    QDateTime startts;
    QDateTime endts;
    QDateTime recstartts;
    QDateTime recendts;

    
    bool isVideo;
    int lenMins;
    
    QString year;
    float stars;

    QDate originalAirDate;
    QDateTime lastmodified;
    
    bool hasAirDate;
    bool repeat;

    int spread;
    int startCol;

    RecStatusType recstatus;
    RecStatusType savedrecstatus;
    int numconflicts;
    int conflictpriority;
    int reactivate;             // 0 = not requested
                                // 1 = requested, pending
                                // 2 = requested, replaced
                                // -1 = reactivated
    int recordid;
    RecordingType rectype;
    RecordingDupInType dupin;
    RecordingDupMethodType dupmethod;

    int sourceid;
    int inputid;
    int cardid;
    bool shareable;
    bool conflictfixed;

    QString schedulerid;
    int findid;

    int programflags;
    QString chanOutputFilters;
    
    QString seriesid;
    QString programid;
    QString catType;

    QString sortTitle;

private:
    bool ignoreBookmark;
    void handleRecording();
    void handleNotRecording();

    class ScheduledRecording* record;
};

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

    bool FromScheduler(bool &hasConflicts);
    bool FromScheduler(void) {
        bool dummyConflicts;
        return FromScheduler(dummyConflicts);
    };

    bool FromProgram(const QString sql,
                     ProgramList &schedList);
    bool FromProgram(const QString sql) {
        ProgramList dummySched;
        return FromProgram(sql, dummySched);
    }

    bool FromOldRecorded(const QString sql);

    bool FromRecorded(const QString sql,
                      ProgramList &schedList);
    bool FromRecorded(const QString sql) {
        ProgramList dummySched;
        return FromRecorded(sql, dummySched);
    }

    bool FromRecord(const QString sql);

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

