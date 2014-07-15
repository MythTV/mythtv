//////////////////////////////////////////////////////////////////////////////
// Program Name: UPnpCMGR.h
// Created     : Dec. 28, 2006
//
// Purpose     : uPnp Connection Manager Service 
//                                                                            
// Copyright (c) 2006 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details                    
//
//////////////////////////////////////////////////////////////////////////////

#ifndef UPnpCMGR_H_
#define UPnpCMGR_H_

#include "httpserver.h"
#include "eventing.h"
              
typedef enum 
{
    CMGRM_Unknown                  = 0,
    CMGRM_GetServiceDescription    = 1,
    CMGRM_GetProtocolInfo          = 2,
    CMGRM_GetCurrentConnectionInfo = 3,
    CMGRM_GetCurrentConnectionIDs  = 4

} UPnpCMGRMethod;

//////////////////////////////////////////////////////////////////////////////

typedef enum 
{
    CMGRSTATUS_Unknown               = 0,
    CMGRSTATUS_OK                    = 1,
    CMGRSTATUS_ContentFormatMismatch = 2,
    CMGRSTATUS_InsufficientBandwidth = 3,
    CMGRSTATUS_UnreliableChannel     = 4

} UPnpCMGRConnectionStatus;

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
// UPnpCMGR Class Definition
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC UPnpCMGR : public Eventing
{
    private:

        QString         m_sServiceDescFileName;
        QString         m_sControlUrl;

        UPnpCMGRMethod  GetMethod                     ( const QString &sURI );

        void            HandleGetProtocolInfo         ( HTTPRequest *pRequest );
        void            HandleGetCurrentConnectionInfo( HTTPRequest *pRequest );
        void            HandleGetCurrentConnectionIDs ( HTTPRequest *pRequest );

    protected:

        // Implement UPnpServiceImpl methods that we can

        virtual QString GetServiceType      () { return "urn:schemas-upnp-org:service:ConnectionManager:1"; }
        virtual QString GetServiceId        () { return "urn:upnp-org:serviceId:ConnectionManager"; }
        virtual QString GetServiceControlURL() { return m_sControlUrl.mid( 1 ); }
        virtual QString GetServiceDescURL   () { return m_sControlUrl.mid( 1 ) + "/GetServDesc"; }

    public:
                 UPnpCMGR(  UPnpDevice *pDevice,
                            const QString &sSharePath,
                            const QString &sSourceProtocols = "",
                            const QString &sSinkProtocols   = "" );

        virtual ~UPnpCMGR();

        void    AddSourceProtocol( const QString &sProtocol );
        void    AddSinkProtocol  ( const QString &sProtocol );

        virtual QStringList GetBasePaths();

        virtual bool     ProcessRequest( HTTPRequest *pRequest );
};

#endif
