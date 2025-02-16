//////////////////////////////////////////////////////////////////////////////
// Program Name: mediaserver.cpp
//
// Purpose - uPnp Media Server main Class
//
// Created By  : David Blain                    Created On : Jan. 15, 2007
// Modified By :                                Modified On:
//
//////////////////////////////////////////////////////////////////////////////

#include "libmythbase/mythconfig.h"

// Qt
#include <QNetworkInterface>
#include <QNetworkProxy>

// MythTV
#if CONFIG_LIBDNS_SD
#include "libmythbase/bonjourregister.h"
#endif
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdb.h"
#include "libmythbase/mythdirs.h"
#include "libmythbase/mythlogging.h"
#include "libmythupnp/htmlserver.h"

// MythBackend
#include "httpconfig.h"
#include "internetContent.h"
#include "mediaserver.h"
#include "upnpcdsmusic.h"
#include "upnpcdstv.h"
#include "upnpcdsvideo.h"


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
    m_sSharePath(GetShareDir())
{
    LOG(VB_UPNP, LOG_INFO, "MediaServer()");
}

void MediaServer::Init(bool bIsMaster, bool bDisableUPnp /* = false */)
{
    LOG(VB_UPNP, LOG_INFO, "MediaServer::Init(): Begin");

    int nPort     = GetMythDB()->GetNumSetting("BackendStatusPort", 6544);
    int nSSLPort  = GetMythDB()->GetNumSetting("BackendSSLPort", nPort + 10);
    int nWSPort   = nPort + 5;
    // UPNP port is now status port + 6 (default 6550)
    nPort += 6;
    nSSLPort += 6;

    auto *pHttpServer = new HttpServer();

    if (!pHttpServer->isListening())
    {
        pHttpServer->setProxy(QNetworkProxy::NoProxy);
        // HTTP
        if (!pHttpServer->listen(nPort))
        {
            LOG(VB_GENERAL, LOG_ERR, "MediaServer: HttpServer Create Error");
            delete pHttpServer;
            pHttpServer = nullptr;
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

    m_webSocketServer = new WebSocketServer();

    if (!m_webSocketServer->isListening())
    {
        if (!m_webSocketServer->listen(nWSPort))
        {
            LOG(VB_GENERAL, LOG_ERR, "MediaServer: WebSocketServer Create Error");
        }
    }

    QString sFileName = GetMythDB()->GetSetting("upnpDescXmlPath", m_sSharePath);

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

    auto *pHtmlServer =
        new HtmlServerExtension(m_sSharePath + "html", "backend_");
    pHttpServer->RegisterExtension( pHtmlServer );
    pHttpServer->RegisterExtension( new HttpConfig() );
    pHttpServer->RegisterExtension( new InternetContent   ( m_sSharePath ));

    // ------------------------------------------------------------------
    // Register Service Types with Scripting Engine
    //
    // -=>NOTE: We need to know the actual type at compile time for this
    //          to work, so it needs to be done here.  I'm still looking
    //          into ways that we may encapsulate this in the service
    //          classes. - dblain
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

#if CONFIG_LIBDNS_SD
        // advertise using Bonjour
        if (gCoreContext)
        {
            m_bonjour = new BonjourRegister();
            if (m_bonjour)
            {
                QByteArray name("Mythbackend on ");
                name.append(gCoreContext->GetHostName().toUtf8());
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

    delete m_webSocketServer;
    delete m_pHttpServer;

#if CONFIG_LIBDNS_SD
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
        MythEvent *me = static_cast<MythEvent *>(e);
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
