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

#include <chrono>
using namespace std::chrono_literals;

// Qt headers
#include <QHostAddress>
#include <QString>
#include <QUdpSocket>

#include "libmythupnp/taskqueue.h"
#include "libmythupnp/upnpdevice.h"

class UPnpSearchTask : public Task
{
    protected:
        int                     m_nServicePort;
        std::chrono::seconds    m_nMaxAge      {1h};

        QHostAddress    m_peerAddress;
        int             m_nPeerPort;
        QString         m_sST; 
        QString         m_sUDN;

        // Destructor protected to force use of Release Method
        ~UPnpSearchTask() override = default;

        void ProcessDevice(QUdpSocket& socket, const UPnpDevice& device);
        void SendMsg(QUdpSocket& socket, const QString& sST, const QString& sUDN);

    public:
        UPnpSearchTask( int          nServicePort,
                        QHostAddress peerAddress,
                        int          nPeerPort,  
                        QString      sST, 
                        QString      sUDN );

        QString Name() override { return( "Search" ); } // Task
        void Execute( TaskQueue *pQueue ) override; // Task
};

#endif // UPNPTASKSEARCH_H
