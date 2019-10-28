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
    static bool HandleQueryLoad(SocketHandler *sock);
    static bool HandleQueryUptime(SocketHandler *sock);
    static bool HandleQueryHostname(SocketHandler *sock);
    static bool HandleQueryMemStats(SocketHandler *sock);
    static bool HandleQueryTimeZone(SocketHandler *sock);
};

#endif
