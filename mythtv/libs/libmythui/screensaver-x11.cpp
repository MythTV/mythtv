
// Own header
#include "screensaver-x11.h"

// QT headers
#include <QDateTime>
#include <QTimer>
#include <QX11Info>

// Mythdb headers
#include "mythverbose.h"
#include "mythdb.h"

// Mythui headers
#include "mythsystem.h"
#include "mythxdisplay.h"

// X11 headers
#include <X11/Xlib.h>

extern "C" {
#include <X11/extensions/dpms.h>
}

#define LOC      QString("ScreenSaverX11Private: ")
#define LOC_WARN QString("ScreenSaverX11Private, Warning: ")
#define LOC_ERR  QString("ScreenSaverX11Private, Error: ")

class ScreenSaverX11Private
{
    friend class ScreenSaverX11;

  public:
    ScreenSaverX11Private(ScreenSaverX11 *outer) :
        m_dpmsaware(false),           m_dpmsdeactivated(false),
        m_xscreensaverRunning(false), m_gscreensaverRunning(false),
        m_dpmsenabled(false),
        m_timeoutInterval(-1),        m_resetTimer(NULL),
        m_display(NULL)
    {
        const uint flags = kMSDontBlockInputDevs | kMSDontDisableDrawing;
        m_xscreensaverRunning =
                myth_system("xscreensaver-command -version >&- 2>&-",
                            flags) == 0;
        m_gscreensaverRunning =
                myth_system("gnome-screensaver-command --help >&- 2>&-",
                            flags) == 0;

        if (IsScreenSaverRunning())
        {
            m_resetTimer = new QTimer(outer);
            m_resetTimer->setSingleShot(false);
            QObject::connect(m_resetTimer, SIGNAL(timeout()),
                             outer, SLOT(resetSlot()));
            if (m_xscreensaverRunning)
                VERBOSE(VB_GENERAL, LOC + "XScreenSaver support enabled");
            if (m_gscreensaverRunning)
                VERBOSE(VB_GENERAL, LOC + "Gnome screen saver support enabled");
        }

        m_display = OpenMythXDisplay();
        if (m_display)
        {
            int dummy0, dummy1;
            m_dpmsaware = DPMSQueryExtension(m_display->GetDisplay(),
                                            &dummy0, &dummy1);
        }
        else
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to open connection to X11 server");
        }

