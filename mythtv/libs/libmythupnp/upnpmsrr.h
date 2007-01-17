#ifndef UPnpMSRR_H_
#define UPnpMSRR_H_

#include <qdom.h>
#include <qdatetime.h> 

#include "httpserver.h"
#include "mythcontext.h"
#include "eventing.h"
              
class UPnpMSRR;
                          
typedef enum 
{
    MSRR_Unknown                = 0,
    MSRR_GetServiceDescription  = 1,
    MSRR_IsAuthorized           = 2,
    MSRR_RegisterDevice         = 3,
    MSRR_IsValidated            = 4

} UPnpMSRRMethod;

//////////////////////////////////////////////////////////////////////////////
//
// UPnpMSRR Class Definition
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

class UPnpMSRR : public Eventing
{
    private:

        QString         m_sServiceDescFileName;
        QString         m_sControlUrl;

        UPnpMSRRMethod  GetMethod                  ( const QString &sURI  );

        void            HandleIsAuthorized         ( HTTPRequest *pRequest );
        void            HandleRegisterDevice       ( HTTPRequest *pRequest );
        void            HandleIsValidated          ( HTTPRequest *pRequest );

    protected:

        // Implement UPnpServiceImpl methods that we can

        virtual QString GetServiceType      () { return "urn:microsoft.com:service:X_MS_MediaReceiverRegistrar:1"; }
        virtual QString GetServiceId        () { return "urn:microsoft.com:serviceId:X_MS_MediaReceiverRegistrar"; }
        virtual QString GetServiceControlURL() { return m_sControlUrl.mid( 1 ); }
        virtual QString GetServiceDescURL   () { return m_sControlUrl.mid( 1 ) + "/GetServDesc"; }

    public:
                 UPnpMSRR( UPnpDevice *pDevice ); 
        virtual ~UPnpMSRR();

        bool     ProcessRequest( HttpWorkerThread *pThread, HTTPRequest *pRequest );
};

#endif
