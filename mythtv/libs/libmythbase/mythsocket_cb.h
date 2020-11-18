/** -*- Mode: c++ -*- */
#ifndef MYTHSOCKET_CB_H
#define MYTHSOCKET_CB_H

#include "mythbaseexp.h"

using namespace std::chrono_literals;

static constexpr std::chrono::milliseconds kMythSocketShortTimeout {  7s };
static constexpr std::chrono::milliseconds kMythSocketLongTimeout  { 30s };

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
