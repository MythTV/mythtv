#ifndef INFOSTRUCTS_H_
#define INFOSTRUCTS_H_

#include <qstring.h>
#include <qpixmap.h>
#include <qdatetime.h>

class ChannelInfo
{
 public:
    ChannelInfo() { icon = NULL; }
   ~ChannelInfo() { if (icon) delete icon; }

    void LoadIcon() { icon = new QPixmap(iconpath); }

    QString callsign;
    QString iconpath;
    QString chanstr;

    QPixmap *icon;
    int channum;
};

class TimeInfo
{
  public:
    QString usertime;
    QString sqltime;

    int year;
    int month;
    int day;
    int hour;
    int min;
};

class ProgramInfo
{
  public:
    ProgramInfo();
    ProgramInfo(const ProgramInfo &other);

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

ProgramInfo *GetProgramAtDateTime(int channel, const char *time);
ProgramInfo *GetProgramAtDateTime(int channel, QDateTime &dtime);

// returns 0 for one-time, 1 for weekdaily, 2 for weekly.
int IsProgramRecurring(ProgramInfo *pginfo);

// returns 0 for no recording, 1 for onetime, 2 for timeslot, 3 for every
int GetProgramRecordingStatus(ProgramInfo *pginfo);
void ApplyRecordStateChange(ProgramInfo *pginfo, int newstate);

#endif
