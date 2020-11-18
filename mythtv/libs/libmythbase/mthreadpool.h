// -*- Mode: c++ -*-

#ifndef MYTH_THREAD_POOL_H
#define MYTH_THREAD_POOL_H

#include <QString>

#include "mythbaseexp.h"
#include "mythchrono.h"

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
    MThreadPool(const MThreadPool &) = delete;            // not copyable
    MThreadPool &operator=(const MThreadPool &) = delete; // not copyable

    void Stop(void);
    void DeletePoolThreads(void);

    static MThreadPool *globalInstance(void);
    static void StopAllPools(void);
    static void ShutdownAllPools(void);

    void start(QRunnable *runnable, const QString& debugName, int priority = 0);
    bool tryStart(QRunnable *runnable, const QString& debugName);

    void startReserved(QRunnable *runnable, const QString& debugName,
                       std::chrono::milliseconds waitForAvailMS = 0ms);

    std::chrono::milliseconds expiryTimeout(void) const;
    void setExpiryTimeout(std::chrono::milliseconds expiryTimeout);

    int maxThreadCount(void) const;
    void setMaxThreadCount(int maxThreadCount);

    int activeThreadCount(void) const;

    void waitForDone(void);

  private:
    bool TryStartInternal(QRunnable *runnable, const QString& debugName, bool reserved);
    void NotifyAvailable(MPoolThread *thread);
    void NotifyDone(MPoolThread *thread);
    void ReleaseThread(void);


    MThreadPoolPrivate *m_priv {nullptr};
};

#endif // MYTH_THREAD_POOL_H

/* vim: set expandtab tabstop=4 shiftwidth=4: */
