#include <errno.h>
#include <sys/ioctl.h>
#include <sys/cdio.h>

#include "mythcdrom.h"
#include "mythcdrom-freebsd.h"
#include "mythverbose.h"


#define ASSUME_WANT_AUDIO 1

class MythCDROMFreeBSD: public MythCDROM
{
public:
    MythCDROMFreeBSD(QObject* par, const char* DevicePath, bool SuperMount,
                     bool AllowEject):
        MythCDROM(par, DevicePath, SuperMount, AllowEject) {
    }

    virtual MythMediaError testMedia(void);
    virtual MythMediaError eject(bool open_close = true);
    virtual MythMediaError lock(void);
    virtual MythMediaError unlock(void);
};

MythCDROM *GetMythCDROMFreeBSD(QObject* par, const char* devicePath,
                               bool SuperMount, bool AllowEject)
{
    return new MythCDROMFreeBSD(par, devicePath, SuperMount, AllowEject);
}

MythMediaError MythCDROMFreeBSD::eject(bool open_close)
{
    if (!isDeviceOpen())
        openDevice();

    if (open_close)
        return (ioctl(m_DeviceHandle, CDIOCEJECT) == 0) ? MEDIAERR_OK :
                                                          MEDIAERR_FAILED;
    else
        return MEDIAERR_UNSUPPORTED;
}

// Helper function, perform a sanity check on the device
MythMediaError MythCDROMFreeBSD::testMedia()
{
    bool OpenedHere = false;
    if (!isDeviceOpen()) 
    {
        if (!openDevice()) 
        {
            if (errno == EBUSY)
            {
                return isMounted() ? MEDIAERR_OK : MEDIAERR_FAILED;
            } 
            else 
            { 
                return MEDIAERR_FAILED; 
            }
        }
        OpenedHere = true;
    }

    // Be nice and close the device if we opened it, otherwise it might be locked when the user doesn't want it to be.
    if (OpenedHere)
        closeDevice();

    return MEDIAERR_OK;
}

MythMediaError MythCDROMFreeBSD::lock() 
{
    MythMediaError ret = MythMediaDevice::lock();
    if (ret == MEDIAERR_OK)
        ioctl(m_DeviceHandle, CDIOCPREVENT);

    return ret;
}

MythMediaError MythCDROMFreeBSD::unlock() 
{
    if (isDeviceOpen() || openDevice()) 
    { 
        //VERBOSE(VB_GENERAL, "Unlocking CDROM door");
        ioctl(m_DeviceHandle, CDIOCALLOW);
    }
    else
    {
        VERBOSE(VB_GENERAL, "Failed to open device, CDROM try will remain "
                            "locked.");
    }

    return MythMediaDevice::unlock();
}
