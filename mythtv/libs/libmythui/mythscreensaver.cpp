// MythTV
#include "config.h"
#include "mythscreensaver.h"
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

#ifdef Q_OS_ANDROID
#include "screensaver-android.h"
#endif

MythScreenSaverControl::MythScreenSaverControl()
{
#if defined(USING_DBUS)
    m_screenSavers.push_back(new MythScreenSaverDBus());
#endif
#if defined(USING_X11)
    MythXDisplay* display = MythXDisplay::OpenMythXDisplay(false);
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

MythScreenSaverControl::~MythScreenSaverControl()
{
    for (auto * screensaver : m_screenSavers)
        delete screensaver;
}

void MythScreenSaverControl::Disable()
{
    for (auto * screensaver : m_screenSavers)
        screensaver->Disable();
}

void MythScreenSaverControl::Restore()
{
    for (auto * screensaver : m_screenSavers)
        screensaver->Restore();
}

void MythScreenSaverControl::Reset()
{
    for (auto * screensaver : m_screenSavers)
        screensaver->Reset();
}

bool MythScreenSaverControl::Asleep()
{
    for (auto * screensaver : m_screenSavers)
        if (screensaver->Asleep())
            return true;
    return false;
}
