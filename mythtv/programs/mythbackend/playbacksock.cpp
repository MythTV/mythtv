#include <unistd.h>
#include <iostream>

using namespace std;

#include "playbacksock.h"

PlaybackSock::PlaybackSock(QSocket *lsock, QString lhostname)
{
    char localhostname[256];
    if (gethostname(localhostname, 256))
    {
        cerr << "Error getting local hostname\n";
        exit(0);
    }

    sock = lsock;
    hostname = lhostname;

    if (hostname == localhostname)
        local = true;
    else
        local = false;
}

PlaybackSock::~PlaybackSock()
{
}


