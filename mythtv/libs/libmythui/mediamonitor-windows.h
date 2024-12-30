#ifndef MYTH_MEDIA_MONITOR_WINDOWS_H
#define MYTH_MEDIA_MONITOR_WINDOWS_H

#include "mediamonitor.h"

#define DEFAULT_DVD "e:"
#define DEFAULT_CD  "e:"

class MediaMonitorWindows : public MediaMonitor
{
  public:
    MediaMonitorWindows(QObject *par, unsigned long interval, bool allowEject);

    bool AddDevice(MythMediaDevice* pDevice) override; // MediaMonitor

    QStringList GetCDROMBlockDevices(void) override; // MediaMonitor
};
#endif // MYTH_MEDIA_MONITOR_WINDOWS_H
