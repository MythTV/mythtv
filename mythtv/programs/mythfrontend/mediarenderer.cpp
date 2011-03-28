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

        QString shortdateformat = gCoreContext->GetSetting("ShortDateFormat", "M/d");
        QString timeformat      = gCoreContext->GetSetting("TimeFormat", "h:mm AP");
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
           << "  <style type=\"text/css\" title=\"Default\" media=\"all\">\r\n"
           << "  <!--\r\n"
           << "  body {\r\n"
           << "    background-color:#fff;\r\n"
           << "    font:11px verdana, arial, helvetica, sans-serif;\r\n"
           << "    margin:20px;\r\n"
           << "  }\r\n"
           << "  h1 {\r\n"
           << "    font-size:28px;\r\n"
           << "    font-weight:900;\r\n"
           << "    color:#ccc;\r\n"
           << "    letter-spacing:0.5em;\r\n"
           << "    margin-bottom:30px;\r\n"
           << "    width:650px;\r\n"
           << "    text-align:center;\r\n"
           << "  }\r\n"
           << "  h2 {\r\n"
           << "    font-size:18px;\r\n"
           << "    font-weight:800;\r\n"
           << "    color:#360;\r\n"
           << "    border:none;\r\n"
           << "    letter-spacing:0.3em;\r\n"
           << "    padding:0px;\r\n"
           << "    margin-bottom:10px;\r\n"
           << "    margin-top:0px;\r\n"
           << "  }\r\n"
           << "  hr {\r\n"
           << "    display:none;\r\n"
           << "  }\r\n"
           << "  div.content {\r\n"
           << "    width:650px;\r\n"
           << "    border-top:1px solid #000;\r\n"
           << "    border-right:1px solid #000;\r\n"
           << "    border-bottom:1px solid #000;\r\n"
           << "    border-left:10px solid #000;\r\n"
           << "    padding:10px;\r\n"
           << "    margin-bottom:30px;\r\n"
           << "    -moz-border-radius:8px 0px 0px 8px;\r\n"
           << "  }\r\n"
           << "  div.schedule a {\r\n"
           << "    display:block;\r\n"
           << "    color:#000;\r\n"
           << "    text-decoration:none;\r\n"
           << "    padding:.2em .8em;\r\n"
           << "    border:thin solid #fff;\r\n"
           << "    width:350px;\r\n"
           << "  }\r\n"
           << "  div.schedule a span {\r\n"
           << "    display:none;\r\n"
           << "  }\r\n"
           << "  div.schedule a:hover {\r\n"
           << "    background-color:#F4F4F4;\r\n"
           << "    border-top:thin solid #000;\r\n"
           << "    border-bottom:thin solid #000;\r\n"
           << "    border-left:thin solid #000;\r\n"
           << "    cursor:default;\r\n"
           << "  }\r\n"
           << "  div.schedule a:hover span {\r\n"
           << "    display:block;\r\n"
           << "    position:absolute;\r\n"
           << "    background-color:#F4F4F4;\r\n"
           << "    color:#000;\r\n"
           << "    left:400px;\r\n"
           << "    margin-top:-20px;\r\n"
           << "    width:280px;\r\n"
           << "    padding:5px;\r\n"
           << "    border:thin dashed #000;\r\n"
           << "  }\r\n"
           << "  div.loadstatus {\r\n"
           << "    width:325px;\r\n"
           << "    height:7em;\r\n"
           << "  }\r\n"
           << "  .jobfinished { color: #0000ff; }\r\n"
           << "  .jobaborted { color: #7f0000; }\r\n"
           << "  .joberrored { color: #ff0000; }\r\n"
           << "  .jobrunning { color: #005f00; }\r\n"
           << "  .jobqueued  {  }\r\n"
           << "  -->\r\n"
           << "  </style>\r\n"
           << "  <title>MythFrontend Status - "
           << qdtNow.toString(shortdateformat) << " "
           << qdtNow.toString(timeformat) << " - "
           << MYTH_BINARY_VERSION << "</title>\r\n"
           << "</head>\r\n"
           << "<body>\r\n\r\n"
           << "  <h1>MythFrontend Status</h1>\r\n";

        stream
           << "  <div class=\"content\">\r\n"
           << "    <h2>Master Backend</h2>\r\n"
           << masterhost << "&nbsp(<a href=\"http://"
           << masterip << ":" << masterport
           << "\">Status page</a>)\r\n"
           << "  </div>\r\n";

        stream
           << "  <div class=\"content\">\r\n"
           << "    <h2>Services</h2>\r\n"
           << "    <a href=\"MythFE/GetRemote\">Remote Control</a>\r\n"
           << "  </div>\r\n";

        double load[3];
        if (getloadavg(load, 3) != -1)
        {
            stream
               << "  <div class=\"content\">\r\n"
               << "    <h2>Machine Information</h2>\r\n"
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
        InitializeSSDPOnly();
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
