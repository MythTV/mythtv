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

#include "serviceHosts/mythServiceHost.h"
#include "serviceHosts/guideServiceHost.h"
#include "serviceHosts/contentServiceHost.h"
#include "serviceHosts/dvrServiceHost.h"
#include "serviceHosts/channelServiceHost.h"
#include "serviceHosts/videoServiceHost.h"

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
    m_pUPnpCDS(NULL), m_pUPnpCMGR(NULL),
    m_sSharePath(GetShareDir())
{
    VERBOSE(VB_UPNP, "MediaServer:ctor:Begin");

    // ----------------------------------------------------------------------
    // Initialize Configuration class (Database for Servers)
    // ----------------------------------------------------------------------

    SetConfiguration( new DBConfiguration() );

    // ----------------------------------------------------------------------
    // Create mini HTTP Server
    // ----------------------------------------------------------------------

    VERBOSE(VB_UPNP, "MediaServer:ctor:End");
}

void MediaServer::Init(bool bIsMaster, bool bDisableUPnp /* = FALSE */)
{
    VERBOSE(VB_UPNP, "MediaServer:Init:Begin");

    int     nPort     = g_pConfig->GetValue( "BackendStatusPort", 6544 );

	if (!m_pHttpServer)
	{
		m_pHttpServer = new HttpServer();

		m_pHttpServer->m_sSharePath = m_sSharePath;

		m_pHttpServer->RegisterExtension(new HttpConfig());
	}

    if (!m_pHttpServer->isListening())
	{
		if (!m_pHttpServer->listen(QHostAddress(gCoreContext->MythHostAddressAny()), nPort))
		{
			VERBOSE(VB_IMPORTANT, "MediaServer::HttpServer Create Error");
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

    VERBOSE(VB_UPNP, "MediaServer::Loading UPnp Description " + sFileName );

    g_UPnpDeviceDesc.Load( sFileName );

    // ------------------------------------------------------------------
    // Register Http Server Extensions...
    // ------------------------------------------------------------------

    VERBOSE(VB_UPNP, "MediaServer::Registering Http Server Extensions." );

    m_pHttpServer->RegisterExtension( new InternetContent   ( m_sSharePath ));

    m_pHttpServer->RegisterExtension( new MythServiceHost   ( m_sSharePath ));
    m_pHttpServer->RegisterExtension( new GuideServiceHost  ( m_sSharePath ));
    m_pHttpServer->RegisterExtension( new ContentServiceHost( m_sSharePath ));
    m_pHttpServer->RegisterExtension( new DvrServiceHost    ( m_sSharePath ));
    m_pHttpServer->RegisterExtension( new ChannelServiceHost( m_sSharePath ));
    m_pHttpServer->RegisterExtension( new VideoServiceHost  ( m_sSharePath ));

    QString sIP = g_pConfig->GetValue( "BackendServerIP"  , ""   );
    if (sIP.isEmpty())
    {
        VERBOSE(VB_IMPORTANT,
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

     pEngine->globalObject().setProperty("Myth"   , pEngine->scriptValueFromQMetaObject< ScriptableMyth    >() );
     pEngine->globalObject().setProperty("Guide"  , pEngine->scriptValueFromQMetaObject< ScriptableGuide   >() );
     pEngine->globalObject().setProperty("Content", pEngine->scriptValueFromQMetaObject< Content           >() );
     pEngine->globalObject().setProperty("Dvr"    , pEngine->scriptValueFromQMetaObject< ScriptableDvr     >() );
     pEngine->globalObject().setProperty("Channel", pEngine->scriptValueFromQMetaObject< ScriptableChannel >() );
     pEngine->globalObject().setProperty("Video"  , pEngine->scriptValueFromQMetaObject< ScriptableVideo   >() );

    // ------------------------------------------------------------------

    if (sIP == "localhost" || sIP.startsWith("127."))
    {
        VERBOSE(VB_IMPORTANT, "MediaServer:: Loopback address specified - "
                              + sIP + ". Disabling UPnP");
        return;
    }

    if (bDisableUPnp)
    {
        VERBOSE(VB_IMPORTANT, "*** The UPNP service has been DISABLED with the "
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

            VERBOSE(VB_UPNP, "MediaServer::Registering MSRR Service." );

            m_pHttpServer->RegisterExtension( new UPnpMSRR( RootDevice(),
                                                m_sSharePath ) );

            VERBOSE(VB_UPNP, "MediaServer::Registering CMGR Service." );

            m_pUPnpCMGR = new UPnpCMGR( RootDevice(),
                                        m_sSharePath, sSourceProtocols );
            m_pHttpServer->RegisterExtension( m_pUPnpCMGR );

            VERBOSE(VB_UPNP, "MediaServer::Registering CDS Service." );

            m_pUPnpCDS = new UPnpCDS ( RootDevice(), m_sSharePath );
            m_pHttpServer->RegisterExtension( m_pUPnpCDS );

            // ----------------------------------------------------------------
            // Register CDS Extensions
            // ----------------------------------------------------------------

            VERBOSE(VB_UPNP, "MediaServer::Registering UPnpCDSTv Extension");

            RegisterExtension(new UPnpCDSTv());

            VERBOSE(VB_UPNP, "MediaServer::Registering UPnpCDSMusic Extension");

            RegisterExtension(new UPnpCDSMusic());

            VERBOSE(VB_UPNP, "MediaServer::Registering UPnpCDSVideo Extension");

            RegisterExtension(new UPnpCDSVideo());
        }

        // VERBOSE(VB_UPNP, QString( "MediaServer::Adding Context Listener" ));

        // gCoreContext->addListener( this );

        Start();

    }

    VERBOSE(VB_UPNP, "MediaServer:Init:End");
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

MediaServer::~MediaServer()
{
    // -=>TODO: Need to check to see if calling this more than once is ok.

//    gCoreContext->removeListener(this);

    delete m_pHttpServer;
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
/*
void MediaServer::customEvent( QEvent *e )
{
    if (MythEvent::Type(e->type()) == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent *)e;
        QString message = me->Message();

        //-=>TODO: Need to handle events to notify clients of changes
    }
}
*/
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
