#ifndef TVREC_H
#define TVREC_H

#include <qstring.h>
#include <pthread.h>
#include <qdatetime.h>

#include "tv.h"

class QSqlDatabase;
class QSocket;
class Channel;
class ProgramInfo;
class RingBuffer;
class NuppelVideoRecorder;

class TVRec
{
 public:
    TVRec(const QString &startchannel, int capturecardnum);
   ~TVRec(void);

    void Init(void);

    int AllowRecording(ProgramInfo *rcinfo, int timeuntil);
    void StartRecording(ProgramInfo *rcinfo);
    void StopRecording(void);

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
    bool ChangeExternalChannel(const QString &channum);
    QString GetNextChannel(Channel *chan, bool direction);

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
    void ChangeChannel(bool direction);
    void SetChannel(QString name);
    void ChangeColour(bool direction);
    void ChangeContrast(bool direction);
    void ChangeBrightness(bool direction);
    bool CheckChannel(QString name);
    void GetChannelInfo(QString &title, QString &subtitle, QString &desc,
                        QString &category, QString &starttime, 
                        QString &endtime, QString &callsign, QString &iconpath,
                        QString &channelname);
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

    void DoReadThread(void);
    static void *ReadThread(void *param);

 private:
    void SetChannel(bool needopen = false);

    void GetChannelInfo(Channel *chan, QString &title, QString &subtitle, 
                        QString &desc, QString &category, QString &starttime, 
                        QString &endtime, QString &callsign, QString &iconpath,
                        QString &channelname);

    void GetDevices(int cardnum, QString &video, QString &vbi, QString &audio,
                    int &rate, QString &defaultinput);

    void ConnectDB(int cardnum);
    void DisconnectDB(void);

    void SetupRecorder(class RecordingProfile& profile);
    void TeardownRecorder(bool killFile = false);
    
    void StateToString(TVState state, QString &statestr);
    void HandleStateChange();
    bool StateIsRecording(TVState state);
    bool StateIsPlaying(TVState state);
    TVState RemovePlaying(TVState state);
    TVState RemoveRecording(TVState state);

    void WriteRecordedRecord();

    NuppelVideoRecorder *nvr;
    RingBuffer *rbuffer;
    Channel *channel;

    QSqlDatabase *db_conn;
    pthread_mutex_t db_lock;

    TVState internalState;

    bool runMainLoop;
    bool exitPlayer;
    bool paused;
    QDateTime recordEndTime;

    float frameRate;

    pthread_t event, encode, readthread;

    bool changeState;
    TVState nextState;
    QString outputFilename;

    bool watchingLiveTV;

    ProgramInfo *curRecording;
    int tvtorecording;
    
    QString videodev, vbidev, audiodev;
    int audiosamplerate;

    bool inoverrecord;
    int overrecordseconds;

    long long readrequest;
    QSocket *readthreadSock;
    bool readthreadlive;
    pthread_mutex_t readthreadLock;

    int m_capturecardnum;

    bool ispip;
};

#endif
