#include "screensaver.h"

#include <X11/Xlib.h>
#include <iostream>

using namespace std;

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
    cerr << "Disabling screensaver\n";
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
    cerr << "Restoring screensaver\n";
    XResetScreenSaver(qt_xdisplay());
    XSetScreenSaver(qt_xdisplay(),
                    d->state.timeout, d->state.interval,
                    d->state.preferblank, 
                    d->state.allowexposure);
    d->state.saved = false;
}

void ScreenSaverControl::Reset(void) {
    cerr << "Resetting screensaver\n";
    XResetScreenSaver(qt_xdisplay());
}
