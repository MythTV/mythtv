//////////////////////////////////////////////////////////////////////////////
// Program Name: upnptasknotify.h
//                                                                            
// Purpose - UPnp Task to send Notification messages
//                                                                            
// Created By  : David Blain                    Created On : Oct. 24, 2005
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#ifndef __UPNPTASKNOTIFY_H__
#define __UPNPTASKNOTIFY_H__

#include <qsocketdevice.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "upnp.h"

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
        int             m_nStatusPort;
        int             m_nMaxAge;    

        UPnpNotifyNTS   m_eNTS;
        
        QStringList     m_addressList;


    protected:

        // Destructor protected to force use of Release Method

        virtual ~UPnpNotifyTask();

        void     ProcessDevice( QMulticastSocket *pSocket, UPnpDevice *pDevice );
        void     SendNotifyMsg( QMulticastSocket *pSocket, QString sNT, QString sUDN );

    public:

        UPnpNotifyTask();

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
