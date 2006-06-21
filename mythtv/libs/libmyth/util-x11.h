/** This file is intended to hold X11 specific utility functions */

#ifndef __UTIL_X11_H__
#define __UTIL_X11_H__

#include <qmutex.h>

#ifdef USING_XV
#include <qwindowdefs.h>
#include <X11/Xlib.h>
#include <vector>

Display *MythXOpenDisplay(void);
int XJ_error_catcher(Display * d, XErrorEvent * xeev);
void InstallXErrorHandler(Display *d);
void PrintXErrors(Display *d, const std::vector<XErrorEvent>& events);
std::vector<XErrorEvent> UninstallXErrorHandler(Display *d, bool printErrors = true);
#endif // USING_XV

int GetNumberOfXineramaScreens();

extern QMutex x11_lock;

#define X11L x11_lock.lock()
#define X11U x11_lock.unlock()
#define X11S(arg) do { X11L; arg; X11U; } while (0)

#endif // __UTIL_X11_H__
