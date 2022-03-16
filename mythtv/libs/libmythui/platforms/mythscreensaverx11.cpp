// Qt
#include <QDateTime>
#include <QTimer>

// MythTV
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythsystemlegacy.h"

#include "platforms/mythxdisplay.h"
#include "platforms/mythscreensaverx11.h"

// X11 headers
#include <X11/Xlib.h>
extern "C" {
#include <X11/extensions/dpms.h>
}

#define LOC QString("ScreenSaverX11: ")

class ScreenSaverX11Private
{
    friend class MythScreenSaverX11;

  public:
    explicit ScreenSaverX11Private(MythScreenSaverX11* Parent)
    {
        const uint flags = kMSDontBlockInputDevs | kMSDontDisableDrawing |
                           kMSProcessEvents;
        m_xscreensaverRunning = myth_system("xscreensaver-command -version >&- 2>&-", flags) == 0;
        if (m_xscreensaverRunning)
        {
            m_resetTimer = new QTimer(Parent);
            m_resetTimer->setSingleShot(false);
            QObject::connect(m_resetTimer, &QTimer::timeout, Parent, &MythScreenSaverX11::ResetSlot);
            if (m_xscreensaverRunning)
                LOG(VB_GENERAL, LOG_INFO, LOC + "XScreenSaver support enabled");
        }

        m_display = MythXDisplay::OpenMythXDisplay(false);
        if (m_display)
        {
            int dummy0 = 0;
            int dummy1 = 0;
            m_dpmsaware = (DPMSQueryExtension(m_display->GetDisplay(), &dummy0, &dummy1) != 0);
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to open connection to X11 server");
        }

        if (m_dpmsaware && m_display)
        {
            CARD16 power_level = 0;

            /* If someone runs into X server weirdness that goes away when
             * they externally disable DPMS, then the 'dpmsenabled' test should
             * be short circuited by a call to 'DPMSCapable()'. Be sure to
             * manually initialize dpmsenabled to false.
             */

            DPMSInfo(m_display->GetDisplay(), &power_level, &m_dpmsenabled);

            if (static_cast<bool>(m_dpmsenabled))
                LOG(VB_GENERAL, LOG_INFO, LOC + "DPMS is active.");
            else
                LOG(VB_GENERAL, LOG_INFO, LOC + "DPMS is disabled.");
        }
        else
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + "DPMS is not supported.");
        }
    }

    ~ScreenSaverX11Private()
    {
        // m_resetTimer deleted by ScreenSaverX11 QObject dtor
        delete m_display;
    }

    bool IsScreenSaverRunning() const
    {
        return m_xscreensaverRunning;
    }

    bool IsDPMSEnabled() const { return static_cast<bool>(m_dpmsenabled); }

    void StopTimer()
    {
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "StopTimer");
        if (m_resetTimer)
            m_resetTimer->stop();
    }

    void StartTimer()
    {
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "StartTimer");
        if (m_resetTimer)
            m_resetTimer->start(m_timeoutInterval);
    }

    void ResetTimer()
    {
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "ResetTimer -- begin");
        StopTimer();
        // To be clear - this setting has no UI
        if (m_timeoutInterval == -1s)
            m_timeoutInterval = gCoreContext->GetDurSetting<std::chrono::seconds>("xscreensaverInterval", 50s);
        if (m_timeoutInterval > 0s)
            StartTimer();
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "ResetTimer -- end");
    }

    // DPMS
    bool DeactivatedDPMS() const
    {
        return m_dpmsdeactivated;
    }

    void DisableDPMS()
    {
        if (IsDPMSEnabled() && m_display)
        {
            m_dpmsdeactivated = true;
            Status status = DPMSDisable(m_display->GetDisplay());
            m_display->Sync();
            LOG(VB_GENERAL, LOG_INFO, LOC + QString("DPMS Deactivated %1").arg(status));
        }
    }

    void RestoreDPMS()
    {
        if (m_dpmsdeactivated && m_display)
        {
            m_dpmsdeactivated = false;
            Status status = DPMSEnable(m_display->GetDisplay());
            m_display->Sync();
            LOG(VB_GENERAL, LOG_INFO, LOC + QString("DPMS Reactivated %1").arg(status));
        }
    }

    void SaveScreenSaver()
    {
        if (!m_state.m_saved && m_display)
        {
            XGetScreenSaver(m_display->GetDisplay(), &m_state.m_timeout,
                            &m_state.m_interval, &m_state.m_preferblank,
                            &m_state.m_allowexposure);
            m_state.m_saved = true;
        }
    }

    void RestoreScreenSaver()
    {
        if (m_state.m_saved && m_display)
        {
            if (XSetScreenSaver(m_display->GetDisplay(), m_state.m_timeout,
                                m_state.m_interval, m_state.m_preferblank,
                                m_state.m_allowexposure) == 1)
            {
                LOG(VB_GENERAL, LOG_INFO, LOC + "Uninhibited screensaver");
            }
            else
            {
                LOG(VB_GENERAL, LOG_WARNING, LOC + "Failed to re-enable screensaver");
            }
            m_display->Sync();
            m_state.m_saved = false;
        }
    }

    void ResetScreenSaver()
    {
        if (!IsScreenSaverRunning())
            return;

        QDateTime current_time = MythDate::current();
        if ((!m_lastDeactivated.isValid()) ||
            (m_lastDeactivated.secsTo(current_time) > 30))
        {
            if (m_xscreensaverRunning)
            {
                LOG(VB_PLAYBACK, LOG_INFO, LOC + "Calling xscreensaver-command -deactivate");
                myth_system("xscreensaver-command -deactivate >&- 2>&- &",
                            kMSDontBlockInputDevs | kMSDontDisableDrawing | kMSRunBackground);
            }
            m_lastDeactivated = current_time;
        }
    }

  private:
    class ScreenSaverState
    {
      public:
        ScreenSaverState() = default;
        bool m_saved         {false};
        int  m_timeout       {-1};
        int  m_interval      {-1};
        int  m_preferblank   {-1};
        int  m_allowexposure {-1};
    };

  private:
    Q_DISABLE_COPY(ScreenSaverX11Private)
    bool m_dpmsaware           {false};
    bool m_dpmsdeactivated     {false}; ///< true if we disabled DPMS
    bool m_xscreensaverRunning {false};
    BOOL m_dpmsenabled         {static_cast<BOOL>(false)};

    std::chrono::seconds m_timeoutInterval {-1s};
    QTimer *m_resetTimer       {nullptr};

    QDateTime m_lastDeactivated;

    ScreenSaverState m_state;
    MythXDisplay *m_display    {nullptr};
};

