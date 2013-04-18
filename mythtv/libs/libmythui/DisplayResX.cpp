
#include "config.h"
#include "DisplayResX.h"

#include <cmath>

#include "mythlogging.h"
#include "mythdb.h"
#include "mythdisplay.h"
#include "mythxdisplay.h"
#include "util-nvctrl.h"

#include <X11/extensions/Xrandr.h> // this has to be after util-x11.h (Qt bug)

static XRRScreenConfiguration *GetScreenConfig(MythXDisplay*& display);

DisplayResX::DisplayResX(void)
{
    Initialize();
}

DisplayResX::~DisplayResX(void)
{
}

bool DisplayResX::GetDisplayInfo(int &w_pix, int &h_pix, int &w_mm,
                                 int &h_mm, double &rate, double &par) const
{
    DisplayInfo info = MythDisplay::GetDisplayInfo();
    w_mm   = info.size.width();
    h_mm   = info.size.height();
    w_pix  = info.res.width();
    h_pix  = info.res.height();
    rate   = 1000000.0f / info.rate;
    par    = 1.0;

    if (w_mm > 0 && h_mm > 0 && w_pix > 0 && h_pix > 0)
        par = ((double)w_mm  / (double)w_pix) / ((double)h_mm / (double)h_pix);

    return true;
}

bool DisplayResX::SwitchToVideoMode(int width, int height, double desired_rate)
{
    double rate;
    DisplayResScreen desired_screen(width, height, 0, 0, -1.0, desired_rate);
    int idx = DisplayResScreen::FindBestMatch(m_videoModesUnsorted,
              desired_screen, rate);

    if (idx >= 0)
    {
        short finalrate;
        MythXDisplay *display = NULL;
        XRRScreenConfiguration *cfg = GetScreenConfig(display);

        if (!cfg)
            return false;

        Rotation rot;

        XRRConfigCurrentConfiguration(cfg, &rot);

        // Search real xrandr rate for desired_rate
        finalrate = (short) rate;

        for (uint i = 0; i < m_videoModes.size(); i++)
        {
            if ((m_videoModes[i].Width() == width) &&
                    (m_videoModes[i].Height() == height))
            {
                if (m_videoModes[i].Custom())
                {
                    finalrate = m_videoModes[i].realRates[rate];
                    LOG(VB_PLAYBACK, LOG_INFO,
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
        delete display;

        if (RRSetConfigSuccess != status)
            LOG(VB_GENERAL, LOG_ERR,
                "XRRSetScreenConfigAndRate() call failed.");

        return RRSetConfigSuccess == status;
    }

    LOG(VB_GENERAL, LOG_ERR, "Desired Resolution and FrameRate not found.");

    return false;
}

const DisplayResVector& DisplayResX::GetVideoModes(void) const
{
    if (!m_videoModes.empty())
        return m_videoModes;

    MythXDisplay *display = NULL;

    XRRScreenConfiguration *cfg = GetScreenConfig(display);

    if (!cfg)
        return m_videoModes;

    int num_sizes, num_rates;

    XRRScreenSize *sizes = NULL;

    sizes = XRRConfigSizes(cfg, &num_sizes);

    for (int i = 0; i < num_sizes; ++i)
    {
        short *rates = NULL;
        rates = XRRRates(display->GetDisplay(), display->GetScreen(),
                         i, &num_rates);
        DisplayResScreen scr(sizes[i].width, sizes[i].height,
                             sizes[i].mwidth, sizes[i].mheight,
                             rates, num_rates);
        m_videoModes.push_back(scr);
    }

    t_screenrate screenmap;

    int nvidiarate = GetNvidiaRates(screenmap);

    if (nvidiarate > 0)
    {
        // Update existing DisplayResScreen vector, and update it with
        // new frequencies
        for (uint i = 0; i < m_videoModes.size(); i++)
        {
            DisplayResScreen scr = m_videoModes[i];
            int w = scr.Width();
            int h = scr.Height();
            int mw = scr.Width_mm();
            int mh = scr.Height_mm();
            std::vector<double> newrates;
            std::map<double, short> realRates;
            const std::vector<double>& rates = scr.RefreshRates();
            bool found = false;

            for (std::vector<double>::const_iterator it = rates.begin();
                    it !=  rates.end(); ++it)
            {
                uint64_t key = DisplayResScreen::CalcKey(w, h, *it);

                if (screenmap.find(key) != screenmap.end())
                {
                    // Rate is defined in NV-CONTROL extension, use it
                    newrates.push_back(screenmap[key]);
                    realRates[screenmap[key]] = (int) round(*it);
                    found = true;
#if 1
                    LOG(VB_PLAYBACK, LOG_DEBUG,
                        QString("CustomRate Found, set %1x%2@%3 as %4Hz")
                        .arg(w) .arg(h) .arg(*it) .arg(screenmap[key]));
#endif
                }
            }

            if (found)
            {
                m_videoModes.erase(m_videoModes.begin() + i);
                std::sort(newrates.begin(), newrates.end());
                m_videoModes.insert(m_videoModes.begin() + i,
                                    DisplayResScreen(w, h, mw, mh, newrates,
                                                     realRates));
            }
        }
    }

    m_videoModesUnsorted = m_videoModes;

    std::sort(m_videoModes.begin(), m_videoModes.end());
    XRRFreeScreenConfigInfo(cfg);
    delete display;

    return m_videoModes;
}

static XRRScreenConfiguration *GetScreenConfig(MythXDisplay*& display)
{
    display = OpenMythXDisplay();

    if (!display)
    {
        LOG(VB_GENERAL, LOG_ERR, "DisplaResX: MythXOpenDisplay call failed");
        return NULL;
    }

    Window root = RootWindow(display->GetDisplay(), display->GetScreen());

    XRRScreenConfiguration *cfg = NULL;
    int event_basep = 0, error_basep = 0;

    if (XRRQueryExtension(display->GetDisplay(), &event_basep, &error_basep))
        cfg = XRRGetScreenInfo(display->GetDisplay(), root);

    if (!cfg)
    {
        delete display;
        display = NULL;
        LOG(VB_GENERAL, LOG_ERR, "DisplaResX: Unable to XRRgetScreenInfo");
    }

    return cfg;
}
