#ifndef MAINSERVER_H_
#define MAINSERVER_H_

#include <qmap.h>
#include <qsocket.h>
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

class HttpStatus;
class ProcessRequestThread;

class MainServer : public QObject
{
    Q_OBJECT
  public:
    MainServer(bool master, int port, int statusport, 
               QMap<int, EncoderLink *> *tvList,
               Scheduler *sched);

   ~MainServer();

    void customEvent(QCustomEvent *e);

    void FillProgramInfo(QDomDocument *pDoc, QDomElement &e, ProgramInfo *pInfo);
    void FillStatusXML(QDomDocument *pDoc);

    bool isClientConnected();
    void ShutSlaveBackendsDown(QString &haltcmd);

    void ProcessRequest(RefSocket *sock); 
    void MarkUnused(ProcessRequestThread *prt);

  protected slots:
    void reconnectTimeout(void);
    void masterServerDied(void);

  private slots:
    void newConnection(RefSocket *);
    void endConnection(RefSocket *);
    void readSocket();

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

    void ProcessRequestWork(RefSocket *sock);
    void HandleAnnounce(QStringList &slist, QStringList commands, 
                        RefSocket *socket);
    void HandleDone(QSocket *socket);

    void HandleIsActiveBackendQuery(QStringList &slist, PlaybackSock *pbs);
    void HandleQueryRecordings(QString type, PlaybackSock *pbs);
    void HandleStopRecording(QStringList &slist, PlaybackSock *pbs);
    void DoHandleStopRecording(ProgramInfo *pginfo, PlaybackSock *pbs);
    void HandleDeleteRecording(QStringList &slist, PlaybackSock *pbs,
                               bool forceMetadataDelete);
    void DoHandleDeleteRecording(ProgramInfo *pginfo, PlaybackSock *pbs,
                                 bool forceMetadataDelete);
    void HandleForgetRecording(QStringList &slist, PlaybackSock *pbs);
    void HandleReactivateRecording(QStringList &slist, PlaybackSock *pbs);
    void HandleRescheduleRecordings(int recordid, PlaybackSock *pbs);
    void HandleQueryFreeSpace(PlaybackSock *pbs);
    void HandleQueryCheckFile(QStringList &slist, PlaybackSock *pbs);
    void HandleQueryGuideDataThrough(PlaybackSock *pbs);
    void HandleGetPendingRecordings(PlaybackSock *pbs);
    void HandleGetScheduledRecordings(PlaybackSock *pbs);
    void HandleGetConflictingRecordings(QStringList &slist, PlaybackSock *pbs);
    void HandleGetNextFreeRecorder(QStringList &slist, PlaybackSock *pbs);
    void HandleGetFreeRecorder(PlaybackSock *pbs);
    void HandleGetFreeRecorderCount(PlaybackSock *pbs);
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
    void HandleQueueTranscode(QStringList &slist, PlaybackSock *pbs,
                              int state);
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
    void HandleVersion(QSocket *socket, QString version);
    void HandleBackendRefresh(QSocket *socket);
    void HandleQueryLoad(PlaybackSock *pbs);
    void HandleQueryUptime(PlaybackSock *pbs);
    void HandleQueryMemStats(PlaybackSock *pbs);

    void SendResponse(QSocket *pbs, QStringList &commands);

    void getFreeSpace(int &total, int &used);
    void getGuideDataThrough(QDateTime &GuideDataThrough);

    PlaybackSock *getSlaveByHostname(QString &hostname);
    PlaybackSock *getPlaybackBySock(QSocket *socket);
    FileTransfer *getFileTransferByID(int id);
    FileTransfer *getFileTransferBySock(QSocket *socket);

    QString LocalFilePath(QUrl &url);

    bool isRingBufSock(QSocket *sock);

    static void *SpawnDeleteThread(void *param);
    void DoDeleteThread(DeleteStruct *ds);

    QMap<int, EncoderLink *> *encoderList;

    MythServer *mythserver;

    vector<PlaybackSock *> playbackList;
    vector<QSocket *> ringBufList;
    vector<FileTransfer *> fileTransferList;

    QString recordfileprefix;

    HttpStatus *statusserver;

    QTimer *masterServerReconnect;
    PlaybackSock *masterServer;
    
    bool ismaster;

    QMutex deletelock;
    QMutex threadPoolLock;
    vector<ProcessRequestThread *> threadPool;

    bool masterBackendOverride;

    Scheduler *m_sched;

    QMutex readReadyLock;
};

#endif
