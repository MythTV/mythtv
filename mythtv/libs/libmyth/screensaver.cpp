using namespace std;
#include "config.h"
#include "screensaver.h"
#include "screensaver-null.h"

#ifdef _WIN32
#undef Q_WS_X11 /* Even if we have an X server in WIN32, don't use X11... */
#endif // _WIN32

#if defined(Q_WS_X11)
#include "screensaver-x11.h"
#endif
#ifdef CONFIG_DARWIN
#include "screensaver-osx.h"
#endif


ScreenSaverControl* ScreenSaverSingleton = NULL;

ScreenSaverControl* ScreenSaverControl::get(void)
{
    if (!ScreenSaverSingleton)
    {
  
#if defined(Q_WS_X11)
        ScreenSaverSingleton = new ScreenSaverX11();
#elif defined(CONFIG_DARWIN)
        ScreenSaverSingleton = new ScreenSaverOSX();
#else
        ScreenSaverSingleton = new ScreenSaverNull();
#endif
    }
    
    return ScreenSaverSingleton;
}
