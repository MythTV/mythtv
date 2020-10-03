// MythTV
#include "config.h"
#include "mythscreensaver.h"

#ifdef USING_DRM
#include "platforms/mythscreensaverdrm.h"
#endif

#ifdef USING_DBUS
#include "platforms/mythscreensaverdbus.h"
#endif

#if CONFIG_DARWIN
#include "platforms/mythscreensaverosx.h"
#endif

#ifdef Q_OS_ANDROID
#include "platforms/mythscreensaverandroid.h"
#endif

#ifdef USING_WAYLANDEXTRAS
#include "platforms/mythscreensaverwayland.h"
#endif

#ifdef USING_X11
#include "platforms/mythscreensaverx11.h"
#include "platforms/mythxdisplay.h"
#endif

MythScreenSaverControl::MythScreenSaverControl(MythDisplay* mDisplay)
{
#if defined(USING_DBUS)
    m_screenSavers.push_back(new MythScreenSaverDBus());
#endif
#if defined(USING_X11)
    MythXDisplay* display = MythXDisplay::OpenMythXDisplay(false);
    if (display)
    {
        m_screenSavers.push_back(new MythScreenSaverX11());
        delete display;
    }
#elif CONFIG_DARWIN
    m_screenSavers.push_back(new MythScreenSaverOSX());
#endif
#if defined(ANDROID)
    m_screenSavers.push_back(new MythScreenSaverAndroid());
#endif
#ifdef USING_DRM
    MythScreenSaverDRM* drmsaver = MythScreenSaverDRM::Create(mDisplay);
    if (drmsaver)
        m_screenSavers.push_back(drmsaver);
#else
    (void)mDisplay;
#endif
#ifdef USING_WAYLANDEXTRAS
    m_screenSavers.push_back(new MythScreenSaverWayland());
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
