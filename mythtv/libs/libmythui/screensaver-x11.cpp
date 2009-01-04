#include <QDateTime>
#include <QTimer>
#include <QX11Info>

#include "screensaver-x11.h"
#include "mythsystem.h"
#include "mythverbose.h"
#include "mythdb.h"
#include "util-x11.h"

extern "C" {
#include <X11/extensions/dpms.h>
}


class ScreenSaverX11Private
{
    friend class ScreenSaverX11;

  public:
    ScreenSaverX11Private(ScreenSaverX11 *outer) : m_dpmsenabled(FALSE),
        m_dpmsdeactivated(false), m_timeoutInterval(-1), m_resetTimer(0)
    {
        m_xscreensaverRunning =
                myth_system("xscreensaver-command -version >&- 2>&-") == 0;
        m_gscreensaverRunning =
                myth_system("gnome-screensaver-command --help >&- 2>&-") == 0;

        if (IsScreenSaverRunning())
        {
            m_resetTimer = new QTimer(outer);
            QObject::connect(m_resetTimer, SIGNAL(timeout()),
                             outer, SLOT(resetSlot()));
            VERBOSE(VB_GENERAL, "XScreenSaver support enabled");
        }

        int dummy0, dummy1;
        X11S(m_dpmsaware = DPMSQueryExtension(
                 QX11Info::display(), &dummy0, &dummy1));
        if (m_dpmsaware)
        {
            CARD16 power_level;

            /* If someone runs into X server weirdness that goes away when
            * they externally disable DPMS, then the 'dpmsenabled' test should
            * be short circuited by a call to 'DPMSCapable()'. Be sure to
            * manually initialize dpmsenabled to false.
            */

            X11S(DPMSInfo(QX11Info::display(), &power_level, &m_dpmsenabled));

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
        delete m_resetTimer;
    }

    bool IsScreenSaverRunning()
    {
        return m_xscreensaverRunning || m_gscreensaverRunning;
    }

    bool IsDPMSEnabled() { return m_dpmsenabled; }

    void StopTimer() { if (m_resetTimer) m_resetTimer->stop(); }

    void StartTimer()
    {
        if (m_resetTimer)
        {
            m_resetTimer->setSingleShot(false);
            m_resetTimer->start(m_timeoutInterval);
        }
    }

    void ResetTimer()
    {
        StopTimer();

        if (m_timeoutInterval == -1)
        {
            m_timeoutInterval = GetMythDB()->GetNumSettingOnHost(
                "xscreensaverInterval", GetMythDB()->GetHostName(), 50) * 1000;
        }

        if (m_timeoutInterval > 0)
            StartTimer();
    }

    // DPMS
    bool DeactivatedDPMS() { return m_dpmsdeactivated; }

    void DisableDPMS()
    {
        if (IsDPMSEnabled())
        {
            m_dpmsdeactivated = true;
            X11S(DPMSDisable(QX11Info::display()));
            VERBOSE(VB_GENERAL, "DPMS Deactivated ");
        }
    }

    void RestoreDPMS()
    {
        if (m_dpmsdeactivated)
        {
            m_dpmsdeactivated = false;
            X11S(DPMSEnable(QX11Info::display()));
            VERBOSE(VB_GENERAL, "DPMS Reactivated.");
        }
    }

    void SaveScreenSaver()
    {
        if (!m_state.saved)
        {
            X11S(XGetScreenSaver(QX11Info::display(),
                                 &m_state.timeout,     &m_state.interval,
                                 &m_state.preferblank, &m_state.allowexposure));
            m_state.saved = true;
        }
    }

    void RestoreScreenSaver()
    {
        if (m_state.saved)
        {
            X11S(XSetScreenSaver(QX11Info::display(),
                                 m_state.timeout,     m_state.interval,
                                 m_state.preferblank, m_state.allowexposure));
            m_state.saved = false;
        }
    }

    void ResetScreenSaver()
    {
        if (IsScreenSaverRunning())
        {
            QDateTime current_time = QDateTime::currentDateTime ();
            if ((!m_last_deactivated.isValid()) ||
                (m_last_deactivated.secsTo(current_time) > 30))
            {
                if (m_xscreensaverRunning)
                    myth_system("xscreensaver-command -deactivate >&- 2>&- &");
                else if (m_gscreensaverRunning)
                    myth_system("gnome-screensaver-command --poke >&- 2>&- &");
                m_last_deactivated = current_time;
            }
        }
    }

  private:
    struct ScreenSaverState
    {
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
    bool m_xscreensaverRunning;
    bool m_gscreensaverRunning;
    BOOL m_dpmsenabled;
    bool m_dpmsdeactivated; // true if we disabled DPMS

    int m_timeoutInterval;
    QTimer *m_resetTimer;

    QDateTime m_last_deactivated;

    ScreenSaverState m_state;
};

ScreenSaverX11::ScreenSaverX11()
{
    d = new ScreenSaverX11Private(this);
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

    X11S(XResetScreenSaver(QX11Info::display()));

    X11S(XSetScreenSaver(QX11Info::display(), 0, 0, 0, 0));

    d->DisableDPMS();

    if (d->IsScreenSaverRunning())
        d->ResetTimer();
}

void ScreenSaverX11::Restore(void)
{
    d->RestoreScreenSaver();
    d->RestoreDPMS();

    // One must reset after the restore
    X11S(XResetScreenSaver(QX11Info::display()));

    if (d->IsScreenSaverRunning())
        d->StopTimer();
}

void ScreenSaverX11::Reset(void)
{
    X11S(XResetScreenSaver(QX11Info::display()));

    if (d->IsScreenSaverRunning())
        resetSlot();

    if (Asleep())
    {
        X11L;

        DPMSForceLevel(QX11Info::display(), DPMSModeOn);

        // Calling XSync is necessary for the case when Myth executes
        // another application before the event loop regains control
        XSync(QX11Info::display(), false);

        X11U;
    }
}

bool ScreenSaverX11::Asleep(void)
{
    if (!d->IsDPMSEnabled())
        return false;

    if (d->DeactivatedDPMS())
        return false;

    BOOL on;
    CARD16 power_level;

    X11S(DPMSInfo(QX11Info::display(), &power_level, &on));

    return (power_level != DPMSModeOn);
}

void ScreenSaverX11::resetSlot()
{
    d->ResetScreenSaver();
}
