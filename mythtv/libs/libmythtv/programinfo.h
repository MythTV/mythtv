#ifndef PROGRAMINFO_H_
#define PROGRAMINFO_H_

#include <qsqldatabase.h>
#include <qstring.h>
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

class ProgramInfo
{
  public:
    ProgramInfo();
    ProgramInfo(const ProgramInfo &other);

    // returns 0 for one-time, 1 for weekdaily, 2 for weekly
    int IsProgramRecurring(void);

    RecordingType GetProgramRecordingStatus(void);
    void ApplyRecordStateChange(RecordingType newstate);

    void WriteRecordedToDB(QSqlDatabase *db);

    QString GetRecordFilename(const QString &prefix);

    int CalculateLength(void);

    QString title;
    QString subtitle;
    QString description;
    QString category;

    QString chanid;
    QString chanstr;
    QString chansign;
    QString channame;

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
    bool conflictfixed;
};

void GetProgramRangeDateTime(QPtrList<ProgramInfo> *proglist, QString channel, 
                             const QString &ltime, const QString &rtime);
ProgramInfo *GetProgramAtDateTime(QString channel, const QString &time);
ProgramInfo *GetProgramAtDateTime(QString channel, QDateTime &dtime);

#endif
