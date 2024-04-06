#ifndef MYTHHTTPTHREADPOOL_H
#define MYTHHTTPTHREADPOOL_H

// MythTV
#include "libmythbase/serverpool.h"

class MythHTTPThread;

class MythHTTPThreadPool : public ServerPool
{
    Q_OBJECT

  public:
    MythHTTPThreadPool();
   ~MythHTTPThreadPool() override;

    size_t AvailableThreads() const;
    size_t MaxThreads() const;
    size_t ThreadCount() const;
    void   AddThread(MythHTTPThread* Thread);

  public slots:
    void   ThreadFinished();
    void   ThreadUpgraded(QThread* Thread);

  private:
    Q_DISABLE_COPY(MythHTTPThreadPool)
    size_t m_maxThreads { 4 };
    std::list<MythHTTPThread*> m_threads;
    std::list<MythHTTPThread*> m_upgradedThreads;
};

#endif
