#include <qstringlist.h>

#include <iostream>

using namespace std;

#include "playbacksock.h"
#include "programinfo.h"
#include "server.h"

#include "libmyth/mythcontext.h"
#include "libmyth/util.h"

PlaybackSock::PlaybackSock(RefSocket *lsock, QString lhostname, bool wantevents)
{
    QString localhostname = gContext->GetHostName();

    sock = lsock;
    hostname = lhostname;
    events = wantevents;
    ip = "";
    backend = false;
    expectingreply = false;

    if (hostname == localhostname)
        local = true;
    else
        local = false;
}

PlaybackSock::~PlaybackSock()
{
    sock->DownRef();
}

bool PlaybackSock::SendReceiveStringList(QStringList &strlist)
{
    sock->Lock();
    sock->UpRef();

    sockLock.lock();
    expectingreply = true;

    WriteStringList(sock, strlist);
    bool ok = ReadStringList(sock, strlist);

    while (ok && strlist[0] == "BACKEND_MESSAGE")
    {
        // oops, not for us
        QString message = strlist[1];
        QString extra = strlist[2];

        MythEvent me(message, extra);
        gContext->dispatch(me);

        ok = ReadStringList(sock, strlist);
    }

    expectingreply = false;
    sockLock.unlock();

    sock->Unlock();

    sock->DownRef();

    return ok;
}

void PlaybackSock::GetFreeSpace(int &totalspace, int &usedspace)
{
    QStringList strlist = QString("QUERY_FREESPACE");

    SendReceiveStringList(strlist);

    totalspace = strlist[0].toInt();
    usedspace = strlist[1].toInt();
}

int PlaybackSock::CheckRecordingActive(ProgramInfo *pginfo)
{
    QStringList strlist = QString("CHECK_RECORDING");
    pginfo->ToStringList(strlist);

    SendReceiveStringList(strlist);

    return strlist[0].toInt();
}

int PlaybackSock::StopRecording(ProgramInfo *pginfo)
{
    QStringList strlist = QString("STOP_RECORDING");
    pginfo->ToStringList(strlist);

    SendReceiveStringList(strlist);

    return strlist[0].toInt();
}

int PlaybackSock::DeleteRecording(ProgramInfo *pginfo, bool forceMetadataDelete)
{
    QStringList strlist;

    if (forceMetadataDelete)
        strlist = QString("FORCE_DELETE_RECORDING");
    else
        strlist = QString("DELETE_RECORDING");

    pginfo->ToStringList(strlist);

    SendReceiveStringList(strlist);

    return strlist[0].toInt();
}

void PlaybackSock::FillProgramInfo(ProgramInfo *pginfo, QString &playbackhost)
{
    QStringList strlist = QString("FILL_PROGRAM_INFO");
    strlist << playbackhost;
    pginfo->ToStringList(strlist);

    SendReceiveStringList(strlist);

    pginfo->FromStringList(strlist, 0);
}

void PlaybackSock::GenPreviewPixmap(ProgramInfo *pginfo)
{
    QStringList strlist = QString("QUERY_GENPIXMAP");
    pginfo->ToStringList(strlist);

    SendReceiveStringList(strlist);
}

bool PlaybackSock::CheckFile(ProgramInfo *pginfo)
{
    QStringList strlist = "QUERY_CHECKFILE";
    pginfo->ToStringList(strlist);

    SendReceiveStringList(strlist);

    bool exists = strlist[0].toInt();
    return exists;
}

bool PlaybackSock::IsBusy(int capturecardnum)
{
    QStringList strlist = QString("QUERY_REMOTEENCODER %1").arg(capturecardnum);

    strlist << "IS_BUSY";

    SendReceiveStringList(strlist);

    bool state = strlist[0].toInt();
    return state;
}

int PlaybackSock::GetEncoderState(int capturecardnum)
{
    QStringList strlist = QString("QUERY_REMOTEENCODER %1").arg(capturecardnum);
    strlist << "GET_STATE";

    SendReceiveStringList(strlist);

    int state = strlist[0].toInt();
    return state;
}

bool PlaybackSock::EncoderIsRecording(int capturecardnum, ProgramInfo *pginfo)
{
    QStringList strlist = QString("QUERY_REMOTEENCODER %1").arg(capturecardnum);
    strlist << "MATCHES_RECORDING";
    pginfo->ToStringList(strlist);

    SendReceiveStringList(strlist);

    bool ret = strlist[0].toInt();
    return ret;
}

int PlaybackSock::StartRecording(int capturecardnum, ProgramInfo *pginfo)
{
    QStringList strlist = QString("QUERY_REMOTEENCODER %1").arg(capturecardnum);
    strlist << "START_RECORDING";
    pginfo->ToStringList(strlist);

    SendReceiveStringList(strlist);

    int ret = strlist[0].toInt();
    return ret;
}

void PlaybackSock::RecordPending(int capturecardnum, ProgramInfo *pginfo,
                                 int secsleft)
{
    QStringList strlist = QString("QUERY_REMOTEENCODER %1").arg(capturecardnum);
    strlist << "RECORD_PENDING";
    strlist << QString::number(secsleft);
    pginfo->ToStringList(strlist);

    SendReceiveStringList(strlist);
}

