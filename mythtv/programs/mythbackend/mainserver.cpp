#include <list>
#include <cstdlib>
#include <iostream>
#include <algorithm>
#include <cerrno>
#include <memory>
using namespace std;

#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include "mythconfig.h"

#ifndef _WIN32
#include <sys/ioctl.h>
#endif

#include <sys/stat.h>
#ifdef __linux__
#  include <sys/vfs.h>
#else // if !__linux__
#  include <sys/param.h>
#  ifndef _WIN32
#    include <sys/mount.h>
#  endif // _WIN32
#endif // !__linux__

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QDir>
#include <QWaitCondition>
#include <QWriteLocker>
#include <QRegExp>
#include <QEvent>
#include <QUrl>
#include <QTcpServer>
#include <QTimer>
#include <QNetworkInterface>
#include <QNetworkProxy>
#include <QHostAddress>

#include "previewgeneratorqueue.h"
#include "mythmiscutil.h"
#include "mythsystemlegacy.h"
#include "exitcodes.h"
#include "mythcontext.h"
#include "mythversion.h"
#include "mythdb.h"
#include "mainserver.h"
#include "server.h"
#include "mthread.h"
#include "scheduler.h"
#include "backendutil.h"
#include "programinfo.h"
#include "mythtimezone.h"
#include "recordinginfo.h"
#include "recordingrule.h"
#include "scheduledrecording.h"
#include "jobqueue.h"
#include "autoexpire.h"
#include "storagegroup.h"
#include "compat.h"
#include "ringbuffer.h"
#include "remotefile.h"
#include "mythsystemevent.h"
#include "tv.h"
#include "mythcorecontext.h"
#include "mythcoreutil.h"
#include "mythdirs.h"
#include "mythdownloadmanager.h"
#include "metadatafactory.h"
#include "videoutils.h"
#include "mythlogging.h"
#include "filesysteminfo.h"
#include "metaio.h"
#include "musicmetadata.h"
#include "imagemanager.h"
#include "cardutil.h"

// mythbackend headers
#include "backendcontext.h"

/** Milliseconds to wait for an existing thread from
 *  process request thread pool.
 */
#define PRT_TIMEOUT 10
/** Number of threads in process request thread pool at startup. */
#define PRT_STARTUP_THREAD_COUNT 5

#define LOC      QString("MainServer: ")
#define LOC_WARN QString("MainServer, Warning: ")
#define LOC_ERR  QString("MainServer, Error: ")

namespace {

int delete_file_immediately(const QString &filename,
                            bool followLinks, bool checkexists)
{
    /* Return 0 for success, non-zero for error. */
    QFile checkFile(filename);
    int success1, success2;

    LOG(VB_FILE, LOG_INFO, LOC +
        QString("About to delete file: %1").arg(filename));
    success1 = true;
    success2 = true;
    if (followLinks)
    {
        QFileInfo finfo(filename);
        if (finfo.isSymLink())
        {
            QString linktext = getSymlinkTarget(filename);

            QFile target(linktext);
            if (!(success1 = target.remove()))
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("Error deleting '%1' -> '%2'")
                    .arg(filename).arg(linktext) + ENO);
            }
        }
    }
    if ((!checkexists || checkFile.exists()) &&
            !(success2 = checkFile.remove()))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Error deleting '%1': %2")
                .arg(filename).arg(strerror(errno)));
    }
    return success1 && success2 ? 0 : -1;
}

};

QMutex MainServer::truncate_and_close_lock;
const uint MainServer::kMasterServerReconnectTimeout = 1000; //ms

class ProcessRequestRunnable : public QRunnable
{
  public:
    ProcessRequestRunnable(MainServer &parent, MythSocket *sock) :
        m_parent(parent), m_sock(sock)
    {
        m_sock->IncrRef();
    }

    virtual ~ProcessRequestRunnable()
    {
        if (m_sock)
        {
            m_sock->DecrRef();
            m_sock = NULL;
        }
    }

    virtual void run(void)
    {
        m_parent.ProcessRequest(m_sock);
        m_sock->DecrRef();
        m_sock = NULL;
    }

  private:
    MainServer &m_parent;
    MythSocket *m_sock;
};

class FreeSpaceUpdater : public QRunnable
{
  public:
    explicit FreeSpaceUpdater(MainServer &parent) :
        m_parent(parent), m_dorun(true), m_running(true)
    {
        m_lastRequest.start();
    }
    ~FreeSpaceUpdater()
    {
        QMutexLocker locker(&m_parent.masterFreeSpaceListLock);
        m_parent.masterFreeSpaceListUpdater = NULL;
        m_parent.masterFreeSpaceListWait.wakeAll();
    }

    virtual void run(void)
    {
        while (true)
        {
            MythTimer t;
            t.start();
            QStringList list;
            m_parent.BackendQueryDiskSpace(list, true, true);
            {
                QMutexLocker locker(&m_parent.masterFreeSpaceListLock);
                m_parent.masterFreeSpaceList = list;
            }
            QMutexLocker locker(&m_lock);
            int left = kRequeryTimeout - t.elapsed();
            if (m_lastRequest.elapsed() + left > kExitTimeout)
                m_dorun = false;
            if (!m_dorun)
            {
                m_running = false;
                break;
            }
            if (left > 50)
                m_wait.wait(locker.mutex(), left);
        }
    }

    bool KeepRunning(bool dorun)
    {
        QMutexLocker locker(&m_lock);
        if (dorun && m_running)
        {
            m_dorun = true;
            m_lastRequest.restart();
        }
        else
        {
            m_dorun = false;
            m_wait.wakeAll();
        }
        return m_running;
    }

    MainServer &m_parent;
    QMutex m_lock;
    bool m_dorun;
    bool m_running;
    MythTimer m_lastRequest;
    QWaitCondition m_wait;
    const static int kRequeryTimeout;
    const static int kExitTimeout;
};
const int FreeSpaceUpdater::kRequeryTimeout = 15000;
const int FreeSpaceUpdater::kExitTimeout = 61000;

MainServer::MainServer(bool master, int port,
                       QMap<int, EncoderLink *> *tvList,
                       Scheduler *sched, AutoExpire *expirer) :
    encoderList(tvList), mythserver(NULL),
    metadatafactory(NULL),
    masterFreeSpaceListUpdater(NULL),
    masterServerReconnect(NULL),
    masterServer(NULL), ismaster(master), threadPool("ProcessRequestPool"),
    masterBackendOverride(false),
    m_sched(sched), m_expirer(expirer), deferredDeleteTimer(NULL),
    autoexpireUpdateTimer(NULL), m_exitCode(GENERIC_EXIT_OK),
    m_stopped(false)
{
    PreviewGeneratorQueue::CreatePreviewGeneratorQueue(
        PreviewGenerator::kLocalAndRemote, ~0, 0);
    PreviewGeneratorQueue::AddListener(this);

    threadPool.setMaxThreadCount(PRT_STARTUP_THREAD_COUNT);

    masterBackendOverride =
        gCoreContext->GetNumSetting("MasterBackendOverride", 0);

    mythserver = new MythServer();
    mythserver->setProxy(QNetworkProxy::NoProxy);

    // test to make sure listen addresses are available
    // no reason to run the backend if the mainserver is not active
    QHostAddress config_v4(gCoreContext->resolveSettingAddress(
                                           "BackendServerIP",
                                           QString(),
                                           gCoreContext->ResolveIPv4, true));
    bool v4IsSet = config_v4.isNull() ? false : true;
#if !defined(QT_NO_IPV6)
    QHostAddress config_v6(gCoreContext->resolveSettingAddress(
                                           "BackendServerIP6",
                                           QString(),
                                           gCoreContext->ResolveIPv6, true));
    bool v6IsSet = config_v6.isNull() ? false : true;
#endif
    QList<QHostAddress> listenAddrs = mythserver->DefaultListen();

    #if !defined(QT_NO_IPV6)
    if (v6IsSet && !listenAddrs.contains(config_v6))
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Unable to find IPv6 address to bind");
    #endif

    if (v4IsSet && !listenAddrs.contains(config_v4))
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Unable to find IPv4 address to bind");

    if ((v4IsSet && !listenAddrs.contains(config_v4))
#if !defined(QT_NO_IPV6)
        && (v6IsSet && !listenAddrs.contains(config_v6))
#endif
       )
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Unable to find either IPv4 or IPv6 "
                                 "address we can bind to, exiting");
        SetExitCode(GENERIC_EXIT_SOCKET_ERROR, false);
        return;
    }

    if (!mythserver->listen(port))
    {
        SetExitCode(GENERIC_EXIT_SOCKET_ERROR, false);
        return;
    }
    connect(mythserver, SIGNAL(NewConnection(qt_socket_fd_t)),
            this,       SLOT(NewConnection(qt_socket_fd_t)));

    gCoreContext->addListener(this);

    if (!ismaster)
    {
        masterServerReconnect = new QTimer(this);
        masterServerReconnect->setSingleShot(true);
        connect(masterServerReconnect, SIGNAL(timeout()),
                this, SLOT(reconnectTimeout()));
        masterServerReconnect->start(kMasterServerReconnectTimeout);
    }

    deferredDeleteTimer = new QTimer(this);
    connect(deferredDeleteTimer, SIGNAL(timeout()),
            this, SLOT(deferredDeleteSlot()));
    deferredDeleteTimer->start(30 * 1000);

    if (sched)
        sched->SetMainServer(this);
    if (expirer)
        expirer->SetMainServer(this);

    metadatafactory = new MetadataFactory(this);

    autoexpireUpdateTimer = new QTimer(this);
    connect(autoexpireUpdateTimer, SIGNAL(timeout()),
            this, SLOT(autoexpireUpdate()));
    autoexpireUpdateTimer->setSingleShot(true);

    AutoExpire::Update(true);

    masterFreeSpaceList << gCoreContext->GetHostName();
    masterFreeSpaceList << "TotalDiskSpace";
    masterFreeSpaceList << "0";
    masterFreeSpaceList << "-2";
    masterFreeSpaceList << "-2";
    masterFreeSpaceList << "0";
    masterFreeSpaceList << "0";
    masterFreeSpaceList << "0";

    masterFreeSpaceListUpdater = (master ? new FreeSpaceUpdater(*this) : NULL);
    if (masterFreeSpaceListUpdater)
    {
        MThreadPool::globalInstance()->startReserved(
            masterFreeSpaceListUpdater, "FreeSpaceUpdater");
    }
}

MainServer::~MainServer()
{
    if (!m_stopped)
        Stop();
}

void MainServer::Stop()
{
    m_stopped = true;

    gCoreContext->removeListener(this);

    {
        QMutexLocker locker(&masterFreeSpaceListLock);
        if (masterFreeSpaceListUpdater)
            masterFreeSpaceListUpdater->KeepRunning(false);
    }

    threadPool.Stop();

    // since Scheduler::SetMainServer() isn't thread-safe
    // we need to shut down the scheduler thread before we
    // can call SetMainServer(NULL)
    if (m_sched)
        m_sched->Stop();

    PreviewGeneratorQueue::RemoveListener(this);
    PreviewGeneratorQueue::TeardownPreviewGeneratorQueue();

    if (mythserver)
    {
        mythserver->disconnect();
        mythserver->deleteLater();
        mythserver = NULL;
    }

    if (m_sched)
    {
        m_sched->Wait();
        m_sched->SetMainServer(NULL);
    }

    if (m_expirer)
        m_expirer->SetMainServer(NULL);

    {
        QMutexLocker locker(&masterFreeSpaceListLock);
        while (masterFreeSpaceListUpdater)
        {
            masterFreeSpaceListUpdater->KeepRunning(false);
            masterFreeSpaceListWait.wait(locker.mutex());
        }
    }

    // Close all open sockets
    QWriteLocker locker(&sockListLock);

    vector<PlaybackSock *>::iterator it = playbackList.begin();
    for (; it != playbackList.end(); ++it)
        (*it)->DecrRef();
    playbackList.clear();

    vector<FileTransfer *>::iterator ft = fileTransferList.begin();
    for (; ft != fileTransferList.end(); ++ft)
        (*ft)->DecrRef();
    fileTransferList.clear();

    QSet<MythSocket*>::iterator cs = controlSocketList.begin();
    for (; cs != controlSocketList.end(); ++cs)
        (*cs)->DecrRef();
    controlSocketList.clear();

    while (!decrRefSocketList.empty())
    {
        (*decrRefSocketList.begin())->DecrRef();
        decrRefSocketList.erase(decrRefSocketList.begin());
    }
}

void MainServer::autoexpireUpdate(void)
{
    AutoExpire::Update(false);
}

void MainServer::NewConnection(qt_socket_fd_t socketDescriptor)
{
    QWriteLocker locker(&sockListLock);
    controlSocketList.insert(new MythSocket(socketDescriptor, this));
}

void MainServer::readyRead(MythSocket *sock)
{
    threadPool.startReserved(
        new ProcessRequestRunnable(*this, sock),
        "ProcessRequest", PRT_TIMEOUT);

    QCoreApplication::processEvents();
}

void MainServer::ProcessRequest(MythSocket *sock)
{
    if (sock->IsDataAvailable())
        ProcessRequestWork(sock);
    else
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("No data on sock %1")
            .arg(sock->GetSocketDescriptor()));
}

void MainServer::ProcessRequestWork(MythSocket *sock)
{
    sockListLock.lockForRead();
    PlaybackSock *pbs = GetPlaybackBySock(sock);
    if (pbs)
        pbs->IncrRef();

    bool bIsControl = (pbs) ? false : controlSocketList.contains(sock);
    sockListLock.unlock();

    QStringList listline;
    if (pbs)
    {
        if (!pbs->ReadStringList(listline) || listline.empty())
        {
            pbs->DecrRef();
            LOG(VB_GENERAL, LOG_INFO, "No data in ProcessRequestWork()");
            return;
        }
        pbs->DecrRef();
    }
    else if (!bIsControl)
    {
        // The socket has been disconnected
        return;
    }
    else if (!sock->ReadStringList(listline) || listline.empty())
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "No data in ProcessRequestWork()");
        return;
    }

    QString line = listline[0];

    line = line.simplified();
    QStringList tokens = line.split(' ', QString::SkipEmptyParts);
    QString command = tokens[0];

    if (command == "MYTH_PROTO_VERSION")
    {
        if (tokens.size() < 2)
            SendErrorResponse(sock, "Bad MYTH_PROTO_VERSION command");
        else
            HandleVersion(sock, tokens);
        return;
    }
    else if (command == "ANN")
    {
        HandleAnnounce(listline, tokens, sock);
        return;
    }
    else if (command == "DONE")
    {
        HandleDone(sock);
        return;
    }

    sockListLock.lockForRead();
    pbs = GetPlaybackBySock(sock);
    if (!pbs)
    {
        sockListLock.unlock();
        LOG(VB_GENERAL, LOG_ERR, LOC + "ProcessRequest unknown socket");
        return;
    }
    pbs->IncrRef();
    sockListLock.unlock();

    if (command == "QUERY_FILETRANSFER")
    {
        if (tokens.size() != 2)
            SendErrorResponse(pbs, "Bad QUERY_FILETRANSFER");
        else
            HandleFileTransferQuery(listline, tokens, pbs);
    }
    else if (command == "QUERY_RECORDINGS")
    {
        if (tokens.size() != 2)
            SendErrorResponse(pbs, "Bad QUERY_RECORDINGS query");
        else
            HandleQueryRecordings(tokens[1], pbs);
    }
    else if (command == "QUERY_RECORDING")
    {
        HandleQueryRecording(tokens, pbs);
    }
    else if (command == "GO_TO_SLEEP")
    {
        HandleGoToSleep(pbs);
    }
    else if (command == "QUERY_FREE_SPACE")
    {
        HandleQueryFreeSpace(pbs, false);
    }
    else if (command == "QUERY_FREE_SPACE_LIST")
    {
        HandleQueryFreeSpace(pbs, true);
    }
    else if (command == "QUERY_FREE_SPACE_SUMMARY")
    {
        HandleQueryFreeSpaceSummary(pbs);
    }
    else if (command == "QUERY_LOAD")
    {
        HandleQueryLoad(pbs);
    }
    else if (command == "QUERY_UPTIME")
    {
        HandleQueryUptime(pbs);
    }
    else if (command == "QUERY_HOSTNAME")
    {
        HandleQueryHostname(pbs);
    }
    else if (command == "QUERY_MEMSTATS")
    {
        HandleQueryMemStats(pbs);
    }
    else if (command == "QUERY_TIME_ZONE")
    {
        HandleQueryTimeZone(pbs);
    }
    else if (command == "QUERY_CHECKFILE")
    {
        HandleQueryCheckFile(listline, pbs);
    }
    else if (command == "QUERY_FILE_EXISTS")
    {
        if (listline.size() < 2)
            SendErrorResponse(pbs, "Bad QUERY_FILE_EXISTS command");
        else
            HandleQueryFileExists(listline, pbs);
    }
    else if (command == "QUERY_FINDFILE")
    {
        if (listline.size() < 4)
            SendErrorResponse(pbs, "Bad QUERY_FINDFILE command");
        else
            HandleQueryFindFile(listline, pbs);
    }
    else if (command == "QUERY_FILE_HASH")
    {
        if (listline.size() < 3)
            SendErrorResponse(pbs, "Bad QUERY_FILE_HASH command");
        else
            HandleQueryFileHash(listline, pbs);
    }
    else if (command == "QUERY_GUIDEDATATHROUGH")
    {
        HandleQueryGuideDataThrough(pbs);
    }
    else if (command == "DELETE_FILE")
    {
        if (listline.size() < 3)
            SendErrorResponse(pbs, "Bad DELETE_FILE command");
        else
            HandleDeleteFile(listline, pbs);
    }
    else if (command == "MOVE_FILE")
    {
        if (listline.size() < 4)
            SendErrorResponse(pbs, "Bad MOVE_FILE command");
        else
            HandleMoveFile(pbs, listline[1], listline[2], listline[3]);
    }
    else if (command == "STOP_RECORDING")
    {
        HandleStopRecording(listline, pbs);
    }
    else if (command == "CHECK_RECORDING")
    {
        HandleCheckRecordingActive(listline, pbs);
    }
    else if (command == "DELETE_RECORDING")
    {
        if (3 <= tokens.size() && tokens.size() <= 5)
        {
            bool force = (tokens.size() >= 4) && (tokens[3] == "FORCE");
            bool forget = (tokens.size() >= 5) && (tokens[4] == "FORGET");
            HandleDeleteRecording(tokens[1], tokens[2], pbs, force, forget);
        }
        else
            HandleDeleteRecording(listline, pbs, false);
    }
    else if (command == "FORCE_DELETE_RECORDING")
    {
        HandleDeleteRecording(listline, pbs, true);
    }
    else if (command == "UNDELETE_RECORDING")
    {
        HandleUndeleteRecording(listline, pbs);
    }
    else if (command == "RESCHEDULE_RECORDINGS")
    {
        listline.pop_front();
        HandleRescheduleRecordings(listline, pbs);
    }
    else if (command == "FORGET_RECORDING")
    {
        HandleForgetRecording(listline, pbs);
    }
    else if (command == "QUERY_GETALLPENDING")
    {
        if (tokens.size() == 1)
            HandleGetPendingRecordings(pbs);
        else if (tokens.size() == 2)
            HandleGetPendingRecordings(pbs, tokens[1]);
        else
            HandleGetPendingRecordings(pbs, tokens[1], tokens[2].toInt());
    }
    else if (command == "QUERY_GETALLSCHEDULED")
    {
        HandleGetScheduledRecordings(pbs);
    }
    else if (command == "QUERY_GETCONFLICTING")
    {
        HandleGetConflictingRecordings(listline, pbs);
    }
    else if (command == "QUERY_GETEXPIRING")
    {
        HandleGetExpiringRecordings(pbs);
    }
    else if (command == "QUERY_SG_GETFILELIST")
    {
        HandleSGGetFileList(listline, pbs);
    }
    else if (command == "QUERY_SG_FILEQUERY")
    {
        HandleSGFileQuery(listline, pbs);
    }
    else if (command == "GET_FREE_INPUT_INFO")
    {
        if (tokens.size() != 2)
            SendErrorResponse(pbs, "Bad GET_FREE_INPUT_INFO");
        else
            HandleGetFreeInputInfo(pbs, tokens[1].toUInt());
    }
    else if (command == "QUERY_RECORDER")
    {
        if (tokens.size() != 2)
            SendErrorResponse(pbs, "Bad QUERY_RECORDER");
        else
            HandleRecorderQuery(listline, tokens, pbs);
    }
    else if (command == "QUERY_RECORDING_DEVICE")
    {
        // TODO
    }
    else if (command == "QUERY_RECORDING_DEVICES")
    {
        // TODO
    }
    else if (command == "SET_NEXT_LIVETV_DIR")
    {
        if (tokens.size() != 3)
            SendErrorResponse(pbs, "Bad SET_NEXT_LIVETV_DIR");
        else
            HandleSetNextLiveTVDir(tokens, pbs);
    }
    else if (command == "SET_CHANNEL_INFO")
    {
        HandleSetChannelInfo(listline, pbs);
    }
    else if (command == "QUERY_REMOTEENCODER")
    {
        if (tokens.size() != 2)
            SendErrorResponse(pbs, "Bad QUERY_REMOTEENCODER");
        else
            HandleRemoteEncoder(listline, tokens, pbs);
    }
    else if (command == "GET_RECORDER_FROM_NUM")
    {
        HandleGetRecorderFromNum(listline, pbs);
    }
    else if (command == "GET_RECORDER_NUM")
    {
        HandleGetRecorderNum(listline, pbs);
    }
    else if (command == "QUERY_GENPIXMAP2")
    {
        HandleGenPreviewPixmap(listline, pbs);
    }
    else if (command == "QUERY_PIXMAP_LASTMODIFIED")
    {
        HandlePixmapLastModified(listline, pbs);
    }
    else if (command == "QUERY_PIXMAP_GET_IF_MODIFIED")
    {
        HandlePixmapGetIfModified(listline, pbs);
    }
    else if (command == "QUERY_ISRECORDING")
    {
        HandleIsRecording(listline, pbs);
    }
    else if (command == "MESSAGE")
    {
        if ((listline.size() >= 2) && (listline[1].startsWith("SET_VERBOSE")))
            HandleSetVerbose(listline, pbs);
        else if ((listline.size() >= 2) &&
                 (listline[1].startsWith("SET_LOG_LEVEL")))
            HandleSetLogLevel(listline, pbs);
        else
            HandleMessage(listline, pbs);
    }
    else if (command == "FILL_PROGRAM_INFO")
    {
        HandleFillProgramInfo(listline, pbs);
    }
    else if (command == "LOCK_TUNER")
    {
        if (tokens.size() == 1)
            HandleLockTuner(pbs);
        else if (tokens.size() == 2)
            HandleLockTuner(pbs, tokens[1].toInt());
        else
            SendErrorResponse(pbs, "Bad LOCK_TUNER query");
    }
    else if (command == "FREE_TUNER")
    {
        if (tokens.size() != 2)
            SendErrorResponse(pbs, "Bad FREE_TUNER query");
        else
            HandleFreeTuner(tokens[1].toInt(), pbs);
    }
    else if (command == "QUERY_ACTIVE_BACKENDS")
    {
        HandleActiveBackendsQuery(pbs);
    }
    else if (command == "QUERY_IS_ACTIVE_BACKEND")
    {
        if (tokens.size() != 1)
            SendErrorResponse(pbs, "Bad QUERY_IS_ACTIVE_BACKEND");
        else
            HandleIsActiveBackendQuery(listline, pbs);
    }
    else if (command == "QUERY_COMMBREAK")
    {
        if (tokens.size() != 3)
            SendErrorResponse(pbs, "Bad QUERY_COMMBREAK");
        else
            HandleCommBreakQuery(tokens[1], tokens[2], pbs);
    }
    else if (command == "QUERY_CUTLIST")
    {
        if (tokens.size() != 3)
            SendErrorResponse(pbs, "Bad QUERY_CUTLIST");
        else
            HandleCutlistQuery(tokens[1], tokens[2], pbs);
    }
    else if (command == "QUERY_BOOKMARK")
    {
        if (tokens.size() != 3)
            SendErrorResponse(pbs, "Bad QUERY_BOOKMARK");
        else
            HandleBookmarkQuery(tokens[1], tokens[2], pbs);
    }
    else if (command == "SET_BOOKMARK")
    {
        if (tokens.size() != 4)
            SendErrorResponse(pbs, "Bad SET_BOOKMARK");
        else
            HandleSetBookmark(tokens, pbs);
    }
    else if (command == "QUERY_SETTING")
    {
        if (tokens.size() != 3)
            SendErrorResponse(pbs, "Bad QUERY_SETTING");
        else
            HandleSettingQuery(tokens, pbs);
    }
    else if (command == "SET_SETTING")
    {
        if (tokens.size() != 4)
            SendErrorResponse(pbs, "Bad SET_SETTING");
        else
            HandleSetSetting(tokens, pbs);
    }
    else if (command == "SCAN_VIDEOS")
    {
        HandleScanVideos(pbs);
    }
    else if (command == "SCAN_MUSIC")
    {
        HandleScanMusic(tokens, pbs);
    }
    else if (command == "MUSIC_TAG_UPDATE_VOLATILE")
    {
        if (listline.size() != 6)
            SendErrorResponse(pbs, "Bad MUSIC_TAG_UPDATE_VOLATILE");
        else
            HandleMusicTagUpdateVolatile(listline, pbs);
    }
    else if (command == "MUSIC_CALC_TRACK_LENGTH")
    {
        if (listline.size() != 3)
            SendErrorResponse(pbs, "Bad MUSIC_CALC_TRACK_LENGTH");
        else
            HandleMusicCalcTrackLen(listline, pbs);
    }
    else if (command == "MUSIC_TAG_UPDATE_METADATA")
    {
        if (listline.size() != 3)
            SendErrorResponse(pbs, "Bad MUSIC_TAG_UPDATE_METADATA");
        else
            HandleMusicTagUpdateMetadata(listline, pbs);
    }
    else if (command == "MUSIC_FIND_ALBUMART")
    {
        if (listline.size() != 4)
            SendErrorResponse(pbs, "Bad MUSIC_FIND_ALBUMART");
        else
            HandleMusicFindAlbumArt(listline, pbs);
    }
    else if (command == "MUSIC_TAG_GETIMAGE")
    {
        if (listline.size() < 4)
            SendErrorResponse(pbs, "Bad MUSIC_TAG_GETIMAGE");
        else
            HandleMusicTagGetImage(listline, pbs);
    }
    else if (command == "MUSIC_TAG_ADDIMAGE")
    {
        if (listline.size() < 5)
            SendErrorResponse(pbs, "Bad MUSIC_TAG_ADDIMAGE");
        else
            HandleMusicTagAddImage(listline, pbs);
    }
    else if (command == "MUSIC_TAG_REMOVEIMAGE")
    {
        if (listline.size() < 4)
            SendErrorResponse(pbs, "Bad MUSIC_TAG_REMOVEIMAGE");
        else
            HandleMusicTagRemoveImage(listline, pbs);
    }
    else if (command == "MUSIC_TAG_CHANGEIMAGE")
    {
        if (listline.size() < 5)
            SendErrorResponse(pbs, "Bad MUSIC_TAG_CHANGEIMAGE");
        else
            HandleMusicTagChangeImage(listline, pbs);
    }
    else if (command == "MUSIC_LYRICS_FIND")
    {
        if (listline.size() < 3)
            SendErrorResponse(pbs, "Bad MUSIC_LYRICS_FIND");
        else
            HandleMusicFindLyrics(listline, pbs);
    }
    else if (command == "MUSIC_LYRICS_GETGRABBERS")
    {
        HandleMusicGetLyricGrabbers(listline, pbs);
    }
    else if (command == "MUSIC_LYRICS_SAVE")
    {
        if (listline.size() < 3)
            SendErrorResponse(pbs, "Bad MUSIC_LYRICS_SAVE");
        else
            HandleMusicSaveLyrics(listline, pbs);
    }
    else if (command == "IMAGE_SCAN")
    {
        // Expects command
        QStringList reply = (listline.size() == 2)
                ? ImageManagerBe::getInstance()->HandleScanRequest(listline[1])
                : QStringList("ERROR") << "Bad: " << listline;

        SendResponse(pbs->getSocket(), reply);
    }
    else if (command == "IMAGE_COPY")
    {
        // Expects at least 1 comma-delimited image definition
        QStringList reply = (listline.size() >= 2)
                ? ImageManagerBe::getInstance()->HandleDbCreate(listline.mid(1))
                : QStringList("ERROR") << "Bad: " << listline;

        SendResponse(pbs->getSocket(), reply);
    }
    else if (command == "IMAGE_MOVE")
    {
        // Expects comma-delimited dir/file ids, path to replace, new path
        QStringList reply = (listline.size() == 4)
                ? ImageManagerBe::getInstance()->
                  HandleDbMove(listline[1], listline[2], listline[3])
                : QStringList("ERROR") << "Bad: " << listline;

        SendResponse(pbs->getSocket(), reply);
    }
    else if (command == "IMAGE_DELETE")
    {
        // Expects comma-delimited dir/file ids
        QStringList reply = (listline.size() == 2)
                ? ImageManagerBe::getInstance()->HandleDelete(listline[1])
                : QStringList("ERROR") << "Bad: " << listline;

        SendResponse(pbs->getSocket(), reply);
    }
    else if (command == "IMAGE_HIDE")
    {
        // Expects hide flag, comma-delimited file/dir ids
        QStringList reply = (listline.size() == 3)
                ? ImageManagerBe::getInstance()->
                  HandleHide(listline[1].toInt(), listline[2])
                : QStringList("ERROR") << "Bad: " << listline;

        SendResponse(pbs->getSocket(), reply);
    }
    else if (command == "IMAGE_TRANSFORM")
    {
        // Expects transformation, write file flag,
        QStringList reply = (listline.size() == 3)
                ? ImageManagerBe::getInstance()->
                  HandleTransform(listline[1].toInt(), listline[2])
                : QStringList("ERROR") << "Bad: " << listline;

        SendResponse(pbs->getSocket(), reply);
    }
    else if (command == "IMAGE_RENAME")
    {
        // Expects file/dir id, new basename
        QStringList reply = (listline.size() == 3)
                ? ImageManagerBe::getInstance()->HandleRename(listline[1], listline[2])
                : QStringList("ERROR") << "Bad: " << listline;

        SendResponse(pbs->getSocket(), reply);
    }
    else if (command == "IMAGE_CREATE_DIRS")
    {
        // Expects destination path, rescan flag, list of dir names
        QStringList reply = (listline.size() >= 4)
                ? ImageManagerBe::getInstance()->
                  HandleDirs(listline[1], listline[2].toInt(), listline.mid(3))
                : QStringList("ERROR") << "Bad: " << listline;

        SendResponse(pbs->getSocket(), reply);
    }
    else if (command == "IMAGE_COVER")
    {
        // Expects dir id, cover id. Cover id of 0 resets dir to use its own
        QStringList reply = (listline.size() == 3)
                ? ImageManagerBe::getInstance()->
                  HandleCover(listline[1].toInt(), listline[2].toInt())
                : QStringList("ERROR") << "Bad: " << listline;

        SendResponse(pbs->getSocket(), reply);
    }
    else if (command == "IMAGE_IGNORE")
    {
        // Expects list of exclusion patterns
        QStringList reply = (listline.size() == 2)
                ? ImageManagerBe::getInstance()->HandleIgnore(listline[1])
                : QStringList("ERROR") << "Bad: " << listline;

        SendResponse(pbs->getSocket(), reply);
    }
    else if (command == "ALLOW_SHUTDOWN")
    {
        if (tokens.size() != 1)
            SendErrorResponse(pbs, "Bad ALLOW_SHUTDOWN");
        else
            HandleBlockShutdown(false, pbs);
    }
    else if (command == "BLOCK_SHUTDOWN")
    {
        if (tokens.size() != 1)
            SendErrorResponse(pbs, "Bad BLOCK_SHUTDOWN");
        else
            HandleBlockShutdown(true, pbs);
    }
    else if (command == "SHUTDOWN_NOW")
    {
        if (tokens.size() != 1)
            SendErrorResponse(pbs, "Bad SHUTDOWN_NOW query");
        else if (!ismaster)
        {
            QString halt_cmd;
            if (listline.size() >= 2)
                halt_cmd = listline[1];

            if (!halt_cmd.isEmpty())
            {
                LOG(VB_GENERAL, LOG_NOTICE, LOC +
                    "Going down now as of Mainserver request!");
                myth_system(halt_cmd);
            }
            else
                SendErrorResponse(pbs, "Received an empty SHUTDOWN_NOW query!");
        }
    }
    else if (command == "BACKEND_MESSAGE")
    {
        QString message = listline[1];
        QStringList extra( listline[2] );
        for (int i = 3; i < listline.size(); i++)
            extra << listline[i];
        MythEvent me(message, extra);
        gCoreContext->dispatch(me);
    }
    else if ((command == "DOWNLOAD_FILE") ||
             (command == "DOWNLOAD_FILE_NOW"))
    {
        if (listline.size() != 4)
            SendErrorResponse(pbs, QString("Bad %1 command").arg(command));
        else
            HandleDownloadFile(listline, pbs);
    }
    else if (command == "REFRESH_BACKEND")
    {
        LOG(VB_GENERAL, LOG_INFO , LOC + "Reloading backend settings");
        HandleBackendRefresh(sock);
    }
    else if (command == "OK")
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Got 'OK' out of sequence.");
    }
    else if (command == "UNKNOWN_COMMAND")
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Got 'UNKNOWN_COMMAND' out of sequence.");
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Unknown command: " + command);

        MythSocket *pbssock = pbs->getSocket();

        QStringList strlist;
        strlist << "UNKNOWN_COMMAND";

        SendResponse(pbssock, strlist);
    }

    pbs->DecrRef();
}

