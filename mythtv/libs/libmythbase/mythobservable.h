#ifndef MYTHOBSERVABLE_H_
#define MYTHOBSERVABLE_H_

#include <QSet>
#include "mythevent.h"
#include "mythbaseexp.h"

class QObject;
class QMutex;

class MBASE_PUBLIC MythObservable
{
  public:
    MythObservable();
    virtual ~MythObservable();

    void addListener(QObject *listener);
    void removeListener(QObject *listener);

    void dispatch(const MythEvent &event);

    bool hasListeners(void) { return !m_listeners.isEmpty(); }

  protected:
    QMutex         *m_lock;
    QSet<QObject*>  m_listeners;
};

#endif /* MYTHOBSERVABLE_H */
