#ifndef INFOSTRUCTS_H_
#define INFOSTRUCTS_H_

#include <QString>
#include <QPixmap>
#include <QVariant>

#include "mythexp.h"

#ifdef USING_MINGW
#undef LoadIcon
#endif

class MPUBLIC ChannelInfo
{
 public:
    ChannelInfo() : chanid(-1), sourceid(-1), favid(-1) {}
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
};

class TimeInfo
{
  public:
    QString usertime;

    int hour;
    int min;
};

Q_DECLARE_METATYPE(ChannelInfo*)

#endif
