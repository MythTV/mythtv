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
#include <QReadWriteLock>
#include <QTcpServer>
#include <QMultiMap>
#include <QRunnable>
#include <QPointer>
#include <QMutex>
#include <QList>

// MythTV headers
#include "httprequest.h"
#include "mthreadpool.h"
#include "refcounted.h"
#include "upnputil.h"
#include "compat.h"

typedef struct timeval  TaskTime;

class HttpWorkerThread;
class QScriptEngine;
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

        virtual bool ProcessRequest(HTTPRequest *pRequest) = 0;

        virtual QStringList GetBasePaths() = 0;
};

typedef QList<QPointer<HttpServerExtension> > HttpServerExtensionList;

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// HttpServer Class Definition
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC HttpServer : public QTcpServer
{
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

  public:
    HttpServer(const QString sApplicationPrefix = QString(""));
    virtual ~HttpServer();

    void RegisterExtension(HttpServerExtension*);
    void UnregisterExtension(HttpServerExtension*);
    void DelegateRequest(HTTPRequest*);

    QScriptEngine *ScriptEngine(void);

    virtual void incomingConnection(int socket); // QTcpServer

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
    HttpWorker(HttpServer &httpServer, int sock);

    virtual void run(void);

  protected:
    HttpServer &m_httpServer; 
    int         m_socket;
    int         m_socketTimeout;
};


#endif
