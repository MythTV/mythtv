#include <errno.h>
#include <sys/fcntl.h>
#include <unistd.h> // for close definition

#include <IOKit/storage/IOCDMediaBSDClient.h>
#include <IOKit/storage/IODVDMediaBSDClient.h>

#include "mythcdrom.h"
#include "mythcdrom-darwin.h"
#include "mythlogging.h"

#define LOC     QString("MythCDROMDarwin:")

class MythCDROMDarwin: public MythCDROM
{
public:
    MythCDROMDarwin(QObject* par, const char* DevicePath,
                    bool SuperMount, bool AllowEject):
        MythCDROM(par, DevicePath, SuperMount, AllowEject) {};

    virtual void setDeviceSpeed(const char *device, int speed);
};

MythCDROM *GetMythCDROMDarwin(QObject* par, const char* devicePath,
                              bool SuperMount, bool AllowEject)
{
    return new MythCDROMDarwin(par, devicePath, SuperMount, AllowEject);
}

void MythCDROMDarwin::setDeviceSpeed(const char *device, int speed)
{
    int       fd;
    QString   raw = device;
    uint16_t  spd = speed;


    fd = open(raw.toLocal8Bit().constData(), O_RDONLY | O_NONBLOCK);
    if (fd == -1)
    {
        LOG(VB_MEDIA, LOG_ERR, LOC +
            "setDeviceSpeed() can't open drive " + raw);
        return;
    }

    if (ioctl(fd, DKIOCCDSETSPEED, &spd) == -1 &&
        ioctl(fd, DKIOCDVDSETSPEED, &spd) == -1)
    {
        LOG(VB_MEDIA, LOG_ERR, LOC + "setDeviceSpeed() failed: " + ENO);
        close(fd);
        return;
    }

    LOG(VB_MEDIA, LOG_INFO, LOC + "setDeviceSpeed() - CD/DVD speed now " +
        QString::number(spd));

    close(fd);
}
