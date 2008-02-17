#include <qstringlist.h>

#include <iostream>

using namespace std;

#include "playbacksock.h"
#include "programinfo.h"
#include "server.h"
#include "mainserver.h"

#include "libmyth/mythcontext.h"
#include "libmyth/util.h"
#include "libmythtv/inputinfo.h"

PlaybackSock::PlaybackSock(MainServer *parent, MythSocket *lsock, 
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
    blockshutdown = true;

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
    QMutexLocker locker(&refLock);
    refCount++;
}

bool PlaybackSock::DownRef(void)
{
    QMutexLocker locker(&refLock);

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

    sock->writeStringList(strlist);
    bool ok = sock->readStringList(strlist);

    while (ok && strlist[0] == "BACKEND_MESSAGE")
    {
        // oops, not for us
        QString message = strlist[1];
        strlist.pop_front(); strlist.pop_front();

        MythEvent me(message, strlist);
        gContext->dispatch(me);

        ok = sock->readStringList(strlist);
    }

    expectingreply = false;
    sockLock.unlock();

    sock->Unlock();
    sock->DownRef();

    return ok;
}

/**
 *  \brief Appends host's dir's total and used space in kilobytes.
 */
void PlaybackSock::GetDiskSpace(QStringList &o_strlist)
{
    QStringList strlist = QString("QUERY_FREE_SPACE");

    SendReceiveStringList(strlist);

    o_strlist += strlist;

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

QStringList PlaybackSock::GenPreviewPixmap(const ProgramInfo *pginfo)
{
    QStringList strlist = QString("QUERY_GENPIXMAP");
    pginfo->ToStringList(strlist);

    SendReceiveStringList(strlist);

    return strlist;
}

QStringList PlaybackSock::GenPreviewPixmap(const ProgramInfo *pginfo,
                                           bool               time_fmt_sec,
                                           long long          time,
                                           const QString     &outputFile,
                                           const QSize       &outputSize)
{
    QStringList strlist = "QUERY_GENPIXMAP";
    pginfo->ToStringList(strlist);
    strlist.push_back(time_fmt_sec ? "s" : "f");
    encodeLongLong(strlist, time);
    strlist.push_back((outputFile.isEmpty()) ? "<EMPTY>" : outputFile);
    strlist.push_back(QString::number(outputSize.width()));
    strlist.push_back(QString::number(outputSize.height()));

    SendReceiveStringList(strlist);

    return strlist;
}

QDateTime PlaybackSock::PixmapLastModified(const ProgramInfo *pginfo)
{
    QStringList strlist = QString("QUERY_PIXMAP_LASTMODIFIED");
    pginfo->ToStringList(strlist);

    SendReceiveStringList(strlist);

    QDateTime datetime;
    if (!strlist.empty() && strlist[0] != "BAD")
    {
        uint timet = strlist[0].toUInt();
        datetime.setTime_t(timet);
    }
    return datetime;
}

bool PlaybackSock::CheckFile(ProgramInfo *pginfo)
{
    QStringList strlist = "QUERY_CHECKFILE";
    strlist << QString::number(0); // don't check slaves
    pginfo->ToStringList(strlist);

    SendReceiveStringList(strlist);

    bool exists = strlist[0].toInt();
    pginfo->pathname = strlist[1];
    return exists;
}

bool PlaybackSock::IsBusy(
    int capturecardnum, InputInfo *busy_input, int time_buffer)
{
    QStringList strlist = QString("QUERY_REMOTEENCODER %1").arg(capturecardnum);

    strlist << "IS_BUSY";
    strlist << QString::number(time_buffer);

    SendReceiveStringList(strlist);

    QStringList::const_iterator it = strlist.begin();
    bool state = (*it).toInt();
    if (busy_input)
    {
        it++;
        busy_input->FromStringList(it, strlist.end());
    }

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

/** \fn *PlaybackSock::GetRecording(int)
 *  \brief Returns the ProgramInfo being used by any current recording.
 *
 *   Caller is responsible for deleting the ProgramInfo when done with it.
 *  \param capturecardnum cardid of recorder
 */
ProgramInfo *PlaybackSock::GetRecording(int capturecardnum)
{
    QStringList strlist = QString("QUERY_REMOTEENCODER %1")
        .arg(capturecardnum);

    strlist << "GET_CURRENT_RECORDING";

    SendReceiveStringList(strlist);

    ProgramInfo *info = new ProgramInfo();
    info->FromStringList(strlist, 0);
    return info;
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

RecStatusType PlaybackSock::StartRecording(int capturecardnum, 
                                           const ProgramInfo *pginfo)
{
    QStringList strlist = QString("QUERY_REMOTEENCODER %1").arg(capturecardnum);
    strlist << "START_RECORDING";
    pginfo->ToStringList(strlist);

    SendReceiveStringList(strlist);

    return RecStatusType(strlist[0].toInt());
}

void PlaybackSock::RecordPending(int capturecardnum, const ProgramInfo *pginfo,
                                 int secsleft, bool hasLater)
{
    QStringList strlist = QString("QUERY_REMOTEENCODER %1").arg(capturecardnum);
    strlist << "RECORD_PENDING";
    strlist << QString::number(secsleft);
    strlist << QString::number(hasLater);
    pginfo->ToStringList(strlist);

    SendReceiveStringList(strlist);
}

int PlaybackSock::SetSignalMonitoringRate(int capturecardnum,
                                          int rate, int notifyFrontend)
{
    QStringList strlist = QString("QUERY_REMOTEENCODER %1").arg(capturecardnum);
    strlist << "SET_SIGNAL_MONITORING_RATE";
    strlist << QString::number(rate);
    strlist << QString::number(notifyFrontend);

    SendReceiveStringList(strlist);

    int ret = strlist[0].toInt();
    return ret;
}

void PlaybackSock::SetNextLiveTVDir(int capturecardnum, QString dir)
{
    QStringList strlist =
        QString("SET_NEXT_LIVETV_DIR %1 %2").arg(capturecardnum).arg(dir);

    SendReceiveStringList(strlist);
}

vector<InputInfo> PlaybackSock::GetFreeInputs(int capturecardnum,
                                        const vector<uint> &excluded_cardids)
{
    QStringList strlist = QString("QUERY_REMOTEENCODER %1").arg(capturecardnum);
    strlist << "GET_FREE_INPUTS";

    for (uint i = 0; i < excluded_cardids.size(); i++)
        strlist << QString::number(excluded_cardids[i]);

    SendReceiveStringList(strlist);

    vector<InputInfo> list;

    QStringList::const_iterator it = strlist.begin();
    if ((it == strlist.end()) || (*it == "EMPTY_LIST"))
        return list;

    while (it != strlist.end())
    {
        InputInfo info;
        if (!info.FromStringList(it, strlist.end()))
            break;
        list.push_back(info);
    }

    return list;
}

void PlaybackSock::CancelNextRecording(int capturecardnum, bool cancel)
{
    QStringList strlist = QString("QUERY_REMOTEENCODER %1")
        .arg(capturecardnum);

    strlist << "CANCEL_NEXT_RECORDING";
    strlist << QString::number(cancel);

    SendReceiveStringList(strlist);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