void MainServer::customEvent(QEvent *e)
{
    if (!e)
        return;

    QStringList broadcast;
    QSet<QString> receivers;

    // delete stale sockets in the UI thread
    sockListLock.lockForRead();
    bool decrRefEmpty = decrRefSocketList.empty();
    sockListLock.unlock();
    if (!decrRefEmpty)
    {
        QWriteLocker locker(&sockListLock);
        while (!decrRefSocketList.empty())
        {
            (*decrRefSocketList.begin())->DecrRef();
            decrRefSocketList.erase(decrRefSocketList.begin());
        }
    }

    if ((MythEvent::Type)(e->type()) == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent *)e;

        QString message = me->Message();
        QString error;
        if ((message == "PREVIEW_SUCCESS" || message == "PREVIEW_QUEUED") &&
            me->ExtraDataCount() >= 5)
        {
            bool ok = true;
            uint recordingID  = me->ExtraData(0).toUInt(); // pginfo->GetRecordingID()
            QString filename  = me->ExtraData(1); // outFileName
            QString msg       = me->ExtraData(2);
            QString datetime  = me->ExtraData(3);

            if (message == "PREVIEW_QUEUED")
            {
                LOG(VB_PLAYBACK, LOG_INFO, LOC +
                    QString("Preview Queued: '%1' '%2'")
                        .arg(recordingID).arg(filename));
                return;
            }

            QFile file(filename);
            ok = ok && file.open(QIODevice::ReadOnly);

            if (ok)
            {
                QByteArray data = file.readAll();
                QStringList extra("OK");
                extra.push_back(QString::number(recordingID));
                extra.push_back(msg);
                extra.push_back(datetime);
                extra.push_back(QString::number(data.size()));
                extra.push_back(
                    QString::number(qChecksum(data.constData(), data.size())));
                extra.push_back(QString(data.toBase64()));

                for (uint i = 4 ; i < (uint) me->ExtraDataCount(); i++)
                {
                    QString token = me->ExtraData(i);
                    extra.push_back(token);
                    RequestedBy::iterator it = m_previewRequestedBy.find(token);
                    if (it != m_previewRequestedBy.end())
                    {
                        receivers.insert(*it);
                        m_previewRequestedBy.erase(it);
                    }
                }

                if (receivers.empty())
                {
                    LOG(VB_GENERAL, LOG_ERR, LOC +
                        "PREVIEW_SUCCESS but no receivers.");
                    return;
                }

                broadcast.push_back("BACKEND_MESSAGE");
                broadcast.push_back("GENERATED_PIXMAP");
                broadcast += extra;
            }
            else
            {
                message = "PREVIEW_FAILED";
                error = QString("Failed to read '%1'").arg(filename);
                LOG(VB_GENERAL, LOG_ERR, LOC + error);
            }
        }

        if (message == "PREVIEW_FAILED" && me->ExtraDataCount() >= 5)
        {
            QString pginfokey = me->ExtraData(0); // pginfo->MakeUniqueKey()
            QString msg       = me->ExtraData(2);

            QStringList extra("ERROR");
            extra.push_back(pginfokey);
            extra.push_back(msg);
            for (uint i = 4 ; i < (uint) me->ExtraDataCount(); i++)
            {
                QString token = me->ExtraData(i);
                extra.push_back(token);
                RequestedBy::iterator it = m_previewRequestedBy.find(token);
                if (it != m_previewRequestedBy.end())
                {
                    receivers.insert(*it);
                    m_previewRequestedBy.erase(it);
                }
            }

            if (receivers.empty())
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "PREVIEW_FAILED but no receivers.");
                return;
            }

            broadcast.push_back("BACKEND_MESSAGE");
            broadcast.push_back("GENERATED_PIXMAP");
            broadcast += extra;
        }

        if (me->Message().startsWith("AUTO_EXPIRE"))
        {
            QStringList tokens = me->Message()
                .split(" ", QString::SkipEmptyParts);

            if (tokens.size() != 3)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + "Bad AUTO_EXPIRE message");
                return;
            }

            QDateTime startts = MythDate::fromString(tokens[2]);
            RecordingInfo recInfo(tokens[1].toUInt(), startts);

            if (recInfo.GetChanID())
            {
                SendMythSystemPlayEvent("REC_EXPIRED", &recInfo);

                // allow re-record if auto expired but not expired live
                // or already "deleted" programs
                if (recInfo.GetRecordingGroup() != "LiveTV" &&
                    recInfo.GetRecordingGroup() != "Deleted" &&
                    (gCoreContext->GetNumSetting("RerecordWatched", 0) ||
                     !recInfo.IsWatched()))
                {
                    recInfo.ForgetHistory();
                }
                DoHandleDeleteRecording(recInfo, NULL, false, true, false);
            }
            else
            {
                QString msg = QString("Cannot find program info for '%1', "
                                      "while attempting to Auto-Expire.")
                    .arg(me->Message());
                LOG(VB_GENERAL, LOG_ERR, LOC + msg);
            }

            return;
        }

        if (me->Message().startsWith("QUERY_NEXT_LIVETV_DIR") && m_sched)
        {
            QStringList tokens = me->Message()
                .split(" ", QString::SkipEmptyParts);

            if (tokens.size() != 2)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("Bad %1 message").arg(tokens[0]));
                return;
            }

            m_sched->GetNextLiveTVDir(tokens[1].toInt());
            return;
        }

        if (me->Message().startsWith("STOP_RECORDING"))
        {
            QStringList tokens = me->Message().split(" ",
                                                     QString::SkipEmptyParts);


            if (tokens.size() < 3 || tokens.size() > 3)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("Bad STOP_RECORDING message: %1")
                        .arg(me->Message()));
                return;
            }

            QDateTime startts = MythDate::fromString(tokens[2]);
            RecordingInfo recInfo(tokens[1].toUInt(), startts);

            if (recInfo.GetChanID())
            {
                DoHandleStopRecording(recInfo, NULL);
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("Cannot find program info for '%1' while "
                            "attempting to stop recording.").arg(me->Message()));
            }

            return;
        }

        if ((me->Message().startsWith("DELETE_RECORDING")) ||
            (me->Message().startsWith("FORCE_DELETE_RECORDING")))
        {
            QStringList tokens = me->Message()
                .split(" ", QString::SkipEmptyParts);


            if (tokens.size() < 3 || tokens.size() > 5)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("Bad %1 message").arg(tokens[0]));
                return;
            }

            bool force = (tokens.size() >= 4) && (tokens[3] == "FORCE");
            bool forget = (tokens.size() >= 5) && (tokens[4] == "FORGET");

            QDateTime startts = MythDate::fromString(tokens[2]);
            RecordingInfo recInfo(tokens[1].toUInt(), startts);

            if (recInfo.GetChanID())
            {
                if (tokens[0] == "FORCE_DELETE_RECORDING")
                    DoHandleDeleteRecording(recInfo, NULL, true, false, forget);
                else
                    DoHandleDeleteRecording(recInfo, NULL, force, false, forget);
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("Cannot find program info for '%1' while "
                            "attempting to delete.").arg(me->Message()));
            }

            return;
        }

        if (me->Message().startsWith("UNDELETE_RECORDING"))
        {
            QStringList tokens = me->Message().split(" ",
                                                     QString::SkipEmptyParts);


            if (tokens.size() < 3 || tokens.size() > 3)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("Bad UNDELETE_RECORDING message: %1")
                        .arg(me->Message()));
                return;
            }

            QDateTime startts = MythDate::fromString(tokens[2]);
            RecordingInfo recInfo(tokens[1].toUInt(), startts);

            if (recInfo.GetChanID())
            {
                DoHandleUndeleteRecording(recInfo, NULL);
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("Cannot find program info for '%1' while "
                            "attempting to undelete.").arg(me->Message()));
            }

            return;
        }

        if (me->Message().startsWith("RESCHEDULE_RECORDINGS") && m_sched)
        {
            QStringList request = me->ExtraDataList();
            m_sched->Reschedule(request);
            return;
        }

        if (me->Message().startsWith("SCHEDULER_ADD_RECORDING") && m_sched)
        {
            ProgramInfo pi(me->ExtraDataList());
            if (!pi.GetChanID())
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "Bad SCHEDULER_ADD_RECORDING message");
                return;
            }

            m_sched->AddRecording(pi);
            return;
        }

        if (me->Message().startsWith("UPDATE_RECORDING_STATUS") && m_sched)
        {
            QStringList tokens = me->Message()
                .split(" ", QString::SkipEmptyParts);

            if (tokens.size() != 6)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "Bad UPDATE_RECORDING_STATUS message");
                return;
            }

            uint cardid = tokens[1].toUInt();
            uint chanid = tokens[2].toUInt();
            QDateTime startts = MythDate::fromString(tokens[3]);
            RecStatus::Type recstatus = RecStatus::Type(tokens[4].toInt());
            QDateTime recendts = MythDate::fromString(tokens[5]);
            m_sched->UpdateRecStatus(cardid, chanid, startts,
                                     recstatus, recendts);
            return;
        }

        if (me->Message().startsWith("LIVETV_EXITED"))
        {
            QString chainid = me->ExtraData();
            LiveTVChain *chain = GetExistingChain(chainid);
            if (chain)
                DeleteChain(chain);

            return;
        }

        if (me->Message() == "CLEAR_SETTINGS_CACHE")
            gCoreContext->ClearSettingsCache();

        if (me->Message().startsWith("RESET_IDLETIME") && m_sched)
            m_sched->ResetIdleTime();

        if (me->Message() == "LOCAL_RECONNECT_TO_MASTER")
            masterServerReconnect->start(kMasterServerReconnectTimeout);

        if (me->Message() == "LOCAL_SLAVE_BACKEND_ENCODERS_OFFLINE")
            HandleSlaveDisconnectedEvent(*me);

        if (me->Message().startsWith("LOCAL_"))
            return;

        if (me->Message() == "CREATE_THUMBNAILS")
            ImageManagerBe::getInstance()->HandleCreateThumbnails(me->ExtraDataList());

        if (me->Message() == "IMAGE_GET_METADATA")
            ImageManagerBe::getInstance()->HandleGetMetadata(me->ExtraData());

        MythEvent mod_me("");
        if (me->Message().startsWith("MASTER_UPDATE_REC_INFO"))
        {
            QStringList tokens = me->Message().simplified().split(" ");
            uint recordedid = 0;
            if (tokens.size() >= 2)
                recordedid = tokens[1].toUInt();

            ProgramInfo evinfo(recordedid);
            if (evinfo.GetChanID())
            {
                QDateTime rectime = MythDate::current().addSecs(
                    -gCoreContext->GetNumSetting("RecordOverTime"));

                if (m_sched && evinfo.GetRecordingEndTime() > rectime)
                    evinfo.SetRecordingStatus(m_sched->GetRecStatus(evinfo));

                QStringList list;
                evinfo.ToStringList(list);
                mod_me = MythEvent("RECORDING_LIST_CHANGE UPDATE", list);
                me = &mod_me;
            }
            else
            {
                return;
            }
        }

        if (me->Message().startsWith("DOWNLOAD_FILE"))
        {
            QStringList extraDataList = me->ExtraDataList();
            QString localFile = extraDataList[1];
            QFile file(localFile);
            QStringList tokens = me->Message().simplified().split(" ");
            QMutexLocker locker(&m_downloadURLsLock);

            if (!m_downloadURLs.contains(localFile))
                return;

            extraDataList[1] = m_downloadURLs[localFile];

            if ((tokens.size() >= 2) && (tokens[1] == "FINISHED"))
                m_downloadURLs.remove(localFile);

            mod_me = MythEvent(me->Message(), extraDataList);
            me = &mod_me;
        }

        if (broadcast.empty())
        {
            broadcast.push_back("BACKEND_MESSAGE");
            broadcast.push_back(me->Message());
            broadcast += me->ExtraDataList();
        }
    }

    if (!broadcast.empty())
    {
        // Make a local copy of the list, upping the refcount as we go..
        vector<PlaybackSock *> localPBSList;
        sockListLock.lockForRead();
        vector<PlaybackSock *>::iterator it = playbackList.begin();
        for (; it != playbackList.end(); ++it)
        {
            (*it)->IncrRef();
            localPBSList.push_back(*it);
        }
        sockListLock.unlock();

        bool sendGlobal = false;
        if (ismaster && broadcast[1].startsWith("GLOBAL_"))
        {
            broadcast[1].replace("GLOBAL_", "LOCAL_");
            MythEvent me(broadcast[1], broadcast[2]);
            gCoreContext->dispatch(me);

            sendGlobal = true;
        }

        QSet<PlaybackSock*> sentSet;

        bool isSystemEvent = broadcast[1].startsWith("SYSTEM_EVENT ");
        QStringList sentSetSystemEvent(gCoreContext->GetHostName());

        vector<PlaybackSock*>::const_iterator iter;
        for (iter = localPBSList.begin(); iter != localPBSList.end(); ++iter)
        {
            PlaybackSock *pbs = *iter;

            if (sentSet.contains(pbs) || pbs->IsDisconnected())
                continue;

            if (!receivers.empty() && !receivers.contains(pbs->getHostname()))
                continue;

            sentSet.insert(pbs);

            bool reallysendit = false;

            if (broadcast[1] == "CLEAR_SETTINGS_CACHE")
            {
                if ((ismaster) &&
                    (pbs->isSlaveBackend() || pbs->wantsEvents()))
                    reallysendit = true;
            }
            else if (sendGlobal)
            {
                if (pbs->isSlaveBackend())
                    reallysendit = true;
            }
            else if (pbs->wantsEvents())
            {
                reallysendit = true;
            }

            if (reallysendit)
            {
                if (isSystemEvent)
                {
                    if (!pbs->wantsSystemEvents())
                    {
                        continue;
                    }
                    else if (!pbs->wantsOnlySystemEvents())
                    {
                        if (sentSetSystemEvent.contains(pbs->getHostname()))
                            continue;

                        sentSetSystemEvent << pbs->getHostname();
                    }
                }
                else if (pbs->wantsOnlySystemEvents())
                    continue;
            }

            MythSocket *sock = pbs->getSocket();
            if (reallysendit && sock->IsConnected())
                sock->WriteStringList(broadcast);
        }

        // Done with the pbs list, so decrement all the instances..
        for (iter = localPBSList.begin(); iter != localPBSList.end(); ++iter)
        {
            PlaybackSock *pbs = *iter;
            pbs->DecrRef();
        }
    }
}

/**
 * \addtogroup myth_network_protocol
 * \par        MYTH_PROTO_VERSION \e version \e token
 * Checks that \e version and \e token match the backend's version.
 * If it matches, the stringlist of "ACCEPT" \e "version" is returned.
 * If it does not, "REJECT" \e "version" is returned,
 * and the socket is closed (for this client)
 */
void MainServer::HandleVersion(MythSocket *socket, const QStringList &slist)
{
    QStringList retlist;
    QString version = slist[1];
    if (version != MYTH_PROTO_VERSION)
    {
        LOG(VB_GENERAL, LOG_CRIT, LOC +
            "MainServer::HandleVersion - Client speaks protocol version " +
            version + " but we speak " + MYTH_PROTO_VERSION + '!');
        retlist << "REJECT" << MYTH_PROTO_VERSION;
        socket->WriteStringList(retlist);
        HandleDone(socket);
        return;
    }

    if (slist.size() < 3)
    {
        LOG(VB_GENERAL, LOG_CRIT, LOC +
            "MainServer::HandleVersion - Client did not pass protocol "
            "token. Refusing connection!");
        retlist << "REJECT" << MYTH_PROTO_VERSION;
        socket->WriteStringList(retlist);
        HandleDone(socket);
        return;
    }

    QString token = slist[2];
    if (token != QString::fromUtf8(MYTH_PROTO_TOKEN))
    {
        LOG(VB_GENERAL, LOG_CRIT, LOC +
            QString("MainServer::HandleVersion - Client sent incorrect "
                    "protocol token \"%1\" for protocol version. Refusing "
                    "connection!").arg(token));
        retlist << "REJECT" << MYTH_PROTO_VERSION;
        socket->WriteStringList(retlist);
        HandleDone(socket);
        return;
    }

    retlist << "ACCEPT" << MYTH_PROTO_VERSION;
    socket->WriteStringList(retlist);
}

/**
 * \addtogroup myth_network_protocol
 * \par        ANN Playback \e host \e wantevents
 * Register \e host as a non-frontend client, and prevent shutdown of the socket.
 *
 * \par        ANN Frontend \e host \e wantevents
 * Register \e host as a Frontend client, and allow shutdown of the socket when idle
 *
 * \par        ANN Monitor  \e host \e wantevents
 * Register \e host as a client, and allow shutdown of the socket
 * \par        ANN SlaveBackend \e IPaddress
 * \par        ANN MediaServer \e IPaddress
 * \par        ANN FileTransfer stringlist(\e hostname, \e filename)
 * \par        ANN FileTransfer stringlist(\e hostname, \e filename) \e useReadahead \e retries
 */
void MainServer::HandleAnnounce(QStringList &slist, QStringList commands,
                                MythSocket *socket)
{
    QStringList retlist( "OK" );
    QStringList errlist( "ERROR" );

    if (commands.size() < 3 || commands.size() > 6)
    {
        QString info = "";
        if (commands.size() == 2)
            info = QString(" %1").arg(commands[1]);

        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Received malformed ANN%1 query")
                .arg(info));

        errlist << "malformed_ann_query";
        socket->WriteStringList(errlist);
        return;
    }

    sockListLock.lockForRead();
    vector<PlaybackSock *>::iterator iter = playbackList.begin();
    for (; iter != playbackList.end(); ++iter)
    {
        PlaybackSock *pbs = *iter;
        if (pbs->getSocket() == socket)
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC +
                QString("Client %1 is trying to announce a socket "
                        "multiple times.")
                    .arg(commands[2]));
            socket->WriteStringList(retlist);
            sockListLock.unlock();
            return;
        }
    }
    sockListLock.unlock();

    if (commands[1] == "Playback" || commands[1] == "Monitor" ||
        commands[1] == "Frontend")
    {
        if (commands.size() < 4)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Received malformed ANN %1 query")
                    .arg(commands[1]));

            errlist << "malformed_ann_query";
            socket->WriteStringList(errlist);
            return;
        }

        // Monitor connections are same as Playback but they don't
        // block shutdowns. See the Scheduler event loop for more.

        PlaybackSockEventsMode eventsMode =
            (PlaybackSockEventsMode)commands[3].toInt();

        QWriteLocker lock(&sockListLock);
        if (!controlSocketList.remove(socket))
            return; // socket was disconnected
        PlaybackSock *pbs = new PlaybackSock(this, socket, commands[2],
                                             eventsMode);
        playbackList.push_back(pbs);
        lock.unlock();

        LOG(VB_GENERAL, LOG_INFO, LOC + QString("MainServer::ANN %1")
                                      .arg(commands[1]));
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("adding: %1(%2) as a client (events: %3)")
                                      .arg(commands[2])
                                      .arg(quintptr(socket),0,16)
                                      .arg(eventsMode));
        pbs->setBlockShutdown((commands[1] == "Playback") ||
                              (commands[1] == "Frontend"));

        if (commands[1] == "Frontend")
        {
            pbs->SetAsFrontend();
            Frontend *frontend = new Frontend();
            frontend->name = commands[2];
            // On a combined mbe/fe the frontend will connect using the localhost
            // address, we need the external IP which happily will be the same as
            // the backend's external IP
            if (frontend->name == gCoreContext->GetMasterHostName())
                frontend->ip = QHostAddress(gCoreContext->GetBackendServerIP());
            else
                frontend->ip = socket->GetPeerAddress();
            if (gBackendContext)
                gBackendContext->SetFrontendConnected(frontend);
            else
                delete frontend;
        }

    }
    else if (commands[1] == "MediaServer")
    {
        if (commands.size() < 3)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "Received malformed ANN MediaServer query");
            errlist << "malformed_ann_query";
            socket->WriteStringList(errlist);
            return;
        }

        QWriteLocker lock(&sockListLock);
        if (!controlSocketList.remove(socket))
            return; // socket was disconnected
        PlaybackSock *pbs = new PlaybackSock(this, socket, commands[2],
                                              kPBSEvents_Normal);
        pbs->setAsMediaServer();
        pbs->setBlockShutdown(false);
        playbackList.push_back(pbs);
        lock.unlock();

        gCoreContext->SendSystemEvent(
            QString("CLIENT_CONNECTED HOSTNAME %1").arg(commands[2]));
    }
    else if (commands[1] == "SlaveBackend")
    {
        if (commands.size() < 4)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Received malformed ANN %1 query")
                    .arg(commands[1]));
            errlist << "malformed_ann_query";
            socket->WriteStringList(errlist);
            return;
        }

        QWriteLocker lock(&sockListLock);
        if (!controlSocketList.remove(socket))
            return; // socket was disconnected
        PlaybackSock *pbs = new PlaybackSock(this, socket, commands[2],
                                             kPBSEvents_None);
        playbackList.push_back(pbs);
        lock.unlock();

        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("adding: %1 as a slave backend server")
                               .arg(commands[2]));
        pbs->setAsSlaveBackend();
        pbs->setIP(commands[3]);

        if (m_sched)
        {
            RecordingList slavelist;
            QStringList::const_iterator sit = slist.begin()+1;
            while (sit != slist.end())
            {
                RecordingInfo *recinfo = new RecordingInfo(sit, slist.end());
                if (!recinfo->GetChanID())
                {
                    delete recinfo;
                    break;
                }
                slavelist.push_back(recinfo);
            }
            m_sched->SlaveConnected(slavelist);
        }

        bool wasAsleep = true;
        QMap<int, EncoderLink *>::Iterator iter = encoderList->begin();
        for (; iter != encoderList->end(); ++iter)
        {
            EncoderLink *elink = *iter;
            if (elink->GetHostName() == commands[2])
            {
                if (! (elink->IsWaking() || elink->IsAsleep()))
                    wasAsleep = false;
                elink->SetSocket(pbs);
            }
        }

        if (!wasAsleep && m_sched)
            m_sched->ReschedulePlace("SlaveConnected");

        QString message = QString("LOCAL_SLAVE_BACKEND_ONLINE %2")
                                  .arg(commands[2]);
        MythEvent me(message);
        gCoreContext->dispatch(me);

        pbs->setBlockShutdown(false);

        autoexpireUpdateTimer->start(1000);

        gCoreContext->SendSystemEvent(
            QString("SLAVE_CONNECTED HOSTNAME %1").arg(commands[2]));
    }
    else if (commands[1] == "FileTransfer")
    {
        if (slist.size() < 3)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "Received malformed FileTransfer command");
            errlist << "malformed_filetransfer_command";
            socket->WriteStringList(errlist);
            return;
        }

        LOG(VB_NETWORK, LOG_INFO, LOC +
            "MainServer::HandleAnnounce FileTransfer");
        LOG(VB_NETWORK, LOG_INFO, LOC +
            QString("adding: %1 as a remote file transfer") .arg(commands[2]));
        QStringList::const_iterator it = slist.begin();
        QUrl qurl = *(++it);
        QString wantgroup = *(++it);
        QString filename;
        QStringList checkfiles;
        for (++it; it != slist.end(); ++it)
            checkfiles += *it;

        FileTransfer *ft = NULL;
        bool writemode = false;
        bool usereadahead = true;
        int timeout_ms = 2000;
        if (commands.size() > 3)
            writemode = commands[3].toInt();

        if (commands.size() > 4)
            usereadahead = commands[4].toInt();

        if (commands.size() > 5)
            timeout_ms = commands[5].toInt();

        if (writemode)
        {
            if (wantgroup.isEmpty())
                wantgroup = "Default";

            StorageGroup sgroup(wantgroup, gCoreContext->GetHostName(), false);
            QString dir = sgroup.FindNextDirMostFree();
            if (dir.isEmpty())
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + "Unable to determine directory "
                        "to write to in FileTransfer write command");
                errlist << "filetransfer_directory_not_found";
                socket->WriteStringList(errlist);
                return;
            }

            QString basename = qurl.path();
            if (qurl.hasFragment())
                basename += "#" + qurl.fragment();

            if (basename.isEmpty())
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("FileTransfer write filename is empty in url '%1'.")
                        .arg(qurl.toString()));
                errlist << "filetransfer_filename_empty";
                socket->WriteStringList(errlist);
                return;
            }

            if ((basename.contains("/../")) ||
                (basename.startsWith("../")))
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("FileTransfer write filename '%1' does not pass "
                            "sanity checks.") .arg(basename));
                errlist << "filetransfer_filename_dangerous";
                socket->WriteStringList(errlist);
                return;
            }

            filename = dir + "/" + basename;
        }
        else
            filename = LocalFilePath(qurl, wantgroup);

        if (filename.isEmpty())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Empty filename, cowardly aborting!");
            errlist << "filetransfer_filename_empty";
            socket->WriteStringList(errlist);
            return;
        }


        QFileInfo finfo(filename);
        if (finfo.isDir())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("FileTransfer filename '%1' is actually a directory, "
                        "cannot transfer.") .arg(filename));
            errlist << "filetransfer_filename_is_a_directory";
            socket->WriteStringList(errlist);
            return;
        }

        if (writemode)
        {
            QString dirPath = finfo.absolutePath();
            QDir qdir(dirPath);
            if (!qdir.exists())
            {
                if (!qdir.mkpath(dirPath))
                {
                    LOG(VB_GENERAL, LOG_ERR, LOC +
                        QString("FileTransfer filename '%1' is in a "
                                "subdirectory which does not exist, and can "
                                "not be created.") .arg(filename));
                    errlist << "filetransfer_unable_to_create_subdirectory";
                    socket->WriteStringList(errlist);
                    return;
                }
            }
            QWriteLocker lock(&sockListLock);
            if (!controlSocketList.remove(socket))
                return; // socket was disconnected
            ft = new FileTransfer(filename, socket, writemode);
        }
        else
        {
            QWriteLocker lock(&sockListLock);
            if (!controlSocketList.remove(socket))
                return; // socket was disconnected
            ft = new FileTransfer(filename, socket, usereadahead, timeout_ms);
        }

        if (!ft->isOpen())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Can't open %1").arg(filename));
            errlist << "filetransfer_unable_to_open_file";
            socket->WriteStringList(errlist);
            socket->IncrRef(); // FileTransfer took ownership of the socket, take it back
            ft->DecrRef();
            return;
        }
        ft->IncrRef();
        fileTransferList.push_back(ft);

        retlist << QString::number(socket->GetSocketDescriptor());
        retlist << QString::number(ft->GetFileSize());

        ft->DecrRef();

        if (checkfiles.size())
        {
            QFileInfo fi(filename);
            QDir dir = fi.absoluteDir();
            for (it = checkfiles.begin(); it != checkfiles.end(); ++it)
            {
                if (dir.exists(*it) &&
                    ((*it).endsWith(".srt") ||
                     QFileInfo(dir, *it).size() >= kReadTestSize))
                {
                    retlist<<*it;
                }
            }
        }
    }

    socket->WriteStringList(retlist);
}

