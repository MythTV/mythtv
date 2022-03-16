// MythTV
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"
#include "mythdisplayrpi.h"

// Broadcom
extern "C" {
#include "interface/vmcs_host/vc_dispmanx_types.h"
}

#define LOC QString("DisplayRPI: ")
#define MAX_MODE_ID (127)

/*! \class MythDisplayRPI
 *
 * MythDisplayRPI uses Broadcom specific APIs to access display information and
 * set the current video mode. It should be used when an X11 display is not
 * present (MythDisplayX11 will offer complete functionality) and is preferred
 * to MythDisplayDRM - which currently has no mode switching functionality.
 *
 * \note MythDisplayRPI only supports changing the display refresh rate and not
 * the resolution. All video modes that do not match the resolution at start up
 * will be filtered out. This is because when X11 is not available, the APIs in
 * use will 'undercut' Qt's display handling i.e. Qt will not be aware that the
 * video mode has been changed and it will not create a new framebuffer of the
 * correct size.
*/

static void MythTVServiceCallback(void *Context, uint32_t Reason, uint32_t Param1, uint32_t Param2)
{
    MythDisplayRPI* display = reinterpret_cast<MythDisplayRPI*>(Context);
    if (display)
        display->Callback(Reason, Param1, Param2);
}

static inline QString DisplayToString(int Id)
{
    switch (Id)
    {
        case DISPMANX_ID_MAIN_LCD:    return "MainLCD";
        case DISPMANX_ID_AUX_LCD:     return "AuxLCD";
        case DISPMANX_ID_HDMI0:       return "HDMI1"; // NB Consistency with DRM code
        case DISPMANX_ID_SDTV:        return "SDTV";
        case DISPMANX_ID_FORCE_LCD:   return "ForceLCD";
        case DISPMANX_ID_FORCE_TV:    return "ForceTV";
        case DISPMANX_ID_FORCE_OTHER: return "ForceOther";
        case DISPMANX_ID_HDMI1:       return "HDMI2";
        case DISPMANX_ID_FORCE_TV2:   return "ForceTV2";
    }
    return "Unknown";
}

MythDisplayRPI::MythDisplayRPI()
  : MythDisplay()
{
    // We use the tv service callback to wait for changes
    m_waitForModeChanges = false;

    if (vchi_initialise(&m_vchiInstance) != 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to initialise VCHI");
        return;
    }

    if (vchi_connect(nullptr, 0, m_vchiInstance) != 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create VCHI connection");
        return;
    }

    VCHI_CONNECTION_T *vchiconnection;
    vc_vchi_tv_init(m_vchiInstance, &vchiconnection, 1);
    vc_tv_register_callback(MythTVServiceCallback, this);

    // Raspberry Pi 4 does some strange things when connected to the second HDMI
    // connector and nothing is connected to the first. For the sake of simplicity
    // here, only support one display connected to the first connector - and warn
    // otherwise.
    TV_ATTACHED_DEVICES_T devices;
    if (vc_tv_get_attached_devices(&devices) == 0)
    {
        // use the first device for now
        m_deviceId = devices.display_number[0];
        if (devices.num_attached > 1)
            LOG(VB_GENERAL, LOG_WARNING, LOC + QString("%1 connected displays - this may not work")
                .arg(devices.num_attached));
    }

    // Note: QScreen name will be based on DRM names here - which at first glance
    // could probably be matched to DISPMANX_IDs (see DisplayToString) but I'm not
    // sure what the correct naming scheme is for anything other than HDMI.
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Connected to display '%1'")
        .arg(DisplayToString(m_deviceId)));
    Initialise();
}

MythDisplayRPI::~MythDisplayRPI()
{
    vc_tv_unregister_callback_full(MythTVServiceCallback, this);
    vc_vchi_tv_stop();
    vchi_disconnect(m_vchiInstance);
}

void MythDisplayRPI::Callback(uint32_t Reason, uint32_t, uint32_t)
{
    if (Reason == VC_HDMI_DVI || Reason == VC_HDMI_HDMI)
        m_modeChangeWait.wakeAll();
}

void MythDisplayRPI::UpdateCurrentMode(void)
{
    TV_DISPLAY_STATE_T tvstate;
    int ret = (m_deviceId != -1) ? vc_tv_get_display_state_id(m_deviceId, &tvstate) :
                                   vc_tv_get_display_state(&tvstate);

    if (ret != 0)
    {
        MythDisplay::UpdateCurrentMode();
        return;
    }

    // The tvservice code has additional handling for PAL v NTSC rates - both for
    // getting the current mode and in setting modes.
    m_refreshRate  = static_cast<double>(tvstate.display.hdmi.frame_rate);
    m_resolution   = QSize(tvstate.display.hdmi.width, tvstate.display.hdmi.height);
    GetEDID();
    m_edid.Debug();
    // I can't see any Pi interface for the physical size but it will just be using
    // the EDID anyway
    m_physicalSize = m_edid.Valid() ? m_edid.DisplaySize() : QSize(0, 0);
    m_modeComplete = true;
}

