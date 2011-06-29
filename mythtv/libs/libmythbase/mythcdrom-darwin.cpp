#include <errno.h>
#include <sys/fcntl.h>

#include <IOKit/storage/IOCDMediaBSDClient.h>
#include <IOKit/storage/IODVDMediaBSDClient.h>

#include "mythcdrom.h"
#include "mythcdrom-darwin.h"
#include "mythlogging.h"

#define LOC     QString("MythCDROMDarwin:")
#define LOC_ERR QString("mythcdrom-darwin, Error: ")

class MythCDROMDarwin: public MythCDROM
{
public:
    MythCDROMDarwin(QObject* par, const char* DevicePath,
                    bool SuperMount, bool AllowEject):
        MythCDROM(par, DevicePath, SuperMount, AllowEject) {};

    virtual void       setSpeed(int speed);
    virtual void       setSpeed(const char *device, int speed);
};

MythCDROM *GetMythCDROMDarwin(QObject* par, const char* devicePath,
                              bool SuperMount, bool AllowEject)
{
    return new MythCDROMDarwin(par, devicePath, SuperMount, AllowEject);
}

void MythCDROMDarwin::setSpeed(int speed)
{
    MythCDROMDarwin::setSpeed(m_DevicePath.toLocal8Bit().constData(), speed);
}

void MythCDROMDarwin::setSpeed(const char *device, int speed)
{
    int       fd;
    QString   raw = "/dev/r"; raw += device;
    uint16_t  spd = speed;


    fd = open(raw.toLocal8Bit().constData(), O_RDONLY | O_NONBLOCK);
    if (fd == -1)
    {
        LOG(VB_MEDIA, LOG_ERR, "setSpeed() can't open drive " + raw);
        return;
    }

    if (ioctl(fd, DKIOCCDSETSPEED, &spd) == -1 &&
        ioctl(fd, DKIOCDVDSETSPEED, &spd) == -1)
    {
        LOG(VB_MEDIA, LOG_ERR, "setSpeed() failed: " + ENO);
        close(fd);
        return;
    }
    LOG(VB_MEDIA, LOG_INFO, ":setSpeed() - CD/DVD Speed Set to " +
                            QString::number(spd));
    close(fd);
}