/**
 * \addtogroup myth_network_protocol
 * \par        DONE
 * Closes this client's socket.
 */
void MainServer::HandleDone(MythSocket *socket)
{
    socket->DisconnectFromHost();
}

void MainServer::SendErrorResponse(PlaybackSock *pbs, const QString &error)
{
    SendErrorResponse(pbs->getSocket(), error);
}

void MainServer::SendErrorResponse(MythSocket* sock, const QString& error)
{
    LOG(VB_GENERAL, LOG_ERR, LOC + error);

    QStringList strList("ERROR");
    strList << error;

    SendResponse(sock, strList);
}

void MainServer::SendResponse(MythSocket *socket, QStringList &commands)
{
    // Note: this method assumes that the playback or filetransfer
    // handler has already been uprefed and the socket as well.

    // These checks are really just to check if the socket has
    // been remotely disconnected while we were working on the
    // response.

    bool do_write = false;
    if (socket)
    {
        sockListLock.lockForRead();
        do_write = (GetPlaybackBySock(socket) ||
                    GetFileTransferBySock(socket));
        sockListLock.unlock();
    }

    if (do_write)
    {
        socket->WriteStringList(commands);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "SendResponse: Unable to write to client socket, as it's no "
            "longer there");
    }
}

/**
 * \addtogroup myth_network_protocol
 * \par        QUERY_RECORDINGS \e type
 * The \e type parameter can be either "Recording", "Unsorted", "Ascending",
 * or "Descending".
 * Returns programinfo (title, subtitle, description, category, chanid,
 * channum, callsign, channel.name, fileURL, \e et \e cetera)
 */
void MainServer::HandleQueryRecordings(QString type, PlaybackSock *pbs)
{
    MythSocket *pbssock = pbs->getSocket();
    QString playbackhost = pbs->getHostname();

    QMap<QString,ProgramInfo*> recMap;
    if (m_sched)
        recMap = m_sched->GetRecording();

    QMap<QString,uint32_t> inUseMap = ProgramInfo::QueryInUseMap();
    QMap<QString,bool> isJobRunning =
        ProgramInfo::QueryJobsRunning(JOB_COMMFLAG);

    int sort = 0;
    // Allow "Play" and "Delete" for backwards compatibility with protocol
    // version 56 and below.
    if ((type == "Ascending") || (type == "Play"))
        sort = 1;
    else if ((type == "Descending") || (type == "Delete"))
        sort = -1;

    ProgramList destination;
    LoadFromRecorded(
        destination, (type == "Recording"),
        inUseMap, isJobRunning, recMap, sort);

    QMap<QString,ProgramInfo*>::iterator mit = recMap.begin();
    for (; mit != recMap.end(); mit = recMap.erase(mit))
        delete *mit;

    QStringList outputlist(QString::number(destination.size()));
    QMap<QString, QString> backendPortMap;
    QString ip   = gCoreContext->GetBackendServerIP();
    int port = gCoreContext->GetBackendServerPort();
    QString host = gCoreContext->GetHostName();

    ProgramList::iterator it = destination.begin();
    for (it = destination.begin(); it != destination.end(); ++it)
    {
        ProgramInfo *proginfo = *it;
        PlaybackSock *slave = NULL;

        if (proginfo->GetHostname() != gCoreContext->GetHostName())
            slave = GetSlaveByHostname(proginfo->GetHostname());

        if ((proginfo->GetHostname() == gCoreContext->GetHostName()) ||
            (!slave && masterBackendOverride))
        {
            proginfo->SetPathname(gCoreContext->GenMythURL(host,port,proginfo->GetBasename()));
            if (!proginfo->GetFilesize())
            {
                QString tmpURL = GetPlaybackURL(proginfo);
                if (tmpURL.startsWith('/'))
                {
                    QFile checkFile(tmpURL);
                    if (!tmpURL.isEmpty() && checkFile.exists())
                    {
                        proginfo->SetFilesize(checkFile.size());
                        if (proginfo->GetRecordingEndTime() <
                            MythDate::current())
                        {
                            proginfo->SaveFilesize(proginfo->GetFilesize());
                        }
                    }
                }
            }
        }
        else if (!slave)
        {
            proginfo->SetPathname(GetPlaybackURL(proginfo));
            if (proginfo->GetPathname().isEmpty())
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("HandleQueryRecordings() "
                            "Couldn't find backend for:\n\t\t\t%1")
                        .arg(proginfo->toString(ProgramInfo::kTitleSubtitle)));

                proginfo->SetFilesize(0);
                proginfo->SetPathname("file not found");
            }
        }
        else
        {
            if (!proginfo->GetFilesize())
            {
                if (!slave->FillProgramInfo(*proginfo, playbackhost))
                {
                    LOG(VB_GENERAL, LOG_ERR, LOC +
                        "MainServer::HandleQueryRecordings()"
                        "\n\t\t\tCould not fill program info "
                        "from backend");
                }
                else
                {
                    if (proginfo->GetRecordingEndTime() <
                        MythDate::current())
                    {
                        proginfo->SaveFilesize(proginfo->GetFilesize());
                    }
                }
            }
            else
            {
                ProgramInfo *p      = proginfo;
                QString hostname    = p->GetHostname();

                if (!backendPortMap.contains(hostname))
                    backendPortMap[hostname] = gCoreContext->GetBackendServerPort(hostname);

                p->SetPathname(gCoreContext->GenMythURL(hostname,
                                                        backendPortMap[hostname],
                                                        p->GetBasename()));
            }
        }

        if (slave)
            slave->DecrRef();

        proginfo->ToStringList(outputlist);
    }

    SendResponse(pbssock, outputlist);
}

/**
 * \addtogroup myth_network_protocol
 * \par        QUERY_RECORDING BASENAME \e basename
 * \par        QUERY_RECORDING TIMESLOT \e chanid \e starttime
 */
void MainServer::HandleQueryRecording(QStringList &slist, PlaybackSock *pbs)
{
    if (slist.size() < 3)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Bad QUERY_RECORDING query");
        return;
    }

    MythSocket *pbssock = pbs->getSocket();
    QString command = slist[1].toUpper();
    ProgramInfo *pginfo = NULL;

    if (command == "BASENAME")
    {
        pginfo = new ProgramInfo(slist[2]);
    }
    else if (command == "TIMESLOT")
    {
        if (slist.size() < 4)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Bad QUERY_RECORDING query");
            return;
        }

        QDateTime recstartts = MythDate::fromString(slist[3]);
        pginfo = new ProgramInfo(slist[2].toUInt(), recstartts);
    }

    QStringList strlist;

    if (pginfo && pginfo->GetChanID())
    {
        strlist << "OK";
        pginfo->ToStringList(strlist);
    }
    else
    {
        strlist << "ERROR";
    }

    delete pginfo;

    SendResponse(pbssock, strlist);
}

void MainServer::HandleFillProgramInfo(QStringList &slist, PlaybackSock *pbs)
{
    MythSocket *pbssock = pbs->getSocket();

    QString playbackhost = slist[1];

    QStringList::const_iterator it = slist.begin() + 2;
    ProgramInfo pginfo(it, slist.end());

    if (pginfo.HasPathname())
    {
        QString lpath = GetPlaybackURL(&pginfo);
        int port  = gCoreContext->GetBackendServerPort();
        QString host = gCoreContext->GetHostName();

        if (playbackhost == gCoreContext->GetHostName())
            pginfo.SetPathname(lpath);
        else
            pginfo.SetPathname(gCoreContext->GenMythURL(host,port,pginfo.GetBasename()));

        const QFileInfo info(lpath);
        pginfo.SetFilesize(info.size());
    }

    QStringList strlist;

    pginfo.ToStringList(strlist);

    SendResponse(pbssock, strlist);
}


void DeleteThread::run(void)
{
    if (m_ms)
        m_ms->DoDeleteThread(this);
}

void MainServer::DoDeleteThread(DeleteStruct *ds)
{
    // sleep a little to let frontends reload the recordings list
    // after deleting a recording, then we can hammer the DB and filesystem
    sleep(3);
    usleep(random()%2000);

    deletelock.lock();

    QString logInfo = QString("recording id %1 (chanid %2 at %3)")
        .arg(ds->m_recordedid)
        .arg(ds->m_chanid)
        .arg(ds->m_recstartts.toString(Qt::ISODate));

    QString name = QString("deleteThread%1%2").arg(getpid()).arg(random());
    QFile checkFile(ds->m_filename);

    if (!MSqlQuery::testDBConnection())
    {
        QString msg = QString("ERROR opening database connection for Delete "
                              "Thread for chanid %1 recorded at %2.  Program "
                              "will NOT be deleted.")
            .arg(ds->m_chanid)
            .arg(ds->m_recstartts.toString(Qt::ISODate));
        LOG(VB_GENERAL, LOG_ERR, LOC + msg);

        deletelock.unlock();
        return;
    }

    ProgramInfo pginfo(ds->m_chanid, ds->m_recstartts);

    if (!pginfo.GetChanID())
    {
        QString msg = QString("ERROR retrieving program info when trying to "
                              "delete program for chanid %1 recorded at %2. "
                              "Recording will NOT be deleted.")
            .arg(ds->m_chanid)
            .arg(ds->m_recstartts.toString(Qt::ISODate));
        LOG(VB_GENERAL, LOG_ERR, LOC + msg);

        deletelock.unlock();
        return;
    }

    // Don't allow deleting files where filesize != 0 and we can't find
    // the file, unless forceMetadataDelete has been set. This allows
    // deleting failed recordings without fuss, but blocks accidental
    // deletion of metadata for files where the filesystem has gone missing.
    if ((!checkFile.exists()) && pginfo.GetFilesize() &&
        (!ds->m_forceMetadataDelete))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("ERROR when trying to delete file: %1. File "
                    "doesn't exist.  Database metadata will not be removed.")
                .arg(ds->m_filename));

        pginfo.SaveDeletePendingFlag(false);
        deletelock.unlock();
        return;
    }

    JobQueue::DeleteAllJobs(ds->m_chanid, ds->m_recstartts);

    LiveTVChain *tvchain = GetChainWithRecording(pginfo);
    if (tvchain)
        tvchain->DeleteProgram(&pginfo);

    bool followLinks = gCoreContext->GetNumSetting("DeletesFollowLinks", 0);
    bool slowDeletes = gCoreContext->GetNumSetting("TruncateDeletesSlowly", 0);
    int fd = -1;
    off_t size = 0;
    bool errmsg = false;

    //-----------------------------------------------------------------------
    // TODO Move the following into DeleteRecordedFiles
    //-----------------------------------------------------------------------

    // Delete recording.
    if (slowDeletes)
    {
        // Since stat fails after unlinking on some filesystems,
        // get the filesize first
        const QFileInfo info(ds->m_filename);
        size = info.size();
        fd = DeleteFile(ds->m_filename, followLinks, ds->m_forceMetadataDelete);

        if ((fd < 0) && checkFile.exists())
            errmsg = true;
    }
    else
    {
        delete_file_immediately(ds->m_filename, followLinks, false);
        sleep(2);
        if (checkFile.exists())
            errmsg = true;
    }

    if (errmsg)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Error deleting file: %1. Keeping metadata in database.")
                    .arg(ds->m_filename));

        pginfo.SaveDeletePendingFlag(false);
        deletelock.unlock();
        return;
    }

    // Delete all related files, though not the recording itself
    // i.e. preview thumbnails, srt subtitles, orphaned transcode temporary
    //      files
    //
    // TODO: Delete everything with this basename to catch stray
    //       .tmp and .old files, and future proof it
    QFileInfo fInfo( ds->m_filename );
    QStringList nameFilters;
    nameFilters.push_back(fInfo.fileName() + "*.png");
    nameFilters.push_back(fInfo.fileName() + "*.jpg");
    nameFilters.push_back(fInfo.fileName() + ".tmp");
    nameFilters.push_back(fInfo.fileName() + ".old");
    nameFilters.push_back(fInfo.fileName() + ".map");
    nameFilters.push_back(fInfo.fileName() + ".tmp.map");
    nameFilters.push_back(fInfo.baseName() + ".srt");  // e.g. 1234_20150213165800.srt

    QDir dir (fInfo.path());
    QFileInfoList miscFiles = dir.entryInfoList(nameFilters);

    for (int nIdx = 0; nIdx < miscFiles.size(); nIdx++)
    {
        QString sFileName = miscFiles.at(nIdx).absoluteFilePath();
        delete_file_immediately( sFileName, followLinks, true);
    }
    // -----------------------------------------------------------------------

    // TODO Have DeleteRecordedFiles do the deletion of all associated files
    DeleteRecordedFiles(ds);

    DoDeleteInDB(ds);

    deletelock.unlock();

    if (slowDeletes && fd >= 0)
        TruncateAndClose(&pginfo, fd, ds->m_filename, size);
}

void MainServer::DeleteRecordedFiles(DeleteStruct *ds)
{
    QString logInfo = QString("recording id %1 filename %2")
        .arg(ds->m_recordedid).arg(ds->m_filename);

    LOG(VB_GENERAL, LOG_NOTICE, "DeleteRecordedFiles - " + logInfo);

    MSqlQuery update(MSqlQuery::InitCon());
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT basename, hostname, storagegroup FROM recordedfile "
                  "WHERE recordedid = :RECORDEDID;");
    query.bindValue(":RECORDEDID", ds->m_recordedid);

    if (!query.exec() || !query.size())
    {
        MythDB::DBError("RecordedFiles deletion", query);
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Error querying recordedfiles for %1.") .arg(logInfo));
    }

    QString basename;
    QString hostname;
    QString storagegroup;
    bool deleteInDB;
    while (query.next())
    {
        basename = query.value(0).toString();
        hostname = query.value(1).toString();
        storagegroup = query.value(2).toString();
        deleteInDB = false;

        if (basename == QFileInfo(ds->m_filename).fileName())
            deleteInDB = true;
        else
        {
//             LOG(VB_FILE, LOG_INFO, LOC +
//                 QString("DeleteRecordedFiles(%1), deleting '%2'")
//                     .arg(logInfo).arg(query.value(0).toString()));
//
//             StorageGroup sgroup(storagegroup);
//             QString localFile = sgroup.FindFile(basename);
//
//             QString url = gCoreContext->GenMythURL(hostname,
//                                   gCoreContext->GetBackendServerPort(hostname),
//                                   basename,
//                                   storagegroup);
//
//             if ((((hostname == gCoreContext->GetHostName()) ||
//                   (!localFile.isEmpty())) &&
//                  (HandleDeleteFile(basename, storagegroup))) ||
//                 (((hostname != gCoreContext->GetHostName()) ||
//                   (localFile.isEmpty())) &&
//                  (RemoteFile::DeleteFile(url))))
//             {
//                 deleteInDB = true;
//             }
        }

        if (deleteInDB)
        {
            update.prepare("DELETE FROM recordedfile "
                           "WHERE recordedid = :RECORDEDID "
                               "AND basename = :BASENAME ;");
            update.bindValue(":RECORDEDID", ds->m_recordedid);
            update.bindValue(":BASENAME", basename);
            if (!update.exec())
            {
                MythDB::DBError("RecordedFiles deletion", update);
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("Error querying recordedfile (%1) for %2.")
                                .arg(query.value(1).toString())
                                .arg(logInfo));
            }
        }
    }
}

void MainServer::DoDeleteInDB(DeleteStruct *ds)
{
    QString logInfo = QString("recording id %1 (chanid %2 at %3)")
        .arg(ds->m_recordedid)
        .arg(ds->m_chanid).arg(ds->m_recstartts.toString(Qt::ISODate));

    LOG(VB_GENERAL, LOG_NOTICE, "DoDeleteINDB - " + logInfo);

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM recorded WHERE recordedid = :RECORDEDID AND "
                  "title = :TITLE;");
    query.bindValue(":RECORDEDID", ds->m_recordedid);
    query.bindValue(":TITLE", ds->m_title);

    if (!query.exec() || !query.size())
    {
        MythDB::DBError("Recorded program deletion", query);
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Error deleting recorded entry for %1.") .arg(logInfo));
    }

    sleep(1);

    // Notify the frontend so it can requery for Free Space
    QString msg = QString("RECORDING_LIST_CHANGE DELETE %1")
        .arg(ds->m_recordedid);
    gCoreContext->SendEvent(MythEvent(msg));

    // sleep a little to let frontends reload the recordings list
    sleep(3);

    query.prepare("DELETE FROM recordedmarkup "
                  "WHERE chanid = :CHANID AND starttime = :STARTTIME;");
    query.bindValue(":CHANID", ds->m_chanid);
    query.bindValue(":STARTTIME", ds->m_recstartts);

    if (!query.exec())
    {
        MythDB::DBError("Recorded program delete recordedmarkup", query);
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Error deleting recordedmarkup for %1.") .arg(logInfo));
    }

    query.prepare("DELETE FROM recordedseek "
                  "WHERE chanid = :CHANID AND starttime = :STARTTIME;");
    query.bindValue(":CHANID", ds->m_chanid);
    query.bindValue(":STARTTIME", ds->m_recstartts);

    if (!query.exec())
    {
        MythDB::DBError("Recorded program delete recordedseek", query);
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Error deleting recordedseek for %1.")
                .arg(logInfo));
    }
}

/**
 *  \brief Deletes links and unlinks the main file and returns the descriptor.
 *
 *  This is meant to be used with TruncateAndClose() to slowly shrink a
 *  large file and then eventually delete the file by closing the file
 *  descriptor.
 *
 *  \return fd for success, -1 for error, -2 for only a symlink deleted.
 */
int MainServer::DeleteFile(const QString &filename, bool followLinks,
                           bool deleteBrokenSymlinks)
{
    QFileInfo finfo(filename);
    int fd = -1, err = 0;
    QString linktext = "";
    QByteArray fname = filename.toLocal8Bit();

    LOG(VB_FILE, LOG_INFO, LOC +
        QString("About to unlink/delete file: '%1'")
            .arg(fname.constData()));

    QString errmsg = QString("Delete Error '%1'").arg(fname.constData());
    if (finfo.isSymLink())
    {
        linktext = getSymlinkTarget(filename);
        QByteArray alink = linktext.toLocal8Bit();
        errmsg += QString(" -> '%2'").arg(alink.constData());
    }

    if (followLinks && finfo.isSymLink())
    {
        if (!finfo.exists() && deleteBrokenSymlinks)
            err = unlink(fname.constData());
        else
        {
            fd = OpenAndUnlink(linktext);
            if (fd >= 0)
                err = unlink(fname.constData());
        }
    }
    else if (!finfo.isSymLink())
    {
        fd = OpenAndUnlink(filename);
    }
    else // just delete symlinks immediately
    {
        err = unlink(fname.constData());
        if (err == 0)
            return -2; // valid result, not an error condition
    }

    if (fd < 0)
        LOG(VB_GENERAL, LOG_ERR, LOC + errmsg + ENO);

    return fd;
}

/** \fn MainServer::OpenAndUnlink(const QString&)
 *  \brief Opens a file, unlinks it and returns the file descriptor.
 *
 *  This is used by DeleteFile(const QString&,bool) to delete recordings.
 *  In order to actually delete the file from the filesystem the user of
 *  this function must close the return file descriptor.
 *
 *  \return fd for success, negative number for error.
 */
int MainServer::OpenAndUnlink(const QString &filename)
{
    QByteArray fname = filename.toLocal8Bit();
    QString msg = QString("Error deleting '%1'").arg(fname.constData());
    int fd = open(fname.constData(), O_WRONLY);

    if (fd == -1)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + msg + " could not open " + ENO);
        return -1;
    }

    if (unlink(fname.constData()))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + msg + " could not unlink " + ENO);
        close(fd);
        return -1;
    }

    return fd;
}

/**
 *  \brief Repeatedly truncate an open file in small increments.
 *
 *   When the file is small enough this closes the file and returns.
 *
 *   NOTE: This acquires a lock so that only one instance of TruncateAndClose()
 *         is running at a time.
 */
bool MainServer::TruncateAndClose(ProgramInfo *pginfo, int fd,
                                  const QString &filename, off_t fsize)
{
    QMutexLocker locker(&truncate_and_close_lock);

    if (pginfo)
    {
        pginfo->SetPathname(filename);
        pginfo->MarkAsInUse(true, kTruncatingDeleteInUseID);
    }

    int cards = 5;
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT COUNT(cardid) FROM capturecard;");
        if (query.exec() && query.next())
            cards = query.value(0).toInt();
    }

    // Time between truncation steps in milliseconds
    const size_t sleep_time = 500;
    const size_t min_tps    = 8 * 1024 * 1024;
    const size_t calc_tps   = (size_t) (cards * 1.2 * (22200000LL / 8));
    const size_t tps = max(min_tps, calc_tps);
    const size_t increment  = (size_t) (tps * (sleep_time * 0.001f));

    LOG(VB_FILE, LOG_INFO, LOC +
        QString("Truncating '%1' by %2 MB every %3 milliseconds")
            .arg(filename)
            .arg(increment / (1024.0 * 1024.0), 0, 'f', 2)
            .arg(sleep_time));

    GetMythDB()->GetDBManager()->PurgeIdleConnections(false);

    int count = 0;
    while (fsize > 0)
    {
#if 0
        LOG(VB_FILE, LOG_DEBUG, LOC + QString("Truncating '%1' to %2 MB")
                .arg(filename).arg(fsize / (1024.0 * 1024.0), 0, 'f', 2));
#endif

        int err = ftruncate(fd, fsize);
        if (err)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("Error truncating '%1'")
                    .arg(filename) + ENO);
            if (pginfo)
                pginfo->MarkAsInUse(false, kTruncatingDeleteInUseID);
            return 0 == close(fd);
        }

        fsize -= increment;

        if (pginfo && ((count % 100) == 0))
            pginfo->UpdateInUseMark(true);

        count++;

        usleep(sleep_time * 1000);
    }

    bool ok = (0 == close(fd));

    if (pginfo)
        pginfo->MarkAsInUse(false, kTruncatingDeleteInUseID);

    LOG(VB_FILE, LOG_INFO, LOC +
        QString("Finished truncating '%1'").arg(filename));

    return ok;
}

void MainServer::HandleCheckRecordingActive(QStringList &slist,
                                            PlaybackSock *pbs)
{
    MythSocket *pbssock = NULL;
    if (pbs)
        pbssock = pbs->getSocket();

    QStringList::const_iterator it = slist.begin() + 1;
    ProgramInfo pginfo(it, slist.end());

    int result = 0;

    if (ismaster && pginfo.GetHostname() != gCoreContext->GetHostName())
    {
        PlaybackSock *slave = GetSlaveByHostname(pginfo.GetHostname());
        if (slave)
        {
            result = slave->CheckRecordingActive(&pginfo);
            slave->DecrRef();
        }
    }
    else
    {
        QMap<int, EncoderLink *>::Iterator iter = encoderList->begin();
        for (; iter != encoderList->end(); ++iter)
        {
            EncoderLink *elink = *iter;

            if (elink->IsLocal() && elink->MatchesRecording(&pginfo))
                result = iter.key();
        }
    }

    QStringList outputlist( QString::number(result) );
    if (pbssock)
        SendResponse(pbssock, outputlist);

    return;
}

void MainServer::HandleStopRecording(QStringList &slist, PlaybackSock *pbs)
{
    QStringList::const_iterator it = slist.begin() + 1;
    RecordingInfo recinfo(it, slist.end());
    if (recinfo.GetChanID())
    {
        if (ismaster)
        {
            // Stop recording may have been called for the same program on
            // different channel in the guide, we need to find the actual channel
            // that the recording is occurring on. This only needs doing once
            // on the master backend, as the correct chanid will then be sent
            // to the slave
            ProgramList schedList;
            bool hasConflicts = false;
            LoadFromScheduler(schedList, hasConflicts);
            for( uint n = 0; n < schedList.size(); n++)
            {
                ProgramInfo *pInfo = schedList[n];
                if ((pInfo->GetRecordingStatus() == RecStatus::Tuning ||
                     pInfo->GetRecordingStatus() == RecStatus::Failing ||
                     pInfo->GetRecordingStatus() == RecStatus::Recording)
                    && recinfo.IsSameProgram(*pInfo))
                    recinfo.SetChanID(pInfo->GetChanID());
            }
        }
        DoHandleStopRecording(recinfo, pbs);
    }
}

void MainServer::DoHandleStopRecording(
    RecordingInfo &recinfo, PlaybackSock *pbs)
{
    MythSocket *pbssock = NULL;
    if (pbs)
        pbssock = pbs->getSocket();

    // FIXME!  We don't know what state the recorder is in at this
    // time.  Simply set the recstatus to RecStatus::Unknown and let the
    // scheduler do the best it can with it.  The proper long term fix
    // is probably to have the recorder return the actual recstatus as
    // part of the stop recording response.  That's a more involved
    // change than I care to make during the 0.25 code freeze.
    recinfo.SetRecordingStatus(RecStatus::Unknown);

    if (ismaster && recinfo.GetHostname() != gCoreContext->GetHostName())
    {
        PlaybackSock *slave = GetSlaveByHostname(recinfo.GetHostname());

        if (slave)
        {
            int num = slave->StopRecording(&recinfo);

            if (num > 0)
            {
                if (encoderList->contains(num))
                {
                    (*encoderList)[num]->StopRecording();
                }
                if (m_sched)
                    m_sched->UpdateRecStatus(&recinfo);
            }
            if (pbssock)
            {
                QStringList outputlist( "0" );
                SendResponse(pbssock, outputlist);
            }

            slave->DecrRef();
            return;
        }
        else
        {
            // If the slave is unreachable, we can assume that the
            // recording has stopped and the status should be updated.
            // Continue so that the master can try to update the endtime
            // of the file is in a shared directory.
            if (m_sched)
                m_sched->UpdateRecStatus(&recinfo);
        }

    }

    int recnum = -1;

    QMap<int, EncoderLink *>::Iterator iter = encoderList->begin();
    for (; iter != encoderList->end(); ++iter)
    {
        EncoderLink *elink = *iter;

        if (elink->IsLocal() && elink->MatchesRecording(&recinfo))
        {
            recnum = iter.key();

            elink->StopRecording();

            while (elink->IsBusyRecording() ||
                   elink->GetState() == kState_ChangingState)
            {
                usleep(100);
            }

            if (ismaster)
            {
                if (m_sched)
                    m_sched->UpdateRecStatus(&recinfo);
            }
        }
    }

    if (pbssock)
    {
        QStringList outputlist( QString::number(recnum) );
        SendResponse(pbssock, outputlist);
    }
}

void MainServer::HandleDeleteRecording(QString &chanid, QString &starttime,
                                       PlaybackSock *pbs,
                                       bool forceMetadataDelete,
                                       bool forgetHistory)
{
    QDateTime recstartts = MythDate::fromString(starttime);
    RecordingInfo recinfo(chanid.toUInt(), recstartts);

    if (!recinfo.GetRecordingID())
    {
        qDebug() << "HandleDeleteRecording(chanid, starttime) Empty Recording ID";
    }

    if (!recinfo.GetChanID()) // !recinfo.GetRecordingID()
    {
        MythSocket *pbssock = NULL;
        if (pbs)
            pbssock = pbs->getSocket();

        QStringList outputlist( QString::number(0) );

        SendResponse(pbssock, outputlist);
        return;
    }

    DoHandleDeleteRecording(recinfo, pbs, forceMetadataDelete, false, forgetHistory);
}

void MainServer::HandleDeleteRecording(QStringList &slist, PlaybackSock *pbs,
                                       bool forceMetadataDelete)
{
    QStringList::const_iterator it = slist.begin() + 1;
    RecordingInfo recinfo(it, slist.end());

    if (!recinfo.GetRecordingID())
    {
        qDebug() << "HandleDeleteRecording(QStringList) Empty Recording ID";
    }

    if (recinfo.GetChanID()) // !recinfo.GetRecordingID()
        DoHandleDeleteRecording(recinfo, pbs, forceMetadataDelete, false, false);
}

