#ifndef PLAYBACKSOCK_H_
#define PLAYBACKSOCK_H_

#include <qsocket.h>
#include <qstring.h>
#include <qmutex.h>

class ProgramInfo;
class RefSocket;

class PlaybackSock
{
  public:
    PlaybackSock(RefSocket *lsock, QString lhostname, bool wantevents);
   ~PlaybackSock();

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
    int StopRecording(ProgramInfo *pginfo);
    int CheckRecordingActive(ProgramInfo *pginfo);
    int DeleteRecording(ProgramInfo *pginfo, bool forceMetadataDelete = false);
    void FillProgramInfo(ProgramInfo *pginfo, QString &playbackhost);
    void GenPreviewPixmap(ProgramInfo *pginfo);
    QString PixmapLastModified(ProgramInfo *pginfo);
    bool CheckFile(ProgramInfo *pginfo);

    bool IsBusy(int capturecardnum);
    int GetEncoderState(int capturecardnum);
    long long GetMaxBitrate(int capturecardnum);
    bool EncoderIsRecording(int capturecardnum, ProgramInfo *pginfo);
    int StartRecording(int capturecardnum, ProgramInfo *pginfo);
    void RecordPending(int capturecardnum, ProgramInfo *pginfo, int secsleft);

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
};

#endif
