/** This file is intended to hold X11 specific utility functions */

#ifndef __UTIL_X11_H__
#define __UTIL_X11_H__

#ifdef USING_XV
#include <qwindowdefs.h>
#include <X11/Xlib.h>
#include <vector>

int XJ_error_catcher(Display * d, XErrorEvent * xeev);
void InstallXErrorHandler(Display *d);
void PrintXErrors(Display *d, const std::vector<XErrorEvent>& events);
std::vector<XErrorEvent> UninstallXErrorHandler(Display *d, bool printErrors = true);
#endif // USING_XV

int GetNumberOfXineramaScreens();

#endif // __UTIL_X11_H__
