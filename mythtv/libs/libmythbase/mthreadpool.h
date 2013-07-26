// -*- Mode: c++ -*-

#ifndef _MYTH_THREAD_POOL_H_
#define _MYTH_THREAD_POOL_H_

#include <QString>

#include "mythbaseexp.h"

class MThreadPoolPrivate;
class MPoolThread;
class QRunnable;

/**
  * \ingroup mthreadpool
  */
class MBASE_PUBLIC MThreadPool
{
    friend class MPoolThread;
  public:
    MThreadPool(const QString &name);
    ~MThreadPool();

    void Stop(void);
    void DeletePoolThreads(void);

    static MThreadPool *globalInstance(void);
    static void StopAllPools(void);
    static void ShutdownAllPools(void);

    void start(QRunnable *runnable, QString debugName, int priority = 0);
    bool tryStart(QRunnable *runnable, QString debugName);

    void startReserved(QRunnable *runnable, QString debugName,
                       int waitForAvailMS = 0);

    int expiryTimeout(void) const;
    void setExpiryTimeout(int expiryTimeout);

    int maxThreadCount(void) const;
    void setMaxThreadCount(int maxThreadCount);

    int activeThreadCount(void) const;

    void waitForDone(void);

  private:
    bool TryStartInternal(QRunnable*, QString, bool);
    void NotifyAvailable(MPoolThread*);
    void NotifyDone(MPoolThread*);
    void ReleaseThread(void);


    MThreadPoolPrivate *m_priv;
};

#endif // _MYTH_THREAD_POOL_H_

/* vim: set expandtab tabstop=4 shiftwidth=4: */
