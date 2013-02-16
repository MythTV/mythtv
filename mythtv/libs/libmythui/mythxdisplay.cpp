/** This file is intended to hold X11 specific utility functions */
#include <map>
#include <vector>

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
typedef int (*XErrorCallbackType)(Display *, XErrorEvent *);
typedef std::vector<XErrorEvent>       XErrorVectorType;
std::map<Display*, XErrorVectorType>   xerrors;
std::map<Display*, XErrorCallbackType> xerror_handlers;
std::map<Display*, MythXDisplay*>      xdisplays;
#endif // USING_X11

#include <QMutex>

// Everything below this line is only compiled if using X11
#ifdef USING_X11

static int ErrorHandler(Display *d, XErrorEvent *xeev)
{
    xerrors[d].push_back(*xeev);
    return 0;
}

void LockMythXDisplays(bool lock)
{
    if (lock)
    {
        std::map<Display*, MythXDisplay*>::iterator it;
        for (it = xdisplays.begin(); it != xdisplays.end(); ++it)
            it->second->Lock();
    }
    else
    {
        std::map<Display*, MythXDisplay*>::reverse_iterator it;
        for (it = xdisplays.rbegin(); it != xdisplays.rend(); ++it)
            it->second->Unlock();
    }
}

MythXDisplay *GetMythXDisplay(Display *d)
{
    if (xdisplays.count(d))
        return xdisplays[d];
    return NULL;
}

MythXDisplay *OpenMythXDisplay(void)
{
    MythXDisplay *disp = new MythXDisplay();
    if (disp && disp->Open())
        return disp;

    LOG(VB_GENERAL, LOG_CRIT, "MythXOpenDisplay() failed");
    delete disp;
    return NULL;
}

MythXDisplay::MythXDisplay()
  : m_disp(NULL), m_screen_num(0), m_screen(NULL),
    m_depth(0), m_black(0), m_gc(0),
    m_root(0), m_lock(QMutex::Recursive)
{
}

MythXDisplay::~MythXDisplay()
{
    MythXLocker locker(this);
    if (m_disp)
    {
        if (m_gc)
            XFreeGC(m_disp, m_gc);
        StopLog();
        if (xdisplays.count(m_disp))
            xdisplays.erase(m_disp);
        XCloseDisplay(m_disp);
        m_disp = NULL;
    }
}

bool MythXDisplay::Open(void)
{
    MythXLocker locker(this);

    QString dispStr = GetMythUI()->GetX11Display();
    const char *dispCStr = NULL;
    if (!dispStr.isEmpty())
        dispCStr = dispStr.toAscii().constData();

    m_disp = XOpenDisplay(dispCStr);
    if (!m_disp)
        return false;

    xdisplays[m_disp] = this;
    m_screen_num = DefaultScreen(m_disp);
    m_screen     = DefaultScreenOfDisplay(m_disp);
    m_black      = XBlackPixel(m_disp, m_screen_num);
    m_depth      = DefaultDepthOfScreen(m_screen);
    m_root       = DefaultRootWindow(m_disp);

    return true;
}

bool MythXDisplay::CreateGC(Window win)
{
    StartLog();
    XLOCK(this, m_gc = XCreateGC(m_disp, win, 0, NULL));
    return StopLog();
}

void MythXDisplay::SetForeground(unsigned long color)
{
    if (!m_gc)
        return;
    XLOCK(this, XSetForeground(m_disp, m_gc, color));
}

void MythXDisplay::FillRectangle(Window win, const QRect &rect)
{
    if (!m_gc)
        return;
    XLOCK(this, XFillRectangle(m_disp, win, m_gc,
                              rect.left(), rect.top(),
                              rect.width(), rect.height()));
}

void MythXDisplay::MoveResizeWin(Window win, const QRect &rect)
{
    XLOCK(this, XMoveResizeWindow(m_disp, win,
                                 rect.left(), rect.top(),
                                 rect.width(), rect.height()));
}

int MythXDisplay::GetNumberXineramaScreens(void)
{
    MythXLocker locker(this);
    int nr_xinerama_screens = 0;
    int event_base = 0, error_base = 0;
    if (XineramaQueryExtension(m_disp, &event_base, &error_base) &&
        XineramaIsActive(m_disp))
    {
        XFree(XineramaQueryScreens(m_disp, &nr_xinerama_screens));
    }
    return nr_xinerama_screens;
}

QSize MythXDisplay::GetDisplaySize(void)
{
    MythXLocker locker(this);
    int displayWidthPixel  = DisplayWidth( m_disp, m_screen_num);
    int displayHeightPixel = DisplayHeight(m_disp, m_screen_num);
    return QSize(displayWidthPixel, displayHeightPixel);
}

QSize MythXDisplay::GetDisplayDimensions(void)
{
    MythXLocker locker(this);
    int displayWidthMM  = DisplayWidthMM( m_disp, m_screen_num);
    int displayHeightMM = DisplayHeightMM(m_disp, m_screen_num);
    return QSize(displayWidthMM, displayHeightMM);
}

float MythXDisplay::GetRefreshRate(void)
{
    XF86VidModeModeLine mode_line;
    int dot_clock;
    MythXLocker locker(this);

    if (!XF86VidModeGetModeLine(m_disp, m_screen_num, &dot_clock, &mode_line))
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

    if (((mode_line.flags & V_INTERLACE) != 0) &&
        rate > 24.5 && rate < 30.5)
    {
        LOG(VB_PLAYBACK, LOG_INFO,
                "Doubling refresh rate for interlaced display.");
        rate *= 2.0;
    }

    return rate;
}

void MythXDisplay::Sync(bool flush)
{
    XLOCK(this, XSync(m_disp, flush));
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

bool MythXDisplay::CheckErrors(Display *disp)
{
    if (!disp)
        CheckOrphanedErrors();

    Display *d = disp ? disp : m_disp;
    if (!d)
        return false;

    if (!xerrors.count(d))
        return true;

    MythXLocker locker(this);
    Sync();
    const std::vector<XErrorEvent>& events = xerrors[d];

    if (events.empty())
        return true;

    for (int i = events.size() -1; i>=0; --i)
    {
        char buf[200];
        XGetErrorText(d, events[i].error_code, buf, sizeof(buf));
        LOG(VB_GENERAL, LOG_ERR,
            QString("\n"
                   "XError type: %1\n"
                   "  serial no: %2\n"
                   "   err code: %3 (%4)\n"
                   "   req code: %5\n"
                   " minor code: %6\n"
                   "resource id: %7\n")
                   .arg(events[i].type)
                   .arg(events[i].serial)
                   .arg(events[i].error_code).arg(buf)
                   .arg(events[i].request_code)
                   .arg(events[i].minor_code)
                   .arg(events[i].resourceid));
    }
    xerrors.erase(d);
    return false;
}

void MythXDisplay::CheckOrphanedErrors(void)
{
    if (xerrors.empty())
        return;

    std::map<Display*, XErrorVectorType>::iterator errors = xerrors.begin();
    for (; errors != xerrors.end(); ++errors)
        if (!xerror_handlers.count(errors->first))
            CheckErrors(errors->first);
}

#endif // USING_X11
