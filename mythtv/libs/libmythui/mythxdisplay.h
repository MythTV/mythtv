/** This file is intended to hold X11 specific utility functions */

#ifndef _MYTHXDISPLAY_X_
#define _MYTHXDISPLAY_X_

#include <QMutex>

#ifdef USING_X11
#include <QSize>
#include <QRect>
#include <X11/Xlib.h>
#include <vector>

#include "mythuiexp.h"

class MUI_PUBLIC MythXDisplay
{
  public:
    MythXDisplay() = default;
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
    QSize    GetDisplaySize(void);
    QSize    GetDisplayDimensions(void);
    float    GetRefreshRate(void);
    void     Sync(bool flush = false);
    void     StartLog(void);
    bool     StopLog(void);

  private:
    bool CheckErrors(Display *disp = nullptr);
    void CheckOrphanedErrors(void);

    Display      *m_disp       {nullptr};
    int           m_screen_num {0};
    Screen       *m_screen     {nullptr};
    int           m_depth      {0};
    unsigned long m_black      {0};
    GC            m_gc         {nullptr};
    Window        m_root       {0};
    QMutex        m_lock       {QMutex::Recursive};
};

class MythXLocker
{
  public:
    explicit MythXLocker(MythXDisplay*d) : disp(d)
    {
        if (disp) disp->Lock();
    }

    ~MythXLocker()
    {
        if (disp) disp->Unlock();
    }

  private:
    MythXDisplay *disp {nullptr};
};

MUI_PUBLIC void          LockMythXDisplays(bool lock);
MUI_PUBLIC MythXDisplay *GetMythXDisplay(Display*);
MUI_PUBLIC MythXDisplay *OpenMythXDisplay(void);
#define XLOCK(dpy, arg) { dpy->Lock(); arg; dpy->Unlock(); }
#endif // USING_X11

// These X11 defines conflict with the QT key event enum values
#undef KeyPress
#undef KeyRelease

#endif // _MYTHXDISPLAY_X_
