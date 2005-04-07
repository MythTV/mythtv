#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace X11
{
#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <X11/Xproto.h>
#include <X11/extensions/Xrandr.h>
}

#include <iostream>

#include "DisplayResX.h"

using namespace X11;

static XRRScreenConfiguration *GetScreenConfig(Display*& display);

DisplayResX::DisplayResX(void)
{
    Initialize();
}

DisplayResX::~DisplayResX(void)
{
}

bool DisplayResX::GetDisplaySize(int &width_mm, int &height_mm) const
{
    Display *display = XOpenDisplay(NULL);
    if (display == NULL)
        return false;

    int screen_num = DefaultScreen(display);

    width_mm = DisplayWidthMM(display, screen_num);
    height_mm = DisplayHeightMM(display, screen_num);

    XCloseDisplay(display);
    return true;
}

bool DisplayResX::SwitchToVideoMode(int width, int height, short desired_rate)
{
    short rate;
    DisplayResScreen desired_screen(width, height, 0, 0, -1.0, desired_rate);
    int idx = DisplayResScreen::FindBestMatch(m_video_modes_unsorted,
                                              desired_screen, rate);
    if (idx >= 0)
    {
        Display *display = NULL;
        XRRScreenConfiguration *cfg = GetScreenConfig(display);
        if (!cfg)
            return false;

        Rotation rot;
        XRRConfigCurrentConfiguration(cfg, &rot);
        
        Window root = DefaultRootWindow(display);
        Status status = XRRSetScreenConfigAndRate(display, cfg, root, idx,
                                                  rot, rate, CurrentTime);
        
        XRRFreeScreenConfigInfo(cfg);
        XCloseDisplay(display);

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

    Display *display = NULL;
    XRRScreenConfiguration *cfg = GetScreenConfig(display);
    if (!cfg)
        return m_video_modes;

    int num_sizes, num_rates;
    XRRScreenSize *sizes = XRRConfigSizes(cfg, &num_sizes);
    for (int i = 0; i < num_sizes; ++i)
    {
        short *rates = XRRRates(display, DefaultScreen(display), i, &num_rates);
        DisplayResScreen scr(sizes[i].width, sizes[i].height,
                             sizes[i].mwidth, sizes[i].mheight,
                             rates, num_rates);
        m_video_modes.push_back(scr);
    }
    m_video_modes_unsorted = m_video_modes;
    sort(m_video_modes.begin(), m_video_modes.end());

    XRRFreeScreenConfigInfo(cfg);
    XCloseDisplay(display);

    return m_video_modes;
}

static XRRScreenConfiguration *GetScreenConfig(Display*& display)
{
    display = XOpenDisplay(NULL);
    if (!display)
    {
        cerr<<"DisplaResX: Unable to XOpenDisplay"<<endl;
        return NULL;
    }

    Window root = RootWindow(display, DefaultScreen(display));
    XRRScreenConfiguration *cfg = XRRGetScreenInfo(display, root);
    if (!cfg)
    {
        if (display)
            XCloseDisplay(display);
        cerr<<"DisplaResX: Unable to XRRgetScreenInfo"<<endl;
    }
    return cfg;
}