MythScreenSaverX11::MythScreenSaverX11(QObject* Parent)
  : MythScreenSaver(Parent),
    m_priv(new ScreenSaverX11Private(this))
{
}

MythScreenSaverX11::~MythScreenSaverX11()
{
    // Ensure DPMS gets left as it was found.
    if (m_priv->DeactivatedDPMS())
        MythScreenSaverX11::Restore();

    delete m_priv;
}

void MythScreenSaverX11::Disable()
{
    m_priv->SaveScreenSaver();

    if (m_priv->m_display)
    {
        int reset = XResetScreenSaver(m_priv->m_display->GetDisplay());
        int set   = XSetScreenSaver(m_priv->m_display->GetDisplay(), 0, 0, 0, 0);
        if (reset != 1)
            LOG(VB_GENERAL, LOG_WARNING, LOC + "Failed to reset screensaver");
        if (set != 1)
            LOG(VB_GENERAL, LOG_WARNING, LOC + "Failed to disable screensaver");
        if (reset && set)
            LOG(VB_GENERAL, LOG_INFO, LOC + "Inhibited X11 screensaver");
        m_priv->m_display->Sync();
    }

    m_priv->DisableDPMS();

    if (m_priv->IsScreenSaverRunning())
        m_priv->ResetTimer();
}

void MythScreenSaverX11::Restore()
{
    m_priv->RestoreScreenSaver();
    m_priv->RestoreDPMS();

    // One must reset after the restore
    if (m_priv->m_display)
    {
        XResetScreenSaver(m_priv->m_display->GetDisplay());
        m_priv->m_display->Sync();
    }

    if (m_priv->IsScreenSaverRunning())
        m_priv->StopTimer();
}

void MythScreenSaverX11::Reset()
{
    bool need_xsync = false;
    Display *dsp = nullptr;
    if (m_priv->m_display)
        dsp = m_priv->m_display->GetDisplay();

    if (dsp)
    {
        XResetScreenSaver(dsp);
        need_xsync = true;
    }

    if (m_priv->IsScreenSaverRunning())
        ResetSlot();

    if (Asleep() && dsp)
    {
        DPMSForceLevel(dsp, DPMSModeOn);
        need_xsync = true;
    }

    if (need_xsync && m_priv->m_display)
        m_priv->m_display->Sync();
}

bool MythScreenSaverX11::Asleep()
{
    if (!m_priv->IsDPMSEnabled())
        return false;

    if (m_priv->DeactivatedDPMS())
        return false;

    BOOL on = static_cast<BOOL>(false);
    CARD16 power_level = DPMSModeOn;

    if (m_priv->m_display)
        DPMSInfo(m_priv->m_display->GetDisplay(), &power_level, &on);

    return (power_level != DPMSModeOn);
}

void MythScreenSaverX11::ResetSlot()
{
    m_priv->ResetScreenSaver();
}
