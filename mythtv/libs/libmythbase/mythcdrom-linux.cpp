#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>       // ioctls
#include <linux/cdrom.h>     // old ioctls for cdrom
#include <scsi/sg.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <limits.h>
#include <linux/iso_fs.h>
#include <unistd.h>

#include <QDateTime>

#include "mythcdrom.h"
#include "mythcdrom-linux.h"
#include "mythconfig.h"      // for HAVE_BIGENDIAN
#include "mythlogging.h"
#include "mythdate.h"

#ifdef USING_LIBUDF
#include <cdio/udf.h>
#endif

#define LOC     QString("MythCDROMLinux:")

// On a mixed-mode disc (audio+data), set this to 0 to mount the data portion:
#ifndef ASSUME_WANT_AUDIO
#define ASSUME_WANT_AUDIO 1
#endif


// Some features cannot be detected (reliably) using the standard
// Linux ioctl()s, so we use some direct low-level device queries.

typedef struct cdrom_generic_command CDROMgenericCmd;

// Some structures stolen from the __KERNEL__ section of linux/cdrom.h.

// This contains the result of a GPCMD_GET_EVENT_STATUS_NOTIFICATION.
// It is the joining of a struct event_header and a struct media_event_desc
typedef struct
{
    uint16_t data_len[2];
#if HAVE_BIGENDIAN
    uint8_t  nea                : 1;
    uint8_t  reserved1          : 4;
    uint8_t  notification_class : 3;
#else
    uint8_t  notification_class : 3;
    uint8_t  reserved1          : 4;
    uint8_t  nea                : 1;
#endif
    uint8_t  supp_event_class;
#if HAVE_BIGENDIAN
    uint8_t  reserved2          : 4;
    uint8_t  media_event_code   : 4;
    uint8_t  reserved3          : 6;
    uint8_t  media_present      : 1;
    uint8_t  door_open          : 1;
#else
    uint8_t  media_event_code   : 4;
    uint8_t  reserved2          : 4;
    uint8_t  door_open          : 1;
    uint8_t  media_present      : 1;
    uint8_t  reserved3          : 6;
#endif
    uint8_t  start_slot;
    uint8_t  end_slot;
} CDROMeventStatus;

// and this is returned by GPCMD_READ_DISC_INFO
typedef struct {
    uint16_t disc_information_length;
#if HAVE_BIGENDIAN
    uint8_t  reserved1          : 3;
    uint8_t  erasable           : 1;
    uint8_t  border_status      : 2;
    uint8_t  disc_status        : 2;
#else
    uint8_t  disc_status        : 2;
    uint8_t  border_status      : 2;
    uint8_t  erasable           : 1;
    uint8_t  reserved1          : 3;
#endif
    uint8_t  n_first_track;
    uint8_t  n_sessions_lsb;
    uint8_t  first_track_lsb;
    uint8_t  last_track_lsb;
#if HAVE_BIGENDIAN
    uint8_t  did_v              : 1;
    uint8_t  dbc_v              : 1;
    uint8_t  uru                : 1;
    uint8_t  reserved2          : 5;
#else
    uint8_t  reserved2          : 5;
    uint8_t  uru                : 1;
    uint8_t  dbc_v              : 1;
    uint8_t  did_v              : 1;
#endif
    uint8_t  disc_type;
    uint8_t  n_sessions_msb;
    uint8_t  first_track_msb;
    uint8_t  last_track_msb;
    uint32_t disc_id;
    uint32_t lead_in;
    uint32_t lead_out;
    uint8_t  disc_bar_code[8];
    uint8_t  reserved3;
    uint8_t  n_opc;
} CDROMdiscInfo;

