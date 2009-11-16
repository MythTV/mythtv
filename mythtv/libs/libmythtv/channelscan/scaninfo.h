#ifndef _CHANNEL_IMPORTER_HELPERS_H_
#define _CHANNEL_IMPORTER_HELPERS_H_

// POSIX headers
#include <stdint.h>
typedef unsigned uint;

// C++ headers
#include <vector>
using namespace std;

// Qt headers
#include <QString>
#include <QDateTime>

// MythTV headers
#include "mythexp.h"
#include "dtvmultiplex.h"

class ScanInfo
{
  public:
    ScanInfo();
    ScanInfo(uint _scanid, uint _cardid, uint _sourceid,
             bool _processed, const QDateTime &_scandate);

    static bool MarkProcessed(uint scanid);
    static bool DeleteScan(uint scanid);

  public:
    uint      scanid;
    uint      cardid;
    uint      sourceid;
    bool      processed;
    QDateTime scandate;
};

MPUBLIC vector<ScanInfo> LoadScanList(void);
uint SaveScan(const ScanDTVTransportList &scan);
MPUBLIC ScanDTVTransportList LoadScan(uint scanid);

#endif // _CHANNEL_IMPORTER_HELPERS_H_
