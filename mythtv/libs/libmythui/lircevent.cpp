#include <QApplication>
#include <QString>

#include "mythmainwindow.h"
#include "lircevent.h"

LircEventLock::LircEventLock(bool lock_events)
             : m_eventsLocked(false)
{
    if (lock_events)
        lock();
}

LircEventLock::~LircEventLock()
{
    if (m_eventsLocked)
        unlock();
}

void LircEventLock::lock()
{
    MythMainWindow *mw = GetMythMainWindow();
    if (mw)
    {
        m_eventsLocked = true;
        QApplication::postEvent((QObject *)mw,
                                new LircMuteEvent(m_eventsLocked));
    }
}

void LircEventLock::unlock()
{
    MythMainWindow *mw = GetMythMainWindow();
    if (mw)
    {
        m_eventsLocked = false;
        QApplication::postEvent((QObject *)mw,
                                new LircMuteEvent(m_eventsLocked));
    }
}
