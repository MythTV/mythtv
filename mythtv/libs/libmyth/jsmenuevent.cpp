/*----------------------------------------------------------------------------
** jsmenuevent.cpp
**  GPL license; Original copyright 2004 Jeremy White <jwhite@whitesen.org>
**     although this is largely a derivative of lircevent.cpp
**--------------------------------------------------------------------------*/
#include <qapplication.h>
#include <qstring.h>
#include "mythcontext.h"

#include "jsmenuevent.h"

JoystickMenuEventLock::JoystickMenuEventLock(bool lock_events) 
             : events_locked(false)
{
    if (lock_events)
        lock();
}

JoystickMenuEventLock::~JoystickMenuEventLock()
{
    if (events_locked)
        unlock();
}

void JoystickMenuEventLock::lock()
{
    MythMainWindow *mw = gContext->GetMainWindow();
    if (mw)
    {
        events_locked = true;
        QApplication::postEvent((QObject *)mw,
                                new JoystickMenuMuteEvent(events_locked));
    }
}

void JoystickMenuEventLock::unlock()
{
    MythMainWindow *mw = gContext->GetMainWindow();
    if (mw)
    {
        events_locked = false;
        QApplication::postEvent((QObject *)mw,
                                new JoystickMenuMuteEvent(events_locked));
    }
}
