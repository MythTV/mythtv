#ifndef MAINSERVER_H_
#define MAINSERVER_H_

#include <QReadWriteLock>
#include <QStringList>
#include <QRunnable>
#include <QEvent>
#include <QMutex>
#include <QHash>
#include <QMap>

#include <vector>
using namespace std;

#include "tv.h"
#include "playbacksock.h"
#include "mthreadpool.h"
#include "encoderlink.h"
#include "filetransfer.h"
#include "scheduler.h"
#include "livetvchain.h"
#include "autoexpire.h"
#include "mythsocket.h"
#include "mythdeque.h"
#include "mythdownloadmanager.h"

#ifdef DeleteFile
#undef DeleteFile
#endif

class QUrl;
class MythServer;
class QTimer;
class FileSystemInfo;
class MetadataFactory;
class FreeSpaceUpdater;

class DeleteStruct 
{
    friend class MainServer;
  public:
    DeleteStruct(MainServer *ms, QString filename, QString title,
                 uint chanid, QDateTime recstartts, QDateTime recendts, 
                 bool forceMetadataDelete) : 
        m_ms(ms), m_filename(filename), m_title(title), 
        m_chanid(chanid), m_recstartts(recstartts), 
        m_recendts(recendts),
        m_forceMetadataDelete(forceMetadataDelete), m_fd(-1), m_size(0)
    {
    }

    DeleteStruct(MainServer *ms, QString filename, int fd, off_t size) : 
        m_ms(ms), m_filename(filename), m_chanid(0),
        m_forceMetadataDelete(false), m_fd(fd), m_size(size)
    {
    }

  protected:
    MainServer *m_ms;
    QString     m_filename;
    QString     m_title;
    uint        m_chanid;
    QDateTime   m_recstartts;
    QDateTime   m_recendts;
    bool        m_forceMetadataDelete;
    int         m_fd;
    off_t       m_size;
};

class DeleteThread : public QRunnable, public DeleteStruct
{
  public:
    DeleteThread(MainServer *ms, QString filename, QString title, uint chanid,
                 QDateTime recstartts, QDateTime recendts, 
                 bool forceMetadataDelete) :
                     DeleteStruct(ms, filename, title, chanid, recstartts,
                                  recendts, forceMetadataDelete)  {}
    void start(void)
        { MThreadPool::globalInstance()->startReserved(this, "DeleteThread"); }
    void run(void);
};

class TruncateThread : public QRunnable, public DeleteStruct
{
  public:
    TruncateThread(MainServer *ms, QString filename, int fd, off_t size) :
                DeleteStruct(ms, filename, fd, size)  {}
    void start(void)
        { MThreadPool::globalInstance()->start(this, "Truncate"); }
    void run(void);
};

class MainServer : public QObject, public MythSocketCBs
{
    Q_OBJECT

    friend class DeleteThread;
    friend class TruncateThread;
    friend class FreeSpaceUpdater;
  public:
    MainServer(bool master, int port,
               QMap<int, EncoderLink *> *tvList,
               Scheduler *sched, AutoExpire *expirer);

    ~MainServer();

    void Stop(void);

    void customEvent(QEvent *e);

    bool isClientConnected();
    void ShutSlaveBackendsDown(QString &haltcmd);

    void ProcessRequest(MythSocket *sock);

    void readyRead(MythSocket *socket);
    void connectionClosed(MythSocket *socket);
    void connectionFailed(MythSocket *socket) { (void)socket; }
    void connected(MythSocket *socket) { (void)socket; }

    void DeletePBS(PlaybackSock *pbs);

    size_t GetCurrentMaxBitrate(void);
    void BackendQueryDiskSpace(QStringList &strlist, bool consolidated,
                               bool allHosts);
    void GetFilesystemInfos(QList<FileSystemInfo> &fsInfos);

    int GetExitCode() const { return m_exitCode; }

  protected slots:
    void reconnectTimeout(void);
    void deferredDeleteSlot(void);
    void autoexpireUpdate(void);

  private slots:
    void NewConnection(int socketDescriptor);

  private:

    void ProcessRequestWork(MythSocket *sock);
    void HandleAnnounce(QStringList &slist, QStringList commands,
                        MythSocket *socket);
    void HandleDone(MythSocket *socket);

