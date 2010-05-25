// -*- Mode: c++ -*-

#ifndef TVPLAY_H
#define TVPLAY_H

// C++
#include <vector>
using namespace std;

// POSIX
#include <pthread.h>

// Qt
#include <QReadWriteLock>
#include <QWaitCondition>
#include <QStringList>
#include <QDateTime>
#include <QKeyEvent>
#include <QObject>
#include <QRegExp>
#include <QString>
#include <QThread>
#include <QEvent>
#include <QMutex>
#include <QHash>
#include <QTime>
#include <QMap>

// MythTV
#include "mythdeque.h"
#include "tv.h"
#include "util.h"
#include "programinfo.h"
#include "channelutil.h"
#include "videoouttypes.h"
#include "volumebase.h"
#include "inputinfo.h"
#include "channelgroup.h"

class QDateTime;
class OSD;
class RemoteEncoder;
class NuppelVideoPlayer;
class DetectLetterbox;
class RingBuffer;
class ProgramInfo;
class MythDialog;
class UDPNotify;
class OSDListTreeType;
class OSDGenericTree;
class PlayerContext;
class UDPNotifyOSDSet;
class TVOSDMenuEntryList;
class TvPlayWindow;
class TV;
class OSDListTreeItemEnteredEvent;
class OSDListTreeItemSelectedEvent;

typedef QMap<QString,InfoMap>    DDValueMap;
typedef QMap<QString,DDValueMap> DDKeyMap;
typedef void (*EMBEDRETURNVOID) (void *, bool);
typedef void (*EMBEDRETURNVOIDEPG) (uint, const QString &, TV *, bool, bool, int);
typedef void (*EMBEDRETURNVOIDFINDER) (TV *, bool, bool);
typedef void (*EMBEDRETURNVOIDSCHEDIT) (const ProgramInfo *, void *);

// Locking order
//
// playerLock -> askAllowLock    -> osdLock
//            -> progListLock    -> osdLock
//            -> chanEditMapLock -> osdLock
//            -> lastProgramLock
//            -> is_tunable_cache_lock
//            -> recorderPlaybackInfoLock
//            -> timerIdLock
//            -> mainLoopCondLock
//            -> stateChangeCondLock
//            -> browseLock
//
// When holding one of these locks, you may lock any lock of  the locks to
// the right of the current lock, but may not lock any lock to the left of
// this lock (which will cause a deadlock). Nor should you lock any other
// lock in the TV class without first adding it to the locking order list
// above.
//
// Note: Taking a middle lock such as askAllowLock, without taking a
// playerLock first does not violate these rules, but once you are
// holding it, you can not later lock playerLock.
//
// It goes without saying that any locks outside of this class should only
// be taken one at a time, and should be taken last and released first of
// all the locks required for any algorithm. (Unless you use tryLock and
// release all locks if it can't gather them all at once, see the
// "multi_lock()" function as an example; but this is not efficient nor
// desirable and should be avoided when possible.)
//

class VBIMode
{
  public:
    typedef enum
    {
        None    = 0,
        PAL_TT  = 1,
        NTSC_CC = 2,
    } vbimode_t;

    static uint Parse(QString vbiformat)
    {
        QString fmt = vbiformat.toLower().left(3);
        vbimode_t mode;
        mode = (fmt == "pal") ? PAL_TT : ((fmt == "nts") ? NTSC_CC : None);
        return (uint) mode;
    }
};

enum scheduleEditTypes {
    kScheduleProgramGuide = 0,
    kScheduleProgramFinder,
    kScheduledRecording,
    kViewSchedule,
    kPlaybackBox
};

typedef enum
{
    kAskAllowCancel,
    kAskAllowOneRec,
    kAskAllowMultiRec,
} AskAllowType;

/**
 * Type of message displayed in ShowNoRecorderDialog()
 */
typedef enum
{
    kNoRecorders = 0,  ///< No free recorders
    kNoCurrRec = 1,    ///< No current recordings
    kNoTuners = 2,     ///< No capture cards configured
} NoRecorderMsg;

enum {
    kStartTVNoFlags          = 0x00,
    kStartTVInGuide          = 0x01,
    kStartTVInPlayList       = 0x02,
    kStartTVByNetworkCommand = 0x04,
    kStartTVIgnoreBookmark   = 0x08,
};

