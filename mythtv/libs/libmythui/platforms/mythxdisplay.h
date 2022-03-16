#ifndef MYTHXDISPLAY_X_
#define MYTHXDISPLAY_X_

// Qt
#include <QString>
#if QT_VERSION < QT_VERSION_CHECK(5,14,0)
#include <QMutex>
#else
#include <QRecursiveMutex>
#endif
#include <QSize>

// MythTV
#include "libmythui/mythuiexp.h"

// X11
#include <X11/Xlib.h>

#define XLOCK(dpy, arg) { (dpy)->Lock(); arg; (dpy)->Unlock(); }

class MUI_PUBLIC MythXDisplay
{
  public:

    static MythXDisplay* OpenMythXDisplay(bool Warn = true);
    static void          SetQtX11Display (const QString &DisplayStr);
    static bool          DisplayIsRemote ();

    MythXDisplay() = default;
    ~MythXDisplay();
    Display* GetDisplay()          { return m_disp;        }
    QString  GetDisplayName() const{ return m_displayName; }
    int      GetScreen() const     { return m_screenNum;   }
    void     Lock()                { m_lock.lock();        }
    void     Unlock()              { m_lock.unlock();      }
    int      GetDepth() const      { return m_depth;       }
    Window   GetRoot() const       { return m_root;        }
    bool     Open();
    QSize    GetDisplayDimensions();
    void     Sync(bool Flush = false);

  private:
    static inline QString s_QtX11Display;

    Display* m_disp        { nullptr };
    int      m_screenNum   { 0 };
    Screen*  m_screen      { nullptr };
    int      m_depth       { 0 };
    Window   m_root        { 0 };
#if QT_VERSION < QT_VERSION_CHECK(5,14,0)
    QMutex   m_lock        { QMutex::Recursive };
#else
    QRecursiveMutex  m_lock;
#endif
    QString  m_displayName { };
};

// These X11 defines conflict with the QT key event enum values
#undef KeyPress
#undef KeyRelease

#endif
