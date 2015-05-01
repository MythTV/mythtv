#include "config.h"
#include "screensaver.h"
#include "screensaver-null.h"

#ifdef USING_DBUS
#include "screensaver-dbus.h"
#endif // USING_DBUS

#ifdef USING_X11
#include "screensaver-x11.h"
#endif // USING_X11

#if CONFIG_DARWIN
#include "screensaver-osx.h"
#endif

QEvent::Type ScreenSaverEvent::kEventType =
    (QEvent::Type) QEvent::registerEventType();

ScreenSaver* ScreenSaverSingleton = NULL;

ScreenSaverControl::ScreenSaverControl()
{
    if (!ScreenSaverSingleton)
    {
#if defined(USING_DBUS)
        ScreenSaverSingleton = new ScreenSaverDBus();
#elif defined(USING_X11)
        ScreenSaverSingleton = new ScreenSaverX11();
#elif CONFIG_DARWIN
        ScreenSaverSingleton = new ScreenSaverOSX();
#else
        ScreenSaverSingleton = new ScreenSaverNull();
#endif
    }
    
    return ScreenSaverSingleton;
}

ScreenSaverControl::~ScreenSaverControl() {
    delete ScreenSaverSingleton;
}

void ScreenSaverControl::Disable(void) {
    ScreenSaverSingleton->Disable();
}

void ScreenSaverControl::Restore(void) {
    ScreenSaverSingleton->Restore();
}

void ScreenSaverControl::Reset(void) {
    ScreenSaverSingleton->Reset();
}

bool ScreenSaverControl::Asleep(void) {
    return ScreenSaverSingleton->Asleep();
}