    void GetActiveBackends(QStringList &hosts);
    void HandleActiveBackendsQuery(PlaybackSock *pbs);
    void HandleIsActiveBackendQuery(QStringList &slist, PlaybackSock *pbs);
    bool HandleDeleteFile(QStringList &slist, PlaybackSock *pbs);
    bool HandleDeleteFile(QString filename, QString storagegroup,
                          PlaybackSock *pbs = NULL);
    void HandleQueryRecordings(QString type, PlaybackSock *pbs);
    void HandleQueryRecording(QStringList &slist, PlaybackSock *pbs);
    void HandleStopRecording(QStringList &slist, PlaybackSock *pbs);
    void DoHandleStopRecording(RecordingInfo &recinfo, PlaybackSock *pbs);
    void HandleDeleteRecording(QString &chanid, QString &starttime,
                               PlaybackSock *pbs, bool forceMetadataDelete,
                               bool forgetHistory);
    void HandleDeleteRecording(QStringList &slist, PlaybackSock *pbs,
                               bool forceMetadataDelete);
    void DoHandleDeleteRecording(RecordingInfo &recinfo, PlaybackSock *pbs,
                                 bool forceMetadataDelete, bool expirer=false,
                                 bool forgetHistory=false);
    void HandleUndeleteRecording(QStringList &slist, PlaybackSock *pbs);
    void DoHandleUndeleteRecording(RecordingInfo &recinfo, PlaybackSock *pbs);
    void HandleForgetRecording(QStringList &slist, PlaybackSock *pbs);
    void HandleRescheduleRecordings(const QStringList &request, 
                                    PlaybackSock *pbs);
    void HandleGoToSleep(PlaybackSock *pbs);
    void HandleQueryFreeSpace(PlaybackSock *pbs, bool allBackends);
    void HandleQueryFreeSpaceSummary(PlaybackSock *pbs);
    void HandleQueryCheckFile(QStringList &slist, PlaybackSock *pbs);
    void HandleQueryFileExists(QStringList &slist, PlaybackSock *pbs);
    void HandleQueryFileHash(QStringList &slist, PlaybackSock *pbs);
    void HandleQueryGuideDataThrough(PlaybackSock *pbs);
    void HandleGetPendingRecordings(PlaybackSock *pbs, QString table = "", int recordid=-1);
    void HandleGetScheduledRecordings(PlaybackSock *pbs);
    void HandleGetConflictingRecordings(QStringList &slist, PlaybackSock *pbs);
    void HandleGetExpiringRecordings(PlaybackSock *pbs);
    void HandleSGGetFileList(QStringList &sList, PlaybackSock *pbs);
    void HandleSGFileQuery(QStringList &sList, PlaybackSock *pbs);
    void HandleGetNextFreeRecorder(QStringList &slist, PlaybackSock *pbs);
    void HandleGetFreeRecorder(PlaybackSock *pbs);
    void HandleGetFreeRecorderCount(PlaybackSock *pbs);
    void HandleGetFreeRecorderList(PlaybackSock *pbs);
    void HandleGetConnectedRecorderList(PlaybackSock *pbs);
    void HandleRecorderQuery(QStringList &slist, QStringList &commands,
                             PlaybackSock *pbs);
    void HandleSetNextLiveTVDir(QStringList &commands, PlaybackSock *pbs);
    void HandleFileTransferQuery(QStringList &slist, QStringList &commands,
                                 PlaybackSock *pbs);
    void HandleGetRecorderNum(QStringList &slist, PlaybackSock *pbs);
    void HandleGetRecorderFromNum(QStringList &slist, PlaybackSock *pbs);
    void HandleMessage(QStringList &slist, PlaybackSock *pbs);
    void HandleSetVerbose(QStringList &slist, PlaybackSock *pbs);
    void HandleSetLogLevel(QStringList &slist, PlaybackSock *pbs);
    void HandleGenPreviewPixmap(QStringList &slist, PlaybackSock *pbs);
    void HandlePixmapLastModified(QStringList &slist, PlaybackSock *pbs);
    void HandlePixmapGetIfModified(const QStringList &slist, PlaybackSock *pbs);
    void HandleIsRecording(QStringList &slist, PlaybackSock *pbs);
    void HandleCheckRecordingActive(QStringList &slist, PlaybackSock *pbs);
    void HandleFillProgramInfo(QStringList &slist, PlaybackSock *pbs);
    void HandleSetChannelInfo(QStringList &slist, PlaybackSock *pbs);
    void HandleRemoteEncoder(QStringList &slist, QStringList &commands,
                             PlaybackSock *pbs);
    void HandleLockTuner(PlaybackSock *pbs, int cardid = -1);
    void HandleFreeTuner(int cardid, PlaybackSock *pbs);
    void HandleCutMapQuery(const QString &chanid, const QString &starttime,
                           PlaybackSock *pbs, bool commbreak);
    void HandleCommBreakQuery(const QString &chanid, const QString &starttime,
                              PlaybackSock *pbs);
    void HandleCutlistQuery(const QString &chanid, const QString &starttime,
                            PlaybackSock *pbs);
    void HandleBookmarkQuery(const QString &chanid, const QString &starttime,
                             PlaybackSock *pbs);
    void HandleSetBookmark(QStringList &tokens, PlaybackSock *pbs);
    void HandleSettingQuery(QStringList &tokens, PlaybackSock *pbs);
    void HandleSetSetting(QStringList &tokens, PlaybackSock *pbs);
    void HandleScanVideos(PlaybackSock *pbs);
    void HandleVersion(MythSocket *socket, const QStringList &slist);
    void HandleBackendRefresh(MythSocket *socket);
    void HandleQueryLoad(PlaybackSock *pbs);
    void HandleQueryUptime(PlaybackSock *pbs);
    void HandleQueryHostname(PlaybackSock *pbs);
    void HandleQueryMemStats(PlaybackSock *pbs);
    void HandleQueryTimeZone(PlaybackSock *pbs);
    void HandleBlockShutdown(bool blockShutdown, PlaybackSock *pbs);
    void HandleDownloadFile(const QStringList &command, PlaybackSock *pbs);
    void HandleSlaveDisconnectedEvent(const MythEvent &event);

