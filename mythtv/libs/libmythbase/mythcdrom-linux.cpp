#include <cerrno>
#include <climits>
#include <cstdint>
#include <fcntl.h>
#include <linux/cdrom.h>     // old ioctls for cdrom
#include <linux/fs.h>        // BLKRRPART
#include <linux/iso_fs.h>
#include <scsi/scsi.h>
#include <scsi/sg.h>
#include <sys/ioctl.h>       // ioctls
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <QtGlobal>
#include <QDateTime>

#include "mythcdrom.h"
#include "mythcdrom-linux.h"
#include "mythlogging.h"
#include "mythdate.h"

#define LOC     QString("MythCDROMLinux:")

// On a mixed-mode disc (audio+data), set this to 0 to mount the data portion:
#ifndef ASSUME_WANT_AUDIO
#define ASSUME_WANT_AUDIO 1 // NOLINT(cppcoreguidelines-macro-usage)
#endif


// Some features cannot be detected (reliably) using the standard
// Linux ioctl()s, so we use some direct low-level device queries.

using CDROMgenericCmd = struct cdrom_generic_command;

// Some structures stolen from the __KERNEL__ section of linux/cdrom.h.

// Prevent clang-tidy modernize-avoid-c-arrays warnings in these
// kernel structures
extern "C" {

// This contains the result of a GPCMD_GET_EVENT_STATUS_NOTIFICATION.
// It is the joining of a struct event_header and a struct media_event_desc
struct CDROMeventStatus
{
    uint16_t m_dataLen[2];
#if (Q_BYTE_ORDER == Q_BIG_ENDIAN)
    uint8_t  m_nea               : 1;
    uint8_t  m_reserved1         : 4;
    uint8_t  m_notificationClass : 3;
#else
    uint8_t  m_notificationClass : 3;
    uint8_t  m_reserved1         : 4;
    uint8_t  m_nea               : 1;
#endif
    uint8_t  m_suppEventClass;
#if (Q_BYTE_ORDER == Q_BIG_ENDIAN)
    uint8_t  m_reserved2         : 4;
    uint8_t  m_mediaEventCode    : 4;
    uint8_t  m_reserved3         : 6;
    uint8_t  m_mediaPresent      : 1;
    uint8_t  m_doorOpen          : 1;
#else
    uint8_t  m_mediaEventCode    : 4;
    uint8_t  m_reserved2         : 4;
    uint8_t  m_doorOpen          : 1;
    uint8_t  m_mediaPresent      : 1;
    uint8_t  m_reserved3         : 6;
#endif
    uint8_t  m_startSlot;
    uint8_t  m_endSlot;
};

// and this is returned by GPCMD_READ_DISC_INFO
struct CDROMdiscInfo {
    uint16_t m_discInformationLength;
#if (Q_BYTE_ORDER == Q_BIG_ENDIAN)
    uint8_t  m_reserved1         : 3;
    uint8_t  m_erasable          : 1;
    uint8_t  m_borderStatus      : 2;
    uint8_t  m_discStatus        : 2;
#else
    uint8_t  m_discStatus        : 2;
    uint8_t  m_borderStatus      : 2;
    uint8_t  m_erasable          : 1;
    uint8_t  m_reserved1         : 3;
#endif
    uint8_t  m_nFirstTrack;
    uint8_t  m_nSessionsLsb;
    uint8_t  m_firstTrackLsb;
    uint8_t  m_lastTrackLsb;
#if (Q_BYTE_ORDER == Q_BIG_ENDIAN)
    uint8_t  m_didV              : 1;
    uint8_t  m_dbcV              : 1;
    uint8_t  m_uru               : 1;
    uint8_t  m_reserved2         : 5;
#else
    uint8_t  m_reserved2         : 5;
    uint8_t  m_uru               : 1;
    uint8_t  m_dbcV              : 1;
    uint8_t  m_didV              : 1;
#endif
    uint8_t  m_discType;
    uint8_t  m_nSessionsMsb;
    uint8_t  m_firstTrackMsb;
    uint8_t  m_lastTrackMsb;
    uint32_t m_discId;
    uint32_t m_leadIn;
    uint32_t m_leadOut;
    uint8_t  m_discBarCode[8];
    uint8_t  m_reserved3;
    uint8_t  m_nOpc;
};

// end of kernel structures.
};