class AskProgramInfo
{
  public:
    AskProgramInfo() :
        has_rec(false),                has_later(false),
        is_in_same_input_group(false), is_conflicting(false),
        info(NULL) {}
    AskProgramInfo(const QDateTime &e, bool r, bool l, ProgramInfo *i) :
        expiry(e), has_rec(r), has_later(l),
        is_in_same_input_group(false), is_conflicting(false),
        info(i) {}

    QDateTime    expiry;
    bool         has_rec;
    bool         has_later;
    bool         is_in_same_input_group;
    bool         is_conflicting;
    ProgramInfo *info;
};

class MPUBLIC TV : public QThread
{
    friend class QTVEventThread;
    friend class PlaybackBox;
    friend class GuideGrid;
    friend class TvPlayWindow;

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

    bool Init(bool createWindow = true);

    // User input processing commands
    void ProcessKeypress(PlayerContext*, QKeyEvent *e);
    void ProcessNetworkControlCommand(PlayerContext *, const QString &command);
    void customEvent(QEvent *e);
    bool HandleTrackAction(PlayerContext*, const QString &action);

    // LiveTV commands
    bool LiveTV(bool showDialogs = true, bool startInGuide = false);

    // Embedding commands for the guidegrid to use in LiveTV
    bool StartEmbedding(PlayerContext*, WId wid, const QRect&);
    void StopEmbedding(PlayerContext*);
    bool IsTunable(const PlayerContext*, uint chanid, bool use_cache = false);
    void ClearTunableCache(void);
    void ChangeChannel(const PlayerContext*, const DBChanList &options);

    void DrawUnusedRects(void);

    // Recording commands
    int  PlayFromRecorder(int recordernum);
    int  Playback(const ProgramInfo &rcinfo);

    // Commands used by frontend playback box
    QString GetRecordingGroup(int player_idx) const;

    // Various commands
    void setInPlayList(bool setting) { inPlaylist = setting; }
    void setUnderNetworkControl(bool setting) { underNetworkControl = setting; }

    void ShowNoRecorderDialog(const PlayerContext*,
                              NoRecorderMsg msgType = kNoRecorders);
    void FinishRecording(int player_idx); ///< Finishes player's recording
    void AskAllowRecording(PlayerContext*, const QStringList&, int, bool, bool);
    void PromptStopWatchingRecording(PlayerContext*);
    void PromptDeleteRecording(PlayerContext*, QString title);
    bool PromptRecGroupPassword(PlayerContext*);

    bool CreatePBP(PlayerContext *lctx, const ProgramInfo *info);
    bool CreatePIP(PlayerContext *lctx, const ProgramInfo *info);
    bool ResizePIPWindow(PlayerContext*);

    // Boolean queries
    /// Returns true if we are currently in the process of switching recorders.
    bool IsSwitchingCards(void)  const { return switchToRec; }
    /// Returns true if the TV event thread is running. Should always be true
    /// between the end of the constructor and the beginning of the destructor.
    bool IsRunning(void)         const { return isRunning(); }
    /// Returns true if the user told Mythtv to allow re-recording of the show
    bool getAllowRerecord(void) const { return allowRerecord;  }
    /// This is set to true if the player reaches the end of the
    /// recording without the user explicitly exiting the player.
    bool getEndOfRecording(void) const { return endOfRecording; }
    /// This is set if the user asked MythTV to jump to the previous
    /// recording in the playlist.
    bool getJumpToProgram(void)  const { return jumpToProgram; }
    /// true iff program is the same as the one in the selected player
    bool IsSameProgram(int player_idx, const ProgramInfo *p) const;
    bool IsBookmarkAllowed(const PlayerContext*) const;
    bool IsDeleteAllowed(const PlayerContext*) const;
    bool IsPBPSupported(const PlayerContext *ctx = NULL) const;
    bool IsPIPSupported(const PlayerContext *ctx = NULL) const;

    // Other queries
    int GetLastRecorderNum(int player_idx) const;
    TVState GetState(int player_idx) const;
    TVState GetState(const PlayerContext*) const;

    // Non-const queries
    OSD *GetOSDL(const char *, int);
    OSD *GetOSDL(const PlayerContext*,const char *, int);
    void ReturnOSDLock(OSD*&);
    void ReturnOSDLock(const PlayerContext*,OSD*&);

    bool ActiveHandleAction(PlayerContext*,
                            const QStringList &actions,
                            bool isDVD, bool isDVDStillFrame);

