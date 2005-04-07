/** This file is intended to hold X11 specific utility functions */

#include "util-x11.h"

#ifdef Q_WS_X11
extern "C" {
#include <X11/Xlib.h>
#include <X11/extensions/Xinerama.h>
}
#endif // Q_WS_X11

int GetNumberOfXineramaScreens()
{
    int nr_xinerama_screens = 0;

#ifdef Q_WS_X11
    Display *d = XOpenDisplay(NULL);
    int event_base = 0, error_base = 0;
    if (XineramaQueryExtension(d, &event_base, &error_base) &&
        XineramaIsActive(d))
        XFree(XineramaQueryScreens(d, &nr_xinerama_screens));
    XCloseDisplay(d);
#endif // Q_WS_X11

    return nr_xinerama_screens;
}
