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
