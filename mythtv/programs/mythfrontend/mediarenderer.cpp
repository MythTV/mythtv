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
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
#include <QScriptEngine>
#endif

#include "upnpsubscription.h"
#include "upnputil.h"
#include "mediarenderer.h"
#include "mythversion.h"
#include "upnpscanner.h"
#include "mythfexml.h"
#include "compat.h"
#include "mythdate.h"
#include "htmlserver.h"

#include "serviceHosts/frontendServiceHost.h"
#include "services/frontend.h"

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
    LOG(VB_UPNP, LOG_INFO, "MediaRenderer(): Begin");

    // ----------------------------------------------------------------------
    // Initialize Configuration class (XML file for frontend)
    // ----------------------------------------------------------------------

    SetConfiguration( new XmlConfiguration( "config.xml" ));

    // ----------------------------------------------------------------------
    // Create mini HTTP Server
    // ----------------------------------------------------------------------

    int nPort = g_pConfig->GetValue( "UPnP/MythFrontend/ServicePort", 6547 );

    auto *pHttpServer = new HttpServer();

    if (!pHttpServer)
        return;

    if (!pHttpServer->listen(nPort))
    {
        LOG(VB_GENERAL, LOG_ERR, "MediaRenderer: HttpServer Create Error");
        delete pHttpServer;
        pHttpServer = nullptr;
        return;
    }

    // ------------------------------------------------------------------
    // Register any HttpServerExtensions...
    // ------------------------------------------------------------------

    auto *pHtmlServer =
                 new HtmlServerExtension(pHttpServer->GetSharePath() + "html",
                                         "frontend_");
    pHttpServer->RegisterExtension(pHtmlServer);
    pHttpServer->RegisterExtension(new FrontendServiceHost(pHttpServer->GetSharePath()));

    // ------------------------------------------------------------------
    // Register Service Types with Scripting Engine
    //
    // -=>NOTE: We need to know the actual type at compile time for this
    //          to work, so it needs to be done here.  I'm still looking
    //          into ways that we may encapsulate this in the service
    //          classes. - dblain
    // ------------------------------------------------------------------

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    QScriptEngine* pEngine = pHtmlServer->ScriptEngine();

    pEngine->globalObject().setProperty("Frontend"   ,
        pEngine->scriptValueFromQMetaObject< ScriptableFrontend    >() );
#endif

    // ----------------------------------------------------------------------
    // Initialize UPnp Stack
    // ----------------------------------------------------------------------

    if (Initialize( nPort, pHttpServer ))
    {
        // ------------------------------------------------------------------
        // Create device Description
        // ------------------------------------------------------------------

        LOG(VB_UPNP, LOG_INFO, "MediaRenderer: Creating UPnp Description");

        UPnpDevice &device = g_UPnpDeviceDesc.m_rootDevice;

        device.m_sDeviceType   = "urn:schemas-upnp-org:device:MediaRenderer:1";
        device.m_sFriendlyName      = "MythTV AV Renderer";
        device.m_sManufacturer      = "MythTV";
        device.m_sManufacturerURL   = "http://www.mythtv.org";
        device.m_sModelDescription  = "MythTV AV Media Renderer";
        device.m_sModelName         = "MythTV AV Media Renderer";
        device.m_sModelURL          = "http://www.mythtv.org";
        device.m_sUPC               = "";
        device.m_sPresentationURL   = "/";

        QString sSinkProtocols = GetSinkProtocolInfos().join(",");

        // ------------------------------------------------------------------
        // Register the MythFEXML protocol...
        // ------------------------------------------------------------------
        LOG(VB_UPNP, LOG_INFO, "MediaRenderer: Registering MythFEXML Extension.");
        m_pHttpServer->RegisterExtension(
            new MythFEXML(RootDevice(), m_pHttpServer->GetSharePath()));

#if 0
        LOG(VB_UPNP, LOG_INFO,
            "MediaRenderer::Registering AVTransport Service.");
        m_pUPnpAVT = new UPnpAVTransport( RootDevice() );
        m_pHttpServer->RegisterExtension( m_pUPnpAVT );
#endif

        LOG(VB_UPNP, LOG_INFO, "MediaRenderer: Registering ConnectionManager Service.");
        // HttpServer will be responsible for deleting UPnpCMGR
        m_pUPnpCMGR = new UPnpCMGR(
            RootDevice(), m_pHttpServer->GetSharePath(), "", sSinkProtocols);
        m_pHttpServer->RegisterExtension( m_pUPnpCMGR );

#if 0
        LOG(VB_UPNP, LOG_INFO,
            "MediaRenderer::Registering RenderingControl Service.");
        m_pUPnpRCTL= new UPnpRCTL( RootDevice() );
        m_pHttpServer->RegisterExtension( m_pUPnpRCTL );
#endif

        UPNPSubscription *subscription = nullptr;
        if (qEnvironmentVariableIsSet("MYTHTV_UPNPSCANNER"))
        {
            LOG(VB_UPNP, LOG_INFO, "MediaRenderer: Registering UPnP Subscription Extension.");
            subscription = new UPNPSubscription(m_pHttpServer->GetSharePath(), nPort);
            m_pHttpServer->RegisterExtension(subscription);
        }

        Start();

        // Start scanning for UPnP media servers
        if (subscription)
            UPNPScanner::Enable(true, subscription);

        // ensure the frontend is aware of all backends (slave and master) and
        // other frontends
        SSDP::Instance()->PerformSearch("ssdp:all");
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR,
            "MediaRenderer: Unable to Initialize UPnp Stack");
    }

    LOG(VB_UPNP, LOG_INFO, "MediaRenderer(): End");
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

MediaRenderer::~MediaRenderer()
{
    UPNPScanner::Enable(false);
    delete m_pHttpServer;
}
