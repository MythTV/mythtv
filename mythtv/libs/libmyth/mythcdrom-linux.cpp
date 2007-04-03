#include "mythcdrom.h"
#include <sys/ioctl.h>                // ioctls
#include <linux/cdrom.h>        // old ioctls for cdrom
#include <errno.h>
#include "mythcontext.h"
#include <linux/iso_fs.h>
#include <unistd.h>

#define ASSUME_WANT_AUDIO 1

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
                            mount(); // onDeviceMounted() called as side-effect

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
