//////////////////////////////////////////////////////////////////////////////
// Program Name: UPnpCMGR.h
//                                                                            
// Purpose - 
//                                                                            
// Created By  : David Blain                    Created On : Dec. 28, 2006
// Modified By :                                Modified On:                  
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

class UPnpCMGR : public Eventing
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
        virtual QString GetServiceId        () { return "urn:upnp-org:serviceId:CMGR_1-0"; }
        virtual QString GetServiceControlURL() { return m_sControlUrl.mid( 1 ); }
        virtual QString GetServiceDescURL   () { return m_sControlUrl.mid( 1 ) + "/GetServDesc"; }

    public:
                 UPnpCMGR(  UPnpDevice *pDevice,
                            const QString &sSourceProtocols = "",
                            const QString &sSinkProtocols   = "" );

        virtual ~UPnpCMGR();

        void    AddSourceProtocol( const QString &sProtocol );
        void    AddSinkProtocol  ( const QString &sProtocol );

        virtual bool     ProcessRequest( HttpWorkerThread *pThread, HTTPRequest *pRequest );
};

#endif
