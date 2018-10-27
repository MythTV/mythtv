#ifndef _BASESOCKETREQUESTHANDLER_H_
#define _BASESOCKETREQUESTHANDLER_H_

#include <QString>
#include <QStringList>

#include "mythsocket.h"
#include "mythsocketmanager.h"
#include "socketrequesthandler.h"
#include "sockethandler.h"
#include "mythprotoserverexp.h"

class PROTOSERVER_PUBLIC BaseRequestHandler : public SocketRequestHandler
{
    Q_OBJECT
  public:
    bool HandleAnnounce(MythSocket *socket, QStringList &commands,
                        QStringList &slist) override; // SocketRequestHandler
    bool HandleQuery(SocketHandler *sock, QStringList &commands,
                     QStringList &slist) override; // SocketRequestHandler
    QString GetHandlerName(void) override // SocketRequestHandler
        { return "BASIC"; }

  private:
    bool HandleQueryLoad(SocketHandler *sock);
    bool HandleQueryUptime(SocketHandler *sock);
    bool HandleQueryHostname(SocketHandler *sock);
    bool HandleQueryMemStats(SocketHandler *sock);
    bool HandleQueryTimeZone(SocketHandler *sock);
};

#endif
