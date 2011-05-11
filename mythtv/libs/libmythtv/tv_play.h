// -*- Mode: c++ -*-

#ifndef TVPLAY_H
#define TVPLAY_H

// C
#include <stdint.h>

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
#include "osd.h"

class QDateTime;
class OSD;
class RemoteEncoder;
class MythPlayer;
class DetectLetterbox;
class RingBuffer;
class ProgramInfo;
class MythDialog;
class UDPNotify;
class OSDListTreeType;
class OSDGenericTree;
class PlayerContext;
class UDPNotifyOSDSet;
class TvPlayWindow;
class TV;
class OSDListTreeItemEnteredEvent;
class OSDListTreeItemSelectedEvent;
class TVBrowseHelper;
struct osdInfo;

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
//            -> channelGroupLock
//
// When holding one of these locks, you may lock any lock of  the locks to
// the right of the current lock, but may not lock any lock to the left of
// this lock (which will cause a deadlock). Nor should you lock any other
// lock in the TV class without first adding it to the locking order list
// above.
//
// Note: Taking a middle lock such as askAllowLock, without taking a
// playerLock first does not violate these rules, but once you are
// holding it, you cannot later lock playerLock.
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

class MPUBLIC TV : public QObject
{
    friend class PlaybackBox;
    friend class GuideGrid;
    friend class TvPlayWindow;
    friend class TVBrowseHelper;

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

    void InitFromDB(void);
    bool Init(bool createWindow = true);

    // User input processing commands
    bool ProcessKeypress(PlayerContext*, QKeyEvent *e);
    void ProcessNetworkControlCommand(PlayerContext *, const QString &command);
    void customEvent(QEvent *e);
    bool event(QEvent *e);
    bool HandleTrackAction(PlayerContext*, const QString &action);

    // LiveTV commands
    bool LiveTV(bool showDialogs = true);
    bool StartLiveTVInGuide(void) { return db_start_in_guide; }

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
    bool PromptRecGroupPassword(PlayerContext*);

    bool CreatePBP(PlayerContext *lctx, const ProgramInfo *info);
    bool CreatePIP(PlayerContext *lctx, const ProgramInfo *info);
    bool ResizePIPWindow(PlayerContext*);

    // Boolean queries
    /// Returns true if we are currently in the process of switching recorders.
    bool IsSwitchingCards(void)  const { return switchToRec; }
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
    void ReturnOSDLock(const PlayerContext*,OSD*&);

    bool ActiveHandleAction(PlayerContext*,
                            const QStringList &actions,
                            bool isDVD, bool isDVDStillFrame);

    // static functions
    static void InitKeys(void);
    static void ResetKeys(void);
    static bool StartTV(ProgramInfo *tvrec = NULL,
                        uint flags = kStartTVNoFlags);
    static void SetFuncPtr(const char *, void *);

    // Used by EPG
    void ChangeVolume(PlayerContext*, bool up);
    void ToggleMute(PlayerContext*, const bool muteIndividualChannels = false);

    void SetNextProgPIPState(PIPState state) { jumpToProgramPIPState = state; }

    // Used for UDPNotify
    bool HasUDPNotifyEvent(void) const;
    void HandleUDPNotifyEvent(void);

    // Channel Groups
    void UpdateChannelList(int groupID);

  public slots:
    void HandleOSDClosed(int osdType);
    void timerEvent(QTimerEvent*);

  protected slots:
    void AddUDPNotifyEvent(const QString &name, const UDPNotifyOSDSet*);
    void ClearUDPNotifyEvents(void);

  protected:
    void OSDDialogEvent(int result, QString text, QString action);

    void DoEditSchedule(int editType = kScheduleProgramGuide);

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
    void PlaybackLoop(void);
    bool ContextIsPaused(PlayerContext *ctx, const char *file, int location);
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

    void ToggleChannelFavorite(PlayerContext *ctx);
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
    bool DoPlayerSeek(PlayerContext*, float time);
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
    int  GetNumTitles(const PlayerContext *ctx) const;
    int  GetCurrentTitle(const PlayerContext *ctx) const;
    int  GetTitleDuration(const PlayerContext *ctx, int title) const;
    QString GetTitleName(const PlayerContext *ctx, int title) const;
    void DoSwitchTitle(PlayerContext*, int title);
    int  GetNumAngles(const PlayerContext *ctx) const;
    int  GetCurrentAngle(const PlayerContext *ctx) const;
    QString GetAngleName(const PlayerContext *ctx, int angle) const;
    void DoSwitchAngle(PlayerContext*, int angle);
    void DoJumpChapter(PlayerContext*, int direction);
    void DoSkipCommercials(PlayerContext*, int direction);

