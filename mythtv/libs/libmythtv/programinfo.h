#ifndef PROGRAMINFO_H_
#define PROGRAMINFO_H_

#include <qsqldatabase.h>
#include <qstring.h>
#include <qdatetime.h>

class ProgramInfo
{
  public:
    ProgramInfo();
    ProgramInfo(const ProgramInfo &other);

    // returns 0 for one-time, 1 for weekdaily, 2 for weekly
    int IsProgramRecurring(void);

    // returns 0 for no recording, 1 for onetime, 2 for timeslot, 3 for every
    int GetProgramRecordingStatus(void);
    void ApplyRecordStateChange(int newstate);

    void WriteRecordedToDB(QSqlDatabase *db);

    QString GetRecordFilename(const QString &prefix);

    int CalculateLength(void);

    QString title;
    QString subtitle;
    QString description;
    QString category;
    QString channum;

    QDateTime startts;
    QDateTime endts;

    int spread;
    int startCol;

    int recordtype;
    bool conflicting;
    bool recording;
};

ProgramInfo *GetProgramAtDateTime(QString channel, const QString &time);
ProgramInfo *GetProgramAtDateTime(QString channel, QDateTime &dtime);

#endif
