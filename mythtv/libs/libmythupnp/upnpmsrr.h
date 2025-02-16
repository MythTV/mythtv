
#ifndef UPnpMSRR_H_
#define UPnpMSRR_H_

#include <QDomDocument>
#include <QString>

#include "eventing.h"
#include "httprequest.h"

enum UPnpMSRRMethod : std::uint8_t
{
    MSRR_Unknown                = 0,
    MSRR_GetServiceDescription  = 1,
    MSRR_IsAuthorized           = 2,
    MSRR_RegisterDevice         = 3,
    MSRR_IsValidated            = 4
};

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

        static UPnpMSRRMethod  GetMethod                  ( const QString &sURI  );

        static void            HandleIsAuthorized         ( HTTPRequest *pRequest );
        static void            HandleRegisterDevice       ( HTTPRequest *pRequest );
        static void            HandleIsValidated          ( HTTPRequest *pRequest );

    protected:

        // Implement UPnpServiceImpl methods that we can

        QString GetServiceType() override // UPnpServiceImpl
            { return "urn:microsoft.com:service:X_MS_MediaReceiverRegistrar:1"; }
        QString GetServiceId() override // UPnpServiceImpl
            { return "urn:microsoft.com:serviceId:X_MS_MediaReceiverRegistrar"; }
        QString GetServiceControlURL() override // UPnpServiceImpl
            { return m_sControlUrl.mid( 1 ); }
        QString GetServiceDescURL() override // UPnpServiceImpl
            { return m_sControlUrl.mid( 1 ) + "/GetServDesc"; }

    public:
                 UPnpMSRR( UPnpDevice *pDevice,
                           const QString &sSharePath ); 

        ~UPnpMSRR() override = default;

        QStringList GetBasePaths() override; // Eventing

        bool ProcessRequest( HTTPRequest *pRequest ) override; // Eventing
};

#endif
