#ifndef TVREC_H
#define TVREC_H

#include <qstring.h>
#include <pthread.h>
#include <qdatetime.h>

#include "tv.h"

#include <map>
using namespace std;

class QSqlDatabase;
class QSocket;
class Channel;
class ProgramInfo;
class RingBuffer;
class NuppelVideoRecorder;

class TVRec
{
 public:
    TVRec(int capturecardnum);
   ~TVRec(void);

    void Init(void);

    void StartRecording(ProgramInfo *rcinfo);
    void StopRecording(void);
    void FinishRecording(void) { finishRecording = true; }

    ProgramInfo *GetRecording(void) { return curRecording; }

    char *GetScreenGrab(QString filename, int secondsin, int &bufferlen,
                        int &video_width, int &video_height);

    bool IsRunning(void) { return runMainLoop; }
    void Stop(void) { runMainLoop = false; }

    TVState GetState(void) { return internalState; }
    bool ChangingState(void) { return changeState; }
    bool IsPlaying(void) { return StateIsPlaying(internalState); }
    bool IsRecording(void) { return StateIsRecording(internalState); }

    bool CheckChannel(Channel *chan, const QString &channum, int &finetuning); 
    void SetChannelValue(QString &field_name,int value, Channel *chan,
                         const QString &channum);
    int GetChannelValue(const QString &channel_field,Channel *chan, 
                        const QString &channum);
    bool SetVideoFiltersForChannel(Channel *chan, const QString &channum);
    QString GetNextChannel(Channel *chan, int channeldirection);

    void RetrieveInputChannels(map<int, QString> &inputChannel,
                               map<int, QString> &inputTuneTo,
                               map<int, QString> &externalChanger);
    void StoreInputChannels(map<int, QString> &inputChannel);

    bool IsReallyRecording(void);
    float GetFramerate(void);
    long long GetFramesWritten(void);
    long long GetFilePosition(void);
    long long GetFreeSpace(long long totalreadpos);
    long long GetKeyframePosition(long long desired);
    void TriggerRecordingTransition(void);
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
    void ChangeDeinterlacer(int deinterlace_mode);
    bool CheckChannel(QString name);
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

 protected:
    void RunTV(void);
    static void *EventThread(void *param);
    static void *FlagCommercialsThread(void *param);

    void DoReadThread(void);
    static void *ReadThread(void *param);

 private:
    void SetChannel(bool needopen = false);

    void GetChannelInfo(Channel *chan, QString &title, QString &subtitle, 
                        QString &desc, QString &category, QString &starttime, 
                        QString &endtime, QString &callsign, QString &iconpath,
                        QString &channelname, QString &chanid);

    void GetDevices(int cardnum, QString &video, QString &vbi, QString &audio,
                    int &rate, QString &defaultinput, QString &startchannel);

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

    void FlagCommercials(void);

    void WriteRecordedRecord(void);

    void SetOption(RecordingProfile &profile, const QString &name);

    NuppelVideoRecorder *nvr;
    RingBuffer *rbuffer;
    Channel *channel;

    int deinterlace_mode;

    QSqlDatabase *db_conn;
    pthread_mutex_t db_lock;

    TVState internalState;

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
    int tvtorecording;
    
    QString videodev, vbidev, audiodev;
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
};

#endif
