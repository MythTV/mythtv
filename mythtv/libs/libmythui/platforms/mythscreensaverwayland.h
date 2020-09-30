#ifndef MYTHSCREENSAVERWAYLAND_H
#define MYTHSCREENSAVERWAYLAND_H

// MythTV
#include "mythscreensaver.h"

class MythScreenSaverWayland : public MythScreenSaver
{
  public:
    void Disable() override;
    void Restore() override;
    void Reset() override;
    bool Asleep() override;
};

#endif
