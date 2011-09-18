//////////////////////////////////////////////////////////////////////////////
// Program Name: upnptaskevent.cpp
// Created     : Dec. 31, 2006
//
// Purpose     : UPnp Task to notifing subscribers of an event
//                                                                            
// Copyright (c) 2006 David Blain <mythtv@theblains.net>
//                                          
// This library is free software; you can redistribute it and/or 
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or at your option any later version of the LGPL.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library.  If not, see <http://www.gnu.org/licenses/>.
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