    void DoQueueTranscode(PlayerContext*, QString profile);

    void SetAutoCommercialSkip(const PlayerContext*,
                               CommSkipMode skipMode = kCommSkipOff);

    // Manual zoom mode
    void SetManualZoom(const PlayerContext *, bool enabled, QString msg);
    bool ManualZoomHandleAction(PlayerContext *actx,
                                const QStringList &actions);

    bool ClearOSD(const PlayerContext*);
    void ToggleOSD(PlayerContext*, bool includeStatusOSD);
    void UpdateOSDProgInfo(const PlayerContext*, const char *whichInfo);
    void UpdateOSDStatus(const PlayerContext *ctx, QString title, QString desc,
                         QString value, int type, QString units,
                         int position = 0, int prev = 0, int next = 0,
                         enum OSDTimeout timeout = kOSDTimeout_Med);
    void UpdateOSDStatus(const PlayerContext *ctx, osdInfo &info,
                         int type, enum OSDTimeout timeout);

    void UpdateOSDSeekMessage(const PlayerContext*,
                              const QString &mesg, enum OSDTimeout timeout);
    void UpdateOSDInput(const PlayerContext*,
                        QString inputname = QString::null);
    void UpdateOSDSignal(const PlayerContext*, const QStringList &strlist);
    void UpdateOSDTimeoutMessage(PlayerContext*);
    void UpdateOSDAskAllowDialog(PlayerContext*);

    void EditSchedule(const PlayerContext*,
                      int editType = kScheduleProgramGuide);

    void TeardownPlayer(PlayerContext *mctx, PlayerContext *ctx);

    void HandleStateChange(PlayerContext *mctx, PlayerContext *ctx);

    bool StartPlayer(PlayerContext *mctx, PlayerContext *ctx,
                     TVState desiredState);

    vector<long long> TeardownAllPlayers(PlayerContext*);
    void RestartAllPlayers(PlayerContext *lctx,
                           const vector<long long> &pos,
                           MuteState mctx_mute);
    void RestartMainPlayer(PlayerContext *mctx);

    void PxPToggleView(  PlayerContext *actx, bool wantPBP);
    void PxPCreateView(  PlayerContext *actx, bool wantPBP);
    void PxPTeardownView(PlayerContext *actx);
    void PxPToggleType(  PlayerContext *mctx, bool wantPBP);
    void PxPSwap(        PlayerContext *mctx, PlayerContext *pipctx);
    bool HandlePxPTimerEvent(void);

    bool PIPAddPlayer(   PlayerContext *mctx, PlayerContext *ctx);
    bool PIPRemovePlayer(PlayerContext *mctx, PlayerContext *ctx);
    void PBPRestartMainPlayer(PlayerContext *mctx);

    void ToggleAutoExpire(PlayerContext*);

    bool BrowseHandleAction(PlayerContext*, const QStringList &actions);

    void ToggleRecord(PlayerContext*);

    void DoTogglePictureAttribute(const PlayerContext*,
                                  PictureAdjustType type);
    void DoChangePictureAttribute(
        PlayerContext*,
        PictureAdjustType type, PictureAttribute attr, bool up);
    bool PictureAttributeHandleAction(PlayerContext*,
                                      const QStringList &actions);

    // Channel editing support
    void StartChannelEditMode(PlayerContext*);
    bool HandleOSDChannelEdit(PlayerContext*, QString action);
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

    // General dialog handling
    bool DialogIsVisible(PlayerContext *ctx, const QString &dialog);
    void HandleOSDInfo(PlayerContext *ctx, QString action);

    // AskAllow dialog handling
    void ShowOSDAskAllow(PlayerContext *ctx);
    void HandleOSDAskAllow(PlayerContext *ctx, QString action);

    // Program editing support
    void ShowOSDCutpoint(PlayerContext *ctx, const QString &type);
    bool HandleOSDCutpoint(PlayerContext *ctx, QString action, long long frame);
    void StartProgramEditMode(PlayerContext*);

    // Already editing dialog
    void ShowOSDAlreadyEditing(PlayerContext *ctx);
    void HandleOSDAlreadyEditing(PlayerContext *ctx, QString action,
                                 bool was_paused);

    // Sleep dialog handling
    void ShowOSDSleep(void);
    void HandleOSDSleep(PlayerContext *ctx, QString action);
    void SleepDialogTimeout(void);

    // Idle dialog handling
    void ShowOSDIdle(void);
    void HandleOSDIdle(PlayerContext *ctx, QString action);
    void IdleDialogTimeout(void);

