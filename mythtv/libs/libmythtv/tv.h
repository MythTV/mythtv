#ifndef TV_H
#define TV_H

#include <qstring.h>
#include <pthread.h>

#include "NuppelVideoRecorder.h"
#include "NuppelVideoPlayer.h"
#include "RingBuffer.h"
#include "settings.h"

#include "channel.h"
#include "programinfo.h"

typedef enum 
{
    kState_Error = -1,
    kState_None = 0,
    kState_WatchingLiveTV,
    kState_WatchingPreRecorded,
    kState_WatchingRecording,       // watching _what_ you're recording
    kState_WatchingOtherRecording,  // watching something else
    kState_RecordingOnly
} TVState;

class QSqlDatabase;
class QDateTime;

class TV
{
 public:
    TV(const QString &startchannel);
   ~TV(void);

    void LiveTV(void);

    int AllowRecording(ProgramInfo *rcinfo, int timeuntil);
    void StartRecording(ProgramInfo *rcinfo);
    void StopRecording(void);
    ProgramInfo *GetRecording(void) { return curRecording; }

    // next two functions only work on recorded programs.
    void Playback(ProgramInfo *rcinfo);
    char *GetScreenGrab(ProgramInfo *rcinfo, int secondsin, int &bufferlen,
                        int &video_width, int &video_height);

    bool IsRunning(void) { return runMainLoop; }
    void Stop(void) { runMainLoop = false; }

    TVState GetState(void) { return internalState; }

    bool CheckChannel(char *channum); 
    bool ChangeExternalChannel(char *channum);

    QString GetFilePrefix() { return settings->GetSetting("RecordFilePrefix"); }
 protected:
    void doLoadMenu(void);
    static void *MenuHandler(void *param);

    void RunTV(void);
    static void *EventThread(void *param);

 private:
    void ChangeChannel(bool up);
    void ChangeChannel(char *name);
    
    void ChannelKey(int key);
    void ChannelCommit(void);

    void ToggleInputs(void); 
 
    void UpdateOSD(void); 
    void GetChannelInfo(int lchannel, QString &title, QString &subtitle, 
                        QString &desc, QString &category, QString &starttime,
                        QString &endtime);

    void ConnectDB(void);
    void DisconnectDB(void);

    void LoadMenu(void);

    void SetupRecorder();
    void TeardownRecorder(bool killFile = false);

    void SetupPlayer();
    void TeardownPlayer();

    void ProcessKeypress(int keypressed);

    void StateToString(TVState state, QString &statestr);
    void HandleStateChange();
    bool StateIsRecording(TVState state);
    bool StateIsPlaying(TVState state);
    TVState RemovePlaying(TVState state);
    TVState RemoveRecording(TVState state);

    void WriteRecordedRecord();

    NuppelVideoRecorder *nvr;
    NuppelVideoPlayer *nvp;

    RingBuffer *rbuffer, *prbuffer;
    Channel *channel;

    Settings *settings;

    int osd_display_time;

    bool channelqueued;
    char channelKeys[4];
    int channelkeysstored;

    QSqlDatabase *db_conn;

    bool menurunning;

    TVState internalState;

    bool runMainLoop;
    bool exitPlayer;
    bool paused;
    QDateTime recordEndTime;

    float frameRate;

    pthread_t event, encode, decode;
    bool changeState;
    TVState nextState;
    QString inputFilename, outputFilename;

    bool watchingLiveTV;

    ProgramInfo *curRecording;
    int tvtorecording;
};

#endif
