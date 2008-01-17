#include "mythcdrom.h"
#include "mythcdrom-linux.h"
#include <sys/ioctl.h>       // ioctls
#include <linux/cdrom.h>     // old ioctls for cdrom
#include <scsi/sg.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>

#include "mythconfig.h"      // for WORDS_BIGENDIAN
#include "mythcontext.h"

#include <linux/iso_fs.h>
#include <unistd.h>

#define LOC     QString("MythCDROMLinux:")
#define LOC_ERR QString("mythcdrom-linux, Error: ")

// On a mixed-mode disc (audio+data), set this to 0 to mount the data portion:
#define ASSUME_WANT_AUDIO 1

// This class is verbose enough, but if you really need more output:
//#define EXTRA_VERBOSITY   1


// Some features cannot be detected (reliably) using the standard
// Linux ioctl()s, so we use some direct low-level device queries.

typedef struct cdrom_generic_command CDROMgenericCmd;

// Some structures which contain the result of a low-level SCSI CDROM query
struct event_header
{
    unsigned char data_len[2];
#ifdef WORDS_BIGENDIAN
    unsigned char nea                : 1;
    unsigned char reserved1          : 4;
    unsigned char notification_class : 3;
#else
    unsigned char notification_class : 3;
    unsigned char reserved1          : 4;
    unsigned char nea                : 1;
#endif
    unsigned char supp_event_class;
};

struct media_event_desc
{
#ifdef WORDS_BIGENDIAN
    unsigned char reserved1          : 4;
    unsigned char media_event_code   : 4;
    unsigned char reserved2          : 6;
    unsigned char media_present      : 1;
    unsigned char door_open          : 1;
#else
    unsigned char media_event_code   : 4;
    unsigned char reserved1          : 4;
    unsigned char door_open          : 1;
    unsigned char media_present      : 1;
    unsigned char reserved2          : 6;
#endif
    unsigned char start_slot;
    unsigned char end_slot;
};

// and this is returned by GPCMD_READ_DISC_INFO
typedef struct {
    uint16_t disc_information_length;
#ifdef WORDS_BIGENDIAN
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
#ifdef WORDS_BIGENDIAN
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

    virtual MediaError testMedia(void);
    virtual bool mediaChanged(void);
    virtual bool checkOK(void);
    virtual MediaStatus checkMedia(void);
    virtual MediaError eject(bool open_close = true);
    virtual void setSpeed(int speed);
    virtual bool isSameDevice(const QString &path);
    virtual MediaError lock(void);
    virtual MediaError unlock(void);

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

    if (drive_status == CDS_TRAY_OPEN && getDevicePath().contains("/dev/scd"))
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
        VERBOSE(VB_MEDIA,
                LOC + ":hasWritableMedia() - failed to send packet to "
                    + m_DevicePath);
        return 0;
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
    unsigned char                buffer[8];
    struct cdrom_generic_command cgc;
    struct event_header         *eh;
    struct media_event_desc     *med;


    memset(buffer, 0, sizeof(buffer));
    memset(&cgc,   0, sizeof(cgc));

    cgc.cmd[0] = GPCMD_GET_EVENT_STATUS_NOTIFICATION;
    cgc.cmd[1] = 1;
    cgc.cmd[4] = 1 << 4;
    cgc.cmd[8] = sizeof(buffer);
    cgc.quiet  = 1;
    cgc.buffer = buffer;
    cgc.buflen = sizeof(buffer);
    cgc.data_direction = CGC_DATA_READ;

    eh  = (struct event_header     *)  buffer;
    med = (struct media_event_desc *) (buffer + sizeof(struct event_header));

    if ((ioctl(m_DeviceHandle, CDROM_SEND_PACKET, &cgc) < 0)
        || eh->nea || (eh->notification_class != 0x4))
    {
        VERBOSE(VB_MEDIA,
                LOC + ":SCSIstatus() - failed to send SCSI packet to "
                    + m_DevicePath);
        return CDS_TRAY_OPEN;
    }

    if (med->media_present)
    {
#ifdef EXTRA_VERBOSITY
        VERBOSE(VB_MEDIA, LOC + ":SCSIstatus() - ioctl() said tray was open,"
                                "but drive is actually closed with a disc");
#endif
        return CDS_DISC_OK;
    }
    else if (med->door_open)
    {
#ifdef EXTRA_VERBOSITY
        VERBOSE(VB_MEDIA, LOC + ":SCSIstatus() - tray is definitely open");
#endif
        return CDS_TRAY_OPEN;
    }

#ifdef EXTRA_VERBOSITY
    VERBOSE(VB_MEDIA, LOC + ":SCSIstatus() - ioctl() said tray was open,"
                            " but drive is actually closed with no disc");
#endif
    return CDS_NO_DISC;
}


