#include <QStringList>

using namespace std;

#include "compat.h"
#include "playbacksock.h"
#include "programinfo.h"
#include "mainserver.h"

#include "mythcorecontext.h"
#include "mythdate.h"
#include "inputinfo.h"

#define LOC QString("PlaybackSock: ")
#define LOC_ERR QString("PlaybackSock, Error: ")

PlaybackSock::PlaybackSock(
    MainServer *parent, MythSocket *lsock,
    QString lhostname, PlaybackSockEventsMode eventsMode) :
    ReferenceCounter("PlaybackSock")
{
    m_parent = parent;
    QString localhostname = gCoreContext->GetHostName();

    sock = lsock;
    hostname = lhostname;
    m_eventsMode = eventsMode;
    ip = "";
    backend = false;
    mediaserver = false;
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
    sock->DecrRef();
    sock = NULL;
}

bool PlaybackSock::wantsEvents(void) const
{
    return (m_eventsMode != kPBSEvents_None);
}

bool PlaybackSock::wantsNonSystemEvents(void) const
{
    return ((m_eventsMode == kPBSEvents_Normal) ||
            (m_eventsMode == kPBSEvents_NonSystem));
}

bool PlaybackSock::wantsSystemEvents(void) const
{
    return ((m_eventsMode == kPBSEvents_Normal) ||
            (m_eventsMode == kPBSEvents_SystemOnly));
}

bool PlaybackSock::wantsOnlySystemEvents(void) const
{
    return (m_eventsMode == kPBSEvents_SystemOnly);
}

PlaybackSockEventsMode PlaybackSock::eventsMode(void) const
{
    return m_eventsMode;
}

bool PlaybackSock::SendReceiveStringList(
    QStringList &strlist, uint min_reply_length)
{
    bool ok = false;

    sock->IncrRef();

    {
        QMutexLocker locker(&sockLock);
        expectingreply = true;
        ok = sock->SendReceiveStringList(strlist);
        expectingreply = false;
    }

    sock->DecrRef();

    if (!ok)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "PlaybackSock::SendReceiveStringList(): No response.");
        return false;
    }

    if (min_reply_length && ((uint)strlist.size() < min_reply_length))
    {
        LOG(VB_GENERAL, LOG_ERR,
            "PlaybackSock::SendReceiveStringList(): Response too short");
        return false;
    }

    return true;
}

/** \brief Tells a slave to go to sleep
 */
bool PlaybackSock::GoToSleep(void)
{
    QStringList strlist(QString("GO_TO_SLEEP"));

    return SendReceiveStringList(strlist, 1) && (strlist[0] == "OK");
}

/**
 *  \brief Appends host's dir's total and used space in kilobytes.
 */
void PlaybackSock::GetDiskSpace(QStringList &o_strlist)
{
    QStringList strlist(QString("QUERY_FREE_SPACE"));

    if (SendReceiveStringList(strlist, 8))
    {
        o_strlist += strlist;
    }
}

int PlaybackSock::CheckRecordingActive(const ProgramInfo *pginfo)
{
    QStringList strlist(QString("CHECK_RECORDING"));
    pginfo->ToStringList(strlist);

    if (SendReceiveStringList(strlist, 1))
        return strlist[0].toInt();

    return 0;
}

int PlaybackSock::DeleteFile(const QString &filename, const QString &sgroup)
{
    QStringList strlist("DELETE_FILE");
    strlist << filename;
    strlist << sgroup;

    if (SendReceiveStringList(strlist, 1))
        return strlist[0].toInt();

    return 0;
}

int PlaybackSock::StopRecording(const ProgramInfo *pginfo)
{
    QStringList strlist(QString("STOP_RECORDING"));
    pginfo->ToStringList(strlist);

    if (SendReceiveStringList(strlist, 1))
        return strlist[0].toInt();

    return 0;
}

int PlaybackSock::DeleteRecording(const ProgramInfo *pginfo,
                                  bool forceMetadataDelete)
{
    QStringList strlist;

    if (forceMetadataDelete)
        strlist = QStringList( QString("FORCE_DELETE_RECORDING"));
    else
        strlist = QStringList( QString("DELETE_RECORDING"));

    pginfo->ToStringList(strlist);

    if (SendReceiveStringList(strlist, 1))
        return strlist[0].toInt();

    return 0;
}

bool PlaybackSock::FillProgramInfo(ProgramInfo &pginfo,
                                   const QString &playbackhost)
{
    QStringList strlist(QString("FILL_PROGRAM_INFO"));
    strlist << playbackhost;
    pginfo.ToStringList(strlist);

    if (SendReceiveStringList(strlist))
    {
        ProgramInfo tmp(strlist);
        if (tmp.HasPathname() || tmp.GetChanID())
        {
            pginfo.clone(tmp, true);
            return true;
        }
    }

    return false;
}

