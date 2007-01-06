//////////////////////////////////////////////////////////////////////////////
// Program Name: upnptaskevent.cpp
//                                                                            
// Purpose - UPnp Task to notifing subscribers of an event
//                                                                            
// Created By  : David Blain                    Created On : Dec. 31, 2006
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#include "upnptaskevent.h"

#include <unistd.h>
#include <sys/time.h>

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// UPnpEventTaskTask Implementation
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpEventTask::UPnpEventTask( QHostAddress peerAddress,
                              int          nPeerPort,  
                              QByteArray  *pPayload )
{
    m_PeerAddress = peerAddress;
    m_nPeerPort   = nPeerPort;
    m_pPayload    = pPayload;  // We take ownership of this pointer.
} 

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpEventTask::~UPnpEventTask()  
{ 
    if (m_pPayload != NULL)
        delete m_pPayload;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpEventTask::Execute( TaskQueue * /*pQueue*/ )
{
    if (m_pPayload == NULL)
        return;

    QSocketDevice        *pSockDev = new QSocketDevice( QSocketDevice::Stream );
    BufferedSocketDevice *pSock    = new BufferedSocketDevice( pSockDev );

    pSockDev->setBlocking( true );

    if (pSock->Connect( m_PeerAddress, m_nPeerPort ))
    {
        // ------------------------------------------------------------------
        // Send NOTIFY message
        // ------------------------------------------------------------------

        if (pSock->WriteBlockDirect( m_pPayload->data(), m_pPayload->size() ) != -1) 
        {
            // --------------------------------------------------------------
            // Read first line to determine success/Fail
            // --------------------------------------------------------------

            QString sResponseLine = pSock->ReadLine( 3000 );

            if ( sResponseLine.length() > 0)
            {
                //if (sResponseLine.contains("200 OK"))
                //{
                    VERBOSE( VB_UPNP, QString( "UPnpEventTask::Execute - NOTIFY to %1:%2 returned %3." )
                                         .arg( m_PeerAddress.toString() )
                                         .arg( m_nPeerPort )
                                         .arg( sResponseLine ));

                //}
            }
            else
            {
                VERBOSE( VB_UPNP, QString( "UPnpEventTask::Execute - Timeout reading first line of reply from %1:%2." )
                                     .arg( m_PeerAddress.toString() )
                                     .arg( m_nPeerPort ) );
            }
        }
        else
            VERBOSE( VB_UPNP, QString( "UPnpEventTask::Execute - Error sending to %1:%2." )
                                 .arg( m_PeerAddress.toString() )
                                 .arg( m_nPeerPort ) );

        pSock->Close();
    }
    else
    {
        VERBOSE( VB_UPNP, QString( "UPnpEventTask::Execute - Error sending to %1:%2." )
                                 .arg( m_PeerAddress.toString() )
                                 .arg( m_nPeerPort ) );
    }

    if ( pSock != NULL )
        delete pSock;

    if ( pSockDev != NULL )
        delete pSockDev;
}

