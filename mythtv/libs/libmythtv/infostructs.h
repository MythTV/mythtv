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
    QString Text(QString format);

    QString callsign;
    QString iconpath;
    QString chanstr;
    QString channame;
    int chanid;
    int sourceid;
    int favid;
    QString recpriority;

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