    void SendResponse(MythSocket *pbs, QStringList &commands);
    void SendSlaveDisconnectedEvent(const QList<uint> &offlineEncoderIDs,
                                    bool needsReschedule);

    void getGuideDataThrough(QDateTime &GuideDataThrough);

    PlaybackSock *GetSlaveByHostname(const QString &hostname);
    PlaybackSock *GetMediaServerByHostname(const QString &hostname);
    PlaybackSock *GetPlaybackBySock(MythSocket *socket);
    FileTransfer *GetFileTransferByID(int id);
    FileTransfer *GetFileTransferBySock(MythSocket *socket);

    QString LocalFilePath(const QUrl &url, const QString &wantgroup);

    int GetfsID(QList<FileSystemInfo>::iterator fsInfo);

    void DoTruncateThread(DeleteStruct *ds);
    void DoDeleteThread(DeleteStruct *ds);
    void DeleteRecordedFiles(DeleteStruct *ds);
    void DoDeleteInDB(DeleteStruct *ds);

    LiveTVChain *GetExistingChain(const QString &id);
    LiveTVChain *GetExistingChain(const MythSocket *sock);
    LiveTVChain *GetChainWithRecording(const ProgramInfo &pginfo);
    void AddToChains(LiveTVChain *chain);
    void DeleteChain(LiveTVChain *chain);

    void SetExitCode(int exitCode, bool closeApplication);

    static int  DeleteFile(const QString &filename, bool followLinks,
                           bool deleteBrokenSymlinks = false);
    static int  OpenAndUnlink(const QString &filename);
    static bool TruncateAndClose(ProgramInfo *pginfo,
                                 int fd, const QString &filename,
                                 off_t fsize);

    vector<LiveTVChain*> liveTVChains;
    QMutex liveTVChainsLock;

    QMap<int, EncoderLink *> *encoderList;

    MythServer *mythserver;
    MetadataFactory *metadatafactory;

    QReadWriteLock sockListLock;
    vector<PlaybackSock *> playbackList;
    vector<FileTransfer *> fileTransferList;
    QSet<MythSocket*> controlSocketList;
    vector<MythSocket*> decrRefSocketList;

    QMutex masterFreeSpaceListLock;
    FreeSpaceUpdater * volatile masterFreeSpaceListUpdater;
    QWaitCondition masterFreeSpaceListWait;
    QStringList masterFreeSpaceList;

    QTimer *masterServerReconnect; // audited ref #5318
    PlaybackSock *masterServer;

    bool ismaster;

    QMutex deletelock;
    MThreadPool threadPool;

    bool masterBackendOverride;

    Scheduler *m_sched;
    AutoExpire *m_expirer;

    struct DeferredDeleteStruct
    {
        PlaybackSock *sock;
        QDateTime ts;
    };

    QMutex deferredDeleteLock;
    QTimer *deferredDeleteTimer; // audited ref #5318
    MythDeque<DeferredDeleteStruct> deferredDeleteList;

    QTimer *autoexpireUpdateTimer; // audited ref #5318
    static QMutex truncate_and_close_lock;

    QMap<QString, int> fsIDcache;
    QMutex fsIDcacheLock;

    QMutex                     m_downloadURLsLock;
    QMap<QString, QString>     m_downloadURLs;

    int m_exitCode;

    typedef QHash<QString,QString> RequestedBy;
    RequestedBy                m_previewRequestedBy;

    bool m_stopped;

    static const uint kMasterServerReconnectTimeout;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
