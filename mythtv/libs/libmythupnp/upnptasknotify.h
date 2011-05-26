//////////////////////////////////////////////////////////////////////////////
// Program Name: upnptasknotify.h
// Created     : Oct. 24, 2005
//
// Purpose     : UPnp Task to send Notification messages
//                                                                            
// Copyright (c) 2005 David Blain <mythtv@theblains.net>
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

#ifndef __UPNPTASKNOTIFY_H__
#define __UPNPTASKNOTIFY_H__

// POSIX headers
#include <sys/types.h>
#ifndef USING_MINGW
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

// Qt headers
#include <QString>
#include <QMutex>

// MythTV headers
#include <msocketdevice.h>
#include "multicast.h"
#include "compat.h"

/////////////////////////////////////////////////////////////////////////////
// Typedefs
/////////////////////////////////////////////////////////////////////////////

typedef enum 
{
    NTS_alive   = 0,
    NTS_byebye  = 1

} UPnpNotifyNTS;

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
        int             m_nMaxAge;    

        UPnpNotifyNTS   m_eNTS;

    protected:

        // Destructor protected to force use of Release Method

        virtual ~UPnpNotifyTask();

        void     ProcessDevice( QMulticastSocket *pSocket, UPnpDevice *pDevice );
        void     SendNotifyMsg( QMulticastSocket *pSocket, QString sNT, QString sUDN );

    public:

        UPnpNotifyTask( int nServicePort );

        virtual QString Name   ()               { return( "Notify" );   }
        virtual void    Execute( TaskQueue * );

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


#endif
