#ifndef MYTH_MEDIA_MONITOR_H
#define MYTH_MEDIA_MONITOR_H

#include <qvaluelist.h>
#include <qguardedptr.h>
#include <qthread.h>
#include <qstring.h>

#include "mythmedia.h"

const int kMediaEventType = 30042;

class MPUBLIC MediaEvent : public QCustomEvent
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

class MediaMonitor;
class MonitorThread : public QThread
{
  public:
    MonitorThread(MediaMonitor* pMon,  unsigned long interval);
    void setMonitor(MediaMonitor* pMon) { m_Monitor = pMon; }
    virtual void run(void);

  protected:
    QGuardedPtr<MediaMonitor> m_Monitor;
    unsigned long m_Interval;
};

class MPUBLIC MediaMonitor : public QObject
{
    Q_OBJECT
    friend class MonitorThread;
    friend class MonitorThreadDarwin;

  protected:
    MediaMonitor(QObject* par, unsigned long interval, bool allowEject);

  public:
    ~MediaMonitor();

    bool IsActive(void) const { return m_Active; }

    virtual void StartMonitoring(void);
    void StopMonitoring(void);
    void ChooseAndEjectMedia(void);

    static MediaMonitor *GetMediaMonitor(void);
    static QString GetMountPath(const QString& devPath);
    static void SetCDSpeed(const char *device, int speed);

    bool ValidateAndLock(MythMediaDevice *pMedia);
    void Unlock(MythMediaDevice *pMedia);

    // To safely dereference the pointers returned by this function
    // first validate the pointer with ValidateAndLock(), if true is returned
    // it is safe to dereference the pointer. When finished call Unlock()
    QValueList<MythMediaDevice*> GetMedias(MediaType mediatype);
    MythMediaDevice* GetMedia(const QString &path);

    void MonitorRegisterExtensions(uint mediaType, const QString &extensions);

    // Plugins should use these if they need to access optical disks:
    static QString defaultCDdevice();
    static QString defaultVCDdevice();
    static QString defaultDVDdevice();
    static QString defaultWriter();

    virtual QStringList GetCDROMBlockDevices(void) = 0;

  public slots:
    void mediaStatusChanged(MediaStatus oldStatus, MythMediaDevice* pMedia);

  protected:
    void CheckDevices(void);
    virtual void CheckDeviceNotifications(void) {};
    virtual bool AddDevice(MythMediaDevice* pDevice) = 0;
    bool RemoveDevice(const QString &dev);
    bool shouldIgnore(MythMediaDevice *device);

    const QString listDevices(void);

    static QString defaultDevice(const QString setting,
                                 const QString label,  char *hardCodedDefault);
    MythMediaDevice *selectDrivePopup(const QString label, bool mounted=false);

  protected:
    QMutex                       m_DevicesLock;
    QValueList<MythMediaDevice*> m_Devices;
    QValueList<MythMediaDevice*> m_RemovedDevices;
    QMap<MythMediaDevice*, int>  m_UseCount;

    // List of devices/mountpoints that the user doesn't want to monitor:
    QStringList                  m_IgnoreList;

    bool                         m_Active;      ///< Was MonitorThread started?
    bool                         m_SendEvent;   ///< Send MediaEvent to plugins?
    bool                         m_StartThread; ///< Should we actually monitor?

    MonitorThread                *m_Thread;
    unsigned long                m_MonitorPollingInterval;
    bool                         m_AllowEject;

    static MediaMonitor         *c_monitor;
};

#endif // MYTH_MEDIA_MONITOR_H
