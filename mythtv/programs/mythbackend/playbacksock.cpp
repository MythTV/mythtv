// C++
#include <utility>

// Qt
#include <QStringList>

// MythTV
#include "libmythbase/compat.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/programinfo.h"
#include "libmythbase/referencecounter.h"
#include "libmythtv/inputinfo.h"

// MythBackend
#include "mainserver.h"
#include "playbacksock.h"

#define LOC QString("PlaybackSock: ")
#define LOC_ERR QString("PlaybackSock, Error: ")

PlaybackSock::PlaybackSock(
    MythSocket *lsock,
    QString lhostname, PlaybackSockEventsMode eventsMode) :
    ReferenceCounter("PlaybackSock"),
    m_sock(lsock),
    m_hostname(std::move(lhostname)),
    m_ip(""),
    m_eventsMode(eventsMode)
{
    QString localhostname = gCoreContext->GetHostName();
    m_local = (m_hostname == localhostname);
}

PlaybackSock::~PlaybackSock()
{
    m_sock->DecrRef();
    m_sock = nullptr;
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

bool PlaybackSock::ReadStringList(QStringList &list)
{
    m_sock->IncrRef();
    ReferenceLocker rlocker(m_sock);
    QMutexLocker locker(&m_sockLock);
    if (!m_sock->IsDataAvailable())
    {
        LOG(VB_GENERAL, LOG_DEBUG,
            "PlaybackSock::ReadStringList(): Data vanished !!!");
        return false;
    }
    return m_sock->ReadStringList(list);
}

bool PlaybackSock::SendReceiveStringList(
    QStringList &strlist, uint min_reply_length)
{
    bool ok = false;

    m_sock->IncrRef();

    {
        QMutexLocker locker(&m_sockLock);
        m_sock->SetReadyReadCallbackEnabled(false);

        ok = m_sock->SendReceiveStringList(strlist);
        while (ok && strlist[0] == "BACKEND_MESSAGE")
        {
            // oops, not for us
            if (strlist.size() >= 2)
            {
                QString message = strlist[1];
                strlist.pop_front();
                strlist.pop_front();
                MythEvent me(message, strlist);
                gCoreContext->dispatch(me);
            }

            ok = m_sock->ReadStringList(strlist);
        }
        m_sock->SetReadyReadCallbackEnabled(true);
    }

    m_sock->DecrRef();

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
 *  \brief Gets the total and used space in kilobytes for the host's directories.
 */
FileSystemInfoList PlaybackSock::GetDiskSpace()
{
    QStringList strlist(QString("QUERY_FREE_SPACE"));

    if (SendReceiveStringList(strlist, FileSystemInfo::kLines))
    {
        return FileSystemInfoManager::FromStringList(strlist);
    }
    return {};
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

QStringList PlaybackSock::GetSGFileList(const QString &host, const QString &groupname,
                                        const QString &directory, bool fileNamesOnly)
{
    QStringList strlist(QString("QUERY_SG_GETFILELIST"));
    strlist << host;
    strlist << groupname;
    strlist << directory;
    strlist << QString::number(static_cast<int>(fileNamesOnly));

    SendReceiveStringList(strlist);

    return strlist;
}

QStringList PlaybackSock::GetSGFileQuery(const QString &host, const QString &groupname,
                                         const QString &filename)
{
    QStringList strlist(QString("QUERY_SG_FILEQUERY"));
    strlist << host;
    strlist << groupname;
    strlist << filename;

    SendReceiveStringList(strlist);

    return strlist;
}

QString PlaybackSock::GetFileHash(const QString& filename, const QString& storageGroup)
{
    QStringList strlist(QString("QUERY_FILE_HASH"));
    strlist << filename
            << storageGroup;

    SendReceiveStringList(strlist);
    return strlist[0];
}

QStringList PlaybackSock::GetFindFile(const QString &host, const QString &filename,
                                      const QString &storageGroup, bool useRegex)
{
    QStringList strlist(QString("QUERY_FINDFILE"));
    strlist << host
            << storageGroup
            << filename
            << (useRegex ? "1" : "0")
            << "0";

    SendReceiveStringList(strlist);
    return strlist;
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
                                           std::chrono::seconds time,
                                           long long          frame,
                                           const QString     &outputFile,
                                           const QSize        outputSize)
{
    QStringList strlist(QString("QUERY_GENPIXMAP2"));
    strlist += token;
    pginfo->ToStringList(strlist);
    if (time != std::chrono::seconds::max())
    {
        strlist.push_back("s");
        strlist.push_back(QString::number(time.count()));
    }
    else
    {
        strlist.push_back("f");
        strlist.push_back(QString::number(frame));
    }
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

    if (!strlist.empty() && !strlist[0].isEmpty() && (strlist[0] != "BAD"))
    {
        return MythDate::fromSecsSinceEpoch(strlist[0].toLongLong());
    }

    return {};
}

bool PlaybackSock::CheckFile(ProgramInfo *pginfo)
{
    QStringList strlist("QUERY_CHECKFILE");
    strlist << QString::number(0); // don't check slaves
    pginfo->ToStringList(strlist);

    if (SendReceiveStringList(strlist, 2))
    {
        pginfo->SetPathname(strlist[1]);
        return strlist[0].toInt() != 0;
    }

    return false;
}

bool PlaybackSock::IsBusy(int capturecardnum, InputInfo *busy_input,
                          std::chrono::seconds time_buffer)
{
    QStringList strlist(QString("QUERY_REMOTEENCODER %1").arg(capturecardnum));

    strlist << "IS_BUSY";
    strlist << QString::number(time_buffer.count());

    if (!SendReceiveStringList(strlist, 1))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "IsBusy: " +
            QString("QUERY_REMOTEENCODER %1").arg(capturecardnum) +
            " gave us no response.");
    }

    bool state = false;

    if (!strlist.isEmpty())
    {
        QStringList::const_iterator it = strlist.cbegin();
        state = ((*it).toInt() != 0);

        if (busy_input)
        {
            ++it;
            if (!busy_input->FromStringList(it, strlist.cend()))
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
        return nullptr;

    auto *pginfo = new ProgramInfo(strlist);
    if (!pginfo->HasPathname() && !pginfo->GetChanID())
    {
        delete pginfo;
        pginfo = nullptr;
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

RecStatus::Type PlaybackSock::StartRecording(int capturecardnum,
                                           ProgramInfo *pginfo)
{
    QStringList strlist(QString("QUERY_REMOTEENCODER %1").arg(capturecardnum));
    strlist << "START_RECORDING";
    pginfo->ToStringList(strlist);

    if (SendReceiveStringList(strlist, 3))
    {
        pginfo->SetRecordingID(strlist[1].toUInt());
        pginfo->SetRecordingStartTime(
            MythDate::fromSecsSinceEpoch(strlist[2].toLongLong()));
        return RecStatus::Type(strlist[0].toInt());
    }

    return RecStatus::Unknown;
}

RecStatus::Type PlaybackSock::GetRecordingStatus(int capturecardnum)
{
    QStringList strlist(QString("QUERY_REMOTEENCODER %1").arg(capturecardnum));
    strlist << "GET_RECORDING_STATUS";

    if (!SendReceiveStringList(strlist, 1))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "GetRecordingStatus: " +
            QString("QUERY_REMOTEENCODER %1").arg(capturecardnum) +
            " did not respond.");

        return RecStatus::Unknown;
    }

    return RecStatus::Type(strlist[0].toInt());
}

void PlaybackSock::RecordPending(int capturecardnum, const ProgramInfo *pginfo,
                                 std::chrono::seconds secsleft, bool hasLater)
{
    QStringList strlist(QString("QUERY_REMOTEENCODER %1").arg(capturecardnum));
    strlist << "RECORD_PENDING";
    strlist << QString::number(secsleft.count());
    strlist << QString::number(static_cast<int>(hasLater));
    pginfo->ToStringList(strlist);

    SendReceiveStringList(strlist);
}

std::chrono::milliseconds PlaybackSock::SetSignalMonitoringRate(int capturecardnum,
                                          std::chrono::milliseconds rate, int notifyFrontend)
{
    QStringList strlist(QString("QUERY_REMOTEENCODER %1").arg(capturecardnum));
    strlist << "SET_SIGNAL_MONITORING_RATE";
    strlist << QString::number(rate.count());
    strlist << QString::number(notifyFrontend);

    if (SendReceiveStringList(strlist, 1))
        return std::chrono::milliseconds(strlist[0].toInt());

    return -1ms;
}

void PlaybackSock::SetNextLiveTVDir(int capturecardnum, const QString& dir)
{
    QStringList strlist(QString("SET_NEXT_LIVETV_DIR %1 %2")
                            .arg(capturecardnum)
                            .arg(dir));

    SendReceiveStringList(strlist);
}

void PlaybackSock::CancelNextRecording(int capturecardnum, bool cancel)
{
    QStringList strlist(QString("QUERY_REMOTEENCODER %1")
                        .arg(capturecardnum));

    strlist << "CANCEL_NEXT_RECORDING";
    strlist << QString::number(static_cast<int>(cancel));

    SendReceiveStringList(strlist);
}

QStringList PlaybackSock::ForwardRequest(const QStringList &slist)
{
    QStringList strlist = slist;

    if (SendReceiveStringList(strlist))
        return strlist;

    return {};
}

/** \brief Tells a slave to add a child input.
 */
bool PlaybackSock::AddChildInput(uint childid)
{
    QStringList strlist(QString("ADD_CHILD_INPUT %1").arg(childid));
    return SendReceiveStringList(strlist, 1) && (strlist[0] == "OK");
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
