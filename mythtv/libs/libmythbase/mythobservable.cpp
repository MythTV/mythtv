#include <QCoreApplication>
#include <QObject>
#include <QMutex>

#include "mythobservable.h"

/** \class MythObservable
 *  \brief Superclass for making an object have a set of listeners
 *
 *  This superclass provides the basic API for adding and removing
 *  listeners and iterating across them. It is typically used to post
 *  events to listening QObjects.
 *
 *  MythEvents can be dispatched to all listeners by calling dispatch
 *
 *  The name MythObservable is 'wrong', since all the methods refer to
 *  the observers as listeners (ie. addListener), however,
 *  MythListenable just doesn't sound right, and fixing all the calls
 *  to addListener was too big a patch.
 */

MythObservable::~MythObservable()
{
    m_lock->lock();
    m_listeners.clear();
    m_lock->unlock();
    delete m_lock;
    m_lock = nullptr;
}

/** \brief Add a listener to the observable
 *
 *  Adds the given QObject to the list of objects that observe
 *  this observable.
 *
 *  \param listener the QObject that will listen to this observable
 */
void MythObservable::addListener(QObject *listener)
{
    if (listener)
    {
        QMutexLocker locker(m_lock);
        m_listeners.insert(listener);
    }
}


/** \brief Remove a listener to the observable
 *
 *  Remove the given QObject from the list of objects that
 *  observe this observable.
 *
 *  \param listener the QObject that already listens to this observable
 */
void MythObservable::removeListener(QObject *listener)
{
    if (listener)
    {
        QMutexLocker locker(m_lock);
        m_listeners.remove(listener);
        QCoreApplication::removePostedEvents(listener);
    }
}

/** \brief Dispatch an event to all listeners
 *
 *  Makes a copy of the event on the heap by calling
 *  MythEvent::clone() and dispatches is by calling
 *  QCoreApplication::postEvent().
 *
 *  \param event MythEvent to dispatch.
 */
void MythObservable::dispatch(const MythEvent &event)
{
    QMutexLocker locker(m_lock);

    for (auto *listener : std::as_const(m_listeners))
        QCoreApplication::postEvent(listener, event.clone());
}
