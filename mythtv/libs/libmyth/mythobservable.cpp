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
    if (m_listeners.find(listener) == -1)
        m_listeners.append(listener);
}

void MythObservable::removeListener(QObject *listener)
{
    if (m_listeners.find(listener) != -1)
        m_listeners.remove(listener);
}

QObject* MythObservable::firstListener()
{
    return m_listeners.first();
}

QObject* MythObservable::nextListener()
{
    return m_listeners.next();
}

QPtrList<QObject> MythObservable::getListeners()
{
    return m_listeners;
}

void MythObservable::dispatch(MythEvent &event)
{
    QObject *listener = firstListener();
    while (listener)
    {
        QApplication::postEvent(listener, event.clone());
        listener = nextListener();
    }
}

void MythObservable::dispatchNow(MythEvent &event)
{
    QObject *listener = firstListener();
    while (listener)
    {
        QApplication::sendEvent(listener, event.clone());
        listener = nextListener();
    }
}

