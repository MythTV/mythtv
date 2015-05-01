#include "config.h"
#include "screensaver.h"
#include "screensaver-null.h"

#ifdef USING_DBUS
#include "screensaver-dbus.h"
#endif // USING_DBUS

#ifdef USING_X11
#include "screensaver-x11.h"
#endif // USING_X11

#if CONFIG_DARWIN
#include "screensaver-osx.h"
#endif

QEvent::Type ScreenSaverEvent::kEventType =
    (QEvent::Type) QEvent::registerEventType();

ScreenSaverControl::ScreenSaverControl() :
    m_screenSavers(QList<ScreenSaver *>())
{
    ScreenSaver * tmp;
#if defined(USING_DBUS)
    tmp = new ScreenSaverDBus();
    m_screenSavers.push_back(tmp);
#endif
#if defined(USING_X11)
    tmp = new ScreenSaverX11();
    m_screenSavers.push_back(tmp);
#elif CONFIG_DARWIN
    tmp = new ScreenSaverOSX();
    m_screenSavers.push_back(tmp);
#endif
#if not (defined(USING_DBUS) || defined(USING_X11) || CONFIG_DARWIN)
    tmp = new ScreenSaverNull();
    m_screenSavers.push_back(tmp);
#endif
}

ScreenSaverControl::~ScreenSaverControl() {
    while (!m_screenSavers.isEmpty()) {
        ScreenSaver *tmp = m_screenSavers.takeLast();
        delete tmp;
    }
}

void ScreenSaverControl::Disable(void) {
    QList<ScreenSaver *>::iterator i;
    for (i = m_screenSavers.begin(); i != m_screenSavers.end(); ++i) {
        (*i)->Disable();
    }
}

void ScreenSaverControl::Restore(void) {
    QList<ScreenSaver *>::iterator i;
    for (i = m_screenSavers.begin(); i != m_screenSavers.end(); ++i) {
        (*i)->Restore();
    }
}

void ScreenSaverControl::Reset(void) {
    QList<ScreenSaver *>::iterator i;
    for (i = m_screenSavers.begin(); i != m_screenSavers.end(); ++i) {
        (*i)->Reset();
    }
}

bool ScreenSaverControl::Asleep(void) {
    QList<ScreenSaver *>::iterator i;
    for (i = m_screenSavers.begin(); i != m_screenSavers.end(); ++i) {
        if((*i)->Asleep()) {
            return true;
        }
    }
    return false;
}
