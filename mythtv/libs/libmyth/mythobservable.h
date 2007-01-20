#ifndef MYTHOBSERVABLE_H_
#define MYTHOBSERVABLE_H_

#include <qptrlist.h>
#include <qmutex.h>
#include "mythexp.h"
#include "mythevent.h"

class QObject;

/** \class MythObservable
    \brief Superclass for making an object have a set of listeners
       
    This superclass provides the basic API for adding and removing
    listeners and iterating across them. It is typically used to post
    events to listening QObjects.

       For example, to post a custom event with event id 100 to all listeners :

       \code
       void dispatch()
       {
           QObject *listener = firstListener();
               while (listener) {
                   QApplication::postEvent (listener, new QCustomEvent(100));
               listener = nextListener();  
           }
       }
       \endcode

    MythEvents can be dispatched to all listeners by calling dispatch
    or dispatchNow. The former is much preferred as it uses
    QApplication::postEvent() while the latter uses the blocking
    QApplication::sendEvent().

    The name MythObservable is 'wrong', since all the methods refer to
    the observers as listeners (ie. addListener), however,
    MythListenable just doesn't sound right, and fixing all the calls
    to addListener was too big a patch.
    
    All public methods in MythObservable are reentrant.
*/
class MPUBLIC MythObservable
{
  public:
    MythObservable();
    virtual ~MythObservable();
       
    /** \brief Add a listener to the observable 

        Adds the given QObject to the list of objects that observe
        this observable.

        \param listener the QObject that will listen to this observable
    */
    void addListener(QObject *listener);

    /** \brief Remove a listener to the observable 

         Remove the given QObject from the list of objects that
         observe this observable.

         \param listener the QObject that already listens to this observable
    */
    void removeListener(QObject *listener);

    /** \brief Dispatch an event to all listeners 
                       
        Makes a copy of the event on the heap by calling
        MythEvent::clone and dispatches is by calling
        QApplication::postEvent.
               
        \param event a MythEvent to dispatch.
    */
    void dispatch(MythEvent &event);

    /** \brief Dispatch an event to all listeners 
                       
        See dispatch.

        \note This uses QApplication::sendEvent, which is
        blocking. It's preferred to use dispatch instead.

        \param event a MythEvent to dispatch.
    */
    void dispatchNow(MythEvent &event);

  private:

    QPtrList<QObject> m_listeners;
    QMutex mutex;
};

#endif /* MYTHOBSERVABLE_H */
