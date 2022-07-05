// MythTV
#include "libmythbase/mythlogging.h"
#include "mythxdisplay.h"

class MythXLocker
{
  public:
    explicit MythXLocker(MythXDisplay* Disp)
      : m_disp(Disp)
    {
        if (m_disp)
            m_disp->Lock();
    }

    ~MythXLocker()
    {
        if (m_disp)
            m_disp->Unlock();
    }

  private:
    MythXDisplay* m_disp { nullptr };
};

MythXDisplay* MythXDisplay::OpenMythXDisplay(bool Warn /*= true*/)
{
    auto * display = new MythXDisplay();
    if (display && display->Open())
        return display;

    if (Warn)
        LOG(VB_GENERAL, LOG_CRIT, "MythXOpenDisplay() failed");
    delete display;
    return nullptr;
}

void MythXDisplay::SetQtX11Display(const QString& DisplayStr)
{
    s_QtX11Display = DisplayStr;
}

/// \brief Determine if we are running a remote X11 session
bool MythXDisplay::DisplayIsRemote()
{
    bool result = false;
    if (auto * display = MythXDisplay::OpenMythXDisplay(false); display != nullptr)
    {
        QString displayname(DisplayString(display->GetDisplay()));

        // DISPLAY=:x or DISPLAY=unix:x are local
        // DISPLAY=hostname:x is remote
        // DISPLAY=/xxx/xxx/.../org.macosforge.xquartz:x is local OS X
        // x can be numbers n or n.n
        // Anything else including DISPLAY not set is assumed local,
        // in that case we are probably not running under X11
        if (!displayname.isEmpty() && !displayname.startsWith(":") &&
            !displayname.startsWith("unix:") && !displayname.startsWith("/") &&
             displayname.contains(':'))
        {
            result = true;
        }
        delete display;
    }
    return result;
}

MythXDisplay::~MythXDisplay()
{
    MythXLocker locker(this);
    if (m_disp)
        XCloseDisplay(m_disp);
}

/*! \brief Open the display
 *
 * \note If the '-display' command line argument is not set both this function
 * and Qt's xcb platform plugin will pass a null string to XOpenDisplay - which
 * will in turn use the DISPLAY environment variable to determince which X11
 * connection to open. If the '-display' command line argument is used, we set
 * s_QtX11Display and the argument is also passed through to the xcb platform
 * plugin by Qt (via the QApplication constructor).
 * So in all cases, the following code should open the same display that is in use by Qt
 * (and avoids linking to QX11Extras or including private Qt platform headers).
 *
*/
bool MythXDisplay::Open(void)
{
    MythXLocker locker(this);

    m_displayName = s_QtX11Display;
    const char *dispCStr = nullptr;
    if (!m_displayName.isEmpty())
        dispCStr = m_displayName.toLatin1().constData();

    m_disp = XOpenDisplay(dispCStr);
    if (!m_disp)
        return false;

    m_screenNum = DefaultScreen(m_disp);
    m_screen    = DefaultScreenOfDisplay(m_disp);
    m_depth     = DefaultDepthOfScreen(m_screen);
    m_root      = DefaultRootWindow(m_disp);

    return true;
}

/**
 * Return the size of the X Display in millimeters.  This corresponds
 * to the size of the desktop, not necessarily to the size of single
 * screen.
 */
QSize MythXDisplay::GetDisplayDimensions(void)
{
    MythXLocker locker(this);
    int displayWidthMM  = DisplayWidthMM( m_disp, m_screenNum);
    int displayHeightMM = DisplayHeightMM(m_disp, m_screenNum);
    return { displayWidthMM, displayHeightMM };
}

void MythXDisplay::Sync(bool Flush)
{
    Lock();
    XSync(m_disp, Flush);
    Unlock();
}
