// MythTV
#include "config.h"
#include "mythlogging.h"
#include "mythuihelper.h"
#include "mythxdisplay.h"

// Std
#include <map>
#include <vector>

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

/// \brief Determine if we are running a remote X11 session
bool MythXDisplay::DisplayIsRemote(void)
{
    bool result = false;
    auto * display = MythXDisplay::OpenMythXDisplay(false);

    if (display)
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
        std::string buf(200,'\0');
        XGetErrorText(d, event.error_code, buf.data(), buf.size());
        LOG(VB_GENERAL, LOG_ERR,
            QString("XError type: %1\nSerial no: %2\nErr code: %3 (%4)\n"
                   "Req code: %5\nmMinor code: %6\nResource id: %7\n")
                   .arg(event.type).arg(event.serial)
                   .arg(event.error_code).arg(buf.data())
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
