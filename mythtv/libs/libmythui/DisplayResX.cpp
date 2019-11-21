// MythTV
#include "config.h"
#include "mythlogging.h"
#include "mythdb.h"
#include "mythdisplay.h"
#include "mythxdisplay.h"
#ifdef CONFIG_XNVCTRL
#include "util-nvctrl.h"
#endif
#include "DisplayResX.h"

// Std
#include <cmath>

#include <X11/extensions/Xrandr.h> // this has to be after util-x11.h (Qt bug)

#define LOC QString("DispResX: ")

static XRRScreenConfiguration *GetScreenConfig(MythXDisplay*& display);

bool DisplayResX::IsAvailable(void)
{
    static bool checked = false;
    static bool available = false;
    if (!checked)
    {
        checked = true;
        MythXDisplay display;
        available = display.Open();
    }
    return available;
}

DisplayResX::DisplayResX(void)
  : DisplayRes()
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "Created");
    Initialize();
}

DisplayResX::~DisplayResX()
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "Deleted");
}

bool DisplayResX::GetDisplayInfo(int &WidthPixels, int &HeightPixels, int &WidthMM,
                                 int &HeightMM, double &RefreshRate, double &PixelAspectRatio) const
{
    DisplayInfo info = MythDisplay::GetDisplayInfo();
    WidthMM          = info.m_size.width();
    HeightMM         = info.m_size.height();
    WidthPixels      = info.m_res.width();
    HeightPixels     = info.m_res.height();
    RefreshRate      = 1000000.0 / static_cast<double>(info.m_rate);
    PixelAspectRatio = 1.0;

    if (WidthMM > 0 && HeightMM > 0 && WidthPixels > 0 && HeightPixels > 0)
        PixelAspectRatio = (static_cast<double>(WidthMM) / static_cast<double>(WidthPixels)) /
                           (static_cast<double>(HeightMM) / static_cast<double>(HeightPixels));

    return true;
}

bool DisplayResX::SwitchToVideoMode(int width, int height, double desired_rate)
{
    double rate = static_cast<double>(NAN);
    DisplayResScreen desired_screen(width, height, 0, 0, -1.0, desired_rate);
    int idx = DisplayResScreen::FindBestMatch(m_videoModesUnsorted,
              desired_screen, rate);

    if (idx >= 0)
    {
        MythXDisplay *display = nullptr;
        XRRScreenConfiguration *cfg = GetScreenConfig(display);

        if (!cfg)
            return false;

        Rotation rot = 0;

        XRRConfigCurrentConfiguration(cfg, &rot);

        // Search real xrandr rate for desired_rate
        short finalrate = static_cast<short>(rate);

        for (size_t i = 0; i < m_videoModes.size(); i++)
        {
            if ((m_videoModes[i].Width() == width) &&
                    (m_videoModes[i].Height() == height))
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
        {
            XRRFreeScreenConfigInfo(cfg);
        }

        delete display;

        if (RRSetConfigSuccess != status)
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "XRRSetScreenConfigAndRate() call failed.");

        return RRSetConfigSuccess == status;
    }

    LOG(VB_GENERAL, LOG_ERR, LOC + "Desired resolution and frame rate not found.");

    return false;
}

const DisplayResVector& DisplayResX::GetVideoModes(void) const
{
    if (!m_videoModes.empty())
        return m_videoModes;

    MythXDisplay *display = nullptr;
    XRRScreenConfiguration *cfg = GetScreenConfig(display);
    if (!cfg)
        return m_videoModes;

    int num_sizes = 0, num_rates = 0;

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

#if defined (CONFIG_XNVCTRL) && defined (USING_XRANDR)
    if (MythNVControl::GetNvidiaRates(display, m_videoModes))
        DebugModes("Updated/sorted XRANDR modes (interlaced modes may be removed)");
#endif

    m_videoModesUnsorted = m_videoModes;
    std::sort(m_videoModes.begin(), m_videoModes.end());
    delete display;

    return m_videoModes;
}

void DisplayResX::DebugModes(const QString &Message) const
{
    // This is intentionally formatted to match the output of xrandr for comparison
    if (VERBOSE_LEVEL_CHECK(VB_PLAYBACK, LOG_INFO))
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + Message + ":");
        std::vector<DisplayResScreen>::const_iterator it = m_videoModes.cbegin();
        for ( ; it != m_videoModes.cend(); ++it)
        {
            const std::vector<double>& rates = (*it).RefreshRates();
            QStringList rateslist;
            std::vector<double>::const_reverse_iterator it2 = rates.crbegin();
            for ( ; it2 != rates.crend(); ++it2)
                rateslist.append(QString("%1").arg(*it2, 2, 'f', 2, '0'));
            LOG(VB_PLAYBACK, LOG_INFO, QString("%1x%2\t%3")
                .arg((*it).Width()).arg((*it).Height()).arg(rateslist.join("\t")));
        }
    }
}

static XRRScreenConfiguration *GetScreenConfig(MythXDisplay*& display)
{
    display = OpenMythXDisplay();
    if (!display)
        return nullptr;

    Window root = RootWindow(display->GetDisplay(), display->GetScreen());

    XRRScreenConfiguration *cfg = nullptr;
    int event_basep = 0, error_basep = 0;

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
