#ifndef BASESOCKETREQUESTHANDLER_H
#define BASESOCKETREQUESTHANDLER_H

#include <QString>
#include <QStringList>

#include "libmythbase/mythsocket.h"
#include "libmythprotoserver/mythprotoserverexp.h"
#include "libmythprotoserver/mythsocketmanager.h"
#include "libmythprotoserver/sockethandler.h"
#include "libmythprotoserver/socketrequesthandler.h"

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
    static bool HandleQueryLoad(SocketHandler *sock);
    static bool HandleQueryUptime(SocketHandler *sock);
    static bool HandleQueryHostname(SocketHandler *sock);
    static bool HandleQueryMemStats(SocketHandler *sock);
    static bool HandleQueryTimeZone(SocketHandler *sock);
};

#endif // BASESOCKETREQUESTHANDLER_H
