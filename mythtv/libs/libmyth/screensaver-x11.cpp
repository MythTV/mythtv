#define QT_CLEAN_NAMESPACE // no qt 1.x compatability, INT32 conflicts with X
#include "screensaver-x11.h"

#include <X11/Xlib.h>

extern "C" {
#include <X11/extensions/dpms.h>
}

#include "mythcontext.h"

class ScreenSaverX11Private 
{
    struct 
    {
        bool saved;
        int timeout;
        int interval;
        int preferblank;
        int allowexposure;
        bool dpmsdisabled;
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

    int nothing;

    if (DPMSQueryExtension(qt_xdisplay(), &nothing, &nothing))
    {
        BOOL on;
        CARD16 power_level;

        DPMSInfo(qt_xdisplay(), &power_level, &on);
        if (on)
        {
            d->state.dpmsdisabled = true;
            DPMSDisable(qt_xdisplay());       // monitor powersave off
            VERBOSE(VB_GENERAL, "Disable DPMS");
        }
    }
}

void ScreenSaverX11::Restore(void) 
{
    XResetScreenSaver(qt_xdisplay());
    XSetScreenSaver(qt_xdisplay(),
                    d->state.timeout, d->state.interval,
                    d->state.preferblank, 
                    d->state.allowexposure);
    d->state.saved = false;

    if (d->state.dpmsdisabled)
    {
        int nothing;
        d->state.dpmsdisabled = false;
        if (DPMSQueryExtension(qt_xdisplay(), &nothing, &nothing))
        {
            DPMSEnable(qt_xdisplay());
            VERBOSE(VB_GENERAL, "Enable DPMS");
        }
    }
}

void ScreenSaverX11::Reset(void) 
{
    XResetScreenSaver(qt_xdisplay());
}

