#include <qapplication.h>
#include <qstring.h>
#include "mythcontext.h"

#include "lircevent.h"

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
    if (!gContext)
        return;
    MythMainWindow *mw = gContext->GetMainWindow();
    if (mw)
    {
        events_locked = true;
        QApplication::postEvent((QObject *)mw,
                                new LircMuteEvent(events_locked));
    }
}

void LircEventLock::unlock()
{
    MythMainWindow *mw = gContext->GetMainWindow();
    if (mw)
    {
        events_locked = false;
        QApplication::postEvent((QObject *)mw,
                                new LircMuteEvent(events_locked));
    }
}
