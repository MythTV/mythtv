#ifndef MYTH_MEDIA_MONITOR_UNIX_H
#define MYTH_MEDIA_MONITOR_UNIX_H

#define DEFAULT_DVD "/dev/dvd"
#define DEFAULT_CD  "/dev/cdrom"

#include "config.h"

#include <QString>
#if CONFIG_QTDBUS
#include <QtDBus>
#endif

#include "mythmediamonitor.h"

class MediaMonitorUnix : public MediaMonitor
{
#if CONFIG_QTDBUS
    Q_OBJECT
  public slots:
    Q_NOREPLY void deviceAdded(QDBusObjectPath);
    Q_NOREPLY void deviceRemoved(QDBusObjectPath);
#endif

  public:
    MediaMonitorUnix(QObject *par, unsigned long interval, bool allowEject);
#if !CONFIG_QTDBUS
    virtual void deleteLater(void);
#endif

  protected:
    ~MediaMonitorUnix() {}

#if !CONFIG_QTDBUS
    virtual void CheckDeviceNotifications(void);
#endif
    bool CheckFileSystemTable(void);
    bool CheckMountable(void);
#if !CONFIG_QTDBUS
    bool CheckRemovable(const QString &dev);
    bool FindPartitions(const QString &dev, bool checkPartitions);
#endif

    virtual bool AddDevice(MythMediaDevice* pDevice);
    bool AddDevice(struct fstab* mep);

#if !CONFIG_QTDBUS
    QString GetDeviceFile(const QString &sysfs);
#endif

    virtual QStringList GetCDROMBlockDevices(void);

  protected:
    int                          m_fifo;
    static const char           *kUDEV_FIFO;
};

#endif // MYTH_MEDIA_MONITOR_H
