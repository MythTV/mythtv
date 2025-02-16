//////////////////////////////////////////////////////////////////////////////
// Program Name: httpserver.h
// Created     : Oct. 1, 2005
//
// Purpose     : HTTP 1.1 Mini Server Implmenetation
//               Used for UPnp/AV implementation & status information
//                                                                            
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef HTTPSERVER_H
#define HTTPSERVER_H

// POSIX headers
#include <sys/types.h>
#ifndef _WIN32
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#include <utility>

// Qt headers
#include <QReadWriteLock>
#include <QMultiMap>
#include <QRunnable>
#include <QPointer>
#include <QMutex>
#include <QList>
#include <QSslConfiguration>
#include <QSslError>
#include <QSslKey>
#include <QSslSocket>

// MythTV headers
#include "libmythbase/compat.h"
#include "libmythbase/mthreadpool.h"
#include "libmythbase/serverpool.h"

#include "httprequest.h"

class HttpWorkerThread;
class HttpServer;
#ifndef QT_NO_OPENSSL
class QSslKey;
class QSslCertificate;
class QSslConfiguration;
#endif

enum ContentProtection : std::uint8_t
{
    cpLocalNoAuth = 0x00,  // Can only be accessed locally, but no authentication is required
    cpLocalAuth   = 0x01,  // Can only be accessed locally, authentication is required
    cpRemoteAuth  = 0x02   // Can be accessed remotely, authentication is required
};

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// HttpServerExtension Class Definition
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC HttpServerExtension : public QObject
{
    Q_OBJECT

    public:

        QString     m_sName;
        QString     m_sSharePath;
        int         m_nSocketTimeout { -1 }; // Extension may wish to adjust the default e.g. UPnP

        // HTTP::RequestType. Defaults, extensions may extend the list
        uint        m_nSupportedMethods
	  {RequestTypeGet | RequestTypePost |
	   RequestTypeHead | RequestTypeOptions};
        
    public:

        HttpServerExtension( QString sName, QString sSharePath)
           : m_sName(std::move( sName )), m_sSharePath(std::move( sSharePath )) {};

        ~HttpServerExtension() override = default;

        virtual bool ProcessRequest(HTTPRequest *pRequest) = 0;

        virtual bool ProcessOptions(HTTPRequest *pRequest);

        virtual QStringList GetBasePaths() = 0;

        virtual int GetSocketTimeout() const { return m_nSocketTimeout; }// -1 = Use config value
};

using HttpServerExtensionList = QList<QPointer<HttpServerExtension> >;

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// HttpServer Class Definition
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC HttpServer : public ServerPool
{
    Q_OBJECT

  public:
    HttpServer();
    ~HttpServer() override;

    void RegisterExtension(HttpServerExtension *pExtension);
    void UnregisterExtension(HttpServerExtension *pExtension);
    void DelegateRequest(HTTPRequest *pRequest);
    /**
     * \brief Get the idle socket timeout value for the relevant extension
     */
    uint GetSocketTimeout(HTTPRequest *pRequest) const;

    QString GetSharePath(void) const
    { // never modified after creation, so no need to lock
        return m_sSharePath;
    }

    bool IsRunning(void) const
    {
        m_rwlock.lockForRead();
        bool tmp = m_running;
        m_rwlock.unlock();
        return tmp;
    }

    static QString GetPlatform(void);
    static QString GetServerVersion(void);

  protected:
    mutable QReadWriteLock  m_rwlock;
    HttpServerExtensionList m_extensions;
    // This multimap does NOT take ownership of the HttpServerExtension*
    QMultiMap< QString, HttpServerExtension* >  m_basePaths;
    QString                 m_sSharePath;
    MThreadPool             m_threadPool;
    bool                    m_running    { true }; // protected by m_rwlock

    static QMutex           s_platformLock;
    static QString          s_platform;

#ifndef QT_NO_OPENSSL
    QSslConfiguration       m_sslConfig;
#endif

    const QString m_privateToken; // Private token; Used to salt digest auth nonce, changes on backend restart

  protected slots:
    void newTcpConnection(qintptr socket) override; // QTcpServer

  private:
    void LoadSSLConfig();
};

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// HttpWorkerThread Class Definition
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class HttpWorker : public QRunnable
{
  public:

    /**
     * \param httpServer The parent server of this request
     * \param sock       The socket
     * \param type       The type of connection - Plain TCP, SSL or other?
     * \param sslConfig  The SSL configuration (for SSL sockets)
     */
    HttpWorker(HttpServer &httpServer, qintptr sock, PoolServerType type
#ifndef QT_NO_OPENSSL
               , const QSslConfiguration& sslConfig
#endif
    );

    void run(void) override; // QRunnable

  protected:
    HttpServer &m_httpServer; 
    qintptr m_socket;
    std::chrono::milliseconds m_socketTimeout;
    PoolServerType m_connectionType;

#ifndef QT_NO_OPENSSL
    QSslConfiguration       m_sslConfig;
#endif
};


#endif // HTTPSERVER_H
