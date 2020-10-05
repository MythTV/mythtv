// Qt
#include <QApplication>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNode>
#include <QEvent>
#include <QFile>
#include <QKeyEvent>
#include <QRegExp>
#include <QRegularExpression>
#include <QRunnable>
#include <QTimerEvent>
#include <utility>

#include "mythconfig.h"

// libmythbase
#include "mthreadpool.h"
#include "signalhandling.h"
#include "mythdb.h"
#include "mythcorecontext.h"
#include "mythlogging.h"
#include "lcddevice.h"
#include "compat.h"
#include "mythdirs.h"
#include "mythmedia.h"

// libmyth
#include "programinfo.h"
#include "remoteutil.h"

// libmythui
#include "mythuistatetracker.h"
#include "mythuihelper.h"
#include "mythdialogbox.h"
#include "mythmainwindow.h"
#include "mythmiscutil.h"
#include "mythscreenstack.h"
#include "mythscreentype.h"
#include "mythuiactions.h"

// libmythtv
#include "DVD/mythdvdbuffer.h"
#include "Bluray/mythbdbuffer.h"
#include "remoteencoder.h"
#include "tvremoteutil.h"
#include "mythplayer.h"
#include "captions/subtitlescreen.h"
#include "DetectLetterbox.h"
#include "jobqueue.h"
#include "livetvchain.h"
#include "playgroup.h"
#include "sourceutil.h"
#include "cardutil.h"
#include "channelutil.h"
#include "tv_play_win.h"
#include "recordinginfo.h"
#include "signalmonitorvalue.h"
#include "recordingrule.h"
#include "mythsystemevent.h"
#include "videometadatautil.h"
#include "playercontext.h"
#include "programtypes.h"
#include "io/mythmediabuffer.h"
#include "tv_actions.h"
#include "mythcodeccontext.h"
#include "tv_play.h"

// Std
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <thread>

#if ! HAVE_ROUND
#define round(x) ((int) ((x) + 0.5))
#endif

#define DEBUG_CHANNEL_PREFIX 0 /**< set to 1 to debug channel prefixing */
#define DEBUG_ACTIONS        0 /**< set to 1 to debug actions           */

#define LOC      QString("TV::%1(): ").arg(__func__)

#define SetOSDText(GROUP, FIELD, TEXT, TIMEOUT) { \
    OSD *osd_m = GetOSDL(); \
    if (osd_m) \
    { \
        InfoMap map; \
        map.insert(FIELD,TEXT); \
        osd_m->SetText(GROUP, map, TIMEOUT); \
    } \
    ReturnOSDLock(); }

#define SetOSDMessage(MESSAGE) \
    SetOSDText("osd_message", "message_text", MESSAGE, kOSDTimeout_Med)

#define HideOSDWindow(WINDOW) { \
    OSD *osd = GetOSDL(); \
    if (osd) \
        osd->HideWindow(WINDOW); \
    ReturnOSDLock(); }

const int  TV::kInitFFRWSpeed                = 0;
const uint TV::kInputKeysMax                 = 6;
const uint TV::kNextSource                   = 1;
const uint TV::kPreviousSource               = 2;

const uint TV::kInputModeTimeout             = 5000;
const uint TV::kLCDTimeout                   = 1000;
const uint TV::kBrowseTimeout                = 30000;
const uint TV::kKeyRepeatTimeout             = 300;
const uint TV::kPrevChanTimeout              = 750;
const uint TV::kSleepTimerDialogTimeout      = 45000;
const uint TV::kIdleTimerDialogTimeout       = 45000;
const uint TV::kVideoExitDialogTimeout       = 120000;

const uint TV::kEndOfPlaybackCheckFrequency  = 250;
const uint TV::kEndOfRecPromptCheckFrequency = 250;
const uint TV::kEmbedCheckFrequency          = 250;
const uint TV::kSpeedChangeCheckFrequency    = 250;
const uint TV::kErrorRecoveryCheckFrequency  = 250;
#ifdef USING_VALGRIND
const uint TV::kEndOfPlaybackFirstCheckTimer = 60000;
#else
const uint TV::kEndOfPlaybackFirstCheckTimer = 5000;
#endif
const uint TV::kSaveLastPlayPosTimeout       = 30000;

/**
 * \brief stores last program info. maintains info so long as
 * mythfrontend is active
 */
QStringList TV::lastProgramStringList = QStringList();

/**
 * \brief function pointer for RunPlaybackBox in playbackbox.cpp
 */
EMBEDRETURNVOID TV::RunPlaybackBoxPtr = nullptr;

/**
 * \brief function pointer for RunViewScheduled in viewscheduled.cpp
 */
EMBEDRETURNVOID TV::RunViewScheduledPtr = nullptr;

/**
 * \brief function pointer for RunScheduleEditor in scheduleeditor.cpp
 */
EMBEDRETURNVOIDSCHEDIT TV::RunScheduleEditorPtr = nullptr;

/**
 * \brief function pointer for RunProgramGuide in guidegrid.cpp
 */
EMBEDRETURNVOIDEPG TV::RunProgramGuidePtr = nullptr;

/**
 * \brief function pointer for RunProgramFinder in progfind.cpp
 */
EMBEDRETURNVOIDFINDER TV::RunProgramFinderPtr = nullptr;

static const MenuBase dummy_menubase;

class MenuNodeTuple
{
public:
    MenuNodeTuple(const MenuBase &menu, const QDomNode &node) :
        m_menu(menu), m_node(node) {}
    MenuNodeTuple() : m_menu(dummy_menubase)
        {
            assert("Should never be reached.");
        }
    const MenuBase &m_menu;
    const QDomNode  m_node;
};

Q_DECLARE_METATYPE(MenuNodeTuple)

/**
 * \brief If any cards are configured, return the number.
 */
int TV::ConfiguredTunerCards()
{
    int count = 0;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT COUNT(cardid) FROM capturecard;");
    if (query.exec() && query.isActive() && query.size() && query.next())
        count = query.value(0).toInt();

    LOG(VB_RECORD, LOG_INFO,
        "ConfiguredTunerCards() = " + QString::number(count));

    return count;
}

/*! \brief Statically create, destroy or check the existence of the TV instance
 *
 * \param RefCount Will be set to the reference count for the return object
 * \param Acquire Increase refcount (or create) if true; reduce refcount (and possibly
 * delete) for false
 * \param Create If false (default) do not create TV if it does not exist and retun nullptr
*/
TV* TV::AcquireRelease(int& RefCount, bool Acquire, bool Create /*=false*/)
{
    static QMutex s_lock;
    static TV*    s_tv = nullptr;
    QMutexLocker locker(&s_lock);

    if (Acquire)
    {
        if (!s_tv && Create)
            s_tv = new TV(GetMythMainWindow());
        else if (s_tv)
            s_tv->IncrRef();
    }
    else
    {
        if (!s_tv)
            LOG(VB_GENERAL, LOG_ERR, LOC + "Ref count error");
        else
            if (s_tv->DecrRef() == 0)
                s_tv = nullptr;
    }

    if (s_tv)
        RefCount = s_tv->m_referenceCount;
    else
        RefCount = 0;
    return s_tv;
}

/*! \brief Check whether media is currently playing
 *
 * This is currently used by the frontend service API and MythAirplayServer
*/
bool TV::IsTVRunning()
{
    bool result = false;
    int dummy = 0;
    TV* tv = AcquireRelease(dummy, true);
    if (tv)
    {
        result = true;
        AcquireRelease(dummy, false);
    }
    return result;
}

/*! \brief Return a pointer to TV::m_playerContext
 *
 * \note This should NOT be called from anything other than the main thread and
 * GetPlayerReadLock or GetPlayerWriteLock should be called first (and ReleasePlayerLock
 * called when finished).
 * \note This method is likely to be changed and/or removed.
*/
PlayerContext* TV::GetPlayerContext()
{
    return &m_playerContext;
}

void TV::StopPlayback()
{
    GetPlayerReadLock();
    PrepareToExitPlayer(__LINE__);
    SetExitPlayer(true, true);
    ReturnPlayerLock();
    gCoreContext->TVInWantingPlayback(true);
}

/*! \brief Start playback of media
 *
 * This is the single global entry point for playing media.
 *
 * This is a 'blocking' call i.e. we do not actually return to the main event
 * loop until this function returns. Playback is handled manually from this function
 * with regular calls to QCoreApplication::processEvents from within PlaybackLoop.
 *
 * \returns true if the recording completed when exiting.
 */
bool TV::StartTV(ProgramInfo* TVRec, uint Flags, const ChannelInfoList& Selection)
{
    int refs = 0;
    TV* tv = AcquireRelease(refs, true, true);
    // handle existing TV object atomically
    if (refs > 1)
    {
        AcquireRelease(refs, false);
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Already have a TV object.");
        gCoreContext->emitTVPlaybackAborted();
        return false;
    }

    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "-- begin");
    bool inPlaylist = (Flags & kStartTVInPlayList) != 0U;
    bool initByNetworkCommand = (Flags & kStartTVByNetworkCommand) != 0U;
    bool quitAll = false;
    bool showDialogs = true;
    bool playCompleted = false;
    ProgramInfo *curProgram = nullptr;
    bool startSysEventSent = false;
    bool startLivetvEventSent = false;

    if (TVRec)
    {
        curProgram = new ProgramInfo(*TVRec);
        curProgram->SetIgnoreBookmark((Flags & kStartTVIgnoreBookmark) != 0U);
        curProgram->SetIgnoreProgStart((Flags & kStartTVIgnoreProgStart) != 0U);
        curProgram->SetAllowLastPlayPos((Flags & kStartTVAllowLastPlayPos) != 0U);
    }

    // Initialize TV
    if (!tv->Init())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed initializing TV");
        AcquireRelease(refs, false);
        delete curProgram;
        gCoreContext->emitTVPlaybackAborted();
        return false;
    }

    if (!lastProgramStringList.empty())
    {
        ProgramInfo pginfo(lastProgramStringList);
        if (pginfo.HasPathname() || pginfo.GetChanID())
            tv->SetLastProgram(&pginfo);
    }

    // Notify others that we are about to play
    gCoreContext->WantingPlayback(tv);

    QString playerError;
    while (!quitAll)
    {
        if (curProgram)
        {
            LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "tv->Playback() -- begin");
            if (!tv->Playback(*curProgram))
            {
                quitAll = true;
            }
            else if (!startSysEventSent)
            {
                startSysEventSent = true;
                SendMythSystemPlayEvent("PLAY_STARTED", curProgram);
            }

            LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "tv->Playback() -- end");
        }
        else if (RemoteGetFreeRecorderCount())
        {
            LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "tv->LiveTV() -- begin");
            if (!tv->LiveTV(showDialogs, Selection))
            {
                tv->SetExitPlayer(true, true);
                quitAll = true;
            }
            else if (!startSysEventSent)
            {
                startSysEventSent = true;
                startLivetvEventSent = true;
                gCoreContext->SendSystemEvent("LIVETV_STARTED");
            }

            LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "tv->LiveTV() -- end");
        }
        else
        {
            if (!ConfiguredTunerCards())
                LOG(VB_GENERAL, LOG_ERR, LOC + "No tuners configured");
            else
                LOG(VB_GENERAL, LOG_ERR, LOC + "No tuners free for live tv");
            quitAll = true;
            continue;
        }

        tv->SetInPlayList(inPlaylist);
        tv->setUnderNetworkControl(initByNetworkCommand);

        gCoreContext->emitTVPlaybackStarted();

        // Process Events
        LOG(VB_GENERAL, LOG_INFO, LOC + "Entering main playback loop.");
        tv->PlaybackLoop();
        LOG(VB_GENERAL, LOG_INFO, LOC + "Exiting main playback loop.");

        if (tv->GetJumpToProgram())
        {
            ProgramInfo *nextProgram = tv->GetLastProgram();

            tv->SetLastProgram(curProgram);
            delete curProgram;
            curProgram = nextProgram;

            SendMythSystemPlayEvent("PLAY_CHANGED", curProgram);
            continue;
        }

        tv->GetPlayerReadLock();
        PlayerContext* context = tv->GetPlayerContext();
        quitAll = tv->m_wantsToQuit || (context->m_errored);
        context->LockDeletePlayer(__FILE__, __LINE__);
        if (context->m_player && context->m_player->IsErrored())
            playerError = context->m_player->GetError();
        context->UnlockDeletePlayer(__FILE__, __LINE__);
        tv->ReturnPlayerLock();
        quitAll |= !playerError.isEmpty();
    }

    QCoreApplication::processEvents();

    // check if the show has reached the end.
    if (TVRec && tv->GetEndOfRecording())
        playCompleted = true;

    bool allowrerecord = tv->GetAllowRerecord();
    bool deleterecording = tv->m_requestDelete;
    AcquireRelease(refs, false);
    gCoreContext->emitTVPlaybackStopped();
    gCoreContext->TVInWantingPlayback(false);

    if (curProgram)
    {
        if (startSysEventSent)
            SendMythSystemPlayEvent("PLAY_STOPPED", curProgram);

        if (deleterecording)
        {
            QStringList list;
            list.push_back(QString::number(curProgram->GetRecordingID()));
            list.push_back("0"); // do not force delete
            list.push_back(allowrerecord ? "1" : "0");
            MythEvent me("LOCAL_PBB_DELETE_RECORDINGS", list);
            gCoreContext->dispatch(me);
        }
        else if (curProgram->IsRecording())
        {
            lastProgramStringList.clear();
            curProgram->ToStringList(lastProgramStringList);
        }

        delete curProgram;
    }
    else if (startSysEventSent)
        gCoreContext->SendSystemEvent("PLAY_STOPPED");

    if (!playerError.isEmpty())
    {
        MythScreenStack *ss = GetMythMainWindow()->GetStack("popup stack");
        auto *dlg = new MythConfirmationDialog(ss, playerError, false);
        if (!dlg->Create())
            delete dlg;
        else
            ss->AddScreen(dlg);
    }

    if (startLivetvEventSent)
        gCoreContext->SendSystemEvent("LIVETV_ENDED");

    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "-- end");

    return playCompleted;
}

/**
 * \brief Import pointers to functions used to embed the TV
 * window into other containers e.g. playbackbox
 */
void TV::SetFuncPtr(const char* Name, void* Pointer)
{
    QString name(Name);
    if (name == "playbackbox")
        RunPlaybackBoxPtr = reinterpret_cast<EMBEDRETURNVOID>(Pointer);
    else if (name == "viewscheduled")
        RunViewScheduledPtr = reinterpret_cast<EMBEDRETURNVOID>(Pointer);
    else if (name == "programguide")
        RunProgramGuidePtr = reinterpret_cast<EMBEDRETURNVOIDEPG>(Pointer);
    else if (name == "programfinder")
        RunProgramFinderPtr = reinterpret_cast<EMBEDRETURNVOIDFINDER>(Pointer);
    else if (name == "scheduleeditor")
        RunScheduleEditorPtr = reinterpret_cast<EMBEDRETURNVOIDSCHEDIT>(Pointer);
}

void TV::InitKeys()
{
    REG_KEY("TV Frontend", ACTION_PLAYBACK, QT_TRANSLATE_NOOP("MythControls",
            "Play Program"), "P");
    REG_KEY("TV Frontend", ACTION_STOP, QT_TRANSLATE_NOOP("MythControls",
            "Stop Program"), "");
    REG_KEY("TV Frontend", ACTION_TOGGLERECORD, QT_TRANSLATE_NOOP("MythControls",
            "Toggle recording status of current program"), "R");
    REG_KEY("TV Frontend", ACTION_DAYLEFT, QT_TRANSLATE_NOOP("MythControls",
            "Page the program guide back one day"), "Home");
    REG_KEY("TV Frontend", ACTION_DAYRIGHT, QT_TRANSLATE_NOOP("MythControls",
            "Page the program guide forward one day"), "End");
    REG_KEY("TV Frontend", ACTION_PAGELEFT, QT_TRANSLATE_NOOP("MythControls",
            "Page the program guide left"), ",,<");
    REG_KEY("TV Frontend", ACTION_PAGERIGHT, QT_TRANSLATE_NOOP("MythControls",
            "Page the program guide right"), ">,.");
    REG_KEY("TV Frontend", ACTION_TOGGLEFAV, QT_TRANSLATE_NOOP("MythControls",
            "Toggle the current channel as a favorite"), "?");
    REG_KEY("TV Frontend", ACTION_TOGGLEPGORDER, QT_TRANSLATE_NOOP("MythControls",
            "Reverse the channel order in the program guide"), "");
    REG_KEY("TV Frontend", ACTION_GUIDE, QT_TRANSLATE_NOOP("MythControls",
            "Show the Program Guide"), "S");
    REG_KEY("TV Frontend", ACTION_FINDER, QT_TRANSLATE_NOOP("MythControls",
            "Show the Program Finder"), "#");
    REG_KEY("TV Frontend", ACTION_CHANNELSEARCH, QT_TRANSLATE_NOOP("MythControls",
            "Show the Channel Search"), "");
    REG_KEY("TV Frontend", "NEXTFAV", QT_TRANSLATE_NOOP("MythControls",
            "Cycle through channel groups and all channels in the "
            "program guide."), "/");
    REG_KEY("TV Frontend", "CHANUPDATE", QT_TRANSLATE_NOOP("MythControls",
            "Switch channels without exiting guide in Live TV mode."), "X");
    REG_KEY("TV Frontend", ACTION_VOLUMEDOWN, QT_TRANSLATE_NOOP("MythControls",
            "Volume down"), "[,{,F10,Volume Down");
    REG_KEY("TV Frontend", ACTION_VOLUMEUP, QT_TRANSLATE_NOOP("MythControls",
            "Volume up"), "],},F11,Volume Up");
    REG_KEY("TV Frontend", ACTION_MUTEAUDIO, QT_TRANSLATE_NOOP("MythControls",
            "Mute"), "|,\\,F9,Volume Mute");
    REG_KEY("TV Frontend", "CYCLEAUDIOCHAN", QT_TRANSLATE_NOOP("MythControls",
            "Cycle audio channels"), "");
    REG_KEY("TV Frontend", "RANKINC", QT_TRANSLATE_NOOP("MythControls",
            "Increase program or channel rank"), "Right");
    REG_KEY("TV Frontend", "RANKDEC", QT_TRANSLATE_NOOP("MythControls",
            "Decrease program or channel rank"), "Left");
    REG_KEY("TV Frontend", "UPCOMING", QT_TRANSLATE_NOOP("MythControls",
            "List upcoming episodes"), "O");
    REG_KEY("TV Frontend", ACTION_VIEWSCHEDULED, QT_TRANSLATE_NOOP("MythControls",
            "List scheduled upcoming episodes"), "");
    REG_KEY("TV Frontend", ACTION_PREVRECORDED, QT_TRANSLATE_NOOP("MythControls",
            "List previously recorded episodes"), "");
    REG_KEY("TV Frontend", "DETAILS", QT_TRANSLATE_NOOP("MythControls",
            "Show details"), "U");
    REG_KEY("TV Frontend", "VIEWINPUT", QT_TRANSLATE_NOOP("MythControls",
            "Switch Recording Input view"), "C");
    REG_KEY("TV Frontend", "CUSTOMEDIT", QT_TRANSLATE_NOOP("MythControls",
            "Edit Custom Record Rule"), "");
    REG_KEY("TV Frontend", "CHANGERECGROUP", QT_TRANSLATE_NOOP("MythControls",
            "Change Recording Group"), "");
    REG_KEY("TV Frontend", "CHANGEGROUPVIEW", QT_TRANSLATE_NOOP("MythControls",
            "Change Group View"), "");
    REG_KEY("TV Frontend", ACTION_LISTRECORDEDEPISODES, QT_TRANSLATE_NOOP("MythControls",
            "List recorded episodes"), "");
    /*
     * TODO DB update needs to perform the necessary conversion and delete
     * the following upgrade code and replace bkmKeys and togBkmKeys  with "" in the
     * REG_KEY for ACTION_SETBOOKMARK and ACTION_TOGGLEBOOKMARK.
     */
    // Bookmarks - Instead of SELECT to add or toggle,
    // Use separate bookmark actions. This code is to convert users
    // who may already be using SELECT. If they are not already using
    // this frontend then nothing will be assigned to bookmark actions.
    QString bkmKeys;
    QString togBkmKeys;
    // Check if this is a new frontend - if PAUSE returns
    // "?" then frontend is new, never used before, so we will not assign
    // any default bookmark keys
    QString testKey = MythMainWindow::GetKey("TV Playback", ACTION_PAUSE);
    if (testKey != "?")
    {
        int alternate = gCoreContext->GetNumSetting("AltClearSavedPosition",0);
        QString selectKeys = MythMainWindow::GetKey("Global", ACTION_SELECT);
        if (selectKeys != "?")
        {
            if (alternate)
                togBkmKeys = selectKeys;
            else
                bkmKeys = selectKeys;
        }
    }
    REG_KEY("TV Playback", ACTION_SETBOOKMARK, QT_TRANSLATE_NOOP("MythControls",
            "Add Bookmark"), bkmKeys);
    REG_KEY("TV Playback", ACTION_TOGGLEBOOKMARK, QT_TRANSLATE_NOOP("MythControls",
            "Toggle Bookmark"), togBkmKeys);
    REG_KEY("TV Playback", "BACK", QT_TRANSLATE_NOOP("MythControls",
            "Exit or return to DVD menu"), "Esc");
    REG_KEY("TV Playback", ACTION_MENUCOMPACT, QT_TRANSLATE_NOOP("MythControls",
            "Playback Compact Menu"), "Alt+M");
    REG_KEY("TV Playback", ACTION_CLEAROSD, QT_TRANSLATE_NOOP("MythControls",
            "Clear OSD"), "Backspace");
    REG_KEY("TV Playback", ACTION_PAUSE, QT_TRANSLATE_NOOP("MythControls",
            "Pause"), "P,Space");
    REG_KEY("TV Playback", ACTION_SEEKFFWD, QT_TRANSLATE_NOOP("MythControls",
            "Fast Forward"), "Right");
    REG_KEY("TV Playback", ACTION_SEEKRWND, QT_TRANSLATE_NOOP("MythControls",
            "Rewind"), "Left");
    REG_KEY("TV Playback", ACTION_SEEKARB, QT_TRANSLATE_NOOP("MythControls",
            "Arbitrary Seek"), "*");
    REG_KEY("TV Playback", ACTION_SEEKABSOLUTE, QT_TRANSLATE_NOOP("MythControls",
            "Seek to a position in seconds"), "");
    REG_KEY("TV Playback", ACTION_CHANNELUP, QT_TRANSLATE_NOOP("MythControls",
            "Channel up"), "Up");
    REG_KEY("TV Playback", ACTION_CHANNELDOWN, QT_TRANSLATE_NOOP("MythControls",
            "Channel down"), "Down");
    REG_KEY("TV Playback", "NEXTFAV", QT_TRANSLATE_NOOP("MythControls",
            "Switch to the next favorite channel"), "/");
    REG_KEY("TV Playback", "PREVCHAN", QT_TRANSLATE_NOOP("MythControls",
            "Switch to the previous channel"), "H");
    REG_KEY("TV Playback", ACTION_JUMPFFWD, QT_TRANSLATE_NOOP("MythControls",
            "Jump ahead"), "PgDown");
    REG_KEY("TV Playback", ACTION_JUMPRWND, QT_TRANSLATE_NOOP("MythControls",
            "Jump back"), "PgUp");
    REG_KEY("TV Playback", "INFOWITHCUTLIST", QT_TRANSLATE_NOOP("MythControls",
            "Info utilizing cutlist"), "");
    REG_KEY("TV Playback", ACTION_JUMPBKMRK, QT_TRANSLATE_NOOP("MythControls",
            "Jump to bookmark"), "K");
    REG_KEY("TV Playback", "FFWDSTICKY", QT_TRANSLATE_NOOP("MythControls",
            "Fast Forward (Sticky) or Forward one second while paused"), ">,.");
    REG_KEY("TV Playback", "RWNDSTICKY", QT_TRANSLATE_NOOP("MythControls",
            "Rewind (Sticky) or Rewind one second while paused"), ",,<");
    REG_KEY("TV Playback", "NEXTSOURCE", QT_TRANSLATE_NOOP("MythControls",
            "Next Video Source"), "Y");
    REG_KEY("TV Playback", "PREVSOURCE", QT_TRANSLATE_NOOP("MythControls",
            "Previous Video Source"), "");
    REG_KEY("TV Playback", "NEXTINPUT", QT_TRANSLATE_NOOP("MythControls",
            "Next Input"), "C");
    REG_KEY("TV Playback", "NEXTCARD", QT_TRANSLATE_NOOP("MythControls",
            "Next Card"), "");
    REG_KEY("TV Playback", "SKIPCOMMERCIAL", QT_TRANSLATE_NOOP("MythControls",
            "Skip Commercial"), "Z,End");
    REG_KEY("TV Playback", "SKIPCOMMBACK", QT_TRANSLATE_NOOP("MythControls",
            "Skip Commercial (Reverse)"), "Q,Home");
    REG_KEY("TV Playback", ACTION_JUMPSTART, QT_TRANSLATE_NOOP("MythControls",
            "Jump to the start of the recording."), "Ctrl+B");
    REG_KEY("TV Playback", "TOGGLEBROWSE", QT_TRANSLATE_NOOP("MythControls",
            "Toggle channel browse mode"), "O");
    REG_KEY("TV Playback", ACTION_TOGGLERECORD, QT_TRANSLATE_NOOP("MythControls",
            "Toggle recording status of current program"), "R");
    REG_KEY("TV Playback", ACTION_TOGGLEFAV, QT_TRANSLATE_NOOP("MythControls",
            "Toggle the current channel as a favorite"), "?");
    REG_KEY("TV Playback", ACTION_VOLUMEDOWN, QT_TRANSLATE_NOOP("MythControls",
            "Volume down"), "[,{,F10,Volume Down");
    REG_KEY("TV Playback", ACTION_VOLUMEUP, QT_TRANSLATE_NOOP("MythControls",
            "Volume up"), "],},F11,Volume Up");
    REG_KEY("TV Playback", ACTION_MUTEAUDIO, QT_TRANSLATE_NOOP("MythControls",
            "Mute"), "|,\\,F9,Volume Mute");
    REG_KEY("TV Playback", ACTION_SETVOLUME, QT_TRANSLATE_NOOP("MythControls",
            "Set the volume"), "");
    REG_KEY("TV Playback", "CYCLEAUDIOCHAN", QT_TRANSLATE_NOOP("MythControls",
            "Cycle audio channels"), "");
    REG_KEY("TV Playback", ACTION_TOGGLEUPMIX, QT_TRANSLATE_NOOP("MythControls",
            "Toggle audio upmixer"), "Ctrl+U");
    REG_KEY("TV Playback", ACTION_BOTTOMLINEMOVE,
            QT_TRANSLATE_NOOP("MythControls", "Move BottomLine off screen"), "L");
    REG_KEY("TV Playback", ACTION_BOTTOMLINESAVE,
            QT_TRANSLATE_NOOP("MythControls", "Save manual zoom for BottomLine"), "");
    REG_KEY("TV Playback", "TOGGLEASPECT", QT_TRANSLATE_NOOP("MythControls",
            "Toggle the video aspect ratio"), "Ctrl+W");
    REG_KEY("TV Playback", "TOGGLEFILL", QT_TRANSLATE_NOOP("MythControls",
            "Next Preconfigured Zoom mode"), "W");
    REG_KEY("TV Playback", ACTION_TOGGLESUBS, QT_TRANSLATE_NOOP("MythControls",
            "Toggle any captions"), "T");
    REG_KEY("TV Playback", ACTION_ENABLESUBS, QT_TRANSLATE_NOOP("MythControls",
            "Enable any captions"), "");
    REG_KEY("TV Playback", ACTION_DISABLESUBS, QT_TRANSLATE_NOOP("MythControls",
            "Disable any captions"), "");
    REG_KEY("TV Playback", "TOGGLETTC", QT_TRANSLATE_NOOP("MythControls",
            "Toggle Teletext Captions"),"");
    REG_KEY("TV Playback", "TOGGLESUBTITLE", QT_TRANSLATE_NOOP("MythControls",
            "Toggle Subtitles"), "");
    REG_KEY("TV Playback", "TOGGLECC608", QT_TRANSLATE_NOOP("MythControls",
            "Toggle VBI CC"), "");
    REG_KEY("TV Playback", "TOGGLECC708", QT_TRANSLATE_NOOP("MythControls",
            "Toggle ATSC CC"), "");
    REG_KEY("TV Playback", "TOGGLETTM", QT_TRANSLATE_NOOP("MythControls",
            "Toggle Teletext Menu"), "");
    REG_KEY("TV Playback", ACTION_TOGGLEEXTTEXT, QT_TRANSLATE_NOOP("MythControls",
            "Toggle External Subtitles"), "");
    REG_KEY("TV Playback", ACTION_ENABLEEXTTEXT, QT_TRANSLATE_NOOP("MythControls",
            "Enable External Subtitles"), "");
    REG_KEY("TV Playback", ACTION_DISABLEEXTTEXT, QT_TRANSLATE_NOOP("MythControls",
            "Disable External Subtitles"), "");
    REG_KEY("TV Playback", "TOGGLERAWTEXT", QT_TRANSLATE_NOOP("MythControls",
            "Toggle Text Subtitles"), "");

    REG_KEY("TV Playback", "SELECTAUDIO_0", QT_TRANSLATE_NOOP("MythControls",
            "Play audio track 1"), "");
    REG_KEY("TV Playback", "SELECTAUDIO_1", QT_TRANSLATE_NOOP("MythControls",
            "Play audio track 2"), "");
    REG_KEY("TV Playback", "SELECTSUBTITLE_0",QT_TRANSLATE_NOOP("MythControls",
            "Display subtitle 1"), "");
    REG_KEY("TV Playback", "SELECTSUBTITLE_1",QT_TRANSLATE_NOOP("MythControls",
            "Display subtitle 2"), "");
    REG_KEY("TV Playback", "SELECTRAWTEXT_0",QT_TRANSLATE_NOOP("MythControls",
            "Display Text Subtitle 1"), "");
    REG_KEY("TV Playback", "SELECTCC608_0", QT_TRANSLATE_NOOP("MythControls",
            "Display VBI CC1"), "");
    REG_KEY("TV Playback", "SELECTCC608_1", QT_TRANSLATE_NOOP("MythControls",
            "Display VBI CC2"), "");
    REG_KEY("TV Playback", "SELECTCC608_2", QT_TRANSLATE_NOOP("MythControls",
            "Display VBI CC3"), "");
    REG_KEY("TV Playback", "SELECTCC608_3", QT_TRANSLATE_NOOP("MythControls",
            "Display VBI CC4"), "");
    REG_KEY("TV Playback", "SELECTCC708_0", QT_TRANSLATE_NOOP("MythControls",
            "Display ATSC CC1"), "");
    REG_KEY("TV Playback", "SELECTCC708_1", QT_TRANSLATE_NOOP("MythControls",
            "Display ATSC CC2"), "");
    REG_KEY("TV Playback", "SELECTCC708_2", QT_TRANSLATE_NOOP("MythControls",
            "Display ATSC CC3"), "");
    REG_KEY("TV Playback", "SELECTCC708_3", QT_TRANSLATE_NOOP("MythControls",
            "Display ATSC CC4"), "");
    REG_KEY("TV Playback", ACTION_ENABLEFORCEDSUBS, QT_TRANSLATE_NOOP("MythControls",
            "Enable Forced Subtitles"), "");
    REG_KEY("TV Playback", ACTION_DISABLEFORCEDSUBS, QT_TRANSLATE_NOOP("MythControls",
            "Disable Forced Subtitles"), "");

    REG_KEY("TV Playback", "NEXTAUDIO", QT_TRANSLATE_NOOP("MythControls",
            "Next audio track"), "+");
    REG_KEY("TV Playback", "PREVAUDIO", QT_TRANSLATE_NOOP("MythControls",
            "Previous audio track"), "-");
    REG_KEY("TV Playback", "NEXTSUBTITLE", QT_TRANSLATE_NOOP("MythControls",
            "Next subtitle track"), "");
    REG_KEY("TV Playback", "PREVSUBTITLE", QT_TRANSLATE_NOOP("MythControls",
            "Previous subtitle track"), "");
    REG_KEY("TV Playback", "NEXTRAWTEXT", QT_TRANSLATE_NOOP("MythControls",
            "Next Text track"), "");
    REG_KEY("TV Playback", "PREVRAWTEXT", QT_TRANSLATE_NOOP("MythControls",
            "Previous Text track"), "");
    REG_KEY("TV Playback", "NEXTCC608", QT_TRANSLATE_NOOP("MythControls",
            "Next VBI CC track"), "");
    REG_KEY("TV Playback", "PREVCC608", QT_TRANSLATE_NOOP("MythControls",
            "Previous VBI CC track"), "");
    REG_KEY("TV Playback", "NEXTCC708", QT_TRANSLATE_NOOP("MythControls",
            "Next ATSC CC track"), "");
    REG_KEY("TV Playback", "PREVCC708", QT_TRANSLATE_NOOP("MythControls",
            "Previous ATSC CC track"), "");
    REG_KEY("TV Playback", "NEXTCC", QT_TRANSLATE_NOOP("MythControls",
            "Next of any captions"), "");

    REG_KEY("TV Playback", "NEXTSCAN", QT_TRANSLATE_NOOP("MythControls",
            "Next video scan overidemode"), "");
    REG_KEY("TV Playback", "QUEUETRANSCODE", QT_TRANSLATE_NOOP("MythControls",
            "Queue the current recording for transcoding"), "X");
    REG_KEY("TV Playback", "SPEEDINC", QT_TRANSLATE_NOOP("MythControls",
            "Increase the playback speed"), "U");
    REG_KEY("TV Playback", "SPEEDDEC", QT_TRANSLATE_NOOP("MythControls",
            "Decrease the playback speed"), "J");
    REG_KEY("TV Playback", "ADJUSTSTRETCH", QT_TRANSLATE_NOOP("MythControls",
            "Turn on time stretch control"), "A");
    REG_KEY("TV Playback", "STRETCHINC", QT_TRANSLATE_NOOP("MythControls",
            "Increase time stretch speed"), "");
    REG_KEY("TV Playback", "STRETCHDEC", QT_TRANSLATE_NOOP("MythControls",
            "Decrease time stretch speed"), "");
    REG_KEY("TV Playback", "TOGGLESTRETCH", QT_TRANSLATE_NOOP("MythControls",
            "Toggle time stretch speed"), "");
    REG_KEY("TV Playback", ACTION_TOGGELAUDIOSYNC,
            QT_TRANSLATE_NOOP("MythControls",
            "Turn on audio sync adjustment controls"), "");
    REG_KEY("TV Playback", ACTION_SETAUDIOSYNC,
            QT_TRANSLATE_NOOP("MythControls",
            "Set the audio sync adjustment"), "");
    REG_KEY("TV Playback", "TOGGLEPICCONTROLS",
            QT_TRANSLATE_NOOP("MythControls", "Playback picture adjustments"),
             "F");
    REG_KEY("TV Playback", ACTION_TOGGLENIGHTMODE,
            QT_TRANSLATE_NOOP("MythControls", "Toggle night mode"), "Ctrl+F");
    REG_KEY("TV Playback", ACTION_SETBRIGHTNESS,
            QT_TRANSLATE_NOOP("MythControls", "Set the picture brightness"), "");
    REG_KEY("TV Playback", ACTION_SETCONTRAST,
            QT_TRANSLATE_NOOP("MythControls", "Set the picture contrast"), "");
    REG_KEY("TV Playback", ACTION_SETCOLOUR,
            QT_TRANSLATE_NOOP("MythControls", "Set the picture color"), "");
    REG_KEY("TV Playback", ACTION_SETHUE,
            QT_TRANSLATE_NOOP("MythControls", "Set the picture hue"), "");
    REG_KEY("TV Playback", ACTION_TOGGLECHANCONTROLS,
            QT_TRANSLATE_NOOP("MythControls", "Recording picture adjustments "
            "for this channel"), "Ctrl+G");
    REG_KEY("TV Playback", ACTION_TOGGLERECCONTROLS,
            QT_TRANSLATE_NOOP("MythControls", "Recording picture adjustments "
            "for this recorder"), "G");
    REG_KEY("TV Playback", "CYCLECOMMSKIPMODE",
            QT_TRANSLATE_NOOP("MythControls", "Cycle Commercial Skip mode"),
            "");
    REG_KEY("TV Playback", ACTION_GUIDE, QT_TRANSLATE_NOOP("MythControls",
            "Show the Program Guide"), "S");
    REG_KEY("TV Playback", ACTION_FINDER, QT_TRANSLATE_NOOP("MythControls",
            "Show the Program Finder"), "#");
    REG_KEY("TV Playback", ACTION_TOGGLESLEEP, QT_TRANSLATE_NOOP("MythControls",
            "Toggle the Sleep Timer"), "F8");
    REG_KEY("TV Playback", ACTION_PLAY, QT_TRANSLATE_NOOP("MythControls", "Play"),
            "Ctrl+P");
    REG_KEY("TV Playback", ACTION_JUMPPREV, QT_TRANSLATE_NOOP("MythControls",
            "Jump to previously played recording"), "");
    REG_KEY("TV Playback", ACTION_JUMPREC, QT_TRANSLATE_NOOP("MythControls",
            "Display menu of recorded programs to jump to"), "");
    REG_KEY("TV Playback", ACTION_VIEWSCHEDULED, QT_TRANSLATE_NOOP("MythControls",
            "Display scheduled recording list"), "");
    REG_KEY("TV Playback", ACTION_PREVRECORDED, QT_TRANSLATE_NOOP("MythControls",
            "Display previously recorded episodes"), "");
    REG_KEY("TV Playback", ACTION_SIGNALMON, QT_TRANSLATE_NOOP("MythControls",
            "Monitor Signal Quality"), "Alt+F7");
    REG_KEY("TV Playback", ACTION_JUMPTODVDROOTMENU,
            QT_TRANSLATE_NOOP("MythControls", "Jump to the DVD Root Menu"), "");
    REG_KEY("TV Playback", ACTION_JUMPTOPOPUPMENU,
            QT_TRANSLATE_NOOP("MythControls", "Jump to the Popup Menu"), "");
    REG_KEY("TV Playback", ACTION_JUMPTODVDCHAPTERMENU,
            QT_TRANSLATE_NOOP("MythControls", "Jump to the DVD Chapter Menu"), "");
    REG_KEY("TV Playback", ACTION_JUMPTODVDTITLEMENU,
            QT_TRANSLATE_NOOP("MythControls", "Jump to the DVD Title Menu"), "");
    REG_KEY("TV Playback", ACTION_EXITSHOWNOPROMPTS,
            QT_TRANSLATE_NOOP("MythControls", "Exit Show without any prompts"),
            "");
    REG_KEY("TV Playback", ACTION_JUMPCHAPTER, QT_TRANSLATE_NOOP("MythControls",
            "Jump to a chapter"), "");
    REG_KEY("TV Playback", ACTION_SWITCHTITLE, QT_TRANSLATE_NOOP("MythControls",
            "Switch title"), "");
    REG_KEY("TV Playback", ACTION_SWITCHANGLE, QT_TRANSLATE_NOOP("MythControls",
            "Switch angle"), "");
    REG_KEY("TV Playback", ACTION_OSDNAVIGATION, QT_TRANSLATE_NOOP("MythControls",
            "OSD Navigation"), "");
    REG_KEY("TV Playback", ACTION_ZOOMUP, QT_TRANSLATE_NOOP("MythControls",
            "Zoom mode - shift up"), "");
    REG_KEY("TV Playback", ACTION_ZOOMDOWN, QT_TRANSLATE_NOOP("MythControls",
            "Zoom mode - shift down"), "");
    REG_KEY("TV Playback", ACTION_ZOOMLEFT, QT_TRANSLATE_NOOP("MythControls",
            "Zoom mode - shift left"), "");
    REG_KEY("TV Playback", ACTION_ZOOMRIGHT, QT_TRANSLATE_NOOP("MythControls",
            "Zoom mode - shift right"), "");
    REG_KEY("TV Playback", ACTION_ZOOMASPECTUP,
            QT_TRANSLATE_NOOP("MythControls",
            "Zoom mode - increase aspect ratio"), "");
    REG_KEY("TV Playback", ACTION_ZOOMASPECTDOWN,
            QT_TRANSLATE_NOOP("MythControls",
            "Zoom mode - decrease aspect ratio"), "");
    REG_KEY("TV Playback", ACTION_ZOOMIN, QT_TRANSLATE_NOOP("MythControls",
            "Zoom mode - zoom in"), "");
    REG_KEY("TV Playback", ACTION_ZOOMOUT, QT_TRANSLATE_NOOP("MythControls",
            "Zoom mode - zoom out"), "");
    REG_KEY("TV Playback", ACTION_ZOOMVERTICALIN,
            QT_TRANSLATE_NOOP("MythControls",
                              "Zoom mode - vertical zoom in"), "8");
    REG_KEY("TV Playback", ACTION_ZOOMVERTICALOUT,
            QT_TRANSLATE_NOOP("MythControls",
                              "Zoom mode - vertical zoom out"), "2");
    REG_KEY("TV Playback", ACTION_ZOOMHORIZONTALIN,
            QT_TRANSLATE_NOOP("MythControls",
                              "Zoom mode - horizontal zoom in"), "6");
    REG_KEY("TV Playback", ACTION_ZOOMHORIZONTALOUT,
            QT_TRANSLATE_NOOP("MythControls",
                              "Zoom mode - horizontal zoom out"), "4");
    REG_KEY("TV Playback", ACTION_ZOOMQUIT, QT_TRANSLATE_NOOP("MythControls",
            "Zoom mode - quit and abandon changes"), "");
    REG_KEY("TV Playback", ACTION_ZOOMCOMMIT, QT_TRANSLATE_NOOP("MythControls",
            "Zoom mode - commit changes"), "");

    /* Interactive Television keys */
    REG_KEY("TV Playback", ACTION_MENURED,    QT_TRANSLATE_NOOP("MythControls",
            "Menu Red"),    "F2");
    REG_KEY("TV Playback", ACTION_MENUGREEN,  QT_TRANSLATE_NOOP("MythControls",
            "Menu Green"),  "F3");
    REG_KEY("TV Playback", ACTION_MENUYELLOW, QT_TRANSLATE_NOOP("MythControls",
            "Menu Yellow"), "F4");
    REG_KEY("TV Playback", ACTION_MENUBLUE,   QT_TRANSLATE_NOOP("MythControls",
            "Menu Blue"),   "F5");
    REG_KEY("TV Playback", ACTION_TEXTEXIT,   QT_TRANSLATE_NOOP("MythControls",
            "Menu Exit"),   "F6");
    REG_KEY("TV Playback", ACTION_MENUTEXT,   QT_TRANSLATE_NOOP("MythControls",
            "Menu Text"),   "F7");
    REG_KEY("TV Playback", ACTION_MENUEPG,    QT_TRANSLATE_NOOP("MythControls",
            "Menu EPG"),    "F12");

    /* Editing keys */
    REG_KEY("TV Editing", ACTION_CLEARMAP,    QT_TRANSLATE_NOOP("MythControls",
            "Clear editing cut points"), "C,Q,Home");
    REG_KEY("TV Editing", ACTION_INVERTMAP,   QT_TRANSLATE_NOOP("MythControls",
            "Invert Begin/End cut points"),"I");
    REG_KEY("TV Editing", ACTION_SAVEMAP,     QT_TRANSLATE_NOOP("MythControls",
            "Save cuts"),"");
    REG_KEY("TV Editing", ACTION_LOADCOMMSKIP,QT_TRANSLATE_NOOP("MythControls",
            "Load cuts from detected commercials"), "Z,End");
    REG_KEY("TV Editing", ACTION_NEXTCUT,     QT_TRANSLATE_NOOP("MythControls",
            "Jump to the next cut point"), "PgDown");
    REG_KEY("TV Editing", ACTION_PREVCUT,     QT_TRANSLATE_NOOP("MythControls",
            "Jump to the previous cut point"), "PgUp");
    REG_KEY("TV Editing", ACTION_BIGJUMPREW,  QT_TRANSLATE_NOOP("MythControls",
            "Jump back 10x the normal amount"), ",,<");
    REG_KEY("TV Editing", ACTION_BIGJUMPFWD,  QT_TRANSLATE_NOOP("MythControls",
            "Jump forward 10x the normal amount"), ">,.");
    REG_KEY("TV Editing", ACTION_MENUCOMPACT, QT_TRANSLATE_NOOP("MythControls",
            "Cut point editor compact menu"), "Alt+M");

    /* Teletext keys */
    REG_KEY("Teletext Menu", ACTION_NEXTPAGE,    QT_TRANSLATE_NOOP("MythControls",
            "Next Page"),             "Down");
    REG_KEY("Teletext Menu", ACTION_PREVPAGE,    QT_TRANSLATE_NOOP("MythControls",
            "Previous Page"),         "Up");
    REG_KEY("Teletext Menu", ACTION_NEXTSUBPAGE, QT_TRANSLATE_NOOP("MythControls",
            "Next Subpage"),          "Right");
    REG_KEY("Teletext Menu", ACTION_PREVSUBPAGE, QT_TRANSLATE_NOOP("MythControls",
            "Previous Subpage"),      "Left");
    REG_KEY("Teletext Menu", ACTION_TOGGLETT,    QT_TRANSLATE_NOOP("MythControls",
            "Toggle Teletext"),       "T");
    REG_KEY("Teletext Menu", ACTION_MENURED,     QT_TRANSLATE_NOOP("MythControls",
            "Menu Red"),              "F2");
    REG_KEY("Teletext Menu", ACTION_MENUGREEN,   QT_TRANSLATE_NOOP("MythControls",
            "Menu Green"),            "F3");
    REG_KEY("Teletext Menu", ACTION_MENUYELLOW,  QT_TRANSLATE_NOOP("MythControls",
            "Menu Yellow"),           "F4");
    REG_KEY("Teletext Menu", ACTION_MENUBLUE,    QT_TRANSLATE_NOOP("MythControls",
            "Menu Blue"),             "F5");
    REG_KEY("Teletext Menu", ACTION_MENUWHITE,   QT_TRANSLATE_NOOP("MythControls",
            "Menu White"),            "F6");
    REG_KEY("Teletext Menu", ACTION_TOGGLEBACKGROUND,
            QT_TRANSLATE_NOOP("MythControls", "Toggle Background"), "F7");
    REG_KEY("Teletext Menu", ACTION_REVEAL,      QT_TRANSLATE_NOOP("MythControls",
            "Reveal hidden Text"),    "F8");

    /* Visualisations */
    REG_KEY("TV Playback", ACTION_TOGGLEVISUALISATION,
            QT_TRANSLATE_NOOP("MythControls", "Toggle audio visualisation"), "");

    /* OSD playback information screen */
    REG_KEY("TV Playback", ACTION_TOGGLEOSDDEBUG,
            QT_TRANSLATE_NOOP("MythControls", "Toggle OSD playback information"), "");

    /* 3D/Frame compatible/Stereoscopic TV */
    REG_KEY("TV Playback", ACTION_3DNONE,
            QT_TRANSLATE_NOOP("MythControls", "Auto 3D"), "");
    REG_KEY("TV Playback", ACTION_3DIGNORE,
            QT_TRANSLATE_NOOP("MythControls", "Ignore 3D"), "");
    REG_KEY("TV Playback", ACTION_3DSIDEBYSIDEDISCARD,
            QT_TRANSLATE_NOOP("MythControls", "Discard 3D Side by Side"), "");
    REG_KEY("TV Playback", ACTION_3DTOPANDBOTTOMDISCARD,
            QT_TRANSLATE_NOOP("MythControls", "Discard 3D Top and Bottom"), "");

/*
  keys already used:

  Global:           I   M              0123456789
  Playback: ABCDEFGH JK  NOPQRSTUVWXYZ
  Frontend:   CD          OP R  U  XY  01 3   7 9
  Editing:    C E   I       Q        Z
  Teletext:                    T

  Playback: <>,.?/|[]{}\+-*#^
  Frontend: <>,.?/
  Editing:  <>,.

  Global:   PgDown, PgUp,  Right, Left, Home, End, Up, Down,
  Playback: PgDown, PgUp,  Right, Left, Home, End, Up, Down, Backspace,
  Frontend:                Right, Left, Home, End
  Editing:  PgDown, PgUp,               Home, End
  Teletext:                Right, Left,            Up, Down,

  Global:   Return, Enter, Space, Esc

  Global:   F1,
  Playback:                   F7,F8,F9,F10,F11
  Teletext     F2,F3,F4,F5,F6,F7,F8
  ITV          F2,F3,F4,F5,F6,F7,F12

  Playback: Ctrl-B,Ctrl-G,Ctrl-Y,Ctrl-U,L
*/
}

void TV::ReloadKeys()
{
    m_mainWindow->ClearKeyContext("TV Frontend");
    m_mainWindow->ClearKeyContext("TV Playback");
    m_mainWindow->ClearKeyContext("TV Editing");
    m_mainWindow->ClearKeyContext("Teletext Menu");
    InitKeys();
}


class TV::SleepTimerInfo
{
  public:
    SleepTimerInfo(QString String, unsigned long Seconds)
      : dispString(std::move(String)),
        seconds(Seconds) {}
    QString   dispString;
    unsigned long seconds;
};

const vector<TV::SleepTimerInfo> TV::s_sleepTimes =
{
    { tr("Off",   "Sleep timer"),      0 },
    { tr("30m",   "Sleep timer"),  30*60 },
    { tr("1h",    "Sleep timer"),  60*60 },
    { tr("1h30m", "Sleep timer"),  90*60 },
    { tr("2h",    "Sleep timer"), 120*60}
};

/*!
 *  \sa Init()
 *
 * This list is not complete:)
 *
 * \todo Remove m_playerLock. m_playerContext is always present and we should be
 * using the lower level locks in PlayerContext (around player etc)
 * \todo Move TV only state currently held in PlayerContext into TV
 * \todo Try and remove friend class usage. This is a blunt tool and allows
 * potentially unsafe calls into TV
 */
TV::TV(MythMainWindow* MainWindow)
  : ReferenceCounter("TV"),
    TVBrowseHelper(this),
    m_mainWindow(MainWindow)

{
    LOG(VB_GENERAL, LOG_INFO, LOC + "Creating TV object");
    m_ctorTime.start();

    QObject::setObjectName("TV");
    m_keyRepeatTimer.start();

    connect(this, &TV::RequestStartEmbedding, this, &TV::StartEmbedding);
    connect(this, &TV::RequestStopEmbedding,  this, &TV::StopEmbedding);

    InitFromDB();

#ifdef Q_OS_ANDROID
    connect(qApp, SIGNAL(applicationStateChanged(Qt::ApplicationState)),
            this, SLOT(onApplicationStateChange(Qt::ApplicationState)));
#endif

    if (m_mainWindow)
        m_mainWindow->PauseIdleTimer(true);

    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Finished creating TV object");
}

void TV::InitFromDB()
{
    QMap<QString,QString> kv;
    kv["LiveTVIdleTimeout"]        = "0";
    kv["BrowseMaxForward"]         = "240";
    kv["PlaybackExitPrompt"]       = "0";
    kv["AutomaticSetWatched"]      = "0";
    kv["EndOfRecordingExitPrompt"] = "0";
    kv["JumpToProgramOSD"]         = "1";
    kv["GuiSizeForTV"]             = "0";
    kv["UseVideoModes"]            = "0";
    kv["ClearSavedPosition"]       = "1";
    kv["JobsRunOnRecordHost"]      = "0";
    kv["ContinueEmbeddedTVPlay"]   = "0";
    kv["UseFixedWindowSize"]       = "1";
    kv["RunFrontendInWindow"]      = "0";
    kv["PersistentBrowseMode"]     = "0";
    kv["BrowseAllTuners"]          = "0";
    kv["ChannelOrdering"]          = "channum";

    kv["CustomFilters"]            = "";
    kv["ChannelFormat"]            = "<num> <sign>";

    kv["TryUnflaggedSkip"]         = "0";

    kv["ChannelGroupDefault"]      = "-1";
    kv["BrowseChannelGroup"]       = "0";
    kv["SmartForward"]             = "0";
    kv["FFRewReposTime"]           = "100";
    kv["FFRewReverse"]             = "1";

    kv["BrowseChannelGroup"]       = "0";
    kv["ChannelGroupDefault"]      = "-1";
    kv["ChannelGroupRememberLast"] = "0";

    kv["VbiFormat"]                = "";
    kv["DecodeVBIFormat"]          = "";

    // these need exactly 12 items, comma cant be used as it is the delimiter
    kv["PlaybackScreenPressKeyMap"]     = "P,Up,Z,],Left,Return,Return,Right,A,Down,Q,[";
    kv["LiveTVScreenPressKeyMap"]     = "P,Up,Z,S,Left,Return,Return,Right,A,Down,Q,F";

    constexpr std::array<const int,8> ff_rew_def { 3, 5, 10, 20, 30, 60, 120, 180 };
    for (size_t i = 0; i < ff_rew_def.size(); i++)
        kv[QString("FFRewSpeed%1").arg(i)] = QString::number(ff_rew_def[i]);

    MythDB::getMythDB()->GetSettings(kv);

    m_screenPressKeyMapPlayback = ConvertScreenPressKeyMap(kv["PlaybackScreenPressKeyMap"]);
    m_screenPressKeyMapLiveTV = ConvertScreenPressKeyMap(kv["LiveTVScreenPressKeyMap"]);

    QString db_channel_ordering;

    // convert from minutes to ms.
    m_dbIdleTimeout        = kv["LiveTVIdleTimeout"].toUInt() * 60 * 1000;
    uint db_browse_max_forward = kv["BrowseMaxForward"].toUInt() * 60;
    m_dbPlaybackExitPrompt = kv["PlaybackExitPrompt"].toInt();
    m_dbAutoSetWatched     = (kv["AutomaticSetWatched"].toInt() != 0);
    m_dbEndOfRecExitPrompt = (kv["EndOfRecordingExitPrompt"].toInt() != 0);
    m_dbJumpPreferOsd      = (kv["JumpToProgramOSD"].toInt() != 0);
    m_dbUseGuiSizeForTv    = (kv["GuiSizeForTV"].toInt() != 0);
    m_dbUseVideoModes      = (kv["UseVideoModes"].toInt() != 0);
    m_dbClearSavedPosition = (kv["ClearSavedPosition"].toInt() != 0);
    m_dbRunJobsOnRemote    = (kv["JobsRunOnRecordHost"].toInt() != 0);
    m_dbContinueEmbedded   = (kv["ContinueEmbeddedTVPlay"].toInt() != 0);
    m_dbRunFrontendInWindow= (kv["RunFrontendInWindow"].toInt() != 0);
    m_dbBrowseAlways       = (kv["PersistentBrowseMode"].toInt() != 0);
    m_dbBrowseAllTuners    = (kv["BrowseAllTuners"].toInt() != 0);
    db_channel_ordering    = kv["ChannelOrdering"];
    m_baseFilters         += kv["CustomFilters"];
    m_dbChannelFormat      = kv["ChannelFormat"];
    m_tryUnflaggedSkip     = (kv["TryUnflaggedSkip"].toInt() != 0);
    m_smartForward         = (kv["SmartForward"].toInt() != 0);
    m_ffRewRepos           = kv["FFRewReposTime"].toFloat() * 0.01F;
    m_ffRewReverse         = (kv["FFRewReverse"].toInt() != 0);

    m_dbUseChannelGroups   = (kv["BrowseChannelGroup"].toInt() != 0);
    m_dbRememberLastChannelGroup = (kv["ChannelGroupRememberLast"].toInt() != 0);
    m_channelGroupId       = kv["ChannelGroupDefault"].toInt();

    QString beVBI          = kv["VbiFormat"];
    QString feVBI          = kv["DecodeVBIFormat"];

    RecordingRule record;
    record.LoadTemplate("Default");
    m_dbAutoexpireDefault  = static_cast<uint>(record.m_autoExpire);

    if (m_dbUseChannelGroups)
    {
        m_dbChannelGroups  = ChannelGroup::GetChannelGroups();
        if (m_channelGroupId > -1)
        {
            m_channelGroupChannelList = ChannelUtil::GetChannels(0, true, "channum, callsign",
                                                                 static_cast<uint>(m_channelGroupId));
            ChannelUtil::SortChannels(m_channelGroupChannelList, "channum", true);
        }
    }

    for (size_t i = 0; i < sizeof(ff_rew_def)/sizeof(ff_rew_def[0]); i++)
        m_ffRewSpeeds.push_back(kv[QString("FFRewSpeed%1").arg(i)].toInt());

    // process it..
    BrowseInit(db_browse_max_forward, m_dbBrowseAllTuners, m_dbUseChannelGroups, db_channel_ordering);

    m_vbimode = VBIMode::Parse(!feVBI.isEmpty() ? feVBI : beVBI);

    gCoreContext->addListener(this);
    gCoreContext->RegisterForPlayback(this, SLOT(StopPlayback()));
}

/** \fn TV::Init(bool)
 *  \brief Performs instance initialization, returns true on success.
 *
 *  \param createWindow If true a MythDialog is created for display.
 *  \return Returns true on success, false on failure.
 */
bool TV::Init()
{
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "-- begin");

    if (!m_mainWindow)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "No MythMainWindow");
        return false;
    }

    bool fullscreen = !m_dbUseGuiSizeForTv;
    m_savedGuiBounds = QRect(m_mainWindow->geometry().topLeft(), m_mainWindow->size());

    // adjust for window manager wierdness.
    QRect screen = m_mainWindow->GetScreenRect();
    if ((abs(m_savedGuiBounds.x() - screen.left()) < 3) &&
        (abs(m_savedGuiBounds.y() - screen.top()) < 3))
    {
        m_savedGuiBounds = QRect(screen.topLeft(), m_mainWindow->size());
    }

    // if width && height are zero users expect fullscreen playback
    if (!fullscreen)
    {
        int gui_width = 0;
        int gui_height = 0;
        gCoreContext->GetResolutionSetting("Gui", gui_width, gui_height);
        fullscreen |= (0 == gui_width && 0 == gui_height);
    }

    m_playerBounds = m_savedGuiBounds;
    if (fullscreen)
        m_playerBounds = m_mainWindow->GetDisplay()->GetScreenBounds();

    // player window sizing
    MythScreenStack *mainStack = m_mainWindow->GetMainStack();

    m_myWindow = new TvPlayWindow(mainStack, "Playback");

    if (m_myWindow->Create())
    {
        mainStack->AddScreen(m_myWindow, false);
        LOG(VB_GENERAL, LOG_INFO, LOC + "Created TvPlayWindow.");
    }
    else
    {
        delete m_myWindow;
        m_myWindow = nullptr;
    }

    if (m_mainWindow->GetPaintWindow())
        m_mainWindow->GetPaintWindow()->update();
    m_mainWindow->installEventFilter(this);
    QCoreApplication::processEvents();

    GetPlayerReadLock();
    m_playerContext.m_ffRewState = 0;
    m_playerContext.m_ffRewIndex = kInitFFRWSpeed;
    m_playerContext.m_ffRewSpeed = 0;
    m_playerContext.m_tsNormal   = 1.0F;
    ReturnPlayerLock();

    m_sleepIndex = 0;

    SetUpdateOSDPosition(false);

    GetPlayerReadLock();
    ClearInputQueues(false);
    ReturnPlayerLock();

    m_switchToRec = nullptr;
    SetExitPlayer(false, false);

    m_errorRecoveryTimerId   = StartTimer(kErrorRecoveryCheckFrequency, __LINE__);
    m_lcdTimerId             = StartTimer(1, __LINE__);
    m_speedChangeTimerId     = StartTimer(kSpeedChangeCheckFrequency, __LINE__);
    m_saveLastPlayPosTimerId = StartTimer(kSaveLastPlayPosTimeout, __LINE__);

    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "-- end");
    return true;
}

TV::~TV()
{
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "-- begin");

    BrowseStop();
    BrowseWait();

    gCoreContext->removeListener(this);
    gCoreContext->UnregisterForPlayback(this);

    if (m_mainWindow)
    {
        m_mainWindow->removeEventFilter(this);
        if (m_weDisabledGUI)
            m_mainWindow->PopDrawDisabled();
    }

    if (m_myWindow)
    {
        m_myWindow->Close();
        m_myWindow = nullptr;
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC + "-- lock");

    // restore window to gui size and position
    if (m_mainWindow)
    {
        MythDisplay* display = m_mainWindow->GetDisplay();
        if (display->UsingVideoModes())
        {
            bool hide = display->NextModeIsLarger(display->GetGUIResolution());
            if (hide)
                m_mainWindow->hide();
            display->SwitchToGUI(true);
            if (hide)
                m_mainWindow->Show();
        }
        m_mainWindow->MoveResize(m_savedGuiBounds);
    #ifdef Q_OS_ANDROID
        m_mainWindow->Show();
    #else
        m_mainWindow->show();
    #endif
        m_mainWindow->PauseIdleTimer(false);
    }


    delete m_lastProgram;

    if (LCD *lcd = LCD::Get())
    {
        lcd->setFunctionLEDs(FUNC_TV, false);
        lcd->setFunctionLEDs(FUNC_MOVIE, false);
        lcd->switchToTime();
    }

    m_playerLock.lockForWrite();
    m_playerContext.TeardownPlayer();
    m_playerLock.unlock();

    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "-- end");
}

/**
 * \brief The main playback loop
 */
void TV::PlaybackLoop()
{
    while (true)
    {
        QCoreApplication::processEvents();
        if (SignalHandler::IsExiting())
        {
            m_wantsToQuit = true;
            return;
        }

        TVState state = GetState();
        if ((kState_Error == state) || (kState_None == state))
            return;

        if (kState_ChangingState == state)
            continue;

        GetPlayerReadLock();
        m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
        if (m_playerContext.m_player && !m_playerContext.m_player->IsErrored())
        {
            m_playerContext.m_player->EventLoop();
            m_playerContext.m_player->VideoLoop();
        }
        m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
        ReturnPlayerLock();

        if (m_playerContext.m_errored || !m_playerContext.m_player)
            return;
    }
}

/**
 * \brief update the channel list with channels from the selected channel group
 */
void TV::UpdateChannelList(int GroupID)
{
    if (!m_dbUseChannelGroups)
        return;

    QMutexLocker locker(&m_channelGroupLock);
    if (GroupID == m_channelGroupId)
        return;

    ChannelInfoList list;
    if (GroupID >= 0)
    {
        list = ChannelUtil::GetChannels(0, true, "channum, callsign", static_cast<uint>(GroupID));
        ChannelUtil::SortChannels(list, "channum", true);
    }

    m_channelGroupId = GroupID;
    m_channelGroupChannelList = list;

    if (m_dbRememberLastChannelGroup)
        gCoreContext->SaveSetting("ChannelGroupDefault", m_channelGroupId);
}

TVState TV::GetState() const
{
    TVState ret = kState_ChangingState;
    if (!m_playerContext.InStateChange())
        ret = m_playerContext.GetState();
    return ret;
}

// XXX what about subtitlezoom?
void TV::GetStatus()
{
    QVariantMap status;

    GetPlayerReadLock();
    status.insert("state", StateToString(GetState()));
    m_playerContext.LockPlayingInfo(__FILE__, __LINE__);
    if (m_playerContext.m_playingInfo)
    {
        status.insert("title", m_playerContext.m_playingInfo->GetTitle());
        status.insert("subtitle", m_playerContext.m_playingInfo->GetSubtitle());
        status.insert("starttime", m_playerContext.m_playingInfo->GetRecordingStartTime()
            .toUTC().toString("yyyy-MM-ddThh:mm:ssZ"));
        status.insert("chanid", QString::number(m_playerContext.m_playingInfo->GetChanID()));
        status.insert("programid", m_playerContext.m_playingInfo->GetProgramID());
        status.insert("pathname", m_playerContext.m_playingInfo->GetPathname());
    }
    m_playerContext.UnlockPlayingInfo(__FILE__, __LINE__);
    osdInfo info;
    m_playerContext.CalcPlayerSliderPosition(info);
    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (m_playerContext.m_player)
    {
        if (!info.text["totalchapters"].isEmpty())
        {
            QList<long long> chapters;
            m_playerContext.m_player->GetChapterTimes(chapters);
            QVariantList var;
            for (long long chapter : qAsConst(chapters))
                var << QVariant(chapter);
            status.insert("chaptertimes", var);
        }

        uint capmode = m_playerContext.m_player->GetCaptionMode();
        QVariantMap tracks;

        QStringList list = m_playerContext.m_player->GetTracks(kTrackTypeSubtitle);
        int currenttrack = -1;
        if (!list.isEmpty() && (kDisplayAVSubtitle == capmode))
            currenttrack = m_playerContext.m_player->GetTrack(kTrackTypeSubtitle);
        for (int i = 0; i < list.size(); i++)
        {
            if (i == currenttrack)
                status.insert("currentsubtitletrack", list[i]);
            tracks.insert("SELECTSUBTITLE_" + QString::number(i), list[i]);
        }

        list = m_playerContext.m_player->GetTracks(kTrackTypeTeletextCaptions);
        currenttrack = -1;
        if (!list.isEmpty() && (kDisplayTeletextCaptions == capmode))
            currenttrack = m_playerContext.m_player->GetTrack(kTrackTypeTeletextCaptions);
        for (int i = 0; i < list.size(); i++)
        {
            if (i == currenttrack)
                status.insert("currentsubtitletrack", list[i]);
            tracks.insert("SELECTTTC_" + QString::number(i), list[i]);
        }

        list = m_playerContext.m_player->GetTracks(kTrackTypeCC708);
        currenttrack = -1;
        if (!list.isEmpty() && (kDisplayCC708 == capmode))
            currenttrack = m_playerContext.m_player->GetTrack(kTrackTypeCC708);
        for (int i = 0; i < list.size(); i++)
        {
            if (i == currenttrack)
                status.insert("currentsubtitletrack", list[i]);
            tracks.insert("SELECTCC708_" + QString::number(i), list[i]);
        }

        list = m_playerContext.m_player->GetTracks(kTrackTypeCC608);
        currenttrack = -1;
        if (!list.isEmpty() && (kDisplayCC608 == capmode))
            currenttrack = m_playerContext.m_player->GetTrack(kTrackTypeCC608);
        for (int i = 0; i < list.size(); i++)
        {
            if (i == currenttrack)
                status.insert("currentsubtitletrack", list[i]);
            tracks.insert("SELECTCC608_" + QString::number(i), list[i]);
        }

        list = m_playerContext.m_player->GetTracks(kTrackTypeRawText);
        currenttrack = -1;
        if (!list.isEmpty() && (kDisplayRawTextSubtitle == capmode))
            currenttrack = m_playerContext.m_player->GetTrack(kTrackTypeRawText);
        for (int i = 0; i < list.size(); i++)
        {
            if (i == currenttrack)
                status.insert("currentsubtitletrack", list[i]);
            tracks.insert("SELECTRAWTEXT_" + QString::number(i), list[i]);
        }

        if (m_playerContext.m_player->HasTextSubtitles())
        {
            if (kDisplayTextSubtitle == capmode)
                status.insert("currentsubtitletrack", tr("External Subtitles"));
            tracks.insert(ACTION_ENABLEEXTTEXT, tr("External Subtitles"));
        }

        status.insert("totalsubtitletracks", tracks.size());
        if (!tracks.isEmpty())
            status.insert("subtitletracks", tracks);

        tracks.clear();
        list = m_playerContext.m_player->GetTracks(kTrackTypeAudio);
        currenttrack = m_playerContext.m_player->GetTrack(kTrackTypeAudio);
        for (int i = 0; i < list.size(); i++)
        {
            if (i == currenttrack)
                status.insert("currentaudiotrack", list[i]);
            tracks.insert("SELECTAUDIO_" + QString::number(i), list[i]);
        }

        status.insert("totalaudiotracks", tracks.size());
        if (!tracks.isEmpty())
            status.insert("audiotracks", tracks);

        status.insert("playspeed", m_playerContext.m_player->GetPlaySpeed());
        status.insert("audiosyncoffset", static_cast<long long>(m_playerContext.m_player->GetAudioTimecodeOffset()));

        if (m_playerContext.m_player->GetAudio()->ControlsVolume())
        {
            status.insert("volume", m_playerContext.m_player->GetVolume());
            status.insert("mute",   m_playerContext.m_player->GetMuteState());
        }

        if (m_playerContext.m_player->GetVideoOutput())
        {
            MythVideoOutput *vo = m_playerContext.m_player->GetVideoOutput();
            PictureAttributeSupported supp = vo->GetSupportedPictureAttributes();
            if (supp & kPictureAttributeSupported_Brightness)
                status.insert("brightness", vo->GetPictureAttribute(kPictureAttribute_Brightness));
            if (supp & kPictureAttributeSupported_Contrast)
                status.insert("contrast", vo->GetPictureAttribute(kPictureAttribute_Contrast));
            if (supp & kPictureAttributeSupported_Colour)
                status.insert("colour", vo->GetPictureAttribute(kPictureAttribute_Colour));
            if (supp & kPictureAttributeSupported_Hue)
                status.insert("hue", vo->GetPictureAttribute(kPictureAttribute_Hue));
        }
    }
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
    ReturnPlayerLock();

    for (auto tit =info.text.cbegin(); tit != info.text.cend(); ++tit)
        status.insert(tit.key(), tit.value());

    QHashIterator<QString,int> vit(info.values);
    while (vit.hasNext())
    {
        vit.next();
        status.insert(vit.key(), vit.value());
    }

    MythUIStateTracker::SetState(status);
}

/**
 *  \brief Starts LiveTV
 *  \param showDialogs if true error dialogs are shown, if false they are not
 *  \param selection What channel to tune.
 */
bool TV::LiveTV(bool ShowDialogs, const ChannelInfoList &Selection)
{
    m_requestDelete = false;
    m_allowRerecord = false;
    m_jumpToProgram = false;

    GetPlayerReadLock();
    if (m_playerContext.GetState() == kState_None && RequestNextRecorder(ShowDialogs, Selection))
    {
        m_playerContext.SetInitialTVState(true);
        HandleStateChange();
        m_switchToRec = nullptr;

        // Start Idle Timer
        if (m_dbIdleTimeout > 0)
        {
            m_idleTimerId = StartTimer(static_cast<int>(m_dbIdleTimeout), __LINE__);
            LOG(VB_GENERAL, LOG_INFO, QString("Using Idle Timer. %1 minutes").arg(m_dbIdleTimeout * (1.0 / 60000.0)));
        }

        ReturnPlayerLock();
        return true;
    }
    ReturnPlayerLock();
    return false;
}

bool TV::RequestNextRecorder(bool ShowDialogs, const ChannelInfoList &Selection)
{
    m_playerContext.SetRecorder(nullptr);

    RemoteEncoder *testrec = nullptr;
    if (m_switchToRec)
    {
        // If this is set we, already got a new recorder in SwitchCards()
        testrec = m_switchToRec;
        m_switchToRec = nullptr;
    }
    else if (!Selection.empty())
    {
        for (const auto & ci : Selection)
        {
            uint    chanid  = ci.m_chanId;
            QString channum = ci.m_chanNum;
            if (!chanid || channum.isEmpty())
                continue;
            QSet<uint> cards = IsTunableOn(chanid);

            if (chanid && !channum.isEmpty() && !cards.isEmpty())
            {
                testrec = RemoteGetExistingRecorder(static_cast<int>(*(cards.begin())));
                m_initialChanID = chanid;
                break;
            }
        }
    }
    else
    {
        // When starting LiveTV we just get the next free recorder
        testrec = RemoteRequestNextFreeRecorder(-1);
    }

    if (!testrec)
        return false;

    if (!testrec->IsValidRecorder())
    {
        if (ShowDialogs)
            ShowNoRecorderDialog();

        delete testrec;

        return false;
    }

    m_playerContext.SetRecorder(testrec);

    return true;
}

void TV::AskAllowRecording(const QStringList &Msg, int Timeuntil, bool HasRec, bool HasLater)
{
    if (!StateIsLiveTV(GetState()))
       return;

    auto *info = new ProgramInfo(Msg);
    if (!info->GetChanID())
    {
        delete info;
        return;
    }

    QMutexLocker locker(&m_askAllowLock);
    QString key = info->MakeUniqueKey();
    if (Timeuntil > 0)
    {
        // add program to list
#if 0
        LOG(VB_GENERAL, LOG_DEBUG, LOC + "AskAllowRecording -- " +
            QString("adding '%1'").arg(info->m_title));
#endif
        QDateTime expiry = MythDate::current().addSecs(Timeuntil);
        m_askAllowPrograms[key] = AskProgramInfo(expiry, HasRec, HasLater, info);
    }
    else
    {
        // remove program from list
        LOG(VB_GENERAL, LOG_INFO, LOC + "-- " +
            QString("removing '%1'").arg(info->GetTitle()));
        QMap<QString,AskProgramInfo>::iterator it = m_askAllowPrograms.find(key);
        if (it != m_askAllowPrograms.end())
        {
            delete (*it).m_info;
            m_askAllowPrograms.erase(it);
        }
        delete info;
    }

    ShowOSDAskAllow();
}

void TV::ShowOSDAskAllow()
{
    QMutexLocker locker(&m_askAllowLock);
    if (!m_playerContext.m_recorder)
        return;

    uint cardid = m_playerContext.GetCardID();

    QString single_rec = tr("MythTV wants to record \"%1\" on %2 in %d seconds. Do you want to:");

    QString record_watch  = tr("Record and watch while it records");
    QString let_record1   = tr("Let it record and go back to the Main Menu");
    QString let_recordm   = tr("Let them record and go back to the Main Menu");
    QString record_later1 = tr("Record it later, I want to watch TV");
    QString record_laterm = tr("Record them later, I want to watch TV");
    QString do_not_record1= tr("Don't let it record, I want to watch TV");
    QString do_not_recordm= tr("Don't let them record, I want to watch TV");

    // eliminate timed out programs
    QDateTime timeNow = MythDate::current();
    QMap<QString,AskProgramInfo>::iterator it = m_askAllowPrograms.begin();
    QMap<QString,AskProgramInfo>::iterator next = it;
    while (it != m_askAllowPrograms.end())
    {
        next = it; ++next;
        if ((*it).m_expiry <= timeNow)
        {
#if 0
            LOG(VB_GENERAL, LOG_DEBUG, LOC + "-- " +
                QString("removing '%1'").arg((*it).m_info->m_title));
#endif
            delete (*it).m_info;
            m_askAllowPrograms.erase(it);
        }
        it = next;
    }
    int          timeuntil = 0;
    QString      message;
    uint conflict_count = static_cast<uint>(m_askAllowPrograms.size());

    it = m_askAllowPrograms.begin();
    if ((1 == m_askAllowPrograms.size()) && ((*it).m_info->GetInputID() == cardid))
    {
        (*it).m_isInSameInputGroup = (*it).m_isConflicting = true;
    }
    else if (!m_askAllowPrograms.empty())
    {
        // get the currently used input on our card
        bool busy_input_grps_loaded = false;
        vector<uint> busy_input_grps;
        InputInfo busy_input;
        RemoteIsBusy(cardid, busy_input);

        // check if current input can conflict
        for (it = m_askAllowPrograms.begin(); it != m_askAllowPrograms.end(); ++it)
        {
            (*it).m_isInSameInputGroup =
                (cardid == (*it).m_info->GetInputID());

            if ((*it).m_isInSameInputGroup)
                continue;

            // is busy_input in same input group as recording
            if (!busy_input_grps_loaded)
            {
                busy_input_grps = CardUtil::GetInputGroups(busy_input.m_inputId);
                busy_input_grps_loaded = true;
            }

            vector<uint> input_grps =
                CardUtil::GetInputGroups((*it).m_info->GetInputID());

            for (uint grp : input_grps)
            {
                if (find(busy_input_grps.begin(), busy_input_grps.end(),
                         grp) !=  busy_input_grps.end())
                {
                    (*it).m_isInSameInputGroup = true;
                    break;
                }
            }
        }

        // check if inputs that can conflict are ok
        conflict_count = 0;
        for (it = m_askAllowPrograms.begin(); it != m_askAllowPrograms.end(); ++it)
        {
            if (!(*it).m_isInSameInputGroup)
                (*it).m_isConflicting = false;    // NOLINT(bugprone-branch-clone)
            else if (cardid == (*it).m_info->GetInputID())
                (*it).m_isConflicting = true;    // NOLINT(bugprone-branch-clone)
            else if (!CardUtil::IsTunerShared(cardid, (*it).m_info->GetInputID()))
                (*it).m_isConflicting = true;
            else if ((busy_input.m_mplexId &&
                      (busy_input.m_mplexId  == (*it).m_info->QueryMplexID())) ||
                     (!busy_input.m_mplexId &&
                      (busy_input.m_chanId == (*it).m_info->GetChanID())))
                (*it).m_isConflicting = false;
            else
                (*it).m_isConflicting = true;

            conflict_count += (*it).m_isConflicting ? 1 : 0;
        }
    }

    it = m_askAllowPrograms.begin();
    for (; it != m_askAllowPrograms.end() && !(*it).m_isConflicting; ++it);

    if (conflict_count == 0)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "The scheduler wants to make "
                                        "a non-conflicting recording.");
        // TODO take down mplexid and inform user of problem
        // on channel changes.
    }
    else if (conflict_count == 1 && ((*it).m_info->GetInputID() == cardid))
    {
#if 0
        LOG(VB_GENERAL, LOG_DEBUG, LOC + "UpdateOSDAskAllowDialog -- " +
            "kAskAllowOneRec");
#endif

        it = m_askAllowPrograms.begin();

        QString channel = m_dbChannelFormat;
        channel
            .replace("<num>",  (*it).m_info->GetChanNum())
            .replace("<sign>", (*it).m_info->GetChannelSchedulingID())
            .replace("<name>", (*it).m_info->GetChannelName());

        message = single_rec.arg((*it).m_info->GetTitle()).arg(channel);

        OSD *osd = GetOSDL();
        if (osd)
        {
            BrowseEnd(false);
            timeuntil = static_cast<int>(MythDate::current().secsTo((*it).m_expiry) * 1000);
            osd->DialogShow(OSD_DLG_ASKALLOW, message, timeuntil);
            osd->DialogAddButton(record_watch, "DIALOG_ASKALLOW_WATCH_0", false, !((*it).m_hasRec));
            osd->DialogAddButton(let_record1, "DIALOG_ASKALLOW_EXIT_0");
            osd->DialogAddButton(((*it).m_hasLater) ? record_later1 : do_not_record1, "DIALOG_ASKALLOW_CANCELRECORDING_0",
                                 false, ((*it).m_hasRec));
        }
        ReturnOSDLock();
    }
    else
    {
        if (conflict_count > 1)
        {
            message = tr(
                "MythTV wants to record these programs in %d seconds:");
            message += "\n";
        }

        bool has_rec = false;
        for (it = m_askAllowPrograms.begin(); it != m_askAllowPrograms.end(); ++it)
        {
            if (!(*it).m_isConflicting)
                continue;

            QString title = (*it).m_info->GetTitle();
            if ((title.length() < 10) && !(*it).m_info->GetSubtitle().isEmpty())
                title += ": " + (*it).m_info->GetSubtitle();
            if (title.length() > 20)
                title = title.left(17) + "...";

            QString channel = m_dbChannelFormat;
            channel
                .replace("<num>",  (*it).m_info->GetChanNum())
                .replace("<sign>", (*it).m_info->GetChannelSchedulingID())
                .replace("<name>", (*it).m_info->GetChannelName());

            if (conflict_count > 1)
            {
                message += tr("\"%1\" on %2").arg(title).arg(channel);
                message += "\n";
            }
            else
            {
                message = single_rec.arg((*it).m_info->GetTitle()).arg(channel);
                has_rec = (*it).m_hasRec;
            }
        }

        if (conflict_count > 1)
        {
            message += "\n";
            message += tr("Do you want to:");
        }

        bool all_have_later = true;
        timeuntil = 9999999;
        for (it = m_askAllowPrograms.begin(); it != m_askAllowPrograms.end(); ++it)
        {
            if ((*it).m_isConflicting)
            {
                all_have_later &= (*it).m_hasLater;
                int tmp = static_cast<int>(MythDate::current().secsTo((*it).m_expiry));
                tmp *= 1000;
                timeuntil = std::min(timeuntil, std::max(tmp, 0));
            }
        }
        timeuntil = (9999999 == timeuntil) ? 0 : timeuntil;

        OSD *osd = GetOSDL();
        if (osd && conflict_count > 1)
        {
            BrowseEnd(false);
            osd->DialogShow(OSD_DLG_ASKALLOW, message, timeuntil);
            osd->DialogAddButton(let_recordm, "DIALOG_ASKALLOW_EXIT_0",
                                 false, true);
            osd->DialogAddButton((all_have_later) ? record_laterm : do_not_recordm,
                                 "DIALOG_ASKALLOW_CANCELCONFLICTING_0");
        }
        else if (osd)
        {
            BrowseEnd(false);
            osd->DialogShow(OSD_DLG_ASKALLOW, message, timeuntil);
            osd->DialogAddButton(let_record1, "DIALOG_ASKALLOW_EXIT_0",
                                 false, !has_rec);
            osd->DialogAddButton((all_have_later) ? record_later1 : do_not_record1,
                                 "DIALOG_ASKALLOW_CANCELRECORDING_0",
                                 false, has_rec);
        }
        ReturnOSDLock();
    }
}

void TV::HandleOSDAskAllow(const QString& Action)
{
    if (!DialogIsVisible(OSD_DLG_ASKALLOW))
        return;

    if (!m_askAllowLock.tryLock())
    {
        LOG(VB_GENERAL, LOG_ERR, "allowrecordingbox : askAllowLock is locked");
        return;
    }

    if (Action == "CANCELRECORDING")
    {
        if (m_playerContext.m_recorder)
            m_playerContext.m_recorder->CancelNextRecording(true);
    }
    else if (Action == "CANCELCONFLICTING")
    {
        for (const auto& pgm : qAsConst(m_askAllowPrograms))
        {
            if (pgm.m_isConflicting)
                RemoteCancelNextRecording(pgm.m_info->GetInputID(), true);
        }
    }
    else if (Action == "WATCH")
    {
        if (m_playerContext.m_recorder)
            m_playerContext.m_recorder->CancelNextRecording(false);
    }
    else // if (action == "EXIT")
    {
        PrepareToExitPlayer(__LINE__);
        SetExitPlayer(true, true);
    }

    m_askAllowLock.unlock();
}

int TV::Playback(const ProgramInfo& ProgInfo)
{
    m_wantsToQuit   = false;
    m_jumpToProgram = false;
    m_allowRerecord = false;
    m_requestDelete = false;
    gCoreContext->TVInWantingPlayback(false);

    GetPlayerReadLock();
    if (m_playerContext.GetState() != kState_None)
    {
        ReturnPlayerLock();
        return 0;
    }

    m_playerContext.SetPlayingInfo(&ProgInfo);
    m_playerContext.SetInitialTVState(false);
    HandleStateChange();

    ReturnPlayerLock();

    if (LCD *lcd = LCD::Get())
    {
        lcd->switchToChannel(ProgInfo.GetChannelSchedulingID(), ProgInfo.GetTitle(), ProgInfo.GetSubtitle());
        lcd->setFunctionLEDs((ProgInfo.IsRecording())?FUNC_TV:FUNC_MOVIE, true);
    }

    return 1;
}

bool TV::StateIsRecording(TVState State)
{
    return (State == kState_RecordingOnly || State == kState_WatchingRecording);
}

bool TV::StateIsPlaying(TVState State)
{
    return (State == kState_WatchingPreRecorded ||
            State == kState_WatchingRecording   ||
            State == kState_WatchingVideo       ||
            State == kState_WatchingDVD         ||
            State == kState_WatchingBD);
}

bool TV::StateIsLiveTV(TVState State)
{
    return (State == kState_WatchingLiveTV);
}

#define TRANSITION(ASTATE,BSTATE) ((ctxState == (ASTATE)) && (desiredNextState == (BSTATE)))

#define SET_NEXT() do { nextState = desiredNextState; changed = true; } while(false)
#define SET_LAST() do { nextState = ctxState; changed = true; } while(false)

static QString tv_i18n(const QString &msg)
{
    QByteArray msg_arr = msg.toLatin1();
    QString msg_i18n = TV::tr(msg_arr.constData());
    QByteArray msg_i18n_arr = msg_i18n.toLatin1();
    return (msg_arr == msg_i18n_arr) ? msg_i18n : msg;
}

/** \fn TV::HandleStateChange(PlayerContext*,PlayerContext*)
 *  \brief Changes the state to the state on the front of the
 *         state change queue.
 *
 *   Note: There must exist a state transition from any state we can enter
 *   to  the kState_None state, as this is used to shutdown TV in RunTV.
 *
 */
void TV::HandleStateChange()
{
    if (m_playerContext.IsErrored())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Called after fatal error detected.");
        return;
    }

    bool changed = false;

    m_playerContext.LockState();
    TVState nextState = m_playerContext.GetState();
    if (m_playerContext.m_nextState.empty())
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Warning, called with no state to change to.");
        m_playerContext.UnlockState();
        return;
    }

    TVState ctxState = m_playerContext.GetState();
    TVState desiredNextState = m_playerContext.DequeueNextState();

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Attempting to change from %1 to %2")
        .arg(StateToString(nextState)).arg(StateToString(desiredNextState)));

    if (desiredNextState == kState_Error)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Attempting to set to an error state!");
        SetErrored();
        m_playerContext.UnlockState();
        return;
    }

    bool ok = false;
    if (TRANSITION(kState_None, kState_WatchingLiveTV))
    {
        m_playerContext.m_lastSignalUIInfo.clear();

        m_playerContext.m_recorder->Setup();

        QDateTime timerOffTime = MythDate::current();
        m_lockTimerOn = false;

        SET_NEXT();

        uint chanid = m_initialChanID;
        if (!chanid)
            chanid = static_cast<uint>(gCoreContext->GetNumSetting("DefaultChanid", 0));

        if (chanid && !IsTunablePriv(chanid))
            chanid = 0;

        QString channum = "";

        if (chanid)
        {
            QStringList reclist;

            MSqlQuery query(MSqlQuery::InitCon());
            query.prepare("SELECT channum FROM channel "
                          "WHERE chanid = :CHANID");
            query.bindValue(":CHANID", chanid);
            if (query.exec() && query.isActive() && query.size() > 0 && query.next())
                channum = query.value(0).toString();
            else
                channum = QString::number(chanid);

            bool getit = m_playerContext.m_recorder->ShouldSwitchToAnotherCard(
                QString::number(chanid));

            if (getit)
                reclist = ChannelUtil::GetValidRecorderList(chanid, channum);

            if (!reclist.empty())
            {
                RemoteEncoder *testrec = RemoteRequestFreeRecorderFromList(reclist, 0);
                if (testrec && testrec->IsValidRecorder())
                {
                    m_playerContext.SetRecorder(testrec);
                    m_playerContext.m_recorder->Setup();
                }
                else
                    delete testrec; // If testrec isn't a valid recorder ...
            }
            else if (getit)
                chanid = 0;
        }

        LOG(VB_GENERAL, LOG_DEBUG, LOC + "Spawning LiveTV Recorder -- begin");

        if (chanid && !channum.isEmpty())
            m_playerContext.m_recorder->SpawnLiveTV(m_playerContext.m_tvchain->GetID(), false, channum);
        else
            m_playerContext.m_recorder->SpawnLiveTV(m_playerContext.m_tvchain->GetID(), false, "");

        LOG(VB_GENERAL, LOG_DEBUG, LOC + "Spawning LiveTV Recorder -- end");

        if (!m_playerContext.ReloadTVChain())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "LiveTV not successfully started");
            MythMainWindow::RestoreScreensaver();
            m_playerContext.SetRecorder(nullptr);
            SetErrored();
            SET_LAST();
        }
        else
        {
            m_playerContext.LockPlayingInfo(__FILE__, __LINE__);
            QString playbackURL = m_playerContext.m_playingInfo->GetPlaybackURL(true);
            m_playerContext.UnlockPlayingInfo(__FILE__, __LINE__);

            bool opennow = (m_playerContext.m_tvchain->GetInputType(-1) != "DUMMY");

            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("playbackURL(%1) inputtype(%2)")
                    .arg(playbackURL).arg(m_playerContext.m_tvchain->GetInputType(-1)));

            m_playerContext.SetRingBuffer(
                MythMediaBuffer::Create(
                    playbackURL, false, true,
                    opennow ? MythMediaBuffer::kLiveTVOpenTimeout : -1));

            if (m_playerContext.m_buffer)
                m_playerContext.m_buffer->SetLiveMode(m_playerContext.m_tvchain);
        }


        if (m_playerContext.m_playingInfo && StartRecorder())
        {
            ok = StartPlayer(desiredNextState);
        }
        if (!ok)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "LiveTV not successfully started");
            MythMainWindow::RestoreScreensaver();
            m_playerContext.SetRecorder(nullptr);
            SetErrored();
            SET_LAST();
        }
        else
        {
            if (!m_lastLockSeenTime.isValid() ||
                (m_lastLockSeenTime < timerOffTime))
            {
                m_lockTimer.start();
                m_lockTimerOn = true;
            }
        }
    }
    else if (TRANSITION(kState_WatchingLiveTV, kState_None))
    {
        SET_NEXT();
        MythMainWindow::RestoreScreensaver();
        StopStuff(true, true, true);
    }
    else if (TRANSITION(kState_None, kState_WatchingPreRecorded) ||
             TRANSITION(kState_None, kState_WatchingVideo) ||
             TRANSITION(kState_None, kState_WatchingDVD)   ||
             TRANSITION(kState_None, kState_WatchingBD)    ||
             TRANSITION(kState_None, kState_WatchingRecording))
    {
        m_playerContext.LockPlayingInfo(__FILE__, __LINE__);
        QString playbackURL = m_playerContext.m_playingInfo->GetPlaybackURL(true);
        m_playerContext.UnlockPlayingInfo(__FILE__, __LINE__);

        MythMediaBuffer *buffer = MythMediaBuffer::Create(playbackURL, false);
        if (buffer && !buffer->GetLastError().isEmpty())
        {
            ShowNotificationError(tr("Can't start playback"),
                                  TV::tr( "TV Player" ), buffer->GetLastError());
            delete buffer;
            buffer = nullptr;
        }
        m_playerContext.SetRingBuffer(buffer);

        if (m_playerContext.m_buffer && m_playerContext.m_buffer->IsOpen())
        {
            if (desiredNextState == kState_WatchingRecording)
            {
                m_playerContext.LockPlayingInfo(__FILE__, __LINE__);
                RemoteEncoder *rec = RemoteGetExistingRecorder(m_playerContext.m_playingInfo);
                m_playerContext.UnlockPlayingInfo(__FILE__, __LINE__);

                m_playerContext.SetRecorder(rec);

                if (!m_playerContext.m_recorder || !m_playerContext.m_recorder->IsValidRecorder())
                {
                    LOG(VB_GENERAL, LOG_ERR, LOC +
                        "Couldn't find recorder for in-progress recording");
                    desiredNextState = kState_WatchingPreRecorded;
                    m_playerContext.SetRecorder(nullptr);
                }
                else
                {
                    m_playerContext.m_recorder->Setup();
                }
            }

            ok = StartPlayer(desiredNextState);

            if (ok)
            {
                SET_NEXT();

                m_playerContext.LockPlayingInfo(__FILE__, __LINE__);
                if (m_playerContext.m_playingInfo->IsRecording())
                {
                    QString message = "COMMFLAG_REQUEST ";
                    message += m_playerContext.m_playingInfo->MakeUniqueKey();
                    gCoreContext->SendMessage(message);
                }
                m_playerContext.UnlockPlayingInfo(__FILE__, __LINE__);
            }
        }

        if (!ok)
        {
            SET_LAST();
            SetErrored();
            if (m_playerContext.IsPlayerErrored())
            {
                ShowNotificationError(m_playerContext.m_player->GetError(),
                                      TV::tr( "TV Player" ),
                                      playbackURL);
                // We're going to display this error as notification
                // no need to display it later as popup
                m_playerContext.m_player->ResetErrored();
            }
        }
    }
    else if (TRANSITION(kState_WatchingPreRecorded, kState_None) ||
             TRANSITION(kState_WatchingVideo, kState_None)       ||
             TRANSITION(kState_WatchingDVD, kState_None)         ||
             TRANSITION(kState_WatchingBD, kState_None)          ||
             TRANSITION(kState_WatchingRecording, kState_None))
    {
        SET_NEXT();
        MythMainWindow::RestoreScreensaver();
        StopStuff(true, true, false);
    }
    else if (TRANSITION(kState_WatchingRecording, kState_WatchingPreRecorded) ||
             TRANSITION(kState_None, kState_None))
    {
        SET_NEXT();
    }

    // Print state changed message...
    if (!changed)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Unknown state transition: %1 to %2")
            .arg(StateToString(m_playerContext.GetState())).arg(StateToString(desiredNextState)));
    }
    else if (m_playerContext.GetState() != nextState)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Changing from %1 to %2")
            .arg(StateToString(m_playerContext.GetState())).arg(StateToString(nextState)));
    }

    // update internal state variable
    TVState lastState = m_playerContext.GetState();
    m_playerContext.m_playingState = nextState;
    m_playerContext.UnlockState();

    if (StateIsLiveTV(m_playerContext.GetState()))
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "State is LiveTV");
        UpdateOSDInput();
        LOG(VB_GENERAL, LOG_INFO, LOC + "UpdateOSDInput done");
        UpdateLCD();
        LOG(VB_GENERAL, LOG_INFO, LOC + "UpdateLCD done");
        ITVRestart(true);
        LOG(VB_GENERAL, LOG_INFO, LOC + "ITVRestart done");
    }
    else if (StateIsPlaying(m_playerContext.GetState()) && lastState == kState_None)
    {
        m_playerContext.LockPlayingInfo(__FILE__, __LINE__);
        int count = PlayGroup::GetCount();
        QString msg = tr("%1 Settings")
                .arg(tv_i18n(m_playerContext.m_playingInfo->GetPlaybackGroup()));
        m_playerContext.UnlockPlayingInfo(__FILE__, __LINE__);
        if (count > 0)
            SetOSDMessage(msg);
        ITVRestart(false);
    }

    if (m_playerContext.m_buffer && m_playerContext.m_buffer->IsDVD())
    {
        UpdateLCD();
    }

    if (m_playerContext.m_recorder)
        m_playerContext.m_recorder->FrontendReady();

    if (m_endOfRecPromptTimerId)
        KillTimer(m_endOfRecPromptTimerId);
    m_endOfRecPromptTimerId = 0;
    if (m_dbEndOfRecExitPrompt && !m_inPlaylist && !m_underNetworkControl)
        m_endOfRecPromptTimerId = StartTimer(kEndOfRecPromptCheckFrequency, __LINE__);

    if (m_endOfPlaybackTimerId)
        KillTimer(m_endOfPlaybackTimerId);
    m_endOfPlaybackTimerId = 0;

    if (StateIsPlaying(m_playerContext.GetState()))
    {
        m_endOfPlaybackTimerId = StartTimer(kEndOfPlaybackFirstCheckTimer, __LINE__);
    }

    if (TRANSITION(kState_None, kState_WatchingPreRecorded) ||
             TRANSITION(kState_None, kState_WatchingVideo) ||
             TRANSITION(kState_None, kState_WatchingDVD)   ||
             TRANSITION(kState_None, kState_WatchingBD)    ||
             TRANSITION(kState_None, kState_WatchingRecording) ||
             TRANSITION(kState_None, kState_WatchingLiveTV))
    {
        MythMainWindow::DisableScreensaver();
        // m_playerBounds is not applicable when switching modes so
        // skip this logic in that case.
        if (!m_dbUseVideoModes)
            m_mainWindow->MoveResize(m_playerBounds);

        if (!m_weDisabledGUI)
        {
            m_weDisabledGUI = true;
            m_mainWindow->PushDrawDisabled();
        }
        // we no longer need the contents of myWindow
        if (m_myWindow)
            m_myWindow->DeleteAllChildren();

        LOG(VB_GENERAL, LOG_INFO, LOC + "Main UI disabled.");
    }

    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + " -- end");
}

#undef TRANSITION
#undef SET_NEXT
#undef SET_LAST

/**
 *  \brief Starts recorder, must be called before StartPlayer().
 *  \param ctx The player context requesting recording.
 *  \param MaxWait How long to wait for RecorderBase to start recording. If
 *                 not provided, this defaults to 40 seconds.
 *  \return true when successful, false otherwise.
 */
bool TV::StartRecorder(int MaxWait)
{
    RemoteEncoder *rec = m_playerContext.m_recorder;
    MaxWait = (MaxWait <= 0) ? 40000 : MaxWait;
    MythTimer t;
    t.start();
    bool recording = false;
    bool ok = true;
    if (!rec)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Invalid Remote Encoder");
        SetErrored();
        return false;
    }
    while (!(recording = rec->IsRecording(&ok)) && !m_exitPlayerTimerId && t.elapsed() < MaxWait)
    {
        if (!ok)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Lost contact with backend");
            SetErrored();
            return false;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(5));
    }

    if (!recording || m_exitPlayerTimerId)
    {
        if (!m_exitPlayerTimerId)
            LOG(VB_GENERAL, LOG_ERR, LOC + "Timed out waiting for recorder to start");
        return false;
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Took %1 ms to start recorder.").arg(t.elapsed()));
    return true;
}

/**
 *  \brief Can shut down the ringbuffers, the players, and in LiveTV it can
 *         shut down the recorders.
 *
 *   The player needs to be partially shutdown before the recorder,
 *   and partially shutdown after the recorder. Hence these are shutdown
 *   from within the same method. Also, shutting down things in the right
 *   order avoids spewing error messages...
 *
 *  \param stopRingBuffer Set to true if ringbuffer must be shut down.
 *  \param stopPlayer     Set to true if player must be shut down.
 *  \param stopRecorder   Set to true if recorder must be shut down.
 */
void TV::StopStuff(bool StopRingBuffer, bool StopPlayer, bool StopRecorder)
{
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "-- begin");

    emit PlaybackExiting(this);

    if (m_playerContext.m_buffer)
        m_playerContext.m_buffer->IgnoreWaitStates(true);

    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (StopPlayer)
        m_playerContext.StopPlaying();
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);

    if (StopRingBuffer)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Stopping ring buffer");
        if (m_playerContext.m_buffer)
        {
            m_playerContext.m_buffer->StopReads();
            m_playerContext.m_buffer->Pause();
            m_playerContext.m_buffer->WaitForPause();
        }
    }

    if (StopRecorder)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "stopping recorder");
        if (m_playerContext.m_recorder)
            m_playerContext.m_recorder->StopLiveTV();
    }

    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "-- end");
}

void TV::timerEvent(QTimerEvent *Event)
{
    const int timer_id = Event->timerId();

    GetPlayerReadLock();
    bool errored = m_playerContext.IsErrored();
    ReturnPlayerLock();
    if (errored)
        return;

    bool handled = true;
    if (timer_id == m_lcdTimerId)
        HandleLCDTimerEvent();
    else if (timer_id == m_lcdVolumeTimerId)
        HandleLCDVolumeTimerEvent();
    else if (timer_id == m_sleepTimerId)
        ShowOSDSleep();
    else if (timer_id == m_sleepDialogTimerId)
        SleepDialogTimeout();
    else if (timer_id == m_idleTimerId)
        ShowOSDIdle();
    else if (timer_id == m_idleDialogTimerId)
        IdleDialogTimeout();
    else if (timer_id == m_endOfPlaybackTimerId)
        HandleEndOfPlaybackTimerEvent();
    else if (timer_id == m_embedCheckTimerId)
        HandleIsNearEndWhenEmbeddingTimerEvent();
    else if (timer_id == m_endOfRecPromptTimerId)
        HandleEndOfRecordingExitPromptTimerEvent();
    else if (timer_id == m_videoExitDialogTimerId)
        HandleVideoExitDialogTimerEvent();
    else if (timer_id == m_pseudoChangeChanTimerId)
        HandlePseudoLiveTVTimerEvent();
    else if (timer_id == m_speedChangeTimerId)
        HandleSpeedChangeTimerEvent();
    else if (timer_id == m_saveLastPlayPosTimerId)
        HandleSaveLastPlayPosEvent();
    else
        handled = false;

    if (handled)
        return;

    // Check if it matches a signalMonitorTimerId
    if (timer_id == m_signalMonitorTimerId)
    {
        KillTimer(m_signalMonitorTimerId);
        m_signalMonitorTimerId = 0;
        GetPlayerReadLock();
        if (!m_playerContext.m_lastSignalMsg.empty())
        {
            // set last signal msg, so we get some feedback...
            UpdateOSDSignal(m_playerContext.m_lastSignalMsg);
            m_playerContext.m_lastSignalMsg.clear();
        }
        UpdateOSDTimeoutMessage();
        ReturnPlayerLock();
        return;
    }

    // Check if it matches networkControlTimerId
    QString netCmd;
    if (timer_id == m_networkControlTimerId)
    {
        if (!m_networkControlCommands.empty())
            netCmd = m_networkControlCommands.dequeue();
        if (m_networkControlCommands.empty())
        {
            KillTimer(m_networkControlTimerId);
            m_networkControlTimerId = 0;
        }
    }

    if (!netCmd.isEmpty())
    {
        GetPlayerReadLock();
        ProcessNetworkControlCommand(netCmd);
        ReturnPlayerLock();
        handled = true;
    }

    if (handled)
        return;

    // Check if it matches exitPlayerTimerId
    if (timer_id == m_exitPlayerTimerId)
    {
        GetPlayerReadLock();
        OSD *osd = GetOSDL();
        if (osd)
        {
            osd->DialogQuit();
            osd->HideAll();
        }
        ReturnOSDLock();

        if (m_jumpToProgram && m_lastProgram)
        {
            if (!m_lastProgram->IsFileReadable())
            {
                SetOSDMessage(tr("Last Program: %1 Doesn't Exist").arg(m_lastProgram->GetTitle()));
                lastProgramStringList.clear();
                SetLastProgram(nullptr);
                LOG(VB_PLAYBACK, LOG_ERR, LOC +
                    "Last Program File does not exist");
                m_jumpToProgram = false;
            }
            else
                ForceNextStateNone();
        }
        else
            ForceNextStateNone();

        ReturnPlayerLock();

        KillTimer(m_exitPlayerTimerId);
        m_exitPlayerTimerId = 0;
        handled = true;
    }

    if (handled)
        return;

    if (timer_id == m_ccInputTimerId)
    {
        GetPlayerReadLock();
        // Clear closed caption input mode when timer expires
        if (m_ccInputMode)
        {
            m_ccInputMode = false;
            ClearInputQueues(true);
        }
        ReturnPlayerLock();

        KillTimer(m_ccInputTimerId);
        m_ccInputTimerId = 0;
        handled = true;
    }

    if (handled)
        return;

    if (timer_id == m_asInputTimerId)
    {
        GetPlayerReadLock();
        // Clear closed caption input mode when timer expires
        if (m_asInputMode)
        {
            m_asInputMode = false;
            ClearInputQueues(true);
        }
        ReturnPlayerLock();

        KillTimer(m_asInputTimerId);
        m_asInputTimerId = 0;
        handled = true;
    }

    if (handled)
        return;

    if (timer_id == m_queueInputTimerId)
    {
        GetPlayerReadLock();
        // Commit input when the OSD fades away
        if (HasQueuedChannel())
        {
            OSD *osd = GetOSDL();
            if (osd && !osd->IsWindowVisible("osd_input"))
            {
                ReturnOSDLock();
                CommitQueuedInput();
            }
            else
            {
                ReturnOSDLock();
            }
        }
        ReturnPlayerLock();

        if (!m_queuedChanID && m_queuedChanNum.isEmpty() && m_queueInputTimerId)
        {
            KillTimer(m_queueInputTimerId);
            m_queueInputTimerId = 0;
        }
        handled = true;
    }

    if (handled)
        return;

    if (timer_id == m_browseTimerId)
    {
        GetPlayerReadLock();
        BrowseEnd(false);
        ReturnPlayerLock();
        handled = true;
    }

    if (handled)
        return;

    if (timer_id == m_updateOSDDebugTimerId)
    {
        bool update = false;
        GetPlayerReadLock();
        OSD *osd = GetOSDL();
        if (osd && osd->IsWindowVisible("osd_debug") &&
            (StateIsLiveTV(m_playerContext.GetState()) || StateIsPlaying(m_playerContext.GetState())))
        {
            update = true;
        }
        else
        {
            KillTimer(m_updateOSDDebugTimerId);
            m_updateOSDDebugTimerId = 0;
            if (m_playerContext.m_buffer)
                m_playerContext.m_buffer->EnableBitrateMonitor(false);
            if (m_playerContext.m_player)
                m_playerContext.m_player->EnableFrameRateMonitor(false);
        }
        ReturnOSDLock();
        if (update)
            UpdateOSDDebug();
        ReturnPlayerLock();
        handled = true;
    }
    if (timer_id == m_updateOSDPosTimerId)
    {
        GetPlayerReadLock();
        OSD *osd = GetOSDL();
        if (osd && osd->IsWindowVisible("osd_status") &&
            (StateIsLiveTV(m_playerContext.GetState()) || StateIsPlaying(m_playerContext.GetState())))
        {
            osdInfo info;
            if (m_playerContext.CalcPlayerSliderPosition(info))
            {
                osd->SetText("osd_status", info.text, kOSDTimeout_Ignore);
                osd->SetValues("osd_status", info.values, kOSDTimeout_Ignore);
            }
        }
        else
            SetUpdateOSDPosition(false);
        ReturnOSDLock();
        ReturnPlayerLock();
        handled = true;
    }

    if (handled)
        return;

    if (timer_id == m_errorRecoveryTimerId)
    {
        GetPlayerReadLock();
        if (m_playerContext.IsRecorderErrored() || m_playerContext.IsPlayerErrored() ||
            m_playerContext.IsErrored())
        {
            SetExitPlayer(true, false);
            ForceNextStateNone();
        }
        ReturnPlayerLock();

        if (m_errorRecoveryTimerId)
            KillTimer(m_errorRecoveryTimerId);
        m_errorRecoveryTimerId = StartTimer(kErrorRecoveryCheckFrequency, __LINE__);
        return;
    }

    LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Unknown timer: %1").arg(timer_id));
}

bool TV::HandleLCDTimerEvent()
{
    GetPlayerReadLock();
    LCD *lcd = LCD::Get();
    if (lcd)
    {
        float progress = 0.0F;
        QString lcd_time_string;
        bool showProgress = true;

        if (StateIsLiveTV(GetState()))
            ShowLCDChannelInfo();

        if (m_playerContext.m_buffer && m_playerContext.m_buffer->IsDVD())
        {
            ShowLCDDVDInfo();
            showProgress = !m_playerContext.m_buffer->IsInDiscMenuOrStillFrame();
        }

        if (showProgress)
        {
            osdInfo info;
            if (m_playerContext.CalcPlayerSliderPosition(info)) {
                progress = info.values["position"] * 0.001F;

                lcd_time_string = info.text["playedtime"] + " / " + info.text["totaltime"];
                // if the string is longer than the LCD width, remove all spaces
                if (lcd_time_string.length() > static_cast<int>(lcd->getLCDWidth()))
                    lcd_time_string.remove(' ');
            }
        }
        lcd->setChannelProgress(lcd_time_string, progress);
    }
    ReturnPlayerLock();

    KillTimer(m_lcdTimerId);
    m_lcdTimerId = StartTimer(kLCDTimeout, __LINE__);

    return true;
}

void TV::HandleLCDVolumeTimerEvent()
{
    GetPlayerReadLock();
    LCD *lcd = LCD::Get();
    if (lcd)
    {
        ShowLCDChannelInfo();
        lcd->switchToChannel(m_lcdCallsign, m_lcdTitle, m_lcdSubtitle);
    }
    ReturnPlayerLock();

    KillTimer(m_lcdVolumeTimerId);
    m_lcdVolumeTimerId = 0;
}

int TV::StartTimer(int Interval, int Line)
{
    int timer = startTimer(Interval);
    if (!timer)
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to start timer on line %1 of %2").arg(Line).arg(__FILE__));
    return timer;
}

void TV::KillTimer(int Id)
{
    killTimer(Id);
}

void TV::ForceNextStateNone()
{
    m_playerContext.ForceNextStateNone();
    ScheduleStateChange();
}

void TV::ScheduleStateChange()
{
    auto StateChange = [&]()
    {
        GetPlayerReadLock();
        if (!m_playerContext.m_nextState.empty())
        {
            HandleStateChange();
            if ((kState_None  == m_playerContext.GetState() ||
                 kState_Error == m_playerContext.GetState()) && m_jumpToProgram)
            {
                ReturnPlayerLock();
                GetPlayerWriteLock();
                m_playerContext.TeardownPlayer();
            }
        }
        ReturnPlayerLock();
    };

    QTimer::singleShot(0, StateChange);
}

void TV::ScheduleInputChange()
{
    auto InputChange = [&]()
    {
        GetPlayerReadLock();
        if (m_switchToInputId)
        {
            uint tmp = m_switchToInputId;
            m_switchToInputId = 0;
            SwitchInputs(0, QString(), tmp);
        }
        ReturnPlayerLock();
    };

    QTimer::singleShot(0, InputChange);
}

void TV::SetErrored()
{
    m_playerContext.m_errored = true;
    KillTimer(m_errorRecoveryTimerId);
    m_errorRecoveryTimerId = StartTimer(1, __LINE__);
}

void TV::PrepToSwitchToRecordedProgram(const ProgramInfo &ProgInfo)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Switching to program: %1")
        .arg(ProgInfo.toString(ProgramInfo::kTitleSubtitle)));
    SetLastProgram(&ProgInfo);
    PrepareToExitPlayer(__LINE__);
    m_jumpToProgram = true;
    SetExitPlayer(true, true);
}

void TV::PrepareToExitPlayer(int Line, BookmarkAction Bookmark)
{
    bool bm_allowed = IsBookmarkAllowed();
    m_playerContext.LockDeletePlayer(__FILE__, Line);
    if (m_playerContext.m_player)
    {
        if (bm_allowed)
        {
            // If we're exiting in the middle of the recording, we
            // automatically save a bookmark when "Action on playback
            // exit" is set to "Save position and exit".
            bool allow_set_before_end =
                (Bookmark == kBookmarkAlways ||
                 (Bookmark == kBookmarkAuto &&
                  m_dbPlaybackExitPrompt == 2));
            // If we're exiting at the end of the recording, we
            // automatically clear the bookmark when "Action on
            // playback exit" is set to "Save position and exit" and
            // "Clear bookmark on playback" is set to true.
            bool allow_clear_at_end =
                (Bookmark == kBookmarkAlways ||
                 (Bookmark == kBookmarkAuto &&
                  m_dbPlaybackExitPrompt == 2 &&
                  m_dbClearSavedPosition));
            // Whether to set/clear a bookmark depends on whether we're
            // exiting at the end of a recording.
            bool at_end = (m_playerContext.m_player->IsNearEnd() || GetEndOfRecording());
            // Don't consider ourselves at the end if the recording is
            // in-progress.
            at_end &= !StateIsRecording(GetState());
            bool clear_lastplaypos = false;
            if (at_end && allow_clear_at_end)
            {
                SetBookmark(true);
                // Tidy up the lastplaypos mark only when we clear the
                // bookmark due to exiting at the end.
                clear_lastplaypos = true;
            }
            else if (!at_end && allow_set_before_end)
            {
                SetBookmark(false);
            }
            if (clear_lastplaypos && m_playerContext.m_playingInfo)
                m_playerContext.m_playingInfo->ClearMarkupMap(MARK_UTIL_LASTPLAYPOS);
        }
        if (m_dbAutoSetWatched)
            m_playerContext.m_player->SetWatched();
    }
    m_playerContext.UnlockDeletePlayer(__FILE__, Line);
}

void TV::SetExitPlayer(bool SetIt, bool WantsTo)
{
    if (SetIt)
    {
        m_wantsToQuit = WantsTo;
        if (!m_exitPlayerTimerId)
            m_exitPlayerTimerId = StartTimer(1, __LINE__);
    }
    else
    {
        if (m_exitPlayerTimerId)
            KillTimer(m_exitPlayerTimerId);
        m_exitPlayerTimerId = 0;
        m_wantsToQuit = WantsTo;
    }
}

void TV::SetUpdateOSDPosition(bool Set)
{
    if (Set)
    {
        if (!m_updateOSDPosTimerId)
            m_updateOSDPosTimerId = StartTimer(500, __LINE__);
    }
    else
    {
        if (m_updateOSDPosTimerId)
            KillTimer(m_updateOSDPosTimerId);
        m_updateOSDPosTimerId = 0;
    }
}

void TV::HandleEndOfPlaybackTimerEvent()
{

    if (m_endOfPlaybackTimerId)
        KillTimer(m_endOfPlaybackTimerId);
    m_endOfPlaybackTimerId = 0;

    bool is_playing = false;
    GetPlayerReadLock();
    if (StateIsPlaying(GetState()))
    {
        if (m_playerContext.IsPlayerPlaying())
        {
            is_playing = true;
        }
        // If the end of playback is destined to pop up the end of
        // recording delete prompt, then don't exit the player here.
        else if (!(GetState() == kState_WatchingPreRecorded &&
                 m_dbEndOfRecExitPrompt && !m_inPlaylist && !m_underNetworkControl))
        {
            ForceNextStateNone();
            m_endOfRecording = true;
            PrepareToExitPlayer(__LINE__);
            SetExitPlayer(true, true);
        }
    }
    ReturnPlayerLock();

    if (is_playing)
        m_endOfPlaybackTimerId = StartTimer(kEndOfPlaybackCheckFrequency, __LINE__);
}

void TV::HandleIsNearEndWhenEmbeddingTimerEvent()
{
    GetPlayerReadLock();
    if (!StateIsLiveTV(GetState()))
    {
        m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
        bool toggle = m_playerContext.m_player && m_playerContext.m_player->IsEmbedding() &&
                      m_playerContext.m_player->IsNearEnd() && !m_playerContext.m_player->IsPaused();
        m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
        if (toggle)
            DoTogglePause(true);
    }
    ReturnPlayerLock();
}

void TV::HandleEndOfRecordingExitPromptTimerEvent()
{
    if (m_endOfRecording || m_inPlaylist || m_editMode || m_underNetworkControl ||
        m_exitPlayerTimerId)
    {
        return;
    }

    GetPlayerReadLock();
    OSD *osd = GetOSDL();
    if (osd && osd->DialogVisible())
    {
        ReturnOSDLock();
        ReturnPlayerLock();
        return;
    }
    ReturnOSDLock();

    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    bool do_prompt = (m_playerContext.GetState() == kState_WatchingPreRecorded &&
                      m_playerContext.m_player && !m_playerContext.m_player->IsEmbedding() &&
                      !m_playerContext.m_player->IsPlaying());
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);

    if (do_prompt)
        ShowOSDPromptDeleteRecording(tr("End Of Recording"));

    ReturnPlayerLock();
}

void TV::HandleVideoExitDialogTimerEvent()
{
    if (m_videoExitDialogTimerId)
        KillTimer(m_videoExitDialogTimerId);
    m_videoExitDialogTimerId = 0;

    // disable dialog and exit playback after timeout
    GetPlayerReadLock();
    OSD *osd = GetOSDL();
    if (!osd || !osd->DialogVisible(OSD_DLG_VIDEOEXIT))
    {
        ReturnOSDLock();
        ReturnPlayerLock();
        return;
    }
    if (osd)
        osd->DialogQuit();
    ReturnOSDLock();
    DoTogglePause(true);
    ClearOSD();
    PrepareToExitPlayer(__LINE__);
    ReturnPlayerLock();

    m_requestDelete = false;
    SetExitPlayer(true, true);
}

void TV::HandlePseudoLiveTVTimerEvent()
{
    KillTimer(m_pseudoChangeChanTimerId);
    m_pseudoChangeChanTimerId = 0;

    bool restartTimer = false;
    GetPlayerReadLock();
    if (m_playerContext.m_pseudoLiveTVState)
    {
        if (m_playerContext.InStateChange())
        {
            restartTimer = true;
        }
        else
        {
            LOG(VB_CHANNEL, LOG_INFO, "REC_PROGRAM -- channel change");

            uint        chanid  = m_playerContext.m_pseudoLiveTVRec->GetChanID();
            QString     channum = m_playerContext.m_pseudoLiveTVRec->GetChanNum();
            StringDeque tmp     = m_playerContext.m_prevChan;

            m_playerContext.m_prevChan.clear();
            ChangeChannel(chanid, channum);
            m_playerContext.m_prevChan = tmp;
            m_playerContext.m_pseudoLiveTVState = kPseudoRecording;
        }
    }
    ReturnPlayerLock();

    if (restartTimer)
        if (!m_pseudoChangeChanTimerId)
            m_pseudoChangeChanTimerId = StartTimer(25, __LINE__);
}

void TV::SetSpeedChangeTimer(int When, int Line)
{
    if (m_speedChangeTimerId)
        KillTimer(m_speedChangeTimerId);
    m_speedChangeTimerId = StartTimer(When, Line);
}

void TV::HandleSpeedChangeTimerEvent()
{
    if (m_speedChangeTimerId)
        KillTimer(m_speedChangeTimerId);
    m_speedChangeTimerId = StartTimer(kSpeedChangeCheckFrequency, __LINE__);

    GetPlayerReadLock();
    bool update_msg = m_playerContext.HandlePlayerSpeedChangeFFRew();
    update_msg     |= m_playerContext.HandlePlayerSpeedChangeEOF();
    if (update_msg)
        UpdateOSDSeekMessage(m_playerContext.GetPlayMessage(), kOSDTimeout_Med);
    ReturnPlayerLock();
}

/// This selectively blocks KeyPress and Resize events
bool TV::eventFilter(QObject* Object, QEvent* Event)
{
    // We want to intercept all resize events sent to the main window
    if ((Event->type() == QEvent::Resize))
        return (m_mainWindow != Object) ? false : event(Event);

    // Intercept keypress events unless they need to be handled by a main UI
    // screen (e.g. GuideGrid, ProgramFinder)

    if ( (QEvent::KeyPress == Event->type() || QEvent::KeyRelease == Event->type())
        && m_ignoreKeyPresses )
        return false;

    QScopedPointer<QEvent> sNewEvent(nullptr);
    if (m_mainWindow->keyLongPressFilter(&Event, sNewEvent))
        return true;

    if (QEvent::KeyPress == Event->type())
        return event(Event);

    if (MythGestureEvent::kEventType == Event->type())
        return m_ignoreKeyPresses ? false : event(Event);

    if (Event->type() == MythEvent::MythEventMessage ||
        Event->type() == MythEvent::MythUserMessage  ||
        Event->type() == MythEvent::kUpdateTvProgressEventType ||
        Event->type() == MythMediaEvent::kEventType)
    {
        customEvent(Event);
        return true;
    }

    switch (Event->type())
    {
        case QEvent::Paint:
        case QEvent::UpdateRequest:
        case QEvent::Enter:
        {
            event(Event);
            return false;
        }
        default:
            return false;
    }
}

/// This handles all standard events
bool TV::event(QEvent* Event)
{
    if (Event == nullptr)
        return false;

    if (QEvent::Resize == Event->type())
    {
        GetPlayerReadLock();
        m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
        const auto *qre = dynamic_cast<const QResizeEvent*>(Event);
        if (qre && m_playerContext.m_player)
            m_playerContext.m_player->WindowResized(qre->size());
        m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
        ReturnPlayerLock();
        return false;
    }

    if (QEvent::KeyPress == Event->type() ||
        MythGestureEvent::kEventType == Event->type())
    {
#if DEBUG_ACTIONS
        if (QEvent::KeyPress == e->type())
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + QString("keypress: %1 '%2'")
                    .arg(((QKeyEvent*)e)->key())
                    .arg(((QKeyEvent*)e)->text()));
        }
        else
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + QString("mythgesture: g:%1 pos:%2,%3 b:%4")
                    .arg(((MythGestureEvent*)e)->gesture())
                    .arg(((MythGestureEvent*)e)->GetPosition().x())
                    .arg(((MythGestureEvent*)e)->GetPosition().y())
                    .arg(((MythGestureEvent*)e)->GetButton())
                    );
        }
#endif
        bool handled = false;
        GetPlayerReadLock();
        if (m_playerContext.HasPlayer())
            handled = ProcessKeypressOrGesture(Event);
        ReturnPlayerLock();
        if (handled)
            return true;
    }

    switch (Event->type())
    {
        case QEvent::Paint:
        case QEvent::UpdateRequest:
        case QEvent::Enter:
            return true;
        default:
            break;
    }

    return QObject::event(Event);
}

bool TV::HandleTrackAction(const QString &Action)
{
    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (!m_playerContext.m_player)
    {
        m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
        return false;
    }

    bool handled = true;

    if (Action == ACTION_TOGGLEEXTTEXT)
        m_playerContext.m_player->ToggleCaptions(kTrackTypeTextSubtitle);
    else if (ACTION_ENABLEEXTTEXT == Action)
        m_playerContext.m_player->EnableCaptions(kDisplayTextSubtitle);
    else if (ACTION_DISABLEEXTTEXT == Action)
        m_playerContext.m_player->DisableCaptions(kDisplayTextSubtitle);
    else if (ACTION_ENABLEFORCEDSUBS == Action)
        m_playerContext.m_player->SetAllowForcedSubtitles(true);
    else if (ACTION_DISABLEFORCEDSUBS == Action)
        m_playerContext.m_player->SetAllowForcedSubtitles(false);
    else if (Action == ACTION_ENABLESUBS)
        m_playerContext.m_player->SetCaptionsEnabled(true, true);
    else if (Action == ACTION_DISABLESUBS)
        m_playerContext.m_player->SetCaptionsEnabled(false, true);
    else if (Action == ACTION_TOGGLESUBS && !IsBrowsing())
    {
        if (m_ccInputMode)
        {
            bool valid = false;
            int page = GetQueuedInputAsInt(&valid, 16);
            if (m_vbimode == VBIMode::PAL_TT && valid)
                m_playerContext.m_player->SetTeletextPage(static_cast<uint>(page));
            else if (m_vbimode == VBIMode::NTSC_CC)
                m_playerContext.m_player->SetTrack(kTrackTypeCC608,std::max(std::min(page - 1, 1), 0));

            ClearInputQueues(true);

            m_ccInputMode = false;
            if (m_ccInputTimerId)
            {
                KillTimer(m_ccInputTimerId);
                m_ccInputTimerId = 0;
            }
        }
        else if (m_playerContext.m_player->GetCaptionMode() & kDisplayNUVTeletextCaptions)
        {
            ClearInputQueues(false);
            AddKeyToInputQueue(0);

            m_ccInputMode      = true;
            m_asInputMode      = false;
            m_ccInputTimerId = StartTimer(kInputModeTimeout, __LINE__);
            if (m_asInputTimerId)
            {
                KillTimer(m_asInputTimerId);
                m_asInputTimerId = 0;
            }
        }
        else
        {
            m_playerContext.m_player->ToggleCaptions();
        }
    }
    else if (Action.startsWith("TOGGLE"))
    {
        int type = to_track_type(Action.mid(6));
        if (type == kTrackTypeTeletextMenu)
            m_playerContext.m_player->EnableTeletext();
        else if (type >= kTrackTypeSubtitle)
            m_playerContext.m_player->ToggleCaptions(static_cast<uint>(type));
        else
            handled = false;
    }
    else if (Action.startsWith("SELECT"))
    {
        int type = to_track_type(Action.mid(6));
        int num = Action.section("_", -1).toInt();
        if (type >= kTrackTypeAudio)
            m_playerContext.m_player->SetTrack(static_cast<uint>(type), num);
        else
            handled = false;
    }
    else if (Action.startsWith("NEXT") || Action.startsWith("PREV"))
    {
        int dir = (Action.startsWith("NEXT")) ? +1 : -1;
        int type = to_track_type(Action.mid(4));
        if (type >= kTrackTypeAudio)
            m_playerContext.m_player->ChangeTrack(static_cast<uint>(type), dir);
        else if (Action.endsWith("CC"))
            m_playerContext.m_player->ChangeCaptionTrack(dir);
        else
            handled = false;
    }
    else
        handled = false;

    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);

    return handled;
}

static bool has_action(const QString& action, const QStringList &actions)
{
    QStringList::const_iterator it;
    for (it = actions.begin(); it != actions.end(); ++it)
    {
        if (action == *it)
            return true;
    }
    return false;
}

// Make a special check for global system-related events.
//
// This check needs to be done early in the keypress event processing,
// because FF/REW processing causes unknown events to stop FF/REW, and
// manual zoom mode processing consumes all but a few event types.
// Ideally, we would just call MythScreenType::keyPressEvent()
// unconditionally, but we only want certain keypresses handled by
// that method.
//
// As a result, some of the MythScreenType::keyPressEvent() string
// compare logic is copied here.
static bool SysEventHandleAction(MythMainWindow* MainWindow, QKeyEvent *e, const QStringList &actions)
{
    QStringList::const_iterator it;
    for (it = actions.begin(); it != actions.end(); ++it)
    {
        if ((*it).startsWith("SYSEVENT") ||
            *it == ACTION_SCREENSHOT ||
            *it == ACTION_TVPOWERON ||
            *it == ACTION_TVPOWEROFF)
        {
            return MainWindow->GetMainStack()->GetTopScreen()->keyPressEvent(e);
        }
    }
    return false;
}

QList<QKeyEvent> TV::ConvertScreenPressKeyMap(const QString &KeyList)
{
    QList<QKeyEvent> keyPressList;
    int i = 0;
    QStringList stringKeyList = KeyList.split(',');
    for (const auto & str : qAsConst(stringKeyList))
    {
        QKeySequence keySequence(str);
        for(i = 0; i < keySequence.count(); i++)
        {
            uint keynum = static_cast<uint>(keySequence[static_cast<uint>(i)]);
            QKeyEvent keyEvent(QEvent::None, keynum & ~Qt::KeyboardModifierMask,
                               static_cast<Qt::KeyboardModifiers>(keynum & Qt::KeyboardModifierMask));
            keyPressList.append(keyEvent);
        }
    }
    if (stringKeyList.count() < kScreenPressRegionCount)
    {
        // add default remainders
        for(; i < kScreenPressRegionCount; i++)
        {
            QKeyEvent keyEvent(QEvent::None, Qt::Key_Escape, Qt::NoModifier);
            keyPressList.append(keyEvent);
        }
    }
    return keyPressList;
}

bool TV::TranslateGesture(const QString &Context, MythGestureEvent *Event,
                          QStringList &Actions, bool IsLiveTV)
{
    if (Event && Context == "TV Playback")
    {
        // TODO make this configuable via a similar mechanism to
        //      TranslateKeyPress
        // possibly with configurable hot zones of various sizes in a theme
        // TODO enhance gestures to support other non Click types too
        if ((Event->GetGesture() == MythGestureEvent::Click) &&
            (Event->GetButton() == Qt::LeftButton))
        {
            // divide screen into 12 regions
            QSize size = m_mainWindow->size();
            QPoint pos = Event->GetPosition();
            int region = 0;
            const int widthDivider = 4;
            int w4 = size.width() / widthDivider;
            region = pos.x() / w4;
            int h3 = size.height() / 3;
            region += (pos.y() / h3) * widthDivider;

            if (IsLiveTV)
                return m_mainWindow->TranslateKeyPress(Context, &(m_screenPressKeyMapLiveTV[region]), Actions, true);
            return m_mainWindow->TranslateKeyPress(Context, &(m_screenPressKeyMapPlayback[region]), Actions, true);
        }
        return false;
    }
    return false;
}

bool TV::TranslateKeyPressOrGesture(const QString &Context, QEvent *Event,
                                    QStringList &Actions, bool IsLiveTV, bool AllowJumps)
{
    if (Event)
    {
        if (QEvent::KeyPress == Event->type())
            return m_mainWindow->TranslateKeyPress(Context, dynamic_cast<QKeyEvent*>(Event), Actions, AllowJumps);
        if (MythGestureEvent::kEventType == Event->type())
            return TranslateGesture(Context, dynamic_cast<MythGestureEvent*>(Event), Actions, IsLiveTV);
    }
    return false;
}

bool TV::ProcessKeypressOrGesture(QEvent* Event)
{
    if (Event == nullptr)
        return false;

    bool ignoreKeys = m_playerContext.IsPlayerChangingBuffers();

#if DEBUG_ACTIONS
    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("ignoreKeys: %1").arg(ignoreKeys));
#endif

    if (m_idleTimerId)
    {
        KillTimer(m_idleTimerId);
        m_idleTimerId = StartTimer(static_cast<int>(m_dbIdleTimeout), __LINE__);
    }

#ifdef Q_OS_LINUX
    // Fixups for _some_ linux native codes that QT doesn't know
    auto* eKeyEvent = dynamic_cast<QKeyEvent*>(Event);
    if (eKeyEvent) {
        if (eKeyEvent->key() <= 0)
        {
            int keycode = 0;
            switch(eKeyEvent->nativeScanCode())
            {
                case 209: // XF86AudioPause
                    keycode = Qt::Key_MediaPause;
                    break;
                default:
                  break;
            }

            if (keycode > 0)
            {
                auto *key = new QKeyEvent(QEvent::KeyPress, keycode,
                                          eKeyEvent->modifiers());
                QCoreApplication::postEvent(this, key);
            }
        }
    }
#endif

    QStringList actions;
    bool handled = false;
    bool alreadyTranslatedPlayback = false;

    TVState state = GetState();
    bool isLiveTV = StateIsLiveTV(state);

    if (ignoreKeys)
    {
        handled = TranslateKeyPressOrGesture("TV Playback", Event, actions, isLiveTV);
        alreadyTranslatedPlayback = true;

        if (handled || actions.isEmpty())
            return handled;

        bool esc   = has_action("ESCAPE", actions) ||
                     has_action("BACK", actions);
        bool pause = has_action(ACTION_PAUSE, actions);
        bool play  = has_action(ACTION_PLAY,  actions);

        if ((!esc || IsBrowsing()) && !pause && !play)
            return false;
    }

    OSD *osd = GetOSDL();
    if (osd && osd->DialogVisible())
    {
        if (QEvent::KeyPress == Event->type())
        {
            auto *qke = dynamic_cast<QKeyEvent*>(Event);
            handled = (qke != nullptr) && osd->DialogHandleKeypress(qke);
        }
        if (MythGestureEvent::kEventType == Event->type())
        {
            auto *mge = dynamic_cast<MythGestureEvent*>(Event);
            handled = (mge != nullptr) && osd->DialogHandleGesture(mge);
        }
    }
    ReturnOSDLock();

    if (m_editMode && !handled)
    {
        handled |= TranslateKeyPressOrGesture(
                   "TV Editing", Event, actions, isLiveTV);

        if (!handled && m_playerContext.m_player)
        {
            if (has_action("MENU", actions))
            {
                ShowOSDCutpoint("EDIT_CUT_POINTS");
                handled = true;
            }
            if (has_action(ACTION_MENUCOMPACT, actions))
            {
                ShowOSDCutpoint("EDIT_CUT_POINTS_COMPACT");
                handled = true;
            }
            if (has_action("ESCAPE", actions))
            {
                if (!m_playerContext.m_player->IsCutListSaved())
                    ShowOSDCutpoint("EXIT_EDIT_MODE");
                else
                {
                    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
                    m_playerContext.m_player->DisableEdit(0);
                    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
                }
                handled = true;
            }
            else
            {
                m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
                auto current_frame = m_playerContext.m_player->GetFramesPlayed();
                m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
                if ((has_action(ACTION_SELECT, actions)) && (m_playerContext.m_player->IsInDelete(current_frame)) &&
                    (!(m_playerContext.m_player->HasTemporaryMark())))
                {
                    ShowOSDCutpoint("EDIT_CUT_POINTS");
                    handled = true;
                }
                else
                {
                    handled |= m_playerContext.m_player->HandleProgramEditorActions(actions);
                }
            }
        }
        if (handled)
            m_editMode = (m_playerContext.m_player && m_playerContext.m_player->GetEditMode());
    }

    if (handled)
        return true;

    // If text is already queued up, be more lax on what is ok.
    // This allows hex teletext entry and minor channel entry.
    if (QEvent::KeyPress == Event->type())
    {
        auto *qke = dynamic_cast<QKeyEvent*>(Event);
        if (qke == nullptr)
            return false;
        const QString txt = qke->text();
        if (HasQueuedInput() && (1 == txt.length()))
        {
            bool ok = false;
            (void)txt.toInt(&ok, 16);
            if (ok || txt=="_" || txt=="-" || txt=="#" || txt==".")
            {
                AddKeyToInputQueue(txt.at(0).toLatin1());
                return true;
            }
        }
    }

    // Teletext menu
    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (m_playerContext.m_player && (m_playerContext.m_player->GetCaptionMode() == kDisplayTeletextMenu))
    {
        QStringList tt_actions;
        handled = TranslateKeyPressOrGesture(
                  "Teletext Menu", Event, tt_actions, isLiveTV);

        if (!handled && !tt_actions.isEmpty())
        {
            for (int i = 0; i < tt_actions.size(); i++)
            {
                if (m_playerContext.m_player->HandleTeletextAction(tt_actions[i]))
                {
                    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
                    return true;
                }
            }
        }
    }

    // Interactive television
    if (m_playerContext.m_player && m_playerContext.m_player->GetInteractiveTV())
    {
        if (!alreadyTranslatedPlayback)
        {
            handled = TranslateKeyPressOrGesture(
                      "TV Playback", Event, actions, isLiveTV);
            alreadyTranslatedPlayback = true;
        }
        if (!handled && !actions.isEmpty())
        {
            for (int i = 0; i < actions.size(); i++)
            {
                if (m_playerContext.m_player->ITVHandleAction(actions[i]))
                {
                    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
                    return true;
                }
            }
        }
    }
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);

    if (!alreadyTranslatedPlayback)
    {
        handled = TranslateKeyPressOrGesture(
                  "TV Playback", Event, actions, isLiveTV);
    }
    if (handled || actions.isEmpty())
        return handled;

    handled = false;

    bool isDVD = m_playerContext.m_buffer && m_playerContext.m_buffer->IsDVD();
    bool isMenuOrStill = m_playerContext.m_buffer && m_playerContext.m_buffer->IsInDiscMenuOrStillFrame();

    if (QEvent::KeyPress == Event->type())
        handled = handled || SysEventHandleAction(m_mainWindow, dynamic_cast<QKeyEvent*>(Event), actions);
    handled = handled || BrowseHandleAction(actions);
    handled = handled || ManualZoomHandleAction(actions);
    handled = handled || PictureAttributeHandleAction(actions);
    handled = handled || TimeStretchHandleAction(actions);
    handled = handled || AudioSyncHandleAction(actions);
    handled = handled || SubtitleZoomHandleAction(actions);
    handled = handled || SubtitleDelayHandleAction(actions);
    handled = handled || DiscMenuHandleAction(actions);
    handled = handled || ActiveHandleAction(actions, isDVD, isMenuOrStill);
    handled = handled || ToggleHandleAction(actions, isDVD);
    handled = handled || FFRewHandleAction(actions);
    handled = handled || ActivePostQHandleAction(actions);

#if DEBUG_ACTIONS
    for (int i = 0; i < actions.size(); ++i)
        LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("handled(%1) actions[%2](%3)")
                .arg(handled).arg(i).arg(actions[i]));
#endif // DEBUG_ACTIONS

    if (handled)
        return true;

    if (!handled)
    {
        for (int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            bool ok = false;
            int val = action.toInt(&ok);

            if (ok)
            {
                AddKeyToInputQueue(static_cast<char>('0' + val));
                handled = true;
            }
        }
    }

    return true;
}

bool TV::BrowseHandleAction(const QStringList &Actions)
{
    if (!IsBrowsing())
        return false;

    bool handled = true;

    if (has_action(ACTION_UP, Actions) || has_action(ACTION_CHANNELUP, Actions))
        BrowseDispInfo(BROWSE_UP);
    else if (has_action(ACTION_DOWN, Actions) || has_action(ACTION_CHANNELDOWN, Actions))
        BrowseDispInfo(BROWSE_DOWN);
    else if (has_action(ACTION_LEFT, Actions))
        BrowseDispInfo(BROWSE_LEFT);
    else if (has_action(ACTION_RIGHT, Actions))
        BrowseDispInfo(BROWSE_RIGHT);
    else if (has_action("NEXTFAV", Actions))
        BrowseDispInfo(BROWSE_FAVORITE);
    else if (has_action(ACTION_SELECT, Actions))
    {
        BrowseEnd(true);
    }
    else if (has_action(ACTION_CLEAROSD, Actions) ||
             has_action("ESCAPE",       Actions) ||
             has_action("BACK",         Actions) ||
             has_action("TOGGLEBROWSE", Actions))
    {
        BrowseEnd(false);
    }
    else if (has_action(ACTION_TOGGLERECORD, Actions))
        QuickRecord();
    else
    {
        handled = false;
        for (const auto& action : qAsConst(Actions))
        {
            if (action.length() == 1 && action[0].isDigit())
            {
                AddKeyToInputQueue(action[0].toLatin1());
                handled = true;
            }
        }
    }

    // only pass-through actions listed below
    return handled ||
        !(has_action(ACTION_VOLUMEDOWN, Actions) ||
          has_action(ACTION_VOLUMEUP,   Actions) ||
          has_action("STRETCHINC",      Actions) ||
          has_action("STRETCHDEC",      Actions) ||
          has_action(ACTION_MUTEAUDIO,  Actions) ||
          has_action("CYCLEAUDIOCHAN",  Actions) ||
          has_action("BOTTOMLINEMOVE",  Actions) ||
          has_action("BOTTOMLINESAVE",  Actions) ||
          has_action("TOGGLEASPECT",    Actions));
}

bool TV::ManualZoomHandleAction(const QStringList &Actions)
{
    if (!m_zoomMode)
        return false;

    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (!m_playerContext.m_player)
    {
        m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
        return false;
    }

    bool end_manual_zoom = false;
    bool handled = true;
    bool updateOSD = true;
    ZoomDirection zoom = kZoom_END;
    if (has_action(ACTION_ZOOMUP, Actions) ||
        has_action(ACTION_UP, Actions)     ||
        has_action(ACTION_CHANNELUP, Actions))
    {
        zoom = kZoomUp;
    }
    else if (has_action(ACTION_ZOOMDOWN, Actions) ||
             has_action(ACTION_DOWN, Actions)     ||
             has_action(ACTION_CHANNELDOWN, Actions))
    {
        zoom = kZoomDown;
    }
    else if (has_action(ACTION_ZOOMLEFT, Actions) ||
             has_action(ACTION_LEFT, Actions))
        zoom = kZoomLeft;
    else if (has_action(ACTION_ZOOMRIGHT, Actions) ||
             has_action(ACTION_RIGHT, Actions))
        zoom = kZoomRight;
    else if (has_action(ACTION_ZOOMASPECTUP, Actions) ||
             has_action(ACTION_VOLUMEUP, Actions))
        zoom = kZoomAspectUp;
    else if (has_action(ACTION_ZOOMASPECTDOWN, Actions) ||
             has_action(ACTION_VOLUMEDOWN, Actions))
        zoom = kZoomAspectDown;
    else if (has_action(ACTION_ZOOMIN, Actions) ||
             has_action(ACTION_JUMPFFWD, Actions))
        zoom = kZoomIn;
    else if (has_action(ACTION_ZOOMOUT, Actions) ||
             has_action(ACTION_JUMPRWND, Actions))
        zoom = kZoomOut;
    else if (has_action(ACTION_ZOOMVERTICALIN, Actions))
        zoom = kZoomVerticalIn;
    else if (has_action(ACTION_ZOOMVERTICALOUT, Actions))
        zoom = kZoomVerticalOut;
    else if (has_action(ACTION_ZOOMHORIZONTALIN, Actions))
        zoom = kZoomHorizontalIn;
    else if (has_action(ACTION_ZOOMHORIZONTALOUT, Actions))
        zoom = kZoomHorizontalOut;
    else if (has_action(ACTION_ZOOMQUIT, Actions) ||
             has_action("ESCAPE", Actions)        ||
             has_action("BACK", Actions))
    {
        zoom = kZoomHome;
        end_manual_zoom = true;
    }
    else if (has_action(ACTION_ZOOMCOMMIT, Actions) ||
             has_action(ACTION_SELECT, Actions))
    {
        end_manual_zoom = true;
        SetManualZoom(false, tr("Zoom Committed"));
    }
    else
    {
        updateOSD = false;
        // only pass-through actions listed below
        handled = !(has_action("STRETCHINC",     Actions) ||
                    has_action("STRETCHDEC",     Actions) ||
                    has_action(ACTION_MUTEAUDIO, Actions) ||
                    has_action("CYCLEAUDIOCHAN", Actions) ||
                    has_action(ACTION_PAUSE,     Actions) ||
                    has_action(ACTION_CLEAROSD,  Actions));
    }
    QString msg = tr("Zoom Committed");
    if (zoom != kZoom_END)
    {
        m_playerContext.m_player->Zoom(zoom);
        if (end_manual_zoom)
            msg = tr("Zoom Ignored");
        else
            msg = m_playerContext.m_player->GetVideoOutput()->GetZoomString();
    }
    else if (end_manual_zoom)
        msg = tr("%1 Committed")
            .arg(m_playerContext.m_player->GetVideoOutput()->GetZoomString());
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);

    if (updateOSD)
        SetManualZoom(!end_manual_zoom, msg);

    return handled;
}

bool TV::PictureAttributeHandleAction(const QStringList &Actions)
{
    if (!m_adjustingPicture)
        return false;

    bool handled = true;
    if (has_action(ACTION_LEFT, Actions))
        DoChangePictureAttribute(m_adjustingPicture, m_adjustingPictureAttribute, false);
    else if (has_action(ACTION_RIGHT, Actions))
        DoChangePictureAttribute(m_adjustingPicture, m_adjustingPictureAttribute, true);
    else
        handled = false;
    return handled;
}

bool TV::TimeStretchHandleAction(const QStringList &Actions)
{
    if (!m_stretchAdjustment)
        return false;

    bool handled = true;

    if (has_action(ACTION_LEFT, Actions))
        ChangeTimeStretch(-1);
    else if (has_action(ACTION_RIGHT, Actions))
        ChangeTimeStretch(1);
    else if (has_action(ACTION_DOWN, Actions))
        ChangeTimeStretch(-5);
    else if (has_action(ACTION_UP, Actions))
        ChangeTimeStretch(5);
    else if (has_action("ADJUSTSTRETCH", Actions))
        ToggleTimeStretch();
    else if (has_action(ACTION_SELECT, Actions))
        ClearOSD();
    else
        handled = false;

    return handled;
}

bool TV::AudioSyncHandleAction(const QStringList& Actions)
{
    if (!m_audiosyncAdjustment)
        return false;

    bool handled = true;

    if (has_action(ACTION_LEFT, Actions))
        ChangeAudioSync(-1);
    else if (has_action(ACTION_RIGHT, Actions))
        ChangeAudioSync(1);
    else if (has_action(ACTION_UP, Actions))
        ChangeAudioSync(10);
    else if (has_action(ACTION_DOWN, Actions))
        ChangeAudioSync(-10);
    else if (has_action(ACTION_TOGGELAUDIOSYNC, Actions) ||
             has_action(ACTION_SELECT, Actions))
        ClearOSD();
    else
        handled = false;

    return handled;
}

bool TV::SubtitleZoomHandleAction(const QStringList &Actions)
{
    if (!m_subtitleZoomAdjustment)
        return false;

    bool handled = true;

    if (has_action(ACTION_LEFT, Actions))
        ChangeSubtitleZoom(-1);
    else if (has_action(ACTION_RIGHT, Actions))
        ChangeSubtitleZoom(1);
    else if (has_action(ACTION_UP, Actions))
        ChangeSubtitleZoom(10);
    else if (has_action(ACTION_DOWN, Actions))
        ChangeSubtitleZoom(-10);
    else if (has_action(ACTION_TOGGLESUBTITLEZOOM, Actions) ||
             has_action(ACTION_SELECT, Actions))
        ClearOSD();
    else
        handled = false;

    return handled;
}

bool TV::SubtitleDelayHandleAction(const QStringList &Actions)
{
    if (!m_subtitleDelayAdjustment)
        return false;

    bool handled = true;

    if (has_action(ACTION_LEFT, Actions))
        ChangeSubtitleDelay(-5);
    else if (has_action(ACTION_RIGHT, Actions))
        ChangeSubtitleDelay(5);
    else if (has_action(ACTION_UP, Actions))
        ChangeSubtitleDelay(25);
    else if (has_action(ACTION_DOWN, Actions))
        ChangeSubtitleDelay(-25);
    else if (has_action(ACTION_TOGGLESUBTITLEDELAY, Actions) || has_action(ACTION_SELECT, Actions))
        ClearOSD();
    else
        handled = false;

    return handled;
}

bool TV::DiscMenuHandleAction(const QStringList& Actions) const
{
    int64_t pts = 0;
    MythVideoOutput *output = m_playerContext.m_player->GetVideoOutput();
    if (output)
    {
        VideoFrame *frame = output->GetLastShownFrame();
        // convert timecode (msec) to pts (90kHz)
        if (frame)
            pts = static_cast<int64_t>(frame->timecode  * 90);
    }
    return m_playerContext.m_buffer->HandleAction(Actions, pts);
}

bool TV::Handle3D(const QString& Action)
{
    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (m_playerContext.m_player && m_playerContext.m_player->GetVideoOutput() &&
        m_playerContext.m_player->GetVideoOutput()->StereoscopicModesAllowed())
    {
        StereoscopicMode mode = kStereoscopicModeAuto;
        if (ACTION_3DSIDEBYSIDEDISCARD == Action)
            mode = kStereoscopicModeSideBySideDiscard;
        else if (ACTION_3DTOPANDBOTTOMDISCARD == Action)
            mode = kStereoscopicModeTopAndBottomDiscard;
        else if (ACTION_3DIGNORE == Action)
            mode = kStereoscopicModeIgnore3D;
        m_playerContext.m_player->GetVideoOutput()->SetStereoscopicMode(mode);
        SetOSDMessage(StereoscopictoString(mode));
    }
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
    return true;
}

bool TV::ActiveHandleAction(const QStringList &Actions,
                            bool IsDVD, bool IsDVDStillFrame)
{
    bool handled = true;

    if (has_action("SKIPCOMMERCIAL", Actions) && !IsDVD)
        DoSkipCommercials(1);
    else if (has_action("SKIPCOMMBACK", Actions) && !IsDVD)
        DoSkipCommercials(-1);
    else if (has_action("QUEUETRANSCODE", Actions) && !IsDVD)
        DoQueueTranscode("Default");
    else if (has_action("QUEUETRANSCODE_AUTO", Actions) && !IsDVD)
        DoQueueTranscode("Autodetect");
    else if (has_action("QUEUETRANSCODE_HIGH", Actions)  && !IsDVD)
        DoQueueTranscode("High Quality");
    else if (has_action("QUEUETRANSCODE_MEDIUM", Actions) && !IsDVD)
        DoQueueTranscode("Medium Quality");
    else if (has_action("QUEUETRANSCODE_LOW", Actions) && !IsDVD)
        DoQueueTranscode("Low Quality");
    else if (has_action(ACTION_PLAY, Actions))
        DoPlay();
    else if (has_action(ACTION_PAUSE, Actions))
        DoTogglePause(true);
    else if (has_action("SPEEDINC", Actions) && !IsDVDStillFrame)
        ChangeSpeed(1);
    else if (has_action("SPEEDDEC", Actions) && !IsDVDStillFrame)
        ChangeSpeed(-1);
    else if (has_action("ADJUSTSTRETCH", Actions))
        ChangeTimeStretch(0);   // just display
    else if (has_action("CYCLECOMMSKIPMODE",Actions) && !IsDVD)
        SetAutoCommercialSkip(kCommSkipIncr);
    else if (has_action("NEXTSCAN", Actions))
    {
        m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
        FrameScanType scan = m_playerContext.m_player->NextScanOverride();
        m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
        OverrideScan(scan);
    }
    else if (has_action(ACTION_SEEKARB, Actions) && !IsDVD)
    {
        if (m_asInputMode)
        {
            ClearInputQueues(true);
            SetOSDText("osd_input", "osd_number_entry", tr("Seek:"), kOSDTimeout_Med);
            m_asInputMode = false;
            if (m_asInputTimerId)
            {
                KillTimer(m_asInputTimerId);
                m_asInputTimerId = 0;
            }
        }
        else
        {
            ClearInputQueues(false);
            AddKeyToInputQueue(0);
            m_asInputMode      = true;
            m_ccInputMode      = false;
            m_asInputTimerId = StartTimer(kInputModeTimeout, __LINE__);
            if (m_ccInputTimerId)
            {
                KillTimer(m_ccInputTimerId);
                m_ccInputTimerId = 0;
            }
        }
    }
    else if (has_action(ACTION_JUMPRWND, Actions))
        DoJumpRWND();
    else if (has_action(ACTION_JUMPFFWD, Actions))
        DoJumpFFWD();
    else if (has_action(ACTION_JUMPBKMRK, Actions))
    {
        m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
        uint64_t bookmark  = m_playerContext.m_player->GetBookmark();
        m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);

        if (bookmark)
        {
            DoPlayerSeekToFrame(bookmark);
            m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
            UpdateOSDSeekMessage(tr("Jump to Bookmark"), kOSDTimeout_Med);
            m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
        }
    }
    else if (has_action(ACTION_JUMPSTART,Actions))
    {
        DoSeek(0, tr("Jump to Beginning"), /*timeIsOffset*/false, /*honorCutlist*/true);
    }
    else if (has_action(ACTION_CLEAROSD, Actions))
    {
        ClearOSD();
    }
    else if (has_action(ACTION_VIEWSCHEDULED, Actions))
        EditSchedule(kViewSchedule);
    else if (HandleJumpToProgramAction(Actions))
    { // NOLINT(bugprone-branch-clone)
    }
    else if (has_action(ACTION_SIGNALMON, Actions))
    {
        if ((GetState() == kState_WatchingLiveTV) && m_playerContext.m_recorder)
        {
            QString input = m_playerContext.m_recorder->GetInput();
            uint timeout  = m_playerContext.m_recorder->GetSignalLockTimeout(input);

            if (timeout == 0xffffffff)
            {
                SetOSDMessage("No Signal Monitor");
                return false;
            }

            int rate   = m_sigMonMode ? 0 : 100;
            bool notify = !m_sigMonMode;

            PauseLiveTV();
            m_playerContext.m_recorder->SetSignalMonitoringRate(rate, notify);
            UnpauseLiveTV();

            m_lockTimerOn = false;
            m_sigMonMode  = !m_sigMonMode;
        }
    }
    else if (has_action(ACTION_SCREENSHOT, Actions))
    {
        MythMainWindow::ScreenShot();
    }
    else if (has_action(ACTION_STOP, Actions))
    {
        PrepareToExitPlayer(__LINE__);
        SetExitPlayer(true, true);
    }
    else if (has_action(ACTION_EXITSHOWNOPROMPTS, Actions))
    {
        m_requestDelete = false;
        PrepareToExitPlayer(__LINE__);
        SetExitPlayer(true, true);
    }
    else if (has_action("ESCAPE", Actions) ||
             has_action("BACK", Actions))
    {
        if (StateIsLiveTV(m_playerContext.GetState()) &&
            (m_playerContext.m_lastSignalMsgTime.elapsed() < static_cast<int>(PlayerContext::kSMExitTimeout)))
        {
            ClearOSD();
        }
        else
        {
            OSD *osd = GetOSDL();
            if (osd && osd->IsVisible())
            {
                ClearOSD();
                ReturnOSDLock();
                return handled;
            }
            ReturnOSDLock();
        }

        NormalSpeed();

        StopFFRew();

        bool do_exit = false;

        if (StateIsLiveTV(GetState()))
        {
            if (m_playerContext.HasPlayer() && (12 & m_dbPlaybackExitPrompt))
            {
                ShowOSDStopWatchingRecording();
                return handled;
            }
            do_exit = true;
        }
        else
        {
            if (m_playerContext.HasPlayer() && (5 & m_dbPlaybackExitPrompt) &&
                !m_underNetworkControl && !IsDVDStillFrame)
            {
                ShowOSDStopWatchingRecording();
                return handled;
            }
            PrepareToExitPlayer(__LINE__);
            m_requestDelete = false;
            do_exit = true;
        }

        if (do_exit)
        {
            // If it's a DVD, and we're not trying to execute a
            // jumppoint, try to back up.
            if (IsDVD && !m_mainWindow->IsExitingToMain() && has_action("BACK", Actions) &&
                m_playerContext.m_buffer && m_playerContext.m_buffer->DVD()->GoBack())
            {
                return handled;
            }
            SetExitPlayer(true, true);
        }
    }
    else if (has_action(ACTION_ENABLEUPMIX, Actions))
        EnableUpmix(true);
    else if (has_action(ACTION_DISABLEUPMIX, Actions))
        EnableUpmix(false);
    else if (has_action(ACTION_VOLUMEDOWN, Actions))
        ChangeVolume(false);
    else if (has_action(ACTION_VOLUMEUP, Actions))
        ChangeVolume(true);
    else if (has_action("CYCLEAUDIOCHAN", Actions))
        ToggleMute(true);
    else if (has_action(ACTION_MUTEAUDIO, Actions))
        ToggleMute();
    else if (has_action("STRETCHINC", Actions))
        ChangeTimeStretch(1);
    else if (has_action("STRETCHDEC", Actions))
        ChangeTimeStretch(-1);
    else if (has_action("MENU", Actions))
        ShowOSDMenu();
    else if (has_action(ACTION_MENUCOMPACT, Actions))
        ShowOSDMenu(true);
    else if (has_action("INFO", Actions) ||
             has_action("INFOWITHCUTLIST", Actions))
    {
        if (HasQueuedInput())
        {
            DoArbSeek(ARBSEEK_SET, has_action("INFOWITHCUTLIST", Actions));
        }
        else
            ToggleOSD(true);
    }
    else if (has_action(ACTION_TOGGLEOSDDEBUG, Actions))
        ToggleOSDDebug();
    else if (!IsDVDStillFrame && SeekHandleAction(Actions, IsDVD))
    {
    }
    else
    {
        handled = false;
        for (auto it = Actions.cbegin(); it != Actions.cend() && !handled; ++it)
            handled = HandleTrackAction(*it);
    }

    return handled;
}

bool TV::FFRewHandleAction(const QStringList &Actions)
{
    bool handled = false;

    if (m_playerContext.m_ffRewState)
    {
        for (int i = 0; i < Actions.size() && !handled; i++)
        {
            QString action = Actions[i];
            bool ok = false;
            int val = action.toInt(&ok);

            if (ok && val < static_cast<int>(m_ffRewSpeeds.size()))
            {
                SetFFRew(val);
                handled = true;
            }
        }

        if (!handled)
        {
            DoPlayerSeek(StopFFRew());
            UpdateOSDSeekMessage(m_playerContext.GetPlayMessage(), kOSDTimeout_Short);
            handled = true;
        }
    }

    if (m_playerContext.m_ffRewSpeed)
    {
        NormalSpeed();
        UpdateOSDSeekMessage(m_playerContext.GetPlayMessage(), kOSDTimeout_Short);
        handled = true;
    }

    return handled;
}

bool TV::ToggleHandleAction(const QStringList &Actions, bool IsDVD)
{
    bool handled = true;
    bool islivetv = StateIsLiveTV(GetState());

    if (has_action(ACTION_BOTTOMLINEMOVE, Actions))
        ToggleMoveBottomLine();
    else if (has_action(ACTION_BOTTOMLINESAVE, Actions))
        SaveBottomLine();
    else if (has_action("TOGGLEASPECT", Actions))
        ToggleAspectOverride();
    else if (has_action("TOGGLEFILL", Actions))
        ToggleAdjustFill();
    else if (has_action(ACTION_TOGGELAUDIOSYNC, Actions))
        ChangeAudioSync(0);   // just display
    else if (has_action(ACTION_TOGGLESUBTITLEZOOM, Actions))
        ChangeSubtitleZoom(0);   // just display
    else if (has_action(ACTION_TOGGLESUBTITLEDELAY, Actions))
        ChangeSubtitleDelay(0);   // just display
    else if (has_action(ACTION_TOGGLEVISUALISATION, Actions))
        EnableVisualisation(false, true);
    else if (has_action(ACTION_ENABLEVISUALISATION, Actions))
        EnableVisualisation(true);
    else if (has_action(ACTION_DISABLEVISUALISATION, Actions))
        EnableVisualisation(false);
    else if (has_action("TOGGLEPICCONTROLS", Actions))
        DoTogglePictureAttribute(kAdjustingPicture_Playback);
    else if (has_action(ACTION_TOGGLENIGHTMODE, Actions))
        DoToggleNightMode();
    else if (has_action("TOGGLESTRETCH", Actions))
        ToggleTimeStretch();
    else if (has_action(ACTION_TOGGLEUPMIX, Actions))
        EnableUpmix(false, true);
    else if (has_action(ACTION_TOGGLESLEEP, Actions))
        ToggleSleepTimer();
    else if (has_action(ACTION_TOGGLERECORD, Actions) && islivetv)
        QuickRecord();
    else if (has_action(ACTION_TOGGLEFAV, Actions) && islivetv)
        ToggleChannelFavorite();
    else if (has_action(ACTION_TOGGLECHANCONTROLS, Actions) && islivetv)
        DoTogglePictureAttribute(kAdjustingPicture_Channel);
    else if (has_action(ACTION_TOGGLERECCONTROLS, Actions) && islivetv)
        DoTogglePictureAttribute(kAdjustingPicture_Recording);
    else if (has_action("TOGGLEBROWSE", Actions))
    {
        if (islivetv)
            BrowseStart();
        else if (!IsDVD)
            ShowOSDMenu();
        else
            handled = false;
    }
    else if (has_action("EDIT", Actions))
    {
        if (islivetv)
            StartChannelEditMode();
        else if (!IsDVD)
            StartProgramEditMode();
    }
    else if (has_action(ACTION_OSDNAVIGATION, Actions))
    {
        StartOsdNavigation();
    }
    else
    {
        handled = false;
    }

    return handled;
}

void TV::EnableVisualisation(bool Enable, bool Toggle, const QString &Action)
{
    QString visualiser = QString("");
    if (Action.startsWith("VISUALISER"))
        visualiser = Action.mid(11);

    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (m_playerContext.m_player && m_playerContext.m_player->CanVisualise())
    {
        if (visualiser.isEmpty())
            visualiser = gCoreContext->GetSetting("AudioVisualiser", "");
        bool want = Enable || !visualiser.isEmpty();
        if (Toggle && visualiser.isEmpty())
            want = !m_playerContext.m_player->IsVisualising();
        bool on = m_playerContext.m_player->EnableVisualisation(want, visualiser);
        SetOSDMessage(on ? m_playerContext.m_player->GetVisualiserName() : tr("Visualisation Off"));
    }
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
}

void TV::SetBookmark(bool Clear)
{
    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (m_playerContext.m_player)
    {
        if (Clear)
        {
            m_playerContext.m_player->SetBookmark(true);
            SetOSDMessage(tr("Bookmark Cleared"));
        }
        else // if (IsBookmarkAllowed(ctx))
        {
            m_playerContext.m_player->SetBookmark();
            osdInfo info;
            m_playerContext.CalcPlayerSliderPosition(info);
            info.text["title"] = tr("Position");
            UpdateOSDStatus(info, kOSDFunctionalType_Default, kOSDTimeout_Med);
            SetOSDMessage(tr("Bookmark Saved"));
        }
    }
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
}

bool TV::ActivePostQHandleAction(const QStringList &Actions)
{
    bool handled = true;
    TVState state = GetState();
    bool islivetv = StateIsLiveTV(state);
    bool isdvd  = state == kState_WatchingDVD;
    bool isdisc = isdvd || state == kState_WatchingBD;

    if (has_action(ACTION_SETBOOKMARK, Actions))
    {
        if (!CommitQueuedInput())
        {
            m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
            SetBookmark(false);
            m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
        }
    }
    if (has_action(ACTION_TOGGLEBOOKMARK, Actions))
    {
        if (!CommitQueuedInput())
        {
            m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
            SetBookmark(m_playerContext.m_player->GetBookmark() != 0U);
            m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
        }
    }
    else if (has_action("NEXTFAV", Actions) && islivetv)
        ChangeChannel(CHANNEL_DIRECTION_FAVORITE);
    else if (has_action("NEXTSOURCE", Actions) && islivetv)
        SwitchSource(kNextSource);
    else if (has_action("PREVSOURCE", Actions) && islivetv)
        SwitchSource(kPreviousSource);
    else if (has_action("NEXTINPUT", Actions) && islivetv)
        SwitchInputs();
    else if (has_action(ACTION_GUIDE, Actions))
        EditSchedule(kScheduleProgramGuide);
    else if (has_action("PREVCHAN", Actions) && islivetv)
        PopPreviousChannel(false);
    else if (has_action(ACTION_CHANNELUP, Actions))
    {
        if (islivetv)
        {
            if (m_dbBrowseAlways)
                BrowseDispInfo(BROWSE_UP);
            else
                ChangeChannel(CHANNEL_DIRECTION_UP);
        }
        else
            DoJumpRWND();
    }
    else if (has_action(ACTION_CHANNELDOWN, Actions))
    {
        if (islivetv)
        {
            if (m_dbBrowseAlways)
                BrowseDispInfo(BROWSE_DOWN);
            else
                ChangeChannel(CHANNEL_DIRECTION_DOWN);
        }
        else
            DoJumpFFWD();
    }
    else if (has_action("DELETE", Actions) && !islivetv)
    {
        NormalSpeed();
        StopFFRew();
        SetBookmark();
        ShowOSDPromptDeleteRecording(tr("Are you sure you want to delete:"));
    }
    else if (has_action(ACTION_JUMPTODVDROOTMENU, Actions) && isdisc)
    {
        m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
        if (m_playerContext.m_player)
            m_playerContext.m_player->GoToMenu("root");
        m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
    }
    else if (has_action(ACTION_JUMPTODVDCHAPTERMENU, Actions) && isdisc)
    {
        m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
        if (m_playerContext.m_player)
            m_playerContext.m_player->GoToMenu("chapter");
        m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
    }
    else if (has_action(ACTION_JUMPTODVDTITLEMENU, Actions) && isdisc)
    {
        m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
        if (m_playerContext.m_player)
            m_playerContext.m_player->GoToMenu("title");
        m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
    }
    else if (has_action(ACTION_JUMPTOPOPUPMENU, Actions) && isdisc)
    {
        m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
        if (m_playerContext.m_player)
            m_playerContext.m_player->GoToMenu("popup");
        m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
    }
    else if (has_action(ACTION_FINDER, Actions))
        EditSchedule(kScheduleProgramFinder);
    else
        handled = false;

    return handled;
}


void TV::ProcessNetworkControlCommand(const QString &Command)
{
    bool ignoreKeys = m_playerContext.IsPlayerChangingBuffers();

#ifdef DEBUG_ACTIONS
    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("(%1) ignoreKeys: %2").arg(Command).arg(ignoreKeys));
#endif

    if (ignoreKeys)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Ignoring network control command because ignoreKeys is set");
        return;
    }

#if QT_VERSION < QT_VERSION_CHECK(5,14,0)
    QStringList tokens = Command.split(" ", QString::SkipEmptyParts);
#else
    QStringList tokens = Command.split(" ", Qt::SkipEmptyParts);
#endif
    if (tokens.size() < 2)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Not enough tokens in network control command " + QString("'%1'").arg(Command));
        return;
    }

    OSD *osd = GetOSDL();
    bool dlg = false;
    if (osd)
        dlg = osd->DialogVisible();
    ReturnOSDLock();

    if (dlg)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Ignoring network control command\n\t\t\t" +
            QString("because dialog is waiting for a response"));
        return;
    }

    if (tokens[1] != "QUERY")
        ClearOSD();

    if (tokens.size() == 3 && tokens[1] == "CHANID")
    {
        m_queuedChanID = tokens[2].toUInt();
        m_queuedChanNum.clear();
        CommitQueuedInput();
    }
    else if (tokens.size() == 3 && tokens[1] == "CHANNEL")
    {
        if (StateIsLiveTV(GetState()))
        {
            if (tokens[2] == "UP")
                ChangeChannel(CHANNEL_DIRECTION_UP);
            else if (tokens[2] == "DOWN")
                ChangeChannel(CHANNEL_DIRECTION_DOWN);
            else if (tokens[2].contains(QRegularExpression(R"(^[-\.\d_#]+$)")))
                ChangeChannel(0, tokens[2]);
        }
    }
    else if (tokens.size() == 3 && tokens[1] == "SPEED")
    {
        bool paused = ContextIsPaused(__FILE__, __LINE__);

        if (tokens[2] == "0x")
        {
            NormalSpeed();
            StopFFRew();
            if (!paused)
                DoTogglePause(true);
        }
        else if (tokens[2] == "normal")
        {
            NormalSpeed();
            StopFFRew();
            if (paused)
                DoTogglePause(true);
            return;
        }
        else
        {
            float tmpSpeed = 1.0F;
            bool ok = false;

            if (tokens[2].contains(QRegularExpression(R"(^\-*(\d*\.)?\d+x$)")))
            {
                QString speed = tokens[2].left(tokens[2].length()-1);
                tmpSpeed = speed.toFloat(&ok);
            }
            else
            {
                QRegularExpression re { R"(^(\-*\d+)\/(\d+)x$)" };
                auto match = re.match(tokens[2]);
                if (match.hasMatch())
                {
                    QStringList matches = match.capturedTexts();
                    int numerator = matches[1].toInt(&ok);
                    int denominator = matches[2].toInt(&ok);

                    if (ok && denominator != 0)
                        tmpSpeed = static_cast<float>(numerator) / static_cast<float>(denominator);
                    else
                        ok = false;
                }
            }

            if (ok)
            {
                float searchSpeed = fabs(tmpSpeed);

                if (paused)
                    DoTogglePause(true);

                if (tmpSpeed == 0.0F)
                {
                    NormalSpeed();
                    StopFFRew();

                    if (!paused)
                        DoTogglePause(true);
                }
                else if (tmpSpeed == 1.0F)
                {
                    StopFFRew();
                    m_playerContext.m_tsNormal = 1.0F;
                    ChangeTimeStretch(0, false);
                    return;
                }

                NormalSpeed();

                size_t index = 0;
                for ( ; index < m_ffRewSpeeds.size(); index++)
                    if (m_ffRewSpeeds[index] == static_cast<int>(searchSpeed))
                        break;

                if ((index < m_ffRewSpeeds.size()) && (m_ffRewSpeeds[index] == static_cast<int>(searchSpeed)))
                {
                    if (tmpSpeed < 0)
                        m_playerContext.m_ffRewState = -1;
                    else if (tmpSpeed > 1)
                        m_playerContext.m_ffRewState = 1;
                    else
                        StopFFRew();

                    if (m_playerContext.m_ffRewState)
                        SetFFRew(static_cast<int>(index));
                }
                else if (0.48F <= tmpSpeed && tmpSpeed <= 2.0F)
                {
                    StopFFRew();
                    m_playerContext.m_tsNormal = tmpSpeed;   // alter speed before display
                    ChangeTimeStretch(0, false);
                }
                else
                {
                    LOG(VB_GENERAL, LOG_WARNING, QString("Couldn't find %1 speed. Setting Speed to 1x")
                        .arg(static_cast<double>(searchSpeed)));
                    m_playerContext.m_ffRewState = 0;
                    SetFFRew(kInitFFRWSpeed);
                }
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, QString("Found an unknown speed of %1").arg(tokens[2]));
            }
        }
    }
    else if (tokens.size() == 2 && tokens[1] == "STOP")
    {
        SetBookmark();
        m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
        if (m_playerContext.m_player && m_dbAutoSetWatched)
            m_playerContext.m_player->SetWatched();
        m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
        SetExitPlayer(true, true);
    }
    else if (tokens.size() >= 3 && tokens[1] == "SEEK" && m_playerContext.HasPlayer())
    {
        if (m_playerContext.m_buffer && m_playerContext.m_buffer->IsInDiscMenuOrStillFrame())
            return;

        if (tokens[2] == "BEGINNING")
        {
            DoSeek(0, tr("Jump to Beginning"), /*timeIsOffset*/false, /*honorCutlist*/true);
        }
        else if (tokens[2] == "FORWARD")
        {
            DoSeek(m_playerContext.m_fftime, tr("Skip Ahead"), /*timeIsOffset*/true, /*honorCutlist*/true);
        }
        else if (tokens[2] == "BACKWARD")
        {
            DoSeek(-m_playerContext.m_rewtime, tr("Skip Back"), /*timeIsOffset*/true, /*honorCutlist*/true);
        }
        else if ((tokens[2] == "POSITION" ||
                  tokens[2] == "POSITIONWITHCUTLIST") &&
                 (tokens.size() == 4) &&
                 (tokens[3].contains(QRegularExpression("^\\d+$"))))
        {
            DoSeekAbsolute(tokens[3].toInt(), tokens[2] == "POSITIONWITHCUTLIST");
        }
    }
    else if (tokens.size() >= 3 && tokens[1] == "SUBTITLES")
    {
        bool ok = false;
        uint track = tokens[2].toUInt(&ok);

        if (!ok)
            return;

        if (track == 0)
        {
            m_playerContext.m_player->SetCaptionsEnabled(false, true);
        }
        else
        {
            QStringList subs = m_playerContext.m_player->GetTracks(kTrackTypeSubtitle);
            uint size = static_cast<uint>(subs.size());
            uint start = 1;
            uint finish = start + size;
            if (track >= start && track < finish)
            {
                m_playerContext.m_player->SetTrack(kTrackTypeSubtitle, static_cast<int>(track - start));
                m_playerContext.m_player->EnableCaptions(kDisplayAVSubtitle);
                return;
            }

            start = finish + 1;
            subs = m_playerContext.m_player->GetTracks(kTrackTypeCC708);
            finish = start + size;
            if (track >= start && track < finish)
            {
                m_playerContext.m_player->SetTrack(kTrackTypeCC708, static_cast<int>(track - start));
                m_playerContext.m_player->EnableCaptions(kDisplayCC708);
                return;
            }

            start = finish + 1;
            subs = m_playerContext.m_player->GetTracks(kTrackTypeCC608);
            finish = start + size;
            if (track >= start && track < finish)
            {
                m_playerContext.m_player->SetTrack(kTrackTypeCC608, static_cast<int>(track - start));
                m_playerContext.m_player->EnableCaptions(kDisplayCC608);
                return;
            }

            start = finish + 1;
            subs = m_playerContext.m_player->GetTracks(kTrackTypeTeletextCaptions);
            finish = start + size;
            if (track >= start && track < finish)
            {
                m_playerContext.m_player->SetTrack(kTrackTypeTeletextCaptions, static_cast<int>(track - start));
                m_playerContext.m_player->EnableCaptions(kDisplayTeletextCaptions);
                return;
            }

            start = finish + 1;
            subs = m_playerContext.m_player->GetTracks(kTrackTypeTeletextMenu);
            finish = start + size;
            if (track >= start && track < finish)
            {
                m_playerContext.m_player->SetTrack(kTrackTypeTeletextMenu, static_cast<int>(track - start));
                m_playerContext.m_player->EnableCaptions(kDisplayTeletextMenu);
                return;
            }

            start = finish + 1;
            subs = m_playerContext.m_player->GetTracks(kTrackTypeRawText);
            finish = start + size;
            if (track >= start && track < finish)
            {
                m_playerContext.m_player->SetTrack(kTrackTypeRawText, static_cast<int>(track - start));
                m_playerContext.m_player->EnableCaptions(kDisplayRawTextSubtitle);
                return;
            }
        }
    }
    else if (tokens.size() >= 3 && tokens[1] == "VOLUME")
    {
        QRegularExpression re { "(\\d+)%" };
        auto match = re.match(tokens[2]);
        if (match.hasMatch())
        {
            QStringList matches = match.capturedTexts();

            LOG(VB_GENERAL, LOG_INFO, QString("Set Volume to %1%")
                    .arg(matches[1]));

            bool ok = false;

            int vol = matches[1].toInt(&ok);

            if (!ok)
                return;

            if (0 <= vol && vol <= 100)
            {
                m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
                if (!m_playerContext.m_player)
                {
                    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
                    return;
                }

                vol -= m_playerContext.m_player->GetVolume();
                vol = static_cast<int>(m_playerContext.m_player->AdjustVolume(vol));
                m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);

                if (!IsBrowsing() && !m_editMode)
                {
                    UpdateOSDStatus(tr("Adjust Volume"), tr("Volume"), QString::number(vol),
                                    kOSDFunctionalType_PictureAdjust, "%", vol * 10, kOSDTimeout_Med);
                    SetUpdateOSDPosition(false);
                }
            }
        }
    }
    else if (tokens.size() >= 3 && tokens[1] == "QUERY")
    {
        if (tokens[2] == "POSITION")
        {
            if (!m_playerContext.m_player)
                return;
            QString speedStr;
            if (ContextIsPaused(__FILE__, __LINE__))
            {
                speedStr = "pause";
            }
            else if (m_playerContext.m_ffRewState)
            {
                speedStr = QString("%1x").arg(m_playerContext.m_ffRewSpeed);
            }
            else
            {
                QRegularExpression re { "Play (.*)x" };
                auto match = re.match(m_playerContext.GetPlayMessage());
                if (match.hasMatch())
                {
                    QStringList matches = match.capturedTexts();
                    speedStr = QString("%1x").arg(matches[1]);
                }
                else
                {
                    speedStr = "1x";
                }
            }

            osdInfo info;
            m_playerContext.CalcPlayerSliderPosition(info, true);

            QDateTime respDate = MythDate::current(true);
            QString infoStr = "";

            m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
            uint64_t fplay = 0;
            double   rate  = 30.0;
            if (m_playerContext.m_player)
            {
                fplay = m_playerContext.m_player->GetFramesPlayed();
                rate  = static_cast<double>(m_playerContext.m_player->GetFrameRate()); // for display only
            }
            m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);

            m_playerContext.LockPlayingInfo(__FILE__, __LINE__);
            if (m_playerContext.GetState() == kState_WatchingLiveTV)
            {
                infoStr = "LiveTV";
                if (m_playerContext.m_playingInfo)
                    respDate = m_playerContext.m_playingInfo->GetScheduledStartTime();
            }
            else
            {
                if (m_playerContext.m_buffer && m_playerContext.m_buffer->IsDVD())
                    infoStr = "DVD";
                else if (m_playerContext.m_playingInfo->IsRecording())
                    infoStr = "Recorded";
                else
                    infoStr = "Video";

                if (m_playerContext.m_playingInfo)
                    respDate = m_playerContext.m_playingInfo->GetRecordingStartTime();
            }

            QString bufferFilename =
              m_playerContext.m_buffer ? m_playerContext.m_buffer->GetFilename() : QString("no buffer");
            if ((infoStr == "Recorded") || (infoStr == "LiveTV"))
            {
                infoStr += QString(" %1 %2 %3 %4 %5 %6 %7")
                    .arg(info.text["description"])
                    .arg(speedStr)
                    .arg(m_playerContext.m_playingInfo != nullptr ?
                         m_playerContext.m_playingInfo->GetChanID() : 0)
                    .arg(respDate.toString(Qt::ISODate))
                    .arg(fplay)
                    .arg(bufferFilename)
                    .arg(rate);
            }
            else
            {
                QString position = info.text["description"].section(" ",0,0);
                infoStr += QString(" %1 %2 %3 %4 %5")
                    .arg(position)
                    .arg(speedStr)
                    .arg(bufferFilename)
                    .arg(fplay)
                    .arg(rate);
            }

            infoStr += QString(" Subtitles:");

            uint subtype = m_playerContext.m_player->GetCaptionMode();

            if (subtype == kDisplayNone)
                infoStr += QString(" *0:[None]*");
            else
                infoStr += QString(" 0:[None]");

            uint n = 1;

            QStringList subs = m_playerContext.m_player->GetTracks(kTrackTypeSubtitle);
            for (int i = 0; i < subs.size(); i++)
            {
                if ((subtype & kDisplayAVSubtitle) && (m_playerContext.m_player->GetTrack(kTrackTypeSubtitle) == i))
                    infoStr += QString(" *%1:[%2]*").arg(n).arg(subs[i]);
                else
                    infoStr += QString(" %1:[%2]").arg(n).arg(subs[i]);
                n++;
            }

            subs = m_playerContext.m_player->GetTracks(kTrackTypeCC708);
            for (int i = 0; i < subs.size(); i++)
            {
                if ((subtype & kDisplayCC708) && (m_playerContext.m_player->GetTrack(kTrackTypeCC708) == i))
                    infoStr += QString(" *%1:[%2]*").arg(n).arg(subs[i]);
                else
                    infoStr += QString(" %1:[%2]").arg(n).arg(subs[i]);
                n++;
            }

            subs = m_playerContext.m_player->GetTracks(kTrackTypeCC608);
            for (int i = 0; i < subs.size(); i++)
            {
                if ((subtype & kDisplayCC608) && (m_playerContext.m_player->GetTrack(kTrackTypeCC608) == i))
                    infoStr += QString(" *%1:[%2]*").arg(n).arg(subs[i]);
                else
                    infoStr += QString(" %1:[%2]").arg(n).arg(subs[i]);
                n++;
            }

            subs = m_playerContext.m_player->GetTracks(kTrackTypeTeletextCaptions);
            for (int i = 0; i < subs.size(); i++)
            {
                if ((subtype & kDisplayTeletextCaptions) && (m_playerContext.m_player->GetTrack(kTrackTypeTeletextCaptions) == i))
                    infoStr += QString(" *%1:[%2]*").arg(n).arg(subs[i]);
                else
                    infoStr += QString(" %1:[%2]").arg(n).arg(subs[i]);
                n++;
            }

            subs = m_playerContext.m_player->GetTracks(kTrackTypeTeletextMenu);
            for (int i = 0; i < subs.size(); i++)
            {
                if ((subtype & kDisplayTeletextMenu) && m_playerContext.m_player->GetTrack(kTrackTypeTeletextMenu) == i)
                    infoStr += QString(" *%1:[%2]*").arg(n).arg(subs[i]);
                else
                    infoStr += QString(" %1:[%2]").arg(n).arg(subs[i]);
                n++;
            }

            subs = m_playerContext.m_player->GetTracks(kTrackTypeRawText);
            for (int i = 0; i < subs.size(); i++)
            {
                if ((subtype & kDisplayRawTextSubtitle) && m_playerContext.m_player->GetTrack(kTrackTypeRawText) == i)
                    infoStr += QString(" *%1:[%2]*").arg(n).arg(subs[i]);
                else
                    infoStr += QString(" %1:[%2]").arg(n).arg(subs[i]);
                n++;
            }

            m_playerContext.UnlockPlayingInfo(__FILE__, __LINE__);

            QString message = QString("NETWORK_CONTROL ANSWER %1").arg(infoStr);
            MythEvent me(message);
            gCoreContext->dispatch(me);
        }
        else if (tokens[2] == "VOLUME")
        {
            if (!m_playerContext.m_player)
                return;
            QString infoStr = QString("%1%").arg(m_playerContext.m_player->GetVolume());
            QString message = QString("NETWORK_CONTROL ANSWER %1").arg(infoStr);
            MythEvent me(message);
            gCoreContext->dispatch(me);
        }
    }
}

bool TV::StartPlayer(TVState desiredState)
{
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("(%1) -- begin").arg(StateToString(desiredState)));
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Elapsed time since TV constructor was called: %1 ms")
        .arg(m_ctorTime.elapsed()));

    bool ok = m_playerContext.CreatePlayer(this, m_mainWindow, desiredState, false);
    ScheduleStateChange();

    if (ok)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Created player."));
        SetSpeedChangeTimer(25, __LINE__);
    }
    else
    {
        LOG(VB_GENERAL, LOG_CRIT, LOC + QString("Failed to create player."));
    }

    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("(%1) -- end %2")
        .arg(StateToString(desiredState)).arg((ok) ? "ok" : "error"));

    return ok;
}

void TV::DoPlay()
{
    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (!m_playerContext.m_player)
    {
        m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
        return;
    }

    float time = 0.0;

    if (m_playerContext.m_ffRewState || (m_playerContext.m_ffRewSpeed != 0) ||
        m_playerContext.m_player->IsPaused())
    {
        if (m_playerContext.m_ffRewState)
            time = StopFFRew();
        else if (m_playerContext.m_player->IsPaused())
            SendMythSystemPlayEvent("PLAY_UNPAUSED", m_playerContext.m_playingInfo);

        m_playerContext.m_player->Play(m_playerContext.m_tsNormal, true);
        gCoreContext->emitTVPlaybackUnpaused();
        m_playerContext.m_ffRewSpeed = 0;
    }
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);

    DoPlayerSeek(time);
    UpdateOSDSeekMessage(m_playerContext.GetPlayMessage(), kOSDTimeout_Med);

    MythMainWindow::DisableScreensaver();

    SetSpeedChangeTimer(0, __LINE__);
    gCoreContext->emitTVPlaybackPlaying();
    UpdateNavDialog();
}

float TV::DoTogglePauseStart()
{

    if (m_playerContext.m_buffer && m_playerContext.m_buffer->IsInDiscMenuOrStillFrame())
        return 0.0F;

    m_playerContext.m_ffRewSpeed = 0;
    float time = 0.0F;

    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (!m_playerContext.m_player)
    {
        m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
        return 0.0F;
    }
    if (m_playerContext.m_player->IsPaused())
    {
        m_playerContext.m_player->Play(m_playerContext.m_tsNormal, true);
    }
    else
    {
        if (m_playerContext.m_ffRewState)
            time = StopFFRew();
        m_playerContext.m_player->Pause();
    }
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
    return time;
}

void TV::DoTogglePauseFinish(float Time, bool ShowOSD)
{
    if (!m_playerContext.HasPlayer())
        return;

    if (m_playerContext.m_buffer && m_playerContext.m_buffer->IsInDiscMenuOrStillFrame())
        return;

    if (ContextIsPaused(__FILE__, __LINE__))
    {
        if (m_playerContext.m_buffer)
            m_playerContext.m_buffer->WaitForPause();

        DoPlayerSeek(Time);
        if (ShowOSD)
            UpdateOSDSeekMessage(tr("Paused"), kOSDTimeout_None);
        MythMainWindow::RestoreScreensaver();
    }
    else
    {
        DoPlayerSeek(Time);
        if (ShowOSD)
            UpdateOSDSeekMessage(m_playerContext.GetPlayMessage(), kOSDTimeout_Short);
        MythMainWindow::DisableScreensaver();
    }

    SetSpeedChangeTimer(0, __LINE__);
}

/*! \brief Check whether playback is paused
 *
 * This is currently used by MythAirplayServer.
 *
 * \returns true if a TV playback is currently going and is paused; otherwise returns false
 */
bool TV::IsPaused()
{
    bool paused = false;
    int dummy = 0;
    TV* tv = AcquireRelease(dummy, true);
    if (tv)
    {
        tv->GetPlayerReadLock();
        PlayerContext* context = tv->GetPlayerContext();
        if (!context->IsErrored())
        {
            context->LockDeletePlayer(__FILE__, __LINE__);
            if (context->m_player)
                paused = context->m_player->IsPaused();
            context->UnlockDeletePlayer(__FILE__, __LINE__);
        }
        tv->ReturnPlayerLock();
        AcquireRelease(dummy, false);
    }
    return paused;
}

void TV::DoTogglePause(bool ShowOSD)
{
    bool ignore = false;
    bool paused = false;
    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (m_playerContext.m_player)
    {
        ignore = m_playerContext.m_player->GetEditMode();
        paused = m_playerContext.m_player->IsPaused();
    }
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);

    if (paused)
        SendMythSystemPlayEvent("PLAY_UNPAUSED", m_playerContext.m_playingInfo);
    else
        SendMythSystemPlayEvent("PLAY_PAUSED", m_playerContext.m_playingInfo);

    if (!ignore)
        DoTogglePauseFinish(DoTogglePauseStart(), ShowOSD);
    // Emit Pause or Unpaused signal
    paused ? gCoreContext->emitTVPlaybackUnpaused() : gCoreContext->emitTVPlaybackPaused();
    UpdateNavDialog();
}

void TV::UpdateNavDialog()
{
    OSD *osd = GetOSDL();
    if (osd && osd->DialogVisible(OSD_DLG_NAVIGATE))
    {
        osdInfo info;
        m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
        bool paused = (m_playerContext.m_player
            && ((m_playerContext.m_ffRewState != 0) || (m_playerContext.m_ffRewSpeed != 0)
                || m_playerContext.m_player->IsPaused()));
        m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
        info.text["paused"] = (paused ? "Y" : "N");
        bool muted = m_playerContext.m_player && m_playerContext.m_player->IsMuted();
        info.text["muted"] = (muted ? "Y" : "N");
        osd->SetText(OSD_DLG_NAVIGATE, info.text, paused ? kOSDTimeout_None : kOSDTimeout_Long);
    }
    ReturnOSDLock();
}

bool TV::DoPlayerSeek(float Time)
{
    if (!m_playerContext.m_buffer)
        return false;

    if (Time > -0.001F && Time < +0.001F)
        return false;

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("%1 seconds").arg(static_cast<double>(Time)));

    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (!m_playerContext.m_player)
    {
        m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
        return false;
    }

    if (!m_playerContext.m_buffer->IsSeekingAllowed())
    {
        m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
        return false;
    }

    PauseAudioUntilBuffered();

    bool res = false;

    if (Time > 0.0F)
        res = m_playerContext.m_player->FastForward(Time);
    else if (Time < 0.0F)
        res = m_playerContext.m_player->Rewind(-Time);
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);

    return res;
}

bool TV::DoPlayerSeekToFrame(uint64_t FrameNum)
{
    if (!m_playerContext.m_buffer)
        return false;

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("%1").arg(FrameNum));

    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (!m_playerContext.m_player)
    {
        m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
        return false;
    }

    if (!m_playerContext.m_buffer->IsSeekingAllowed())
    {
        m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
        return false;
    }

    PauseAudioUntilBuffered();

    bool res = m_playerContext.m_player->JumpToFrame(FrameNum);

    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);

    return res;
}

bool TV::SeekHandleAction(const QStringList& Actions, const bool IsDVD)
{
    const int kRewind = 4;
    const int kForward = 8;
    const int kSticky = 16;
    const int kSlippery = 32;
    const int kRelative = 64;
    const int kAbsolute = 128;
    const int kIgnoreCutlist = 256;
    const int kWhenceMask = 3;
    int flags = 0;
    if (has_action(ACTION_SEEKFFWD, Actions))
        flags = ARBSEEK_FORWARD | kForward | kSlippery | kRelative;
    else if (has_action("FFWDSTICKY", Actions))
        flags = ARBSEEK_END     | kForward | kSticky   | kAbsolute;
    else if (has_action(ACTION_RIGHT, Actions))
        flags = ARBSEEK_FORWARD | kForward | kSticky   | kRelative;
    else if (has_action(ACTION_SEEKRWND, Actions))
        flags = ARBSEEK_REWIND  | kRewind  | kSlippery | kRelative;
    else if (has_action("RWNDSTICKY", Actions))
        flags = ARBSEEK_SET     | kRewind  | kSticky   | kAbsolute;
    else if (has_action(ACTION_LEFT, Actions))
        flags = ARBSEEK_REWIND  | kRewind  | kSticky   | kRelative;
    else
        return false;

    int direction = (flags & kRewind) ? -1 : 1;
    if (HasQueuedInput())
    {
        DoArbSeek(static_cast<ArbSeekWhence>(flags & kWhenceMask), (flags & kIgnoreCutlist) == 0);
    }
    else if (ContextIsPaused(__FILE__, __LINE__))
    {
        if (!IsDVD)
        {
            QString message = (flags & kRewind) ? tr("Rewind") :
                                                  tr("Forward");
            if (flags & kAbsolute) // FFWDSTICKY/RWNDSTICKY
            {
                float time = direction;
                DoSeek(time, message, /*timeIsOffset*/true, /*honorCutlist*/(flags & kIgnoreCutlist) == 0);
            }
            else
            {
                m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
                uint64_t frameAbs = m_playerContext.m_player->GetFramesPlayed();
                uint64_t frameRel = m_playerContext.m_player->TranslatePositionAbsToRel(frameAbs);
                uint64_t targetRel = frameRel + static_cast<uint64_t>(direction);
                if (frameRel == 0 && direction < 0)
                    targetRel = 0;
                uint64_t maxAbs = m_playerContext.m_player->GetCurrentFrameCount();
                uint64_t maxRel = m_playerContext.m_player->TranslatePositionAbsToRel(maxAbs);
                if (targetRel > maxRel)
                    targetRel = maxRel;
                uint64_t targetAbs = m_playerContext.m_player->TranslatePositionRelToAbs(targetRel);
                m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
                DoPlayerSeekToFrame(targetAbs);
                UpdateOSDSeekMessage(message, kOSDTimeout_Short);
            }
        }
    }
    else if (flags & kSticky)
    {
        ChangeFFRew(direction);
    }
    else if (flags & kRewind)
    {
            if (m_smartForward)
                m_doSmartForward = true;
            DoSeek(-m_playerContext.m_rewtime, tr("Skip Back"), /*timeIsOffset*/true, /*honorCutlist*/(flags & kIgnoreCutlist) == 0);
    }
    else
    {
        if (m_smartForward && m_doSmartForward)
        {
            DoSeek(m_playerContext.m_rewtime, tr("Skip Ahead"), /*timeIsOffset*/true, /*honorCutlist*/(flags & kIgnoreCutlist) == 0);
        }
        else
        {
            DoSeek(m_playerContext.m_fftime, tr("Skip Ahead"), /*timeIsOffset*/true, /*honorCutlist*/(flags & kIgnoreCutlist) == 0);
        }
    }
    UpdateNavDialog();
    return true;
}

void TV::DoSeek(float Time, const QString &Msg, bool TimeIsOffset, bool HonorCutlist)
{
    if (!m_playerContext.m_player)
        return;

    bool limitkeys = false;

    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (m_playerContext.m_player->GetLimitKeyRepeat())
        limitkeys = true;

    if (!limitkeys || (m_keyRepeatTimer.elapsed() > static_cast<int>(kKeyRepeatTimeout)))
    {
        m_keyRepeatTimer.start();
        NormalSpeed();
        Time += StopFFRew();
        if (TimeIsOffset)
        {
            m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
            DoPlayerSeek(Time);
        }
        else
        {
            auto time = static_cast<uint64_t>(Time);
            uint64_t desiredFrameRel = m_playerContext.m_player->TranslatePositionMsToFrame(time * 1000, HonorCutlist);
            m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
            DoPlayerSeekToFrame(desiredFrameRel);
        }
        bool paused = m_playerContext.m_player->IsPaused();
        UpdateOSDSeekMessage(Msg, paused ? kOSDTimeout_None : kOSDTimeout_Med);
    }
    else
        m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
}

void TV::DoSeekAbsolute(long long Seconds, bool HonorCutlist)
{
    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (!m_playerContext.m_player)
    {
        m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
        gCoreContext->emitTVPlaybackSought(-1);
        return;
    }
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
    DoSeek(Seconds, tr("Jump To"), /*timeIsOffset*/false, HonorCutlist);
    gCoreContext->emitTVPlaybackSought(Seconds);
}

void TV::DoArbSeek(ArbSeekWhence Whence, bool HonorCutlist)
{
    bool ok = false;
    int seek = GetQueuedInputAsInt(&ok);
    ClearInputQueues(true);
    if (!ok)
        return;

    int64_t time = (int(seek / 100) * 3600) + ((seek % 100) * 60);

    if (Whence == ARBSEEK_FORWARD)
    {
        DoSeek(time, tr("Jump Ahead"), /*timeIsOffset*/true, HonorCutlist);
    }
    else if (Whence == ARBSEEK_REWIND)
    {
        DoSeek(-time, tr("Jump Back"), /*timeIsOffset*/true, HonorCutlist);
    }
    else if (Whence == ARBSEEK_END)
    {
        m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
        if (!m_playerContext.m_player)
        {
            m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
            return;
        }
        uint64_t total_frames = m_playerContext.m_player->GetCurrentFrameCount();
        float dur = m_playerContext.m_player->ComputeSecs(total_frames, HonorCutlist);
        m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
        DoSeek(std::max(0.0F, dur - static_cast<float>(time)), tr("Jump To"), /*timeIsOffset*/false, HonorCutlist);
    }
    else
    {
        DoSeekAbsolute(time, HonorCutlist);
    }
}

void TV::NormalSpeed()
{
    if (!m_playerContext.m_ffRewSpeed)
        return;

    m_playerContext.m_ffRewSpeed = 0;

    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (m_playerContext.m_player)
        m_playerContext.m_player->Play(m_playerContext.m_tsNormal, true);
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);

    SetSpeedChangeTimer(0, __LINE__);
}

void TV::ChangeSpeed(int Direction)
{
    int old_speed = m_playerContext.m_ffRewSpeed;

    if (ContextIsPaused(__FILE__, __LINE__))
        m_playerContext.m_ffRewSpeed = -4;

    m_playerContext.m_ffRewSpeed += Direction;

    float time = StopFFRew();
    float speed = NAN;
    QString mesg;

    switch (m_playerContext.m_ffRewSpeed)
    {
        case  4: speed = 16.0F;     mesg = tr("Speed 16X");   break;
        case  3: speed = 8.0F;      mesg = tr("Speed 8X");    break;
        case  2: speed = 3.0F;      mesg = tr("Speed 3X");    break;
        case  1: speed = 2.0F;      mesg = tr("Speed 2X");    break;
        case  0: speed = 1.0F;      mesg = m_playerContext.GetPlayMessage(); break;
        case -1: speed = 1.0F / 3;  mesg = tr("Speed 1/3X");  break;
        case -2: speed = 1.0F / 8;  mesg = tr("Speed 1/8X");  break;
        case -3: speed = 1.0F / 16; mesg = tr("Speed 1/16X"); break;
        case -4:
            DoTogglePause(true);
            return;
        default:
            m_playerContext.m_ffRewSpeed = old_speed;
            return;
    }

    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (m_playerContext.m_player && !m_playerContext.m_player->Play(
            (!m_playerContext.m_ffRewSpeed) ? m_playerContext.m_tsNormal: speed, m_playerContext.m_ffRewSpeed == 0))
    {
        m_playerContext.m_ffRewSpeed = old_speed;
        m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
        return;
    }
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
    DoPlayerSeek(time);
    UpdateOSDSeekMessage(mesg, kOSDTimeout_Med);

    SetSpeedChangeTimer(0, __LINE__);
}

float TV::StopFFRew()
{
    float time = 0.0;

    if (!m_playerContext.m_ffRewState)
        return time;

    if (m_playerContext.m_ffRewState > 0)
        time = -m_ffRewSpeeds[static_cast<size_t>(m_playerContext.m_ffRewIndex)] * m_ffRewRepos;
    else
        time = m_ffRewSpeeds[static_cast<size_t>(m_playerContext.m_ffRewIndex)] * m_ffRewRepos;

    m_playerContext.m_ffRewState = 0;
    m_playerContext.m_ffRewIndex = kInitFFRWSpeed;

    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (m_playerContext.m_player)
        m_playerContext.m_player->Play(m_playerContext.m_tsNormal, true);
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);

    SetSpeedChangeTimer(0, __LINE__);

    return time;
}

void TV::ChangeFFRew(int Direction)
{
    if (m_playerContext.m_ffRewState == Direction)
    {
        while (++m_playerContext.m_ffRewIndex < static_cast<int>(m_ffRewSpeeds.size()))
            if (m_ffRewSpeeds[static_cast<size_t>(m_playerContext.m_ffRewIndex)])
                break;
        if (m_playerContext.m_ffRewIndex >= static_cast<int>(m_ffRewSpeeds.size()))
            m_playerContext.m_ffRewIndex = kInitFFRWSpeed;
        SetFFRew(m_playerContext.m_ffRewIndex);
    }
    else if (!m_ffRewReverse && m_playerContext.m_ffRewState == -Direction)
    {
        while (--m_playerContext.m_ffRewIndex >= kInitFFRWSpeed)
            if (m_ffRewSpeeds[static_cast<size_t>(m_playerContext.m_ffRewIndex)])
                break;
        if (m_playerContext.m_ffRewIndex >= kInitFFRWSpeed)
            SetFFRew(m_playerContext.m_ffRewIndex);
        else
        {
            float time = StopFFRew();
            DoPlayerSeek(time);
            UpdateOSDSeekMessage(m_playerContext.GetPlayMessage(), kOSDTimeout_Med);
        }
    }
    else
    {
        NormalSpeed();
        m_playerContext.m_ffRewState = Direction;
        SetFFRew(kInitFFRWSpeed);
    }
}

void TV::SetFFRew(int Index)
{
    if (!m_playerContext.m_ffRewState)
        return;

    auto index = static_cast<size_t>(Index);
    if (!m_ffRewSpeeds[index])
        return;

    auto ffrewindex = static_cast<size_t>(m_playerContext.m_ffRewIndex);
    int speed = 0;
    QString mesg;
    if (m_playerContext.m_ffRewState > 0)
    {
        speed = m_ffRewSpeeds[index];
        // Don't allow ffwd if seeking is needed but not available
        if (!m_playerContext.m_buffer || (!m_playerContext.m_buffer->IsSeekingAllowed() && speed > 3))
            return;

        m_playerContext.m_ffRewIndex = Index;
        mesg = tr("Forward %1X").arg(m_ffRewSpeeds[ffrewindex]);
        m_playerContext.m_ffRewSpeed = speed;
    }
    else
    {
        // Don't rewind if we cannot seek
        if (!m_playerContext.m_buffer || !m_playerContext.m_buffer->IsSeekingAllowed())
            return;

        m_playerContext.m_ffRewIndex = Index;
        mesg = tr("Rewind %1X").arg(m_ffRewSpeeds[ffrewindex]);
        speed = -m_ffRewSpeeds[ffrewindex];
        m_playerContext.m_ffRewSpeed = speed;
    }

    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (m_playerContext.m_player)
        m_playerContext.m_player->Play(static_cast<float>(speed), (speed == 1) && (m_playerContext.m_ffRewState > 0));
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);

    UpdateOSDSeekMessage(mesg, kOSDTimeout_None);

    SetSpeedChangeTimer(0, __LINE__);
}

void TV::DoQueueTranscode(const QString& Profile)
{
    m_playerContext.LockPlayingInfo(__FILE__, __LINE__);

    if (m_playerContext.GetState() == kState_WatchingPreRecorded)
    {
        bool stop = false;
        if (m_queuedTranscode ||
            JobQueue::IsJobQueuedOrRunning(
                     JOB_TRANSCODE,
                     m_playerContext.m_playingInfo->GetChanID(),
                     m_playerContext.m_playingInfo->GetRecordingStartTime()))
        {
            stop = true;
        }

        if (stop)
        {
            JobQueue::ChangeJobCmds(
                JOB_TRANSCODE,
                m_playerContext.m_playingInfo->GetChanID(),
                m_playerContext.m_playingInfo->GetRecordingStartTime(), JOB_STOP);
            m_queuedTranscode = false;
            SetOSDMessage(tr("Stopping Transcode"));
        }
        else
        {
            const RecordingInfo recinfo(*m_playerContext.m_playingInfo);
            recinfo.ApplyTranscoderProfileChange(Profile);
            QString jobHost = "";

            if (m_dbRunJobsOnRemote)
                jobHost = m_playerContext.m_playingInfo->GetHostname();

            QString msg = tr("Try Again");
            if (JobQueue::QueueJob(JOB_TRANSCODE,
                       m_playerContext.m_playingInfo->GetChanID(),
                       m_playerContext.m_playingInfo->GetRecordingStartTime(),
                       jobHost, "", "", JOB_USE_CUTLIST))
            {
                m_queuedTranscode = true;
                msg = tr("Transcoding");
            }
            SetOSDMessage(msg);
        }
    }
    m_playerContext.UnlockPlayingInfo(__FILE__, __LINE__);
}

int TV::GetNumChapters()
{
    int num_chapters = 0;
    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (m_playerContext.m_player)
        num_chapters = m_playerContext.m_player->GetNumChapters();
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
    return num_chapters;
}

void TV::GetChapterTimes(QList<long long> &Times)
{
    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (m_playerContext.m_player)
        m_playerContext.m_player->GetChapterTimes(Times);
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
}

int TV::GetCurrentChapter()
{
    int chapter = 0;
    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (m_playerContext.m_player)
        chapter = m_playerContext.m_player->GetCurrentChapter();
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
    return chapter;
}

void TV::DoJumpChapter(int Chapter)
{
    NormalSpeed();
    StopFFRew();

    PauseAudioUntilBuffered();

    UpdateOSDSeekMessage(tr("Jump Chapter"), kOSDTimeout_Med);
    SetUpdateOSDPosition(true);

    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (m_playerContext.m_player)
        m_playerContext.m_player->JumpChapter(Chapter);
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
}

int TV::GetNumTitles()
{
    int num_titles = 0;
    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (m_playerContext.m_player)
        num_titles = m_playerContext.m_player->GetNumTitles();
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
    return num_titles;
}

int TV::GetCurrentTitle()
{
    int currentTitle = 0;
    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (m_playerContext.m_player)
        currentTitle = m_playerContext.m_player->GetCurrentTitle();
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
    return currentTitle;
}

int TV::GetNumAngles()
{
    int num_angles = 0;
    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (m_playerContext.m_player)
        num_angles = m_playerContext.m_player->GetNumAngles();
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
    return num_angles;
}

int TV::GetCurrentAngle()
{
    int currentAngle = 0;
    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (m_playerContext.m_player)
        currentAngle = m_playerContext.m_player->GetCurrentAngle();
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
    return currentAngle;
}

QString TV::GetAngleName(int Angle)
{
    QString name;
    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (m_playerContext.m_player)
        name = m_playerContext.m_player->GetAngleName(Angle);
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
    return name;
}

int TV::GetTitleDuration(int Title)
{
    int seconds = 0;
    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (m_playerContext.m_player)
        seconds = m_playerContext.m_player->GetTitleDuration(Title);
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
    return seconds;
}


QString TV::GetTitleName(int Title)
{
    QString name;
    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (m_playerContext.m_player)
        name = m_playerContext.m_player->GetTitleName(Title);
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
    return name;
}

void TV::DoSwitchTitle(int Title)
{
    NormalSpeed();
    StopFFRew();

    PauseAudioUntilBuffered();

    UpdateOSDSeekMessage(tr("Switch Title"), kOSDTimeout_Med);
    SetUpdateOSDPosition(true);

    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (m_playerContext.m_player)
        m_playerContext.m_player->SwitchTitle(Title);
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
}

void TV::DoSwitchAngle(int Angle)
{
    NormalSpeed();
    StopFFRew();

    PauseAudioUntilBuffered();

    UpdateOSDSeekMessage(tr("Switch Angle"), kOSDTimeout_Med);
    SetUpdateOSDPosition(true);

    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (m_playerContext.m_player)
        m_playerContext.m_player->SwitchAngle(Angle);
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
}

void TV::DoSkipCommercials(int Direction)
{
    NormalSpeed();
    StopFFRew();

    if (StateIsLiveTV(GetState()))
        return;

    PauseAudioUntilBuffered();

    osdInfo info;
    m_playerContext.CalcPlayerSliderPosition(info);
    info.text["title"] = tr("Skip");
    info.text["description"] = tr("Searching");
    UpdateOSDStatus(info, kOSDFunctionalType_Default, kOSDTimeout_Med);
    SetUpdateOSDPosition(true);

    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (m_playerContext.m_player)
        m_playerContext.m_player->SkipCommercials(Direction);
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
}

void TV::SwitchSource(uint Direction)
{
    QMap<uint,InputInfo> sources;
    uint cardid  = m_playerContext.GetCardID();

    InfoMap info;
    m_playerContext.m_recorder->GetChannelInfo(info);
    uint sourceid = info["sourceid"].toUInt();

    std::vector<InputInfo> inputs = RemoteRequestFreeInputInfo(cardid);
    for (auto & input : inputs)
    {
        // prefer the current card's input in sources list
        if ((sources.find(input.m_sourceId) == sources.end()) ||
            ((cardid == input.m_inputId) && (cardid != sources[input.m_sourceId].m_inputId)))
        {
            sources[input.m_sourceId] = input;
        }
    }

    // Source switching
    QMap<uint,InputInfo>::const_iterator beg = sources.constFind(sourceid);
    QMap<uint,InputInfo>::const_iterator sit = beg;

    if (sit == sources.constEnd())
        return;

    if (kNextSource == Direction)
    {
        ++sit;
        if (sit == sources.constEnd())
            sit = sources.constBegin();
    }

    if (kPreviousSource == Direction)
    {
        if (sit != sources.constBegin())
            --sit;
        else
        {
            QMap<uint,InputInfo>::const_iterator tmp = sources.constBegin();
            while (tmp != sources.constEnd())
            {
                sit = tmp;
                ++tmp;
            }
        }
    }

    if (sit == beg)
        return;

    m_switchToInputId = (*sit).m_inputId;
    ScheduleInputChange();
}

void TV::SwitchInputs(uint ChanID, QString ChanNum, uint InputID)
{
    if (!m_playerContext.m_recorder)
        return;

    // this will re-create the player. Ensure any outstanding events are delivered
    // and processed before the player is deleted so that we don't confuse the
    // state of the new player e.g. when switching inputs from the guide grid,
    // "EPG_EXITING" may not be received until after the player is re-created
    // and we inadvertantly disable drawing...
    // TODO with recent changes, embedding should be ended synchronously and hence
    // this extra call should no longer be needed
    QCoreApplication::processEvents();

    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("(%1,'%2',%3)").arg(ChanID).arg(ChanNum).arg(InputID));

    RemoteEncoder *testrec = nullptr;

    if (!StateIsLiveTV(GetState()))
        return;

    QStringList reclist;
    if (InputID)
    {
        reclist.push_back(QString::number(InputID));
    }
    else if (ChanID || !ChanNum.isEmpty())
    {
        // If we are switching to a channel not on the current recorder
        // we need to find the next free recorder with that channel.
        reclist = ChannelUtil::GetValidRecorderList(ChanID, ChanNum);
    }

    if (!reclist.empty())
        testrec = RemoteRequestFreeRecorderFromList(reclist, m_playerContext.GetCardID());

    if (testrec && testrec->IsValidRecorder())
    {
        InputID = static_cast<uint>(testrec->GetRecorderNumber());

        // We are switching to a specific channel...
        if (ChanID && ChanNum.isEmpty())
            ChanNum = ChannelUtil::GetChanNum(static_cast<int>(ChanID));

        if (!ChanNum.isEmpty())
            CardUtil::SetStartChannel(InputID, ChanNum);
    }

    // If we are just switching recorders find first available recorder.
    if (!testrec)
        testrec = RemoteRequestNextFreeRecorder(static_cast<int>(m_playerContext.GetCardID()));

    if (testrec && testrec->IsValidRecorder())
    {
        // Switching inputs so clear the pseudoLiveTVState.
        m_playerContext.SetPseudoLiveTV(nullptr, kPseudoNormalLiveTV);
        bool muted = false;
        m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
        if (m_playerContext.m_player && m_playerContext.m_player->IsMuted())
            muted = true;
        m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);

        // pause the decoder first, so we're not reading too close to the end.
        if (m_playerContext.m_buffer)
        {
            m_playerContext.m_buffer->IgnoreLiveEOF(true);
            m_playerContext.m_buffer->StopReads();
        }

        if (m_playerContext.m_player)
            m_playerContext.m_player->PauseDecoder();

        // shutdown stuff
        if (m_playerContext.m_buffer)
        {
            m_playerContext.m_buffer->Pause();
            m_playerContext.m_buffer->WaitForPause();
        }

        m_playerContext.StopPlaying();
        m_playerContext.m_recorder->StopLiveTV();
        m_playerContext.SetPlayer(nullptr);

        // now restart stuff
        m_playerContext.m_lastSignalUIInfo.clear();
        m_lockTimerOn = false;

        m_playerContext.SetRecorder(testrec);
        m_playerContext.m_recorder->Setup();
        // We need to set channum for SpawnLiveTV..
        if (ChanNum.isEmpty() && ChanID)
            ChanNum = ChannelUtil::GetChanNum(static_cast<int>(ChanID));
        if (ChanNum.isEmpty() && InputID)
            ChanNum = CardUtil::GetStartingChannel(InputID);
        m_playerContext.m_recorder->SpawnLiveTV(m_playerContext.m_tvchain->GetID(), false, ChanNum);

        if (!m_playerContext.ReloadTVChain())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "LiveTV not successfully restarted");
            MythMainWindow::RestoreScreensaver();
            m_playerContext.SetRecorder(nullptr);
            SetErrored();
            SetExitPlayer(true, false);
        }
        else
        {
            m_playerContext.LockPlayingInfo(__FILE__, __LINE__);
            QString playbackURL = m_playerContext.m_playingInfo->GetPlaybackURL(true);
            bool opennow = (m_playerContext.m_tvchain->GetInputType(-1) != "DUMMY");
            m_playerContext.SetRingBuffer(
                MythMediaBuffer::Create(
                    playbackURL, false, true,
                    opennow ? MythMediaBuffer::kLiveTVOpenTimeout : -1));

            m_playerContext.m_tvchain->SetProgram(*m_playerContext.m_playingInfo);
            if (m_playerContext.m_buffer)
                m_playerContext.m_buffer->SetLiveMode(m_playerContext.m_tvchain);
            m_playerContext.UnlockPlayingInfo(__FILE__, __LINE__);
        }

        bool ok = false;
        if (m_playerContext.m_playingInfo && StartRecorder())
        {
            QRect dummy = QRect();
            if (m_playerContext.CreatePlayer(this, m_mainWindow, m_playerContext.GetState(),
                                             false, dummy, muted))
            {
                ScheduleStateChange();
                ok = true;
                m_playerContext.PushPreviousChannel();
                SetSpeedChangeTimer(25, __LINE__);
            }
            else
                StopStuff(true, true, true);
        }

        if (!ok)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "LiveTV not successfully started");
            MythMainWindow::RestoreScreensaver();
            m_playerContext.SetRecorder(nullptr);
            SetErrored();
            SetExitPlayer(true, false);
        }
        else
        {
            m_lockTimer.start();
            m_lockTimerOn = true;
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "No recorder to switch to...");
        delete testrec;
    }

    UnpauseLiveTV();
    UpdateOSDInput();

    ITVRestart(true);
}

void TV::ToggleChannelFavorite()
{
    // TOGGLEFAV was broken in [20523], this just prints something
    // out so as not to cause further confusion. See #8948.
    LOG(VB_GENERAL, LOG_ERR, "TV::ToggleChannelFavorite() -- currently disabled");
}

void TV::ToggleChannelFavorite(const QString& ChangroupName) const
{
    if (m_playerContext.m_recorder)
        m_playerContext.m_recorder->ToggleChannelFavorite(ChangroupName);
}

QString TV::GetQueuedInput() const
{
    return m_queuedInput;
}

int TV::GetQueuedInputAsInt(bool *OK, int Base) const
{
    return m_queuedInput.toInt(OK, Base);
}

QString TV::GetQueuedChanNum() const
{
    if (m_queuedChanNum.isEmpty())
        return "";

    // strip initial zeros and other undesirable characters
    int i = 0;
    for (; i < m_queuedChanNum.length(); i++)
    {
        if ((m_queuedChanNum[i] > '0') && (m_queuedChanNum[i] <= '9'))
            break;
    }
    m_queuedChanNum = m_queuedChanNum.right(m_queuedChanNum.length() - i);

    // strip whitespace at end of string
    m_queuedChanNum = m_queuedChanNum.trimmed();

    return m_queuedChanNum;
}

/**
 *  \brief Clear channel key buffer of input keys.
 *  \param ctx     Current player context
 *  \param hideosd if true, hides "channel_number" OSDSet.
 */
void TV::ClearInputQueues(bool Hideosd)
{
    if (Hideosd)
        HideOSDWindow("osd_input");

    m_queuedInput   = "";
    m_queuedChanNum = "";
    m_queuedChanID  = 0;
    if (m_queueInputTimerId)
    {
        KillTimer(m_queueInputTimerId);
        m_queueInputTimerId = 0;
    }
}

void TV::AddKeyToInputQueue(char Key)
{
    if (Key)
    {
        m_queuedInput   = m_queuedInput.append(Key).right(kInputKeysMax);
        m_queuedChanNum = m_queuedChanNum.append(Key).right(kInputKeysMax);
        if (!m_queueInputTimerId)
            m_queueInputTimerId = StartTimer(10, __LINE__);
    }

    bool commitSmart = false;
    QString inputStr = GetQueuedInput();

    // Always use immediate channel change when channel numbers are entered
    // in browse mode because in browse mode space/enter exit browse
    // mode and change to the currently browsed channel.
    if (StateIsLiveTV(GetState()) && !m_ccInputMode && !m_asInputMode &&
        IsBrowsing())
    {
        commitSmart = ProcessSmartChannel(inputStr);
    }

    // Handle OSD...
    inputStr = inputStr.isEmpty() ? "?" : inputStr;
    if (m_ccInputMode)
    {
        QString entryStr = (m_vbimode==VBIMode::PAL_TT) ? tr("TXT:") : tr("CC:");
        inputStr = entryStr + " " + inputStr;
    }
    else if (m_asInputMode)
    {
        inputStr = tr("Seek:", "seek to location") + " " + inputStr;
    }
    SetOSDText("osd_input", "osd_number_entry", inputStr, kOSDTimeout_Med);

    // Commit the channel if it is complete and smart changing is enabled.
    if (commitSmart)
        CommitQueuedInput();
}

static QString add_spacer(const QString &chan, const QString &spacer)
{
    if ((chan.length() >= 2) && !spacer.isEmpty())
        return chan.left(chan.length()-1) + spacer + chan.right(1);
    return chan;
}

bool TV::ProcessSmartChannel(QString &InputStr)
{
    QString chan = GetQueuedChanNum();

    if (chan.isEmpty())
        return false;

    // Check for and remove duplicate separator characters
    int size = static_cast<int>(chan.size());
    if ((size > 2) && (chan.at(size - 1) == chan.at(size - 2)))
    {
        bool ok = false;
        chan.rightRef(1).toUInt(&ok);
        if (!ok)
        {
            chan = chan.left(chan.length()-1);
            m_queuedChanNum = chan;
            if (!m_queueInputTimerId)
                m_queueInputTimerId = StartTimer(10, __LINE__);
        }
    }

    // Look for channel in line-up
    QString needed_spacer;
    // cppcheck-suppress variableScope
    uint    pref_cardid = 0;
    bool    is_not_complete = true;

    bool valid_prefix = false;
    if (m_playerContext.m_recorder)
    {
        valid_prefix = m_playerContext.m_recorder->CheckChannelPrefix(
            chan, pref_cardid, is_not_complete, needed_spacer);
    }

#if DEBUG_CHANNEL_PREFIX
    LOG(VB_GENERAL, LOG_DEBUG, QString("valid_pref(%1) cardid(%2) chan(%3) "
                                       "pref_cardid(%4) complete(%5) sp(%6)")
            .arg(valid_prefix).arg(0).arg(chan)
            .arg(pref_cardid).arg(is_not_complete).arg(needed_spacer));
#endif

    if (!valid_prefix)
    {
        // not a valid prefix.. reset...
        m_queuedChanNum = "";
    }
    else if (!needed_spacer.isEmpty())
    {
        // need a spacer..
        m_queuedChanNum = add_spacer(chan, needed_spacer);
    }

#if DEBUG_CHANNEL_PREFIX
    LOG(VB_GENERAL, LOG_DEBUG, QString(" ValidPref(%1) CardId(%2) Chan(%3) "
                                       " PrefCardId(%4) Complete(%5) Sp(%6)")
            .arg(valid_prefix).arg(0).arg(GetQueuedChanNum())
            .arg(pref_cardid).arg(is_not_complete).arg(needed_spacer));
#endif

    InputStr = m_queuedChanNum;
    if (!m_queueInputTimerId)
        m_queueInputTimerId = StartTimer(10, __LINE__);

    return !is_not_complete;
}

bool TV::CommitQueuedInput()
{
    bool commited = false;

    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("livetv(%1) qchannum(%2) qchanid(%3)")
            .arg(StateIsLiveTV(GetState()))
            .arg(GetQueuedChanNum())
            .arg(GetQueuedChanID()));

    if (m_ccInputMode)
    {
        commited = true;
        if (HasQueuedInput())
            HandleTrackAction(ACTION_TOGGLESUBS);
    }
    else if (m_asInputMode)
    {
        commited = true;
        if (HasQueuedInput())
            // XXX Should the cutlist be honored?
            DoArbSeek(ARBSEEK_FORWARD, /*honorCutlist*/false);
    }
    else if (StateIsLiveTV(GetState()))
    {
        QString channum = GetQueuedChanNum();
        if (IsBrowsing())
        {
            uint sourceid = 0;
            m_playerContext.LockPlayingInfo(__FILE__, __LINE__);
            if (m_playerContext.m_playingInfo)
                sourceid = m_playerContext.m_playingInfo->GetSourceID();
            m_playerContext.UnlockPlayingInfo(__FILE__, __LINE__);

            commited = true;
            if (channum.isEmpty())
                channum = GetBrowsedInfo().m_chanNum;
            uint chanid = GetBrowseChanId(channum, m_playerContext.GetCardID(), sourceid);
            if (chanid)
                BrowseChannel(channum);

            HideOSDWindow("osd_input");
        }
        else if (GetQueuedChanID() || !channum.isEmpty())
        {
            commited = true;
            ChangeChannel(GetQueuedChanID(), channum);
        }
    }

    ClearInputQueues(true);
    return commited;
}

void TV::ChangeChannel(ChannelChangeDirection Direction)
{
    if (m_dbUseChannelGroups || (Direction == CHANNEL_DIRECTION_FAVORITE))
    {
        uint old_chanid = 0;
        if (m_channelGroupId > -1)
        {
            m_playerContext.LockPlayingInfo(__FILE__, __LINE__);
            if (!m_playerContext.m_playingInfo)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "no active ctx playingInfo.");
                m_playerContext.UnlockPlayingInfo(__FILE__, __LINE__);
                ReturnPlayerLock();
                return;
            }
            // Collect channel info
            old_chanid = m_playerContext.m_playingInfo->GetChanID();
            m_playerContext.UnlockPlayingInfo(__FILE__, __LINE__);
        }

        if (old_chanid)
        {
            QMutexLocker locker(&m_channelGroupLock);
            if (m_channelGroupId > -1)
            {
                uint chanid = ChannelUtil::GetNextChannel(
                    m_channelGroupChannelList, old_chanid, 0, 0, Direction);
                if (chanid)
                    ChangeChannel(chanid, "");
                return;
            }
        }
    }

    if (Direction == CHANNEL_DIRECTION_FAVORITE)
        Direction = CHANNEL_DIRECTION_UP;

    QString oldinputname = m_playerContext.m_recorder->GetInput();

    if (ContextIsPaused(__FILE__, __LINE__))
    {
        HideOSDWindow("osd_status");
        MythMainWindow::DisableScreensaver();
    }

    // Save the current channel if this is the first time
    if (m_playerContext.m_prevChan.empty())
        m_playerContext.PushPreviousChannel();

    PauseAudioUntilBuffered();
    PauseLiveTV();

    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (m_playerContext.m_player)
    {
        m_playerContext.m_player->ResetCaptions();
        m_playerContext.m_player->ResetTeletext();
    }
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);

    m_playerContext.m_recorder->ChangeChannel(Direction);
    ClearInputQueues(false);

    if (m_playerContext.m_player)
        m_playerContext.m_player->GetAudio()->Reset();

    UnpauseLiveTV();

    if (oldinputname != m_playerContext.m_recorder->GetInput())
        UpdateOSDInput();
}

static uint get_chanid(const PlayerContext *ctx,
                       uint cardid, const QString &channum)
{
    uint chanid = 0;
    uint cur_sourceid = 0;
    // try to find channel on current input
    if (ctx && ctx->m_playingInfo && ctx->m_playingInfo->GetSourceID())
    {
        cur_sourceid = ctx->m_playingInfo->GetSourceID();
        chanid = std::max(static_cast<uint>(ChannelUtil::GetChanID(cur_sourceid, channum)), 0U);
        if (chanid)
            return chanid;
    }
    // try to find channel on specified input

    uint sourceid = CardUtil::GetSourceID(cardid);
    if (cur_sourceid != sourceid && sourceid)
        chanid = std::max(static_cast<uint>(ChannelUtil::GetChanID(sourceid, channum)), 0U);
    return chanid;
}

void TV::ChangeChannel(uint Chanid, const QString &Channum)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("(%1, '%2')").arg(Chanid).arg(Channum));

    if ((!Chanid && Channum.isEmpty()) || !m_playerContext.m_recorder)
        return;

    QString channum = Channum;
    QStringList reclist;
    QSet<uint> tunable_on;

    QString oldinputname = m_playerContext.m_recorder->GetInput();

    if (channum.isEmpty() && Chanid)
        channum = ChannelUtil::GetChanNum(static_cast<int>(Chanid));

    bool getit = false;
    if (m_playerContext.m_recorder)
    {
        if (kPseudoChangeChannel == m_playerContext.m_pseudoLiveTVState)
        {
            getit = false;
        }
        else if (kPseudoRecording == m_playerContext.m_pseudoLiveTVState)
        {
            getit = true;
        }
        else if (Chanid)
        {
            tunable_on = IsTunableOn(Chanid);
            getit = !tunable_on.contains(m_playerContext.GetCardID());
        }
        else
        {
            QString needed_spacer;
            uint pref_cardid = 0;
            uint cardid = m_playerContext.GetCardID();
            bool dummy = false;

            m_playerContext.m_recorder->CheckChannelPrefix(Channum,  pref_cardid,
                                              dummy, needed_spacer);

            LOG(VB_CHANNEL, LOG_INFO, LOC +
                QString("CheckChannelPrefix(%1, pref_cardid %2, %3, '%4') "
                        "cardid %5")
                .arg(Channum).arg(pref_cardid).arg(dummy).arg(needed_spacer)
                .arg(cardid));

            channum = add_spacer(Channum, needed_spacer);
            if (pref_cardid != cardid)
            {
                getit = true;
            }
            else
            {
                if (!Chanid)
                    Chanid = get_chanid(&m_playerContext, cardid, Channum);
                tunable_on = IsTunableOn(Chanid);
                getit = !tunable_on.contains(cardid);
            }
        }

        if (getit)
        {
            QStringList tmp =
                ChannelUtil::GetValidRecorderList(Chanid, channum);
            if (tunable_on.empty())
            {
                if (!Chanid)
                    Chanid = get_chanid(&m_playerContext, m_playerContext.GetCardID(), Channum);
                tunable_on = IsTunableOn(Chanid);
            }
            for (const auto& rec : qAsConst(tmp))
            {
                if ((Chanid == 0U) || tunable_on.contains(rec.toUInt()))
                    reclist.push_back(rec);
            }
        }
    }

    if (!reclist.empty())
    {
        RemoteEncoder *testrec = RemoteRequestFreeRecorderFromList(reclist, m_playerContext.GetCardID());
        if (!testrec || !testrec->IsValidRecorder())
        {
            ClearInputQueues(true);
            ShowNoRecorderDialog();
            delete testrec;
            return;
        }

        if (!m_playerContext.m_prevChan.empty() && m_playerContext.m_prevChan.back() == channum)
        {
            // need to remove it if the new channel is the same as the old.
            m_playerContext.m_prevChan.pop_back();
        }

        // found the card on a different recorder.
        uint inputid = static_cast<uint>(testrec->GetRecorderNumber());
        delete testrec;
        // Save the current channel if this is the first time
        if (m_playerContext.m_prevChan.empty())
            m_playerContext.PushPreviousChannel();
        SwitchInputs(Chanid, channum, inputid);
        return;
    }

    if (getit || !m_playerContext.m_recorder || !m_playerContext.m_recorder->CheckChannel(channum))
        return;

    if (ContextIsPaused(__FILE__, __LINE__))
    {
        HideOSDWindow("osd_status");
        MythMainWindow::DisableScreensaver();
    }

    // Save the current channel if this is the first time
    if (m_playerContext.m_prevChan.empty())
        m_playerContext.PushPreviousChannel();

    PauseAudioUntilBuffered();
    PauseLiveTV();

    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (m_playerContext.m_player)
    {
        m_playerContext.m_player->ResetCaptions();
        m_playerContext.m_player->ResetTeletext();
    }
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);

    m_playerContext.m_recorder->SetChannel(channum);

    if (m_playerContext.m_player)
        m_playerContext.m_player->GetAudio()->Reset();

    UnpauseLiveTV((Chanid != 0U) && (GetQueuedChanID() != 0U));

    if (oldinputname != m_playerContext.m_recorder->GetInput())
        UpdateOSDInput();
}

void TV::ChangeChannel(const ChannelInfoList &Options)
{
    for (const auto & option : Options)
    {
        uint    chanid  = option.m_chanId;
        QString channum = option.m_chanNum;

        if (chanid && !channum.isEmpty() && IsTunablePriv(chanid))
        {
            // hide the channel number, activated by certain signal monitors
            HideOSDWindow("osd_input");
            m_queuedInput   = channum;
            m_queuedChanNum = channum;
            m_queuedChanID  = chanid;
            if (!m_queueInputTimerId)
                m_queueInputTimerId = StartTimer(10, __LINE__);
            break;
        }
    }
}

void TV::ShowPreviousChannel()
{
    QString channum = m_playerContext.GetPreviousChannel();
    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("Previous channel number '%1'").arg(channum));
    if (channum.isEmpty())
        return;
    SetOSDText("osd_input", "osd_number_entry", channum, kOSDTimeout_Med);
}

void TV::PopPreviousChannel(bool ImmediateChange)
{
    if (!m_playerContext.m_tvchain)
        return;

    if (!ImmediateChange)
        ShowPreviousChannel();

    QString prev_channum = m_playerContext.PopPreviousChannel();
    QString cur_channum  = m_playerContext.m_tvchain->GetChannelName(-1);

    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("'%1'->'%2'")
            .arg(cur_channum).arg(prev_channum));

    // Only change channel if previous channel != current channel
    if (cur_channum != prev_channum && !prev_channum.isEmpty())
    {
        m_queuedInput   = prev_channum;
        m_queuedChanNum = prev_channum;
        m_queuedChanID  = 0;
        if (!m_queueInputTimerId)
            m_queueInputTimerId = StartTimer(10, __LINE__);
    }

    if (ImmediateChange)
    {
        // Turn off OSD Channel Num so the channel changes right away
        HideOSDWindow("osd_input");
    }
}

bool TV::ClearOSD()
{
    bool res = false;

    if (HasQueuedInput() || HasQueuedChannel())
    {
        ClearInputQueues(true);
        res = true;
    }

    OSD *osd = GetOSDL();
    if (osd)
    {
        osd->DialogQuit();
        osd->HideAll(true, nullptr, true); // pop OSD screen
        res = true;
    }
    ReturnOSDLock();

    if (IsBrowsing())
        BrowseEnd(false);

    return res;
}

/** \fn TV::ToggleOSD(const PlayerContext*, bool includeStatus)
 *  \brief Cycle through the available Info OSDs.
 */
void TV::ToggleOSD(bool IncludeStatusOSD)
{
    OSD *osd = GetOSDL();
    if (!osd)
    {
        ReturnOSDLock();
        return;
    }

    bool hideAll    = false;
    bool showStatus = false;
    bool paused     = ContextIsPaused(__FILE__, __LINE__);
    bool is_status_disp    = osd->IsWindowVisible("osd_status");
    bool has_prog_info     = osd->HasWindow("program_info");
    bool is_prog_info_disp = osd->IsWindowVisible("program_info");

    ReturnOSDLock();

    if (is_status_disp)
    {
        if (has_prog_info)
            UpdateOSDProgInfo("program_info");
        else
            hideAll = true;
    }
    else if (is_prog_info_disp && !paused)
    {
        hideAll = true;
    }
    else if (IncludeStatusOSD)
    {
        showStatus = true;
    }
    else
    {
        if (has_prog_info)
            UpdateOSDProgInfo("program_info");
    }

    if (hideAll || showStatus)
    {
        OSD *osd2 = GetOSDL();
        if (osd2)
            osd2->HideAll();
        ReturnOSDLock();
    }

    if (showStatus)
    {
        osdInfo info;
        if (m_playerContext.CalcPlayerSliderPosition(info))
        {
            info.text["title"] = (paused ? tr("Paused") : tr("Position"));
            UpdateOSDStatus(info, kOSDFunctionalType_Default,
                            paused ? kOSDTimeout_None : kOSDTimeout_Med);
            SetUpdateOSDPosition(true);
        }
        else
        {
            SetUpdateOSDPosition(false);
        }
    }
    else
    {
        SetUpdateOSDPosition(false);
    }
}

void TV::ToggleOSDDebug()
{
    bool show = false;
    OSD *osd = GetOSDL();
    if (osd && osd->IsWindowVisible("osd_debug"))
    {
        if (m_playerContext.m_buffer)
            m_playerContext.m_buffer->EnableBitrateMonitor(false);
        if (m_playerContext.m_player)
            m_playerContext.m_player->EnableFrameRateMonitor(false);
        osd->HideWindow("osd_debug");
    }
    else if (osd)
    {
        if (m_playerContext.m_buffer)
            m_playerContext.m_buffer->EnableBitrateMonitor(true);
        if (m_playerContext.m_player)
            m_playerContext.m_player->EnableFrameRateMonitor(true);
        show = true;
        if (!m_updateOSDDebugTimerId)
            m_updateOSDDebugTimerId = StartTimer(250, __LINE__);
    }
    ReturnOSDLock();
    if (show)
        UpdateOSDDebug();
}

void TV::UpdateOSDDebug()
{
    OSD *osd = GetOSDL();
    if (osd && m_playerContext.m_player)
    {
        InfoMap infoMap;
        m_playerContext.m_player->GetPlaybackData(infoMap);
        osd->ResetWindow("osd_debug");
        osd->SetText("osd_debug", infoMap, kOSDTimeout_None);
    }
    ReturnOSDLock();
}

/** \fn TV::UpdateOSDProgInfo(const PlayerContext*, const char *whichInfo)
 *  \brief Update and display the passed OSD set with programinfo
 */
void TV::UpdateOSDProgInfo(const char *WhichInfo)
{
    InfoMap infoMap;
    m_playerContext.GetPlayingInfoMap(infoMap);

    QString nightmode = gCoreContext->GetBoolSetting("NightModeEnabled", false) ? "yes" : "no";
    infoMap["nightmode"] = nightmode;

    // Clear previous osd and add new info
    OSD *osd = GetOSDL();
    if (osd)
    {
        osd->HideAll();
        osd->SetText(WhichInfo, infoMap, kOSDTimeout_Long);
    }
    ReturnOSDLock();
}

void TV::UpdateOSDStatus(osdInfo &Info, int Type, OSDTimeout Timeout)
{
    OSD *osd = GetOSDL();
    if (osd)
    {
        osd->ResetWindow("osd_status");
        QString nightmode = gCoreContext->GetBoolSetting("NightModeEnabled", false) ? "yes" : "no";
        Info.text.insert("nightmode", nightmode);
        osd->SetValues("osd_status", Info.values, Timeout);
        osd->SetText("osd_status",   Info.text, Timeout);
        if (Type != kOSDFunctionalType_Default)
            osd->SetFunctionalWindow("osd_status", static_cast<OSDFunctionalType>(Type));
    }
    ReturnOSDLock();
}

void TV::UpdateOSDStatus(const QString& Title, const QString& Desc,
                         const QString& Value, int Type, const QString& Units,
                         int Position, OSDTimeout Timeout)
{
    osdInfo info;
    info.values.insert("position", Position);
    info.values.insert("relposition", Position);
    info.text.insert("title", Title);
    info.text.insert("description", Desc);
    info.text.insert("value", Value);
    info.text.insert("units", Units);
    UpdateOSDStatus(info, Type, Timeout);
}

void TV::UpdateOSDSeekMessage(const QString &Msg, enum OSDTimeout Timeout)
{
    LOG(VB_PLAYBACK, LOG_INFO, QString("UpdateOSDSeekMessage(%1, %2)").arg(Msg).arg(Timeout));

    osdInfo info;
    if (m_playerContext.CalcPlayerSliderPosition(info))
    {
        int osdtype = (m_doSmartForward) ? kOSDFunctionalType_SmartForward : kOSDFunctionalType_Default;
        info.text["title"] = Msg;
        UpdateOSDStatus(info, osdtype, Timeout);
        SetUpdateOSDPosition(true);
    }
}

void TV::UpdateOSDInput()
{
    if (!m_playerContext.m_recorder || !m_playerContext.m_tvchain)
        return;
    QString displayName = CardUtil::GetDisplayName(m_playerContext.GetCardID());
    SetOSDMessage(displayName);
}

/** \fn TV::UpdateOSDSignal(const PlayerContext*, const QStringList&)
 *  \brief Updates Signal portion of OSD...
 */
void TV::UpdateOSDSignal(const QStringList &List)
{
    OSD *osd = GetOSDL();
    if (!osd || IsBrowsing() || !m_queuedChanNum.isEmpty())
    {
        if (&m_playerContext.m_lastSignalMsg != &List)
            m_playerContext.m_lastSignalMsg = List;
        ReturnOSDLock();
        m_signalMonitorTimerId = StartTimer(1, __LINE__);
        return;
    }
    ReturnOSDLock();

    SignalMonitorList slist = SignalMonitorValue::Parse(List);

    InfoMap infoMap = m_playerContext.m_lastSignalUIInfo;
    if ((!m_playerContext.m_lastSignalUIInfoTime.isRunning() ||
         (m_playerContext.m_lastSignalUIInfoTime.elapsed() > 5000)) ||
        infoMap["callsign"].isEmpty())
    {
        m_playerContext.m_lastSignalUIInfo.clear();
        m_playerContext.GetPlayingInfoMap(m_playerContext.m_lastSignalUIInfo);

        infoMap = m_playerContext.m_lastSignalUIInfo;
        m_playerContext.m_lastSignalUIInfoTime.start();
    }

    int i = 0;
    SignalMonitorList::const_iterator it;
    for (it = slist.begin(); it != slist.end(); ++it)
        if ("error" == it->GetShortName())
            infoMap[QString("error%1").arg(i++)] = it->GetName();
    i = 0;
    for (it = slist.begin(); it != slist.end(); ++it)
        if ("message" == it->GetShortName())
            infoMap[QString("message%1").arg(i++)] = it->GetName();

    int  sig  = 0;
    double snr = 0.0;
    uint  ber  = 0xffffffff;
    int   pos  = -1;
    int   tuned = -1;
    QString pat("");
    QString pmt("");
    QString mgt("");
    QString vct("");
    QString nit("");
    QString sdt("");
    QString crypt("");
    QString err;
    QString msg;
    for (it = slist.begin(); it != slist.end(); ++it)
    {
        if ("error" == it->GetShortName())
        {
            err = it->GetName();
            continue;
        }

        if ("message" == it->GetShortName())
        {
            msg = it->GetName();
            LOG(VB_GENERAL, LOG_INFO, "msg: " + msg);
            continue;
        }

        infoMap[it->GetShortName()] = QString::number(it->GetValue());
        if ("signal" == it->GetShortName())
            sig = it->GetNormalizedValue(0, 100);
        else if ("snr" == it->GetShortName())
            snr = it->GetValue();
        else if ("ber" == it->GetShortName())
            ber = static_cast<uint>(it->GetValue());
        else if ("pos" == it->GetShortName())
            pos = it->GetValue();
        else if ("script" == it->GetShortName())
            tuned = it->GetValue();
        else if ("seen_pat" == it->GetShortName())
            pat = it->IsGood() ? "a" : "_";
        else if ("matching_pat" == it->GetShortName())
            pat = it->IsGood() ? "A" : pat;
        else if ("seen_pmt" == it->GetShortName())
            pmt = it->IsGood() ? "m" : "_";
        else if ("matching_pmt" == it->GetShortName())
            pmt = it->IsGood() ? "M" : pmt;
        else if ("seen_mgt" == it->GetShortName())
            mgt = it->IsGood() ? "g" : "_";
        else if ("matching_mgt" == it->GetShortName())
            mgt = it->IsGood() ? "G" : mgt;
        else if ("seen_vct" == it->GetShortName())
            vct = it->IsGood() ? "v" : "_";
        else if ("matching_vct" == it->GetShortName())
            vct = it->IsGood() ? "V" : vct;
        else if ("seen_nit" == it->GetShortName())
            nit = it->IsGood() ? "n" : "_";
        else if ("matching_nit" == it->GetShortName())
            nit = it->IsGood() ? "N" : nit;
        else if ("seen_sdt" == it->GetShortName())
            sdt = it->IsGood() ? "s" : "_";
        else if ("matching_sdt" == it->GetShortName())
            sdt = it->IsGood() ? "S" : sdt;
        else if ("seen_crypt" == it->GetShortName())
            crypt = it->IsGood() ? "c" : "_";
        else if ("matching_crypt" == it->GetShortName())
            crypt = it->IsGood() ? "C" : crypt;
    }
    if (sig)
        infoMap["signal"] = QString::number(sig); // use normalized value

    bool    allGood = SignalMonitorValue::AllGood(slist);
    char    tuneCode = 0;
    QString slock   = ("1" == infoMap["slock"]) ? "L" : "l";
    QString lockMsg = (slock=="L") ? tr("Partial Lock") : tr("No Lock");
    QString sigMsg  = allGood ? tr("Lock") : lockMsg;

    QString sigDesc = tr("Signal %1%").arg(sig,2);
    if (snr > 0.0)
        sigDesc += " | " + tr("S/N %1dB").arg(log10(snr), 3, 'f', 1);
    if (ber != 0xffffffff)
        sigDesc += " | " + tr("BE %1", "Bit Errors").arg(ber, 2);
    if ((pos >= 0) && (pos < 100))
        sigDesc += " | " + tr("Rotor %1%").arg(pos,2);

    if (tuned == 1)
        tuneCode = 't';
    else if (tuned == 2)
        tuneCode = 'F';
    else if (tuned == 3)
        tuneCode = 'T';
    else
        tuneCode = '_';

    sigDesc = sigDesc + QString(" | (%1%2%3%4%5%6%7%8%9) %10")
              .arg(tuneCode).arg(slock).arg(pat).arg(pmt).arg(mgt).arg(vct)
              .arg(nit).arg(sdt).arg(crypt).arg(sigMsg);

    if (!err.isEmpty())
        sigDesc = err;
    else if (!msg.isEmpty())
        sigDesc = msg;

    osd = GetOSDL();
    if (osd)
    {
        infoMap["description"] = sigDesc;
        osd->SetText("program_info", infoMap, kOSDTimeout_Med);
    }
    ReturnOSDLock();

    m_playerContext.m_lastSignalMsg.clear();
    m_playerContext.m_lastSignalMsgTime.start();

    // Turn off lock timer if we have an "All Good" or good PMT
    if (allGood || (pmt == "M"))
    {
        m_lockTimerOn = false;
        m_lastLockSeenTime = MythDate::current();
    }
}

void TV::UpdateOSDTimeoutMessage()
{
    bool timed_out = false;

    if (m_playerContext.m_recorder)
    {
        QString input = m_playerContext.m_recorder->GetInput();
        uint timeout  = m_playerContext.m_recorder->GetSignalLockTimeout(input);
        timed_out = m_lockTimerOn && m_lockTimer.hasExpired(timeout);
    }

    OSD *osd = GetOSDL();

    if (!osd)
    {
        if (timed_out)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "You have no OSD, but tuning has already taken too long.");
        }
        ReturnOSDLock();
        return;
    }

    bool showing = osd->DialogVisible(OSD_DLG_INFO);
    if (!timed_out)
    {
        if (showing)
            osd->DialogQuit();
        ReturnOSDLock();
        return;
    }

    if (showing)
    {
        ReturnOSDLock();
        return;
    }

    // create dialog...
    static QString s_chanUp   = GET_KEY("TV Playback", ACTION_CHANNELUP);
    static QString s_chanDown = GET_KEY("TV Playback", ACTION_CHANNELDOWN);
    static QString s_nextSrc  = GET_KEY("TV Playback", "NEXTSOURCE");
    static QString s_togCards = GET_KEY("TV Playback", "NEXTINPUT");

    QString message = tr(
        "You should have received a channel lock by now. "
        "You can continue to wait for a signal, or you "
        "can change the channel with %1 or %2, change "
        "video source (%3), inputs (%4), etc.")
        .arg(s_chanUp).arg(s_chanDown).arg(s_nextSrc).arg(s_togCards);

    osd->DialogShow(OSD_DLG_INFO, message);
    QString action = "DIALOG_INFO_CHANNELLOCK_0";
    osd->DialogAddButton(tr("OK"), action);
    osd->DialogBack("", action, true);

    ReturnOSDLock();
}

void TV::UpdateLCD()
{
    // Make sure the LCD information gets updated shortly
    if (m_lcdTimerId)
        KillTimer(m_lcdTimerId);
    m_lcdTimerId = StartTimer(1, __LINE__);
}

void TV::ShowLCDChannelInfo()
{
    LCD *lcd = LCD::Get();
    m_playerContext.LockPlayingInfo(__FILE__, __LINE__);
    if (!lcd || !m_playerContext.m_playingInfo)
    {
        m_playerContext.UnlockPlayingInfo(__FILE__, __LINE__);
        return;
    }

    QString title    = m_playerContext.m_playingInfo->GetTitle();
    QString subtitle = m_playerContext.m_playingInfo->GetSubtitle();
    QString callsign = m_playerContext.m_playingInfo->GetChannelSchedulingID();

    m_playerContext.UnlockPlayingInfo(__FILE__, __LINE__);

    if ((callsign != m_lcdCallsign) || (title != m_lcdTitle) ||
        (subtitle != m_lcdSubtitle))
    {
        lcd->switchToChannel(callsign, title, subtitle);
        m_lcdCallsign = callsign;
        m_lcdTitle = title;
        m_lcdSubtitle = subtitle;
    }
}

void TV::ShowLCDDVDInfo()
{
    LCD *lcd = LCD::Get();
    if (!lcd || !m_playerContext.m_buffer || !m_playerContext.m_buffer->IsDVD())
        return;

    MythDVDBuffer *dvd = m_playerContext.m_buffer->DVD();
    QString dvdName;
    QString dvdSerial;
    QString mainStatus;
    QString subStatus;

    if (!dvd->GetNameAndSerialNum(dvdName, dvdSerial))
        dvdName = tr("DVD");

    if (dvd->IsInMenu())
    {
        mainStatus = tr("Menu");
    }
    else if (dvd->IsInStillFrame())
    {
        mainStatus = tr("Still Frame");
    }
    else
    {
        int playingTitle = 0;
        int playingPart = 0;

        dvd->GetPartAndTitle(playingPart, playingTitle);
        int totalParts = dvd->NumPartsInTitle();

        mainStatus = tr("Title: %1 (%2)").arg(playingTitle)
            .arg(MythFormatTime(static_cast<int>(dvd->GetTotalTimeOfTitle()), "HH:mm"));
        subStatus = tr("Chapter: %1/%2").arg(playingPart).arg(totalParts);
    }
    if ((dvdName != m_lcdCallsign) || (mainStatus != m_lcdTitle) || (subStatus != m_lcdSubtitle))
    {
        lcd->switchToChannel(dvdName, mainStatus, subStatus);
        m_lcdCallsign = dvdName;
        m_lcdTitle    = mainStatus;
        m_lcdSubtitle = subStatus;
    }
}

bool TV::IsTunable(uint ChanId)
{
    int dummy = 0;
    TV* tv = AcquireRelease(dummy, true);
    if (tv)
    {
        bool result = tv->IsTunableOn(ChanId).empty();
        AcquireRelease(dummy, false);
        return result;
    }
    return false;
}

bool TV::IsTunablePriv(uint ChanId)
{
    return !IsTunableOn(ChanId).empty();
}

static QString toCommaList(const QSet<uint> &list)
{
    QString ret = "";
    for (uint i : qAsConst(list))
        ret += QString("%1,").arg(i);

    if (!ret.isEmpty())
        return ret.left(ret.length()-1);

    return "";
}

QSet<uint> TV::IsTunableOn(uint ChanId)
{
    QSet<uint> tunable_cards;

    if (!ChanId)
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC + QString("ChanId (%1) - no").arg(ChanId));
        return tunable_cards;
    }

    GetPlayerReadLock();
    uint mplexid = ChannelUtil::GetMplexID(ChanId);
    mplexid = (32767 == mplexid) ? 0 : mplexid;

    uint excluded_input = 0;
    if (m_playerContext.m_recorder && m_playerContext.m_pseudoLiveTVState == kPseudoNormalLiveTV)
        excluded_input = m_playerContext.GetCardID();

    uint sourceid = ChannelUtil::GetSourceIDForChannel(ChanId);

    std::vector<InputInfo> inputs = RemoteRequestFreeInputInfo(excluded_input);

    for (auto & input : inputs)
    {
        if (input.m_sourceId != sourceid)
            continue;

        if (input.m_mplexId &&
            input.m_mplexId != mplexid)
            continue;

        if (!input.m_mplexId && input.m_chanId &&
            input.m_chanId != ChanId)
            continue;

        tunable_cards.insert(input.m_inputId);
    }

    if (tunable_cards.empty())
        LOG(VB_CHANNEL, LOG_INFO, LOC + QString("ChanId (%1) - no").arg(ChanId));
    else
        LOG(VB_CHANNEL, LOG_INFO, LOC + QString("ChanId (%1) yes { %2 }").arg(ChanId).arg(toCommaList(tunable_cards)));
    ReturnPlayerLock();
    return tunable_cards;
}

bool TV::StartEmbedding(const QRect &EmbedRect)
{
    GetPlayerReadLock();

    m_playerContext.StartEmbedding(EmbedRect);

    // Start checking for end of file for embedded window..
    if (m_embedCheckTimerId)
        KillTimer(m_embedCheckTimerId);
    m_embedCheckTimerId = StartTimer(kEmbedCheckFrequency, __LINE__);

    bool embedding = m_playerContext.IsEmbedding();
    ReturnPlayerLock();
    return embedding;
}

void TV::StopEmbedding(const QStringList &Data)
{    
    // Resize the window back to the MythTV Player size
    GetPlayerReadLock();

    if (m_playerContext.IsEmbedding())
        m_playerContext.StopEmbedding();

    // Stop checking for end of file for embedded window..
    {
        if (m_embedCheckTimerId)
            KillTimer(m_embedCheckTimerId);
        m_embedCheckTimerId = 0;
    }

    MythPainter *painter = m_mainWindow->GetPainter();
    if (painter)
        painter->FreeResources();

    GetPlayerReadLock();
    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (m_playerContext.m_player && m_playerContext.m_player->GetVideoOutput())
        m_playerContext.m_player->GetVideoOutput()->ResizeForVideo();
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
    ReturnPlayerLock();

    // m_playerBounds is not applicable when switching modes so
    // skip this logic in that case.
    if (!m_dbUseVideoModes)
        m_mainWindow->MoveResize(m_playerBounds);

    // Restore pause
    DoSetPauseState(m_savedPause);

    if (!m_weDisabledGUI)
    {
        m_weDisabledGUI = true;
        m_mainWindow->PushDrawDisabled();
    }

    m_ignoreKeyPresses = false;

    // additional data provided by PlaybackBox
    if (!Data.isEmpty())
    {
        ProgramInfo pginfo(Data);
        if (pginfo.HasPathname() || pginfo.GetChanID())
            PrepToSwitchToRecordedProgram(pginfo);
    }

    ReturnPlayerLock();
}

bool TV::DoSetPauseState(const bool& Pause)
{
    bool waspaused = ContextIsPaused(__FILE__, __LINE__);
    float time = 0.0F;
    if (Pause ^ waspaused)
        time = DoTogglePauseStart();
    if (Pause ^ waspaused)
        DoTogglePauseFinish(time, false);
    return waspaused;
}

void TV::DoEditSchedule(int EditType)
{
    // Prevent nesting of the pop-up UI
    if (m_ignoreKeyPresses)
        return;

    if ((EditType == kScheduleProgramGuide  && !RunProgramGuidePtr) ||
        (EditType == kScheduleProgramFinder && !RunProgramFinderPtr) ||
        (EditType == kScheduledRecording    && !RunScheduleEditorPtr) ||
        (EditType == kViewSchedule          && !RunViewScheduledPtr))
    {
        return;
    }

    GetPlayerReadLock();

    m_playerContext.LockPlayingInfo(__FILE__, __LINE__);
    if (!m_playerContext.m_playingInfo)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "no active ctx playingInfo.");
        m_playerContext.UnlockPlayingInfo(__FILE__, __LINE__);
        ReturnPlayerLock();
        return;
    }

    // Collect channel info
    const ProgramInfo pginfo(*m_playerContext.m_playingInfo);
    uint    chanid  = pginfo.GetChanID();
    QString channum = pginfo.GetChanNum();
    QDateTime starttime = MythDate::current();
    m_playerContext.UnlockPlayingInfo(__FILE__, __LINE__);

    ClearOSD();

    // Pause playback as needed...
    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    bool pause = !m_playerContext.m_player || (!StateIsLiveTV(GetState()) && !m_dbContinueEmbedded);
    if (m_playerContext.m_player)
    {
        pause |= !m_playerContext.m_player->GetVideoOutput();
        pause |= m_playerContext.m_player->IsPaused();
        if (!pause)
            pause |= m_playerContext.m_player->IsNearEnd();
    }
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);

    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("Pausing player: %1").arg(pause));
    m_savedPause = DoSetPauseState(pause);

    // Resize window to the MythTV GUI size
    MythDisplay* display = m_mainWindow->GetDisplay();
    if (display->UsingVideoModes())
    {
        bool hide = display->NextModeIsLarger(display->GetGUIResolution());
        if (hide)
            m_mainWindow->hide();
        display->SwitchToGUI(true);
        if (hide)
            m_mainWindow->Show();
    }

    if (!m_dbUseGuiSizeForTv)
        m_mainWindow->MoveResize(m_savedGuiBounds);
#ifdef Q_OS_ANDROID
    m_mainWindow->Show();
#else
    m_mainWindow->show();
#endif
    ReturnPlayerLock();


    // Actually show the pop-up UI
    switch (EditType)
    {
        case kScheduleProgramGuide:
        {
            RunProgramGuidePtr(chanid, channum, starttime, this,
                               !pause, true, m_channelGroupId);
            m_ignoreKeyPresses = true;
            break;
        }
        case kScheduleProgramFinder:
        {
            RunProgramFinderPtr(this, !pause, true);
            m_ignoreKeyPresses = true;
            break;
        }
        case kScheduledRecording:
        {
            RunScheduleEditorPtr(&pginfo, reinterpret_cast<void*>(this));
            m_ignoreKeyPresses = true;
            break;
        }
        case kViewSchedule:
        {
            RunViewScheduledPtr(reinterpret_cast<void*>(this), !pause);
            m_ignoreKeyPresses = true;
            break;
        }
        case kPlaybackBox:
        {
            RunPlaybackBoxPtr(reinterpret_cast<void*>(this), !pause);
            m_ignoreKeyPresses = true;
            break;
        }
    }

    // We are embedding in a mythui window so assuming no one
    // else has disabled painting show the MythUI window again.
    if (m_weDisabledGUI)
    {
        m_mainWindow->PopDrawDisabled();
        m_weDisabledGUI = false;
    }
}

void TV::EditSchedule(int EditType)
{
    // post the request so the guide will be created in the UI thread
    QString message = QString("START_EPG %1").arg(EditType);
    auto* me = new MythEvent(message);
    QCoreApplication::postEvent(this, me);
}

void TV::ChangeVolume(bool Up, int NewVolume)
{
    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (!m_playerContext.m_player || !m_playerContext.m_player->PlayerControlsVolume())
    {
        m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
        return;
    }

    bool setabsolute = (NewVolume >= 0 && NewVolume <= 100);

    if (m_playerContext.m_player->IsMuted() && (Up || setabsolute))
        ToggleMute();

    uint curvol = setabsolute ? m_playerContext.m_player->SetVolume(NewVolume) :
                                m_playerContext.m_player->AdjustVolume((Up) ? +2 : -2);

    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);

    if (!IsBrowsing())
    {
        UpdateOSDStatus(tr("Adjust Volume"), tr("Volume"), QString::number(curvol),
                        kOSDFunctionalType_PictureAdjust, "%", static_cast<int>(curvol * 10),
                        kOSDTimeout_Med);
        SetUpdateOSDPosition(false);

        if (LCD *lcd = LCD::Get())
        {
            QString appName = tr("Video");

            if (StateIsLiveTV(GetState()))
                appName = tr("TV");

            if (m_playerContext.m_buffer && m_playerContext.m_buffer->IsDVD())
                appName = tr("DVD");

            lcd->switchToVolume(appName);
            lcd->setVolumeLevel(static_cast<float>(curvol) / 100);

            if (m_lcdVolumeTimerId)
                KillTimer(m_lcdVolumeTimerId);

            m_lcdVolumeTimerId = StartTimer(2000, __LINE__);
        }
    }
}

void TV::ToggleTimeStretch()
{
    if (m_playerContext.m_tsNormal == 1.0F)
    {
        m_playerContext.m_tsNormal = m_playerContext.m_tsAlt;
    }
    else
    {
        m_playerContext.m_tsAlt = m_playerContext.m_tsNormal;
        m_playerContext.m_tsNormal = 1.0F;
    }
    ChangeTimeStretch(0, false);
}

void TV::ChangeTimeStretch(int Dir, bool AllowEdit)
{
    const float kTimeStretchMin = 0.5;
    const float kTimeStretchMax = 2.0;
    const float kTimeStretchStep = 0.05F;
    float new_ts_normal = m_playerContext.m_tsNormal + (kTimeStretchStep * Dir);
    m_stretchAdjustment = AllowEdit;

    if (new_ts_normal > kTimeStretchMax &&
        m_playerContext.m_tsNormal < kTimeStretchMax)
    {
        new_ts_normal = kTimeStretchMax;
    }
    else if (new_ts_normal < kTimeStretchMin &&
             m_playerContext.m_tsNormal > kTimeStretchMin)
    {
        new_ts_normal = kTimeStretchMin;
    }

    if (new_ts_normal > kTimeStretchMax ||
        new_ts_normal < kTimeStretchMin)
    {
        return;
    }

    m_playerContext.m_tsNormal = kTimeStretchStep * lroundf(new_ts_normal / kTimeStretchStep);

    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (m_playerContext.m_player && !m_playerContext.m_player->IsPaused())
            m_playerContext.m_player->Play(m_playerContext.m_tsNormal, true);
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);

    if (!IsBrowsing())
    {
        if (!AllowEdit)
        {
            UpdateOSDSeekMessage(m_playerContext.GetPlayMessage(), kOSDTimeout_Short);
        }
        else
        {
            UpdateOSDStatus(tr("Adjust Time Stretch"), tr("Time Stretch"),
                            QString::number(static_cast<double>(m_playerContext.m_tsNormal), 'f', 2),
                            kOSDFunctionalType_TimeStretchAdjust, "X",
                            static_cast<int>(m_playerContext.m_tsNormal * (1000 / kTimeStretchMax)),
                            kOSDTimeout_None);
            SetUpdateOSDPosition(false);
        }
    }

    SetSpeedChangeTimer(0, __LINE__);
}

void TV::EnableUpmix(bool Enable, bool Toggle)
{
    if (!m_playerContext.m_player || !m_playerContext.m_player->HasAudioOut())
        return;

    bool enabled = false;

    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (Toggle)
        enabled = m_playerContext.m_player->GetAudio()->EnableUpmix(false, true);
    else
        enabled = m_playerContext.m_player->GetAudio()->EnableUpmix(Enable);
    // May have to disable digital passthrough
    m_playerContext.m_player->ForceSetupAudioStream();
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);

    if (!IsBrowsing())
        SetOSDMessage(enabled ? tr("Upmixer On") : tr("Upmixer Off"));
}

void TV::ChangeSubtitleZoom(int Dir)
{
    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (!m_playerContext.m_player)
    {
        m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
        return;
    }

    OSD *osd = GetOSDL();
    SubtitleScreen *subs = nullptr;
    if (osd)
        subs = osd->InitSubtitles();
    ReturnOSDLock();
    m_subtitleZoomAdjustment = true;
    bool showing = m_playerContext.m_player->GetCaptionsEnabled();
    int newval = (subs ? subs->GetZoom() : 100) + Dir;
    newval = std::max(50, newval);
    newval = std::min(200, newval);
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);

    if (showing && !IsBrowsing())
    {
        UpdateOSDStatus(tr("Adjust Subtitle Zoom"), tr("Subtitle Zoom"),
                        QString::number(newval),
                        kOSDFunctionalType_SubtitleZoomAdjust,
                        "%", newval * 1000 / 200, kOSDTimeout_None);
        SetUpdateOSDPosition(false);
        if (subs)
            subs->SetZoom(newval);
    }
}

// dir in 10ms jumps
void TV::ChangeSubtitleDelay(int Dir)
{
    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (!m_playerContext.m_player)
    {
        m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
        return;
    }

    OSD *osd = GetOSDL();
    SubtitleScreen *subs = nullptr;
    if (osd)
        subs = osd->InitSubtitles();
    ReturnOSDLock();
    m_subtitleDelayAdjustment = true;
    uint capmode = m_playerContext.m_player->GetCaptionMode();
    bool showing = m_playerContext.m_player->GetCaptionsEnabled() &&
                       (capmode == kDisplayRawTextSubtitle || capmode == kDisplayTextSubtitle);
    int newval = (subs ? subs->GetDelay() : 100) + Dir * 10;
    newval = std::max(-5000, newval);
    newval = std::min(5000, newval);
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);

    if (showing && !IsBrowsing())
    {
        // range of -5000ms..+5000ms, scale to 0..1000
        UpdateOSDStatus(tr("Adjust Subtitle Delay"), tr("Subtitle Delay"),
                        QString::number(newval),
                        kOSDFunctionalType_SubtitleDelayAdjust,
                        "ms", newval / 10 + 500, kOSDTimeout_None);
        SetUpdateOSDPosition(false);
        if (subs)
            subs->SetDelay(newval);
    }
}

// dir in 10ms jumps
void TV::ChangeAudioSync(int Dir, int NewSync)
{
    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (!m_playerContext.m_player)
    {
        m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
        return;
    }

    m_audiosyncAdjustment = true;
    long long newval = m_playerContext.m_player->AdjustAudioTimecodeOffset(Dir * 10, NewSync);
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);

    if (!IsBrowsing())
    {
        UpdateOSDStatus(tr("Adjust Audio Sync"), tr("Audio Sync"),
                        QString::number(newval), kOSDFunctionalType_AudioSyncAdjust,
                        "ms", (static_cast<int>(newval) / 2) + 500, kOSDTimeout_None);
        SetUpdateOSDPosition(false);
    }
}

void TV::ToggleMute(const bool MuteIndividualChannels)
{
    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (!m_playerContext.m_player || !m_playerContext.m_player->HasAudioOut() ||
        !m_playerContext.m_player->PlayerControlsVolume())
    {
        m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
        return;
    }

    MuteState mute_status = kMuteOff;

    if (!MuteIndividualChannels)
    {
        m_playerContext.m_player->SetMuted(!m_playerContext.m_player->IsMuted());
        mute_status = (m_playerContext.m_player->IsMuted()) ? kMuteAll : kMuteOff;
    }
    else
    {
        mute_status = m_playerContext.m_player->IncrMuteState();
    }
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);

    QString text;

    switch (mute_status)
    {
        case kMuteOff:   text = tr("Mute Off"); break;
        case kMuteAll:   text = tr("Mute On"); break;
        case kMuteLeft:  text = tr("Left Channel Muted"); break;
        case kMuteRight: text = tr("Right Channel Muted"); break;
    }

    UpdateNavDialog();
    SetOSDMessage(text);
}

void TV::ToggleSleepTimer()
{
    QString text;

    // increment sleep index, cycle through
    if (++m_sleepIndex == s_sleepTimes.size())
        m_sleepIndex = 0;

    // set sleep timer to next sleep_index timeout
    if (m_sleepTimerId)
    {
        KillTimer(m_sleepTimerId);
        m_sleepTimerId = 0;
        m_sleepTimerTimeout = 0;
    }

    if (s_sleepTimes[m_sleepIndex].seconds != 0)
    {
        m_sleepTimerTimeout = static_cast<uint>(s_sleepTimes[m_sleepIndex].seconds * 1000);
        m_sleepTimerId = StartTimer(static_cast<int>(m_sleepTimerTimeout), __LINE__);
    }

    text = tr("Sleep ") + " " + s_sleepTimes[m_sleepIndex].dispString;

    if (!IsBrowsing())
        SetOSDMessage(text);
}

void TV::ShowOSDSleep()
{
    KillTimer(m_sleepTimerId);
    m_sleepTimerId = 0;

    GetPlayerReadLock();
    OSD *osd = GetOSDL();
    if (osd)
    {
        QString message = tr("MythTV was set to sleep after %1 minutes and "
                             "will exit in %d seconds.\n"
                             "Do you wish to continue watching?")
                             .arg(static_cast<double>(m_sleepTimerTimeout * (1.0F / 60000.0F)));

        osd->DialogShow(OSD_DLG_SLEEP, message, kSleepTimerDialogTimeout);
        osd->DialogAddButton(tr("Yes"), "DIALOG_SLEEP_YES_0");
        osd->DialogAddButton(tr("No"),  "DIALOG_SLEEP_NO_0");
    }
    ReturnOSDLock();
    ReturnPlayerLock();

    m_sleepDialogTimerId = StartTimer(kSleepTimerDialogTimeout, __LINE__);
}

void TV::HandleOSDSleep(const QString& Action)
{
    if (!DialogIsVisible(OSD_DLG_SLEEP))
        return;

    if (Action == "YES")
    {
        if (m_sleepDialogTimerId)
        {
            KillTimer(m_sleepDialogTimerId);
            m_sleepDialogTimerId = 0;
        }
        m_sleepTimerId = StartTimer(static_cast<int>(m_sleepTimerTimeout) * 1000, __LINE__);
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "No longer watching TV, exiting");
        SetExitPlayer(true, true);
    }
}

void TV::SleepDialogTimeout()
{
    KillTimer(m_sleepDialogTimerId);
    m_sleepDialogTimerId = 0;

    LOG(VB_GENERAL, LOG_INFO, LOC + "Sleep timeout reached, exiting player.");

    SetExitPlayer(true, true);
}

/*!
 *  \brief After idleTimer has expired, display a dialogue warning the user
 *         that we will exit LiveTV unless they take action.
 *         We change idleTimer, to 45 seconds and when it expires for a second
 *         time we quit the player.
 *         If the user so decides, they may hit ok and we reset the timer
 *         back to the default expiry period.
 */
void TV::ShowOSDIdle()
{
    KillTimer(m_idleTimerId);
    m_idleTimerId = 0;

    GetPlayerReadLock();
    OSD *osd = GetOSDL();
    if (osd)
    {
        QString message = tr("MythTV has been idle for %1 minutes and "
                             "will exit in %d seconds. Are you still watching?")
                             .arg(static_cast<double>(m_dbIdleTimeout * (1.0F / 60000.0F)));

        osd->DialogShow(OSD_DLG_IDLE, message, kIdleTimerDialogTimeout);
        osd->DialogAddButton(tr("Yes"), "DIALOG_IDLE_YES_0");
        osd->DialogAddButton(tr("No"),  "DIALOG_IDLE_NO_0");
    }
    ReturnOSDLock();
    ReturnPlayerLock();

    m_idleDialogTimerId = StartTimer(kIdleTimerDialogTimeout, __LINE__);
}

void TV::HandleOSDIdle(const QString& Action)
{
    if (!DialogIsVisible(OSD_DLG_IDLE))
        return;

    if (Action == "YES")
    {
        if (m_idleDialogTimerId)
        {
            KillTimer(m_idleDialogTimerId);
            m_idleDialogTimerId = 0;
        }
        if (m_idleTimerId)
            KillTimer(m_idleTimerId);
        m_idleTimerId = StartTimer(static_cast<int>(m_dbIdleTimeout), __LINE__);
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "No longer watching LiveTV, exiting");
        SetExitPlayer(true, true);
    }
}

void TV::IdleDialogTimeout()
{
    KillTimer(m_idleDialogTimerId);
    m_idleDialogTimerId = 0;

    GetPlayerReadLock();
    if (StateIsLiveTV(m_playerContext.GetState()))
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "Idle timeout reached, leaving LiveTV");
        SetExitPlayer(true, true);
    }
    ReturnPlayerLock();
}

void TV::ToggleMoveBottomLine()
{
    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (!m_playerContext.m_player)
    {
        m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
        return;
    }

    m_playerContext.m_player->ToggleMoveBottomLine();
    QString msg = m_playerContext.m_player->GetVideoOutput()->GetZoomString();

    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);

    SetOSDMessage(msg);
}

void TV::SaveBottomLine()
{
    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (!m_playerContext.m_player)
    {
        m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
        return;
    }

    m_playerContext.m_player->SaveBottomLine();

    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);

    SetOSDMessage("Current 'Manual Zoom' saved for 'BottomLine'.");
}

void TV::ToggleAspectOverride(AspectOverrideMode AspectMode)
{
    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (!m_playerContext.m_player)
    {
        m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
        return;
    }
    m_playerContext.m_player->ToggleAspectOverride(AspectMode);
    QString text = toString(m_playerContext.m_player->GetAspectOverride());
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);

    SetOSDMessage(text);
}

void TV::ToggleAdjustFill(AdjustFillMode AdjustfillMode)
{
    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (!m_playerContext.m_player)
    {
        m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
        return;
    }
    m_playerContext.m_player->ToggleAdjustFill(AdjustfillMode);
    QString text = toString(m_playerContext.m_player->GetAdjustFill());
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);

    SetOSDMessage(text);
}

void TV::PauseAudioUntilBuffered()
{
    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (m_playerContext.m_player)
        m_playerContext.m_player->GetAudio()->PauseAudioUntilBuffered();
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
}

/// This handles all custom events
void TV::customEvent(QEvent *Event)
{
    if (Event->type() == MythEvent::kUpdateTvProgressEventType && m_myWindow)
    {
        m_myWindow->UpdateProgress();
        return;
    }

    if (Event->type() == MythEvent::MythUserMessage)
    {
        auto *me = dynamic_cast<MythEvent*>(Event);
        if (me == nullptr)
            return;
        QString message = me->Message();

        if (message.isEmpty())
            return;

        int timeout = 0;
        if (me->ExtraDataCount() == 1)
        {
            auto t = me->ExtraData(0).toInt();
            if (t > 0 && t < 1000)
                timeout = t * 1000;
        }

        if (timeout > 0)
            message += " (%d)";

        GetPlayerReadLock();
        OSD *osd = GetOSDL();
        if (osd)
            osd->DialogShow(OSD_DLG_CONFIRM, message, timeout);
        ReturnOSDLock();
        ReturnPlayerLock();

        return;
    }

    if (Event->type() == MythEvent::kUpdateBrowseInfoEventType)
    {
        auto *b = reinterpret_cast<UpdateBrowseInfoEvent*>(Event);
        GetPlayerReadLock();
        OSD *osd = GetOSDL();
        if (osd)
        {
            osd->SetText("browse_info", b->m_im, kOSDTimeout_None);
            osd->SetExpiry("browse_info", kOSDTimeout_None);
        }
        ReturnOSDLock();
        ReturnPlayerLock();
        return;
    }

    if (Event->type() == DialogCompletionEvent::kEventType)
    {
        auto *dce = reinterpret_cast<DialogCompletionEvent*>(Event);
        if (dce->GetData().userType() == qMetaTypeId<MenuNodeTuple>())
        {
            auto data = dce->GetData().value<MenuNodeTuple>();
            if (dce->GetResult() == -1) // menu exit/back
            {
                PlaybackMenuShow(data.m_menu, data.m_node.parentNode(),
                                 data.m_node);
            }
            else
            {
                PlaybackMenuShow(data.m_menu, data.m_node, QDomNode());
            }
        }
        else
        {
            OSDDialogEvent(dce->GetResult(), dce->GetResultText(), dce->GetData().toString());
        }
        return;
    }

    if (Event->type() == OSDHideEvent::kEventType)
    {
        auto *ce = reinterpret_cast<OSDHideEvent*>(Event);
        HandleOSDClosed(ce->GetFunctionalType());
        return;
    }

    // Stop DVD playback cleanly when the DVD is ejected
    if (Event->type() == MythMediaEvent::kEventType)
    {
        GetPlayerReadLock();
        TVState state = m_playerContext.GetState();
        if (state != kState_WatchingDVD)
        {
            ReturnPlayerLock();
            return;
        }

        auto *me = dynamic_cast<MythMediaEvent*>(Event);
        if (me == nullptr)
            return;
        MythMediaDevice *device = me->getDevice();

        QString filename = m_playerContext.m_buffer ? m_playerContext.m_buffer->GetFilename() : "";

        if (device && filename.endsWith(device->getDevicePath()) && (device->getStatus() == MEDIASTAT_OPEN))
        {
            LOG(VB_GENERAL, LOG_NOTICE, "DVD has been ejected, exiting playback");
            PrepareToExitPlayer(__LINE__, kBookmarkAuto);
            SetExitPlayer(true, true);
        }
        ReturnPlayerLock();
        return;
    }

    if (Event->type() != MythEvent::MythEventMessage)
        return;

    uint cardnum   = 0;
    auto *me = dynamic_cast<MythEvent*>(Event);
    if (me == nullptr)
        return;
    QString message = me->Message();

    // TODO Go through these and make sure they make sense...
#if QT_VERSION < QT_VERSION_CHECK(5,14,0)
    QStringList tokens = message.split(" ", QString::SkipEmptyParts);
#else
    QStringList tokens = message.split(" ", Qt::SkipEmptyParts);
#endif

    if (me->ExtraDataCount() == 1)
    {
        GetPlayerReadLock();
        int value = me->ExtraData(0).toInt();
        if (message == ACTION_SETVOLUME)
            ChangeVolume(false, value);
        else if (message == ACTION_SETAUDIOSYNC)
            ChangeAudioSync(0, value);
        else if (message == ACTION_SETBRIGHTNESS)
        {
            DoChangePictureAttribute(kAdjustingPicture_Playback,
                                     kPictureAttribute_Brightness,
                                     false, value);
        }
        else if (message == ACTION_SETCONTRAST)
        {
            DoChangePictureAttribute(kAdjustingPicture_Playback,
                                     kPictureAttribute_Contrast,
                                     false, value);
        }
        else if (message == ACTION_SETCOLOUR)
        {
            DoChangePictureAttribute(kAdjustingPicture_Playback,
                                     kPictureAttribute_Colour,
                                     false, value);
        }
        else if (message == ACTION_SETHUE)
        {
            DoChangePictureAttribute(kAdjustingPicture_Playback,
                                     kPictureAttribute_Hue,
                                     false, value);
        }
        else if (message == ACTION_JUMPCHAPTER)
        {
            DoJumpChapter(value);
        }
        else if (message == ACTION_SWITCHTITLE)
        {
            DoSwitchTitle(value - 1);
        }
        else if (message == ACTION_SWITCHANGLE)
        {
            DoSwitchAngle(value);
        }
        else if (message == ACTION_SEEKABSOLUTE)
        {
            DoSeekAbsolute(value, /*honorCutlist*/true);
        }
        ReturnPlayerLock();
    }

    if (message == ACTION_SCREENSHOT)
    {
        int width = 0;
        int height = 0;
        QString filename;

        if (me->ExtraDataCount() >= 2)
        {
            width  = me->ExtraData(0).toInt();
            height = me->ExtraData(1).toInt();

            if (me->ExtraDataCount() == 3)
                filename = me->ExtraData(2);
        }
        MythMainWindow::ScreenShot(width, height, filename);
    }
    else if (message == ACTION_GETSTATUS)
    {
        GetStatus();
    }
    else if (message.startsWith("DONE_RECORDING"))
    {
        int seconds = 0;
        //long long frames = 0;
        int NUMTOKENS = 4; // Number of tokens expected
        if (tokens.size() == NUMTOKENS)
        {
            cardnum = tokens[1].toUInt();
            seconds = tokens[2].toInt();
            //frames = tokens[3].toLongLong();
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, QString("DONE_RECORDING event received "
                                             "with invalid number of arguments, "
                                             "%1 expected, %2 actual")
                                                .arg(NUMTOKENS-1)
                                                .arg(tokens.size()-1));
            return;
        }

        GetPlayerReadLock();
        if (m_playerContext.GetState() == kState_WatchingRecording)
        {
            if (m_playerContext.m_recorder && (cardnum == m_playerContext.GetCardID()))
            {
                m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
                if (m_playerContext.m_player)
                {
                    m_playerContext.m_player->SetWatchingRecording(false);
                    if (seconds > 0)
                        m_playerContext.m_player->SetLength(seconds);
                }
                m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);

                m_playerContext.ChangeState(kState_WatchingPreRecorded);
                ScheduleStateChange();
            }
        }
        else if (StateIsLiveTV(m_playerContext.GetState()))
        {
            if (m_playerContext.m_recorder && cardnum == m_playerContext.GetCardID() &&
                m_playerContext.m_tvchain && m_playerContext.m_tvchain->HasNext())
            {
                m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
                if (m_playerContext.m_player)
                {
                    m_playerContext.m_player->SetWatchingRecording(false);
                    if (seconds > 0)
                        m_playerContext.m_player->SetLength(seconds);
                }
                m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
            }
        }
        ReturnPlayerLock();
    }

    if (message.startsWith("ASK_RECORDING "))
    {
        int timeuntil = 0;
        bool hasrec = false;
        bool haslater = false;
        if (tokens.size() >= 5)
        {
            cardnum   = tokens[1].toUInt();
            timeuntil = tokens[2].toInt();
            hasrec    = (tokens[3].toInt() != 0);
            haslater  = (tokens[4].toInt() != 0);
        }
        LOG(VB_GENERAL, LOG_DEBUG,
            LOC + message + QString(" hasrec: %1 haslater: %2")
                .arg(hasrec).arg(haslater));

        GetPlayerReadLock();
        if (m_playerContext.m_recorder && cardnum == m_playerContext.GetCardID())
            AskAllowRecording(me->ExtraDataList(), timeuntil, hasrec, haslater);

        ReturnPlayerLock();
    }

    if (message.startsWith("QUIT_LIVETV"))
    {
        cardnum = (tokens.size() >= 2) ? tokens[1].toUInt() : 0;

        GetPlayerReadLock();
        bool match = m_playerContext.GetCardID() == cardnum;
        if (match && m_playerContext.m_recorder)
        {
            SetLastProgram(nullptr);
            m_jumpToProgram = true;
            SetExitPlayer(true, false);
        }
        ReturnPlayerLock();
    }

    if (message.startsWith("LIVETV_WATCH"))
    {
        int watch = 0;
        if (tokens.size() >= 3)
        {
            cardnum = tokens[1].toUInt();
            watch   = tokens[2].toInt();
        }

        GetPlayerWriteLock();
        if (m_playerContext.GetCardID() == cardnum)
        {
            if (watch)
            {
                ProgramInfo pi(me->ExtraDataList());
                if (pi.HasPathname() || pi.GetChanID())
                {
                    m_playerContext.SetPseudoLiveTV(&pi, kPseudoChangeChannel);
                    if (!m_pseudoChangeChanTimerId)
                        m_pseudoChangeChanTimerId = StartTimer(0, __LINE__);
                }
            }
            else
            {
                m_playerContext.SetPseudoLiveTV(nullptr, kPseudoNormalLiveTV);
            }
        }
        ReturnPlayerLock();
    }

    if (message.startsWith("LIVETV_CHAIN"))
    {
        QString id;
        if ((tokens.size() >= 2) && tokens[1] == "UPDATE")
            id = tokens[2];

        GetPlayerReadLock();
        if (m_playerContext.m_tvchain && m_playerContext.m_tvchain->GetID() == id)
            m_playerContext.UpdateTVChain(me->ExtraDataList());
        ReturnPlayerLock();
    }

    if (message.startsWith("EXIT_TO_MENU"))
    {
        GetPlayerReadLock();
        PrepareToExitPlayer(__LINE__);
        SetExitPlayer(true, true);
        if (m_playerContext.m_player)
            m_playerContext.m_player->DisableEdit(-1);
        ReturnPlayerLock();
    }

    if (message.startsWith("SIGNAL"))
    {
        cardnum = (tokens.size() >= 2) ? tokens[1].toUInt() : 0;
        const QStringList& signalList = me->ExtraDataList();

        GetPlayerReadLock();
        OSD *osd = GetOSDL();
        if (osd)
        {
            if (m_playerContext.m_recorder && (m_playerContext.GetCardID() == cardnum) && !signalList.empty())
            {
                UpdateOSDSignal(signalList);
                UpdateOSDTimeoutMessage();
            }
        }
        ReturnOSDLock();
        ReturnPlayerLock();
    }

    if (message.startsWith("NETWORK_CONTROL"))
    {
        if ((tokens.size() >= 2) &&
            (tokens[1] != "ANSWER") && (tokens[1] != "RESPONSE"))
        {
#if QT_VERSION < QT_VERSION_CHECK(5,14,0)
            QStringList tokens2 = message.split(" ", QString::SkipEmptyParts);
#else
            QStringList tokens2 = message.split(" ", Qt::SkipEmptyParts);
#endif
            if ((tokens2.size() >= 2) &&
                (tokens2[1] != "ANSWER") && (tokens2[1] != "RESPONSE"))
            {
                m_networkControlCommands.enqueue(message);
                if (!m_networkControlTimerId)
                    m_networkControlTimerId = StartTimer(1, __LINE__);
            }
        }
    }

    if (message.startsWith("START_EPG"))
    {
        int editType = tokens[1].toInt();
        DoEditSchedule(editType);
    }

    if (message.startsWith("COMMFLAG_START") && (tokens.size() >= 2))
    {
        uint evchanid = 0;
        QDateTime evrecstartts;
        ProgramInfo::ExtractKey(tokens[1], evchanid, evrecstartts);

        GetPlayerReadLock();
        m_playerContext.LockPlayingInfo(__FILE__, __LINE__);
        bool doit = ((m_playerContext.m_playingInfo) &&
                     (m_playerContext.m_playingInfo->GetChanID() == evchanid) &&
                     (m_playerContext.m_playingInfo->GetRecordingStartTime() == evrecstartts));
        m_playerContext.UnlockPlayingInfo(__FILE__, __LINE__);

        if (doit)
        {
            QString msg = "COMMFLAG_REQUEST ";
            msg += ProgramInfo::MakeUniqueKey(evchanid, evrecstartts);
            gCoreContext->SendMessage(msg);
        }
        ReturnPlayerLock();
    }

    if (message.startsWith("COMMFLAG_UPDATE") && (tokens.size() >= 3))
    {
        uint evchanid = 0;
        QDateTime evrecstartts;
        ProgramInfo::ExtractKey(tokens[1], evchanid, evrecstartts);

        GetPlayerReadLock();
        m_playerContext.LockPlayingInfo(__FILE__, __LINE__);
        bool doit = ((m_playerContext.m_playingInfo) &&
                     (m_playerContext.m_playingInfo->GetChanID() == evchanid) &&
                     (m_playerContext.m_playingInfo->GetRecordingStartTime() == evrecstartts));
        m_playerContext.UnlockPlayingInfo(__FILE__, __LINE__);

        if (doit)
        {
            frm_dir_map_t newMap;
            QStringList mark;
#if QT_VERSION < QT_VERSION_CHECK(5,14,0)
            QStringList marks =
                tokens[2].split(",", QString::SkipEmptyParts);
#else
            QStringList marks = tokens[2].split(",", Qt::SkipEmptyParts);
#endif
            for (int j = 0; j < marks.size(); j++)
            {
#if QT_VERSION < QT_VERSION_CHECK(5,14,0)
                mark = marks[j].split(":", QString::SkipEmptyParts);
#else
                mark = marks[j].split(":", Qt::SkipEmptyParts);
#endif
                if (marks.size() >= 2)
                    newMap[mark[0].toULongLong()] = static_cast<MarkTypes>(mark[1].toInt());
            }
            m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
            if (m_playerContext.m_player)
                m_playerContext.m_player->SetCommBreakMap(newMap);
            m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
        }
        ReturnPlayerLock();
    }

    if (message == "NOTIFICATION")
    {
        if (!GetNotificationCenter())
            return;
        MythNotification mn(*me);
        MythNotificationCenter::GetInstance()->Queue(mn);
    }
}

void TV::QuickRecord()
{
    BrowseInfo bi = GetBrowsedInfo();
    if (bi.m_chanId)
    {
        InfoMap infoMap;
        QDateTime startts = MythDate::fromString(bi.m_startTime);

        RecordingInfo::LoadStatus status = RecordingInfo::kNoProgram;
        RecordingInfo recinfo(bi.m_chanId, startts, false, 0, &status);
        if (RecordingInfo::kFoundProgram == status)
            recinfo.QuickRecord();
        recinfo.ToMap(infoMap);
        infoMap["iconpath"] = ChannelUtil::GetIcon(recinfo.GetChanID());
        if ((recinfo.IsVideoFile() || recinfo.IsVideoDVD() ||
            recinfo.IsVideoBD()) && recinfo.GetPathname() != recinfo.GetBasename())
        {
            infoMap["coverartpath"] = VideoMetaDataUtil::GetArtPath(
                recinfo.GetPathname(), "Coverart");
            infoMap["fanartpath"] = VideoMetaDataUtil::GetArtPath(
                recinfo.GetPathname(), "Fanart");
            infoMap["bannerpath"] = VideoMetaDataUtil::GetArtPath(
                recinfo.GetPathname(), "Banners");
            infoMap["screenshotpath"] = VideoMetaDataUtil::GetArtPath(
                recinfo.GetPathname(), "Screenshots");
        }

        OSD *osd = GetOSDL();
        if (osd)
        {
            osd->SetText("browse_info", infoMap, kOSDTimeout_Med);
            InfoMap map;
            map.insert("message_text", tr("Record"));
            osd->SetText("osd_message", map, kOSDTimeout_Med);
        }
        ReturnOSDLock();
        return;
    }

    m_playerContext.LockPlayingInfo(__FILE__, __LINE__);
    if (!m_playerContext.m_playingInfo)
    {
        LOG(VB_GENERAL, LOG_CRIT, LOC + "Unknown recording during live tv.");
        m_playerContext.UnlockPlayingInfo(__FILE__, __LINE__);
        return;
    }

    QString cmdmsg("");
    if (m_playerContext.m_playingInfo->QueryAutoExpire() == kLiveTVAutoExpire)
    {
        RecordingInfo recInfo(*m_playerContext.m_playingInfo);
        recInfo.SaveAutoExpire(static_cast<AutoExpireType>(m_dbAutoexpireDefault));
        recInfo.ApplyRecordRecGroupChange("Default");
        *m_playerContext.m_playingInfo = recInfo;

        cmdmsg = tr("Record");
        m_playerContext.SetPseudoLiveTV(m_playerContext.m_playingInfo, kPseudoRecording);
        m_playerContext.m_recorder->SetLiveRecording(true);
        LOG(VB_RECORD, LOG_INFO, LOC + "Toggling Record on");
    }
    else
    {
        RecordingInfo recInfo(*m_playerContext.m_playingInfo);
        recInfo.SaveAutoExpire(kLiveTVAutoExpire);
        recInfo.ApplyRecordRecGroupChange("LiveTV");
        *m_playerContext.m_playingInfo = recInfo;

        cmdmsg = tr("Cancel Record");
        m_playerContext.SetPseudoLiveTV(m_playerContext.m_playingInfo, kPseudoNormalLiveTV);
        m_playerContext.m_recorder->SetLiveRecording(false);
        LOG(VB_RECORD, LOG_INFO, LOC + "Toggling Record off");
    }

    QString msg = cmdmsg + " \"" + m_playerContext.m_playingInfo->GetTitle() + "\"";

    m_playerContext.UnlockPlayingInfo(__FILE__, __LINE__);

    SetOSDMessage(msg);
}

void TV::HandleOSDClosed(int OSDType)
{
    switch (OSDType)
    {
        case kOSDFunctionalType_PictureAdjust:
            m_adjustingPicture = kAdjustingPicture_None;
            m_adjustingPictureAttribute = kPictureAttribute_None;
            break;
        case kOSDFunctionalType_SmartForward:
            m_doSmartForward = false;
            break;
        case kOSDFunctionalType_TimeStretchAdjust:
            m_stretchAdjustment = false;
            break;
        case kOSDFunctionalType_AudioSyncAdjust:
            m_audiosyncAdjustment = false;
            {
            GetPlayerReadLock();
            m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
            if (m_playerContext.m_player)
            {
                int64_t aoff = m_playerContext.m_player->GetAudioTimecodeOffset();
                gCoreContext->SaveSetting("AudioSyncOffset", QString::number(aoff));
            }
            m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
            ReturnPlayerLock();
            }
            break;
        case kOSDFunctionalType_SubtitleZoomAdjust:
            m_subtitleZoomAdjustment = false;
            break;
        case kOSDFunctionalType_SubtitleDelayAdjust:
            m_subtitleDelayAdjustment = false;
            break;
        case kOSDFunctionalType_Default:
            break;
    }
}

PictureAttribute TV::NextPictureAdjustType(PictureAdjustType Type, MythPlayer *Player, PictureAttribute Attr)
{
    if (!Player)
        return kPictureAttribute_None;

    uint sup = kPictureAttributeSupported_None;
    if ((kAdjustingPicture_Playback == Type) && Player && Player->GetVideoOutput())
    {
        sup = Player->GetVideoOutput()->GetSupportedPictureAttributes();
        if (Player->HasAudioOut() && Player->PlayerControlsVolume())
            sup |= kPictureAttributeSupported_Volume;
    }
    else if ((kAdjustingPicture_Channel == Type) ||
             (kAdjustingPicture_Recording == Type))
    {
        sup = (kPictureAttributeSupported_Brightness |
               kPictureAttributeSupported_Contrast |
               kPictureAttributeSupported_Colour |
               kPictureAttributeSupported_Hue);
    }

    return ::next_picattr(static_cast<PictureAttributeSupported>(sup), Attr);
}

void TV::DoToggleNightMode()
{
    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    m_playerContext.m_player->ToggleNightMode();
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
}

void TV::DoTogglePictureAttribute(PictureAdjustType Type)
{
    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    PictureAttribute attr = NextPictureAdjustType(Type, m_playerContext.m_player,
                                                  m_adjustingPictureAttribute);
    if (kPictureAttribute_None == attr)
    {
        m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
        return;
    }

    m_adjustingPicture          = Type;
    m_adjustingPictureAttribute = attr;

    QString title = toTitleString(Type);

    int value = 99;
    if (kAdjustingPicture_Playback == Type)
    {
        if (!m_playerContext.m_player)
        {
            m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
            return;
        }
        if (kPictureAttribute_Volume != m_adjustingPictureAttribute)
        {
            value = m_playerContext.m_player->GetVideoOutput()->GetPictureAttribute(attr);
        }
        else if (m_playerContext.m_player->HasAudioOut())
        {
            value = static_cast<int>(m_playerContext.m_player->GetVolume());
            title = tr("Adjust Volume");
        }
    }
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);

    if (m_playerContext.m_recorder && (kAdjustingPicture_Playback != Type))
    {
        value = m_playerContext.m_recorder->GetPictureAttribute(attr);
    }

    QString text = toString(attr) + " " + toTypeString(Type);

    UpdateOSDStatus(title, text, QString::number(value),
                    kOSDFunctionalType_PictureAdjust, "%",
                    value * 10, kOSDTimeout_Med);
    SetUpdateOSDPosition(false);
}

void TV::DoChangePictureAttribute(PictureAdjustType Type, PictureAttribute Attr,
                                  bool Up, int NewValue)
{
    int value = 99;

    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (kAdjustingPicture_Playback == Type)
    {
        if (kPictureAttribute_Volume == Attr)
        {
            m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
            ChangeVolume(Up, NewValue);
            return;
        }
        if (!m_playerContext.m_player)
        {
            m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
            return;
        }

        if (m_playerContext.m_player->GetVideoOutput())
        {
            MythVideoOutput *vo = m_playerContext.m_player->GetVideoOutput();
            if ((NewValue >= 0) && (NewValue <= 100))
                value = vo->SetPictureAttribute(Attr, NewValue);
            else
                value = vo->ChangePictureAttribute(Attr, Up);
        }
    }
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);

    if (m_playerContext.m_recorder && (kAdjustingPicture_Playback != Type))
    {
        value = m_playerContext.m_recorder->ChangePictureAttribute(Type, Attr, Up);
    }

    QString text = toString(Attr) + " " + toTypeString(Type);

    UpdateOSDStatus(toTitleString(Type), text, QString::number(value),
                    kOSDFunctionalType_PictureAdjust, "%",
                    value * 10, kOSDTimeout_Med);
    SetUpdateOSDPosition(false);
}

void TV::ShowOSDCutpoint(const QString &Type)
{
    if (Type == "EDIT_CUT_POINTS")
    {
        if (!m_cutlistMenu.IsLoaded())
        {
            m_cutlistMenu.LoadFromFile("menu_cutlist.xml",
                                       tr("Edit Cut Points"),
                                       // XXX which translation context to use?
                                       metaObject()->className(),
                                       "TV Editing");
        }
        if (m_cutlistMenu.IsLoaded())
        {
            PlaybackMenuShow(m_cutlistMenu,
                             m_cutlistMenu.GetRoot(),
                             QDomNode());
        }
    }
    else if (Type == "EDIT_CUT_POINTS_COMPACT")
    {
        if (!m_cutlistCompactMenu.IsLoaded())
        {
            m_cutlistCompactMenu.LoadFromFile("menu_cutlist_compact.xml",
                                              tr("Edit Cut Points"),
                                              // XXX which translation context to use?
                                              metaObject()->className(),
                                              "TV Editing");
        }
        if (m_cutlistCompactMenu.IsLoaded())
        {
            PlaybackMenuShow(m_cutlistCompactMenu,
                             m_cutlistCompactMenu.GetRoot(),
                             QDomNode());
        }
    }
    else if (Type == "EXIT_EDIT_MODE")
    {
        OSD *osd = GetOSDL();
        if (!osd)
        {
            ReturnOSDLock();
            return;
        }
        osd->DialogShow(OSD_DLG_CUTPOINT,
                        tr("Exit Recording Editor"));
        osd->DialogAddButton(tr("Save Cuts and Exit"),
                             "DIALOG_CUTPOINT_SAVEEXIT_0");
        osd->DialogAddButton(tr("Exit Without Saving"),
                             "DIALOG_CUTPOINT_REVERTEXIT_0");
        osd->DialogAddButton(tr("Save Cuts"),
                             "DIALOG_CUTPOINT_SAVEMAP_0");
        osd->DialogAddButton(tr("Undo Changes"),
                             "DIALOG_CUTPOINT_REVERT_0");
        osd->DialogBack("", "DIALOG_CUTPOINT_DONOTHING_0", true);
        InfoMap map;
        map.insert("title", tr("Edit"));
        osd->SetText("osd_program_editor", map, kOSDTimeout_None);
        ReturnOSDLock();
    }
}

bool TV::HandleOSDCutpoint(const QString& Action)
{
    bool res = true;
    if (!DialogIsVisible(OSD_DLG_CUTPOINT))
        return res;

    OSD *osd = GetOSDL();
    if (Action == "DONOTHING" && osd)
    {
    }
    else if (osd)
    {
        QStringList actions(Action);
        if (!m_playerContext.m_player->HandleProgramEditorActions(actions))
            LOG(VB_GENERAL, LOG_ERR, LOC + "Unrecognised cutpoint action");
        else
            m_editMode = m_playerContext.m_player->GetEditMode();
    }
    ReturnOSDLock();
    return res;
}

/** \fn TV::StartProgramEditMode(PlayerContext *ctx)
 *  \brief Starts Program Cut Map Editing mode
 */
void TV::StartProgramEditMode()
{
    m_playerContext.LockPlayingInfo(__FILE__, __LINE__);
    bool isEditing = m_playerContext.m_playingInfo->QueryIsEditing();
    m_playerContext.UnlockPlayingInfo(__FILE__, __LINE__);

    if (isEditing)
    {
        ShowOSDAlreadyEditing();
        return;
    }

    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (m_playerContext.m_player)
        m_editMode = m_playerContext.m_player->EnableEdit();
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
}

void TV::ShowOSDAlreadyEditing()
{
    OSD *osd = GetOSDL();
    if (osd)
    {
        osd->DialogQuit();
        bool was_paused = ContextIsPaused(__FILE__, __LINE__);
        if (!was_paused)
            DoTogglePause(true);

        QString message = tr("This program is currently being edited");
        osd->DialogShow(OSD_DLG_EDITING, message);
        QString def = QString("DIALOG_EDITING_CONTINUE_%1").arg(static_cast<int>(was_paused));
        osd->DialogAddButton(tr("Continue Editing"), def, false, true);
        osd->DialogAddButton(tr("Do not edit"),
                             QString("DIALOG_EDITING_STOP_%1").arg(static_cast<int>(was_paused)));
        osd->DialogBack("", def, true);
    }
    ReturnOSDLock();
}

void TV::HandleOSDAlreadyEditing(const QString& Action, bool WasPaused)
{
    if (!DialogIsVisible(OSD_DLG_EDITING))
        return;

    bool paused = ContextIsPaused(__FILE__, __LINE__);

    if (Action == "STOP")
    {
        m_playerContext.LockPlayingInfo(__FILE__, __LINE__);
        if (m_playerContext.m_playingInfo)
            m_playerContext.m_playingInfo->SaveEditing(false);
        m_playerContext.UnlockPlayingInfo(__FILE__, __LINE__);
        if (!WasPaused && paused)
            DoTogglePause(true);
    }
    else // action == "CONTINUE"
    {
        m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
        if (m_playerContext.m_player)
        {
            m_playerContext.m_playingInfo->SaveEditing(false);
            m_editMode = m_playerContext.m_player->EnableEdit();
            if (!m_editMode && !WasPaused && paused)
                DoTogglePause(false);
        }
        m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
    }

}

static void insert_map(InfoMap &infoMap, const InfoMap &newMap)
{
    for (auto it = newMap.cbegin(); it != newMap.cend(); ++it)
        infoMap.insert(it.key(), *it);
}

/** \fn TV::StartChannelEditMode(PlayerContext*)
 *  \brief Starts channel editing mode.
 */
void TV::StartChannelEditMode()
{
    OSD *osd = GetOSDL();
    if (!m_playerContext.m_recorder || !osd)
    {
        ReturnOSDLock();
        return;
    }
    ReturnOSDLock();

    QMutexLocker locker(&m_chanEditMapLock);

    // Get the info available from the backend
    m_chanEditMap.clear();
    m_playerContext.m_recorder->GetChannelInfo(m_chanEditMap);

    // Update with XDS Info
    ChannelEditAutoFill(m_chanEditMap);

    // Set proper initial values for channel editor, and make it visible..
    osd = GetOSDL();
    if (osd)
    {
        osd->DialogQuit();
        osd->DialogShow(OSD_DLG_EDITOR);
        osd->SetText(OSD_DLG_EDITOR, m_chanEditMap, kOSDTimeout_None);
    }
    ReturnOSDLock();
}

void TV::StartOsdNavigation()
{
    OSD *osd = GetOSDL();
    if (osd)
    {
        osd->DialogQuit();
        osd->HideAll();
        ToggleOSD(true);
        osd->DialogShow(OSD_DLG_NAVIGATE);
    }
    ReturnOSDLock();
    UpdateNavDialog();
}

/**
 *  \brief Processes channel editing key.
 */
bool TV::HandleOSDChannelEdit(const QString& Action)
{
    QMutexLocker locker(&m_chanEditMapLock);
    bool hide = false;

    if (!DialogIsVisible(OSD_DLG_EDITOR))
        return hide;

    OSD *osd = GetOSDL();
    if (osd && Action == "PROBE")
    {
        InfoMap infoMap;
        osd->DialogGetText(infoMap);
        ChannelEditAutoFill(infoMap);
        insert_map(m_chanEditMap, infoMap);
        osd->SetText(OSD_DLG_EDITOR, m_chanEditMap, kOSDTimeout_None);
    }
    else if (osd && Action == "OK")
    {
        InfoMap infoMap;
        osd->DialogGetText(infoMap);
        insert_map(m_chanEditMap, infoMap);
        m_playerContext.m_recorder->SetChannelInfo(m_chanEditMap);
        hide = true;
    }
    else if (osd && Action == "QUIT")
    {
        hide = true;
    }
    ReturnOSDLock();
    return hide;
}

/** \fn TV::ChannelEditAutoFill(const PlayerContext*,InfoMap&) const
 *  \brief Automatically fills in as much information as possible.
 */
void TV::ChannelEditAutoFill(InfoMap &Info)
{
#if 0
    const QString keys[4] = { "XMLTV", "callsign", "channame", "channum", };
#endif

    // fill in uninitialized and unchanged fields from XDS
    ChannelEditXDSFill(Info);
}

void TV::ChannelEditXDSFill(InfoMap &Info)
{
    QMap<QString,bool> modifiable;
    if (!(modifiable["callsign"] = Info["callsign"].isEmpty()))
    {
        QString unsetsign = tr("UNKNOWN%1", "Synthesized callsign");
        int unsetcmpl = unsetsign.length() - 2;
        unsetsign = unsetsign.left(unsetcmpl);
        if (Info["callsign"].left(unsetcmpl) == unsetsign) // was unsetcmpl????
            modifiable["callsign"] = true;
    }
    modifiable["channame"] = Info["channame"].isEmpty();

    const std::array<const QString,2> xds_keys { "callsign", "channame", };
    for (const auto & key : xds_keys)
    {
        if (!modifiable[key])
            continue;

        m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
        QString tmp = m_playerContext.m_player->GetXDS(key).toUpper();
        m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);

        if (tmp.isEmpty())
            continue;

        if ((key == "callsign") &&
            ((tmp.length() > 5) || (tmp.indexOf(" ") >= 0)))
        {
            continue;
        }

        Info[key] = tmp;
    }
}

void TV::OSDDialogEvent(int Result, const QString& Text, QString Action)
{
    GetPlayerReadLock();
    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("result %1 text %2 action %3")
        .arg(Result).arg(Text).arg(Action));

    bool hide = true;
    if (Result == 100)
        hide = false;

    bool handled = true;
    if (Action.startsWith("DIALOG_"))
    {
        Action.remove("DIALOG_");
        QStringList desc = Action.split("_");
        bool valid = desc.size() == 3;
        if (valid && desc[0] == ACTION_JUMPREC)
        {
            FillOSDMenuJumpRec(desc[1], desc[2].toInt(), Text);
            hide = false;
        }
        else if (valid && desc[0] == "VIDEOEXIT")
        {
            hide = HandleOSDVideoExit(desc[1]);
        }
        else if (valid && desc[0] == "SLEEP")
        {
            HandleOSDSleep(desc[1]);
        }
        else if (valid && desc[0] == "IDLE")
        {
            HandleOSDIdle(desc[1]);
        }
        else if (valid && desc[0] == "INFO")
        {
            HandleOSDInfo(desc[1]);
        }
        else if (valid && desc[0] == "EDITING")
        {
            HandleOSDAlreadyEditing(desc[1], desc[2].toInt() != 0);
        }
        else if (valid && desc[0] == "ASKALLOW")
        {
            HandleOSDAskAllow(desc[1]);
        }
        else if (valid && desc[0] == "EDITOR")
        {
            hide = HandleOSDChannelEdit(desc[1]);
        }
        else if (valid && desc[0] == "CUTPOINT")
        {
            hide = HandleOSDCutpoint(desc[1]);
        }
        else if ((valid && desc[0] == "DELETE") ||
                 (valid && desc[0] == "CONFIRM"))
        {
        }
        else if (valid && desc[0] == ACTION_PLAY)
        {
            DoPlay();
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, "Unrecognised dialog event.");
        }
    }
    else if (Result < 0)
        ; // exit dialog // NOLINT(bugprone-branch-clone)
    else if (HandleTrackAction(Action))
        ;
    else if (Action == ACTION_PAUSE)
        DoTogglePause(true);
    else if (Action == ACTION_STOP)
    {
        PrepareToExitPlayer(__LINE__);
        SetExitPlayer(true, true);
    }
    else if (Action == "CANCELPLAYLIST")
    {
        SetInPlayList(false);
        MythEvent xe("CANCEL_PLAYLIST");
        gCoreContext->dispatch(xe);
    }
    else if (Action == ACTION_JUMPFFWD)
        DoJumpFFWD();
    else if (Action == ACTION_JUMPRWND)
        DoJumpRWND();
    else if (Action == ACTION_SEEKFFWD)
        DoSeekFFWD();
    else if (Action == ACTION_SEEKRWND)
        DoSeekRWND();
    else if (Action == ACTION_TOGGLEOSDDEBUG)
        ToggleOSDDebug();
    else if (Action == "TOGGLEMANUALZOOM")
        SetManualZoom(true, tr("Zoom Mode ON"));
    else if (Action == ACTION_BOTTOMLINEMOVE)
        ToggleMoveBottomLine();
    else if (Action == ACTION_BOTTOMLINESAVE)
        SaveBottomLine();
    else if (Action == "TOGGLESTRETCH")
        ToggleTimeStretch();
    else if (Action == ACTION_ENABLEUPMIX)
        EnableUpmix(true);
    else if (Action == ACTION_DISABLEUPMIX)
        EnableUpmix(false);
    else if (Action.startsWith("ADJUSTSTRETCH"))
    {
        bool floatRead = false;
        float stretch = Action.rightRef(Action.length() - 13).toFloat(&floatRead);
        if (floatRead &&
            stretch <= 2.0F &&
            stretch >= 0.48F)
        {
            m_playerContext.m_tsNormal = stretch;   // alter speed before display
        }

        StopFFRew();

        if (ContextIsPaused(__FILE__, __LINE__))
            DoTogglePause(true);

        ChangeTimeStretch(0, !floatRead);   // just display
    }
    else if (Action.startsWith("SELECTSCAN_"))
        OverrideScan(static_cast<FrameScanType>(Action.rightRef(1).toInt()));
    else if (Action.startsWith(ACTION_TOGGELAUDIOSYNC))
        ChangeAudioSync(0);
    else if (Action == ACTION_TOGGLESUBTITLEZOOM)
        ChangeSubtitleZoom(0);
    else if (Action == ACTION_TOGGLESUBTITLEDELAY)
        ChangeSubtitleDelay(0);
    else if (Action == ACTION_TOGGLEVISUALISATION)
        EnableVisualisation(false, true /*toggle*/);
    else if (Action == ACTION_ENABLEVISUALISATION)
        EnableVisualisation(true);
    else if (Action == ACTION_DISABLEVISUALISATION)
        EnableVisualisation(false);
    else if (Action.startsWith(ACTION_TOGGLESLEEP))
        ToggleSleepTimer(Action.left(13));
    else if (Action.startsWith("TOGGLEPICCONTROLS"))
    {
        m_adjustingPictureAttribute = static_cast<PictureAttribute>(Action.rightRef(1).toInt() - 1);
        DoTogglePictureAttribute(kAdjustingPicture_Playback);
    }
    else if (Action == ACTION_TOGGLENIGHTMODE)
        DoToggleNightMode();
    else if (Action == "TOGGLEASPECT")
        ToggleAspectOverride();
    else if (Action.startsWith("TOGGLEASPECT"))
        ToggleAspectOverride(static_cast<AspectOverrideMode>(Action.rightRef(1).toInt()));
    else if (Action == "TOGGLEFILL")
        ToggleAdjustFill();
    else if (Action.startsWith("TOGGLEFILL"))
        ToggleAdjustFill(static_cast<AdjustFillMode>(Action.rightRef(1).toInt()));
    else if (Action == "MENU")
         ShowOSDMenu();
    else if (Action == "AUTODETECT_FILL")
    {
        m_playerContext.m_player->m_detectLetterBox->SetDetectLetterbox(!m_playerContext.m_player->m_detectLetterBox->GetDetectLetterbox());
    }
    else if (Action == ACTION_GUIDE)
        EditSchedule(kScheduleProgramGuide);
    else if (Action.startsWith("CHANGROUP_") && m_dbUseChannelGroups)
    {
        if (Action == "CHANGROUP_ALL_CHANNELS")
        {
            UpdateChannelList(-1);
        }
        else
        {
            Action.remove("CHANGROUP_");

            UpdateChannelList(Action.toInt());

            // make sure the current channel is from the selected group
            // or tune to the first in the group
            QString cur_channum;
            QString new_channum;
            if (m_playerContext.m_tvchain)
            {
                QMutexLocker locker(&m_channelGroupLock);
                const ChannelInfoList &list = m_channelGroupChannelList;
                cur_channum = m_playerContext.m_tvchain->GetChannelName(-1);
                new_channum = cur_channum;

                auto it = list.cbegin();
                for (; it != list.cend(); ++it)
                {
                    if ((*it).m_chanNum == cur_channum)
                    {
                        break;
                    }
                }

                if (it == list.end())
                {
                    // current channel not found so switch to the
                    // first channel in the group
                    it = list.begin();
                    if (it != list.end())
                        new_channum = (*it).m_chanNum;
                }

                LOG(VB_CHANNEL, LOG_INFO, LOC +
                    QString("Channel Group: '%1'->'%2'")
                        .arg(cur_channum).arg(new_channum));
            }

            if (m_playerContext.m_tvchain)
            {
                // Only change channel if new channel != current channel
                if (cur_channum != new_channum && !new_channum.isEmpty())
                {
                    m_queuedInput   = new_channum;
                    m_queuedChanNum = new_channum;
                    m_queuedChanID  = 0;
                    if (!m_queueInputTimerId)
                        m_queueInputTimerId = StartTimer(10, __LINE__);
                }

                // Turn off OSD Channel Num so the channel
                // changes right away
                HideOSDWindow("osd_input");
            }
        }
    }
    else if (Action == ACTION_FINDER)
        EditSchedule(kScheduleProgramFinder);
    else if (Action == "SCHEDULE")
        EditSchedule(kScheduledRecording);
    else if (Action == ACTION_VIEWSCHEDULED)
        EditSchedule(kViewSchedule);
    else if (Action.startsWith("VISUALISER"))
        EnableVisualisation(true, false, Action);
    else if (Action.startsWith("3D"))
        Handle3D(Action);
    else if (HandleJumpToProgramAction(QStringList(Action)))
    {
    }
    else if (StateIsLiveTV(GetState()))
    {
        if (Action == "TOGGLEBROWSE")
            BrowseStart();
        else if (Action == "PREVCHAN")
            PopPreviousChannel(true);
        else if (Action.startsWith("SWITCHTOINPUT_"))
        {
            m_switchToInputId = Action.midRef(14).toUInt();
            ScheduleInputChange();
        }
        else if (Action == "EDIT")
        {
            StartChannelEditMode();
            hide = false;
        }
        else
            handled = false;
    }
    else
        handled = false;
    if (!handled && StateIsPlaying(m_playerContext.GetState()))
    {
        handled = true;
        if (Action == ACTION_JUMPTODVDROOTMENU ||
            Action == ACTION_JUMPTODVDCHAPTERMENU ||
            Action == ACTION_JUMPTOPOPUPMENU ||
            Action == ACTION_JUMPTODVDTITLEMENU)
        {
            QString menu = "root";
            if (Action == ACTION_JUMPTODVDCHAPTERMENU)
                menu = "chapter";
            else if (Action == ACTION_JUMPTODVDTITLEMENU)
                menu = "title";
            else if (Action == ACTION_JUMPTOPOPUPMENU)
                menu = "popup";
            m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
            if (m_playerContext.m_player)
                m_playerContext.m_player->GoToMenu(menu);
            m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
        }
        else if (Action.startsWith(ACTION_JUMPCHAPTER))
        {
            int chapter = Action.rightRef(3).toInt();
            DoJumpChapter(chapter);
        }
        else if (Action.startsWith(ACTION_SWITCHTITLE))
        {
            int title = Action.rightRef(3).toInt();
            DoSwitchTitle(title);
        }
        else if (Action.startsWith(ACTION_SWITCHANGLE))
        {
            int angle = Action.rightRef(3).toInt();
            DoSwitchAngle(angle);
        }
        else if (Action == "EDIT")
        {
            StartProgramEditMode();
            hide = false;
        }
        else if (Action == "TOGGLEAUTOEXPIRE")
            ToggleAutoExpire();
        else if (Action.startsWith("TOGGLECOMMSKIP"))
        {
            SetAutoCommercialSkip(static_cast<CommSkipMode>(Action.rightRef(1).toInt()));
        }
        else if (Action == "QUEUETRANSCODE")
        {
            DoQueueTranscode("Default");
        }
        else if (Action == "QUEUETRANSCODE_AUTO")
        {
            DoQueueTranscode("Autodetect");
        }
        else if (Action == "QUEUETRANSCODE_HIGH")
        {
            DoQueueTranscode("High Quality");
        }
        else if (Action == "QUEUETRANSCODE_MEDIUM")
        {
            DoQueueTranscode("Medium Quality");
        }
        else if (Action == "QUEUETRANSCODE_LOW")
        {
            DoQueueTranscode("Low Quality");
        }
        else
        {
            handled = false;
        }
    }

    if (!handled)
    {
        bool isDVD = m_playerContext.m_buffer && m_playerContext.m_buffer->IsDVD();
        bool isMenuOrStill = m_playerContext.m_buffer && m_playerContext.m_buffer->IsInDiscMenuOrStillFrame();
        handled = ActiveHandleAction(QStringList(Action), isDVD, isMenuOrStill);
    }

    if (!handled)
        handled = ActivePostQHandleAction(QStringList(Action));

    if (!handled)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Unknown menu action selected: " + Action);
        hide = false;
    }

    if (hide)
    {
        OSD *osd = GetOSDL();
        if (osd)
            osd->DialogQuit();
        ReturnOSDLock();
    }

    ReturnPlayerLock();
}

bool TV::DialogIsVisible(const QString &Dialog)
{
    bool visible = false;
    OSD *osd = GetOSDL();
    if (osd)
        visible = osd->DialogVisible(Dialog);
    ReturnOSDLock();
    return visible;
}

void TV::HandleOSDInfo(const QString& Action)
{
    if (!DialogIsVisible(OSD_DLG_INFO))
        return;

    if (Action == "CHANNELLOCK")
        m_lockTimerOn = false;
}

bool MenuBase::LoadFromFile(const QString &filename,
                            const QString &menuname,
                            const char *translationContext,
                            const QString &keyBindingContext)
{
    return LoadFileHelper(filename, menuname, translationContext,
                          keyBindingContext, 0);
}

bool MenuBase::LoadFromString(const QString &text,
                              const QString &menuname,
                              const char *translationContext,
                              const QString &keyBindingContext)
{
    return LoadStringHelper(text, menuname, translationContext,
                            keyBindingContext, 0);
}

bool MenuBase::LoadFileHelper(const QString &filename,
                              const QString &menuname,
                              const char *translationContext,
                              const QString &keyBindingContext,
                              int includeLevel)
{
    bool result = false;

    m_translationContext = translationContext;
    m_keyBindingContext = keyBindingContext;
    QStringList searchpath = GetMythUI()->GetThemeSearchPath();
    searchpath.prepend(GetConfDir() + '/');
    for (auto it = searchpath.cbegin(); !result && it != searchpath.cend(); ++it)
    {
        QString themefile = *it + filename;
        LOG(VB_PLAYBACK, LOG_INFO,
            LOC + QString("Loading menu %1").arg(themefile));
        QFile file(themefile);
        if (file.open(QIODevice::ReadOnly))
        {
            m_document = new QDomDocument();
            if (m_document->setContent(&file))
            {
                result = true;
                QDomElement root = GetRoot();
                m_menuName = Translate(root.attribute("text", menuname));
                ProcessIncludes(root, includeLevel);
            }
            else
            {
                delete m_document;
                m_document = nullptr;
            }
            file.close();
        }
        if (!result)
        {
            LOG(VB_FILE, LOG_ERR, LOC + "No theme file " + themefile);
        }
    }

    return result;
}

bool MenuBase::LoadStringHelper(const QString &text,
                                const QString &menuname,
                                const char *translationContext,
                                const QString &keyBindingContext,
                                int includeLevel)
{
    bool result = false;

    m_translationContext = translationContext;
    m_keyBindingContext = keyBindingContext;
    m_document = new QDomDocument();
    if (m_document->setContent(text))
    {
        result = true;
        QDomElement root = GetRoot();
        m_menuName = Translate(root.attribute("text", menuname));
        ProcessIncludes(root, includeLevel);
    }
    else
    {
        delete m_document;
        m_document = nullptr;
    }
    return result;
}

void MenuBase::ProcessIncludes(QDomElement &root, int includeLevel)
{
    const int maxInclude = 10;
    for (QDomNode n = root.firstChild(); !n.isNull(); n = n.nextSibling())
    {
        if (n.isElement())
        {
            QDomElement e = n.toElement();
            if (e.tagName() == "include")
            {
                QString include = e.attribute("file", "");
                if (include.isEmpty())
                    continue;
                if (includeLevel >= maxInclude)
                {
                    LOG(VB_GENERAL, LOG_ERR,
                        QString("Maximum include depth (%1) "
                                "exceeded for %2")
                        .arg(maxInclude).arg(include));
                    return;
                }
                MenuBase menu;
                if (menu.LoadFileHelper(include,
                                        include, // fallback menu name
                                                 // is filename
                                        m_translationContext,
                                        m_keyBindingContext,
                                        includeLevel + 1))
                {
                    QDomNode newChild = menu.GetRoot();
                    newChild = m_document->importNode(newChild, true);
                    root.replaceChild(newChild, n);
                    n = newChild;
                }
            }
            else if (e.tagName() == "menu")
            {
                ProcessIncludes(e, includeLevel + 1);
            }
        }
    }
}

MenuBase::~MenuBase()
{
    if (m_document)
    {
        delete m_document;
        m_document = nullptr;
    }
}

QDomElement MenuBase::GetRoot() const
{
    return m_document->documentElement();
}

QString MenuBase::Translate(const QString &text) const
{
    return QCoreApplication::translate(m_translationContext, text.toUtf8(), nullptr);
}

bool MenuBase::Show(const QDomNode &node,
                    const QDomNode &selected,
                    MenuItemDisplayer &displayer,
                    bool doDisplay) const
{
    bool hasSelected = false;
    bool displayed = false;
    for (QDomNode n = node.firstChild(); !n.isNull(); n = n.nextSibling())
    {
        if (n == selected)
            hasSelected = true;
    }
    for (QDomNode n = node.firstChild(); !n.isNull(); n = n.nextSibling())
    {
        if (n.isElement())
        {
            QDomElement e = n.toElement();
            QString text  = Translate(e.attribute("text", ""));
            QString show = e.attribute("show", "");
            MenuShowContext showContext =
                (show == "active" ? kMenuShowActive :
                 show == "inactive" ? kMenuShowInactive : kMenuShowAlways);
            QString current = e.attribute("current", "");
            MenuCurrentContext currentContext = kMenuCurrentDefault;
            if ((current == "active") && !hasSelected)
                currentContext = kMenuCurrentActive;
            else if (((current.startsWith("y") ||
                       current.startsWith("t") ||
                       current == "1")) && !hasSelected)
                currentContext = kMenuCurrentAlways;
            if (e.tagName() == "menu")
            {
                if (hasSelected && n == selected)
                    currentContext = kMenuCurrentAlways;
                MenuItemContext c(*this, n, text,
                                  currentContext,
                                  doDisplay);
                displayed |= displayer.MenuItemDisplay(c);
            }
            else if (e.tagName() == "item")
            {
                QString action = e.attribute("action", "");
                MenuItemContext c(*this, n, showContext, currentContext,
                                  action, text, doDisplay);
                displayed |= displayer.MenuItemDisplay(c);
            }
            else if (e.tagName() == "itemlist")
            {
                QString actiongroup = e.attribute("actiongroup", "");
                MenuItemContext c(*this, n, showContext, currentContext,
                                  actiongroup, doDisplay);
                displayed |= displayer.MenuItemDisplay(c);
            }
        }
        if (!doDisplay && displayed)
            break; // early exit optimization
    }
    return displayed;
}

static bool matchesGroup(const QString &name, const QString &inPrefix,
                         MenuCategory category, QString &outPrefix)
{
    outPrefix = name;
    return ((category == kMenuCategoryItem && name.startsWith(inPrefix)) ||
            (category == kMenuCategoryItemlist && name == inPrefix));
}

static void addButton(const MenuItemContext &c, OSD *osd, bool active,
                      bool &result, const QString &action,
                      const QString &defaultTextActive,
                      const QString &defaultTextInactive,
                      bool isMenu,
                      const QString &textArg)
{
    if (c.m_category == kMenuCategoryItemlist || action == c.m_action)
    {
        if ((c.m_showContext != kMenuShowInactive && active) ||
            (c.m_showContext != kMenuShowActive && !active))
        {
            result = true;
            if (c.m_doDisplay)
            {
                QString text = c.m_actionText;
                if (text.isEmpty())
                    text = (active || defaultTextInactive.isEmpty()) ?
                        defaultTextActive : defaultTextInactive;
                if (!textArg.isEmpty())
                    text = text.arg(textArg);
                bool current = false;
                if (c.m_currentContext == kMenuCurrentActive)
                    current = active;
                else if (c.m_currentContext == kMenuCurrentAlways)
                    current = true;
                osd->DialogAddButton(text, action, isMenu, current);
            }
        }
    }
}

#define BUTTON(action, text) \
    addButton(Context, osd, active, result, (action), (text), "", false, "")
#define BUTTON2(action, textActive, textInactive) \
    addButton(Context, osd, active, result, (action), (textActive), \
              (textInactive), false, "")
#define BUTTON3(action, textActive, textInactive, isMenu)     \
    addButton(Context, osd, active, result, (action), (textActive), \
              (textInactive), (isMenu), "")

bool TV::MenuItemDisplay(const MenuItemContext &Context)
{
    if (&Context.m_menu == &m_playbackMenu || &Context.m_menu == &m_playbackCompactMenu)
        return MenuItemDisplayPlayback(Context);
    if (&Context.m_menu == &m_cutlistMenu || &Context.m_menu == &m_cutlistCompactMenu)
        return MenuItemDisplayCutlist(Context);
    return false;
}

bool TV::MenuItemDisplayCutlist(const MenuItemContext &Context)
{
    MenuCategory category = Context.m_category;
    const QString &actionName = Context.m_action;

    bool result = false;
    OSD *osd = m_tvmOsd;
    if (!osd)
        return result;
    if (category == kMenuCategoryMenu)
    {
        result = Context.m_menu.Show(Context.m_node, QDomNode(), *this, false);
        if (result && Context.m_doDisplay)
        {
            QVariant v;
            v.setValue(MenuNodeTuple(Context.m_menu, Context.m_node));
            osd->DialogAddButton(Context.m_menuName, v, true,
                                 Context.m_currentContext != kMenuCurrentDefault);
        }
        return result;
    }
    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    uint64_t frame   = m_playerContext.m_player->GetFramesPlayed();
    uint64_t previous_cut = m_playerContext.m_player->GetNearestMark(frame, false);
    uint64_t next_cut = m_playerContext.m_player->GetNearestMark(frame, true);
    uint64_t total_frames = m_playerContext.m_player->GetTotalFrameCount();
    bool is_in_delete = m_playerContext.m_player->IsInDelete(frame);
    bool is_temporary_mark = m_playerContext.m_player->IsTemporaryMark(frame);
    if (category == kMenuCategoryItem)
    {
        bool active = true;
        if (actionName == "DIALOG_CUTPOINT_MOVEPREV_0")
        {
            if ((is_in_delete && is_temporary_mark &&
                 previous_cut > 0) ||
                (is_in_delete && !is_temporary_mark) ||
                (!is_temporary_mark && previous_cut > 0))
            {
                active = !(is_in_delete && !is_temporary_mark);
                BUTTON2(actionName, tr("Move Previous Cut End Here"),
                        tr("Move Start of Cut Here"));
            }
        }
        else if (actionName == "DIALOG_CUTPOINT_MOVENEXT_0")
        {
            if ((is_in_delete && is_temporary_mark &&
                 next_cut != total_frames) ||
                (is_in_delete && !is_temporary_mark) ||
                (!is_temporary_mark && next_cut != total_frames))
            {
                active = !(is_in_delete && !is_temporary_mark);
                BUTTON2(actionName, tr("Move Next Cut Start Here"),
                        tr("Move End of Cut Here"));
            }
        }
        else if (actionName == "DIALOG_CUTPOINT_CUTTOBEGINNING_0")
        {
            if (previous_cut == 0 &&
                (is_temporary_mark || !is_in_delete))
            {
                BUTTON(actionName, tr("Cut to Beginning"));
            }
        }
        else if (actionName == "DIALOG_CUTPOINT_CUTTOEND_0")
        {
            if (next_cut == total_frames &&
                (is_temporary_mark || !is_in_delete))
            {
                BUTTON(actionName, tr("Cut to End"));
            }
        }
        else if (actionName == "DIALOG_CUTPOINT_DELETE_0")
        {
            active = is_in_delete;
            BUTTON2(actionName, tr("Delete This Cut"),
                    tr("Join Surrounding Cuts"));
        }
        else if (actionName == "DIALOG_CUTPOINT_NEWCUT_0")
        {
            if (!is_in_delete)
                BUTTON(actionName, tr("Add New Cut"));
        }
        else if (actionName == "DIALOG_CUTPOINT_UNDO_0")
        {
            active = m_playerContext.m_player->DeleteMapHasUndo();
            //: %1 is the undo message
            QString text = tr("Undo - %1");
            addButton(Context, osd, active, result, actionName, text, "", false,
                      m_playerContext.m_player->DeleteMapGetUndoMessage());
        }
        else if (actionName == "DIALOG_CUTPOINT_REDO_0")
        {
            active = m_playerContext.m_player->DeleteMapHasRedo();
            //: %1 is the redo message
            QString text = tr("Redo - %1");
            addButton(Context, osd, active, result, actionName, text, "", false,
                      m_playerContext.m_player->DeleteMapGetRedoMessage());
        }
        else if (actionName == "DIALOG_CUTPOINT_CLEARMAP_0")
        {
            BUTTON(actionName, tr("Clear Cuts"));
        }
        else if (actionName == "DIALOG_CUTPOINT_INVERTMAP_0")
        {
            BUTTON(actionName, tr("Reverse Cuts"));
        }
        else if (actionName == "DIALOG_CUTPOINT_LOADCOMMSKIP_0")
        {
            BUTTON(actionName, tr("Load Detected Commercials"));
        }
        else if (actionName == "DIALOG_CUTPOINT_REVERT_0")
        {
            BUTTON(actionName, tr("Undo Changes"));
        }
        else if (actionName == "DIALOG_CUTPOINT_REVERTEXIT_0")
        {
            BUTTON(actionName, tr("Exit Without Saving"));
        }
        else if (actionName == "DIALOG_CUTPOINT_SAVEMAP_0")
        {
            BUTTON(actionName, tr("Save Cuts"));
        }
        else if (actionName == "DIALOG_CUTPOINT_SAVEEXIT_0")
        {
            BUTTON(actionName, tr("Save Cuts and Exit"));
        }
        else
        {
            // Allow an arbitrary action if it has a translated
            // description available to be used as the button text.
            // Look in the specified keybinding context as well as the
            // Global context.
            // XXX This doesn't work well (yet) because a keybinding
            // action named "foo" is actually a menu action named
            // "DIALOG_CUTPOINT_foo_0".
            QString text = m_mainWindow->GetActionText(Context.m_menu.GetKeyBindingContext(), actionName);
            if (text.isEmpty())
                text = m_mainWindow->GetActionText("Global", actionName);
            if (!text.isEmpty())
                BUTTON(actionName, text);
        }
    }
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
    return result;
}

// Returns true if at least one item should be displayed.
bool TV::MenuItemDisplayPlayback(const MenuItemContext &Context)
{
    MenuCategory category = Context.m_category;
    const QString &actionName = Context.m_action;

    bool result = false;
    bool active = true;
    OSD *osd = m_tvmOsd;
    if (!osd)
        return result;
    if (category == kMenuCategoryMenu)
    {
        result = Context.m_menu.Show(Context.m_node, QDomNode(), *this, false);
        if (result && Context.m_doDisplay)
        {
            QVariant v;
            v.setValue(MenuNodeTuple(Context.m_menu, Context.m_node));
            osd->DialogAddButton(Context.m_menuName, v, true,
                                 Context.m_currentContext != kMenuCurrentDefault);
        }
        return result;
    }
    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    QString prefix;
    if (matchesGroup(actionName, "VISUALISER_", category, prefix))
    {
        for (int i = 0; i < m_tvmVisualisers.size(); i++)
        {
            QString action = prefix + m_tvmVisualisers[i];
            active = (m_tvmActive == m_tvmVisualisers[i]);
            BUTTON(action, m_tvmVisualisers[i]);
        }
    }
    else if (matchesGroup(actionName, "TOGGLEASPECT", category, prefix))
    {
        for (int j = kAspect_Off; j < kAspect_END; j++)
        {
            // swap 14:9 and 16:9
            int i = ((kAspect_14_9 == j) ? kAspect_16_9 :
                     ((kAspect_16_9 == j) ? kAspect_14_9 : j));
            QString action = prefix + QString::number(i);
            active = (m_tvmAspectOverride == i);
            BUTTON(action, toString(static_cast<AspectOverrideMode>(i)));
        }
    }
    else if (matchesGroup(actionName, "TOGGLEFILL", category, prefix))
    {
        for (int i = kAdjustFill_Off; i < kAdjustFill_END; i++)
        {
            QString action = prefix + QString::number(i);
            active = (m_tvmAdjustFill == i);
            BUTTON(action, toString(static_cast<AdjustFillMode>(i)));
        }
    }
    else if (matchesGroup(actionName, "TOGGLEPICCONTROLS", category, prefix))
    {
        for (int i = kPictureAttribute_MIN; i < kPictureAttribute_MAX; i++)
        {
            if (toMask(static_cast<PictureAttribute>(i)) & m_tvmSup)
            {
                QString action = prefix + QString::number(i - kPictureAttribute_MIN);
                if (static_cast<PictureAttribute>(i) != kPictureAttribute_Range)
                    BUTTON(action, toString(static_cast<PictureAttribute>(i)));
            }
        }
    }
    else if (matchesGroup(actionName, "3D", category, prefix))
    {
        if (m_tvmStereoAllowed)
        {
            active = (m_tvmStereoMode == kStereoscopicModeAuto);
            BUTTON(ACTION_3DNONE, tr("Auto"));
            active = (m_tvmStereoMode == kStereoscopicModeIgnore3D);
            BUTTON(ACTION_3DIGNORE, tr("Ignore"));
            active = (m_tvmStereoMode == kStereoscopicModeSideBySideDiscard);
            BUTTON(ACTION_3DSIDEBYSIDEDISCARD, tr("Discard Side by Side"));
            active = (m_tvmStereoMode == kStereoscopicModeTopAndBottomDiscard);
            BUTTON(ACTION_3DTOPANDBOTTOMDISCARD, tr("Discard Top and Bottom"));
        }
    }
    else if (matchesGroup(actionName, "SELECTSCAN_", category, prefix) && m_playerContext.m_player)
    {
        FrameScanType scan = m_playerContext.m_player->GetScanType();
        active = (scan == kScan_Detect);
        BUTTON("SELECTSCAN_0", ScanTypeToUserString(kScan_Detect));
        active = (scan == kScan_Progressive);
        BUTTON("SELECTSCAN_3", ScanTypeToUserString(kScan_Progressive));
        active = (scan == kScan_Interlaced);
        BUTTON("SELECTSCAN_1", ScanTypeToUserString(kScan_Interlaced));
        active = (scan == kScan_Intr2ndField);
        BUTTON("SELECTSCAN_2", ScanTypeToUserString(kScan_Intr2ndField));
    }
    else if (matchesGroup(actionName, "SELECTSUBTITLE_", category, prefix) ||
             matchesGroup(actionName, "SELECTRAWTEXT_",  category, prefix) ||
             matchesGroup(actionName, "SELECTCC708_",    category, prefix) ||
             matchesGroup(actionName, "SELECTCC608_",    category, prefix) ||
             matchesGroup(actionName, "SELECTTTC_",      category, prefix) ||
             matchesGroup(actionName, "SELECTAUDIO_",    category, prefix))
    {
        int i = 0;
        TrackType type = kTrackTypeUnknown;
        if (prefix == "SELECTSUBTITLE_")
            type = kTrackTypeSubtitle;
        else if (prefix == "SELECTRAWTEXT_")
            type = kTrackTypeRawText;
        else if (prefix == "SELECTCC708_")
            type = kTrackTypeCC708;
        else if (prefix == "SELECTCC608_")
            type = kTrackTypeCC608;
        else if (prefix == "SELECTTTC_")
            type = kTrackTypeTeletextCaptions;
        else if (prefix == "SELECTAUDIO_")
        {
            type = kTrackTypeAudio;
            if (m_tvmTracks[type].size() <= 1)
                i = 1; // don't show choices if only 1 audio track
        }

        for (; i < m_tvmTracks[type].size(); i++)
        {
            QString action = prefix + QString::number(i);
            active = (i == m_tvmCurtrack[type]);
            BUTTON(action, m_tvmTracks[type][i]);
        }
    }
    else if (matchesGroup(actionName, "ADJUSTSTRETCH", category, prefix))
    {
        struct speed
        {
            int     m_speedX100;
            QString m_suffix;
            QString m_trans;
        };

        static const std::array<const speed,9> s_speeds {{
            {  0, "",    tr("Adjust")},
            { 50, "0.5", tr("0.5x")},
            { 90, "0.9", tr("0.9x")},
            {100, "1.0", tr("1.0x")},
            {110, "1.1", tr("1.1x")},
            {120, "1.2", tr("1.2x")},
            {130, "1.3", tr("1.3x")},
            {140, "1.4", tr("1.4x")},
            {150, "1.5", tr("1.5x")},
        }};

        for (const auto & speed : s_speeds)
        {
            QString action = prefix + speed.m_suffix;
            active = (m_tvmSpeedX100 == speed.m_speedX100);
            BUTTON(action, speed.m_trans);
        }
    }
    else if (matchesGroup(actionName, "TOGGLESLEEP", category, prefix))
    {
        active = false;
        if (m_sleepTimerId)
            BUTTON(ACTION_TOGGLESLEEP + "ON", tr("Sleep Off"));
        BUTTON(ACTION_TOGGLESLEEP + "30", tr("%n minute(s)", "", 30));
        BUTTON(ACTION_TOGGLESLEEP + "60", tr("%n minute(s)", "", 60));
        BUTTON(ACTION_TOGGLESLEEP + "90", tr("%n minute(s)", "", 90));
        BUTTON(ACTION_TOGGLESLEEP + "120", tr("%n minute(s)", "", 120));
    }
    else if (matchesGroup(actionName, "CHANGROUP_", category, prefix))
    {
        if (m_dbUseChannelGroups)
        {
            active = false;
            BUTTON("CHANGROUP_ALL_CHANNELS", tr("All Channels"));
            ChannelGroupList::const_iterator it;
            for (it = m_dbChannelGroups.begin();
                 it != m_dbChannelGroups.end(); ++it)
            {
                QString action = prefix + QString::number(it->m_grpId);
                active = (static_cast<int>(it->m_grpId) == m_channelGroupId);
                BUTTON(action, it->m_name);
            }
        }
    }
    else if (matchesGroup(actionName, "TOGGLECOMMSKIP", category, prefix))
    {
        if (m_tvmIsRecording || m_tvmIsRecorded)
        {
            static constexpr std::array<const uint,3> kCasOrd { 0, 2, 1 };
            for (uint csm : kCasOrd)
            {
                const auto mode = static_cast<CommSkipMode>(csm);
                QString action = prefix + QString::number(csm);
                active = (mode == m_tvmCurSkip);
                BUTTON(action, toString(static_cast<CommSkipMode>(csm)));
            }
        }
    }
    else if (matchesGroup(actionName, "JUMPTOCHAPTER", category, prefix))
    {
        if (m_tvmNumChapters &&
            m_tvmNumChapters == m_tvmChapterTimes.size())
        {
            int size = QString::number(m_tvmNumChapters).size();
            for (int i = 0; i < m_tvmNumChapters; i++)
            {
                QString chapter1 = QString("%1").arg(i+1, size, 10, QChar(48));
                QString chapter2 = QString("%1").arg(i+1, 3   , 10, QChar(48));
                QString timestr = MythFormatTime(static_cast<int>(m_tvmChapterTimes[i]), "HH:mm:ss");
                QString desc = chapter1 + QString(" (%1)").arg(timestr);
                QString action = prefix + chapter2;
                active = (m_tvmCurrentChapter == (i + 1));
                BUTTON(action, desc);
            }
        }
    }
    else if (matchesGroup(actionName, "SWITCHTOANGLE", category, prefix))
    {
        if (m_tvmNumAngles > 1)
        {
            for (int i = 1; i <= m_tvmNumAngles; i++)
            {
                QString angleIdx = QString("%1").arg(i, 3, 10, QChar(48));
                QString desc = GetAngleName(i);
                QString action = prefix + angleIdx;
                active = (m_tvmCurrentAngle == i);
                BUTTON(action, desc);
            }
        }
    }
    else if (matchesGroup(actionName, "JUMPTOTITLE", category, prefix))
    {
        for (int i = 0; i < m_tvmNumTitles; i++)
        {
            if (GetTitleDuration(i) < 120) // Ignore < 2 minutes long
                continue;

            QString titleIdx = QString("%1").arg(i, 3, 10, QChar(48));
            QString desc = GetTitleName(i);
            QString action = prefix + titleIdx;
            active = (m_tvmCurrentTitle == i);
            BUTTON(action, desc);
        }
    }
    else if (matchesGroup(actionName, "SWITCHTOINPUT_", category, prefix))
    {
        if (m_playerContext.m_recorder)
        {
            uint inputid  = m_playerContext.GetCardID();
            std::vector<InputInfo> inputs = RemoteRequestFreeInputInfo(inputid);
            QSet <QString> addednames;
            addednames += CardUtil::GetDisplayName(inputid);
            for (auto & input : inputs)
            {
                if (input.m_inputId == inputid ||
                    addednames.contains(input.m_displayName))
                    continue;
                active = false;
                addednames += input.m_displayName;
                QString action = QString("SWITCHTOINPUT_") +
                    QString::number(input.m_inputId);
                BUTTON(action, input.m_displayName);
            }
        }
    }
    else if (matchesGroup(actionName, "SWITCHTOSOURCE_", category, prefix))
    {
        if (m_playerContext.m_recorder)
        {
            uint inputid  = m_playerContext.GetCardID();
            InfoMap info;
            m_playerContext.m_recorder->GetChannelInfo(info);
            uint sourceid = info["sourceid"].toUInt();
            QMap<uint, bool> sourceids;
            std::vector<InputInfo> inputs = RemoteRequestFreeInputInfo(inputid);
            for (auto & input : inputs)
            {
                if (input.m_sourceId == sourceid ||
                    sourceids[input.m_sourceId])
                    continue;
                active = false;
                sourceids[input.m_sourceId] = true;
                QString action = QString("SWITCHTOINPUT_") +
                    QString::number(input.m_inputId);
                BUTTON(action, SourceUtil::GetSourceName(input.m_sourceId));
            }
        }
    }
    else if (category == kMenuCategoryItem)
    {
        if (actionName == "TOGGLEAUDIOSYNC")
        {
            BUTTON(actionName, tr("Adjust Audio Sync"));
        }
        else if (actionName == "DISABLEVISUALISATION")
        {
            if (m_tvmVisual)
                BUTTON(actionName, tr("None"));
        }
        else if (actionName == "DISABLEUPMIX")
        {
            if (m_tvmCanUpmix)
            {
                active = !m_tvmUpmixing;
                BUTTON(actionName, tr("Disable Audio Upmixer"));
            }
        }
        else if (actionName == "ENABLEUPMIX")
        {
            if (m_tvmCanUpmix)
            {
                active = m_tvmUpmixing;
                BUTTON(actionName, tr("Auto Detect"));
            }
        }
        else if (actionName == "AUTODETECT_FILL")
        {
            if (m_tvmFillAutoDetect)
            {
                active =
                    (m_tvmAdjustFill == kAdjustFill_AutoDetect_DefaultHalf) ||
                    (m_tvmAdjustFill == kAdjustFill_AutoDetect_DefaultOff);
                BUTTON(actionName, tr("Auto Detect"));
            }
        }
        else if (actionName == "TOGGLEMANUALZOOM")
        {
            BUTTON(actionName, tr("Manual Zoom Mode"));
        }
        else if (actionName == "TOGGLENIGHTMODE")
        {
            if (m_tvmSup != kPictureAttributeSupported_None)
            {
                active = gCoreContext->GetBoolSetting("NightModeEnabled", false);
                BUTTON2(actionName,
                        tr("Disable Night Mode"), tr("Enable Night Mode"));
            }
        }
        else if (actionName == "DISABLESUBS")
        {
            active = !m_tvmSubsEnabled;
            if (m_tvmSubsHaveSubs)
                BUTTON(actionName, tr("Disable Subtitles"));
        }
        else if (actionName == "ENABLESUBS")
        {
            active = m_tvmSubsEnabled;
            if (m_tvmSubsHaveSubs)
                BUTTON(actionName, tr("Enable Subtitles"));
        }
        else if (actionName == "DISABLEFORCEDSUBS")
        {
            active = !m_tvmSubsForcedOn;
            if (!m_tvmTracks[kTrackTypeSubtitle].empty() ||
                !m_tvmTracks[kTrackTypeRawText].empty())
            {
                BUTTON(actionName, tr("Disable Forced Subtitles"));
            }
        }
        else if (actionName == "ENABLEFORCEDSUBS")
        {
            active = m_tvmSubsForcedOn;
            if (!m_tvmTracks[kTrackTypeSubtitle].empty() ||
                !m_tvmTracks[kTrackTypeRawText].empty())
            {
                BUTTON(actionName, tr("Enable Forced Subtitles"));
            }
        }
        else if (actionName == "DISABLEEXTTEXT")
        {
            active = m_tvmSubsCapMode != kDisplayTextSubtitle;
            if (m_tvmSubsHaveText)
                BUTTON(actionName, tr("Disable External Subtitles"));
        }
        else if (actionName == "ENABLEEXTTEXT")
        {
            active = m_tvmSubsCapMode == kDisplayTextSubtitle;
            if (m_tvmSubsHaveText)
                BUTTON(actionName, tr("Enable External Subtitles"));
        }
        else if (actionName == "TOGGLETTM")
        {
            if (!m_tvmTracks[kTrackTypeTeletextMenu].empty())
                BUTTON(actionName, tr("Toggle Teletext Menu"));
        }
        else if (actionName == "TOGGLESUBZOOM")
        {
            if (m_tvmSubsEnabled)
                BUTTON(actionName, tr("Adjust Subtitle Zoom"));
        }
        else if (actionName == "TOGGLESUBDELAY")
        {
            if (m_tvmSubsEnabled &&
                (m_tvmSubsCapMode == kDisplayRawTextSubtitle ||
                 m_tvmSubsCapMode == kDisplayTextSubtitle))
            {
                BUTTON(actionName, tr("Adjust Subtitle Delay"));
            }
        }
        else if (actionName == "PAUSE")
        {
            active = m_tvmIsPaused;
            BUTTON2(actionName, tr("Play"), tr("Pause"));
        }
        else if (actionName == "TOGGLESTRETCH")
        {
            BUTTON(actionName, tr("Toggle"));
        }
        else if (actionName == "TOGGLEBROWSE")
        {
            if (m_dbUseChannelGroups)
                BUTTON(actionName, tr("Toggle Browse Mode"));
        }
        else if (actionName == "CANCELPLAYLIST")
        {
            if (m_inPlaylist)
                BUTTON(actionName, tr("Cancel Playlist"));
        }
        else if (actionName == "DEBUGOSD")
        {
            BUTTON(actionName, tr("Playback Data"));
        }
        else if (actionName == "JUMPFFWD")
        {
            if (m_tvmJump)
                BUTTON(actionName, tr("Jump Ahead"));
        }
        else if (actionName == "JUMPRWND")
        {
            if (m_tvmJump)
                BUTTON(actionName, tr("Jump Back"));
        }
        else if (actionName == "JUMPTODVDROOTMENU")
        {
            if (m_tvmIsBd || m_tvmIsDvd)
            {
                active = m_tvmIsDvd;
                BUTTON2(actionName, tr("DVD Root Menu"), tr("Top menu"));
            }
        }
        else if (actionName == "JUMPTOPOPUPMENU")
        {
            if (m_tvmIsBd)
                BUTTON(actionName, tr("Popup menu"));
        }
        else if (actionName == "JUMPTODVDTITLEMENU")
        {
            if (m_tvmIsDvd)
                BUTTON(actionName, tr("DVD Title Menu"));
        }
        else if (actionName == "JUMPTODVDCHAPTERMENU")
        {
            if (m_tvmIsDvd)
                BUTTON(actionName, tr("DVD Chapter Menu"));
        }
        else if (actionName == "PREVCHAN")
        {
            if (m_tvmPreviousChan)
                BUTTON(actionName, tr("Previous Channel"));
        }
        else if (actionName == "GUIDE")
        {
            BUTTON(actionName, tr("Program Guide"));
        }
        else if (actionName == "FINDER")
        {
            BUTTON(actionName, tr("Program Finder"));
        }
        else if (actionName == "VIEWSCHEDULED")
        {
            BUTTON(actionName, tr("Upcoming Recordings"));
        }
        else if (actionName == "SCHEDULE")
        {
            BUTTON(actionName, tr("Edit Recording Schedule"));
        }
        else if (actionName == "DIALOG_JUMPREC_X_0")
        {
            BUTTON3(actionName, tr("Recorded Program"), "", true);
            QVariant v;
            v.setValue(MenuNodeTuple(Context.m_menu, Context.m_node));
            m_tvmJumprecBackHack = v;
        }
        else if (actionName == "JUMPPREV")
        {
            if (m_lastProgram != nullptr)
            {
                if (m_lastProgram->GetSubtitle().isEmpty())
                    BUTTON(actionName, m_lastProgram->GetTitle());
                else
                {
                    BUTTON(actionName,
                           QString("%1: %2")
                           .arg(m_lastProgram->GetTitle())
                           .arg(m_lastProgram->GetSubtitle()));
                }
            }
        }
        else if (actionName == "EDIT")
        {
            if (m_tvmIsLiveTv || m_tvmIsRecorded ||
                m_tvmIsRecording || m_tvmIsVideo)
            {
                active = m_tvmIsLiveTv;
                BUTTON2(actionName, tr("Edit Channel"), tr("Edit Recording"));
            }
        }
        else if (actionName == "TOGGLEAUTOEXPIRE")
        {
            if (m_tvmIsRecorded || m_tvmIsRecording)
            {
                active = m_tvmIsOn;
                BUTTON2(actionName,
                        tr("Turn Auto-Expire OFF"), tr("Turn Auto-Expire ON"));
            }
        }
        else if (actionName == "QUEUETRANSCODE")
        {
            if (m_tvmIsRecorded)
            {
                active = m_tvmTranscoding;
                BUTTON2(actionName, tr("Stop Transcoding"), tr("Default"));
            }
        }
        else if (actionName == "QUEUETRANSCODE_AUTO")
        {
            if (m_tvmIsRecorded)
            {
                active = m_tvmTranscoding;
                BUTTON(actionName, tr("Autodetect"));
            }
        }
        else if (actionName == "QUEUETRANSCODE_HIGH")
        {
            if (m_tvmIsRecorded)
            {
                active = m_tvmTranscoding;
                BUTTON(actionName, tr("High Quality"));
            }
        }
        else if (actionName == "QUEUETRANSCODE_MEDIUM")
        {
            if (m_tvmIsRecorded)
            {
                active = m_tvmTranscoding;
                BUTTON(actionName, tr("Medium Quality"));
            }
        }
        else if (actionName == "QUEUETRANSCODE_LOW")
        {
            if (m_tvmIsRecorded)
            {
                active = m_tvmTranscoding;
                BUTTON(actionName, tr("Low Quality"));
            }
        }
        else
        {
            // Allow an arbitrary action if it has a translated
            // description available to be used as the button text.
            // Look in the specified keybinding context as well as the
            // Global context.
            QString text = m_mainWindow->GetActionText(Context.m_menu.GetKeyBindingContext(), actionName);
            if (text.isEmpty())
                text = m_mainWindow->GetActionText("Global", actionName);
            if (!text.isEmpty())
                BUTTON(actionName, text);
        }
    }

    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
    return result;
}

void TV::MenuLazyInit(void *Field)
{
    if (Field == &m_tvmFreeRecorderCount)
        if (m_tvmFreeRecorderCount < 0)
            m_tvmFreeRecorderCount = RemoteGetFreeRecorderCount();
}

void TV::PlaybackMenuInit(const MenuBase &Menu)
{
    GetPlayerReadLock();
    m_tvmOsd = GetOSDL();
    if (&Menu != &m_playbackMenu && &Menu != &m_playbackCompactMenu)
        return;

    m_tvmAvsync   = true;
    m_tvmVisual   = false;
    m_tvmActive   = "";
    m_tvmUpmixing = false;
    m_tvmCanUpmix = false;

    m_tvmAspectOverride    = kAspect_Off;
    m_tvmAdjustFill        = kAdjustFill_Off;
    m_tvmFillAutoDetect    = false;
    m_tvmSup               = kPictureAttributeSupported_None;
    m_tvmStereoAllowed     = false;
    m_tvmStereoMode        = kStereoscopicModeAuto;

    m_tvmSpeedX100         = static_cast<int>(round(m_playerContext.m_tsNormal * 100));
    m_tvmState             = m_playerContext.GetState();
    m_tvmIsRecording       = (m_tvmState == kState_WatchingRecording);
    m_tvmIsRecorded        = (m_tvmState == kState_WatchingPreRecorded);
    m_tvmIsVideo           = (m_tvmState == kState_WatchingVideo);
    m_tvmCurSkip           = kCommSkipOff;
    m_tvmIsPaused          = false;
    m_tvmFreeRecorderCount = -1;
    m_tvmIsDvd             = (m_tvmState == kState_WatchingDVD);
    m_tvmIsBd              = (m_playerContext.m_buffer && m_playerContext.m_buffer->IsBD() &&
                              m_playerContext.m_buffer->BD()->IsHDMVNavigation());
    m_tvmJump              = ((m_tvmNumChapters == 0) && !m_tvmIsDvd &&
                              !m_tvmIsBd && m_playerContext.m_buffer &&
                              m_playerContext.m_buffer->IsSeekingAllowed());
    m_tvmIsLiveTv          = StateIsLiveTV(m_tvmState);
    m_tvmPreviousChan      = false;

    m_tvmNumChapters    = GetNumChapters();
    m_tvmCurrentChapter = GetCurrentChapter();
    m_tvmNumAngles      = GetNumAngles();
    m_tvmCurrentAngle   = GetCurrentAngle();
    m_tvmNumTitles      = GetNumTitles();
    m_tvmCurrentTitle   = GetCurrentTitle();
    m_tvmChapterTimes.clear();
    GetChapterTimes(m_tvmChapterTimes);

    m_tvmSubsCapMode   = 0;
    m_tvmSubsHaveText  = false;
    m_tvmSubsForcedOn  = true;
    m_tvmSubsEnabled   = false;
    m_tvmSubsHaveSubs  = false;

    for (int i = kTrackTypeUnknown ; i < kTrackTypeCount ; ++i)
        m_tvmCurtrack[i] = -1;

    if (m_tvmIsLiveTv)
    {
        QString prev_channum = m_playerContext.GetPreviousChannel();
        QString cur_channum  = QString();
        if (m_playerContext.m_tvchain)
            cur_channum = m_playerContext.m_tvchain->GetChannelName(-1);
        if (!prev_channum.isEmpty() && prev_channum != cur_channum)
            m_tvmPreviousChan = true;
    }

    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);

    if (m_playerContext.m_player && m_playerContext.m_player->GetVideoOutput())
    {
        for (uint i = kTrackTypeUnknown ; i < kTrackTypeCount ; ++i)
        {
            m_tvmTracks[i] = m_playerContext.m_player->GetTracks(i);
            if (!m_tvmTracks[i].empty())
                m_tvmCurtrack[i] = m_playerContext.m_player->GetTrack(i);
        }
        m_tvmSubsHaveSubs =
            !m_tvmTracks[kTrackTypeSubtitle].empty() ||
            m_tvmSubsHaveText ||
            !m_tvmTracks[kTrackTypeCC708].empty() ||
            !m_tvmTracks[kTrackTypeCC608].empty() ||
            !m_tvmTracks[kTrackTypeTeletextCaptions].empty() ||
            !m_tvmTracks[kTrackTypeRawText].empty();
        m_tvmAvsync = (m_playerContext.m_player->GetTrackCount(kTrackTypeVideo) > 0) &&
            !m_tvmTracks[kTrackTypeAudio].empty();
        m_tvmVisual           = m_playerContext.m_player->CanVisualise();
        m_tvmActive           = m_playerContext.m_player->GetVisualiserName();
        m_tvmUpmixing         = m_playerContext.m_player->GetAudio()->IsUpmixing();
        m_tvmCanUpmix         = m_playerContext.m_player->GetAudio()->CanUpmix();
        m_tvmAspectOverride   = m_playerContext.m_player->GetAspectOverride();
        m_tvmAdjustFill       = m_playerContext.m_player->GetAdjustFill();
        m_tvmCurSkip          = m_playerContext.m_player->GetAutoCommercialSkip();
        m_tvmIsPaused         = m_playerContext.m_player->IsPaused();
        m_tvmSubsCapMode      = m_playerContext.m_player->GetCaptionMode();
        m_tvmSubsEnabled      = m_playerContext.m_player->GetCaptionsEnabled();
        m_tvmSubsHaveText     = m_playerContext.m_player->HasTextSubtitles();
        m_tvmSubsForcedOn     = m_playerContext.m_player->GetAllowForcedSubtitles();
        if (m_tvmVisual)
            m_tvmVisualisers = m_playerContext.m_player->GetVisualiserList();
        MythVideoOutput *vo = m_playerContext.m_player->GetVideoOutput();
        if (vo)
        {
            m_tvmSup            = vo->GetSupportedPictureAttributes();
            m_tvmStereoAllowed  = vo->StereoscopicModesAllowed();
            m_tvmStereoMode     = vo->GetStereoscopicMode();
            m_tvmFillAutoDetect = vo->HasSoftwareFrames();  
        }
    }
    m_playerContext.LockPlayingInfo(__FILE__, __LINE__);
    m_tvmIsOn = m_playerContext.m_playingInfo->QueryAutoExpire() != kDisableAutoExpire;
    m_tvmTranscoding = JobQueue::IsJobQueuedOrRunning
        (JOB_TRANSCODE, m_playerContext.m_playingInfo->GetChanID(),
         m_playerContext.m_playingInfo->GetScheduledStartTime());
    m_playerContext.UnlockPlayingInfo(__FILE__, __LINE__);

    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
}

void TV::PlaybackMenuDeinit(const MenuBase& /*Menu*/)
{
    ReturnOSDLock();
    ReturnPlayerLock();
    m_tvmOsd = nullptr;
    m_tvmOsd = nullptr;
}

void TV::PlaybackMenuShow(const MenuBase &Menu, const QDomNode &Node, const QDomNode &Selected)
{
    PlaybackMenuInit(Menu);
    if (m_tvmOsd)
    {
        bool isPlayback = (&Menu == &m_playbackMenu || &Menu == &m_playbackCompactMenu);
        bool isCutlist = (&Menu == &m_cutlistMenu || &Menu == &m_cutlistCompactMenu);
        m_tvmOsd->DialogShow(isPlayback ? OSD_DLG_MENU : isCutlist ? OSD_DLG_CUTPOINT : "???", Menu.GetName());
        Menu.Show(Node, Selected, *this);
        QString text = Menu.Translate(Node.toElement().attribute("text", Menu.GetName()));
        m_tvmOsd->DialogSetText(text);
        QDomNode parent = Node.parentNode();
        if (!parent.parentNode().isNull())
        {
            QVariant v;
            v.setValue(MenuNodeTuple(Menu, Node));
            m_tvmOsd->DialogBack("", v);
        }
        if (isCutlist)
        {
            // hack to unhide the editbar
            InfoMap map;
            map.insert("title", tr("Edit"));
            m_tvmOsd->SetText("osd_program_editor", map, kOSDTimeout_None);
        }
    }
    PlaybackMenuDeinit(Menu);
}

void TV::MenuStrings()
{
    // Playback menu
    (void)tr("Playback Menu");
    (void)tr("Playback Compact Menu");
    (void)tr("Audio");
    (void)tr("Select Audio Track");
    (void)tr("Visualisation");
    (void)tr("Video");
    (void)tr("Change Aspect Ratio");
    (void)tr("Adjust Fill");
    (void)tr("Adjust Picture");
    (void)tr("3D");
    (void)tr("Advanced");
    (void)tr("Video Scan");
    (void)tr("Deinterlacer");
    (void)tr("Subtitles");
    (void)tr("Select Subtitle");
    (void)tr("Text Subtitles");
    (void)tr("Select ATSC CC");
    (void)tr("Select VBI CC");
    (void)tr("Select Teletext CC");
    (void)tr("Playback");
    (void)tr("Adjust Time Stretch");
    (void)tr("Picture-in-Picture");
    (void)tr("Sleep");
    (void)tr("Channel Groups");
    (void)tr("Navigate");
    (void)tr("Commercial Auto-Skip");
    (void)tr("Chapter");
    (void)tr("Angle");
    (void)tr("Title");
    (void)tr("Schedule");
    (void)tr("Source");
    (void)tr("Jump to Program");
    (void)tr("Switch Input");
    (void)tr("Switch Source");
    (void)tr("Jobs");
    (void)tr("Begin Transcoding");

    // Cutlist editor menu
    (void)tr("Edit Cut Points");
    (void)tr("Edit Cut Points (Compact)");
    (void)tr("Cut List Options");
}

void TV::ShowOSDMenu(bool isCompact)
{
    if (!m_playbackMenu.IsLoaded())
    {
        m_playbackMenu.LoadFromFile("menu_playback.xml",
                                    tr("Playback Menu"),
                                    metaObject()->className(),
                                    "TV Playback");
        m_playbackCompactMenu.LoadFromFile("menu_playback_compact.xml",
                                           tr("Playback Compact Menu"),
                                           metaObject()->className(),
                                           "TV Playback");
    }
    if (isCompact && m_playbackCompactMenu.IsLoaded())
    {
        PlaybackMenuShow(m_playbackCompactMenu,
                         m_playbackCompactMenu.GetRoot(),
                         QDomNode());
    }
    else if (m_playbackMenu.IsLoaded())
    {
        PlaybackMenuShow(m_playbackMenu,
                         m_playbackMenu.GetRoot(),
                         QDomNode());
    }
}

void TV::FillOSDMenuJumpRec(const QString &Category, int Level, const QString &Selected)
{
    // bool in_recgroup = !category.isEmpty() && level > 0;
    if (Level < 0 || Level > 1)
    {
        Level = 0;
        // in_recgroup = false;
    }

    OSD *osd = GetOSDL();
    if (osd)
    {
        QString title = tr("Recorded Program");
        osd->DialogShow("osd_jumprec", title);

        QMutexLocker locker(&m_progListsLock);
        m_progLists.clear();
        std::vector<ProgramInfo*> *infoList = RemoteGetRecordedList(0);
        bool LiveTVInAllPrograms = gCoreContext->GetBoolSetting("LiveTVInAllPrograms",false);
        if (infoList)
        {
            QList<QString> titles_seen;

            m_playerContext.LockPlayingInfo(__FILE__, __LINE__);
            QString currecgroup = m_playerContext.m_playingInfo->GetRecordingGroup();
            m_playerContext.UnlockPlayingInfo(__FILE__, __LINE__);

            for (auto *pi : *infoList)
            {
                if (pi->GetRecordingGroup() != "LiveTV" || LiveTVInAllPrograms ||
                     pi->GetRecordingGroup() == currecgroup)
                {
                    m_progLists[pi->GetRecordingGroup()].push_front(
                        new ProgramInfo(*pi));
                }
            }

            ProgramInfo *lastprog = GetLastProgram();
            QMap<QString,ProgramList>::const_iterator Iprog;
            for (Iprog = m_progLists.cbegin(); Iprog != m_progLists.cend(); ++Iprog)
            {
                const ProgramList &plist = *Iprog;
                auto progIndex = static_cast<uint>(plist.size());
                const QString& group = Iprog.key();

                if (plist[0]->GetRecordingGroup() != currecgroup)
                    SetLastProgram(plist[0]);

                if (progIndex == 1 && Level == 0)
                {
                    osd->DialogAddButton(Iprog.key(),
                                         QString("JUMPPROG %1 0").arg(group));
                }
                else if (progIndex > 1 && Level == 0)
                {
                    QString act = QString("DIALOG_%1_%2_1")
                                    .arg(ACTION_JUMPREC).arg(group);
                    osd->DialogAddButton(group, act,
                                         true, Selected == group);
                }
                else if (Level == 1 && Iprog.key() == Category)
                {
                    for (auto pit = plist.begin(); pit != plist.end(); ++pit)
                    {
                        const ProgramInfo *p = *pit;

                        if (titles_seen.contains(p->GetTitle()))
                            continue;

                        titles_seen.push_back(p->GetTitle());

                        int j = -1;
                        for (auto *q : plist)
                        {
                            j++;

                            if (q->GetTitle() != p->GetTitle())
                                continue;

                            osd->DialogAddButton(q->GetSubtitle().isEmpty() ?
                                    q->GetTitle() : q->GetSubtitle(),
                                    QString("JUMPPROG %1 %2")
                                        .arg(Iprog.key()).arg(j));
                        }
                    }
                }
            }
            SetLastProgram(lastprog);
            delete lastprog;

            while (!infoList->empty())
            {
                delete infoList->back();
                infoList->pop_back();
            }
            delete infoList;
        }

        if (!Category.isEmpty())
        {
            if (Level == 1)
                osd->DialogBack(Category, "DIALOG_" + ACTION_JUMPREC + "_X_0");
            else if (Level == 0)
            {
                if (m_tvmJumprecBackHack.isValid())
                    osd->DialogBack("", m_tvmJumprecBackHack);
                else
                    osd->DialogBack(ACTION_JUMPREC,
                                    "DIALOG_MENU_" + ACTION_JUMPREC +"_0");
            }
        }
    }
    ReturnOSDLock();
}

void TV::OverrideScan(FrameScanType Scan)
{
    QString message;
    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (m_playerContext.m_player)
    {
        m_playerContext.m_player->SetScanOverride(Scan);
        message = ScanTypeToUserString(Scan == kScan_Detect ? kScan_Detect :
                    m_playerContext.m_player->GetScanType(), Scan > kScan_Detect);
    }
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);

    if (!message.isEmpty())
        SetOSDMessage(message);
}

void TV::ToggleAutoExpire()
{
    QString desc;

    m_playerContext.LockPlayingInfo(__FILE__, __LINE__);

    if (m_playerContext.m_playingInfo->QueryAutoExpire() != kDisableAutoExpire)
    {
        m_playerContext.m_playingInfo->SaveAutoExpire(kDisableAutoExpire);
        desc = tr("Auto-Expire OFF");
    }
    else
    {
        m_playerContext.m_playingInfo->SaveAutoExpire(kNormalAutoExpire);
        desc = tr("Auto-Expire ON");
    }

    m_playerContext.UnlockPlayingInfo(__FILE__, __LINE__);

    if (!desc.isEmpty())
        UpdateOSDSeekMessage(desc, kOSDTimeout_Med);
}

void TV::SetAutoCommercialSkip(CommSkipMode SkipMode)
{
    QString desc;

    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (m_playerContext.m_player)
    {
        m_playerContext.m_player->SetAutoCommercialSkip(SkipMode);
        desc = toString(m_playerContext.m_player->GetAutoCommercialSkip());
    }
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);

    if (!desc.isEmpty())
        UpdateOSDSeekMessage(desc, kOSDTimeout_Med);
}

void TV::SetManualZoom(bool ZoomON, const QString& Desc)
{
    m_zoomMode = ZoomON;
    if (ZoomON)
        ClearOSD();
    if (!Desc.isEmpty())
        UpdateOSDSeekMessage(Desc, ZoomON ? kOSDTimeout_None : kOSDTimeout_Med);
}

bool TV::HandleJumpToProgramAction(const QStringList &Actions)
{
    TVState state = GetState();
    if (has_action(ACTION_JUMPPREV, Actions) || (has_action("PREVCHAN", Actions) && !StateIsLiveTV(state)))
    {
        PrepareToExitPlayer(__LINE__);
        m_jumpToProgram = true;
        SetExitPlayer(true, true);
        return true;
    }

    for (const auto& action : qAsConst(Actions))
    {
        if (!action.startsWith("JUMPPROG"))
            continue;

        bool ok = false;
        QString key   = action.section(" ",1,-2);
        uint    index = action.section(" ",-1,-1).toUInt(&ok);
        ProgramInfo* proginfo = nullptr;

        if (ok)
        {
            QMutexLocker locker(&m_progListsLock);
            auto pit = m_progLists.find(key);
            if (pit != m_progLists.end())
            {
                const ProgramInfo* tmp = (*pit)[index];
                if (tmp)
                    proginfo = new ProgramInfo(*tmp);
            }
        }

        if (!proginfo)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to locate jump to program '%1' @ %2")
                .arg(key).arg(action.section(" ",-1,-1)));
            return true;
        }

        PrepToSwitchToRecordedProgram(*proginfo);

        delete proginfo;
        return true;
    }

    bool wants_jump = has_action(ACTION_JUMPREC, Actions);
    if (!wants_jump)
        return false;

    if (m_dbJumpPreferOsd && (StateIsPlaying(state) || StateIsLiveTV(state)))
    {
        // TODO I'm not sure this really needs to be asyncronous
        auto Jump = [&]()
        {
            GetPlayerReadLock();
            FillOSDMenuJumpRec();
            ReturnPlayerLock();
        };
        QTimer::singleShot(0, Jump);
    }
    else if (RunPlaybackBoxPtr)
    {
        EditSchedule(kPlaybackBox);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to open jump to program GUI");
    }

    return true;
}

#define MINUTE (60*1000)

void TV::ToggleSleepTimer(const QString& Time)
{
    uint mins = 0;

    if (Time == ACTION_TOGGLESLEEP + "ON")
    {
        if (m_sleepTimerId)
        {
            KillTimer(m_sleepTimerId);
            m_sleepTimerId = 0;
        }
        else
        {
            mins = 60;
            m_sleepTimerTimeout = mins * MINUTE;
            m_sleepTimerId = StartTimer(static_cast<int>(m_sleepTimerTimeout), __LINE__);
        }
    }
    else
    {
        if (m_sleepTimerId)
        {
            KillTimer(m_sleepTimerId);
            m_sleepTimerId = 0;
        }

        if (Time.length() > 11)
        {
            bool intRead = false;
            mins = Time.rightRef(Time.length() - 11).toUInt(&intRead);

            if (intRead)
            {
                // catch 120 -> 240 mins
                if (mins < 30)
                {
                    mins *= 10;
                }
            }
            else
            {
                mins = 0;
                LOG(VB_GENERAL, LOG_ERR, LOC + "Invalid time " + Time);
            }
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Invalid time string " + Time);
        }

        if (mins)
        {
            m_sleepTimerTimeout = mins * MINUTE;
            m_sleepTimerId = StartTimer(static_cast<int>(m_sleepTimerTimeout), __LINE__);
        }
    }

    QString out;
    if (mins != 0)
        out = tr("Sleep") + " " + QString::number(mins);
    else
        out = tr("Sleep") + " " + s_sleepTimes[0].dispString;
    SetOSDMessage(out);
}

void TV::ShowNoRecorderDialog(NoRecorderMsg MsgType)
{
    QString errorText;

    switch (MsgType)
    {
        case kNoRecorders:
            errorText = tr("MythTV is already using all available "
                           "inputs for the channel you selected. "
                           "If you want to watch an in-progress recording, "
                           "select one from the playback menu.  If you "
                           "want to watch Live TV, cancel one of the "
                           "in-progress recordings from the delete "
                           "menu.");
            break;
        case kNoCurrRec:
            errorText = tr("Error: MythTV is using all inputs, "
                           "but there are no active recordings?");
            break;
        case kNoTuners:
            errorText = tr("MythTV has no capture cards defined. "
                           "Please run the mythtv-setup program.");
            break;
    }

    OSD *osd = GetOSDL();
    if (osd)
    {
        osd->DialogShow(OSD_DLG_INFO, errorText);
        osd->DialogAddButton(tr("OK"), "DIALOG_INFO_X_X");
    }
    else
    {
        ShowOkPopup(errorText);
    }
    ReturnOSDLock();
}

/**
 *  \brief Used in ChangeChannel() to temporarily stop video output.
 */
void TV::PauseLiveTV()
{
    m_lockTimerOn = false;

    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (m_playerContext.m_player && m_playerContext.m_buffer)
    {
        m_playerContext.m_buffer->IgnoreLiveEOF(true);
        m_playerContext.m_buffer->StopReads();
        m_playerContext.m_player->PauseDecoder();
        m_playerContext.m_buffer->StartReads();
    }
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);

    // XXX: Get rid of this?
    m_playerContext.m_recorder->PauseRecorder();

    m_playerContext.m_lastSignalMsg.clear();
    m_playerContext.m_lastSignalUIInfo.clear();

    m_lockTimerOn = false;

    QString input = m_playerContext.m_recorder->GetInput();
    uint timeout  = m_playerContext.m_recorder->GetSignalLockTimeout(input);

    if (timeout < 0xffffffff)
    {
        m_lockTimer.start();
        m_lockTimerOn = true;
    }

    SetSpeedChangeTimer(0, __LINE__);
}

/**
 *  \brief Used in ChangeChannel() to restart video output.
 */
void TV::UnpauseLiveTV(bool Quietly)
{
    if (m_playerContext.HasPlayer() && m_playerContext.m_tvchain)
    {
        m_playerContext.ReloadTVChain();
        m_playerContext.m_tvchain->JumpTo(-1, 1);
        m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
        if (m_playerContext.m_player)
            m_playerContext.m_player->Play(m_playerContext.m_tsNormal, true, false);
        m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
        if (m_playerContext.m_buffer)
            m_playerContext.m_buffer->IgnoreLiveEOF(false);
        SetSpeedChangeTimer(0, __LINE__);
    }

    ITVRestart(true);

    if (m_playerContext.HasPlayer() && !Quietly)
    {
        UpdateOSDProgInfo("program_info");
        UpdateLCD();
        m_playerContext.PushPreviousChannel();
    }
}

/**
 * \brief Restart the MHEG/MHP engine.
 */
void TV::ITVRestart(bool IsLive)
{
    int chanid = -1;
    int sourceid = -1;

    if (ContextIsPaused(__FILE__, __LINE__))
        return;

    m_playerContext.LockPlayingInfo(__FILE__, __LINE__);
    if (m_playerContext.m_playingInfo)
    {
        chanid = static_cast<int>(m_playerContext.m_playingInfo->GetChanID());
        sourceid = static_cast<int>(ChannelUtil::GetSourceIDForChannel(static_cast<uint>(chanid)));
    }
    m_playerContext.UnlockPlayingInfo(__FILE__, __LINE__);

    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (m_playerContext.m_player)
        m_playerContext.m_player->ITVRestart(static_cast<uint>(chanid), static_cast<uint>(sourceid), IsLive);
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
}

void TV::DoJumpFFWD()
{
    if (GetState() == kState_WatchingDVD)
        DVDJumpForward();
    else if (GetNumChapters() > 0)
        DoJumpChapter(9999);
    else
        DoSeek(m_playerContext.m_jumptime * 60, tr("Jump Ahead"), /*timeIsOffset*/true, /*honorCutlist*/true);
}

void TV::DoSeekFFWD()
{
    DoSeek(m_playerContext.m_fftime, tr("Skip Ahead"), /*timeIsOffset*/true, /*honorCutlist*/true);
}

void TV::DoJumpRWND()
{
    if (GetState() == kState_WatchingDVD)
        DVDJumpBack();
    else if (GetNumChapters() > 0)
        DoJumpChapter(-1);
    else
        DoSeek(-m_playerContext.m_jumptime * 60, tr("Jump Back"), /*timeIsOffset*/true, /*honorCutlist*/true);
}

void TV::DoSeekRWND()
{
    DoSeek(-m_playerContext.m_rewtime, tr("Jump Back"), /*timeIsOffset*/true, /*honorCutlist*/true);
}

/*  \fn TV::DVDJumpBack(PlayerContext*)
    \brief jump to the previous dvd title or chapter
*/
void TV::DVDJumpBack()
{
    auto *dvd = dynamic_cast<MythDVDBuffer*>(m_playerContext.m_buffer);
    if (!m_playerContext.HasPlayer() || !dvd)
        return;

    if (m_playerContext.m_buffer->IsInDiscMenuOrStillFrame())
    {
        UpdateOSDSeekMessage(tr("Skip Back Not Allowed"), kOSDTimeout_Med);
    }
    else if (!dvd->StartOfTitle())
    {
        DoJumpChapter(-1);
    }
    else
    {
        uint titleLength = dvd->GetTotalTimeOfTitle();
        uint chapterLength = dvd->GetChapterLength();
        if ((titleLength == chapterLength) && chapterLength > 300)
        {
            DoSeek(-m_playerContext.m_jumptime * 60, tr("Jump Back"), /*timeIsOffset*/true, /*honorCutlist*/true);
        }
        else
        {
            m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
            if (m_playerContext.m_player)
                m_playerContext.m_player->GoToDVDProgram(false);
            m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);

            UpdateOSDSeekMessage(tr("Previous Title"), kOSDTimeout_Med);
        }
    }
}

/* \fn TV::DVDJumpForward(PlayerContext*)
 * \brief jump to the next dvd title or chapter
 */
void TV::DVDJumpForward()
{
    auto *dvd = dynamic_cast<MythDVDBuffer*>(m_playerContext.m_buffer);
    if (!m_playerContext.HasPlayer() || !dvd)
        return;

    bool in_still = dvd->IsInStillFrame();
    bool in_menu  = dvd->IsInMenu();
    if (in_still && !dvd->NumMenuButtons())
    {
        dvd->SkipStillFrame();
        UpdateOSDSeekMessage(tr("Skip Still Frame"), kOSDTimeout_Med);
    }
    else if (!dvd->EndOfTitle() && !in_still && !in_menu)
    {
        DoJumpChapter(9999);
    }
    else if (!in_still && !in_menu)
    {
        uint titleLength = dvd->GetTotalTimeOfTitle();
        uint chapterLength = dvd->GetChapterLength();
        uint currentTime = static_cast<uint>(dvd->GetCurrentTime());
        if ((titleLength == chapterLength) && chapterLength > 300 &&
             (currentTime < (chapterLength - (static_cast<uint>(m_playerContext.m_jumptime) * 60))))
        {
            DoSeek(m_playerContext.m_jumptime * 60, tr("Jump Ahead"), /*timeIsOffset*/true, /*honorCutlist*/true);
        }
        else
        {
            m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
            if (m_playerContext.m_player)
                m_playerContext.m_player->GoToDVDProgram(true);
            m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);

            UpdateOSDSeekMessage(tr("Next Title"), kOSDTimeout_Med);
        }
    }
}

/* \fn TV::IsBookmarkAllowed(const PlayerContext*) const
 * \brief Returns true if bookmarks are allowed for the current player.
 */
bool TV::IsBookmarkAllowed()
{
    m_playerContext.LockPlayingInfo(__FILE__, __LINE__);

    // Allow bookmark of "Record current LiveTV program"
    if (StateIsLiveTV(GetState()) && m_playerContext.m_playingInfo &&
        (m_playerContext.m_playingInfo->QueryAutoExpire() == kLiveTVAutoExpire))
    {
        m_playerContext.UnlockPlayingInfo(__FILE__, __LINE__);
        return false;
    }

    if (StateIsLiveTV(GetState()) && !m_playerContext.m_playingInfo)
    {
        m_playerContext.UnlockPlayingInfo(__FILE__, __LINE__);
        return false;
    }

    m_playerContext.UnlockPlayingInfo(__FILE__, __LINE__);

    return m_playerContext.m_buffer && m_playerContext.m_buffer->IsBookmarkAllowed();
}

/* \fn TV::IsDeleteAllowed() const
 * \brief Returns true if the delete menu option should be offered.
 */
bool TV::IsDeleteAllowed()
{
    bool allowed = false;

    if (!StateIsLiveTV(GetState()))
    {
        m_playerContext.LockPlayingInfo(__FILE__, __LINE__);
        ProgramInfo *curProgram = m_playerContext.m_playingInfo;
        allowed = curProgram && curProgram->QueryIsDeleteCandidate(true);
        m_playerContext.UnlockPlayingInfo(__FILE__, __LINE__);
    }

    return allowed;
}

void TV::ShowOSDStopWatchingRecording()
{
    ClearOSD();

    if (!ContextIsPaused(__FILE__, __LINE__))
        DoTogglePause(false);

    QString videotype;

    if (StateIsLiveTV(GetState()))
        videotype = tr("Live TV");
    else if (m_playerContext.m_buffer && m_playerContext.m_buffer->IsDVD())
        videotype = tr("this DVD");

    m_playerContext.LockPlayingInfo(__FILE__, __LINE__);
    if (videotype.isEmpty() && m_playerContext.m_playingInfo->IsVideo())
        videotype = tr("this Video");
    m_playerContext.UnlockPlayingInfo(__FILE__, __LINE__);

    if (videotype.isEmpty())
        videotype = tr("this recording");

    OSD *osd = GetOSDL();
    if (osd)
    {
        osd->DialogShow(OSD_DLG_VIDEOEXIT,
                        tr("You are exiting %1").arg(videotype));

        if (IsBookmarkAllowed())
        {
            osd->DialogAddButton(tr("Save this position and go to the menu"), "DIALOG_VIDEOEXIT_SAVEPOSITIONANDEXIT_0");
            osd->DialogAddButton(tr("Do not save, just exit to the menu"), ACTION_STOP);
        }
        else
        {
            osd->DialogAddButton(tr("Exit %1").arg(videotype), ACTION_STOP);
        }

        if (IsDeleteAllowed())
            osd->DialogAddButton(tr("Delete this recording"), "DIALOG_VIDEOEXIT_CONFIRMDELETE_0");

        osd->DialogAddButton(tr("Keep watching"), "DIALOG_VIDEOEXIT_KEEPWATCHING_0");
        osd->DialogBack("", "DIALOG_VIDEOEXIT_KEEPWATCHING_0", true);
    }
    ReturnOSDLock();

    if (m_videoExitDialogTimerId)
        KillTimer(m_videoExitDialogTimerId);
    m_videoExitDialogTimerId = StartTimer(kVideoExitDialogTimeout, __LINE__);
}

void TV::ShowOSDPromptDeleteRecording(const QString& Title, bool Force)
{
    m_playerContext.LockPlayingInfo(__FILE__, __LINE__);

    if (m_playerContext.m_ffRewState || StateIsLiveTV(m_playerContext.GetState()) || m_exitPlayerTimerId)
    {
        // this should only occur when the cat walks on the keyboard.
        LOG(VB_GENERAL, LOG_ERR, "It is unsafe to delete at the moment");
        m_playerContext.UnlockPlayingInfo(__FILE__, __LINE__);
        return;
    }

    bool paused = ContextIsPaused(__FILE__, __LINE__);
    if (!m_playerContext.m_playingInfo->QueryIsDeleteCandidate(true))
    {
        LOG(VB_GENERAL, LOG_ERR, "This program cannot be deleted at this time.");
        ProgramInfo pginfo(*m_playerContext.m_playingInfo); // cppcheck-suppress variableScope
        m_playerContext.UnlockPlayingInfo(__FILE__, __LINE__);

        OSD *osd = GetOSDL();
        if (osd && !osd->DialogVisible())
        {
            QString message = tr("Cannot delete program ") + QString("%1 ").arg(pginfo.GetTitle());

            if (!pginfo.GetSubtitle().isEmpty())
                message += QString("\"%1\" ").arg(pginfo.GetSubtitle());

            if (!pginfo.IsRecording())
            {
                message += tr("because it is not a recording.");
            }
            else
            {
                message += tr("because it is in use by");
                QStringList byWho;
                pginfo.QueryIsInUse(byWho);
                for (int i = 0; (i + 2) < byWho.size(); i += 3)
                {
                    if (byWho[i + 1] == gCoreContext->GetHostName() && byWho[i].contains(kPlayerInUseID))
                        continue;
                    if (byWho[i].contains(kRecorderInUseID))
                        continue;
                    message += " " + byWho[i+2];
                }
            }
            osd->DialogShow(OSD_DLG_DELETE, message);
            QString action = "DIALOG_DELETE_OK_0";
            osd->DialogAddButton(tr("OK"), action);
            osd->DialogBack("", action, true);
        }
        ReturnOSDLock();
        // If the delete prompt is to be displayed at the end of a
        // recording that ends in a final cut region, it will get into
        // a loop of popping up the OK button while the cut region
        // plays.  Avoid this.
        if (m_playerContext.m_player->IsNearEnd() && !paused)
            SetExitPlayer(true, true);

        return;
    }
    m_playerContext.UnlockPlayingInfo(__FILE__, __LINE__);

    ClearOSD();

    if (!paused)
        DoTogglePause(false);

    InfoMap infoMap;
    m_playerContext.GetPlayingInfoMap(infoMap);
    QString message = QString("%1\n%2\n%3")
        .arg(Title).arg(infoMap["title"]).arg(infoMap["timedate"]);

    OSD *osd = GetOSDL();
    if (osd && (!osd->DialogVisible() || Force))
    {
        osd->DialogShow(OSD_DLG_VIDEOEXIT, message);
        if (Title == "End Of Recording")
        {
            osd->DialogAddButton(tr("Delete it, but allow it to re-record"),
                                 "DIALOG_VIDEOEXIT_DELETEANDRERECORD_0");
            osd->DialogAddButton(tr("Delete it"),
                                 "DIALOG_VIDEOEXIT_JUSTDELETE_0");
            osd->DialogAddButton(tr("Save it so I can watch it again"),
                                 ACTION_STOP, false, true);
        }
        else
        {
            osd->DialogAddButton(tr("Yes, and allow re-record"),
                                 "DIALOG_VIDEOEXIT_DELETEANDRERECORD_0");
            osd->DialogAddButton(tr("Yes, delete it"),
                                 "DIALOG_VIDEOEXIT_JUSTDELETE_0");
            osd->DialogAddButton(tr("No, keep it"),
                                 ACTION_STOP, false, true);
            if (!paused)
                osd->DialogBack("", "DIALOG_PLAY_0_0", true);
        }

        if (m_videoExitDialogTimerId)
            KillTimer(m_videoExitDialogTimerId);
        m_videoExitDialogTimerId = StartTimer(kVideoExitDialogTimeout, __LINE__);
    }
    ReturnOSDLock();
}

bool TV::HandleOSDVideoExit(const QString& Action)
{
    if (!DialogIsVisible(OSD_DLG_VIDEOEXIT))
        return false;

    bool hide        = true;
    bool delete_ok   = IsDeleteAllowed();
    bool bookmark_ok = IsBookmarkAllowed();

    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    bool near_end = m_playerContext.m_player && m_playerContext.m_player->IsNearEnd();
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);

    if (Action == "DELETEANDRERECORD" && delete_ok)
    {
        m_allowRerecord = true;
        m_requestDelete = true;
        SetExitPlayer(true, true);
    }
    else if (Action == "JUSTDELETE" && delete_ok)
    {
        m_requestDelete = true;
        SetExitPlayer(true, true);
    }
    else if (Action == "CONFIRMDELETE")
    {
        hide = false;
        ShowOSDPromptDeleteRecording(tr("Are you sure you want to delete:"), true);
    }
    else if (Action == "SAVEPOSITIONANDEXIT" && bookmark_ok)
    {
        PrepareToExitPlayer(__LINE__, kBookmarkAlways);
        SetExitPlayer(true, true);
    }
    else if (Action == "KEEPWATCHING" && !near_end)
    {
        DoTogglePause(true);
    }

    return hide;
}

void TV::HandleSaveLastPlayPosEvent()
{
    // Helper class to save the latest playback position (in a background thread
    // to avoid playback glitches).  The ctor makes a copy of the ProgramInfo
    // struct to avoid race conditions if playback ends and deletes objects
    // before or while the background thread runs.
    class PositionSaver : public QRunnable
    {
      public:
        PositionSaver(const ProgramInfo &pginfo, uint64_t frame)
          : m_pginfo(pginfo),
            m_frame(frame) {}

        void run() override
        {
            LOG(VB_PLAYBACK, LOG_DEBUG, QString("PositionSaver frame=%1").arg(m_frame));
            frm_dir_map_t lastPlayPosMap;
            lastPlayPosMap[m_frame] = MARK_UTIL_LASTPLAYPOS;
            m_pginfo.ClearMarkupMap(MARK_UTIL_LASTPLAYPOS);
            m_pginfo.SaveMarkupMap(lastPlayPosMap, MARK_UTIL_LASTPLAYPOS);
        }

      private:
        const ProgramInfo m_pginfo;
        const uint64_t m_frame;
    };

    GetPlayerReadLock();
    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    bool playing = m_playerContext.m_player && !m_playerContext.m_player->IsPaused();
    // Don't bother saving lastplaypos while paused
    if (playing)
    {
        uint64_t framesPlayed = m_playerContext.m_player->GetFramesPlayed();
        MThreadPool::globalInstance()->start(new PositionSaver(*m_playerContext.m_playingInfo, framesPlayed), "PositionSaver");
    }
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
    ReturnPlayerLock();

    KillTimer(m_saveLastPlayPosTimerId);
    m_saveLastPlayPosTimerId = StartTimer(kSaveLastPlayPosTimeout, __LINE__);
}

void TV::SetLastProgram(const ProgramInfo* ProgInfo)
{
    QMutexLocker locker(&m_lastProgramLock);

    delete m_lastProgram;

    if (ProgInfo)
        m_lastProgram = new ProgramInfo(*ProgInfo);
    else
        m_lastProgram = nullptr;
}

ProgramInfo *TV::GetLastProgram() const
{
    QMutexLocker locker(&m_lastProgramLock);
    if (m_lastProgram)
        return new ProgramInfo(*m_lastProgram);
    return nullptr;
}

QString TV::GetRecordingGroup() const
{
    QString ret;

    GetPlayerReadLock();
    if (StateIsPlaying(GetState()))
    {
        m_playerContext.LockPlayingInfo(__FILE__, __LINE__);
        if (m_playerContext.m_playingInfo)
            ret = m_playerContext.m_playingInfo->GetRecordingGroup();
        m_playerContext.UnlockPlayingInfo(__FILE__, __LINE__);
    }
    ReturnPlayerLock();
    return ret;
}

bool TV::IsSameProgram(const ProgramInfo* ProgInfo) const
{
    if (!ProgInfo)
        return false;

    bool ret = false;
    GetPlayerReadLock();
    ret = m_playerContext.IsSameProgram(*ProgInfo);
    ReturnPlayerLock();
    return ret;
}

bool TV::ContextIsPaused(const char *File, int Location)
{
    bool paused = false;
    m_playerContext.LockDeletePlayer(File, Location);
    if (m_playerContext.m_player)
        paused = m_playerContext.m_player->IsPaused();
    m_playerContext.UnlockDeletePlayer(File, Location);
    return paused;
}

OSD *TV::GetOSDL()
{
    m_playerContext.LockDeletePlayer(__FILE__, __LINE__);
    if (m_playerContext.m_player)
    {
        m_playerContext.LockOSD();
        OSD *osd = m_playerContext.m_player->GetOSD();
        if (!osd)
        {
            m_playerContext.UnlockOSD();
            m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
        }
        return osd;
    }
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
    return nullptr;
}

void TV::ReturnOSDLock()
{
    m_playerContext.UnlockOSD();
    m_playerContext.UnlockDeletePlayer(__FILE__, __LINE__);
}

void TV::GetPlayerWriteLock()
{
    m_playerLock.lockForWrite();
}

void TV::GetPlayerReadLock()
{
    m_playerLock.lockForRead();
}

void TV::GetPlayerReadLock() const
{
    m_playerLock.lockForRead();
}

void TV::ReturnPlayerLock()
{
    m_playerLock.unlock();
}

void TV::ReturnPlayerLock() const
{
    m_playerLock.unlock();
}

void TV::onApplicationStateChange(Qt::ApplicationState State)
{
    switch (State)
    {
        case Qt::ApplicationState::ApplicationSuspended:
        {
            LOG(VB_GENERAL, LOG_NOTICE, "Exiting playback on app suspecnd");
            StopPlayback();
            break;
        }
        default:
            break;
    }
}
