#ifndef MAINSERVER_H_
#define MAINSERVER_H_

#include <qmap.h>
#include <qsocket.h>
#include <qtimer.h>
#include <vector>
using namespace std;

#include "tv.h"
#include "server.h"
#include "playbacksock.h"
#include "encoderlink.h"
#include "filetransfer.h"

class HttpStatus;

class MainServer : public QObject
{
    Q_OBJECT
  public:
    MainServer(bool master, int port, int statusport, 
               QMap<int, EncoderLink *> *tvList);

   ~MainServer();

    void customEvent(QCustomEvent *e);
    void PrintStatus(QSocket *socket);

  protected slots:
    void reconnectTimeout(void);
    void masterServerDied(void);

  private slots:
    void newConnection(QSocket *);
    void endConnection(QSocket *);
    void readSocket();

  private:
    void HandleAnnounce(QStringList &slist, QStringList commands, 
                        QSocket *socket);
    void HandleDone(QSocket *socket);

    // playback
    void HandleQueryRecordings(QString type, PlaybackSock *pbs);
    void HandleDeleteRecording(QStringList &slist, PlaybackSock *pbs);
    void HandleQueryFreeSpace(PlaybackSock *pbs);
    void HandleQueryCheckFile(QStringList &slist, PlaybackSock *pbs);
    void HandleGetPendingRecordings(PlaybackSock *pbs);
    void HandleGetConflictingRecordings(QStringList &slist, QString purge, 
                                        PlaybackSock *pbs);
    void HandleGetFreeRecorder(PlaybackSock *pbs);
    void HandleRecorderQuery(QStringList &slist, QStringList &commands,
                             PlaybackSock *pbs);
    void HandleFileTransferQuery(QStringList &slist, QStringList &commands,
                                 PlaybackSock *pbs); 
    void HandleGetRecorderNum(QStringList &slist, PlaybackSock *pbs);
    void HandleMessage(QStringList &slist, PlaybackSock *pbs);
    void HandleGenPreviewPixmap(QStringList &slist, PlaybackSock *pbs);
    void HandleFillProgramInfo(QStringList &slist, PlaybackSock *pbs);
    void HandleRemoteEncoder(QStringList &slist, QStringList &commands,
                             PlaybackSock *pbs);

    PlaybackSock *getSlaveByHostname(QString &hostname);
    PlaybackSock *getPlaybackBySock(QSocket *socket);
    FileTransfer *getFileTransferByID(int id);
    FileTransfer *getFileTransferBySock(QSocket *socket);

    bool isRingBufSock(QSocket *sock);

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
};

#endif
