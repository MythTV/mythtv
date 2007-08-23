//////////////////////////////////////////////////////////////////////////////
// Program Name: upnptaskevent.h
//                                                                            
// Purpose - UPnp Task to notifing subscribers of an event
//                                                                            
// Created By  : David Blain                    Created On : Dec. 31, 2006
// Modified By :                                Modified On:                  
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

        QCString     m_sPayload;

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
