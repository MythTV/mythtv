//////////////////////////////////////////////////////////////////////////////
// Program Name: upnptaskevent.cpp
// Created     : Dec. 31, 2006
//
// Purpose     : UPnp Task to notifing subscribers of an event
//                                                                            
// Copyright (c) 2006 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////
#include "upnptaskevent.h"

#include <utility>

#include "libmythbase/mythlogging.h"

#include "libmythupnp/bufferedsocketdevice.h"
#include "libmythupnp/msocketdevice.h"

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

UPnpEventTask::~UPnpEventTask()  
{ 
    delete m_pPayload;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpEventTask::Execute( TaskQueue * /*pQueue*/ )
{
    if (m_pPayload == nullptr)
        return;

    MSocketDevice        sockDev( MSocketDevice::Stream );
    BufferedSocketDevice sock   ( &sockDev );

    sockDev.setBlocking( true );

    if (sock.Connect( m_peerAddress, m_nPeerPort ))
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

            QString sResponseLine = sock.ReadLine( 3s );

            if ( sResponseLine.length() > 0)
            {
#if 0
                if (sResponseLine.contains("200 OK"))
                {
#endif
                    LOG(VB_UPNP, LOG_INFO,
                        QString("UPnpEventTask::Execute - NOTIFY to "
                                "%1:%2 returned %3.")
                            .arg(m_peerAddress.toString()) .arg(m_nPeerPort)
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
                        .arg(m_peerAddress.toString()) .arg(m_nPeerPort));
            }
        }
        else
        {
            LOG(VB_UPNP, LOG_ERR,
                QString("UPnpEventTask::Execute - Error sending to %1:%2.")
                    .arg(m_peerAddress.toString()) .arg(m_nPeerPort));
        }

        sock.Close();
    }
    else
    {
        LOG(VB_UPNP, LOG_ERR,
            QString("UPnpEventTask::Execute - Error sending to %1:%2.")
                .arg(m_peerAddress.toString()) .arg(m_nPeerPort));
    }
}

