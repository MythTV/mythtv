// MythTV
#include "platforms/mythscreensaverwayland.h"

MythScreenSaverWayland::MythScreenSaverWayland(QObject* Parent)
  : MythScreenSaver(Parent)
{
}

void MythScreenSaverWayland::Disable()
{
}

void MythScreenSaverWayland::Restore()
{
}

void MythScreenSaverWayland::Reset()
{
}

bool MythScreenSaverWayland::Asleep()
{
    return false;
}