    void GetNextProgram(RemoteEncoder *enc,
                        BrowseDirection direction, InfoMap &infoMap) const;
    void GetNextProgram(BrowseDirection direction, InfoMap &infoMap) const;

    // static functions
    static void InitKeys(void);
    static void ResetKeys(void);
    static bool StartTV(ProgramInfo *tvrec = NULL,
                        uint flags = kStartTVNoFlags);
    static void SetFuncPtr(const char *, void *);

    // Used by EPG
    void ChangeVolume(PlayerContext*, bool up);
    void ToggleMute(PlayerContext*);

    void SetNextProgPIPState(PIPState state) { jumpToProgramPIPState = state; }

    // Used for UDPNotify
    bool HasUDPNotifyEvent(void) const;
    void HandleUDPNotifyEvent(void);

    // Channel Groups
    void SaveChannelGroup(void);
    void UpdateChannelList(int groupID);

  public slots:
    void HandleOSDClosed(int osdType);
    void timerEvent(QTimerEvent*);

  protected slots:
    void AddUDPNotifyEvent(const QString &name, const UDPNotifyOSDSet*);
    void ClearUDPNotifyEvents(void);

  protected:
    void TreeMenuEntered(OSDListTreeItemEnteredEvent *e);
    void TreeMenuSelected(OSDListTreeItemSelectedEvent *e);

    void DoEditSchedule(int editType = kScheduleProgramGuide);

    virtual void run(void);
    void TVEventThreadChecks(void);

    void PauseAudioUntilBuffered(PlayerContext *ctx);

    bool eventFilter(QObject *o, QEvent *e);
    static QStringList lastProgramStringList;
    static EMBEDRETURNVOID RunPlaybackBoxPtr;
    static EMBEDRETURNVOID RunViewScheduledPtr;
    static EMBEDRETURNVOIDEPG RunProgramGuidePtr;
    static EMBEDRETURNVOIDFINDER RunProgramFinderPtr;
    static EMBEDRETURNVOIDSCHEDIT RunScheduleEditorPtr;

  private:
    void SetActive(PlayerContext *lctx, int index, bool osd_msg);

    PlayerContext       *GetPlayerWriteLock(
        int which, const char *file, int location);
    PlayerContext       *GetPlayerReadLock(
        int which, const char *file, int location);
    const PlayerContext *GetPlayerReadLock(
        int which, const char *file, int location) const;

    PlayerContext       *GetPlayerHaveLock(
        PlayerContext*,
        int which, const char *file, int location);
    const PlayerContext *GetPlayerHaveLock(
        const PlayerContext*,
        int which, const char *file, int location) const;

    void ReturnPlayerLock(PlayerContext*&);
    void ReturnPlayerLock(const PlayerContext*&) const;

    int  StartTimer(int interval, int line);
    void KillTimer(int id);
    void ForceNextStateNone(PlayerContext*);
    void ScheduleStateChange(PlayerContext*);
    void SetErrored(PlayerContext*);
    void PrepToSwitchToRecordedProgram(PlayerContext*,
                                       const ProgramInfo &);
    void PrepareToExitPlayer(PlayerContext*, int line,
                             bool bookmark = true) const;
    void SetExitPlayer(bool set_it, bool wants_to) const;
    void SetUpdateOSDPosition(bool set_it);

    bool PxPHandleAction(PlayerContext*,const QStringList &actions);
    bool ToggleHandleAction(PlayerContext*,
                            const QStringList &actions, bool isDVD);
    bool FFRewHandleAction(PlayerContext*, const QStringList &actions);
    bool ActivePostQHandleAction(PlayerContext*,
                                 const QStringList &actions, bool isDVD);
    bool HandleJumpToProgramAction(PlayerContext *ctx,
                                   const QStringList   &actions);

    bool RequestNextRecorder(PlayerContext *, bool);
    void DeleteRecorder();

    bool StartRecorder(PlayerContext *ctx, int maxWait=-1);
    void StopStuff(PlayerContext *mctx, PlayerContext *ctx,
                   bool stopRingbuffers, bool stopPlayers, bool stopRecorders);

    void ToggleChannelFavorite(PlayerContext*, QString);
    void ChangeChannel(PlayerContext*, int direction);
    void ChangeChannel(PlayerContext*, uint chanid, const QString &channum);
    void PauseLiveTV(PlayerContext*);
    void UnpauseLiveTV(PlayerContext*);

