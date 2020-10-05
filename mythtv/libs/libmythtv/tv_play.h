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
#include <QRegExp>
#include <QString>
#include <QMutex>
#include <QMap>
#include <QSet>

// MythTV
#include "mythdeque.h"
#include "tv.h"
#include "channelinfo.h"
#include "videoouttypes.h"
#include "volumebase.h"
#include "inputinfo.h"
#include "channelgroup.h"
#include "mythtimer.h"
#include "osd.h"
#include "playercontext.h"
#include "decoders/decoderbase.h"
#include "mythmiscutil.h"
#include "tvbrowsehelper.h"
#include "referencecounter.h"

class QEvent;
class QKeyEvent;
class QTimerEvent;
class QDateTime;
class QDomDocument;
class QDomNode;
class OSD;
class RemoteEncoder;
class MythPlayer;
class DetectLetterbox;
class MythMediaBuffer;
class ProgramInfo;
class TvPlayWindow;
class TV;
class MythMainWindow;
struct osdInfo;

using EMBEDRETURNVOID        = void (*) (void *, bool);
using EMBEDRETURNVOIDEPG     = void (*) (uint, const QString &, const QDateTime, TV *, bool, bool, int);
using EMBEDRETURNVOIDFINDER  = void (*) (TV *, bool, bool);
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
    kPlaybackBox
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
    kStartTVNoFlags          = 0x00,
    kStartTVInPlayList       = 0x02,
    kStartTVByNetworkCommand = 0x04,
    kStartTVIgnoreBookmark   = 0x08,
    kStartTVIgnoreProgStart  = 0x10,
    kStartTVAllowLastPlayPos = 0x20,
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

enum MenuCategory {
    kMenuCategoryItem,
    kMenuCategoryItemlist,
    kMenuCategoryMenu
};
enum MenuShowContext {
    kMenuShowActive,
    kMenuShowInactive,
    kMenuShowAlways
};

enum MenuCurrentContext {
    kMenuCurrentDefault,
    kMenuCurrentActive,
    kMenuCurrentAlways
};

class MenuBase;

class MenuItemContext
{
public:
    // Constructor for a menu element.
    MenuItemContext(const MenuBase &menu,
                    const QDomNode &node,
                    QString menuName,
                    MenuCurrentContext current,
                    bool doDisplay) :
        m_menu(menu),
        m_node(node),
        m_category(kMenuCategoryMenu),
        m_menuName(std::move(menuName)),
        m_showContext(kMenuShowAlways),
        m_currentContext(current),
        m_action(""),
        m_actionText(""),
        m_doDisplay(doDisplay) {}
    // Constructor for an item element.
    MenuItemContext(const MenuBase &menu,
                    const QDomNode &node,
                    MenuShowContext showContext,
                    MenuCurrentContext current,
                    QString action,
                    QString actionText,
                    bool doDisplay) :
        m_menu(menu),
        m_node(node),
        m_category(kMenuCategoryItem),
        m_menuName(""),
        m_showContext(showContext),
        m_currentContext(current),
        m_action(std::move(action)),
        m_actionText(std::move(actionText)),
        m_doDisplay(doDisplay) {}
    // Constructor for an itemlist element.
    MenuItemContext(const MenuBase &menu,
                    const QDomNode &node,
                    MenuShowContext showContext,
                    MenuCurrentContext current,
                    QString action,
                    bool doDisplay) :
        m_menu(menu),
        m_node(node),
        m_category(kMenuCategoryItemlist),
        m_menuName(""),
        m_showContext(showContext),
        m_currentContext(current),
        m_action(std::move(action)),
        m_actionText(""),
        m_doDisplay(doDisplay) {}
    const MenuBase    &m_menu;
    const QDomNode    &m_node;
    MenuCategory       m_category;
    const QString      m_menuName;
    MenuShowContext    m_showContext;
    MenuCurrentContext m_currentContext;
    const QString      m_action;
    const QString      m_actionText;
    bool               m_doDisplay;
};

