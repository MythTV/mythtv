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
    MythObservable()
        : m_lock(new QMutex()) {}
    virtual ~MythObservable();

    void addListener(QObject *listener);
    void removeListener(QObject *listener);

    void dispatch(const MythEvent &event);

    bool hasListeners(void) { return !m_listeners.isEmpty(); }

  private:
    Q_DISABLE_COPY_MOVE(MythObservable)

  protected:
    QMutex         *m_lock {nullptr};
    QSet<QObject*>  m_listeners;
};

#endif /* MYTHOBSERVABLE_H */
