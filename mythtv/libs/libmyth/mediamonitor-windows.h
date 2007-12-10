#ifndef MYTH_MEDIA_MONITOR_WINDOWS_H
#define MYTH_MEDIA_MONITOR_WINDOWS_H

#include "mythmediamonitor.h"

class MediaMonitorWindows : public MediaMonitor
{
  public:
    MediaMonitorWindows(QObject *par, unsigned long interval, bool allowEject);

    virtual bool AddDevice(MythMediaDevice* pDevice);

    QStringList GetCDROMBlockDevices(void);
};
#endif // MYTH_MEDIA_MONITOR_WINDOWS_H
