#ifndef MYTHXDISPLAY_X_
#define MYTHXDISPLAY_X_

// Qt
#include <QString>
#include <QRecursiveMutex>

// MythTV
#include "libmythui/mythuiexp.h"

// X11
#include <X11/Xlib.h>

class MUI_PUBLIC MythXDisplay
{
  public:

    static MythXDisplay* OpenMythXDisplay(bool Warn = true);
    static void          SetQtX11Display (const QString &DisplayStr);
    static bool          DisplayIsRemote ();

    MythXDisplay() = default;
    ~MythXDisplay();
    Display* GetDisplay()          { return m_disp;        }
    int      GetScreen() const     { return m_screenNum;   }
    void     Lock()                { m_lock.lock();        }
    void     Unlock()              { m_lock.unlock();      }
    Window   GetRoot() const       { return m_root;        }
    bool     Open();
    void     Sync(bool Flush = false);

  private:
    static inline QString s_QtX11Display;

    Display* m_disp        { nullptr };
    int      m_screenNum   { 0 };
    Window   m_root        { 0 };
    QRecursiveMutex  m_lock;
};

// These X11 defines conflict with the QT key event enum values
#undef KeyPress
#undef KeyRelease

#endif
