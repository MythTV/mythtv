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

    virtual MediaError testMedia(void);
    bool mediaChanged(void);
    bool checkOK(void);
    virtual bool openDevice(void);
    virtual MediaStatus checkMedia(void);
    virtual MediaError eject(void);
    virtual MediaError lock(void);
    virtual MediaError unlock(void);

  protected:
    virtual void onDeviceMounted();
};

#endif
