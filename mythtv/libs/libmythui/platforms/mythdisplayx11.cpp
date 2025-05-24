// MythTV
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"
#include "mythdisplayx11.h"

// X11
#include <X11/Xatom.h>

#define LOC QString("DisplayX11: ")

MythDisplayX11::MythDisplayX11()
{
    Initialise();
}

bool MythDisplayX11::IsAvailable()
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

/*! \brief Retrieve details for the current video mode.
 *
 * The Qt default implementation tends to get the details correct but only uses
 * an integer refresh rate for some backends.
 *
 * The MythXDisplay methods now use the modeline to get accurate refresh, resolution
 * and display size details regardless of the number of displays connected - but
 * the closed source NVidia drivers do their own thing and return fictitious
 * modelines for mutiple connected displays.
 *
 * So we now use the Qt defaults and override where possible with XRANDR versions.
 * If XRANDR is not available we try and get a more accurate refresh rate only.
*/
void MythDisplayX11::UpdateCurrentMode()
{
    // Get some Qt basics first
    MythDisplay::UpdateCurrentMode();

    auto * display = MythXDisplay::OpenMythXDisplay();
    if (display)
    {
        // XRANDR should always be accurate
        GetEDID(display);
        auto * res = XRRGetScreenResourcesCurrent(display->GetDisplay(), display->GetRoot());
        auto * output = GetOutput(res, display, m_screen);
        if (output)
        {
            m_physicalSize = QSize(static_cast<int>(output->mm_width),
                                   static_cast<int>(output->mm_height));
            XRRFreeOutputInfo(output);
        }

        if (!m_crtc)
            (void)GetVideoModes();
        while (m_crtc && res)
        {
            auto * currentcrtc = XRRGetCrtcInfo(display->GetDisplay(), res, m_crtc);
            if (!currentcrtc)
                break;
            for (int i = 0; i < res->nmode; ++i)
            {
                if (res->modes[i].id != currentcrtc->mode)
                    continue;
                auto mode = res->modes[i];
                m_resolution = QSize(static_cast<int>(mode.width),
                                     static_cast<int>(mode.height));
                if (mode.dotClock > 1 && mode.vTotal > 1 && mode.hTotal > 1)
                {
                    m_refreshRate = static_cast<double>(mode.dotClock) / (mode.vTotal * mode.hTotal);
                    if (mode.modeFlags & RR_Interlace)
                        m_refreshRate *= 2.0;
                }
            }
            XRRFreeCrtcInfo(currentcrtc);
            break;
        }
        XRRFreeScreenResources(res);

        delete display;
        m_modeComplete = true;
        return;
    }
}

bool MythDisplayX11::UsingVideoModes()
{
    if (gCoreContext)
        return gCoreContext->GetBoolSetting("UseVideoModes", false);
    return false;
}

const MythDisplayModes& MythDisplayX11::GetVideoModes()
{
    if (!m_videoModes.empty() || !m_screen)
        return m_videoModes;

    m_videoModes.clear();
    m_modeMap.clear();

    auto * display = MythXDisplay::OpenMythXDisplay();
    if (!display)
        return m_videoModes;

    auto * res = XRRGetScreenResourcesCurrent(display->GetDisplay(), display->GetRoot());
    auto * output = GetOutput(res, display, m_screen);

    if (!output)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to find an output that matches '%1'")
            .arg(m_screen->name()));
        XRRFreeScreenResources(res);
        delete display;
        return m_videoModes;
    }

    auto mmwidth = static_cast<int>(output->mm_width);
    auto mmheight = static_cast<int>(output->mm_height);
    m_crtc = output->crtc;

    DisplayModeMap screenmap;
    for (int i = 0; i < output->nmode; ++i)
    {
        RRMode rrmode = output->modes[i];
        for (int j = 0; j < res->nmode; ++j)
        {
            if (res->modes[j].id != rrmode)
                continue;

            auto mode = res->modes[j];
            if (mode.id != rrmode)
                continue;
            if (mode.dotClock <= 1 || mode.vTotal <= 1 || mode.hTotal <= 1)
                continue;
            auto width = static_cast<int>(mode.width);
            auto height = static_cast<int>(mode.height);
            auto rate = static_cast<double>(mode.dotClock) / (mode.vTotal * mode.hTotal);

            // TODO don't filter out interlaced modes but ignore them in MythDisplayMode
            // when not required. This may then be used in future to allow 'exact' match
            // display modes to display interlaced material on interlaced displays
            if ((mode.modeFlags & RR_Interlace) != 0U)
            {
                LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Ignoring interlaced mode %1x%2 %3i")
                    .arg(width).arg(height).arg(rate * 2.0, 2, 'f', 2, '0'));
                continue;
            }

            QSize resolution(width, height);
            QSize physical(mmwidth, mmheight);
            auto key = MythDisplayMode::CalcKey(resolution, 0.0);
            if (screenmap.find(key) == screenmap.end())
                screenmap[key] = MythDisplayMode(resolution, physical, -1.0, rate);
            else
                screenmap[key].AddRefreshRate(rate);
            m_modeMap.insert(MythDisplayMode::CalcKey(resolution, rate), rrmode);
        }
    }

    for (auto & it : screenmap)
        m_videoModes.push_back(it.second);

    DebugModes();
    XRRFreeOutputInfo(output);
    XRRFreeScreenResources(res);
    delete display;
    return m_videoModes;
}