enum CDROMdiscStatus : std::uint8_t
{
    MEDIA_IS_EMPTY      = 0x0,
    MEDIA_IS_APPENDABLE = 0x1,
    MEDIA_IS_COMPLETE   = 0x2,
    MEDIA_IS_OTHER      = 0x3
};


/** \class MythCDROMLinux
 *
 * Use Linux-specific ioctl()s to detect Audio-CDs,
 * changed media, open trays and blank writable media.
 */

class MythCDROMLinux: public MythCDROM
{
public:
    MythCDROMLinux(QObject* par, const QString& DevicePath, bool SuperMount,
                   bool AllowEject):
        MythCDROM(par, DevicePath, SuperMount, AllowEject) {
    }

    MythMediaError testMedia(void) override; // MythMediaDevice
    bool mediaChanged(void) override; // MythCDROM
    bool checkOK(void) override; // MythCDROM
    MythMediaStatus checkMedia(void) override; // MythMediaDevice
    MythMediaError eject(bool open_close = true) override; // MythMediaDevice
    void setDeviceSpeed(const char *device, int speed) override; // MythMediaDevice
    bool isSameDevice(const QString &path) override; // MythMediaDevice
    MythMediaError lock(void) override; // MythMediaDevice
    MythMediaError unlock(void) override; // MythMediaDevice

protected:
    MythMediaError ejectCDROM(bool open_close);
    MythMediaError ejectSCSI();

private:
    int driveStatus(void);
    bool hasWritableMedia(void);
    int SCSIstatus(void);
};

MythCDROM *GetMythCDROMLinux(QObject* par, const QString& devicePath,
                             bool SuperMount, bool AllowEject)
{
    return new MythCDROMLinux(par, devicePath, SuperMount, AllowEject);
}


/** \brief Exhaustively determine the status.
 *
 * If the CDROM is managed by the SCSI driver, then CDROM_DRIVE_STATUS
 * as reported by ioctl will always return CDS_TRAY_OPEN if the tray is
 * closed with no media.  To determine the actual drive status we need
 * to ask the drive directly by sending a SCSI packet to the drive.
 */

int MythCDROMLinux::driveStatus()
{
    int drive_status = ioctl(m_deviceHandle, CDROM_DRIVE_STATUS, CDSL_CURRENT);

    if (drive_status == -1)   // Very unlikely, but we should check
    {
        LOG(VB_MEDIA, LOG_ERR, LOC + ":driveStatus() - ioctl failed: " + ENO);
        return CDS_NO_INFO;
    }

    if (drive_status == CDS_TRAY_OPEN && m_devicePath.contains("/dev/scd"))
        return SCSIstatus();

    return drive_status;
}

/** \brief Is there blank or eraseable media in the drive?
 */

bool MythCDROMLinux::hasWritableMedia()
{
    CDROMdiscInfo    di {};
    CDROMgenericCmd  cgc {};

    cgc.cmd[0] = GPCMD_READ_DISC_INFO;
    cgc.cmd[8] = sizeof(CDROMdiscInfo);
    cgc.quiet  = 1;
    cgc.buffer = reinterpret_cast<uint8_t*>(&di);
    cgc.buflen = sizeof(CDROMdiscInfo);
    cgc.data_direction = CGC_DATA_READ;

    if (ioctl(m_deviceHandle, CDROM_SEND_PACKET, &cgc) < 0)
    {
        LOG(VB_MEDIA, LOG_ERR, LOC +
            ":hasWritableMedia() - failed to send packet to " + m_devicePath + ENO);
        return false;
    }

    switch (di.m_discStatus)
    {
        case MEDIA_IS_EMPTY:
            return true;

        case MEDIA_IS_APPENDABLE:
            // It is unlikely that any plugins will support multi-session
            // writing, so we treat it just like a finished disc:

        case MEDIA_IS_COMPLETE:
            return di.m_erasable;

        case MEDIA_IS_OTHER:
            ;
    }

    return false;
}

