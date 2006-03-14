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

    // Commented out functions are implemented in MythMediaDevice, so can't
    // be abstract here.

    //virtual MediaError testMedia(void) = 0;
    virtual bool mediaChanged(void) = 0;
    virtual bool checkOK(void) = 0;
    virtual bool openDevice(void);
    virtual MediaStatus checkMedia(void) = 0;
    //virtual MediaError eject(bool open_close = true) = 0;
    //virtual MediaError lock(void) = 0;
    //virtual MediaError unlock(void) = 0;

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
    virtual bool mediaChanged(void);
    virtual bool checkOK(void);
    virtual MediaStatus checkMedia(void);
    virtual MediaError eject(bool open_close = true);
    virtual MediaError lock(void);
    virtual MediaError unlock(void);
};

#endif
