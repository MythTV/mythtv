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

#include "configuration.h"

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

#include "upnpdevice.h"
#include "taskqueue.h"
#include "httpserver.h"
#include "ssdp.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

class UPnp
{

    protected:

        HttpServer             *m_pHttpServer;
        int                     m_nServicePort;

    public:

        static Configuration   *g_pConfig;

        static QString          g_sPlatform;

        static UPnpDeviceDesc   g_UPnpDeviceDesc;
        static TaskQueue       *g_pTaskQueue;
        static SSDP            *g_pSSDP;
        static SSDPCache        g_SSDPCache;

    public:
                 UPnp();
        virtual ~UPnp();

        void SetConfiguration( Configuration *pConfig );

        bool Initialize( int nServicePort, HttpServer *pHttpServer );

        virtual void Start();

        void CleanUp      ( void );

        UPnpDevice *RootDevice() { return &(g_UPnpDeviceDesc.m_rootDevice); }

        static void PerformSearch( const QString &sST )
        {
            if (g_pSSDP)
                g_pSSDP->PerformSearch( sST );
        }

        HttpServer *GetHttpServer() { return m_pHttpServer; }

        static void AddListener   ( QObject *listener ) { g_SSDPCache.addListener   ( listener ); }
        static void RemoveListener( QObject *listener ) { g_SSDPCache.removeListener( listener ); }

        static SSDPCacheEntries *Find( const QString &sURI );
        static DeviceLocation   *Find( const QString &sURI, const QString &sUSN );

        static UPnpDeviceDesc *GetDeviceDesc( QString &sURL, bool bInQtThread = TRUE);

};

#endif
