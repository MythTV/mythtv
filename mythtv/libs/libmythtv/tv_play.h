// -*- Mode: c++ -*-

#ifndef TVPLAY_H
#define TVPLAY_H

// C++
#include <cstdint>
#include <utility>
#include <vector>

// Qt
#include <QReadWriteLock>
#include <QWaitCondition>
#include <QStringList>
#include <QDateTime>
#include <QObject>
#include <QString>
#include <QMutex>
#include <QMap>
#include <QRecursiveMutex>
#include <QVector>

// MythTV
#include "libmyth/audio/volumebase.h"
#include "libmythbase/mythdeque.h"
#include "libmythbase/mythtimer.h"
#include "libmythbase/referencecounter.h"
#include "libmythbase/mthreadpool.h"

#include "channelgroup.h"
#include "channelinfo.h"
#include "decoders/decoderbase.h"
#include "inputinfo.h"
#include "mythtvmenu.h"
#include "osd.h"
#include "playercontext.h"
#include "tv.h"
#include "tvbrowsehelper.h"
#include "tvplaybackstate.h"
#include "videoouttypes.h"

class QEvent;
class QKeyEvent;
class QTimerEvent;
class QDateTime;
class QDomDocument;
class QDomNode;
class OSD;
class RemoteEncoder;
class MythPlayer;
class MythPlayerUI;
class MythMediaBuffer;
class ProgramInfo;
class TvPlayWindow;
class TV;
class MythMainWindow;
struct osdInfo;

using EMBEDRETURNVOID        = void (*) (void *, bool);
using EMBEDRETURNVOIDEPG     = void (*) (uint, const QString &, const QDateTime, TV *, bool, bool, int);
using EMBEDRETURNVOIDFINDER  = void (*) (TV *, bool, bool);
using EMBEDRETURNVOIDPROGLIST = void (*) (TV *, int, const QString &);
using EMBEDRETURNVOIDSCHEDIT = void (*) (const ProgramInfo *, void *);

// Locking order
//
// playerLock -> askAllowLock    -> osdLock
//            -> progListLock    -> osdLock
//            -> chanEditMapLock -> osdLock
//            -> lastProgramLock
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

enum scheduleEditTypes {
    kScheduleProgramGuide = 0,
    kScheduleProgramFinder,
    kScheduledRecording,
    kViewSchedule,
    kPlaybackBox,
    kScheduleProgramList
};

/**
 * Type of message displayed in ShowNoRecorderDialog()
 */
enum NoRecorderMsg
{
    kNoRecorders = 0,  ///< No free recorders
    kNoCurrRec = 1,    ///< No current recordings
    kNoTuners = 2,     ///< No capture cards configured
};

enum {
    kStartTVNoFlags           = 0x00,
    kStartTVInPlayList        = 0x02,
    kStartTVByNetworkCommand  = 0x04,
    kStartTVIgnoreBookmark    = 0x08,
    kStartTVIgnoreProgStart   = 0x10,
    kStartTVIgnoreLastPlayPos = 0x20,
};

class AskProgramInfo
{
  public:
    AskProgramInfo() = default;
    AskProgramInfo(QDateTime e, bool r, bool l, ProgramInfo *i) :
        m_expiry(std::move(e)), m_hasRec(r), m_hasLater(l), m_info(i) {}

    QDateTime    m_expiry;
    bool         m_hasRec             {false};
    bool         m_hasLater           {false};
    bool         m_isInSameInputGroup {false};
    bool         m_isConflicting      {false};
    ProgramInfo *m_info               {nullptr};
};

/**
 * \class TV
 *
 * \brief Control TV playback
 *
 * \qmlsignal TVPlaybackAborted()
 * TV playback failed to start (typically, TV playback was started when another playback is currently going)
 * \qmlsignal TVPlaybackStarted()
 * TV playback has started, video is now playing
 * \qmlsignal TVPlaybackStopped()
 * TV playback has stopped and playback has exited
 * \qmlsignal TVPlaybackUnpaused()
 * TV playback has resumed, following a Pause action
 * \qmlsignal TVPlaybackPaused()
 * TV playback has been paused
 * \qmlsignal TVPlaybackSought(qint position_seconds)
 * Absolute seek has completed to position_seconds
 */
