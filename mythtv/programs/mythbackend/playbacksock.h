#ifndef PLAYBACKSOCK_H_
#define PLAYBACKSOCK_H_

#include <qsocket.h>
#include <qstring.h>

class PlaybackSock
{
  public:
    PlaybackSock(QSocket *lsock, QString lhostname, bool wantevents);
   ~PlaybackSock();

    QSocket *getSocket() { return sock; }
    QString getHostname() { return hostname; }

    bool isLocal() { return local; }
    bool wantsEvents() { return events; }

  private:
    QSocket *sock;
    QString hostname;
    bool local;
    bool events;
};

#endif
