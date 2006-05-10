#ifndef MYTHCDROM_H
#define MYTHCDROM_H

#include "mythmedia.h"

class MythCDROM : public MythMediaDevice
{
    Q_OBJECT
  public:
    MythCDROM(QObject* par, const char* DevicePath, bool SuperMount, 
              bool AllowEject);
    virtual ~MythCDROM() {};

    virtual bool mediaChanged(void) { return false; }
    virtual bool checkOK(void)      { return true; }
    virtual bool openDevice(void);
    virtual MediaStatus checkMedia(void)
    {
        return setStatus(MEDIASTAT_UNKNOWN, false);
    }

    static MythCDROM* get(QObject* par, const char* devicePath, bool SuperMount,
                          bool AllowEject);

  protected:
    virtual void onDeviceMounted();
};

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


class MythCDROMFreeBSD: public MythCDROM
{
public:
    MythCDROMFreeBSD(QObject* par, const char* DevicePath, bool SuperMount,
                     bool AllowEject):
        MythCDROM(par, DevicePath, SuperMount, AllowEject) {
    }

    virtual MediaError testMedia(void);
    virtual MediaError eject(bool open_close = true);
    virtual MediaError lock(void);
    virtual MediaError unlock(void);
};

#endif
