
using namespace std;

#include <QTimer>
#include <QString>
#include <QStringList>

#include "mythsocket.h"
#include "mythsocketmanager.h"
#include "socketrequesthandler.h"
#include "sockethandler.h"
#include "mythprotoserverexp.h"

class MainServer;

class PROTOSERVER_PUBLIC OutboundRequestHandler : public SocketRequestHandler
{
    Q_OBJECT
  public:
    OutboundRequestHandler(void);

    QString GetHandlerName(void)                    { return "OUTBOUND"; }
    virtual bool AnnounceSocket(void)               { return false; }
    void connectionClosed(MythSocket *socket);

  public slots:
    void ConnectToMaster(void);

  private:
    bool DoConnectToMaster(void);

    MythSocket *m_socket;
    QTimer      m_timer;
};


