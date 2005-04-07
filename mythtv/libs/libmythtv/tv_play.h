#ifndef TVPLAY_H
#define TVPLAY_H

#include <qstring.h>
#include <qmap.h>
#include <qdatetime.h>
#include <pthread.h>
#include <qvaluevector.h>
#include <qptrlist.h>
#include <qmutex.h>

#include "tv.h"

#include <qobject.h>

class QDateTime;
class OSD;
class RemoteEncoder;
class NuppelVideoPlayer;
class RingBuffer;
class ProgramInfo;
class MythDialog;
class UDPNotify;
class OSDListTreeType;
class OSDGenericTree;

class TV : public QObject
{
    Q_OBJECT
  public:
    TV(void);
   ~TV();

    static void InitKeys(void);

    bool Init(bool createWindow = true);

    int LiveTV(bool showDialogs = true);
    void StopLiveTV(void) { exitPlayer = true; }
    void FinishRecording(void);
    bool WantsToQuit(void) { return wantsToQuit; }
    int GetLastRecorderNum(void) { return lastRecorderNum; }
    int PlayFromRecorder(int recordernum);

    void AskAllowRecording(const QStringList &messages, int timeuntil);

    // next two functions only work on recorded programs.
    int Playback(ProgramInfo *rcinfo);

    bool IsRunning(void) { return runMainLoop; }
    void Stop(void) { runMainLoop = false; }

    TVState GetState(void);
    bool IsPlaying(void) { return StateIsPlaying(GetState()); }
    bool IsRecording(void) { return StateIsRecording(GetState()); }
    bool IsMenuRunning(void) { return menurunning; }
    bool IsSwitchingCards(void) { return switchingCards; }

    void GetNextProgram(RemoteEncoder *enc, int direction,
                        QMap<QString, QString> &infoMap);
    void GetNextProgram(RemoteEncoder *enc, int direction,
                        QString &title, QString &subtitle,
                        QString &desc, QString &category, QString &starttime,
                        QString &endtime, QString &callsign, QString &iconpath,
                        QString &channelname, QString &chanid,
                        QString &seriesid, QString &programid);

    void GetChannelInfo(RemoteEncoder *enc, QMap<QString, QString> &infoMap);
    void GetChannelInfo(RemoteEncoder *enc, QString &title, QString &subtitle,
                        QString &desc, QString &category, QString &starttime,
                        QString &endtime, QString &callsign, QString &iconpath,
                        QString &channelname, QString &chanid,
                        QString &seriesid, QString &programid, QString &outFilters, 
                        QString &repeat, QString &airdate, QString &stars);

    // for the guidegrid to use
    void EmbedOutput(WId wid, int x, int y, int w, int h);
    void StopEmbeddingOutput(void);
    void EPGChannelUpdate(QString chanstr);
   
    bool getRequestDelete(void) { return requestDelete; }
    bool getEndOfRecording(void) { return endOfRecording; }

    void ProcessKeypress(QKeyEvent *e);
    void customEvent(QCustomEvent *e);

    void AddPreviousChannel(void);
    void PreviousChannel(void);

    OSD *GetOSD(void);

    bool IsErrored() { return errored; }
  public slots:
    void HandleOSDClosed(int osdType);

  protected slots:
    void SetPreviousChannel(void);
    void UnMute(void);
    void KeyRepeatOK(void);
    void BrowseEndTimer(void) { BrowseEnd(false); }
    void SleepEndTimer(void);
    void TreeMenuEntered(OSDListTreeType *tree, OSDGenericTree *item);
    void TreeMenuSelected(OSDListTreeType *tree, OSDGenericTree *item);

  protected:
    void doLoadMenu(void);
    static void *MenuHandler(void *param);

    void RunTV(void);
    static void *EventThread(void *param);

    bool eventFilter(QObject *o, QEvent *e);

  private:
    void StartPlayerAndRecorder(bool StartPlayer, bool StartRecorder);
    void StopPlayerAndRecorder(bool StopPlayer, bool StopRecorder);
    
    void SetChannel(bool needopen = false);
    QString getFiltersForChannel();

    void ToggleChannelFavorite(void);
    void ChangeChannel(int direction, bool force = false);
    void ChangeChannelByString(QString &name, bool force = false);

    void ChangeVolume(bool up);
    void ToggleMute(void);
    void ToggleLetterbox(int letterboxMode = -1);
    void ChangeContrast(bool up, bool recorder);
    void ChangeBrightness(bool up, bool recorder);
    void ChangeColour(bool up, bool recorder);
    void ChangeHue(bool up, bool recorder);

    QString QueuedChannel(void);
    void ChannelClear(bool hideosd = false); 
    void ChannelKey(char key);
    void ChannelCommit(void);

    void ToggleInputs(void); 

    void SwitchCards(void);

    void ToggleSleepTimer(void);
    void ToggleSleepTimer(const QString);

    void DoInfo(void);
    void DoPlay(void);
    void DoPause(void);
    bool UpdatePosOSD(float time, const QString &mesg, int disptime = 2);
    void DoSeek(float time, const QString &mesg);
    void DoArbSeek(int dir);
    void NormalSpeed(void);
    void ChangeSpeed(int direction);
    void ChangeTimeStretch(int dir, bool allowEdit = true);
    float StopFFRew(void);
    void ChangeFFRew(int direction);
    void SetFFRew(int index);
    void DoToggleCC(int mode);
    void DoSkipCommercials(int direction);
    void DoEditMode(void);

