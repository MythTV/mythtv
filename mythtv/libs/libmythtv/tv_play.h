#ifndef TVPLAY_H
#define TVPLAY_H

#include <qstring.h>
#include <pthread.h>

#include "NuppelVideoRecorder.h"
#include "NuppelVideoPlayer.h"
#include "RingBuffer.h"

#include "channel.h"

#include "libmyth/programinfo.h"

class QSqlDatabase;
class QDateTime;
class MythContext;
class OSD;

class TV
{
 public:
    TV(MythContext *lcontext);
   ~TV(void);

    void Init(void);

    TVState LiveTV(void);

    int AllowRecording(ProgramInfo *rcinfo, int timeuntil);

    // next two functions only work on recorded programs.
    void Playback(ProgramInfo *rcinfo);

    bool IsRunning(void) { return runMainLoop; }
    void Stop(void) { runMainLoop = false; }

    TVState GetState(void) { return internalState; }
    bool ChangingState(void) { return changeState; }
    bool IsPlaying(void) { return StateIsPlaying(internalState); }
    bool IsRecording(void) { return StateIsRecording(internalState); }

    void EmbedOutput(unsigned long wid, int x, int y, int w, int h);
    void StopEmbeddingOutput(void);

 protected:
    void doLoadMenu(void);
    static void *MenuHandler(void *param);

    void RunTV(void);
    static void *EventThread(void *param);

 private:
    void SetChannel(bool needopen = false);

    void ChangeChannel(bool up);
    void ChangeChannelByString(QString &name);
    
    void ChannelKey(int key);
    void ChannelCommit(void);

    void ToggleInputs(void); 

    void DoPosition(void);
    void DoPause(void);
    void DoFF(void);
    void DoRew(void);
    int  calcSliderPos(int offset, QString &desc);
    
    void UpdateOSD(void);
    void UpdateOSDInput(void);
    void GetChannelInfo(int recnum, QString &title, QString &subtitle, 
                        QString &desc, QString &category, QString &starttime, 
                        QString &endtime, QString &callsign, QString &iconpath,
                        QString &channelname);

    void LoadMenu(void);

    //void SetupPipRecorder();
    //void TeardownPipRecorder();
    
    void SetupPlayer();
    void TeardownPlayer();
    //void SetupPipPlayer();
    //void TeardownPipPlayer();
    
    void ProcessKeypress(int keypressed);

    void StateToString(TVState state, QString &statestr);
    void HandleStateChange();
    bool StateIsRecording(TVState state);
    bool StateIsPlaying(TVState state);
    TVState RemovePlaying(TVState state);
    TVState RemoveRecording(TVState state);

    //void TogglePIPView(void);
    //void ToggleActiveWindow(void);
    //void SwapPIP(void);

    MythContext *m_context;
    
    int osd_display_time;

    bool channelqueued;
    char channelKeys[4];
    int channelkeysstored;

    bool menurunning;

    TVState internalState;

    bool runMainLoop;
    bool exitPlayer;
    bool paused;

    float frameRate;

    pthread_t event, decode, pipdecode;
    bool changeState;
    TVState nextState;
    QString inputFilename;

    bool watchingLiveTV;

    int playbackLen;

    int fftime;
    int rewtime;
    int stickykeys;
    bool doing_ff;
    bool doing_rew;
    float ff_rew_scaling;

    OSD *osd;

    NuppelVideoPlayer *nvp;
    NuppelVideoPlayer *pipnvp;
    NuppelVideoPlayer *activenvp;

    int recorder_num;
    int piprecorder_num;
    int activerecorder_num;

    RingBuffer *prbuffer;
    RingBuffer *piprbuffer;
    RingBuffer *activerbuffer;

    QString dialogname;
    bool editmode;
};

#endif
