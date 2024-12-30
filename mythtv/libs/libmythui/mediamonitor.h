#ifndef MYTH_MEDIA_MONITOR_H
#define MYTH_MEDIA_MONITOR_H

#include <QStringList>
#include <QPointer>
#include <QRecursiveMutex>
#include <QWaitCondition>
#include <QList>
#include <QDateTime>

#include "libmythbase/mthread.h"
#include "libmythbase/mythmedia.h"
#include "libmythui/mythuiexp.h"

/// Stores details of media handlers

// Adding member initializers caused compilation to fail with an error
// that it cannot convert a brace-enclosed initializer list to MHData.
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
struct MHData
{
    void   (*callback)(MythMediaDevice *mediadevice);
    int      MythMediaType;
    QString  destination;
    QString  description;
};

class MediaMonitor;
class MonitorThread : public MThread
{
  public:
    MonitorThread(MediaMonitor* pMon,  unsigned long interval) :
        MThread("Monitor"), m_monitor(pMon), m_interval(interval),
        m_lastCheckTime(QDateTime::currentDateTimeUtc()) {}
    ~MonitorThread() override { wait(); m_monitor = nullptr; }
    void setMonitor(MediaMonitor* pMon) { m_monitor = pMon; }
    void run(void) override; // MThread

  protected:
    QPointer<MediaMonitor> m_monitor;
    unsigned long m_interval;
    QDateTime m_lastCheckTime;
};

class MUI_PUBLIC MediaMonitor : public QObject
{
    Q_OBJECT
    friend class MonitorThread;
    friend class MonitorThreadDarwin;

  public:
    virtual void deleteLater(void);
    bool IsActive(void) const { return m_active; }

    virtual void StartMonitoring(void);
    void StopMonitoring(void);
    void ChooseAndEjectMedia(void);
    void EjectMedia(const QString &path);

    static MediaMonitor *GetMediaMonitor(void);
    static QString GetMountPath(const QString& devPath);
    static void SetCDSpeed(const char *device, int speed);

    bool ValidateAndLock(MythMediaDevice *pMedia);
    void Unlock(MythMediaDevice *pMedia);

    // To safely dereference the pointers returned by this function
    // first validate the pointer with ValidateAndLock(), if true is returned
    // it is safe to dereference the pointer. When finished call Unlock()
    QList<MythMediaDevice*> GetRemovable(bool showMounted = false,
                                         bool showUsable = false);
    QList<MythMediaDevice*> GetMedias(unsigned mediatypes);
    MythMediaDevice*        GetMedia(const QString &path);

    void RegisterMediaHandler(const QString  &destination,
                              const QString  &description,
                              void          (*callback) (MythMediaDevice*),
                              int             mediaType,
                              const QString  &extensions);
    void JumpToMediaHandler(MythMediaDevice*  pMedia);

    // Plugins should use these if they need to access optical disks:
    static QString defaultCDdevice();
    static QString defaultVCDdevice();
    static QString defaultDVDdevice();
    static QString defaultCDWriter();
    static QString defaultDVDWriter();

    static void ejectOpticalDisc(void);

    virtual QStringList GetCDROMBlockDevices(void) = 0;

  public slots:
    void mediaStatusChanged(MythMediaStatus oldStatus, MythMediaDevice* pMedia) const;

  protected:
    MediaMonitor(QObject *par, unsigned long interval, bool allowEject);
    ~MediaMonitor() override = default;

    static void AttemptEject(MythMediaDevice *device);
    void CheckDevices(void);
    virtual void CheckDeviceNotifications(void) {};
    virtual bool AddDevice(MythMediaDevice* pDevice) = 0;
    bool RemoveDevice(const QString &dev);
    bool shouldIgnore(const MythMediaDevice *device);
    bool eventFilter(QObject *obj, QEvent *event) override; // QObject

    QString listDevices(void);

    static QString defaultDevice(const QString &setting,
                                 const QString &label,
                                 const char *hardCodedDefault);
    MythMediaDevice *selectDrivePopup(const QString &label,
                                      bool showMounted = false,
                                      bool showUsable = false);

  protected:
    QRecursiveMutex              m_devicesLock;
    QList<MythMediaDevice*>      m_devices;
    QList<MythMediaDevice*>      m_removedDevices;
    QMap<MythMediaDevice*, int>  m_useCount;

    // List of devices/mountpoints that the user doesn't want to monitor:
    QStringList                  m_ignoreList;

    bool volatile                m_active {false};      ///< Was MonitorThread started?
    QWaitCondition               m_wait;
    MonitorThread               *m_thread {nullptr};
    unsigned long                m_monitorPollingInterval;
    bool                         m_allowEject;

    QMap<QString, MHData>        m_handlerMap;  ///< Registered Media Handlers

    static MediaMonitor         *s_monitor;
};

static inline void
REG_MEDIA_HANDLER (const QString&  destination,
                   const QString&  description,
                   void          (*callback)(MythMediaDevice*),
                   int             mediaType,
                   const QString&  extensions)
{
    MediaMonitor::GetMediaMonitor()->RegisterMediaHandler(destination,  description,
                                                          callback, mediaType, extensions);
}


#endif // MYTH_MEDIA_MONITOR_H
