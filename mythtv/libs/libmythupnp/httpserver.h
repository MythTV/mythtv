//////////////////////////////////////////////////////////////////////////////
// Program Name: httpserver.h
//                                                                            
// Purpose - HTTP 1.1 Mini Server Implmenetation
//           Used for UPnp/AV implementation & status information
//                                                                            
// Created By  : David Blain                    Created On : Oct. 1, 2005
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#ifndef __HTTPSERVER_H__
#define __HTTPSERVER_H__

#include <qthread.h>
#include <qserversocket.h>
#include <qsocketdevice.h>
#include <qsocket.h>
#include <qdom.h>
#include <qdatetime.h> 
#include <qtimer.h>
#include <qptrlist.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "upnpglobal.h"
#include "httprequest.h"
#include "threadpool.h"
#include "refcounted.h"

typedef struct timeval  TaskTime;

class HttpWorkerThread;
class HttpServer;

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// HttpServerExtension Class Definition
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class HttpServerExtension
{
    public:

        QString     m_sName;

    public:

                 HttpServerExtension( const QString &sName ):m_sName( sName ){};
        virtual ~HttpServerExtension() {};

//        virtual bool  Initialize    ( HttpServer  *pServer  ) = 0;
        virtual bool  ProcessRequest( HttpWorkerThread *pThread, HTTPRequest *pRequest ) = 0;
//        virtual bool  Uninitialize  ( ) = 0;
};

typedef QPtrList< HttpServerExtension > HttpServerExtensionList;

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// HttpServer Class Definition
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class HttpServer : public QServerSocket,
                   public ThreadPool
{

    protected:

        HttpServerExtensionList m_extensions;

        virtual WorkerThread *CreateWorkerThread( ThreadPool *, const QString &sName );
        virtual void          newConnection     ( int socket );

    public:

                 HttpServer( int nPort );
        virtual ~HttpServer();

        void     RegisterExtension  ( HttpServerExtension *pExtension );
        void     UnregisterExtension( HttpServerExtension *pExtension );

        void     DelegateRequest    ( HttpWorkerThread *pThread, HTTPRequest *pRequest );

};

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// Base class for WorkerThread Specific data
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class HttpWorkerData 
{
    public:

                 HttpWorkerData() {};
        virtual ~HttpWorkerData() {};
};

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// HttpWorkerThread Class Definition
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class HttpWorkerThread : public WorkerThread
{
    protected:

        HttpServer      *m_pHttpServer; 
        int              m_nSocket;
        int              m_nSocketTimeout;

        HttpWorkerData  *m_pData;

    protected:

        virtual void  ProcessWork();

    public:

                 HttpWorkerThread( HttpServer *pParent, const QString &sName );
        virtual ~HttpWorkerThread();

        void            StartWork( int nSocket );

        void            SetWorkerData( HttpWorkerData *pData );
        HttpWorkerData *GetWorkerData( ) { return( m_pData ); }
};


#endif
