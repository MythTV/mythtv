#ifndef TVPLAY_H
#define TVPLAY_H

#include <qstring.h>
#include <pthread.h>

#include <qobject.h>

class QSqlDatabase;
class QDateTime;
class OSD;
class RemoteEncoder;
class VolumeControl;
class NuppelVideoPlayer;
class RingBuffer;
class ProgramInfo;

class TV : public QObject
{
 public:
    TV(QSqlDatabase *db);
   ~TV(void);

    void Init(void);

    TVState LiveTV(void);
    void StopLiveTV(void) { exitPlayer = true; }
    void FinishRecording(void);

    int AllowRecording(const QString &message, int timeuntil);

    // next two functions only work on recorded programs.
    void Playback(ProgramInfo *rcinfo);

    bool IsRunning(void) { return runMainLoop; }
    void Stop(void) { runMainLoop = false; }

    TVState GetState(void) { return internalState; }
    bool ChangingState(void) { return changeState; }
    bool IsPlaying(void) { return StateIsPlaying(internalState); }
    bool IsRecording(void) { return StateIsRecording(internalState); }

    void GetChannelInfo(RemoteEncoder *enc, QString &title, QString &subtitle,
                        QString &desc, QString &category, QString &starttime,
                        QString &endtime, QString &callsign, QString &iconpath,
                        QString &channelname, QString &chanid);

    // for the guidegrid to use
    void EmbedOutput(unsigned long wid, int x, int y, int w, int h);
    void StopEmbeddingOutput(void);
    void EPGChannelUpdate(QString chanstr);
   
    bool getRequestDelete(void) { return requestDelete; }
    bool getEndOfRecording(void) { return endOfRecording; }

    void ProcessKeypress(int keypressed);
    void customEvent(QCustomEvent *e);

 protected:
    void doLoadMenu(void);
    static void *MenuHandler(void *param);

    void RunTV(void);
    static void *EventThread(void *param);

 private:
    void SetChannel(bool needopen = false);

    void ToggleChannelFavorite(void);
    void ChangeChannel(int direction);
    void ChangeChannelByString(QString &name);

    void ChangeVolume(bool up);
    void ToggleMute(void);
    void ChangeContrast(bool up);
    void ChangeBrightness(bool up);
    void ChangeColour(bool up);
    void ChangeDeinterlacer();   
 
    void ChannelKey(int key);
    void ChannelCommit(void);

    void ToggleInputs(void); 

    void DoPosition(void);
    void DoPause(void);
    void DoFF(void);
    void DoRew(void);
    void DoJumpAhead(void);
    void DoJumpBack(void);
    void DoSkipCommercials(int direction);
    int  calcSliderPos(int offset, QString &desc);
    
    void UpdateOSD(void);
    void UpdateOSDInput(void);

    void LoadMenu(void);

    void SetupPlayer();
    void TeardownPlayer();
    void SetupPipPlayer();
    void TeardownPipPlayer();
    
    void StateToString(TVState state, QString &statestr);
    void HandleStateChange();
    bool StateIsRecording(TVState state);
    bool StateIsPlaying(TVState state);
    TVState RemovePlaying(TVState state);
    TVState RemoveRecording(TVState state);

    void TogglePIPView(void);
    void ToggleActiveWindow(void);
    void SwapPIP(void);

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

    int jumptime;
    int fftime;
    int rewtime;
    int stickykeys;
    bool doing_ff;
    bool doing_rew;
    int ff_rew_index;
    float ff_rew_scaling;

    OSD *osd;

    NuppelVideoPlayer *nvp;
    NuppelVideoPlayer *pipnvp;
    NuppelVideoPlayer *activenvp;

    RemoteEncoder *recorder;
    RemoteEncoder *piprecorder;
    RemoteEncoder *activerecorder;

    RingBuffer *prbuffer;
    RingBuffer *piprbuffer;
    RingBuffer *activerbuffer;

    QString dialogname;
    bool editmode;
    bool requestDelete;
    bool endOfRecording;

    ProgramInfo *playbackinfo;

    QSqlDatabase *m_db;

    VolumeControl *volumeControl;

    int deinterlace_mode;

    unsigned int embedid;
    int embx, emby, embw, embh;
};

#endif
