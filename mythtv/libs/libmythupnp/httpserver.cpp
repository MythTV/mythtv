//////////////////////////////////////////////////////////////////////////////
// Program Name: httpserver.cpp
// Created     : Oct. 1, 2005
//
// Purpose     : HTTP 1.1 Mini Server Implmenetation
//               Used for UPnp/AV implementation & status information
//                                                                            
// Copyright (c) 2005 David Blain <mythtv@theblains.net>
//                                          
// This library is free software; you can redistribute it and/or 
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or at your option any later version of the LGPL.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

// ANSI C headers
#include <cmath>

// POSIX headers
#include <compat.h>
#ifndef USING_MINGW
#include <sys/utsname.h> 
#endif

// MythTV headers
#include "httpserver.h"
#include "upnputil.h"
#include "upnp.h" // only needed for Config... remove once config is moved.
#include "compat.h"
#include "mythdirs.h"
#include "mythverbose.h"
#include "htmlserver.h"

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// HttpServer Class Implementation
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

QString  HttpServer::g_sPlatform;

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

HttpServer::HttpServer() : QTcpServer(), ThreadPool("HTTP")
{
    setMaxPendingConnections(20);
    InitializeThreads();

    // ----------------------------------------------------------------------
    // Build Platform String
    // ----------------------------------------------------------------------

#ifdef USING_MINGW
    g_sPlatform = QString( "Windows %1.%1" )
        .arg(LOBYTE(LOWORD(GetVersion())))
        .arg(HIBYTE(LOWORD(GetVersion())));
#else
    struct utsname uname_info;

    uname( &uname_info );

    g_sPlatform = QString( "%1 %2" ).arg( uname_info.sysname )
                                    .arg( uname_info.release );
#endif

    // ----------------------------------------------------------------------
    // Initialize Share Path
    // ----------------------------------------------------------------------

    m_sSharePath = GetShareDir();
    VERBOSE(VB_UPNP, QString( "HttpServer() - SharePath = %1")
            .arg(m_sSharePath));

    // ----------------------------------------------------------------------
    // The HtmlServer Extension is our fall back if a request isn't processed
    // by any other extension.  (This is needed here since it listens for
    // '/' as it's base url ).
    // ----------------------------------------------------------------------

    m_pHtmlServer = new HtmlServerExtension( m_sSharePath );


    // -=>TODO: Load Config XML
    // -=>TODO: Load & initialize - HttpServerExtensions
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

HttpServer::~HttpServer()
{
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

QScriptEngine* HttpServer::ScriptEngine()
{
    return ((HtmlServerExtension *)m_pHtmlServer)->ScriptEngine();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

WorkerThread *HttpServer::CreateWorkerThread( ThreadPool * /*pThreadPool */, 
                                              const QString &sName )
{
    return( new HttpWorkerThread( this, sName ));
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void HttpServer::incomingConnection(int nSocket)
{
    HttpWorkerThread *pThread = (HttpWorkerThread *)GetWorkerThread();

    if (pThread != NULL)
        pThread->StartWork( nSocket );
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

void HttpServer::DelegateRequest( HttpWorkerThread *pThread, HTTPRequest *pRequest )
{
    bool bProcessed = false;

    m_rwlock.lockForRead();

    QList< HttpServerExtension* > list = m_basePaths.values( pRequest->m_sBaseUrl );

    for (int nIdx=0; nIdx < list.size() && !bProcessed; nIdx++ )
    {
        try
        {
            bProcessed = list[ nIdx ]->ProcessRequest(pThread, pRequest);
        }
        catch(...)
        {
            VERBOSE(VB_IMPORTANT,
                    QString("HttpServer::DelegateRequest - "
                            "Unexpected Exception - "
                            "pExtension->ProcessRequest()."));
        }
    }
/*
    HttpServerExtensionList::iterator it = m_extensions.begin();

    for (; (it != m_extensions.end()) && !bProcessed; ++it)
    {
        try
        {
            bProcessed = (*it)->ProcessRequest(pThread, pRequest);
        }
        catch(...)
        {
            VERBOSE(VB_IMPORTANT,
                    QString("HttpServer::DelegateRequest - "
                            "Unexpected Exception - "
                            "pExtension->ProcessRequest()."));
        }
    }
*/
    m_rwlock.unlock();

    if (!bProcessed)
        bProcessed = m_pHtmlServer->ProcessRequest( pThread, pRequest );

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

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

HttpWorkerThread::HttpWorkerThread( HttpServer *pParent, const QString &sName ) :
                  WorkerThread( (ThreadPool *)pParent, sName )
{
    m_pHttpServer    = pParent;
    m_nSocket        = 0;                                                  
    m_nSocketTimeout = UPnp::GetConfiguration()->GetValue( "HTTP/KeepAliveTimeoutSecs", 10 ) * 1000;

    m_pData          = NULL;
}                  

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

HttpWorkerThread::~HttpWorkerThread()
{
    if (m_pData != NULL)
        delete m_pData;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void HttpWorkerThread::SetWorkerData( HttpWorkerData *pData )
{
    // WorkerThread takes ownership of pData pointer.
    //  (Must be allocated on heap)

    if (m_pData != NULL)
        delete m_pData;

    m_pData = pData;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void HttpWorkerThread::StartWork( int nSocket )
{
    m_nSocket = nSocket;

    SignalWork();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void  HttpWorkerThread::ProcessWork()
{
//    VERBOSE( VB_UPNP, QString( "HttpWorkerThread::ProcessWork:Begin( %1 ) socket=%2" )
//                                    .arg( (long)QThread::currentThread() )
//                                    .arg( m_nSocket ));

    bool                    bTimeout   = false;
    bool                    bKeepAlive = true;
    BufferedSocketDevice   *pSocket    = NULL;
    HTTPRequest            *pRequest   = NULL;

    try
    {
        if ((pSocket = new BufferedSocketDevice( m_nSocket )) == NULL)
        {
            VERBOSE( VB_IMPORTANT, QString( "HttpWorkerThread::ProcessWork - Error Creating BufferedSocketDevice" ));
            return;
        }

        pSocket->SocketDevice()->setBlocking( true );

        while( !m_bTermRequested && bKeepAlive && pSocket->IsValid())
        {
            bTimeout = 0;

            int64_t nBytes = pSocket->WaitForMore(m_nSocketTimeout, &bTimeout);

            if ( nBytes > 0)
            {
                // ----------------------------------------------------------
                // See if this is a valid request
                // ----------------------------------------------------------

                if ((pRequest = new BufferedSocketDeviceRequest( pSocket )) != NULL)
                {
                    if ( pRequest->ParseRequest() )
                    {
                        bKeepAlive = pRequest->GetKeepAlive();

                        // ------------------------------------------------------
                        // Request Parsed... Pass on to Main HttpServer class to 
                        // delegate processing to HttpServerExtensions.
                        // ------------------------------------------------------

                        if (pRequest->m_nResponseStatus != 401)
                            m_pHttpServer->DelegateRequest( this, pRequest );
                    }
                    else
                    {
                        VERBOSE( VB_UPNP, QString( "HttpWorkerThread::ProcessWork - ParseRequest Failed." ));

                        pRequest->m_nResponseStatus = 501;
                        bKeepAlive = false;
                    }

                    /*
                    // Dump Request Header 
                    if (!bKeepAlive )
                    {
                        for ( QStringMap::iterator it  = pRequest->m_mapHeaders.begin(); 
                                                   it != pRequest->m_mapHeaders.end(); 
                                                 ++it ) 
                        {  
                            cout << it.key() << ": " << it.data() << endl;
                        }
                    }
                    */

                    // ----------------------------------------------------------
                    // Always MUST send a response.
                    // ----------------------------------------------------------

                    if (pRequest->SendResponse() < 0)
                    {
                        bKeepAlive = false;
                        VERBOSE( VB_UPNP, QString( "HttpWorkerThread::ProcessWork socket(%1) - Error returned from SendResponse... Closing connection" )
                                             .arg( m_nSocket ));
                    }

                    // ----------------------------------------------------------
                    // Check to see if a PostProcess was registered
                    // ----------------------------------------------------------

                    if ( pRequest->m_pPostProcess != NULL )
                        pRequest->m_pPostProcess->ExecutePostProcess();

                    delete pRequest;
                    pRequest = NULL;


                }
                else
                {
                    VERBOSE( VB_IMPORTANT, QString( "HttpWorkerThread::ProcessWork - Error Creating BufferedSocketDeviceRequest" ));
                    bKeepAlive = false;
                }
            }
            else
            {
                bKeepAlive = false;
            }
        }
    }
    catch( ... )
    {
        VERBOSE( VB_IMPORTANT, QString( "HttpWorkerThread::ProcessWork - Unexpected Exception." ));
    }

    if (pRequest != NULL)
        delete pRequest;

    pSocket->Close();

    delete pSocket;
    m_nSocket = 0;

//    VERBOSE( VB_UPNP, QString( "HttpWorkerThread::ProcessWork:End( %1 )")
//                                    .arg( (long)QThread::currentThread() ));
}


