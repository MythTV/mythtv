#ifndef MYTHOBSERVABLE_H_
#define MYTHOBSERVABLE_H_

#include <QSet>
#include "mythevent.h"
#include "mythexp.h"

class QObject;
class QMutex;

class MPUBLIC MythObservable
{
  public:
    MythObservable();
    virtual ~MythObservable();

    void addListener(QObject *listener);
    void removeListener(QObject *listener);

    void dispatch(const MythEvent &event);
    void dispatchNow(const MythEvent &event) MDEPRECATED;

    bool hasListeners(void) { return !m_listeners.isEmpty(); }

  protected:
    QMutex         *m_lock;
    QSet<QObject*>  m_listeners;
};

#endif /* MYTHOBSERVABLE_H */
