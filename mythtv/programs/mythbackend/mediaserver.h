//////////////////////////////////////////////////////////////////////////////
// Program Name: mediaserver.h
//
// Purpose - uPnp Media Server main Class
//
// Created By  : David Blain                    Created On : Jan. 15, 2007
// Modified By :                                Modified On:
//
//////////////////////////////////////////////////////////////////////////////

#ifndef MEDIASERVER_H
#define MEDIASERVER_H

#include <QString>

#include "libmythupnp/upnp.h"
#include "libmythupnp/upnpcds.h"
#include "libmythupnp/upnpcmgr.h"
#include "libmythupnp/upnpmsrr.h"
#include "libmythupnp/websocket.h"

class BonjourRegister;

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

class MediaServer : public UPnp
{
    private:

#ifdef USING_LIBDNS_SD
    BonjourRegister     *m_bonjour   {nullptr};
#endif
    WebSocketServer     *m_webSocketServer {nullptr};

    protected:

        UPnpCDS         *m_pUPnpCDS  {nullptr};    // Do not delete (auto deleted)
        UPnpCMGR        *m_pUPnpCMGR {nullptr};    // Do not delete (auto deleted)

        QString          m_sSharePath;

    public:
        explicit MediaServer();
        void Init(bool bIsMaster, bool bDisableUPnp = false);

        ~MediaServer() override;

        void     RegisterExtension  ( UPnpCDSExtension    *pExtension );
        void     UnregisterExtension( UPnpCDSExtension    *pExtension );

};

#endif // MEDIASERVER_H
