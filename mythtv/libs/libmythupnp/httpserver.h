//////////////////////////////////////////////////////////////////////////////
// Program Name: httpserver.h
// Created     : Oct. 1, 2005
//
// Purpose     : HTTP 1.1 Mini Server Implmenetation
//               Used for UPnp/AV implementation & status information
//                                                                            
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details                    
//
//////////////////////////////////////////////////////////////////////////////

#ifndef __HTTPSERVER_H__
#define __HTTPSERVER_H__

// POSIX headers
#include <sys/types.h>
#ifndef _WIN32
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

// Qt headers
#include <QReadWriteLock>
#include <QMultiMap>
#include <QRunnable>
#include <QPointer>
#include <QMutex>
#include <QList>

#include <QSslConfiguration>
#include <QSslError>
#include <QSslSocket>
#include <QSslKey>

// MythTV headers
#include "mythqtcompat.h"
#include "serverpool.h"
#include "httprequest.h"
#include "mthreadpool.h"
#include "upnputil.h"
#include "compat.h"

typedef struct timeval  TaskTime;

class HttpWorkerThread;
class QScriptEngine;
class HttpServer;
class QSslKey;
class QSslCertificate;
class QSslConfiguration;

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
        int         m_nSocketTimeout; // Extension may wish to adjust the default e.g. UPnP
        
    public:

        HttpServerExtension( const QString &sName, const  QString &sSharePath )
           : m_sName( sName ), m_sSharePath( sSharePath ), m_nSocketTimeout(-1) {};

        virtual ~HttpServerExtension() {};

        virtual bool ProcessRequest(HTTPRequest *pRequest) = 0;

        virtual QStringList GetBasePaths() = 0;

        virtual int GetSocketTimeout() const { return m_nSocketTimeout; }// -1 = Use config value
};

typedef QList<QPointer<HttpServerExtension> > HttpServerExtensionList;

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

  protected:
    mutable QReadWriteLock  m_rwlock;
    HttpServerExtensionList m_extensions;
    // This multimap does NOT take ownership of the HttpServerExtension*
    QMultiMap< QString, HttpServerExtension* >  m_basePaths;
    QString                 m_sSharePath;
    HttpServerExtension    *m_pHtmlServer;
    MThreadPool             m_threadPool;
    bool                    m_running; // protected by m_rwlock

    static QMutex           s_platformLock;
    static QString          s_platform;

    QSslConfiguration       m_sslConfig;

  public:
    HttpServer(const QString &sApplicationPrefix = QString(""));
    virtual ~HttpServer();

    void RegisterExtension(HttpServerExtension*);
    void UnregisterExtension(HttpServerExtension*);
    void DelegateRequest(HTTPRequest*);
    /**
     * \brief Get the idle socket timeout value for the relevant extension
     */
    uint GetSocketTimeout(HTTPRequest*) const;

    QScriptEngine *ScriptEngine(void);

  private:
     void LoadSSLConfig();

  protected slots:
    virtual void newTcpConnection(qt_socket_fd_t socket); // QTcpServer

  public:

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
     * \param socketTimeout The time in seconds to wait after the connection goes idle before closing the socket
     * \param type       The type of connection - Plain TCP, SSL or other?
     * \param sslConfig  The SSL configuration (for SSL sockets)
     */
    HttpWorker(HttpServer &httpServer, qt_socket_fd_t sock, PoolServerType type,
               QSslConfiguration sslConfig);

    virtual void run(void);

  protected:
    HttpServer &m_httpServer; 
    qt_socket_fd_t m_socket;
    int         m_socketTimeout;
    PoolServerType m_connectionType;

    QSslConfiguration       m_sslConfig;
};


#endif