    void ShowPreviousChannel(PlayerContext*);
    void PopPreviousChannel(PlayerContext*, bool immediate_change);

    void ToggleAspectOverride(PlayerContext*,
                              AspectOverrideMode aspectMode = kAspect_Toggle);
    void ToggleAdjustFill(PlayerContext*,
                          AdjustFillMode adjustfillMode = kAdjustFill_Toggle);

    void SetSpeedChangeTimer(uint when, int line);

    void HandleEndOfPlaybackTimerEvent(void);
    void HandleIsNearEndWhenEmbeddingTimerEvent(void);
    void HandleEndOfRecordingExitPromptTimerEvent(void);
    void HandleVideoExitDialogTimerEvent(void);
    void HandlePseudoLiveTVTimerEvent(void);
    void HandleSpeedChangeTimerEvent(void);

    // key queue commands
    void AddKeyToInputQueue(PlayerContext*, char key);
    void ClearInputQueues(const PlayerContext*, bool hideosd);
    bool CommitQueuedInput(PlayerContext*);
    bool ProcessSmartChannel(const PlayerContext*, QString&);

    // query key queues
    bool HasQueuedInput(void) const
        { return !GetQueuedInput().isEmpty(); }
    bool HasQueuedChannel(void) const
        { return queuedChanID || !GetQueuedChanNum().isEmpty(); }

    // get queued up input
    QString GetQueuedInput(void)   const;
    int     GetQueuedInputAsInt(bool *ok = NULL, int base = 10) const;
    QString GetQueuedChanNum(void) const;
    uint    GetQueuedChanID(void)  const { return queuedChanID; }

    void SwitchSource(PlayerContext*, uint source_direction);
    void SwitchInputs(PlayerContext*, uint inputid);
    void ToggleInputs(PlayerContext*, uint inputid = 0);
    void SwitchCards(PlayerContext*,
                     uint chanid = 0, QString channum = "", uint inputid = 0);

    void ToggleSleepTimer(const PlayerContext*);
    void ToggleSleepTimer(const PlayerContext*, const QString &time);

    void DoPlay(PlayerContext*);
    float DoTogglePauseStart(PlayerContext*);
    void DoTogglePauseFinish(PlayerContext*, float time, bool showOSD);
    void DoTogglePause(PlayerContext*, bool showOSD);
    vector<bool> DoSetPauseState(PlayerContext *lctx, const vector<bool>&);

    bool SeekHandleAction(PlayerContext *actx, const QStringList &actions,
                          const bool isDVD);
    void DoSeek(PlayerContext*, float time, const QString &mesg);
    bool DoNVPSeek(PlayerContext*, float time);
    enum ArbSeekWhence {
        ARBSEEK_SET = 0,
        ARBSEEK_REWIND,
        ARBSEEK_FORWARD,
        ARBSEEK_END
    };
    void DoArbSeek(PlayerContext*, ArbSeekWhence whence);
    void NormalSpeed(PlayerContext*);
    void ChangeSpeed(PlayerContext*, int direction);
    void ToggleTimeStretch(PlayerContext*);
    void ChangeTimeStretch(PlayerContext*, int dir, bool allowEdit = true);
    bool TimeStretchHandleAction(PlayerContext*,
                                 const QStringList &actions);

    void ToggleUpmix(PlayerContext*);
    void ChangeAudioSync(PlayerContext*, int dir, bool allowEdit = true);
    bool AudioSyncHandleAction(PlayerContext*, const QStringList &actions);

    float StopFFRew(PlayerContext*);
    void ChangeFFRew(PlayerContext*, int direction);
    void SetFFRew(PlayerContext*, int index);
    int  GetNumChapters(const PlayerContext*) const;
    void GetChapterTimes(const PlayerContext*, QList<long long> &times) const;
    int  GetCurrentChapter(const PlayerContext*) const;
    void DoJumpChapter(PlayerContext*, int direction);
    void DoSkipCommercials(PlayerContext*, int direction);
    void StartProgramEditMode(PlayerContext*);

