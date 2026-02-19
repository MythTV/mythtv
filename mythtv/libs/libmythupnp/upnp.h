//////////////////////////////////////////////////////////////////////////////
// Program Name: upnp.h
// Created     : Oct. 24, 2005
//
// Purpose     : UPnp Main Class
//                                                                            
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef UPNP_H
#define UPNP_H

// Qt
#include <QObject>

// MythTV
#include "libmythbase/mythpower.h"

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

#include "upnpdevice.h"
#include "httpserver.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC UPnp : public QObject
{
    Q_OBJECT

    protected:

        HttpServer             *m_pHttpServer  {nullptr};
        int                     m_nServicePort {0};

    public:

        static UPnpDeviceDesc       g_UPnpDeviceDesc;
        static QList<QHostAddress>  g_IPAddrList;

    public:
        UPnp();
       ~UPnp() override;

        bool Initialize( int nServicePort, HttpServer *pHttpServer );
        bool Initialize( QList<QHostAddress> &sIPAddrList, int nServicePort,
                         HttpServer *pHttpServer );

        bool isInitialized() { return (m_pHttpServer != nullptr); }

        virtual void Start();

        static void CleanUp      ();

        static UPnpDevice *RootDevice() { return &(g_UPnpDeviceDesc.m_rootDevice); }

        HttpServer *GetHttpServer() { return m_pHttpServer; }

        static UPnpDeviceDesc *GetDeviceDesc( QString &sURL );

    public slots:
        static void DisableNotifications(std::chrono::milliseconds /*unused*/);
        void EnableNotificatins(std::chrono::milliseconds /*unused*/) const;

    private:
        MythPower* m_power;
};

#endif // UPNP_H