class MTV_PUBLIC TV : public TVPlaybackState, public MythTVMenuItemDisplayer, public ReferenceCounter, protected TVBrowseHelper
{
    friend class PlaybackBox;
    friend class GuideGrid;
    friend class TVBrowseHelper;

    using string_pair = QPair<QString, QString>;

    Q_OBJECT

  public:
    static bool IsTVRunning();
    static bool StartTV(ProgramInfo* TVRec, uint Flags, const ChannelInfoList& Selection = ChannelInfoList());
    static bool IsPaused();
    static void InitKeys();
    static void SetFuncPtr(const char* Name, void* Pointer);
    static int  ConfiguredTunerCards();
    static bool IsTunable(uint ChanId);
    void        ReloadKeys();
    MThreadPool* GetPosThreadPool();

    bool IsSameProgram(const ProgramInfo* ProgInfo) const;

  public slots:
    bool event(QEvent* Event) override;
    bool eventFilter(QObject* Object, QEvent* Event) override;
    void timerEvent(QTimerEvent* Event) override;
    void StopPlayback();
    void HandleOSDClosed(int OSDType);

  signals:
    void PlaybackExiting(TV* Player);

  protected slots:
    void onApplicationStateChange(Qt::ApplicationState State);
    void customEvent(QEvent* Event) override;
    void VolumeChange(bool Up, int NewVolume = -1);

  private slots:
    void Embed(bool Embed, QRect Rect = {}, const QStringList& Data = {});

  private:
    static inline QStringList lastProgramStringList = {};
    static inline EMBEDRETURNVOID RunPlaybackBoxPtr = nullptr;
    static inline EMBEDRETURNVOID RunViewScheduledPtr = nullptr;
    static inline EMBEDRETURNVOIDEPG RunProgramGuidePtr = nullptr;
    static inline EMBEDRETURNVOIDFINDER RunProgramFinderPtr = nullptr;
    static inline EMBEDRETURNVOIDSCHEDIT RunScheduleEditorPtr = nullptr;
    static inline EMBEDRETURNVOIDPROGLIST RunProgramListPtr = nullptr;

    explicit TV(MythMainWindow* MainWindow);
   ~TV() override;
    PlayerContext*  GetPlayerContext();
    bool CreatePlayer(TVState State, bool Muted = false);
    bool StartPlaying(std::chrono::milliseconds MaxWait = -1ms);

    // Private initialisation
    static TV* AcquireRelease(int& RefCount, bool Acquire, bool Create = false);
    bool Init();
    void InitFromDB();
    static QList<QKeyEvent*> ConvertScreenPressKeyMap(const QString& KeyList);

    // Top level playback methods
    bool LiveTV(bool ShowDialogs, const ChannelInfoList &Selection);
    int  Playback(const ProgramInfo &ProgInfo);
    void PlaybackLoop();

    // Private event handling
    bool ProcessKeypressOrGesture(QEvent* Event);
    bool TranslateKeyPressOrGesture(const QString& Context, QEvent* Event,
                                    QStringList& Actions, bool IsLiveTV,
                                    bool AllowJumps = true);
    bool TranslateGesture(const QString &Context, MythGestureEvent *Event,
                          QStringList& Actions, bool IsLiveTV);
    void ProcessNetworkControlCommand(const QString& Command);

    bool HandleTrackAction(const QString& Action);
    bool ActiveHandleAction(const QStringList& Actions, bool IsDVD, bool IsDVDStillFrame);
    bool BrowseHandleAction(const QStringList& Actions);
    void OSDDialogEvent(int Result, const QString& Text, QString Action);
    bool ToggleHandleAction(const QStringList& Actions, bool IsDVD);
    bool FFRewHandleAction(const QStringList& Actions);
    bool ActivePostQHandleAction(const QStringList& Actions);
    bool HandleJumpToProgramAction(const QStringList& Actions);
    bool SeekHandleAction(const QStringList& Actions, bool IsDVD);
    bool TimeStretchHandleAction(const QStringList& Actions);
    bool DiscMenuHandleAction(const QStringList& Actions) const;

