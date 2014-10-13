//////////////////////////////////////////////////////////////////////////////
// Program Name: httpserver.cpp
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

// ANSI C headers
#include <cmath>

// POSIX headers
#include <compat.h>
#ifndef _WIN32
#include <sys/utsname.h> 
#endif

// Qt headers
#include <QScriptEngine>

// MythTV headers
#include "httpserver.h"
#include "upnputil.h"
#include "upnp.h" // only needed for Config... remove once config is moved.
#include "compat.h"
#include "mythdirs.h"
#include "mythlogging.h"
#include "htmlserver.h"
#include "mythversion.h"
#include <mythcorecontext.h>

#include "serviceHosts/rttiServiceHost.h"

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// HttpServer Class Implementation
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

QMutex   HttpServer::s_platformLock;
QString  HttpServer::s_platform;

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

HttpServer::HttpServer(const QString &sApplicationPrefix) :
    ServerPool(), m_sSharePath(GetShareDir()),
    m_pHtmlServer(new HtmlServerExtension(m_sSharePath, sApplicationPrefix)),
    m_threadPool("HttpServerPool"), m_running(true)
{
    // Number of connections processed concurrently
    int maxHttpWorkers = max(QThread::idealThreadCount() * 2, 2); // idealThreadCount can return -1
    // Don't allow more connections than we can process, it causes browsers
    // to open lots of new connections instead of reusing existing ones
    setMaxPendingConnections(maxHttpWorkers);
    m_threadPool.setMaxThreadCount(maxHttpWorkers);

    LOG(VB_HTTP, LOG_NOTICE, QString("HttpServer(): Max Thread Count %1")
                                .arg(m_threadPool.maxThreadCount()));

    // ----------------------------------------------------------------------
    // Build Platform String
    // ----------------------------------------------------------------------
    {
        QMutexLocker locker(&s_platformLock);
#ifdef _WIN32
        s_platform = QString("Windows/%1.%2")
            .arg(LOBYTE(LOWORD(GetVersion())))
            .arg(HIBYTE(LOWORD(GetVersion())));
#else
        struct utsname uname_info;
        uname( &uname_info );
        s_platform = QString("%1/%2")
            .arg(uname_info.sysname).arg(uname_info.release);
#endif
    }

    LOG(VB_HTTP, LOG_INFO, QString("HttpServer() - SharePath = %1")
            .arg(m_sSharePath));

    // -=>TODO: Load Config XML
    // -=>TODO: Load & initialize - HttpServerExtensions

    // ----------------------------------------------------------------------
    // Enable Rtti Service for all instances of HttpServer
    // and register with QtScript Engine.
    // Rtti service is an alternative to using the xsd uri
    // it returns xml/json/etc... definitions of types/enums 
    // ----------------------------------------------------------------------

    RegisterExtension( new RttiServiceHost( m_sSharePath ));

    QScriptEngine *pEngine = ScriptEngine();

    pEngine->globalObject().setProperty("Rtti",
         pEngine->scriptValueFromQMetaObject< ScriptableRtti >() );

    LoadSSLConfig();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

HttpServer::~HttpServer()
{
    m_rwlock.lockForWrite();
    m_running = false;
    m_rwlock.unlock();

    m_threadPool.Stop();

    while (!m_extensions.empty())
    {
        delete m_extensions.takeFirst();
    }

    if (m_pHtmlServer != NULL)
        delete m_pHtmlServer;
}

void HttpServer::LoadSSLConfig()
{
    m_sslConfig = QSslConfiguration::defaultConfiguration();

    QString hostKeyPath = gCoreContext->GetSetting("hostSSLKey", "");

    if (hostKeyPath.isEmpty()) // No key, assume no SSL
        return;

    QFile hostKeyFile(hostKeyPath);
    if (!hostKeyFile.exists() || !hostKeyFile.open(QIODevice::ReadOnly))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("HttpServer: SSL Host key file (%1) does not exist or is not readable").arg(hostKeyPath));
        return;
    }

    QByteArray rawHostKey = hostKeyFile.readAll();
    QSslKey hostKey = QSslKey(rawHostKey, QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey);
    if (!hostKey.isNull())
        m_sslConfig.setPrivateKey(hostKey);
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QString("HttpServer: Unable to load host key from file (%1)").arg(hostKeyPath));
        return;
    }

    QString hostCertPath = gCoreContext->GetSetting("hostSSLCertificate", "");
    QSslCertificate hostCert;
    QList<QSslCertificate> certList = QSslCertificate::fromPath(hostCertPath);
    if (!certList.isEmpty())
        hostCert = certList.first();

    if (hostCert.isValid())
        m_sslConfig.setLocalCertificate(hostCert);
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QString("HttpServer: Unable to load host cert from file (%1)").arg(hostCertPath));
        return;
    }

    QString caCertPath = gCoreContext->GetSetting("caSSLCertificate", "");
    QList< QSslCertificate > CACertList = QSslCertificate::fromPath(caCertPath);

    if (!CACertList.isEmpty())
        m_sslConfig.setCaCertificates(CACertList);
    else if (!caCertPath.isEmpty()) // Only warn if a path was actually configured, this isn't an error otherwise
        LOG(VB_GENERAL, LOG_ERR, QString("HttpServer: Unable to load CA cert file (%1)").arg(caCertPath));
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString HttpServer::GetPlatform(void)
{
    QMutexLocker locker(&s_platformLock);
    return s_platform;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString HttpServer::GetServerVersion(void)
{
    QString mythVersion = MYTH_SOURCE_VERSION;
    mythVersion = mythVersion.right(mythVersion.length() - 1); // Trim off the leading 'v'
    return QString("MythTV/%2 %1 UPnP/1.0").arg(HttpServer::GetPlatform())
                                             .arg(mythVersion);
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QScriptEngine* HttpServer::ScriptEngine()
{
    return ((HtmlServerExtension *)m_pHtmlServer)->ScriptEngine();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void HttpServer::newTcpConnection(qt_socket_fd_t socket)
{
    PoolServerType type = kTCPServer;
    PrivTcpServer *server = dynamic_cast<PrivTcpServer *>(QObject::sender());
    if (server)
        type = server->GetServerType();

    m_threadPool.startReserved(
        new HttpWorker(*this, socket, type,
                       m_sslConfig),
        QString("HttpServer%1").arg(socket));
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void HttpServer::RegisterExtension( HttpServerExtension *pExtension )
{
    if (pExtension != NULL )
    {
        m_rwlock.lockForWrite();
        m_extensions.append( pExtension );

        // Add to multimap for quick lookup.

        QStringList list = pExtension->GetBasePaths();

        for( int nIdx = 0; nIdx < list.size(); nIdx++)
            m_basePaths.insert( list[ nIdx ], pExtension );

        m_rwlock.unlock();
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void HttpServer::UnregisterExtension( HttpServerExtension *pExtension )
{
    if (pExtension != NULL )
    {
        m_rwlock.lockForWrite();

        QStringList list = pExtension->GetBasePaths();

        for( int nIdx = 0; nIdx < list.size(); nIdx++)
            m_basePaths.remove( list[ nIdx ], pExtension );

        m_extensions.removeAll(pExtension);

        delete pExtension;

        m_rwlock.unlock();
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void HttpServer::DelegateRequest(HTTPRequest *pRequest)
{
    bool bProcessed = false;

    LOG(VB_HTTP, LOG_DEBUG, QString("m_sBaseUrl: %1").arg( pRequest->m_sBaseUrl ));
    m_rwlock.lockForRead();

    QList< HttpServerExtension* > list = m_basePaths.values( pRequest->m_sBaseUrl );

    for (int nIdx=0; nIdx < list.size() && !bProcessed; nIdx++ )
    {
        try
        {
            bProcessed = list[ nIdx ]->ProcessRequest(pRequest);
        }
        catch(...)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("HttpServer::DelegateRequest - "
                                             "Unexpected Exception - "
                                             "pExtension->ProcessRequest()."));
        }
    }

#if 0
    HttpServerExtensionList::iterator it = m_extensions.begin();

    for (; (it != m_extensions.end()) && !bProcessed; ++it)
    {
        try
        {
            bProcessed = (*it)->ProcessRequest(pRequest);
        }
        catch(...)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("HttpServer::DelegateRequest - "
                                             "Unexpected Exception - "
                                             "pExtension->ProcessRequest()."));
        }
    }
#endif
    m_rwlock.unlock();

    if (!bProcessed)
        bProcessed = m_pHtmlServer->ProcessRequest(pRequest);

    if (!bProcessed)
    {
        pRequest->m_eResponseType   = ResponseTypeHTML;
        pRequest->m_nResponseStatus = 404; 
    }
}

uint HttpServer::GetSocketTimeout(HTTPRequest* pRequest) const
{
    int timeout = -1;

    m_rwlock.lockForRead();
    QList< HttpServerExtension* > list = m_basePaths.values( pRequest->m_sBaseUrl );
    if (!list.isEmpty())
        timeout = list.first()->GetSocketTimeout();
    m_rwlock.unlock();

    if (timeout < 0)
        timeout = gCoreContext->GetNumSetting("HTTP/KeepAliveTimeoutSecs", 10);

    return timeout;
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// HttpWorkerThread Class Implementation
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

HttpWorker::HttpWorker(HttpServer &httpServer, qt_socket_fd_t sock,
                       PoolServerType type, QSslConfiguration sslConfig)
           : m_httpServer(httpServer), m_socket(sock),
             m_socketTimeout(5 * 1000), m_connectionType(type),
             m_sslConfig(sslConfig)
{
    LOG(VB_HTTP, LOG_INFO, QString("HttpWorker(%1): New connection")
                                        .arg(m_socket));
}                  

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void HttpWorker::run(void)
{
#if 0
    LOG(VB_HTTP, LOG_DEBUG,
        QString("HttpWorker::run() socket=%1 -- begin").arg(m_socket));
#endif

    bool                    bTimeout   = false;
    bool                    bKeepAlive = true;
    HTTPRequest            *pRequest   = NULL;
    QTcpSocket             *pSocket;

    if (m_connectionType == kSSLServer)
    {

#ifndef QT_NO_OPENSSL
        QSslSocket *pSslSocket = new QSslSocket();
        if (pSslSocket->setSocketDescriptor(m_socket))
        {
            pSslSocket->setSslConfiguration(m_sslConfig);
            pSslSocket->setPrivateKey(m_sslConfig.privateKey());
            pSslSocket->setLocalCertificate(m_sslConfig.localCertificate());
            pSslSocket->addCaCertificates(m_sslConfig.caCertificates());
            pSslSocket->startServerEncryption();
            if (pSslSocket->waitForEncrypted(5000))
            {
                LOG(VB_HTTP, LOG_INFO, "SSL Handshake occurred, connection encrypted");
            }
            else
            {
                LOG(VB_HTTP, LOG_DEBUG, "SSL Handshake FAILED, connection terminated");
                delete pSslSocket;
                pSslSocket = NULL;
            }
        }
        else
        {
            delete pSslSocket;
            pSslSocket = NULL;
        }

        if (pSslSocket)
            pSocket = dynamic_cast<QTcpSocket *>(pSslSocket);
        else
            return;
#else
        return;
#endif
    }
    else // Plain old unencrypted socket
    {
        pSocket = new QTcpSocket();
        pSocket->setSocketDescriptor(m_socket);
    }

    int nRequestsHandled = 0; // Allow debugging of keep-alive and connection re-use

    try
    {
        while (m_httpServer.IsRunning() && bKeepAlive && pSocket &&
               pSocket->isValid() &&
               pSocket->state() == QAbstractSocket::ConnectedState)
        {
            // We set a timeout on keep-alive connections to avoid blocking
            // new clients from connecting - Default at time of writing was
            // 5 seconds for initial connection, then up to 10 seconds of idle
            // time between each subsequent request on the same connection
            bTimeout = !(pSocket->waitForReadyRead(m_socketTimeout));

            if (bTimeout) // Either client closed the socket or we timed out waiting for new data
                break;

            int64_t nBytes = pSocket->bytesAvailable();
            if (!m_httpServer.IsRunning())
                break;

            if ( nBytes > 0)
            {
                // ----------------------------------------------------------
                // See if this is a valid request
                // ----------------------------------------------------------

                pRequest = new BufferedSocketDeviceRequest( pSocket );
                if (pRequest != NULL)
                {
                    if ( pRequest->ParseRequest() )
                    {
                        bKeepAlive = pRequest->GetKeepAlive();
                        // The timeout is defined by the Server/Server Extension
                        // but must appear in the response headers
                        uint nTimeout = m_httpServer.GetSocketTimeout(pRequest); // Seconds
                        pRequest->SetKeepAliveTimeout(nTimeout);
                        m_socketTimeout = nTimeout * 1000; // Milliseconds

                        // ------------------------------------------------------
                        // Request Parsed... Pass on to Main HttpServer class to 
                        // delegate processing to HttpServerExtensions.
                        // ------------------------------------------------------
                        if ((pRequest->m_nResponseStatus != 400) &&
                            (pRequest->m_nResponseStatus != 401))
                            m_httpServer.DelegateRequest(pRequest);

                        nRequestsHandled++;
                    }
                    else
                    {
                        LOG(VB_HTTP, LOG_ERR, "ParseRequest Failed.");

                        pRequest->m_nResponseStatus = 501;
                        bKeepAlive = false;
                    }

                    // -------------------------------------------------------
                    // Always MUST send a response.
                    // -------------------------------------------------------
                    if (pRequest->SendResponse() < 0)
                    {
                        bKeepAlive = false;
                        LOG(VB_HTTP, LOG_ERR,
                            QString("socket(%1) - Error returned from "
                                    "SendResponse... Closing connection")
                                .arg(pSocket->socketDescriptor()));
                    }

                    // -------------------------------------------------------
                    // Check to see if a PostProcess was registered
                    // -------------------------------------------------------
                    if ( pRequest->m_pPostProcess != NULL )
                        pRequest->m_pPostProcess->ExecutePostProcess();

                    delete pRequest;
                    pRequest = NULL;
                }
                else
                {
                    LOG(VB_GENERAL, LOG_ERR,
                        "Error Creating BufferedSocketDeviceRequest");
                    bKeepAlive = false;
                }
            }
            else
            {
                bKeepAlive = false;
            }
        }
    }
    catch(...)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "HttpWorkerThread::ProcessWork - Unexpected Exception.");
    }

    delete pRequest;

    if ((pSocket->error() != QAbstractSocket::UnknownSocketError) &&
        (bKeepAlive && pSocket->error() == QAbstractSocket::SocketTimeoutError)) // This 'error' isn't an error when keep-alive is active
    {
        LOG(VB_HTTP, LOG_WARNING, QString("HttpWorker(%1): Error %2 (%3)")
                                   .arg(m_socket)
                                   .arg(pSocket->errorString())
                                   .arg(pSocket->error()));
    }

    int writeTimeout = 5000; // 5 Seconds
    // Make sure any data in the buffer is flushed before the socket is closed
    while (m_httpServer.IsRunning() &&
           pSocket->isValid() &&
           pSocket->state() == QAbstractSocket::ConnectedState &&
           pSocket->bytesToWrite() > 0)
    {
        LOG(VB_HTTP, LOG_DEBUG, QString("HttpWorker(%1): "
                                        "Waiting for %2 bytes to be written "
                                        "before closing the connection.")
                                            .arg(m_socket)
                                            .arg(pSocket->bytesToWrite()));

        // If the client stops reading for longer than 'writeTimeout' then
        // stop waiting for them. We can't afford to leave the socket
        // connected indefinately, it could be used by another client.
        //
        // NOTE: Some clients deliberately stall as a way of 'pausing' A/V
        // streaming. We should create a new server extension or adjust the
        // timeout according to the User-Agent, instead of increasing the
        // standard timeout. However we should ALWAYS have a timeout.
        if (!pSocket->waitForBytesWritten(writeTimeout))
        {
            LOG(VB_GENERAL, LOG_WARNING, QString("HttpWorker(%1): "
                                         "Timed out waiting to write bytes to "
                                         "the socket, waited %2 seconds")
                                            .arg(m_socket)
                                            .arg(writeTimeout / 1000));
            break;
        }
    }

    if (pSocket->bytesToWrite() > 0)
    {
        LOG(VB_HTTP, LOG_WARNING, QString("HttpWorker(%1): "
                                          "Failed to write %2 bytes to "
                                          "socket, (%3)")
                                            .arg(m_socket)
                                            .arg(pSocket->bytesToWrite())
                                            .arg(pSocket->errorString()));
    }

    LOG(VB_HTTP, LOG_INFO, QString("HttpWorker(%1): Connection %2 closed, requests handled %3")
                                        .arg(m_socket)
                                        .arg(pSocket->socketDescriptor())
                                        .arg(nRequestsHandled));

    pSocket->close();
    delete pSocket;
    pSocket = NULL;

#if 0
    LOG(VB_HTTP, LOG_DEBUG, "HttpWorkerThread::run() -- end");
#endif
}


