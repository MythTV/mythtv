#ifndef _CONTROLREQUESTHANDLER_H_
#define _CONTROLREQUESTHANDLER_H_

using namespace std;

#include <QString>
#include <QStringList>

#include "mythsocket.h"
#include "sockethandler.h"
#include "requesthandler/outboundhandler.h"

class ControlRequestHandler : public OutboundRequestHandler
{
  public:
    bool HandleQuery(SocketHandler *socket, QStringList &commands,
                            QStringList &slist);

  protected:
    bool AnnounceSocket(void);
};

#endif