    // Exit/delete dialog handling
    void ShowOSDStopWatchingRecording(PlayerContext *ctx);
    void ShowOSDPromptDeleteRecording(PlayerContext *ctx, QString title,
                                      bool force = false);
    bool HandleOSDVideoExit(PlayerContext *ctx, QString action);

    // Menu dialog
    void ShowOSDMenu(const PlayerContext*, const QString category = "",
                     const QString selected = "");

    void FillOSDMenuAudio    (const PlayerContext *ctx, OSD *osd,
                              QString category, const QString selected,
                              QString &currenttext, QString &backaction);
    void FillOSDMenuVideo    (const PlayerContext *ctx, OSD *osd,
                              QString category, const QString selected,
                              QString &currenttext, QString &backaction);
    void FillOSDMenuSubtitles(const PlayerContext *ctx, OSD *osd,
                              QString category, const QString selected,
                              QString &currenttext, QString &backaction);
    void FillOSDMenuNavigate (const PlayerContext *ctx, OSD *osd,
                              QString category, const QString selected,
                              QString &currenttext, QString &backaction);
    void FillOSDMenuJobs     (const PlayerContext *ctx, OSD *osd,
                              QString category, const QString selected,
                              QString &currenttext, QString &backaction);
    void FillOSDMenuPlayback (const PlayerContext *ctx, OSD *osd,
                              QString category, const QString selected,
                              QString &currenttext, QString &backaction);
    void FillOSDMenuSchedule (const PlayerContext *ctx, OSD *osd,
                              QString category, const QString selected,
                              QString &currenttext, QString &backaction);
    void FillOSDMenuSource   (const PlayerContext *ctx, OSD *osd,
                              QString category, const QString selected,
                              QString &currenttext, QString &backaction);
    void FillOSDMenuJumpRec  (PlayerContext* ctx, const QString category = "",
                              int level = 0, const QString selected = "");
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

    // Program jumping stuff
    void SetLastProgram(const ProgramInfo *rcinfo);
    ProgramInfo *GetLastProgram(void) const;

    static bool LoadExternalSubtitles(MythPlayer *player,
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

    // for temp debugging only..
    int find_player_index(const PlayerContext*) const;

  private:
    // Configuration variables from database
    QString baseFilters;
    QString db_channel_format;
    uint    db_idle_timeout;
    uint    db_udpnotify_port;
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
    bool    db_use_channel_groups;
    bool    db_remember_last_channel_group;
    ChannelGroupList db_channel_groups;

    bool    arrowAccel;

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

    QMutex         initFromDBLock;
    bool           initFromDBDone;
    QWaitCondition initFromDBWait;

    /// True if the user told MythTV to stop plaback. If this is false
    /// when we exit the player, we display an error screen.
    mutable bool wantsToQuit;
    bool stretchAdjustment; ///< True if time stretch is turned on
    bool audiosyncAdjustment; ///< True if audiosync is turned on
    int64_t audiosyncBaseline;
    bool editmode;          ///< Are we in video editing mode
    bool zoomMode;
    bool sigMonMode;     ///< Are we in signal monitoring mode?
    bool endOfRecording; ///< !player->IsPlaying() && StateIsPlaying()
    bool requestDelete;  ///< User wants last video deleted
    bool allowRerecord;  ///< User wants to rerecord the last video if deleted
    bool doSmartForward;
    bool queuedTranscode;
    /// Picture attribute type to modify.
    PictureAdjustType adjustingPicture;
    /// Picture attribute to modify (on arrow left or right)
    PictureAttribute  adjustingPictureAttribute;

    // Ask Allow state
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
    TVBrowseHelper *browsehelper;

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
    QMap<OSD*,const PlayerContext*> osd_lctx;

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
    /// true if this instance disabled MythUI drawing.
    bool  weDisabledGUI;
    /// true if video chromakey and frame should not be drawn
    bool  disableDrawUnusedRects;

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
    /// \brief Lock necessary when modifying channel group variables.
    /// These are only modified in UI thread, so no lock is needed
    /// to read this value in the UI thread.
    mutable QMutex channelGroupLock;
    volatile int   channelGroupId;
    DBChanList     channelGroupChannelList;

    // Network Control stuff
    MythDeque<QString> networkControlCommands;

    // Timers
    typedef QMap<int,PlayerContext*>       TimerContextMap;
    typedef QMap<int,const PlayerContext*> TimerContextConstMap;
    mutable QMutex       timerIdLock;
    volatile int         lcdTimerId;
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
