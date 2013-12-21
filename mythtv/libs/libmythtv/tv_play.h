// -*- Mode: c++ -*-

#ifndef TVPLAY_H
#define TVPLAY_H

// C
#include <stdint.h>

// C++
#include <vector>
using namespace std;

// Qt
#include <QReadWriteLock>
#include <QWaitCondition>
#include <QStringList>
#include <QDateTime>
#include <QKeyEvent>
#include <QObject>
#include <QRegExp>
#include <QString>
#include <QEvent>
#include <QMutex>
#include <QTime>
#include <QMap>
#include <QSet>

// MythTV
#include "mythdeque.h"
#include "tv.h"
#include "mythdate.h"
#include "programinfo.h"
#include "channelinfo.h"
#include "channelutil.h"
#include "videoouttypes.h"
#include "volumebase.h"
#include "inputinfo.h"
#include "channelgroup.h"
#include "mythtimer.h"
#include "osd.h"
#include "decoderbase.h"

class QDateTime;
class QDomDocument;
class QDomNode;
class OSD;
class RemoteEncoder;
class MythPlayer;
class DetectLetterbox;
class RingBuffer;
class ProgramInfo;
class MythDialog;
class PlayerContext;
class TvPlayWindow;
class TV;
class TVBrowseHelper;
class DDLoader;
struct osdInfo;

typedef QMap<QString,InfoMap>    DDValueMap;
typedef QMap<QString,DDValueMap> DDKeyMap;
typedef void (*EMBEDRETURNVOID) (void *, bool);
typedef void (*EMBEDRETURNVOIDEPG) (uint, const QString &, const QDateTime, TV *, bool, bool, int);
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
                    const QString &menuName,
                    MenuCurrentContext current,
                    bool doDisplay) :
        m_menu(menu),
        m_node(node),
        m_category(kMenuCategoryMenu),
        m_menuName(menuName),
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
                    const QString &action,
                    const QString &actionText,
                    bool doDisplay) :
        m_menu(menu),
        m_node(node),
        m_category(kMenuCategoryItem),
        m_menuName(""),
        m_showContext(showContext),
        m_currentContext(current),
        m_action(action),
        m_actionText(actionText),
        m_doDisplay(doDisplay) {}
    // Constructor for an itemlist element.
    MenuItemContext(const MenuBase &menu,
                    const QDomNode &node,
                    MenuShowContext showContext,
                    MenuCurrentContext current,
                    const QString &action,
                    bool doDisplay) :
        m_menu(menu),
        m_node(node),
        m_category(kMenuCategoryItemlist),
        m_menuName(""),
        m_showContext(showContext),
        m_currentContext(current),
        m_action(action),
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
    MenuBase() : m_document(NULL), m_translationContext("") {}
    ~MenuBase();
    bool        LoadFromFile(const QString &filename,
                             const QString &menuname,
                             const char *translationContext,
                             const QString &keyBindingContext);
    bool        LoadFromString(const QString &text,
                               const QString &menuname,
                               const char *translationContext,
                               const QString &keyBindingContext);
    bool        IsLoaded(void) const { return (m_document != NULL); }
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
    QDomDocument *m_document;
    const char   *m_translationContext;
    QString       m_menuName;
    QString       m_keyBindingContext;
};

