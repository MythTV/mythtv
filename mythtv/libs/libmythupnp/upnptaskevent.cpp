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

#include <chrono>

#include <QString>

#include "libmythbase/mythlogging.h"

#include "blockingtcpsocket.h"

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

    LOG(VB_UPNP, LOG_INFO, QString("UPnpEventTask::Execute - NOTIFY to %1:%2.")
        .arg(m_peerAddress.toString(), QString::number(m_nPeerPort))
        );

    const std::chrono::milliseconds timeout {3s};
    BlockingTcpSocket socket;
    if (socket.connect(m_peerAddress, m_nPeerPort, timeout))
    {
        // ------------------------------------------------------------------
        // Send NOTIFY message
        // ------------------------------------------------------------------

        if (socket.write(m_pPayload->data(), m_pPayload->size(), timeout) != -1)
        {
            // --------------------------------------------------------------
            // Read first line to determine success/Fail
            // --------------------------------------------------------------

            QString sResponseLine = socket.readLine(timeout);

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
        }
    }
}
