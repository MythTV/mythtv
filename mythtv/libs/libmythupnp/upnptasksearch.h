//////////////////////////////////////////////////////////////////////////////
// Program Name: upnptasksearch.h
// Created     : Oct. 24, 2005
//
// Purpose     : UPnp Task to handle Discovery Responses
//                                                                            
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details                    
//
//////////////////////////////////////////////////////////////////////////////

#ifndef __UPNPTASKSEARCH_H__
#define __UPNPTASKSEARCH_H__

// POSIX headers
#include <sys/types.h>
#ifndef _WIN32
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

// Qt headers
#include <QStringList>
#include <QHostAddress>

// MythTV headers
#include "upnp.h"
#include "msocketdevice.h"
#include "compat.h"

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// UPnpSearchTask Class Definition
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class UPnpSearchTask : public Task
{
    protected: 

        QStringList     m_addressList;
        int             m_nServicePort;
        int             m_nMaxAge;

        QHostAddress    m_PeerAddress;
        int             m_nPeerPort;
        QString         m_sST; 
        QString         m_sUDN;


    protected:

        // Destructor protected to force use of Release Method

        virtual ~UPnpSearchTask();

        void     ProcessDevice ( MSocketDevice *pSocket, UPnpDevice *pDevice );
        void     SendMsg       ( MSocketDevice  *pSocket,
                                 QString         sST,
                                 QString         sUDN );

    public:

        UPnpSearchTask( int          nServicePort,
                        QHostAddress peerAddress,
                        int          nPeerPort,  
                        QString      sST, 
                        QString      sUDN );

        virtual QString Name   ()               { return( "Search" );   }
        virtual void    Execute( TaskQueue * );

};


#endif
