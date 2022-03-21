#ifndef CONTROLREQUESTHANDLER_H
#define CONTROLREQUESTHANDLER_H

// Qt
#include <QString>
#include <QStringList>

// MythTV
#include "libmythbase/mythsocket.h"
#include "libmythprotoserver/requesthandler/outboundhandler.h"
#include "libmythprotoserver/sockethandler.h"

class ControlRequestHandler : public OutboundRequestHandler
{
  public:
    bool HandleQuery(SocketHandler *socket, QStringList &commands,
                     QStringList &slist)override; // SocketRequestHandler

  protected:
    bool AnnounceSocket(void) override; // OutboundRequestHandler
};

#endif // CONTROLREQUESTHANDLER_H