    // Timers and timer events
    int  StartTimer(std::chrono::milliseconds Interval, int Line);
    void KillTimer(int Id);

    void SetSpeedChangeTimer(std::chrono::milliseconds When, int Line);
    void HandleEndOfPlaybackTimerEvent();
    void HandleEndOfRecordingExitPromptTimerEvent();
    void HandleVideoExitDialogTimerEvent();
    void HandlePseudoLiveTVTimerEvent();
    void HandleSpeedChangeTimerEvent();
    void ToggleSleepTimer();
    void ToggleSleepTimer(const QString& Time);
    bool HandleLCDTimerEvent();
    void HandleLCDVolumeTimerEvent();
    void HandleSaveLastPlayPosEvent();

    // Commands used by frontend UI screens (PlaybackBox, GuideGrid etc)
    void EditSchedule(int EditType = kScheduleProgramGuide,
                      const QString& arg = "");
    bool IsTunablePriv(uint ChanId);
    static QVector<uint> IsTunableOn(PlayerContext* Context, uint ChanId);
    void ChangeChannel(const ChannelInfoList& Options);
    void DoEditSchedule(int EditType = kScheduleProgramGuide,
                        const QString & EditArg = "");
    QString GetRecordingGroup() const;
    void UpdateChannelList(int GroupID);

    // Lock handling
    OSD* GetOSDL();
    void ReturnOSDLock() const;
    void GetPlayerWriteLock() const;
    void GetPlayerReadLock() const;
    void ReturnPlayerLock() const;

    // Other toggles
    void ToggleAutoExpire();
    void QuickRecord();

    // General TV state
    static bool StateIsRecording(TVState State);
    static bool StateIsPlaying(TVState State);
    static bool StateIsLiveTV(TVState State);

    TVState GetState() const;
    void HandleStateChange();
    void GetStatus();
    void ForceNextStateNone();
    void ScheduleStateChange();
    void ScheduleInputChange();
    void SetErrored();
    void SetInPlayList(bool InPlayList) { m_inPlaylist = InPlayList; }
    void setUnderNetworkControl(bool setting) { m_underNetworkControl = setting; }
    void PrepToSwitchToRecordedProgram(const ProgramInfo& ProgInfo);
    enum BookmarkAction {
        kBookmarkAlways,
        kBookmarkNever,
        kBookmarkAuto // set iff db_playback_exit_prompt==2
    };
    void PrepareToExitPlayer(int Line);
    void SetExitPlayer(bool SetIt, bool WantsTo);

    bool RequestNextRecorder(bool ShowDialogs, const ChannelInfoList &Selection = ChannelInfoList());
    void DeleteRecorder();

    bool StartRecorder(std::chrono::milliseconds MaxWait = -1ms);
    void StopStuff(bool StopRingBuffer, bool StopPlayer, bool StopRecorder);
    bool StartPlayer(TVState desiredState);

    /// Returns true if we are currently in the process of switching recorders.
    bool IsSwitchingCards()  const { return m_switchToRec; }
    /// Returns true if the user told Mythtv to allow re-recording of the show
    bool GetAllowRerecord() const { return m_allowRerecord;  }
    /// This is set to true if the player reaches the end of the
    /// recording without the user explicitly exiting the player.
    bool GetEndOfRecording() const { return m_endOfRecording; }
    /// This is set if the user asked MythTV to jump to the previous
    /// recording in the playlist.
    bool GetJumpToProgram()  const { return m_jumpToProgram; }
    bool IsDeleteAllowed();

    // Channels
    static void ToggleChannelFavorite();
    void ToggleChannelFavorite(const QString &ChangroupName) const;
    void ChangeChannel(ChannelChangeDirection Direction);
    void ChangeChannel(uint Chanid, const QString& Channum);

    void ShowPreviousChannel();
    void PopPreviousChannel(bool ImmediateChange);

    // key queue commands
    void AddKeyToInputQueue(char Key);
    void ClearInputQueues(bool Hideosd);
    bool CommitQueuedInput();
    bool ProcessSmartChannel(QString &InputStr);

