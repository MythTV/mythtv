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

#include <chrono>
using namespace std::chrono_literals;
#include <cstdint>

#include <QHostAddress>
#include <QString>
#include <QUdpSocket>

#include "upnpexp.h"

static constexpr const char* SSDP_GROUP { "239.255.255.250" };
static constexpr uint16_t SSDP_PORT       { 1900 };

enum SSDPRequestType : std::uint8_t
{
    SSDP_Unknown        = 0,
    SSDP_MSearch        = 1,
    SSDP_MSearchResp    = 2,
    SSDP_Notify         = 3
};

class SSDPReceiver : public QObject
{
    Q_OBJECT

  public:
    SSDPReceiver();

  private slots:
    void processPendingDatagrams();

  private:
    QUdpSocket          m_socket;
    const uint16_t      m_port          {SSDP_PORT};
    const QHostAddress  m_groupAddress  {SSDP_GROUP};
};

class UPNP_PUBLIC SSDP
{
    private:
        // Singleton instance used by all.
        static SSDP*        g_pSSDP;  

        int                 m_nServicePort          {0};

        class UPnpNotifyTask* m_pNotifyTask         {nullptr};

        SSDPReceiver m_receiver;

    private:

        // ------------------------------------------------------------------
        // Private so the singleton pattern can be enforced.
        // ------------------------------------------------------------------

        SSDP   ();

    public:

        static inline const QString kBackendURI = "urn:schemas-mythtv-org:device:MasterMediaServer:1";

        static SSDP* Instance();
        static void Shutdown();

        ~SSDP();

        /** @brief Send a SSDP discover multicast datagram.
        @note This needs an SSDPReceiver instance to process the replies and add to the SSDPCache.
        */
        static void PerformSearch(const QString &sST, std::chrono::seconds timeout = 2s);

        void EnableNotifications ( int nServicePort );
        void DisableNotifications();
        int getNotificationPort() const
        {
            if (m_pNotifyTask != nullptr)
            {
                return m_nServicePort;
            }
            return 0;
        }
};

#endif // SSDP_H
