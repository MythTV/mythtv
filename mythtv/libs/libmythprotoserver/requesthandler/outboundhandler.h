#ifndef OUTBOUNDREQUESTHANDLER_H
#define OUTBOUNDREQUESTHANDLER_H

#include <QTimer>
#include <QString>
#include <QStringList>

#include "libmythbase/mythsocket.h"
#include "libmythprotoserver/mythprotoserverexp.h"
#include "libmythprotoserver/mythsocketmanager.h"
#include "libmythprotoserver/sockethandler.h"
#include "libmythprotoserver/socketrequesthandler.h"

class MainServer;

class PROTOSERVER_PUBLIC OutboundRequestHandler : public SocketRequestHandler
{
    Q_OBJECT
  public:
    OutboundRequestHandler(void);
    QString GetHandlerName(void) override // SocketRequestHandler
        { return "OUTBOUND"; }
    void connectionClosed(MythSocket *socket) override; // SocketRequestHandler

  public slots:
    void ConnectToMaster(void);

  protected:
    virtual bool AnnounceSocket(void)               { return false; }
    MythSocket *m_socket { nullptr };

  private:
    bool DoConnectToMaster(void);
    QTimer      m_timer;
};

#endif // OUTBOUNDREQUESTHANDLER_H
