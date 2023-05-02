#ifndef SATIPUTILS_H
#define SATIPUTILS_H

// Qt headers
#include <QString>
#include <QStringList>

// MythTV headers
#include "libmythtv/dtvconfparserhelpers.h"
#include "libmythtv/cardutil.h"

class MTV_PUBLIC SatIP
{
  public:
    static QStringList probeDevices(void);
    static QString findDeviceIP(const QString& deviceuuid);
    static CardUtil::INPUT_TYPES toDVBInputType(const QString& deviceid);
    static int toTunerType(const QString& deviceid);
    static QString freq(uint64_t freq);
    static QString bw(DTVBandwidth bw);
    static QString msys(DTVModulationSystem msys);
    static QString mtype(DTVModulation mtype);
    static QString tmode(DTVTransmitMode tmode);
    static QString gi(DTVGuardInterval gi);
    static QString fec(DTVCodeRate fec);
    static QString ro(DTVRollOff ro);
    static QString pol(DTVPolarity pol);

  private:
    static QStringList doUPNPsearch(bool loginfo);
    static QStringList findServers(void);
};

#endif // SATIPUTILS_H