void MainServer::DoHandleDeleteRecording(
    RecordingInfo &recinfo, PlaybackSock *pbs,
    bool forceMetadataDelete, bool expirer, bool forgetHistory)
{
    int resultCode = -1;
    MythSocket *pbssock = NULL;
    if (pbs)
        pbssock = pbs->getSocket();

    bool justexpire = expirer ? false :
            ( //gCoreContext->GetNumSetting("AutoExpireInsteadOfDelete") &&
             (recinfo.GetRecordingGroup() != "Deleted") &&
             (recinfo.GetRecordingGroup() != "LiveTV"));

    QString filename = GetPlaybackURL(&recinfo, false);
    if (filename.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("ERROR when trying to delete file for %1.  Unable "
                    "to determine filename of recording.")
                .arg(recinfo.toString(ProgramInfo::kRecordingKey)));

        if (pbssock)
        {
            resultCode = -2;
            QStringList outputlist(QString::number(resultCode));
            SendResponse(pbssock, outputlist);
        }

        return;
    }

    // Stop the recording if it's still in progress.
    DoHandleStopRecording(recinfo, NULL);

    if (justexpire && !forceMetadataDelete &&
        recinfo.GetFilesize() > (1024 * 1024) )
    {
        recinfo.ApplyRecordRecGroupChange("Deleted");
        recinfo.SaveAutoExpire(kDeletedAutoExpire, true);
        if (forgetHistory)
            recinfo.ForgetHistory();
        else if (m_sched)
            m_sched->RescheduleCheck(recinfo, "DoHandleDelete1");
        QStringList outputlist( QString::number(0) );
        SendResponse(pbssock, outputlist);
        return;
    }

    // If this recording was made by a another recorder, and that
    // recorder is available, tell it to do the deletion.
    if (ismaster && recinfo.GetHostname() != gCoreContext->GetHostName())
    {
        PlaybackSock *slave = GetSlaveByHostname(recinfo.GetHostname());

        if (slave)
        {
            int num = slave->DeleteRecording(&recinfo, forceMetadataDelete);

            if (forgetHistory)
                recinfo.ForgetHistory();
            else if (m_sched &&
                     recinfo.GetRecordingGroup() != "Deleted" &&
                     recinfo.GetRecordingGroup() != "LiveTV")
                m_sched->RescheduleCheck(recinfo, "DoHandleDelete2");

            if (pbssock)
            {
                QStringList outputlist( QString::number(num) );
                SendResponse(pbssock, outputlist);
            }

            slave->DecrRef();
            return;
        }
    }

    QFile checkFile(filename);
    bool fileExists = checkFile.exists();
    if (!fileExists)
    {
        QFile checkFileUTF8(QString::fromUtf8(filename.toLatin1().constData()));
        fileExists = checkFileUTF8.exists();
        if (fileExists)
            filename = QString::fromUtf8(filename.toLatin1().constData());
    }

    // Allow deleting of files where the recording failed meaning size == 0
    // But do not allow deleting of files that appear to be completely absent.
    // The latter condition indicates the filesystem containing the file is
    // most likely absent and deleting the file metadata is unsafe.
    if (fileExists || !recinfo.GetFilesize() || forceMetadataDelete)
    {
        recinfo.SaveDeletePendingFlag(true);

        if (!recinfo.GetRecordingID())
        {
            qDebug() << "DoHandleDeleteRecording() Empty Recording ID";
        }

        DeleteThread *deleteThread = new DeleteThread(this, filename,
            recinfo.GetTitle(), recinfo.GetChanID(),
            recinfo.GetRecordingStartTime(), recinfo.GetRecordingEndTime(),
            recinfo.GetRecordingID(),
            forceMetadataDelete);
        deleteThread->start();
    }
    else
    {
        QString logInfo = QString("chanid %1")
            .arg(recinfo.toString(ProgramInfo::kRecordingKey));

        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("ERROR when trying to delete file: %1. File doesn't "
                    "exist.  Database metadata will not be removed.")
                        .arg(filename));
        resultCode = -2;
    }

    if (pbssock)
    {
        QStringList outputlist( QString::number(resultCode) );
        SendResponse(pbssock, outputlist);
    }

    if (forgetHistory)
        recinfo.ForgetHistory();
    else if (m_sched &&
             recinfo.GetRecordingGroup() != "Deleted" &&
             recinfo.GetRecordingGroup() != "LiveTV")
        m_sched->RescheduleCheck(recinfo, "DoHandleDelete3");

    // Tell MythTV frontends that the recording list needs to be updated.
    if (fileExists || !recinfo.GetFilesize() || forceMetadataDelete)
    {
        gCoreContext->SendSystemEvent(
            QString("REC_DELETED CHANID %1 STARTTIME %2")
            .arg(recinfo.GetChanID())
            .arg(recinfo.GetRecordingStartTime(MythDate::ISODate)));

        recinfo.SendDeletedEvent();
    }
}

void MainServer::HandleUndeleteRecording(QStringList &slist, PlaybackSock *pbs)
{
    if (slist.size() == 3)
    {
        RecordingInfo recinfo(
            slist[1].toUInt(), MythDate::fromString(slist[2]));
        if (recinfo.GetChanID())
            DoHandleUndeleteRecording(recinfo, pbs);
    }
    else if (slist.size() >= (1 + NUMPROGRAMLINES))
    {
        QStringList::const_iterator it = slist.begin()+1;
        RecordingInfo recinfo(it, slist.end());
        if (recinfo.GetChanID())
            DoHandleUndeleteRecording(recinfo, pbs);
    }
}

void MainServer::DoHandleUndeleteRecording(
    RecordingInfo &recinfo, PlaybackSock *pbs)
{
    int ret = -1;

    MythSocket *pbssock = NULL;
    if (pbs)
        pbssock = pbs->getSocket();

#if 0
    if (gCoreContext->GetNumSetting("AutoExpireInsteadOfDelete", 0))
    {
#endif
        recinfo.ApplyRecordRecGroupChange("Default");
        recinfo.UpdateLastDelete(false);
        recinfo.SaveAutoExpire(kDisableAutoExpire);
        if (m_sched)
            m_sched->RescheduleCheck(recinfo, "DoHandleUndelete");
        ret = 0;
#if 0
    }
#endif

    QStringList outputlist( QString::number(ret) );
    SendResponse(pbssock, outputlist);
}

void MainServer::HandleRescheduleRecordings(const QStringList &request,
                                            PlaybackSock *pbs)
{
     ScheduledRecording::RescheduleMatch(0, 0, 0, QDateTime(),
                                         "HandleRescheduleRecordings");

    if (pbs)
    {
        QStringList result  = QStringList( QString::number(1) );
        MythSocket *pbssock = pbs->getSocket();
        if (pbssock)
            SendResponse(pbssock, result);
    }
}

void MainServer::HandleForgetRecording(QStringList &slist, PlaybackSock *pbs)
{
    QStringList::const_iterator it = slist.begin() + 1;
    RecordingInfo recinfo(it, slist.end());
    if (recinfo.GetChanID())
        recinfo.ForgetHistory();

    MythSocket *pbssock = NULL;
    if (pbs)
        pbssock = pbs->getSocket();
    if (pbssock)
    {
        QStringList outputlist( QString::number(0) );
        SendResponse(pbssock, outputlist);
    }
}

/*
 * \addtogroup myth_network_protocol
 * \par        GO_TO_SLEEP
 * Commands a slave to go to sleep
 */
void MainServer::HandleGoToSleep(PlaybackSock *pbs)
{
    QStringList strlist;

    QString sleepCmd = gCoreContext->GetSetting("SleepCommand");
    if (!sleepCmd.isEmpty())
    {
        strlist << "OK";
        SendResponse(pbs->getSocket(), strlist);
        LOG(VB_GENERAL, LOG_NOTICE, LOC +
            "Received GO_TO_SLEEP command from master, running SleepCommand.");
        myth_system(sleepCmd);
    }
    else
    {
        strlist << "ERROR: SleepCommand is empty";
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "ERROR: in HandleGoToSleep(), but no SleepCommand found!");
        SendResponse(pbs->getSocket(), strlist);
    }
}

/**
 * \addtogroup myth_network_protocol
 * \par        QUERY_FREE_SPACE
 * Returns the free space on this backend, as a list of hostname, directory,
 * 1, -1, total size, used (both in K and 64bit, so two 32bit numbers each).
 * \par        QUERY_FREE_SPACE_LIST
 * Returns the free space on \e all hosts. (each host as above,
 * except that the directory becomes a URL, and a TotalDiskSpace is appended)
 */
void MainServer::HandleQueryFreeSpace(PlaybackSock *pbs, bool allHosts)
{
    QStringList strlist;

    if (allHosts)
    {
        QMutexLocker locker(&masterFreeSpaceListLock);
        strlist = masterFreeSpaceList;
        if (!masterFreeSpaceListUpdater ||
            !masterFreeSpaceListUpdater->KeepRunning(true))
        {
            while (masterFreeSpaceListUpdater)
            {
                masterFreeSpaceListUpdater->KeepRunning(false);
                masterFreeSpaceListWait.wait(locker.mutex());
            }
            masterFreeSpaceListUpdater = new FreeSpaceUpdater(*this);
            MThreadPool::globalInstance()->startReserved(
                masterFreeSpaceListUpdater, "FreeSpaceUpdater");
        }
    }
    else
        BackendQueryDiskSpace(strlist, allHosts, allHosts);

    SendResponse(pbs->getSocket(), strlist);
}

/**
 * \addtogroup myth_network_protocol
 * \par        QUERY_FREE_SPACE_SUMMARY
 * Summarises the free space on this backend, as list of total size, used
 */
void MainServer::HandleQueryFreeSpaceSummary(PlaybackSock *pbs)
{
    QStringList strlist;
    {
        QMutexLocker locker(&masterFreeSpaceListLock);
        strlist = masterFreeSpaceList;
        if (!masterFreeSpaceListUpdater ||
            !masterFreeSpaceListUpdater->KeepRunning(true))
        {
            while (masterFreeSpaceListUpdater)
            {
                masterFreeSpaceListUpdater->KeepRunning(false);
                masterFreeSpaceListWait.wait(locker.mutex());
            }
            masterFreeSpaceListUpdater = new FreeSpaceUpdater(*this);
            MThreadPool::globalInstance()->startReserved(
                masterFreeSpaceListUpdater, "FreeSpaceUpdater");
        }
    }

    // The TotalKB and UsedKB are the last two numbers encoded in the list
    QStringList shortlist;
    if (strlist.size() < 4)
    {
        shortlist << QString("0");
        shortlist << QString("0");
    }
    else
    {
        unsigned int index = (uint)(strlist.size()) - 2;
        shortlist << strlist[index++];
        shortlist << strlist[index++];
    }

    SendResponse(pbs->getSocket(), shortlist);
}

/**
 * \addtogroup myth_network_protocol
 * \par        QUERY_LOAD
 * Returns the Unix load on this backend
 * (three floats - the average over 1, 5 and 15 mins).
 */
void MainServer::HandleQueryLoad(PlaybackSock *pbs)
{
    MythSocket *pbssock = pbs->getSocket();

    QStringList strlist;

#ifdef Q_OS_ANDROID
    strlist << "0" << "0" << "0";
#else
    double loads[3];
    if (getloadavg(loads,3) == -1)
    {
        strlist << "ERROR";
        strlist << "getloadavg() failed";
    }
    else
        strlist << QString::number(loads[0])
                << QString::number(loads[1])
                << QString::number(loads[2]);
#endif

    SendResponse(pbssock, strlist);
}

/**
 * \addtogroup myth_network_protocol
 * \par        QUERY_UPTIME
 * Returns the number of seconds this backend's host has been running
 */
void MainServer::HandleQueryUptime(PlaybackSock *pbs)
{
    MythSocket    *pbssock = pbs->getSocket();
    QStringList strlist;
    time_t      uptime;

    if (getUptime(uptime))
        strlist << QString::number(uptime);
    else
    {
        strlist << "ERROR";
        strlist << "Could not determine uptime.";
    }

    SendResponse(pbssock, strlist);
}

/**
 * \addtogroup myth_network_protocol
 * \par        QUERY_HOSTNAME
 * Returns the hostname of this backend
 */
void MainServer::HandleQueryHostname(PlaybackSock *pbs)
{
    MythSocket    *pbssock = pbs->getSocket();
    QStringList strlist;

    strlist << gCoreContext->GetHostName();

    SendResponse(pbssock, strlist);
}

/**
 * \addtogroup myth_network_protocol
 * \par        QUERY_MEMSTATS
 * Returns total RAM, free RAM, total VM and free VM (all in MB)
 */
void MainServer::HandleQueryMemStats(PlaybackSock *pbs)
{
    MythSocket    *pbssock = pbs->getSocket();
    QStringList strlist;
    int         totalMB, freeMB, totalVM, freeVM;

    if (getMemStats(totalMB, freeMB, totalVM, freeVM))
        strlist << QString::number(totalMB) << QString::number(freeMB)
                << QString::number(totalVM) << QString::number(freeVM);
    else
    {
        strlist << "ERROR";
        strlist << "Could not determine memory stats.";
    }

    SendResponse(pbssock, strlist);
}

/**
 * \addtogroup myth_network_protocol
 * \par        QUERY_TIME_ZONE
 * Returns time zone ID, current offset, current time
 */
void MainServer::HandleQueryTimeZone(PlaybackSock *pbs)
{
    MythSocket *pbssock = pbs->getSocket();
    QStringList strlist;
    strlist << MythTZ::getTimeZoneID()
            << QString::number(MythTZ::calc_utc_offset())
            << MythDate::current_iso_string(true);

    SendResponse(pbssock, strlist);
}

/**
 * \addtogroup myth_network_protocol
 * \par        QUERY_CHECKFILE \e checkslaves \e programinfo
 */
void MainServer::HandleQueryCheckFile(QStringList &slist, PlaybackSock *pbs)
{
    MythSocket *pbssock = pbs->getSocket();
    bool checkSlaves = slist[1].toInt();

    QStringList::const_iterator it = slist.begin() + 2;
    RecordingInfo recinfo(it, slist.end());

    int exists = 0;

    if (recinfo.HasPathname() && (ismaster) &&
        (recinfo.GetHostname() != gCoreContext->GetHostName()) &&
        (checkSlaves))
    {
        PlaybackSock *slave = GetSlaveByHostname(recinfo.GetHostname());

        if (slave)
        {
            exists = slave->CheckFile(&recinfo);
            slave->DecrRef();

            QStringList outputlist( QString::number(exists) );
            if (exists)
                outputlist << recinfo.GetPathname();
            else
                outputlist << "";

            SendResponse(pbssock, outputlist);
            return;
        }
    }

    QString pburl;
    if (recinfo.HasPathname())
    {
        pburl = GetPlaybackURL(&recinfo);
        exists = QFileInfo(pburl).exists();
        if (!exists)
            pburl.clear();
    }

    QStringList strlist( QString::number(exists) );
    strlist << pburl;
    SendResponse(pbssock, strlist);
}


/**
 * \addtogroup myth_network_protocol
 * \par        QUERY_FILE_HASH \e storagegroup \e filename
 */
void MainServer::HandleQueryFileHash(QStringList &slist, PlaybackSock *pbs)
{
    QString storageGroup = "Default";
    QString hostname     = gCoreContext->GetHostName();
    QString filename     = "";
    QStringList res;

    switch (slist.size()) {
      case 4:
        if (!slist[3].isEmpty())
            hostname = slist[3];
      case 3:
        if (slist[2].isEmpty())
            storageGroup = slist[2];
      case 2:
        filename = slist[1];
        if (filename.isEmpty() ||
            filename.contains("/../") ||
            filename.startsWith("../"))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("ERROR checking for file, filename '%1' "
                        "fails sanity checks").arg(filename));
            res << "";
            SendResponse(pbs->getSocket(), res);
            return;
        }
        break;
      default:
        LOG(VB_GENERAL, LOG_ERR,  LOC +
            "ERROR, invalid input count for QUERY_FILE_HASH");
        res << "";
        SendResponse(pbs->getSocket(), res);
        return;
    }

    QString hash = "";

    if (gCoreContext->IsThisHost(hostname))
    {
        StorageGroup sgroup(storageGroup, gCoreContext->GetHostName());
        QString fullname = sgroup.FindFile(filename);
        hash = FileHash(fullname);
    }
    else
    {
        PlaybackSock *slave = GetMediaServerByHostname(hostname);
        if (slave)
        {
            hash = slave->GetFileHash(filename, storageGroup);
            slave->DecrRef();
        }
        else
        {
            MSqlQuery query(MSqlQuery::InitCon());
            query.prepare("SELECT hostname FROM settings "
                           "WHERE value='BackendServerIP' "
                              "OR value='BackendServerIP6' "
                             "AND data=:HOSTNAME;");
            query.bindValue(":HOSTNAME", hostname);

            if (query.exec() && query.next())
            {
                hostname = query.value(0).toString();
                slave = GetMediaServerByHostname(hostname);
                if (slave)
                {
                    hash = slave->GetFileHash(filename, storageGroup);
                    slave->DecrRef();
                }
            }
        }
    }

    res << hash;
    SendResponse(pbs->getSocket(), res);
}

/**
 * \addtogroup myth_network_protocol
 * \par        QUERY_FILE_EXISTS \e filename \e storagegroup
 */
void MainServer::HandleQueryFileExists(QStringList &slist, PlaybackSock *pbs)
{
    QString filename = slist[1];
    QString storageGroup = "Default";
    QStringList retlist;

    if (slist.size() > 2)
        storageGroup = slist[2];

    if ((filename.isEmpty()) ||
        (filename.contains("/../")) ||
        (filename.startsWith("../")))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("ERROR checking for file, filename '%1' "
                    "fails sanity checks").arg(filename));
        retlist << "0";
        SendResponse(pbs->getSocket(), retlist);
        return;
    }

    if (storageGroup.isEmpty())
        storageGroup = "Default";

    StorageGroup sgroup(storageGroup, gCoreContext->GetHostName());

    QString fullname = sgroup.FindFile(filename);

    if (!fullname.isEmpty())
    {
        retlist << "1";
        retlist << fullname;

        struct stat fileinfo;
        if (stat(fullname.toLocal8Bit().constData(), &fileinfo) >= 0)
        {
            retlist << QString::number(fileinfo.st_dev);
            retlist << QString::number(fileinfo.st_ino);
            retlist << QString::number(fileinfo.st_mode);
            retlist << QString::number(fileinfo.st_nlink);
            retlist << QString::number(fileinfo.st_uid);
            retlist << QString::number(fileinfo.st_gid);
            retlist << QString::number(fileinfo.st_rdev);
            retlist << QString::number(fileinfo.st_size);
#ifdef _WIN32
            retlist << "0"; // st_blksize
            retlist << "0"; // st_blocks
#else
            retlist << QString::number(fileinfo.st_blksize);
            retlist << QString::number(fileinfo.st_blocks);
#endif
            retlist << QString::number(fileinfo.st_atime);
            retlist << QString::number(fileinfo.st_mtime);
            retlist << QString::number(fileinfo.st_ctime);
        }
    }
    else
        retlist << "0";

    SendResponse(pbs->getSocket(), retlist);
}

void MainServer::getGuideDataThrough(QDateTime &GuideDataThrough)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT MAX(endtime) FROM program WHERE manualid = 0;");

    if (query.exec() && query.next())
    {
        GuideDataThrough = MythDate::fromString(query.value(0).toString());
    }
}

void MainServer::HandleQueryGuideDataThrough(PlaybackSock *pbs)
{
    QDateTime GuideDataThrough;
    MythSocket *pbssock = pbs->getSocket();
    QStringList strlist;

    getGuideDataThrough(GuideDataThrough);

    if (GuideDataThrough.isNull())
        strlist << QString("0000-00-00 00:00");
    else
        strlist << QDateTime(GuideDataThrough).toString("yyyy-MM-dd hh:mm");

    SendResponse(pbssock, strlist);
}

void MainServer::HandleGetPendingRecordings(PlaybackSock *pbs,
                                            QString tmptable, int recordid)
{
    MythSocket *pbssock = pbs->getSocket();

    QStringList strList;

    if (m_sched)
    {
        if (tmptable.isEmpty())
            m_sched->GetAllPending(strList);
        else
        {
            Scheduler *sched = new Scheduler(false, encoderList,
                                             tmptable, m_sched);
            sched->FillRecordListFromDB(recordid);
            sched->GetAllPending(strList);
            delete sched;

            if (recordid > 0)
            {
                MSqlQuery query(MSqlQuery::InitCon());
                query.prepare("SELECT NULL FROM record "
                              "WHERE recordid = :RECID;");
                query.bindValue(":RECID", recordid);

                if (query.exec() && query.size())
                {
                    RecordingRule *record = new RecordingRule();
                    record->m_recordID = recordid;
                    if (record->Load() &&
                        record->m_searchType == kManualSearch)
                        m_sched->RescheduleMatch(recordid, 0, 0, QDateTime(),
                                                 "Speculation");
                    delete record;
                }
                query.prepare("DELETE FROM program WHERE manualid = :RECID;");
                query.bindValue(":RECID", recordid);
                if (!query.exec())
                    MythDB::DBError("MainServer::HandleGetPendingRecordings "
                                    "- delete", query);
            }
        }
    }
    else
    {
        strList << QString::number(0);
        strList << QString::number(0);
    }

    SendResponse(pbssock, strList);
}

void MainServer::HandleGetScheduledRecordings(PlaybackSock *pbs)
{
    MythSocket *pbssock = pbs->getSocket();

    QStringList strList;

    if (m_sched)
        Scheduler::GetAllScheduled(strList);
    else
        strList << QString::number(0);

    SendResponse(pbssock, strList);
}

void MainServer::HandleGetConflictingRecordings(QStringList &slist,
                                                PlaybackSock *pbs)
{
    MythSocket *pbssock = pbs->getSocket();

    QStringList::const_iterator it = slist.begin() + 1;
    RecordingInfo recinfo(it, slist.end());

    QStringList strlist;

    if (m_sched && recinfo.GetChanID())
        m_sched->getConflicting(&recinfo, strlist);
    else
        strlist << QString::number(0);

    SendResponse(pbssock, strlist);
}

void MainServer::HandleGetExpiringRecordings(PlaybackSock *pbs)
{
    MythSocket *pbssock = pbs->getSocket();

    QStringList strList;

    if (m_expirer)
        m_expirer->GetAllExpiring(strList);
    else
        strList << QString::number(0);

    SendResponse(pbssock, strList);
}

void MainServer::HandleSGGetFileList(QStringList &sList,
                                     PlaybackSock *pbs)
{
    MythSocket *pbssock = pbs->getSocket();
    QStringList strList;

    if ((sList.size() < 4) || (sList.size() > 5))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("HandleSGGetFileList: Invalid Request. %1")
                .arg(sList.join("[]:[]")));
        strList << "EMPTY LIST";
        SendResponse(pbssock, strList);
        return;
    }

    QString host = gCoreContext->GetHostName();
    QString wantHost = sList.at(1);
    QHostAddress wantHostaddr(wantHost);
    QString groupname = sList.at(2);
    QString path = sList.at(3);
    bool fileNamesOnly = false;

    if (sList.size() >= 5)
        fileNamesOnly = sList.at(4).toInt();

    bool slaveUnreachable = false;

    LOG(VB_FILE, LOG_INFO,  LOC +
        QString("HandleSGGetFileList: group = %1  host = %2 "
                " path = %3 wanthost = %4")
            .arg(groupname).arg(host).arg(path).arg(wantHost));

    QString addr4 = gCoreContext->GetBackendServerIP4();
    QString addr6 = gCoreContext->GetBackendServerIP6();

    if ((host.toLower() == wantHost.toLower()) ||
        (!addr4.isEmpty() && addr4 == wantHostaddr.toString()) ||
        (!addr6.isEmpty() && addr6 == wantHostaddr.toString()))
    {
        StorageGroup sg(groupname, host);
        LOG(VB_FILE, LOG_INFO, LOC + "HandleSGGetFileList: Getting local info");
        if (fileNamesOnly)
            strList = sg.GetFileList(path);
        else
            strList = sg.GetFileInfoList(path);
    }
    else
    {
        PlaybackSock *slave = GetMediaServerByHostname(wantHost);
        if (slave)
        {
            LOG(VB_FILE, LOG_INFO, LOC +
                "HandleSGGetFileList: Getting remote info");
            strList = slave->GetSGFileList(wantHost, groupname, path,
                                           fileNamesOnly);
            slave->DecrRef();
            slaveUnreachable = false;
        }
        else
        {
            LOG(VB_FILE, LOG_INFO, LOC +
                QString("HandleSGGetFileList: Failed to grab slave socket "
                        ": %1 :").arg(wantHost));
            slaveUnreachable = true;
        }

    }

    if (slaveUnreachable)
        strList << "SLAVE UNREACHABLE: " << host;

    if (strList.isEmpty() || (strList.at(0) == "0"))
        strList << "EMPTY LIST";

    SendResponse(pbssock, strList);
}

void MainServer::HandleQueryFindFile(QStringList &slist, PlaybackSock *pbs)
{
//format: QUERY_FINDFILE <host> <storagegroup> <filename> <useregex (optional)> <allowfallback (optional)>

    QString hostname = slist[1];
    QString storageGroup = slist[2];
    QString filename = slist[3];
    bool allowFallback = true;
    bool useRegex = false;
    QStringList fileList;

    if (!QHostAddress(hostname).isNull())
        LOG(VB_GENERAL, LOG_ERR, QString("Mainserver: QUERY_FINDFILE called "
                                         "with IP (%1) instead of hostname. "
                                         "This is invalid.").arg(hostname));

    if (hostname.isEmpty())
        hostname = gCoreContext->GetHostName();

    if (storageGroup.isEmpty())
        storageGroup = "Default";

    if (filename.isEmpty() || filename.contains("/../") ||
        filename.startsWith("../"))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("ERROR QueryFindFile, filename '%1' "
                    "fails sanity checks").arg(filename));
        fileList << "ERROR: Bad/Missing Filename";
        SendResponse(pbs->getSocket(), fileList);
        return;
    }

    if (slist.size() >= 5)
        useRegex = (slist[4].toInt() > 0);

    if (slist.size() >= 6)
        allowFallback = (slist[5].toInt() > 0);

    LOG(VB_FILE, LOG_INFO, LOC +
        QString("Looking for file '%1' on host '%2' in group '%3' (useregex: %4, allowfallback: %5")
        .arg(filename).arg(hostname).arg(storageGroup).arg(useRegex).arg(allowFallback));

    // first check the given host
    if (gCoreContext->IsThisHost(hostname))
    {
        LOG(VB_FILE, LOG_INFO, LOC + QString("Checking local host '%1' for file").arg(gCoreContext->GetHostName()));

        // check the local storage group
        StorageGroup sgroup(storageGroup, gCoreContext->GetHostName(), false);

        if (useRegex)
        {
            QFileInfo fi(filename);
            QStringList files = sgroup.GetFileList('/' + fi.path());

            LOG(VB_FILE, LOG_INFO, LOC + QString("Looking in dir '%1' for '%2'").arg(fi.path()).arg(fi.fileName()));

            for (int x = 0; x < files.size(); x++)
            {
                LOG(VB_FILE, LOG_INFO, LOC + QString("Found '%1 - %2'").arg(x).arg(files[x]));
            }

            QStringList filteredFiles = files.filter(QRegExp(fi.fileName()));
            for (int x = 0; x < filteredFiles.size(); x++)
            {
                fileList << gCoreContext->GenMythURL(gCoreContext->GetHostName(),
                                                     gCoreContext->GetBackendServerPort(),
                                                     fi.path() + '/' + filteredFiles[x],
                                                     storageGroup);
            }
        }
        else
        {
            if (!sgroup.FindFile(filename).isEmpty())
            {
                fileList << gCoreContext->GenMythURL(gCoreContext->GetHostName(),
                                                     gCoreContext->GetBackendServerPort(),
                                                     filename, storageGroup);
            }
        }
    }
    else
    {
        LOG(VB_FILE, LOG_INFO, LOC + QString("Checking remote host '%1' for file").arg(hostname));

        // check the given slave hostname
        PlaybackSock *slave = GetMediaServerByHostname(hostname);
        if (slave)
        {
            QStringList slaveFiles = slave->GetFindFile(hostname, filename, storageGroup, useRegex);

            if (!slaveFiles.isEmpty() && slaveFiles[0] != "NOT FOUND" && !slaveFiles[0].startsWith("ERROR: "))
                fileList += slaveFiles;

            slave->DecrRef();
        }
        else
        {
            LOG(VB_FILE, LOG_INFO, LOC + QString("Slave '%1' was unreachable").arg(hostname));
            fileList << QString("ERROR: SLAVE UNREACHABLE: %1").arg(hostname);
            SendResponse(pbs->getSocket(), fileList);
            return;
        }
    }

    // if we still haven't found it and this is the master and fallback is enabled
    // check all other slaves that have a directory in the storagegroup
    if (ismaster && fileList.isEmpty() && allowFallback)
    {
        // get a list of hosts
        MSqlQuery query(MSqlQuery::InitCon());

        QString sql = "SELECT DISTINCT hostname "
                        "FROM storagegroup "
                        "WHERE groupname = :GROUP "
                        "AND hostname != :HOSTNAME";
        query.prepare(sql);
        query.bindValue(":GROUP", storageGroup);
        query.bindValue(":HOSTNAME", hostname);

        if (!query.exec() || !query.isActive())
        {
            MythDB::DBError(LOC + "FindFile() get host list", query);
            fileList << "ERROR: failed to get host list";
            SendResponse(pbs->getSocket(), fileList);
            return;
        }

        while(query.next())
        {
            hostname = query.value(0).toString();

            if (hostname == gCoreContext->GetMasterHostName())
            {
                StorageGroup sgroup(storageGroup, hostname);

                if (useRegex)
                {
                    QFileInfo fi(filename);
                    QStringList files = sgroup.GetFileList('/' + fi.path());

                    LOG(VB_FILE, LOG_INFO, LOC + QString("Looking in dir '%1' for '%2'").arg(fi.path()).arg(fi.fileName()));

                    for (int x = 0; x < files.size(); x++)
                    {
                        LOG(VB_FILE, LOG_INFO, LOC + QString("Found '%1 - %2'").arg(x).arg(files[x]));
                    }

                    QStringList filteredFiles = files.filter(QRegExp(fi.fileName()));

                    for (int x = 0; x < filteredFiles.size(); x++)
                    {
                        fileList << gCoreContext->GenMythURL(gCoreContext->GetHostName(),
                                                                gCoreContext->GetBackendServerPort(),
                                                                fi.path() + '/' + filteredFiles[x],
                                                                storageGroup);
                    }
                }
                else
                {
                    QString fname = sgroup.FindFile(filename);
                    if (!fname.isEmpty())
                    {
                        fileList << gCoreContext->GenMythURL(gCoreContext->GetMasterHostName(),
                                                        gCoreContext->GetMasterServerPort(),
                                                        filename, storageGroup);
                    }
                }
            }
            else
            {
                // check the slave host
                PlaybackSock *slave = GetMediaServerByHostname(hostname);
                if (slave)
                {
                    QStringList slaveFiles = slave->GetFindFile(hostname, filename, storageGroup, useRegex);
                    if (!slaveFiles.isEmpty() && slaveFiles[0] != "NOT FOUND" && !slaveFiles[0].startsWith("ERROR: "))
                        fileList += slaveFiles;

                    slave->DecrRef();
                }
            }

            if (!fileList.isEmpty())
                break;
        }
    }

    if (fileList.isEmpty())
    {
        fileList << "NOT FOUND";
        LOG(VB_FILE, LOG_INFO, LOC + QString("File was not found"));
    }
    else
    {
        for (int x = 0; x < fileList.size(); x++)
        {
            LOG(VB_FILE, LOG_INFO, LOC + QString("File %1 was found at: '%2'").arg(x).arg(fileList[0]));
        }
    }

    SendResponse(pbs->getSocket(), fileList);
}

