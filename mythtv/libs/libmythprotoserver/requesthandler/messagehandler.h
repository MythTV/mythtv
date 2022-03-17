#ifndef MESSAGEHANDLER_H
#define MESSAGEHANDLER_H

#include <QEvent>
#include <QStringList>

#include "libmythprotoserver/sockethandler.h"
#include "libmythprotoserver/socketrequesthandler.h"

class PROTOSERVER_PUBLIC MessageHandler : public SocketRequestHandler
{
    Q_OBJECT
  public:
    MessageHandler(void);
    bool HandleQuery(SocketHandler *socket, QStringList &commands,
                      QStringList &slist) override; // SocketRequestHandler
    QString GetHandlerName(void) override // SocketRequestHandler
        { return "MESSAGE"; }
    void customEvent(QEvent *e) override; // QObject

  private:
    static bool HandleInbound(SocketHandler *sock, QStringList &slist);
    static bool HandleOutbound(SocketHandler *sock, QStringList &slist);
};

#endif // MESSAGEHANDLER_H
