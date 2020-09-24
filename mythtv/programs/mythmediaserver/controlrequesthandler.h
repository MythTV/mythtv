#ifndef CONTROLREQUESTHANDLER_H
#define CONTROLREQUESTHANDLER_H

#include <QString>
#include <QStringList>

#include "mythsocket.h"
#include "sockethandler.h"
#include "requesthandler/outboundhandler.h"

class ControlRequestHandler : public OutboundRequestHandler
{
  public:
    bool HandleQuery(SocketHandler *socket, QStringList &commands,
                     QStringList &slist)override; // SocketRequestHandler

  protected:
    bool AnnounceSocket(void) override; // OutboundRequestHandler
};

#endif // CONTROLREQUESTHANDLER_H
