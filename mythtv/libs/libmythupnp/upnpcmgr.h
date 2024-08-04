//////////////////////////////////////////////////////////////////////////////
// Program Name: UPnpCMGR.h
// Created     : Dec. 28, 2006
//
// Purpose     : uPnp Connection Manager Service 
//                                                                            
// Copyright (c) 2006 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef UPnpCMGR_H_
#define UPnpCMGR_H_

#include "httpserver.h"
#include "eventing.h"
              
enum UPnpCMGRMethod : std::uint8_t
{
    CMGRM_Unknown                  = 0,
    CMGRM_GetServiceDescription    = 1,
    CMGRM_GetProtocolInfo          = 2,
    CMGRM_GetCurrentConnectionInfo = 3,
    CMGRM_GetCurrentConnectionIDs  = 4,
    CMGRM_GetFeatureList           = 5
};

//////////////////////////////////////////////////////////////////////////////

enum UPnpCMGRConnectionStatus : std::uint8_t
{
    CMGRSTATUS_Unknown               = 0,
    CMGRSTATUS_OK                    = 1,
    CMGRSTATUS_ContentFormatMismatch = 2,
    CMGRSTATUS_InsufficientBandwidth = 3,
    CMGRSTATUS_UnreliableChannel     = 4
};

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

        UPnPFeatureList m_features;

        static UPnpCMGRMethod  GetMethod                     ( const QString &sURI );

        void            HandleGetProtocolInfo         ( HTTPRequest *pRequest );
        static void            HandleGetCurrentConnectionInfo( HTTPRequest *pRequest );
        void            HandleGetCurrentConnectionIDs ( HTTPRequest *pRequest );
        void            HandleGetFeatureList          ( HTTPRequest *pRequest );

    protected:

        // Implement UPnpServiceImpl methods that we can

        QString GetServiceType() override // UPnpServiceImpl
            { return "urn:schemas-upnp-org:service:ConnectionManager:3"; }
        QString GetServiceId() override // UPnpServiceImpl
            { return "urn:upnp-org:serviceId:ConnectionManager"; }
        QString GetServiceControlURL() override // UPnpServiceImpl
            { return m_sControlUrl.mid( 1 ); }
        QString GetServiceDescURL() override // UPnpServiceImpl
            { return m_sControlUrl.mid( 1 ) + "/GetServDesc"; }

    public:
                 UPnpCMGR(  UPnpDevice *pDevice,
                            const QString &sSharePath,
                            const QString &sSourceProtocols = "",
                            const QString &sSinkProtocols   = "" );

        ~UPnpCMGR() override = default;

        void    AddSourceProtocol( const QString &sProtocol );
        void    AddSinkProtocol  ( const QString &sProtocol );

        QStringList GetBasePaths() override; // Eventing

        bool    ProcessRequest( HTTPRequest *pRequest ) override; // Eventing
};

#endif