    void DoQueueTranscode(void);  

    void SetAutoCommercialSkip(int skipMode = 0);
    void SetManualZoom(bool zoomON = false);
 
    bool ClearOSD(void);
    void ToggleOSD(void); 
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

    void ToggleAutoExpire(void);

    void BrowseStart(void);
    void BrowseEnd(bool change);
    void BrowseDispInfo(int direction);
    void ToggleRecord(void);
    void BrowseChannel(QString &chan);

    void DoTogglePictureAttribute(void);
    void DoToggleRecPictureAttribute(void);
    void DoChangePictureAttribute(int control, bool up, bool rec);

    void BuildOSDTreeMenu(void);
    void ShowOSDTreeMenu(void);

    void UpdateLCD(void);
    void ShowLCDChannelInfo(void);

    QString PlayMesg(void);

    // Configuration variables from database
    int repoLevel;
    QString baseFilters;
    int fftime;
    int rewtime;
    int jumptime;
    bool usePicControls;
    bool smartChannelChange;
    bool showBufferedWarnings;
    int bufferedChannelThreshold;
    bool MuteIndividualChannels;
    bool arrowAccel;
    char vbimode;

    // Configuretion variables from DB in RunTV
    int stickykeys;
    int ff_rew_repos;
    bool ff_rew_reverse;
    bool smartForward;
    
    // Configuration variables from DB just before playback
    int autoCommercialSkip;
    bool tryUnflaggedSkip;
    int osd_display_time;

    // State variables
    TVState internalState;
    TVState nextState;
    bool changeState;
    bool menurunning;
    bool runMainLoop;
    bool wantsToQuit;
    bool exitPlayer;
    bool paused;
    bool errored;
    bool stretchAdjustment; // is time stretch turned on
    bool editmode;       // are we in video editing mode
    bool zoomMode;
    bool update_osd_pos; // redisplay osd?
    bool endOfRecording; // !nvp->IsPlaying() && StateIsPlaying(internalState)
    bool requestDelete;  // user wants last video deleted
    bool doSmartForward;
    bool switchingCards; // user wants new recorder, see mythfrontend/main.cpp
    int lastRecorderNum; // last recorder, for implementing SwitchCards()
    bool queuedTranscode;
    bool getRecorderPlaybackInfo; // main loop should get recorderPlaybackInfo
    int sleep_index;     // Off, go to sleep in 30 minutes, 60 minutes...
    QTimer *sleepTimer;  // timer for turning off playback
    int picAdjustment;   // player pict attr to modify (on arrow left or right)
    int recAdjustment;   // which recorder picture attribute to modify...

    // Key processing buffer, lock, and state
    QMutex keyListLock;  // keys are processed outside Qt Event loop, need lock
    QPtrList<QKeyEvent> keyList; // list of unprocessed key presses
    bool keyRepeat;      // are repeats logical on last key?
    QTimer *keyrepeatTimer; // timeout timer for repeat key filtering

    // Fast forward state
    int doing_ff_rew;
    int ff_rew_index;
    int speed_index;
    float normal_speed;

    // Channel changing state variables
    bool channelqueued;
    char channelKeys[5];
    int channelkeysstored;
    QString lastCC;      // last channel
    int lastCCDir;       // last channel changing direction
    QTimer *muteTimer;   // for temporary audio muting during channel changes

    // previous channel functionality state variables
    typedef QValueVector<QString> PrevChannelVector;
    PrevChannelVector channame_vector; // previous channels
    unsigned int times_pressed; // number of repeats detected
    QTimer *prevChannelTimer;   // for special (slower) repeat key filtering

    // channel browsing state variables
    bool browsemode;
    bool persistentbrowsemode;
    QTimer *browseTimer;
    QString browsechannum;
    QString browsechanid;
    QString browsestarttime;

    // Program Info for currently playing video
    // (or next video if changeState is true)
    ProgramInfo *playbackinfo; // info sent in via Playback()
    QString inputFilename;     // playbackinfo->pathname
    int playbackLen;           // initial playbackinfo->CalculateLength()
    ProgramInfo *recorderPlaybackInfo; // info requested from recorder

    // other info on currently playing video
    float frameRate; // estimated framerate from recorder

    // Video Players
    NuppelVideoPlayer *nvp;
    NuppelVideoPlayer *pipnvp;
    NuppelVideoPlayer *activenvp;

    // Remote Encoders
    RemoteEncoder *recorder;
    RemoteEncoder *piprecorder;
    RemoteEncoder *activerecorder;

    // RingBuffers
    RingBuffer *prbuffer;
    RingBuffer *piprbuffer;
    RingBuffer *activerbuffer;

    // OSD info
    OSD *osd;
    QString dialogname;
    OSDGenericTree *treeMenu;
    UDPNotify *udpnotify;

    // LCD Info
    QDateTime lastLcdUpdate;
    QString lcdTitle, lcdSubtitle, lcdCallsign;

    // Window info (GUI is optional, transcoding, preview img, etc)
    MythDialog *myWindow;// Our MythDialow window, if it exists
    WId embedid;         // Window ID when embedded in another widget
    int embx, emby, embw, embh; // bounds when embedded in another widget
    QRect saved_gui_bounds; // prior GUI window bounds, for after player is done

    // Various threads
    pthread_t event;     // TV::RunTV(), the event thread
    pthread_t decode;    // nvp::StartPlaying(), video decoder thread
    pthread_t pipdecode; // pipnvp, Picture-in-Picture video decoder thread
};

#endif
