#ifndef MYTHHTTPSERVER_H
#define MYTHHTTPSERVER_H

#include "libmythbase/mythconfig.h"

// Qt
#include <QHostInfo>
#include <QQueue>

// MythTV
#include "libmythbase/http/mythhttpthreadpool.h"
#include "libmythbase/http/mythhttptypes.h"

class MythTcpQueueEntry
{
  public:
    qintptr         m_socketFD {0};
    bool            m_ssl {false};
};

class MythHTTPServer : public MythHTTPThreadPool
{
    Q_OBJECT

    friend class MythHTTPInstance;

  signals:
    // Inbound
    void EnableHTTP     (bool Enable);
    void AddPaths       (const QStringList& Paths);
    void RemovePaths    (const QStringList& Paths);
    void AddHandlers    (const HTTPHandlers& Handlers);
    void RemoveHandlers (const HTTPHandlers& Handlers);
    void AddServices    (const HTTPServices& Services);
    void RemoveServices (const HTTPServices& Services);
    void AddErrorPageHandler (const HTTPHandler& Handler);
    // Outbound
    void PathsChanged   (const QStringList& Paths);
    void HandlersChanged(const HTTPHandlers& Handlers);
    void ServicesChanged(const HTTPServices& Services);
    void OriginsChanged (const QStringList& Origins);
    void HostsChanged   (const QStringList& Hosts);
    void ErrorHandlerChanged (const HTTPHandler& Handler);
    // Internal
    void MasterResolved (QHostInfo Info);
    void HostResolved   (QHostInfo Info);
    void ProcessTCPQueue();

  public slots:
    void ThreadFinished ();

  protected slots:
    void newTcpConnection(qintptr socket) override;
    void EnableDisable  (bool Enable);
    void NewPaths       (const QStringList& Paths);
    void StalePaths     (const QStringList& Paths);
    void NewHandlers    (const HTTPHandlers& Handlers);
    void StaleHandlers  (const HTTPHandlers& Handlers);
    void NewServices    (const HTTPServices& Services);
    void StaleServices  (const HTTPServices& Services);
    void ResolveMaster  (QHostInfo Info);
    void ResolveHost    (QHostInfo Info);
    void NewErrorPageHandler (const HTTPHandler& Handler);
    void ProcessTCPQueueHandler();

  protected:
    MythHTTPServer();
   ~MythHTTPServer() override;

  private:
    Q_DISABLE_COPY(MythHTTPServer)
    void Init();
    void Started(bool Tcp, bool Ssl);
    void Stopped();
    void BuildHosts();
    void BuildOrigins();
    void DebugHosts();
    void DebugOrigins();
    static bool ReservedPath(const QString& Path);
    static QStringList BuildAddressList(QHostInfo& Info);

#if CONFIG_LIBDNS_SD
    class BonjourRegister* m_bonjour    { nullptr };
    BonjourRegister*       m_bonjourSSL { nullptr };
#endif
    int               m_originLookups { 0 };
    int               m_hostLookups   { 0 };
    MythHTTPConfig    m_config;
    int               m_masterStatusPort { 0 };
    int               m_masterSSLPort    { 0 };
    QString           m_masterIPAddress;
    QQueue<MythTcpQueueEntry>
                      m_connectionQueue;
    int               m_threadNum { 0 };
};

#endif
