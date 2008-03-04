/** This file is intended to hold X11 specific utility functions */

#ifndef __UTIL_X11_H__
#define __UTIL_X11_H__

#include <qmutex.h>

#ifdef USING_X11
#include <qsize.h>
#include <qwindowdefs.h>
#include <X11/Xlib.h>
#include <vector>

#include "mythexp.h"

MPUBLIC Display *MythXOpenDisplay(void);
MPUBLIC void InstallXErrorHandler(Display *d);
MPUBLIC void PrintXErrors(Display *d, const std::vector<XErrorEvent>& events);
MPUBLIC std::vector<XErrorEvent> UninstallXErrorHandler(Display *d, bool printErrors = true);
MPUBLIC QSize  MythXGetDisplaySize(      Display *d = NULL, int screen = -1);
MPUBLIC QSize  MythXGetDisplayDimensions(Display *d = NULL, int screen = -1);
MPUBLIC double MythXGetPixelAspectRatio( Display *d = NULL, int screen = -1);
#endif // USING_X11

MPUBLIC int GetNumberOfXineramaScreens();

MPUBLIC extern QMutex x11_lock;

#define X11L x11_lock.lock()
#define X11U x11_lock.unlock()
#define X11S(arg) do { X11L; arg; X11U; } while (0)

// These X11 defines conflict with the QT key event enum values
#undef KeyPress
#undef KeyRelease

#endif // __UTIL_X11_H__