    // query key queues
    bool HasQueuedInput() const { return !GetQueuedInput().isEmpty(); }
    bool HasQueuedChannel() const { return m_queuedChanID || !GetQueuedChanNum().isEmpty(); }

    // get queued up input
    QString GetQueuedInput()   const;
    int     GetQueuedInputAsInt(bool *OK = nullptr, int Base = 10) const;
    QString GetQueuedChanNum() const;
    uint    GetQueuedChanID()  const { return m_queuedChanID; }

    // Source and input
    void SwitchSource(uint Direction);
    void SwitchInputs(uint ChanID = 0, QString ChanNum = "", uint InputID = 0);

    // Pause/play
    void PauseLiveTV();
    void UnpauseLiveTV(bool Quietly = false);
    void DoPlay();
    float DoTogglePauseStart();
    void DoTogglePauseFinish(float Time, bool ShowOSD);
    void DoTogglePause(bool ShowOSD);
    bool DoSetPauseState(bool Pause);
    bool ContextIsPaused(const char* File, int Location);

    // Program jumping stuff
    void SetLastProgram(const ProgramInfo* ProgInfo);
    ProgramInfo *GetLastProgram() const;

    // Seek, skip, jump, speed
    void DoSeek(float Time, const QString &Msg, bool TimeIsOffset, bool HonorCutlist);
    void DoSeek(std::chrono::seconds TimeInSec, const QString &Msg, bool TimeIsOffset, bool HonorCutlist) {
        DoSeek(TimeInSec.count(), Msg, TimeIsOffset, HonorCutlist); };
    bool DoPlayerSeek(float Time);
    bool DoPlayerSeekToFrame(uint64_t FrameNum);
    enum ArbSeekWhence { ARBSEEK_SET = 0, ARBSEEK_REWIND, ARBSEEK_FORWARD, ARBSEEK_END };
    void DoSeekAbsolute(long long Seconds, bool HonorCutlist);
    void DoArbSeek(ArbSeekWhence Whence, bool HonorCutlist);
    void DoJumpFFWD();
    void DoJumpRWND();
    void DoSeekFFWD();
    void DoSeekRWND();
    void NormalSpeed();
    void ChangeSpeed(int Direction);
    void ToggleTimeStretch();
    void ChangeTimeStretch(int Dir, bool AllowEdit = true);
    void DVDJumpBack();
    void DVDJumpForward();
    float StopFFRew();
    void ChangeFFRew(int Direction);
    void SetFFRew(int Index);

    // Private audio methods
    bool AudioSyncHandleAction(const QStringList& Actions);

    // Chapters, titles and angles
    int  GetNumChapters();
    void GetChapterTimes(QList<std::chrono::seconds> &Times);
    int  GetCurrentChapter();
    int  GetNumTitles();
    int  GetCurrentTitle();
    std::chrono::seconds  GetTitleDuration(int Title);
    QString GetTitleName(int Title);
    void DoSwitchTitle(int Title);
    int  GetNumAngles();
    int  GetCurrentAngle();
    QString GetAngleName(int Angle);
    void DoSwitchAngle(int Angle);
    void DoJumpChapter(int Chapter);

    // Commercial skipping
    void DoSkipCommercials(int Direction);
    void SetAutoCommercialSkip(CommSkipMode SkipMode = kCommSkipOff);

    // Transcode
    void DoQueueTranscode(const QString& Profile);

    // Bookmarks
    bool IsBookmarkAllowed();
    void SetBookmark(bool Clear = false);

    // OSD
    void ClearOSD();
    void ToggleOSD( bool IncludeStatusOSD);
    void UpdateOSDProgInfo(const char *WhichInfo);
    void UpdateOSDStatus(const QString& Title, const QString& Desc,
                         const QString& Value, int Type, const QString& Units,
                         int Position = 0, enum OSDTimeout Timeout = kOSDTimeout_Med);
    void UpdateOSDStatus(osdInfo &Info, int Type, enum OSDTimeout Timeout);
    void UpdateOSDSeekMessage(const QString &Msg, enum OSDTimeout Timeout);
    void UpdateOSDInput();
    void UpdateOSDSignal(const QStringList &List);
    void UpdateOSDTimeoutMessage();
    bool CalcPlayerSliderPosition(osdInfo &info, bool paddedFields = false) const;
    void HideOSDWindow(const char *window);

