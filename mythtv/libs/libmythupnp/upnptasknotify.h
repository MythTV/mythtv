//////////////////////////////////////////////////////////////////////////////
// Program Name: upnptasknotify.h
// Created     : Oct. 24, 2005
//
// Purpose     : UPnp Task to send Notification messages
//
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef UPNPTASKNOTIFY_H
#define UPNPTASKNOTIFY_H

#include <chrono>
#include <cstdint>

// Qt headers
#include <QString>
#include <QMutex>

#include "taskqueue.h"

class MSocketDevice;
class UPnpDevice;

/////////////////////////////////////////////////////////////////////////////
// Typedefs
/////////////////////////////////////////////////////////////////////////////

enum UPnpNotifyNTS : std::uint8_t
{
    NTS_alive   = 0,
    NTS_byebye  = 1
};

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// UPnpNotifyTask Class Definition
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class UPnpNotifyTask : public Task
{
    protected:

        QMutex          m_mutex;

        QString         m_sMasterIP;
        int             m_nServicePort;
        std::chrono::seconds m_nMaxAge  {1h};

        UPnpNotifyNTS   m_eNTS          {NTS_alive};

    protected:

        // Destructor protected to force use of Release Method

        ~UPnpNotifyTask() override = default;

        void     ProcessDevice( MSocketDevice *pSocket, UPnpDevice *pDevice );
        void     SendNotifyMsg( MSocketDevice *pSocket, const QString& sNT, const QString& sUDN );

    public:

        explicit UPnpNotifyTask( int nServicePort );

        QString Name() override { return( "Notify" ); } // Task
        void Execute( TaskQueue *pQueue ) override; // Task

        // ------------------------------------------------------------------

        QString GetNTSString()
        {
            m_mutex.lock();
            UPnpNotifyNTS nts = m_eNTS;
            m_mutex.unlock();

            switch( nts )
            {
                case NTS_alive : return( "ssdp:alive"  );
                case NTS_byebye: return( "ssdp:byebye" );
            }
            return( "unknown" );
        }

        // ------------------------------------------------------------------

        UPnpNotifyNTS GetNTS()
        {
            m_mutex.lock();
            UPnpNotifyNTS nts = m_eNTS;
            m_mutex.unlock();

            return( nts );
        }

        // ------------------------------------------------------------------

        void SetNTS( UPnpNotifyNTS nts)
        {
            m_mutex.lock();
            m_eNTS = nts;
            m_mutex.unlock();
        }

};


#endif // UPNPTASKNOTIFY_H
