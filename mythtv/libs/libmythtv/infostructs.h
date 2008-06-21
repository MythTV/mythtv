#ifndef INFOSTRUCTS_H_
#define INFOSTRUCTS_H_

#include <qstring.h>
#include <QPixmap>

#include "mythexp.h"

#ifdef USING_MINGW
#undef LoadIcon
#endif

class MPUBLIC ChannelInfo
{
 public:
    ChannelInfo() : chanid(-1), sourceid(-1), favid(-1), iconload(false) {}

    void LoadChannelIcon(int width, int height = 0);
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
