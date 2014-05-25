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
    setMaxPendingConnections(20);

    // ----------------------------------------------------------------------
    // Build Platform String
    // ----------------------------------------------------------------------
    {
        QMutexLocker locker(&s_platformLock);
#ifdef _WIN32
        s_platform = QString("Windows %1.%2")
            .arg(LOBYTE(LOWORD(GetVersion())))
            .arg(HIBYTE(LOWORD(GetVersion())));
#else
        struct utsname uname_info;
        uname( &uname_info );
        s_platform = QString("%1 %2")
            .arg(uname_info.sysname).arg(uname_info.release);
#endif
    }

    LOG(VB_UPNP, LOG_INFO, QString("HttpServer() - SharePath = %1")
            .arg(m_sSharePath));

    // -=>TODO: Load Config XML
    // -=>TODO: Load & initialize - HttpServerExtensions
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
    return QString("%1, UPnP/1.0, MythTV %2").arg(HttpServer::GetPlatform())
                                             .arg(MYTH_SOURCE_VERSION);
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

void HttpServer::newTcpConnection(qt_socket_fd_t nSocket)
{
    m_threadPool.startReserved(
        new HttpWorker(*this, nSocket),
        QString("HttpServer%1").arg(nSocket));
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

    LOG(VB_UPNP, LOG_DEBUG, QString("m_sBaseUrl: %1").arg( pRequest->m_sBaseUrl ));
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

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// HttpWorkerThread Class Implementation
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

HttpWorker::HttpWorker(HttpServer &httpServer, qt_socket_fd_t sock) :
    m_httpServer(httpServer), m_socket(sock), m_socketTimeout(10000)
{
    m_socketTimeout = 1000 *
        UPnp::GetConfiguration()->GetValue("HTTP/KeepAliveTimeoutSecs", 10);
}                  

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void HttpWorker::run(void)
{
#if 0
    LOG(VB_UPNP, LOG_DEBUG,
        QString("HttpWorker::run() socket=%1 -- begin").arg(m_socket));
#endif

    bool                    bTimeout   = false;
    bool                    bKeepAlive = true;
    BufferedSocketDevice   *pSocket    = NULL;
    HTTPRequest            *pRequest   = NULL;

    try
    {
        if ((pSocket = new BufferedSocketDevice( m_socket )) == NULL)
        {
            LOG(VB_GENERAL, LOG_ERR, "Error Creating BufferedSocketDevice");
            return;
        }

        pSocket->SocketDevice()->setBlocking( true );

        while (m_httpServer.IsRunning() && bKeepAlive && pSocket->IsValid())
        {
            bTimeout = false;

            int64_t nBytes = pSocket->WaitForMore(m_socketTimeout, &bTimeout);
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

                        // ------------------------------------------------------
                        // Request Parsed... Pass on to Main HttpServer class to 
                        // delegate processing to HttpServerExtensions.
                        // ------------------------------------------------------

                        if ((pRequest->m_nResponseStatus != 400) &&
                            (pRequest->m_nResponseStatus != 401))
                            m_httpServer.DelegateRequest(pRequest);
                    }
                    else
                    {
                        LOG(VB_UPNP, LOG_ERR, "ParseRequest Failed.");

                        pRequest->m_nResponseStatus = 501;
                        bKeepAlive = false;
                    }

#if 0
                    // Dump Request Header 
                    if (!bKeepAlive )
                    {
                        for ( QStringMap::iterator it  = pRequest->m_mapHeaders.begin(); 
                                                   it != pRequest->m_mapHeaders.end(); 
                                                 ++it ) 
                        {  
                            LOG(VB_GENERAL, LOG_DEBUG, QString("%1: %2") 
                                .arg(it.key()) .arg(it.data()));
                        }
                    }
#endif

                    // -------------------------------------------------------
                    // Always MUST send a response.
                    // -------------------------------------------------------

                    if (pRequest->SendResponse() < 0)
                    {
                        bKeepAlive = false;
                        LOG(VB_UPNP, LOG_ERR,
                            QString("socket(%1) - Error returned from "
                                    "SendResponse... Closing connection")
                                .arg(m_socket));
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

    if (pRequest != NULL)
        delete pRequest;

    pSocket->Close();

    delete pSocket;
    m_socket = 0;

#if 0
    LOG(VB_UPNP, LOG_DEBUG, "HttpWorkerThread::run() -- end");
#endif
}


