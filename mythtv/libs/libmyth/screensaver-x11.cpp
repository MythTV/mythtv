#define QT_CLEAN_NAMESPACE // no qt 1.x compatability, INT32 conflicts with X
#include "screensaver-x11.h"
#include <qtimer.h>

#include <X11/Xlib.h>

extern "C" {
#include <X11/extensions/dpms.h>
}

#include "mythcontext.h"
#include "util.h"

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
        bool xscreensaverRunning;
    } state;

    QTimer *resetTimer;
    int timeoutInterval;

    friend class ScreenSaverX11;

};

ScreenSaverX11::ScreenSaverX11() 
{
    d = new ScreenSaverX11Private();
    d->state.xscreensaverRunning = 
                  (myth_system("xscreensaver-command -version >&- 2>&-") == 0); 
    if (d->state.xscreensaverRunning)
    {
        d->resetTimer = new QTimer(this);
        connect(d->resetTimer, SIGNAL(timeout()), this, SLOT(resetSlot()));

        d->timeoutInterval = -1;
        VERBOSE(VB_GENERAL, "XScreenSaver support enabled");
    }
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

    if (d->state.xscreensaverRunning)
    {
        if (d->resetTimer)
            d->resetTimer->stop();

        if (d->timeoutInterval == -1)
        {
            d->timeoutInterval = 
                gContext->GetNumSettingOnHost("xscreensaverInterval",
                                          gContext->GetHostName(),
                                          60) * 1000;
        }
        if (d->timeoutInterval > 0)
        {
            d->resetTimer->start(d->timeoutInterval, FALSE);
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

    if (d->state.xscreensaverRunning && d->resetTimer)
        d->resetTimer->stop();
}

void ScreenSaverX11::Reset(void) 
{
    XResetScreenSaver(qt_xdisplay());
    if (d->state.xscreensaverRunning)
        resetSlot();

    int nothing;
    if (DPMSQueryExtension(qt_xdisplay(), &nothing, &nothing))
    {
        BOOL on;
        CARD16 power_level;
        if (!d->state.dpmsdisabled) 
        {
            DPMSInfo(qt_xdisplay(), &power_level, &on);
            if (power_level != DPMSModeOn)
                DPMSForceLevel(qt_xdisplay(), DPMSModeOn);
        }
    }
}

void ScreenSaverX11::resetSlot() 
{
    myth_system(QString("xscreensaver-command -deactivate >&- 2>&- &")); 
}
