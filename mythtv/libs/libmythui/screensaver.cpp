// MythTV
#include "config.h"
#include "screensaver.h"
#include "screensaver-null.h"

#ifdef USING_DBUS
#include "screensaver-dbus.h"
#endif // USING_DBUS

#ifdef USING_X11
#include "screensaver-x11.h"
#include "mythxdisplay.h"
#endif // USING_X11

#if CONFIG_DARWIN
#include "screensaver-osx.h"
#endif

#if ANDROID
#include "screensaver-android.h"
#endif

QEvent::Type ScreenSaverEvent::kEventType =
    static_cast<QEvent::Type>(QEvent::registerEventType());

ScreenSaverControl::ScreenSaverControl() :
    m_screenSavers(QList<ScreenSaver *>())
{
#if defined(USING_DBUS)
    m_screenSavers.push_back(new ScreenSaverDBus());
#endif
#if defined(USING_X11)
    MythXDisplay* display = OpenMythXDisplay(false);
    if (display)
    {
        m_screenSavers.push_back(new ScreenSaverX11());
        delete display;
    }
#elif CONFIG_DARWIN
    m_screenSavers.push_back(new ScreenSaverOSX());
#endif
#if defined(ANDROID)
    m_screenSavers.push_back(new ScreenSaverAndroid());
#endif
#if not (defined(USING_DBUS) || defined(USING_X11) || CONFIG_DARWIN || defined(ANDROID))
    m_screenSavers.push_back(new ScreenSaverNull());
#endif
}

ScreenSaverControl::~ScreenSaverControl()
{
    while (!m_screenSavers.isEmpty())
        delete m_screenSavers.takeLast();
}

void ScreenSaverControl::Disable(void)
{
    QList<ScreenSaver *>::iterator i;
    for (i = m_screenSavers.begin(); i != m_screenSavers.end(); ++i)
        (*i)->Disable();
}

void ScreenSaverControl::Restore(void)
{
    QList<ScreenSaver *>::iterator i;
    for (i = m_screenSavers.begin(); i != m_screenSavers.end(); ++i)
        (*i)->Restore();
}

void ScreenSaverControl::Reset(void)
{
    QList<ScreenSaver *>::iterator i;
    for (i = m_screenSavers.begin(); i != m_screenSavers.end(); ++i)
        (*i)->Reset();
}

bool ScreenSaverControl::Asleep(void)
{
    QList<ScreenSaver *>::iterator i;
    for (i = m_screenSavers.begin(); i != m_screenSavers.end(); ++i)
        if((*i)->Asleep())
            return true;
    return false;
}
