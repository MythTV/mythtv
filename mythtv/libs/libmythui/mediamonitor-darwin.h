#ifndef MYTH_MEDIA_MONITOR_DARWIN_H
#define MYTH_MEDIA_MONITOR_DARWIN_H

#define DEFAULT_DVD "disk1"
#define DEFAULT_CD  "disk1"

class MonitorThreadDarwin : public MonitorThread
{
  public:
    MonitorThreadDarwin(MediaMonitor* pMon,  unsigned long interval)
        : MonitorThread(pMon, interval) {};

    void run(void) override; // MThread

    void  diskInsert(const char *devName,
                     const char *volName, QString model, bool isCDorDVD = 1);
    void  diskRemove(QString devName);
    void  diskRename(const char *devName, const char *volName);
};

class MediaMonitorDarwin : public MediaMonitor
{
  public:
    MediaMonitorDarwin(QObject* par, unsigned long interval, bool allowEject)
        : MediaMonitor(par, interval, allowEject) {};

    void StartMonitoring(void) override; // MediaMonitor
    bool AddDevice(MythMediaDevice* pDevice) override; // MediaMonitor
    QStringList GetCDROMBlockDevices(void) override; // MediaMonitor
};
#endif // MYTH_MEDIA_MONITOR_DARWIN_H
