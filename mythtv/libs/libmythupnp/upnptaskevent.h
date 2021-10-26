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

#include "upnp.h"
#include "bufferedsocketdevice.h"

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

        UPnpEventTask( QHostAddress peerAddress,
                       int          nPeerPort,  
                       QByteArray  *m_pPayload );

        QString Name() override { return( "Event" ); } // Task
        void Execute( TaskQueue *pQueue ) override; // Task

};


#endif // UPNPTASKEVENT_H
