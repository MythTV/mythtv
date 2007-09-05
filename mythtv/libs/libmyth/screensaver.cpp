using namespace std;
#include "config.h"
#include "screensaver.h"
#include "screensaver-null.h"

#ifdef USING_X11
#include "screensaver-x11.h"
#endif // USING_X11

#ifdef CONFIG_DARWIN
#include "screensaver-osx.h"
#endif


ScreenSaverControl* ScreenSaverSingleton = NULL;

ScreenSaverControl* ScreenSaverControl::get(void)
{
    if (!ScreenSaverSingleton)
    {
  
#if defined(USING_X11)
        ScreenSaverSingleton = new ScreenSaverX11();
#elif defined(CONFIG_DARWIN)
        ScreenSaverSingleton = new ScreenSaverOSX();
#else
        ScreenSaverSingleton = new ScreenSaverNull();
#endif
    }
    
    return ScreenSaverSingleton;
}
