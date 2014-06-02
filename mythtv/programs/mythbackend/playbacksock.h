#ifndef PLAYBACKSOCK_H_
#define PLAYBACKSOCK_H_

#include <vector>
using namespace std;

#include <QStringList>
#include <QDateTime>
#include <QMutex>
#include <QSize>

#include "referencecounter.h"
#include "programinfo.h" // For RecStatusType
#include "inputinfo.h"

class MythSocket;
class MainServer;
class ProgramInfo;

typedef enum {
    kPBSEvents_None       = 0,
    kPBSEvents_Normal     = 1,
    kPBSEvents_NonSystem  = 2,
    kPBSEvents_SystemOnly = 3
} PlaybackSockEventsMode;

class PlaybackSock : public ReferenceCounter
{
  public:
    PlaybackSock(MainServer *parent, MythSocket *lsock,
                 QString lhostname, PlaybackSockEventsMode eventsMode);
    virtual ~PlaybackSock();

    void SetDisconnected(void) { disconnected = true; }
    bool IsDisconnected(void) const { return disconnected; }

    MythSocket *getSocket(void) const { return sock; }
    QString getHostname(void) const { return hostname; }

    bool isLocal(void) const { return local; }
    bool wantsEvents(void) const;
    bool wantsNonSystemEvents(void) const;
    bool wantsSystemEvents(void) const;
    bool wantsOnlySystemEvents(void) const;
    PlaybackSockEventsMode eventsMode(void) const;

    bool getBlockShutdown(void) const { return blockshutdown; }
    void setBlockShutdown(bool value) { blockshutdown = value; }

    // all backend<->backend stuff below here
    bool isSlaveBackend(void) const { return backend; }
    void setAsSlaveBackend(void) { backend = true; mediaserver = true; }

    bool isMediaServer(void) const { return mediaserver; }
    void setAsMediaServer(void) { mediaserver = true; }

    void setIP(QString &lip) { ip = lip; }
    QString getIP(void) const { return ip; }

    bool GoToSleep(void);
    void GetDiskSpace(QStringList &o_strlist);
    int DeleteFile(const QString &filename, const QString &sgroup);
    int StopRecording(const ProgramInfo *pginfo);
    int CheckRecordingActive(const ProgramInfo *pginfo);
    int DeleteRecording(const ProgramInfo *pginfo, bool forceMetadataDelete = false);
    bool FillProgramInfo(ProgramInfo &pginfo, const QString &playbackhost);
    QStringList GetSGFileList(QString &host, QString &groupname,
                              QString &directory, bool fileNamesOnly);
    QStringList GetSGFileQuery(QString &host, QString &groupname,
                               QString &filename);
    QString GetFileHash(QString filename, QString storageGroup);

    QStringList GenPreviewPixmap(const QString     &token,
                                 const ProgramInfo *pginfo);
    QStringList GenPreviewPixmap(const QString     &token,
                                 const ProgramInfo *pginfo,
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
    ProgramInfo *GetRecording(uint cardid);
    bool EncoderIsRecording(int capturecardnum, const ProgramInfo *pginfo);
    RecStatusType StartRecording(int capturecardnum,
                                 ProgramInfo *pginfo);
    RecStatusType GetRecordingStatus(int capturecardnum);
    void RecordPending(int capturecardnum, const ProgramInfo *pginfo,
                       int secsleft, bool hasLater);
    int SetSignalMonitoringRate(int capturecardnum, int rate, int notifyFrontend);
    void SetNextLiveTVDir(int capturecardnum, QString dir);
    vector<InputInfo> GetFreeInputs(int capturecardnum,
                                    const vector<uint> &excluded_cardids);
    void CancelNextRecording(int capturecardnum, bool cancel);

    QStringList ForwardRequest(const QStringList&);

    bool ReadStringList(QStringList &list);

  private:
    bool SendReceiveStringList(QStringList &strlist, uint min_reply_length = 0);

    MythSocket *sock;
    QString hostname;
    QString ip;

    bool local;
    PlaybackSockEventsMode m_eventsMode;
    bool blockshutdown;
    bool backend;
    bool mediaserver;

    QMutex sockLock;

    bool disconnected;

    MainServer *m_parent;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
