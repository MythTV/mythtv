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

MediaServer::MediaServer( bool bIsMaster, HttpServer *pHttpServer ) :
                    UPnp( pHttpServer )
{
    VERBOSE(VB_UPNP, QString("MediaServer::Begin"));

    QString sSharePath = gContext->GetShareDir();
    QString sFileName  = gContext->GetSetting( "upnpDescXmlPath", sSharePath );

    if ( bIsMaster )
        sFileName += "devicemaster.xml";
    else
        sFileName += "deviceslave.xml";

    // ----------------------------------------------------------------------
    // Make sure our device Description is loaded.
    // ----------------------------------------------------------------------

    VERBOSE(VB_UPNP, QString( "MediaServer::Loading UPnp Description" ));

    g_UPnpDeviceDesc.Load( sFileName );

    // ----------------------------------------------------------------------
    // Register any HttpServerExtensions... Only The Master Backend 
    // ----------------------------------------------------------------------

    if (bIsMaster)
    {
        VERBOSE(VB_UPNP, QString( "MediaServer::Registering MSRR Service." ));

        m_pHttpServer->RegisterExtension( new UPnpMSRR( RootDevice() ));

        VERBOSE(VB_UPNP, QString( "MediaServer::Registering CMGR Service." ));

        m_pHttpServer->RegisterExtension( m_pUPnpCMGR= new UPnpCMGR( RootDevice() ));

        VERBOSE(VB_UPNP, QString( "MediaServer::Registering CDS Service." ));

        m_pHttpServer->RegisterExtension( m_pUPnpCDS = new UPnpCDS ( RootDevice() ));
    }

//    VERBOSE(VB_UPNP, QString( "MediaServer::Adding Context Listener" ));

//    gContext->addListener( this );

    VERBOSE(VB_UPNP, QString( "MediaServer::End" ));
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

MediaServer::~MediaServer()
{
    // -=>TODO: Need to check to see if calling this more than once is ok.

//    gContext->removeListener(this);
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
/*
void MediaServer::customEvent( QCustomEvent *e )
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