class MenuItemDisplayer
{
  public:
    virtual ~MenuItemDisplayer() = default;
    virtual bool MenuItemDisplay(const MenuItemContext &c) = 0;
};

class MenuBase
{
public:
    MenuBase() = default;
    ~MenuBase();
    bool        LoadFromFile(const QString &filename,
                             const QString &menuname,
                             const char *translationContext,
                             const QString &keyBindingContext);
    bool        LoadFromString(const QString &text,
                               const QString &menuname,
                               const char *translationContext,
                               const QString &keyBindingContext);
    bool        IsLoaded() const { return (m_document != nullptr); }
    QDomElement GetRoot() const;
    QString     Translate(const QString &text) const;
    bool        Show(const QDomNode &node, const QDomNode &selected,
                     MenuItemDisplayer &displayer,
                     bool doDisplay = true) const;
    QString     GetName() const { return m_menuName; }
    const char *GetTranslationContext() const {
        return m_translationContext;
    }
    const QString &GetKeyBindingContext() const {
        return m_keyBindingContext;
    }
private:
    bool LoadFileHelper(const QString &filename,
                        const QString &menuname,
                        const char *translationContext,
                        const QString &keyBindingContext,
                        int includeLevel);
    bool LoadStringHelper(const QString &text,
                          const QString &menuname,
                          const char *translationContext,
                          const QString &keyBindingContext,
                          int includeLevel);
    void ProcessIncludes(QDomElement &root, int includeLevel);
    QDomDocument *m_document           {nullptr};
    const char   *m_translationContext {""};
    QString       m_menuName;
    QString       m_keyBindingContext;
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
class MTV_PUBLIC TV : public QObject, public MenuItemDisplayer, public ReferenceCounter, protected TVBrowseHelper
{
    friend class PlaybackBox;
    friend class GuideGrid;
    friend class TVBrowseHelper;

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

    bool IsSameProgram(const ProgramInfo* ProgInfo) const;

  public slots:
    bool event(QEvent* Event) override;
    bool eventFilter(QObject* Object, QEvent* Event) override;
    void timerEvent(QTimerEvent* Event) override;
    void StopPlayback();

  signals:
    void PlaybackExiting(TV* Player);
    void RequestStartEmbedding(const QRect& EmbedRect);
    void RequestStopEmbedding(const QStringList& Data = {});

  protected slots:
    void onApplicationStateChange(Qt::ApplicationState State);
    void customEvent(QEvent* Event) override;

  private slots:
    bool StartEmbedding(const QRect& EmbedRect);
    void StopEmbedding(const QStringList& Data = {});
    void HandleOSDClosed(int OSDType);

  private:
    static QStringList lastProgramStringList;
    static EMBEDRETURNVOID RunPlaybackBoxPtr;
    static EMBEDRETURNVOID RunViewScheduledPtr;
    static EMBEDRETURNVOIDEPG RunProgramGuidePtr;
    static EMBEDRETURNVOIDFINDER RunProgramFinderPtr;
    static EMBEDRETURNVOIDSCHEDIT RunScheduleEditorPtr;

    explicit TV(MythMainWindow* MainWindow);
   ~TV() override;
    PlayerContext*  GetPlayerContext();

    // Private initialisation
    static TV* AcquireRelease(int& RefCount, bool Acquire, bool Create = false);
    bool Init();
    void InitFromDB();
    static QList<QKeyEvent> ConvertScreenPressKeyMap(const QString& KeyList);

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
    bool Handle3D(const QString& Action);

    // Timers and timer events
    int  StartTimer(int Interval, int Line);
    void KillTimer(int Id);

