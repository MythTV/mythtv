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
        bool xscreensaverRunning;
        BOOL dpmsaware;
        BOOL dpmsenabled;
        bool dpmsdeactivated;
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

    int dummy;
    if ((d->state.dpmsaware = DPMSQueryExtension(qt_xdisplay(),&dummy,&dummy)))
    {
        CARD16 power_level;

	    /* If someone runs into X server weirdness that goes away when
	    * they externally disable DPMS, then the 'dpmsenabled' test should
	    * be short circuited by a call to 'DPMSCapable()'. Be sure to
	    * manually initialize dpmsenabled to false.
	    */

        DPMSInfo(qt_xdisplay(), &power_level, &(d->state.dpmsenabled));

        if (d->state.dpmsenabled)
            VERBOSE(VB_GENERAL, "DPMS is active.");
        else
            VERBOSE(VB_GENERAL, "DPMS is disabled.");

    } else {
        d->state.dpmsenabled = false;
        VERBOSE(VB_GENERAL, "DPMS is not supported.");
    }

    d->state.dpmsdeactivated = false;

}

ScreenSaverX11::~ScreenSaverX11() 
{
    /* Ensure DPMS gets left as it was found. */
    if (d->state.dpmsdeactivated)
        Restore();

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

    if (d->state.dpmsenabled)
    {
        d->state.dpmsdeactivated = true;
        DPMSDisable(qt_xdisplay());
        VERBOSE(VB_GENERAL, "DPMS Deactivated ");
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

    if (d->state.dpmsdeactivated)
    {
        d->state.dpmsdeactivated = false;
        DPMSEnable(qt_xdisplay());
        VERBOSE(VB_GENERAL, "DPMS Reactivated.");
    }

    if (d->state.xscreensaverRunning && d->resetTimer)
        d->resetTimer->stop();
}

void ScreenSaverX11::Reset(void) 
{
    XResetScreenSaver(qt_xdisplay());
    if (d->state.xscreensaverRunning)
        resetSlot();

    if (Asleep())
    {
        DPMSForceLevel(qt_xdisplay(), DPMSModeOn);
	// Calling XSync is necessary for the case when Myth executes
	// another application before the event loop regains control
        XSync(qt_xdisplay(), false);
    }
}

bool ScreenSaverX11::Asleep(void)
{
    if (!d->state.dpmsenabled)
        return 0;

    if (d->state.dpmsdeactivated)
        return 0;

    BOOL on;
    CARD16 power_level;

    DPMSInfo(qt_xdisplay(), &power_level, &on);

    return (power_level != DPMSModeOn);
}

void ScreenSaverX11::resetSlot() 
{
    myth_system(QString("xscreensaver-command -deactivate >&- 2>&- &")); 
}
