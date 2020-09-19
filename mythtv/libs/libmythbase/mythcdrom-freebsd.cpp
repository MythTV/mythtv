#include <cerrno>
#include <sys/cdio.h>
#include <sys/ioctl.h>

#include "mythcdrom.h"
#include "mythcdrom-freebsd.h"
#include "mythlogging.h"


#define ASSUME_WANT_AUDIO 1

class MythCDROMFreeBSD: public MythCDROM
{
public:
    MythCDROMFreeBSD(QObject* par, const QString& DevicePath, bool SuperMount,
                     bool AllowEject):
        MythCDROM(par, DevicePath, SuperMount, AllowEject) {
    }

    MythMediaError testMedia(void) override; // MythMediaDevice
    MythMediaError eject(bool open_close = true) override; // MythMediaDevice
    MythMediaError lock(void) override; // MythMediaDevice
    MythMediaError unlock(void) override; // MythMediaDevice
};

MythCDROM *GetMythCDROMFreeBSD(QObject* par, const QString& devicePath,
                               bool SuperMount, bool AllowEject)
{
    return new MythCDROMFreeBSD(par, devicePath, SuperMount, AllowEject);
}

MythMediaError MythCDROMFreeBSD::eject(bool open_close)
{
    if (!isDeviceOpen())
        openDevice();

    if (open_close)
        return (ioctl(m_deviceHandle, CDIOCEJECT) == 0) ? MEDIAERR_OK :
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
        ioctl(m_deviceHandle, CDIOCPREVENT);

    return ret;
}

MythMediaError MythCDROMFreeBSD::unlock()
{
    if (isDeviceOpen() || openDevice())
    {
#if 0
        LOG(VB_GENERAL, LOG_DEBUG, "Unlocking CDROM door");
#endif
        ioctl(m_deviceHandle, CDIOCALLOW);
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, "Failed to open device, CDROM tray will "
                                  "remain locked.");
    }

    return MythMediaDevice::unlock();
}