void MythDisplayRPI::GetEDID(void)
{
    QByteArray result;
    uint8_t buffer[128];
    int offset = 0;
    int size = (m_deviceId != -1) ? vc_tv_hdmi_ddc_read_id(m_deviceId, offset, 128, buffer) :
                                    vc_tv_hdmi_ddc_read(offset, 128, buffer);
    if (size != 128)
    {
        m_edid = MythEDID();
        return;
    }

    result.append(reinterpret_cast<const char *>(buffer), 128);
    int extensions = buffer[0x7e];
    // guard against bogus EDID
    if (extensions > 10) extensions = 10;
    for (int i = 0; i < extensions; ++i)
    {
        offset += 128;
        size = (m_deviceId != -1) ? vc_tv_hdmi_ddc_read_id(m_deviceId, offset, 128, buffer) :
                                    vc_tv_hdmi_ddc_read(offset, 128, buffer);
        if (size == 128)
            result.append(reinterpret_cast<const char *>(buffer), 128);
        else
            break;
    }
    m_edid = MythEDID(result);
}

bool MythDisplayRPI::UsingVideoModes(void)
{
    if (gCoreContext)
        return gCoreContext->GetBoolSetting("UseVideoModes", false);
    return false;
}

const MythDisplayModes& MythDisplayRPI::GetVideoModes(void)
{
    if (!m_videoModes.empty())
        return m_videoModes;

    m_videoModes.clear();
    m_modeMap.clear();
    DisplayModeMap screenmap;
    const HDMI_RES_GROUP_T groups[2] = { HDMI_RES_GROUP_CEA, HDMI_RES_GROUP_DMT };

    for (int i = 0; i < 2; ++i)
    {
        HDMI_RES_GROUP_T group = groups[i];
        TV_SUPPORTED_MODE_NEW_T modes[MAX_MODE_ID];
        memset(modes, 0, sizeof(modes));
        HDMI_RES_GROUP_T preferredgroup;
        uint32_t preferred;
        int count;

        if (m_deviceId != -1)
        {
            count = vc_tv_hdmi_get_supported_modes_new_id(m_deviceId, group, modes,
                                                          MAX_MODE_ID, &preferredgroup,
                                                          &preferred);
        }
        else
        {
            count = vc_tv_hdmi_get_supported_modes_new(group, modes, MAX_MODE_ID,
                                                       &preferredgroup, &preferred);
        }

        for (int j = 0; j < count; ++j)
        {
            if (modes[j].width != m_resolution.width() || modes[j].height != m_resolution.height())
            {
                LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Ignoring mode %1x%2 %3 - cannot resize framebuffer")
                    .arg(modes[j].width).arg(modes[j].height).arg(modes[j].frame_rate));
                continue;
            }

            if (modes[j].scan_mode)
            {
                LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Ignoring interlaced mode %1x%2 %3")
                    .arg(modes[j].width).arg(modes[j].height).arg(modes[j].frame_rate));
                continue;
            }

            double rate = static_cast<double>(modes[j].frame_rate);
            QSize resolution(modes[j].width, modes[j].height);
            uint64_t key = MythDisplayMode::CalcKey(resolution, 0.0);
            if (screenmap.find(key) == screenmap.end())
                screenmap[key] = MythDisplayMode(resolution, QSize(), -1.0, rate);
            else
                screenmap[key].AddRefreshRate(rate);
            m_modeMap.insert(MythDisplayMode::CalcKey(resolution, rate),
                             QPair<uint32_t,uint32_t>(modes[j].code, modes[j].group));
        }
    }

    for (auto it = screenmap.begin(); screenmap.end() != it; ++it)
        m_videoModes.push_back(it->second);

    DebugModes();
    return m_videoModes;
}

bool MythDisplayRPI::SwitchToVideoMode(QSize Size, double Framerate)
{
    auto rate = static_cast<double>(NAN);
    QSize dummy(0, 0);
    MythDisplayMode desired(Size, dummy, -1.0, Framerate);
    int idx = MythDisplayMode::FindBestMatch(m_videoModes, desired, rate);

    if (idx < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Desired resolution and frame rate not found.");
        return false;
    }

    auto mode = MythDisplayMode::CalcKey(Size, rate);
    if (!m_modeMap.contains(mode))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to find mode");
        return false;
    }

    HDMI_RES_GROUP_T group = static_cast<HDMI_RES_GROUP_T>(m_modeMap.value(mode).second);
    uint32_t modeid        = m_modeMap.value(mode).first;
    // This is just a guess...
    HDMI_MODE_T modetype = (m_edid.Valid() && m_edid.IsHDMI()) ? HDMI_MODE_HDMI : HDMI_MODE_DVI;

    int ret = (m_deviceId != -1) ? vc_tv_hdmi_power_on_explicit_new_id(m_deviceId, modetype, group, modeid) :
                                   vc_tv_hdmi_power_on_explicit_new(modetype, group, modeid);

    if (ret == 0)
    {
        // We need to wait for the mode change, otherwise we don't get the
        // correct details in UpdateCurrentMode
        m_modeChangeLock.lock();
        if (!m_modeChangeWait.wait(&m_modeChangeLock, 500))
            LOG(VB_GENERAL, LOG_WARNING, LOC + "Timed out waiting for mode switch");
        m_modeChangeLock.unlock();
    }

    return ret == 0;
}
