#include "screensaver-x11.h"

#include <X11/Xlib.h>

class ScreenSaverX11Private 
{
    struct 
    {
        bool saved;
        int timeout;
        int interval;
        int preferblank;
        int allowexposure;
    } state;

    friend class ScreenSaverX11;
};

ScreenSaverX11::ScreenSaverX11() 
{
    d = new ScreenSaverX11Private();
}

ScreenSaverX11::~ScreenSaverX11() 
{
    delete d;
}

void ScreenSaverX11::Disable(void) 
{
    if (!d->state.saved)
    {
        XGetScreenSaver(qt_xdisplay(),
                        &d->state.timeout, &d->state.interval,
                        &d->state.preferblank, 
                        &d->state.allowexposure);
        d->state.saved = true;
    }

    XResetScreenSaver(qt_xdisplay());
    XSetScreenSaver(qt_xdisplay(), 0, 0, 0, 0);
}

void ScreenSaverX11::Restore(void) 
{
    XResetScreenSaver(qt_xdisplay());
    XSetScreenSaver(qt_xdisplay(),
                    d->state.timeout, d->state.interval,
                    d->state.preferblank, 
                    d->state.allowexposure);
    d->state.saved = false;
}

void ScreenSaverX11::Reset(void) 
{
    XResetScreenSaver(qt_xdisplay());
}

