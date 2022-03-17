#ifndef MYTHHTTPTHREAD_H
#define MYTHHTTPTHREAD_H

// MythTV
#include "libmythbase/http/mythhttptypes.h"
#include "libmythbase/mthread.h"

class MythHTTPSocket;
class MythHTTPServer;
class MythHTTPThreadPool;

class MythHTTPThread : public MThread
{
  public:
    MythHTTPThread(MythHTTPServer* Server, MythHTTPConfig Config,
                   const QString& ThreadName, qintptr Socket, bool Ssl);
    void Quit();

  protected:
    void run() override;

  private:
    Q_DISABLE_COPY(MythHTTPThread)

    MythHTTPServer* m_server { nullptr };
    MythHTTPConfig  m_config;
    qintptr         m_socketFD { 0 };
    MythHTTPSocket* m_socket { nullptr };
    bool            m_ssl    { false };
};

#endif
