#ifndef MAINSERVER_H_
#define MAINSERVER_H_

#include <qmap.h>
#include <qtimer.h>
#include <qurl.h>
#include <qmutex.h>
#include <qdom.h>
#include <vector>
using namespace std;

#include "tv.h"
#include "server.h"
#include "playbacksock.h"
#include "encoderlink.h"
#include "filetransfer.h"
#include "scheduler.h"
#include "livetvchain.h"
#include "autoexpire.h"
#include "mythsocket.h"

class ProcessRequestThread;

class MainServer : public QObject, public MythSocketCBs
{
    Q_OBJECT
  public:
    MainServer(bool master, int port, int statusport, 
               QMap<int, EncoderLink *> *tvList,
               Scheduler *sched, AutoExpire *expirer);

   ~MainServer();

    void customEvent(QCustomEvent *e);

    bool isClientConnected();
    void ShutSlaveBackendsDown(QString &haltcmd);

    void ProcessRequest(MythSocket *sock); 
    void MarkUnused(ProcessRequestThread *prt);

    void readyRead(MythSocket *socket);
    void connectionClosed(MythSocket *socket);
    void connectionFailed(MythSocket *socket) { (void)socket; }
    void connected(MythSocket *socket) { (void)socket; }

    void DeletePBS(PlaybackSock *pbs);

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
        QString chanid;
        QDateTime recstartts;
        QDateTime recendts;
        QString filename;
        QString title;
        bool forceMetadataDelete;
    } DeleteStruct;

    void ProcessRequestWork(MythSocket *sock);
    void HandleAnnounce(QStringList &slist, QStringList commands, 
                        MythSocket *socket);
    void HandleDone(MythSocket *socket);

    void HandleIsActiveBackendQuery(QStringList &slist, PlaybackSock *pbs);
    void HandleQueryRecordings(QString type, PlaybackSock *pbs);
    void HandleStopRecording(QStringList &slist, PlaybackSock *pbs);
    void DoHandleStopRecording(ProgramInfo *pginfo, PlaybackSock *pbs);
    void HandleDeleteRecording(QStringList &slist, PlaybackSock *pbs,
                               bool forceMetadataDelete);
    void DoHandleDeleteRecording(ProgramInfo *pginfo, PlaybackSock *pbs,
                                 bool forceMetadataDelete);
    void HandleForgetRecording(QStringList &slist, PlaybackSock *pbs);
    void HandleRescheduleRecordings(int recordid, PlaybackSock *pbs);
    void HandleQueryFreeSpace(PlaybackSock *pbs, bool allBackends);
    void HandleQueryCheckFile(QStringList &slist, PlaybackSock *pbs);
    void HandleQueryGuideDataThrough(PlaybackSock *pbs);
    void HandleGetPendingRecordings(PlaybackSock *pbs, QString table = "", int recordid=-1);
    void HandleGetScheduledRecordings(PlaybackSock *pbs);
    void HandleGetConflictingRecordings(QStringList &slist, PlaybackSock *pbs);
    void HandleGetExpiringRecordings(PlaybackSock *pbs);
    void HandleGetNextFreeRecorder(QStringList &slist, PlaybackSock *pbs);
    void HandleGetFreeRecorder(PlaybackSock *pbs);
    void HandleGetFreeRecorderCount(PlaybackSock *pbs);
    void HandleGetFreeRecorderList(PlaybackSock *pbs);
    void HandleGetConnectedRecorderList(PlaybackSock *pbs);
    void HandleRecorderQuery(QStringList &slist, QStringList &commands,
                             PlaybackSock *pbs);
    void HandleFileTransferQuery(QStringList &slist, QStringList &commands,
                                 PlaybackSock *pbs); 
    void HandleGetRecorderNum(QStringList &slist, PlaybackSock *pbs);
    void HandleGetRecorderFromNum(QStringList &slist, PlaybackSock *pbs);
    void HandleMessage(QStringList &slist, PlaybackSock *pbs);
    void HandleGenPreviewPixmap(QStringList &slist, PlaybackSock *pbs);
    void HandlePixmapLastModified(QStringList &slist, PlaybackSock *pbs);
    void HandleIsRecording(QStringList &slist, PlaybackSock *pbs);
    void HandleCheckRecordingActive(QStringList &slist, PlaybackSock *pbs);
    void HandleFillProgramInfo(QStringList &slist, PlaybackSock *pbs);
    void HandleSetChannelInfo(QStringList &slist, PlaybackSock *pbs);
    void HandleRemoteEncoder(QStringList &slist, QStringList &commands,
                             PlaybackSock *pbs);
    void HandleLockTuner(PlaybackSock *pbs);
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
    void HandleQueryMemStats(PlaybackSock *pbs);
    void HandleBlockShutdown(bool blockShutdown, PlaybackSock *pbs);
    
    void SendResponse(MythSocket *pbs, QStringList &commands);

    void getGuideDataThrough(QDateTime &GuideDataThrough);

    PlaybackSock *getSlaveByHostname(QString &hostname);
    PlaybackSock *getPlaybackBySock(MythSocket *socket);
    FileTransfer *getFileTransferByID(int id);
    FileTransfer *getFileTransferBySock(MythSocket *socket);

    QString LocalFilePath(QUrl &url);

    static void *SpawnDeleteThread(void *param);
    void DoDeleteThread(const DeleteStruct *ds);
    void DoDeleteInDB(const DeleteStruct *ds, const ProgramInfo *pginfo);

    LiveTVChain *GetExistingChain(QString id);
    LiveTVChain *GetExistingChain(MythSocket *sock);
    LiveTVChain *GetChainWithRecording(ProgramInfo *pginfo);
    void AddToChains(LiveTVChain *chain);
    void DeleteChain(LiveTVChain *chain);

    static int  DeleteFile(const QString &filename, bool followLinks);
    static int  OpenAndUnlink(const QString &filename);
    static bool TruncateAndClose(const AutoExpire *expirer,
                                 int fd, const QString &filename);

    QPtrList<LiveTVChain> liveTVChains;
    QMutex liveTVChainsLock;

    QMap<int, EncoderLink *> *encoderList;

    MythServer *mythserver;

    vector<PlaybackSock *> playbackList;
    vector<FileTransfer *> fileTransferList;

    QString recordfileprefix;

    QTimer *masterServerReconnect;
    PlaybackSock *masterServer;
    
    bool ismaster;

    QMutex deletelock;
    QMutex threadPoolLock;
    vector<ProcessRequestThread *> threadPool;

    bool masterBackendOverride;

    Scheduler *m_sched;
    AutoExpire *m_expirer;

    QMutex readReadyLock;

    struct DeferredDeleteStruct
    {
        PlaybackSock *sock; 
        QDateTime ts; 
    };

    QMutex deferredDeleteLock;
    QTimer *deferredDeleteTimer;
    QValueList<DeferredDeleteStruct> deferredDeleteList;

    QTimer *autoexpireUpdateTimer;

    static QMutex truncate_and_close_lock;
};

#endif
