#include "screensaver-osx.h"

void ScreenSaverOSX::Disable(void)
{
    IOPMAssertionCreateWithName(kIOPMAssertPreventUserIdleDisplaySleep,
				kIOPMAssertionLevelOn,
				CFSTR("MythTV Activity"),
				&iopm_id);
}

void ScreenSaverOSX::Restore(void)
{
    IOPMAssertionRelease(iopm_id);
}

void ScreenSaverOSX::Reset(void)
{
}

bool ScreenSaverOSX::Asleep(void)
{
    return false;
}
