/** -*- Mode: c++ -*- */
#ifndef MYTHSOCKET_CB_H
#define MYTHSOCKET_CB_H

#include "mythbaseexp.h"

#define kMythSocketShortTimeout  7000
#define kMythSocketLongTimeout  30000

class MythSocket;
class MBASE_PUBLIC MythSocketCBs
{
  public:
    virtual ~MythSocketCBs() = default;
    virtual void connected(MythSocket*) = 0;
    virtual void error(MythSocket */*socket*/, int /*err*/) {}
    virtual void readyRead(MythSocket*) = 0;
    virtual void connectionFailed(MythSocket*) = 0;
    virtual void connectionClosed(MythSocket*) = 0;
};

#endif // MYTHSOCKET_CB_H