    // Captions/subtitles
    bool SubtitleZoomHandleAction(const QStringList& Actions);
    bool SubtitleDelayHandleAction(const QStringList &Actions);

    // Video controls
    void DoTogglePictureAttribute(PictureAdjustType Type);
    bool PictureAttributeHandleAction(const QStringList &Actions);
    PictureAttribute NextPictureAdjustType(PictureAdjustType Type, PictureAttribute Attr);
    void OverrideScan(FrameScanType Scan);

    // Sundry on screen
    void ITVRestart(bool IsLive);

    // Manual zoom mode
    void SetManualZoom(bool ZoomON, const QString& Desc);
    bool ManualZoomHandleAction(const QStringList &Actions);

    // Channel editing support
    void StartChannelEditMode();
    bool HandleOSDChannelEdit(const QString& Action);
    void ChannelEditAutoFill(InfoMap &Info);
    void ChannelEditXDSFill(InfoMap &Info);

    // General dialog handling
    bool DialogIsVisible(const QString &Dialog);
    void HandleOSDInfo(const QString& Action);
    void ShowNoRecorderDialog(NoRecorderMsg MsgType = kNoRecorders);

    // AskAllow dialog handling
    void ShowOSDAskAllow();
    void HandleOSDAskAllow(const QString& Action);
    void AskAllowRecording(const QStringList &Msg, int Timeuntil, bool HasRec, bool HasLater);

    // Program editing support
    void ShowOSDCutpoint(const QString &Type);
    bool HandleOSDCutpoint(const QString& Action);
    void StartProgramEditMode();

    // Already editing dialog
    void ShowOSDAlreadyEditing();
    void HandleOSDAlreadyEditing(const QString& Action, bool WasPaused);

    // Sleep dialog handling
    void ShowOSDSleep();
    void HandleOSDSleep(const QString& Action);
    void SleepDialogTimeout();

    // Idle dialog handling
    void ShowOSDIdle();
    void HandleOSDIdle(const QString& Action);
    void IdleDialogTimeout();

    // Exit/delete dialog handling
    void ShowOSDStopWatchingRecording();
    void ShowOSDPromptDeleteRecording(const QString& Title, bool Force = false);
    bool HandleOSDVideoExit(const QString& Action);

    // Navigation Dialog
    void StartOsdNavigation();

    // Menu dialog
    void ShowOSDMenu(bool isCompact = false);
    void FillOSDMenuJumpRec(const QString &Category = "", int Level = 0, const QString &Selected = "");
    static void FillOSDMenuCastButton(MythOSDDialogData & dialog,
                               const QVector<string_pair> & people);
    void FillOSDMenuCast(void);
    void FillOSDMenuActorShows(const QString & actor, int person_id,
                               const QString & category = "");
    void PlaybackMenuShow(const MythTVMenu &Menu, const QDomNode &Node, const QDomNode &Selected);
    bool MenuItemDisplay(const MythTVMenuItemContext& Context, MythOSDDialogData* Menu) override;
    bool MenuItemDisplayPlayback(const MythTVMenuItemContext& Context, MythOSDDialogData* Menu);
    bool MenuItemDisplayCutlist(const MythTVMenuItemContext& Context, MythOSDDialogData* Menu);
    void PlaybackMenuInit(const MythTVMenu& Menu);
    void PlaybackMenuDeinit(const MythTVMenu& Menu);
    static void MenuStrings();
    void MenuLazyInit(void* Field);
    const MythTVMenu& getMenuFromId(MenuTypeId id);

    // LCD
    void UpdateLCD();
    void ShowLCDChannelInfo();
    void ShowLCDDVDInfo();

  private:
    void RetrieveCast(const ProgramInfo& ProgInfo);

    MythMainWindow*   m_mainWindow { nullptr };
    MThreadPool*      m_posThreadPool { nullptr };