/** \brief Use a SCSI query packet to see if the drive is _really_ open.
 *
 * Note that in recent kernels, whether you have an IDE/ATA/SATA or SCSI CDROM,
 * the drive is managed by the SCSI driver and therefore needs this workaround.
 * This code is based on the routine cdrom_get_media_event
 * in cdrom.c of the linux kernel
 */

int MythCDROMLinux::SCSIstatus()
{
    CDROMeventStatus es {};
    CDROMgenericCmd  cgc {};

    cgc.cmd[0] = GPCMD_GET_EVENT_STATUS_NOTIFICATION;
    cgc.cmd[1] = 1;       // Tell us immediately
    cgc.cmd[4] = 1 << 4;  // notification class of media
    cgc.cmd[8] = sizeof(CDROMeventStatus);
    cgc.quiet  = 1;
    cgc.buffer = reinterpret_cast<uint8_t*>(&es);
    cgc.buflen = sizeof(CDROMeventStatus);
    cgc.data_direction = CGC_DATA_READ;

    if ((ioctl(m_deviceHandle, CDROM_SEND_PACKET, &cgc) < 0)
        || es.m_nea                          // drive does not support request
        || (es.m_notificationClass != 0x4))  // notification class mismatch
    {
        LOG(VB_MEDIA, LOG_ERR, LOC +
            ":SCSIstatus() - failed to send SCSI packet to " + m_devicePath + ENO);
        return CDS_TRAY_OPEN;
    }

    if (es.m_mediaPresent)
    {
        LOG(VB_MEDIA, LOG_DEBUG, LOC +
            ":SCSIstatus() - ioctl said tray was open, "
            "but drive is actually closed with a disc");
        return CDS_DISC_OK;
    }
    if (es.m_doorOpen)
    {
        LOG(VB_MEDIA, LOG_DEBUG, LOC +
            ":SCSIstatus() - tray is definitely open");
        return CDS_TRAY_OPEN;
    }

    LOG(VB_MEDIA, LOG_DEBUG, LOC + ":SCSIstatus() - ioctl said tray was open, "
                                   "but drive is actually closed with no disc");
    return CDS_NO_DISC;
}


MythMediaError MythCDROMLinux::eject(bool open_close)
{
    if (!isDeviceOpen())
    {
        if (!openDevice())
            return MEDIAERR_FAILED;
    }

    MythMediaError err = ejectCDROM(open_close);
    if (MEDIAERR_OK != err && open_close)
        err = ejectSCSI();

    return err;
}

MythMediaError MythCDROMLinux::ejectCDROM(bool open_close)
{
    if (open_close)
    {
        LOG(VB_MEDIA, LOG_DEBUG, LOC + ":eject - Ejecting CDROM");
        int res = ioctl(m_deviceHandle, CDROMEJECT);

        if (res < 0)
            LOG(VB_MEDIA, LOG_DEBUG, "CDROMEJECT ioctl failed" + ENO);

        return (res == 0) ? MEDIAERR_OK : MEDIAERR_FAILED;
    }

    LOG(VB_MEDIA, LOG_DEBUG, LOC + ":eject - Loading CDROM");
    // If the tray is empty, this will fail (Input/Output error)
    int res = ioctl(m_deviceHandle, CDROMCLOSETRAY);

    if (res < 0)
        LOG(VB_MEDIA, LOG_DEBUG, "CDROMCLOSETRAY ioctl failed" + ENO);

    // This allows us to catch any drives that the OS has problems
    // detecting the status of (some always report OPEN when empty)
    if (driveStatus() == CDS_TRAY_OPEN)
        return MEDIAERR_FAILED;
    return MEDIAERR_OK;
}

struct StHandle {
    const int m_fd;
    explicit StHandle(const char *dev) : m_fd(open(dev, O_RDWR | O_NONBLOCK)) { }
    ~StHandle() { close(m_fd); }
    operator int() const { return m_fd; } // NOLINT(google-explicit-constructor)
};

