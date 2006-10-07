#ifndef MYTHCDROM_H
#define MYTHCDROM_H

#include "mythmedia.h"

class MPUBLIC MythCDROM : public MythMediaDevice
{
    Q_OBJECT
  protected:
    MythCDROM(QObject* par, const char* DevicePath, bool SuperMount,
              bool AllowEject);
  public:
    virtual ~MythCDROM() {};

    virtual bool mediaChanged(void) { return false; }
    virtual bool checkOK(void)      { return true; }
    virtual bool openDevice(void);
    virtual MediaStatus checkMedia(void)
    {
        return setStatus(MEDIASTAT_UNKNOWN, false);
    }

    static MythCDROM* get(QObject* par, const char* devicePath,
                                  bool SuperMount, bool AllowEject);

  protected:
    virtual void onDeviceMounted();
};

#endif
