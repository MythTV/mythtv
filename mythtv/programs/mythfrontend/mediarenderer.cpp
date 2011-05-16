/////////////////////////////////////////////////////////////////////////////
// Program Name: mediarenderer.cpp
//
// Purpose - uPnp Media Renderer main Class
//
// Created By  : David Blain                    Created On : Jan. 15, 2007
// Modified By :                                Modified On:
//
/////////////////////////////////////////////////////////////////////////////

#include <QTextStream>
#include "mediarenderer.h"
#include "mythfexml.h"
#include "compat.h"

class MythFrontendStatus : public HttpServerExtension
{
  public:
    MythFrontendStatus(const QString &sSharePath)
      : HttpServerExtension("MythFrontendStatus", sSharePath) { }

    virtual QStringList GetBasePaths() { return QStringList( "/" ); }

    virtual bool ProcessRequest(HttpWorkerThread *pThread,
                                HTTPRequest *pRequest)
    {
        (void)pThread;

        if (!pRequest)
            return false;

        if (pRequest->m_sBaseUrl != "/")
            return false;

        pRequest->m_eResponseType = ResponseTypeHTML;
        pRequest->m_mapRespHeaders[ "Cache-Control" ] = "no-cache=\"Ext\", max-age = 5000";

        SSDPCacheEntries* cache = NULL;
        QString ipaddress = QString();
        if (!UPnp::g_IPAddrList.isEmpty())
            ipaddress = UPnp::g_IPAddrList.at(0);

        QString shortdateformat = gCoreContext->GetSetting("ShortDateFormat", "M/d");
        QString timeformat      = gCoreContext->GetSetting("TimeFormat", "h:mm AP");
        QString hostname   = gCoreContext->GetHostName();
        QDateTime qdtNow   = QDateTime::currentDateTime();
        QString masterhost = gCoreContext->GetMasterHostName();
        QString masterip   = gCoreContext->GetSetting("MasterServerIP");
        QString masterport = gCoreContext->GetSettingOnHost("BackendStatusPort", masterhost, "6544");

        QTextStream stream ( &pRequest->m_response );
        stream.setCodec("UTF-8");

        stream
           << "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" "
           << "\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\r\n"
           << "<html xmlns=\"http://www.w3.org/1999/xhtml\""
           << " xml:lang=\"en\" lang=\"en\">\r\n"
           << "<head>\r\n"
           << "  <meta http-equiv=\"Content-Type\""
           << "content=\"text/html; charset=UTF-8\" />\r\n"
           << "  <title>MythFrontend Status - "
           << qdtNow.toString(shortdateformat) << " "
           << qdtNow.toString(timeformat) << " - "
           << MYTH_BINARY_VERSION << "</title>\r\n"
           << "  <link rel=\"stylesheet\" href=\"/css/site.css\"   type=\"text/css\">\r\n"
           << "  <link rel=\"stylesheet\" href=\"/css/Status.css\" type=\"text/css\">\r\n"
           << "</head>\r\n"
           << "<body>\r\n\r\n"
           << "  <h1 class=\"status\">MythFrontend Status</h1>\r\n";

        // This frontend
        stream
           << "  <div class=\"content\">\r\n"
           << "    <h2 class=\"status\">This Frontend</h2>\r\n"
           << "Name : " << hostname << "<br />\r\n"
           << "Version : " << MYTH_BINARY_VERSION << "\r\n"
           << "  </div>\r\n";

        // Other frontends

        // This will not work with multiple frontends on the same machine (upnp
        // setup will fail on a second frontend anyway) and the ip address
        // filtering of the current frontend may not work in all situations

        cache = SSDP::Find("urn:schemas-mythtv-org:service:MythFrontend:1");
        if (cache)
        {
            stream
               << "  <div class=\"content\">\r\n"
               << "    <h2 class=\"status\">Other Frontends</h2>\r\n";
            cache->AddRef();
            cache->Lock();
            EntryMap* map = cache->GetEntryMap();
            QMapIterator< QString, DeviceLocation * > i(*map);
            while (i.hasNext())
            {
                i.next();
                QUrl url(i.value()->m_sLocation);
                if (url.host() != ipaddress)
                {
                    stream << "<br />" << url.host() << "&nbsp(<a href=\""
                           << url.toString(QUrl::RemovePath) << "\">Status page</a>)\r\n";
                }
            }
            cache->Unlock();
            cache->Release();
            stream << "  </div>\r\n";
        }

        // Master backend
        stream
           << "  <div class=\"content\">\r\n"
           << "    <h2 class=\"status\">MythTV Backends</h2>\r\n"
           << "Master: " << masterhost << "&nbsp(<a href=\"http://"
           << masterip << ":" << masterport
           << "\">Status page</a>)\r\n";

        // Slave backends
        cache = SSDP::Find("urn:schemas-mythtv-org:device:SlaveMediaServer:1");
        if (cache)
        {
            cache->AddRef();
            cache->Lock();
            EntryMap* map = cache->GetEntryMap();
            QMapIterator< QString, DeviceLocation * > i(*map);
            while (i.hasNext())
            {
                i.next();
                QUrl url(i.value()->m_sLocation);
                stream << "<br />" << "Slave: " << url.host() << "&nbsp(<a href=\""
                       << url.toString(QUrl::RemovePath) << "\">Status page</a>)\r\n";
            }
            cache->Unlock();
            cache->Release();
        }

        stream
           << "  </div>\r\n";

        stream
           << "  <div class=\"content\">\r\n"
           << "    <h2 class=\"status\">Services</h2>\r\n"
           << "    <a href=\"MythFE/GetRemote\">Remote Control</a>\r\n"
           << "  </div>\r\n";

        double load[3];
        if (getloadavg(load, 3) != -1)
        {
            stream
               << "  <div class=\"content\">\r\n"
               << "    <h2 class=\"status\">Machine Information</h2>\r\n"
               << "    <div class=\"loadstatus\">\r\n"
               << "      This machine's load average:"
               << "\r\n      <ul>\r\n        <li>"
               << "1 Minute: " << QString::number(load[0]) << "</li>\r\n"
               << "        <li>5 Minutes: " << QString::number(load[1]) << "</li>\r\n"
               << "        <li>15 Minutes: " << QString::number(load[2])
               << "</li>\r\n      </ul>\r\n"
               << "    </div>\r\n"
               << "  </div>\r\n";
        }

        stream << "</body>\r\n</html>\r\n";
        stream.flush();

        return true;
    }
};

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// UPnp MediaRenderer Class implementaion
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

