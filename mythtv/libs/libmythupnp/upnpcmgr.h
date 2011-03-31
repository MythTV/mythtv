//////////////////////////////////////////////////////////////////////////////
// Program Name: UPnpCMGR.h
// Created     : Dec. 28, 2006
//
// Purpose     : uPnp Connection Manager Service 
//                                                                            
// Copyright (c) 2006 David Blain <mythtv@theblains.net>
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
        virtual QString GetServiceId        () { return "urn:upnp-org:serviceId:CMGR_1-0"; }
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

        virtual bool     ProcessRequest( HttpWorkerThread *pThread, HTTPRequest *pRequest );
};

#endif
