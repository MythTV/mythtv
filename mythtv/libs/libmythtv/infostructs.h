#ifndef INFOSTRUCTS_H_
#define INFOSTRUCTS_H_

#include <qstring.h>
#include <qpixmap.h>
#include <qdatetime.h>

class MythContext;

class ChannelInfo
{
 public:
    ChannelInfo() { icon = NULL; }
   ~ChannelInfo() { if (icon) delete icon; }

    void LoadIcon(MythContext *context);

    QString callsign;
    QString iconpath;
    QString chanstr;
    int chanid;

    QPixmap *icon;
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

#endif
