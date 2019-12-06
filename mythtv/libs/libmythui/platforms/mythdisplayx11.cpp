// MythTV
#include "config.h"
#include "mythcorecontext.h"
#include "mythlogging.h"
#include "mythdisplayx11.h"
#include "mythxdisplay.h"

#ifdef CONFIG_XNVCTRL
#include "mythnvcontrol.h"
#endif

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
static XRRScreenConfiguration *GetScreenConfig(MythXDisplay*& display)
{
    display = OpenMythXDisplay();
    if (!display)
        return nullptr;

    Window root = RootWindow(display->GetDisplay(), display->GetScreen());

    XRRScreenConfiguration *cfg = nullptr;
    int event_basep = 0;
    int error_basep = 0;

    if (XRRQueryExtension(display->GetDisplay(), &event_basep, &error_basep))
        cfg = XRRGetScreenInfo(display->GetDisplay(), root);

    if (!cfg)
    {
        delete display;
        display = nullptr;
        LOG(VB_GENERAL, LOG_ERR, LOC + "Unable to XRRgetScreenInfo");
    }

    return cfg;
}

bool MythDisplayX11::UsingVideoModes(void)
{
    if (gCoreContext)
        return gCoreContext->GetBoolSetting("UseVideoModes", false);
    return false;
}

const std::vector<DisplayResScreen>& MythDisplayX11::GetVideoModes(void)
{
    if (!m_videoModes.empty())
        return m_videoModes;

    MythXDisplay *display = nullptr;
    XRRScreenConfiguration *cfg = GetScreenConfig(display);
    if (!cfg)
        return m_videoModes;

    int num_sizes = 0;
    int num_rates = 0;

    XRRScreenSize *sizes = XRRConfigSizes(cfg, &num_sizes);
    for (int i = 0; i < num_sizes; ++i)
    {
        short *rates = nullptr;
        rates = XRRRates(display->GetDisplay(), display->GetScreen(),
                         i, &num_rates);
        DisplayResScreen scr(sizes[i].width, sizes[i].height,
                             sizes[i].mwidth, sizes[i].mheight,
                             rates, static_cast<uint>(num_rates));
        m_videoModes.push_back(scr);
    }
    XRRFreeScreenConfigInfo(cfg);

    DebugModes("Raw/unsorted XRANDR modes");

#if defined (CONFIG_XNVCTRL)
    if (MythNVControl::GetNvidiaRates(display, m_videoModes))
        DebugModes("Updated/sorted XRANDR modes (interlaced modes may be removed)");
#endif

    m_videoModesUnsorted = m_videoModes;
    std::sort(m_videoModes.begin(), m_videoModes.end());
    delete display;

    return m_videoModes;
}

bool MythDisplayX11::SwitchToVideoMode(int Width, int Height, double DesiredRate)
{
    auto rate = static_cast<double>(NAN);
    DisplayResScreen desired_screen(Width, Height, 0, 0, -1.0, DesiredRate);
    int idx = DisplayResScreen::FindBestMatch(m_videoModesUnsorted, desired_screen, rate);

    if (idx >= 0)
    {
        MythXDisplay *display = nullptr;
        XRRScreenConfiguration *cfg = GetScreenConfig(display);

        if (!cfg)
            return false;

        Rotation rot = 0;

        XRRConfigCurrentConfiguration(cfg, &rot);

        // Search real xrandr rate for desired_rate
        auto finalrate = static_cast<short>(rate);

        for (size_t i = 0; i < m_videoModes.size(); i++)
        {
            if ((m_videoModes[i].Width() == Width) && (m_videoModes[i].Height() == Height))
            {
                if (m_videoModes[i].Custom())
                {
                    finalrate = m_videoModes[i].realRates[rate];
                    LOG(VB_PLAYBACK, LOG_INFO, LOC +
                        QString("Dynamic TwinView rate found, set %1Hz as "
                                "XRandR %2") .arg(rate) .arg(finalrate));
                }

                break;
            }
        }

        Window root = display->GetRoot();

        Status status = XRRSetScreenConfigAndRate(display->GetDisplay(), cfg,
                        root, idx, rot, finalrate,
                        CurrentTime);

        XRRFreeScreenConfigInfo(cfg);

        // Force refresh of xf86VidMode current modeline
        cfg = XRRGetScreenInfo(display->GetDisplay(), root);
        if (cfg)
            XRRFreeScreenConfigInfo(cfg);

        delete display;

        if (RRSetConfigSuccess != status)
            LOG(VB_GENERAL, LOG_ERR, LOC + "XRRSetScreenConfigAndRate() call failed.");

        return RRSetConfigSuccess == status;
    }

    LOG(VB_GENERAL, LOG_ERR, LOC + "Desired resolution and frame rate not found.");

    return false;
}
#endif

void MythDisplayX11::DebugModes(const QString& Message) const
{
    // This is intentionally formatted to match the output of xrandr for comparison
    if (VERBOSE_LEVEL_CHECK(VB_PLAYBACK, LOG_INFO))
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + Message + ":");
        auto it = m_videoModes.cbegin();
        for ( ; it != m_videoModes.cend(); ++it)
        {
            const std::vector<double>& rates = (*it).RefreshRates();
            QStringList rateslist;
            auto it2 = rates.crbegin();
            for ( ; it2 != rates.crend(); ++it2)
                rateslist.append(QString("%1").arg(*it2, 2, 'f', 2, '0'));
            LOG(VB_PLAYBACK, LOG_INFO, QString("%1x%2\t%3")
                .arg((*it).Width()).arg((*it).Height()).arg(rateslist.join("\t")));
        }
    }
}
