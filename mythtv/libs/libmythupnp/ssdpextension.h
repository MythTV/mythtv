#ifndef LIBUPNP_SSDPEXTENSION_H
#define LIBUPNP_SSDPEXTENSION_H

#include <cstdint>

#include <QString>
#include <QStringList>

#include "httprequest.h"
#include "httpserver.h"

class SSDPExtension : public HttpServerExtension
{
    private:

        QString     m_sUPnpDescPath;
        int         m_nServicePort;

    private:
        enum SSDPMethod : std::uint8_t
        {
            SSDPM_Unknown         = 0,
            SSDPM_GetDeviceDesc   = 1,
            SSDPM_GetDeviceList   = 2
        };

        static SSDPMethod GetMethod( const QString &sURI );

        void       GetDeviceDesc( HTTPRequest *pRequest ) const;
        void       GetFile      ( HTTPRequest *pRequest, const QString& sFileName );
        static void       GetDeviceList( HTTPRequest *pRequest );

    public:
                 SSDPExtension( int nServicePort, const QString &sSharePath);
        ~SSDPExtension( ) override = default;

        QStringList GetBasePaths() override; // HttpServerExtension

        bool     ProcessRequest( HTTPRequest *pRequest ) override; // HttpServerExtension
};

#endif // LIBUPNP_SSDPEXTENSION_H
