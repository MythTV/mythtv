//////////////////////////////////////////////////////////////////////////////
// Program Name: mediaserver.cpp
//
// Purpose - uPnp Media Server main Class
//
// Created By  : David Blain                    Created On : Jan. 15, 2007
// Modified By :                                Modified On:
//
//////////////////////////////////////////////////////////////////////////////

#include "mediaserver.h"
#include "httpconfig.h"
#include "internetContent.h"
#include "mythdirs.h"
#include "htmlserver.h"
#include <websocket.h>

#include "upnpcdstv.h"
#include "upnpcdsmusic.h"
#include "upnpcdsvideo.h"

#include <QScriptEngine>
#include <QNetworkProxy>
#include <QNetworkInterface>

#include "serviceHosts/mythServiceHost.h"
#include "serviceHosts/guideServiceHost.h"
#include "serviceHosts/contentServiceHost.h"
#include "serviceHosts/dvrServiceHost.h"
#include "serviceHosts/channelServiceHost.h"
#include "serviceHosts/videoServiceHost.h"
#include "serviceHosts/musicServiceHost.h"
#include "serviceHosts/captureServiceHost.h"
#include "serviceHosts/imageServiceHost.h"

#ifdef USING_LIBDNS_SD
#include "bonjourregister.h"
#endif

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
// UPnp Class implementaion
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

MediaServer::MediaServer(void) :
#ifdef USING_LIBDNS_SD
    m_bonjour(NULL),
#endif
    m_pUPnpCDS(NULL), m_pUPnpCMGR(NULL),
    m_sSharePath(GetShareDir())
{
    LOG(VB_UPNP, LOG_INFO, "MediaServer(): Begin");

    // ----------------------------------------------------------------------
    // Initialize Configuration class (Database for Servers)
    // ----------------------------------------------------------------------

    SetConfiguration( new DBConfiguration() );

    // ----------------------------------------------------------------------
    // Create mini HTTP Server
    // ----------------------------------------------------------------------

    LOG(VB_UPNP, LOG_INFO, "MediaServer(): End");
}