    // Configuration variables from database
    QString           m_dbChannelFormat {"<num> <sign>"};
    std::chrono::milliseconds m_dbIdleTimeout {0ms};
    int               m_dbPlaybackExitPrompt {0};
    uint              m_dbAutoexpireDefault {0};
    bool              m_dbAutoSetWatched {false};
    bool              m_dbEndOfRecExitPrompt {false};
    bool              m_dbJumpPreferOsd {true};
    bool              m_dbUseGuiSizeForTv {false};
    bool              m_dbUseVideoModes {false};
    bool              m_dbRunJobsOnRemote {false};
    bool              m_dbContinueEmbedded {false};
    bool              m_dbBrowseAlways {false};
    bool              m_dbBrowseAllTuners {false};
    bool              m_dbUseChannelGroups {false};
    bool              m_dbRememberLastChannelGroup {false};
    ChannelGroupList  m_dbChannelGroups;

    bool              m_smartForward {false};
    float             m_ffRewRepos {1.0F};
    bool              m_ffRewReverse {false};
    std::vector<int>  m_ffRewSpeeds;

    uint              m_vbimode {VBIMode::None};
    uint              m_switchToInputId {0};

    /// True if the user told MythTV to stop playback. If this is false
    /// when we exit the player, we display an error screen.
    mutable bool      m_wantsToQuit {true};
    bool              m_stretchAdjustment {false}; ///< True if time stretch is turned on
    bool              m_audiosyncAdjustment {false}; ///< True if audiosync is turned on
    bool              m_subtitleZoomAdjustment {false}; ///< True if subtitle zoom is turned on
    bool              m_subtitleDelayAdjustment {false}; ///< True if subtitle delay is turned on
    bool              m_zoomMode {false};
    bool              m_sigMonMode {false};     ///< Are we in signal monitoring mode?
    bool              m_endOfRecording {false}; ///< !player->IsPlaying() && StateIsPlaying()
    bool              m_requestDelete {false};  ///< User wants last video deleted
    bool              m_allowRerecord {false};  ///< User wants to rerecord the last video if deleted
    bool              m_doSmartForward {false};
    bool              m_queuedTranscode {false};
    bool              m_savePosOnExit {false};  ///< False until first timer event
    bool              m_clearPosOnExit {false}; ///< False unless requested by user on playback exit
    /// Picture attribute type to modify.
    PictureAdjustType m_adjustingPicture {kAdjustingPicture_None};
    /// Picture attribute to modify (on arrow left or right)
    PictureAttribute  m_adjustingPictureAttribute {kPictureAttribute_None};

    // Ask Allow state
    QMap<QString,AskProgramInfo> m_askAllowPrograms;
    QRecursiveMutex              m_askAllowLock;

    QMutex                    m_progListsLock;
    QMap<QString,ProgramList> m_progLists;

    QVector<string_pair> m_actors;
    QVector<string_pair> m_guest_stars;
    QVector<string_pair> m_guests;

    mutable QRecursiveMutex m_chanEditMapLock; ///< Lock for chanEditMap and ddMap
    InfoMap        m_chanEditMap;          ///< Channel Editing initial map

    class SleepTimerInfo;
    static const std::vector<SleepTimerInfo> s_sleepTimes;
    uint                   m_sleepIndex {0};          ///< Index into sleep_times.
    std::chrono::milliseconds m_sleepTimerTimeout {0ms};   ///< Current sleep timeout in msec
    int                    m_sleepTimerId {0};        ///< Timer for turning off playback.
    int                    m_sleepDialogTimerId {0};  ///< Timer for sleep dialog.
    /// Timer for turning off playback after idle period.
    int                    m_idleTimerId {0};
    int                    m_idleDialogTimerId {0}; ///< Timer for idle dialog.

    /// Queue of unprocessed key presses.
    MythTimer              m_keyRepeatTimer; ///< Timeout timer for repeat key filtering

    // CC/Teletex input state variables
    /// Are we in CC/Teletext page/stream selection mode?
    bool                   m_ccInputMode {false};