void MainServer::HandleSGFileQuery(QStringList &sList,
                                     PlaybackSock *pbs)
{
//format: QUERY_SG_FILEQUERY <host> <storagegroup> <filename> <allowfallback (optional)>

    MythSocket *pbssock = pbs->getSocket();
    QStringList strList;

    if (sList.size() < 4)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("HandleSGFileQuery: Invalid Request. %1")
                .arg(sList.join("[]:[]")));
        strList << "EMPTY LIST";
        SendResponse(pbssock, strList);
        return;
    }

    QString host = gCoreContext->GetHostName();
    QString wantHost = sList.at(1);
    QHostAddress wantHostaddr(wantHost);
    QString groupname = sList.at(2);
    QString filename = sList.at(3);

    bool allowFallback = true;
    if (sList.size() >= 5)
        allowFallback = (sList.at(4).toInt() > 0);
    LOG(VB_FILE, LOG_ERR, QString("HandleSGFileQuery - allowFallback: %1").arg(allowFallback));

    bool slaveUnreachable = false;

    LOG(VB_FILE, LOG_INFO, LOC + QString("HandleSGFileQuery: %1")
            .arg(gCoreContext->GenMythURL(wantHost, 0, filename, groupname)));

    QString addr4 = gCoreContext->GetBackendServerIP4();
    QString addr6 = gCoreContext->GetBackendServerIP6();

    if ((host.toLower() == wantHost.toLower()) ||
        (!addr4.isEmpty() && addr4 == wantHostaddr.toString()) ||
        (!addr6.isEmpty() && addr6 == wantHostaddr.toString()))
    {
        LOG(VB_FILE, LOG_INFO, LOC + "HandleSGFileQuery: Getting local info");
        StorageGroup sg(groupname, gCoreContext->GetHostName(), allowFallback);
        strList = sg.GetFileInfo(filename);
    }
    else
    {
        PlaybackSock *slave = GetMediaServerByHostname(wantHost);
        if (slave)
        {
            LOG(VB_FILE, LOG_INFO, LOC +
                "HandleSGFileQuery: Getting remote info");
            strList = slave->GetSGFileQuery(wantHost, groupname, filename);
            slave->DecrRef();
            slaveUnreachable = false;
        }
        else
        {
            LOG(VB_FILE, LOG_INFO, LOC +
                QString("HandleSGFileQuery: Failed to grab slave socket : %1 :")
                    .arg(wantHost));
            slaveUnreachable = true;
        }

    }

    if (slaveUnreachable)
        strList << "SLAVE UNREACHABLE: " << wantHost;

    if (strList.count() == 0 || (strList.at(0) == "0"))
        strList << "EMPTY LIST";

    SendResponse(pbssock, strList);
}

void MainServer::HandleLockTuner(PlaybackSock *pbs, int cardid)
{
    MythSocket *pbssock = pbs->getSocket();
    QString pbshost = pbs->getHostname();

    QStringList strlist;
    int retval;

    EncoderLink *encoder = NULL;
    QString enchost;

    QMap<int, EncoderLink *>::Iterator iter = encoderList->begin();
    for (; iter != encoderList->end(); ++iter)
    {
        EncoderLink *elink = *iter;

        // we're looking for a specific card but this isn't the one we want
        if ((cardid != -1) && (cardid != elink->GetInputID()))
            continue;

        if (elink->IsLocal())
            enchost = gCoreContext->GetHostName();
        else
            enchost = elink->GetHostName();

        if ((enchost == pbshost) &&
            (elink->IsConnected()) &&
            (!elink->IsBusy()) &&
            (!elink->IsTunerLocked()))
        {
            encoder = elink;
            break;
        }
    }

    if (encoder)
    {
        retval = encoder->LockTuner();

        if (retval != -1)
        {
            QString msg = QString("Cardid %1 LOCKed for external use on %2.")
                                  .arg(retval).arg(pbshost);
            LOG(VB_GENERAL, LOG_INFO, LOC + msg);

            MSqlQuery query(MSqlQuery::InitCon());
            query.prepare("SELECT videodevice, audiodevice, "
                          "vbidevice "
                          "FROM capturecard "
                          "WHERE cardid = :CARDID ;");
            query.bindValue(":CARDID", retval);

            if (query.exec() && query.next())
            {
                // Success
                strlist << QString::number(retval)
                        << query.value(0).toString()
                        << query.value(1).toString()
                        << query.value(2).toString();

                if (m_sched)
                    m_sched->ReschedulePlace("LockTuner");

                SendResponse(pbssock, strlist);
                return;
            }
            else
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "MainServer::LockTuner(): Could not find "
                    "card info in database");
        }
        else
        {
            // Tuner already locked
            strlist << "-2" << "" << "" << "";
            SendResponse(pbssock, strlist);
            return;
        }
    }

    strlist << "-1" << "" << "" << "";
    SendResponse(pbssock, strlist);
}

void MainServer::HandleFreeTuner(int cardid, PlaybackSock *pbs)
{
    MythSocket *pbssock = pbs->getSocket();
    QStringList strlist;
    EncoderLink *encoder = NULL;

    QMap<int, EncoderLink *>::Iterator iter = encoderList->find(cardid);
    if (iter == encoderList->end())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "MainServer::HandleFreeTuner() " +
            QString("Unknown encoder: %1").arg(cardid));
        strlist << "FAILED";
    }
    else
    {
        encoder = *iter;
        encoder->FreeTuner();

        QString msg = QString("Cardid %1 FREED from external use on %2.")
                              .arg(cardid).arg(pbs->getHostname());
        LOG(VB_GENERAL, LOG_INFO, LOC + msg);

        if (m_sched)
            m_sched->ReschedulePlace("FreeTuner");

        strlist << "OK";
    }

    SendResponse(pbssock, strlist);
}

static bool comp_livetvorder(const InputInfo &a, const InputInfo &b)
{
    if (a.livetvorder != b.livetvorder)
        return a.livetvorder < b.livetvorder;
    return a.inputid < b.inputid;
}

void MainServer::HandleGetFreeInputInfo(PlaybackSock *pbs,
                                        uint excluded_input)
{
    LOG(VB_CHANNEL, LOG_INFO,
        LOC + QString("Excluding input %1")
        .arg(excluded_input));

    MythSocket *pbssock = pbs->getSocket();
    vector<InputInfo> busyinputs;
    vector<InputInfo> freeinputs;
    QMap<uint, QSet<uint> > groupids;

    // Lopp over each encoder and divide the inputs into busy and free
    // lists.
    QMap<int, EncoderLink *>::Iterator iter = encoderList->begin();
    for (; iter != encoderList->end(); ++iter)
    {
        EncoderLink *elink = *iter;
        InputInfo info;
        info.inputid = elink->GetInputID();

        if (!elink->IsConnected() || elink->IsTunerLocked())
        {
            LOG(VB_CHANNEL, LOG_INFO,
                LOC + QString("Input %1 is locked or not connected")
                .arg(info.inputid));
            continue;
        }

        vector<uint> infogroups;
        CardUtil::GetInputInfo(info, &infogroups);
        for (uint i = 0; i < infogroups.size(); ++i)
            groupids[info.inputid].insert(infogroups[i]);

        InputInfo busyinfo;
        if (info.inputid != excluded_input && elink->IsBusy(&busyinfo))
        {
            LOG(VB_CHANNEL, LOG_DEBUG,
                LOC + QString("Input %1 is busy on %2/%3")
                .arg(info.inputid).arg(busyinfo.chanid).arg(busyinfo.mplexid));
            info.chanid = busyinfo.chanid;
            info.mplexid = busyinfo.mplexid;
            busyinputs.push_back(info);
        }
        else if (info.livetvorder)
        {
            LOG(VB_CHANNEL, LOG_DEBUG,
                LOC + QString("Input %1 is free")
                .arg(info.inputid));
            freeinputs.push_back(info);
        }
    }

    // Loop over each busy input and restrict or delete any free
    // inputs that are in the same group.
    vector<InputInfo>::iterator busyiter = busyinputs.begin();
    for (; busyiter != busyinputs.end(); ++busyiter)
    {
        InputInfo &busyinfo = *busyiter;

        vector<InputInfo>::iterator freeiter = freeinputs.begin();
        while (freeiter != freeinputs.end())
        {
            InputInfo &freeinfo = *freeiter;

            if ((groupids[busyinfo.inputid] & groupids[freeinfo.inputid])
                .isEmpty())
            {
                ++freeiter;
                continue;
            }

            if (busyinfo.sourceid == freeinfo.sourceid)
            {
                LOG(VB_CHANNEL, LOG_DEBUG,
                    LOC + QString("Input %1 is limited to %2/%3 by input %4")
                    .arg(freeinfo.inputid).arg(busyinfo.chanid)
                    .arg(busyinfo.mplexid).arg(busyinfo.inputid));
                freeinfo.chanid = busyinfo.chanid;
                freeinfo.mplexid = busyinfo.mplexid;
                ++freeiter;
                continue;
            }

            LOG(VB_CHANNEL, LOG_DEBUG,
                LOC + QString("Input %1 is unavailable by input %2")
                .arg(freeinfo.inputid).arg(busyinfo.inputid));
            freeiter = freeinputs.erase(freeiter);
        }
    }

    // Return the results in livetvorder.
    stable_sort(freeinputs.begin(), freeinputs.end(), comp_livetvorder);
    QStringList strlist;
    for (uint i = 0; i < freeinputs.size(); ++i)
    {
        LOG(VB_CHANNEL, LOG_INFO,
            LOC + QString("Input %1 is available on %2/%3")
            .arg(freeinputs[i].inputid).arg(freeinputs[i].chanid)
            .arg(freeinputs[i].mplexid));
        freeinputs[i].ToStringList(strlist);
    }

    if (strlist.empty())
        strlist << "OK";

    SendResponse(pbssock, strlist);
}

static QString cleanup(const QString &str)
{
    if (str == " ")
        return "";
    return str;
}

static QString make_safe(const QString &str)
{
    if (str.isEmpty())
        return " ";
    return str;
}

void MainServer::HandleRecorderQuery(QStringList &slist, QStringList &commands,
                                     PlaybackSock *pbs)
{
    MythSocket *pbssock = pbs->getSocket();

    if (commands.size() < 2 || slist.size() < 2)
        return;

    int recnum = commands[1].toInt();

    QMap<int, EncoderLink *>::Iterator iter = encoderList->find(recnum);
    if (iter == encoderList->end())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "MainServer::HandleRecorderQuery() " +
            QString("Unknown encoder: %1").arg(recnum));
        QStringList retlist( "bad" );
        SendResponse(pbssock, retlist);
        return;
    }

    QString command = slist[1];

    QStringList retlist;

    EncoderLink *enc = *iter;
    if (!enc->IsConnected())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + " MainServer::HandleRecorderQuery() " +
            QString("Command %1 for unconnected encoder %2")
                .arg(command).arg(recnum));
        retlist << "bad";
        SendResponse(pbssock, retlist);
        return;
    }

    if (command == "IS_RECORDING")
    {
        retlist << QString::number((int)enc->IsReallyRecording());
    }
    else if (command == "GET_FRAMERATE")
    {
        retlist << QString::number(enc->GetFramerate());
    }
    else if (command == "GET_FRAMES_WRITTEN")
    {
        retlist << QString::number(enc->GetFramesWritten());
    }
    else if (command == "GET_FILE_POSITION")
    {
        retlist << QString::number(enc->GetFilePosition());
    }
    else if (command == "GET_MAX_BITRATE")
    {
        retlist << QString::number(enc->GetMaxBitrate());
    }
    else if (command == "GET_CURRENT_RECORDING")
    {
        ProgramInfo *info = enc->GetRecording();
        if (info)
        {
            info->ToStringList(retlist);
            delete info;
        }
        else
        {
            ProgramInfo dummy;
            dummy.SetInputID(enc->GetInputID());
            dummy.ToStringList(retlist);
        }
    }
    else if (command == "GET_KEYFRAME_POS")
    {
        long long desired = slist[2].toLongLong();
        retlist << QString::number(enc->GetKeyframePosition(desired));
    }
    else if (command == "FILL_POSITION_MAP")
    {
        int64_t start = slist[2].toLongLong();
        int64_t end   = slist[3].toLongLong();
        frm_pos_map_t map;

        if (!enc->GetKeyframePositions(start, end, map))
        {
            retlist << "error";
        }
        else
        {
            frm_pos_map_t::const_iterator it = map.begin();
            for (; it != map.end(); ++it)
            {
                retlist += QString::number(it.key());
                retlist += QString::number(*it);
            }
            if (retlist.empty())
                retlist << "OK";
        }
    }
    else if (command == "FILL_DURATION_MAP")
    {
        int64_t start = slist[2].toLongLong();
        int64_t end   = slist[3].toLongLong();
        frm_pos_map_t map;

        if (!enc->GetKeyframeDurations(start, end, map))
        {
            retlist << "error";
        }
        else
        {
            frm_pos_map_t::const_iterator it = map.begin();
            for (; it != map.end(); ++it)
            {
                retlist += QString::number(it.key());
                retlist += QString::number(*it);
            }
            if (retlist.empty())
                retlist << "OK";
        }
    }
    else if (command == "GET_RECORDING")
    {
        ProgramInfo *pginfo = enc->GetRecording();
        if (pginfo)
        {
            pginfo->ToStringList(retlist);
            delete pginfo;
        }
        else
        {
            ProgramInfo dummy;
            dummy.SetInputID(enc->GetInputID());
            dummy.ToStringList(retlist);
        }
    }
    else if (command == "FRONTEND_READY")
    {
        enc->FrontendReady();
        retlist << "OK";
    }
    else if (command == "CANCEL_NEXT_RECORDING")
    {
        QString cancel = slist[2];
        LOG(VB_GENERAL, LOG_NOTICE, LOC +
            QString("Received: CANCEL_NEXT_RECORDING %1").arg(cancel));
        enc->CancelNextRecording(cancel == "1");
        retlist << "OK";
    }
    else if (command == "SPAWN_LIVETV")
    {
        QString chainid = slist[2];
        LiveTVChain *chain = GetExistingChain(chainid);
        if (!chain)
        {
            chain = new LiveTVChain();
            chain->LoadFromExistingChain(chainid);
            AddToChains(chain);
        }

        chain->SetHostSocket(pbssock);

        enc->SpawnLiveTV(chain, slist[3].toInt(), slist[4]);
        retlist << "OK";
    }
    else if (command == "STOP_LIVETV")
    {
        QString chainid = enc->GetChainID();
        enc->StopLiveTV();

        LiveTVChain *chain = GetExistingChain(chainid);
        if (chain)
        {
            chain->DelHostSocket(pbssock);
            if (chain->HostSocketCount() == 0)
            {
                DeleteChain(chain);
            }
        }

        retlist << "OK";
    }
    else if (command == "PAUSE")
    {
        enc->PauseRecorder();
        retlist << "OK";
    }
    else if (command == "FINISH_RECORDING")
    {
        enc->FinishRecording();
        retlist << "OK";
    }
    else if (command == "SET_LIVE_RECORDING")
    {
        int recording = slist[2].toInt();
        enc->SetLiveRecording(recording);
        retlist << "OK";
    }
    else if (command == "GET_INPUT")
    {
        QString ret = enc->GetInput();
        ret = (ret.isEmpty()) ? "UNKNOWN" : ret;
        retlist << ret;
    }
    else if (command == "SET_INPUT")
    {
        QString input = slist[2];
        QString ret   = enc->SetInput(input);
        ret = (ret.isEmpty()) ? "UNKNOWN" : ret;
        retlist << ret;
    }
    else if (command == "TOGGLE_CHANNEL_FAVORITE")
    {
        QString changroup = slist[2];
        enc->ToggleChannelFavorite(changroup);
        retlist << "OK";
    }
    else if (command == "CHANGE_CHANNEL")
    {
        ChannelChangeDirection direction =
            (ChannelChangeDirection) slist[2].toInt();
        enc->ChangeChannel(direction);
        retlist << "OK";
    }
    else if (command == "SET_CHANNEL")
    {
        QString name = slist[2];
        enc->SetChannel(name);
        retlist << "OK";
    }
    else if (command == "SET_SIGNAL_MONITORING_RATE")
    {
        int rate = slist[2].toInt();
        int notifyFrontend = slist[3].toInt();
        int oldrate = enc->SetSignalMonitoringRate(rate, notifyFrontend);
        retlist << QString::number(oldrate);
    }
    else if (command == "GET_COLOUR")
    {
        int ret = enc->GetPictureAttribute(kPictureAttribute_Colour);
        retlist << QString::number(ret);
    }
    else if (command == "GET_CONTRAST")
    {
        int ret = enc->GetPictureAttribute(kPictureAttribute_Contrast);
        retlist << QString::number(ret);
    }
    else if (command == "GET_BRIGHTNESS")
    {
        int ret = enc->GetPictureAttribute(kPictureAttribute_Brightness);
        retlist << QString::number(ret);
    }
    else if (command == "GET_HUE")
    {
        int ret = enc->GetPictureAttribute(kPictureAttribute_Hue);
        retlist << QString::number(ret);
    }
    else if (command == "CHANGE_COLOUR")
    {
        int  type = slist[2].toInt();
        bool up   = slist[3].toInt();
        int  ret = enc->ChangePictureAttribute(
            (PictureAdjustType) type, kPictureAttribute_Colour, up);
        retlist << QString::number(ret);
    }
    else if (command == "CHANGE_CONTRAST")
    {
        int  type = slist[2].toInt();
        bool up   = slist[3].toInt();
        int  ret = enc->ChangePictureAttribute(
            (PictureAdjustType) type, kPictureAttribute_Contrast, up);
        retlist << QString::number(ret);
    }
    else if (command == "CHANGE_BRIGHTNESS")
    {
        int  type= slist[2].toInt();
        bool up  = slist[3].toInt();
        int  ret = enc->ChangePictureAttribute(
            (PictureAdjustType) type, kPictureAttribute_Brightness, up);
        retlist << QString::number(ret);
    }
    else if (command == "CHANGE_HUE")
    {
        int  type= slist[2].toInt();
        bool up  = slist[3].toInt();
        int  ret = enc->ChangePictureAttribute(
            (PictureAdjustType) type, kPictureAttribute_Hue, up);
        retlist << QString::number(ret);
    }
    else if (command == "CHECK_CHANNEL")
    {
        QString name = slist[2];
        retlist << QString::number((int)(enc->CheckChannel(name)));
    }
    else if (command == "SHOULD_SWITCH_CARD")
    {
        QString chanid = slist[2];
        retlist << QString::number((int)(enc->ShouldSwitchToAnotherInput(chanid)));
    }
    else if (command == "CHECK_CHANNEL_PREFIX")
    {
        QString needed_spacer;
        QString prefix        = slist[2];
        uint    is_complete_valid_channel_on_rec = 0;
        bool    is_extra_char_useful             = false;

        bool match = enc->CheckChannelPrefix(
            prefix, is_complete_valid_channel_on_rec,
            is_extra_char_useful, needed_spacer);

        retlist << QString::number((int)match);
        retlist << QString::number(is_complete_valid_channel_on_rec);
        retlist << QString::number((int)is_extra_char_useful);
        retlist << ((needed_spacer.isEmpty()) ? QString("X") : needed_spacer);
    }
    else if (command == "GET_NEXT_PROGRAM_INFO" && (slist.size() >= 6))
    {
        QString channelname = slist[2];
        uint chanid = slist[3].toUInt();
        BrowseDirection direction = (BrowseDirection)slist[4].toInt();
        QString starttime = slist[5];

        QString title = "", subtitle = "", desc = "", category = "";
        QString endtime = "", callsign = "", iconpath = "";
        QString seriesid = "", programid = "";

        enc->GetNextProgram(direction,
                            title, subtitle, desc, category, starttime,
                            endtime, callsign, iconpath, channelname, chanid,
                            seriesid, programid);

        retlist << make_safe(title);
        retlist << make_safe(subtitle);
        retlist << make_safe(desc);
        retlist << make_safe(category);
        retlist << make_safe(starttime);
        retlist << make_safe(endtime);
        retlist << make_safe(callsign);
        retlist << make_safe(iconpath);
        retlist << make_safe(channelname);
        retlist << QString::number(chanid);
        retlist << make_safe(seriesid);
        retlist << make_safe(programid);
    }
    else if (command == "GET_CHANNEL_INFO")
    {
        uint chanid = slist[2].toUInt();
        uint sourceid = 0;
        QString callsign = "", channum = "", channame = "", xmltv = "";

        enc->GetChannelInfo(chanid, sourceid,
                            callsign, channum, channame, xmltv);

        retlist << QString::number(chanid);
        retlist << QString::number(sourceid);
        retlist << make_safe(callsign);
        retlist << make_safe(channum);
        retlist << make_safe(channame);
        retlist << make_safe(xmltv);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Unknown command: %1").arg(command));
        retlist << "OK";
    }

    SendResponse(pbssock, retlist);
}

void MainServer::HandleSetNextLiveTVDir(QStringList &commands,
                                        PlaybackSock *pbs)
{
    MythSocket *pbssock = pbs->getSocket();

    int recnum = commands[1].toInt();

    QMap<int, EncoderLink *>::Iterator iter = encoderList->find(recnum);
    if (iter == encoderList->end())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "MainServer::HandleSetNextLiveTVDir() " +
            QString("Unknown encoder: %1").arg(recnum));
        QStringList retlist( "bad" );
        SendResponse(pbssock, retlist);
        return;
    }

    EncoderLink *enc = *iter;
    enc->SetNextLiveTVDir(commands[2]);

    QStringList retlist( "OK" );
    SendResponse(pbssock, retlist);
}

void MainServer::HandleSetChannelInfo(QStringList &slist, PlaybackSock *pbs)
{
    bool        ok       = true;
    MythSocket *pbssock  = pbs->getSocket();
    uint        chanid   = slist[1].toUInt();
    uint        sourceid = slist[2].toUInt();
    QString     oldcnum  = cleanup(slist[3]);
    QString     callsign = cleanup(slist[4]);
    QString     channum  = cleanup(slist[5]);
    QString     channame = cleanup(slist[6]);
    QString     xmltv    = cleanup(slist[7]);

    QStringList retlist;
    if (!chanid || !sourceid)
    {
        retlist << "0";
        SendResponse(pbssock, retlist);
        return;
    }

    QMap<int, EncoderLink *>::iterator it = encoderList->begin();
    for (; it != encoderList->end(); ++it)
    {
        if (*it)
        {
            ok &= (*it)->SetChannelInfo(chanid, sourceid, oldcnum,
                                        callsign, channum, channame, xmltv);
        }
    }

    retlist << ((ok) ? "1" : "0");
    SendResponse(pbssock, retlist);
}

void MainServer::HandleRemoteEncoder(QStringList &slist, QStringList &commands,
                                     PlaybackSock *pbs)
{
    MythSocket *pbssock = pbs->getSocket();

    int recnum = commands[1].toInt();
    QStringList retlist;

    QMap<int, EncoderLink *>::Iterator iter = encoderList->find(recnum);
    if (iter == encoderList->end())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("HandleRemoteEncoder(cmd %1) ").arg(slist[1]) +
            QString("Unknown encoder: %1").arg(recnum));
        retlist << QString::number((int) kState_Error);
        SendResponse(pbssock, retlist);
        return;
    }

    EncoderLink *enc = *iter;

    QString command = slist[1];

    if (command == "GET_STATE")
    {
        retlist << QString::number((int)enc->GetState());
    }
    else if (command == "GET_SLEEPSTATUS")
    {
        retlist << QString::number(enc->GetSleepStatus());
    }
    else if (command == "GET_FLAGS")
    {
        retlist << QString::number(enc->GetFlags());
    }
    else if (command == "IS_BUSY")
    {
        int time_buffer = (slist.size() >= 3) ? slist[2].toInt() : 5;
        InputInfo busy_input;
        retlist << QString::number((int)enc->IsBusy(&busy_input, time_buffer));
        busy_input.ToStringList(retlist);
    }
    else if (command == "MATCHES_RECORDING" &&
             slist.size() >= (2 + NUMPROGRAMLINES))
    {
        QStringList::const_iterator it = slist.begin() + 2;
        ProgramInfo pginfo(it, slist.end());

        retlist << QString::number((int)enc->MatchesRecording(&pginfo));
    }
    else if (command == "START_RECORDING" &&
             slist.size() >= (2 + NUMPROGRAMLINES))
    {
        QStringList::const_iterator it = slist.begin() + 2;
        ProgramInfo pginfo(it, slist.end());

        retlist << QString::number(enc->StartRecording(&pginfo));
        retlist << QString::number(pginfo.GetRecordingID());
        retlist << QString::number(pginfo.GetRecordingStartTime().toTime_t());
    }
    else if (command == "GET_RECORDING_STATUS")
    {
        retlist << QString::number((int)enc->GetRecordingStatus());
    }
    else if (command == "RECORD_PENDING" &&
             (slist.size() >= 4 + NUMPROGRAMLINES))
    {
        int secsleft = slist[2].toInt();
        int haslater = slist[3].toInt();
        QStringList::const_iterator it = slist.begin() + 4;
        ProgramInfo pginfo(it, slist.end());

        enc->RecordPending(&pginfo, secsleft, haslater);

        retlist << "OK";
    }
    else if (command == "CANCEL_NEXT_RECORDING" &&
             (slist.size() >= 3))
    {
        bool cancel = (bool) slist[2].toInt();
        enc->CancelNextRecording(cancel);
        retlist << "OK";
    }
    else if (command == "STOP_RECORDING")
    {
        enc->StopRecording();
        retlist << "OK";
    }
    else if (command == "GET_MAX_BITRATE")
    {
        retlist << QString::number(enc->GetMaxBitrate());
    }
    else if (command == "GET_CURRENT_RECORDING")
    {
        ProgramInfo *info = enc->GetRecording();
        if (info)
        {
            info->ToStringList(retlist);
            delete info;
        }
        else
        {
            ProgramInfo dummy;
            dummy.SetInputID(enc->GetInputID());
            dummy.ToStringList(retlist);
        }
    }

    SendResponse(pbssock, retlist);
}

void MainServer::GetActiveBackends(QStringList &hosts)
{
    hosts.clear();
    hosts << gCoreContext->GetHostName();

    QString hostname;
    QReadLocker rlock(&sockListLock);
    vector<PlaybackSock*>::iterator it;
    for (it = playbackList.begin(); it != playbackList.end(); ++it)
    {
        if ((*it)->isMediaServer())
        {
            hostname = (*it)->getHostname();
            if (!hosts.contains(hostname))
                hosts << hostname;
        }
    }
}

void MainServer::HandleActiveBackendsQuery(PlaybackSock *pbs)
{
    QStringList retlist;
    GetActiveBackends(retlist);
    retlist.push_front(QString::number(retlist.size()));
    SendResponse(pbs->getSocket(), retlist);
}

void MainServer::HandleIsActiveBackendQuery(QStringList &slist,
                                            PlaybackSock *pbs)
{
    QStringList retlist;
    QString queryhostname = slist[1];

    if (gCoreContext->GetHostName() != queryhostname)
    {
        PlaybackSock *slave = GetSlaveByHostname(queryhostname);
        if (slave != NULL)
        {
            retlist << "TRUE";
            slave->DecrRef();
        }
        else
            retlist << "FALSE";
    }
    else
        retlist << "TRUE";

    SendResponse(pbs->getSocket(), retlist);
}

int MainServer::GetfsID(QList<FileSystemInfo>::iterator fsInfo)
{
    QString fskey = fsInfo->getHostname() + ":" + fsInfo->getPath();
    QMutexLocker lock(&fsIDcacheLock);
    if (!fsIDcache.contains(fskey))
        fsIDcache[fskey] = fsIDcache.count();

    return fsIDcache[fskey];
}

size_t MainServer::GetCurrentMaxBitrate(void)
{
    size_t totalKBperMin = 0;

    QMap<int, EncoderLink*>::iterator it = encoderList->begin();
    for (; it != encoderList->end(); ++it)
    {
        EncoderLink *enc = *it;

        if (!enc->IsConnected() || !enc->IsBusy())
            continue;

        long long maxBitrate = enc->GetMaxBitrate();
        if (maxBitrate<=0)
            maxBitrate = 19500000LL;
        long long thisKBperMin = (((size_t)maxBitrate)*((size_t)15))>>11;
        totalKBperMin += thisKBperMin;
        LOG(VB_FILE, LOG_INFO, LOC + QString("Cardid %1: max bitrate %2 KB/min")
                .arg(enc->GetInputID()).arg(thisKBperMin));
    }

    LOG(VB_FILE, LOG_INFO, LOC +
        QString("Maximal bitrate of busy encoders is %1 KB/min")
            .arg(totalKBperMin));

    return totalKBperMin;
}

