#ifndef PROGRAMINFO_H_
#define PROGRAMINFO_H_

#include <qsqldatabase.h>
#include <qstring.h>
#include <qregexp.h>
#include <qdatetime.h>

enum RecordingType
{
    kUnknown = -1,
    kNotRecording = 0,
    kSingleRecord = 1,
    kTimeslotRecord,
    kChannelRecord,
    kAllRecord
};

#define NUMPROGRAMLINES 15

class ProgramInfo
{
  public:
    ProgramInfo();
    ProgramInfo(const ProgramInfo &other);

    // returns 0 for one-time, 1 for weekdaily, 2 for weekly
    int IsProgramRecurring(void);

    bool IsSameProgram(const ProgramInfo& other) const;
    bool IsSameTimeslot(const ProgramInfo& other) const;

    RecordingType GetProgramRecordingStatus(void);
    void ApplyRecordStateChange(RecordingType newstate);
    void ApplyRecordTimeChange(const QDateTime &newstartts,
                               const QDateTime &newendts);

    void WriteRecordedToDB(QSqlDatabase *db);

    QString GetRecordFilename(const QString &prefix);

    int CalculateLength(void);

    void ToStringList(QStringList &list);
    void FromStringList(QStringList &list, int offset);

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

    RecordingType recordtype;
    bool conflicting;
    bool recording;

    int sourceid;
    int inputid;
    int cardid;
    int recordingprofileid;
    bool conflictfixed;
};

void GetProgramRangeDateTime(QPtrList<ProgramInfo> *proglist, QString channel, 
                             const QString &ltime, const QString &rtime);
ProgramInfo *GetProgramAtDateTime(QString channel, const QString &time);
ProgramInfo *GetProgramAtDateTime(QString channel, QDateTime &dtime);

#endif