QStringList PlaybackSock::GetSGFileList(QString &host, QString &groupname,
                                        QString &directory, bool fileNamesOnly)
{
    QStringList strlist(QString("QUERY_SG_GETFILELIST"));
    strlist << host;
    strlist << groupname;
    strlist << directory;
    strlist << QString::number(fileNamesOnly);

    SendReceiveStringList(strlist);

    return strlist;
}

QStringList PlaybackSock::GetSGFileQuery(QString &host, QString &groupname,
                                         QString &filename)
{
    QStringList strlist(QString("QUERY_SG_FILEQUERY"));
    strlist << host;
    strlist << groupname;
    strlist << filename;

    SendReceiveStringList(strlist);

    return strlist;
}

QString PlaybackSock::GetFileHash(QString filename, QString storageGroup)
{
    QStringList strlist(QString("QUERY_FILE_HASH"));
    strlist << filename
            << storageGroup;

    SendReceiveStringList(strlist);
    return strlist[0];
}

QStringList PlaybackSock::GenPreviewPixmap(const QString &token,
                                           const ProgramInfo *pginfo)
{
    QStringList strlist(QString("QUERY_GENPIXMAP2"));
    strlist += token;
    pginfo->ToStringList(strlist);

    SendReceiveStringList(strlist);

    return strlist;
}

QStringList PlaybackSock::GenPreviewPixmap(const QString &token,
                                           const ProgramInfo *pginfo,
                                           bool               time_fmt_sec,
                                           long long          time,
                                           const QString     &outputFile,
                                           const QSize       &outputSize)
{
    QStringList strlist(QString("QUERY_GENPIXMAP2"));
    strlist += token;
    pginfo->ToStringList(strlist);
    strlist.push_back(time_fmt_sec ? "s" : "f");
    strlist.push_back(QString::number(time));
    strlist.push_back((outputFile.isEmpty()) ? "<EMPTY>" : outputFile);
    strlist.push_back(QString::number(outputSize.width()));
    strlist.push_back(QString::number(outputSize.height()));

    SendReceiveStringList(strlist);

    return strlist;
}

QDateTime PlaybackSock::PixmapLastModified(const ProgramInfo *pginfo)
{
    QStringList strlist(QString("QUERY_PIXMAP_LASTMODIFIED"));
    pginfo->ToStringList(strlist);

    SendReceiveStringList(strlist);

    if (!strlist.empty() && strlist[0] != "BAD")
        return MythDate::fromTime_t(strlist[0].toUInt());

    return QDateTime();
}

bool PlaybackSock::CheckFile(ProgramInfo *pginfo)
{
    QStringList strlist("QUERY_CHECKFILE");
    strlist << QString::number(0); // don't check slaves
    pginfo->ToStringList(strlist);

    if (SendReceiveStringList(strlist, 2))
    {
        pginfo->SetPathname(strlist[1]);
        return strlist[0].toInt();
    }

    return false;
}

bool PlaybackSock::IsBusy(int capturecardnum, InputInfo *busy_input,
                          int time_buffer)
{
    QStringList strlist(QString("QUERY_REMOTEENCODER %1").arg(capturecardnum));

    strlist << "IS_BUSY";
    strlist << QString::number(time_buffer);

    if (!SendReceiveStringList(strlist, 1))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "IsBusy: " +
            QString("QUERY_REMOTEENCODER %1").arg(capturecardnum) +
            " gave us no response.");
    }

    bool state = false;

    if (!strlist.isEmpty())
    {
        QStringList::const_iterator it = strlist.begin();
        state = (*it).toInt();

        if (busy_input)
        {
            ++it;
            if (!busy_input->FromStringList(it, strlist.end()))
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + "IsBusy: "
                    "Failed to parse response to " +
                    QString("QUERY_REMOTEENCODER %1").arg(capturecardnum));
                state = false;
                // pretend it's not busy if we can't parse response
            }
        }
    }

    return state;
}

/** \fn PlaybackSock::GetEncoderState(int)
 *   Returns the maximum bits per second the recorder can produce.
 *  \param capturecardnum Recorder ID in the database.
 */
int PlaybackSock::GetEncoderState(int capturecardnum)
{
    QStringList strlist(QString("QUERY_REMOTEENCODER %1").arg(capturecardnum));
    strlist << "GET_STATE";

    if (!SendReceiveStringList(strlist, 1))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "GetEncoderState: " +
            QString("QUERY_REMOTEENCODER %1").arg(capturecardnum) +
            " gave us no response.");

        return kState_Error;
    }

    return strlist[0].toInt();
}

