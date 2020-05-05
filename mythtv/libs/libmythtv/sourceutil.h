// -*- Mode: c++ -*-
#ifndef _SOURCEUTIL_H_
#define _SOURCEUTIL_H_

// C++ headers
#include <vector>
using namespace std;

// Qt headers
#include <QString>

// MythTV headers
#include "mythtvexp.h"

class MTV_PUBLIC SourceUtil
{
  public:
    static bool    HasDigitalChannel(uint sourceid);
    static QString GetSourceName(uint sourceid);
    static uint    GetSourceID(const QString &name);
    static QString GetChannelSeparator(uint sourceid);
    static QString GetChannelFormat(uint sourceid);
    static uint    GetChannelCount(uint sourceid);
    static vector<uint> GetMplexIDs(uint sourceid);
    static bool    GetListingsLoginData(uint sourceid,
                                        QString &grabber, QString &userid,
                                        QString &passwd,  QString &lineupid);
    static uint    GetConnectionCount(uint sourceid);
    static bool    IsProperlyConnected(uint sourceid, bool strict = false);
    static bool    IsEncoder(uint sourceid, bool strict = false);
    static bool    IsUnscanable(uint sourceid);
    static bool    IsCableCardPresent(uint sourceid);
    static bool    IsAnySourceScanable(void);
    static bool    IsSourceIDValid(uint sourceid);
    static bool    UpdateChannelsFromListings(
        uint sourceid, const QString& inputtype = QString(), bool wait = false);

    static bool    UpdateSource( uint sourceid, const QString& sourcename,
                                 const QString& grabber, const QString& userid,
                                 const QString& freqtable, const QString& lineupid,
                                 const QString& password, bool useeit,
                                 const QString& configpath, int nitid,
                                 uint bouquetid, uint regionid, uint scanfrequency);
    static int     CreateSource( const QString& sourcename,
                                 const QString& grabber, const QString& userid,
                                 const QString& freqtable, const QString& lineupid,
                                 const QString& password, bool useeit,
                                 const QString& configpath, int nitid,
                                 uint bouquetid, uint regionid, uint scanfrequency);
    static bool    DeleteSource(uint sourceid);
    static bool    DeleteAllSources(void);
};

#endif //_SOURCEUTIL_H_
