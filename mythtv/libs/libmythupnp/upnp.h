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

    public:

        static QString          g_sPlatform;

        static UPnpDeviceDesc   g_UPnpDeviceDesc;
        static TaskQueue       *g_pTaskQueue;
        static SSDP            *g_pSSDP;
        static SSDP            *g_pSSDPBroadcast;
        static SSDPCache        g_SSDPCache;

    public:
                 UPnp( HttpServer *pHttpServer );
        virtual ~UPnp();

        virtual void Start();

        void CleanUp( void );

        UPnpDevice *RootDevice() { return &(g_UPnpDeviceDesc.m_rootDevice); }

};

#endif
