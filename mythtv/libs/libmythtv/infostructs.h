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

    void LoadIcon(void);

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

    int hour;
    int min;
};

#endif