long long PlaybackSock::GetMaxBitrate(int capturecardnum)
{
    QStringList strlist(QString("QUERY_REMOTEENCODER %1").arg(capturecardnum));
    strlist << "GET_MAX_BITRATE";

    if (SendReceiveStringList(strlist, 1))
        return strlist[0].toLongLong();

    return 20200000LL; // Peak bit rate for HD-PVR
}

/** \brief Returns the ProgramInfo being used by any current recording.
 *
 *   Caller is responsible for deleting the ProgramInfo when done with it.
 */
ProgramInfo *PlaybackSock::GetRecording(uint cardid)
{
    QStringList strlist(QString("QUERY_REMOTEENCODER %1").arg(cardid));
    strlist << "GET_CURRENT_RECORDING";

    if (!SendReceiveStringList(strlist))
        return NULL;

    ProgramInfo *pginfo = new ProgramInfo(strlist);
    if (!pginfo->HasPathname() && !pginfo->GetChanID())
    {
        delete pginfo;
        pginfo = NULL;
    }

    return pginfo;
}

bool PlaybackSock::EncoderIsRecording(int capturecardnum,
                                      const ProgramInfo *pginfo)
{
    QStringList strlist(QString("QUERY_REMOTEENCODER %1").arg(capturecardnum));
    strlist << "MATCHES_RECORDING";
    pginfo->ToStringList(strlist);

    if (SendReceiveStringList(strlist, 1))
        return (bool) strlist[0].toInt();

    return false;
}

RecStatusType PlaybackSock::StartRecording(int capturecardnum,
                                           const ProgramInfo *pginfo)
{
    QStringList strlist(QString("QUERY_REMOTEENCODER %1").arg(capturecardnum));
    strlist << "START_RECORDING";
    pginfo->ToStringList(strlist);

    if (SendReceiveStringList(strlist, 1))
        return RecStatusType(strlist[0].toInt());

    return rsUnknown;
}

RecStatusType PlaybackSock::GetRecordingStatus(int capturecardnum)
{
    QStringList strlist(QString("QUERY_REMOTEENCODER %1").arg(capturecardnum));
    strlist << "GET_RECORDING_STATUS";

    if (!SendReceiveStringList(strlist, 1))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "GetRecordingStatus: " +
            QString("QUERY_REMOTEENCODER %1").arg(capturecardnum) +
            " did not respond.");

        return rsUnknown;
    }

    return RecStatusType(strlist[0].toInt());
}

void PlaybackSock::RecordPending(int capturecardnum, const ProgramInfo *pginfo,
                                 int secsleft, bool hasLater)
{
    QStringList strlist(QString("QUERY_REMOTEENCODER %1").arg(capturecardnum));
    strlist << "RECORD_PENDING";
    strlist << QString::number(secsleft);
    strlist << QString::number(hasLater);
    pginfo->ToStringList(strlist);

    SendReceiveStringList(strlist);
}

int PlaybackSock::SetSignalMonitoringRate(int capturecardnum,
                                          int rate, int notifyFrontend)
{
    QStringList strlist(QString("QUERY_REMOTEENCODER %1").arg(capturecardnum));
    strlist << "SET_SIGNAL_MONITORING_RATE";
    strlist << QString::number(rate);
    strlist << QString::number(notifyFrontend);

    if (SendReceiveStringList(strlist, 1))
        return strlist[0].toInt();

    return -1;
}

void PlaybackSock::SetNextLiveTVDir(int capturecardnum, QString dir)
{
    QStringList strlist(QString("SET_NEXT_LIVETV_DIR %1 %2")
                            .arg(capturecardnum)
                            .arg(dir));

    SendReceiveStringList(strlist);
}

vector<InputInfo> PlaybackSock::GetFreeInputs(int capturecardnum,
                                           const vector<uint> &excluded_cardids)
{
    QStringList strlist(QString("QUERY_REMOTEENCODER %1").arg(capturecardnum));
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
    QStringList strlist(QString("QUERY_REMOTEENCODER %1")
                        .arg(capturecardnum));

    strlist << "CANCEL_NEXT_RECORDING";
    strlist << QString::number(cancel);

    SendReceiveStringList(strlist);
}

QStringList PlaybackSock::ForwardRequest(const QStringList &slist)
{
    QStringList strlist = slist;

    if (SendReceiveStringList(strlist))
        return strlist;

    return QStringList();
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
