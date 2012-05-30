#ifndef _MYTH_TIME_ZONE_H_
#define _MYTH_TIME_ZONE_H_

#include <QString>

#include "mythbaseexp.h"

namespace MythTZ
{
MBASE_PUBLIC int calc_utc_offset(void);
MBASE_PUBLIC QString getTimeZoneID(void);
MBASE_PUBLIC bool checkTimeZone(void);
MBASE_PUBLIC bool checkTimeZone(const QStringList &master_settings);
};

#endif // _MYTH_TIME_ZONE_H_