// This is copied from eject.c by Jeff Tranter (tranter@pobox.com)
MythMediaError MythCDROMLinux::ejectSCSI()
{
    int k = 0;
    sg_io_hdr_t io_hdr;
    std::array<uint8_t,6> allowRmBlk    {ALLOW_MEDIUM_REMOVAL, 0, 0, 0, 0, 0};
    std::array<uint8_t,6> startStop1Blk {START_STOP, 0, 0, 0, 1, 0}; // start
    std::array<uint8_t,6> startStop2Blk {START_STOP, 0, 0, 0, 2, 0}; // load eject
    std::array<uint8_t,16> sense_buffer {};
    const unsigned DID_OK = 0;
    const unsigned DRIVER_OK = 0;

    // ALLOW_MEDIUM_REMOVAL requires r/w access so re-open the device
    struct StHandle fd(qPrintable(m_devicePath));

    LOG(VB_MEDIA, LOG_DEBUG, LOC + ":ejectSCSI");
    if ((ioctl(fd, SG_GET_VERSION_NUM, &k) < 0) || (k < 30000))
    {
	    // not an sg device, or old sg driver
        LOG(VB_MEDIA, LOG_DEBUG, "SG_GET_VERSION_NUM ioctl failed" + ENO);
        return MEDIAERR_FAILED;
    }

    memset(&io_hdr, 0, sizeof(sg_io_hdr_t));
    io_hdr.interface_id = 'S';
    io_hdr.cmd_len = 6;
    io_hdr.mx_sb_len = sense_buffer.size();
    io_hdr.dxfer_direction = SG_DXFER_NONE;
    io_hdr.sbp = sense_buffer.data();
    io_hdr.timeout = 10000; // millisecs

    io_hdr.cmdp = allowRmBlk.data();
    if (ioctl(fd, SG_IO, &io_hdr) < 0)
    {
        LOG(VB_MEDIA, LOG_DEBUG, "SG_IO allowRmBlk ioctl failed" + ENO);
	    return MEDIAERR_FAILED;
    }
    if (io_hdr.host_status != DID_OK || io_hdr.driver_status != DRIVER_OK)
    {
        LOG(VB_MEDIA, LOG_DEBUG, "SG_IO allowRmBlk failed");
	    return MEDIAERR_FAILED;
    }

    io_hdr.cmdp = startStop1Blk.data();
    if (ioctl(fd, SG_IO, &io_hdr) < 0)
    {
        LOG(VB_MEDIA, LOG_DEBUG, "SG_IO START_STOP(start) ioctl failed" + ENO);
	    return MEDIAERR_FAILED;
    }
    if (io_hdr.host_status != DID_OK || io_hdr.driver_status != DRIVER_OK)
    {
        LOG(VB_MEDIA, LOG_DEBUG, "SG_IO START_STOP(start) failed");
	    return MEDIAERR_FAILED;
    }

    io_hdr.cmdp = startStop2Blk.data();
    if (ioctl(fd, SG_IO, &io_hdr) < 0)
    {
        LOG(VB_MEDIA, LOG_DEBUG, "SG_IO START_STOP(eject) ioctl failed" + ENO);
	    return MEDIAERR_FAILED;
    }
    if (io_hdr.host_status != DID_OK || io_hdr.driver_status != DRIVER_OK)
    {
        LOG(VB_MEDIA, LOG_DEBUG, "SG_IO START_STOP(eject) failed");
	    return MEDIAERR_FAILED;
    }

    /* force kernel to reread partition table when new disc inserted */
    (void)ioctl(fd, BLKRRPART);
    return MEDIAERR_OK;
}


bool MythCDROMLinux::mediaChanged()
{
    return (ioctl(m_deviceHandle, CDROM_MEDIA_CHANGED, CDSL_CURRENT) > 0);
}

bool MythCDROMLinux::checkOK()
{
    return (ioctl(m_deviceHandle, CDROM_DRIVE_STATUS, CDSL_CURRENT) ==
                  CDS_DISC_OK);
}

