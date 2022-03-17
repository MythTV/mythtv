#ifndef SOCKETREQUESTHANDLER_H
#define SOCKETREQUESTHANDLER_H

#include <QString>

#include "libmythbase/mythsocket.h"
#include "mythprotoserverexp.h"
#include "sockethandler.h"

class MythSocketManager;

class PROTOSERVER_PUBLIC SocketRequestHandler : public QObject
{
    Q_OBJECT
  public:
    SocketRequestHandler() = default;
   ~SocketRequestHandler() override = default;

    virtual bool HandleAnnounce(MythSocket */*socket*/, QStringList &/*commands*/,
                                QStringList &/*slist*/)
                    { return false; }
    virtual bool HandleQuery(SocketHandler */*socket*/, QStringList &/*commands*/,
                             QStringList &/*slist*/)
                    { return false; }
    virtual QString GetHandlerName(void)                { return "BASE"; }
    virtual void connectionAnnounced(MythSocket */*socket*/, QStringList &/*commands*/,
                                     QStringList &/*slist*/)   { }
    virtual void connectionClosed(MythSocket */*socket*/)   { }
    virtual void SetParent(MythSocketManager *parent)   { m_parent = parent; }
    MythSocketManager *GetParent(void)                  { return m_parent; }

  protected:
    MythSocketManager *m_parent { nullptr };
};

#endif // SOCKETREQUESTHANDLER_H
