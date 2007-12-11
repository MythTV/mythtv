//////////////////////////////////////////////////////////////////////////////
// Program Name: upnptasksearch.h
//                                                                            
// Purpose - UPnp Task to handle Discovery Responses
//                                                                            
// Created By  : David Blain                    Created On : Oct. 24, 2005
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#ifndef __UPNPTASKSEARCH_H__
#define __UPNPTASKSEARCH_H__

// POSIX headers
#include <sys/types.h>
#ifndef USING_MINGW
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

// Qt headers
#include <qsocketdevice.h>

// MythTV headers
#include "upnp.h"
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

        void     ProcessDevice ( QSocketDevice *pSocket, UPnpDevice *pDevice );
        void     SendMsg       ( QSocketDevice  *pSocket, 
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
