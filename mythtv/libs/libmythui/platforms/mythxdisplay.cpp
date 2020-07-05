// Std
#include <map>
#include <vector>

// MythTV
#include "config.h" // for CONFIG_DARWIN
#include "mythlogging.h"
#include "mythuihelper.h"

#ifdef USING_X11
#include "mythxdisplay.h"
#ifndef V_INTERLACE
#define V_INTERLACE (0x010)
#endif
extern "C" {
#include <X11/extensions/Xinerama.h>
#include <X11/extensions/xf86vmode.h>
}
using XErrorCallbackType = int (*)(Display *, XErrorEvent *);
using XErrorVectorType = std::vector<XErrorEvent>;
static std::map<Display*, XErrorVectorType>   xerrors;
static std::map<Display*, XErrorCallbackType> xerror_handlers;

QString MythXDisplay::s_QtX11Display;

static int ErrorHandler(Display *d, XErrorEvent *xeev)
{
    xerrors[d].push_back(*xeev);
    return 0;
}

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
    MythXDisplay *m_disp { nullptr };
};

MythXDisplay* MythXDisplay::OpenMythXDisplay(bool Warn /*= true*/)
{
    auto *disp = new MythXDisplay();
    if (disp && disp->Open())
        return disp;

    if (Warn)
        LOG(VB_GENERAL, LOG_CRIT, "MythXOpenDisplay() failed");
    delete disp;
    return nullptr;
}

void MythXDisplay::SetQtX11Display(const QString &_Display)
{
    s_QtX11Display = _Display;
}

MythXDisplay::~MythXDisplay()
{
    MythXLocker locker(this);
    if (m_disp)
    {
        StopLog();
        XCloseDisplay(m_disp);
        m_disp = nullptr;
    }
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

    m_screenNum  = DefaultScreen(m_disp);
    m_screen     = DefaultScreenOfDisplay(m_disp);
    m_depth      = DefaultDepthOfScreen(m_screen);
    m_root       = DefaultRootWindow(m_disp);

    return true;
}

/**
 * Return the size of the X Display in pixels.  This corresponds to
 * the size of the desktop, not necessarily to the size of single
 * screen.
 */
QSize MythXDisplay::GetDisplaySize(void)
{
    XF86VidModeModeLine mode;
    int dummy = 0;
    MythXLocker locker(this);

    if (!XF86VidModeGetModeLine(m_disp, m_screenNum, &dummy, &mode))
    {
        LOG(VB_GENERAL, LOG_ERR, "X11 ModeLine query failed");
        // Fallback to old display size code - which is not updated for mode switches
        return {DisplayWidth(m_disp, m_screenNum),
                DisplayHeight(m_disp, m_screenNum)};
    }

    return { mode.hdisplay, mode.vdisplay };
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

double MythXDisplay::GetRefreshRate(void)
{
    XF86VidModeModeLine mode_line;
    int dot_clock = 0;
    MythXLocker locker(this);

    if (!XF86VidModeGetModeLine(m_disp, m_screenNum, &dot_clock, &mode_line))
    {
        LOG(VB_GENERAL, LOG_ERR, "X11 ModeLine query failed");
        return -1;
    }

    double rate = mode_line.htotal * mode_line.vtotal;

    // Catch bad data from video drivers (divide by zero causes return of NaN)
    if (rate == 0.0 || dot_clock == 0)
    {
        LOG(VB_GENERAL, LOG_ERR, "X11 ModeLine query returned zeroes");
        return -1;
    }

    rate = (dot_clock * 1000.0) / rate;

    if (((mode_line.flags & V_INTERLACE) != 0) && rate > 24.5 && rate < 30.5)
    {
        LOG(VB_PLAYBACK, LOG_INFO,
                "Doubling refresh rate for interlaced display.");
        rate *= 2.0;
    }

    return rate;
}

void MythXDisplay::Sync(bool Flush)
{
    XLOCK(this, XSync(m_disp, Flush));
}

void MythXDisplay::StartLog(void)
{
    if (!m_disp || xerror_handlers.count(m_disp))
        return;

    Sync();
    XLOCK(this, xerror_handlers[m_disp] = XSetErrorHandler(ErrorHandler));
}

bool MythXDisplay::StopLog(void)
{
    if (!(m_disp && xerror_handlers.count(m_disp)))
        return false;

    Sync();
    XErrorCallbackType old_handler = xerror_handlers[m_disp];
    XLOCK(this, XSetErrorHandler(old_handler));
    xerror_handlers.erase(m_disp);
    return CheckErrors();
}

bool MythXDisplay::CheckErrors(Display *Disp)
{
    if (!Disp)
        CheckOrphanedErrors();

    Display *d = Disp ? Disp : m_disp;
    if (!d)
        return false;

    if (!xerrors.count(d))
        return true;

    MythXLocker locker(this);
    Sync();
    const std::vector<XErrorEvent>& events = xerrors[d];

    if (events.empty())
        return true;

    for (const auto & event : events)
    {
        char buf[200];
        XGetErrorText(d, event.error_code, buf, sizeof(buf));
        LOG(VB_GENERAL, LOG_ERR,
            QString("XError type: %1\nSerial no: %2\nErr code: %3 (%4)\n"
                   "Req code: %5\nmMinor code: %6\nResource id: %7\n")
                   .arg(event.type).arg(event.serial)
                   .arg(event.error_code).arg(buf)
                   .arg(event.request_code).arg(event.minor_code)
                   .arg(event.resourceid));
    }
    xerrors.erase(d);
    return false;
}

void MythXDisplay::CheckOrphanedErrors(void)
{
    if (xerrors.empty())
        return;

    for (auto & xerror : xerrors)
        if (!xerror_handlers.count(xerror.first))
            CheckErrors(xerror.first);
}

#endif // USING_X11
