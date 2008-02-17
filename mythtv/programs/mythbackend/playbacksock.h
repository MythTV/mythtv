#ifndef PLAYBACKSOCK_H_
#define PLAYBACKSOCK_H_

#include <qstring.h>
#include <qmutex.h>

#include "programinfo.h"

class MythSocket;
class MainServer;
class ProgramInfo;
class InputInfo;

class PlaybackSock
{
  public:
    PlaybackSock(MainServer *parent, MythSocket *lsock, 
                 QString lhostname, bool wantevents);
    virtual ~PlaybackSock();

    void UpRef(void);
    bool DownRef(void);

    void SetDisconnected(void) { disconnected = true; }
    bool IsDisconnected(void) { return disconnected; }

    MythSocket *getSocket(void) { return sock; }
    QString getHostname(void) { return hostname; }

    bool isLocal(void) { return local; }
    bool wantsEvents(void) { return events; }

    bool getBlockShutdown(void) { return blockshutdown; }
    void setBlockShutdown(bool value) { blockshutdown = value; }

    // all backend<->backend stuff below here
    bool isSlaveBackend(void) { return backend; }
    void setAsSlaveBackend(void) { backend = true; }

    bool isExpectingReply(void) { return expectingreply; }

    void setIP(QString &lip) { ip = lip; }
    QString getIP(void) { return ip; }

    void GetDiskSpace(QStringList &o_strlist);
    int StopRecording(const ProgramInfo *pginfo);
    int CheckRecordingActive(const ProgramInfo *pginfo);
    int DeleteRecording(const ProgramInfo *pginfo, bool forceMetadataDelete = false);
    void FillProgramInfo(ProgramInfo *pginfo, QString &playbackhost);
    QStringList GenPreviewPixmap(const ProgramInfo *pginfo);
    QStringList GenPreviewPixmap(const ProgramInfo *pginfo,
                                 bool               time_fmt_sec,
                                 long long          time,
                                 const QString     &outputFile,
                                 const QSize       &outputSize);
    QDateTime PixmapLastModified(const ProgramInfo *pginfo);
    bool CheckFile(ProgramInfo *pginfo);

    bool IsBusy(int        capturecardnum,
                InputInfo *busy_input  = NULL,
                int        time_buffer = 5);
    int GetEncoderState(int capturecardnum);
    long long GetMaxBitrate(int capturecardnum);
    ProgramInfo *GetRecording(int capturecardnum);
    bool EncoderIsRecording(int capturecardnum, const ProgramInfo *pginfo);
    RecStatusType StartRecording(int capturecardnum, 
                                 const ProgramInfo *pginfo);
    void RecordPending(int capturecardnum, const ProgramInfo *pginfo,
                       int secsleft, bool hasLater);
    int SetSignalMonitoringRate(int capturecardnum, int rate, int notifyFrontend);
    void SetNextLiveTVDir(int capturecardnum, QString dir);
    vector<InputInfo> GetFreeInputs(int capturecardnum,
                                    const vector<uint> &excluded_cardids);
    void CancelNextRecording(int capturecardnum, bool cancel);

  private:
    bool SendReceiveStringList(QStringList &strlist);

    MythSocket *sock;
    QString hostname;
    QString ip;

    bool local;
    bool events;
    bool blockshutdown;
    bool backend;

    QMutex refLock;
    QMutex sockLock;

    bool expectingreply;
    bool disconnected;

    int refCount;

    MainServer *m_parent;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
