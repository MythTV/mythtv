#ifndef PROGRAMINFO_H_
#define PROGRAMINFO_H_

#include <qsqldatabase.h>
#include <qstring.h>
#include <qregexp.h>
#include <qdatetime.h>
#include "scheduledrecording.h"

#define NUMPROGRAMLINES 15

class MythContext;

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

    ScheduledRecording::RecordingType GetProgramRecordingStatus(void);
    void ApplyRecordStateChange(ScheduledRecording::RecordingType newstate);
    void ApplyRecordTimeChange(const QDateTime &newstartts,
                               const QDateTime &newendts);
    ScheduledRecording* GetScheduledRecording(void) {
        GetProgramRecordingStatus();
        return record;
    };

    void WriteRecordedToDB(QSqlDatabase *db);

    QString GetRecordFilename(const QString &prefix);

    int CalculateLength(void);

    void ToStringList(QStringList &list);
    void FromStringList(QStringList &list, int offset);


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
