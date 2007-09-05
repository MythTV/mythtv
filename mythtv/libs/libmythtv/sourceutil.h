// -*- Mode: c++ -*-
#ifndef _SOURCEUTIL_H_
#define _SOURCEUTIL_H_

// Qt headers
#include <qstring.h>

class SourceUtil
{
  public:
    static QString GetChannelSeparator(uint sourceid);
    static QString GetChannelFormat(uint sourceid);
    static uint    GetChannelCount(uint sourceid);
    static bool    GetListingsLoginData(uint sourceid,
                                        QString &grabber, QString &userid,
                                        QString &passwd,  QString &lineupid);
    static uint    GetConnectionCount(uint sourceid);
    static bool    IsProperlyConnected(uint sourceid, bool strich = false);
    static bool    IsEncoder(uint sourceid, bool strict = false);
    static bool    IsUnscanable(uint sourceid);
    static bool    UpdateChannelsFromListings(
        uint sourceid, QString cardtype = QString::null);
};

#endif //_SOURCEUTIL_H_
