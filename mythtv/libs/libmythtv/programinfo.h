#ifndef PROGRAMINFO_H_
#define PROGRAMINFO_H_

#include <qsqldatabase.h>
#include <qstring.h>
#include <qdatetime.h>
#include <qmap.h>
#include "scheduledrecording.h"

#define NUMPROGRAMLINES 16

class QSqlDatabase;

class ProgramInfo
{
  public:
    ProgramInfo();
    ProgramInfo(const ProgramInfo &other);
    ~ProgramInfo();

    // returns 0 for one-time, 1 for weekdaily, 2 for weekly
    int IsProgramRecurring(void);

    bool IsSameProgram(const ProgramInfo& other) const;
    bool IsSameTimeslot(const ProgramInfo& other) const;

    ScheduledRecording::RecordingType GetProgramRecordingStatus(QSqlDatabase *db);
    void ApplyRecordStateChange(QSqlDatabase *db,
                                ScheduledRecording::RecordingType newstate);
    void ApplyRecordTimeChange(QSqlDatabase *db, 
                               const QDateTime &newstartts,
                               const QDateTime &newendts);
    ScheduledRecording* GetScheduledRecording(QSqlDatabase *db) {
        GetProgramRecordingStatus(db);
        return record;
    };

    void WriteRecordedToDB(QSqlDatabase *db);

    QString GetRecordFilename(const QString &prefix);

    int CalculateLength(void);

    void ToStringList(QStringList &list);
    void FromStringList(QStringList &list, int offset);

    void SetBookmark(long long pos, QSqlDatabase *db);
    long long GetBookmark(QSqlDatabase *db);
    bool IsEditing(QSqlDatabase *db);
    void SetEditing(bool edit, QSqlDatabase *db);
    void GetCutList(QMap<long long, int> &delMap, QSqlDatabase *db);
    void SetCutList(QMap<long long, int> &delMap, QSqlDatabase *db);

    static void GetProgramRangeDateTime(QPtrList<ProgramInfo> *proglist, QString channel, 
                                        const QString &ltime, const QString &rtime);
    static ProgramInfo *GetProgramAtDateTime(QString channel, const QString &time);
    static ProgramInfo *GetProgramAtDateTime(QString channel, QDateTime &dtime);

    QString title;
    QString subtitle;
    QString description;
    QString category;

    QString chanid;
    QString chanstr;
    QString chansign;
    QString channame;

    QString pathname;
    long long filesize;
    QString hostname;

    QDateTime startts;
    QDateTime endts;

    int spread;
    int startCol;

    bool conflicting;
    bool recording;

    int sourceid;
    int inputid;
    int cardid;
    bool conflictfixed;

    QString schedulerid;

private:
    class ScheduledRecording* record;
};

#endif