    // Channel editing support
    void StartChannelEditMode(PlayerContext*);
    void ChannelEditKey(const PlayerContext*, const QKeyEvent*);
    void ChannelEditAutoFill(const PlayerContext*, InfoMap&) const;
    void ChannelEditAutoFill(const PlayerContext*, InfoMap&,
                             const QMap<QString,bool>&) const;
    void ChannelEditXDSFill(const PlayerContext*, InfoMap&) const;
    void ChannelEditDDFill(InfoMap&, const QMap<QString,bool>&, bool) const;
    QString GetDataDirect(QString key,   QString value,
                          QString field, bool    allow_partial = false) const;
    bool LoadDDMap(uint sourceid);
    void RunLoadDDMap(uint sourceid);
    static void *load_dd_map_thunk(void*);
    static void *load_dd_map_post_thunk(void*);

    void DoQueueTranscode(PlayerContext*,QString profile);

    void SetAutoCommercialSkip(const PlayerContext*,
                               CommSkipMode skipMode = kCommSkipOff);

    // Manual zoom mode
    void SetManualZoom(const PlayerContext *, bool enabled, QString msg);
    bool ManualZoomHandleAction(PlayerContext *actx,
                                const QStringList &actions);

    void DoDisplayJumpMenu(void);

    bool ClearOSD(const PlayerContext*);
    void ToggleOSD(const PlayerContext*, bool includeStatusOSD);
    void UpdateOSDProgInfo(const PlayerContext*, const char *whichInfo);
    void UpdateOSDSeekMessage(const PlayerContext*,
                              const QString &mesg, int disptime);
    void UpdateOSDInput(const PlayerContext*,
                        QString inputname = QString::null);
    void UpdateOSDTextEntry(const PlayerContext*, const QString &message);
    void UpdateOSDSignal(const PlayerContext*, const QStringList &strlist);
    void UpdateOSDTimeoutMessage(PlayerContext*);
    void UpdateOSDAskAllowDialog(PlayerContext*);
    void HandleOSDAskAllowResponse(PlayerContext*, int dialog_result);
    bool OSDDialogHandleAction(PlayerContext *actx, const QStringList &actions);

    void EditSchedule(const PlayerContext*,
                      int editType = kScheduleProgramGuide);

    void TeardownPlayer(PlayerContext *mctx, PlayerContext *ctx);

    void HandleStateChange(PlayerContext *mctx, PlayerContext *ctx);

    bool StartPlayer(PlayerContext *mctx, PlayerContext *ctx,
                     TVState desiredState);

    vector<long long> TeardownAllNVPs(PlayerContext*);
    void RestartAllNVPs(PlayerContext *lctx,
                        const vector<long long> &pos,
                        MuteState mctx_mute);
    void RestartMainNVP(PlayerContext *mctx);

    void PxPToggleView(  PlayerContext *actx, bool wantPBP);
    void PxPCreateView(  PlayerContext *actx, bool wantPBP);
    void PxPTeardownView(PlayerContext *actx);
    void PxPToggleType(  PlayerContext *mctx, bool wantPBP);
    void PxPSwap(        PlayerContext *mctx, PlayerContext *pipctx);
    bool HandlePxPTimerEvent(void);

    bool PIPAddPlayer(   PlayerContext *mctx, PlayerContext *ctx);
    bool PIPRemovePlayer(PlayerContext *mctx, PlayerContext *ctx);
    void PBPRestartMainNVP(PlayerContext *mctx);

    void ToggleAutoExpire(PlayerContext*);

    void BrowseStart(PlayerContext*);
    void BrowseEnd(PlayerContext*, bool change_channel);
    void BrowseDispInfo(PlayerContext*, BrowseDirection direction);
    void BrowseChannel(PlayerContext*, const QString &channum);
    bool BrowseHandleAction(PlayerContext*, const QStringList &actions);
    uint BrowseAllGetChanId(const QString &chan) const;

    void ToggleRecord(PlayerContext*);

    void DoTogglePictureAttribute(const PlayerContext*,
                                  PictureAdjustType type);
    void DoChangePictureAttribute(
        PlayerContext*,
        PictureAdjustType type, PictureAttribute attr, bool up);
    bool PictureAttributeHandleAction(PlayerContext*,
                                      const QStringList &actions);

    void ShowOSDTreeMenu(const PlayerContext*);

