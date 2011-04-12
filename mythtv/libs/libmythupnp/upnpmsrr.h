#ifndef UPnpMSRR_H_
#define UPnpMSRR_H_

#include <QDomDocument>
#include <QString>

#include "httpserver.h"
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

class UPNP_PUBLIC  UPnpMSRR : public Eventing
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
                 UPnpMSRR( UPnpDevice *pDevice,
			   const QString &sSharePath ); 

        virtual ~UPnpMSRR();

        virtual QStringList GetBasePaths();

        bool     ProcessRequest( HttpWorkerThread *pThread, HTTPRequest *pRequest );
};

#endif
