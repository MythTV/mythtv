// -*- Mode: c++ -*-

#ifndef TVPLAY_H
#define TVPLAY_H

// C++
#include <cstdint>
#include <utility>
#include <vector>
using namespace std;

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
#include "decoders/decoderbase.h"

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
class RingBuffer;
class ProgramInfo;
class PlayerContext;
class TvPlayWindow;
class TV;
class TVBrowseHelper;
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
    bool        IsLoaded(void) const { return (m_document != nullptr); }
    QDomElement GetRoot(void) const;
    QString     Translate(const QString &text) const;
    bool        Show(const QDomNode &node, const QDomNode &selected,
                     MenuItemDisplayer &displayer,
                     bool doDisplay = true) const;
    QString     GetName(void) const { return m_menuName; }
    const char *GetTranslationContext(void) const {
        return m_translationContext;
    }
    const QString &GetKeyBindingContext(void) const {
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
 * \qmlsignal TVPlaybackAborted(void)
 * TV playback failed to start (typically, TV playback was started when another playback is currently going)
 * \qmlsignal TVPlaybackStarted(void)
 * TV playback has started, video is now playing
 * \qmlsignal TVPlaybackStopped(void)
 * TV playback has stopped and playback has exited
 * \qmlsignal TVPlaybackUnpaused(void)
 * TV playback has resumed, following a Pause action
 * \qmlsignal TVPlaybackPaused(void)
 * TV playback has been paused
 * \qmlsignal TVPlaybackSought(qint position_seconds)
 * Absolute seek has completed to position_seconds
 */
class MTV_PUBLIC TV : public QObject, public MenuItemDisplayer
{
    friend class PlaybackBox;
    friend class GuideGrid;
    friend class ProgFinder;
    friend class ViewScheduled;
    friend class ScheduleEditor;
    friend class TvPlayWindow;
    friend class TVBrowseHelper;

    Q_OBJECT
  public:
    // Check whether we already have a TV object
    static bool IsTVRunning(void);
    static TV*  CurrentTVInstance(void) { return gTV; }
    // Start media playback
    static bool StartTV(ProgramInfo *tvrec,
                        uint flags,
                        const ChannelInfoList &selection = ChannelInfoList());
    static bool IsPaused(void);

    // Public event handling
    bool event(QEvent *e) override; // QObject
    bool eventFilter(QObject *o, QEvent *e) override; // QObject

    // Public PlaybackBox methods
    /// true iff program is the same as the one in the selected player
    bool IsSameProgram(int player_idx, const ProgramInfo *rcinfo) const;

    // Public recorder methods
    void FinishRecording(int player_ctx); ///< Finishes player's recording

    // static functions
    static void InitKeys(void);
    static void ReloadKeys(void);
    static void SetFuncPtr(const char *string, void *lptr);
    static int  ConfiguredTunerCards(void);
    static bool IsTunable(uint chanid);

    /// \brief Helper class for Sleep Timer code.
    class SleepTimerInfo
    {
      public:
        SleepTimerInfo(QString  str, unsigned long secs)
            : dispString(std::move(str)), seconds(secs) { ; }
        QString   dispString;
        unsigned long seconds;
    };

  public slots:
    void HandleOSDClosed(int osdType);
    void timerEvent(QTimerEvent *te) override; // QObject
    void StopPlayback(void);

  signals:
    void PlaybackExiting(TV* Player);

  protected:
    // Protected event handling
    void customEvent(QEvent *e) override; // QObject

    static QStringList lastProgramStringList;
    static EMBEDRETURNVOID RunPlaybackBoxPtr;
    static EMBEDRETURNVOID RunViewScheduledPtr;
    static EMBEDRETURNVOIDEPG RunProgramGuidePtr;
    static EMBEDRETURNVOIDFINDER RunProgramFinderPtr;
    static EMBEDRETURNVOIDSCHEDIT RunScheduleEditorPtr;

  protected slots:
    void onApplicationStateChange(Qt::ApplicationState state);

  private:
    TV();
   ~TV() override;
    static TV*      GetTV(void);
    static void     ReleaseTV(TV* tv);
    static QMutex  *gTVLock;
    static TV      *gTV;

    // Private initialisation
    bool Init(bool createWindow = true);
    void InitFromDB(void);
    static QList<QKeyEvent> ConvertScreenPressKeyMap(const QString& keyList);

    // Top level playback methods
    bool LiveTV(bool showDialogs, const ChannelInfoList &selection);
    int  Playback(const ProgramInfo &rcinfo);
    void PlaybackLoop(void);

    // Private event handling
    bool ProcessKeypressOrGesture(PlayerContext *actx, QEvent *e);
    bool TranslateKeyPressOrGesture(const QString &context, QEvent *e,
                                    QStringList &actions, bool isLiveTV,
                                    bool allowJumps = true);
    bool TranslateGesture(const QString &context, MythGestureEvent *e,
                          QStringList &actions, bool isLiveTV);
    void ProcessNetworkControlCommand(PlayerContext *ctx, const QString &command);

    bool HandleTrackAction(PlayerContext *ctx, const QString &action);
    bool ActiveHandleAction(PlayerContext *ctx,
                            const QStringList &actions,
                            bool isDVD, bool isDVDStillFrame);
    bool BrowseHandleAction(PlayerContext *ctx, const QStringList &actions);
    void OSDDialogEvent(int result, const QString& text, QString action);
    bool PxPHandleAction(PlayerContext *ctx,const QStringList &actions);
    bool ToggleHandleAction(PlayerContext *ctx,
                            const QStringList &actions, bool isDVD);
    bool FFRewHandleAction(PlayerContext *ctx, const QStringList &actions);
    bool ActivePostQHandleAction(PlayerContext *ctx, const QStringList &actions);
    bool HandleJumpToProgramAction(PlayerContext *ctx,
                                   const QStringList   &actions);
    bool SeekHandleAction(PlayerContext *actx, const QStringList &actions,
                          bool isDVD);
    bool TimeStretchHandleAction(PlayerContext *ctx,
                                 const QStringList &actions);
    static bool DiscMenuHandleAction(PlayerContext *ctx, const QStringList &actions);
    bool Handle3D(PlayerContext *ctx, const QString &action);

    // Timers and timer events
    int  StartTimer(int interval, int line);
    void KillTimer(int id);

    void SetSpeedChangeTimer(uint when, int line);
    void HandleEndOfPlaybackTimerEvent(void);
    void HandleIsNearEndWhenEmbeddingTimerEvent(void);
    void HandleEndOfRecordingExitPromptTimerEvent(void);
    void HandleVideoExitDialogTimerEvent(void);
    void HandlePseudoLiveTVTimerEvent(void);
    void HandleSpeedChangeTimerEvent(void);
    void ToggleSleepTimer(const PlayerContext *ctx);
    void ToggleSleepTimer(const PlayerContext *ctx, const QString &time);
    bool HandlePxPTimerEvent(void);
    bool HandleLCDTimerEvent(void);
    void HandleLCDVolumeTimerEvent(void);
    void HandleSaveLastPlayPosEvent();

    // Commands used by frontend UI screens (PlaybackBox, GuideGrid etc)
    void EditSchedule(const PlayerContext *ctx,
                      int editType = kScheduleProgramGuide);
    bool StartEmbedding(const QRect &embedRect);
    void StopEmbedding(void);
    static bool IsTunable(const PlayerContext *ctx, uint chanid);
    static QSet<uint> IsTunableOn(const PlayerContext *ctx, uint chanid);
    void ChangeChannel(const PlayerContext *ctx, const ChannelInfoList &options);
    void DoEditSchedule(int editType = kScheduleProgramGuide);
    QString GetRecordingGroup(int player_idx) const;
    void ChangeVolume(PlayerContext *ctx, bool up, int newvolume = -1);
    void ToggleMute(PlayerContext *ctx, bool muteIndividualChannels = false);
    void UpdateChannelList(int groupID);

    // Lock handling
    OSD *GetOSDL(const char *file, int location);
    OSD *GetOSDL(const PlayerContext *ctx, const char *file, int location);
    void ReturnOSDLock(const PlayerContext *ctx, OSD *&osd);
    PlayerContext       *GetPlayerWriteLock(
        int which, const char *file, int location);
    PlayerContext       *GetPlayerReadLock(
        int which, const char *file, int location);
    const PlayerContext *GetPlayerReadLock(
        int which, const char *file, int location) const;
    PlayerContext       *GetPlayerHaveLock(
        PlayerContext *locked_context,
        int which, const char *file, int location);
    const PlayerContext *GetPlayerHaveLock(
        const PlayerContext *locked_context,
        int which, const char *file, int location) const;
    void ReturnPlayerLock(PlayerContext *&ctx);
    void ReturnPlayerLock(const PlayerContext *&ctx) const;

    // Other toggles
    void ToggleAutoExpire(PlayerContext *ctx);
    void QuickRecord(PlayerContext *ctx);

    // General TV state
    static bool StateIsRecording(TVState state);
    static bool StateIsPlaying(TVState state);
    static bool StateIsLiveTV(TVState state);

    TVState GetState(int player_idx) const;
    static TVState GetState(const PlayerContext *actx);
    void HandleStateChange(PlayerContext *mctx, PlayerContext *ctx);
    void GetStatus(void);
    void ForceNextStateNone(PlayerContext *ctx);
    void ScheduleStateChange(PlayerContext *ctx);
    void SetErrored(PlayerContext *ctx);
    void setInPlayList(bool setting) { m_inPlaylist = setting; }
    void setUnderNetworkControl(bool setting) { m_underNetworkControl = setting; }
    void PrepToSwitchToRecordedProgram(PlayerContext *ctx,
                                       const ProgramInfo &p);
    enum BookmarkAction {
        kBookmarkAlways,
        kBookmarkNever,
        kBookmarkAuto // set iff db_playback_exit_prompt==2
    };
    void PrepareToExitPlayer(PlayerContext *ctx, int line,
                             BookmarkAction bookmark = kBookmarkAuto);
    void SetExitPlayer(bool set_it, bool wants_to);

    bool RequestNextRecorder(PlayerContext *ctx, bool showDialogs,
                             const ChannelInfoList &sel = ChannelInfoList());
    void DeleteRecorder();

    bool StartRecorder(PlayerContext *ctx, int maxWait=-1);
    void StopStuff(PlayerContext *mctx, PlayerContext *ctx,
                   bool stopRingBuffer, bool stopPlayer, bool stopRecorder);
    void TeardownPlayer(PlayerContext *mctx, PlayerContext *ctx);


    bool StartPlayer(PlayerContext *mctx, PlayerContext *ctx,
                     TVState desiredState);

    vector<long long> TeardownAllPlayers(PlayerContext *lctx);
    void RestartAllPlayers(PlayerContext *lctx,
                           const vector<long long> &pos,
                           MuteState mctx_mute);
    void RestartMainPlayer(PlayerContext *mctx);

    /// Returns true if we are currently in the process of switching recorders.
    bool IsSwitchingCards(void)  const { return m_switchToRec; }
    /// Returns true if the user told Mythtv to allow re-recording of the show
    bool getAllowRerecord(void) const { return m_allowRerecord;  }
    /// This is set to true if the player reaches the end of the
    /// recording without the user explicitly exiting the player.
    bool getEndOfRecording(void) const { return m_endOfRecording; }
    /// This is set if the user asked MythTV to jump to the previous
    /// recording in the playlist.
    bool getJumpToProgram(void)  const { return m_jumpToProgram; }
    static bool IsDeleteAllowed(const PlayerContext *ctx);

    // Channels
    static void ToggleChannelFavorite(PlayerContext *ctx);
    static void ToggleChannelFavorite(PlayerContext *ctx, const QString &changroup_name);
    void ChangeChannel(PlayerContext *ctx, ChannelChangeDirection direction);
    void ChangeChannel(PlayerContext *ctx, uint chanid, const QString &channum);

    void ShowPreviousChannel(PlayerContext *ctx);
    void PopPreviousChannel(PlayerContext *ctx, bool immediate_change);

    // key queue commands
    void AddKeyToInputQueue(PlayerContext *ctx, char key);
    void ClearInputQueues(const PlayerContext *ctx, bool hideosd);
    bool CommitQueuedInput(PlayerContext *ctx);
    bool ProcessSmartChannel(const PlayerContext *ctx, QString &inputStr);

    // query key queues
    bool HasQueuedInput(void) const
        { return !GetQueuedInput().isEmpty(); }
    bool HasQueuedChannel(void) const
        { return m_queuedChanID || !GetQueuedChanNum().isEmpty(); }

    // get queued up input
    QString GetQueuedInput(void)   const;
    int     GetQueuedInputAsInt(bool *ok = nullptr, int base = 10) const;
    QString GetQueuedChanNum(void) const;
    uint    GetQueuedChanID(void)  const { return m_queuedChanID; }

    // Source and input
    void SwitchSource(PlayerContext *ctx, uint source_direction);
    void SwitchInputs(PlayerContext *ctx,
                      uint chanid = 0, QString channum = "", uint inputid = 0);

    // Pause/play
    void PauseLiveTV(PlayerContext *ctx);
    void UnpauseLiveTV(PlayerContext *ctx, bool bQuietly = false);
    void DoPlay(PlayerContext *ctx);
    float DoTogglePauseStart(PlayerContext *ctx);
    void DoTogglePauseFinish(PlayerContext *ctx, float time, bool showOSD);
    void DoTogglePause(PlayerContext *ctx, bool showOSD);
    vector<bool> DoSetPauseState(PlayerContext *lctx, const vector<bool>&pause);
    static bool ContextIsPaused(PlayerContext *ctx, const char *file, int location);

    // Program jumping stuff
    void SetLastProgram(const ProgramInfo *rcinfo);
    ProgramInfo *GetLastProgram(void) const;

    // Seek, skip, jump, speed
    void DoSeek(PlayerContext *ctx, float time, const QString &mesg,
                bool timeIsOffset, bool honorCutlist);
    bool DoPlayerSeek(PlayerContext *ctx, float time);
    bool DoPlayerSeekToFrame(PlayerContext *ctx, uint64_t target);
    enum ArbSeekWhence {
        ARBSEEK_SET = 0,
        ARBSEEK_REWIND,
        ARBSEEK_FORWARD,
        ARBSEEK_END
    };
    void DoSeekAbsolute(PlayerContext *ctx, long long seconds, bool honorCutlist);
    void DoArbSeek(PlayerContext *ctx, ArbSeekWhence whence, bool honorCutlist);
    void DoJumpFFWD(PlayerContext *ctx);
    void DoJumpRWND(PlayerContext *ctx);
    void DoSeekFFWD(PlayerContext *ctx);
    void DoSeekRWND(PlayerContext *ctx);
    void NormalSpeed(PlayerContext *ctx);
    void ChangeSpeed(PlayerContext *ctx, int direction);
    void ToggleTimeStretch(PlayerContext *ctx);
    void ChangeTimeStretch(PlayerContext *ctx, int dir, bool allowEdit = true);
    void DVDJumpBack(PlayerContext *ctx);
    void DVDJumpForward(PlayerContext *ctx);
    float StopFFRew(PlayerContext *ctx);
    void ChangeFFRew(PlayerContext *ctx, int direction);
    void SetFFRew(PlayerContext *ctx, int index);

    // Private audio methods
    void EnableUpmix(PlayerContext *ctx, bool enable, bool toggle = false);
    void ChangeAudioSync(PlayerContext *ctx, int dir, int newsync = -9999);
    bool AudioSyncHandleAction(PlayerContext *ctx, const QStringList &actions);
    static void PauseAudioUntilBuffered(PlayerContext *ctx);

    // Chapters, titles and angles
    static int  GetNumChapters(const PlayerContext *ctx);
    static void GetChapterTimes(const PlayerContext *ctx, QList<long long> &times);
    static int  GetCurrentChapter(const PlayerContext *ctx);
    static int  GetNumTitles(const PlayerContext *ctx);
    static int  GetCurrentTitle(const PlayerContext *ctx);
    static int  GetTitleDuration(const PlayerContext *ctx, int title);
    static QString GetTitleName(const PlayerContext *ctx, int title);
    void DoSwitchTitle(PlayerContext *ctx, int title);
    static int  GetNumAngles(const PlayerContext *ctx);
    static int  GetCurrentAngle(const PlayerContext *ctx);
    static QString GetAngleName(const PlayerContext *ctx, int angle);
    void DoSwitchAngle(PlayerContext *ctx, int angle);
    void DoJumpChapter(PlayerContext *ctx, int chapter);

    // Commercial skipping
    void DoSkipCommercials(PlayerContext *ctx, int direction);
    void SetAutoCommercialSkip(const PlayerContext *ctx,
                               CommSkipMode skipMode = kCommSkipOff);

    // Transcode
    void DoQueueTranscode(PlayerContext *ctx, const QString& profile);

    // Bookmarks
    static bool IsBookmarkAllowed(const PlayerContext *ctx);
    void SetBookmark(PlayerContext* ctx, bool clear = false);

    // OSD
    bool ClearOSD(const PlayerContext *ctx);
    void ToggleOSD(PlayerContext *ctx, bool includeStatusOSD);
    void ToggleOSDDebug(PlayerContext *ctx);
    void UpdateOSDDebug(const PlayerContext *ctx);
    void UpdateOSDProgInfo(const PlayerContext *ctx, const char *whichInfo);
    void UpdateOSDStatus(const PlayerContext *ctx, const QString& title, const QString& desc,
                         const QString& value, int type, const QString& units,
                         int position = 0,
                         enum OSDTimeout timeout = kOSDTimeout_Med);
    void UpdateOSDStatus(const PlayerContext *ctx, osdInfo &info,
                         int type, enum OSDTimeout timeout);

    void UpdateOSDSeekMessage(const PlayerContext *ctx,
                              const QString &mesg, enum OSDTimeout timeout);
    void UpdateOSDInput(const PlayerContext *ctx);
    void UpdateOSDSignal(PlayerContext *ctx, const QStringList &strlist);
    void UpdateOSDTimeoutMessage(PlayerContext *ctx);
    void UpdateOSDAskAllowDialog(PlayerContext*);
    void SetUpdateOSDPosition(bool set_it);

    // Captions/subtitles
    bool SubtitleZoomHandleAction(PlayerContext *ctx,
                                  const QStringList &actions);
    void ChangeSubtitleZoom(PlayerContext *ctx, int dir);
    bool SubtitleDelayHandleAction(PlayerContext *ctx,
                                   const QStringList &actions);
    void ChangeSubtitleDelay(PlayerContext *ctx, int dir);

    // PxP handling
    bool CreatePBP(PlayerContext *lctx, const ProgramInfo *info);
    bool CreatePIP(PlayerContext *lctx, const ProgramInfo *info);
    bool ResizePIPWindow(PlayerContext *ctx);
    bool IsPBPSupported(const PlayerContext *ctx = nullptr) const;
    bool IsPIPSupported(const PlayerContext *ctx = nullptr) const;
    void PxPToggleView(  PlayerContext *actx, bool wantPBP);
    void PxPCreateView(  PlayerContext *actx, bool wantPBP);
    void PxPTeardownView(PlayerContext *actx);
    void PxPToggleType(  PlayerContext *mctx, bool wantPBP);
    void PxPSwap(        PlayerContext *mctx, PlayerContext *pipctx);
    bool PIPAddPlayer(   PlayerContext *mctx, PlayerContext *ctx);
    static bool PIPRemovePlayer(PlayerContext *mctx, PlayerContext *ctx);
    void PBPRestartMainPlayer(PlayerContext *mctx);
    void SetActive(PlayerContext *lctx, int index, bool osd_msg);

    // Video controls
    void ToggleMoveBottomLine(PlayerContext *ctx);
    void SaveBottomLine(PlayerContext *ctx);
    void ToggleAspectOverride(PlayerContext *ctx,
                              AspectOverrideMode aspectMode = kAspect_Toggle);
    void ToggleAdjustFill(PlayerContext *ctx,
                          AdjustFillMode adjustfillMode = kAdjustFill_Toggle);
    static void DoToggleNightMode(const PlayerContext *ctx);
    void DoTogglePictureAttribute(const PlayerContext *ctx,
                                  PictureAdjustType type);
    void DoChangePictureAttribute(
        PlayerContext *ctx,
        PictureAdjustType type, PictureAttribute attr,
        bool up, int newvalue = -1);
    bool PictureAttributeHandleAction(PlayerContext *ctx,
                                      const QStringList &actions);
    static PictureAttribute NextPictureAdjustType(
        PictureAdjustType type, MythPlayer *mp, PictureAttribute attr);
    static void HandleDeinterlacer(PlayerContext* ctx, const QString &action);
    void OverrideScan(PlayerContext *Context, FrameScanType Scan);

    // Sundry on screen
    static void ITVRestart(PlayerContext *ctx, bool isLive);
    void EnableVisualisation(const PlayerContext *ctx, bool enable, bool toggle = false,
                             const QString &action = QString(""));

    // Manual zoom mode
    void SetManualZoom(const PlayerContext *ctx, bool zoomON, const QString& desc);
    bool ManualZoomHandleAction(PlayerContext *actx,
                                const QStringList &actions);

    // Channel editing support
    void StartChannelEditMode(PlayerContext *ctx);
    bool HandleOSDChannelEdit(PlayerContext *ctx, const QString& action);
    static void ChannelEditAutoFill(const PlayerContext *ctx, InfoMap &infoMap);
    static void ChannelEditXDSFill(const PlayerContext *ctx, InfoMap &infoMap);

    // General dialog handling
    bool DialogIsVisible(PlayerContext *ctx, const QString &dialog);
    void HandleOSDInfo(PlayerContext *ctx, const QString& action);
    void ShowNoRecorderDialog(const PlayerContext *ctx,
                              NoRecorderMsg msgType = kNoRecorders);

    // AskAllow dialog handling
    void ShowOSDAskAllow(PlayerContext *ctx);
    void HandleOSDAskAllow(PlayerContext *ctx, const QString& action);
    void AskAllowRecording(PlayerContext *ctx, const QStringList &msg,
                           int timeuntil, bool hasrec, bool haslater);

    // Program editing support
    void ShowOSDCutpoint(PlayerContext *ctx, const QString &type);
    bool HandleOSDCutpoint(PlayerContext *ctx, const QString& action);
    void StartProgramEditMode(PlayerContext *ctx);

    // Already editing dialog
    void ShowOSDAlreadyEditing(PlayerContext *ctx);
    void HandleOSDAlreadyEditing(PlayerContext *ctx, const QString& action,
                                 bool was_paused);

    // Sleep dialog handling
    void ShowOSDSleep(void);
    void HandleOSDSleep(PlayerContext *ctx, const QString& action);
    void SleepDialogTimeout(void);

    // Idle dialog handling
    void ShowOSDIdle(void);
    void HandleOSDIdle(PlayerContext *ctx, const QString& action);
    void IdleDialogTimeout(void);

    // Exit/delete dialog handling
    void ShowOSDStopWatchingRecording(PlayerContext *ctx);
    void ShowOSDPromptDeleteRecording(PlayerContext *ctx, const QString& title,
                                      bool force = false);
    bool HandleOSDVideoExit(PlayerContext *ctx, const QString& action);

    // Navigation Dialog
    void StartOsdNavigation(PlayerContext *ctx);
    void UpdateNavDialog(PlayerContext *ctx);

    // Menu dialog
    void ShowOSDMenu(bool isCompact = false);

    void FillOSDMenuJumpRec  (PlayerContext* ctx, const QString &category = "",
                              int level = 0, const QString &selected = "");

    void PlaybackMenuShow(const MenuBase &menu,
                          const QDomNode &node, const QDomNode &selected);
    void CutlistMenuShow(const MenuBase &menu,
                         const QDomNode &node, const QDomNode &selected);
    bool MenuItemDisplay(const MenuItemContext &c) override; // MenuItemDisplayer
    bool MenuItemDisplayPlayback(const MenuItemContext &c);
    bool MenuItemDisplayCutlist(const MenuItemContext &c);
    void PlaybackMenuInit(const MenuBase &menu);
    void PlaybackMenuDeinit(const MenuBase &menu);
    static void MenuStrings(void);
    void MenuLazyInit(void *field);

    // LCD
    void UpdateLCD(void);
    void ShowLCDChannelInfo(const PlayerContext *ctx);
    void ShowLCDDVDInfo(const PlayerContext *ctx);

    // Other stuff
    int GetLastRecorderNum(int player_idx) const;
    static QStringList GetValidRecorderList(uint chanid);
    static QStringList GetValidRecorderList(const QString &channum);
    static QStringList GetValidRecorderList(uint, const QString&);

    static TVState RemoveRecording(TVState state);
    void RestoreScreenSaver(const PlayerContext *ctx);

    // for temp debugging only..
    int find_player_index(const PlayerContext *ctx) const;
    static QString GetLiveTVIndex(const PlayerContext *ctx);

  private:
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
    vector<int>       m_ffRewSpeeds;

    uint              m_vbimode {VBIMode::None};

    QElapsedTimer     m_ctorTime;
    uint              m_switchToInputId {0};

    QMutex            m_initFromDBLock;
    bool              m_initFromDBDone {false};
    QWaitCondition    m_initFromDBWait;

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

    /// Vector or sleep timer sleep times in seconds,
    /// with the appropriate UI message.
    vector<SleepTimerInfo> m_sleepTimes;
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

    // Channel browsing state variables
    TVBrowseHelper *m_browseHelper {nullptr};

    // Program Info for currently playing video
    // (or next video if InChangeState() is true)
    mutable QMutex  m_lastProgramLock;
    ProgramInfo    *m_lastProgram {nullptr};   ///< last program played with this player
    bool            m_inPlaylist {false}; ///< show is part of a playlist
    bool            m_underNetworkControl {false}; ///< initial show started via by the network control interface

    // Program Jumping
    PIPState     m_jumpToProgramPIPState {kPIPOff};
    bool         m_jumpToProgram {false};

    // Video Players
    vector<PlayerContext*>  m_player;
    /// Video Player to which events are sent to
    int                     m_playerActive {-1};
    /// lock on player and playerActive changes
    mutable QReadWriteLock  m_playerLock;

    bool                    m_noHardwareDecoders {false};

    // Remote Encoders
    /// Main recorder to use after a successful SwitchCards() call.
    RemoteEncoder          *m_switchToRec {nullptr};

    // OSD info
    QMap<OSD*,const PlayerContext*> m_osdLctx;

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
    bool         m_isEmbedded {false};       ///< are we currently embedded
    bool         m_ignoreKeyPresses {false}; ///< should we ignore keypresses
    vector<bool> m_savedPause;      ///< saved pause state before embedding

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
    using TimerContextMap      = QMap<int,PlayerContext*>;
    using TimerContextConstMap = QMap<int,const PlayerContext*>;
    mutable QMutex       m_timerIdLock;
    volatile int         m_lcdTimerId              {0};
    volatile int         m_lcdVolumeTimerId        {0};
    volatile int         m_networkControlTimerId   {0};
    volatile int         m_jumpMenuTimerId         {0};
    volatile int         m_pipChangeTimerId        {0};
    volatile int         m_switchToInputTimerId    {0};
    volatile int         m_ccInputTimerId          {0};
    volatile int         m_asInputTimerId          {0};
    volatile int         m_queueInputTimerId       {0};
    volatile int         m_browseTimerId           {0};
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
    TimerContextMap      m_stateChangeTimerId;
    TimerContextMap      m_signalMonitorTimerId;

    // Playback menu state caching
    PlayerContext *m_tvmCtx {nullptr};
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
    StereoscopicMode m_tvmStereoMode          {kStereoscopicModeNone};
    QStringList      m_tvmDeinterlacers       {};
    QString          m_tvmCurrentDeinterlacer {};
    bool             m_tvmDoubleRate          {false};

    // Playback
    int          m_tvmSpeedX100         {100};
    TVState      m_tvmState             {kState_None};
    bool         m_tvmIsRecording       {false};
    bool         m_tvmIsRecorded        {false};
    bool         m_tvmIsVideo           {false};
    CommSkipMode m_tvmCurSkip           {kCommSkipOff};
    bool         m_tvmIsPaused          {false};
    bool         m_tvmAllowPIP          {false};
    bool         m_tvmAllowPBP          {false};
    bool         m_tvmHasPIP            {false};
    bool         m_tvmHasPBP            {false};
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
    static const uint kSaveLastPlayPosTimeout;
};

#endif
