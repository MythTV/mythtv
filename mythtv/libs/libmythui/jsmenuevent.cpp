/*----------------------------------------------------------------------------
** jsmenuevent.cpp
**  GPL license; Original copyright 2004 Jeremy White <jwhite@whitesen.org>
**     although this is largely a derivative of lircevent.cpp
**--------------------------------------------------------------------------*/

// Own header
#include "jsmenuevent.h"

// QT headers
#include <QCoreApplication>
#include <QString>

// Mythui headers
#include "mythmainwindow.h"

QEvent::Type JoystickKeycodeEvent::kEventType =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type JoystickMenuMuteEvent::kEventType =
    (QEvent::Type) QEvent::registerEventType();

JoystickMenuEventLock::JoystickMenuEventLock(bool lock_events)
             : m_eventsLocked(false)
{
    if (lock_events)
        lock();
}

JoystickMenuEventLock::~JoystickMenuEventLock()
{
    if (m_eventsLocked)
        unlock();
}

void JoystickMenuEventLock::lock()
{
    MythMainWindow *mw = GetMythMainWindow();
    if (mw)
    {
        m_eventsLocked = true;
        QCoreApplication::postEvent((QObject *)mw,
                                new JoystickMenuMuteEvent(m_eventsLocked));
    }
}

void JoystickMenuEventLock::unlock()
{
    MythMainWindow *mw = GetMythMainWindow();
    if (mw)
    {
        m_eventsLocked = false;
        QCoreApplication::postEvent((QObject *)mw,
                                new JoystickMenuMuteEvent(m_eventsLocked));
    }
}