void MainServer::BackendQueryDiskSpace(QStringList &strlist, bool consolidated,
                                       bool allHosts)
{
    QString allHostList = gCoreContext->GetHostName();
    int64_t totalKB = -1, usedKB = -1;
    QMap <QString, bool>foundDirs;
    QString driveKey;
    QString localStr = "1";
    struct statfs statbuf;
    QStringList groups(StorageGroup::kSpecialGroups);
    groups.removeAll("LiveTV");
    QString specialGroups = groups.join("', '");
    QString sql = QString("SELECT MIN(id),dirname "
                            "FROM storagegroup "
                           "WHERE hostname = :HOSTNAME "
                             "AND groupname NOT IN ( '%1' ) "
                           "GROUP BY dirname;").arg(specialGroups);
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(sql);
    query.bindValue(":HOSTNAME", gCoreContext->GetHostName());

    if (query.exec())
    {
        // If we don't have any dirs of our own, fallback to list of Default
        // dirs since that is what StorageGroup::Init() does.
        if (!query.size())
        {
            query.prepare("SELECT MIN(id),dirname "
                          "FROM storagegroup "
                          "WHERE groupname = :GROUP "
                          "GROUP BY dirname;");
            query.bindValue(":GROUP", "Default");
            if (!query.exec())
                MythDB::DBError("BackendQueryDiskSpace", query);
        }

        QDir checkDir("");
        QString dirID;
        QString currentDir;
        int bSize;
        while (query.next())
        {
            dirID = query.value(0).toString();
            /* The storagegroup.dirname column uses utf8_bin collation, so Qt
             * uses QString::fromAscii() for toString(). Explicitly convert the
             * value using QString::fromUtf8() to prevent corruption. */
            currentDir = QString::fromUtf8(query.value(1)
                                           .toByteArray().constData());
            if (currentDir.endsWith("/"))
                currentDir.remove(currentDir.length() - 1, 1);

            checkDir.setPath(currentDir);
            if (!foundDirs.contains(currentDir))
            {
                if (checkDir.exists())
                {
                    QByteArray cdir = currentDir.toLatin1();
                    getDiskSpace(cdir.constData(), totalKB, usedKB);
                    memset(&statbuf, 0, sizeof(statbuf));
                    localStr = "1"; // Assume local
                    bSize = 0;

                    if (!statfs(currentDir.toLocal8Bit().constData(), &statbuf))
                    {
#if CONFIG_DARWIN
                        char *fstypename = statbuf.f_fstypename;
                        if ((!strcmp(fstypename, "nfs")) ||   // NFS|FTP
                            (!strcmp(fstypename, "afpfs")) || // ApplShr
                            (!strcmp(fstypename, "smbfs")))   // SMB
                            localStr = "0";
#elif __linux__
                        long fstype = statbuf.f_type;
                        if ((fstype == 0x6969) ||             // NFS
                            (fstype == 0x517B) ||             // SMB
                            (fstype == (long)0xFF534D42))     // CIFS
                            localStr = "0";
#endif
                        bSize = statbuf.f_bsize;
                    }

                    strlist << gCoreContext->GetHostName();
                    strlist << currentDir;
                    strlist << localStr;
                    strlist << "-1"; // Ignore fsID
                    strlist << dirID;
                    strlist << QString::number(bSize);
                    strlist << QString::number(totalKB);
                    strlist << QString::number(usedKB);

                    foundDirs[currentDir] = true;
                }
                else
                    foundDirs[currentDir] = false;
            }
        }
    }

    if (allHosts)
    {
        QMap <QString, bool> backendsCounted;
        QString pbsHost;

        list<PlaybackSock *> localPlaybackList;

        sockListLock.lockForRead();

        vector<PlaybackSock *>::iterator pbsit = playbackList.begin();
        for (; pbsit != playbackList.end(); ++pbsit)
        {
            PlaybackSock *pbs = *pbsit;

            if ((pbs->IsDisconnected()) ||
                (!pbs->isMediaServer()) ||
                (pbs->isLocal()) ||
                (backendsCounted.contains(pbs->getHostname())))
                continue;

            backendsCounted[pbs->getHostname()] = true;
            pbs->IncrRef();
            localPlaybackList.push_back(pbs);
            allHostList += "," + pbs->getHostname();
        }

        sockListLock.unlock();

        for (list<PlaybackSock *>::iterator p = localPlaybackList.begin() ;
             p != localPlaybackList.end() ; ++p) {
            (*p)->GetDiskSpace(strlist);
            (*p)->DecrRef();
        }
    }

    if (!consolidated)
        return;

    FileSystemInfo fsInfo;
    QList<FileSystemInfo> fsInfos;

    QStringList::const_iterator it = strlist.begin();
    while (it != strlist.end())
    {
        fsInfo.setHostname(*(it++));
        fsInfo.setPath(*(it++));
        fsInfo.setLocal((*(it++)).toInt() > 0);
        fsInfo.setFSysID(-1);
        ++it;   // Without this, the strlist gets out of whack
        fsInfo.setGroupID((*(it++)).toInt());
        fsInfo.setBlockSize((*(it++)).toInt());
        fsInfo.setTotalSpace((*(it++)).toLongLong());
        fsInfo.setUsedSpace((*(it++)).toLongLong());
        fsInfos.push_back(fsInfo);
    }
    strlist.clear();

    // Consolidate hosts sharing storage
    int64_t maxWriteFiveSec = GetCurrentMaxBitrate()/12 /*5 seconds*/;
    maxWriteFiveSec = max((int64_t)2048, maxWriteFiveSec); // safety for NFS mounted dirs
    QList<FileSystemInfo>::iterator it1, it2;
    int bSize = 32;
    for (it1 = fsInfos.begin(); it1 != fsInfos.end(); ++it1)
    {
        if (it1->getFSysID() == -1)
        {
            it1->setFSysID(GetfsID(it1));
            it1->setPath(
                it1->getHostname().section(".", 0, 0) + ":" + it1->getPath());
        }

        for (it2 = it1 + 1; it2 != fsInfos.end(); ++it2)
        {
            // our fuzzy comparison uses the maximum of the two block sizes
            // or 32, whichever is greater
            bSize = max(32, max(it1->getBlockSize(), it2->getBlockSize()) / 1024);
            int64_t diffSize = it1->getTotalSpace() - it2->getTotalSpace();
            int64_t diffUsed = it1->getUsedSpace() - it2->getUsedSpace();
            if (diffSize < 0)
                diffSize = 0 - diffSize;
            if (diffUsed < 0)
                diffUsed = 0 - diffUsed;

            if (it2->getFSysID() == -1 && (diffSize <= bSize) &&
                (diffUsed <= maxWriteFiveSec))
            {
                if (!it1->getHostname().contains(it2->getHostname()))
                    it1->setHostname(it1->getHostname() + "," + it2->getHostname());
                it1->setPath(it1->getPath() + "," +
                    it2->getHostname().section(".", 0, 0) + ":" + it2->getPath());
                fsInfos.erase(it2);
                it2 = it1;
            }
        }
    }

    // Passed the cleaned list back
    totalKB = 0;
    usedKB  = 0;
    for (it1 = fsInfos.begin(); it1 != fsInfos.end(); ++it1)
    {
        strlist << it1->getHostname();
        strlist << it1->getPath();
        strlist << QString::number(it1->isLocal());
        strlist << QString::number(it1->getFSysID());
        strlist << QString::number(it1->getGroupID());
        strlist << QString::number(it1->getBlockSize());
        strlist << QString::number(it1->getTotalSpace());
        strlist << QString::number(it1->getUsedSpace());

        totalKB += it1->getTotalSpace();
        usedKB  += it1->getUsedSpace();
    }

    if (allHosts)
    {
        strlist << allHostList;
        strlist << "TotalDiskSpace";
        strlist << "0";
        strlist << "-2";
        strlist << "-2";
        strlist << "0";
        strlist << QString::number(totalKB);
        strlist << QString::number(usedKB);
    }
}

void MainServer::GetFilesystemInfos(QList<FileSystemInfo> &fsInfos)
{
    QStringList strlist;
    FileSystemInfo fsInfo;

    fsInfos.clear();

    BackendQueryDiskSpace(strlist, false, true);

    QStringList::const_iterator it = strlist.begin();
    while (it != strlist.end())
    {
        fsInfo.setHostname(*(it++));
        fsInfo.setPath(*(it++));
        fsInfo.setLocal((*(it++)).toInt() > 0);
        fsInfo.setFSysID(-1);
        ++it;
        fsInfo.setGroupID((*(it++)).toInt());
        fsInfo.setBlockSize((*(it++)).toInt());
        fsInfo.setTotalSpace((*(it++)).toLongLong());
        fsInfo.setUsedSpace((*(it++)).toLongLong());
        fsInfo.setWeight(0);
        fsInfos.push_back(fsInfo);
    }

    LOG(VB_SCHEDULE | VB_FILE, LOG_DEBUG, LOC +
        "Determining unique filesystems");
    size_t maxWriteFiveSec = GetCurrentMaxBitrate()/12  /*5 seconds*/;
    // safety for NFS mounted dirs
    maxWriteFiveSec = max((size_t)2048, maxWriteFiveSec);

    FileSystemInfo::Consolidate(fsInfos, false, maxWriteFiveSec);

    QList<FileSystemInfo>::iterator it1;
    if (VERBOSE_LEVEL_CHECK(VB_FILE | VB_SCHEDULE, LOG_INFO))
    {
        LOG(VB_FILE | VB_SCHEDULE, LOG_INFO, LOC +
            "--- GetFilesystemInfos directory list start ---");
        for (it1 = fsInfos.begin(); it1 != fsInfos.end(); ++it1)
        {
            QString msg =
                QString("Dir: %1:%2")
                    .arg(it1->getHostname())
                    .arg(it1->getPath());
            LOG(VB_FILE | VB_SCHEDULE, LOG_INFO, LOC + msg) ;
            LOG(VB_FILE | VB_SCHEDULE, LOG_INFO, LOC +
                QString("     Location: %1")
                .arg(it1->isLocal() ? "Local" : "Remote"));
            LOG(VB_FILE | VB_SCHEDULE, LOG_INFO, LOC +
                QString("     fsID    : %1")
                .arg(it1->getFSysID()));
            LOG(VB_FILE | VB_SCHEDULE, LOG_INFO, LOC +
                QString("     dirID   : %1")
                .arg(it1->getGroupID()));
            LOG(VB_FILE | VB_SCHEDULE, LOG_INFO, LOC +
                QString("     BlkSize : %1")
                .arg(it1->getBlockSize()));
            LOG(VB_FILE | VB_SCHEDULE, LOG_INFO, LOC +
                QString("     TotalKB : %1")
                .arg(it1->getTotalSpace()));
            LOG(VB_FILE | VB_SCHEDULE, LOG_INFO, LOC +
                QString("     UsedKB  : %1")
                .arg(it1->getUsedSpace()));
            LOG(VB_FILE | VB_SCHEDULE, LOG_INFO, LOC +
                QString("     FreeKB  : %1")
                .arg(it1->getFreeSpace()));
        }
        LOG(VB_FILE | VB_SCHEDULE, LOG_INFO, LOC +
            "--- GetFilesystemInfos directory list end ---");
    }
}

void MainServer::HandleMoveFile(PlaybackSock *pbs, const QString &storagegroup,
                                const QString &src, const QString &dst)
{
    StorageGroup sgroup(storagegroup, "", false);
    QStringList retlist;

    if (src.isEmpty() || dst.isEmpty()
        || src.contains("..") || dst.contains(".."))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("HandleMoveFile: ERROR moving file '%1' -> '%2', "
                    "a path fails sanity checks").arg(src, dst));
        retlist << "0" << "Invalid path";
        SendResponse(pbs->getSocket(), retlist);
        return;
    }

    QString srcAbs = sgroup.FindFile(src);
    if (srcAbs.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("HandleMoveFile: Unable to find %1").arg(src));
        retlist << "0" << "Source file not found";
        SendResponse(pbs->getSocket(), retlist);
        return;
    }

    // Path of files must be unique within SG. Rename will permit <sgdir1>/<dst>
    // even when <sgdir2>/<dst> already exists.
    // Directory paths do not have to be unique.
    QString dstAbs = sgroup.FindFile(dst);
    if (!dstAbs.isEmpty() && QFileInfo(dstAbs).isFile())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("HandleMoveFile: Destination exists at %1").arg(dstAbs));
        retlist << "0" << "Destination file exists";
        SendResponse(pbs->getSocket(), retlist);
        return;
    }

    // Files never move filesystems, so use current SG dir
    int sgPathSize = srcAbs.size() - src.size();
    dstAbs = srcAbs.mid(0, sgPathSize) + dst;

    // Renaming on same filesystem should always be fast but is liable to delays
    // for unknowable reasons so we delegate to a separate thread for safety.
    RenameThread *renamer = new RenameThread(*this, *pbs, srcAbs, dstAbs);
    MThreadPool::globalInstance()->start(renamer, "Rename");
}

QMutex RenameThread::m_renamelock;

void RenameThread::run()
{
    // Only permit one rename to run at any time
    QMutexLocker lock(&m_renamelock);
    LOG(VB_FILE, LOG_INFO, QString("MainServer::RenameThread: Renaming %1 -> %2")
        .arg(m_src, m_dst));

    QStringList retlist;
    QFileInfo   fi(m_dst);

    if (QDir().mkpath(fi.path()) && QFile::rename(m_src, m_dst))
    {
        retlist << "1";
    }
    else
    {
        retlist << "0" << "Rename failed";
        LOG(VB_FILE, LOG_ERR, "MainServer::DoRenameThread: Rename failed");
    }
    m_ms.SendResponse(m_pbs.getSocket(), retlist);
}

void TruncateThread::run(void)
{
    if (m_ms)
        m_ms->DoTruncateThread(this);
}

void MainServer::DoTruncateThread(DeleteStruct *ds)
{
    if (gCoreContext->GetNumSetting("TruncateDeletesSlowly", 0))
    {
        TruncateAndClose(NULL, ds->m_fd, ds->m_filename, ds->m_size);
    }
    else
    {
        QMutexLocker dl(&deletelock);
        close(ds->m_fd);
    }
}

bool MainServer::HandleDeleteFile(QStringList &slist, PlaybackSock *pbs)
{
    return HandleDeleteFile(slist[1], slist[2], pbs);
}

bool MainServer::HandleDeleteFile(QString filename, QString storagegroup,
                                  PlaybackSock *pbs)
{
    StorageGroup sgroup(storagegroup, "", false);
    QStringList retlist;

    if ((filename.isEmpty()) ||
        (filename.contains("/../")) ||
        (filename.startsWith("../")))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("ERROR deleting file, filename '%1' "
                "fails sanity checks").arg(filename));
        if (pbs)
        {
            retlist << "0";
            SendResponse(pbs->getSocket(), retlist);
        }
        return false;
    }

    QString fullfile = sgroup.FindFile(filename);

    if (fullfile.isEmpty()) {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Unable to find %1 in HandleDeleteFile()") .arg(filename));
        if (pbs)
        {
            retlist << "0";
            SendResponse(pbs->getSocket(), retlist);
        }
        return false;
    }

    QFile checkFile(fullfile);
    bool followLinks = gCoreContext->GetNumSetting("DeletesFollowLinks", 0);
    int fd = -1;
    off_t size = 0;

    // This will open the file and unlink the dir entry.  The actual file
    // data will be deleted in the truncate thread spawned below.
    // Since stat fails after unlinking on some filesystems, get the size first
    const QFileInfo info(fullfile);
    size = info.size();
    fd = DeleteFile(fullfile, followLinks);

    if ((fd < 0) && checkFile.exists())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Error deleting file: %1.")
                .arg(fullfile));
        if (pbs)
        {
            retlist << "0";
            SendResponse(pbs->getSocket(), retlist);
        }
        return false;
    }

    if (pbs)
    {
        retlist << "1";
        SendResponse(pbs->getSocket(), retlist);
    }

    // DeleteFile() opened up a file for us to delete
    if (fd >= 0)
    {
        // Thread off the actual file truncate
        TruncateThread *truncateThread =
            new TruncateThread(this, fullfile, fd, size);
        truncateThread->run();
    }

    return true;
}

// Helper function for the guts of HandleCommBreakQuery + HandleCutlistQuery
void MainServer::HandleCutMapQuery(const QString &chanid,
                                   const QString &starttime,
                                   PlaybackSock *pbs, bool commbreak)
{
    MythSocket *pbssock = NULL;
    if (pbs)
        pbssock = pbs->getSocket();

    frm_dir_map_t markMap;
    frm_dir_map_t::const_iterator it;
    QDateTime recstartdt = MythDate::fromTime_t(starttime.toULongLong());
    QStringList retlist;
    int rowcnt = 0;

    const ProgramInfo pginfo(chanid.toUInt(), recstartdt);

    if (pginfo.GetChanID())
    {
        if (commbreak)
            pginfo.QueryCommBreakList(markMap);
        else
            pginfo.QueryCutList(markMap);

        for (it = markMap.begin(); it != markMap.end(); ++it)
        {
            rowcnt++;
            QString intstr = QString("%1").arg(*it);
            retlist << intstr;
            retlist << QString::number(it.key());
        }
    }

    if (rowcnt > 0)
        retlist.prepend(QString("%1").arg(rowcnt));
    else
        retlist << "-1";

    if (pbssock)
        SendResponse(pbssock, retlist);

    return;
}

void MainServer::HandleCommBreakQuery(const QString &chanid,
                                      const QString &starttime,
                                      PlaybackSock *pbs)
{
// Commercial break query
// Format:  QUERY_COMMBREAK <chanid> <starttime>
// chanid is chanid, starttime is startime of program in
//   # of seconds since Jan 1, 1970, in UTC time.  Same format as in
//   a ProgramInfo structure in a string list.
// Return structure is [number of rows] followed by a triplet of values:
//   each triplet : [type] [long portion 1] [long portion 2]
// type is the value in the map, right now 4 = commbreak start, 5= end
    return HandleCutMapQuery(chanid, starttime, pbs, true);
}

void MainServer::HandleCutlistQuery(const QString &chanid,
                                    const QString &starttime,
                                    PlaybackSock *pbs)
{
// Cutlist query
// Format:  QUERY_CUTLIST <chanid> <starttime>
// chanid is chanid, starttime is startime of program in
//   # of seconds since Jan 1, 1970, in UTC time.  Same format as in
//   a ProgramInfo structure in a string list.
// Return structure is [number of rows] followed by a triplet of values:
//   each triplet : [type] [long portion 1] [long portion 2]
// type is the value in the map, right now 0 = commbreak start, 1 = end
    return HandleCutMapQuery(chanid, starttime, pbs, false);
}


void MainServer::HandleBookmarkQuery(const QString &chanid,
                                     const QString &starttime,
                                     PlaybackSock *pbs)
// Bookmark query
// Format:  QUERY_BOOKMARK <chanid> <starttime>
// chanid is chanid, starttime is startime of program in
//   # of seconds since Jan 1, 1970, in UTC time.  Same format as in
//   a ProgramInfo structure in a string list.
// Return value is a long-long encoded as two separate values
{
    MythSocket *pbssock = NULL;
    if (pbs)
        pbssock = pbs->getSocket();

    QDateTime recstartts = MythDate::fromTime_t(starttime.toULongLong());

    uint64_t bookmark = ProgramInfo::QueryBookmark(
        chanid.toUInt(), recstartts);

    QStringList retlist;
    retlist << QString::number(bookmark);

    if (pbssock)
        SendResponse(pbssock, retlist);

    return;
}


void MainServer::HandleSetBookmark(QStringList &tokens,
                                   PlaybackSock *pbs)
{
// Bookmark query
// Format:  SET_BOOKMARK <chanid> <starttime> <position>
// chanid is chanid, starttime is startime of program in
//   # of seconds since Jan 1, 1970, in UTC time.  Same format as in
//   a ProgramInfo structure in a string list.  The two longs are the two
//   portions of the bookmark value to set.

    MythSocket *pbssock = NULL;
    if (pbs)
        pbssock = pbs->getSocket();

    QString chanid = tokens[1];
    QString starttime = tokens[2];
    long long bookmark = tokens[3].toLongLong();

    QDateTime recstartts = MythDate::fromTime_t(starttime.toULongLong());
    QStringList retlist;

    ProgramInfo pginfo(chanid.toUInt(), recstartts);

    if (pginfo.GetChanID())
    {
        pginfo.SaveBookmark(bookmark);
        retlist << "OK";
    }
    else
        retlist << "FAILED";

    if (pbssock)
        SendResponse(pbssock, retlist);

    return;
}

void MainServer::HandleSettingQuery(QStringList &tokens, PlaybackSock *pbs)
{
// Format: QUERY_SETTING <hostname> <setting>
// Returns setting value as a string

    MythSocket *pbssock = NULL;
    if (pbs)
        pbssock = pbs->getSocket();

    QString hostname = tokens[1];
    QString setting = tokens[2];
    QStringList retlist;

    QString retvalue = gCoreContext->GetSettingOnHost(setting, hostname, "-1");

    retlist << retvalue;
    if (pbssock)
        SendResponse(pbssock, retlist);

    return;
}

void MainServer::HandleDownloadFile(const QStringList &command,
                                    PlaybackSock *pbs)
{
    bool synchronous = (command[0] == "DOWNLOAD_FILE_NOW");
    QString srcURL = command[1];
    QString storageGroup = command[2];
    QString filename = command[3];
    StorageGroup sgroup(storageGroup, gCoreContext->GetHostName(), false);
    QString outDir = sgroup.FindNextDirMostFree();
    QString outFile;
    QStringList retlist;

    MythSocket *pbssock = NULL;
    if (pbs)
        pbssock = pbs->getSocket();

    if (filename.isEmpty())
    {
        QFileInfo finfo(srcURL);
        filename = finfo.fileName();
    }

    if (outDir.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Unable to determine directory "
                    "to write to in %1 write command").arg(command[0]));
        retlist << "downloadfile_directory_not_found";
        if (pbssock)
            SendResponse(pbssock, retlist);
        return;
    }

    if ((filename.contains("/../")) ||
        (filename.startsWith("../")))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("ERROR: %1 write filename '%2' does not pass "
                    "sanity checks.") .arg(command[0]).arg(filename));
        retlist << "downloadfile_filename_dangerous";
        if (pbssock)
            SendResponse(pbssock, retlist);
        return;
    }

    outFile = outDir + "/" + filename;

    if (synchronous)
    {
        if (GetMythDownloadManager()->download(srcURL, outFile))
        {
            retlist << "OK";
            retlist << gCoreContext->GetMasterHostPrefix(storageGroup)
                       + filename;
        }
        else
            retlist << "ERROR";
    }
    else
    {
        QMutexLocker locker(&m_downloadURLsLock);
        m_downloadURLs[outFile] =
            gCoreContext->GetMasterHostPrefix(storageGroup) +
            StorageGroup::GetRelativePathname(outFile);

        GetMythDownloadManager()->queueDownload(srcURL, outFile, this);
        retlist << "OK";
        retlist << gCoreContext->GetMasterHostPrefix(storageGroup) + filename;
    }

    if (pbssock)
        SendResponse(pbssock, retlist);
}

void MainServer::HandleSetSetting(QStringList &tokens,
                                  PlaybackSock *pbs)
{
// Format: SET_SETTING <hostname> <setting> <value>
    MythSocket *pbssock = NULL;
    if (pbs)
        pbssock = pbs->getSocket();

    QString hostname = tokens[1];
    QString setting = tokens[2];
    QString svalue = tokens[3];
    QStringList retlist;

    if (gCoreContext->SaveSettingOnHost(setting, svalue, hostname))
        retlist << "OK";
    else
        retlist << "ERROR";

    if (pbssock)
        SendResponse(pbssock, retlist);

    return;
}

void MainServer::HandleScanVideos(PlaybackSock *pbs)
{
    MythSocket *pbssock = pbs->getSocket();

    QStringList retlist;

    if (metadatafactory)
    {
        QStringList hosts;
        GetActiveBackends(hosts);
        metadatafactory->VideoScan(hosts);
        retlist << "OK";
    }
    else
        retlist << "ERROR";

    if (pbssock)
        SendResponse(pbssock, retlist);
}

void MainServer::HandleScanMusic(const QStringList &slist, PlaybackSock *pbs)
{
    MythSocket *pbssock = pbs->getSocket();

    QStringList strlist;

    if (ismaster)
    {
        // get a list of hosts with a directory defined for the 'Music' storage group
        MSqlQuery query(MSqlQuery::InitCon());
        QString sql = "SELECT DISTINCT hostname "
                      "FROM storagegroup "
                      "WHERE groupname = 'Music'";
        if (!query.exec(sql) || !query.isActive())
            MythDB::DBError("MainServer::HandleScanMusic get host list", query);
        else
        {
            while(query.next())
            {
                QString hostname = query.value(0).toString();

                if (hostname == gCoreContext->GetHostName())
                {
                    // this is the master BE with a music storage group directory defined so run the file scanner
                    LOG(VB_GENERAL, LOG_INFO, LOC +
                        QString("HandleScanMusic: running filescanner on master BE '%1'").arg(hostname));
                    QScopedPointer<MythSystem> cmd(MythSystem::Create(GetAppBinDir() + "mythutil --scanmusic",
                                                                      kMSAutoCleanup | kMSRunBackground |
                                                                      kMSDontDisableDrawing | kMSProcessEvents |
                                                                      kMSDontBlockInputDevs));
                }
                else
                {
                    // found a slave BE so ask it to run the file scanner
                    PlaybackSock *slave = GetMediaServerByHostname(hostname);
                    if (slave)
                    {
                        LOG(VB_GENERAL, LOG_INFO, LOC +
                            QString("HandleScanMusic: asking slave '%1' to run file scanner").arg(hostname));
                        slave->ForwardRequest(slist);
                        slave->DecrRef();
                    }
                    else
                    {
                        LOG(VB_GENERAL, LOG_INFO, LOC +
                            QString("HandleScanMusic: Failed to grab slave socket on '%1'").arg(hostname));
                    }
                }
            }
        }
    }
    else
    {
        // must be a slave with a music storage group directory defined so run the file scanner
        LOG(VB_GENERAL, LOG_INFO,  LOC +
            QString("HandleScanMusic: running filescanner on slave BE '%1'")
                .arg(gCoreContext->GetHostName()));
        QScopedPointer<MythSystem> cmd(MythSystem::Create(GetAppBinDir() + "mythutil --scanmusic",
                                                          kMSAutoCleanup | kMSRunBackground |
                                                          kMSDontDisableDrawing | kMSProcessEvents |
                                                          kMSDontBlockInputDevs));
    }

    strlist << "OK";

    if (pbssock)
        SendResponse(pbssock, strlist);
}

void MainServer::HandleMusicTagUpdateVolatile(const QStringList &slist, PlaybackSock *pbs)
{
// format: MUSIC_TAG_UPDATE_VOLATILE <hostname> <songid> <rating> <playcount> <lastplayed>

    QStringList strlist;

    MythSocket *pbssock = pbs->getSocket();

    QString hostname = slist[1];

    if (ismaster && !gCoreContext->IsThisHost(hostname))
    {
        // forward the request to the slave BE
        PlaybackSock *slave = GetMediaServerByHostname(hostname);
        if (slave)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("HandleMusicTagUpdateVolatile: asking slave '%1' to update the metadata").arg(hostname));
            strlist = slave->ForwardRequest(slist);
            slave->DecrRef();

            if (pbssock)
                SendResponse(pbssock, strlist);

            return;
        }
        else
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("HandleMusicTagUpdateVolatile: Failed to grab slave socket on '%1'").arg(hostname));

            strlist << "ERROR: slave not found";

            if (pbssock)
                SendResponse(pbssock, strlist);

            return;
        }
    }
    else
    {
        //  run mythutil to update the metadata
        QStringList paramList;
        paramList.append(QString("--songid='%1'").arg(slist[2]));
        paramList.append(QString("--rating='%1'").arg(slist[3]));
        paramList.append(QString("--playcount='%1'").arg(slist[4]));
        paramList.append(QString("--lastplayed='%1'").arg(slist[5]));

        QString command = GetAppBinDir() + "mythutil --updatemeta " + paramList.join(" ");

        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("HandleMusicTagUpdateVolatile: running %1'").arg(command));
        QScopedPointer<MythSystem> cmd(MythSystem::Create(command,
                                                          kMSAutoCleanup | kMSRunBackground |
                                                          kMSDontDisableDrawing | kMSProcessEvents |
                                                          kMSDontBlockInputDevs));
    }

    strlist << "OK";

    if (pbssock)
        SendResponse(pbssock, strlist);
}

void MainServer::HandleMusicCalcTrackLen(const QStringList &slist, PlaybackSock *pbs)
{
// format: MUSIC_CALC_TRACK_LENGTH <hostname> <songid>

    QStringList strlist;

    MythSocket *pbssock = pbs->getSocket();

    QString hostname = slist[1];

    if (ismaster && !gCoreContext->IsThisHost(hostname))
    {
        // forward the request to the slave BE
        PlaybackSock *slave = GetMediaServerByHostname(hostname);
        if (slave)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("HandleMusicCalcTrackLen: asking slave '%1' to update the track length").arg(hostname));
            strlist = slave->ForwardRequest(slist);
            slave->DecrRef();

            if (pbssock)
                SendResponse(pbssock, strlist);

            return;
        }
        else
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("HandleMusicCalcTrackLen: Failed to grab slave socket on '%1'").arg(hostname));

            strlist << "ERROR: slave not found";

            if (pbssock)
                SendResponse(pbssock, strlist);

            return;
        }
    }
    else
    {
        //  run mythutil to calc the tracks length
        QStringList paramList;
        paramList.append(QString("--songid='%1'").arg(slist[2]));

        QString command = GetAppBinDir() + "mythutil --calctracklen " + paramList.join(" ");

        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("HandleMusicCalcTrackLen: running %1'").arg(command));
        QScopedPointer<MythSystem> cmd(MythSystem::Create(command,
                                                          kMSAutoCleanup | kMSRunBackground |
                                                          kMSDontDisableDrawing | kMSProcessEvents |
                                                          kMSDontBlockInputDevs));
    }

    strlist << "OK";

    if (pbssock)
        SendResponse(pbssock, strlist);
}