enum CDROMdiscStatus
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
    MythCDROMLinux(QObject* par, const char* DevicePath, bool SuperMount,
                   bool AllowEject):
        MythCDROM(par, DevicePath, SuperMount, AllowEject) {
    }

    virtual MythMediaError testMedia(void);
    virtual bool mediaChanged(void);
    virtual bool checkOK(void);
    virtual MythMediaStatus checkMedia(void);
    virtual MythMediaError eject(bool open_close = true);
    virtual void setDeviceSpeed(const char *device, int speed);
    virtual bool isSameDevice(const QString &path);
    virtual MythMediaError lock(void);
    virtual MythMediaError unlock(void);

private:
    int driveStatus(void);
    bool hasWritableMedia(void);
    int SCSIstatus(void);
};

MythCDROM *GetMythCDROMLinux(QObject* par, const char* devicePath,
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
    int drive_status = ioctl(m_DeviceHandle, CDROM_DRIVE_STATUS, CDSL_CURRENT);

    if (drive_status == -1)   // Very unlikely, but we should check
    {
        LOG(VB_MEDIA, LOG_ERR, LOC + ":driveStatus() - ioctl failed: " + ENO);
        return CDS_NO_INFO;
    }

    if (drive_status == CDS_TRAY_OPEN && m_DevicePath.contains("/dev/scd"))
        return SCSIstatus();

    return drive_status;
}

/** \brief Is there blank or eraseable media in the drive?
 */

