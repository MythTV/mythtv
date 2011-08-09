//////////////////////////////////////////////////////////////////////////////
// Program Name: ssdp.h
// Created     : Oct. 1, 2005
//
// Purpose     : SSDP Discovery Service Implmenetation
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

#ifndef __SSDP_H__
#define __SSDP_H__

#include <QFile>

#include "upnpexp.h"
#include "mthread.h"
#include "httpserver.h"
#include "taskqueue.h"
#include "msocketdevice.h"
#include "ssdpcache.h"
#include "upnptasknotify.h"

#define SSDP_GROUP      "239.255.255.250"
#define SSDP_PORT       1900
#define SSDP_SEARCHPORT 6549

typedef enum
{
    SSDPM_Unknown         = 0,
    SSDPM_GetDeviceDesc   = 1,
    SSDPM_GetDeviceList   = 2

} SSDPMethod;

typedef enum
{
    SSDP_Unknown        = 0,
    SSDP_MSearch        = 1,
    SSDP_MSearchResp    = 2,
    SSDP_Notify         = 3

} SSDPRequestType;

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// SSDPThread Class Definition  (Singleton)
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

#define SocketIdx_Search     0
#define SocketIdx_Multicast  1
#define SocketIdx_Broadcast  2

#define NumberOfSockets     (sizeof( m_Sockets ) / sizeof( MSocketDevice * ))

class UPNP_PUBLIC SSDP : public MThread
{
    private:
        // Singleton instance used by all.
        static SSDP*        g_pSSDP;  

        QRegExp             m_procReqLineExp;
        MSocketDevice      *m_Sockets[3];

        int                 m_nPort;
        int                 m_nSearchPort;
        int                 m_nServicePort;

        UPnpNotifyTask     *m_pNotifyTask;
        bool                m_bAnnouncementsEnabled;

        bool                m_bTermRequested;
        QMutex              m_lock;

    private:

        // ------------------------------------------------------------------
        // Private so the singleton pattern can be inforced.
        // ------------------------------------------------------------------

        SSDP   ();
        
    protected:

        bool    ProcessSearchRequest ( const QStringMap &sHeaders,
                                       QHostAddress  peerAddress,
                                       quint16       peerPort );
        bool    ProcessSearchResponse( const QStringMap &sHeaders );
        bool    ProcessNotify        ( const QStringMap &sHeaders );

        bool    IsTermRequested      ();

        QString GetHeaderValue    ( const QStringMap &headers,
                                    const QString    &sKey,
                                    const QString    &sDefault );

        void    ProcessData       ( MSocketDevice *pSocket );

        SSDPRequestType ProcessRequestLine( const QString &sLine );

        virtual void run    ();
 
    public:

        static SSDP* Instance();

            ~SSDP();

        void RequestTerminate(void);

        void PerformSearch(const QString &sST, uint timeout_secs = 2);

        void EnableNotifications ( int nServicePort );
        void DisableNotifications();

        // ------------------------------------------------------------------

        static void AddListener(QObject *listener)
            { SSDPCache::Instance()->addListener(listener); }
        static void RemoveListener(QObject *listener)
            { SSDPCache::Instance()->removeListener(listener); }

        static SSDPCacheEntries *Find(const QString &sURI)
            { return SSDPCache::Instance()->Find(sURI); }
        static DeviceLocation   *Find(const QString &sURI, 
                                      const QString &sUSN)
            { return SSDPCache::Instance()->Find( sURI, sUSN ); }
};

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// SSDPExtension Class Definition
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class SSDPExtension : public HttpServerExtension
{
    private:

        QString     m_sUPnpDescPath;
        int         m_nServicePort;

    private:

        SSDPMethod GetMethod( const QString &sURI );

        void       GetDeviceDesc( HTTPRequest *pRequest );
        void       GetFile      ( HTTPRequest *pRequest, QString sFileName );
        void       GetDeviceList( HTTPRequest *pRequest );

    public:
                 SSDPExtension( int nServicePort, const QString sSharePath);
        virtual ~SSDPExtension( );

        virtual QStringList GetBasePaths();
        
        bool     ProcessRequest( HTTPRequest *pRequest );
};

#endif
