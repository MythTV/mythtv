#ifndef PROGRAMINFO_H_
#define PROGRAMINFO_H_

#include <qsqldatabase.h>
#include <qstring.h>
#include <qdatetime.h>
#include <qmap.h>
#include "scheduledrecording.h"

#define NUMPROGRAMLINES 21

#define MARK_UPDATED_CUT  -3
#define MARK_EDIT_MODE    -2
#define MARK_PROCESSING   -1
#define MARK_CUT_END       0
#define MARK_CUT_START     1
#define MARK_BOOKMARK      2
#define MARK_BLANK_FRAME   3
#define MARK_COMM_START    4
#define MARK_COMM_END      5
#define MARK_GOP_START     6
#define MARK_KEYFRAME      7
#define MARK_SCENE_CHANGE  8

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

    void Save(void);

    ScheduledRecording::RecordingType GetProgramRecordingStatus(QSqlDatabase *db);
    int GetChannelRank(QString chanid, QSqlDatabase *db);
    int GetRecordingTypeRank(ScheduledRecording::RecordingType type);

    void ApplyRecordStateChange(QSqlDatabase *db,
                                ScheduledRecording::RecordingType newstate);
    void ApplyRecordTimeChange(QSqlDatabase *db, 
                               const QDateTime &newstartts,
                               const QDateTime &newendts);
    void ApplyRecordRankChange(QSqlDatabase *db,
                               const QString &newrank);
    void ToggleRecord(QSqlDatabase *dB);
    ScheduledRecording* GetScheduledRecording(QSqlDatabase *db) {
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

    static void GetProgramRangeDateTime(QPtrList<ProgramInfo> *proglist, 
                                        QString channel, 
                                        const QString &ltime, 
                                        const QString &rtime);
    static ProgramInfo *GetProgramAtDateTime(QString channel, 
                                             const QString &time);
    static ProgramInfo *GetProgramAtDateTime(QString channel, QDateTime &dtime);
    static ProgramInfo *GetProgramFromRecorded(QString channel, 
                                               QString starttime);
    static ProgramInfo *GetProgramFromRecorded(QString channel, 
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

    int sourceid;
    int inputid;
    int cardid;
    bool conflictfixed;

    QString schedulerid;

private:
    class ScheduledRecording* record;
};

#endif
