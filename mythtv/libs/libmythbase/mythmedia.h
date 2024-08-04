#ifndef MYTH_MEDIA_H
#define MYTH_MEDIA_H

#include <QObject>
#include <QMap>
#include <QString>
#include <QEvent>
#include <QPointer>

#include "mythbaseexp.h"

enum MythMediaStatus : std::uint8_t {
    MEDIASTAT_ERROR,        ///< Unable to mount, but could be usable
    MEDIASTAT_UNKNOWN,
    MEDIASTAT_UNPLUGGED,
    MEDIASTAT_OPEN,         ///< CD/DVD tray open (meaningless for non-CDs?)
    MEDIASTAT_NODISK,       ///< CD/DVD tray closed but empty, device unusable
    MEDIASTAT_UNFORMATTED,  ///< For devices/media a plugin might erase/format
    MEDIASTAT_USEABLE,
    MEDIASTAT_NOTMOUNTED,
    MEDIASTAT_MOUNTED
};

enum MythMediaType : std::uint16_t {
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
};
// MediaType conflicts with a definition within OSX Quicktime Libraries.

enum MythMediaError : std::uint8_t {
    MEDIAERR_OK,
    MEDIAERR_FAILED,
    MEDIAERR_UNSUPPORTED
};

using ext_cnt_t = QMap<QString,uint>;
using ext_to_media_t = QMap<QString,uint>;

class MBASE_PUBLIC MythMediaDevice : public QObject
{
    Q_OBJECT
    friend class MediaMonitorDarwin;   // So these can call setStatus(),
    friend class MonitorThreadDarwin;  // and trigger posting of MythMediaEvents

 public:
    MythMediaDevice(QObject* par, QString DevicePath, bool SuperMount,
                    bool AllowEject);

    const QString& getMountPath() const { return m_mountPath; }
    void setMountPath(const char *path) { m_mountPath = path; }

    const QString& getDevicePath() const { return m_devicePath; }

    const QString& getRealDevice() const
    { return !m_realDevice.isEmpty() ? m_realDevice : m_devicePath; }


    const QString& getDeviceModel() const  { return m_deviceModel;  }
    void setDeviceModel(const char *model) { m_deviceModel = model; }

    MythMediaStatus getStatus() const { return m_status; }

    const QString& getVolumeID() const { return m_volumeID; }
    void  setVolumeID(const char *vol)  { m_volumeID = vol; }

    const QString& getKeyID() const { return m_keyID; }

    bool getAllowEject() const { return m_allowEject; }

    bool getLocked() const { return m_locked; }

    bool isDeviceOpen() const;

    /// Is this device "ready", for a plugin to access?
    bool isUsable() const
    {
        return m_status == MEDIASTAT_USEABLE
            || m_status == MEDIASTAT_MOUNTED
            || m_status == MEDIASTAT_NOTMOUNTED;
    }

    MythMediaType getMediaType() const { return m_mediaType; }

    bool isSuperMount()      const { return m_superMount; }

    virtual MythMediaError  testMedia() { return MEDIAERR_UNSUPPORTED; }
    virtual bool openDevice();
    virtual bool closeDevice();
    virtual bool isSameDevice(const QString &path);
    virtual void setSpeed(int speed);
    virtual void setDeviceSpeed(const char * /*devicePath*/, int /*speed*/) { }
    virtual MythMediaStatus checkMedia() = 0;// Derived classes MUST implement this.
    virtual MythMediaError eject(bool open_close = true);
    virtual MythMediaError lock();
    virtual MythMediaError unlock();
    virtual bool performMountCmd( bool DoMount );

    bool mount()   { return performMountCmd(true);  }
    bool unmount() { return performMountCmd(false); }

    bool isMounted(bool bVerify = true);
    bool findMountPath();

    static void RegisterMediaExtensions(uint mediatype,
                                 const QString& extensions);


    static const std::array<const QString,9> kMediaStatusStrings;
    static const std::array<const QString,3> kMediaErrorStrings;

    void clearData();

    QString MediaTypeString();

    static QString MediaTypeString(uint type);

 signals:
    void statusChanged(MythMediaStatus oldStatus, MythMediaDevice* pMedia);

 protected:
    ~MythMediaDevice() override = default; // use deleteLater...

    /// Override this to perform any post mount logic.
    virtual void onDeviceMounted(void)
    {
        MythMediaType type = DetectMediaType();
        if (type != MEDIATYPE_UNKNOWN)
            m_mediaType = type;
    }

    /// Override this to perform any post unmount logic.
    virtual void onDeviceUnmounted() {};

    MythMediaType DetectMediaType(void);
    bool ScanMediaType(const QString &directory, ext_cnt_t &cnt);

    MythMediaStatus setStatus(MythMediaStatus newStat, bool CloseIt=false);

    QString m_deviceModel;   ///< The device Manufacturer/Model. Read/write
    QString m_devicePath;    ///< The path to this media's device.
                             ///  (e.g. /dev/cdrom) Read only
    QString m_keyID;         ///< KeyID of the media. Read only
                             ///  (For ISO9660, volumeid + creation_date)
    QString m_mountPath;     ///< The path to this media's mount point.
                             ///  (e.g. /mnt/cdrom) Read/write
    QString m_realDevice;    ///< If m_devicePath is a symlink, its target.
                             ///  (e.g. /dev/hdc) Read only
    QString m_volumeID;      ///< The volume ID of the media. Read/write

    MythMediaStatus m_status {MEDIASTAT_UNKNOWN};
                             ///< The status of the media as of the
                             ///  last call to checkMedia. Read only
    MythMediaType   m_mediaType {MEDIATYPE_UNKNOWN};
                             ///< The type of media. Read only

    bool m_allowEject;       ///< Allow the user to eject the media?. Read only
    bool m_locked {false};   ///< Is this media locked?.              Read only

    bool m_superMount;       ///< Is this a supermount device?.       Read only
                             ///  The OS handles mounting/unmounting of
                             ///  'supermount' devices.  Myth only need to give
                             ///  derived classes a chance to perform their
                             ///  mount/unmount logic.


    int  m_deviceHandle {-1};///< A file handle for opening and closing
                             ///  the device, ioctls(), et c. This should
                             ///  be private, but a subclass of a
                             ///  subclass needs it (MythCDRomLinux)
 private:
    static ext_to_media_t s_ext_to_media; ///< Map of extension to media type.
};

class MBASE_PUBLIC MythMediaEvent : public QEvent
{
  public:
    MythMediaEvent(MythMediaStatus oldStatus, MythMediaDevice *pDevice) :
        QEvent(kEventType), m_oldStatus(oldStatus), m_device(pDevice) {}
    ~MythMediaEvent() override;

    MythMediaStatus getOldStatus(void) const { return m_oldStatus; }
    MythMediaDevice* getDevice(void) { return m_device.isNull() ? nullptr : m_device.data(); }

    static const Type kEventType;

  protected:
    MythMediaStatus m_oldStatus;
    QPointer<MythMediaDevice> m_device;
};

#endif