void MainServer::HandleMusicTagUpdateMetadata(const QStringList &slist, PlaybackSock *pbs)
{
// format: MUSIC_TAG_UPDATE_METADATA <hostname> <songid>
// this assumes the new metadata has already been saved to the database for this track

    QStringList strlist;

    MythSocket *pbssock = pbs->getSocket();

    QString hostname = slist[1];

    if (ismaster && !gCoreContext->IsThisHost(hostname))
    {
        // forward the request to the slave BE
        PlaybackSock *slave = GetMediaServerByHostname(hostname);
        if (slave)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("HandleMusicTagUpdateMetadata: asking slave '%1' "
                        "to update the metadata").arg(hostname));
            strlist = slave->ForwardRequest(slist);
            slave->DecrRef();

            if (pbssock)
                SendResponse(pbssock, strlist);

            return;
        }
        else
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("HandleMusicTagUpdateMetadata: Failed to grab "
                        "slave socket on '%1'").arg(hostname));

            strlist << "ERROR: slave not found";

            if (pbssock)
                SendResponse(pbssock, strlist);

            return;
        }
    }
    else
    {
        // load the new metadata from the database
        int songID = slist[2].toInt();

        MusicMetadata *mdata = MusicMetadata::createFromID(songID);

        if (!mdata)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("HandleMusicTagUpdateMetadata: "
                        "Cannot find metadata for trackid: %1")
                    .arg(songID));

            strlist << "ERROR: track not found";

            if (pbssock)
                SendResponse(pbssock, strlist);

            return;
        }

        MetaIO *tagger = mdata->getTagger();
        if (tagger)
        {
            if (!tagger->write(mdata->getLocalFilename(), mdata))
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("HandleMusicTagUpdateMetadata: "
                            "Failed to write to tag for trackid: %1")
                        .arg(songID));

                strlist << "ERROR: write to tag failed";

                if (pbssock)
                    SendResponse(pbssock, strlist);

                return;
            }
        }
    }

    strlist << "OK";

    if (pbssock)
        SendResponse(pbssock, strlist);
}


void MainServer::HandleMusicFindAlbumArt(const QStringList &slist, PlaybackSock *pbs)
{
// format: MUSIC_FIND_ALBUMART <hostname> <songid> <update_database>

    QStringList strlist;

    MythSocket *pbssock = pbs->getSocket();

    QString hostname = slist[1];

    if (ismaster && !gCoreContext->IsThisHost(hostname))
    {
        // forward the request to the slave BE
        PlaybackSock *slave = GetMediaServerByHostname(hostname);
        if (slave)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("HandleMusicFindAlbumArt: asking slave '%1' "
                        "to update the albumart").arg(hostname));
            strlist = slave->ForwardRequest(slist);
            slave->DecrRef();

            if (pbssock)
                SendResponse(pbssock, strlist);

            return;
        }
        else
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("HandleMusicFindAlbumArt: Failed to grab "
                        "slave socket on '%1'").arg(hostname));

            strlist << "ERROR: slave not found";

            if (pbssock)
                SendResponse(pbssock, strlist);

            return;
        }
    }
    else
    {
        // find the track in the database
        int songID = slist[2].toInt();
        bool updateDatabase = (slist[3].toInt() == 1);

        MusicMetadata *mdata = MusicMetadata::createFromID(songID);

        if (!mdata)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("HandleMusicFindAlbumArt: "
                        "Cannot find metadata for trackid: %1").arg(songID));

            strlist << "ERROR: track not found";

            if (pbssock)
                SendResponse(pbssock, strlist);

            return;
        }

        // find any directory images
        QFileInfo fi(mdata->getLocalFilename());
        QDir dir = fi.absoluteDir();

        QString nameFilter = gCoreContext->GetSetting("AlbumArtFilter",
                                                    "*.png;*.jpg;*.jpeg;*.gif;*.bmp");
        dir.setNameFilters(nameFilter.split(";"));

        QStringList files = dir.entryList();

        // create an empty image list
        AlbumArtImages *images = new AlbumArtImages(mdata, false);

        fi.setFile(mdata->Filename(false));
        QString startDir = fi.path();

        for (int x = 0; x < files.size(); x++)
        {
            fi.setFile(files.at(x));
            AlbumArtImage *image = new AlbumArtImage();
            image->filename = startDir + '/' + fi.fileName();
            image->hostname = gCoreContext->GetHostName();
            image->embedded = false;
            image->imageType = AlbumArtImages::guessImageType(image->filename);
            image->description = "";
            images->addImage(image);
            delete image;
        }

        // find any embedded albumart in the tracks tag
        MetaIO *tagger = mdata->getTagger();
        if (tagger)
        {
            if (tagger->supportsEmbeddedImages())
            {
                AlbumArtList artList = tagger->getAlbumArtList(mdata->getLocalFilename());

                for (int x = 0; x < artList.count(); x++)
                {
                    AlbumArtImage *image = artList.at(x);
                    image->filename = QString("%1-%2").arg(mdata->ID()).arg(image->filename);
                    images->addImage(image);
                }
            }

            delete tagger;
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("HandleMusicFindAlbumArt: "
                        "Failed to find a tagger for trackid: %1").arg(songID));
        }

        // finally save the result to the database
        if (updateDatabase)
            images->dumpToDatabase();

        strlist << "OK";
        strlist.append(QString("%1").arg(images->getImageCount()));

        for (uint x = 0; x < images->getImageCount(); x++)
        {
            AlbumArtImage *image = images->getImageAt(x);
            strlist.append(QString("%1").arg(image->id));
            strlist.append(QString("%1").arg((int)image->imageType));
            strlist.append(QString("%1").arg(image->embedded));
            strlist.append(image->description);
            strlist.append(image->filename);
            strlist.append(image->hostname);

            // if this is an embedded image update the cached image
            if (image->embedded)
            {
                QStringList paramList;
                paramList.append(QString("--songid='%1'").arg(mdata->ID()));
                paramList.append(QString("--imagetype='%1'").arg(image->imageType));

                QString command = GetAppBinDir() + "mythutil --extractimage " + paramList.join(" ");
                QScopedPointer<MythSystem> cmd(MythSystem::Create(command,
                                                    kMSAutoCleanup | kMSRunBackground |
                                                    kMSDontDisableDrawing | kMSProcessEvents |
                                                    kMSDontBlockInputDevs));
            }
        }

        delete images;
    }

    if (pbssock)
        SendResponse(pbssock, strlist);
}

void MainServer::HandleMusicTagGetImage(const QStringList &slist, PlaybackSock *pbs)
{
// format: MUSIC_TAG_GETIMAGE <hostname> <songid> <imagetype>

    QStringList strlist;

    MythSocket *pbssock = pbs->getSocket();

    QString hostname = slist[1];
    QString songid = slist[2];
    QString imagetype = slist[3];

    if (ismaster && !gCoreContext->IsThisHost(hostname))
    {
        // forward the request to the slave BE
        PlaybackSock *slave = GetMediaServerByHostname(hostname);
        if (slave)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("HandleMusicTagGetImage: asking slave '%1' to "
                        "extract the image").arg(hostname));
            strlist = slave->ForwardRequest(slist);
            slave->DecrRef();

            if (pbssock)
                SendResponse(pbssock, strlist);

            return;
        }
        else
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("HandleMusicTagGetImage: Failed to grab slave "
                        "socket on '%1'").arg(hostname));
        }
    }
    else
    {
        QStringList paramList;
        paramList.append(QString("--songid='%1'").arg(songid));
        paramList.append(QString("--imagetype='%1'").arg(imagetype));

        QString command = GetAppBinDir() + "mythutil --extractimage " + paramList.join(" ");

        QScopedPointer<MythSystem> cmd(MythSystem::Create(command,
                                       kMSAutoCleanup | kMSRunBackground |
                                       kMSDontDisableDrawing | kMSProcessEvents |
                                       kMSDontBlockInputDevs));
    }

    strlist << "OK";

    if (pbssock)
        SendResponse(pbssock, strlist);
}

void MainServer::HandleMusicTagChangeImage(const QStringList &slist, PlaybackSock *pbs)
{
// format: MUSIC_TAG_CHANGEIMAGE <hostname> <songid> <oldtype> <newtype>

    QStringList strlist;

    MythSocket *pbssock = pbs->getSocket();

    QString hostname = slist[1];

    if (ismaster && !gCoreContext->IsThisHost(hostname))
    {
        // forward the request to the slave BE
        PlaybackSock *slave = GetMediaServerByHostname(hostname);
        if (slave)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("HandleMusicTagChangeImage: asking slave '%1' "
                        "to update the metadata").arg(hostname));
            strlist = slave->ForwardRequest(slist);
            slave->DecrRef();

            if (pbssock)
                SendResponse(pbssock, strlist);

            return;
        }
        else
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("HandleMusicTagChangeImage: Failed to grab "
                        "slave socket on '%1'").arg(hostname));

            strlist << "ERROR: slave not found";

            if (pbssock)
                SendResponse(pbssock, strlist);

            return;
        }
    }
    else
    {
        int songID = slist[2].toInt();
        ImageType oldType = (ImageType)slist[3].toInt();
        ImageType newType = (ImageType)slist[4].toInt();

        // load the metadata from the database
        MusicMetadata *mdata = MusicMetadata::createFromID(songID);

        if (!mdata)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("HandleMusicTagChangeImage: "
                        "Cannot find metadata for trackid: %1")
                    .arg(songID));

            strlist << "ERROR: track not found";

            if (pbssock)
                SendResponse(pbssock, strlist);

            return;
        }

        mdata->setFilename(mdata->getLocalFilename());

        AlbumArtImages *albumArt = mdata->getAlbumArtImages();
        AlbumArtImage *image = albumArt->getImage(oldType);
        if (image)
        {
            AlbumArtImage oldImage = *image;

            image->imageType = (ImageType) newType;

            if (image->imageType == oldImage.imageType)
            {
                // nothing to change
                strlist << "OK";

                if (pbssock)
                    SendResponse(pbssock, strlist);

                delete mdata;

                return;
            }

            // rename any cached image to match the new type
            if (image->embedded)
            {
                // change the image type in the tag if it supports it
                MetaIO *tagger = mdata->getTagger();

                if (tagger && tagger->supportsEmbeddedImages())
                {
                    if (!tagger->changeImageType(mdata->getLocalFilename(), &oldImage, image->imageType))
                    {
                        LOG(VB_GENERAL, LOG_ERR, "HandleMusicTagChangeImage: failed to change image type");

                        strlist << "ERROR: failed to change image type";

                        if (pbssock)
                            SendResponse(pbssock, strlist);

                        delete mdata;
                        delete tagger;

                        return;
                    }
                }

                if (tagger)
                    delete tagger;

                // update the new cached image filename
                StorageGroup artGroup("MusicArt", gCoreContext->GetHostName(), false);
                oldImage.filename = artGroup.FindFile("AlbumArt/" + image->filename);

                QFileInfo fi(oldImage.filename);
                image->filename = QString(fi.path() + "/%1-%2.jpg")
                                          .arg(mdata->ID())
                                          .arg(AlbumArtImages::getTypeFilename(image->imageType));

                // remove any old cached file with the same name as the new one
                if (QFile::exists(image->filename))
                    QFile::remove(image->filename);

                // rename the old cached file to the new one
                if (image->filename != oldImage.filename && QFile::exists(oldImage.filename))
                    QFile::rename(oldImage.filename, image->filename);
                else
                {
                    // extract the image from the tag and cache it
                    QStringList paramList;
                    paramList.append(QString("--songid='%1'").arg(mdata->ID()));
                    paramList.append(QString("--imagetype='%1'").arg(image->imageType));

                    QString command = GetAppBinDir() + "mythutil --extractimage " + paramList.join(" ");

                    QScopedPointer<MythSystem> cmd(MythSystem::Create(command,
                                                   kMSAutoCleanup | kMSRunBackground |
                                                   kMSDontDisableDrawing | kMSProcessEvents |
                                                   kMSDontBlockInputDevs));
                }
            }
            else
            {
                QFileInfo fi(oldImage.filename);

                // get the new images filename
                image->filename = QString(fi.absolutePath() + "/%1.jpg")
                        .arg(AlbumArtImages::getTypeFilename(image->imageType));

                if (image->filename != oldImage.filename && QFile::exists(oldImage.filename))
                {
                    // remove any old cached file with the same name as the new one
                    QFile::remove(image->filename);
                    // rename the old cached file to the new one
                    QFile::rename(oldImage.filename, image->filename);
                }
            }
        }

        delete mdata;
    }

    strlist << "OK";

    if (pbssock)
        SendResponse(pbssock, strlist);
}

void MainServer::HandleMusicTagAddImage(const QStringList& slist, PlaybackSock* pbs)
{
// format: MUSIC_TAG_ADDIMAGE <hostname> <songid> <filename> <imagetype>

    QStringList strlist;

    MythSocket *pbssock = pbs->getSocket();

    QString hostname = slist[1];

    if (ismaster && !gCoreContext->IsThisHost(hostname))
    {
        // forward the request to the slave BE
        PlaybackSock *slave = GetMediaServerByHostname(hostname);
        if (slave)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("HandleMusicTagAddImage: asking slave '%1' "
                        "to add the image").arg(hostname));
            strlist = slave->ForwardRequest(slist);
            slave->DecrRef();

            if (pbssock)
                SendResponse(pbssock, strlist);

            return;
        }
        else
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("HandleMusicTagAddImage: Failed to grab "
                        "slave socket on '%1'").arg(hostname));

            strlist << "ERROR: slave not found";

            if (pbssock)
                SendResponse(pbssock, strlist);

            return;
        }
    }
    else
    {
        // load the metadata from the database
        int songID = slist[2].toInt();
        QString filename = slist[3];
        ImageType imageType = (ImageType) slist[4].toInt();

        MusicMetadata *mdata = MusicMetadata::createFromID(songID);

        if (!mdata)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("HandleMusicTagAddImage: Cannot find metadata for trackid: %1")
                        .arg(songID));

            strlist << "ERROR: track not found";

            if (pbssock)
                SendResponse(pbssock, strlist);

            return;
        }

        MetaIO *tagger = mdata->getTagger();

        if (!tagger)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "HandleMusicTagAddImage: failed to find a tagger for track");

            strlist << "ERROR: tagger not found";

            if (pbssock)
                SendResponse(pbssock, strlist);

            delete mdata;
            return;
        }

        if (!tagger->supportsEmbeddedImages())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "HandleMusicTagAddImage: asked to write album art to the tag "
                "but the tagger doesn't support it!");

            strlist << "ERROR: embedded images not supported by tag";

            if (pbssock)
                SendResponse(pbssock, strlist);

            delete tagger;
            delete mdata;
            return;
        }

        // is the image in the 'MusicArt' storage group
        bool isDirectoryImage = false;
        StorageGroup storageGroup("MusicArt", gCoreContext->GetHostName(), false);
        QString imageFilename = storageGroup.FindFile("AlbumArt/" + filename);
        if (imageFilename.isEmpty())
        {
            // not found there so look in the tracks directory
            QFileInfo fi(mdata->getLocalFilename());
            imageFilename = fi.absolutePath() + '/' + filename;
            isDirectoryImage = true;
        }

        if (!QFile::exists(imageFilename))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("HandleMusicTagAddImage: cannot find image file %1").arg(filename));

            strlist << "ERROR: failed to find image file";

            if (pbssock)
                SendResponse(pbssock, strlist);

            delete tagger;
            delete mdata;
            return;
        }

        AlbumArtImage image;
        image.filename = imageFilename;
        image.imageType = imageType;

        if (!tagger->writeAlbumArt(mdata->getLocalFilename(), &image))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "HandleMusicTagAddImage: failed to write album art to tag");

            strlist << "ERROR: failed to write album art to tag";

            if (pbssock)
                SendResponse(pbssock, strlist);

            if (!isDirectoryImage)
                QFile::remove(imageFilename);

            delete tagger;
            delete mdata;
            return;
        }

        // only remove the image if we temporarily saved one to the 'AlbumArt' storage group
        if (!isDirectoryImage)
            QFile::remove(imageFilename);

        delete tagger;
        delete mdata;
    }


    strlist << "OK";

    if (pbssock)
        SendResponse(pbssock, strlist);
}


void MainServer::HandleMusicTagRemoveImage(const QStringList& slist, PlaybackSock* pbs)
{
// format: MUSIC_TAG_REMOVEIMAGE <hostname> <songid> <imageid>

    QStringList strlist;

    MythSocket *pbssock = pbs->getSocket();

    QString hostname = slist[1];

    if (ismaster && !gCoreContext->IsThisHost(hostname))
    {
        // forward the request to the slave BE
        PlaybackSock *slave = GetMediaServerByHostname(hostname);
        if (slave)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("HandleMusicTagRemoveImage: asking slave '%1' "
                        "to remove the image").arg(hostname));
            strlist = slave->ForwardRequest(slist);
            slave->DecrRef();

            if (pbssock)
                SendResponse(pbssock, strlist);

            return;
        }
        else
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("HandleMusicTagRemoveImage: Failed to grab "
                        "slave socket on '%1'").arg(hostname));

            strlist << "ERROR: slave not found";

            if (pbssock)
                SendResponse(pbssock, strlist);

            return;
        }
    }
    else
    {
        int songID = slist[2].toInt();
        int imageID = slist[3].toInt();

        // load the metadata from the database
        MusicMetadata *mdata = MusicMetadata::createFromID(songID);

        if (!mdata)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("HandleMusicTagRemoveImage: Cannot find metadata for trackid: %1")
                        .arg(songID));

            strlist << "ERROR: track not found";

            if (pbssock)
                SendResponse(pbssock, strlist);

            return;
        }

        MetaIO *tagger = mdata->getTagger();

        if (!tagger)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "HandleMusicTagRemoveImage: failed to find a tagger for track");

            strlist << "ERROR: tagger not found";

            if (pbssock)
                SendResponse(pbssock, strlist);

            delete mdata;
            return;
        }

        if (!tagger->supportsEmbeddedImages())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "HandleMusicTagRemoveImage: asked to remove album art "
                                           "from the tag but the tagger doesn't support it!");

            strlist << "ERROR: embedded images not supported by tag";

            if (pbssock)
                SendResponse(pbssock, strlist);

            delete mdata;
            delete tagger;
            return;
        }

        AlbumArtImage *image = mdata->getAlbumArtImages()->getImageByID(imageID);
        if (!image)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("HandleMusicTagRemoveImage: Cannot find image for imageid: %1")
                        .arg(imageID));

            strlist << "ERROR: image not found";

            if (pbssock)
                SendResponse(pbssock, strlist);

            delete mdata;
            delete tagger;
            return;
        }

        if (!tagger->removeAlbumArt(mdata->getLocalFilename(), image))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "HandleMusicTagRemoveImage: failed to remove album art from tag");

            strlist << "ERROR: failed to remove album art from tag";

            if (pbssock)
                SendResponse(pbssock, strlist);

            return;
        }
    }

    strlist << "OK";

    if (pbssock)
        SendResponse(pbssock, strlist);
}

void MainServer::HandleMusicFindLyrics(const QStringList &slist, PlaybackSock *pbs)
{
// format: MUSIC_LYRICS_FIND <hostname> <songid> <grabbername> <artist (optional)> <album (optional)> <title (optional)>
// if artist is present then album and title must also be included (only used for radio and cd tracks)

    QStringList strlist;

    MythSocket *pbssock = pbs->getSocket();

    QString hostname = slist[1];
    QString songid = slist[2];
    QString grabberName = slist[3];
    QString artist = "";
    QString album = "";
    QString title = "";

    if (slist.size() == 7)
    {
        artist = slist[4];
        album = slist[5];
        title = slist[6];
    }

    if (ismaster && !gCoreContext->IsThisHost(hostname))
    {
        // forward the request to the slave BE
        PlaybackSock *slave = GetMediaServerByHostname(hostname);
        if (slave)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("HandleMusicFindLyrics: asking slave '%1' to "
                        "find lyrics").arg(hostname));
            strlist = slave->ForwardRequest(slist);
            slave->DecrRef();

            if (pbssock)
                SendResponse(pbssock, strlist);

            return;
        }
        else
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("HandleMusicFindLyrics: Failed to grab slave "
                        "socket on '%1'").arg(hostname));
        }
    }
    else
    {
        QStringList paramList;
        paramList.append(QString("--songid='%1'").arg(songid));
        paramList.append(QString("--grabber='%1'").arg(grabberName));

        if (!artist.isEmpty())
            paramList.append(QString("--artist=\"%1\"").arg(artist));

        if (!album.isEmpty())
            paramList.append(QString("--album=\"%1\"").arg(album));

        if (!title.isEmpty())
            paramList.append(QString("--title=\"%1\"").arg(title));

        QString command = GetAppBinDir() + "mythutil --findlyrics " + paramList.join(" ");

        QScopedPointer<MythSystem> cmd(MythSystem::Create(command,
                                       kMSAutoCleanup | kMSRunBackground |
                                       kMSDontDisableDrawing | kMSProcessEvents |
                                       kMSDontBlockInputDevs));
    }

    strlist << "OK";

    if (pbssock)
        SendResponse(pbssock, strlist);
}

void MainServer::HandleMusicGetLyricGrabbers(const QStringList &slist, PlaybackSock *pbs)
{
    QStringList strlist;

    MythSocket *pbssock = pbs->getSocket();

    QString  scriptDir = GetShareDir() + "metadata/Music/lyrics";
    QDir d(scriptDir);

    if (!d.exists())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Cannot find lyric scripts directory: %1").arg(scriptDir));
        strlist << QString("ERROR: Cannot find lyric scripts directory: %1").arg(scriptDir);

        if (pbssock)
            SendResponse(pbssock, strlist);

        return;
    }

    d.setFilter(QDir::Files | QDir::NoDotAndDotDot);
    d.setNameFilters(QStringList("*.py"));
    QFileInfoList list = d.entryInfoList();
    if (list.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Cannot find any lyric scripts in: %1").arg(scriptDir));
        strlist << QString("ERROR: Cannot find any lyric scripts in: %1").arg(scriptDir);

        if (pbssock)
            SendResponse(pbssock, strlist);

        return;
    }

    QStringList scripts;
    QFileInfoList::const_iterator it = list.begin();
    const QFileInfo *fi;

    while (it != list.end())
    {
        fi = &(*it);
        ++it;
        LOG(VB_FILE, LOG_NOTICE, QString("Found lyric script at: %1").arg(fi->filePath()));
        scripts.append(fi->filePath());
    }

    QStringList grabbers;

    // query the grabbers to get their name
    for (int x = 0; x < scripts.count(); x++)
    {
        QProcess p;
        p.start(QString("python %1 -v").arg(scripts.at(x)));
        p.waitForFinished(-1);
        QString result = p.readAllStandardOutput();

        QDomDocument domDoc;
        QString errorMsg;
        int errorLine, errorColumn;

        if (!domDoc.setContent(result, false, &errorMsg, &errorLine, &errorColumn))
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("FindLyrics: Could not parse version from %1").arg(scripts.at(x)) +
                QString("\n\t\t\tError at line: %1  column: %2 msg: %3").arg(errorLine).arg(errorColumn).arg(errorMsg));
            continue;
        }

        QDomNodeList itemList = domDoc.elementsByTagName("grabber");
        QDomNode itemNode = itemList.item(0);

        grabbers.append(itemNode.namedItem(QString("name")).toElement().text());
    }

    grabbers.sort();

    strlist << "OK";

    for (int x = 0; x < grabbers.count(); x++)
        strlist.append(grabbers.at(x));

    if (pbssock)
        SendResponse(pbssock, strlist);
}

void MainServer::HandleMusicSaveLyrics(const QStringList& slist, PlaybackSock* pbs)
{
// format: MUSIC_LYRICS_SAVE <hostname> <songid>
// followed by the lyrics lines

    QStringList strlist;

    MythSocket *pbssock = pbs->getSocket();

    QString hostname = slist[1];
    int songID = slist[2].toInt();

    if (ismaster && !gCoreContext->IsThisHost(hostname))
    {
        // forward the request to the slave BE
        PlaybackSock *slave = GetMediaServerByHostname(hostname);
        if (slave)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("HandleMusicSaveLyrics: asking slave '%1' to "
                        "save the lyrics").arg(hostname));
            strlist = slave->ForwardRequest(slist);
            slave->DecrRef();

            if (pbssock)
                SendResponse(pbssock, strlist);

            return;
        }
        else
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
               QString("HandleMusicSaveLyrics: Failed to grab slave "
                        "socket on '%1'").arg(hostname));
        }
    }
    else
    {
        MusicMetadata *mdata = MusicMetadata::createFromID(songID);
        if (!mdata)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Cannot find metadata for trackid: %1").arg(songID));
            strlist << QString("ERROR: Cannot find metadata for trackid: %1").arg(songID);

            if (pbssock)
                SendResponse(pbssock, strlist);

            return;
        }

        QString lyricsFile = GetConfDir() + QString("/MythMusic/Lyrics/%1.txt").arg(songID);

        // remove any existing lyrics for this songID
        if (QFile::exists(lyricsFile))
            QFile::remove(lyricsFile);

        // save the new lyrics
        QFile file(QLatin1String(qPrintable(lyricsFile)));

        if (file.open(QIODevice::WriteOnly))
        {
            QTextStream stream(&file);
            for (int x = 3; x < slist.count(); x++)
                stream << slist.at(x);
            file.close();
        }
    }

    strlist << "OK";

    if (pbssock)
        SendResponse(pbssock, strlist);
}

void MainServer::HandleFileTransferQuery(QStringList &slist,
                                         QStringList &commands,
                                         PlaybackSock *pbs)
{
    MythSocket *pbssock = pbs->getSocket();

    int recnum = commands[1].toInt();
    QString command = slist[1];

    QStringList retlist;

    sockListLock.lockForRead();
    FileTransfer *ft = GetFileTransferByID(recnum);
    if (!ft)
    {
        if (command == "DONE")
        {
            // if there is an error opening the file, we may not have a
            // FileTransfer instance for this connection.
            retlist << "OK";
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Unknown file transfer socket: %1").arg(recnum));
            retlist << QString("ERROR: Unknown file transfer socket: %1")
                               .arg(recnum);
        }

        sockListLock.unlock();
        SendResponse(pbssock, retlist);
        return;
    }

    ft->IncrRef();
    sockListLock.unlock();

    if (command == "REQUEST_BLOCK")
    {
        int size = slist[2].toInt();

        retlist << QString::number(ft->RequestBlock(size));
    }
    else if (command == "WRITE_BLOCK")
    {
        int size = slist[2].toInt();

        retlist << QString::number(ft->WriteBlock(size));
    }
    else if (command == "SEEK")
    {
        long long pos = slist[2].toLongLong();
        int whence = slist[3].toInt();
        long long curpos = slist[4].toLongLong();

        long long ret = ft->Seek(curpos, pos, whence);
        retlist << QString::number(ret);
    }
    else if (command == "IS_OPEN")
    {
        bool isopen = ft->isOpen();

        retlist << QString::number(isopen);
    }
    else if (command == "REOPEN")
    {
        retlist << QString::number(ft->ReOpen(slist[2]));
    }
    else if (command == "DONE")
    {
        ft->Stop();
        retlist << "OK";
    }
    else if (command == "SET_TIMEOUT")
    {
        bool fast = slist[2].toInt();
        ft->SetTimeout(fast);
        retlist << "OK";
    }
    else if (command == "REQUEST_SIZE")
    {
        // return size and if the file is not opened for writing
        retlist << QString::number(ft->GetFileSize());
        retlist << QString::number(!gCoreContext->IsRegisteredFileForWrite(ft->GetFileName()));
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Unknown command: %1").arg(command));
        retlist << "ERROR" << "invalid_call";
    }

    ft->DecrRef();

    SendResponse(pbssock, retlist);
}

void MainServer::HandleGetRecorderNum(QStringList &slist, PlaybackSock *pbs)
{
    MythSocket *pbssock = pbs->getSocket();

    int retval = -1;

    QStringList::const_iterator it = slist.begin() + 1;
    ProgramInfo pginfo(it, slist.end());

    EncoderLink *encoder = NULL;

    QMap<int, EncoderLink *>::Iterator iter = encoderList->begin();
    for (; iter != encoderList->end(); ++iter)
    {
        EncoderLink *elink = *iter;

        if (elink->IsConnected() && elink->MatchesRecording(&pginfo))
        {
            retval = iter.key();
            encoder = elink;
        }
    }

    QStringList strlist( QString::number(retval) );

    if (encoder)
    {
        if (encoder->IsLocal())
        {
            strlist << gCoreContext->GetBackendServerIP();
            strlist << QString::number(gCoreContext->GetBackendServerPort());
        }
        else
        {
            strlist << gCoreContext->GetBackendServerIP(encoder->GetHostName());
            strlist << QString::number(gCoreContext->GetBackendServerPort(encoder->GetHostName()));
        }
    }
    else
    {
        strlist << "nohost";
        strlist << "-1";
    }

    SendResponse(pbssock, strlist);
}

void MainServer::HandleGetRecorderFromNum(QStringList &slist,
                                          PlaybackSock *pbs)
{
    MythSocket *pbssock = pbs->getSocket();

    int recordernum = slist[1].toInt();
    EncoderLink *encoder = NULL;
    QStringList strlist;

    QMap<int, EncoderLink *>::Iterator iter = encoderList->find(recordernum);

    if (iter != encoderList->end())
        encoder =  (*iter);

    if (encoder && encoder->IsConnected())
    {
        if (encoder->IsLocal())
        {
            strlist << gCoreContext->GetBackendServerIP();
            strlist << QString::number(gCoreContext->GetBackendServerPort());
        }
        else
        {
            strlist << gCoreContext->GetBackendServerIP(encoder->GetHostName());
            strlist << QString::number(gCoreContext->GetBackendServerPort(encoder->GetHostName()));
        }
    }
    else
    {
        strlist << "nohost";
        strlist << "-1";
    }

    SendResponse(pbssock, strlist);
}

