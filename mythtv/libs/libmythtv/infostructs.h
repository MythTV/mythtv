#ifndef INFOSTRUCTS_H_
#define INFOSTRUCTS_H_

#include <qstring.h>

class QPixmap;

class ChannelInfo
{
 public:
    ChannelInfo() { icon = NULL; }
   ~ChannelInfo() { if (icon) delete icon; }

    void LoadIcon(int size);

    QString callsign;
    QString iconpath;
    QString chanstr;
    int chanid;
    int favid;
    QString rank;

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