    void FillOSDTreeMenu(       const PlayerContext*, OSDGenericTree*, QString category) const;
    void FillMenuPlaying(       const PlayerContext*, OSDGenericTree*, QString category) const;
    void FillMenuAVChapter(     const PlayerContext*, OSDGenericTree*) const;
    void FillMenuPxP(           const PlayerContext*, OSDGenericTree*) const;
    void FillMenuInputSwitching(const PlayerContext*, OSDGenericTree*) const;
    void FillMenuVideoAspect(   const PlayerContext*, OSDGenericTree*) const;
    void FillMenuVideoScan(     const PlayerContext*, OSDGenericTree*) const;
    void FillMenuAdjustFill(    const PlayerContext*, OSDGenericTree*) const;
    void FillMenuAdjustPicture( const PlayerContext*, OSDGenericTree*) const;
    void FillMenuTimeStretch(   const PlayerContext*, OSDGenericTree*) const;
    void FillMenuSleepMode(     const PlayerContext*, OSDGenericTree*) const;
    bool FillMenuTracks(        const PlayerContext*, OSDGenericTree*, uint type) const;
    void FillMenuChanGroups(    const PlayerContext *ctx, OSDGenericTree *treeMenu) const;

    void UpdateLCD(void);
    bool HandleLCDTimerEvent(void);
    void ShowLCDChannelInfo(const PlayerContext*);
    void ShowLCDDVDInfo(const PlayerContext*);

    void ITVRestart(PlayerContext*, bool isLive);

    bool ScreenShot(PlayerContext*, long long frameNumber);

    // DVD methods
    void DVDJumpBack(PlayerContext*);
    void DVDJumpForward(PlayerContext*);
    bool DVDMenuHandleAction(PlayerContext*,
                             const QStringList &actions,
                             bool isDVD, bool isDVDStill);

    // Sleep dialog handling
    void SleepDialogCreate(void);
    void SleepDialogTimeout(void);

    // Idle dialog handling
    void IdleDialogCreate(void);
    void IdleDialogTimeout(void);

    // Program jumping stuff
    void SetLastProgram(const ProgramInfo *rcinfo);
    ProgramInfo *GetLastProgram(void) const;

    static bool LoadExternalSubtitles(NuppelVideoPlayer *nvp,
                                      const QString &videoFile);

    static QStringList GetValidRecorderList(uint chanid);
    static QStringList GetValidRecorderList(const QString &channum);
    static QStringList GetValidRecorderList(uint, const QString&);

    static bool StateIsRecording(TVState state);
    static bool StateIsPlaying(TVState state);
    static bool StateIsLiveTV(TVState state);
    static TVState RemoveRecording(TVState state);
    void RestoreScreenSaver(const PlayerContext*);

    void InitUDPNotifyEvent(void);

    /// true if dialog is either videoplayexit, playexit or askdelete dialog
    bool IsVideoExitDialog(const QString &dialog_name);

    // for temp debugging only..
    int find_player_index(const PlayerContext*) const;

  private:
    // Configuration variables from database
    QString baseFilters;
    QString db_channel_format;
    QString db_time_format;
    QString db_short_date_format;
    uint    db_idle_timeout;
    uint    db_udpnotify_port;
    uint    db_browse_max_forward;
    int     db_playback_exit_prompt;
    uint    db_autoexpire_default;
    bool    db_auto_set_watched;
    bool    db_end_of_rec_exit_prompt;
    bool    db_jump_prefer_osd;
    bool    db_use_gui_size_for_tv;
    bool    db_start_in_guide;
    bool    db_toggle_bookmark;
    bool    db_run_jobs_on_remote;
    bool    db_continue_embedded;
    bool    db_use_fixed_size;
    bool    db_browse_always;
    bool    db_browse_all_tuners;
    DBChanList db_browse_all_channels;

    bool    smartChannelChange;
    bool    MuteIndividualChannels;
    bool    arrowAccel;
    int     osd_general_timeout;
    int     osd_prog_info_timeout;

    CommSkipMode autoCommercialSkip;
    bool    tryUnflaggedSkip;

    bool    smartForward;
    float   ff_rew_repos;
    bool    ff_rew_reverse;
    bool    jumped_back; ///< Used by PromptDeleteRecording
    vector<int> ff_rew_speeds;

    uint    vbimode;

