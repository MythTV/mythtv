// MythTV
#include "config.h"
#include "mythcorecontext.h"
#include "mythlogging.h"
#include "mythdisplayx11.h"
#include "mythxdisplay.h"

#ifdef USING_XRANDR
#include <X11/extensions/Xrandr.h> // always last
#endif

#define LOC QString("DisplayX11: ")

MythDisplayX11::MythDisplayX11()
{
    InitialiseModes();
}

bool MythDisplayX11::IsAvailable(void)
{
    static bool s_checked = false;
    static bool s_available = false;
    if (!s_checked)
    {
        s_checked = true;
        MythXDisplay display;
        s_available = display.Open();
    }
    return s_available;
}

DisplayInfo MythDisplayX11::GetDisplayInfo(int VideoRate)
{
    DisplayInfo ret;
    MythXDisplay *disp = OpenMythXDisplay();
    if (!disp)
        return ret;

    float rate = disp->GetRefreshRate();
    if (VALID_RATE(rate))
        ret.m_rate = 1000000.0F / rate;
    else
        ret.m_rate = SanitiseRefreshRate(VideoRate);
    ret.m_res  = disp->GetDisplaySize();
    ret.m_size = disp->GetDisplayDimensions();
    delete disp;
    return ret;
}

#ifdef USING_XRANDR
bool MythDisplayX11::UsingVideoModes(void)
{
    if (gCoreContext)
        return gCoreContext->GetBoolSetting("UseVideoModes", false);
    return false;
}

const std::vector<MythDisplayMode>& MythDisplayX11::GetVideoModes(void)
{
    if (!m_videoModes.empty())
        return m_videoModes;

    m_videoModes.clear();
    m_modeMap.clear();

    MythXDisplay *display = OpenMythXDisplay();
    if (!display)
        return m_videoModes;

    RROutput current = 0;
    XRROutputInfo *output = nullptr;
    XRRScreenResources* res = XRRGetScreenResourcesCurrent(display->GetDisplay(), display->GetRoot());
    for (int i = 0; i < res->noutput; ++i)
    {
        if (output)
        {
            XRRFreeOutputInfo(output);
            output = nullptr;
        }

        output = XRRGetOutputInfo(display->GetDisplay(), res, res->outputs[i]);
        if (!output || output->nameLen < 1)
            continue;
        if (output->connection != RR_Connected)
        {
            LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Output '%1' is disconnected")
                .arg(output->name));
            continue;
        }

        QString name(output->name);
        if (name == m_screen->name())
        {
            LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Matched '%1' to output %2")
                .arg(m_screen->name()).arg(res->outputs[i]));
            current = res->outputs[i];
            break;
        }
    }

    if (!current)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to find an output that matches '%1'")
            .arg(m_screen->name()));
        XRRFreeOutputInfo(output);
        XRRFreeScreenResources(res);
        delete display;
        return m_videoModes;
    }

    int mmwidth = static_cast<int>(output->mm_width);
    int mmheight = static_cast<int>(output->mm_height);
    m_crtc = output->crtc;

    DisplayModeMap screenmap;
    for (int i = 0; i < output->nmode; ++i)
    {
        RRMode rrmode = output->modes[i];
        XRRModeInfo mode = res->modes[i];
        if (mode.id != rrmode)
            continue;
        if (!(mode.dotClock > 1 && mode.vTotal > 1 && mode.hTotal > 1))
            continue;
        int width = static_cast<int>(mode.width);
        int height = static_cast<int>(mode.height);
        double rate = static_cast<double>(mode.dotClock) / (mode.vTotal * mode.hTotal);
        bool interlaced = mode.modeFlags & RR_Interlace;
        if (interlaced)
            rate *= 2.0;

        // TODO don't filter out interlaced modes but ignore them in MythDisplayMode
        // when not required. This may then be used in future to allow 'exact' match
        // display modes to display interlaced material on interlaced displays
        if (interlaced)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Ignoring interlaced mode %1x%2 %3i")
                .arg(width).arg(height).arg(rate, 2, 'f', 2, '0'));
            continue;
        }

        uint64_t key = MythDisplayMode::CalcKey(width, height, 0.0);
        if (screenmap.find(key) == screenmap.end())
            screenmap[key] = MythDisplayMode(width, height, mmwidth, mmheight, -1.0, rate);
        else
            screenmap[key].AddRefreshRate(rate);
        m_modeMap.insert(MythDisplayMode::CalcKey(width, height, rate), rrmode);
    }

    for (auto it = screenmap.begin(); screenmap.end() != it; ++it)
        m_videoModes.push_back(it->second);

    DebugModes();
    XRRFreeOutputInfo(output);
    XRRFreeScreenResources(res);
    delete display;
    return m_videoModes;
}

bool MythDisplayX11::SwitchToVideoMode(int Width, int Height, double DesiredRate)
{
    if (!m_crtc)
        (void)GetVideoModes();
    if (!m_crtc)
        return false;

    auto rate = static_cast<double>(NAN);
    MythDisplayMode desired(Width, Height, 0, 0, -1.0, DesiredRate);
    int idx = MythDisplayMode::FindBestMatch(m_videoModes, desired, rate);

    if (idx < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Desired resolution and frame rate not found.");
        return false;
    }

    auto mode = MythDisplayMode::CalcKey(Width, Height, rate);
    if (!m_modeMap.contains(mode))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to find mode");
        return false;
    }

    MythXDisplay *display = OpenMythXDisplay();
    if (!display)
        return false;

    Status status = RRSetConfigFailed;
    XRRScreenResources* res = XRRGetScreenResourcesCurrent(display->GetDisplay(), display->GetRoot());
    if (res)
    {
        XRRCrtcInfo *currentcrtc = XRRGetCrtcInfo(display->GetDisplay(), res, m_crtc);
        if (currentcrtc)
        {
            status = XRRSetCrtcConfig(display->GetDisplay(), res, m_crtc, CurrentTime,
                                      currentcrtc->x, currentcrtc->y, m_modeMap.value(mode),
                                      currentcrtc->rotation, currentcrtc->outputs,
                                      currentcrtc->noutput);
            XRRFreeCrtcInfo(currentcrtc);
            XRRScreenConfiguration *config = XRRGetScreenInfo(display->GetDisplay(), display->GetRoot());
            if (config)
                XRRFreeScreenConfigInfo(config);
        }
        XRRFreeScreenResources(res);
    }
    delete display;

    if (RRSetConfigSuccess != status)
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to set video mode");
    return RRSetConfigSuccess == status;
}
#endif
