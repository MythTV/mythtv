//////////////////////////////////////////////////////////////////////////////
// Program Name: upnp.h
//                                                                            
// Purpose - uPnp main Class
//                                                                            
// Created By  : David Blain                    Created On : Oct. 24, 2005
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#ifndef __UPNP_H__
#define __UPNP_H__

#include <qobject.h>
#include <qmutex.h>

#include "mythcontext.h"

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

#include "upnpdevice.h"
#include "taskqueue.h"
#include "httpserver.h"
#include "ssdp.h"
#include "upnpcds.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

class UPnp : public QObject
{
    Q_OBJECT

    protected:
        
        UPnpCDS                *m_pUPnpCDS;      // Do not delete (auto deleted)
        HttpServer             *m_pHttpServer;

    public:

        static QString          g_sPlatform;

        static UPnpDeviceDesc   g_UPnpDeviceDesc;
        static TaskQueue       *g_pTaskQueue;
        static SSDP            *g_pSSDP;

    public:
                 UPnp( bool bMaster, HttpServer *pHttpServer );
        virtual ~UPnp();

        void     CleanUp( void );

        void     customEvent( QCustomEvent *e );

        void     RegisterExtension  ( UPnpCDSExtension    *pExtension );
        void     UnregisterExtension( UPnpCDSExtension    *pExtension );

};

#endif
