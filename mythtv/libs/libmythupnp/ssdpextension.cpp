#include "ssdpextension.h"

#include <QString>
#include <QStringList>
#include <QTextStream>

#include "libmythbase/configuration.h"
#include "libmythbase/mythlogging.h"

#include "ssdpcache.h"
#include "upnp.h"
#include "upnputil.h"

SSDPExtension::SSDPExtension( int nServicePort , const QString &sSharePath)
  : HttpServerExtension( "SSDP" , sSharePath),
    m_nServicePort(nServicePort)
{
    m_nSupportedMethods |= (RequestTypeMSearch | RequestTypeNotify);
    m_sUPnpDescPath = XmlConfiguration().GetValue("UPnP/DescXmlPath", m_sSharePath);
}

SSDPExtension::SSDPMethod SSDPExtension::GetMethod( const QString &sURI )
{
    if (sURI == "getDeviceDesc"     ) return( SSDPM_GetDeviceDesc    );
    if (sURI == "getDeviceList"     ) return( SSDPM_GetDeviceList    );

    return( SSDPM_Unknown );
}

QStringList SSDPExtension::GetBasePaths()
{
    // -=>TODO: This is very inefficient... should look into making
    //          it a unique path.

    return QStringList( "/" );
}

bool SSDPExtension::ProcessRequest( HTTPRequest *pRequest )
{
    if (pRequest)
    {
        if ( pRequest->m_sBaseUrl != "/")
            return( false );

        switch( GetMethod( pRequest->m_sMethod ))
        {
            case SSDPM_GetDeviceDesc: GetDeviceDesc( pRequest ); return( true );
            case SSDPM_GetDeviceList: GetDeviceList( pRequest ); return( true );

            default: break;
        }
    }

    return( false );
}

void SSDPExtension::GetDeviceDesc( HTTPRequest *pRequest ) const
{
    pRequest->m_eResponseType = ResponseTypeXML;

    QString sUserAgent = pRequest->GetRequestHeader( "User-Agent", "" );

    LOG(VB_UPNP, LOG_DEBUG, "SSDPExtension::GetDeviceDesc - " +
        QString( "Host=%1 Port=%2 UserAgent=%3" )
            .arg(pRequest->GetHostAddress()) .arg(m_nServicePort)
            .arg(sUserAgent));

    QTextStream stream( &(pRequest->m_response) );

    UPnp::g_UPnpDeviceDesc.GetValidXML( pRequest->GetHostAddress(),
                                        m_nServicePort,
                                        stream,
                                        sUserAgent  );
}

void SSDPExtension::GetFile( HTTPRequest *pRequest, const QString& sFileName )
{
    pRequest->m_eResponseType   = ResponseTypeHTML;

    pRequest->m_sFileName = m_sUPnpDescPath + sFileName;

    if (QFile::exists( pRequest->m_sFileName ))
    {
        LOG(VB_UPNP, LOG_DEBUG,
            QString("SSDPExtension::GetFile( %1 ) - Exists")
                .arg(pRequest->m_sFileName));

        pRequest->m_eResponseType   = ResponseTypeFile;
        pRequest->m_nResponseStatus = 200;
        pRequest->m_mapRespHeaders[ "Cache-Control" ]
            = "no-cache=\"Ext\", max-age = 7200"; // 2 hours
    }
    else
    {
        pRequest->m_nResponseStatus = 404;
        pRequest->m_response.write( pRequest->GetResponsePage() );
        LOG(VB_UPNP, LOG_ERR,
            QString("SSDPExtension::GetFile( %1 ) - Not Found")
                .arg(pRequest->m_sFileName));
    }

}

void SSDPExtension::GetDeviceList( HTTPRequest *pRequest )
{
    LOG(VB_UPNP, LOG_DEBUG, "SSDPExtension::GetDeviceList");

    QString     sXML;
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    QTextStream os(&sXML, QIODevice::WriteOnly);
#else
    QTextStream os(&sXML, QIODeviceBase::WriteOnly);
#endif

    uint nDevCount = 0;
    uint nEntryCount = 0;
    SSDPCache::Instance()->OutputXML(os, &nDevCount, &nEntryCount);

    NameValues list;
    list.push_back(
        NameValue("DeviceCount",           (int)nDevCount));
    list.push_back(
        NameValue("DevicesAllocated",      SSDPCacheEntries::g_nAllocated));
    list.push_back(
        NameValue("CacheEntriesFound",     (int)nEntryCount));
    list.push_back(
        NameValue("CacheEntriesAllocated", DeviceLocation::g_nAllocated));
    list.push_back(
        NameValue("DeviceList",            sXML));

    pRequest->FormatActionResponse(list);

    pRequest->m_eResponseType   = ResponseTypeXML;
    pRequest->m_nResponseStatus = 200;
}
