#include "screensaver.h"

#include <X11/Xlib.h>

class ScreenSaverPrivate {
    struct {
        bool saved;
        int timeout;
        int interval;
        int preferblank;
        int allowexposure;
    } state;

     friend class ScreenSaverControl;
};

ScreenSaverControl::ScreenSaverControl() {
     d = new ScreenSaverPrivate();
}

ScreenSaverControl::~ScreenSaverControl() {
     if (d)
          delete d;
}

void ScreenSaverControl::Disable(void) {
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

void ScreenSaverControl::Restore(void) {
    XResetScreenSaver(qt_xdisplay());
    XSetScreenSaver(qt_xdisplay(),
                    d->state.timeout, d->state.interval,
                    d->state.preferblank, 
                    d->state.allowexposure);
    d->state.saved = false;
}

void ScreenSaverControl::Reset(void) {
    XResetScreenSaver(qt_xdisplay());
}
