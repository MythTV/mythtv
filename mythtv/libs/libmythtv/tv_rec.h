#ifndef TVREC_H
#define TVREC_H

#include <qstring.h>
#include <pthread.h>
#include <qdatetime.h>
#include <qvaluelist.h>
#include <qptrlist.h>

#include "tv.h"

#include <map>
using namespace std;

class QSqlDatabase;
class QSocket;
class ChannelBase;
class ProgramInfo;
class RingBuffer;
class NuppelVideoRecorder;
class RecorderBase;

typedef enum
{
    BROWSE_SAME,
    BROWSE_UP,
    BROWSE_DOWN,
    BROWSE_LEFT,
    BROWSE_RIGHT
} BrowseDirections;


class TVRec
{
 public:
    TVRec(int capturecardnum);
   ~TVRec(void);

    void Init(void);

    void RecordPending(ProgramInfo *rcinfo, int secsleft);
    int StartRecording(ProgramInfo *rcinfo);

    void StopRecording(void);
    void FinishRecording(void) { finishRecording = true; }

    void FrontendReady(void) { frontendReady = true; }
    void CancelNextRecording(void) { cancelNextRecording = true; }
    ProgramInfo *GetRecording(void);

    char *GetScreenGrab(ProgramInfo *pginfo, const QString &filename, 
                        int secondsin, int &bufferlen,
                        int &video_width, int &video_height);

    bool IsRunning(void) { return runMainLoop; }
    void Stop(void) { runMainLoop = false; }

    TVState GetState(void);
    bool IsPlaying(void) { return StateIsPlaying(internalState); }
    bool IsRecording(void) { return StateIsRecording(internalState); }

    bool CheckChannel(ChannelBase *chan, const QString &channum, 
                      QSqlDatabase *& a_db_conn, pthread_mutex_t &a_db_lock); 
    void SetChannelValue(QString &field_name,int value, ChannelBase *chan,
                         const QString &channum);
    int GetChannelValue(const QString &channel_field, ChannelBase *chan, 
                        const QString &channum);
    bool SetVideoFiltersForChannel(ChannelBase *chan, const QString &channum);
    QString GetNextChannel(ChannelBase *chan, int channeldirection);
    QString GetNextRelativeChanID(QString channum, int channeldirection);
    void DoGetNextChannel(QString &channum, QString channelinput,
                          int cardid, QString channelorder,
                          int channeldirection, QString &chanid);

    void RetrieveInputChannels(map<int, QString> &inputChannel,
                               map<int, QString> &inputTuneTo,
                               map<int, QString> &externalChanger);
    void StoreInputChannels(map<int, QString> &inputChannel);

    RecorderBase *GetRecorder(void);

    bool IsBusy(void);
    bool IsReallyRecording(void);

    float GetFramerate(void);
    long long GetFramesWritten(void);
    long long GetFilePosition(void);
    long long GetFreeSpace(long long totalreadpos);
    long long GetKeyframePosition(long long desired);
    void StopPlaying(void);
    void SetupRingBuffer(QString &path, long long &filesize, 
                         long long &fillamount, bool pip = false);
    void SpawnLiveTV(void);
    void StopLiveTV(void);
    void PauseRecorder(void);
    void ToggleInputs(void);
    void ToggleChannelFavorite(void);
    void ChangeChannel(int channeldirection);
    void SetChannel(QString name);
    int ChangeColour(bool direction);
    int ChangeContrast(bool direction);
    int ChangeBrightness(bool direction);
    int ChangeHue(bool direction);
    bool CheckChannel(QString name);
    void GetNextProgram(int direction,
                        QString &title, QString &subtitle, QString &desc,
                        QString &category, QString &starttime, 
                        QString &endtime, QString &callsign, QString &iconpath,
                        QString &channelname, QString &chanid);
    void GetChannelInfo(QString &title, QString &subtitle, QString &desc,
                        QString &category, QString &starttime, 
                        QString &endtime, QString &callsign, QString &iconpath,
                        QString &channelname, QString &chanid);
    void GetInputName(QString &inputname);

    void SpawnReadThread(QSocket *sock);
    void KillReadThread(void);
    QSocket *GetReadThreadSocket(void);

    void PauseRingBuffer(void);
    void UnpauseRingBuffer(void);
    void PauseClearRingBuffer(void);
    void RequestRingBufferBlock(int size);
    long long SeekRingBuffer(long long curpos, long long pos, int whence);

    bool isParsingCommercials(ProgramInfo *pginfo);

 protected:
    void RunTV(void);
    static void *EventThread(void *param);

    void DoFlagCommercialsThread(void);
    static void *FlagCommercialsThread(void *param);
    void FlagCommercials(void);

    void DoReadThread(void);
    static void *ReadThread(void *param);

 private:
    void SetChannel(bool needopen = false);

    void GetChannelInfo(ChannelBase *chan, QString &title, QString &subtitle, 
                        QString &desc, QString &category, QString &starttime, 
                        QString &endtime, QString &callsign, QString &iconpath,
                        QString &channelname, QString &chanid);

    void GetDevices(int cardnum, QString &video, QString &vbi, QString &audio,
                    int &rate, QString &defaultinput, QString &startchannel,
                    QString &type, int &use_ts, char &dvb_type);

    void ConnectDB(int cardnum);
    void DisconnectDB(void);

    void SetupRecorder(class RecordingProfile& profile);
    void TeardownRecorder(bool killFile = false);
    
    void StateToString(TVState state, QString &statestr);
    void HandleStateChange(void);
    bool StateIsRecording(TVState state);
    bool StateIsPlaying(TVState state);
    TVState RemovePlaying(TVState state);
    TVState RemoveRecording(TVState state);

    void StartedRecording(void);
    void FinishedRecording(void);

    void SetOption(RecordingProfile &profile, const QString &name);

    RecorderBase *nvr;
    RingBuffer *rbuffer;
    ChannelBase *channel;

    QSqlDatabase *db_conn;
    pthread_mutex_t db_lock;

    TVState internalState;

    bool frontendReady;
    bool runMainLoop;
    bool exitPlayer;
    bool finishRecording;
    bool paused;
    bool prematurelystopped;
    QDateTime recordEndTime;

    float frameRate;

    pthread_t event, encode, readthread, commercials;

    bool changeState;
    TVState nextState;
    QString outputFilename;

    bool watchingLiveTV;

    ProgramInfo *curRecording;
    ProgramInfo *prevRecording;
 
    QString videodev, vbidev, audiodev, cardtype;
    int audiosamplerate;

    bool inoverrecord;
    int overrecordseconds;

    long long readrequest;
    QSocket *readthreadSock;
    bool readthreadlive;
    bool flagthreadstarted;
    pthread_mutex_t readthreadLock;

    int m_capturecardnum;

    bool ispip;

    QPtrList <ProgramInfo> commercialFlag;
    pthread_mutex_t commLock;

    bool askAllowRecording;
    bool recordPending;
    bool cancelNextRecording;

    ProgramInfo *pendingRecording;
    QDateTime recordPendingStart;
};

#endif
