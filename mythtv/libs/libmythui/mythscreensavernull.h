#ifndef MYTH_SCREENSAVER_NULL_H
#define MYTH_SCREENSAVER_NULL_H

// MythTV
#include "mythscreensaver.h"

class MythScreenSaverNull : public MythScreenSaver
{
  public:
    MythScreenSaverNull() = default;
   ~MythScreenSaverNull() override = default;

    void Disable() override;
    void Restore() override;
    void Reset() override;
    bool Asleep() override;
};

#endif

