/** This file is intended to hold X11 specific utility functions */

#ifndef _MYTHXDISPLAY_X_
#define _MYTHXDISPLAY_X_

#include <QMutex>

#ifdef USING_X11
#include <QSize>
#include <QRect>
#include <X11/Xlib.h>
#include <vector>

#include "mythexp.h"

class MPUBLIC MythXDisplay
{
  public:
    MythXDisplay();
    ~MythXDisplay();
    Display *GetDisplay(void)          { return m_disp;       }
    int      GetScreen(void) const     { return m_screen_num; }
    void     Lock(void)                { m_lock.lock();       }
    void     Unlock(void)              { m_lock.unlock();     }
    int      GetDepth(void) const      { return m_depth;      }
    Window   GetRoot(void) const       { return m_root;       }
    GC       GetGC(void) const         { return m_gc;         }
    unsigned long GetBlack(void) const { return m_black;      }
    bool     Open(void);
    bool     CreateGC(Window win);
    void     SetForeground(unsigned long color);
    void     FillRectangle(Window win, const QRect &rect);
    void     MoveResizeWin(Window win, const QRect &rect);
    int      GetNumberXineramaScreens(void);
    QSize    GetDisplaySize(void);
    QSize    GetDisplayDimensions(void);
    float    GetRefreshRate(void);
    void     Sync(bool flush = false);
    void     StartLog(void);
    bool     StopLog(void);

  private:
    bool CheckErrors(Display *disp = NULL);
    void CheckOrphanedErrors(void);

    Display      *m_disp;
    int           m_screen_num;
    Screen       *m_screen;
    int           m_depth;
    unsigned long m_black;
    GC            m_gc;
    Window        m_root;
    QMutex        m_lock;
};

class MythXLocker
{
  public:
    MythXLocker(MythXDisplay*d) : disp(d)
    {
        if (disp) disp->Lock();
    }

    ~MythXLocker()
    {
        if (disp) disp->Unlock();
    }

  private:
    MythXDisplay *disp;
};

MPUBLIC void          LockMythXDisplays(bool lock);
MPUBLIC MythXDisplay *GetMythXDisplay(Display*);
MPUBLIC MythXDisplay *OpenMythXDisplay(void);
MPUBLIC QSize         MythXGetDisplayDimensions(void);
MPUBLIC float         MythXGetRefreshRate(void);
#define XLOCK(dpy, arg) { dpy->Lock(); arg; dpy->Unlock(); }
#endif // USING_X11

MPUBLIC int GetNumberXineramaScreens();

// These X11 defines conflict with the QT key event enum values
#undef KeyPress
#undef KeyRelease

#endif // _MYTHXDISPLAY_X_
