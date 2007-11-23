#ifndef INFOSTRUCTS_H_
#define INFOSTRUCTS_H_

#include <qstring.h>

#include "mythexp.h"

class QPixmap;

class MPUBLIC ChannelInfo
{
 public:
    ChannelInfo() {}
   ~ChannelInfo() {}

    void LoadIcon(int width, int height=0);
    QString Text(QString format);

    QString callsign;
    QString iconpath;
    QString chanstr;
    QString channame;
    int chanid;
    int sourceid;
    QString sourcename;
    int favid;
    QString recpriority;

    QPixmap icon;
    bool iconload;
};

class TimeInfo
{
  public:
    QString usertime;

    int hour;
    int min;
};

#endif
