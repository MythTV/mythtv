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
    QMutexLocker locked (&mutex);
    if (m_listeners.find(listener) == -1)
        m_listeners.append(listener);
}

void MythObservable::removeListener(QObject *listener)
{
    QMutexLocker locked (&mutex);
    if (m_listeners.find(listener) != -1)
        m_listeners.remove(listener);
}

void MythObservable::dispatch(MythEvent &event)
{
    QMutexLocker locked (&mutex);
    QObject *listener = m_listeners.first();
    while (listener)
    {
        QApplication::postEvent(listener, event.clone());
        listener = m_listeners.next();
    }
}

void MythObservable::dispatchNow(MythEvent &event)
{
    QMutexLocker locked (&mutex);
    QObject *listener = m_listeners.first();
    while (listener)
    {
        QApplication::sendEvent(listener, event.clone());
        listener = m_listeners.next();
    }
}

