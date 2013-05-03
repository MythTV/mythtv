#ifndef _MESSAGEHANDLER_H_
#define _MESSAGEHANDLER_H_

#include <QEvent>
#include <QStringList>

#include "socketrequesthandler.h"
#include "sockethandler.h"

class PROTOSERVER_PUBLIC MessageHandler : public SocketRequestHandler
{
    Q_OBJECT
  public:
    MessageHandler(void);
    bool HandleQuery(SocketHandler *socket, QStringList &commands,
                      QStringList &slist);
    QString GetHandlerName(void)                { return "MESSAGE"; }
    void customEvent(QEvent *e);

  private:
    bool HandleInbound(SocketHandler *sock, QStringList &slist);
    bool HandleOutbound(SocketHandler *sock, QStringList &slist);
};

#endif
