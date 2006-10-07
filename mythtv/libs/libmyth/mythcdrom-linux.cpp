#include "mythcdrom.h"
#include "mythcdrom-linux.h"
#include <sys/ioctl.h>                // ioctls
#include <linux/cdrom.h>        // old ioctls for cdrom
#include <errno.h>
#include "mythcontext.h"
#include <linux/iso_fs.h>
#include <unistd.h>

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
    if (open_close)
        return (ioctl(m_DeviceHandle, CDROMEJECT) == 0) ? MEDIAERR_OK
                                                        : MEDIAERR_FAILED;
    else
        return (ioctl(m_DeviceHandle, CDROMCLOSETRAY) == 0) ? MEDIAERR_OK
                                                            : MEDIAERR_FAILED;
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
            //cout << "failed to open device - ";
            if (errno == EBUSY)
            {
                //cout << "errno == EBUSY" << endl;
                return isMounted(true) ? MEDIAERR_OK : MEDIAERR_FAILED;
            } 
            else 
            { 
                return MEDIAERR_FAILED; 
            }
        }
        //cout << "Opened it - ";
        OpenedHere = true;
    }

    // Since the device was is/was open we can get it's status...
    int Stat = ioctl(m_DeviceHandle, CDROM_DRIVE_STATUS, CDSL_CURRENT);
    
    // Be nice and close the device if we opened it, otherwise it might be locked when the user doesn't want it to be.
    if (OpenedHere)
        closeDevice();
    //cout << "Stat == " << Stat << endl;
    return (Stat >= 0) ? MEDIAERR_OK : MEDIAERR_FAILED;    
}

MediaStatus MythCDROMLinux::checkMedia()
{
    bool OpenedHere = false;    
   
    if (testMedia() != MEDIAERR_OK) 
    {
        //cout << "MythCDROMLinux::checkMedia - ";
        //cout << "Test Media result != MEDIAERR_OK" << endl;
        m_MediaType = MEDIATYPE_UNKNOWN;
        return setStatus(MEDIASTAT_UNKNOWN, OpenedHere);
    }

    //cout << "MythCDROMLinux::checkMedia - ";
    // If it's not already open we need to at least TRY to open it for most of these checks to work.
    if (!isDeviceOpen())
        OpenedHere = openDevice();

    if (isDeviceOpen()) 
    {
        //cout << "device is open - ";
        int ret = ioctl(m_DeviceHandle, CDROM_DRIVE_STATUS, CDSL_CURRENT);
        switch (ret) 
        {
            case CDS_DISC_OK:
                //cout << "disk ok - ";
                // If the disc is ok and we already know it's mediatype
                // returns MOUNTED.
                if (isMounted(true) && m_MediaType != MEDIATYPE_UNKNOWN)
                    return setStatus(MEDIASTAT_MOUNTED, OpenedHere);
                // If the disk is ok but not yet mounted we'll test it further down after this switch exits.
                break;
            case CDS_TRAY_OPEN:
            case CDS_NO_DISC:
                //cout << "Tray open or no disc" << endl;
                m_MediaType = MEDIATYPE_UNKNOWN;
                return setStatus(MEDIASTAT_OPEN, OpenedHere);
                break;
            case CDS_NO_INFO:
            case CDS_DRIVE_NOT_READY:
                //cout << "No info or drive not ready" << endl;
                m_MediaType = MEDIATYPE_UNKNOWN;
                return setStatus(MEDIASTAT_UNKNOWN, OpenedHere);
            default:
                //cout << "unknown result from ioctl (" << ret << ")" << endl;
                m_MediaType = MEDIATYPE_UNKNOWN;
                return setStatus(MEDIASTAT_UNKNOWN, OpenedHere);
        }

        if (mediaChanged()) 
        {
            //cout << "media changed - ";
            // Regardless of the actual status lie here and say
            // it's open for now, so we can cover the case of a missed open.
            return setStatus(MEDIASTAT_OPEN, OpenedHere);
        } 
        else 
        {
            //cout << "media unchanged - ";
            if ((m_Status == MEDIASTAT_OPEN) || 
                (m_Status == MEDIASTAT_UNKNOWN)) 
            {
                //cout << "Current status == " << MythMediaDevice::MediaStatusStrings[m_Status]  << endl;
                int type = ioctl(m_DeviceHandle, CDROM_DISC_STATUS, CDSL_CURRENT);
                switch (type) 
                {
                    case CDS_DATA_1:
                    case CDS_DATA_2:
                        m_MediaType = MEDIATYPE_DATA;
                        //cout << "found a data disk" << endl;
                        //grab information from iso9660 (& udf)
                        struct iso_primary_descriptor buf;
                        lseek(this->m_DeviceHandle,(off_t) 2048*16,SEEK_SET);
                        read(this->m_DeviceHandle, &buf,2048);
                        this->m_VolumeID = QString(buf.volume_id)
                                                       .stripWhiteSpace();
                        this->m_KeyID = QString("%1%2")
                                      .arg(this->m_VolumeID)
                                      .arg(QString(buf.creation_date).left(16));

                        // attempt to mount the disc
                        // the base class's onDeviceMounted will do fine
                        // grained detection of the type of data on this disc
                        mount();

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
                        //cout << "found an audio disk" << endl;
                        m_MediaType = MEDIATYPE_AUDIO;
                        return setStatus(MEDIASTAT_USEABLE, OpenedHere);
                        break;
                    case CDS_MIXED:
                        m_MediaType = MEDIATYPE_MIXED;
                        //cout << "found a mixed CD" << endl;
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
                        //cout << "found no disk" << endl;
                        m_MediaType = MEDIATYPE_UNKNOWN;
                        return setStatus(MEDIASTAT_UNKNOWN, OpenedHere);
                        break;
                    default:
                        //cout << "found unknown disk type" << endl;
                        fprintf(stderr, "Unknown data type: %d\n", type);
                        m_MediaType = MEDIATYPE_UNKNOWN;
                        return setStatus(MEDIASTAT_UNKNOWN, OpenedHere);
                }            
            }
            else if (m_Status == MEDIASTAT_MOUNTED || 
                     m_Status == MEDIASTAT_NOTMOUNTED) 
            {
                //cout << "current status == " << MythMediaDevice::MediaStatusStrings[m_Status] << " setting status to not mounted - ";
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
        //cout << "device not open returning unknown" << endl;
        m_MediaType = MEDIATYPE_UNKNOWN;
        return setStatus(MEDIASTAT_UNKNOWN, OpenedHere);
    }

    if (OpenedHere)
        closeDevice();

    //cout << "returning " << MythMediaDevice::MediaStatusStrings[m_Status] << endl;
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
        //cout <<  "Unlocking CDROM door" << endl;
        ioctl(m_DeviceHandle, CDROM_LOCKDOOR, 0);
    }
    else
    {
        VERBOSE(VB_GENERAL, "Failed to open device, CDROM try will remain "
                            "locked.");
    }

    return MythMediaDevice::unlock();
}
