
// Own header
#include "screensaver-xdg.h"

// QT headers
#include <QString>

// Mythdb headers
#include "mythverbose.h"

// Mythui headers
#include "mythmainwindow.h"
#include "mythsystem.h"
#include "mythxdisplay.h"

#define LOC QString("ScreenSaverXDG: ")

class ScreenSaverXDGPrivate
{
    friend class ScreenSaverXDG;

public:
    ScreenSaverXDGPrivate(ScreenSaverXDG *outer) :
        m_xdgscreensaverRunning(false),
        m_display(NULL)
    {
        m_xdgscreensaverRunning =
                    myth_system("xdg-screensaver --version >&- 2>&-") == 0;

        if (IsScreenSaverRunning())
            VERBOSE(VB_GENERAL, "xdg-screensaver support enabled");

        m_display = OpenMythXDisplay();
        if (!m_display)
            VERBOSE(VB_IMPORTANT, "ScreenSaverXDGPrivate: Failed to open "
                                  "connection to X11 server");
    }

    ~ScreenSaverXDGPrivate()
    {
        delete m_display;
    }

    bool IsScreenSaverRunning(void) const
    {
        return m_xdgscreensaverRunning;
    }

    QString GetXWindowId(void)
    {
        // return window id
        WId myWId = GetMythMainWindow()->winId();
        QString windowId = QString("0x%1").arg(myWId, 0, 16);
        VERBOSE(VB_PLAYBACK, QString("ScreenSaverXDGPrivate::"
                "GetXWindowId: X window ID of MythMainWindow: %1")
                .arg(windowId));
        return windowId;
    }

private:
    bool m_xdgscreensaverRunning;

    MythXDisplay *m_display;
};

ScreenSaverXDG::ScreenSaverXDG() :
            d(new ScreenSaverXDGPrivate(this))
{
}

ScreenSaverXDG::~ScreenSaverXDG()
{
    delete d;
}

bool ScreenSaverXDG::IsXDGInstalled(void)
{
    bool result = myth_system("xdg-screensaver --version >&- 2>&-") == 0;
    if (result)
        VERBOSE(VB_GENERAL, "xdg-screensaver is available.");
    else
        VERBOSE(VB_GENERAL, "xdg-utils are not installed. For best "
                "screensaver-handling performance and compatibility, please "
                "install xdg-utils.");
    return result;
}

void ScreenSaverXDG::Disable(void)
{
    if (d->m_display)
    {
        VERBOSE(VB_PLAYBACK, LOC + "Calling xdg-screensaver suspend");
        myth_system(QString("xdg-screensaver suspend %1 >&- 2>&- &")
                            .arg(d->GetXWindowId()));
    }
}

void ScreenSaverXDG::Restore(void)
{
    if (d->m_display)
    {
        VERBOSE(VB_PLAYBACK, LOC + "Calling xdg-screensaver resume");
        myth_system(QString("xdg-screensaver resume %1 >&- 2>&- &")
                            .arg(d->GetXWindowId()));
    }
}

void ScreenSaverXDG::Reset(void)
{
    VERBOSE(VB_PLAYBACK, LOC + "Calling xdg-screensaver reset");
    myth_system(QString("xdg-screensaver reset >&- 2>&- &"));
}

bool ScreenSaverXDG::Asleep(void)
{
    return false;
}

