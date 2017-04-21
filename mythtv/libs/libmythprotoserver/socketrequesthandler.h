#ifndef _SOCKETREQUESTHANDLER_H_
#define _SOCKETREQUESTHANDLER_H_

#include <QString>

#include "mythsocket.h"
#include "mythprotoserverexp.h"
#include "sockethandler.h"

class MythSocketManager;

class PROTOSERVER_PUBLIC SocketRequestHandler : public QObject
{
    Q_OBJECT
  public:
    SocketRequestHandler() : m_parent(NULL) {};
   ~SocketRequestHandler() {};

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
    MythSocketManager *m_parent;
};

#endif
