#include <stdio.h>
#include <string.h>
#include <cstdlib>

#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <X11/Xproto.h>
#include <X11/extensions/Xrandr.h>

#include <iostream>

#include "DisplayResX.h"

DisplayResX::DisplayResX(void)
{
    Initialize();
}

DisplayResX::~DisplayResX(void)
{
}

bool DisplayResX::get_display_size(int & width_mm, int & height_mm)
{
    Display         *display;
    int              screen_num;

    display = XOpenDisplay(NULL);
    if (display == NULL)
        return false;

    screen_num = DefaultScreen(display);

    width_mm = DisplayWidthMM(display, screen_num);
    height_mm = DisplayHeightMM(display, screen_num);

    XCloseDisplay(display);
    return true;
}

bool DisplayResX::switch_res(int width, int height)
{
    Display         *display;
    XRRScreenSize   *sizes;
    SizeID           current_size;
    XRRScreenConfiguration *screen_config;
    int              screen_num;
    Window           root;
    Rotation         current_rotation;
    int              num_sizes;
    int              size = -1;
    Status           status = RRSetConfigFailed;

    display = XOpenDisplay(NULL);
    if (display == NULL)
    {
        std::cerr << "DisplaRes::switch_res: Unable to XOpenDisplay\n";
        return false;
    }

    screen_num = DefaultScreen(display);

    root = RootWindow (display, screen_num);
    if ((screen_config = XRRGetScreenInfo (display, root)) == 0)
    {
        std::cerr << "DisplaRes::switch_res: Unable to XRRgetScreenInfo\n";
        return false;
    }

    current_size = XRRConfigCurrentConfiguration (screen_config,
                                                  &current_rotation);

    if ((sizes = XRRConfigSizes(screen_config, &num_sizes)) == 0)
    {
        std::cerr << "DisplaRes::switch_res: Unable to XRRConfigSizes\n";
        return false;
    }

    for (size = 0; size < num_sizes; ++size)
    {
        if (sizes[size].width == width && sizes[size].height == height)
            break;
    }
    if (size == num_sizes)
    {
        std::cerr << "DisplaRes::switch_res: desired resolution "
                  << "not available\n";
        return false;
    }

    status = XRRSetScreenConfig (display, screen_config,
                                 DefaultRootWindow (display),
                                 (SizeID) size,
                                 current_rotation,
                                 CurrentTime);

    XRRFreeScreenConfigInfo(screen_config);
    XCloseDisplay(display);
    return true;
}

