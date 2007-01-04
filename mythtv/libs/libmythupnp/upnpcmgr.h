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
#include "mythcontext.h"
#include "eventing.h"
              
typedef enum 
{
    CMGRM_Unknown                  = 0,
    CMGRM_GetProtocolInfo          = 1,
    CMGRM_GetCurrentConnectionInfo = 2,
    CMGRM_GetCurrentConnectionIDs  = 3

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

        UPnpCMGRMethod  GetMethod                     ( const QString &sURI );

        void            HandleGetProtocolInfo         ( HTTPRequest *pRequest );
        void            HandleGetCurrentConnectionInfo( HTTPRequest *pRequest );
        void            HandleGetCurrentConnectionIDs ( HTTPRequest *pRequest );

    public:
                 UPnpCMGR(); 
        virtual ~UPnpCMGR();

        virtual bool     ProcessRequest( HttpWorkerThread *pThread, HTTPRequest *pRequest );
};

#endif