    void SetSpeedChangeTimer(int When, int Line);
    void HandleEndOfPlaybackTimerEvent();
    void HandleIsNearEndWhenEmbeddingTimerEvent();
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
    void EditSchedule(int EditType = kScheduleProgramGuide);
    bool IsTunablePriv(uint ChanId);
    QSet<uint> IsTunableOn(uint ChanId);
    void ChangeChannel(const ChannelInfoList& Options);
    void DoEditSchedule(int EditType = kScheduleProgramGuide);
    QString GetRecordingGroup() const;
    void ChangeVolume(bool Up, int NewVolume = -1);
    void ToggleMute(bool MuteIndividualChannels = false);
    void UpdateChannelList(int GroupID);

    // Lock handling
    OSD* GetOSDL();
    void ReturnOSDLock();
    void GetPlayerWriteLock();
    void GetPlayerReadLock();
    void GetPlayerReadLock() const;
    void ReturnPlayerLock();
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
    void PrepareToExitPlayer(int Line, BookmarkAction Bookmark = kBookmarkAuto);
    void SetExitPlayer(bool SetIt, bool WantsTo);

    bool RequestNextRecorder(bool ShowDialogs, const ChannelInfoList &Selection = ChannelInfoList());
    void DeleteRecorder();

    bool StartRecorder(int MaxWait = -1);
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
    bool DoSetPauseState(const bool& Pause);
    bool ContextIsPaused(const char* File, int Location);

    // Program jumping stuff
    void SetLastProgram(const ProgramInfo* ProgInfo);
    ProgramInfo *GetLastProgram() const;

    // Seek, skip, jump, speed
    void DoSeek(float Time, const QString &Msg, bool TimeIsOffset, bool HonorCutlist);
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
    void EnableUpmix(bool Enable, bool Toggle = false);
    void ChangeAudioSync(int Dir, int NewSync = -9999);
    bool AudioSyncHandleAction(const QStringList& Actions);
    void PauseAudioUntilBuffered();

    // Chapters, titles and angles
    int  GetNumChapters();
    void GetChapterTimes(QList<long long> &Times);
    int  GetCurrentChapter();
    int  GetNumTitles();
    int  GetCurrentTitle();
    int  GetTitleDuration(int Title);
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
    bool ClearOSD();
    void ToggleOSD( bool IncludeStatusOSD);
    void ToggleOSDDebug();
    void UpdateOSDDebug();
    void UpdateOSDProgInfo(const char *WhichInfo);
    void UpdateOSDStatus(const QString& Title, const QString& Desc,
                         const QString& Value, int Type, const QString& Units,
                         int Position = 0, enum OSDTimeout Timeout = kOSDTimeout_Med);
    void UpdateOSDStatus(osdInfo &Info, int Type, enum OSDTimeout Timeout);
    void UpdateOSDSeekMessage(const QString &Msg, enum OSDTimeout Timeout);
    void UpdateOSDInput();
    void UpdateOSDSignal(const QStringList &List);
    void UpdateOSDTimeoutMessage();
    void SetUpdateOSDPosition(bool Set);

    // Captions/subtitles
    bool SubtitleZoomHandleAction(const QStringList& Actions);
    void ChangeSubtitleZoom(int Dir);
    bool SubtitleDelayHandleAction(const QStringList &Actions);
    void ChangeSubtitleDelay(int Dir);

    // Video controls
    void ToggleMoveBottomLine();
    void SaveBottomLine();
    void ToggleAspectOverride(AspectOverrideMode AspectMode = kAspect_Toggle);
    void ToggleAdjustFill(AdjustFillMode AdjustfillMode = kAdjustFill_Toggle);
    void DoToggleNightMode();
    void DoTogglePictureAttribute(PictureAdjustType Type);
    void DoChangePictureAttribute(PictureAdjustType Type, PictureAttribute Attr, bool Up, int NewValue = -1);
    bool PictureAttributeHandleAction(const QStringList &Actions);
    static PictureAttribute NextPictureAdjustType(PictureAdjustType Type, MythPlayer *Player, PictureAttribute Attr);
    void OverrideScan(FrameScanType Scan);