// Helper function, perform a sanity check on the device
MythMediaError MythCDROMLinux::testMedia()
{
    bool OpenedHere = false;
    if (!isDeviceOpen())
    {
        if (!openDevice())
        {
            LOG(VB_MEDIA, LOG_DEBUG, LOC + ":testMedia - failed to open '" +
                                     m_devicePath +  "' : " +ENO);
            if (errno == EBUSY)
                return isMounted() ? MEDIAERR_OK : MEDIAERR_FAILED;
            return MEDIAERR_FAILED;
        }
        LOG(VB_MEDIA, LOG_DEBUG, LOC + ":testMedia - Opened device");
        OpenedHere = true;
    }

    // Since the device was is/was open we can get it's status...
    int Stat = driveStatus();

    // Be nice and close the device if we opened it,
    // otherwise it might be locked when the user doesn't want it to be.
    if (OpenedHere)
        closeDevice();

    if (Stat == -1)
    {
        LOG(VB_MEDIA, LOG_DEBUG, LOC +
            ":testMedia - Failed to get drive status of '" + m_devicePath +
            "' : " + ENO);
        return MEDIAERR_FAILED;
    }

    return MEDIAERR_OK;
}

MythMediaStatus MythCDROMLinux::checkMedia()
{
    bool OpenedHere = false;

    // If it's not already open we need to at least
    // TRY to open it for most of these checks to work.
    if (!isDeviceOpen())
    {
        OpenedHere = openDevice();

        if (!OpenedHere)
        {
            LOG(VB_MEDIA, LOG_ERR, LOC +
                ":checkMedia() - cannot open device '" + m_devicePath + "' : " +
                ENO + "- returning UNKNOWN");
            m_mediaType = MEDIATYPE_UNKNOWN;
            return setStatus(MEDIASTAT_UNKNOWN, false);
        }
    }

    switch (driveStatus())
    {
        case CDS_DISC_OK:
            LOG(VB_MEDIA, LOG_DEBUG, m_devicePath + " Disk OK, type = " +
                                     MediaTypeString(m_mediaType) );
            // further checking is required
            break;
        case CDS_TRAY_OPEN:
            LOG(VB_MEDIA, LOG_DEBUG, m_devicePath + " Tray open or no disc");
            // First, send a message to the
            // plugins to forget the current media type
            setStatus(MEDIASTAT_OPEN, OpenedHere);
            // then "clear out" this device
            m_mediaType = MEDIATYPE_UNKNOWN;
            return MEDIASTAT_OPEN;
            break;
        case CDS_NO_DISC:
            LOG(VB_MEDIA, LOG_DEBUG, m_devicePath + " No disc");
            m_mediaType = MEDIATYPE_UNKNOWN;
            return setStatus(MEDIASTAT_NODISK, OpenedHere);
            break;
        case CDS_NO_INFO:
        case CDS_DRIVE_NOT_READY:
            LOG(VB_MEDIA, LOG_DEBUG, m_devicePath +
                                     " No info or drive not ready");
            m_mediaType = MEDIATYPE_UNKNOWN;
            return setStatus(MEDIASTAT_UNKNOWN, OpenedHere);
        default:
            LOG(VB_GENERAL, LOG_ERR, "Failed to get drive status of " +
                m_devicePath + " : " + ENO);
            m_mediaType = MEDIATYPE_UNKNOWN;
            return setStatus(MEDIASTAT_UNKNOWN, OpenedHere);
    }

    // NB must call mediaChanged before testing m_status otherwise will get
    // an unwanted mediaChanged on next pass
    if (mediaChanged() && m_status != MEDIASTAT_UNKNOWN)
    {
        LOG(VB_MEDIA, LOG_INFO, m_devicePath + " Media changed");
        // Regardless of the actual status lie here and say
        // it's open for now, so we can cover the case of a missed open.
        return setStatus(MEDIASTAT_OPEN, OpenedHere);
    }


    if (isUsable())
    {
        LOG(VB_MEDIA, LOG_DEBUG, "Disc useable, media unchanged. All good!");
        if (OpenedHere)
            closeDevice();
        return MEDIASTAT_USEABLE;
    }

    // If we have tried to mount and failed, don't keep trying
    if (m_status == MEDIASTAT_ERROR)
    {
        // Check if an external agent (like Gnome/KDE) mounted the disk
        if (isMounted())
        {
            onDeviceMounted();
            // pretend we're NOTMOUNTED so setStatus emits a signal
            m_status = MEDIASTAT_NOTMOUNTED;
            return setStatus(MEDIASTAT_MOUNTED, OpenedHere);
        }

        LOG(VB_MEDIA, LOG_DEBUG, "Disc is unmountable?");
        if (OpenedHere)
            closeDevice();
        return m_status;
    }

    if ((m_status == MEDIASTAT_OPEN) ||
        (m_status == MEDIASTAT_UNKNOWN))
    {
        LOG(VB_MEDIA, LOG_INFO, m_devicePath + " Current status " +
                MythMediaDevice::kMediaStatusStrings[m_status]);
        int type = ioctl(m_deviceHandle, CDROM_DISC_STATUS, CDSL_CURRENT);
        switch (type)
        {
            case CDS_DATA_1:
            case CDS_DATA_2:
            {
                m_mediaType = MEDIATYPE_DATA;
                LOG(VB_MEDIA, LOG_INFO, "Found a data disk");

                //grab information from iso9660 (& udf)
                off_t sr = lseek(m_deviceHandle,
                                 (off_t) 2048*16, SEEK_SET);

                struct iso_primary_descriptor buf {};
                ssize_t readin = 0;
                while ((sr != (off_t) -1) && (readin < 2048))
                {
                    ssize_t rr = read(
                        m_deviceHandle, ((char*)&buf) + readin, 2048 - readin);
                    if ((rr < 0) && ((EAGAIN == errno) || (EINTR == errno)))
                        continue;
                    if (rr < 0)
                        break;
                    readin += rr;
                }

                if (readin == 2048)
                {
                    m_volumeID = QString(buf.volume_id).trimmed();
                    m_keyID = QString("%1%2")
                        .arg(m_volumeID,
                             QString(reinterpret_cast<char*>(buf.creation_date)).left(16));
                }
                else
                {
                    m_volumeID = "UNKNOWN";
                    m_keyID = m_volumeID + MythDate::current_iso_string();
                }

                LOG(VB_MEDIA, LOG_INFO,
                    QString("Volume ID: %1").arg(m_volumeID));
                {
                    MythCDROM::ImageType imageType = MythCDROM::inspectImage(m_devicePath);
/*
                    if( imageType == MythCDROM::kBluray )
                        m_mediaType = MEDIATYPE_BD;
                    else
*/
                    if( imageType == MythCDROM::kDVD )
                        m_mediaType = MEDIATYPE_DVD;

                    if (MEDIATYPE_DATA != m_mediaType)
                    {
                        // pretend we're NOTMOUNTED so setStatus emits a signal
                        m_status = MEDIASTAT_NOTMOUNTED;
                        return setStatus(MEDIASTAT_USEABLE, OpenedHere);
                    }
                }

                // the base class's onDeviceMounted will do fine
                // grained detection of the type of data on this disc
                if (isMounted())
                    onDeviceMounted();
                else if (!mount()) // onDeviceMounted() called as side-effect
                    return setStatus(MEDIASTAT_NOTMOUNTED, OpenedHere);

                if (isMounted())
                {
                    // pretend we're NOTMOUNTED so setStatus emits a signal
                    m_status = MEDIASTAT_NOTMOUNTED;
                    return setStatus(MEDIASTAT_MOUNTED, OpenedHere);
                }
                if (m_mediaType == MEDIATYPE_DVD)
                {
                    // pretend we're NOTMOUNTED so setStatus emits a signal
                    m_status = MEDIASTAT_NOTMOUNTED;
                    return setStatus(MEDIASTAT_USEABLE, OpenedHere);
                }
                return setStatus(MEDIASTAT_NOTMOUNTED, OpenedHere);
            }
            case CDS_AUDIO:
                LOG(VB_MEDIA, LOG_DEBUG, "found an audio disk");
                // pretend we're NOTMOUNTED so setStatus emits a signal
                m_status = MEDIASTAT_NOTMOUNTED;
                m_mediaType = MEDIATYPE_AUDIO;
                return setStatus(MEDIASTAT_USEABLE, OpenedHere);
            case CDS_MIXED:
                LOG(VB_MEDIA, LOG_DEBUG, "found a mixed CD");
                // Note: Mixed mode CDs require an explixit mount call
                //       since we'll usually want the audio portion.
                // undefine ASSUME_WANT_AUDIO to change this behavior.
                #if ASSUME_WANT_AUDIO
                    // pretend we're NOTMOUNTED so setStatus emits a signal
                    m_status = MEDIASTAT_NOTMOUNTED;
                    m_mediaType = MEDIATYPE_AUDIO;
                    return setStatus(MEDIASTAT_USEABLE, OpenedHere);
                #else
                    m_mediaType = MEDIATYPE_MIXED;
                    mount();
                    if (isMounted())
                    {
                        // pretend we're NOTMOUNTED so setStatus
                        // emits a signal
                        m_status = MEDIASTAT_NOTMOUNTED;
                        return setStatus(MEDIASTAT_MOUNTED, OpenedHere);
                    }
                    else
                    {
                        return setStatus(MEDIASTAT_USEABLE, OpenedHere);
                    }
                #endif
                break;
            case CDS_NO_INFO:
            case CDS_NO_DISC:
                if (hasWritableMedia())
                {
                    LOG(VB_MEDIA, LOG_DEBUG, "found a blank or writable disk");
                    return setStatus(MEDIASTAT_UNFORMATTED, OpenedHere);
                }

                LOG(VB_MEDIA, LOG_DEBUG, "found no disk");
                m_mediaType = MEDIATYPE_UNKNOWN;
                return setStatus(MEDIASTAT_UNKNOWN, OpenedHere);
                break;
            default:
                LOG(VB_MEDIA, LOG_DEBUG, "found unknown disk type: " +
                                         QString::number(type));
                m_mediaType = MEDIATYPE_UNKNOWN;
                return setStatus(MEDIASTAT_UNKNOWN, OpenedHere);
        }
    }

    if (m_allowEject)
        unlock();
    else
        lock();

    if (OpenedHere)
        closeDevice();

    LOG(VB_MEDIA, LOG_DEBUG, QString("Returning %1")
                           .arg(MythMediaDevice::kMediaStatusStrings[m_status]));
    return m_status;
}

