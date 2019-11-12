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
    explicit MThreadPool(const QString &name);
    ~MThreadPool();

    void Stop(void);
    void DeletePoolThreads(void);

    static MThreadPool *globalInstance(void);
    static void StopAllPools(void);
    static void ShutdownAllPools(void);

    void start(QRunnable *runnable, const QString& debugName, int priority = 0);
    bool tryStart(QRunnable *runnable, const QString& debugName);

    void startReserved(QRunnable *runnable, const QString& debugName,
                       int waitForAvailMS = 0);

    int expiryTimeout(void) const;
    void setExpiryTimeout(int expiryTimeout);

    int maxThreadCount(void) const;
    void setMaxThreadCount(int maxThreadCount);

    int activeThreadCount(void) const;

    void waitForDone(void);

  private:
    MThreadPool(const MThreadPool &) = delete;            // not copyable
    MThreadPool &operator=(const MThreadPool &) = delete; // not copyable
    bool TryStartInternal(QRunnable*, const QString&, bool);
    void NotifyAvailable(MPoolThread*);
    void NotifyDone(MPoolThread*);
    void ReleaseThread(void);


    MThreadPoolPrivate *m_priv {nullptr};
};

#endif // _MYTH_THREAD_POOL_H_

/* vim: set expandtab tabstop=4 shiftwidth=4: */
