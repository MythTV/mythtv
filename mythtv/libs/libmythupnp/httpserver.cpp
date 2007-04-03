//////////////////////////////////////////////////////////////////////////////
// Program Name: httpserver.cpp
//                                                                            
// Purpose - HTTP 1.1 Mini Server Implmenetation
//           Used for UPnp/AV implementation & status information
//                                                                            
// Created By  : David Blain                    Created On : Oct. 1, 2005
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#include <unistd.h>

#include <qregexp.h>
#include <qstringlist.h>
#include <qtextstream.h>
#include <qdatetime.h>
#include <math.h>
#include <sys/time.h>

#include <sys/utsname.h> 

#include "httpserver.h"
#include "upnputil.h"

#include "upnp.h"       // only needed for Config... remove once config is moved.

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

HttpServer::HttpServer( int nPort ) 
          : QServerSocket( nPort, 20 ), //5),
            ThreadPool( "HTTP" )
{
    m_extensions.setAutoDelete( true );

    InitializeThreads();

    // ----------------------------------------------------------------------
    // Build Platform String
    // ----------------------------------------------------------------------

    struct utsname uname_info;

    uname( &uname_info );

    g_sPlatform = QString( "%1 %2" ).arg( uname_info.sysname )
                                    .arg( uname_info.release );

    // -=>TODO: Load Config XML
    // -=>TODO: Load & initialize - HttpServerExtensions
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

HttpServer::~HttpServer()
{
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

void HttpServer::newConnection(int nSocket)
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
        m_mutex.lock();
        m_extensions.append( pExtension );
        m_mutex.unlock();
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void HttpServer::UnregisterExtension( HttpServerExtension *pExtension )
{
    if (pExtension != NULL )
    {
        m_mutex.lock();
        m_extensions.remove( pExtension );
        m_mutex.unlock();
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void HttpServer::DelegateRequest( HttpWorkerThread *pThread, HTTPRequest *pRequest )
{
    bool bProcessed = false;

    m_mutex.lock();

    HttpServerExtension *pExtension = m_extensions.first();

    while (( pExtension != NULL ) && !bProcessed )
    {
        try
        {
            bProcessed = pExtension->ProcessRequest( pThread, pRequest );
        }
        catch(...)
        {
            VERBOSE( VB_IMPORTANT, QString( "HttpServer::DelegateRequest - Unexpected Exception - pExtension->ProcessRequest()." ));
        }

        pExtension = m_extensions.next();
    }

    m_mutex.unlock();

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
    m_nSocketTimeout = UPnp::g_pConfig->GetValue( "HTTP/KeepAliveTimeoutSecs", 10 ) * 1000;

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

        while( !IsTermRequested() && bKeepAlive && pSocket->IsValid())
        {
            bTimeout = 0;

            Q_LONG nBytes = pSocket->WaitForMore( m_nSocketTimeout, &bTimeout );

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