bool MythCDROMLinux::hasWritableMedia()
{
    unsigned char    buffer[32];
    CDROMgenericCmd  cgc;
    CDROMdiscInfo   *di;


    memset(buffer, 0, sizeof(buffer));
    memset(&cgc,   0, sizeof(cgc));

    cgc.cmd[0] = GPCMD_READ_DISC_INFO;
    cgc.cmd[8] = sizeof(buffer);
    cgc.quiet  = 1;
    cgc.buffer = buffer;
    cgc.buflen = sizeof(buffer);
    cgc.data_direction = CGC_DATA_READ;

    if (ioctl(m_DeviceHandle, CDROM_SEND_PACKET, &cgc) < 0)
    {
        LOG(VB_MEDIA, LOG_ERR, LOC +
            ":hasWritableMedia() - failed to send packet to " + m_DevicePath + ENO);
        return false;
    }

    di = (CDROMdiscInfo *) buffer;

    switch (di->disc_status)
    {
        case MEDIA_IS_EMPTY:
            return true;

        case MEDIA_IS_APPENDABLE:
            // It is unlikely that any plugins will support multi-session
            // writing, so we treat it just like a finished disc:

        case MEDIA_IS_COMPLETE:
            return di->erasable;

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
    unsigned char    buffer[8];
    CDROMgenericCmd  cgc;
    CDROMeventStatus *es;


    memset(buffer, 0, sizeof(buffer));
    memset(&cgc,   0, sizeof(cgc));

    cgc.cmd[0] = GPCMD_GET_EVENT_STATUS_NOTIFICATION;
    cgc.cmd[1] = 1;       // Tell us immediately
    cgc.cmd[4] = 1 << 4;  // notification class of media
    cgc.cmd[8] = sizeof(buffer);
    cgc.quiet  = 1;
    cgc.buffer = buffer;
    cgc.buflen = sizeof(buffer);
    cgc.data_direction = CGC_DATA_READ;

    es = (CDROMeventStatus *) buffer;

    if ((ioctl(m_DeviceHandle, CDROM_SEND_PACKET, &cgc) < 0)
        || es->nea                           // drive does not support request
        || (es->notification_class != 0x4))  // notification class mismatch
    {
        LOG(VB_MEDIA, LOG_ERR, LOC +
            ":SCSIstatus() - failed to send SCSI packet to " + m_DevicePath + ENO);
        return CDS_TRAY_OPEN;
    }

    if (es->media_present)
    {
        LOG(VB_MEDIA, LOG_DEBUG, LOC + 
            ":SCSIstatus() - ioctl said tray was open, "
            "but drive is actually closed with a disc");
        return CDS_DISC_OK;
    }
    else if (es->door_open)
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

    if (open_close)
        return (ioctl(m_DeviceHandle, CDROMEJECT) == 0) ? MEDIAERR_OK
                                                        : MEDIAERR_FAILED;
    else
    {
        // If the tray is empty, this will fail (Input/Output error)
        int res = ioctl(m_DeviceHandle, CDROMCLOSETRAY);

        if (res < 0)
            LOG(VB_MEDIA, LOG_DEBUG, "CDROMCLOSETRAY ioctl failed" + ENO);

        // This allows us to catch any drives that the OS has problems
        // detecting the status of (some always report OPEN when empty)
        if (driveStatus() == CDS_TRAY_OPEN)
            return MEDIAERR_FAILED;
        else
            return MEDIAERR_OK;
    }
}


bool MythCDROMLinux::mediaChanged()
{
    return (ioctl(m_DeviceHandle, CDROM_MEDIA_CHANGED, CDSL_CURRENT) > 0);
}

bool MythCDROMLinux::checkOK()
{
    return (ioctl(m_DeviceHandle, CDROM_DRIVE_STATUS, CDSL_CURRENT) ==
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
                                     m_DevicePath +  "' : " +ENO);
            if (errno == EBUSY)
                return isMounted() ? MEDIAERR_OK : MEDIAERR_FAILED;
            else
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
            ":testMedia - Failed to get drive status of '" + m_DevicePath +
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
                ":checkMedia() - cannot open device '" + m_DevicePath + "' : " +
                ENO + "- returning UNKNOWN");
            m_MediaType = MEDIATYPE_UNKNOWN;
            return setStatus(MEDIASTAT_UNKNOWN, false);
        }
    }

    switch (driveStatus())
    {
        case CDS_DISC_OK:
            LOG(VB_MEDIA, LOG_DEBUG, m_DevicePath + " Disk OK, type = " +
                                     MediaTypeString(m_MediaType) );
            // further checking is required
            break;
        case CDS_TRAY_OPEN:
            LOG(VB_MEDIA, LOG_DEBUG, m_DevicePath + " Tray open or no disc");
            // First, send a message to the
            // plugins to forget the current media type
            setStatus(MEDIASTAT_OPEN, OpenedHere);
            // then "clear out" this device
            m_MediaType = MEDIATYPE_UNKNOWN;
            return MEDIASTAT_OPEN;
            break;
        case CDS_NO_DISC:
            LOG(VB_MEDIA, LOG_DEBUG, m_DevicePath + " No disc");
            m_MediaType = MEDIATYPE_UNKNOWN;
            return setStatus(MEDIASTAT_NODISK, OpenedHere);
            break;
        case CDS_NO_INFO:
        case CDS_DRIVE_NOT_READY:
            LOG(VB_MEDIA, LOG_DEBUG, m_DevicePath + 
                                     " No info or drive not ready");
            m_MediaType = MEDIATYPE_UNKNOWN;
            return setStatus(MEDIASTAT_UNKNOWN, OpenedHere);
        default:
            LOG(VB_GENERAL, LOG_ERR, "Failed to get drive status of " +
                m_DevicePath + " : " + ENO);
            m_MediaType = MEDIATYPE_UNKNOWN;
            return setStatus(MEDIASTAT_UNKNOWN, OpenedHere);
    }

    if (mediaChanged())
    {
        LOG(VB_MEDIA, LOG_INFO, m_DevicePath + " Media changed");
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
    if (m_Status == MEDIASTAT_ERROR)
    {
        // Check if an external agent (like Gnome/KDE) mounted the disk
        if (isMounted())
        {
            onDeviceMounted();
            // pretend we're NOTMOUNTED so setStatus emits a signal
            m_Status = MEDIASTAT_NOTMOUNTED;
            return setStatus(MEDIASTAT_MOUNTED, OpenedHere);
        }

        LOG(VB_MEDIA, LOG_DEBUG, "Disc is unmountable?");
        if (OpenedHere)
            closeDevice();
        return m_Status;
    }

    if ((m_Status == MEDIASTAT_OPEN) ||
        (m_Status == MEDIASTAT_UNKNOWN))
    {
        LOG(VB_MEDIA, LOG_INFO, m_DevicePath + " Current status " +
                MythMediaDevice::MediaStatusStrings[m_Status]);
        int type = ioctl(m_DeviceHandle, CDROM_DISC_STATUS, CDSL_CURRENT);
        switch (type)
        {
            case CDS_DATA_1:
            case CDS_DATA_2:
            {
                m_MediaType = MEDIATYPE_DATA;
                LOG(VB_MEDIA, LOG_INFO, "Found a data disk");

                //grab information from iso9660 (& udf)
                off_t sr = lseek(m_DeviceHandle,
                                 (off_t) 2048*16, SEEK_SET);

                struct iso_primary_descriptor buf;
                ssize_t readin = 0;
                while ((sr != (off_t) -1) && (readin < 2048))
                {
                    ssize_t rr = read(
                        m_DeviceHandle, ((char*)&buf) + readin, 2048 - readin);
                    if ((rr < 0) && ((EAGAIN == errno) || (EINTR == errno)))
                        continue;
                    else if (rr < 0)
                        break;
                    readin += rr;
                }

                if (readin == 2048)
                {
                    m_VolumeID = QString(buf.volume_id).trimmed();
                    m_KeyID = QString("%1%2")
                        .arg(m_VolumeID)
                        .arg(QString(buf.creation_date).left(16));
                }
                else
                {
                    m_VolumeID = "UNKNOWN";
                    m_KeyID = m_VolumeID + MythDate::current_iso_string();
                }

                LOG(VB_MEDIA, LOG_INFO,
                    QString("Volume ID: %1").arg(m_VolumeID));
#ifdef USING_LIBUDF
                // Check for a DVD/BD disk by reading the UDF root dir.
                // This allows DVD's to play immediately upon insertion without
                // calling mount, which either needs pmount or changes to fstab.
                udf_t *pUdf = udf_open(m_DevicePath.toLatin1());
                if (NULL != pUdf)
                {
                    udf_dirent_t *pUdfRoot = udf_get_root(pUdf, true, 0);
                    if (NULL != pUdfRoot)
                    {
                        if (NULL != udf_fopen(pUdfRoot, "VIDEO_TS"))
                            m_MediaType = MEDIATYPE_DVD;
                        else if (NULL != udf_fopen(pUdfRoot, "BDMV"))
                            m_MediaType = MEDIATYPE_BD;

                        udf_dirent_free(pUdfRoot);
                    }
                    udf_close(pUdf);

                    if (MEDIATYPE_DATA != m_MediaType)
                    {
                        // pretend we're NOTMOUNTED so setStatus emits a signal
                        m_Status = MEDIASTAT_NOTMOUNTED;
                        return setStatus(MEDIASTAT_USEABLE, OpenedHere);
                    }
                }
#endif
                // the base class's onDeviceMounted will do fine
                // grained detection of the type of data on this disc
                if (isMounted())
                    onDeviceMounted();
                else if (!mount()) // onDeviceMounted() called as side-effect
                    return setStatus(MEDIASTAT_NOTMOUNTED, OpenedHere);

                if (isMounted())
                {
                    // pretend we're NOTMOUNTED so setStatus emits a signal
                    m_Status = MEDIASTAT_NOTMOUNTED;
                    return setStatus(MEDIASTAT_MOUNTED, OpenedHere);
                }
                else if (m_MediaType == MEDIATYPE_DVD)
                {
                    // pretend we're NOTMOUNTED so setStatus emits a signal
                    m_Status = MEDIASTAT_NOTMOUNTED;
                    return setStatus(MEDIASTAT_USEABLE, OpenedHere);
                }
                else
                    return setStatus(MEDIASTAT_NOTMOUNTED, OpenedHere);
                break;
            }
            case CDS_AUDIO:
                LOG(VB_MEDIA, LOG_DEBUG, "found an audio disk");
                // pretend we're NOTMOUNTED so setStatus emits a signal
                m_Status = MEDIASTAT_NOTMOUNTED;
                m_MediaType = MEDIATYPE_AUDIO;
                return setStatus(MEDIASTAT_USEABLE, OpenedHere);
                break;
            case CDS_MIXED:
                LOG(VB_MEDIA, LOG_DEBUG, "found a mixed CD");
                // Note: Mixed mode CDs require an explixit mount call
                //       since we'll usually want the audio portion.
                // undefine ASSUME_WANT_AUDIO to change this behavior.
                #if ASSUME_WANT_AUDIO
                    // pretend we're NOTMOUNTED so setStatus emits a signal
                    m_Status = MEDIASTAT_NOTMOUNTED;
                    m_MediaType = MEDIATYPE_AUDIO;
                    return setStatus(MEDIASTAT_USEABLE, OpenedHere);
                #else
                    m_MediaType = MEDIATYPE_MIXED;
                    mount();
                    if (isMounted())
                    {
                        // pretend we're NOTMOUNTED so setStatus
                        // emits a signal
                        m_Status = MEDIASTAT_NOTMOUNTED;
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
                m_MediaType = MEDIATYPE_UNKNOWN;
                return setStatus(MEDIASTAT_UNKNOWN, OpenedHere);
                break;
            default:
                LOG(VB_MEDIA, LOG_DEBUG, "found unknown disk type: " +
                                         QString::number(type));
                m_MediaType = MEDIATYPE_UNKNOWN;
                return setStatus(MEDIASTAT_UNKNOWN, OpenedHere);
        }
    }

    if (m_AllowEject)
        unlock();
    else
        lock();

    if (OpenedHere)
        closeDevice();

    LOG(VB_MEDIA, LOG_DEBUG, QString("Returning %1")
                           .arg(MythMediaDevice::MediaStatusStrings[m_Status]));
    return m_Status;
}

MythMediaError MythCDROMLinux::lock()
{
    MythMediaError ret = MythMediaDevice::lock();
    if (ret == MEDIAERR_OK)
    {
        int res = ioctl(m_DeviceHandle, CDROM_LOCKDOOR, 1);

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
        int res = ioctl(m_DeviceHandle, CDROM_LOCKDOOR, 0);

        if (res < 0)
            LOG(VB_MEDIA, LOG_WARNING, "unlock() - CDROM_LOCKDOOR ioctl failed" + ENO);
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, "Failed to open device, CDROM try will "
                                  "remain locked.");
    }

    return MythMediaDevice::unlock();
}

