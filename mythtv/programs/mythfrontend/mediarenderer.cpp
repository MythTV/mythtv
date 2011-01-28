/////////////////////////////////////////////////////////////////////////////
// Program Name: mediarenderer.cpp
//
// Purpose - uPnp Media Renderer main Class
//
// Created By  : David Blain                    Created On : Jan. 15, 2007
// Modified By :                                Modified On:
//
/////////////////////////////////////////////////////////////////////////////

#include "mediarenderer.h"
#include "mythfexml.h"
#include "compat.h"

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
        // exit(BACKEND_BUGGY_EXIT_NO_BIND_STATUS);
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
        device.m_sPresentationURL   = "";

        // ------------------------------------------------------------------
        // Register any HttpServerExtensions...
        // ------------------------------------------------------------------

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
        // exit(BACKEND_BUGGY_EXIT_NO_BIND_STATUS);
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
