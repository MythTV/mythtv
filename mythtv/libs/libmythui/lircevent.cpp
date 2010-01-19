
#include "lircevent.h"

#include <QCoreApplication>
#include <QString>

#include "mythmainwindow.h"

QEvent::Type LircKeycodeEvent::kEventType =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type LircMuteEvent::kEventType =
    (QEvent::Type) QEvent::registerEventType();

LircEventLock::LircEventLock(bool lock_events) 
             : events_locked(false)
{
    if (lock_events)
        lock();
}

LircEventLock::~LircEventLock()
{
    if (events_locked)
        unlock();
}

void LircEventLock::lock()
{
    MythMainWindow *mw = GetMythMainWindow();
    if (mw)
    {
        events_locked = true;
        QCoreApplication::postEvent((QObject *)mw,
                                new LircMuteEvent(events_locked));
    }
}

void LircEventLock::unlock()
{
    MythMainWindow *mw = GetMythMainWindow();
    if (mw)
    {
        events_locked = false;
        QCoreApplication::postEvent((QObject *)mw,
                                new LircMuteEvent(events_locked));
    }
}
