#ifndef MYTHSCREENSAVERDRM_H
#define MYTHSCREENSAVERDRM_H

// MythTV
#include "mythscreensaver.h"

class MythScreenSaverDRM : public MythScreenSaver
{
  public:
    void Disable() override;
    void Restore() override;
    void Reset() override;
    bool Asleep() override;
};

#endif