void MainServer::HandleMessage(QStringList &slist, PlaybackSock *pbs)
{
    if (slist.size() < 2)
        return;

    MythSocket *pbssock = pbs->getSocket();

    QString message = slist[1];
    QStringList extra_data;
    for (uint i = 2; i < (uint) slist.size(); i++)
        extra_data.push_back(slist[i]);

    if (extra_data.empty())
    {
        MythEvent me(message);
        gCoreContext->dispatch(me);
    }
    else
    {
        MythEvent me(message, extra_data);
        gCoreContext->dispatch(me);
    }

    QStringList retlist( "OK" );

    SendResponse(pbssock, retlist);
}

void MainServer::HandleSetVerbose(QStringList &slist, PlaybackSock *pbs)
{
    MythSocket *pbssock = pbs->getSocket();
    QStringList retlist;

    QString newverbose = slist[1];
    int len = newverbose.length();
    if (len > 12)
    {
        verboseArgParse(newverbose.right(len-12));
        logPropagateCalc();

        LOG(VB_GENERAL, LOG_NOTICE, LOC +
            QString("Verbose mask changed, new mask is: %1").arg(verboseString));

        retlist << "OK";
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Invalid SET_VERBOSE string: '%1'").arg(newverbose));
        retlist << "Failed";
    }

    SendResponse(pbssock, retlist);
}

void MainServer::HandleSetLogLevel(QStringList &slist, PlaybackSock *pbs)
{
    MythSocket *pbssock = pbs->getSocket();
    QStringList retlist;
    QString newstring = slist[1];
    LogLevel_t newlevel = LOG_UNKNOWN;

    int len = newstring.length();
    if (len > 14)
    {
        newlevel = logLevelGet(newstring.right(len-14));
        if (newlevel != LOG_UNKNOWN)
        {
            logLevel = newlevel;
            logPropagateCalc();
            LOG(VB_GENERAL, LOG_NOTICE, LOC +
                QString("Log level changed, new level is: %1")
                    .arg(logLevelGetName(logLevel)));

            retlist << "OK";
        }
    }

    if (newlevel == LOG_UNKNOWN)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Invalid SET_VERBOSE string: '%1'").arg(newstring));
        retlist << "Failed";
    }

    SendResponse(pbssock, retlist);
}

void MainServer::HandleIsRecording(QStringList &slist, PlaybackSock *pbs)
{
    (void)slist;

    MythSocket *pbssock = pbs->getSocket();
    int RecordingsInProgress = 0;
    int LiveTVRecordingsInProgress = 0;
    QStringList retlist;

    QMap<int, EncoderLink *>::Iterator iter = encoderList->begin();
    for (; iter != encoderList->end(); ++iter)
    {
        EncoderLink *elink = *iter;
        if (elink->IsBusyRecording()) {
            RecordingsInProgress++;

            ProgramInfo *info = elink->GetRecording();
            if (info && info->GetRecordingGroup() == "LiveTV")
                LiveTVRecordingsInProgress++;

            delete info;
        }
    }

    retlist << QString::number(RecordingsInProgress);
    retlist << QString::number(LiveTVRecordingsInProgress);

    SendResponse(pbssock, retlist);
}

void MainServer::HandleGenPreviewPixmap(QStringList &slist, PlaybackSock *pbs)
{
    MythSocket *pbssock = pbs->getSocket();

    if (slist.size() < 3)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Too few params in pixmap request");
        QStringList outputlist("ERROR");
        outputlist += "TOO_FEW_PARAMS";
        SendResponse(pbssock, outputlist);
        return;
    }

    bool      time_fmt_sec   = true;
    long long time           = -1;
    QString   outputfile;
    int       width          = -1;
    int       height         = -1;
    bool      has_extra_data = false;

    QString token = slist[1];
    if (token.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Failed to parse pixmap request. Token absent");
        QStringList outputlist("ERROR");
        outputlist += "TOKEN_ABSENT";
        SendResponse(pbssock, outputlist);
        return;
    }

    QStringList::const_iterator it = slist.begin() + 2;
    QStringList::const_iterator end = slist.end();
    ProgramInfo pginfo(it, end);
    bool ok = pginfo.HasPathname();
    if (!ok)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to parse pixmap request. "
                "ProgramInfo missing pathname");
        QStringList outputlist("BAD");
        outputlist += "NO_PATHNAME";
        SendResponse(pbssock, outputlist);
        return;
    }
    if (token.toLower() == "do_not_care")
    {
        token = QString("%1:%2")
            .arg(pginfo.MakeUniqueKey()).arg(random());
    }
    if (it != slist.end())
        (time_fmt_sec = ((*it).toLower() == "s")), ++it;
    if (it != slist.end())
        (time = (*it).toLongLong()), ++it;
    if (it != slist.end())
        (outputfile = *it), ++it;
    outputfile = (outputfile == "<EMPTY>") ? QString::null : outputfile;
    if (it != slist.end())
    {
        width = (*it).toInt(&ok); ++it;
        width = (ok) ? width : -1;
    }
    if (it != slist.end())
    {
        height = (*it).toInt(&ok); ++it;
        height = (ok) ? height : -1;
        has_extra_data = true;
    }
    QSize outputsize = QSize(width, height);

    if (has_extra_data)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("HandleGenPreviewPixmap got extra data\n\t\t\t"
                    "%1%2 %3x%4 '%5'")
                .arg(time).arg(time_fmt_sec?"s":"f")
                .arg(width).arg(height).arg(outputfile));
    }

    pginfo.SetPathname(GetPlaybackURL(&pginfo));

    m_previewRequestedBy[token] = pbs->getHostname();

    if ((ismaster) &&
        (pginfo.GetHostname() != gCoreContext->GetHostName()) &&
        (!masterBackendOverride || !pginfo.IsLocal()))
    {
        PlaybackSock *slave = GetSlaveByHostname(pginfo.GetHostname());

        if (slave)
        {
            QStringList outputlist;
            if (has_extra_data)
            {
                outputlist = slave->GenPreviewPixmap(
                    token, &pginfo, time_fmt_sec, time, outputfile, outputsize);
            }
            else
            {
                outputlist = slave->GenPreviewPixmap(token, &pginfo);
            }

            slave->DecrRef();

            if (outputlist.empty() || outputlist[0] != "OK")
                m_previewRequestedBy.remove(token);

            SendResponse(pbssock, outputlist);
            return;
        }
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("HandleGenPreviewPixmap() "
                    "Couldn't find backend for:\n\t\t\t%1")
                .arg(pginfo.toString(ProgramInfo::kTitleSubtitle)));
    }

    if (!pginfo.IsLocal())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "HandleGenPreviewPixmap: Unable to "
                "find file locally, unable to make preview image.");
        QStringList outputlist( "ERROR" );
        outputlist += "FILE_INACCESSIBLE";
        SendResponse(pbssock, outputlist);
        m_previewRequestedBy.remove(token);
        return;
    }

    if (has_extra_data)
    {
        PreviewGeneratorQueue::GetPreviewImage(
            pginfo, outputsize, outputfile, time, time_fmt_sec, token);
    }
    else
    {
        PreviewGeneratorQueue::GetPreviewImage(pginfo, token);
    }

    QStringList outputlist("OK");
    if (!outputfile.isEmpty())
        outputlist += outputfile;
    SendResponse(pbssock, outputlist);
}

void MainServer::HandlePixmapLastModified(QStringList &slist, PlaybackSock *pbs)
{
    MythSocket *pbssock = pbs->getSocket();

    QStringList::const_iterator it = slist.begin() + 1;
    ProgramInfo pginfo(it, slist.end());

    pginfo.SetPathname(GetPlaybackURL(&pginfo));

    QStringList strlist;

    if (ismaster &&
        (pginfo.GetHostname() != gCoreContext->GetHostName()) &&
        (!masterBackendOverride || !pginfo.IsLocal()))
    {
        PlaybackSock *slave = GetSlaveByHostname(pginfo.GetHostname());

        if (slave)
        {
             QDateTime slavetime = slave->PixmapLastModified(&pginfo);
             slave->DecrRef();

             strlist = (slavetime.isValid()) ?
                 QStringList(QString::number(slavetime.toTime_t())) :
                 QStringList("BAD");

             SendResponse(pbssock, strlist);
             return;
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("HandlePixmapLastModified() "
                        "Couldn't find backend for:\n\t\t\t%1")
                    .arg(pginfo.toString(ProgramInfo::kTitleSubtitle)));
        }
    }

    if (!pginfo.IsLocal())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "MainServer: HandlePixmapLastModified: Unable to "
            "find file locally, unable to get last modified date.");
        QStringList outputlist( "BAD" );
        SendResponse(pbssock, outputlist);
        return;
    }

    QString filename = pginfo.GetPathname() + ".png";

    QFileInfo finfo(filename);

    if (finfo.exists())
    {
        QDateTime lastmodified = finfo.lastModified();
        strlist = QStringList(QString::number(lastmodified.toTime_t()));
    }
    else
        strlist = QStringList( "BAD" );

    SendResponse(pbssock, strlist);
}

void MainServer::HandlePixmapGetIfModified(
    const QStringList &slist, PlaybackSock *pbs)
{
    QStringList strlist;

    MythSocket *pbssock = pbs->getSocket();
    if (slist.size() < (3 + NUMPROGRAMLINES))
    {
        strlist = QStringList("ERROR");
        strlist += "1: Parameter list too short";
        SendResponse(pbssock, strlist);
        return;
    }

    QDateTime cachemodified;
    if (slist[1].toInt() != -1)
        cachemodified = MythDate::fromTime_t(slist[1].toInt());

    int max_file_size = slist[2].toInt();

    QStringList::const_iterator it = slist.begin() + 3;
    ProgramInfo pginfo(it, slist.end());

    if (!pginfo.HasPathname())
    {
        strlist = QStringList("ERROR");
        strlist += "2: Invalid ProgramInfo";
        SendResponse(pbssock, strlist);
        return;
    }

    pginfo.SetPathname(GetPlaybackURL(&pginfo) + ".png");
    if (pginfo.IsLocal())
    {
        QFileInfo finfo(pginfo.GetPathname());
        if (finfo.exists())
        {
            size_t fsize = finfo.size();
            QDateTime lastmodified = finfo.lastModified();
            bool out_of_date = !cachemodified.isValid() ||
                (lastmodified > cachemodified);

            if (out_of_date && (fsize > 0) && ((ssize_t)fsize < max_file_size))
            {
                QByteArray data;
                QFile file(pginfo.GetPathname());
                bool open_ok = file.open(QIODevice::ReadOnly);
                if (open_ok)
                    data = file.readAll();

                if (data.size())
                {
                    LOG(VB_FILE, LOG_INFO, LOC +
                        QString("Read preview file '%1'")
                            .arg(pginfo.GetPathname()));
                    strlist += QString::number(lastmodified.toTime_t());
                    strlist += QString::number(data.size());
                    strlist += QString::number(qChecksum(data.constData(),
                                               data.size()));
                    strlist += QString(data.toBase64());
                }
                else
                {
                    LOG(VB_GENERAL, LOG_ERR, LOC +
                        QString("Failed to read preview file '%1'")
                            .arg(pginfo.GetPathname()));

                    strlist = QStringList("ERROR");
                    strlist +=
                        QString("3: Failed to read preview file '%1'%2")
                        .arg(pginfo.GetPathname())
                        .arg((open_ok) ? "" : " open failed");
                }
            }
            else if (out_of_date && (max_file_size > 0))
            {
                if (fsize >= (size_t) max_file_size)
                {
                    strlist = QStringList("WARNING");
                    strlist += QString("1: Preview file too big %1 > %2")
                        .arg(fsize).arg(max_file_size);
                }
                else
                {
                    strlist = QStringList("ERROR");
                    strlist += "4: Preview file is invalid";
                }
            }
            else
            {
                strlist += QString::number(lastmodified.toTime_t());
            }

            SendResponse(pbssock, strlist);
            return;
        }
    }

    // handle remote ...
    if (ismaster && pginfo.GetHostname() != gCoreContext->GetHostName())
    {
        PlaybackSock *slave = GetSlaveByHostname(pginfo.GetHostname());
        if (!slave)
        {
            strlist = QStringList("ERROR");
            strlist +=
                "5: Could not locate mythbackend that made this recording";
            SendResponse(pbssock, strlist);
            return;
        }

        strlist = slave->ForwardRequest(slist);

        slave->DecrRef();

        if (!strlist.empty())
        {
            SendResponse(pbssock, strlist);
            return;
        }
    }

    strlist = QStringList("WARNING");
    strlist += "2: Could not locate requested file";
    SendResponse(pbssock, strlist);
}

void MainServer::HandleBackendRefresh(MythSocket *socket)
{
    QStringList retlist( "OK" );
    SendResponse(socket, retlist);
}

void MainServer::HandleBlockShutdown(bool blockShutdown, PlaybackSock *pbs)
{
    pbs->setBlockShutdown(blockShutdown);

    MythSocket *socket = pbs->getSocket();
    QStringList retlist( "OK" );
    SendResponse(socket, retlist);
}

void MainServer::deferredDeleteSlot(void)
{
    QMutexLocker lock(&deferredDeleteLock);

    if (deferredDeleteList.empty())
        return;

    DeferredDeleteStruct dds = deferredDeleteList.front();
    while (dds.ts.secsTo(MythDate::current()) > 30)
    {
        dds.sock->DecrRef();
        deferredDeleteList.pop_front();
        if (deferredDeleteList.empty())
            return;
        dds = deferredDeleteList.front();
    }
}

void MainServer::DeletePBS(PlaybackSock *sock)
{
    DeferredDeleteStruct dds;
    dds.sock = sock;
    dds.ts = MythDate::current();

    QMutexLocker lock(&deferredDeleteLock);
    deferredDeleteList.push_back(dds);
}

#undef QT_NO_DEBUG

void MainServer::connectionClosed(MythSocket *socket)
{
    // we're in the middle of stopping, prevent deadlock
    if (m_stopped)
        return;

    sockListLock.lockForWrite();

    // make sure these are not actually deleted in the callback
    socket->IncrRef();
    decrRefSocketList.push_back(socket);

    vector<PlaybackSock *>::iterator it = playbackList.begin();
    for (; it != playbackList.end(); ++it)
    {
        PlaybackSock *pbs = (*it);
        MythSocket *sock = pbs->getSocket();
        if (sock == socket && pbs == masterServer)
        {
            playbackList.erase(it);
            sockListLock.unlock();
            masterServer->DecrRef();
            masterServer = NULL;
            MythEvent me("LOCAL_RECONNECT_TO_MASTER");
            gCoreContext->dispatch(me);
            return;
        }
        else if (sock == socket)
        {
            QList<uint> disconnectedSlaves;
            bool needsReschedule = false;

            if (ismaster && pbs->isSlaveBackend())
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("Slave backend: %1 no longer connected")
                        .arg(pbs->getHostname()));

                bool isFallingAsleep = true;
                QMap<int, EncoderLink *>::Iterator iter = encoderList->begin();
                for (; iter != encoderList->end(); ++iter)
                {
                    EncoderLink *elink = *iter;
                    if (elink->GetSocket() == pbs)
                    {
                        if (!elink->IsFallingAsleep())
                            isFallingAsleep = false;

                        elink->SetSocket(NULL);
                        if (m_sched)
                            disconnectedSlaves.push_back(elink->GetInputID());
                    }
                }
                if (m_sched && !isFallingAsleep)
                    needsReschedule = true;

                QString message = QString("LOCAL_SLAVE_BACKEND_OFFLINE %1")
                                          .arg(pbs->getHostname());
                MythEvent me(message);
                gCoreContext->dispatch(me);

                MythEvent me2("RECORDING_LIST_CHANGE");
                gCoreContext->dispatch(me2);

                gCoreContext->SendSystemEvent(
                    QString("SLAVE_DISCONNECTED HOSTNAME %1")
                            .arg(pbs->getHostname()));
            }
            else if (ismaster && pbs->IsFrontend())
            {
                if (gBackendContext)
                    gBackendContext->SetFrontendDisconnected(pbs->getHostname());
            }

            LiveTVChain *chain;
            if ((chain = GetExistingChain(sock)))
            {
                chain->DelHostSocket(sock);
                if (chain->HostSocketCount() == 0)
                {
                    QMap<int, EncoderLink *>::iterator it =
                        encoderList->begin();
                    for (; it != encoderList->end(); ++it)
                    {
                        EncoderLink *enc = *it;
                        if (enc->IsLocal())
                        {
                            while (enc->GetState() == kState_ChangingState)
                                usleep(500);

                            if (enc->IsBusy() &&
                                enc->GetChainID() == chain->GetID())
                            {
                                enc->StopLiveTV();
                            }
                        }
                    }
                    DeleteChain(chain);
                }
            }

            LOG(VB_GENERAL, LOG_INFO, QString("%1 sock(%2) '%3' disconnected")
                .arg(pbs->getBlockShutdown() ? "Playback" : "Monitor")
                .arg(quintptr(socket),0,16)
                .arg(pbs->getHostname()) );
            pbs->SetDisconnected();
            playbackList.erase(it);

            PlaybackSock *testsock = GetPlaybackBySock(socket);
            if (testsock)
                LOG(VB_GENERAL, LOG_ERR, LOC + "Playback sock still exists?");

            pbs->DecrRef();

            sockListLock.unlock();

            // Since we may already be holding the scheduler lock
            // delay handling the disconnect until a little later. #9885
            if (!disconnectedSlaves.isEmpty())
                SendSlaveDisconnectedEvent(disconnectedSlaves, needsReschedule);

            return;
        }
    }

    vector<FileTransfer *>::iterator ft = fileTransferList.begin();
    for (; ft != fileTransferList.end(); ++ft)
    {
        MythSocket *sock = (*ft)->getSocket();
        if (sock == socket)
        {
            LOG(VB_GENERAL, LOG_INFO, QString("FileTransfer sock(%1) disconnected")
                .arg(quintptr(socket),0,16) );
            (*ft)->DecrRef();
            fileTransferList.erase(ft);
            sockListLock.unlock();
            return;
        }
    }

    QSet<MythSocket*>::iterator cs = controlSocketList.find(socket);
    if (cs != controlSocketList.end())
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Control sock(%1) disconnected")
            .arg(quintptr(socket),0,16) );
        (*cs)->DecrRef();
        controlSocketList.erase(cs);
        sockListLock.unlock();
        return;
    }

    sockListLock.unlock();

    LOG(VB_GENERAL, LOG_WARNING, LOC +
        QString("Unknown socket closing MythSocket(0x%1)")
            .arg((intptr_t)socket,0,16));
}

PlaybackSock *MainServer::GetSlaveByHostname(const QString &hostname)
{
    if (!ismaster)
        return NULL;

    sockListLock.lockForRead();

    vector<PlaybackSock *>::iterator iter = playbackList.begin();
    for (; iter != playbackList.end(); ++iter)
    {
        PlaybackSock *pbs = *iter;
        if (pbs->isSlaveBackend() &&
            gCoreContext->IsThisHost(hostname, pbs->getHostname()))
        {
            sockListLock.unlock();
            pbs->IncrRef();
            return pbs;
        }
    }

    sockListLock.unlock();

    return NULL;
}

PlaybackSock *MainServer::GetMediaServerByHostname(const QString &hostname)
{
    if (!ismaster)
        return NULL;

    QReadLocker rlock(&sockListLock);

    vector<PlaybackSock *>::iterator iter = playbackList.begin();
    for (; iter != playbackList.end(); ++iter)
    {
        PlaybackSock *pbs = *iter;
        if (pbs->isMediaServer() &&
            gCoreContext->IsThisHost(hostname, pbs->getHostname()))
        {
            pbs->IncrRef();
            return pbs;
        }
    }

    return NULL;
}

/// Warning you must hold a sockListLock lock before calling this
PlaybackSock *MainServer::GetPlaybackBySock(MythSocket *sock)
{
    PlaybackSock *retval = NULL;

    vector<PlaybackSock *>::iterator it = playbackList.begin();
    for (; it != playbackList.end(); ++it)
    {
        if (sock == (*it)->getSocket())
        {
            retval = (*it);
            break;
        }
    }

    return retval;
}

/// Warning you must hold a sockListLock lock before calling this
FileTransfer *MainServer::GetFileTransferByID(int id)
{
    FileTransfer *retval = NULL;

    vector<FileTransfer *>::iterator it = fileTransferList.begin();
    for (; it != fileTransferList.end(); ++it)
    {
        if (id == (*it)->getSocket()->GetSocketDescriptor())
        {
            retval = (*it);
            break;
        }
    }

    return retval;
}

/// Warning you must hold a sockListLock lock before calling this
FileTransfer *MainServer::GetFileTransferBySock(MythSocket *sock)
{
    FileTransfer *retval = NULL;

    vector<FileTransfer *>::iterator it = fileTransferList.begin();
    for (; it != fileTransferList.end(); ++it)
    {
        if (sock == (*it)->getSocket())
        {
            retval = (*it);
            break;
        }
    }

    return retval;
}

LiveTVChain *MainServer::GetExistingChain(const QString &id)
{
    QMutexLocker lock(&liveTVChainsLock);

    vector<LiveTVChain*>::iterator it = liveTVChains.begin();
    for (; it != liveTVChains.end(); ++it)
    {
        if ((*it)->GetID() == id)
            return *it;
    }

    return NULL;
}

LiveTVChain *MainServer::GetExistingChain(const MythSocket *sock)
{
    QMutexLocker lock(&liveTVChainsLock);

    vector<LiveTVChain*>::iterator it = liveTVChains.begin();
    for (; it != liveTVChains.end(); ++it)
    {
        if ((*it)->IsHostSocket(sock))
            return *it;
    }

    return NULL;
}

LiveTVChain *MainServer::GetChainWithRecording(const ProgramInfo &pginfo)
{
    QMutexLocker lock(&liveTVChainsLock);

    vector<LiveTVChain*>::iterator it = liveTVChains.begin();
    for (; it != liveTVChains.end(); ++it)
    {
        if ((*it)->ProgramIsAt(pginfo) >= 0)
            return *it;
    }

    return NULL;
}

void MainServer::AddToChains(LiveTVChain *chain)
{
    QMutexLocker lock(&liveTVChainsLock);

    if (chain)
        liveTVChains.push_back(chain);
}

void MainServer::DeleteChain(LiveTVChain *chain)
{
    QMutexLocker lock(&liveTVChainsLock);

    if (!chain)
        return;

    vector<LiveTVChain*> newChains;

    vector<LiveTVChain*>::iterator it = liveTVChains.begin();
    for (; it != liveTVChains.end(); ++it)
    {
        if (*it != chain)
            newChains.push_back(*it);
    }
    liveTVChains = newChains;

    chain->DecrRef();
}

void MainServer::SetExitCode(int exitCode, bool closeApplication)
{
    m_exitCode = exitCode;
    if (closeApplication)
        QCoreApplication::exit(m_exitCode);
}

QString MainServer::LocalFilePath(const QUrl &url, const QString &wantgroup)
{
    QString lpath = url.path();

    if (url.hasFragment())
        lpath += "#" + url.fragment();

    if (lpath.section('/', -2, -2) == "channels")
    {
        // This must be an icon request. Check channel.icon to be safe.
        QString querytext;

        QString file = lpath.section('/', -1);
        lpath = "";

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT icon FROM channel WHERE icon LIKE :FILENAME ;");
        query.bindValue(":FILENAME", QString("%/") + file);

        if (query.exec() && query.next())
        {
            lpath = query.value(0).toString();
        }
        else
        {
            MythDB::DBError("Icon path", query);
        }
    }
    else
    {
        lpath = lpath.section('/', -1);

        QString fpath = lpath;
        if (fpath.endsWith(".png"))
            fpath = fpath.left(fpath.length() - 4);

        ProgramInfo pginfo(fpath);
        if (pginfo.GetChanID())
        {
            QString pburl = GetPlaybackURL(&pginfo);
            if (pburl.startsWith("/"))
            {
                lpath = pburl.section('/', 0, -2) + "/" + lpath;
                LOG(VB_FILE, LOG_INFO, LOC +
                    QString("Local file path: %1").arg(lpath));
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("ERROR: LocalFilePath unable to find local "
                            "path for '%1', found '%2' instead.")
                        .arg(lpath).arg(pburl));
                lpath = "";
            }
        }
        else if (!lpath.isEmpty())
        {
            // For securities sake, make sure filename is really the pathless.
            QString opath = lpath;
            StorageGroup sgroup;

            if (!wantgroup.isEmpty())
            {
                sgroup.Init(wantgroup);
                lpath = url.toString();
            }
            else
            {
                lpath = QFileInfo(lpath).fileName();
            }

            QString tmpFile = sgroup.FindFile(lpath);
            if (!tmpFile.isEmpty())
            {
                lpath = tmpFile;
                LOG(VB_FILE, LOG_INFO, LOC +
                    QString("LocalFilePath(%1 '%2'), found file through "
                            "exhaustive search at '%3'")
                        .arg(url.toString()).arg(opath).arg(lpath));
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + QString("ERROR: LocalFilePath "
                    "unable to find local path for '%1'.") .arg(url.toString()));
                lpath = "";
            }

        }
        else
        {
            lpath = "";
        }
    }

    return lpath;
}

void MainServer::reconnectTimeout(void)
{
    MythSocket *masterServerSock = new MythSocket(-1, this);

    QString server = gCoreContext->GetMasterServerIP();
    int port = gCoreContext->GetMasterServerPort();

    LOG(VB_GENERAL, LOG_NOTICE, LOC +
        QString("Connecting to master server: %1:%2")
            .arg(server).arg(port));

    if (!masterServerSock->ConnectToHost(server, port))
    {
        LOG(VB_GENERAL, LOG_NOTICE, LOC +
            "Connection to master server timed out.");
        masterServerReconnect->start(kMasterServerReconnectTimeout);
        masterServerSock->DecrRef();
        return;
    }

    LOG(VB_GENERAL, LOG_NOTICE, LOC + "Connected successfully");

    QString str = QString("ANN SlaveBackend %1 %2")
                          .arg(gCoreContext->GetHostName())
                          .arg(gCoreContext->GetBackendServerIP());

    QStringList strlist( str );

    QMap<int, EncoderLink *>::Iterator iter = encoderList->begin();
    for (; iter != encoderList->end(); ++iter)
    {
        EncoderLink *elink = *iter;
        elink->CancelNextRecording(true);
        ProgramInfo *pinfo = elink->GetRecording();
        if (pinfo)
        {
            pinfo->ToStringList(strlist);
            delete pinfo;
        }
        else
        {
            ProgramInfo dummy;
            dummy.SetInputID(elink->GetInputID());
            dummy.ToStringList(strlist);
        }
    }

    // Calling SendReceiveStringList() with callbacks enabled is asking for
    // trouble, our reply might be swallowed by readyRead
    masterServerSock->SetReadyReadCallbackEnabled(false);
    if (!masterServerSock->SendReceiveStringList(strlist, 1) ||
        (strlist[0] == "ERROR"))
    {
        masterServerSock->DecrRef();
        masterServerSock = NULL;
        if (strlist.empty())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "Failed to open master server socket, timeout");
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "Failed to open master server socket" +
                ((strlist.size() >= 2) ?
                QString(", error was %1").arg(strlist[1]) :
                QString(", remote error")));
        }
        masterServerReconnect->start(kMasterServerReconnectTimeout);
        return;
    }
    masterServerSock->SetReadyReadCallbackEnabled(true);

    masterServer = new PlaybackSock(this, masterServerSock, server,
                                    kPBSEvents_Normal);
    sockListLock.lockForWrite();
    playbackList.push_back(masterServer);
    sockListLock.unlock();

    autoexpireUpdateTimer->start(1000);
}

// returns true, if a client (slavebackends are not counted!)
// is connected by checking the lists.
bool MainServer::isClientConnected(bool onlyBlockingClients)
{
    bool foundClient = false;

    sockListLock.lockForRead();

    foundClient |= !fileTransferList.empty();

    vector<PlaybackSock *>::iterator it = playbackList.begin();
    for (; !foundClient && (it != playbackList.end()); ++it)
    {
        // Ignore slave backends
        if ((*it)->isSlaveBackend())
            continue;

        // If we are only interested in blocking clients then ignore
        // non-blocking ones
        if (onlyBlockingClients && !(*it)->getBlockShutdown())
            continue;

        foundClient = true;
    }

    sockListLock.unlock();

    return (foundClient);
}

/// Sends the Slavebackends the request to shut down using haltcmd
void MainServer::ShutSlaveBackendsDown(QString &haltcmd)
{
// TODO FIXME We should issue a MythEvent and have customEvent
// send this with the proper syncronisation and locking.

    QStringList bcast( "SHUTDOWN_NOW" );
    bcast << haltcmd;

    sockListLock.lockForRead();

    vector<PlaybackSock *>::iterator it = playbackList.begin();
    for (; it != playbackList.end(); ++it)
    {
        if ((*it)->isSlaveBackend())
            (*it)->getSocket()->WriteStringList(bcast);
    }

    sockListLock.unlock();
}

void MainServer::HandleSlaveDisconnectedEvent(const MythEvent &event)
{
    if (event.ExtraDataCount() > 0 && m_sched)
    {
        bool needsReschedule = event.ExtraData(0).toUInt();
        for (int i = 1; i < event.ExtraDataCount(); i++)
            m_sched->SlaveDisconnected(event.ExtraData(i).toUInt());

        if (needsReschedule)
            m_sched->ReschedulePlace("SlaveDisconnected");
    }
}

void MainServer::SendSlaveDisconnectedEvent(
    const QList<uint> &cardids, bool needsReschedule)
{
    QStringList extraData;
    extraData.push_back(
        QString::number(static_cast<uint>(needsReschedule)));

    QList<uint>::const_iterator it;
    for (it = cardids.begin(); it != cardids.end(); ++it)
        extraData.push_back(QString::number(*it));

    MythEvent me("LOCAL_SLAVE_BACKEND_ENCODERS_OFFLINE", extraData);
    gCoreContext->dispatch(me);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
