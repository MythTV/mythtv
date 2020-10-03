// Qt
#include <QTimerEvent>

// MythTV
#include "mythlogging.h"
#include "platforms/mythscreensaverdrm.h"

#define LOC QString("ScreenSaverDRM: ")

MythScreenSaverDRM* MythScreenSaverDRM::Create(MythDisplay* mDisplay)
{
    auto* result = new MythScreenSaverDRM(mDisplay);
    if (result->IsValid())
        return result;
    delete result;
    return nullptr;
}

/*! \class MythScreenSaverDRM
 *
 * This is the screensaver 'of last resort' on most linux systems. It is only
 * used when no others are available (no X, no Wayland etc).
 *
 * As such, it is assumed that there are no other daemons/processes monitoring
 * power saving and/or screensavers.
 *
 * Hence this class actually implements its own power management (when authenticated) - *but* see below
 *
 * \todo Other screensavers assume system daemons are hooked into input events
 * and reset when user activity is detected. Hence they only reset for 'external' inputs
 * (e.g. network control, services API, cec, lirc etc). When authentication is available
 * this class will also need to reset on each keypress, mouse action etc (probably just
 * connect to a new signal from MythMainWindow).
 *
 * \note This class is of limited use. We do not yet have authenticated
 * acccess to the drm device - which means we cannot update the DPMS state.
 * Without authentication it will simply poll the DPMS state every 5 seconds and will
 * block *external* input devices when the screen is not in the 'On' state (N.B.
 * Asleep is only called from MythInputDeviceHandler which will block input from
 * Joystick, lirc and appleremote devices ONLY).
*/
MythScreenSaverDRM::MythScreenSaverDRM(MythDisplay *mDisplay)
{
    auto* drmdisplay = dynamic_cast<MythDisplayDRM*>(mDisplay);
    if (drmdisplay)
    {
        m_display = drmdisplay;
        m_valid = true;
        Init();
        connect(m_display, &MythDisplayDRM::screenChanged, this, &MythScreenSaverDRM::ScreenChanged);
    }
}

MythScreenSaverDRM::~MythScreenSaverDRM()
{
    m_device = nullptr;
}

void MythScreenSaverDRM::Init()
{
    m_device = m_display->GetDevice();
    m_authenticated = m_device->Authenticated();
    killTimer(m_dpmsTimer);
    killTimer(m_inactiveTimer);
    UpdateDPMS();
    if (!m_authenticated)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Not authenticated - cannot control DPMS");
        m_dpmsTimer = startTimer(DRM_DPMS_POLL * 1000);
    }
    else
    {
        m_inactiveTimer = startTimer(m_timeout * 1000);
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Authenticated");
    }

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("DPMS is %1").arg(m_asleep ? "disabled" : "enabled"));
}

void MythScreenSaverDRM::ScreenChanged()
{
    Init();
}

bool MythScreenSaverDRM::IsValid() const
{
    return m_valid;
}

void MythScreenSaverDRM::timerEvent(QTimerEvent* Event)
{
    if (Event->timerId() == m_dpmsTimer)
        UpdateDPMS();
    else if (Event->timerId() == m_inactiveTimer)
        TurnScreenOnOff(false /*turn off*/);
}

void MythScreenSaverDRM::TurnScreenOnOff(bool On)
{
    if (m_asleep != On)
    {
        m_device->SetEnumProperty(DRM_DPMS, On ? m_dpmsOn : m_dpmsStandby);
        UpdateDPMS();
    }
}

void MythScreenSaverDRM::UpdateDPMS()
{
    if (!m_device)
        return;

    m_asleep = true;
    auto dpms = m_device->GetEnumProperty(DRM_DPMS);
    for (const auto & value : qAsConst(dpms.m_enums))
    {
        if ((value.first == dpms.m_value) && (value.second == DRM_ON))
        {
            m_dpmsOn = value.first;
            m_asleep = false;
        }

        if (value.second == DRM_STANDBY)
            m_dpmsStandby = value.first;
    }
}

void MythScreenSaverDRM::Disable()
{
    if (!m_authenticated)
        return;

    killTimer(m_inactiveTimer);
    TurnScreenOnOff(true /*on*/);
}

void MythScreenSaverDRM::Restore()
{
    if (!m_authenticated)
        return;

    Disable();
    m_inactiveTimer = startTimer(m_timeout);
}

void MythScreenSaverDRM::Reset()
{
    if (!m_authenticated)
        return;

    killTimer(m_inactiveTimer);
    m_inactiveTimer = startTimer(m_timeout);
}

bool MythScreenSaverDRM::Asleep()
{
    return m_asleep;
}
