#ifndef MYTH_MEDIA_MONITOR_WINDOWS_H
#define MYTH_MEDIA_MONITOR_WINDOWS_H

class MediaMonitorWindows : public MediaMonitor
{
  public:
    MediaMonitorWindows(QObject* par, unsigned long interval, bool allowEject)
        : MediaMonitor(par, interval, allowEject) {};

    virtual bool AddDevice(MythMediaDevice* pDevice);

    QStringList GetCDROMBlockDevices(void);
};
#endif // MYTH_MEDIA_MONITOR_WINDOWS_H