/**
 * \class TV
 *
 * \brief Control TV playback
 * \qmlsignal TVPlaybackAborted(void)
 *
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
    friend class DDLoader;

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
    bool event(QEvent *e);
    bool eventFilter(QObject *o, QEvent *e);

    // Public PlaybackBox methods
    /// true iff program is the same as the one in the selected player
    bool IsSameProgram(int player_idx, const ProgramInfo *p) const;

    // Public recorder methods
    void FinishRecording(int player_idx); ///< Finishes player's recording

    // static functions
    static void InitKeys(void);
    static void ReloadKeys(void);
    static void SetFuncPtr(const char *, void *);
    static int  ConfiguredTunerCards(void);
    static bool IsTunable(uint chanid);

    /// \brief Helper class for Sleep Timer code.
    class SleepTimerInfo
    {
      public:
        SleepTimerInfo(QString str, unsigned long secs)
            : dispString(str), seconds(secs) { ; }
        QString   dispString;
        unsigned long seconds;
    };

  public slots:
    void HandleOSDClosed(int osdType);
    void timerEvent(QTimerEvent*);
    void StopPlayback(void);

  protected:
    // Protected event handling
    void customEvent(QEvent *e);

    static QStringList lastProgramStringList;
    static EMBEDRETURNVOID RunPlaybackBoxPtr;
    static EMBEDRETURNVOID RunViewScheduledPtr;
    static EMBEDRETURNVOIDEPG RunProgramGuidePtr;
    static EMBEDRETURNVOIDFINDER RunProgramFinderPtr;
    static EMBEDRETURNVOIDSCHEDIT RunScheduleEditorPtr;

  private:
    TV();
   ~TV();
    static TV*      GetTV(void);
    static void     ReleaseTV(TV* tv);
    static QMutex  *gTVLock;
    static TV      *gTV;

    // Private initialisation
    bool Init(bool createWindow = true);
    void InitFromDB(void);

    // Top level playback methods
    bool LiveTV(bool showDialogs, const ChannelInfoList &selection);
    int  Playback(const ProgramInfo &rcinfo);
    void PlaybackLoop(void);

    // Private event handling
    bool ProcessKeypress(PlayerContext*, QKeyEvent *e);
    void ProcessNetworkControlCommand(PlayerContext *, const QString &command);

    bool HandleTrackAction(PlayerContext*, const QString &action);
    bool ActiveHandleAction(PlayerContext*,
                            const QStringList &actions,
                            bool isDVD, bool isDVDStillFrame);
    bool BrowseHandleAction(PlayerContext*, const QStringList &actions);
    void OSDDialogEvent(int result, QString text, QString action);
    bool PxPHandleAction(PlayerContext*,const QStringList &actions);
    bool ToggleHandleAction(PlayerContext*,
                            const QStringList &actions, bool isDVD);
    bool FFRewHandleAction(PlayerContext*, const QStringList &actions);
    bool ActivePostQHandleAction(PlayerContext*, const QStringList &actions);
    bool HandleJumpToProgramAction(PlayerContext *ctx,
                                   const QStringList   &actions);
    bool SeekHandleAction(PlayerContext *actx, const QStringList &actions,
                          const bool isDVD);
    bool TimeStretchHandleAction(PlayerContext*,
                                 const QStringList &actions);
    bool DiscMenuHandleAction(PlayerContext*, const QStringList &actions);
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
    void ToggleSleepTimer(const PlayerContext*);
    void ToggleSleepTimer(const PlayerContext*, const QString &time);
    bool HandlePxPTimerEvent(void);
    bool HandleLCDTimerEvent(void);
    void HandleLCDVolumeTimerEvent(void);

    // Commands used by frontend UI screens (PlaybackBox, GuideGrid etc)
    void EditSchedule(const PlayerContext*,
                      int editType = kScheduleProgramGuide);
    bool StartEmbedding(const QRect&);
    void StopEmbedding(void);
    bool IsTunable(const PlayerContext*, uint chanid,
                   bool use_cache = false);
    QSet<uint> IsTunableOn(const PlayerContext*, uint chanid,
                           bool use_cache, bool early_exit);
    static QSet<uint> IsTunableOn(TV *tv, const PlayerContext*, uint chanid,
                                  bool use_cache, bool early_exit);
    void ClearTunableCache(void);
    void ChangeChannel(const PlayerContext*, const ChannelInfoList &options);
    void DrawUnusedRects(void);
    void DoEditSchedule(int editType = kScheduleProgramGuide);
    QString GetRecordingGroup(int player_idx) const;
    void ChangeVolume(PlayerContext*, bool up, int newvolume = -1);
    void ToggleMute(PlayerContext*, const bool muteIndividualChannels = false);
    void UpdateChannelList(int groupID);

    // Lock handling
    OSD *GetOSDL(const char *, int);
    OSD *GetOSDL(const PlayerContext*,const char *, int);
    void ReturnOSDLock(const PlayerContext*,OSD*&);
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

    // Other toggles
    void ToggleAutoExpire(PlayerContext*);
    void QuickRecord(PlayerContext*);

    // General TV state
    static bool StateIsRecording(TVState state);
    static bool StateIsPlaying(TVState state);
    static bool StateIsLiveTV(TVState state);

    TVState GetState(int player_idx) const;
    TVState GetState(const PlayerContext*) const;
    void HandleStateChange(PlayerContext *mctx, PlayerContext *ctx);
    void GetStatus(void);
    void ForceNextStateNone(PlayerContext*);
    void ScheduleStateChange(PlayerContext*);
    void SetErrored(PlayerContext*);
    void setInPlayList(bool setting) { inPlaylist = setting; }
    void setUnderNetworkControl(bool setting) { underNetworkControl = setting; }
    void PrepToSwitchToRecordedProgram(PlayerContext*,
                                       const ProgramInfo &);
    enum BookmarkAction {
        kBookmarkAlways,
        kBookmarkNever,
        kBookmarkAuto // set iff db_playback_exit_prompt==2
    };
    void PrepareToExitPlayer(PlayerContext*, int line,
                             BookmarkAction bookmark = kBookmarkAuto);
    void SetExitPlayer(bool set_it, bool wants_to);

    bool RequestNextRecorder(PlayerContext *, bool,
                             const ChannelInfoList &sel = ChannelInfoList());
    void DeleteRecorder();

    bool StartRecorder(PlayerContext *ctx, int maxWait=-1);
    void StopStuff(PlayerContext *mctx, PlayerContext *ctx,
                   bool stopRingbuffers, bool stopPlayers, bool stopRecorders);
    void TeardownPlayer(PlayerContext *mctx, PlayerContext *ctx);


    bool StartPlayer(PlayerContext *mctx, PlayerContext *ctx,
                     TVState desiredState);

    vector<long long> TeardownAllPlayers(PlayerContext*);
    void RestartAllPlayers(PlayerContext *lctx,
                           const vector<long long> &pos,
                           MuteState mctx_mute);
    void RestartMainPlayer(PlayerContext *mctx);

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
    bool IsDeleteAllowed(const PlayerContext*) const;

    // Channels
    void ToggleChannelFavorite(PlayerContext *ctx);
    void ToggleChannelFavorite(PlayerContext*, QString);
    void ChangeChannel(PlayerContext*, ChannelChangeDirection direction);
    void ChangeChannel(PlayerContext*, uint chanid, const QString &channum);

    void ShowPreviousChannel(PlayerContext*);
    void PopPreviousChannel(PlayerContext*, bool immediate_change);

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

    // Source and input
    void SwitchSource(PlayerContext*, uint source_direction);
    void SwitchInputs(PlayerContext*, uint inputid);
    void ToggleInputs(PlayerContext*, uint inputid = 0);
    void SwitchCards(PlayerContext*,
                     uint chanid = 0, QString channum = "", uint inputid = 0);

    // Pause/play
    void PauseLiveTV(PlayerContext*);
    void UnpauseLiveTV(PlayerContext*, bool bQuietly = false);
    void DoPlay(PlayerContext*);
    float DoTogglePauseStart(PlayerContext*);
    void DoTogglePauseFinish(PlayerContext*, float time, bool showOSD);
    void DoTogglePause(PlayerContext*, bool showOSD);
    vector<bool> DoSetPauseState(PlayerContext *lctx, const vector<bool>&);
    bool ContextIsPaused(PlayerContext *ctx, const char *file, int location);

    // Program jumping stuff
    void SetLastProgram(const ProgramInfo *rcinfo);
    ProgramInfo *GetLastProgram(void) const;

    // Seek, skip, jump, speed
    void DoSeek(PlayerContext*, float time, const QString &mesg,
                bool timeIsOffset, bool honorCutlist);
    bool DoPlayerSeek(PlayerContext*, float time);
    bool DoPlayerSeekToFrame(PlayerContext *ctx, uint64_t target);
    enum ArbSeekWhence {
        ARBSEEK_SET = 0,
        ARBSEEK_REWIND,
        ARBSEEK_FORWARD,
        ARBSEEK_END
    };
    void DoSeekAbsolute(PlayerContext *ctx, long long seconds, bool honorCutlist);
    void DoArbSeek(PlayerContext*, ArbSeekWhence whence, bool honorCutlist);
    void DoJumpFFWD(PlayerContext *ctx);
    void DoJumpRWND(PlayerContext *ctx);
    void NormalSpeed(PlayerContext*);
    void ChangeSpeed(PlayerContext*, int direction);
    void ToggleTimeStretch(PlayerContext*);
    void ChangeTimeStretch(PlayerContext*, int dir, bool allowEdit = true);
    void DVDJumpBack(PlayerContext*);
    void DVDJumpForward(PlayerContext*);
    float StopFFRew(PlayerContext*);
    void ChangeFFRew(PlayerContext*, int direction);
    void SetFFRew(PlayerContext*, int index);

    // Private audio methods
    void EnableUpmix(PlayerContext*, bool enable, bool toggle = false);
    void ChangeAudioSync(PlayerContext*, int dir, int newsync = -9999);
    bool AudioSyncHandleAction(PlayerContext*, const QStringList &actions);
    void PauseAudioUntilBuffered(PlayerContext *ctx);

    // Chapters, titles and angles
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
    void DoJumpChapter(PlayerContext*, int chapter);

    // Commercial skipping
    void DoSkipCommercials(PlayerContext*, int direction);
    void SetAutoCommercialSkip(const PlayerContext*,
                               CommSkipMode skipMode = kCommSkipOff);

    // Transcode
    void DoQueueTranscode(PlayerContext*, QString profile);

    // Bookmarks
    bool IsBookmarkAllowed(const PlayerContext*) const;
    void SetBookmark(PlayerContext* ctx, bool clear = false);

    // OSD
    bool ClearOSD(const PlayerContext*);
    void ToggleOSD(PlayerContext*, bool includeStatusOSD);
    void ToggleOSDDebug(PlayerContext*);
    void UpdateOSDDebug(const PlayerContext *ctx);
    void UpdateOSDProgInfo(const PlayerContext*, const char *whichInfo);
    void UpdateOSDStatus(const PlayerContext *ctx, QString title, QString desc,
                         QString value, int type, QString units,
                         int position = 0,
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
    bool ResizePIPWindow(PlayerContext*);
    bool IsPBPSupported(const PlayerContext *ctx = NULL) const;
    bool IsPIPSupported(const PlayerContext *ctx = NULL) const;
    void PxPToggleView(  PlayerContext *actx, bool wantPBP);
    void PxPCreateView(  PlayerContext *actx, bool wantPBP);
    void PxPTeardownView(PlayerContext *actx);
    void PxPToggleType(  PlayerContext *mctx, bool wantPBP);
    void PxPSwap(        PlayerContext *mctx, PlayerContext *pipctx);
    bool PIPAddPlayer(   PlayerContext *mctx, PlayerContext *ctx);
    bool PIPRemovePlayer(PlayerContext *mctx, PlayerContext *ctx);
    void PBPRestartMainPlayer(PlayerContext *mctx);
    void SetActive(PlayerContext *lctx, int index, bool osd_msg);

    // Video controls
    void ToggleAspectOverride(PlayerContext*,
                              AspectOverrideMode aspectMode = kAspect_Toggle);
    void ToggleAdjustFill(PlayerContext*,
                          AdjustFillMode adjustfillMode = kAdjustFill_Toggle);
    void DoToggleStudioLevels(const PlayerContext *ctx);
    void DoToggleNightMode(const PlayerContext*);
    void DoTogglePictureAttribute(const PlayerContext*,
                                  PictureAdjustType type);
    void DoChangePictureAttribute(
        PlayerContext*,
        PictureAdjustType type, PictureAttribute attr,
        bool up, int newvalue = -1);
    bool PictureAttributeHandleAction(PlayerContext*,
                                      const QStringList &actions);
    static PictureAttribute NextPictureAdjustType(
        PictureAdjustType type, MythPlayer *mp, PictureAttribute attr);
    void HandleDeinterlacer(PlayerContext* ctx, const QString &action);

    // Sundry on screen
    void ITVRestart(PlayerContext*, bool isLive);
    void EnableVisualisation(const PlayerContext*, bool enable, bool toggle = false,
                             const QString &action = QString(""));

    // Manual zoom mode
    void SetManualZoom(const PlayerContext *, bool enabled, QString msg);
    bool ManualZoomHandleAction(PlayerContext *actx,
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

    // General dialog handling
    bool DialogIsVisible(PlayerContext *ctx, const QString &dialog);
    void HandleOSDInfo(PlayerContext *ctx, QString action);
    void ShowNoRecorderDialog(const PlayerContext*,
                              NoRecorderMsg msgType = kNoRecorders);

    // AskAllow dialog handling
    void ShowOSDAskAllow(PlayerContext *ctx);
    void HandleOSDAskAllow(PlayerContext *ctx, QString action);
    void AskAllowRecording(PlayerContext*, const QStringList&, int, bool, bool);

    // Program editing support
    void ShowOSDCutpoint(PlayerContext *ctx, const QString &type);
    bool HandleOSDCutpoint(PlayerContext *ctx, QString action);
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
    void ShowOSDMenu(const PlayerContext*, bool isCompact = false);

    void FillOSDMenuJumpRec  (PlayerContext* ctx, const QString category = "",
                              int level = 0, const QString selected = "");

    void PlaybackMenuShow(const MenuBase &menu,
                          const QDomNode &node, const QDomNode &selected);
    void CutlistMenuShow(const MenuBase &menu,
                         const QDomNode &node, const QDomNode &selected);
    virtual bool MenuItemDisplay(const MenuItemContext &c);
    bool MenuItemDisplayPlayback(const MenuItemContext &c);
    bool MenuItemDisplayCutlist(const MenuItemContext &c);
    void PlaybackMenuInit(const MenuBase &menu);
    void PlaybackMenuDeinit(const MenuBase &menu);
    void MenuStrings(void) const;
    void MenuLazyInit(void *field);

    // LCD
    void UpdateLCD(void);
    void ShowLCDChannelInfo(const PlayerContext*);
    void ShowLCDDVDInfo(const PlayerContext*);

    // Other stuff
    int GetLastRecorderNum(int player_idx) const;
    static QStringList GetValidRecorderList(uint chanid);
    static QStringList GetValidRecorderList(const QString &channum);
    static QStringList GetValidRecorderList(uint, const QString&);

    static TVState RemoveRecording(TVState state);
    void RestoreScreenSaver(const PlayerContext*);

    // for temp debugging only..
    int find_player_index(const PlayerContext*) const;

  private:
    // Configuration variables from database
    QString baseFilters;
    QString db_channel_format;
    uint    db_idle_timeout;
    int     db_playback_exit_prompt;
    uint    db_autoexpire_default;
    bool    db_auto_set_watched;
    bool    db_end_of_rec_exit_prompt;
    bool    db_jump_prefer_osd;
    bool    db_use_gui_size_for_tv;
    bool    db_start_in_guide;
    bool    db_clear_saved_position;
    bool    db_toggle_bookmark;
    bool    db_run_jobs_on_remote;
    bool    db_continue_embedded;
    bool    db_use_fixed_size;
    bool    db_browse_always;
    bool    db_browse_all_tuners;
    bool    db_use_channel_groups;
    bool    db_remember_last_channel_group;
    ChannelGroupList db_channel_groups;

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
    bool subtitleZoomAdjustment; ///< True if subtitle zoom is turned on
    bool subtitleDelayAdjustment; ///< True if subtitle delay is turned on
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
    DDLoader *ddMapLoader;          ///< DataDirect map loader runnable

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
    /// Initial chanid override for Live TV
    uint            initialChanID;

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

    // Channel group stuff
    /// \brief Lock necessary when modifying channel group variables.
    /// These are only modified in UI thread, so no lock is needed
    /// to read this value in the UI thread.
    mutable QMutex channelGroupLock;
    volatile int   channelGroupId;
    ChannelInfoList     channelGroupChannelList;

    // Network Control stuff
    MythDeque<QString> networkControlCommands;

    // Timers
    typedef QMap<int,PlayerContext*>       TimerContextMap;
    typedef QMap<int,const PlayerContext*> TimerContextConstMap;
    mutable QMutex       timerIdLock;
    volatile int         lcdTimerId;
    volatile int         lcdVolumeTimerId;
    volatile int         networkControlTimerId;
    volatile int         jumpMenuTimerId;
    volatile int         pipChangeTimerId;
    volatile int         switchToInputTimerId;
    volatile int         ccInputTimerId;
    volatile int         asInputTimerId;
    volatile int         queueInputTimerId;
    volatile int         browseTimerId;
    volatile int         updateOSDPosTimerId;
    volatile int         updateOSDDebugTimerId;
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

    // Playback menu state caching
    PlayerContext *m_tvmCtx;
    OSD *m_tvmOsd;

    // Various tracks
    // XXX This ignores kTrackTypeTextSubtitle which is greater than
    // kTrackTypeCount, and it unnecessarily includes
    // kTrackTypeUnknown.
    QStringList m_tvm_tracks[kTrackTypeCount];
    int m_tvm_curtrack[kTrackTypeCount];

    // Audio
    bool m_tvm_avsync;
    bool m_tvm_visual;
    QString m_tvm_active;
    bool m_tvm_upmixing;
    bool m_tvm_canupmix;
    QStringList m_tvm_visualisers;

    // Video
    AspectOverrideMode m_tvm_aspectoverride;
    AdjustFillMode m_tvm_adjustfill;
    bool m_tvm_fill_autodetect;
    uint m_tvm_sup;
    bool m_tvm_studio_levels;
    bool m_tvm_stereoallowed;
    StereoscopicMode m_tvm_stereomode;
    FrameScanType m_tvm_scan_type;
    FrameScanType m_tvm_scan_type_unlocked;
    bool m_tvm_scan_type_locked;
    QString m_tvm_cur_mode;
    QStringList m_tvm_deinterlacers;
    QString     m_tvm_currentdeinterlacer;
    bool        m_tvm_doublerate;

    // Playback
    int m_tvm_speedX100;
    TVState m_tvm_state;
    bool m_tvm_isrecording;
    bool m_tvm_isrecorded;
    bool m_tvm_isvideo;
    CommSkipMode m_tvm_curskip;
    bool m_tvm_ispaused;
    bool m_tvm_allowPIP;
    bool m_tvm_allowPBP;
    bool m_tvm_hasPIP;
    bool m_tvm_hasPBP;
    int m_tvm_freerecordercount;
    bool m_tvm_isdvd;
    bool m_tvm_isbd;
    bool m_tvm_jump;
    bool m_tvm_islivetv;
    bool m_tvm_previouschan;

    // Navigate
    int m_tvm_num_chapters;
    int m_tvm_current_chapter;
    QList<long long> m_tvm_chapter_times;
    int m_tvm_num_angles;
    int m_tvm_current_angle;
    int m_tvm_num_titles;
    int m_tvm_current_title;

    // Subtitles
    uint m_tvm_subs_capmode;
    bool m_tvm_subs_havetext;
    bool m_tvm_subs_forcedon;
    bool m_tvm_subs_enabled;
    bool m_tvm_subs_have_subs;

    bool m_tvm_is_on;
    bool m_tvm_transcoding;

    QVariant m_tvm_jumprec_back_hack;
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
};

#endif
