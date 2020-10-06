// MythTV
#include "platforms/mythscreensaverosx.h"

MythScreenSaverOSX::MythScreenSaverOSX(QObject* Parent)
  : MythScreenSaver(Parent)
{
}

void MythScreenSaverOSX::Disable()
{
    IOPMAssertionCreateWithName(kIOPMAssertPreventUserIdleDisplaySleep,
				kIOPMAssertionLevelOn,
				CFSTR("MythTV Activity"),
				&iopm_id);
}

void MythScreenSaverOSX::Restore()
{
    IOPMAssertionRelease(iopm_id);
}

void MythScreenSaverOSX::Reset()
{
}

bool MythScreenSaverOSX::Asleep()
{
    return false;
}