MythMediaError MythCDROMLinux::lock()
{
    MythMediaError ret = MythMediaDevice::lock();
    if (ret == MEDIAERR_OK)
    {
        LOG(VB_MEDIA, LOG_DEBUG, LOC + ":lock - Locking CDROM door");
        int res = ioctl(m_deviceHandle, CDROM_LOCKDOOR, 1);

        if (res < 0)
            LOG(VB_MEDIA, LOG_WARNING, "lock() - CDROM_LOCKDOOR ioctl failed" + ENO);
    }

    return ret;
}

MythMediaError MythCDROMLinux::unlock()
{
    if (isDeviceOpen() || openDevice())
    {
        LOG(VB_MEDIA, LOG_DEBUG, LOC + ":unlock - Unlocking CDROM door");
        int res = ioctl(m_deviceHandle, CDROM_LOCKDOOR, 0);

        if (res < 0)
            LOG(VB_MEDIA, LOG_WARNING, "unlock() - CDROM_LOCKDOOR ioctl failed" + ENO);
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, "Failed to open device, CDROM tray will "
                                  "remain locked.");
    }

    return MythMediaDevice::unlock();
}

bool MythCDROMLinux::isSameDevice(const QString &path)
{
    struct stat sb {};

    if (stat(path.toLocal8Bit().constData(), &sb) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + ":isSameDevice() -- " +
            QString("Failed to stat '%1'").arg(path) + ENO);
        return false;
    }
    dev_t new_rdev = sb.st_rdev;

    // Check against m_devicePath...
    if (stat(m_devicePath.toLocal8Bit().constData(), &sb) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + ":isSameDevice() -- " +
            QString("Failed to stat '%1'").arg(m_devicePath) + ENO);
        return false;
    }
    return (sb.st_rdev == new_rdev);
}

