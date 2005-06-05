#ifndef PLAYBACKSOCK_H_
#define PLAYBACKSOCK_H_

#include <qsocket.h>
#include <qstring.h>
#include <qmutex.h>

class ProgramInfo;
class RefSocket;
class MainServer;

class PlaybackSock
{
  public:
    PlaybackSock(MainServer *parent, RefSocket *lsock, 
                 QString lhostname, bool wantevents);
    virtual ~PlaybackSock();

    void UpRef(void);
    bool DownRef(void);

    void SetDisconnected(void) { disconnected = true; }
    bool IsDisconnected(void) { return disconnected; }

    RefSocket *getSocket(void) { return sock; }
    QString getHostname(void) { return hostname; }

    bool isLocal(void) { return local; }
    bool wantsEvents(void) { return events; }

    // all backend<->backend stuff below here
    bool isSlaveBackend(void) { return backend; }
    void setAsSlaveBackend(void) { backend = true; }

    bool isExpectingReply(void) { return expectingreply; }

    void setIP(QString &lip) { ip = lip; }
    QString getIP(void) { return ip; }

    void GetFreeDiskSpace(long long &totalspace, long long &usedspace);
    int StopRecording(const ProgramInfo *pginfo);
    int CheckRecordingActive(const ProgramInfo *pginfo);
    int DeleteRecording(const ProgramInfo *pginfo, bool forceMetadataDelete = false);
    void FillProgramInfo(ProgramInfo *pginfo, QString &playbackhost);
    void GenPreviewPixmap(const ProgramInfo *pginfo);
    QString PixmapLastModified(const ProgramInfo *pginfo);
    bool CheckFile(const ProgramInfo *pginfo);

    bool IsBusy(int capturecardnum);
    int GetEncoderState(int capturecardnum);
    long long GetMaxBitrate(int capturecardnum);
    bool EncoderIsRecording(int capturecardnum, const ProgramInfo *pginfo);
    int StartRecording(int capturecardnum, const ProgramInfo *pginfo);
    void RecordPending(int capturecardnum, const ProgramInfo *pginfo, int secsleft);

  private:
    bool SendReceiveStringList(QStringList &strlist);

    RefSocket *sock;
    QString hostname;
    QString ip;

    bool local;
    bool events;

    bool backend;

    QMutex sockLock;

    bool expectingreply;
    bool disconnected;

    int refCount;

    MainServer *m_parent;
};

#endif
