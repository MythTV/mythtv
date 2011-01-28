#ifndef MYTH_MEDIA_H
#define MYTH_MEDIA_H

#include <QObject>
#include <QMap>
#include <QString>
#include <QEvent>
#include <QPointer>

#include "mythexp.h"

typedef enum {
    MEDIASTAT_ERROR,        ///< Unable to mount, but could be usable
    MEDIASTAT_UNKNOWN,
    MEDIASTAT_UNPLUGGED,
    MEDIASTAT_OPEN,         ///< CD/DVD tray open (meaningless for non-CDs?)
    MEDIASTAT_NODISK,       ///< CD/DVD tray closed but empty, device unusable
    MEDIASTAT_UNFORMATTED,  ///< For devices/media a plugin might erase/format
    MEDIASTAT_USEABLE,
    MEDIASTAT_NOTMOUNTED,
    MEDIASTAT_MOUNTED
} MediaStatus;

typedef enum {
    MEDIATYPE_UNKNOWN  = 0x0001,
    MEDIATYPE_DATA     = 0x0002,
    MEDIATYPE_MIXED    = 0x0004,
    MEDIATYPE_AUDIO    = 0x0008,
    MEDIATYPE_DVD      = 0x0010,
    MEDIATYPE_VCD      = 0x0020,
    MEDIATYPE_MMUSIC   = 0x0040,
    MEDIATYPE_MVIDEO   = 0x0080,
    MEDIATYPE_MGALLERY = 0x0100,
    MEDIATYPE_BD       = 0x0200,
    MEDIATYPE_END      = 0x0400,
} MediaType;

typedef enum {
    MEDIAERR_OK,
    MEDIAERR_FAILED,
    MEDIAERR_UNSUPPORTED
} MediaError;

typedef QMap<QString,uint> ext_cnt_t;
typedef QMap<QString,uint> ext_to_media_t;

class MPUBLIC MythMediaDevice : public QObject
{
    Q_OBJECT
    friend class MediaMonitorDarwin;   // So these can call setStatus(),
    friend class MonitorThreadDarwin;  // and trigger posting of MediaEvents

 public:
    MythMediaDevice(QObject* par, const char* DevicePath, bool SuperMount,
                    bool AllowEject);

    const QString& getMountPath() const { return m_MountPath; }
    void setMountPath(const char *path) { m_MountPath = path; }

    const QString& getDevicePath() const { return m_DevicePath; }

    const QString& getRealDevice() const
    { return m_RealDevice.length() ? m_RealDevice : m_DevicePath; }


    const QString& getDeviceModel() const  { return m_DeviceModel;  }
    void setDeviceModel(const char *model) { m_DeviceModel = model; }

    MediaStatus getStatus() const { return m_Status; }

    const QString& getVolumeID() const { return m_VolumeID; }
    void  setVolumeID(const char *vol)  { m_VolumeID = vol; }

    const QString& getKeyID() const { return m_KeyID; }

    bool getAllowEject() const { return m_AllowEject; }

    bool getLocked() const { return m_Locked; }

    bool isDeviceOpen() const;

    /// Is this device "ready", for a plugin to access?
    bool isUsable() const
    {
        return m_Status == MEDIASTAT_USEABLE
            || m_Status == MEDIASTAT_MOUNTED
            || m_Status == MEDIASTAT_NOTMOUNTED;
    }

    MediaType getMediaType() const { return m_MediaType; }

    bool isSuperMount()      const { return m_SuperMount; }

    virtual MediaError  testMedia() { return MEDIAERR_UNSUPPORTED; }
    virtual bool openDevice();
    virtual bool closeDevice();
    virtual bool isSameDevice(const QString &path);
    virtual void setSpeed(int speed);
    virtual MediaStatus checkMedia() = 0;// Derived classes MUST implement this.
    virtual MediaError eject(bool open_close = true);
    virtual MediaError lock();
    virtual MediaError unlock();
    virtual bool performMountCmd( bool DoMount );

    bool mount()   { return performMountCmd(true);  }
    bool unmount() { return performMountCmd(false); }

    bool isMounted(bool bVerify = true);
    bool findMountPath();

    void RegisterMediaExtensions(uint mediatype,
                                 const QString& extensions);


    static const char* MediaStatusStrings[];
    static const char* MediaErrorStrings[];

    void clearData();

    const char* MediaTypeString();

    static const char* MediaTypeString(MediaType type);

 signals:
    void statusChanged(MediaStatus oldStatus, MythMediaDevice* pMedia);

 protected:
    virtual ~MythMediaDevice() {} // use deleteLater...

    /// Override this to perform any post mount logic.
    virtual void onDeviceMounted(void)
    {
        MediaType type = DetectMediaType();
        if (type != MEDIATYPE_UNKNOWN)
            m_MediaType = type;
    }

    /// Override this to perform any post unmount logic.
    virtual void onDeviceUnmounted() {};

    MediaType DetectMediaType(void);
    bool ScanMediaType(const QString &directory, ext_cnt_t &counts);

    MediaStatus setStatus(MediaStatus newStat, bool CloseIt=false);

    QString m_DeviceModel;   ///< The device Manufacturer/Model. Read/write
    QString m_DevicePath;    ///< The path to this media's device.
                             ///  (e.g. /dev/cdrom) Read only
    QString m_KeyID;         ///< KeyID of the media. Read only
                             ///  (For ISO9660, volumeid + creation_date)
    QString m_MountPath;     ///< The path to this media's mount point.
                             ///  (e.g. /mnt/cdrom) Read/write
    QString m_RealDevice;    ///< If m_DevicePath is a symlink, its target.
                             ///  (e.g. /dev/hdc) Read only
    QString m_VolumeID;      ///< The volume ID of the media. Read/write

    MediaStatus m_Status;    ///< The status of the media as of the
                             ///  last call to checkMedia. Read only
    MediaType   m_MediaType; ///< The type of media. Read only

    bool m_AllowEject;       ///< Allow the user to eject the media?. Read only
    bool m_Locked;           ///< Is this media locked?.              Read only
    bool m_SuperMount;       ///< Is this a supermount device?.       Read only

    int  m_DeviceHandle;     ///< A file handle for opening and closing
                             ///  the device, ioctls(), et c. This should
                             ///  be private, but a subclass of a
                             ///  subclass needs it (MythCDRomLinux)
 private:
    ext_to_media_t m_ext_to_media; ///< Map of extension to media type.
};

class MPUBLIC MediaEvent : public QEvent
{
  public:
    MediaEvent(MediaStatus oldStatus, MythMediaDevice *pDevice) :
        QEvent(kEventType), m_OldStatus(oldStatus), m_Device(pDevice) {}

    MediaStatus getOldStatus(void) const { return m_OldStatus; }
    MythMediaDevice* getDevice(void) { return m_Device; }

    static Type kEventType;

  protected:
    MediaStatus m_OldStatus;
    QPointer<MythMediaDevice> m_Device;
};

#endif
