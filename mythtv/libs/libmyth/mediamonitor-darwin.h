#ifndef MYTH_MEDIA_MONITOR_DARWIN_H
#define MYTH_MEDIA_MONITOR_DARWIN_H

class MonitorThreadDarwin : public MonitorThread
{
  public:
    MonitorThreadDarwin(MediaMonitor* pMon,  unsigned long interval)
        : MonitorThread(pMon, interval) {};

    virtual void run(void);

    void  diskInsert(const char *devName,
                     const char *volName, bool isCDorDVD = 1);
    void  diskRemove(QString devName);
};

class MediaMonitorDarwin : public MediaMonitor
{
  public:
    MediaMonitorDarwin(QObject* par, unsigned long interval, bool allowEject)
        : MediaMonitor(par, interval, allowEject) {};

    virtual void StartMonitoring(void);
    virtual bool AddDevice(MythMediaDevice* pDevice);
};
#endif // MYTH_MEDIA_MONITOR_DARWIN_H
