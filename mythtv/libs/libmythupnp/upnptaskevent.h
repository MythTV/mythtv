//////////////////////////////////////////////////////////////////////////////
// Program Name: upnptaskevent.h
// Created     : Dec. 31, 2006
//
// Purpose     : UPnp Task to notifing subscribers of an event
//                                                                            
// Copyright (c) 2006 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details                    
//
//////////////////////////////////////////////////////////////////////////////

#ifndef __UPNPTASKEVENT_H__
#define __UPNPTASKEVENT_H__

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

        QHostAddress m_PeerAddress;
        int          m_nPeerPort;
        QByteArray  *m_pPayload;

    protected:

        // Destructor protected to force use of Release Method

        virtual ~UPnpEventTask();

    public:

        UPnpEventTask( QHostAddress peerAddress,
                       int          nPeerPort,  
                       QByteArray  *m_pPayload );

        virtual QString Name   ()               { return( "Event" );   }
        virtual void    Execute( TaskQueue * );

};


#endif
