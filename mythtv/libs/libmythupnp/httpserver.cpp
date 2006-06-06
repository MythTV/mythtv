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

#include <qregexp.h>
#include <qstringlist.h>
#include <qtextstream.h>
#include <qdatetime.h>
#include <math.h>
#include <sys/time.h>

#include "httpserver.h"
#include "upnpglobal.h"

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// HttpServer Class Implementation
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

HttpServer::HttpServer( int nPort ) 
          : QServerSocket( nPort, 1), // 5),
            ThreadPool( "HTTP" )
{
    m_extensions.setAutoDelete( true );

    InitializeThreads();

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
        m_extensions.append( pExtension );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void HttpServer::UnregisterExtension( HttpServerExtension *pExtension )
{
    if (pExtension != NULL )
        m_extensions.remove( pExtension );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void HttpServer::DelegateRequest( HttpWorkerThread *pThread, HTTPRequest *pRequest )
{
    bool bProcessed = false;

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
    m_nSocketTimeout = gContext->GetNumSetting( "HTTPKeepAliveTimeoutSecs", 600 ) * 1000;

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
//cout << "HttpWorkerThread::StartWork:Begin" << endl;

    m_nSocket = nSocket;

    SignalWork();

//cout << "HttpWorkerThread::StartWork:End" << endl;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void  HttpWorkerThread::ProcessWork()
{
    VERBOSE( VB_UPNP, QString( "HttpWorkerThread::ProcessWork:Begin( %1 ) socket=%2" )
                                    .arg( QThread::currentThread() )
                                    .arg( m_nSocket ));

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

                        //  cout << "Request: (" << QThread::currentThread() << ") socket=" << m_nSocket << " " << pRequest->m_sRequest;

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
                }
                else
                {
                    VERBOSE( VB_IMPORTANT, QString( "HttpWorkerThread::ProcessWork - Error Creating BufferedSocketDeviceRequest" ));
                    pRequest->m_nResponseStatus = 501;
                    bKeepAlive = false;
                }

                // ----------------------------------------------------------
                // Always MUST send a response.
                // ----------------------------------------------------------

                pRequest->SendResponse();

                delete pRequest;
                pRequest = NULL;

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

    VERBOSE( VB_UPNP, QString( "HttpWorkerThread::ProcessWork:End( %1 )")
                                    .arg( QThread::currentThread() ));
}


