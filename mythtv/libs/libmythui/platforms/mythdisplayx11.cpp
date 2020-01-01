// MythTV
#include "config.h"
#include "mythcorecontext.h"
#include "mythlogging.h"
#include "mythdisplayx11.h"

// X11
#include <X11/Xatom.h>

#define LOC QString("DisplayX11: ")

MythDisplayX11::MythDisplayX11()
  : MythDisplay()
{
    Initialise();
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

void MythDisplayX11::UpdateCurrentMode(void)
{
    MythXDisplay *display = MythXDisplay::OpenMythXDisplay();
    if (display)
    {
        m_refreshRate  = display->GetRefreshRate();
        m_resolution   = display->GetDisplaySize();
        m_physicalSize = display->GetDisplayDimensions();
        GetEDID(display);
        delete display;
        m_modeComplete = true;
        return;
    }
    MythDisplay::UpdateCurrentMode();
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
    if (!m_videoModes.empty() || !m_screen)
        return m_videoModes;

    m_videoModes.clear();
    m_modeMap.clear();

    MythXDisplay *display = MythXDisplay::OpenMythXDisplay();
    if (!display)
        return m_videoModes;

    XRRScreenResources* res = XRRGetScreenResourcesCurrent(display->GetDisplay(), display->GetRoot());
    XRROutputInfo *output = GetOutput(res, display, m_screen);

    if (!output)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to find an output that matches '%1'")
            .arg(m_screen->name()));
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
        for (int j = 0; j < res->nmode; ++j)
        {
            if (res->modes[j].id != rrmode)
                continue;

            XRRModeInfo mode = res->modes[j];
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

            QSize resolution(width, height);
            QSize physical(mmwidth, mmheight);
            uint64_t key = MythDisplayMode::CalcKey(resolution, 0.0);
            if (screenmap.find(key) == screenmap.end())
                screenmap[key] = MythDisplayMode(resolution, physical, -1.0, rate);
            else
                screenmap[key].AddRefreshRate(rate);
            m_modeMap.insert(MythDisplayMode::CalcKey(resolution, rate), rrmode);
        }
    }

    for (auto it = screenmap.begin(); screenmap.end() != it; ++it)
        m_videoModes.push_back(it->second);

    DebugModes();
    XRRFreeOutputInfo(output);
    XRRFreeScreenResources(res);
    delete display;
    return m_videoModes;
}

bool MythDisplayX11::SwitchToVideoMode(QSize Size, double DesiredRate)
{
    if (!m_crtc)
        (void)GetVideoModes();
    if (!m_crtc)
        return false;

    auto rate = static_cast<double>(NAN);
    QSize dummy(0, 0);
    MythDisplayMode desired(Size, dummy, -1.0, DesiredRate);
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

    MythXDisplay *display = MythXDisplay::OpenMythXDisplay();
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

XRROutputInfo* MythDisplayX11::GetOutput(XRRScreenResources* Resources,
                                         MythXDisplay* mDisplay,
                                         QScreen* qScreen, RROutput *Output)
{
    if (!(Resources && mDisplay && qScreen))
        return nullptr;

    XRROutputInfo *result = nullptr;
    for (int i = 0; i < Resources->noutput; ++i)
    {
        if (result)
        {
            XRRFreeOutputInfo(result);
            result = nullptr;
        }

        result = XRRGetOutputInfo(mDisplay->GetDisplay(), Resources, Resources->outputs[i]);
        if (!result || result->nameLen < 1)
            continue;
        if (result->connection != RR_Connected)
        {
            LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Output '%1' is disconnected")
                .arg(result->name));
            continue;
        }

        QString name(result->name);
        if (name == qScreen->name())
        {
            LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Matched '%1' to output %2")
                .arg(qScreen->name()).arg(Resources->outputs[i]));
            if (Output)
                *Output = Resources->outputs[i];
            return result;
        }
    }
    XRRFreeOutputInfo(result);
    return nullptr;
}
#endif // USING_XRANDR

void MythDisplayX11::GetEDID(MythXDisplay *mDisplay)
{
#ifdef USING_XRANDR
    if (!mDisplay)
    {
        m_edid = MythEDID();
        return;
    }

    XRRScreenResources* res = XRRGetScreenResourcesCurrent(mDisplay->GetDisplay(), mDisplay->GetRoot());
    RROutput rroutput = 0;
    XRROutputInfo *output = GetOutput(res, mDisplay, m_screen, &rroutput);

    while (rroutput)
    {
        Atom edidproperty = XInternAtom(mDisplay->GetDisplay(), RR_PROPERTY_RANDR_EDID, false);
        if (!edidproperty)
            break;

        int propertycount = 0;
        Atom* properties = XRRListOutputProperties(mDisplay->GetDisplay(), rroutput, &propertycount);
        if (!properties)
            break;

        bool found = false;
        for (int i = 0; i < propertycount; ++i)
        {
            if (properties[i] == edidproperty)
            {
                found = true;
                break;
            }
        }
        XFree(properties);
        if (!found)
            break;

        Atom actualtype = 0;
        int actualformat = 0;;
        unsigned long bytesafter = 0;
        unsigned long nitems = 0;
        unsigned char* data = nullptr;
        if (XRRGetOutputProperty(mDisplay->GetDisplay(), rroutput, edidproperty,
                                 0, 128, false, false, AnyPropertyType, &actualtype,
                                 &actualformat, &nitems, &bytesafter, &data) == Success)
        {
            if (actualtype == XA_INTEGER && actualformat == 8)
                m_edid = MythEDID(reinterpret_cast<const char*>(data),
                                  static_cast<int>(nitems));
        }
        break;
    }
    XRRFreeOutputInfo(output);
    XRRFreeScreenResources(res);
#else
    (void)mDisplay;
    m_edid = MythEDID();
#endif
}