    QTime ctorTime;
    uint switchToInputId;
    /// True if the user told MythTV to stop plaback. If this is false
    /// when we exit the player, we display an error screen.
    mutable bool wantsToQuit;
    bool stretchAdjustment; ///< True if time stretch is turned on
    bool audiosyncAdjustment; ///< True if audiosync is turned on
    long long audiosyncBaseline;
    bool editmode;          ///< Are we in video editing mode
    bool zoomMode;
    bool sigMonMode;     ///< Are we in signal monitoring mode?
    bool endOfRecording; ///< !nvp->IsPlaying() && StateIsPlaying()
    bool requestDelete;  ///< User wants last video deleted
    bool allowRerecord;  ///< User wants to rerecord the last video if deleted
    bool doSmartForward;
    bool queuedTranscode;
    /// Picture attribute type to modify.
    PictureAdjustType adjustingPicture;
    /// Picture attribute to modify (on arrow left or right)
    PictureAttribute  adjustingPictureAttribute;

    // Ask Allow state
    AskAllowType                 askAllowType;
    QMap<QString,AskProgramInfo> askAllowPrograms;
    QMutex                       askAllowLock;

    MythDeque<QString> changePxP;
    QMutex progListsLock;
    QMap<QString,ProgramList> progLists;

    mutable QMutex chanEditMapLock; ///< Lock for chanEditMap and ddMap
    InfoMap   chanEditMap;          ///< Channel Editing initial map
    DDKeyMap  ddMap;                ///< DataDirect channel map
    uint      ddMapSourceId;        ///< DataDirect channel map sourceid
    bool      ddMapLoaderRunning;   ///< Is DataDirect loader thread running
    pthread_t ddMapLoader;          ///< DataDirect map loader thread

    /// Vector or sleep timer sleep times in seconds,
    /// with the appropriate UI message.
    vector<SleepTimerInfo> sleep_times;
    uint      sleep_index;          ///< Index into sleep_times.
    uint      sleepTimerTimeout;    ///< Current sleep timeout in msec
    int       sleepTimerId;         ///< Timer for turning off playback.
    int       sleepDialogTimerId;   ///< Timer for sleep dialog.
    /// Timer for turning off playback after idle period.
    int       idleTimerId;
    int       idleDialogTimerId; ///< Timer for idle dialog.

    /// Queue of unprocessed key presses.
    MythDeque<QKeyEvent*> keyList;
    MythTimer keyRepeatTimer; ///< Timeout timer for repeat key filtering

    // CC/Teletex input state variables
    /// Are we in CC/Teletext page/stream selection mode?
    bool  ccInputMode;

    // Arbitrary Seek input state variables
    /// Are we in Arbitrary seek input mode?
    bool  asInputMode;

    // Channel changing state variables
    /// Input key presses queued up so far...
    QString         queuedInput;
    /// Input key presses queued up so far to form a valid ChanNum
    mutable QString queuedChanNum;
    /// Queued ChanID (from EPG channel selector)
    uint            queuedChanID;

    // Channel changing timeout notification variables
    QTime   lockTimer;
    bool    lockTimerOn;
    QDateTime lastLockSeenTime;

    // Channel browsing state variables
    bool       browsemode;
    QString    browsechannum;
    uint       browsechanid;
    QString    browsestarttime;
    mutable QMutex browseLock; // protects db_browse_all_channels

    // Program Info for currently playing video
    // (or next video if InChangeState() is true)
    mutable QMutex lastProgramLock;
    ProgramInfo *lastProgram;   ///< last program played with this player
    bool         inPlaylist; ///< show is part of a playlist
    bool         underNetworkControl; ///< initial show started via by the network control interface

    // Program Jumping
    PIPState     jumpToProgramPIPState;
    bool         jumpToProgram;

    // Video Players
    vector<PlayerContext*>  player;
    /// Video Player to which events are sent to
    int                     playerActive;
    /// lock on player and playerActive changes
    mutable QReadWriteLock  playerLock;

    bool noHardwareDecoders;

    // Remote Encoders
    /// Main recorder to use after a successful SwitchCards() call.
    RemoteEncoder *switchToRec;

    // OSD info
    OSDGenericTree *treeMenu;   ///< OSD menu, 'm' using default keybindings
    QMap<OSD*,const PlayerContext*> osd_lctx;
    TVOSDMenuEntryList *osdMenuEntries;

    /// UDPNotify instance which shows messages sent
    /// to the "UDPNotifyPort" in an OSD dialog.
    UDPNotify                        *udpnotify;
    MythDeque<QString>                udpnotifyEventName;
    MythDeque<const UDPNotifyOSDSet*> udpnotifyEventSet;