MediaRenderer::MediaRenderer()
{
    VERBOSE(VB_UPNP, "MediaRenderer::Begin" );

    // ----------------------------------------------------------------------
    // Initialize Configuration class (XML file for frontend)
    // ----------------------------------------------------------------------

    SetConfiguration( new XmlConfiguration( "config.xml" ));

    // ----------------------------------------------------------------------
    // Create mini HTTP Server
    // ----------------------------------------------------------------------

    int nPort = g_pConfig->GetValue( "UPnP/MythFrontend/ServicePort", 6547 );

    m_pHttpServer = new HttpServer();

    if (!m_pHttpServer)
        return;

    if (!m_pHttpServer->listen(QHostAddress::Any, nPort))
    {
        VERBOSE(VB_IMPORTANT, "MediaRenderer::HttpServer Create Error");
        delete m_pHttpServer;
        m_pHttpServer = NULL;
        return;
    }

    // ----------------------------------------------------------------------
    // Initialize UPnp Stack
    // ----------------------------------------------------------------------

    if (Initialize( nPort, m_pHttpServer ))
    {
        // ------------------------------------------------------------------
        // Create device Description
        // ------------------------------------------------------------------

        VERBOSE(VB_UPNP, QString( "MediaRenderer::Creating UPnp Description" ));

        UPnpDevice &device = g_UPnpDeviceDesc.m_rootDevice;

        device.m_sDeviceType        = "urn:schemas-upnp-org:device:MediaRenderer:1";
        device.m_sFriendlyName      = "MythTv AV Renderer";
        device.m_sManufacturer      = "MythTV";
        device.m_sManufacturerURL   = "http://www.mythtv.org";
        device.m_sModelDescription  = "MythTV AV Media Renderer";
        device.m_sModelName         = "MythTV AV Media Renderer";
        device.m_sModelURL          = "http://www.mythtv.org";
        device.m_sUPC               = "";
        device.m_sPresentationURL   = "/";

        // ------------------------------------------------------------------
        // Register any HttpServerExtensions...
        // ------------------------------------------------------------------

        VERBOSE(VB_UPNP, "MediaRenderer::Registering MythFrontendStatus service.");
        m_pHttpServer->RegisterExtension(new MythFrontendStatus(m_pHttpServer->m_sSharePath));

        QString sSinkProtocols = "http-get:*:image/gif:*,"
                                 "http-get:*:image/jpeg:*,"
                                 "http-get:*:image/png:*,"
                                 "http-get:*:video/avi:*,"
                                 "http-get:*:audio/mpeg:*,"
                                 "http-get:*:audio/wav:*,"
                                 "http-get:*:video/mpeg:*,"
                                 "http-get:*:video/nupplevideo:*,"
                                 "http-get:*:video/x-ms-wmv:*";
        // ------------------------------------------------------------------
        // Register the MythFEXML protocol...
        // ------------------------------------------------------------------
        VERBOSE(VB_UPNP, "MediaRenderer::Registering MythFEXML Service." );
        m_pHttpServer->RegisterExtension( new MythFEXML( RootDevice(),
                                          m_pHttpServer->m_sSharePath));

        // VERBOSE(VB_UPNP, QString( "MediaRenderer::Registering AVTransport Service." ));
        // m_pHttpServer->RegisterExtension( m_pUPnpAVT = new UPnpAVTransport( RootDevice() ));

        VERBOSE(VB_UPNP, QString( "MediaRenderer::Registering CMGR Service." ));
        // HttpServer will be responsible for deleting UPnpCMGR
        m_pHttpServer->RegisterExtension( m_pUPnpCMGR = new UPnpCMGR(
                                                RootDevice(),
                                                m_pHttpServer->m_sSharePath,
                                                "", sSinkProtocols ));

        // VERBOSE(VB_UPNP, QString( "MediaRenderer::Registering RenderingControl Service." ));
        // m_pHttpServer->RegisterExtension( m_pUPnpRCTL= new UPnpRCTL( RootDevice() ));

        Start();

        // ensure the frontend is aware of all backends (slave and master) and
        // other frontends
        SSDP::Instance()->PerformSearch("ssdp:all");
    }
    else
    {
        VERBOSE(VB_IMPORTANT, "MediaRenderer::Unable to Initialize UPnp Stack");
    }

    VERBOSE(VB_UPNP, QString( "MediaRenderer::End" ));
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

MediaRenderer::~MediaRenderer()
{
    delete m_pHttpServer;
}
