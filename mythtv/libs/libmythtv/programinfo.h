#ifndef PROGRAMINFO_H_
#define PROGRAMINFO_H_

#include <qsqldatabase.h>
#include <qstring.h>
#include <qdatetime.h>
#include <qmap.h>
#include "scheduledrecording.h"

#define NUMPROGRAMLINES 23

enum MarkTypes {
    MARK_UPDATED_CUT = -3,
    MARK_EDIT_MODE = -2,
    MARK_PROCESSING = -1,
    MARK_CUT_END = 0,
    MARK_CUT_START = 1,
    MARK_BOOKMARK = 2,
    MARK_BLANK_FRAME = 3,
    MARK_COMM_START = 4,
    MARK_COMM_END = 5,
    MARK_GOP_START = 6,
    MARK_KEYFRAME = 7,
    MARK_SCENE_CHANGE = 8
};

enum TranscoderStatus {
    TRANSCODE_QUEUED = -3,
    TRANSCODE_RETRY = -2,
    TRANSCODE_FAILED = -1,
    TRANSCODE_LAUNCHED = 0,
    TRANSCODE_STARTED = 1,
    TRANSCODE_FINISHED = 2,
};

class QSqlDatabase;

class ProgramInfo
{
  public:
    ProgramInfo();
    ProgramInfo(const ProgramInfo &other);
    ~ProgramInfo();

    // returns 0 for one-time, 1 for weekdaily, 2 for weekly
    int IsProgramRecurring(void);

    // checks title, subtitle, description
    bool IsSameProgram(const ProgramInfo& other) const;
    // checks chanid, start/end times, sourceid, cardid, inputid.
    bool IsSameTimeslot(const ProgramInfo& other) const;
    // checks chanid, start/end times, sourceid
    bool IsSameProgramTimeslot(const ProgramInfo& other) const;

    void Save(QSqlDatabase *db);

    RecordingType GetProgramRecordingStatus(QSqlDatabase *db);
    bool AllowRecordingNewEpisodes(QSqlDatabase *db);
    int GetChannelRank(QSqlDatabase *db, const QString &chanid);
    int GetRecordingTypeRank(RecordingType type);

    void ApplyRecordStateChange(QSqlDatabase *db, RecordingType newstate);
    void ApplyRecordTimeChange(QSqlDatabase *db, 
                               const QDateTime &newstartts,
                               const QDateTime &newendts);
    void ApplyRecordRankChange(QSqlDatabase *db,
                               const QString &newrank);
    void ToggleRecord(QSqlDatabase *dB);

    ScheduledRecording* GetScheduledRecording(QSqlDatabase *db) 
    {
        GetProgramRecordingStatus(db);
        return record;
    };

    void StartedRecording(QSqlDatabase *db);
    void FinishedRecording(QSqlDatabase* db);

    QGridLayout* DisplayWidget(ScheduledRecording *rec = NULL,
                               QWidget *parent = NULL);

    QString GetRecordBasename(void);
    QString GetRecordFilename(const QString &prefix);

    int CalculateLength(void);

    void ToStringList(QStringList &list);
    void FromStringList(QStringList &list, int offset);
    void ToMap(QSqlDatabase *db, QMap<QString, QString> &progMap);

    void SetBookmark(long long pos, QSqlDatabase *db);
    long long GetBookmark(QSqlDatabase *db);
    bool IsEditing(QSqlDatabase *db);
    void SetEditing(bool edit, QSqlDatabase *db);
    void SetAutoExpire(bool autoExpire, QSqlDatabase *db);
    bool GetAutoExpireFromRecorded(QSqlDatabase *db);

    void GetCutList(QMap<long long, int> &delMap, QSqlDatabase *db);
    void SetCutList(QMap<long long, int> &delMap, QSqlDatabase *db);

    void SetBlankFrameList(QMap<long long, int> &frames, QSqlDatabase *db,
                           long long min_frame = -1, long long max_frame = -1);
    void GetBlankFrameList(QMap<long long, int> &frames, QSqlDatabase *db);

    void SetCommBreakList(QMap<long long, int> &frames, QSqlDatabase *db);
    void GetCommBreakList(QMap<long long, int> &frames, QSqlDatabase *db);

    void ClearMarkupMap(QSqlDatabase *db, int type = -100,
                      long long min_frame = -1, long long max_frame = -1);
    void SetMarkupMap(QMap<long long, int> &marks, QSqlDatabase *db,
                      int type = -100,
                      long long min_frame = -1, long long max_frame = -1);
    void GetMarkupMap(QMap<long long, int> &marks, QSqlDatabase *db,
                      int type, bool mergeIntoMap = false);
    bool CheckMarkupFlag(int type, QSqlDatabase *db);
    void SetMarkupFlag(int type, bool processing, QSqlDatabase *db);
    void GetPositionMap(QMap<long long, long long> &posMap, int type,
                        QSqlDatabase *db);
    void SetPositionMap(QMap<long long, long long> &posMap, int type,
                        QSqlDatabase *db,
                        long long min_frame = -1, long long max_frame = -1);

    void DeleteHistory(QSqlDatabase *db);

    static void GetProgramListByQuery(QSqlDatabase *db,
                                        QPtrList<ProgramInfo> *proglist,
                                        const QString &where);
    static void GetProgramRangeDateTime(QSqlDatabase *db,
                                        QPtrList<ProgramInfo> *proglist, 
                                        const QString &channel, 
                                        const QString &ltime, 
                                        const QString &rtime);
    static ProgramInfo *GetProgramAtDateTime(QSqlDatabase *db, 
                                             const QString &channel, 
                                             const QString &time);
    static ProgramInfo *GetProgramAtDateTime(QSqlDatabase *db,
                                             const QString &channel, 
                                             QDateTime &dtime);
    static ProgramInfo *GetProgramFromRecorded(QSqlDatabase *db,
                                               const QString &channel, 
                                               const QString &starttime);
    static ProgramInfo *GetProgramFromRecorded(QSqlDatabase *db,
                                               const QString &channel, 
                                               QDateTime &dtime);

    QString title;
    QString subtitle;
    QString description;
    QString category;

    QString chanid;
    QString chanstr;
    QString chansign;
    QString channame;
    QString rank;

    QString pathname;
    long long filesize;
    QString hostname;

    QDateTime startts;
    QDateTime endts;

    int spread;
    int startCol;

    bool conflicting;
    bool recording;
    bool duplicate;
    bool suppressed;
    QString reasonsuppressed;

    int sourceid;
    int inputid;
    int cardid;
    bool conflictfixed;

    QString schedulerid;

private:
    class ScheduledRecording* record;
};

#endif
