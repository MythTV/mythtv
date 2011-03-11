//////////////////////////////////////////////////////////////////////////////
// Program Name: upnp.h
// Created     : Oct. 24, 2005
//
// Purpose     : UPnp Main Class
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

#ifndef __UPNP_H__
#define __UPNP_H__

#include "configuration.h"

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

#include "upnpdevice.h"
#include "taskqueue.h"
#include "httpserver.h"
#include "ssdp.h"

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

typedef enum 
{
    UPnPResult_Success                       =   0,

    UPnPResult_InvalidAction                 = 401,
    UPnPResult_InvalidArgs                   = 402,
    UPnPResult_ActionFailed                  = 501,
    UPnPResult_ArgumentValueInvalid          = 600,
    UPnPResult_ArgumentValueOutOfRange       = 601,
    UPnPResult_OptionalActionNotImplemented  = 602,
    UPnPResult_OutOfMemory                   = 603,
    UPnPResult_HumanInterventionRequired     = 604,
    UPnPResult_StringArgumentTooLong         = 605,
    UPnPResult_ActionNotAuthorized           = 606,
    UPnPResult_SignatureFailure              = 607,
    UPnPResult_SignatureMissing              = 608,
    UPnPResult_NotEncrypted                  = 609,
    UPnPResult_InvalidSequence               = 610,
    UPnPResult_InvalidControlURL             = 611,
    UPnPResult_NoSuchSession                 = 612,

    UPnPResult_CDS_NoSuchObject              = 701,
    UPnPResult_CDS_InvalidCurrentTagValue    = 702,
    UPnPResult_CDS_InvalidNewTagValue        = 703,
    UPnPResult_CDS_RequiredTag               = 704,
    UPnPResult_CDS_ReadOnlyTag               = 705,
    UPnPResult_CDS_ParameterMismatch         = 706,
    UPnPResult_CDS_InvalidSearchCriteria     = 708,
    UPnPResult_CDS_InvalidSortCriteria       = 709,
    UPnPResult_CDS_NoSuchContainer           = 710,
    UPnPResult_CDS_RestrictedObject          = 711,
    UPnPResult_CDS_BadMetadata               = 712,
    UPnPResult_CDS_ResrtictedParentObject    = 713,
    UPnPResult_CDS_NoSuchSourceResource      = 714,
    UPnPResult_CDS_ResourceAccessDenied      = 715,
    UPnPResult_CDS_TransferBusy              = 716,
    UPnPResult_CDS_NoSuchFileTransfer        = 717,
    UPnPResult_CDS_NoSuchDestRes             = 718,
    UPnPResult_CDS_DestResAccessDenied       = 719,
    UPnPResult_CDS_CannotProcessRequest      = 720,

    UPnPResult_CMGR_IncompatibleProtocol     = 701,
    UPnPResult_CMGR_IncompatibleDirections   = 702,
    UPnPResult_CMGR_InsufficientNetResources = 703,
    UPnPResult_CMGR_LocalRestrictions        = 704,
    UPnPResult_CMGR_AccessDenied             = 705,
    UPnPResult_CMGR_InvalidConnectionRef     = 706,
    UPnPResult_CMGR_NotInNetwork             = 707,

    UPnPResult_MS_AccessDenied               = 801

} UPnPResultCode;

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC UPnp
{

    protected:

        HttpServer             *m_pHttpServer;
        int                     m_nServicePort;

    public:

        static Configuration   *g_pConfig;

        static UPnpDeviceDesc   g_UPnpDeviceDesc;
        static TaskQueue       *g_pTaskQueue;
        static SSDP            *g_pSSDP;
        static SSDPCache        g_SSDPCache;
        static QStringList      g_IPAddrList;

    public:
                 UPnp();
        virtual ~UPnp();

        static void SetConfiguration( Configuration *pConfig );

        bool Initialize( int nServicePort, HttpServer *pHttpServer );
        bool Initialize( QStringList &sIPAddrList, int nServicePort, HttpServer *pHttpServer );
        bool InitializeSSDPOnly( void );

        virtual void Start();

        void CleanUp      ();

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

        static UPnpDeviceDesc *GetDeviceDesc( QString &sURL, bool bInQtThread = true);

        static QString         GetResultDesc( UPnPResultCode eCode );
        static void            FormatErrorResponse( HTTPRequest   *pRequest, 
                                                    UPnPResultCode  eCode, 
                                                    const QString &sMsg = "" );

};

#endif