    // Arbitrary Seek input state variables
    /// Are we in Arbitrary seek input mode?
    bool                   m_asInputMode {false};

    // Channel changing state variables
    /// Input key presses queued up so far...
    QString                m_queuedInput;
    /// Input key presses queued up so far to form a valid ChanNum
    mutable QString        m_queuedChanNum;
    /// Queued ChanID (from EPG channel selector)
    uint                   m_queuedChanID {0};
    /// Initial chanid override for Live TV
    uint                   m_initialChanID {0};

    /// screen area to keypress translation
    /// region is now 0..11
    ///  0  1   2  3
    ///  4  5   6  7
    ///  8  9  10 11
    static const int kScreenPressRegionCount = 12;
    QList<QKeyEvent*> m_screenPressKeyMapPlayback;
    QList<QKeyEvent*> m_screenPressKeyMapLiveTV;

    // Channel changing timeout notification variables
    QElapsedTimer m_lockTimer;
    bool      m_lockTimerOn {false};
    QDateTime m_lastLockSeenTime;

    // Program Info for currently playing video
    // (or next video if InChangeState() is true)
    mutable QMutex  m_lastProgramLock;
    ProgramInfo    *m_lastProgram {nullptr};   ///< last program played with this player
    bool            m_inPlaylist {false}; ///< show is part of a playlist
    bool            m_underNetworkControl {false}; ///< initial show started via by the network control interface

    // Program Jumping
    bool         m_jumpToProgram {false};

    // Video Player
    PlayerContext           m_playerContext { kPlayerInUseID };
    // Ugly temporary workaround. We need a full MythPlayerInterface object but
    // avoid endless casts
    MythPlayerUI*    m_player        { nullptr };
    /// lock on player and playerActive changes
    mutable QReadWriteLock  m_playerLock;

    bool                    m_noHardwareDecoders {false};

    // Remote Encoders
    /// Main recorder to use after a successful SwitchCards() call.
    RemoteEncoder          *m_switchToRec {nullptr};

    // LCD Info
    QString   m_lcdTitle;
    QString   m_lcdSubtitle;
    QString   m_lcdCallsign;

    // Window info (GUI is optional, transcoding, preview img, etc)
    TvPlayWindow *m_myWindow {nullptr};   ///< Our screen, if it exists
    ///< player bounds, for after DoEditSchedule() returns to normal playing.
    QRect         m_playerBounds;
    ///< Prior GUI window bounds, for DoEditSchedule() and player exit().
    QRect         m_savedGuiBounds;
    /// true if this instance disabled MythUI drawing.
    bool          m_weDisabledGUI {false};

    // embedded and suspended status
    bool         m_ignoreKeyPresses {false}; ///< should we ignore keypresses
    bool         m_savedPause       {false}; ///< saved pause state before embedding

    // Channel group stuff
    /// \brief Lock necessary when modifying channel group variables.
    /// These are only modified in UI thread, so no lock is needed
    /// to read this value in the UI thread.
    mutable QMutex      m_channelGroupLock;
    volatile int        m_channelGroupId {-1};
    ChannelInfoList     m_channelGroupChannelList;

    // Network Control stuff
    MythDeque<QString> m_networkControlCommands;

    // Timers
    volatile int         m_lcdTimerId              {0};
    volatile int         m_lcdVolumeTimerId        {0};
    volatile int         m_networkControlTimerId   {0};
    volatile int         m_ccInputTimerId          {0};
    volatile int         m_asInputTimerId          {0};
    volatile int         m_queueInputTimerId       {0};
    volatile int         m_endOfPlaybackTimerId    {0};
    volatile int         m_endOfRecPromptTimerId   {0};
    volatile int         m_videoExitDialogTimerId  {0};
    volatile int         m_pseudoChangeChanTimerId {0};
    volatile int         m_speedChangeTimerId      {0};
    volatile int         m_errorRecoveryTimerId    {0};
    mutable volatile int m_exitPlayerTimerId       {0};
    volatile int         m_saveLastPlayPosTimerId  {0};
    volatile int         m_signalMonitorTimerId    {0};