bool MythDisplayX11::SwitchToVideoMode(QSize Size, double DesiredRate)
{
    if (!m_crtc)
    {
        (void)GetVideoModes();
        if (!m_crtc)
            return false;
    }

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

    auto * display = MythXDisplay::OpenMythXDisplay();
    if (!display)
        return false;

    Status status = RRSetConfigFailed;
    auto * res = XRRGetScreenResourcesCurrent(display->GetDisplay(), display->GetRoot());
    if (res)
    {
        auto * currentcrtc = XRRGetCrtcInfo(display->GetDisplay(), res, m_crtc);
        if (currentcrtc)
        {
            status = XRRSetCrtcConfig(display->GetDisplay(), res, m_crtc, CurrentTime,
                                      currentcrtc->x, currentcrtc->y, m_modeMap.value(mode),
                                      currentcrtc->rotation, currentcrtc->outputs,
                                      currentcrtc->noutput);
            XRRFreeCrtcInfo(currentcrtc);
            auto * config = XRRGetScreenInfo(display->GetDisplay(), display->GetRoot());
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
                                         QScreen* qScreen, RROutput* Output)
{
    if (!(Resources && mDisplay && qScreen))
        return nullptr;

    XRROutputInfo* result = nullptr;
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

void MythDisplayX11::GetEDID(MythXDisplay *mDisplay)
{
    if (!mDisplay)
    {
        m_edid = MythEDID();
        return;
    }

    auto * res = XRRGetScreenResourcesCurrent(mDisplay->GetDisplay(), mDisplay->GetRoot());
    RROutput rroutput = 0;
    auto * output = GetOutput(res, mDisplay, m_screen, &rroutput);

    while (rroutput)
    {
        auto edidproperty = XInternAtom(mDisplay->GetDisplay(), RR_PROPERTY_RANDR_EDID,
                                        static_cast<Bool>(false));
        if (!edidproperty)
            break;

        int propertycount = 0;
        auto * properties = XRRListOutputProperties(mDisplay->GetDisplay(), rroutput, &propertycount);
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
        int actualformat = 0;
        unsigned long bytesafter = 0;
        unsigned long nitems = 0;
        unsigned char* data = nullptr;
        if (XRRGetOutputProperty(mDisplay->GetDisplay(), rroutput, edidproperty,
                                 0, 128, static_cast<Bool>(false), static_cast<Bool>(false),
                                 AnyPropertyType, &actualtype,
                                 &actualformat, &nitems, &bytesafter, &data) == Success)
        {
            if (actualtype == XA_INTEGER && actualformat == 8)
                m_edid = MythEDID(reinterpret_cast<const char*>(data), static_cast<int>(nitems));
            if (data)
            {
                XFree(data);
            }
        }
        break;
    }
    XRRFreeOutputInfo(output);
    XRRFreeScreenResources(res);
}