MediaError MythCDROMLinux::eject(bool open_close)
{
    if (!isDeviceOpen())
        openDevice();

    if (open_close)
        return (ioctl(m_DeviceHandle, CDROMEJECT) == 0) ? MEDIAERR_OK
                                                        : MEDIAERR_FAILED;
    else
    {
        // If the tray is empty, this will fail (Input/Output error)
        ioctl(m_DeviceHandle, CDROMCLOSETRAY);

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
MediaError MythCDROMLinux::testMedia()
{
    //cout << "MythCDROMLinux::testMedia - ";
    bool OpenedHere = false;
    if (!isDeviceOpen())
    {
        //cout << "Device is not open - ";
        if (!openDevice())
        {
#ifdef EXTRA_VERBOSITY
            VERBOSE(VB_MEDIA, LOC + ":testMedia - failed to open "
                              + getDevicePath() + ENO);
#endif
            if (errno == EBUSY)
                return isMounted(true) ? MEDIAERR_OK : MEDIAERR_FAILED;
            else
                return MEDIAERR_FAILED;
        }
#ifdef EXTRA_VERBOSITY
        VERBOSE(VB_MEDIA, LOC + ":testMedia - Opened device");
#endif
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
        VERBOSE(VB_MEDIA, LOC + ":testMedia - Failed to get drive status of "
                          + getDevicePath() + ENO);
        return MEDIAERR_FAILED;
    }

    return MEDIAERR_OK;
}

MediaStatus MythCDROMLinux::checkMedia()
{
    bool OpenedHere = false;

    if (testMedia() != MEDIAERR_OK)
    {
        m_MediaType = MEDIATYPE_UNKNOWN;
        return setStatus(MEDIASTAT_UNKNOWN, OpenedHere);
    }

    // If it's not already open we need to at least
    // TRY to open it for most of these checks to work.
    if (!isDeviceOpen())
        OpenedHere = openDevice();

    if (isDeviceOpen())
    {
#ifdef EXTRA_VERBOSITY
        VERBOSE(VB_MEDIA, LOC + ":checkMedia - Device is open...");
#endif
        switch (driveStatus())
        {
            case CDS_DISC_OK:
                VERBOSE(VB_MEDIA, getDevicePath() + " Disk OK, type = "
                                  + MediaTypeString(m_MediaType) );
                // 1. Audio CDs are not mounted
                // 2. If we don't know the media type yet,
                //    test the disk after this switch exits
                if (m_MediaType == MEDIATYPE_AUDIO ||
                    m_MediaType == MEDIATYPE_UNKNOWN)
                    break;
                // If we have tried to mount and failed, don't keep trying:
                if (m_Status == MEDIASTAT_ERROR)
                    return m_Status;
                // If the disc is ok and we already know it's mediatype
                // returns MOUNTED.
                if (isMounted(true))
                    return setStatus(MEDIASTAT_MOUNTED, OpenedHere);
                break;
            case CDS_TRAY_OPEN:
                VERBOSE(VB_MEDIA, getDevicePath() + " Tray open or no disc");
                setStatus(MEDIASTAT_OPEN, OpenedHere);
                m_MediaType = MEDIATYPE_UNKNOWN;
                return MEDIASTAT_OPEN;
                break;
            case CDS_NO_DISC:
                VERBOSE(VB_MEDIA, getDevicePath() + " No disc");
                m_MediaType = MEDIATYPE_UNKNOWN;
                return setStatus(MEDIASTAT_NODISK, OpenedHere);
                break;
            case CDS_NO_INFO:
            case CDS_DRIVE_NOT_READY:
                VERBOSE(VB_MEDIA, getDevicePath()
                                  + " No info or drive not ready");
                m_MediaType = MEDIATYPE_UNKNOWN;
                return setStatus(MEDIASTAT_UNKNOWN, OpenedHere);
            default:
                VERBOSE(VB_IMPORTANT, "Failed to get drive status of "
                                      + getDevicePath() + " : " + ENO);
                m_MediaType = MEDIATYPE_UNKNOWN;
                return setStatus(MEDIASTAT_UNKNOWN, OpenedHere);
        }

        if (mediaChanged())
        {
            VERBOSE(VB_MEDIA, getDevicePath() + " Media changed");
            // Regardless of the actual status lie here and say
            // it's open for now, so we can cover the case of a missed open.
            return setStatus(MEDIASTAT_OPEN, OpenedHere);
        }
        else
        {
#ifdef EXTRA_VERBOSITY
            VERBOSE(VB_MEDIA, getDevicePath() + " Media unchanged...");
#endif
            if ((m_Status == MEDIASTAT_OPEN) ||
                (m_Status == MEDIASTAT_UNKNOWN))
            {
                VERBOSE(VB_MEDIA, getDevicePath() + " Current status " +
                        MythMediaDevice::MediaStatusStrings[m_Status]);
                int type = ioctl(m_DeviceHandle,
                                 CDROM_DISC_STATUS, CDSL_CURRENT);
                switch (type)
                {
                    case CDS_DATA_1:
                    case CDS_DATA_2:
                        m_MediaType = MEDIATYPE_DATA;
                        VERBOSE(VB_MEDIA, "Found a data disk");
                        //grab information from iso9660 (& udf)
                        struct iso_primary_descriptor buf;
                        lseek(this->m_DeviceHandle,(off_t) 2048*16,SEEK_SET);
                        read(this->m_DeviceHandle, &buf,2048);
                        this->m_VolumeID = QString(buf.volume_id)
                                                       .stripWhiteSpace();
                        this->m_KeyID = QString("%1%2")
                                      .arg(this->m_VolumeID)
                                      .arg(QString(buf.creation_date).left(16));

                        // the base class's onDeviceMounted will do fine
                        // grained detection of the type of data on this disc
                        if (isMounted(true))
                            onDeviceMounted();
                        else
                            if (mount())
                                ;    // onDeviceMounted() called as side-effect
                            else
                                return setStatus(MEDIASTAT_ERROR, OpenedHere);

                        if (isMounted(true))
                        {
                            // pretend we're NOTMOUNTED so setStatus emits
                            // a signal
                            m_Status = MEDIASTAT_NOTMOUNTED;
                            return setStatus(MEDIASTAT_MOUNTED, OpenedHere);
                        }
                        else if (m_MediaType == MEDIATYPE_DVD)
                        {
                            // pretend we're NOTMOUNTED so setStatus emits
                            // a signal
                            m_Status = MEDIASTAT_NOTMOUNTED;
                            return setStatus(MEDIASTAT_USEABLE, OpenedHere);
                        }
                        else
                            return setStatus(MEDIASTAT_NOTMOUNTED, OpenedHere);
                        break;
                    case CDS_AUDIO:
                        VERBOSE(VB_MEDIA, "found an audio disk");
                        m_MediaType = MEDIATYPE_AUDIO;
                        return setStatus(MEDIASTAT_USEABLE, OpenedHere);
                        break;
                    case CDS_MIXED:
                        m_MediaType = MEDIATYPE_MIXED;
                        VERBOSE(VB_MEDIA, "found a mixed CD");
                        // Note: Mixed mode CDs require an explixit mount call
                        //       since we'll usually want the audio portion.
                        // undefine ASSUME_WANT_AUDIO to change this behavior.
                        #ifdef ASSUME_WANT_AUDIO
                            return setStatus(MEDIASTAT_USEABLE, OpenedHere);
                        #else
                            mount();
                            if (isMounted(true))
                            {
                                // pretend we're NOTMOUNTED so setStatus
                                // emits a signal
                                m_Status = MEDIASTAT_NOTMOUNTED;
                                return setStatus(MEDIASTAT_MOUNTED,
                                                 OpenedHere);
                            }
                            else
                            {
                                return setStatus(MEDIASTAT_USEABLE,
                                                 OpenedHere);
                            }
                        #endif
                        break;
                    case CDS_NO_INFO:
                    case CDS_NO_DISC:
                        if (hasWritableMedia())
                        {
                            VERBOSE(VB_MEDIA, "found a blank or writable disk");
                            return setStatus(MEDIASTAT_UNFORMATTED, OpenedHere);
                        }

                        VERBOSE(VB_MEDIA, "found no disk");
                        m_MediaType = MEDIATYPE_UNKNOWN;
                        return setStatus(MEDIASTAT_UNKNOWN, OpenedHere);
                        break;
                    default:
                        VERBOSE(VB_MEDIA, "found unknown disk type: "
                                          + QString().setNum(type));
                        m_MediaType = MEDIATYPE_UNKNOWN;
                        return setStatus(MEDIASTAT_UNKNOWN, OpenedHere);
                }
            }
            else if (m_Status == MEDIASTAT_MOUNTED ||
                     m_Status == MEDIASTAT_NOTMOUNTED)
            {
                VERBOSE(VB_MEDIA, QString("Current status == ") +
                        MythMediaDevice::MediaStatusStrings[m_Status]);
                VERBOSE(VB_MEDIA, "Setting status to not mounted?");
                if (isMounted(true))
                    setStatus(MEDIASTAT_MOUNTED, OpenedHere);
                else
                    setStatus(MEDIASTAT_NOTMOUNTED, OpenedHere);
            }

            if (m_AllowEject)
                ioctl(m_DeviceHandle, CDROM_LOCKDOOR, 0);
        }// mediaChanged()
    } // isDeviceOpen();
    else
    {
        VERBOSE(VB_MEDIA, "Device not open - returning UNKNOWN");
        m_MediaType = MEDIATYPE_UNKNOWN;
        return setStatus(MEDIASTAT_UNKNOWN, OpenedHere);
    }

    if (OpenedHere)
        closeDevice();

#ifdef EXTRA_VERBOSITY
    VERBOSE(VB_MEDIA, QString("Returning ")
                      + MythMediaDevice::MediaStatusStrings[m_Status]);
#endif
    return m_Status;
}

MediaError MythCDROMLinux::lock()
{
    MediaError ret = MythMediaDevice::lock();
    if (ret == MEDIAERR_OK)
        ioctl(m_DeviceHandle, CDROM_LOCKDOOR, 1);

    return ret;
}

MediaError MythCDROMLinux::unlock()
{
    if (isDeviceOpen() || openDevice())
    {
#ifdef EXTRA_VERBOSITY
        VERBOSE(VB_MEDIA, LOC + ":unlock - Unlocking CDROM door");
#endif
        ioctl(m_DeviceHandle, CDROM_LOCKDOOR, 0);
    }
    else
    {
        VERBOSE(VB_GENERAL, "Failed to open device, CDROM try will remain "
                            "locked.");
    }

    return MythMediaDevice::unlock();
}

bool MythCDROMLinux::isSameDevice(const QString &path)
{
    dev_t new_rdev;
    struct stat sb;

    if (stat(path, &sb) < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC + ":isSameDevice() -- " +
                QString("Failed to stat '%1'")
                .arg(path) + ENO);
        return false;
    }
    new_rdev = sb.st_rdev;

    // Check against m_DevicePath...
    if (stat(m_DevicePath, &sb) < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC + ":isSameDevice() -- " +
                QString("Failed to stat '%1'")
                .arg(m_DevicePath) + ENO);
        return false;
    }
    return (sb.st_rdev == new_rdev);
}