        if (m_dpmsaware)
        {
            CARD16 power_level;

            /* If someone runs into X server weirdness that goes away when
             * they externally disable DPMS, then the 'dpmsenabled' test should
             * be short circuited by a call to 'DPMSCapable()'. Be sure to
             * manually initialize dpmsenabled to false.
             */

            DPMSInfo(m_display->GetDisplay(), &power_level, &m_dpmsenabled);

            if (m_dpmsenabled)
                VERBOSE(VB_GENERAL, "DPMS is active.");
            else
                VERBOSE(VB_GENERAL, "DPMS is disabled.");
        }
        else
        {
            VERBOSE(VB_GENERAL, "DPMS is not supported.");
        }
    }

    ~ScreenSaverX11Private()
    {
        // m_resetTimer deleted by ScreenSaverX11 QObject dtor
        delete m_display;
    }

    bool IsScreenSaverRunning(void) const
    {
        return m_xscreensaverRunning || m_gscreensaverRunning;
    }

    bool IsDPMSEnabled(void) const { return m_dpmsenabled; }

    void StopTimer(void)
    {
        VERBOSE(VB_PLAYBACK, LOC + "StopTimer");
        if (m_resetTimer)
            m_resetTimer->stop();
    }

    void StartTimer(void)
    {
        VERBOSE(VB_PLAYBACK, LOC + "StartTimer");
        if (m_resetTimer)
            m_resetTimer->start(m_timeoutInterval);
    }

    void ResetTimer(void)
    {
        VERBOSE(VB_PLAYBACK, LOC + "ResetTimer -- begin");

        StopTimer();

        if (m_timeoutInterval == -1)
        {
            m_timeoutInterval = GetMythDB()->GetNumSettingOnHost(
                "xscreensaverInterval", GetMythDB()->GetHostName(), 50) * 1000;
        }

        if (m_timeoutInterval > 0)
            StartTimer();

        VERBOSE(VB_PLAYBACK, LOC + "ResetTimer -- end");
    }

    // DPMS
    bool DeactivatedDPMS(void) const
    {
        return m_dpmsdeactivated;
    }

    void DisableDPMS(void)
    {
        if (IsDPMSEnabled() && m_display)
        {
            m_dpmsdeactivated = true;
            Status status = DPMSDisable(m_display->GetDisplay());
            m_display->Sync();
            VERBOSE(VB_GENERAL, LOC +
                QString("DPMS Deactivated %1").arg(status));
        }
    }

    void RestoreDPMS(void)
    {
        if (m_dpmsdeactivated && m_display)
        {
            m_dpmsdeactivated = false;
            Status status = DPMSEnable(m_display->GetDisplay());
            m_display->Sync();
            VERBOSE(VB_GENERAL, LOC +
                QString("DPMS Reactivated %1").arg(status));
        }
    }

    void SaveScreenSaver(void)
    {
        if (!m_state.saved && m_display)
        {
            XGetScreenSaver(m_display->GetDisplay(), &m_state.timeout,
                            &m_state.interval, &m_state.preferblank,
                            &m_state.allowexposure);
            m_state.saved = true;
        }
    }

    void RestoreScreenSaver(void)
    {
        if (m_state.saved && m_display)
        {
            XSetScreenSaver(m_display->GetDisplay(), m_state.timeout,
                            m_state.interval, m_state.preferblank,
                            m_state.allowexposure);
            m_display->Sync();
            m_state.saved = false;
        }
    }

    void ResetScreenSaver(void)
    {
        if (!IsScreenSaverRunning())
            return;

        QDateTime current_time = QDateTime::currentDateTime();
        if ((!m_last_deactivated.isValid()) ||
            (m_last_deactivated.secsTo(current_time) > 30))
        {
            if (m_xscreensaverRunning)
            {
                VERBOSE(VB_PLAYBACK, LOC + "Calling xscreensaver-command -deactivate");
                myth_system("xscreensaver-command -deactivate >&- 2>&- &",
                            kMSDontBlockInputDevs |
                            kMSDontDisableDrawing |
                            kMSRunBackground);
            }
            if (m_gscreensaverRunning)
            {
                VERBOSE(VB_PLAYBACK, LOC + "Calling gnome-screensaver-command --poke");
                myth_system("gnome-screensaver-command --poke >&- 2>&- &",
                            kMSDontBlockInputDevs |
                            kMSDontDisableDrawing |
                            kMSRunBackground);
            }
            m_last_deactivated = current_time;
        }
    }

  private:
    class ScreenSaverState
    {
      public:
        ScreenSaverState() :
            saved(false), timeout(-1), interval(-1),
            preferblank(-1), allowexposure(-1) {}
        bool saved;
        int timeout;
        int interval;
        int preferblank;
        int allowexposure;
    };

  private:
    bool m_dpmsaware;
    bool m_dpmsdeactivated; ///< true if we disabled DPMS
    bool m_xscreensaverRunning;
    bool m_gscreensaverRunning;
    BOOL m_dpmsenabled;

    int m_timeoutInterval;
    QTimer *m_resetTimer;

    QDateTime m_last_deactivated;

    ScreenSaverState m_state;
    MythXDisplay *m_display;
};

ScreenSaverX11::ScreenSaverX11() :
    d(new ScreenSaverX11Private(this))
{
}

ScreenSaverX11::~ScreenSaverX11()
{
    /* Ensure DPMS gets left as it was found. */
    if (d->DeactivatedDPMS())
        Restore();

    delete d;
}

void ScreenSaverX11::Disable(void)
{
    d->SaveScreenSaver();

    if (d->m_display)
    {
        XResetScreenSaver(d->m_display->GetDisplay());
        XSetScreenSaver(d->m_display->GetDisplay(), 0, 0, 0, 0);
        d->m_display->Sync();
    }

    d->DisableDPMS();

    if (d->IsScreenSaverRunning())
        d->ResetTimer();
}

void ScreenSaverX11::Restore(void)
{
    d->RestoreScreenSaver();
    d->RestoreDPMS();

    // One must reset after the restore
    if (d->m_display)
    {
        XResetScreenSaver(d->m_display->GetDisplay());
        d->m_display->Sync();
    }

    if (d->IsScreenSaverRunning())
        d->StopTimer();
}

void ScreenSaverX11::Reset(void)
{
    bool need_xsync = false;
    Display *dsp = NULL;
    if (d->m_display)
        dsp = d->m_display->GetDisplay();

    if (dsp)
    {
        XResetScreenSaver(dsp);
        need_xsync = true;
    }

    if (d->IsScreenSaverRunning())
        resetSlot();

    if (Asleep() && dsp)
    {
        DPMSForceLevel(dsp, DPMSModeOn);
        need_xsync = true;
    }

    if (need_xsync && d->m_display)
        d->m_display->Sync();
}

bool ScreenSaverX11::Asleep(void)
{
    if (!d->IsDPMSEnabled())
        return false;

    if (d->DeactivatedDPMS())
        return false;

    BOOL on;
    CARD16 power_level = DPMSModeOn;

    if (d->m_display)
        DPMSInfo(d->m_display->GetDisplay(), &power_level, &on);

    return (power_level != DPMSModeOn);
}

void ScreenSaverX11::resetSlot(void)
{
    d->ResetScreenSaver();
}
