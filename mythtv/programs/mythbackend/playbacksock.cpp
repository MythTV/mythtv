#include <qstringlist.h>

#include <iostream>

using namespace std;

#include "playbacksock.h"
#include "programinfo.h"
#include "server.h"
#include "mainserver.h"

#include "libmyth/mythcontext.h"
#include "libmyth/util.h"

PlaybackSock::PlaybackSock(MainServer *parent, RefSocket *lsock, 
                           QString lhostname, bool wantevents)
{
    m_parent = parent;
    QString localhostname = gContext->GetHostName();

    refCount = 0;

    sock = lsock;
    hostname = lhostname;
    events = wantevents;
    ip = "";
    backend = false;
    expectingreply = false;

    disconnected = false;

    if (hostname == localhostname)
        local = true;
    else
        local = false;
}

PlaybackSock::~PlaybackSock()
{
    sock->DownRef();
}

void PlaybackSock::UpRef(void)
{
    refCount++;
}

bool PlaybackSock::DownRef(void)
{
    refCount--;
    if (refCount < 0)
    {
        m_parent->DeletePBS(this);
        return true;
    }
    return false;
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

/** \fn RemoteGetFreeSpace(long long&, long long&)
 *  \brief Returns total and used space in kilobytes.
 */
void PlaybackSock::GetFreeDiskSpace(long long &totalKB, long long &usedKB)
{
    QStringList strlist = QString("QUERY_FREE_SPACE");

    SendReceiveStringList(strlist);

    totalKB = decodeLongLong(strlist, 0);
    usedKB = decodeLongLong(strlist, 2);
}

int PlaybackSock::CheckRecordingActive(const ProgramInfo *pginfo)
{
    QStringList strlist = QString("CHECK_RECORDING");
    pginfo->ToStringList(strlist);

    SendReceiveStringList(strlist);

    return strlist[0].toInt();
}

int PlaybackSock::StopRecording(const ProgramInfo *pginfo)
{
    QStringList strlist = QString("STOP_RECORDING");
    pginfo->ToStringList(strlist);

    SendReceiveStringList(strlist);

    return strlist[0].toInt();
}

int PlaybackSock::DeleteRecording(const ProgramInfo *pginfo, bool forceMetadataDelete)
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

void PlaybackSock::GenPreviewPixmap(const ProgramInfo *pginfo)
{
    QStringList strlist = QString("QUERY_GENPIXMAP");
    pginfo->ToStringList(strlist);

    SendReceiveStringList(strlist);
}

QString PlaybackSock::PixmapLastModified(const ProgramInfo *pginfo)
{
    QStringList strlist = QString("QUERY_PIXMAP_LASTMODIFIED");
    pginfo->ToStringList(strlist);

    SendReceiveStringList(strlist);

    return strlist[0];
}

bool PlaybackSock::CheckFile(const ProgramInfo *pginfo)
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

/** \fn PlaybackSock::GetEncoderState(int)
 *   Returns the maximum bits per second the recorder can produce.
 *  \param capturecardnum Recorder ID in the database.
 */
int PlaybackSock::GetEncoderState(int capturecardnum)
{
    QStringList strlist = QString("QUERY_REMOTEENCODER %1").arg(capturecardnum);
    strlist << "GET_STATE";

    SendReceiveStringList(strlist);

    int state = strlist[0].toInt();
    return state;
}

long long PlaybackSock::GetMaxBitrate(int capturecardnum)
{
    QStringList strlist = QString("QUERY_REMOTEENCODER %1").arg(capturecardnum);
    strlist << "GET_MAX_BITRATE";

    SendReceiveStringList(strlist);

    long long ret = decodeLongLong(strlist, 0);
    return ret;
}

bool PlaybackSock::EncoderIsRecording(int capturecardnum, const ProgramInfo *pginfo)
{
    QStringList strlist = QString("QUERY_REMOTEENCODER %1").arg(capturecardnum);
    strlist << "MATCHES_RECORDING";
    pginfo->ToStringList(strlist);

    SendReceiveStringList(strlist);

    bool ret = strlist[0].toInt();
    return ret;
}

int PlaybackSock::StartRecording(int capturecardnum, const ProgramInfo *pginfo)
{
    QStringList strlist = QString("QUERY_REMOTEENCODER %1").arg(capturecardnum);
    strlist << "START_RECORDING";
    pginfo->ToStringList(strlist);

    SendReceiveStringList(strlist);

    int ret = strlist[0].toInt();
    return ret;
}

void PlaybackSock::RecordPending(int capturecardnum, const ProgramInfo *pginfo,
                                 int secsleft)
{
    QStringList strlist = QString("QUERY_REMOTEENCODER %1").arg(capturecardnum);
    strlist << "RECORD_PENDING";
    strlist << QString::number(secsleft);
    pginfo->ToStringList(strlist);

    SendReceiveStringList(strlist);
}