void MediaServer::Init(bool bIsMaster, bool bDisableUPnp /* = false */)
{
    LOG(VB_UPNP, LOG_INFO, "MediaServer::Init(): Begin");

    int     nPort     = g_pConfig->GetValue( "BackendStatusPort", 6544 );
    int     nSSLPort  = g_pConfig->GetValue( "BackendSSLPort", (g_pConfig->GetValue( "BackendStatusPort", 6544 ) + 10) );
    int     nWSPort   = (g_pConfig->GetValue( "BackendStatusPort", 6544 ) + 5);

    HttpServer *pHttpServer = new HttpServer();

    if (!pHttpServer->isListening())
    {
        pHttpServer->setProxy(QNetworkProxy::NoProxy);
        // HTTP
        if (!pHttpServer->listen(nPort))
        {
            LOG(VB_GENERAL, LOG_ERR, "MediaServer: HttpServer Create Error");
            delete pHttpServer;
            pHttpServer = NULL;
            return;
        }

#ifndef QT_NO_OPENSSL
        // HTTPS (SSL)
        if (!pHttpServer->listen(nSSLPort, true, kSSLServer))
        {
            LOG(VB_GENERAL, LOG_ERR, "MediaServer: HttpServer failed to create SSL server");
        }
#endif
    }

    WebSocketServer *pWebSocketServer = new WebSocketServer();

    if (!pWebSocketServer->isListening())
    {
        if (!pWebSocketServer->listen(nWSPort))
        {
            LOG(VB_GENERAL, LOG_ERR, "MediaServer: WebSocketServer Create Error");
        }
    }

    QString sFileName = g_pConfig->GetValue( "upnpDescXmlPath",
                                                m_sSharePath );

    if ( bIsMaster )
        sFileName  += "devicemaster.xml";
    else
        sFileName += "deviceslave.xml";

    // ------------------------------------------------------------------
    // Make sure our device Description is loaded.
    // ------------------------------------------------------------------

    LOG(VB_UPNP, LOG_INFO,
        "MediaServer: Loading UPnp Description " + sFileName);

    g_UPnpDeviceDesc.Load( sFileName );

    // ------------------------------------------------------------------
    // Register Http Server Extensions...
    // ------------------------------------------------------------------

    LOG(VB_UPNP, LOG_INFO, "MediaServer: Registering Http Server Extensions.");

    HtmlServerExtension *pHtmlServer;
    pHtmlServer = new HtmlServerExtension(m_sSharePath + "html", "backend_");
    pHttpServer->RegisterExtension( pHtmlServer );
    pHttpServer->RegisterExtension( new HttpConfig() );
    pHttpServer->RegisterExtension( new InternetContent   ( m_sSharePath ));

    pHttpServer->RegisterExtension( new MythServiceHost   ( m_sSharePath ));
    pHttpServer->RegisterExtension( new GuideServiceHost  ( m_sSharePath ));
    pHttpServer->RegisterExtension( new ContentServiceHost( m_sSharePath ));
    pHttpServer->RegisterExtension( new DvrServiceHost    ( m_sSharePath ));
    pHttpServer->RegisterExtension( new ChannelServiceHost( m_sSharePath ));
    pHttpServer->RegisterExtension( new VideoServiceHost  ( m_sSharePath ));
    pHttpServer->RegisterExtension( new MusicServiceHost  ( m_sSharePath ));
    pHttpServer->RegisterExtension( new CaptureServiceHost( m_sSharePath ));
    pHttpServer->RegisterExtension( new ImageServiceHost  ( m_sSharePath ));


    // ------------------------------------------------------------------
    // Register Service Types with Scripting Engine
    //
    // -=>NOTE: We need to know the actual type at compile time for this
    //          to work, so it needs to be done here.  I'm still looking
    //          into ways that we may encapsulate this in the service
    //          classes. - dblain
    // ------------------------------------------------------------------

     QScriptEngine* pEngine = pHtmlServer->ScriptEngine();

     pEngine->globalObject().setProperty("Myth"   ,
         pEngine->scriptValueFromQMetaObject< ScriptableMyth    >() );
     pEngine->globalObject().setProperty("Guide"  ,
         pEngine->scriptValueFromQMetaObject< ScriptableGuide   >() );
     pEngine->globalObject().setProperty("Content",
         pEngine->scriptValueFromQMetaObject< ScriptableContent >() );
     pEngine->globalObject().setProperty("Dvr"    ,
         pEngine->scriptValueFromQMetaObject< ScriptableDvr     >() );
     pEngine->globalObject().setProperty("Channel",
         pEngine->scriptValueFromQMetaObject< ScriptableChannel >() );
     pEngine->globalObject().setProperty("Video"  ,
         pEngine->scriptValueFromQMetaObject< ScriptableVideo   >() );
     pEngine->globalObject().setProperty("Music"  ,
         pEngine->scriptValueFromQMetaObject< ScriptableVideo   >() );
     pEngine->globalObject().setProperty("Capture"  ,
         pEngine->scriptValueFromQMetaObject< ScriptableCapture  >() );
     pEngine->globalObject().setProperty("Image"  ,
         pEngine->scriptValueFromQMetaObject< ScriptableImage   >() );

    // ------------------------------------------------------------------

    if (bDisableUPnp)
    {
        LOG(VB_GENERAL, LOG_NOTICE,
            "*** The UPNP service has been DISABLED with the "
            "--noupnp option ***");
        return;
    }

    QList<QHostAddress> IPAddrList = ServerPool::DefaultListen();
    if (IPAddrList.contains(QHostAddress(QHostAddress::AnyIPv4)))
    {
        IPAddrList.removeAll(QHostAddress(QHostAddress::AnyIPv4));
        IPAddrList.removeAll(QHostAddress(QHostAddress::AnyIPv6));
        IPAddrList.append(QNetworkInterface::allAddresses());
    }

    if (IPAddrList.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR,
            "MediaServer: No Listenable IP Addresses found - "
            "Disabling UPnP");
        return;
    }

    // ----------------------------------------------------------------------
    // Initialize UPnp Stack
    // ----------------------------------------------------------------------

    if (Initialize( IPAddrList, nPort, pHttpServer ))
    {

        // ------------------------------------------------------------------
        // Register any HttpServerExtensions... Only The Master Backend
        // ------------------------------------------------------------------

        if (bIsMaster)
        {
            QString sSourceProtocols = GetSourceProtocolInfos().join(",");

            LOG(VB_UPNP, LOG_INFO, "MediaServer: Registering MS_MediaReceiverRegistrar Service.");

            m_pHttpServer->RegisterExtension( new UPnpMSRR( RootDevice(),
                                                m_sSharePath ) );

            LOG(VB_UPNP, LOG_INFO, "MediaServer: Registering ConnnectionManager Service.");

            m_pUPnpCMGR = new UPnpCMGR( RootDevice(),
                                        m_sSharePath, sSourceProtocols );
            m_pHttpServer->RegisterExtension( m_pUPnpCMGR );

            LOG(VB_UPNP, LOG_INFO, "MediaServer: Registering ContentDirectory Service.");

            m_pUPnpCDS = new UPnpCDS ( RootDevice(), m_sSharePath );
            m_pHttpServer->RegisterExtension( m_pUPnpCDS );

            // ----------------------------------------------------------------
            // Register CDS Extensions
            // ----------------------------------------------------------------

            LOG(VB_UPNP, LOG_INFO,
                "MediaServer: Registering UPnpCDSTv Extension");

            RegisterExtension(new UPnpCDSTv());

            LOG(VB_UPNP, LOG_INFO,
                "MediaServer: Registering UPnpCDSMusic Extension");

            RegisterExtension(new UPnpCDSMusic());

            LOG(VB_UPNP, LOG_INFO,
                "MediaServer: Registering UPnpCDSVideo Extension");

            RegisterExtension(new UPnpCDSVideo());
        }

#if 0
        LOG(VB_UPNP, LOG_INFO, "MediaServer::Adding Context Listener");

        gCoreContext->addListener( this );
#endif

        Start();

#ifdef USING_LIBDNS_SD
        // advertise using Bonjour
        if (gCoreContext)
        {
            m_bonjour = new BonjourRegister();
            if (m_bonjour)
            {
                QByteArray name("Mythbackend on ");
                name.append(gCoreContext->GetHostName());
                QByteArray txt(bIsMaster ? "\x06master" : "\x05slave");
                m_bonjour->Register(nPort, "_mythbackend._tcp", name, txt);
            }
        }
#endif
    }

    LOG(VB_UPNP, LOG_INFO, "MediaServer::Init(): End");
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

MediaServer::~MediaServer()
{
    // -=>TODO: Need to check to see if calling this more than once is ok.

#if 0
    gCoreContext->removeListener(this);
#endif

    delete m_pHttpServer;

#ifdef USING_LIBDNS_SD
    delete m_bonjour;
#endif
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
#if 0
void MediaServer::customEvent( QEvent *e )
{
    if (MythEvent::Type(e->type()) == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent *)e;
        QString message = me->Message();

        //-=>TODO: Need to handle events to notify clients of changes
    }
}
#endif
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void MediaServer::RegisterExtension( UPnpCDSExtension *pExtension )
{
    m_pUPnpCDS->RegisterExtension( pExtension );
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void MediaServer::UnregisterExtension( UPnpCDSExtension *pExtension )
{
    m_pUPnpCDS->UnregisterExtension( pExtension );
}
