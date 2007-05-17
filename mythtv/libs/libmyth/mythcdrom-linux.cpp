#include "mythcdrom.h"
#include "mythcdrom-linux.h"
#include <sys/ioctl.h>                // ioctls
#include <linux/cdrom.h>        // old ioctls for cdrom
#include <scsi/sg.h>
#include <fcntl.h>
#include <errno.h>
#include "mythcontext.h"
#include <linux/iso_fs.h>
#include <unistd.h>

#define LOC QString("mythcdrom-linux: ")
#define LOC_ERR QString("mythcdrom-linux, Error: ")

#define ASSUME_WANT_AUDIO 1

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
};

MythCDROM *GetMythCDROMLinux(QObject* par, const char* devicePath,
                             bool SuperMount, bool AllowEject)
{
    return new MythCDROMLinux(par, devicePath, SuperMount, AllowEject);
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
        if (ioctl(m_DeviceHandle, CDROM_DRIVE_STATUS) == CDS_TRAY_OPEN)
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
            //VERBOSE(VB_MEDIA, "MythCDROMLinux::testMedia - failed to open "
            //                  + getDevicePath() + ENO);
            if (errno == EBUSY)
                return isMounted(true) ? MEDIAERR_OK : MEDIAERR_FAILED;
            else 
                return MEDIAERR_FAILED; 
        }
        //VERBOSE(VB_MEDIA, "MythCDROMLinux::testMedia - Opened device");
        OpenedHere = true;
    }

    // Since the device was is/was open we can get it's status...
    int Stat = ioctl(m_DeviceHandle, CDROM_DRIVE_STATUS, CDSL_CURRENT);
    
    // Be nice and close the device if we opened it, otherwise it might be locked when the user doesn't want it to be.
    if (OpenedHere)
        closeDevice();

    if (Stat == -1)
    {
        VERBOSE(VB_MEDIA,
                "MythCDROMLinux::testMedia - Failed to get drive status of "
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

    // If it's not already open we need to at least TRY to open it for most of these checks to work.
    if (!isDeviceOpen())
        OpenedHere = openDevice();

    if (isDeviceOpen()) 
    {
        //VERBOSE(VB_MEDIA, "MythCDROMLinux::checkMedia - Device is open...");
        int ret = ioctl(m_DeviceHandle, CDROM_DRIVE_STATUS, CDSL_CURRENT);
        switch (ret) 
        {
            case CDS_DISC_OK:
                VERBOSE(VB_MEDIA, getDevicePath() + " Disk OK...");
                // If the disc is ok and we already know it's mediatype
                // returns MOUNTED.
                if (isMounted(true) && m_MediaType != MEDIATYPE_UNKNOWN)
                    return setStatus(MEDIASTAT_MOUNTED, OpenedHere);
                // If the disk is ok but not yet mounted we'll test it further down after this switch exits.
                break;
            case CDS_TRAY_OPEN:
                VERBOSE(VB_MEDIA, getDevicePath() + " Tray open");
                m_MediaType = MEDIATYPE_UNKNOWN;
                return setStatus(MEDIASTAT_OPEN, OpenedHere);
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
            //VERBOSE(VB_MEDIA, getDevicePath() + " Media unchanged...");
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
                            mount();  // onDeviceMounted() called as side-effect

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
                        // Note: Mixed mode CDs require an explixit mount call since we'll usually want the audio portion.
                        //         undefine ASSUME_WANT_AUDIO to change this behavior.
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

    //VERBOSE(VB_MEDIA, QString("Returning ")
    //                  + MythMediaDevice::MediaStatusStrings[m_Status]);
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
        //VERBOSE(VB_MEDIA, "MythCDROMLinux::unlock - Unlocking CDROM door");
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
        VERBOSE(VB_IMPORTANT, "MythCDROMLinux::SameDevice() -- " +
                QString("Failed to stat '%1'")
                .arg(path) + ENO);
        return false;
    }
    new_rdev = sb.st_rdev;

    // Check against m_DevicePath...
    if (stat(m_DevicePath, &sb) < 0)
    {
        VERBOSE(VB_IMPORTANT, "MythCDROMLinux::SameDevice() -- " +
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

    if (stat(m_DevicePath, &st) == -1 ) 
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("setSpeed() Failed. device %1 not found")
                .arg(m_DevicePath));
        return;
    }

    if (!S_ISBLK(st.st_mode)) 
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + 
                "MythCDROMLinux::SetSpeed() Failed. Not a block device");
        return;
    }

    if ((fd = open(m_DevicePath, O_RDWR | O_NONBLOCK)) == -1)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + 
                "Changing CD/DVD speed needs write access");
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
            VERBOSE(VB_IMPORTANT, LOC + "Restored CD/DVD Speed");
            break;
        }
        default:
        {
            // Speed in Kilobyte/Second. 177KB/s is the maximum data rate
            // for standard Audio CD's.

            rate = (speed > 0 && speed < 100) ? speed * 177 : speed;

            VERBOSE(VB_IMPORTANT, LOC + QString("Limit CD/DVD Speed to %1KB/s")
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
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Limit CD/DVD Speed Failed");
    }
    else 
        VERBOSE(VB_IMPORTANT, LOC + "CD/DVD Speed Set Successful");
    
    close(fd);
}
#endif