bool MythCDROMLinux::isSameDevice(const QString &path)
{
    dev_t new_rdev;
    struct stat sb;

    if (stat(path.toLocal8Bit().constData(), &sb) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + ":isSameDevice() -- " +
            QString("Failed to stat '%1'").arg(path) + ENO);
        return false;
    }
    new_rdev = sb.st_rdev;

    // Check against m_DevicePath...
    if (stat(m_DevicePath.toLocal8Bit().constData(), &sb) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + ":isSameDevice() -- " +
            QString("Failed to stat '%1'").arg(m_DevicePath) + ENO);
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
    int fd;
    unsigned char buffer[28];
    unsigned char cmd[16];
    unsigned char sense[16];
    struct sg_io_hdr sghdr;
    struct stat st;
    int rate = 0;

    memset(&sghdr, 0, sizeof(sghdr));
    memset(buffer, 0, sizeof(buffer));
    memset(sense, 0, sizeof(sense));
    memset(cmd, 0, sizeof(cmd));
    memset(&st, 0, sizeof(st));

    if ((fd = open(device, O_RDWR | O_NONBLOCK)) == -1)
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
    sghdr.mx_sb_len = sizeof(sense);
    sghdr.dxfer_len = sizeof(buffer);
    sghdr.cmd_len = sizeof(cmd);
    sghdr.sbp = sense;
    sghdr.dxferp = buffer;
    sghdr.cmdp = cmd;

    cmd[0] = GPCMD_SET_STREAMING;
    cmd[10] = sizeof(buffer);

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
