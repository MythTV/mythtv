#include <qstringlist.h>

#include <iostream>

using namespace std;

#include "playbacksock.h"
#include "programinfo.h"

#include "libmyth/mythcontext.h"
#include "libmyth/util.h"

PlaybackSock::PlaybackSock(QSocket *lsock, QString lhostname, bool wantevents)
{
    QString localhostname = gContext->GetHostName();

    sock = lsock;
    hostname = lhostname;
    events = wantevents;
    ip = "";
    backend = false;
    expectingreply = false;

    pthread_mutex_init(&sockLock, NULL);

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

void PlaybackSock::SendReceiveStringList(QStringList &strlist)
{
    pthread_mutex_lock(&sockLock);
    expectingreply = true;

    WriteStringList(sock, strlist);
    ReadStringList(sock, strlist);

    while (strlist[0] == "BACKEND_MESSAGE")
    {
        // oops, not for us
        QString message = strlist[1];
        QString extra = strlist[2];

        MythEvent me(message, extra);
        gContext->dispatch(me);

        ReadStringList(sock, strlist);
    }

    expectingreply = false;
    pthread_mutex_unlock(&sockLock);
}

void PlaybackSock::GetFreeSpace(int &totalspace, int &usedspace)
{
    QStringList strlist = QString("QUERY_FREESPACE");

    SendReceiveStringList(strlist);

    totalspace = strlist[0].toInt();
    usedspace = strlist[0].toInt();
}

void PlaybackSock::DeleteRecording(ProgramInfo *pginfo)
{
    QStringList strlist = QString("DELETE_RECORDING");
    pginfo->ToStringList(strlist);

    SendReceiveStringList(strlist);
}

void PlaybackSock::FillProgramInfo(ProgramInfo *pginfo, QString &playbackhost)
{
    QStringList strlist = QString("FILL_PROGRAM_INFO");
    strlist << playbackhost;
    pginfo->ToStringList(strlist);

    SendReceiveStringList(strlist);

    pginfo->FromStringList(strlist, 0);
}
