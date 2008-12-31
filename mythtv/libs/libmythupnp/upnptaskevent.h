//////////////////////////////////////////////////////////////////////////////
// Program Name: upnptaskevent.h
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
