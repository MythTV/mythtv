//////////////////////////////////////////////////////////////////////////////
// Program Name: upnptasksearch.h
// Created     : Oct. 24, 2005
//
// Purpose     : UPnp Task to handle Discovery Responses
//                                                                            
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef UPNPTASKSEARCH_H
#define UPNPTASKSEARCH_H

// POSIX headers
#include <sys/types.h>
#ifndef _WIN32
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

// Qt headers
#include <QList>
#include <QHostAddress>

// MythTV headers
#include "libmythbase/compat.h"

#include "libmythupnp/msocketdevice.h"
#include "libmythupnp/upnp.h"

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

        QList<QHostAddress>     m_addressList;
        int                     m_nServicePort;
        std::chrono::seconds    m_nMaxAge      {1h};

        QHostAddress    m_peerAddress;
        int             m_nPeerPort;
        QString         m_sST; 
        QString         m_sUDN;


    protected:

        // Destructor protected to force use of Release Method

        ~UPnpSearchTask() override = default;

        void     ProcessDevice ( MSocketDevice *pSocket, UPnpDevice *pDevice );
        void     SendMsg       ( MSocketDevice  *pSocket,
                                 const QString&  sST,
                                 const QString&  sUDN );

    public:

        UPnpSearchTask( int          nServicePort,
                        const QHostAddress& peerAddress,
                        int          nPeerPort,  
                        QString      sST, 
                        QString      sUDN );

        QString Name() override { return( "Search" ); } // Task
        void Execute( TaskQueue *pQueue ) override; // Task

};


#endif // UPNPTASKSEARCH_H
