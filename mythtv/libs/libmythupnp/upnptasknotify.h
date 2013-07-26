//////////////////////////////////////////////////////////////////////////////
// Program Name: upnptasknotify.h
// Created     : Oct. 24, 2005
//
// Purpose     : UPnp Task to send Notification messages
//                                                                            
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details                    
//
//////////////////////////////////////////////////////////////////////////////

#ifndef __UPNPTASKNOTIFY_H__
#define __UPNPTASKNOTIFY_H__

// POSIX headers
#include <sys/types.h>
#ifndef _WIN32
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

// Qt headers
#include <QString>
#include <QMutex>

// MythTV headers
#include "compat.h"

class MSocketDevice;
class UPnpDevice;

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

        void     ProcessDevice( MSocketDevice *pSocket, UPnpDevice *pDevice );
        void     SendNotifyMsg( MSocketDevice *pSocket, QString sNT, QString sUDN );

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
