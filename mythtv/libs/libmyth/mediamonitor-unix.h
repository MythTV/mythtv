#ifndef MYTH_MEDIA_MONITOR_UNIX_H
#define MYTH_MEDIA_MONITOR_UNIX_H

class MediaMonitorUnix : public MediaMonitor
{
  public:
    MediaMonitorUnix(QObject* par, unsigned long interval, bool allowEject);
    ~MediaMonitorUnix();

  protected:
    void CheckDevices(void);
    void CheckDeviceNotifications(void);
    bool CheckFileSystemTable(void);
    bool CheckMountable(void);
    bool FindPartitions(const QString &dev, bool checkPartitions);

    virtual bool AddDevice(MythMediaDevice* pDevice);
    bool AddDevice(const char* dev);
    bool AddDevice(struct fstab* mep);

    QString GetDeviceFile(const QString &sysfs);

    QStringList GetCDROMBlockDevices(void);

  protected:
    int                          m_fifo;
    static const QString         kUDEV_FIFO;
};

#endif // MYTH_MEDIA_MONITOR_H