    // Sundry on screen
    void ITVRestart(bool IsLive);
    void EnableVisualisation(bool Enable, bool Toggle = false, const QString &Action = QString(""));

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
    void UpdateNavDialog();

    // Menu dialog
    void ShowOSDMenu(bool isCompact = false);
    void FillOSDMenuJumpRec(const QString &Category = "", int Level = 0, const QString &Selected = "");
    void PlaybackMenuShow(const MenuBase &Menu, const QDomNode &Node, const QDomNode &Selected);
    bool MenuItemDisplay(const MenuItemContext &Context) override;
    bool MenuItemDisplayPlayback(const MenuItemContext &Context);
    bool MenuItemDisplayCutlist(const MenuItemContext &Context);
    void PlaybackMenuInit(const MenuBase &Menu);
    void PlaybackMenuDeinit(const MenuBase &Menu);
    static void MenuStrings();
    void MenuLazyInit(void* Field);

    // LCD
    void UpdateLCD();
    void ShowLCDChannelInfo();
    void ShowLCDDVDInfo();

  private:
    MythMainWindow*   m_mainWindow { nullptr };

    // Configuration variables from database
    QString           m_baseFilters;
    QString           m_dbChannelFormat {"<num> <sign>"};
    uint              m_dbIdleTimeout {0};
    int               m_dbPlaybackExitPrompt {0};
    uint              m_dbAutoexpireDefault {0};
    bool              m_dbAutoSetWatched {false};
    bool              m_dbEndOfRecExitPrompt {false};
    bool              m_dbJumpPreferOsd {true};
    bool              m_dbUseGuiSizeForTv {false};
    bool              m_dbUseVideoModes {false};
    bool              m_dbClearSavedPosition {false};
    bool              m_dbRunJobsOnRemote {false};
    bool              m_dbContinueEmbedded {false};
    bool              m_dbRunFrontendInWindow {false};
    bool              m_dbBrowseAlways {false};
    bool              m_dbBrowseAllTuners {false};
    bool              m_dbUseChannelGroups {false};
    bool              m_dbRememberLastChannelGroup {false};
    ChannelGroupList  m_dbChannelGroups;

    bool              m_tryUnflaggedSkip {false};

    bool              m_smartForward {false};
    float             m_ffRewRepos {1.0F};
    bool              m_ffRewReverse {false};
    std::vector<int>  m_ffRewSpeeds;

    uint              m_vbimode {VBIMode::None};

    QElapsedTimer     m_ctorTime;
    uint              m_switchToInputId {0};

    /// True if the user told MythTV to stop plaback. If this is false
    /// when we exit the player, we display an error screen.
    mutable bool      m_wantsToQuit {true};
    bool              m_stretchAdjustment {false}; ///< True if time stretch is turned on
    bool              m_audiosyncAdjustment {false}; ///< True if audiosync is turned on
    bool              m_subtitleZoomAdjustment {false}; ///< True if subtitle zoom is turned on
    bool              m_subtitleDelayAdjustment {false}; ///< True if subtitle delay is turned on
    bool              m_editMode {false};          ///< Are we in video editing mode
    bool              m_zoomMode {false};
    bool              m_sigMonMode {false};     ///< Are we in signal monitoring mode?
    bool              m_endOfRecording {false}; ///< !player->IsPlaying() && StateIsPlaying()
    bool              m_requestDelete {false};  ///< User wants last video deleted
    bool              m_allowRerecord {false};  ///< User wants to rerecord the last video if deleted
    bool              m_doSmartForward {false};
    bool              m_queuedTranscode {false};
    /// Picture attribute type to modify.
    PictureAdjustType m_adjustingPicture {kAdjustingPicture_None};
    /// Picture attribute to modify (on arrow left or right)
    PictureAttribute  m_adjustingPictureAttribute {kPictureAttribute_None};

    // Ask Allow state
    QMap<QString,AskProgramInfo> m_askAllowPrograms;
    QMutex                       m_askAllowLock {QMutex::Recursive};

