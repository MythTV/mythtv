//////////////////////////////////////////////////////////////////////////////
// Program Name: httpserver.h
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

#ifndef __HTTPSERVER_H__
#define __HTTPSERVER_H__

// POSIX headers
#include <sys/types.h>
#ifndef USING_MINGW
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

// Qt headers
#include <QThread>
#include <QTcpServer>
#include <QReadWriteLock>

// MythTV headers
#include "upnputil.h"
#include "httprequest.h"
#include "threadpool.h"
#include "refcounted.h"
#include "compat.h"

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

class UPNP_PUBLIC HttpServerExtension : public QObject
{
    Q_OBJECT

    public:

        QString     m_sName;
        QString     m_sSharePath;

    public:

        HttpServerExtension( const QString &sName, const  QString &sSharePath )
           :m_sName( sName ), m_sSharePath( sSharePath ) {};

        virtual ~HttpServerExtension() {};

//        virtual bool  Initialize    ( HttpServer  *pServer  ) = 0;
        virtual bool  ProcessRequest( HttpWorkerThread *pThread,
                                      HTTPRequest      *pRequest ) = 0;
//        virtual bool  Uninitialize  ( ) = 0;
};

typedef QList<QPointer<HttpServerExtension> > HttpServerExtensionList;

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// HttpServer Class Definition
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC HttpServer : public QTcpServer,
                               public ThreadPool
{

    protected:

        QReadWriteLock          m_rwlock;
        HttpServerExtensionList m_extensions;

        virtual WorkerThread *CreateWorkerThread( ThreadPool *,
                                                  const QString &sName );
        virtual void          incomingConnection     ( int socket );

    public:

        static QString      g_sPlatform;
               QString      m_sSharePath;

    public:

                 HttpServer();
        virtual ~HttpServer();

        void     RegisterExtension  ( HttpServerExtension *pExtension );
        void     UnregisterExtension( HttpServerExtension *pExtension );

        void     DelegateRequest    ( HttpWorkerThread *pThread,
                                      HTTPRequest      *pRequest );

};

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// Base class for WorkerThread Specific data
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC HttpWorkerData 
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

class UPNP_PUBLIC HttpWorkerThread : public WorkerThread
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
