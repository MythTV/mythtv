using namespace std;
#include "config.h"
#include "screensaver.h"
#include "screensaver-null.h"
#if defined(Q_WS_X11)
#include "screensaver-x11.h"
#endif
#ifdef CONFIG_DARWIN
#include "screensaver-osx.h"
#endif


ScreenSaverControl* ScreenSaverControl::get(void)
{
#if defined(Q_WS_X11)
    return new ScreenSaverX11();
#endif
#ifdef CONFIG_DARWIN
    return new ScreenSaverOSX();
#endif
    return new ScreenSaverNull();
}
