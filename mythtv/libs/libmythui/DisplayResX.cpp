#include <iostream>

#include "mythxdisplay.h"

#include <X11/extensions/Xrandr.h> // this has to be after util-x11.h (Qt bug)

#include "DisplayResX.h"

using std::cerr;
using std::endl;

static XRRScreenConfiguration *GetScreenConfig(MythXDisplay*& display);

DisplayResX::DisplayResX(void)
{
    Initialize();
}

DisplayResX::~DisplayResX(void)
{
}

bool DisplayResX::GetDisplaySize(int &width_mm, int &height_mm) const
{
    QSize dim = MythXGetDisplayDimensions();
    if ((dim.width() > 0) && (dim.height() > 0))
    {
        width_mm  = dim.width();
        height_mm = dim.height();
        return true;
    }

    return false;
}

bool DisplayResX::SwitchToVideoMode(int width, int height, short desired_rate)
{
    short rate;
    DisplayResScreen desired_screen(width, height, 0, 0, -1.0, desired_rate);
    int idx = DisplayResScreen::FindBestMatch(m_video_modes_unsorted,
                                              desired_screen, rate);
    if (idx >= 0)
    {
        MythXDisplay *display = NULL;
        XRRScreenConfiguration *cfg = GetScreenConfig(display);
        if (!cfg)
            return false;

        Rotation rot;
        XRRConfigCurrentConfiguration(cfg, &rot);
        
        Window root = display->GetRoot();
        Status status = XRRSetScreenConfigAndRate(display->GetDisplay(), cfg,
                                                  root, idx, rot, rate,
                                                  CurrentTime);
        
        XRRFreeScreenConfigInfo(cfg);
        delete display;

        if (RRSetConfigSuccess != status)
            cerr<<"DisplaResX: XRRSetScreenConfigAndRate() call failed."<<endl;
        return RRSetConfigSuccess == status;
    }
    cerr<<"DisplaResX: Desired Resolution and FrameRate not found."<<endl;
    return false;
}

const DisplayResVector& DisplayResX::GetVideoModes(void) const
{
    if (m_video_modes.size())
        return m_video_modes;

    MythXDisplay *display = NULL;
    XRRScreenConfiguration *cfg = GetScreenConfig(display);
    if (!cfg)
        return m_video_modes;

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
        m_video_modes.push_back(scr);
    }
    m_video_modes_unsorted = m_video_modes;
    std::sort(m_video_modes.begin(), m_video_modes.end());
    XRRFreeScreenConfigInfo(cfg);
    delete display;

    return m_video_modes;
}

static XRRScreenConfiguration *GetScreenConfig(MythXDisplay*& display)
{
    display = OpenMythXDisplay();
    if (!display)
    {
        cerr<<"DisplaResX: MythXOpenDisplay call failed"<<endl;
        return NULL;
    }

    Window root = RootWindow(display->GetDisplay(), display->GetScreen());

    XRRScreenConfiguration *cfg = NULL;
    int event_basep = 0, error_basep = 0;
    if (XRRQueryExtension(display->GetDisplay(), &event_basep, &error_basep))
        cfg = XRRGetScreenInfo(display->GetDisplay(), root);

    if (!cfg)
    {
        if (display)
        {
            delete display;
            display = NULL;
        }
        cerr<<"DisplaResX: Unable to XRRgetScreenInfo"<<endl;
    }

    return cfg;
}
