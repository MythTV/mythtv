//////////////////////////////////////////////////////////////////////////////
// Program Name: upnptaskevent.cpp
// Created     : Dec. 31, 2006
//
// Purpose     : UPnp Task to notifing subscribers of an event
//                                                                            
// Copyright (c) 2006 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details                    
//
//////////////////////////////////////////////////////////////////////////////

#include "upnptaskevent.h"
#include "mythlogging.h"

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
                              QByteArray  *pPayload ) :
    Task("UPnpEventTask")
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

    MSocketDevice        sockDev( MSocketDevice::Stream );
    BufferedSocketDevice sock   ( &sockDev );

    sockDev.setBlocking( true );

    if (sock.Connect( m_PeerAddress, m_nPeerPort ))
    {
        // ------------------------------------------------------------------
        // Send NOTIFY message
        // ------------------------------------------------------------------

        if (sock.WriteBlockDirect( m_pPayload->data(),
                                     m_pPayload->size() ) != -1) 
        {
            // --------------------------------------------------------------
            // Read first line to determine success/Fail
            // --------------------------------------------------------------

            QString sResponseLine = sock.ReadLine( 3000 );

            if ( sResponseLine.length() > 0)
            {
#if 0
                if (sResponseLine.contains("200 OK"))
                {
#endif
                    LOG(VB_UPNP, LOG_INFO,
                        QString("UPnpEventTask::Execute - NOTIFY to "
                                "%1:%2 returned %3.")
                            .arg(m_PeerAddress.toString()) .arg(m_nPeerPort)
                            .arg(sResponseLine));
#if 0
                }
#endif
            }
            else
            {
                LOG(VB_UPNP, LOG_ERR,
                    QString("UPnpEventTask::Execute - Timeout reading first "
                            "line of reply from %1:%2.")
                        .arg(m_PeerAddress.toString()) .arg(m_nPeerPort));
            }
        }
        else
            LOG(VB_UPNP, LOG_ERR,
                QString("UPnpEventTask::Execute - Error sending to %1:%2.")
                    .arg(m_PeerAddress.toString()) .arg(m_nPeerPort));

        sock.Close();
    }
    else
    {
        LOG(VB_UPNP, LOG_ERR,
            QString("UPnpEventTask::Execute - Error sending to %1:%2.")
                .arg(m_PeerAddress.toString()) .arg(m_nPeerPort));
    }
}

