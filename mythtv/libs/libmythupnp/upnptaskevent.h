//////////////////////////////////////////////////////////////////////////////
// Program Name: upnptaskevent.h
// Created     : Dec. 31, 2006
//
// Purpose     : UPnp Task to notifing subscribers of an event
//                                                                            
// Copyright (c) 2006 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef UPNPTASKEVENT_H
#define UPNPTASKEVENT_H

#include <QByteArray>
#include <QHostAddress>

#include "libmythupnp/taskqueue.h"

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// UPnpEventTask Class Definition
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class UPnpEventTask : public Task
{
    protected: 

        QHostAddress m_peerAddress;
        int          m_nPeerPort;
        QByteArray  *m_pPayload {nullptr};

    protected:

        // Destructor protected to force use of Release Method

        ~UPnpEventTask() override;

    public:

        UPnpEventTask( const QHostAddress& peerAddress,
                       int          nPeerPort,  
                       QByteArray  *pPayload ) :
            Task("UPnpEventTask"),
            m_peerAddress(peerAddress),
            m_nPeerPort(nPeerPort),
            m_pPayload(pPayload)  // We take ownership of this pointer.
        {}

        QString Name() override { return( "Event" ); } // Task
        void Execute( TaskQueue *pQueue ) override; // Task

};


#endif // UPNPTASKEVENT_H
