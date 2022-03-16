// MythTV
#include "mythdrmvrr.h"
#include "libmythbase/mythlogging.h"

#define LOC QString("FreeSync: ")

/*! \brief Force FreeSync on or off *before* the main app is started
 */
void MythDRMVRR::ForceFreeSync(const MythDRMPtr& Device, bool Enable)
{
    if (!(Device && Device->Authenticated() && Device->GetCrtc()))
        return;

    auto freesync = CreateFreeSync(Device, {0,0,false});
    if (!freesync)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "No FreeSync support detected - cannot force");
        return;
    }

    if (freesync->Enabled() == Enable)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("FreeSync already %1abled")
            .arg(Enable ? "en" : "dis"));
        return;
    }

    if (!freesync->IsControllable())
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Cannot %1able FreeSync - do not have permission")
            .arg(Enable ? "en" : "dis"));
        return;
    }

    auto * freesync2 = dynamic_cast<MythDRMVRR*>(freesync.get());
    if (!freesync2)
        return;

    // We have no Qt QPA plugin at this point, so no atomic modesetting etc etc.
    // Just try and enable the property directly.
    if (drmModeObjectSetProperty(Device->GetFD(), Device->GetCrtc()->m_id,
            DRM_MODE_OBJECT_CRTC, freesync2->GetVRRProperty()->m_id, Enable ? 1 : 0) == 0)
    {
        // Release freesync now so that it doesn't reset the state on deletion
        freesync = nullptr;
        s_freeSyncDefaultValue = !Enable;
        s_freeSyncResetOnExit  = true;
        LOG(VB_GENERAL, LOG_INFO, LOC + (Enable ? "Enabled" : "Disabled"));
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Error setting FreeSync");
    }
}

MythVRRPtr MythDRMVRR::CreateFreeSync(const MythDRMPtr& Device, MythVRRRange Range)
{
    if (!(Device && Device->GetConnector() && Device->GetCrtc()))
        return nullptr;

    auto connector = Device->GetConnector();
    auto capable = MythDRMProperty::GetProperty("VRR_CAPABLE", connector->m_properties);
    if (!capable)
        return nullptr;
    auto * capableval = dynamic_cast<MythDRMRangeProperty*>(capable.get());
    if (!capableval || capableval->m_value < 1)
        return nullptr;

    auto crtc = Device->GetCrtc();
    auto enabled = MythDRMProperty::GetProperty("VRR_ENABLED", crtc->m_properties);
    if (!enabled)
        return nullptr;
    auto * enabledval = dynamic_cast<MythDRMRangeProperty*>(enabled.get());
    if (!enabledval)
        return nullptr;

    // We have a valid device with VRR_CAPABLE property (connector) and VRR_ENABLED
    // property (CRTC). Now check whether it is enabled and whether we can control it.
    bool isenabled = enabledval->m_value > 0;
    bool controllable = Device->Atomic() && Device->Authenticated();
    return std::shared_ptr<MythVRR>(new MythDRMVRR(Device, enabled, controllable, isenabled, Range));
}

/*! \class MythDRMVRR
 * \brief A wrapper around FreeSync/Adaptive-Sync support.
 *
 * FreeSync support on linux is currently limited to AMD when using a Display Port
 * connection. FreeSync must be either enabled before starting MythTV (e.g xorg.conf)
 * or the command line option -/--vrr can be used to try and force it on or off
 * at startup. Note however that if running under X11 or Wayland, MythTV will not
 * have the correct permissions to set FreeSync; it will only work via DRM (and
 * typically using the eglfs QPA plugin) - although its current state can still
 * be detected (and hence unnecessary mode switches avoided etc).
 *
 * \sa MythGSync
 * \sa MythVRR
*/
MythDRMVRR::MythDRMVRR(MythDRMPtr Device, DRMProp VRRProp, bool Controllable,
                       bool Enabled, MythVRRRange Range)
  : MythVRR(Controllable, FreeSync, Enabled, Range),
    m_device(std::move(Device)),
    m_vrrProp(std::move(VRRProp))
{
}

MythDRMVRR::~MythDRMVRR()
{
    if (s_freeSyncResetOnExit)
    {
        if (m_controllable)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + "Resetting FreeSync to desktop default");
            MythDRMVRR::SetEnabled(s_freeSyncDefaultValue);
        }
        s_freeSyncResetOnExit = false;
    }
}

void MythDRMVRR::SetEnabled(bool Enable)
{
#ifdef USING_QTPRIVATEHEADERS
    if (m_device && m_vrrProp && m_device->GetCrtc() &&
        m_device->QueueAtomics({{ m_device->GetCrtc()->m_id, m_vrrProp->m_id, Enable ? 1 : 0 }}))
#endif
    {
        m_enabled = Enable;
        LOG(VB_GENERAL, LOG_INFO, LOC + (Enable ? "Enabled" : "Disabled"));
    }
}

DRMProp MythDRMVRR::GetVRRProperty()
{
    return m_vrrProp;
}