#if defined(SG_IO) && defined(GPCMD_SET_STREAMING)
/*
 * \brief obtained from the mplayer project
 */
void MythCDROMLinux::setSpeed(int speed)
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

    if (stat(m_DevicePath, &st) == -1)
    {
        VERBOSE(VB_MEDIA, LOC_ERR +
                QString("setSpeed() Failed. device %1 not found")
                .arg(m_DevicePath));
        return;
    }

    if (!S_ISBLK(st.st_mode))
    {
        VERBOSE(VB_MEDIA, LOC_ERR + "setSpeed() Failed. Not a block device");
        return;
    }

    if ((fd = open(m_DevicePath, O_RDWR | O_NONBLOCK)) == -1)
    {
        VERBOSE(VB_MEDIA, LOC_ERR + "Changing CD/DVD speed needs write access");
        return;
    }

    if (speed < 0)
        speed = -1;

    switch(speed)
    {
        case 0: // don't touch speed setting
            return;
        case -1: // restore default value
        {
            rate = 0;
            buffer[0] = 4;
            VERBOSE(VB_MEDIA, LOC + ":setSpeed() - Restored CD/DVD Speed");
            break;
        }
        default:
        {
            // Speed in Kilobyte/Second. 177KB/s is the maximum data rate
            // for standard Audio CD's.

            rate = (speed > 0 && speed < 100) ? speed * 177 : speed;

            VERBOSE(VB_MEDIA,
                    (LOC + ":setSpeed() - Limiting CD/DVD Speed to %1KB/s")
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

    // On my system (2.6.18 + ide-cd),  SG_IO succeeds without doing anything,
    // while CDROM_SELECT_SPEED works...
    if (ioctl(fd, CDROM_SELECT_SPEED, speed) < 0)
    {
        if (ioctl(fd, SG_IO, &sghdr) < 0)
            VERBOSE(VB_MEDIA, LOC_ERR + "Limit CD/DVD Speed Failed");
    }
    else
        VERBOSE(VB_MEDIA, LOC + ":setSpeed() - CD/DVD Speed Set Successful");

    close(fd);
}
#endif

