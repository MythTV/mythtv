//////////////////////////////////////////////////////////////////////////////
// Program Name: ssdp.h
// Created     : Oct. 1, 2005
//
// Purpose     : SSDP Discovery Service Implmenetation
//
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef SSDP_H
#define SSDP_H

#include <array>

#include <QHostAddress>
#include <QObject>
#include <QRegularExpression>
#include <QString>

#include "libmythbase/mthread.h"

#include "upnpexp.h"
#include "httprequest.h"
#include "httpserver.h"
#include "msocketdevice.h"
#include "ssdpcache.h"
#include "upnptasknotify.h"

static constexpr const char* SSDP_GROUP { "239.255.255.250" };
static constexpr uint16_t SSDP_PORT       { 1900 };
static constexpr uint16_t SSDP_SEARCHPORT { 6549 };

enum SSDPMethod : std::uint8_t
{
    SSDPM_Unknown         = 0,
    SSDPM_GetDeviceDesc   = 1,
    SSDPM_GetDeviceList   = 2
};

enum SSDPRequestType : std::uint8_t
{
    SSDP_Unknown        = 0,
    SSDP_MSearch        = 1,
    SSDP_MSearchResp    = 2,
    SSDP_Notify         = 3
};

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// SSDPThread Class Definition  (Singleton)
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

enum SocketIdxType : std::uint8_t
{
    SocketIdx_Search     = 0,
    SocketIdx_Multicast  = 1,
    SocketIdx_Broadcast  = 2
};

class UPNP_PUBLIC SSDP : public MThread
{
    private:
        // Singleton instance used by all.
        static SSDP*        g_pSSDP;  

        QRegularExpression  m_procReqLineExp        {"\\s+"};
        constexpr static int kNumberOfSockets = 3;
        std::array<MSocketDevice*,kNumberOfSockets> m_sockets {nullptr,nullptr,nullptr};

        int                 m_nPort                 {SSDP_PORT};
        int                 m_nSearchPort           {SSDP_SEARCHPORT};
        int                 m_nServicePort          {0};

        UPnpNotifyTask     *m_pNotifyTask           {nullptr};
        bool                m_bAnnouncementsEnabled {false};

        bool                m_bTermRequested        {false};
        QMutex              m_lock;

    private:

        // ------------------------------------------------------------------
        // Private so the singleton pattern can be enforced.
        // ------------------------------------------------------------------

        SSDP   ();
        
    protected:

        bool    ProcessSearchRequest ( const QStringMap &sHeaders,
                                       const QHostAddress&  peerAddress,
                                       quint16       peerPort ) const;
        static bool    ProcessSearchResponse( const QStringMap &sHeaders );
        static bool    ProcessNotify        ( const QStringMap &sHeaders );

        bool    IsTermRequested      ();

        static QString GetHeaderValue    ( const QStringMap &headers,
                                    const QString    &sKey,
                                    const QString    &sDefault );

        void    ProcessData       ( MSocketDevice *pSocket );

        SSDPRequestType ProcessRequestLine( const QString &sLine );

        void    run() override; // MThread
 
    public:

        static inline const QString kBackendURI = "urn:schemas-mythtv-org:device:MasterMediaServer:1";

        static SSDP* Instance();
        static void Shutdown();

            ~SSDP() override;

        void RequestTerminate(void);

        void PerformSearch(const QString &sST, std::chrono::seconds timeout = 2s);

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

        static SSDPMethod GetMethod( const QString &sURI );

        void       GetDeviceDesc( HTTPRequest *pRequest ) const;
        void       GetFile      ( HTTPRequest *pRequest, const QString& sFileName );
        static void       GetDeviceList( HTTPRequest *pRequest );

    public:
                 SSDPExtension( int nServicePort, const QString &sSharePath);
        ~SSDPExtension( ) override = default;

        QStringList GetBasePaths() override; // HttpServerExtension
        
        bool     ProcessRequest( HTTPRequest *pRequest ) override; // HttpServerExtension
};

#endif // SSDP_H
