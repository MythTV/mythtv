#ifndef MYTH_MEDIA_MONITOR_H
#define MYTH_MEDIA_MONITOR_H

#include "mythmedia.h"
#include <qvaluelist.h>
#include <qguardedptr.h>
#include <qthread.h>
#include <fstab.h>

const int kMediaEventType = 30042;

class MediaMonitor;

MediaMonitor* getMediaMonitor();

class MediaEvent : public QCustomEvent
{
  public:
    MediaEvent(MediaStatus oldStatus, MythMediaDevice* pDevice) 
         : QCustomEvent(kMediaEventType) 
           { m_OldStatus = oldStatus; m_Device = pDevice;}

    MediaStatus getOldStatus(void) const { return m_OldStatus; }
    MythMediaDevice* getDevice(void) { return m_Device; }

  protected:
    MediaStatus m_OldStatus;
    QGuardedPtr<MythMediaDevice> m_Device;
};

struct mntent;

class MonitorThread : public QThread
{
  public:
    MonitorThread( MediaMonitor* pMon,  unsigned long interval);
    void setMonitor(MediaMonitor* pMon) { m_Monitor = pMon; }
    virtual void run(void);

  protected:
    QGuardedPtr<MediaMonitor> m_Monitor;
    unsigned long m_Interval;
};

class MediaMonitor;

class MediaMonitor : public QObject
{
    Q_OBJECT
  public:
    bool addFSTab(void);
    void addDevice(MythMediaDevice* pDevice);
    bool addDevice(const char* dev);
    bool addDevice(struct fstab* mep);

    bool isActive(void) const { return m_Active; }
    void checkDevices(void);
    void startMonitoring(void);
    void stopMonitoring(void);
    QValueList <MythMediaDevice*> getMedias(MediaType mediatype);
    
    static MediaMonitor* getMediaMonitor();
    
  public slots:
    void mediaStatusChanged( MediaStatus oldStatus, MythMediaDevice* pMedia);

  protected:
    MediaMonitor(QObject* par, unsigned long interval, bool allowEject);
    QValueList<MythMediaDevice*> m_Devices;
    bool m_Active;
    MonitorThread m_Thread;
    bool m_AllowEject;
};

#endif
