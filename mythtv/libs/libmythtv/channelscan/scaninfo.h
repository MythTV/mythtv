#ifndef CHANNEL_IMPORTER_HELPERS_H
#define CHANNEL_IMPORTER_HELPERS_H

// C++ headers
#include <cstdint>
using uint = unsigned;
#include <vector>

// Qt headers
#include <QString>
#include <QDateTime>

// MythTV headers
#include "libmythtv/dtvmultiplex.h"
#include "libmythtv/mythtvexp.h"

class ScanInfo
{
  public:
    ScanInfo() = default;
    ScanInfo(uint scanid, uint cardid, uint sourceid,
             bool processed, QDateTime scandate);

    static bool MarkProcessed(uint scanid);
    static bool DeleteScan(uint scanid);
    static void DeleteScansFromSource(uint sourceid);

  public:
    uint      m_scanid    {0};
    uint      m_cardid    {0};
    uint      m_sourceid  {0};
    bool      m_processed {false};
    QDateTime m_scandate;
};

MTV_PUBLIC std::vector<ScanInfo> LoadScanList(void);
MTV_PUBLIC std::vector<ScanInfo> LoadScanList(uint sourceid);
uint SaveScan(const ScanDTVTransportList &scan);
MTV_PUBLIC ScanDTVTransportList LoadScan(uint scanid);

#endif // CHANNEL_IMPORTER_HELPERS_H
