#include <qobject.h>
#include <qapplication.h>

#include "mythobservable.h"

MythObservable::MythObservable()
{
}

MythObservable::~MythObservable()
{
}

void MythObservable::addListener(QObject *listener)
{
    if (m_listeners.indexOf(listener) == -1)
        m_listeners.append(listener);
}

void MythObservable::removeListener(QObject *listener)
{
    if (m_listeners.indexOf(listener) != -1)
        m_listeners.removeAll(listener);
}

QObject* MythObservable::firstListener()
{
    m_listener_it = m_listeners.begin();
    return m_listeners.first();
}

QObject* MythObservable::nextListener()
{
    ++m_listener_it;
    if (m_listener_it != m_listeners.end())
        return *m_listener_it;
    else
        return NULL;
}

QList<QObject*> MythObservable::getListeners()
{
    return m_listeners;
}

void MythObservable::dispatch(MythEvent &event)
{
    QList<QObject*>::const_iterator it = m_listeners.begin();
    while (it != m_listeners.end())
    {
        QApplication::postEvent(*it, event.clone());
        ++it;
    }
}

void MythObservable::dispatchNow(MythEvent &event)
{
    QList<QObject*>::const_iterator it = m_listeners.begin();
    while (it != m_listeners.end())
    {
        QApplication::sendEvent(*it, event.clone());
        ++it;
    }
}

