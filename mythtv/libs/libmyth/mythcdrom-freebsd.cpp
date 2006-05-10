#include "mythcdrom.h"
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/cdio.h>
#include "mythcontext.h"


#define ASSUME_WANT_AUDIO 1

MediaError MythCDROMFreeBSD::eject(bool open_close)
{
    if (open_close)
        return (ioctl(m_DeviceHandle, CDIOCEJECT) == 0) ? MEDIAERR_OK :
                                                          MEDIAERR_FAILED;
    else
        return MEDIAERR_UNSUPPORTED;
}

// Helper function, perform a sanity check on the device
MediaError MythCDROMFreeBSD::testMedia()
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

    // Be nice and close the device if we opened it, otherwise it might be locked when the user doesn't want it to be.
    if (OpenedHere)
        closeDevice();

    return MEDIAERR_OK;
}

MediaError MythCDROMFreeBSD::lock() 
{
    MediaError ret = MythMediaDevice::lock();
    if (ret == MEDIAERR_OK)
        ioctl(m_DeviceHandle, CDIOCPREVENT);

    return ret;
}

MediaError MythCDROMFreeBSD::unlock() 
{
    if (isDeviceOpen() || openDevice()) 
    { 
        //cout <<  "Unlocking CDROM door" << endl;
        ioctl(m_DeviceHandle, CDIOCALLOW);
    }
    else
    {
        VERBOSE(VB_GENERAL, "Failed to open device, CDROM try will remain "
                            "locked.");
    }

    return MythMediaDevice::unlock();
}