    // LCD Info
    QString   lcdTitle;
    QString   lcdSubtitle;
    QString   lcdCallsign;

    // Window info (GUI is optional, transcoding, preview img, etc)
    TvPlayWindow *myWindow;   ///< Our screen, if it exists
    ///< player bounds, for after DoEditSchedule() returns to normal playing.
    QRect player_bounds;
    ///< Prior GUI window bounds, for DoEditSchedule() and player exit().
    QRect saved_gui_bounds;

    // embedded status
    bool         isEmbedded;       ///< are we currently embedded
    bool         ignoreKeyPresses; ///< should we ignore keypresses
    vector<bool> saved_pause;      ///< saved pause state before embedding

    // IsTunable() cache, used by embedded program guide
    mutable QMutex                 is_tunable_cache_lock;
    QMap< uint,vector<InputInfo> > is_tunable_cache_inputs;

#ifdef PLAY_FROM_RECORDER
    /// Info requested by PlayFromRecorder
    QMutex                    recorderPlaybackInfoLock;
    QWaitCondition            recorderPlaybackInfoWaitCond;
    QMap<int,int>             recorderPlaybackInfoTimerId;
    QMap<int,ProgramInfo>     recorderPlaybackInfo;
#endif // PLAY_FROM_RECORDER

    // Channel group stuff
    int channel_group_id;
    uint browse_changrp;
    ChannelGroupList m_changrplist;
    DBChanList m_channellist;

    // Network Control stuff
    MythDeque<QString> networkControlCommands;

    // Timers
    typedef QMap<int,PlayerContext*>       TimerContextMap;
    typedef QMap<int,const PlayerContext*> TimerContextConstMap;
    mutable QMutex       timerIdLock;
    volatile int         lcdTimerId;
    volatile int         keyListTimerId;
    volatile int         networkControlTimerId;
    volatile int         jumpMenuTimerId;
    volatile int         pipChangeTimerId;
    volatile int         udpNotifyTimerId;
    volatile int         switchToInputTimerId;
    volatile int         ccInputTimerId;
    volatile int         asInputTimerId;
    volatile int         queueInputTimerId;
    volatile int         browseTimerId;
    volatile int         updateOSDPosTimerId;
    volatile int         endOfPlaybackTimerId;
    volatile int         embedCheckTimerId;
    volatile int         endOfRecPromptTimerId;
    volatile int         videoExitDialogTimerId;
    volatile int         pseudoChangeChanTimerId;
    volatile int         speedChangeTimerId;
    volatile int         errorRecoveryTimerId;
    mutable volatile int exitPlayerTimerId;
    TimerContextMap      stateChangeTimerId;
    TimerContextMap      signalMonitorTimerId;
    TimerContextMap      tvchainUpdateTimerId;

    /// Condition to signal that the Event thread is up and running
    QWaitCondition mainLoopCond;
    QMutex mainLoopCondLock;

    /// Condition to signal State changes
    QWaitCondition stateChangeCond;
    QMutex stateChangeCondLock;

  public:
    // Constants
    static const int kInitFFRWSpeed; ///< 1x, default to normal speed
    static const uint kInputKeysMax; ///< When to start discarding early keys
    static const uint kNextSource;
    static const uint kPreviousSource;
    static const uint kMaxPIPCount;
    static const uint kMaxPBPCount;

    ///< Timeout for entry modes in msec
    static const uint kInputModeTimeout;
    /// Timeout for updating LCD info in msec
    static const uint kLCDTimeout;
    /// Timeout for browse mode exit in msec
    static const uint kBrowseTimeout;
    /// Seek key repeat timeout in msec
    static const uint kKeyRepeatTimeout;
    /// How long to wait before applying all previous channel keypresses in msec
    static const uint kPrevChanTimeout;
    /// How long to display sleep timer dialog in msec
    static const uint kSleepTimerDialogTimeout;
    /// How long to display idle timer dialog in seconds
    static const uint kIdleTimerDialogTimeout;
    /// How long to display idle timer dialog in msec
    static const uint kVideoExitDialogTimeout;

    static const uint kEndOfPlaybackCheckFrequency;
    static const uint kEmbedCheckFrequency;
    static const uint kSpeedChangeCheckFrequency;
    static const uint kErrorRecoveryCheckFrequency;
    static const uint kEndOfRecPromptCheckFrequency;
    static const uint kEndOfPlaybackFirstCheckTimer;
};

#endif
