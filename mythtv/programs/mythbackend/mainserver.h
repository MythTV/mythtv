#ifndef MAINSERVER_H_
#define MAINSERVER_H_

#include <QMap>
#include <QMutex>
#include <QReadWriteLock>
#include <QEvent>

#include <vector>
using namespace std;

#include "tv.h"
#include "playbacksock.h"
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

class ProcessRequestThread;
class QUrl;
class MythServer;
class QTimer;

class MainServer : public QObject, public MythSocketCBs
{
    Q_OBJECT
  public:
    MainServer(bool master, int port,
               QMap<int, EncoderLink *> *tvList,
               Scheduler *sched, AutoExpire *expirer);

   ~MainServer();

    void customEvent(QEvent *e);

    bool isClientConnected();
    void ShutSlaveBackendsDown(QString &haltcmd);

    void ProcessRequest(MythSocket *sock);
    void MarkUnused(ProcessRequestThread *prt);

    void readyRead(MythSocket *socket);
    void connectionClosed(MythSocket *socket);
    void connectionFailed(MythSocket *socket) { (void)socket; }
    void connected(MythSocket *socket) { (void)socket; }

    void DeletePBS(PlaybackSock *pbs);

    size_t GetCurrentMaxBitrate(void);
    void BackendQueryDiskSpace(QStringList &strlist, bool consolidated,
                               bool allHosts);
    void GetFilesystemInfos(vector <FileSystemInfo> &fsInfos);

    int GetExitCode() const { return m_exitCode; }

  protected slots:
    void reconnectTimeout(void);
    void deferredDeleteSlot(void);
    void autoexpireUpdate(void);

  private slots:
    void newConnection(MythSocket *);

  private:
    typedef struct deletestruct
    {
        MainServer *ms;
        uint chanid;
        QDateTime recstartts;
        QDateTime recendts;
        QString filename;
        int fd;
        off_t size;
        QString title;
        bool forceMetadataDelete;
    } DeleteStruct;

    void ProcessRequestWork(MythSocket *sock);
    void HandleAnnounce(QStringList &slist, QStringList commands,
                        MythSocket *socket);
    void HandleDone(MythSocket *socket);

    void HandleIsActiveBackendQuery(QStringList &slist, PlaybackSock *pbs);
    bool HandleDeleteFile(QStringList &slist, PlaybackSock *pbs);
    bool HandleDeleteFile(QString filename, QString storagegroup,
                          PlaybackSock *pbs = NULL);
    void HandleQueryRecordings(QString type, PlaybackSock *pbs);
    void HandleQueryRecording(QStringList &slist, PlaybackSock *pbs);
    void HandleStopRecording(QStringList &slist, PlaybackSock *pbs);
    void DoHandleStopRecording(RecordingInfo &recinfo, PlaybackSock *pbs);
    void HandleDeleteRecording(QString &chanid, QString &starttime,
                               PlaybackSock *pbs, bool forceMetadataDelete);
    void HandleDeleteRecording(QStringList &slist, PlaybackSock *pbs,
                               bool forceMetadataDelete);
    void DoHandleDeleteRecording(RecordingInfo &recinfo, PlaybackSock *pbs,
                                 bool forceMetadataDelete, bool expirer=false);
    void HandleUndeleteRecording(QStringList &slist, PlaybackSock *pbs);
    void DoHandleUndeleteRecording(RecordingInfo &recinfo, PlaybackSock *pbs);
    void HandleForgetRecording(QStringList &slist, PlaybackSock *pbs);
    void HandleRescheduleRecordings(int recordid, PlaybackSock *pbs);
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
    void HandleVersion(MythSocket *socket, QString version);
    void HandleBackendRefresh(MythSocket *socket);
    void HandleQueryLoad(PlaybackSock *pbs);
    void HandleQueryUptime(PlaybackSock *pbs);
    void HandleQueryHostname(PlaybackSock *pbs);
    void HandleQueryMemStats(PlaybackSock *pbs);
    void HandleQueryTimeZone(PlaybackSock *pbs);
    void HandleBlockShutdown(bool blockShutdown, PlaybackSock *pbs);
    void HandleDownloadFile(const QStringList &command, PlaybackSock *pbs);

    void SendResponse(MythSocket *pbs, QStringList &commands);

    void getGuideDataThrough(QDateTime &GuideDataThrough);

    PlaybackSock *GetSlaveByHostname(const QString &hostname);
    PlaybackSock *GetPlaybackBySock(MythSocket *socket);
    FileTransfer *GetFileTransferByID(int id);
    FileTransfer *GetFileTransferBySock(MythSocket *socket);

    QString LocalFilePath(const QUrl &url, const QString &wantgroup);

    int GetfsID(vector<FileSystemInfo>::iterator fsInfo);

    static void *SpawnTruncateThread(void *param);
    void DoTruncateThread(const DeleteStruct *ds);
    static void *SpawnDeleteThread(void *param);
    void DoDeleteThread(const DeleteStruct *ds);
    void DeleteRecordedFiles(const DeleteStruct *ds);
    void DoDeleteInDB(const DeleteStruct *ds);

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

    QReadWriteLock sockListLock;
    vector<PlaybackSock *> playbackList;
    vector<FileTransfer *> fileTransferList;

    QTimer *masterServerReconnect; // audited ref #5318
    PlaybackSock *masterServer;

    bool ismaster;

    QMutex deletelock;
    QMutex threadPoolLock;
    QWaitCondition threadPoolCond;
    MythDeque<ProcessRequestThread *> threadPool;

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

    static const uint kMasterServerReconnectTimeout;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
