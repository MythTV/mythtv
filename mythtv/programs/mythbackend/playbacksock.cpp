#include <unistd.h>
#include <iostream>

using namespace std;

#include "playbacksock.h"

#include "libmyth/mythcontext.h"

PlaybackSock::PlaybackSock(QSocket *lsock, QString lhostname, bool wantevents)
{
    QString localhostname = gContext->GetHostName();

    sock = lsock;
    hostname = lhostname;
    events = wantevents;

    if (hostname == localhostname)
        local = true;
    else
        local = false;
}

PlaybackSock::~PlaybackSock()
{
    if (sock)
        delete sock;
}