#if defined(SG_IO) && defined(GPCMD_SET_STREAMING)
/*
 * \brief obtained from the mplayer project
 */
void MythCDROMLinux::setDeviceSpeed(const char *device, int speed)
{
    std::array<uint8_t,28> buffer {};
    std::array<uint8_t,16> cmd {};
    std::array<uint8_t,16> sense {};
    struct sg_io_hdr sghdr {};
    struct stat st {};
    int rate = 0;

    int fd = open(device, O_RDWR | O_NONBLOCK);
    if (fd == -1)
    {
        LOG(VB_MEDIA, LOG_ERR, LOC +
            " Changing CD/DVD speed needs write access");
        return;
    }

    if (fstat(fd, &st) == -1)
    {
        close(fd);
        LOG(VB_MEDIA, LOG_ERR, LOC +
            QString(":setDeviceSpeed() Failed. device %1 not found")
            .arg(device));
        return;
    }

    if (!S_ISBLK(st.st_mode))
    {
        close(fd);
        LOG(VB_MEDIA, LOG_ERR, LOC +
            ":setDeviceSpeed() Failed. Not a block device");
        return;
    }

    if (speed < 0)
        speed = -1;

    switch(speed)
    {
        case 0: // don't touch speed setting
            close(fd);
            return;
        case -1: // restore default value
        {
            rate = 0;
            buffer[0] = 4;
            LOG(VB_MEDIA, LOG_INFO, LOC +
                ":setDeviceSpeed() - Restored CD/DVD Speed");
            break;
        }
        default:
        {
            // Speed in Kilobyte/Second. 177KB/s is the maximum data rate
            // for standard Audio CD's.

            rate = (speed > 0 && speed < 100) ? speed * 177 : speed;

            LOG(VB_MEDIA, LOG_INFO, LOC +
                QString(":setDeviceSpeed() - Limiting CD/DVD Speed to %1KB/s")
                    .arg(rate));
            break;
        }
    }

    sghdr.interface_id = 'S';
    sghdr.timeout = 5000;
    sghdr.dxfer_direction = SG_DXFER_TO_DEV;
    sghdr.mx_sb_len = sense.size();
    sghdr.dxfer_len = buffer.size();
    sghdr.cmd_len = cmd.size();
    sghdr.sbp = sense.data();
    sghdr.dxferp = buffer.data();
    sghdr.cmdp = cmd.data();

    cmd[0] = GPCMD_SET_STREAMING;
    cmd[10] = buffer.size();

    buffer[8]  = 0xff;
    buffer[9]  = 0xff;
    buffer[10] = 0xff;
    buffer[11] = 0xff;

    buffer[12] = buffer[20] = (rate >> 24) & 0xff;
    buffer[13] = buffer[21] = (rate >> 16) & 0xff;
    buffer[14] = buffer[22] = (rate >> 8)  & 0xff;
    buffer[15] = buffer[23] = rate & 0xff;

    // Note: 0x3e8 == 1000, hence speed = data amount per 1000 milliseconds.
    buffer[18] = buffer[26] = 0x03;
    buffer[19] = buffer[27] = 0xe8;

    if (ioctl(fd, SG_IO, &sghdr) < 0)
    {
        LOG(VB_MEDIA, LOG_ERR, LOC + " Limit CD/DVD Speed Failed" + ENO);
    }
    else
    {
        // On my system (2.6.18+ide-cd), SG_IO succeeds without doing anything,
        // while CDROM_SELECT_SPEED works...
        if (ioctl(fd, CDROM_SELECT_SPEED, speed) < 0)
        {
            LOG(VB_MEDIA, LOG_ERR, LOC +
                " Limit CD/DVD CDROM_SELECT_SPEED Failed" + ENO);
        }
        LOG(VB_MEDIA, LOG_INFO, LOC +
            ":setDeviceSpeed() - CD/DVD Speed Set Successful");
    }

    close(fd);
}
#endif