    // Various tracks
    // XXX This ignores kTrackTypeTextSubtitle which is greater than
    // kTrackTypeCount, and it unnecessarily includes
    // kTrackTypeUnknown.
    QStringList m_tvmTracks[kTrackTypeCount];
    int         m_tvmCurtrack[kTrackTypeCount] {};

    // Audio
    bool    m_tvmAvsync {true};

    // Video
    bool             m_tvmFillAutoDetect      {false};

    // Playback
    int          m_tvmSpeedX100         {100};
    TVState      m_tvmState             {kState_None};
    bool         m_tvmIsRecording       {false};
    bool         m_tvmIsRecorded        {false};
    bool         m_tvmIsVideo           {false};
    CommSkipMode m_tvmCurSkip           {kCommSkipOff};
    bool         m_tvmIsPaused          {false};
    int          m_tvmFreeRecorderCount {-1};
    bool         m_tvmIsDvd             {false};
    bool         m_tvmIsBd              {false};
    bool         m_tvmJump              {false};
    bool         m_tvmIsLiveTv          {false};
    bool         m_tvmPreviousChan      {false};

    // Navigate
    int              m_tvmNumChapters {0};
    int              m_tvmCurrentChapter {0};
    QList<std::chrono::seconds> m_tvmChapterTimes;
    int              m_tvmNumAngles {0};
    int              m_tvmCurrentAngle {0};
    int              m_tvmNumTitles {0};
    int              m_tvmCurrentTitle {0};
    // Subtitles
    bool             m_tvmSubsForcedOn {true};
    bool             m_tvmSubsHaveSubs {false};

    bool             m_tvmIsOn {false};
    bool             m_tvmTranscoding {false};

    QVariant         m_tvmJumprecBackHack;
    // End of playback menu state caching

    MythTVMenu m_playbackMenu;
    MythTVMenu m_playbackCompactMenu;
    MythTVMenu m_cutlistMenu;
    MythTVMenu m_cutlistCompactMenu;

  public:
    static inline const int kInitFFRWSpeed   = 0; // 1x, default to normal speed
    static inline const uint kInputKeysMax   = 6; // When to start discarding early keys
    static inline const uint kNextSource     = 1;
    static inline const uint kPreviousSource = 2;

    static inline const std::chrono::milliseconds kInputModeTimeout        = 5s;
    static inline const std::chrono::milliseconds kLCDTimeout              = 1s;
    static inline const std::chrono::milliseconds kBrowseTimeout           = 30s;
    static inline const std::chrono::milliseconds kKeyRepeatTimeout        = 300ms;
    static inline const std::chrono::milliseconds kPrevChanTimeout         = 750ms;
    static inline const std::chrono::milliseconds kSleepTimerDialogTimeout = 45s;
    static inline const std::chrono::milliseconds kIdleTimerDialogTimeout  = 45s;
    static inline const std::chrono::milliseconds kVideoExitDialogTimeout  = 2min;
    static inline const std::chrono::milliseconds kEndOfPlaybackCheckFrequency  = 250ms;
    static inline const std::chrono::milliseconds kEmbedCheckFrequency          = 250ms;
    static inline const std::chrono::milliseconds kSpeedChangeCheckFrequency    = 250ms;
    static inline const std::chrono::milliseconds kErrorRecoveryCheckFrequency  = 250ms;
    static inline const std::chrono::milliseconds kEndOfRecPromptCheckFrequency = 250ms;
    static inline const std::chrono::milliseconds kSaveLastPlayPosTimeout       = 5s;
#if CONFIG_VALGRIND
    static inline const std::chrono::milliseconds kEndOfPlaybackFirstCheckTimer = 1min;
#else
    static inline const std::chrono::milliseconds kEndOfPlaybackFirstCheckTimer = 5s;
#endif
};

class SavePositionThread : public QRunnable
{
    public:
        SavePositionThread(ProgramInfo *progInfoPtr, uint64_t framesPos):
            m_progInfo(progInfoPtr),
            m_framesPlayed(framesPos)
        {
        }

        void run() override; // QRunnable

    private:
        ProgramInfo *m_progInfo {nullptr};
        uint64_t     m_framesPlayed {0};
};

#endif
