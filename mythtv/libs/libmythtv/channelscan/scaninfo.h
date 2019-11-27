#ifndef _CHANNEL_IMPORTER_HELPERS_H_
#define _CHANNEL_IMPORTER_HELPERS_H_

// C++ headers
#include <cstdint>
using uint = unsigned;
#include <vector>
using namespace std;

// Qt headers
#include <QString>
#include <QDateTime>

// MythTV headers
#include "mythtvexp.h"
#include "dtvmultiplex.h"

class ScanInfo
{
  public:
    ScanInfo() = default;
    ScanInfo(uint scanid, uint cardid, uint sourceid,
             bool processed, QDateTime scandate);

    static bool MarkProcessed(uint scanid);
    static bool DeleteScan(uint scanid);

  public:
    uint      m_scanid    {0};
    uint      m_cardid    {0};
    uint      m_sourceid  {0};
    bool      m_processed {false};
    QDateTime m_scandate;
};

MTV_PUBLIC vector<ScanInfo> LoadScanList(void);
uint SaveScan(const ScanDTVTransportList &scan);
MTV_PUBLIC ScanDTVTransportList LoadScan(uint scanid);

#endif // _CHANNEL_IMPORTER_HELPERS_H_
