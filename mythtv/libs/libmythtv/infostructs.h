#ifndef INFOSTRUCTS_H_
#define INFOSTRUCTS_H_

#include <qstring.h>

class QPixmap;

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
    int favid;

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