    MythDeque<QString>        m_changePxP;
    QMutex                    m_progListsLock;
    QMap<QString,ProgramList> m_progLists;

    mutable QMutex m_chanEditMapLock {QMutex::Recursive}; ///< Lock for chanEditMap and ddMap
    InfoMap        m_chanEditMap;          ///< Channel Editing initial map

    class SleepTimerInfo;
    static const std::vector<SleepTimerInfo> s_sleepTimes;
    uint                   m_sleepIndex {0};          ///< Index into sleep_times.
    uint                   m_sleepTimerTimeout {0};   ///< Current sleep timeout in msec
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
    QList<QKeyEvent> m_screenPressKeyMapPlayback;
    QList<QKeyEvent> m_screenPressKeyMapLiveTV;

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
    volatile int         m_updateOSDPosTimerId     {0};
    volatile int         m_updateOSDDebugTimerId   {0};
    volatile int         m_endOfPlaybackTimerId    {0};
    volatile int         m_embedCheckTimerId       {0};
    volatile int         m_endOfRecPromptTimerId   {0};
    volatile int         m_videoExitDialogTimerId  {0};
    volatile int         m_pseudoChangeChanTimerId {0};
    volatile int         m_speedChangeTimerId      {0};
    volatile int         m_errorRecoveryTimerId    {0};
    mutable volatile int m_exitPlayerTimerId       {0};
    volatile int         m_saveLastPlayPosTimerId  {0};
    volatile int         m_signalMonitorTimerId    {0};

    // Playback menu state caching
    OSD           *m_tvmOsd {nullptr};

    // Various tracks
    // XXX This ignores kTrackTypeTextSubtitle which is greater than
    // kTrackTypeCount, and it unnecessarily includes
    // kTrackTypeUnknown.
    QStringList m_tvmTracks[kTrackTypeCount];
    int         m_tvmCurtrack[kTrackTypeCount] {};

    // Audio
    bool    m_tvmAvsync {true};
    bool    m_tvmVisual {false};
    QString m_tvmActive;
    bool    m_tvmUpmixing {false};
    bool    m_tvmCanUpmix {false};
    QStringList m_tvmVisualisers;

    // Video
    AspectOverrideMode m_tvmAspectOverride    {kAspect_Off};
    AdjustFillMode   m_tvmAdjustFill          {kAdjustFill_Off};
    bool             m_tvmFillAutoDetect      {false};
    uint             m_tvmSup                 {kPictureAttributeSupported_None};
    bool             m_tvmStereoAllowed       {false};
    StereoscopicMode m_tvmStereoMode          {kStereoscopicModeAuto};
    QStringList      m_tvmDeinterlacers       {};
    QString          m_tvmCurrentDeinterlacer {};

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
    QList<long long> m_tvmChapterTimes;
    int              m_tvmNumAngles {0};
    int              m_tvmCurrentAngle {0};
    int              m_tvmNumTitles {0};
    int              m_tvmCurrentTitle {0};
    // Subtitles
    uint             m_tvmSubsCapMode {0};
    bool             m_tvmSubsHaveText {false};
    bool             m_tvmSubsForcedOn {true};
    bool             m_tvmSubsEnabled {false};
    bool             m_tvmSubsHaveSubs {false};

    bool             m_tvmIsOn {false};
    bool             m_tvmTranscoding {false};

    QVariant         m_tvmJumprecBackHack;
    // End of playback menu state caching

    MenuBase m_playbackMenu;
    MenuBase m_playbackCompactMenu;
    MenuBase m_cutlistMenu;
    MenuBase m_cutlistCompactMenu;

  public:
    // Constants
    static const int kInitFFRWSpeed; ///< 1x, default to normal speed
    static const uint kInputKeysMax; ///< When to start discarding early keys
    static const uint kNextSource;
    static const uint kPreviousSource;

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
    static const uint kSaveLastPlayPosTimeout;
};

#endif
