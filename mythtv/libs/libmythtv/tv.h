#ifndef TV_H
#define TV_H

#include <string>
using namespace std;

#include <mysql.h>
#include <pthread.h>

#include "NuppelVideoRecorder.h"
#include "NuppelVideoPlayer.h"
#include "RingBuffer.h"
#include "settings.h"

#include "channel.h"

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

class TV
{
 public:
    TV(const string &startchannel);
   ~TV(void);

    void LiveTV(void);
    void StartRecording(const string &channelName, int duration, 
                        const string &outputFileName);
    void Playback(const string &inputFileName);

    bool IsRunning(void) { return runMainLoop; }
    void Stop(void) { runMainLoop = false; }

    TVState GetState(void) { return internalState; }

    bool CheckChannel(int channum); 

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
  
    void UpdateOSD(void); 
    void GetChannelInfo(int lchannel, string &title, string &subtitle, 
                        string &desc, string &category, string &starttime,
                        string &endtime);

    void ConnectDB(void);
    void DisconnectDB(void);

    void LoadMenu(void);

    void SetupRecorder();
    void TeardownRecorder(bool killFile = false);

    void SetupPlayer();
    void TeardownPlayer();

    void ProcessKeypress(int keypressed);

    void StateToString(TVState state, string &statestr);
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

    MYSQL *db_conn;

    bool menurunning;

    TVState internalState;

    bool runMainLoop;
    bool exitPlayer;
    bool paused;
    int secsToRecord;

    float frameRate;

    pthread_t event, encode, decode;
    bool changeState;
    TVState nextState;
    string inputFilename, outputFilename;
    string recordChannel;

    bool watchingLiveTV;
};

#endif
