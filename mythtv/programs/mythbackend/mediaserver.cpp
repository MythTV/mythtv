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

#include "upnpcdstv.h"
#include "upnpcdsmusic.h"
#include "upnpcdsvideo.h"

#include <QScriptEngine>
#include <QNetworkProxy>

#include "serviceHosts/mythServiceHost.h"
#include "serviceHosts/guideServiceHost.h"
#include "serviceHosts/contentServiceHost.h"
#include "serviceHosts/dvrServiceHost.h"
#include "serviceHosts/channelServiceHost.h"
#include "serviceHosts/videoServiceHost.h"
#include "serviceHosts/captureServiceHost.h"

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
    LOG(VB_UPNP, LOG_INFO, "MediaServer:ctor:Begin");

    // ----------------------------------------------------------------------
    // Initialize Configuration class (Database for Servers)
    // ----------------------------------------------------------------------

    SetConfiguration( new DBConfiguration() );

    // ----------------------------------------------------------------------
    // Create mini HTTP Server
    // ----------------------------------------------------------------------

    LOG(VB_UPNP, LOG_INFO, "MediaServer:ctor:End");
}

void MediaServer::Init(bool bIsMaster, bool bDisableUPnp /* = false */)
{
    LOG(VB_UPNP, LOG_INFO, "MediaServer:Init:Begin");

    int     nPort     = g_pConfig->GetValue( "BackendStatusPort", 6544 );

    if (!m_pHttpServer)
    {
        m_pHttpServer = new HttpServer();
        m_pHttpServer->RegisterExtension(new HttpConfig());
    }

    if (!m_pHttpServer->isListening())
    {
        m_pHttpServer->setProxy(QNetworkProxy::NoProxy);
        if (!m_pHttpServer->listen(nPort))
        {
            LOG(VB_GENERAL, LOG_ERR, "MediaServer::HttpServer Create Error");
            delete m_pHttpServer;
            m_pHttpServer = NULL;
            return;
        }
    }

    QString sFileName = g_pConfig->GetValue( "upnpDescXmlPath",
                                                m_sSharePath );
    QString sDeviceType;

    if ( bIsMaster )
        sFileName  += "devicemaster.xml";
    else
        sFileName += "deviceslave.xml";

    // ------------------------------------------------------------------
    // Make sure our device Description is loaded.
    // ------------------------------------------------------------------

    LOG(VB_UPNP, LOG_INFO,
        "MediaServer::Loading UPnp Description " + sFileName);

    g_UPnpDeviceDesc.Load( sFileName );

    // ------------------------------------------------------------------
    // Register Http Server Extensions...
    // ------------------------------------------------------------------

    LOG(VB_UPNP, LOG_INFO, "MediaServer::Registering Http Server Extensions.");

    m_pHttpServer->RegisterExtension( new InternetContent   ( m_sSharePath ));

    m_pHttpServer->RegisterExtension( new MythServiceHost   ( m_sSharePath ));
    m_pHttpServer->RegisterExtension( new GuideServiceHost  ( m_sSharePath ));
    m_pHttpServer->RegisterExtension( new ContentServiceHost( m_sSharePath ));
    m_pHttpServer->RegisterExtension( new DvrServiceHost    ( m_sSharePath ));
    m_pHttpServer->RegisterExtension( new ChannelServiceHost( m_sSharePath ));
    m_pHttpServer->RegisterExtension( new VideoServiceHost  ( m_sSharePath ));
    m_pHttpServer->RegisterExtension( new CaptureServiceHost( m_sSharePath ));

    QString sIP = g_pConfig->GetValue( "BackendServerIP"  , ""   );
    if (sIP.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR,
            "MediaServer:: No BackendServerIP Address defined - "
            "Disabling UPnP");
        return;
    }

    // ------------------------------------------------------------------
    // Register Service Types with Scripting Engine
    //
    // -=>NOTE: We need to know the actual type at compile time for this 
    //          to work, so it needs to be done here.  I'm still looking
    //          into ways that we may encapsulate this in the service 
    //          classes.
    // ------------------------------------------------------------------


     QScriptEngine* pEngine = m_pHttpServer->ScriptEngine();

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
     pEngine->globalObject().setProperty("Capture"  ,
         pEngine->scriptValueFromQMetaObject< ScriptableCapture   >() );

    // ------------------------------------------------------------------

    if (sIP == "localhost" || sIP.startsWith("127."))
    {
        LOG(VB_GENERAL, LOG_NOTICE,
            "MediaServer:: Loopback address specified - " + sIP +
            ". Disabling UPnP");
        return;
    }

    if (bDisableUPnp)
    {
        LOG(VB_GENERAL, LOG_NOTICE,
            "*** The UPNP service has been DISABLED with the "
            "--noupnp option ***");
        return;
    }

    // ----------------------------------------------------------------------
    // BackendServerIP is only one IP address at this time... Doing Split anyway
    // ----------------------------------------------------------------------

    QStringList sIPAddrList = sIP.split(';', QString::SkipEmptyParts);

    // ----------------------------------------------------------------------
    // Initialize UPnp Stack
    // ----------------------------------------------------------------------

    if (Initialize( sIPAddrList, nPort, m_pHttpServer ))
    {

        // ------------------------------------------------------------------
        // Register any HttpServerExtensions... Only The Master Backend
        // ------------------------------------------------------------------

        if (bIsMaster)
        {
            QString sSourceProtocols = "http-get:*:image/gif:*,"
                                       "http-get:*:image/jpeg:*,"
                                       "http-get:*:image/png:*,"
                                       "http-get:*:video/avi:*,"
                                       "http-get:*:audio/mpeg:*,"
                                       "http-get:*:audio/wav:*,"
                                       "http-get:*:video/mpeg:*,"
                                       "http-get:*:video/nupplevideo:*,"
                                       "http-get:*:video/x-ms-wmv:*";

            LOG(VB_UPNP, LOG_INFO, "MediaServer::Registering MSRR Service.");

            m_pHttpServer->RegisterExtension( new UPnpMSRR( RootDevice(),
                                                m_sSharePath ) );

            LOG(VB_UPNP, LOG_INFO, "MediaServer::Registering CMGR Service.");

            m_pUPnpCMGR = new UPnpCMGR( RootDevice(),
                                        m_sSharePath, sSourceProtocols );
            m_pHttpServer->RegisterExtension( m_pUPnpCMGR );

            LOG(VB_UPNP, LOG_INFO, "MediaServer::Registering CDS Service.");

            m_pUPnpCDS = new UPnpCDS ( RootDevice(), m_sSharePath );
            m_pHttpServer->RegisterExtension( m_pUPnpCDS );

            // ----------------------------------------------------------------
            // Register CDS Extensions
            // ----------------------------------------------------------------

            LOG(VB_UPNP, LOG_INFO, 
                "MediaServer::Registering UPnpCDSTv Extension");

            RegisterExtension(new UPnpCDSTv());

            LOG(VB_UPNP, LOG_INFO,
                "MediaServer::Registering UPnpCDSMusic Extension");

            RegisterExtension(new UPnpCDSMusic());

            LOG(VB_UPNP, LOG_INFO,
                "MediaServer::Registering UPnpCDSVideo Extension");

            RegisterExtension(new UPnpCDSVideo());
        }

#if 0
        LOG(VB_UPNP, LOG_INFO, "MediaServer::Adding Context Listener");

        gCoreContext->addListener( this );
#endif

        Start();

#ifdef USING_LIBDNS_SD
        // advertise using Bonjour
        m_bonjour = new BonjourRegister();
        if (m_bonjour)
        {
            QByteArray dummy;
            QByteArray name("Mythbackend on ");
            name.append(gCoreContext->GetHostName());
            m_bonjour->Register(nPort,
                                bIsMaster ? "_mythbackend-master._tcp" :
                                            "_mythbackend-slave._tcp",
                                name, dummy);
        }
#endif
    }

    LOG(VB_UPNP, LOG_INFO, "MediaServer:Init:End");
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
