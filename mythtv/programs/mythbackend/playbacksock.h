#ifndef PLAYBACKSOCK_H_
#define PLAYBACKSOCK_H_

#include <qsocket.h>
#include <qstring.h>
#include <qmutex.h>

class ProgramInfo;

class PlaybackSock
{
  public:
    PlaybackSock(QSocket *lsock, QString lhostname, bool wantevents);
   ~PlaybackSock();

    QSocket *getSocket(void) { return sock; }
    QString getHostname(void) { return hostname; }

    bool isLocal(void) { return local; }
    bool wantsEvents(void) { return events; }

    // all backend<->backend stuff below here
    bool isSlaveBackend(void) { return backend; }
    void setAsSlaveBackend(void) { backend = true; }

    bool isExpectingReply(void) { return expectingreply; }

    void setIP(QString &lip) { ip = lip; }
    QString getIP(void) { return ip; }

    void GetFreeSpace(int &totalspace, int &usedspace);
    int StopRecording(ProgramInfo *pginfo);
    int DeleteRecording(ProgramInfo *pginfo);
    void FillProgramInfo(ProgramInfo *pginfo, QString &playbackhost);
    void GenPreviewPixmap(ProgramInfo *pginfo);
    bool CheckFile(ProgramInfo *pginfo);

    bool IsBusy(int capturecardnum);
    int GetEncoderState(int capturecardnum);
    bool EncoderIsRecording(int capturecardnum, ProgramInfo *pginfo);
    int StartRecording(int capturecardnum, ProgramInfo *pginfo);
    void RecordPending(int capturecardnum, ProgramInfo *pginfo, int secsleft);

  private:
    bool SendReceiveStringList(QStringList &strlist);

    QSocket *sock;
    QString hostname;
    QString ip;

    bool local;
    bool events;

    bool backend;

    QMutex sockLock;

    bool expectingreply;
};

#endif
