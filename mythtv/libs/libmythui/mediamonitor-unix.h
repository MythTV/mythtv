#ifndef MYTH_MEDIA_MONITOR_UNIX_H
#define MYTH_MEDIA_MONITOR_UNIX_H

#define DEFAULT_DVD "/dev/dvd"
#define DEFAULT_CD  "/dev/cdrom"

#include "libmythbase/mythconfig.h"

#include <QString>
#if CONFIG_QTDBUS
#include <QDBusObjectPath>
#endif

#include "mediamonitor.h"

class MediaMonitorUnix : public MediaMonitor
{
#if CONFIG_QTDBUS
    Q_OBJECT
  public slots:
    Q_NOREPLY void deviceAdded(const QDBusObjectPath& o,
                               const QMap<QString, QVariant> &interfaces);
    Q_NOREPLY void deviceRemoved(const QDBusObjectPath& o,
                                 const QStringList &interfaces);
#endif

  public:
    MediaMonitorUnix(QObject *par, unsigned long interval, bool allowEject);
#if !CONFIG_QTDBUS
    void deleteLater(void) override; // MediaMonitor
#endif

  protected:
    ~MediaMonitorUnix() override = default;

#if !CONFIG_QTDBUS
    void CheckDeviceNotifications(void) override; // MediaMonitor
#endif
    bool CheckFileSystemTable(void);
    bool CheckMountable(void);
#if !CONFIG_QTDBUS
    static bool CheckRemovable(const QString &dev);
    bool FindPartitions(const QString &dev, bool checkPartitions);
#endif

    bool AddDevice(MythMediaDevice* pDevice) override; // MediaMonitor
    bool AddDevice(struct fstab* mep);

#if !CONFIG_QTDBUS
    static QString GetDeviceFile(const QString &sysfs);
#endif

    QStringList GetCDROMBlockDevices(void) override; // MediaMonitor

  protected:
    int                          m_fifo {-1};
    static constexpr const char *kUDEV_FIFO { "/tmp/mythtv_media" };
;
};

#if CONFIG_QTDBUS
    enum MythUdisksDevice
    {
        UDisks2INVALID = 0, 
        UDisks2DVD     = 1,
        UDisks2HDD     = 2,
    };
#endif
#endif // MYTH_MEDIA_MONITOR_H
