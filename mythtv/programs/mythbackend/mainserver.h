#ifndef MAINSERVER_H_
#define MAINSERVER_H_

#include <qvbox.h>
#include <qmap.h>
#include <qsocket.h>

#include "tv.h"
#include "server.h"
#include "playbacksock.h"
#include "encoderlink.h"
#include "filetransfer.h"

class MythContext;

class MainServer : public QVBox
{
    Q_OBJECT
  public:
    MainServer(MythContext *context, int port, 
               QMap<int, EncoderLink *> *tvList);

   ~MainServer();

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
    void HandleGetPendingRecordings(PlaybackSock *pbs);
    void HandleGetConflictingRecordings(QStringList &slist, QString purge, 
                                        PlaybackSock *pbs);
    void HandleGetFreeRecorder(PlaybackSock *pbs);
    void HandleRecorderQuery(QStringList &slist, QStringList &commands,
                             PlaybackSock *pbs);
    void HandleFileTransferQuery(QStringList &slist, QStringList &commands,
                                 PlaybackSock *pbs); 

    PlaybackSock *getPlaybackBySock(QSocket *socket);
    FileTransfer *getFileTransferByID(int id);
    FileTransfer *getFileTransferBySock(QSocket *socket);

    bool isRingBufSock(QSocket *sock);

    QMap<int, EncoderLink *> *encoderList;

    MythServer *mythserver;

    vector<PlaybackSock *> playbackList;
    vector<QSocket *> ringBufList;
    vector<FileTransfer *> fileTransferList;

    MythContext *m_context;

    QString recordfileprefix;
};

#endif
