// -*- Mode: c++ -*-

#ifndef SOCKETHANDLER_H
#define SOCKETHANDLER_H

#include <QString>
#include <QMutex>
#include <QStringList>

#include "libmythbase/mythsocket.h"
#include "libmythbase/referencecounter.h"
#include "mythprotoserverexp.h"

class MythSocketManager;

class PROTOSERVER_PUBLIC SocketHandler : public ReferenceCounter
{
  public:
    SocketHandler(MythSocket *sock, MythSocketManager *parent,
                  QString hostname);
   ~SocketHandler() override;

    bool DoesBlockShutdown(void) const  { return m_blockShutdown; }
    bool GetsStandardEvents(void) const { return m_standardEvents; }
    bool GetsSystemEvents(void) const   { return m_systemEvents; }

    QString GetHostname(void)           { return m_hostname; }

    MythSocket *GetSocket(void)         { return m_socket; }
    MythSocketManager *GetParent(void)  { return m_parent; }

    bool WriteStringList(const QStringList &strlist);
    bool SendReceiveStringList(QStringList &strlist, uint min_reply_length=0);

    void BlockShutdown(bool block)      { m_blockShutdown = block; }
    void AllowStandardEvents(bool allow){ m_standardEvents = allow; }
    void AllowSystemEvents(bool allow)  { m_systemEvents = allow; }

  private:
    bool                m_blockShutdown  { false    };
    bool                m_standardEvents { false    };
    bool                m_systemEvents   { false    };

    MythSocket         *m_socket         { nullptr };
    MythSocketManager  *m_parent         { nullptr };

    QString             m_hostname;
};

#endif // SOCKETHANDLER_H
