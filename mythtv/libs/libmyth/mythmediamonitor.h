#ifndef MYTH_MEDIA_MONITOR_H
#define MYTH_MEDIA_MONITOR_H

#include <fstab.h>

#include <qvaluelist.h>
#include <qguardedptr.h>
#include <qthread.h>
#include <qstring.h>

#include "mythmedia.h"

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
    friend class MonitorThread;

  protected:
    MediaMonitor(QObject* par, unsigned long interval, bool allowEject);

  public:
    ~MediaMonitor();

    bool IsActive(void) const { return m_Active; }

    void StartMonitoring(void);
    void StopMonitoring(void);
    void ChooseAndEjectMedia(void);

    static MediaMonitor *GetMediaMonitor(void);

    // this is not safe.. device could get deleted...
    QValueList <MythMediaDevice*> GetMedias(MediaType mediatype);

  public slots:
    void mediaStatusChanged(MediaStatus oldStatus, MythMediaDevice* pMedia);

  protected:
    void CheckDevices(void);
    void CheckDeviceNotifications(void);
    bool CheckFileSystemTable(void);
    bool CheckMountable(void);
    bool FindPartitions(const QString &dev, bool checkPartitions);

    void AddDevice(MythMediaDevice* pDevice);
    bool AddDevice(const char* dev);
    bool AddDevice(struct fstab* mep);
    bool RemoveDevice(const QString &dev);

    QString GetDeviceFile(const QString &sysfs);

    static QStringList GetCDROMBlockDevices(void);

  protected:
    QValueList<MythMediaDevice*> m_Devices;
    bool                         m_Active;
    MonitorThread                m_Thread;
    bool                         m_AllowEject;
    int                          m_fifo;

    static const QString         kUDEV_FIFO;
    static MediaMonitor         *c_monitor;
};

#endif
