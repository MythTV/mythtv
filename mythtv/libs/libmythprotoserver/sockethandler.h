// -*- Mode: c++ -*-

#ifndef _SOCKETHANDLER_H_
#define _SOCKETHANDLER_H_

#include <QString>
#include <QMutex>
#include <QStringList>

#include "mythsocket.h"
#include "referencecounter.h"
#include "mythprotoserverexp.h"

class MythSocketManager;

class PROTOSERVER_PUBLIC SocketHandler : public ReferenceCounter
{
  public:
    SocketHandler(MythSocket *sock, MythSocketManager *parent,
                  QString hostname);
   ~SocketHandler();

    bool DoesBlockShutdown(void)        { return m_blockShutdown; }
    bool GetsStandardEvents(void)       { return m_standardEvents; }
    bool GetsSystemEvents(void)         { return m_systemEvents; }

    QString GetHostname(void)           { return m_hostname; }

    MythSocket *GetSocket(void)         { return m_socket; }
    MythSocketManager *GetParent(void)  { return m_parent; }

    bool WriteStringList(const QStringList &strlist);
    bool SendReceiveStringList(QStringList &strlist, uint min_reply_length=0);

    void BlockShutdown(bool block)      { m_blockShutdown = block; }
    void AllowStandardEvents(bool allow){ m_standardEvents = allow; }
    void AllowSystemEvents(bool allow)  { m_systemEvents = allow; }

  private:
    bool                m_blockShutdown;
    bool                m_standardEvents;
    bool                m_systemEvents;

    MythSocket         *m_socket;
    MythSocketManager  *m_parent;

    QString             m_hostname;
};

#endif
