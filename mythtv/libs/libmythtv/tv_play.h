#ifndef TVPLAY_H
#define TVPLAY_H

#include <qstring.h>
#include <qmap.h>
#include <qdatetime.h>
#include <pthread.h>
#include <qvaluevector.h>
#include <qptrlist.h>
#include <qmutex.h>
#include <qstringlist.h>

#include "mythdeque.h"
#include "tv.h"
#include "util.h"

#include <qobject.h>

#include <vector>
using namespace std;

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

typedef QValueVector<QString> str_vec_t;
typedef QMap<QString, QString> InfoMap;

class TV : public QObject
{
    Q_OBJECT
  public:
    /// \brief Helper class for Sleep Timer code.
    class SleepTimerInfo
    {
      public:
        SleepTimerInfo(QString str, unsigned long secs)
            : dispString(str), seconds(secs) { ; }
        QString   dispString;
        unsigned long seconds;
    };

    TV(void);
   ~TV();

    static void InitKeys(void);

    bool Init(bool createWindow = true);

    int LiveTV(bool showDialogs = true);
    void ShowNoRecorderDialog(void);
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
                        InfoMap &infoMap);
    void GetNextProgram(RemoteEncoder *enc, int direction,
                        QString &title,     QString &subtitle,
                        QString &desc,      QString &category,
                        QString &starttime, QString &endtime,
                        QString &callsign,  QString &iconpath,
                        QString &channame,  QString &chanid,
                        QString &seriesid,  QString &programid);

    void GetChannelInfo(RemoteEncoder *enc, InfoMap &infoMap);
    void GetChannelInfo(RemoteEncoder *enc, QString &title,
                        QString &subtitle,  QString &desc,
                        QString &category,  QString &starttime,
                        QString &endtime,   QString &callsign,
                        QString &iconpath,  QString &channelname,
                        QString &chanid,    QString &seriesid,
                        QString &programid, QString &outFilters, 
                        QString &repeat,    QString &airdate,
                        QString &stars);

    // for the guidegrid to use
    void EmbedOutput(WId wid, int x, int y, int w, int h);
    void StopEmbeddingOutput(void);
    void EPGChannelUpdate(QString chanid, QString chanstr);
    static void GetValidRecorderList(uint chanid, QStringList &reclist);
   
    bool getRequestDelete(void) { return requestDelete; }
    bool getEndOfRecording(void) { return endOfRecording; }

    void ProcessKeypress(QKeyEvent *e);
    void customEvent(QCustomEvent *e);

    void AddPreviousChannel(void);
    void PreviousChannel(void);

    OSD *GetOSD(void);

    bool IsErrored(void) { return errored; }

    void setLastProgram(ProgramInfo *rcinfo);
    bool getJumpToProgram(void) { return jumpToProgram; }

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
    bool RequestNextRecorder(bool showDialogs);
    void DeleteRecorder();

    static uint GetLockTimeout(uint cardid);
    bool StartRecorder(int maxWait=-1);
    bool StartPlayer(bool isWatchingRecording, int maxWait=-1);
    void StartOSD(void);
    void StopStuff(bool stopRingbuffers, bool stopPlayers, bool stopRecorders);
    
    QString GetFiltersForChannel(void);

    void ToggleChannelFavorite(void);
    void ChangeChannel(int direction, bool force = false);
    void ChangeChannelByString(QString &name, bool force = false);
    void PauseLiveTV(void);
    void UnpauseLiveTV(void);

    void ChangeVolume(bool up);
    void ToggleMute(void);
    void ToggleLetterbox(int letterboxMode = -1);
    void ChangeContrast(bool up, bool recorder);
    void ChangeBrightness(bool up, bool recorder);
    void ChangeColour(bool up, bool recorder);
    void ChangeHue(bool up, bool recorder);

    void ChangeAudioTrack(int dir);
    void SetAudioTrack(int track);

    void ChangeSubtitleTrack(int dir);
    void SetSubtitleTrack(int track);
    
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
    enum ArbSeekWhence {
        ARBSEEK_SET = 0,
        ARBSEEK_REWIND,
        ARBSEEK_FORWARD,
        ARBSEEK_END
    };
    void DoArbSeek(ArbSeekWhence whence);
    void NormalSpeed(void);
    void ChangeSpeed(int direction);
    void ChangeTimeStretch(int dir, bool allowEdit = true);
    void ChangeAudioSync(int dir, bool allowEdit = true);
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
    void UpdateOSDSignal(const QStringList& strlist);
    void UpdateOSDTimeoutMessage(void);

    void LoadMenu(void);

    void SetupPlayer(bool isWatchingRecording);
    void TeardownPlayer(void);
    void SetupPipPlayer(void);
    void TeardownPipPlayer(void);
    
    void HandleStateChange(void);
    bool InStateChange(void) const;
    void ChangeState(TVState nextState);
    void ForceNextStateNone(void);
    bool StateIsRecording(TVState state);
    bool StateIsPlaying(TVState state);
    bool StateIsLiveTV(TVState state);
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

  private:
    // Configuration variables from database
    QString baseFilters;
    int     fftime;
    int     rewtime;
    int     jumptime;
    bool    usePicControls;
    bool    smartChannelChange;
    bool    MuteIndividualChannels;
    bool    arrowAccel;
    int     osd_display_time;

    int     autoCommercialSkip;
    bool    tryUnflaggedSkip;

    bool    smartForward;
    int     stickykeys;
    float   ff_rew_repos;
    bool    ff_rew_reverse;
    vector<int> ff_rew_speeds;

    bool    showBufferedWarnings;
    int     bufferedChannelThreshold;
    int     vbimode;

    // State variables
    MythDeque<TVState> nextStates;
    mutable QMutex     stateLock;
    TVState            internalState;

    bool menurunning;
    bool runMainLoop;
    bool wantsToQuit;
    bool exitPlayer;
    bool paused;
    bool errored;
    bool stretchAdjustment; ///< True if time stretch is turned on
    bool audiosyncAdjustment; ///< True if audiosync is turned on
    long long audiosyncBaseline;
    bool editmode;          ///< Are we in video editing mode
    bool zoomMode;
    bool sigMonMode;     ///< Are we in signal monitoring mode?
    bool update_osd_pos; ///< Redisplay osd?
    bool endOfRecording; ///< !nvp->IsPlaying() && StateIsPlaying(internalState)
    bool requestDelete;  ///< User wants last video deleted
    bool doSmartForward;
    bool switchingCards; ///< User wants new recorder, see mythfrontend/main.cpp
    int lastRecorderNum; ///< Last recorder, for implementing SwitchCards()
    bool queuedTranscode;
    bool getRecorderPlaybackInfo; ///< Main loop should get recorderPlaybackInfo
    int picAdjustment;   ///< Player pict attr to modify (on arrow left or right)
    int recAdjustment;   ///< Which recorder picture attribute to modify...

    /// Vector or sleep timer sleep times in seconds,
    /// with the appropriate UI message.
    vector<SleepTimerInfo> sleep_times;
    uint    sleep_index; ///< Index into sleep_times.
    QTimer *sleepTimer;  ///< Timer for turning off playback.

    /// Queue of unprocessed key presses.
    QPtrList<QKeyEvent> keyList;
    /// Since keys are processed outside Qt event loop, we need a lock.
    QMutex  keyListLock;
    bool    keyRepeat;      ///< Are repeats logical on last key?
    QTimer *keyrepeatTimer; ///< Timeout timer for repeat key filtering

    int   doing_ff_rew;  ///< If true we are doing a rewind not a fast forward
    int   ff_rew_index;  ///< Index into ff_rew_speeds for FF and Rewind speeds
    int   speed_index;   ///< Caches value of ff_rew_speeds[ff_rew_index]

    /** \brief Time stretch speed, 1.0f for normal playback.
     *
     *  Begins at 1.0f meaning normal playback, but can be increased
     *  or decreased to speedup or slowdown playback.
     *  Ignored when doing Fast Forward or Rewind.
     */
    float normal_speed; 

    float frameRate;     ///< Estimated framerate from recorder

    // Channel changing state variables
    bool    channelqueued;  ///< If true info is queued up for a channel change
    QString channelKeys;    ///< Channel key presses queued up so far...
    QString inputKeys;      ///< Input key presses queued up so far...
    bool    lookForChannel; ///< If true try to find recorder for channel
    QString channelid;      ///< Non-numeric Channel Name
    QString lastCC;         ///< Last channel
    int     lastCCDir;      ///< Last channel changing direction
    QTimer *muteTimer;      ///< For temp. audio muting during channel changes

    // Channel changing timeout notification variables
    QTime   lockTimer;
    bool    lockTimerOn;
    QMap<uint,uint> lockTimeout;

    // Previous channel functionality state variables
    str_vec_t prevChan;       ///< Previous channels
    uint      prevChanKeyCnt; ///< Number of repeated channel button presses
    QTimer   *prevChanTimer;  ///< Special (slower) repeat key filtering

    // Channel browsing state variables
    bool browsemode;
    bool persistentbrowsemode;
    QTimer *browseTimer;
    QString browsechannum;
    QString browsechanid;
    QString browsestarttime;

    // Program Info for currently playing video
    // (or next video if InChangeState() is true)
    ProgramInfo *recorderPlaybackInfo; ///< info requested from recorder
    ProgramInfo *playbackinfo;  ///< info sent in via Playback()
    QString      inputFilename; ///< playbackinfo->pathname
    int          playbackLen;   ///< initial playbackinfo->CalculateLength()
    ProgramInfo *lastProgram;   ///< last program played with this player
    bool         jumpToProgram;

    // Video Players
    NuppelVideoPlayer *nvp;
    NuppelVideoPlayer *pipnvp;
    NuppelVideoPlayer *activenvp;  ///< Player to which LiveTV events are sent

    // Remote Encoders
    RemoteEncoder *recorder;
    RemoteEncoder *piprecorder;
    RemoteEncoder *activerecorder; ///< Recorder to which LiveTV events are sent

    // RingBuffers
    RingBuffer *prbuffer;
    RingBuffer *piprbuffer;
    RingBuffer *activerbuffer; ///< Ringbuffer to which LiveTV events are sent

    // OSD info
    QString         dialogname; ///< Name of current OSD dialog
    OSDGenericTree *treeMenu;   ///< OSD menu, 'm' using default keybindings
    /// UDPNotify instance which shows messages sent
    /// to the "UDPNotifyPort" in an OSD dialog.
    UDPNotify      *udpnotify;
    QStringList     lastSignalMsg;
    MythTimer       lastSignalMsgTime;
    InfoMap         lastSignalUIInfo;
    MythTimer       lastSignalUIInfoTime;
    QMutex          osdlock;

    // LCD Info
    QDateTime lastLcdUpdate;
    QString   lcdTitle;
    QString   lcdSubtitle;
    QString   lcdCallsign;

    // Window info (GUI is optional, transcoding, preview img, etc)
    MythDialog *myWindow;   ///< Our MythDialog window, if it exists
    WId   embedWinID;       ///< Window ID when embedded in another widget
    QRect embedBounds;      ///< Bounds when embedded in another widget
    ///< player bounds, for after doLoadMenu() returns to normal playing.
    QRect player_bounds;
    ///< Prior GUI window bounds, for doLoadMenu() and player exit().
    QRect saved_gui_bounds;

    // Various threads
    /// Event processing thread, runs RunTV().
    pthread_t event;
    /// Video decoder thread, runs nvp's NuppelVideoPlayer::StartPlaying().
    pthread_t decode;
    /// Picture-in-Picture video decoder thread,
    /// runs pipnvp's NuppelVideoPlayer::StartPlaying().
    pthread_t pipdecode;

    // Constants
    static const int kInitFFRWSpeed; ///< 1x, default to normal speed
    static const int kMuteTimeout;   ///< Channel changing mute timeout in msec
    static const int kLCDTimeout;    ///< Timeout for updating LCD info in msec
    static const int kBrowseTimeout; ///< Timeout for browse mode exit in msec
    /// Timeout after last Signal Monitor message for ignoring OSD when exiting.
    static const int kSMExitTimeout;
    static const int kChannelKeysMax;///< When to start discarding early keys
};

#endif
