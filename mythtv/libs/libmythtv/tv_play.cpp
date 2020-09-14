
#include "tv_play.h"

#include <algorithm>
#include <cassert>
#include <chrono> // for milliseconds
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <thread> // for sleep_for

using namespace std;

#include <QApplication>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNode>
#include <QEvent>
#include <QFile>
#include <QKeyEvent>
#include <QRegExp>
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
#include "mythscreenstack.h"
#include "mythscreentype.h"
#include "mythuiactions.h"              // for ACTION_LEFT, ACTION_RIGHT, etc

// libmythtv
#include "DVD/dvdringbuffer.h"
#include "Bluray/bdringbuffer.h"
#include "remoteencoder.h"
#include "tvremoteutil.h"
#include "mythplayer.h"
#include "subtitlescreen.h"
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
#include "tvbrowsehelper.h"
#include "playercontext.h"              // for PlayerContext, osdInfo, etc
#include "programtypes.h"
#include "ringbuffer.h"                 // for RingBuffer, etc
#include "tv_actions.h"                 // for ACTION_TOGGLESLEEP, etc
#include "mythcodeccontext.h"

#if ! HAVE_ROUND
#define round(x) ((int) ((x) + 0.5))
#endif

#define DEBUG_CHANNEL_PREFIX 0 /**< set to 1 to debug channel prefixing */
#define DEBUG_ACTIONS        0 /**< set to 1 to debug actions           */

#define LOC      QString("TV::%1(): ").arg(__func__)

#define GetPlayer(X,Y) GetPlayerHaveLock(X, Y, __FILE__ , __LINE__)
#define GetOSDLock(X) GetOSDL(X, __FILE__, __LINE__)

#define SetOSDText(CTX, GROUP, FIELD, TEXT, TIMEOUT) { \
    OSD *osd_m = GetOSDLock(CTX); \
    if (osd_m) \
    { \
        InfoMap map; \
        map.insert(FIELD,TEXT); \
        osd_m->SetText(GROUP, map, TIMEOUT); \
    } \
    ReturnOSDLock(CTX, osd_m); }

#define SetOSDMessage(CTX, MESSAGE) \
    SetOSDText(CTX, "osd_message", "message_text", MESSAGE, kOSDTimeout_Med)

#define HideOSDWindow(CTX, WINDOW) { \
    OSD *osd = GetOSDLock(CTX); \
    if (osd) \
        osd->HideWindow(WINDOW); \
    ReturnOSDLock(CTX, osd); }

//static const QString _Location = TV::tr("TV Player");

const int  TV::kInitFFRWSpeed                = 0;
const uint TV::kInputKeysMax                 = 6;
const uint TV::kNextSource                   = 1;
const uint TV::kPreviousSource               = 2;
const uint TV::kMaxPIPCount                  = 4;
const uint TV::kMaxPBPCount                  = 2;


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
    MenuNodeTuple(void) : m_menu(dummy_menubase)
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
int TV::ConfiguredTunerCards(void)
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

static void multi_lock(QMutex *mutex0, ...)
{
    vector<QMutex*> mutex;
    mutex.push_back(mutex0);

    va_list argp;
    va_start(argp, mutex0);
    QMutex *cur = va_arg(argp, QMutex*);
    while (cur)
    {
        mutex.push_back(cur);
        cur = va_arg(argp, QMutex*);
    }
    va_end(argp);

    for (bool success = false; !success;)
    {
        success = true;
        for (uint i = 0; success && (i < mutex.size()); i++)
        {
            if (!(success = mutex[i]->tryLock()))
            {
                for (uint j = 0; j < i; j++)
                    mutex[j]->unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(25));
            }
        }
    }
}

QMutex* TV::gTVLock = new QMutex();
TV*     TV::gTV     = nullptr;

bool TV::IsTVRunning(void)
{
    QMutexLocker locker(gTVLock);
    return gTV;
}

TV* TV::GetTV(void)
{
    QMutexLocker locker(gTVLock);
    if (gTV)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Already have a TV object.");
        return nullptr;
    }
    gTV = new TV();
    return gTV;
}

void TV::ReleaseTV(TV* tv)
{
    QMutexLocker locker(gTVLock);
    if (!tv || !gTV || (gTV != tv))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "- programmer error.");
        return;
    }

    delete gTV;
    gTV = nullptr;
}

void TV::StopPlayback(void)
{
    if (TV::IsTVRunning())
    {
        QMutexLocker lock(gTVLock);

        PlayerContext *ctx = gTV->GetPlayerReadLock(0, __FILE__, __LINE__);
        PrepareToExitPlayer(ctx, __LINE__);
        SetExitPlayer(true, true);
        ReturnPlayerLock(ctx);
        gCoreContext->TVInWantingPlayback(true);
    }
}

/**
 * \brief returns true if the recording completed when exiting.
 */
bool TV::StartTV(ProgramInfo *tvrec, uint flags,
                 const ChannelInfoList &selection)
{
    TV *tv = GetTV();
    if (!tv)
    {
        gCoreContext->emitTVPlaybackAborted();
        return false;
    }

    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "-- begin");
    bool inPlaylist = (flags & kStartTVInPlayList) != 0U;
    bool initByNetworkCommand = (flags & kStartTVByNetworkCommand) != 0U;
    bool quitAll = false;
    bool showDialogs = true;
    bool playCompleted = false;
    ProgramInfo *curProgram = nullptr;
    bool startSysEventSent = false;
    bool startLivetvEventSent = false;

    if (tvrec)
    {
        curProgram = new ProgramInfo(*tvrec);
        curProgram->SetIgnoreBookmark((flags & kStartTVIgnoreBookmark) != 0U);
        curProgram->SetIgnoreProgStart((flags & kStartTVIgnoreProgStart) != 0U);
        curProgram->SetAllowLastPlayPos((flags & kStartTVAllowLastPlayPos) != 0U);
    }

    GetMythMainWindow()->PauseIdleTimer(true);

    // Initialize TV
    if (!tv->Init())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed initializing TV");
        ReleaseTV(tv);
        GetMythMainWindow()->PauseIdleTimer(false);
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
            if (!tv->LiveTV(showDialogs, selection))
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

        tv->setInPlayList(inPlaylist);
        tv->setUnderNetworkControl(initByNetworkCommand);

        gCoreContext->emitTVPlaybackStarted();

        // Process Events
        LOG(VB_GENERAL, LOG_INFO, LOC + "Entering main playback loop.");
        tv->PlaybackLoop();
        LOG(VB_GENERAL, LOG_INFO, LOC + "Exiting main playback loop.");

        if (tv->getJumpToProgram())
        {
            ProgramInfo *nextProgram = tv->GetLastProgram();

            tv->SetLastProgram(curProgram);
            delete curProgram;
            curProgram = nextProgram;

            SendMythSystemPlayEvent("PLAY_CHANGED", curProgram);
            continue;
        }

        const PlayerContext *mctx =
            tv->GetPlayerReadLock(0, __FILE__, __LINE__);
        quitAll = tv->m_wantsToQuit || (mctx && mctx->m_errored);
        if (mctx)
        {
            mctx->LockDeletePlayer(__FILE__, __LINE__);
            if (mctx->m_player && mctx->m_player->IsErrored())
                playerError = mctx->m_player->GetError();
            mctx->UnlockDeletePlayer(__FILE__, __LINE__);
        }
        tv->ReturnPlayerLock(mctx);
        quitAll |= !playerError.isEmpty();
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC + "-- process events 2 begin");
    do
        qApp->processEvents();
    while (tv->m_isEmbedded);
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "-- process events 2 end");

    // check if the show has reached the end.
    if (tvrec && tv->getEndOfRecording())
        playCompleted = true;

    bool allowrerecord = tv->getAllowRerecord();
    bool deleterecording = tv->m_requestDelete;

    ReleaseTV(tv);

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

    GetMythMainWindow()->PauseIdleTimer(false);

    if (startLivetvEventSent)
        gCoreContext->SendSystemEvent("LIVETV_ENDED");

    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "-- end");

    return playCompleted;
}

/**
 * \brief Import pointers to functions used to embed the TV
 * window into other containers e.g. playbackbox
 */
void TV::SetFuncPtr(const char *string, void *lptr)
{
    QString name(string);
    if (name == "playbackbox")
        RunPlaybackBoxPtr = (EMBEDRETURNVOID)lptr;
    else if (name == "viewscheduled")
        RunViewScheduledPtr = (EMBEDRETURNVOID)lptr;
    else if (name == "programguide")
        RunProgramGuidePtr = (EMBEDRETURNVOIDEPG)lptr;
    else if (name == "programfinder")
        RunProgramFinderPtr = (EMBEDRETURNVOIDFINDER)lptr;
    else if (name == "scheduleeditor")
        RunScheduleEditorPtr = (EMBEDRETURNVOIDSCHEDIT)lptr;
}

void TV::InitKeys(void)
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
    REG_KEY("TV Playback", "TOGGLEPIPMODE", QT_TRANSLATE_NOOP("MythControls",
            "Toggle Picture-in-Picture view"), "V");
    REG_KEY("TV Playback", "TOGGLEPBPMODE", QT_TRANSLATE_NOOP("MythControls",
            "Toggle Picture-by-Picture view"), "Ctrl+V");
    REG_KEY("TV Playback", "CREATEPIPVIEW", QT_TRANSLATE_NOOP("MythControls",
            "Create Picture-in-Picture view"), "");
    REG_KEY("TV Playback", "CREATEPBPVIEW", QT_TRANSLATE_NOOP("MythControls",
            "Create Picture-by-Picture view"), "");
    REG_KEY("TV Playback", "NEXTPIPWINDOW", QT_TRANSLATE_NOOP("MythControls",
            "Toggle active PIP/PBP window"), "B");
    REG_KEY("TV Playback", "SWAPPIP", QT_TRANSLATE_NOOP("MythControls",
            "Swap PBP/PIP Windows"), "N");
    REG_KEY("TV Playback", "TOGGLEPIPSTATE", QT_TRANSLATE_NOOP("MythControls",
            "Change PxP view"), "");
    REG_KEY("TV Playback", ACTION_BOTTOMLINEMOVE,
            QT_TRANSLATE_NOOP("MythControls", "Move BottomLine off screen"),
            "L");
    REG_KEY("TV Playback", ACTION_BOTTOMLINESAVE,
            QT_TRANSLATE_NOOP("MythControls", "Save manual zoom for BottomLine"),
            ""),
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
            QT_TRANSLATE_NOOP("MythControls", "No 3D"), "");
    REG_KEY("TV Playback", ACTION_3DSIDEBYSIDE,
            QT_TRANSLATE_NOOP("MythControls", "3D Side by Side"), "");
    REG_KEY("TV Playback", ACTION_3DSIDEBYSIDEDISCARD,
            QT_TRANSLATE_NOOP("MythControls", "Discard 3D Side by Side"), "");
    REG_KEY("TV Playback", ACTION_3DTOPANDBOTTOM,
            QT_TRANSLATE_NOOP("MythControls", "3D Top and Bottom"), "");
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

void TV::ReloadKeys(void)
{
    MythMainWindow *mainWindow = GetMythMainWindow();
    mainWindow->ClearKeyContext("TV Frontend");
    mainWindow->ClearKeyContext("TV Playback");
    mainWindow->ClearKeyContext("TV Editing");
    mainWindow->ClearKeyContext("Teletext Menu");
    InitKeys();
}

/** \fn TV::TV(void)
 *  \sa Init(void)
 */
TV::TV(void)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "Creating TV object");
    m_ctorTime.start();

    setObjectName("TV");
    m_keyRepeatTimer.start();

    m_sleepTimes.emplace_back(tr("Off",   "Sleep timer"),      0);
    m_sleepTimes.emplace_back(tr("30m",   "Sleep timer"),  30*60);
    m_sleepTimes.emplace_back(tr("1h",    "Sleep timer"),  60*60);
    m_sleepTimes.emplace_back(tr("1h30m", "Sleep timer"),  90*60);
    m_sleepTimes.emplace_back(tr("2h",    "Sleep timer"), 120*60);

    m_playerLock.lockForWrite();
    m_player.push_back(new PlayerContext(kPlayerInUseID));
    m_playerActive = 0;
    m_playerLock.unlock();

    InitFromDB();

#ifdef Q_OS_ANDROID
    connect(qApp, SIGNAL(applicationStateChanged(Qt::ApplicationState)),
            this, SLOT(onApplicationStateChange(Qt::ApplicationState)));
#endif

    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Finished creating TV object");
}

void TV::InitFromDB(void)
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

    int ff_rew_def[8] = { 3, 5, 10, 20, 30, 60, 120, 180 };
    for (size_t i = 0; i < sizeof(ff_rew_def)/sizeof(ff_rew_def[0]); i++)
        kv[QString("FFRewSpeed%1").arg(i)] = QString::number(ff_rew_def[i]);

    MythDB::getMythDB()->GetSettings(kv);

    m_screenPressKeyMapPlayback = ConvertScreenPressKeyMap(kv["PlaybackScreenPressKeyMap"]);
    m_screenPressKeyMapLiveTV = ConvertScreenPressKeyMap(kv["LiveTVScreenPressKeyMap"]);

    QString db_channel_ordering;

    // convert from minutes to ms.
    m_dbIdleTimeout        = kv["LiveTVIdleTimeout"].toInt() * 60 * 1000;
    uint db_browse_max_forward  = kv["BrowseMaxForward"].toInt() * 60;
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
    m_dbAutoexpireDefault  = record.m_autoExpire;

    if (m_dbUseChannelGroups)
    {
        m_dbChannelGroups  = ChannelGroup::GetChannelGroups();
        if (m_channelGroupId > -1)
        {
            m_channelGroupChannelList = ChannelUtil::GetChannels(
                0, true, "channum, callsign", m_channelGroupId);
            ChannelUtil::SortChannels(
                m_channelGroupChannelList, "channum", true);
        }
    }

    for (size_t i = 0; i < sizeof(ff_rew_def)/sizeof(ff_rew_def[0]); i++)
        m_ffRewSpeeds.push_back(kv[QString("FFRewSpeed%1").arg(i)].toInt());

    // process it..
    m_browseHelper = new TVBrowseHelper(
        this,
        db_browse_max_forward,   m_dbBrowseAllTuners,
        m_dbUseChannelGroups,    db_channel_ordering);

    m_vbimode = VBIMode::Parse(!feVBI.isEmpty() ? feVBI : beVBI);

    gCoreContext->addListener(this);
    gCoreContext->RegisterForPlayback(this, SLOT(StopPlayback()));

    QMutexLocker lock(&m_initFromDBLock);
    m_initFromDBDone = true;
    m_initFromDBWait.wakeAll();
}

/** \fn TV::Init(bool)
 *  \brief Performs instance initialization, returns true on success.
 *
 *  \param createWindow If true a MythDialog is created for display.
 *  \return Returns true on success, false on failure.
 */
bool TV::Init(bool createWindow)
{
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "-- begin");

    if (createWindow)
    {
        MythMainWindow *mainwindow = GetMythMainWindow();
        if (!mainwindow)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "No MythMainWindow");
            return false;
        }

        bool fullscreen = !m_dbUseGuiSizeForTv;
        m_savedGuiBounds = QRect(GetMythMainWindow()->geometry().topLeft(),
                                 GetMythMainWindow()->size());

        // adjust for window manager wierdness.
        QRect screen = GetMythUI()->GetScreenSettings();
        if ((abs(m_savedGuiBounds.x() - screen.left()) < 3) &&
            (abs(m_savedGuiBounds.y() - screen.top()) < 3))
        {
            m_savedGuiBounds = QRect(screen.topLeft(), mainwindow->size());
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
        {
            m_playerBounds = MythDisplay::AcquireRelease()->GetScreenBounds();
            MythDisplay::AcquireRelease(false);
        }

        // player window sizing
        MythScreenStack *mainStack = mainwindow->GetMainStack();

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

        if (mainwindow->GetPaintWindow())
            mainwindow->GetPaintWindow()->update();
        mainwindow->installEventFilter(this);
        qApp->processEvents();
    }

    {
        QMutexLocker locker(&m_initFromDBLock);
        while (!m_initFromDBDone)
        {
            qApp->processEvents();
            m_initFromDBWait.wait(&m_initFromDBLock, 50);
        }
    }

    PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
    mctx->m_ffRewState = 0;
    mctx->m_ffRewIndex = kInitFFRWSpeed;
    mctx->m_ffRewSpeed = 0;
    mctx->m_tsNormal   = 1.0F;
    ReturnPlayerLock(mctx);

    m_sleepIndex = 0;

    SetUpdateOSDPosition(false);

    const PlayerContext *ctx = GetPlayerReadLock(0, __FILE__, __LINE__);
    ClearInputQueues(ctx, false);
    ReturnPlayerLock(ctx);

    m_switchToRec = nullptr;
    SetExitPlayer(false, false);

    m_errorRecoveryTimerId   = StartTimer(kErrorRecoveryCheckFrequency, __LINE__);
    m_lcdTimerId             = StartTimer(1, __LINE__);
    m_speedChangeTimerId     = StartTimer(kSpeedChangeCheckFrequency, __LINE__);
    m_saveLastPlayPosTimerId = StartTimer(kSaveLastPlayPosTimeout, __LINE__);

    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "-- end");
    return true;
}

TV::~TV(void)
{
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "-- begin");

    if (m_browseHelper)
        m_browseHelper->Stop();

    gCoreContext->removeListener(this);
    gCoreContext->UnregisterForPlayback(this);

    MythMainWindow* mwnd = GetMythMainWindow();
    mwnd->removeEventFilter(this);

    if (m_weDisabledGUI)
        mwnd->PopDrawDisabled();

    if (m_myWindow)
    {
        m_myWindow->Close();
        m_myWindow = nullptr;
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC + "-- lock");

    // restore window to gui size and position
    MythDisplay* display = MythDisplay::AcquireRelease();
    if (display->UsingVideoModes())
    {
        bool hide = display->NextModeIsLarger(display->GetGUIResolution());
        if (hide)
            mwnd->hide();
        display->SwitchToGUI(true);
        if (hide)
            mwnd->Show();
    }
    MythDisplay::AcquireRelease(false);

    mwnd->MoveResize(m_savedGuiBounds);
#ifdef Q_OS_ANDROID
    mwnd->Show();
#else
    mwnd->show();
#endif

    delete m_lastProgram;

    if (LCD *lcd = LCD::Get())
    {
        lcd->setFunctionLEDs(FUNC_TV, false);
        lcd->setFunctionLEDs(FUNC_MOVIE, false);
        lcd->switchToTime();
    }

    if (m_browseHelper)
    {
        delete m_browseHelper;
        m_browseHelper = nullptr;
    }

    PlayerContext *mctx = GetPlayerWriteLock(0, __FILE__, __LINE__);
    while (!m_player.empty())
    {
        delete m_player.back();
        m_player.pop_back();
    }
    ReturnPlayerLock(mctx);

    if (m_browseHelper)
    {
        delete m_browseHelper;
        m_browseHelper = nullptr;
    }

    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "-- end");
}

/**
 * \brief The main playback loop
 */
void TV::PlaybackLoop(void)
{
    while (true)
    {
        qApp->processEvents();
        if (SignalHandler::IsExiting())
        {
            m_wantsToQuit = true;
            return;
        }

        TVState state = GetState(0);
        if ((kState_Error == state) || (kState_None == state))
            return;

        if (kState_ChangingState == state)
            continue;

        int count = m_player.size();
        int errorCount = 0;
        for (int i = 0; i < count; i++)
        {
            const PlayerContext *mctx = GetPlayerReadLock(i, __FILE__, __LINE__);
            if (mctx)
            {
                // If the master player is using hardware decoding then it may
                // callback to the UI thread at any point when creating or destroying
                // a hardware context and may be holding avcodeclock. If this happens
                // the secondary players will almost certainly block so try and
                // catch callbacks before we enter EventLoop and VideoLoop for
                // the secondary players.
                // NOTE this is not perfect. It should catch most cases but if
                // the master player calls back during EventLoop or VideoLoop
                // it may still deadlock. This needs a complete re-work of avcodeclock
                // which is now ubiquitous throughout the decoding code - and is
                // probably now used inappropriately (avcodeclock is a global static).
                if (i > 0)
                {
                    // ensure we don't introduce another lockup. If we fail to get
                    // the lock, just continue. We will miss a pip frame but that
                    // is not fatal.
                    int tries = 10;
                    while (--tries && !avcodeclock->tryLock(10))
                    {
                        PlayerContext *master = GetPlayerReadLock(0, __FILE__, __LINE__);
                        if (master && master->m_player)
                        {
                            master->LockDeletePlayer(__FILE__, __LINE__);
                            master->m_player->ProcessCallbacks();
                            master->UnlockDeletePlayer(__FILE__, __LINE__);
                        }
                        ReturnPlayerLock(master);
                    }

                    if (tries)
                    {
                        avcodeclock->unlock();
                    }
                    else
                    {
                        LOG(VB_GENERAL, LOG_INFO, LOC + "Failed to get PiP lock");
                        ReturnPlayerLock(mctx);
                        continue;
                    }
                }

                mctx->LockDeletePlayer(__FILE__, __LINE__);
                if (mctx->m_player && !mctx->m_player->IsErrored())
                {
                    mctx->m_player->EventLoop();
                    mctx->m_player->VideoLoop();
                }
                mctx->UnlockDeletePlayer(__FILE__, __LINE__);

                if (mctx->m_errored || !mctx->m_player)
                    errorCount++;
            }
            ReturnPlayerLock(mctx);
        }

        // break out of the loop if there are no valid players
        // or all PlayerContexts are errored
        if (errorCount == count)
            return;
    }
}

/**
 * \brief update the channel list with channels from the selected channel group
 */
void TV::UpdateChannelList(int groupID)
{
    if (!m_dbUseChannelGroups)
        return;

    QMutexLocker locker(&m_channelGroupLock);
    if (groupID == m_channelGroupId)
        return;

    ChannelInfoList list;
    if (groupID != -1)
    {
        list = ChannelUtil::GetChannels(
            0, true, "channum, callsign", groupID);
        ChannelUtil::SortChannels(list, "channum", true);
    }

    m_channelGroupId = groupID;
    m_channelGroupChannelList = list;

    if (m_dbRememberLastChannelGroup)
        gCoreContext->SaveSetting("ChannelGroupDefault", m_channelGroupId);
}

/**
 * \brief get tv state of active player context
 */
TVState TV::GetState(int player_idx) const
{
    const PlayerContext *ctx = GetPlayerReadLock(player_idx, __FILE__, __LINE__);
    TVState ret = GetState(ctx);
    ReturnPlayerLock(ctx);
    return ret;
}

// XXX what about subtitlezoom?
void TV::GetStatus(void)
{
    QVariantMap status;

    const PlayerContext *ctx = GetPlayerReadLock(-1, __FILE__, __LINE__);

    status.insert("state", StateToString(GetState(ctx)));
    ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (ctx->m_playingInfo)
    {
        status.insert("title", ctx->m_playingInfo->GetTitle());
        status.insert("subtitle", ctx->m_playingInfo->GetSubtitle());
        status.insert("starttime",
                           ctx->m_playingInfo->GetRecordingStartTime()
                           .toUTC().toString("yyyy-MM-ddThh:mm:ssZ"));
        status.insert("chanid",
                           QString::number(ctx->m_playingInfo->GetChanID()));
        status.insert("programid", ctx->m_playingInfo->GetProgramID());
        status.insert("pathname", ctx->m_playingInfo->GetPathname());
    }
    ctx->UnlockPlayingInfo(__FILE__, __LINE__);
    osdInfo info;
    ctx->CalcPlayerSliderPosition(info);
    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (ctx->m_player)
    {
        if (!info.text["totalchapters"].isEmpty())
        {
            QList<long long> chapters;
            ctx->m_player->GetChapterTimes(chapters);
            QVariantList var;
            foreach (long long chapter, chapters)
                var << QVariant(chapter);
            status.insert("chaptertimes", var);
        }

        uint capmode = ctx->m_player->GetCaptionMode();
        QVariantMap tracks;

        QStringList list = ctx->m_player->GetTracks(kTrackTypeSubtitle);
        int currenttrack = -1;
        if (!list.isEmpty() && (kDisplayAVSubtitle == capmode))
            currenttrack = ctx->m_player->GetTrack(kTrackTypeSubtitle);
        for (int i = 0; i < list.size(); i++)
        {
            if (i == currenttrack)
                status.insert("currentsubtitletrack", list[i]);
            tracks.insert("SELECTSUBTITLE_" + QString::number(i), list[i]);
        }

        list = ctx->m_player->GetTracks(kTrackTypeTeletextCaptions);
        currenttrack = -1;
        if (!list.isEmpty() && (kDisplayTeletextCaptions == capmode))
            currenttrack = ctx->m_player->GetTrack(kTrackTypeTeletextCaptions);
        for (int i = 0; i < list.size(); i++)
        {
            if (i == currenttrack)
                status.insert("currentsubtitletrack", list[i]);
            tracks.insert("SELECTTTC_" + QString::number(i), list[i]);
        }

        list = ctx->m_player->GetTracks(kTrackTypeCC708);
        currenttrack = -1;
        if (!list.isEmpty() && (kDisplayCC708 == capmode))
            currenttrack = ctx->m_player->GetTrack(kTrackTypeCC708);
        for (int i = 0; i < list.size(); i++)
        {
            if (i == currenttrack)
                status.insert("currentsubtitletrack", list[i]);
            tracks.insert("SELECTCC708_" + QString::number(i), list[i]);
        }

        list = ctx->m_player->GetTracks(kTrackTypeCC608);
        currenttrack = -1;
        if (!list.isEmpty() && (kDisplayCC608 == capmode))
            currenttrack = ctx->m_player->GetTrack(kTrackTypeCC608);
        for (int i = 0; i < list.size(); i++)
        {
            if (i == currenttrack)
                status.insert("currentsubtitletrack", list[i]);
            tracks.insert("SELECTCC608_" + QString::number(i), list[i]);
        }

        list = ctx->m_player->GetTracks(kTrackTypeRawText);
        currenttrack = -1;
        if (!list.isEmpty() && (kDisplayRawTextSubtitle == capmode))
            currenttrack = ctx->m_player->GetTrack(kTrackTypeRawText);
        for (int i = 0; i < list.size(); i++)
        {
            if (i == currenttrack)
                status.insert("currentsubtitletrack", list[i]);
            tracks.insert("SELECTRAWTEXT_" + QString::number(i), list[i]);
        }

        if (ctx->m_player->HasTextSubtitles())
        {
            if (kDisplayTextSubtitle == capmode)
                status.insert("currentsubtitletrack", tr("External Subtitles"));
            tracks.insert(ACTION_ENABLEEXTTEXT, tr("External Subtitles"));
        }

        status.insert("totalsubtitletracks", tracks.size());
        if (!tracks.isEmpty())
            status.insert("subtitletracks", tracks);

        tracks.clear();
        list = ctx->m_player->GetTracks(kTrackTypeAudio);
        currenttrack = ctx->m_player->GetTrack(kTrackTypeAudio);
        for (int i = 0; i < list.size(); i++)
        {
            if (i == currenttrack)
                status.insert("currentaudiotrack", list[i]);
            tracks.insert("SELECTAUDIO_" + QString::number(i), list[i]);
        }

        status.insert("totalaudiotracks", tracks.size());
        if (!tracks.isEmpty())
            status.insert("audiotracks", tracks);

        status.insert("playspeed", ctx->m_player->GetPlaySpeed());
        status.insert("audiosyncoffset", (long long)ctx->m_player->GetAudioTimecodeOffset());
        if (ctx->m_player->GetAudio()->ControlsVolume())
        {
            status.insert("volume", ctx->m_player->GetVolume());
            status.insert("mute",   ctx->m_player->GetMuteState());
        }
        if (ctx->m_player->GetVideoOutput())
        {
            MythVideoOutput *vo = ctx->m_player->GetVideoOutput();
            PictureAttributeSupported supp =
                    vo->GetSupportedPictureAttributes();
            if (supp & kPictureAttributeSupported_Brightness)
            {
                status.insert("brightness",
                  vo->GetPictureAttribute(kPictureAttribute_Brightness));
            }
            if (supp & kPictureAttributeSupported_Contrast)
            {
                status.insert("contrast",
                  vo->GetPictureAttribute(kPictureAttribute_Contrast));
            }
            if (supp & kPictureAttributeSupported_Colour)
            {
                status.insert("colour",
                  vo->GetPictureAttribute(kPictureAttribute_Colour));
            }
            if (supp & kPictureAttributeSupported_Hue)
            {
                status.insert("hue",
                  vo->GetPictureAttribute(kPictureAttribute_Hue));
            }
        }
    }
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);

    ReturnPlayerLock(ctx);

    for (auto tit =info.text.cbegin(); tit != info.text.cend(); ++tit)
    {
        status.insert(tit.key(), tit.value());
    }

    QHashIterator<QString,int> vit(info.values);
    while (vit.hasNext())
    {
        vit.next();
        status.insert(vit.key(), vit.value());
    }

    MythUIStateTracker::SetState(status);
}

/**
 * \brief get tv state of active player context
 */
TVState TV::GetState(const PlayerContext *actx)
{
    TVState ret = kState_ChangingState;
    if (!actx->InStateChange())
        ret = actx->GetState();
    return ret;
}

/**
 *  \brief Starts LiveTV
 *  \param showDialogs if true error dialogs are shown, if false they are not
 *  \param selection What channel to tune.
 */
bool TV::LiveTV(bool showDialogs, const ChannelInfoList &selection)
{
    m_requestDelete = false;
    m_allowRerecord = false;
    m_jumpToProgram = false;

    PlayerContext *actx = GetPlayerReadLock(-1, __FILE__, __LINE__);
    if (actx->GetState() == kState_None &&
        RequestNextRecorder(actx, showDialogs, selection))
    {
        actx->SetInitialTVState(true);
        HandleStateChange(actx, actx);
        m_switchToRec = nullptr;

        // Start Idle Timer
        if (m_dbIdleTimeout > 0)
        {
            m_idleTimerId = StartTimer(m_dbIdleTimeout, __LINE__);
            LOG(VB_GENERAL, LOG_INFO, QString("Using Idle Timer. %1 minutes")
                    .arg(m_dbIdleTimeout*(1.0F/60000.0F)));
        }

        ReturnPlayerLock(actx);
        return true;
    }
    ReturnPlayerLock(actx);
    return false;
}

int TV::GetLastRecorderNum(int player_idx) const
{
    const PlayerContext *ctx = GetPlayerReadLock(player_idx, __FILE__, __LINE__);
    int ret = ctx->GetCardID();
    ReturnPlayerLock(ctx);
    return ret;
}

bool TV::RequestNextRecorder(PlayerContext *ctx, bool showDialogs,
                             const ChannelInfoList &selection)
{
    if (!ctx)
        return false;

    ctx->SetRecorder(nullptr);

    RemoteEncoder *testrec = nullptr;
    if (m_switchToRec)
    {
        // If this is set we, already got a new recorder in SwitchCards()
        testrec = m_switchToRec;
        m_switchToRec = nullptr;
    }
    else if (!selection.empty())
    {
        for (const auto & ci : selection)
        {
            uint    chanid  = ci.m_chanId;
            QString channum = ci.m_chanNum;
            if (!chanid || channum.isEmpty())
                continue;
            QSet<uint> cards = IsTunableOn(ctx, chanid);

            if (chanid && !channum.isEmpty() && !cards.isEmpty())
            {
                testrec = RemoteGetExistingRecorder(*(cards.begin()));
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
        if (showDialogs)
            ShowNoRecorderDialog(ctx);

        delete testrec;

        return false;
    }

    ctx->SetRecorder(testrec);

    return true;
}

void TV::FinishRecording(int player_ctx)
{
    PlayerContext *ctx = GetPlayerReadLock(player_ctx, __FILE__, __LINE__);
    if (StateIsRecording(GetState(ctx)) && ctx->m_recorder)
        ctx->m_recorder->FinishRecording();
    ReturnPlayerLock(ctx);
}

void TV::AskAllowRecording(PlayerContext *ctx,
                           const QStringList &msg, int timeuntil,
                           bool hasrec, bool haslater)
{
#if 0
    LOG(VB_GENERAL, LOG_DEBUG, LOC + "AskAllowRecording");
#endif
    if (!StateIsLiveTV(GetState(ctx)))
       return;

    auto *info = new ProgramInfo(msg);
    if (!info->GetChanID())
    {
        delete info;
        return;
    }

    QMutexLocker locker(&m_askAllowLock);
    QString key = info->MakeUniqueKey();
    if (timeuntil > 0)
    {
        // add program to list
#if 0
        LOG(VB_GENERAL, LOG_DEBUG, LOC + "AskAllowRecording -- " +
            QString("adding '%1'").arg(info->m_title));
#endif
        QDateTime expiry = MythDate::current().addSecs(timeuntil);
        m_askAllowPrograms[key] = AskProgramInfo(expiry, hasrec, haslater, info);
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

    ShowOSDAskAllow(ctx);
}

void TV::ShowOSDAskAllow(PlayerContext *ctx)
{
    QMutexLocker locker(&m_askAllowLock);
    if (!ctx->m_recorder)
        return;

    uint cardid = ctx->GetCardID();

    QString single_rec =
        tr("MythTV wants to record \"%1\" on %2 in %d seconds. "
           "Do you want to:");

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
    uint conflict_count = m_askAllowPrograms.size();

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

        OSD *osd = GetOSDLock(ctx);
        if (osd)
        {
            m_browseHelper->BrowseEnd(ctx, false);
            timeuntil = MythDate::current().secsTo((*it).m_expiry) * 1000;
            osd->DialogShow(OSD_DLG_ASKALLOW, message, timeuntil);
            osd->DialogAddButton(record_watch, "DIALOG_ASKALLOW_WATCH_0",
                                 false, !((*it).m_hasRec));
            osd->DialogAddButton(let_record1, "DIALOG_ASKALLOW_EXIT_0");
            osd->DialogAddButton(((*it).m_hasLater) ? record_later1 : do_not_record1,
                                 "DIALOG_ASKALLOW_CANCELRECORDING_0",
                                 false, ((*it).m_hasRec));
        }
        ReturnOSDLock(ctx, osd);
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
                int tmp = MythDate::current().secsTo((*it).m_expiry);
                tmp *= 1000;
                timeuntil = min(timeuntil, max(tmp, 0));
            }
        }
        timeuntil = (9999999 == timeuntil) ? 0 : timeuntil;

        OSD *osd = GetOSDLock(ctx);
        if (osd && conflict_count > 1)
        {
            m_browseHelper->BrowseEnd(ctx, false);
            osd->DialogShow(OSD_DLG_ASKALLOW, message, timeuntil);
            osd->DialogAddButton(let_recordm, "DIALOG_ASKALLOW_EXIT_0",
                                 false, true);
            osd->DialogAddButton((all_have_later) ? record_laterm : do_not_recordm,
                                 "DIALOG_ASKALLOW_CANCELCONFLICTING_0");
        }
        else if (osd)
        {
            m_browseHelper->BrowseEnd(ctx, false);
            osd->DialogShow(OSD_DLG_ASKALLOW, message, timeuntil);
            osd->DialogAddButton(let_record1, "DIALOG_ASKALLOW_EXIT_0",
                                 false, !has_rec);
            osd->DialogAddButton((all_have_later) ? record_later1 : do_not_record1,
                                 "DIALOG_ASKALLOW_CANCELRECORDING_0",
                                 false, has_rec);
        }
        ReturnOSDLock(ctx, osd);
    }
}

void TV::HandleOSDAskAllow(PlayerContext *ctx, const QString& action)
{
    if (!DialogIsVisible(ctx, OSD_DLG_ASKALLOW))
        return;

    if (!m_askAllowLock.tryLock())
    {
        LOG(VB_GENERAL, LOG_ERR, "allowrecordingbox : askAllowLock is locked");
        return;
    }

    if (action == "CANCELRECORDING")
    {
        if (ctx->m_recorder)
            ctx->m_recorder->CancelNextRecording(true);
    }
    else if (action == "CANCELCONFLICTING")
    {
        foreach (auto & pgm, m_askAllowPrograms)
        {
            if (pgm.m_isConflicting)
                RemoteCancelNextRecording(pgm.m_info->GetInputID(), true);
        }
    }
    else if (action == "WATCH")
    {
        if (ctx->m_recorder)
            ctx->m_recorder->CancelNextRecording(false);
    }
    else // if (action == "EXIT")
    {
        PrepareToExitPlayer(ctx, __LINE__);
        SetExitPlayer(true, true);
    }

    m_askAllowLock.unlock();
}

int TV::Playback(const ProgramInfo &rcinfo)
{
    m_wantsToQuit   = false;
    m_jumpToProgram = false;
    m_allowRerecord = false;
    m_requestDelete = false;
    gCoreContext->TVInWantingPlayback(false);

    PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
    if (mctx->GetState() != kState_None)
    {
        ReturnPlayerLock(mctx);
        return 0;
    }

    mctx->SetPlayingInfo(&rcinfo);
    mctx->SetInitialTVState(false);
    HandleStateChange(mctx, mctx);

    ReturnPlayerLock(mctx);

    if (LCD *lcd = LCD::Get())
    {
        lcd->switchToChannel(rcinfo.GetChannelSchedulingID(),
                             rcinfo.GetTitle(), rcinfo.GetSubtitle());
        lcd->setFunctionLEDs((rcinfo.IsRecording())?FUNC_TV:FUNC_MOVIE, true);
    }

    return 1;
}

bool TV::StateIsRecording(TVState state)
{
    return (state == kState_RecordingOnly ||
            state == kState_WatchingRecording);
}

bool TV::StateIsPlaying(TVState state)
{
    return (state == kState_WatchingPreRecorded ||
            state == kState_WatchingRecording   ||
            state == kState_WatchingVideo       ||
            state == kState_WatchingDVD         ||
            state == kState_WatchingBD);
}

bool TV::StateIsLiveTV(TVState state)
{
    return (state == kState_WatchingLiveTV);
}

TVState TV::RemoveRecording(TVState state)
{
    if (StateIsRecording(state))
    {
        if (state == kState_RecordingOnly)
            return kState_None;
        return kState_WatchingPreRecorded;
    }
    return kState_Error;
}

#define TRANSITION(ASTATE,BSTATE) \
   ((ctxState == (ASTATE)) && (desiredNextState == (BSTATE)))

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
void TV::HandleStateChange(PlayerContext *mctx, PlayerContext *ctx)
{
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("(%1) -- begin")
            .arg(find_player_index(ctx)));

    if (!ctx)   // can never happen, but keep coverity happy
        return;

    if (ctx->IsErrored())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Called after fatal error detected.");
        return;
    }

    bool changed = false;

    ctx->LockState();
    TVState nextState = ctx->GetState();
    if (ctx->m_nextState.empty())
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Warning, called with no state to change to.");
        ctx->UnlockState();
        return;
    }

    TVState ctxState = ctx->GetState();
    TVState desiredNextState = ctx->DequeueNextState();

    LOG(VB_GENERAL, LOG_INFO, LOC +
        QString("Attempting to change from %1 to %2")
            .arg(StateToString(nextState))
            .arg(StateToString(desiredNextState)));

    if (desiredNextState == kState_Error)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Attempting to set to an error state!");
        SetErrored(ctx);
        ctx->UnlockState();
        return;
    }

    bool ok = false;
    if (TRANSITION(kState_None, kState_WatchingLiveTV))
    {
        ctx->m_lastSignalUIInfo.clear();

        ctx->m_recorder->Setup();

        QDateTime timerOffTime = MythDate::current();
        m_lockTimerOn = false;

        SET_NEXT();

        uint chanid = m_initialChanID;
        if (!chanid)
            chanid = gCoreContext->GetNumSetting("DefaultChanid", 0);

        if (chanid && !IsTunable(ctx, chanid))
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

            bool getit = ctx->m_recorder->ShouldSwitchToAnotherCard(
                QString::number(chanid));

            if (getit)
                reclist = ChannelUtil::GetValidRecorderList(chanid, channum);

            if (!reclist.empty())
            {
                RemoteEncoder *testrec = RemoteRequestFreeRecorderFromList(reclist, 0);
                if (testrec && testrec->IsValidRecorder())
                {
                    ctx->SetRecorder(testrec);
                    ctx->m_recorder->Setup();
                }
                else
                    delete testrec; // If testrec isn't a valid recorder ...
            }
            else if (getit)
                chanid = 0;
        }

        LOG(VB_GENERAL, LOG_DEBUG, LOC + "Spawning LiveTV Recorder -- begin");

        if (chanid && !channum.isEmpty())
            ctx->m_recorder->SpawnLiveTV(ctx->m_tvchain->GetID(), false, channum);
        else
            ctx->m_recorder->SpawnLiveTV(ctx->m_tvchain->GetID(), false, "");

        LOG(VB_GENERAL, LOG_DEBUG, LOC + "Spawning LiveTV Recorder -- end");

        if (!ctx->ReloadTVChain())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                    "LiveTV not successfully started");
            RestoreScreenSaver(ctx);
            ctx->SetRecorder(nullptr);
            SetErrored(ctx);
            SET_LAST();
        }
        else
        {
            ctx->LockPlayingInfo(__FILE__, __LINE__);
            QString playbackURL = ctx->m_playingInfo->GetPlaybackURL(true);
            ctx->UnlockPlayingInfo(__FILE__, __LINE__);

            bool opennow = (ctx->m_tvchain->GetInputType(-1) != "DUMMY");

            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("playbackURL(%1) inputtype(%2)")
                    .arg(playbackURL).arg(ctx->m_tvchain->GetInputType(-1)));

            ctx->SetRingBuffer(
                RingBuffer::Create(
                    playbackURL, false, true,
                    opennow ? RingBuffer::kLiveTVOpenTimeout : -1));

            if (ctx->m_buffer)
                ctx->m_buffer->SetLiveMode(ctx->m_tvchain);
        }


        if (ctx->m_playingInfo && StartRecorder(ctx,-1))
        {
            ok = StartPlayer(mctx, ctx, desiredNextState);
        }
        if (!ok)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "LiveTV not successfully started");
            RestoreScreenSaver(ctx);
            ctx->SetRecorder(nullptr);
            SetErrored(ctx);
            SET_LAST();
        }
        else if (!ctx->IsPIP())
        {
            if (!m_lastLockSeenTime.isValid() ||
                (m_lastLockSeenTime < timerOffTime))
            {
                m_lockTimer.start();
                m_lockTimerOn = true;
            }
        }

        if (mctx != ctx)
            SetActive(ctx, find_player_index(ctx), false);
    }
    else if (TRANSITION(kState_WatchingLiveTV, kState_None))
    {
        SET_NEXT();
        RestoreScreenSaver(ctx);
        StopStuff(mctx, ctx, true, true, true);

        if ((mctx != ctx) && (GetPlayer(ctx,-1) == ctx))
            SetActive(mctx, 0, true);
    }
    else if (TRANSITION(kState_None, kState_WatchingPreRecorded) ||
             TRANSITION(kState_None, kState_WatchingVideo) ||
             TRANSITION(kState_None, kState_WatchingDVD)   ||
             TRANSITION(kState_None, kState_WatchingBD)    ||
             TRANSITION(kState_None, kState_WatchingRecording))
    {
        ctx->LockPlayingInfo(__FILE__, __LINE__);
        QString playbackURL = ctx->m_playingInfo->GetPlaybackURL(true);
        ctx->UnlockPlayingInfo(__FILE__, __LINE__);

        RingBuffer *buffer = RingBuffer::Create(playbackURL, false);
        if (buffer && !buffer->GetLastError().isEmpty())
        {
            ShowNotificationError(tr("Can't start playback"),
                                  TV::tr( "TV Player" ), buffer->GetLastError());
            delete buffer;
            buffer = nullptr;
        }
        ctx->SetRingBuffer(buffer);

        if (ctx->m_buffer && ctx->m_buffer->IsOpen())
        {
            if (desiredNextState == kState_WatchingRecording)
            {
                ctx->LockPlayingInfo(__FILE__, __LINE__);
                RemoteEncoder *rec = RemoteGetExistingRecorder(
                    ctx->m_playingInfo);
                ctx->UnlockPlayingInfo(__FILE__, __LINE__);

                ctx->SetRecorder(rec);

                if (!ctx->m_recorder ||
                    !ctx->m_recorder->IsValidRecorder())
                {
                    LOG(VB_GENERAL, LOG_ERR, LOC +
                        "Couldn't find recorder for in-progress recording");
                    desiredNextState = kState_WatchingPreRecorded;
                    ctx->SetRecorder(nullptr);
                }
                else
                {
                    ctx->m_recorder->Setup();
                }
            }

            ok = StartPlayer(mctx, ctx, desiredNextState);

            if (ok)
            {
                SET_NEXT();

                ctx->LockPlayingInfo(__FILE__, __LINE__);
                if (ctx->m_playingInfo->IsRecording())
                {
                    QString message = "COMMFLAG_REQUEST ";
                    message += ctx->m_playingInfo->MakeUniqueKey();
                    gCoreContext->SendMessage(message);
                }
                ctx->UnlockPlayingInfo(__FILE__, __LINE__);
            }
        }

        if (!ok)
        {
            SET_LAST();
            SetErrored(ctx);
            if (ctx->IsPlayerErrored())
            {
                ShowNotificationError(ctx->m_player->GetError(),
                                      TV::tr( "TV Player" ),
                                      playbackURL);
                // We're going to display this error as notification
                // no need to display it later as popup
                ctx->m_player->ResetErrored();
            }
        }
        else if (mctx != ctx)
        {
            SetActive(ctx, find_player_index(ctx), false);
        }
    }
    else if (TRANSITION(kState_WatchingPreRecorded, kState_None) ||
             TRANSITION(kState_WatchingVideo, kState_None)       ||
             TRANSITION(kState_WatchingDVD, kState_None)         ||
             TRANSITION(kState_WatchingBD, kState_None)          ||
             TRANSITION(kState_WatchingRecording, kState_None))
    {
        SET_NEXT();

        RestoreScreenSaver(ctx);
        StopStuff(mctx, ctx, true, true, false);

        if ((mctx != ctx) && (GetPlayer(ctx,-1) == ctx))
            SetActive(mctx, 0, true);
    }
    else if (TRANSITION(kState_WatchingRecording, kState_WatchingPreRecorded) ||
             TRANSITION(kState_None, kState_None))
    {
        SET_NEXT();
    }

    // Print state changed message...
    if (!changed)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Unknown state transition: %1 to %2")
                .arg(StateToString(ctx->GetState()))
                .arg(StateToString(desiredNextState)));
    }
    else if (ctx->GetState() != nextState)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Changing from %1 to %2")
                .arg(StateToString(ctx->GetState()))
                .arg(StateToString(nextState)));
    }

    // update internal state variable
    TVState lastState = ctx->GetState();
    ctx->m_playingState = nextState;
    ctx->UnlockState();

    if (mctx == ctx)
    {
        if (StateIsLiveTV(ctx->GetState()))
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + "State is LiveTV & mctx == ctx");
            UpdateOSDInput(ctx);
            LOG(VB_GENERAL, LOG_INFO, LOC + "UpdateOSDInput done");
            UpdateLCD();
            LOG(VB_GENERAL, LOG_INFO, LOC + "UpdateLCD done");
            ITVRestart(ctx, true);
            LOG(VB_GENERAL, LOG_INFO, LOC + "ITVRestart done");
        }
        else if (StateIsPlaying(ctx->GetState()) && lastState == kState_None)
        {
            ctx->LockPlayingInfo(__FILE__, __LINE__);
            int count = PlayGroup::GetCount();
            QString msg = tr("%1 Settings")
                    .arg(tv_i18n(ctx->m_playingInfo->GetPlaybackGroup()));
            ctx->UnlockPlayingInfo(__FILE__, __LINE__);
            if (count > 0)
                SetOSDMessage(ctx, msg);
            ITVRestart(ctx, false);
        }

        if (ctx->m_buffer && ctx->m_buffer->IsDVD())
        {
            UpdateLCD();
        }

        if (ctx->m_recorder)
            ctx->m_recorder->FrontendReady();

        QMutexLocker locker(&m_timerIdLock);
        if (m_endOfRecPromptTimerId)
            KillTimer(m_endOfRecPromptTimerId);
        m_endOfRecPromptTimerId = 0;
        if (m_dbEndOfRecExitPrompt && !m_inPlaylist && !m_underNetworkControl)
        {
            m_endOfRecPromptTimerId =
                StartTimer(kEndOfRecPromptCheckFrequency, __LINE__);
        }

        if (m_endOfPlaybackTimerId)
            KillTimer(m_endOfPlaybackTimerId);
        m_endOfPlaybackTimerId = 0;

        if (StateIsPlaying(ctx->GetState()))
        {
            m_endOfPlaybackTimerId =
                StartTimer(kEndOfPlaybackFirstCheckTimer, __LINE__);

        }

    }

    if (TRANSITION(kState_None, kState_WatchingPreRecorded) ||
             TRANSITION(kState_None, kState_WatchingVideo) ||
             TRANSITION(kState_None, kState_WatchingDVD)   ||
             TRANSITION(kState_None, kState_WatchingBD)    ||
             TRANSITION(kState_None, kState_WatchingRecording) ||
             TRANSITION(kState_None, kState_WatchingLiveTV))
    {
        if (!ctx->IsPIP())
            MythUIHelper::DisableScreensaver();
        // m_playerBounds is not applicable when switching modes so
        // skip this logic in that case.
        if (!m_dbUseVideoModes)
            GetMythMainWindow()->MoveResize(m_playerBounds);

        if (!m_weDisabledGUI)
        {
            m_weDisabledGUI = true;
            GetMythMainWindow()->PushDrawDisabled();
        }
        // we no longer need the contents of myWindow
        if (m_myWindow)
            m_myWindow->DeleteAllChildren();

        LOG(VB_GENERAL, LOG_INFO, LOC + "Main UI disabled.");
    }

    LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
        QString("(%1) -- end")
            .arg(find_player_index(ctx)));
}
#undef TRANSITION
#undef SET_NEXT
#undef SET_LAST

/**
 *  \brief Starts recorder, must be called before StartPlayer().
 *  \param ctx The player context requesting recording.
 *  \param maxWait How long to wait for RecorderBase to start recording. If
 *                 not provided, this defaults to 40 seconds.
 *  \return true when successful, false otherwise.
 */
bool TV::StartRecorder(PlayerContext *ctx, int maxWait)
{
    RemoteEncoder *rec = ctx->m_recorder;
    maxWait = (maxWait <= 0) ? 40000 : maxWait;
    MythTimer t;
    t.start();
    bool recording = false;
    bool ok = true;
    if (!rec) {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Invalid Remote Encoder");
        SetErrored(ctx);
        return false;
    }
    while (!(recording = rec->IsRecording(&ok)) &&
           !m_exitPlayerTimerId && t.elapsed() < maxWait)
    {
        if (!ok)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Lost contact with backend");
            SetErrored(ctx);
            return false;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(5));
    }

    if (!recording || m_exitPlayerTimerId)
    {
        if (!m_exitPlayerTimerId)
            LOG(VB_GENERAL, LOG_ERR, LOC +
                                "Timed out waiting for recorder to start");
        return false;
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("Took %1 ms to start recorder.")
            .arg(t.elapsed()));

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
 *  \param mctx           Master player context
 *  \param ctx            Current player context
 *  \param stopRingBuffer Set to true if ringbuffer must be shut down.
 *  \param stopPlayer     Set to true if player must be shut down.
 *  \param stopRecorder   Set to true if recorder must be shut down.
 */
void TV::StopStuff(PlayerContext *mctx, PlayerContext *ctx,
                   bool stopRingBuffer, bool stopPlayer, bool stopRecorder)
{
    LOG(VB_PLAYBACK, LOG_DEBUG,
        LOC + QString("For player ctx %1 -- begin")
            .arg(find_player_index(ctx)));

    emit PlaybackExiting(this);
    m_isEmbedded = false;

    SetActive(mctx, 0, false);

    if (ctx->m_buffer)
        ctx->m_buffer->IgnoreWaitStates(true);

    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (stopPlayer)
        ctx->StopPlaying();
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);

    if (stopRingBuffer)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Stopping ring buffer");
        if (ctx->m_buffer)
        {
            ctx->m_buffer->StopReads();
            ctx->m_buffer->Pause();
            ctx->m_buffer->WaitForPause();
        }
    }

    if (stopPlayer)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Stopping player");
        if (ctx == mctx)
        {
            for (uint i = 1; mctx && (i < m_player.size()); i++)
                StopStuff(mctx, GetPlayer(mctx,i), true, true, true);
        }
    }

    if (stopRecorder)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "stopping recorder");
        if (ctx->m_recorder)
            ctx->m_recorder->StopLiveTV();
    }

    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "-- end");
}

void TV::TeardownPlayer(PlayerContext *mctx, PlayerContext *ctx)
{
    int ctx_index = find_player_index(ctx);

    QString loc = LOC + QString("player ctx %1")
        .arg(ctx_index);

    if (!mctx || !ctx || ctx_index < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, loc + "-- error");
        return;
    }

    LOG(VB_PLAYBACK, LOG_INFO, loc);

    if (mctx != ctx)
    {
        if (ctx->HasPlayer())
        {
            PIPRemovePlayer(mctx, ctx);
            ctx->SetPlayer(nullptr);
        }

        m_player.erase(m_player.begin() + ctx_index);
        delete ctx;
        if (mctx->IsPBP())
            PBPRestartMainPlayer(mctx);
        SetActive(mctx, m_playerActive, false);
        return;
    }

    ctx->TeardownPlayer();
}

void TV::timerEvent(QTimerEvent *te)
{
    const int timer_id = te->timerId();

    PlayerContext *mctx2 = GetPlayerReadLock(0, __FILE__, __LINE__);
    if (mctx2->IsErrored())
    {
        ReturnPlayerLock(mctx2);
        return;
    }
    ReturnPlayerLock(mctx2);

    bool ignore = false;
    {
        QMutexLocker locker(&m_timerIdLock);
        ignore =
            (!m_stateChangeTimerId.empty() &&
             m_stateChangeTimerId.find(timer_id) == m_stateChangeTimerId.end());
    }
    if (ignore)
        return; // Always handle state changes first...

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
    else if (timer_id == m_pipChangeTimerId)
        HandlePxPTimerEvent();
    else if (timer_id == m_saveLastPlayPosTimerId)
        HandleSaveLastPlayPosEvent();
    else
        handled = false;

    if (handled)
        return;

    // Check if it matches a stateChangeTimerId
    PlayerContext *ctx = nullptr;
    {
        QMutexLocker locker(&m_timerIdLock);
        TimerContextMap::iterator it = m_stateChangeTimerId.find(timer_id);
        if (it != m_stateChangeTimerId.end())
        {
            KillTimer(timer_id);
            ctx = *it;
            m_stateChangeTimerId.erase(it);
        }
    }

    if (ctx)
    {
        PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
        bool still_exists = find_player_index(ctx) >= 0;

        while (still_exists && !ctx->m_nextState.empty())
        {
            HandleStateChange(mctx, ctx);
            if ((kState_None  == ctx->GetState() ||
                 kState_Error == ctx->GetState()) &&
                ((mctx != ctx) || m_jumpToProgram))
            {
                ReturnPlayerLock(mctx);
                mctx = GetPlayerWriteLock(0, __FILE__, __LINE__);
                TeardownPlayer(mctx, ctx);
                still_exists = false;
            }
        }
        ReturnPlayerLock(mctx);
        handled = true;
    }

    if (handled)
        return;

    // Check if it matches a signalMonitorTimerId
    ctx = nullptr;
    {
        QMutexLocker locker(&m_timerIdLock);
        TimerContextMap::iterator it = m_signalMonitorTimerId.find(timer_id);
        if (it != m_signalMonitorTimerId.end())
        {
            KillTimer(timer_id);
            ctx = *it;
            m_signalMonitorTimerId.erase(it);
        }
    }

    if (ctx)
    {
        PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
        bool still_exists = find_player_index(ctx) >= 0;

        if (still_exists && !ctx->m_lastSignalMsg.empty())
        {   // set last signal msg, so we get some feedback...
            UpdateOSDSignal(ctx, ctx->m_lastSignalMsg);
            ctx->m_lastSignalMsg.clear();
        }
        UpdateOSDTimeoutMessage(ctx);

        ReturnPlayerLock(mctx);
        handled = true;
    }

    if (handled)
        return;

    // Check if it matches networkControlTimerId
    QString netCmd;
    {
        QMutexLocker locker(&m_timerIdLock);
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
    }

    if (!netCmd.isEmpty())
    {
        PlayerContext *actx = GetPlayerReadLock(-1, __FILE__, __LINE__);
        ProcessNetworkControlCommand(actx, netCmd);
        ReturnPlayerLock(actx);
        handled = true;
    }

    if (handled)
        return;

    // Check if it matches exitPlayerTimerId
    if (timer_id == m_exitPlayerTimerId)
    {
        PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);

        OSD *osd = GetOSDLock(mctx);
        if (osd)
        {
            osd->DialogQuit();
            osd->HideAll();
        }
        ReturnOSDLock(mctx, osd);

        if (m_jumpToProgram && m_lastProgram)
        {
            if (!m_lastProgram->IsFileReadable())
            {
                SetOSDMessage(mctx, tr("Last Program: %1 Doesn't Exist")
                                        .arg(m_lastProgram->GetTitle()));
                lastProgramStringList.clear();
                SetLastProgram(nullptr);
                LOG(VB_PLAYBACK, LOG_ERR, LOC +
                    "Last Program File does not exist");
                m_jumpToProgram = false;
            }
            else
                ForceNextStateNone(mctx);
        }
        else
            ForceNextStateNone(mctx);

        ReturnPlayerLock(mctx);

        QMutexLocker locker(&m_timerIdLock);
        KillTimer(m_exitPlayerTimerId);
        m_exitPlayerTimerId = 0;
        handled = true;
    }

    if (handled)
        return;

    if (timer_id == m_jumpMenuTimerId)
    {
        PlayerContext *actx = GetPlayerReadLock(-1, __FILE__, __LINE__);
        if (actx)
            FillOSDMenuJumpRec(actx);
        ReturnPlayerLock(actx);

        QMutexLocker locker(&m_timerIdLock);
        KillTimer(m_jumpMenuTimerId);
        m_jumpMenuTimerId = 0;
        handled = true;
    }

    if (handled)
        return;

    if (timer_id == m_switchToInputTimerId)
    {
        PlayerContext *actx = GetPlayerReadLock(-1, __FILE__, __LINE__);
        if (m_switchToInputId)
        {
            uint tmp = m_switchToInputId;
            m_switchToInputId = 0;
            SwitchInputs(actx, 0, QString(), tmp);
        }
        ReturnPlayerLock(actx);

        QMutexLocker locker(&m_timerIdLock);
        KillTimer(m_switchToInputTimerId);
        m_switchToInputTimerId = 0;
        handled = true;
    }

    if (handled)
        return;

    if (timer_id == m_ccInputTimerId)
    {
        PlayerContext *actx = GetPlayerReadLock(-1, __FILE__, __LINE__);
        // Clear closed caption input mode when timer expires
        if (m_ccInputMode)
        {
            m_ccInputMode = false;
            ClearInputQueues(actx, true);
        }
        ReturnPlayerLock(actx);

        QMutexLocker locker(&m_timerIdLock);
        KillTimer(m_ccInputTimerId);
        m_ccInputTimerId = 0;
        handled = true;
    }

    if (handled)
        return;

    if (timer_id == m_asInputTimerId)
    {
        PlayerContext *actx = GetPlayerReadLock(-1, __FILE__, __LINE__);
        // Clear closed caption input mode when timer expires
        if (m_asInputMode)
        {
            m_asInputMode = false;
            ClearInputQueues(actx, true);
        }
        ReturnPlayerLock(actx);

        QMutexLocker locker(&m_timerIdLock);
        KillTimer(m_asInputTimerId);
        m_asInputTimerId = 0;
        handled = true;
    }

    if (handled)
        return;

    if (timer_id == m_queueInputTimerId)
    {
        PlayerContext *actx = GetPlayerReadLock(-1, __FILE__, __LINE__);
        // Commit input when the OSD fades away
        if (HasQueuedChannel())
        {
            OSD *osd = GetOSDLock(actx);
            if (osd && !osd->IsWindowVisible("osd_input"))
            {
                ReturnOSDLock(actx, osd);
                CommitQueuedInput(actx);
            }
            else
                ReturnOSDLock(actx, osd);
        }
        ReturnPlayerLock(actx);

        QMutexLocker locker(&m_timerIdLock);
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
        PlayerContext *actx = GetPlayerReadLock(-1, __FILE__, __LINE__);
        m_browseHelper->BrowseEnd(actx, false);
        ReturnPlayerLock(actx);
        handled = true;
    }

    if (handled)
        return;

    if (timer_id == m_updateOSDDebugTimerId)
    {
        bool update = false;
        PlayerContext *actx = GetPlayerReadLock(-1, __FILE__, __LINE__);
        OSD *osd = GetOSDLock(actx);
        if (osd && osd->IsWindowVisible("osd_debug") &&
            (StateIsLiveTV(actx->GetState()) ||
             StateIsPlaying(actx->GetState())))
        {
            update = true;
        }
        else
        {
            QMutexLocker locker(&m_timerIdLock);
            KillTimer(m_updateOSDDebugTimerId);
            m_updateOSDDebugTimerId = 0;
            if (actx->m_buffer)
                actx->m_buffer->EnableBitrateMonitor(false);
            if (actx->m_player)
                actx->m_player->EnableFrameRateMonitor(false);
        }
        ReturnOSDLock(actx, osd);
        if (update)
            UpdateOSDDebug(actx);
        ReturnPlayerLock(actx);
        handled = true;
    }
    if (timer_id == m_updateOSDPosTimerId)
    {
        PlayerContext *actx = GetPlayerReadLock(-1, __FILE__, __LINE__);
        OSD *osd = GetOSDLock(actx);
        if (osd && osd->IsWindowVisible("osd_status") &&
            (StateIsLiveTV(actx->GetState()) ||
             StateIsPlaying(actx->GetState())))
        {
            osdInfo info;
            if (actx->CalcPlayerSliderPosition(info))
            {
                osd->SetText("osd_status", info.text, kOSDTimeout_Ignore);
                osd->SetValues("osd_status", info.values, kOSDTimeout_Ignore);
            }
        }
        else
            SetUpdateOSDPosition(false);
        ReturnOSDLock(actx, osd);
        ReturnPlayerLock(actx);
        handled = true;
    }

    if (handled)
        return;

    if (timer_id == m_errorRecoveryTimerId)
    {
        bool error = false;
        PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);

        if (mctx->IsRecorderErrored() ||
            mctx->IsPlayerErrored() ||
            mctx->IsErrored())
        {
            SetExitPlayer(true, false);
            ForceNextStateNone(mctx);
            error = true;
        }

        for (size_t i = 0; i < m_player.size(); i++)
        {
            PlayerContext *ctx2 = GetPlayer(mctx, i);
            if (error || ctx2->IsErrored())
                ForceNextStateNone(ctx2);
        }
        ReturnPlayerLock(mctx);

        QMutexLocker locker(&m_timerIdLock);
        if (m_errorRecoveryTimerId)
            KillTimer(m_errorRecoveryTimerId);
        m_errorRecoveryTimerId =
            StartTimer(kErrorRecoveryCheckFrequency, __LINE__);
    }
}

bool TV::HandlePxPTimerEvent(void)
{
    QString cmd;

    {
        QMutexLocker locker(&m_timerIdLock);
        if (m_changePxP.empty())
        {
            if (m_pipChangeTimerId)
                KillTimer(m_pipChangeTimerId);
            m_pipChangeTimerId = 0;
            return true;
        }
        cmd = m_changePxP.dequeue();
    }

    PlayerContext *mctx = GetPlayerWriteLock(0, __FILE__, __LINE__);
    PlayerContext *actx = GetPlayer(mctx, -1);

    if (cmd == "TOGGLEPIPMODE")
        PxPToggleView(actx, false);
    else if (cmd == "TOGGLEPBPMODE")
        PxPToggleView(actx, true);
    else if (cmd == "CREATEPIPVIEW")
        PxPCreateView(actx, false);
    else if (cmd == "CREATEPBPVIEW")
        PxPCreateView(actx, true);
    else if (cmd == "SWAPPIP")
    {
        if (mctx != actx)
            PxPSwap(mctx, actx);
        else if (mctx && m_player.size() == 2)
            PxPSwap(mctx, GetPlayer(mctx,1));
    }
    else if (cmd == "TOGGLEPIPSTATE")
        PxPToggleType(mctx, !mctx->IsPBP());

    ReturnPlayerLock(mctx);

    QMutexLocker locker(&m_timerIdLock);

    if (m_pipChangeTimerId)
        KillTimer(m_pipChangeTimerId);

    if (m_changePxP.empty())
        m_pipChangeTimerId = 0;
    else
        m_pipChangeTimerId = StartTimer(20, __LINE__);

    return true;
}

bool TV::HandleLCDTimerEvent(void)
{
    PlayerContext *actx = GetPlayerReadLock(-1, __FILE__, __LINE__);
    LCD *lcd = LCD::Get();
    if (lcd)
    {
        float progress = 0.0F;
        QString lcd_time_string;
        bool showProgress = true;

        if (StateIsLiveTV(GetState(actx)))
            ShowLCDChannelInfo(actx);

        if (actx->m_buffer && actx->m_buffer->IsDVD())
        {
            ShowLCDDVDInfo(actx);
            showProgress = !actx->m_buffer->IsInDiscMenuOrStillFrame();
        }

        if (showProgress)
        {
            osdInfo info;
            if (actx->CalcPlayerSliderPosition(info)) {
                progress = info.values["position"] * 0.001F;

                lcd_time_string = info.text["playedtime"] + " / " + info.text["totaltime"];
                // if the string is longer than the LCD width, remove all spaces
                if (lcd_time_string.length() > (int)lcd->getLCDWidth())
                    lcd_time_string.remove(' ');
            }
        }
        lcd->setChannelProgress(lcd_time_string, progress);
    }
    ReturnPlayerLock(actx);

    QMutexLocker locker(&m_timerIdLock);
    KillTimer(m_lcdTimerId);
    m_lcdTimerId = StartTimer(kLCDTimeout, __LINE__);

    return true;
}

void TV::HandleLCDVolumeTimerEvent()
{
    PlayerContext *actx = GetPlayerReadLock(-1, __FILE__, __LINE__);
    LCD *lcd = LCD::Get();
    if (lcd)
    {
        ShowLCDChannelInfo(actx);
        lcd->switchToChannel(m_lcdCallsign, m_lcdTitle, m_lcdSubtitle);
    }
    ReturnPlayerLock(actx);

    QMutexLocker locker(&m_timerIdLock);
    KillTimer(m_lcdVolumeTimerId);
    m_lcdVolumeTimerId = 0;
}

int TV::StartTimer(int interval, int line)
{
    int x = QObject::startTimer(interval);
    if (!x)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Failed to start timer on line %1 of %2")
                .arg(line).arg(__FILE__));
    }
    return x;
}

void TV::KillTimer(int id)
{
    QObject::killTimer(id);
}

void TV::ForceNextStateNone(PlayerContext *ctx)
{
    ctx->ForceNextStateNone();
    ScheduleStateChange(ctx);
}

void TV::ScheduleStateChange(PlayerContext *ctx)
{
    QMutexLocker locker(&m_timerIdLock);
    m_stateChangeTimerId[StartTimer(1, __LINE__)] = ctx;
}

void TV::SetErrored(PlayerContext *ctx)
{
    if (!ctx)
        return;
    QMutexLocker locker(&m_timerIdLock);
    ctx->m_errored = true;
    KillTimer(m_errorRecoveryTimerId);
    m_errorRecoveryTimerId = StartTimer(1, __LINE__);
}

void TV::PrepToSwitchToRecordedProgram(PlayerContext *ctx,
                                       const ProgramInfo &p)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Switching to program: %1")
            .arg(p.toString(ProgramInfo::kTitleSubtitle)));
    SetLastProgram(&p);
    PrepareToExitPlayer(ctx,__LINE__);
    m_jumpToProgram = true;
    SetExitPlayer(true, true);
}

void TV::PrepareToExitPlayer(PlayerContext *ctx, int line, BookmarkAction bookmark)
{
    bool bm_allowed = IsBookmarkAllowed(ctx);
    ctx->LockDeletePlayer(__FILE__, line);
    if (ctx->m_player)
    {
        if (bm_allowed)
        {
            // If we're exiting in the middle of the recording, we
            // automatically save a bookmark when "Action on playback
            // exit" is set to "Save position and exit".
            bool allow_set_before_end =
                (bookmark == kBookmarkAlways ||
                 (bookmark == kBookmarkAuto &&
                  m_dbPlaybackExitPrompt == 2));
            // If we're exiting at the end of the recording, we
            // automatically clear the bookmark when "Action on
            // playback exit" is set to "Save position and exit" and
            // "Clear bookmark on playback" is set to true.
            bool allow_clear_at_end =
                (bookmark == kBookmarkAlways ||
                 (bookmark == kBookmarkAuto &&
                  m_dbPlaybackExitPrompt == 2 &&
                  m_dbClearSavedPosition));
            // Whether to set/clear a bookmark depends on whether we're
            // exiting at the end of a recording.
            bool at_end = (ctx->m_player->IsNearEnd() || getEndOfRecording());
            // Don't consider ourselves at the end if the recording is
            // in-progress.
            at_end &= !StateIsRecording(GetState(ctx));
            bool clear_lastplaypos = false;
            if (at_end && allow_clear_at_end)
            {
                SetBookmark(ctx, true);
                // Tidy up the lastplaypos mark only when we clear the
                // bookmark due to exiting at the end.
                clear_lastplaypos = true;
            }
            else if (!at_end && allow_set_before_end)
            {
                SetBookmark(ctx, false);
            }
            if (clear_lastplaypos && ctx->m_playingInfo)
                ctx->m_playingInfo->ClearMarkupMap(MARK_UTIL_LASTPLAYPOS);
        }
        if (m_dbAutoSetWatched)
            ctx->m_player->SetWatched();
    }
    ctx->UnlockDeletePlayer(__FILE__, line);
}

void TV::SetExitPlayer(bool set_it, bool wants_to)
{
    QMutexLocker locker(&m_timerIdLock);
    if (set_it)
    {
        m_wantsToQuit = wants_to;
        if (!m_exitPlayerTimerId)
            m_exitPlayerTimerId = StartTimer(1, __LINE__);
    }
    else
    {
        if (m_exitPlayerTimerId)
            KillTimer(m_exitPlayerTimerId);
        m_exitPlayerTimerId = 0;
        m_wantsToQuit = wants_to;
    }
}

void TV::SetUpdateOSDPosition(bool set_it)
{
    QMutexLocker locker(&m_timerIdLock);
    if (set_it)
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

void TV::HandleEndOfPlaybackTimerEvent(void)
{
    {
        QMutexLocker locker(&m_timerIdLock);
        if (m_endOfPlaybackTimerId)
            KillTimer(m_endOfPlaybackTimerId);
        m_endOfPlaybackTimerId = 0;
    }

    bool is_playing = false;
    PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
    for (uint i = 0; mctx && (i < m_player.size()); i++)
    {
        PlayerContext *ctx = GetPlayer(mctx, i);
        if (!StateIsPlaying(ctx->GetState()))
            continue;

        if (ctx->IsPlayerPlaying())
        {
            is_playing = true;
            continue;
        }

        // If the end of playback is destined to pop up the end of
        // recording delete prompt, then don't exit the player here.
        if (ctx->GetState() == kState_WatchingPreRecorded &&
            m_dbEndOfRecExitPrompt && !m_inPlaylist && !m_underNetworkControl)
            continue;

        ForceNextStateNone(ctx);
        if (mctx == ctx)
        {
            m_endOfRecording = true;
            PrepareToExitPlayer(mctx, __LINE__);
            SetExitPlayer(true, true);
        }
    }
    ReturnPlayerLock(mctx);

    if (is_playing)
    {
        QMutexLocker locker(&m_timerIdLock);
        m_endOfPlaybackTimerId =
            StartTimer(kEndOfPlaybackCheckFrequency, __LINE__);
    }
}

void TV::HandleIsNearEndWhenEmbeddingTimerEvent(void)
{
    PlayerContext *actx = GetPlayerReadLock(-1, __FILE__, __LINE__);
    if (!StateIsLiveTV(GetState(actx)))
    {
        actx->LockDeletePlayer(__FILE__, __LINE__);
        bool toggle = actx->m_player && actx->m_player->IsEmbedding() &&
                      actx->m_player->IsNearEnd() && !actx->m_player->IsPaused();
        actx->UnlockDeletePlayer(__FILE__, __LINE__);
        if (toggle)
            DoTogglePause(actx, true);
    }
    ReturnPlayerLock(actx);
}

void TV::HandleEndOfRecordingExitPromptTimerEvent(void)
{
    if (m_endOfRecording || m_inPlaylist || m_editMode || m_underNetworkControl ||
        m_exitPlayerTimerId)
    {
        return;
    }

    PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
    OSD *osd = GetOSDLock(mctx);
    if (osd && osd->DialogVisible())
    {
        ReturnOSDLock(mctx, osd);
        ReturnPlayerLock(mctx);
        return;
    }
    ReturnOSDLock(mctx, osd);

    mctx->LockDeletePlayer(__FILE__, __LINE__);
    bool do_prompt = (mctx->GetState() == kState_WatchingPreRecorded &&
                 mctx->m_player &&
                 !mctx->m_player->IsEmbedding() &&
                 !mctx->m_player->IsPlaying());
    mctx->UnlockDeletePlayer(__FILE__, __LINE__);

    if (do_prompt)
        ShowOSDPromptDeleteRecording(mctx, tr("End Of Recording"));

    ReturnPlayerLock(mctx);
}

void TV::HandleVideoExitDialogTimerEvent(void)
{
    {
        QMutexLocker locker(&m_timerIdLock);
        if (m_videoExitDialogTimerId)
            KillTimer(m_videoExitDialogTimerId);
        m_videoExitDialogTimerId = 0;
    }

    // disable dialog and exit playback after timeout
    PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
    OSD *osd = GetOSDLock(mctx);
    if (!osd || !osd->DialogVisible(OSD_DLG_VIDEOEXIT))
    {
        ReturnOSDLock(mctx, osd);
        ReturnPlayerLock(mctx);
        return;
    }
    if (osd)
        osd->DialogQuit();
    ReturnOSDLock(mctx, osd);
    DoTogglePause(mctx, true);
    ClearOSD(mctx);
    PrepareToExitPlayer(mctx, __LINE__);
    ReturnPlayerLock(mctx);

    m_requestDelete = false;
    SetExitPlayer(true, true);
}

void TV::HandlePseudoLiveTVTimerEvent(void)
{
    {
        QMutexLocker locker(&m_timerIdLock);
        KillTimer(m_pseudoChangeChanTimerId);
        m_pseudoChangeChanTimerId = 0;
    }

    bool restartTimer = false;
    PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
    for (uint i = 0; mctx && (i < m_player.size()); i++)
    {
        PlayerContext *ctx = GetPlayer(mctx, i);
        if (kPseudoChangeChannel != ctx->m_pseudoLiveTVState)
            continue;

        if (ctx->InStateChange())
        {
            restartTimer = true;
            continue;
        }

        LOG(VB_CHANNEL, LOG_INFO,
            QString("REC_PROGRAM -- channel change %1").arg(i));

        uint        chanid  = ctx->m_pseudoLiveTVRec->GetChanID();
        QString     channum = ctx->m_pseudoLiveTVRec->GetChanNum();
        StringDeque tmp     = ctx->m_prevChan;

        ctx->m_prevChan.clear();
        ChangeChannel(ctx, chanid, channum);
        ctx->m_prevChan = tmp;
        ctx->m_pseudoLiveTVState = kPseudoRecording;
    }
    ReturnPlayerLock(mctx);

    if (restartTimer)
    {
        QMutexLocker locker(&m_timerIdLock);
        if (!m_pseudoChangeChanTimerId)
            m_pseudoChangeChanTimerId = StartTimer(25, __LINE__);
    }
}

void TV::SetSpeedChangeTimer(uint when, int line)
{
    QMutexLocker locker(&m_timerIdLock);
    if (m_speedChangeTimerId)
        KillTimer(m_speedChangeTimerId);
    m_speedChangeTimerId = StartTimer(when, line);
}

void TV::HandleSpeedChangeTimerEvent(void)
{
    {
        QMutexLocker locker(&m_timerIdLock);
        if (m_speedChangeTimerId)
            KillTimer(m_speedChangeTimerId);
        m_speedChangeTimerId = StartTimer(kSpeedChangeCheckFrequency, __LINE__);
    }

    bool update_msg = false;
    PlayerContext *actx = GetPlayerReadLock(-1, __FILE__, __LINE__);
    for (uint i = 0; actx && (i < m_player.size()); i++)
    {
        PlayerContext *ctx = GetPlayer(actx, i);
        update_msg |= ctx->HandlePlayerSpeedChangeFFRew() && (ctx == actx);
    }
    ReturnPlayerLock(actx);

    actx = GetPlayerReadLock(-1, __FILE__, __LINE__);
    for (uint i = 0; actx && (i < m_player.size()); i++)
    {
        PlayerContext *ctx = GetPlayer(actx, i);
        update_msg |= ctx->HandlePlayerSpeedChangeEOF() && (ctx == actx);
    }

    if (actx && update_msg)
    {
        UpdateOSDSeekMessage(actx, actx->GetPlayMessage(), kOSDTimeout_Med);
    }
    ReturnPlayerLock(actx);
}

/// This selectively blocks KeyPress and Resize events
bool TV::eventFilter(QObject *o, QEvent *e)
{
    // We want to intercept all resize events sent to the main window
    if ((e->type() == QEvent::Resize))
        return (GetMythMainWindow() != o) ? false : event(e);

    // Intercept keypress events unless they need to be handled by a main UI
    // screen (e.g. GuideGrid, ProgramFinder)

    if ( (QEvent::KeyPress == e->type() || QEvent::KeyRelease == e->type())
        && m_ignoreKeyPresses )
        return false;

    QScopedPointer<QEvent> sNewEvent(nullptr);
    if (GetMythMainWindow()->keyLongPressFilter(&e, sNewEvent))
        return true;

    if (QEvent::KeyPress == e->type())
        return event(e);

    if (MythGestureEvent::kEventType == e->type())
        return m_ignoreKeyPresses ? false : event(e);

    if (e->type() == MythEvent::MythEventMessage ||
        e->type() == MythEvent::MythUserMessage  ||
        e->type() == MythEvent::kUpdateTvProgressEventType ||
        e->type() == MythMediaEvent::kEventType)
    {
        customEvent(e);
        return true;
    }

    switch (e->type())
    {
        case QEvent::Paint:
        case QEvent::UpdateRequest:
        case QEvent::Enter:
        {
            event(e);
            return false;
        }
        default:
            return false;
    }
}

/// This handles all standard events
bool TV::event(QEvent *e)
{
    if (QEvent::Resize == e->type())
    {
        PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
        mctx->LockDeletePlayer(__FILE__, __LINE__);
        const auto *qre = dynamic_cast<const QResizeEvent*>(e);
        if (qre && mctx->m_player)
            mctx->m_player->WindowResized(qre->size());
        mctx->UnlockDeletePlayer(__FILE__, __LINE__);
        ReturnPlayerLock(mctx);
        return false;
    }

    if (QEvent::KeyPress == e->type() ||
        MythGestureEvent::kEventType == e->type())
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
#endif // DEBUG_ACTIONS
        bool handled = false;
        PlayerContext *actx = GetPlayerReadLock(-1, __FILE__, __LINE__);
        if (actx->HasPlayer())
            handled = ProcessKeypressOrGesture(actx, e);
        ReturnPlayerLock(actx);
        if (handled)
            return true;
    }

    switch (e->type())
    {
        case QEvent::Paint:
        case QEvent::UpdateRequest:
        case QEvent::Enter:
            return true;
        default:
            break;
    }

    return QObject::event(e);
}

bool TV::HandleTrackAction(PlayerContext *ctx, const QString &action)
{
    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (!ctx->m_player)
    {
        ctx->UnlockDeletePlayer(__FILE__, __LINE__);
        return false;
    }

    bool handled = true;

    if (action == ACTION_TOGGLEEXTTEXT)
        ctx->m_player->ToggleCaptions(kTrackTypeTextSubtitle);
    else if (ACTION_ENABLEEXTTEXT == action)
        ctx->m_player->EnableCaptions(kDisplayTextSubtitle);
    else if (ACTION_DISABLEEXTTEXT == action)
        ctx->m_player->DisableCaptions(kDisplayTextSubtitle);
    else if (ACTION_ENABLEFORCEDSUBS == action)
        ctx->m_player->SetAllowForcedSubtitles(true);
    else if (ACTION_DISABLEFORCEDSUBS == action)
        ctx->m_player->SetAllowForcedSubtitles(false);
    else if (action == ACTION_ENABLESUBS)
        ctx->m_player->SetCaptionsEnabled(true, true);
    else if (action == ACTION_DISABLESUBS)
        ctx->m_player->SetCaptionsEnabled(false, true);
    else if (action == ACTION_TOGGLESUBS && !m_browseHelper->IsBrowsing())
    {
        if (m_ccInputMode)
        {
            bool valid = false;
            int page = GetQueuedInputAsInt(&valid, 16);
            if (m_vbimode == VBIMode::PAL_TT && valid)
                ctx->m_player->SetTeletextPage(page);
            else if (m_vbimode == VBIMode::NTSC_CC)
                ctx->m_player->SetTrack(kTrackTypeCC608,
                                   max(min(page - 1, 1), 0));

            ClearInputQueues(ctx, true);

            QMutexLocker locker(&m_timerIdLock);
            m_ccInputMode = false;
            if (m_ccInputTimerId)
            {
                KillTimer(m_ccInputTimerId);
                m_ccInputTimerId = 0;
            }
        }
        else if (ctx->m_player->GetCaptionMode() & kDisplayNUVTeletextCaptions)
        {
            ClearInputQueues(ctx, false);
            AddKeyToInputQueue(ctx, 0);

            QMutexLocker locker(&m_timerIdLock);
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
            ctx->m_player->ToggleCaptions();
        }
    }
    else if (action.startsWith("TOGGLE"))
    {
        int type = to_track_type(action.mid(6));
        if (type == kTrackTypeTeletextMenu)
            ctx->m_player->EnableTeletext();
        else if (type >= kTrackTypeSubtitle)
            ctx->m_player->ToggleCaptions(type);
        else
            handled = false;
    }
    else if (action.startsWith("SELECT"))
    {
        int type = to_track_type(action.mid(6));
        int num = action.section("_", -1).toInt();
        if (type >= kTrackTypeAudio)
            ctx->m_player->SetTrack(type, num);
        else
            handled = false;
    }
    else if (action.startsWith("NEXT") || action.startsWith("PREV"))
    {
        int dir = (action.startsWith("NEXT")) ? +1 : -1;
        int type = to_track_type(action.mid(4));
        if (type >= kTrackTypeAudio)
            ctx->m_player->ChangeTrack(type, dir);
        else if (action.endsWith("CC"))
            ctx->m_player->ChangeCaptionTrack(dir);
        else
            handled = false;
    }
    else
        handled = false;

    ctx->UnlockDeletePlayer(__FILE__, __LINE__);

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
static bool SysEventHandleAction(QKeyEvent *e, const QStringList &actions)
{
    QStringList::const_iterator it;
    for (it = actions.begin(); it != actions.end(); ++it)
    {
        if ((*it).startsWith("SYSEVENT") ||
            *it == ACTION_SCREENSHOT ||
            *it == ACTION_TVPOWERON ||
            *it == ACTION_TVPOWEROFF)
        {
            return GetMythMainWindow()->GetMainStack()->GetTopScreen()->
                keyPressEvent(e);
        }
    }
    return false;
}

QList<QKeyEvent> TV::ConvertScreenPressKeyMap(const QString &keyList)
{
    QList<QKeyEvent> keyPressList;
    int i = 0;
    QStringList stringKeyList = keyList.split(',');
    QStringList::const_iterator it;
    for (it = stringKeyList.begin(); it != stringKeyList.end(); ++it)
    {
        QKeySequence keySequence(*it);
        for(i = 0; i < keySequence.count(); i++)
        {
            unsigned int keynum = keySequence[i];
            QKeyEvent keyEvent(QEvent::None,
                               (int)(keynum & ~Qt::KeyboardModifierMask),
                               (Qt::KeyboardModifiers)(keynum & Qt::KeyboardModifierMask));
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

bool TV::TranslateGesture(const QString &context, MythGestureEvent *e,
                          QStringList &actions, bool isLiveTV)
{
    if (e && context == "TV Playback")
    {
        // TODO make this configuable via a similar mechanism to
        //      TranslateKeyPress
        // possibly with configurable hot zones of various sizes in a theme
        // TODO enhance gestures to support other non Click types too
        if ((e->gesture() == MythGestureEvent::Click) &&
            (e->GetButton() == MythGestureEvent::LeftButton))
        {
            // divide screen into 12 regions
            QSize size = GetMythMainWindow()->size();
            QPoint pos = e->GetPosition();
            int region = 0;
            const int widthDivider = 4;
            int w4 = size.width() / widthDivider;
            region = pos.x() / w4;
            int h3 = size.height() / 3;
            region += (pos.y() / h3) * widthDivider;

            if (isLiveTV)
            {
                return GetMythMainWindow()->TranslateKeyPress(
                        context, &(m_screenPressKeyMapLiveTV[region]), actions, true);
            }
            return GetMythMainWindow()->TranslateKeyPress(
                context, &(m_screenPressKeyMapPlayback[region]), actions, true);
        }
        return false;
    }
    return false;
}

bool TV::TranslateKeyPressOrGesture(const QString &context,
                                    QEvent *e, QStringList &actions,
                                    bool isLiveTV, bool allowJumps)
{
    if (QEvent::KeyPress == e->type())
    {
        return GetMythMainWindow()->TranslateKeyPress(
            context, dynamic_cast<QKeyEvent*>(e), actions, allowJumps);
    }
    if (MythGestureEvent::kEventType == e->type())
    {
        return TranslateGesture(context, dynamic_cast<MythGestureEvent*>(e), actions, isLiveTV);
    }

    return false;
}

bool TV::ProcessKeypressOrGesture(PlayerContext *actx, QEvent *e)
{
    bool ignoreKeys = actx->IsPlayerChangingBuffers();
#if DEBUG_ACTIONS
    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("ignoreKeys: %1")
            .arg(ignoreKeys));
#endif // DEBUG_ACTIONS

    if (m_idleTimerId)
    {
        KillTimer(m_idleTimerId);
        m_idleTimerId = StartTimer(m_dbIdleTimeout, __LINE__);
    }

#ifdef Q_OS_LINUX
    // Fixups for _some_ linux native codes that QT doesn't know
    auto* eKeyEvent = dynamic_cast<QKeyEvent*>(e);
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

    TVState state = GetState(actx);
    bool isLiveTV = StateIsLiveTV(state);

    if (ignoreKeys)
    {
        handled = TranslateKeyPressOrGesture("TV Playback", e, actions, isLiveTV);
        alreadyTranslatedPlayback = true;

        if (handled || actions.isEmpty())
            return handled;

        bool esc   = has_action("ESCAPE", actions) ||
                     has_action("BACK", actions);
        bool pause = has_action(ACTION_PAUSE, actions);
        bool play  = has_action(ACTION_PLAY,  actions);

        if ((!esc || m_browseHelper->IsBrowsing()) && !pause && !play)
            return false;
    }

    OSD *osd = GetOSDLock(actx);
    if (osd && osd->DialogVisible())
    {
        if (QEvent::KeyPress == e->type())
        {
            auto *qke = dynamic_cast<QKeyEvent*>(e);
            handled = (qke != nullptr) && osd->DialogHandleKeypress(qke);
        }
        if (MythGestureEvent::kEventType == e->type())
        {
            auto *mge = dynamic_cast<MythGestureEvent*>(e);
            handled = (mge != nullptr) && osd->DialogHandleGesture(mge);
        }
    }
    ReturnOSDLock(actx, osd);

    if (m_editMode && !handled)
    {
        handled |= TranslateKeyPressOrGesture(
                   "TV Editing", e, actions, isLiveTV);

        if (!handled && actx->m_player)
        {
            if (has_action("MENU", actions))
            {
                ShowOSDCutpoint(actx, "EDIT_CUT_POINTS");
                handled = true;
            }
            if (has_action(ACTION_MENUCOMPACT, actions))
            {
                ShowOSDCutpoint(actx, "EDIT_CUT_POINTS_COMPACT");
                handled = true;
            }
            if (has_action("ESCAPE", actions))
            {
                if (!actx->m_player->IsCutListSaved())
                    ShowOSDCutpoint(actx, "EXIT_EDIT_MODE");
                else
                {
                    actx->LockDeletePlayer(__FILE__, __LINE__);
                    actx->m_player->DisableEdit(0);
                    actx->UnlockDeletePlayer(__FILE__, __LINE__);
                }
                handled = true;
            }
            else
            {
                actx->LockDeletePlayer(__FILE__, __LINE__);
                int64_t current_frame = actx->m_player->GetFramesPlayed();
                actx->UnlockDeletePlayer(__FILE__, __LINE__);
                if ((has_action(ACTION_SELECT, actions)) &&
                    (actx->m_player->IsInDelete(current_frame)) &&
                    (!(actx->m_player->HasTemporaryMark())))
                {
                    ShowOSDCutpoint(actx, "EDIT_CUT_POINTS");
                    handled = true;
                }
                else
                    handled |=
                        actx->m_player->HandleProgramEditorActions(actions);
            }
        }
        if (handled)
            m_editMode = (actx->m_player && actx->m_player->GetEditMode());
    }

    if (handled)
        return true;

    // If text is already queued up, be more lax on what is ok.
    // This allows hex teletext entry and minor channel entry.
    if (QEvent::KeyPress == e->type())
    {
        auto *qke = dynamic_cast<QKeyEvent*>(e);
        if (qke == nullptr)
            return false;
        const QString txt = qke->text();
        if (HasQueuedInput() && (1 == txt.length()))
        {
            bool ok = false;
            (void)txt.toInt(&ok, 16);
            if (ok || txt=="_" || txt=="-" || txt=="#" || txt==".")
            {
                AddKeyToInputQueue(actx, txt.at(0).toLatin1());
                return true;
            }
        }
    }

    // Teletext menu
    actx->LockDeletePlayer(__FILE__, __LINE__);
    if (actx->m_player && (actx->m_player->GetCaptionMode() == kDisplayTeletextMenu))
    {
        QStringList tt_actions;
        handled = TranslateKeyPressOrGesture(
                  "Teletext Menu", e, tt_actions, isLiveTV);

        if (!handled && !tt_actions.isEmpty())
        {
            for (int i = 0; i < tt_actions.size(); i++)
            {
                if (actx->m_player->HandleTeletextAction(tt_actions[i]))
                {
                    actx->UnlockDeletePlayer(__FILE__, __LINE__);
                    return true;
                }
            }
        }
    }

    // Interactive television
    if (actx->m_player && actx->m_player->GetInteractiveTV())
    {
        if (!alreadyTranslatedPlayback)
        {
            handled = TranslateKeyPressOrGesture(
                      "TV Playback", e, actions, isLiveTV);
            alreadyTranslatedPlayback = true;
        }
        if (!handled && !actions.isEmpty())
        {
            for (int i = 0; i < actions.size(); i++)
            {
                if (actx->m_player->ITVHandleAction(actions[i]))
                {
                    actx->UnlockDeletePlayer(__FILE__, __LINE__);
                    return true;
                }
            }
        }
    }
    actx->UnlockDeletePlayer(__FILE__, __LINE__);

    if (!alreadyTranslatedPlayback)
    {
        handled = TranslateKeyPressOrGesture(
                  "TV Playback", e, actions, isLiveTV);
    }
    if (handled || actions.isEmpty())
        return handled;

    handled = false;

    bool isDVD = actx->m_buffer && actx->m_buffer->IsDVD();
    bool isMenuOrStill = actx->m_buffer && actx->m_buffer->IsInDiscMenuOrStillFrame();

    if (QEvent::KeyPress == e->type())
    {
        handled = handled || SysEventHandleAction(dynamic_cast<QKeyEvent*>(e), actions);
    }
    handled = handled || BrowseHandleAction(actx, actions);
    handled = handled || ManualZoomHandleAction(actx, actions);
    handled = handled || PictureAttributeHandleAction(actx, actions);
    handled = handled || TimeStretchHandleAction(actx, actions);
    handled = handled || AudioSyncHandleAction(actx, actions);
    handled = handled || SubtitleZoomHandleAction(actx, actions);
    handled = handled || SubtitleDelayHandleAction(actx, actions);
    handled = handled || DiscMenuHandleAction(actx, actions);
    handled = handled || ActiveHandleAction(
        actx, actions, isDVD, isMenuOrStill);
    handled = handled || ToggleHandleAction(actx, actions, isDVD);
    handled = handled || PxPHandleAction(actx, actions);
    handled = handled || FFRewHandleAction(actx, actions);
    handled = handled || ActivePostQHandleAction(actx, actions);

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
                AddKeyToInputQueue(actx, '0' + val);
                handled = true;
            }
        }
    }

    return true;
}

bool TV::BrowseHandleAction(PlayerContext *ctx, const QStringList &actions)
{
    if (!m_browseHelper->IsBrowsing())
        return false;

    bool handled = true;

    if (has_action(ACTION_UP, actions) || has_action(ACTION_CHANNELUP, actions))
        m_browseHelper->BrowseDispInfo(ctx, BROWSE_UP);
    else if (has_action(ACTION_DOWN, actions) || has_action(ACTION_CHANNELDOWN, actions))
        m_browseHelper->BrowseDispInfo(ctx, BROWSE_DOWN);
    else if (has_action(ACTION_LEFT, actions))
        m_browseHelper->BrowseDispInfo(ctx, BROWSE_LEFT);
    else if (has_action(ACTION_RIGHT, actions))
        m_browseHelper->BrowseDispInfo(ctx, BROWSE_RIGHT);
    else if (has_action("NEXTFAV", actions))
        m_browseHelper->BrowseDispInfo(ctx, BROWSE_FAVORITE);
    else if (has_action(ACTION_SELECT, actions))
    {
        m_browseHelper->BrowseEnd(ctx, true);
    }
    else if (has_action(ACTION_CLEAROSD, actions) ||
             has_action("ESCAPE",       actions) ||
             has_action("BACK",         actions) ||
             has_action("TOGGLEBROWSE", actions))
    {
        m_browseHelper->BrowseEnd(ctx, false);
    }
    else if (has_action(ACTION_TOGGLERECORD, actions))
        QuickRecord(ctx);
    else
    {
        handled = false;
        foreach (const auto & action, actions)
        {
            if (action.length() == 1 && action[0].isDigit())
            {
                AddKeyToInputQueue(ctx, action[0].toLatin1());
                handled = true;
            }
        }
    }

    // only pass-through actions listed below
    return handled ||
        !(has_action(ACTION_VOLUMEDOWN, actions) ||
          has_action(ACTION_VOLUMEUP,   actions) ||
          has_action("STRETCHINC",      actions) ||
          has_action("STRETCHDEC",      actions) ||
          has_action(ACTION_MUTEAUDIO,  actions) ||
          has_action("CYCLEAUDIOCHAN",  actions) ||
          has_action("BOTTOMLINEMOVE",  actions) ||
          has_action("BOTTOMLINESAVE",  actions) ||
          has_action("TOGGLEASPECT",    actions) ||
          has_action("TOGGLEPIPMODE",   actions) ||
          has_action("TOGGLEPIPSTATE",  actions) ||
          has_action("NEXTPIPWINDOW",   actions) ||
          has_action("CREATEPIPVIEW",   actions) ||
          has_action("CREATEPBPVIEW",   actions) ||
          has_action("SWAPPIP",         actions));
}

bool TV::ManualZoomHandleAction(PlayerContext *actx, const QStringList &actions)
{
    if (!m_zoomMode)
        return false;

    actx->LockDeletePlayer(__FILE__, __LINE__);
    if (!actx->m_player)
    {
        actx->UnlockDeletePlayer(__FILE__, __LINE__);
        return false;
    }

    bool end_manual_zoom = false;
    bool handled = true;
    bool updateOSD = true;
    ZoomDirection zoom = kZoom_END;
    if (has_action(ACTION_ZOOMUP, actions) ||
        has_action(ACTION_UP, actions)     ||
        has_action(ACTION_CHANNELUP, actions))
    {
        zoom = kZoomUp;
    }
    else if (has_action(ACTION_ZOOMDOWN, actions) ||
             has_action(ACTION_DOWN, actions)     ||
             has_action(ACTION_CHANNELDOWN, actions))
    {
        zoom = kZoomDown;
    }
    else if (has_action(ACTION_ZOOMLEFT, actions) ||
             has_action(ACTION_LEFT, actions))
        zoom = kZoomLeft;
    else if (has_action(ACTION_ZOOMRIGHT, actions) ||
             has_action(ACTION_RIGHT, actions))
        zoom = kZoomRight;
    else if (has_action(ACTION_ZOOMASPECTUP, actions) ||
             has_action(ACTION_VOLUMEUP, actions))
        zoom = kZoomAspectUp;
    else if (has_action(ACTION_ZOOMASPECTDOWN, actions) ||
             has_action(ACTION_VOLUMEDOWN, actions))
        zoom = kZoomAspectDown;
    else if (has_action(ACTION_ZOOMIN, actions) ||
             has_action(ACTION_JUMPFFWD, actions))
        zoom = kZoomIn;
    else if (has_action(ACTION_ZOOMOUT, actions) ||
             has_action(ACTION_JUMPRWND, actions))
        zoom = kZoomOut;
    else if (has_action(ACTION_ZOOMVERTICALIN, actions))
        zoom = kZoomVerticalIn;
    else if (has_action(ACTION_ZOOMVERTICALOUT, actions))
        zoom = kZoomVerticalOut;
    else if (has_action(ACTION_ZOOMHORIZONTALIN, actions))
        zoom = kZoomHorizontalIn;
    else if (has_action(ACTION_ZOOMHORIZONTALOUT, actions))
        zoom = kZoomHorizontalOut;
    else if (has_action(ACTION_ZOOMQUIT, actions) ||
             has_action("ESCAPE", actions)        ||
             has_action("BACK", actions))
    {
        zoom = kZoomHome;
        end_manual_zoom = true;
    }
    else if (has_action(ACTION_ZOOMCOMMIT, actions) ||
             has_action(ACTION_SELECT, actions))
    {
        end_manual_zoom = true;
        SetManualZoom(actx, false, tr("Zoom Committed"));
    }
    else
    {
        updateOSD = false;
        // only pass-through actions listed below
        handled = !(has_action("STRETCHINC",     actions) ||
                    has_action("STRETCHDEC",     actions) ||
                    has_action(ACTION_MUTEAUDIO, actions) ||
                    has_action("CYCLEAUDIOCHAN", actions) ||
                    has_action(ACTION_PAUSE,     actions) ||
                    has_action(ACTION_CLEAROSD,  actions));
    }
    QString msg = tr("Zoom Committed");
    if (zoom != kZoom_END)
    {
        actx->m_player->Zoom(zoom);
        if (end_manual_zoom)
            msg = tr("Zoom Ignored");
        else
            msg = actx->m_player->GetVideoOutput()->GetZoomString();
    }
    else if (end_manual_zoom)
        msg = tr("%1 Committed")
            .arg(actx->m_player->GetVideoOutput()->GetZoomString());
    actx->UnlockDeletePlayer(__FILE__, __LINE__);

    if (updateOSD)
        SetManualZoom(actx, !end_manual_zoom, msg);

    return handled;
}

bool TV::PictureAttributeHandleAction(PlayerContext *ctx,
                                      const QStringList &actions)
{
    if (!m_adjustingPicture)
        return false;

    bool handled = true;
    if (has_action(ACTION_LEFT, actions))
    {
        DoChangePictureAttribute(ctx, m_adjustingPicture,
                                 m_adjustingPictureAttribute, false);
    }
    else if (has_action(ACTION_RIGHT, actions))
    {
        DoChangePictureAttribute(ctx, m_adjustingPicture,
                                 m_adjustingPictureAttribute, true);
    }
    else
        handled = false;

    return handled;
}

bool TV::TimeStretchHandleAction(PlayerContext *ctx,
                                 const QStringList &actions)
{
    if (!m_stretchAdjustment)
        return false;

    bool handled = true;

    if (has_action(ACTION_LEFT, actions))
        ChangeTimeStretch(ctx, -1);
    else if (has_action(ACTION_RIGHT, actions))
        ChangeTimeStretch(ctx, 1);
    else if (has_action(ACTION_DOWN, actions))
        ChangeTimeStretch(ctx, -5);
    else if (has_action(ACTION_UP, actions))
        ChangeTimeStretch(ctx, 5);
    else if (has_action("ADJUSTSTRETCH", actions))
        ToggleTimeStretch(ctx);
    else if (has_action(ACTION_SELECT, actions))
        ClearOSD(ctx);
    else
        handled = false;

    return handled;
}

bool TV::AudioSyncHandleAction(PlayerContext *ctx,
                               const QStringList &actions)
{
    if (!m_audiosyncAdjustment)
        return false;

    bool handled = true;

    if (has_action(ACTION_LEFT, actions))
        ChangeAudioSync(ctx, -1);
    else if (has_action(ACTION_RIGHT, actions))
        ChangeAudioSync(ctx, 1);
    else if (has_action(ACTION_UP, actions))
        ChangeAudioSync(ctx, 10);
    else if (has_action(ACTION_DOWN, actions))
        ChangeAudioSync(ctx, -10);
    else if (has_action(ACTION_TOGGELAUDIOSYNC, actions) ||
             has_action(ACTION_SELECT, actions))
        ClearOSD(ctx);
    else
        handled = false;

    return handled;
}

bool TV::SubtitleZoomHandleAction(PlayerContext *ctx,
                                  const QStringList &actions)
{
    if (!m_subtitleZoomAdjustment)
        return false;

    bool handled = true;

    if (has_action(ACTION_LEFT, actions))
        ChangeSubtitleZoom(ctx, -1);
    else if (has_action(ACTION_RIGHT, actions))
        ChangeSubtitleZoom(ctx, 1);
    else if (has_action(ACTION_UP, actions))
        ChangeSubtitleZoom(ctx, 10);
    else if (has_action(ACTION_DOWN, actions))
        ChangeSubtitleZoom(ctx, -10);
    else if (has_action(ACTION_TOGGLESUBTITLEZOOM, actions) ||
             has_action(ACTION_SELECT, actions))
        ClearOSD(ctx);
    else
        handled = false;

    return handled;
}

bool TV::SubtitleDelayHandleAction(PlayerContext *ctx,
                                   const QStringList &actions)
{
    if (!m_subtitleDelayAdjustment)
        return false;

    bool handled = true;

    if (has_action(ACTION_LEFT, actions))
        ChangeSubtitleDelay(ctx, -5);
    else if (has_action(ACTION_RIGHT, actions))
        ChangeSubtitleDelay(ctx, 5);
    else if (has_action(ACTION_UP, actions))
        ChangeSubtitleDelay(ctx, 25);
    else if (has_action(ACTION_DOWN, actions))
        ChangeSubtitleDelay(ctx, -25);
    else if (has_action(ACTION_TOGGLESUBTITLEDELAY, actions) ||
             has_action(ACTION_SELECT, actions))
        ClearOSD(ctx);
    else
        handled = false;

    return handled;
}

bool TV::DiscMenuHandleAction(PlayerContext *ctx, const QStringList &actions)
{
    int64_t pts = 0;
    MythVideoOutput *output = ctx->m_player->GetVideoOutput();
    if (output)
    {
        VideoFrame *frame = output->GetLastShownFrame();
        if (frame)
        {
            // convert timecode (msec) to pts (90kHz)
            pts = (int64_t)(frame->timecode  * 90);
        }
    }
    return ctx->m_buffer->HandleAction(actions, pts);
}

bool TV::Handle3D(PlayerContext *ctx, const QString &action)
{
    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (ctx->m_player && ctx->m_player->GetVideoOutput() &&
        ctx->m_player->GetVideoOutput()->StereoscopicModesAllowed())
    {
        StereoscopicMode mode = kStereoscopicModeNone;
        if (ACTION_3DSIDEBYSIDE == action)
            mode = kStereoscopicModeSideBySide;
        else if (ACTION_3DSIDEBYSIDEDISCARD == action)
            mode = kStereoscopicModeSideBySideDiscard;
        else if (ACTION_3DTOPANDBOTTOM == action)
            mode = kStereoscopicModeTopAndBottom;
        else if (ACTION_3DTOPANDBOTTOMDISCARD == action)
            mode = kStereoscopicModeTopAndBottomDiscard;
        ctx->m_player->GetVideoOutput()->SetStereoscopicMode(mode);
        SetOSDMessage(ctx, StereoscopictoString(mode));
    }
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);
    return true;
}

bool TV::ActiveHandleAction(PlayerContext *ctx,
                            const QStringList &actions,
                            bool isDVD, bool isDVDStill)
{
    bool handled = true;

    if (has_action("SKIPCOMMERCIAL", actions) && !isDVD)
        DoSkipCommercials(ctx, 1);
    else if (has_action("SKIPCOMMBACK", actions) && !isDVD)
        DoSkipCommercials(ctx, -1);
    else if (has_action("QUEUETRANSCODE", actions) && !isDVD)
        DoQueueTranscode(ctx, "Default");
    else if (has_action("QUEUETRANSCODE_AUTO", actions) && !isDVD)
        DoQueueTranscode(ctx, "Autodetect");
    else if (has_action("QUEUETRANSCODE_HIGH", actions)  && !isDVD)
        DoQueueTranscode(ctx, "High Quality");
    else if (has_action("QUEUETRANSCODE_MEDIUM", actions) && !isDVD)
        DoQueueTranscode(ctx, "Medium Quality");
    else if (has_action("QUEUETRANSCODE_LOW", actions) && !isDVD)
        DoQueueTranscode(ctx, "Low Quality");
    else if (has_action(ACTION_PLAY, actions))
        DoPlay(ctx);
    else if (has_action(ACTION_PAUSE, actions))
        DoTogglePause(ctx, true);
    else if (has_action("SPEEDINC", actions) && !isDVDStill)
        ChangeSpeed(ctx, 1);
    else if (has_action("SPEEDDEC", actions) && !isDVDStill)
        ChangeSpeed(ctx, -1);
    else if (has_action("ADJUSTSTRETCH", actions))
        ChangeTimeStretch(ctx, 0);   // just display
    else if (has_action("CYCLECOMMSKIPMODE",actions) && !isDVD)
        SetAutoCommercialSkip(ctx, kCommSkipIncr);
    else if (has_action("NEXTSCAN", actions))
    {
        ctx->LockDeletePlayer(__FILE__, __LINE__);
        FrameScanType scan = ctx->m_player->NextScanOverride();
        ctx->UnlockDeletePlayer(__FILE__, __LINE__);
        OverrideScan(ctx, scan);
    }
    else if (has_action(ACTION_SEEKARB, actions) && !isDVD)
    {
        if (m_asInputMode)
        {
            ClearInputQueues(ctx, true);
            SetOSDText(ctx, "osd_input", "osd_number_entry", tr("Seek:"),
                       kOSDTimeout_Med);

            QMutexLocker locker(&m_timerIdLock);
            m_asInputMode = false;
            if (m_asInputTimerId)
            {
                KillTimer(m_asInputTimerId);
                m_asInputTimerId = 0;
            }
        }
        else
        {
            ClearInputQueues(ctx, false);
            AddKeyToInputQueue(ctx, 0);

            QMutexLocker locker(&m_timerIdLock);
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
    else if (has_action(ACTION_JUMPRWND, actions))
        DoJumpRWND(ctx);
    else if (has_action(ACTION_JUMPFFWD, actions))
        DoJumpFFWD(ctx);
    else if (has_action(ACTION_JUMPBKMRK, actions))
    {
        ctx->LockDeletePlayer(__FILE__, __LINE__);
        uint64_t bookmark  = ctx->m_player->GetBookmark();
        ctx->UnlockDeletePlayer(__FILE__, __LINE__);

        if (bookmark)
        {
            DoPlayerSeekToFrame(ctx, bookmark);
            ctx->LockDeletePlayer(__FILE__, __LINE__);
            UpdateOSDSeekMessage(ctx, tr("Jump to Bookmark"), kOSDTimeout_Med);
            ctx->UnlockDeletePlayer(__FILE__, __LINE__);
        }
    }
    else if (has_action(ACTION_JUMPSTART,actions))
    {
        DoSeek(ctx, 0, tr("Jump to Beginning"),
               /*timeIsOffset*/false,
               /*honorCutlist*/true);
    }
    else if (has_action(ACTION_CLEAROSD, actions))
    {
        ClearOSD(ctx);
    }
    else if (has_action(ACTION_VIEWSCHEDULED, actions))
        EditSchedule(ctx, kViewSchedule);
    else if (HandleJumpToProgramAction(ctx, actions))
    { // NOLINT(bugprone-branch-clone)
    }
    else if (has_action(ACTION_SIGNALMON, actions))
    {
        if ((GetState(ctx) == kState_WatchingLiveTV) && ctx->m_recorder)
        {
            QString input = ctx->m_recorder->GetInput();
            uint timeout  = ctx->m_recorder->GetSignalLockTimeout(input);

            if (timeout == 0xffffffff)
            {
                SetOSDMessage(ctx, "No Signal Monitor");
                return false;
            }

            int rate   = m_sigMonMode ? 0 : 100;
            bool notify = !m_sigMonMode;

            PauseLiveTV(ctx);
            ctx->m_recorder->SetSignalMonitoringRate(rate, notify);
            UnpauseLiveTV(ctx);

            m_lockTimerOn = false;
            m_sigMonMode  = !m_sigMonMode;
        }
    }
    else if (has_action(ACTION_SCREENSHOT, actions))
    {
        MythMainWindow::ScreenShot();
    }
    else if (has_action(ACTION_STOP, actions))
    {
        PrepareToExitPlayer(ctx, __LINE__);
        SetExitPlayer(true, true);
    }
    else if (has_action(ACTION_EXITSHOWNOPROMPTS, actions))
    {
        m_requestDelete = false;
        PrepareToExitPlayer(ctx, __LINE__);
        SetExitPlayer(true, true);
    }
    else if (has_action("ESCAPE", actions) ||
             has_action("BACK", actions))
    {
        if (StateIsLiveTV(ctx->GetState()) &&
            (ctx->m_lastSignalMsgTime.elapsed() <
             (int)PlayerContext::kSMExitTimeout))
        {
            ClearOSD(ctx);
        }
        else
        {
            OSD *osd = GetOSDLock(ctx);
            if (osd && osd->IsVisible())
            {
                ClearOSD(ctx);
                ReturnOSDLock(ctx, osd);
                return handled;
            }
            ReturnOSDLock(ctx, osd);
        }

        NormalSpeed(ctx);

        StopFFRew(ctx);

        bool do_exit = false;

        if (StateIsLiveTV(GetState(ctx)))
        {
            if (ctx->HasPlayer() && (12 & m_dbPlaybackExitPrompt))
            {
                ShowOSDStopWatchingRecording(ctx);
                return handled;
            }
            do_exit = true;
        }
        else
        {
            if (ctx->HasPlayer() && (5 & m_dbPlaybackExitPrompt) &&
                !m_underNetworkControl && !isDVDStill)
            {
                ShowOSDStopWatchingRecording(ctx);
                return handled;
            }
            PrepareToExitPlayer(ctx, __LINE__);
            m_requestDelete = false;
            do_exit = true;
        }

        if (do_exit)
        {
            PlayerContext *mctx = GetPlayer(ctx, 0);
            if (mctx != ctx)
            { // A PIP is active, just tear it down..
                PxPTeardownView(ctx);
                return handled;
            }

            // If it's a DVD, and we're not trying to execute a
            // jumppoint, try to back up.
            if (isDVD &&
                !GetMythMainWindow()->IsExitingToMain() &&
                has_action("BACK", actions) &&
                ctx->m_buffer && ctx->m_buffer->DVD()->GoBack())
            {
                return handled;
            }
            SetExitPlayer(true, true);
        }

        SetActive(ctx, 0, false);
    }
    else if (has_action(ACTION_ENABLEUPMIX, actions))
        EnableUpmix(ctx, true);
    else if (has_action(ACTION_DISABLEUPMIX, actions))
        EnableUpmix(ctx, false);
    else if (has_action(ACTION_VOLUMEDOWN, actions))
        ChangeVolume(ctx, false);
    else if (has_action(ACTION_VOLUMEUP, actions))
        ChangeVolume(ctx, true);
    else if (has_action("CYCLEAUDIOCHAN", actions))
        ToggleMute(ctx, true);
    else if (has_action(ACTION_MUTEAUDIO, actions))
        ToggleMute(ctx);
    else if (has_action("STRETCHINC", actions))
        ChangeTimeStretch(ctx, 1);
    else if (has_action("STRETCHDEC", actions))
        ChangeTimeStretch(ctx, -1);
    else if (has_action("MENU", actions))
        ShowOSDMenu();
    else if (has_action(ACTION_MENUCOMPACT, actions))
        ShowOSDMenu(true);
    else if (has_action("INFO", actions) ||
             has_action("INFOWITHCUTLIST", actions))
    {
        if (HasQueuedInput())
        {
            DoArbSeek(ctx, ARBSEEK_SET,
                      has_action("INFOWITHCUTLIST", actions));
        }
        else
            ToggleOSD(ctx, true);
    }
    else if (has_action(ACTION_TOGGLEOSDDEBUG, actions))
        ToggleOSDDebug(ctx);
    else if (!isDVDStill && SeekHandleAction(ctx, actions, isDVD))
    {
    }
    else
    {
        handled = false;
        for (auto it = actions.cbegin(); it != actions.cend() && !handled; ++it)
            handled = HandleTrackAction(ctx, *it);
    }

    return handled;
}

bool TV::FFRewHandleAction(PlayerContext *ctx, const QStringList &actions)
{
    bool handled = false;

    if (ctx->m_ffRewState)
    {
        for (int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            bool ok = false;
            int val = action.toInt(&ok);

            if (ok && val < (int)m_ffRewSpeeds.size())
            {
                SetFFRew(ctx, val);
                handled = true;
            }
        }

        if (!handled)
        {
            DoPlayerSeek(ctx, StopFFRew(ctx));
            UpdateOSDSeekMessage(ctx, ctx->GetPlayMessage(), kOSDTimeout_Short);
            handled = true;
        }
    }

    if (ctx->m_ffRewSpeed)
    {
        NormalSpeed(ctx);
        UpdateOSDSeekMessage(ctx, ctx->GetPlayMessage(), kOSDTimeout_Short);
        handled = true;
    }

    return handled;
}

bool TV::ToggleHandleAction(PlayerContext *ctx,
                            const QStringList &actions, bool isDVD)
{
    bool handled = true;
    bool islivetv = StateIsLiveTV(GetState(ctx));

    if (has_action(ACTION_BOTTOMLINEMOVE, actions))
        ToggleMoveBottomLine(ctx);
    else if (has_action(ACTION_BOTTOMLINESAVE, actions))
        SaveBottomLine(ctx);
    else if (has_action("TOGGLEASPECT", actions))
        ToggleAspectOverride(ctx);
    else if (has_action("TOGGLEFILL", actions))
        ToggleAdjustFill(ctx);
    else if (has_action(ACTION_TOGGELAUDIOSYNC, actions))
        ChangeAudioSync(ctx, 0);   // just display
    else if (has_action(ACTION_TOGGLESUBTITLEZOOM, actions))
        ChangeSubtitleZoom(ctx, 0);   // just display
    else if (has_action(ACTION_TOGGLESUBTITLEDELAY, actions))
        ChangeSubtitleDelay(ctx, 0);   // just display
    else if (has_action(ACTION_TOGGLEVISUALISATION, actions))
        EnableVisualisation(ctx, false, true /*toggle*/);
    else if (has_action(ACTION_ENABLEVISUALISATION, actions))
        EnableVisualisation(ctx, true);
    else if (has_action(ACTION_DISABLEVISUALISATION, actions))
        EnableVisualisation(ctx, false);
    else if (has_action("TOGGLEPICCONTROLS", actions))
        DoTogglePictureAttribute(ctx, kAdjustingPicture_Playback);
    else if (has_action(ACTION_TOGGLENIGHTMODE, actions))
        DoToggleNightMode(ctx);
    else if (has_action("TOGGLESTRETCH", actions))
        ToggleTimeStretch(ctx);
    else if (has_action(ACTION_TOGGLEUPMIX, actions))
        EnableUpmix(ctx, false, true);
    else if (has_action(ACTION_TOGGLESLEEP, actions))
        ToggleSleepTimer(ctx);
    else if (has_action(ACTION_TOGGLERECORD, actions) && islivetv)
        QuickRecord(ctx);
    else if (has_action(ACTION_TOGGLEFAV, actions) && islivetv)
        ToggleChannelFavorite(ctx);
    else if (has_action(ACTION_TOGGLECHANCONTROLS, actions) && islivetv)
        DoTogglePictureAttribute(ctx, kAdjustingPicture_Channel);
    else if (has_action(ACTION_TOGGLERECCONTROLS, actions) && islivetv)
        DoTogglePictureAttribute(ctx, kAdjustingPicture_Recording);
    else if (has_action("TOGGLEBROWSE", actions))
    {
        if (islivetv)
            m_browseHelper->BrowseStart(ctx);
        else if (!isDVD)
            ShowOSDMenu();
        else
            handled = false;
    }
    else if (has_action("EDIT", actions))
    {
        if (islivetv)
            StartChannelEditMode(ctx);
        else if (!isDVD)
            StartProgramEditMode(ctx);
    }
    else if (has_action(ACTION_OSDNAVIGATION, actions))
        StartOsdNavigation(ctx);
    else
        handled = false;

    return handled;
}

void TV::EnableVisualisation(const PlayerContext *ctx, bool enable,
                            bool toggle, const QString &action)
{
    QString visualiser = QString("");
    if (action.startsWith("VISUALISER"))
        visualiser = action.mid(11);

    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (ctx->m_player && ctx->m_player->CanVisualise())
    {
        if (visualiser.isEmpty())
            visualiser = gCoreContext->GetSetting("AudioVisualiser", "");
        bool want = enable || !visualiser.isEmpty();
        if (toggle && visualiser.isEmpty())
            want = !ctx->m_player->IsVisualising();
        bool on = ctx->m_player->EnableVisualisation(want, visualiser);
        SetOSDMessage(ctx, on ? ctx->m_player->GetVisualiserName() :
                                tr("Visualisation Off"));
    }
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);
}

bool TV::PxPHandleAction(PlayerContext *ctx, const QStringList &actions)
{
    if (!IsPIPSupported(ctx) && !IsPBPSupported(ctx))
        return false;

    bool handled = true;
    {
        QMutexLocker locker(&m_timerIdLock);

        if (has_action("TOGGLEPIPMODE", actions))
            m_changePxP.enqueue("TOGGLEPIPMODE");
        else if (has_action("TOGGLEPBPMODE", actions))
            m_changePxP.enqueue("TOGGLEPBPMODE");
        else if (has_action("CREATEPIPVIEW", actions))
            m_changePxP.enqueue("CREATEPIPVIEW");
        else if (has_action("CREATEPBPVIEW", actions))
            m_changePxP.enqueue("CREATEPBPVIEW");
        else if (has_action("SWAPPIP", actions))
            m_changePxP.enqueue("SWAPPIP");
        else if (has_action("TOGGLEPIPSTATE", actions))
            m_changePxP.enqueue("TOGGLEPIPSTATE");
        else
            handled = false;

        if (!m_changePxP.empty() && !m_pipChangeTimerId)
            m_pipChangeTimerId = StartTimer(1, __LINE__);
    }

    if (has_action("NEXTPIPWINDOW", actions))
    {
        SetActive(ctx, -1, true);
        handled = true;
    }

    return handled;
}

void TV::SetBookmark(PlayerContext *ctx, bool clear)
{
    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (ctx->m_player)
    {
        if (clear)
        {
            ctx->m_player->SetBookmark(true);
            SetOSDMessage(ctx, tr("Bookmark Cleared"));
        }
        else // if (IsBookmarkAllowed(ctx))
        {
            ctx->m_player->SetBookmark();
            osdInfo info;
            ctx->CalcPlayerSliderPosition(info);
            info.text["title"] = tr("Position");
            UpdateOSDStatus(ctx, info, kOSDFunctionalType_Default,
                            kOSDTimeout_Med);
            SetOSDMessage(ctx, tr("Bookmark Saved"));
        }
    }
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);
}

bool TV::ActivePostQHandleAction(PlayerContext *ctx, const QStringList &actions)
{
    bool handled = true;
    TVState state = GetState(ctx);
    bool islivetv = StateIsLiveTV(state);
    bool isdvd  = state == kState_WatchingDVD;
    bool isdisc = isdvd || state == kState_WatchingBD;

    if (has_action(ACTION_SETBOOKMARK, actions))
    {
        if (!CommitQueuedInput(ctx))
        {
            ctx->LockDeletePlayer(__FILE__, __LINE__);
            SetBookmark(ctx, false);
            ctx->UnlockDeletePlayer(__FILE__, __LINE__);
        }
    }
    if (has_action(ACTION_TOGGLEBOOKMARK, actions))
    {
        if (!CommitQueuedInput(ctx))
        {
            ctx->LockDeletePlayer(__FILE__, __LINE__);
            SetBookmark(ctx, ctx->m_player->GetBookmark());
            ctx->UnlockDeletePlayer(__FILE__, __LINE__);
        }
    }
    else if (has_action("NEXTFAV", actions) && islivetv)
        ChangeChannel(ctx, CHANNEL_DIRECTION_FAVORITE);
    else if (has_action("NEXTSOURCE", actions) && islivetv)
        SwitchSource(ctx, kNextSource);
    else if (has_action("PREVSOURCE", actions) && islivetv)
        SwitchSource(ctx, kPreviousSource);
    else if (has_action("NEXTINPUT", actions) && islivetv)
        SwitchInputs(ctx);
    else if (has_action(ACTION_GUIDE, actions))
        EditSchedule(ctx, kScheduleProgramGuide);
    else if (has_action("PREVCHAN", actions) && islivetv)
        PopPreviousChannel(ctx, false);
    else if (has_action(ACTION_CHANNELUP, actions))
    {
        if (islivetv)
        {
            if (m_dbBrowseAlways)
                m_browseHelper->BrowseDispInfo(ctx, BROWSE_UP);
            else
                ChangeChannel(ctx, CHANNEL_DIRECTION_UP);
        }
        else
            DoJumpRWND(ctx);
    }
    else if (has_action(ACTION_CHANNELDOWN, actions))
    {
        if (islivetv)
        {
            if (m_dbBrowseAlways)
                m_browseHelper->BrowseDispInfo(ctx, BROWSE_DOWN);
            else
                ChangeChannel(ctx, CHANNEL_DIRECTION_DOWN);
        }
        else
            DoJumpFFWD(ctx);
    }
    else if (has_action("DELETE", actions) && !islivetv)
    {
        NormalSpeed(ctx);
        StopFFRew(ctx);
        SetBookmark(ctx);
        ShowOSDPromptDeleteRecording(ctx, tr("Are you sure you want to delete:"));
    }
    else if (has_action(ACTION_JUMPTODVDROOTMENU, actions) && isdisc)
    {
        ctx->LockDeletePlayer(__FILE__, __LINE__);
        if (ctx->m_player)
            ctx->m_player->GoToMenu("root");
        ctx->UnlockDeletePlayer(__FILE__, __LINE__);
    }
    else if (has_action(ACTION_JUMPTODVDCHAPTERMENU, actions) && isdisc)
    {
        ctx->LockDeletePlayer(__FILE__, __LINE__);
        if (ctx->m_player)
            ctx->m_player->GoToMenu("chapter");
        ctx->UnlockDeletePlayer(__FILE__, __LINE__);
    }
    else if (has_action(ACTION_JUMPTODVDTITLEMENU, actions) && isdisc)
    {
        ctx->LockDeletePlayer(__FILE__, __LINE__);
        if (ctx->m_player)
            ctx->m_player->GoToMenu("title");
        ctx->UnlockDeletePlayer(__FILE__, __LINE__);
    }
    else if (has_action(ACTION_JUMPTOPOPUPMENU, actions) && isdisc)
    {
        ctx->LockDeletePlayer(__FILE__, __LINE__);
        if (ctx->m_player)
            ctx->m_player->GoToMenu("popup");
        ctx->UnlockDeletePlayer(__FILE__, __LINE__);
    }
    else if (has_action(ACTION_FINDER, actions))
        EditSchedule(ctx, kScheduleProgramFinder);
    else
        handled = false;

    return handled;
}


void TV::ProcessNetworkControlCommand(PlayerContext *ctx,
                                      const QString &command)
{
    bool ignoreKeys = ctx->IsPlayerChangingBuffers();
#ifdef DEBUG_ACTIONS
    LOG(VB_GENERAL, LOG_DEBUG, LOC +
                    QString("(%1) ignoreKeys: %2").arg(command).arg(ignoreKeys));
#endif

    if (ignoreKeys)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
                "Ignoring network control command"
                "\n\t\t\tbecause ignoreKeys is set");
        return;
    }

    QStringList tokens = command.split(" ", QString::SkipEmptyParts);
    if (tokens.size() < 2)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Not enough tokens"
                "in network control command" + "\n\t\t\t" +
                QString("'%1'").arg(command));
        return;
    }

    OSD *osd = GetOSDLock(ctx);
    bool dlg = false;
    if (osd)
        dlg = osd->DialogVisible();
    ReturnOSDLock(ctx, osd);

    if (dlg)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Ignoring network control command\n\t\t\t" +
            QString("because dialog is waiting for a response"));
        return;
    }

    if (tokens[1] != "QUERY")
        ClearOSD(ctx);

    if (tokens.size() == 3 && tokens[1] == "CHANID")
    {
        m_queuedChanID = tokens[2].toUInt();
        m_queuedChanNum.clear();
        CommitQueuedInput(ctx);
    }
    else if (tokens.size() == 3 && tokens[1] == "CHANNEL")
    {
        if (StateIsLiveTV(GetState(ctx)))
        {
            if (tokens[2] == "UP")
                ChangeChannel(ctx, CHANNEL_DIRECTION_UP);
            else if (tokens[2] == "DOWN")
                ChangeChannel(ctx, CHANNEL_DIRECTION_DOWN);
            else if (tokens[2].contains(QRegExp("^[-\\.\\d_#]+$")))
                ChangeChannel(ctx, 0, tokens[2]);
        }
    }
    else if (tokens.size() == 3 && tokens[1] == "SPEED")
    {
        bool paused = ContextIsPaused(ctx, __FILE__, __LINE__);

        if (tokens[2] == "0x")
        {
            NormalSpeed(ctx);
            StopFFRew(ctx);
            if (!paused)
                DoTogglePause(ctx, true);
        }
        else if (tokens[2] == "normal")
        {
            NormalSpeed(ctx);
            StopFFRew(ctx);
            if (paused)
                DoTogglePause(ctx, true);
            return;
        }
        else
        {
            float tmpSpeed = 1.0F;
            bool ok = false;

            if (tokens[2].contains(QRegExp("^\\-*\\d+x$")))
            {
                QString speed = tokens[2].left(tokens[2].length()-1);
                tmpSpeed = speed.toFloat(&ok);
            }
            else if (tokens[2].contains(QRegExp("^\\-*\\d*\\.\\d+x$")))
            {
                QString speed = tokens[2].left(tokens[2].length() - 1);
                tmpSpeed = speed.toFloat(&ok);
            }
            else
            {
                QRegExp re = QRegExp("^(\\-*\\d+)\\/(\\d+)x$");
                if (tokens[2].contains(re))
                {
                    QStringList matches = re.capturedTexts();
                    int numerator = matches[1].toInt(&ok);
                    int denominator = matches[2].toInt(&ok);

                    if (ok && denominator != 0)
                    {
                        tmpSpeed = static_cast<float>(numerator) /
                                   static_cast<float>(denominator);
                    }
                    else
                    {
                        ok = false;
                    }
                }
            }

            if (ok)
            {
                float searchSpeed = fabs(tmpSpeed);

                if (paused)
                    DoTogglePause(ctx, true);

                if (tmpSpeed == 0.0F)
                {
                    NormalSpeed(ctx);
                    StopFFRew(ctx);

                    if (!paused)
                        DoTogglePause(ctx, true);
                }
                else if (tmpSpeed == 1.0F)
                {
                    StopFFRew(ctx);
                    ctx->m_tsNormal = 1.0F;
                    ChangeTimeStretch(ctx, 0, false);
                    return;
                }

                NormalSpeed(ctx);

                size_t index = 0;
                for ( ; index < m_ffRewSpeeds.size(); index++)
                    if (float(m_ffRewSpeeds[index]) == searchSpeed)
                        break;

                if ((index < m_ffRewSpeeds.size()) &&
                    (float(m_ffRewSpeeds[index]) == searchSpeed))
                {
                    if (tmpSpeed < 0)
                        ctx->m_ffRewState = -1;
                    else if (tmpSpeed > 1)
                        ctx->m_ffRewState = 1;
                    else
                        StopFFRew(ctx);

                    if (ctx->m_ffRewState)
                        SetFFRew(ctx, index);
                }
                else if (0.48F <= tmpSpeed && tmpSpeed <= 2.0F) {
                    StopFFRew(ctx);

                    ctx->m_tsNormal = tmpSpeed;   // alter speed before display
                    ChangeTimeStretch(ctx, 0, false);
                }
                else
                {
                    LOG(VB_GENERAL, LOG_WARNING,
                        QString("Couldn't find %1 speed. Setting Speed to 1x")
                            .arg(searchSpeed));

                    ctx->m_ffRewState = 0;
                    SetFFRew(ctx, kInitFFRWSpeed);
                }
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR,
                    QString("Found an unknown speed of %1").arg(tokens[2]));
            }
        }
    }
    else if (tokens.size() == 2 && tokens[1] == "STOP")
    {
        SetBookmark(ctx);
        ctx->LockDeletePlayer(__FILE__, __LINE__);
        if (ctx->m_player && m_dbAutoSetWatched)
            ctx->m_player->SetWatched();
        ctx->UnlockDeletePlayer(__FILE__, __LINE__);
        SetExitPlayer(true, true);
    }
    else if (tokens.size() >= 3 && tokens[1] == "SEEK" && ctx->HasPlayer())
    {
        if (ctx->m_buffer && ctx->m_buffer->IsInDiscMenuOrStillFrame())
            return;

        if (tokens[2] == "BEGINNING")
        {
            DoSeek(ctx, 0, tr("Jump to Beginning"),
                   /*timeIsOffset*/false,
                   /*honorCutlist*/true);
        }
        else if (tokens[2] == "FORWARD")
        {
            DoSeek(ctx, ctx->m_fftime, tr("Skip Ahead"),
                   /*timeIsOffset*/true,
                   /*honorCutlist*/true);
        }
        else if (tokens[2] == "BACKWARD")
        {
            DoSeek(ctx, -ctx->m_rewtime, tr("Skip Back"),
                   /*timeIsOffset*/true,
                   /*honorCutlist*/true);
        }
        else if ((tokens[2] == "POSITION" ||
                  tokens[2] == "POSITIONWITHCUTLIST") &&
                 (tokens.size() == 4) &&
                 (tokens[3].contains(QRegExp("^\\d+$"))))
        {
            DoSeekAbsolute(ctx, tokens[3].toInt(),
                           tokens[2] == "POSITIONWITHCUTLIST");
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
            ctx->m_player->SetCaptionsEnabled(false, true);
        }
        else
        {
            uint start = 1;
            QStringList subs = ctx->m_player->GetTracks(kTrackTypeSubtitle);
            uint finish = start + subs.size();
            if (track >= start && track < finish)
            {
                ctx->m_player->SetTrack(kTrackTypeSubtitle, track - start);
                ctx->m_player->EnableCaptions(kDisplayAVSubtitle);
                return;
            }

            start = finish + 1;
            subs = ctx->m_player->GetTracks(kTrackTypeCC708);
            finish = start + subs.size();
            if (track >= start && track < finish)
            {
                ctx->m_player->SetTrack(kTrackTypeCC708, track - start);
                ctx->m_player->EnableCaptions(kDisplayCC708);
                return;
            }

            start = finish + 1;
            subs = ctx->m_player->GetTracks(kTrackTypeCC608);
            finish = start + subs.size();
            if (track >= start && track < finish)
            {
                ctx->m_player->SetTrack(kTrackTypeCC608, track - start);
                ctx->m_player->EnableCaptions(kDisplayCC608);
                return;
            }

            start = finish + 1;
            subs = ctx->m_player->GetTracks(kTrackTypeTeletextCaptions);
            finish = start + subs.size();
            if (track >= start && track < finish)
            {
                ctx->m_player->SetTrack(kTrackTypeTeletextCaptions, track-start);
                ctx->m_player->EnableCaptions(kDisplayTeletextCaptions);
                return;
            }

            start = finish + 1;
            subs = ctx->m_player->GetTracks(kTrackTypeTeletextMenu);
            finish = start + subs.size();
            if (track >= start && track < finish)
            {
                ctx->m_player->SetTrack(kTrackTypeTeletextMenu, track - start);
                ctx->m_player->EnableCaptions(kDisplayTeletextMenu);
                return;
            }

            start = finish + 1;
            subs = ctx->m_player->GetTracks(kTrackTypeRawText);
            finish = start + subs.size();
            if (track >= start && track < finish)
            {
                ctx->m_player->SetTrack(kTrackTypeRawText, track - start);
                ctx->m_player->EnableCaptions(kDisplayRawTextSubtitle);
                return;
            }
        }
    }
    else if (tokens.size() >= 3 && tokens[1] == "VOLUME")
    {
        QRegExp re = QRegExp("(\\d+)%");
        if (tokens[2].contains(re))
        {
            QStringList matches = re.capturedTexts();

            LOG(VB_GENERAL, LOG_INFO, QString("Set Volume to %1%")
                    .arg(matches[1]));

            bool ok = false;

            int vol = matches[1].toInt(&ok);

            if (!ok)
                return;

            if (0 <= vol && vol <= 100)
            {
                ctx->LockDeletePlayer(__FILE__, __LINE__);
                if (!ctx->m_player)
                {
                    ctx->UnlockDeletePlayer(__FILE__, __LINE__);
                    return;
                }

                vol -= ctx->m_player->GetVolume();
                vol = ctx->m_player->AdjustVolume(vol);
                ctx->UnlockDeletePlayer(__FILE__, __LINE__);

                if (!m_browseHelper->IsBrowsing() && !m_editMode)
                {
                    UpdateOSDStatus(
                        ctx, tr("Adjust Volume"), tr("Volume"),
                        QString::number(vol),
                        kOSDFunctionalType_PictureAdjust, "%", vol * 10,
                        kOSDTimeout_Med);
                    SetUpdateOSDPosition(false);
                }
            }
        }
    }
    else if (tokens.size() >= 3 && tokens[1] == "QUERY")
    {
        if (tokens[2] == "POSITION")
        {
            if (!ctx->m_player)
                return;
            QString speedStr;
            if (ContextIsPaused(ctx, __FILE__, __LINE__))
            {
                speedStr = "pause";
            }
            else if (ctx->m_ffRewState)
            {
                speedStr = QString("%1x").arg(ctx->m_ffRewSpeed);
            }
            else
            {
                QRegExp re = QRegExp("Play (.*)x");
                if (ctx->GetPlayMessage().contains(re))
                {
                    QStringList matches = re.capturedTexts();
                    speedStr = QString("%1x").arg(matches[1]);
                }
                else
                {
                    speedStr = "1x";
                }
            }

            osdInfo info;
            ctx->CalcPlayerSliderPosition(info, true);

            QDateTime respDate = MythDate::current(true);
            QString infoStr = "";

            ctx->LockDeletePlayer(__FILE__, __LINE__);
            long long fplay = 0;
            float     rate  = 30.0F;
            if (ctx->m_player)
            {
                fplay = ctx->m_player->GetFramesPlayed();
                rate  = ctx->m_player->GetFrameRate(); // for display only
            }
            ctx->UnlockDeletePlayer(__FILE__, __LINE__);

            ctx->LockPlayingInfo(__FILE__, __LINE__);
            if (ctx->GetState() == kState_WatchingLiveTV)
            {
                infoStr = "LiveTV";
                if (ctx->m_playingInfo)
                    respDate = ctx->m_playingInfo->GetScheduledStartTime();
            }
            else
            {
                if (ctx->m_buffer && ctx->m_buffer->IsDVD())
                    infoStr = "DVD";
                else if (ctx->m_playingInfo->IsRecording())
                    infoStr = "Recorded";
                else
                    infoStr = "Video";

                if (ctx->m_playingInfo)
                    respDate = ctx->m_playingInfo->GetRecordingStartTime();
            }

            QString bufferFilename =
              ctx->m_buffer ? ctx->m_buffer->GetFilename() : QString("no buffer");
            if ((infoStr == "Recorded") || (infoStr == "LiveTV"))
            {
                infoStr += QString(" %1 %2 %3 %4 %5 %6 %7")
                    .arg(info.text["description"])
                    .arg(speedStr)
                    .arg(ctx->m_playingInfo != nullptr ?
                         ctx->m_playingInfo->GetChanID() : 0)
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

            uint subtype = ctx->m_player->GetCaptionMode();

            if (subtype == kDisplayNone)
                infoStr += QString(" *0:[None]*");
            else
                infoStr += QString(" 0:[None]");

            uint n = 1;

            QStringList subs = ctx->m_player->GetTracks(kTrackTypeSubtitle);
            for (uint i = 0; i < (uint)subs.size(); i++)
            {
                if ((subtype & kDisplayAVSubtitle) &&
                    (ctx->m_player->GetTrack(kTrackTypeSubtitle) == (int)i))
                {
                    infoStr += QString(" *%1:[%2]*").arg(n).arg(subs[i]);
                }
                else
                {
                    infoStr += QString(" %1:[%2]").arg(n).arg(subs[i]);
                }
                n++;
            }

            subs = ctx->m_player->GetTracks(kTrackTypeCC708);
            for (uint i = 0; i < (uint)subs.size(); i++)
            {
                if ((subtype & kDisplayCC708) &&
                    (ctx->m_player->GetTrack(kTrackTypeCC708) == (int)i))
                {
                    infoStr += QString(" *%1:[%2]*").arg(n).arg(subs[i]);
                }
                else
                {
                    infoStr += QString(" %1:[%2]").arg(n).arg(subs[i]);
                }
                n++;
            }

            subs = ctx->m_player->GetTracks(kTrackTypeCC608);
            for (uint i = 0; i < (uint)subs.size(); i++)
            {
                if ((subtype & kDisplayCC608) &&
                    (ctx->m_player->GetTrack(kTrackTypeCC608) == (int)i))
                {
                    infoStr += QString(" *%1:[%2]*").arg(n).arg(subs[i]);
                }
                else
                {
                    infoStr += QString(" %1:[%2]").arg(n).arg(subs[i]);
                }
                n++;
            }

            subs = ctx->m_player->GetTracks(kTrackTypeTeletextCaptions);
            for (uint i = 0; i < (uint)subs.size(); i++)
            {
                if ((subtype & kDisplayTeletextCaptions) &&
                    (ctx->m_player->GetTrack(kTrackTypeTeletextCaptions)==(int)i))
                {
                    infoStr += QString(" *%1:[%2]*").arg(n).arg(subs[i]);
                }
                else
                {
                    infoStr += QString(" %1:[%2]").arg(n).arg(subs[i]);
                }
                n++;
            }

            subs = ctx->m_player->GetTracks(kTrackTypeTeletextMenu);
            for (uint i = 0; i < (uint)subs.size(); i++)
            {
                if ((subtype & kDisplayTeletextMenu) &&
                    ctx->m_player->GetTrack(kTrackTypeTeletextMenu) == (int)i)
                {
                    infoStr += QString(" *%1:[%2]*").arg(n).arg(subs[i]);
                }
                else
                {
                    infoStr += QString(" %1:[%2]").arg(n).arg(subs[i]);
                }
                n++;
            }

            subs = ctx->m_player->GetTracks(kTrackTypeRawText);
            for (uint i = 0; i < (uint)subs.size(); i++)
            {
                if ((subtype & kDisplayRawTextSubtitle) &&
                    ctx->m_player->GetTrack(kTrackTypeRawText) == (int)i)
                {
                    infoStr += QString(" *%1:[%2]*").arg(n).arg(subs[i]);
                }
                else
                {
                    infoStr += QString(" %1:[%2]").arg(n).arg(subs[i]);
                }
                n++;
            }

            ctx->UnlockPlayingInfo(__FILE__, __LINE__);

            QString message = QString("NETWORK_CONTROL ANSWER %1")
                                .arg(infoStr);
            MythEvent me(message);
            gCoreContext->dispatch(me);
        }
        else if (tokens[2] == "VOLUME")
        {
            if (!ctx->m_player)
                return;
            QString infoStr = QString("%1%").arg(ctx->m_player->GetVolume());

            QString message = QString("NETWORK_CONTROL ANSWER %1")
                .arg(infoStr);
            MythEvent me(message);
            gCoreContext->dispatch(me);
        }
    }
}

/**
 * \brief Setup Picture by Picture. right side will be the current video.
 * \param ctx  Current player context
 * \param info programinfo for PBP to use for left Picture. is nullptr for Live TV
 */
bool TV::CreatePBP(PlayerContext *ctx, const ProgramInfo *info)
{
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "-- begin");

    if (m_player.size() > 1)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Only allowed when player.size() == 1");
        return false;
    }

    PlayerContext *mctx = GetPlayer(ctx, 0);
    if (!IsPBPSupported(mctx))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "PBP not supported by video method.");
        return false;
    }

    if (!mctx->m_player)
        return false;
    mctx->LockDeletePlayer(__FILE__, __LINE__);
    long long mctx_frame = mctx->m_player->GetFramesPlayed();
    mctx->UnlockDeletePlayer(__FILE__, __LINE__);

    // This is safe because we are already holding lock for a ctx
    m_player.push_back(new PlayerContext(kPBPPlayerInUseID));
    PlayerContext *pbpctx = m_player.back();
    // see comment in CreatePIP on disabling hardware acceleration for secondary players
    //if (m_noHardwareDecoders)
        pbpctx->SetNoHardwareDecoders();
    pbpctx->SetPIPState(kPBPRight);

    if (info)
    {
        pbpctx->SetPlayingInfo(info);
        pbpctx->SetInitialTVState(false);
        ScheduleStateChange(pbpctx);
    }
    else if (RequestNextRecorder(pbpctx, false))
    {
        pbpctx->SetInitialTVState(true);
        ScheduleStateChange(pbpctx);
    }
    else
    {
        delete m_player.back();
        m_player.pop_back();
        return false;
    }

    mctx->PIPTeardown();
    mctx->SetPIPState(kPBPLeft);
    if (mctx->m_buffer)
        mctx->m_buffer->Seek(0, SEEK_SET);

    if (StateIsLiveTV(mctx->GetState()))
        if (mctx->m_buffer)
            mctx->m_buffer->Unpause();

    bool ok = mctx->CreatePlayer(
        this, GetMythMainWindow(), mctx->GetState(), false);

    if (ok)
    {
        ScheduleStateChange(mctx);
        mctx->LockDeletePlayer(__FILE__, __LINE__);
        if (mctx->m_player)
            mctx->m_player->JumpToFrame(mctx_frame);
        mctx->UnlockDeletePlayer(__FILE__, __LINE__);
        SetSpeedChangeTimer(25, __LINE__);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to restart new main context");
        // Make putative PBP context the main context
        swap(m_player[0],m_player[1]);
        m_player[0]->SetPIPState(kPIPOff);
        // End the old main context..
        ForceNextStateNone(mctx);
    }

    LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
        QString("-- end : %1").arg(ok));
    return ok;
}

/**
 * \brief create PIP.
 * \param ctx  Current player context
 * \param info programinfo for PIP to create. is nullptr for LiveTV PIP
 */
bool TV::CreatePIP(PlayerContext *ctx, const ProgramInfo *info)
{
    PlayerContext *mctx = GetPlayer(ctx, 0);
    if (!mctx)
        return false;

    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "-- begin");

    if (mctx->IsPBP())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "CreatePIP called, but we're in PBP mode already, ignoring.");
        return false;
    }

    if (!IsPIPSupported(mctx))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "PiP not supported by video method.");
        return false;
    }

    auto *pipctx = new PlayerContext(kPIPPlayerInUseID);
    // Hardware acceleration of PiP is currently disabled as the null video
    // renderer cannot deal with hardware codecs which are returned by the display
    // profile. The workaround would be to encourage the decoder, when using null
    // video, to change the decoder to a decode only version - which will work
    // with null video
    //if (m_noHardwareDecoders)
        pipctx->SetNoHardwareDecoders();
    pipctx->SetNullVideo(true);
    pipctx->SetPIPState(kPIPonTV);
    if (info)
    {
        pipctx->SetPlayingInfo(info);
        pipctx->SetInitialTVState(false);
        ScheduleStateChange(pipctx);
    }
    else if (RequestNextRecorder(pipctx, false))
    {
        pipctx->SetInitialTVState(true);
        ScheduleStateChange(pipctx);
    }
    else
    {
        delete pipctx;
        return false;
    }

    // this is safe because we are already holding lock for ctx
    m_player.push_back(pipctx);

    return true;
}

int TV::find_player_index(const PlayerContext *ctx) const
{
    for (size_t i = 0; i < m_player.size(); i++)
        if (GetPlayer(ctx, i) == ctx)
            return i;
    return -1;
}

bool TV::StartPlayer(PlayerContext *mctx, PlayerContext *ctx,
                     TVState desiredState)
{
    bool wantPiP = ctx->IsPIP();

    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("(%1, %2, %3) -- begin")
            .arg(find_player_index(ctx)).arg(StateToString(desiredState))
            .arg((wantPiP) ? "PiP" : "main"));

    LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("Elapsed time since TV constructor was called: %1 ms")
            .arg(m_ctorTime.elapsed()));

    if (wantPiP)
    {
        if (mctx->HasPlayer() && ctx->StartPIPPlayer(this, desiredState) &&
            ctx->HasPlayer() && PIPAddPlayer(mctx, ctx))
        {
            ScheduleStateChange(ctx);
            LOG(VB_GENERAL, LOG_DEBUG, "PiP -- end : ok");
            return true;
        }

        ForceNextStateNone(ctx);
        LOG(VB_GENERAL, LOG_ERR, "PiP -- end : !ok");
        return false;
    }

    bool ok = false;
    if (ctx->IsNullVideoDesired())
    {
        ok = ctx->CreatePlayer(this, nullptr, desiredState, false);
        ScheduleStateChange(ctx);
        if (ok)
            ok = PIPAddPlayer(mctx, ctx);
    }
    else
    {
        ok = ctx->CreatePlayer(this, GetMythMainWindow(), desiredState, false);
        ScheduleStateChange(ctx);
    }

    if (ok)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Created player."));
        SetSpeedChangeTimer(25, __LINE__);
    }
    else
        LOG(VB_GENERAL, LOG_CRIT, LOC + QString("Failed to create player."));

    LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
        QString("(%1, %2, %3) -- end %4")
            .arg(find_player_index(ctx)).arg(StateToString(desiredState))
            .arg((wantPiP) ? "PiP" : "main").arg((ok) ? "ok" : "error"));

    return ok;
}

/// \brief Maps Player of software scaled PIP to the main player
bool TV::PIPAddPlayer(PlayerContext *mctx, PlayerContext *pipctx)
{
    if (!mctx || !pipctx)
        return false;

    if (!mctx->IsPlayerPlaying())
        return false;

    bool ok = false;
    bool addCondition = false;
    bool is_using_null = false;
    pipctx->LockDeletePlayer(__FILE__, __LINE__);
    if (pipctx->m_player)
    {
        is_using_null = pipctx->m_player->UsingNullVideo();
        pipctx->UnlockDeletePlayer(__FILE__, __LINE__);

        if (is_using_null)
        {
            addCondition = true;
            multi_lock(&mctx->m_deletePlayerLock, &pipctx->m_deletePlayerLock, nullptr);
            if (mctx->m_player && pipctx->m_player)
            {
                PIPLocation loc = mctx->m_player->GetNextPIPLocation();
                if (loc != kPIP_END)
                    ok = mctx->m_player->AddPIPPlayer(pipctx->m_player, loc);
            }
            mctx->m_deletePlayerLock.unlock();
            pipctx->m_deletePlayerLock.unlock();
        }
        else if (pipctx->IsPIP())
        {
            ok = ResizePIPWindow(pipctx);
        }
    }
    else
        pipctx->UnlockDeletePlayer(__FILE__, __LINE__);

    LOG(VB_GENERAL, LOG_ERR,
        QString("AddPIPPlayer null: %1 IsPIP: %2 addCond: %3 ok: %4")
            .arg(is_using_null)
            .arg(pipctx->IsPIP()).arg(addCondition).arg(ok));

    return ok;
}

/// \brief Unmaps Player of software scaled PIP from the main player
bool TV::PIPRemovePlayer(PlayerContext *mctx, PlayerContext *pipctx)
{
    if (!mctx || !pipctx)
        return false;

    bool ok = false;
    multi_lock(&mctx->m_deletePlayerLock, &pipctx->m_deletePlayerLock, nullptr);
    if (mctx->m_player && pipctx->m_player)
        ok = mctx->m_player->RemovePIPPlayer(pipctx->m_player);
    mctx->m_deletePlayerLock.unlock();
    pipctx->m_deletePlayerLock.unlock();

    LOG(VB_GENERAL, LOG_INFO, QString("PIPRemovePlayer ok: %1").arg(ok));

    return ok;
}

/// \brief start/stop PIP/PBP
void TV::PxPToggleView(PlayerContext *actx, bool wantPBP)
{
    if (wantPBP && !IsPBPSupported(actx))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "-- end: PBP not supported by video method.");
        return;
    }

    if (m_player.size() <= 1)
        PxPCreateView(actx, wantPBP);
    else
        PxPTeardownView(actx);
}

/// \brief start PIP/PBP
void TV::PxPCreateView(PlayerContext *actx, bool wantPBP)
{
    if (!actx)
        return;

    QString err_msg;
    if ((m_player.size() > kMaxPBPCount) && (wantPBP || actx->IsPBP()))
    {
        err_msg = tr("Sorry, PBP only supports %n video stream(s)", "",
                     kMaxPBPCount);
    }

    if ((m_player.size() > kMaxPIPCount) &&
        (!wantPBP || GetPlayer(actx,1)->IsPIP()))
    {
        err_msg = tr("Sorry, PIP only supports %n video stream(s)", "",
                     kMaxPIPCount);
    }

    if ((m_player.size() > 1) && (wantPBP ^ actx->IsPBP()))
        err_msg = tr("Sorry, cannot mix PBP and PIP views");

    if (!err_msg.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + err_msg);
        SetOSDMessage(actx, err_msg);
        return;
    }

    bool ok = false;
    if (wantPBP)
        ok = CreatePBP(actx, nullptr);
    else
        ok = CreatePIP(actx, nullptr);
    actx = GetPlayer(actx, -1); // CreatePBP/PIP mess with ctx's

    QString msg = (ok) ?
        ((wantPBP) ? tr("Creating PBP")      : tr("Creating PIP")) :
        ((wantPBP) ? tr("Cannot create PBP") : tr("Cannot create PIP"));

    SetOSDMessage(actx, msg);
}

/// \brief stop PIP/PBP
void TV::PxPTeardownView(PlayerContext *actx)
{
    LOG(VB_GENERAL, LOG_INFO, "PxPTeardownView()");

    QString msg;
    PlayerContext *mctx = GetPlayer(actx, 0);
    PlayerContext *dctx = nullptr;
    dctx = (mctx != actx)       ? actx               : dctx;
    dctx = (2 == m_player.size()) ? GetPlayer(actx, 1) : dctx;

    SetActive(actx, 0, false);

    PlayerContext *ctx1 = GetPlayer(actx, 1);
    msg = (ctx1->IsPIP()) ? tr("Stopping PIP") : tr("Stopping PBP");
    if (dctx)
    {
        ForceNextStateNone(dctx);
    }
    else
    {
        if (m_player.size() > 2)
        {
            msg = (ctx1->IsPIP()) ?
                tr("Stopping all PIPs") : tr("Stopping all PBPs");
        }

        for (uint i = m_player.size() - 1; i > 0; i--)
            ForceNextStateNone(GetPlayer(actx,i));
    }

    SetOSDMessage(mctx, msg);
}

/**
* \brief Change PIP View from PIP to PBP and visa versa
*/
void TV::PxPToggleType(PlayerContext *mctx, bool wantPBP)
{
    const QString before = (mctx->IsPBP()) ? "PBP" : "PIP";
    const QString after  = (wantPBP)       ? "PBP" : "PIP";

    // TODO renderer may change depending on display profile
    //      check for support in new renderer
    if (wantPBP && !IsPBPSupported(mctx))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "-- end: PBP not supported by video method.");
        return;
    }


    LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
        QString("converting from %1 to %2 -- begin")
            .arg(before).arg(after));

    if (mctx->IsPBP() == wantPBP)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "-- end: already in desired mode");
        return;
    }

    uint max_cnt = min(kMaxPBPCount, kMaxPIPCount+1);
    if (m_player.size() > max_cnt)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("-- end: # player contexts must be %1 or "
                    "less, but it is currently %1")
                .arg(max_cnt).arg(m_player.size()));

        QString err_msg = tr("Too many views to switch");

        PlayerContext *actx = GetPlayer(mctx, -1);
        SetOSDMessage(actx, err_msg);
        return;
    }

    for (size_t i = 0; i < m_player.size(); i++)
    {
        PlayerContext *ctx = GetPlayer(mctx, i);
        if (!ctx->IsPlayerPlaying())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "-- end: " +
                    QString("player #%1 is not active, exiting without "
                            "doing anything to avoid danger").arg(i));
            return;
        }
    }

    MuteState mctx_mute = kMuteOff;
    mctx->LockDeletePlayer(__FILE__, __LINE__);
    if (mctx->m_player)
        mctx_mute = mctx->m_player->GetMuteState();
    mctx->UnlockDeletePlayer(__FILE__, __LINE__);

    vector<long long> pos = TeardownAllPlayers(mctx);

    if (wantPBP)
    {
        GetPlayer(mctx, 0)->SetPIPState(kPBPLeft);
        if (m_player.size() > 1)
            GetPlayer(mctx, 1)->SetPIPState(kPBPRight);
    }
    else
    {
        GetPlayer(mctx, 0)->SetPIPState(kPIPOff);
        for (size_t i = 1; i < m_player.size(); i++)
        {
            GetPlayer(mctx, i)->SetPIPState(kPIPonTV);
            GetPlayer(mctx, i)->SetNullVideo(true);
        }
    }

    RestartAllPlayers(mctx, pos, mctx_mute);

    LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
        QString("converting from %1 to %2 -- end")
            .arg(before).arg(after));
}

/**
 * \brief resize PIP Window. done when changing channels or swapping PIP
 */
bool TV::ResizePIPWindow(PlayerContext *ctx)
{
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "-- begin");
    PlayerContext *mctx = GetPlayer(ctx, 0);
    if (mctx->HasPlayer() && ctx->HasPlayer())
    {
        QRect rect;

        multi_lock(&mctx->m_deletePlayerLock, &ctx->m_deletePlayerLock, (QMutex*)nullptr);
        if (mctx->m_player && ctx->m_player)
        {
            PIPLocation loc = mctx->m_player->GetNextPIPLocation();
            LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("-- loc %1")
                    .arg(loc));
            if (loc != kPIP_END)
            {
                rect = mctx->m_player->GetVideoOutput()->GetPIPRect(
                    loc, ctx->m_player, false);
            }
        }
        mctx->UnlockDeletePlayer(__FILE__, __LINE__);
        ctx->UnlockDeletePlayer(__FILE__, __LINE__);

        if (rect.isValid())
        {
            ctx->ResizePIPWindow(rect);
            LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "-- end : ok");
            return true;
        }
    }
    LOG(VB_PLAYBACK, LOG_ERR, LOC + "-- end : !ok");
    return false;
}

bool TV::IsPBPSupported(const PlayerContext *ctx) const
{
    const PlayerContext *mctx = nullptr;
    if (ctx)
        mctx = GetPlayer(ctx, 0);
    else
        mctx = GetPlayerReadLock(0, __FILE__, __LINE__);

    bool yes = mctx->IsPBPSupported();

    if (!ctx)
        ReturnPlayerLock(mctx);

    return yes;
}

bool TV::IsPIPSupported(const PlayerContext *ctx) const
{
    const PlayerContext *mctx = nullptr;
    if (ctx)
        mctx = GetPlayer(ctx, 0);
    else
        mctx = GetPlayerReadLock(0, __FILE__, __LINE__);

    bool yes = mctx->IsPIPSupported();

    if (!ctx)
        ReturnPlayerLock(mctx);

    return yes;
}

/** \brief Teardown all Player's in preparation for PxP Swap or
 *         change from PIP -> PBP or PBP -> PIP
 */
vector<long long> TV::TeardownAllPlayers(PlayerContext *lctx)
{
    vector<long long> pos;
    for (size_t i = 0; i < m_player.size(); i++)
    {
        const PlayerContext *ctx = GetPlayer(lctx, i);
        ctx->LockDeletePlayer(__FILE__, __LINE__);
        pos.push_back((ctx->m_player) ? ctx->m_player->GetFramesPlayed() : 0);
        ctx->UnlockDeletePlayer(__FILE__, __LINE__);
    }

    for (size_t i = 0; i < m_player.size(); i++)
    {
        PlayerContext *ctx = GetPlayer(lctx, i);
        ctx->PIPTeardown();
    }

    return pos;
}

/**
 * \brief tear down remaining PBP video and restore
 * fullscreen display
 */
void TV::PBPRestartMainPlayer(PlayerContext *mctx)
{
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC  + "-- begin");

    if (!mctx->IsPlayerPlaying() ||
        mctx->GetPIPState() != kPBPLeft || m_exitPlayerTimerId)
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC +
            "-- end !ok !valid");
        return;
    }

    mctx->LockDeletePlayer(__FILE__, __LINE__);
    long long mctx_frame = (mctx->m_player) ? mctx->m_player->GetFramesPlayed() : 0;
    mctx->UnlockDeletePlayer(__FILE__, __LINE__);

    mctx->PIPTeardown();
    mctx->SetPIPState(kPIPOff);
    if (mctx->m_buffer)
        mctx->m_buffer->Seek(0, SEEK_SET);

    if (mctx->CreatePlayer(this, GetMythMainWindow(), mctx->GetState(), false))
    {
        ScheduleStateChange(mctx);
        mctx->LockDeletePlayer(__FILE__, __LINE__);
        if (mctx->m_player)
            mctx->m_player->JumpToFrame(mctx_frame);
        mctx->UnlockDeletePlayer(__FILE__, __LINE__);
        SetSpeedChangeTimer(25, __LINE__);
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "-- end ok");
        return;
    }

    ForceNextStateNone(mctx);
    LOG(VB_PLAYBACK, LOG_ERR, LOC + "-- end !ok Player did not restart");
}

/**
* \brief Recreate Main and PIP windows. Could be either PIP or PBP views.
*/
void TV::RestartAllPlayers(PlayerContext *lctx,
                        const vector<long long> &pos,
                        MuteState mctx_mute)
{
    PlayerContext *mctx = GetPlayer(lctx, 0);

    if (!mctx)
        return;

    if (mctx->m_buffer) {
        mctx->m_buffer->Seek(0, SEEK_SET);
        if (StateIsLiveTV(mctx->GetState()))
            mctx->m_buffer->Unpause();
    }

    mctx->SetNullVideo(false);
    mctx->SetNoHardwareDecoders(false);
    bool ok = StartPlayer(mctx, mctx, mctx->GetState());

    if (ok)
    {
        mctx->LockDeletePlayer(__FILE__, __LINE__);
        if (mctx->m_player)
            mctx->m_player->JumpToFrame(pos[0]);
        mctx->UnlockDeletePlayer(__FILE__, __LINE__);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
                "Failed to restart new main context (was pip context)");
        ForceNextStateNone(mctx);
        return;
    }

    for (size_t i = 1; i < m_player.size(); i++)
    {
        PlayerContext *pipctx = GetPlayer(lctx, i);

        if (pipctx->m_buffer) {
            pipctx->m_buffer->Seek(0, SEEK_SET);
            if (StateIsLiveTV(pipctx->GetState()))
                pipctx->m_buffer->Unpause();
        }

        pipctx->SetNullVideo(true);
        pipctx->SetNoHardwareDecoders(true);

        // if the main context is using a hardware decoder, it will callback to
        // the main thread at some point while holding avcodeclock, which causes
        // a deadlock. If we can't obtain the lock, try and process the callback.
        while (!avcodeclock->tryLock(10))
        {
            mctx->LockDeletePlayer(__FILE__, __LINE__);
            if (mctx->m_player)
                mctx->m_player->ProcessCallbacks();
            mctx->UnlockDeletePlayer(__FILE__, __LINE__);
        }

        ok = StartPlayer(mctx, pipctx, pipctx->GetState());

        if (ok)
        {
            pipctx->LockDeletePlayer(__FILE__, __LINE__);
            if (pipctx->m_player)
            {
                pipctx->m_player->SetMuted(true);
                pipctx->m_player->JumpToFrame(pos[i]);
            }
            pipctx->UnlockDeletePlayer(__FILE__, __LINE__);
        }
        else
        { // TODO print OSD informing user of Swap failure ?
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "Failed to restart new pip context (was main context)");
            ForceNextStateNone(pipctx);
        }

        avcodeclock->unlock();
    }

    // If old main player had a kMuteAll | kMuteOff setting,
    // apply old main player's mute setting to new main player.
    mctx->LockDeletePlayer(__FILE__, __LINE__);
    if (mctx->m_player && ((kMuteAll == mctx_mute) || (kMuteOff == mctx_mute)))
        mctx->m_player->SetMuteState(mctx_mute);
    mctx->UnlockDeletePlayer(__FILE__, __LINE__);
}

void TV::PxPSwap(PlayerContext *mctx, PlayerContext *pipctx)
{
    if (!mctx || !pipctx)
        return;

    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "-- begin");
    if (mctx == pipctx)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "-- need two contexts");
        return;
    }

    m_lockTimerOn = false;

    multi_lock(&mctx->m_deletePlayerLock, &pipctx->m_deletePlayerLock, nullptr);
    if (!mctx->m_player   || !mctx->m_player->IsPlaying() ||
        !pipctx->m_player || !pipctx->m_player->IsPlaying())
    {
        mctx->m_deletePlayerLock.unlock();
        pipctx->m_deletePlayerLock.unlock();
        LOG(VB_GENERAL, LOG_ERR, LOC + "-- a player is not playing");
        return;
    }

    MuteState mctx_mute = mctx->m_player->GetMuteState();
    mctx->m_deletePlayerLock.unlock();
    pipctx->m_deletePlayerLock.unlock();

    int ctx_index = find_player_index(pipctx);

    if (ctx_index < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "-- failed to find player index by context");
        return;
    }

    vector<long long> pos = TeardownAllPlayers(mctx);

    swap(m_player[0],         m_player[ctx_index]);
    swap(pos[0],              pos[ctx_index]);
    swap(m_player[0]->m_pipState, m_player[ctx_index]->m_pipState);
    m_playerActive = (ctx_index == m_playerActive) ?
        0 : ((ctx_index == 0) ? ctx_index : m_playerActive);

    RestartAllPlayers(mctx, pos, mctx_mute);

    SetActive(mctx, m_playerActive, false);

    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "-- end");
}

void TV::RestartMainPlayer(PlayerContext *mctx)
{
    if (!mctx)
        return;

    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "-- begin");
    m_lockTimerOn = false;

    mctx->LockDeletePlayer(__FILE__, __LINE__);
    if (!mctx->m_player)
    {
        mctx->m_deletePlayerLock.unlock();
        return;
    }

//  MuteState mctx_mute = mctx->m_player->GetMuteState();
    MuteState mctx_mute = kMuteOff; //FOR HACK

    // HACK - FIXME
    // workaround muted audio when Player is re-created
    mctx_mute = kMuteOff;
    // FIXME - end
    mctx->m_deletePlayerLock.unlock();

    vector<long long> pos = TeardownAllPlayers(mctx);
    RestartAllPlayers(mctx, pos, mctx_mute);
    SetActive(mctx, m_playerActive, false);

    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "-- end");
}

void TV::DoPlay(PlayerContext *ctx)
{
    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (!ctx->m_player)
    {
        ctx->UnlockDeletePlayer(__FILE__, __LINE__);
        return;
    }

    float time = 0.0;

    if (ctx->m_ffRewState || (ctx->m_ffRewSpeed != 0) ||
        ctx->m_player->IsPaused())
    {
        if (ctx->m_ffRewState)
            time = StopFFRew(ctx);
        else if (ctx->m_player->IsPaused())
            SendMythSystemPlayEvent("PLAY_UNPAUSED", ctx->m_playingInfo);

        ctx->m_player->Play(ctx->m_tsNormal, true);
        gCoreContext->emitTVPlaybackUnpaused();
        ctx->m_ffRewSpeed = 0;
    }
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);

    DoPlayerSeek(ctx, time);
    UpdateOSDSeekMessage(ctx, ctx->GetPlayMessage(), kOSDTimeout_Med);

    MythUIHelper::DisableScreensaver();

    SetSpeedChangeTimer(0, __LINE__);
    gCoreContext->emitTVPlaybackPlaying();
    UpdateNavDialog(ctx);
}

float TV::DoTogglePauseStart(PlayerContext *ctx)
{
    if (!ctx)
        return 0.0F;

    if (ctx->m_buffer && ctx->m_buffer->IsInDiscMenuOrStillFrame())
        return 0.0F;

    ctx->m_ffRewSpeed = 0;
    float time = 0.0F;

    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (!ctx->m_player)
    {
        ctx->UnlockDeletePlayer(__FILE__, __LINE__);
        return 0.0F;
    }
    if (ctx->m_player->IsPaused())
    {
        ctx->m_player->Play(ctx->m_tsNormal, true);
    }
    else
    {
        if (ctx->m_ffRewState)
            time = StopFFRew(ctx);
        ctx->m_player->Pause();
    }
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);
    return time;
}

void TV::DoTogglePauseFinish(PlayerContext *ctx, float time, bool showOSD)
{
    if (!ctx || !ctx->HasPlayer())
        return;

    if (ctx->m_buffer && ctx->m_buffer->IsInDiscMenuOrStillFrame())
        return;

    if (ContextIsPaused(ctx, __FILE__, __LINE__))
    {
        if (ctx->m_buffer)
            ctx->m_buffer->WaitForPause();

        DoPlayerSeek(ctx, time);

        if (showOSD && ctx == m_player[0])
            UpdateOSDSeekMessage(ctx, tr("Paused"), kOSDTimeout_None);
        else if (showOSD)
            UpdateOSDSeekMessage(ctx, tr("Aux Paused"), kOSDTimeout_None);

        RestoreScreenSaver(ctx);
    }
    else
    {
        DoPlayerSeek(ctx, time);
        if (showOSD)
            UpdateOSDSeekMessage(ctx, ctx->GetPlayMessage(), kOSDTimeout_Short);
        MythUIHelper::DisableScreensaver();
    }

    SetSpeedChangeTimer(0, __LINE__);
}

/**
 * \fn bool TV::IsPaused(void) [static]
 * Returns true if a TV playback is currently going; otherwise returns false
 */
bool TV::IsPaused(void)
{
    if (!IsTVRunning())
        return false;

    QMutexLocker lock(gTVLock);
    PlayerContext *ctx = gTV->GetPlayerReadLock(0, __FILE__, __LINE__);

    if (!ctx || ctx->IsErrored())
    {
        gTV->ReturnPlayerLock(ctx);
        return false;
    }
    ctx->LockDeletePlayer(__FILE__, __LINE__);
    bool paused = false;
    if (ctx->m_player)
    {
        paused = ctx->m_player->IsPaused();
    }
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);
    gTV->ReturnPlayerLock(ctx);
    return paused;
}

void TV::DoTogglePause(PlayerContext *ctx, bool showOSD)
{
    bool ignore = false;
    bool paused = false;
    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (ctx->m_player)
    {
        ignore = ctx->m_player->GetEditMode();
        paused = ctx->m_player->IsPaused();
    }
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);

    if (paused)
        SendMythSystemPlayEvent("PLAY_UNPAUSED", ctx->m_playingInfo);
    else
        SendMythSystemPlayEvent("PLAY_PAUSED", ctx->m_playingInfo);

    if (!ignore)
        DoTogglePauseFinish(ctx, DoTogglePauseStart(ctx), showOSD);
    // Emit Pause or Unpaused signal
    paused ? gCoreContext->emitTVPlaybackUnpaused() : gCoreContext->emitTVPlaybackPaused();
    UpdateNavDialog(ctx);
}

void TV::UpdateNavDialog(PlayerContext *ctx)
{
    OSD *osd = GetOSDLock(ctx);
    if (osd && osd->DialogVisible(OSD_DLG_NAVIGATE))
    {
        osdInfo info;
        ctx->LockDeletePlayer(__FILE__, __LINE__);
        bool paused = (ctx->m_player
            && (ctx->m_ffRewState || ctx->m_ffRewSpeed != 0
                || ctx->m_player->IsPaused()));
        ctx->UnlockDeletePlayer(__FILE__, __LINE__);
        info.text["paused"] = (paused ? "Y" : "N");
        bool muted = ctx->m_player && ctx->m_player->IsMuted();
        info.text["muted"] = (muted ? "Y" : "N");
        osd->SetText(OSD_DLG_NAVIGATE, info.text, paused ? kOSDTimeout_None : kOSDTimeout_Long);
    }
    ReturnOSDLock(ctx, osd);
}

bool TV::DoPlayerSeek(PlayerContext *ctx, float time)
{
    if (!ctx || !ctx->m_buffer)
        return false;

    if (time > -0.001F && time < +0.001F)
        return false;

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("%1 seconds").arg(time));

    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (!ctx->m_player)
    {
        ctx->UnlockDeletePlayer(__FILE__, __LINE__);
        return false;
    }

    if (!ctx->m_buffer->IsSeekingAllowed())
    {
        ctx->UnlockDeletePlayer(__FILE__, __LINE__);
        return false;
    }

    if (ctx == GetPlayer(ctx, 0))
        PauseAudioUntilBuffered(ctx);

    bool res = false;

    if (time > 0.0F)
        res = ctx->m_player->FastForward(time);
    else if (time < 0.0F)
        res = ctx->m_player->Rewind(-time);
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);

    return res;
}

bool TV::DoPlayerSeekToFrame(PlayerContext *ctx, uint64_t target)
{
    if (!ctx || !ctx->m_buffer)
        return false;

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("%1").arg(target));

    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (!ctx->m_player)
    {
        ctx->UnlockDeletePlayer(__FILE__, __LINE__);
        return false;
    }

    if (!ctx->m_buffer->IsSeekingAllowed())
    {
        ctx->UnlockDeletePlayer(__FILE__, __LINE__);
        return false;
    }

    if (ctx == GetPlayer(ctx, 0))
        PauseAudioUntilBuffered(ctx);

    bool res = ctx->m_player->JumpToFrame(target);

    ctx->UnlockDeletePlayer(__FILE__, __LINE__);

    return res;
}

bool TV::SeekHandleAction(PlayerContext *actx, const QStringList &actions,
                          const bool isDVD)
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
    if (has_action(ACTION_SEEKFFWD, actions))
        flags = ARBSEEK_FORWARD | kForward | kSlippery | kRelative;
    else if (has_action("FFWDSTICKY", actions))
        flags = ARBSEEK_END     | kForward | kSticky   | kAbsolute;
    else if (has_action(ACTION_RIGHT, actions))
        flags = ARBSEEK_FORWARD | kForward | kSticky   | kRelative;
    else if (has_action(ACTION_SEEKRWND, actions))
        flags = ARBSEEK_REWIND  | kRewind  | kSlippery | kRelative;
    else if (has_action("RWNDSTICKY", actions))
        flags = ARBSEEK_SET     | kRewind  | kSticky   | kAbsolute;
    else if (has_action(ACTION_LEFT, actions))
        flags = ARBSEEK_REWIND  | kRewind  | kSticky   | kRelative;
    else
        return false;

    int direction = (flags & kRewind) ? -1 : 1;
    if (HasQueuedInput())
    {
        DoArbSeek(actx, static_cast<ArbSeekWhence>(flags & kWhenceMask),
                  (flags & kIgnoreCutlist) == 0);
    }
    else if (ContextIsPaused(actx, __FILE__, __LINE__))
    {
        if (!isDVD)
        {
            QString message = (flags & kRewind) ? tr("Rewind") :
                                                  tr("Forward");
            if (flags & kAbsolute) // FFWDSTICKY/RWNDSTICKY
            {
                float time = direction;
                DoSeek(actx, time, message,
                       /*timeIsOffset*/true,
                       /*honorCutlist*/(flags & kIgnoreCutlist) == 0);
            }
            else
            {
                actx->LockDeletePlayer(__FILE__, __LINE__);
                uint64_t frameAbs = actx->m_player->GetFramesPlayed();
                uint64_t frameRel =
                    actx->m_player->TranslatePositionAbsToRel(frameAbs);
                uint64_t targetRel = frameRel + direction;
                if (frameRel == 0 && direction < 0)
                    targetRel = 0;
                uint64_t maxAbs = actx->m_player->GetCurrentFrameCount();
                uint64_t maxRel =
                    actx->m_player->TranslatePositionAbsToRel(maxAbs);
                if (targetRel > maxRel)
                    targetRel = maxRel;
                uint64_t targetAbs =
                    actx->m_player->TranslatePositionRelToAbs(targetRel);
                actx->UnlockDeletePlayer(__FILE__, __LINE__);
                DoPlayerSeekToFrame(actx, targetAbs);
                UpdateOSDSeekMessage(actx, message, kOSDTimeout_Short);
            }
        }
    }
    else if (flags & kSticky)
    {
        ChangeFFRew(actx, direction);
    }
    else if (flags & kRewind)
    {
            if (m_smartForward)
                m_doSmartForward = true;
            DoSeek(actx, -actx->m_rewtime, tr("Skip Back"),
                   /*timeIsOffset*/true,
                   /*honorCutlist*/(flags & kIgnoreCutlist) == 0);
    }
    else
    {
        if (m_smartForward && m_doSmartForward)
        {
            DoSeek(actx, actx->m_rewtime, tr("Skip Ahead"),
                   /*timeIsOffset*/true,
                   /*honorCutlist*/(flags & kIgnoreCutlist) == 0);
        }
        else
        {
            DoSeek(actx, actx->m_fftime, tr("Skip Ahead"),
                   /*timeIsOffset*/true,
                   /*honorCutlist*/(flags & kIgnoreCutlist) == 0);
        }
    }
    UpdateNavDialog(actx);
    return true;
}

void TV::DoSeek(PlayerContext *ctx, float time, const QString &mesg,
                bool timeIsOffset, bool honorCutlist)
{
    if (!ctx->m_player)
        return;

    bool limitkeys = false;

    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (ctx->m_player->GetLimitKeyRepeat())
        limitkeys = true;

    if (!limitkeys || (m_keyRepeatTimer.elapsed() > (int)kKeyRepeatTimeout))
    {
        m_keyRepeatTimer.start();
        NormalSpeed(ctx);
        time += StopFFRew(ctx);
        if (timeIsOffset)
        {
            ctx->UnlockDeletePlayer(__FILE__, __LINE__);
            DoPlayerSeek(ctx, time);
        }
        else
        {
            uint64_t desiredFrameRel = ctx->m_player->
                TranslatePositionMsToFrame(time * 1000, honorCutlist);
            ctx->UnlockDeletePlayer(__FILE__, __LINE__);
            DoPlayerSeekToFrame(ctx, desiredFrameRel);
        }
        bool paused = ctx->m_player->IsPaused();
        UpdateOSDSeekMessage(ctx, mesg, paused ? kOSDTimeout_None : kOSDTimeout_Med);
    }
    else
        ctx->UnlockDeletePlayer(__FILE__, __LINE__);
}

void TV::DoSeekAbsolute(PlayerContext *ctx, long long seconds,
                        bool honorCutlist)
{
    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (!ctx->m_player)
    {
        ctx->UnlockDeletePlayer(__FILE__, __LINE__);
        gCoreContext->emitTVPlaybackSought((qint64)-1);
        return;
    }
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);
    DoSeek(ctx, seconds, tr("Jump To"),
           /*timeIsOffset*/false,
           honorCutlist);
    gCoreContext->emitTVPlaybackSought(seconds);
}

void TV::DoArbSeek(PlayerContext *ctx, ArbSeekWhence whence,
                   bool honorCutlist)
{
    bool ok = false;
    int seek = GetQueuedInputAsInt(&ok);
    ClearInputQueues(ctx, true);
    if (!ok)
        return;

    float time = (int(seek / 100) * 3600) + ((seek % 100) * 60);

    if (whence == ARBSEEK_FORWARD)
    {
        DoSeek(ctx, time, tr("Jump Ahead"),
               /*timeIsOffset*/true, honorCutlist);
    }
    else if (whence == ARBSEEK_REWIND)
    {
        DoSeek(ctx, -time, tr("Jump Back"),
               /*timeIsOffset*/true, honorCutlist);
    }
    else if (whence == ARBSEEK_END)
    {
        ctx->LockDeletePlayer(__FILE__, __LINE__);
        if (!ctx->m_player)
        {
            ctx->UnlockDeletePlayer(__FILE__, __LINE__);
            return;
        }
        uint64_t total_frames = ctx->m_player->GetCurrentFrameCount();
        float dur = ctx->m_player->ComputeSecs(total_frames, honorCutlist);
        time = max(0.0F, dur - time);
        ctx->UnlockDeletePlayer(__FILE__, __LINE__);
        DoSeek(ctx, time, tr("Jump To"), /*timeIsOffset*/false, honorCutlist);
    }
    else
        DoSeekAbsolute(ctx, time, honorCutlist);
}

void TV::NormalSpeed(PlayerContext *ctx)
{
    if (!ctx->m_ffRewSpeed)
        return;

    ctx->m_ffRewSpeed = 0;

    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (ctx->m_player)
        ctx->m_player->Play(ctx->m_tsNormal, true);
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);

    SetSpeedChangeTimer(0, __LINE__);
}

void TV::ChangeSpeed(PlayerContext *ctx, int direction)
{
    int old_speed = ctx->m_ffRewSpeed;

    if (ContextIsPaused(ctx, __FILE__, __LINE__))
        ctx->m_ffRewSpeed = -4;

    ctx->m_ffRewSpeed += direction;

    float time = StopFFRew(ctx);
    float speed = NAN;
    QString mesg;

    switch (ctx->m_ffRewSpeed)
    {
        case  4: speed = 16.0;     mesg = tr("Speed 16X");   break;
        case  3: speed = 8.0;      mesg = tr("Speed 8X");    break;
        case  2: speed = 3.0;      mesg = tr("Speed 3X");    break;
        case  1: speed = 2.0;      mesg = tr("Speed 2X");    break;
        case  0: speed = 1.0;      mesg = ctx->GetPlayMessage();      break;
        case -1: speed = 1.0 / 3;  mesg = tr("Speed 1/3X");  break;
        case -2: speed = 1.0 / 8;  mesg = tr("Speed 1/8X");  break;
        case -3: speed = 1.0 / 16; mesg = tr("Speed 1/16X"); break;
        case -4:
            DoTogglePause(ctx, true);
            return;
        default:
            ctx->m_ffRewSpeed = old_speed;
            return;
    }

    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (ctx->m_player && !ctx->m_player->Play(
            (!ctx->m_ffRewSpeed) ? ctx->m_tsNormal: speed, !ctx->m_ffRewSpeed))
    {
        ctx->m_ffRewSpeed = old_speed;
        ctx->UnlockDeletePlayer(__FILE__, __LINE__);
        return;
    }
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);
    DoPlayerSeek(ctx, time);
    UpdateOSDSeekMessage(ctx, mesg, kOSDTimeout_Med);

    SetSpeedChangeTimer(0, __LINE__);
}

float TV::StopFFRew(PlayerContext *ctx)
{
    float time = 0.0;

    if (!ctx->m_ffRewState)
        return time;

    if (ctx->m_ffRewState > 0)
        time = -m_ffRewSpeeds[ctx->m_ffRewIndex] * m_ffRewRepos;
    else
        time = m_ffRewSpeeds[ctx->m_ffRewIndex] * m_ffRewRepos;

    ctx->m_ffRewState = 0;
    ctx->m_ffRewIndex = kInitFFRWSpeed;

    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (ctx->m_player)
        ctx->m_player->Play(ctx->m_tsNormal, true);
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);

    SetSpeedChangeTimer(0, __LINE__);

    return time;
}

void TV::ChangeFFRew(PlayerContext *ctx, int direction)
{
    if (ctx->m_ffRewState == direction)
    {
        while (++ctx->m_ffRewIndex < (int)m_ffRewSpeeds.size())
            if (m_ffRewSpeeds[ctx->m_ffRewIndex])
                break;
        if (ctx->m_ffRewIndex >= (int)m_ffRewSpeeds.size())
            ctx->m_ffRewIndex = kInitFFRWSpeed;
        SetFFRew(ctx, ctx->m_ffRewIndex);
    }
    else if (!m_ffRewReverse && ctx->m_ffRewState == -direction)
    {
        while (--ctx->m_ffRewIndex >= kInitFFRWSpeed)
            if (m_ffRewSpeeds[ctx->m_ffRewIndex])
                break;
        if (ctx->m_ffRewIndex >= kInitFFRWSpeed)
            SetFFRew(ctx, ctx->m_ffRewIndex);
        else
        {
            float time = StopFFRew(ctx);
            DoPlayerSeek(ctx, time);
            UpdateOSDSeekMessage(ctx, ctx->GetPlayMessage(), kOSDTimeout_Med);
        }
    }
    else
    {
        NormalSpeed(ctx);
        ctx->m_ffRewState = direction;
        SetFFRew(ctx, kInitFFRWSpeed);
    }
}

void TV::SetFFRew(PlayerContext *ctx, int index)
{
    if (!ctx->m_ffRewState)
    {
        return;
    }

    if (!m_ffRewSpeeds[index])
    {
        return;
    }

    int speed = 0;
    QString mesg;
    if (ctx->m_ffRewState > 0)
    {
        speed = m_ffRewSpeeds[index];
        // Don't allow ffwd if seeking is needed but not available
        if (!ctx->m_buffer || (!ctx->m_buffer->IsSeekingAllowed() && speed > 3))
            return;

        ctx->m_ffRewIndex = index;
        mesg = tr("Forward %1X").arg(m_ffRewSpeeds[ctx->m_ffRewIndex]);
        ctx->m_ffRewSpeed = speed;
    }
    else
    {
        // Don't rewind if we cannot seek
        if (!ctx->m_buffer || !ctx->m_buffer->IsSeekingAllowed())
            return;

        ctx->m_ffRewIndex = index;
        mesg = tr("Rewind %1X").arg(m_ffRewSpeeds[ctx->m_ffRewIndex]);
        speed = -m_ffRewSpeeds[ctx->m_ffRewIndex];
        ctx->m_ffRewSpeed = speed;
    }

    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (ctx->m_player)
        ctx->m_player->Play((float)speed, (speed == 1) && (ctx->m_ffRewState > 0));
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);

    UpdateOSDSeekMessage(ctx, mesg, kOSDTimeout_None);

    SetSpeedChangeTimer(0, __LINE__);
}

void TV::DoQueueTranscode(PlayerContext *ctx, const QString& profile)
{
    ctx->LockPlayingInfo(__FILE__, __LINE__);

    if (ctx->GetState() == kState_WatchingPreRecorded)
    {
        bool stop = false;
        if (m_queuedTranscode ||
            JobQueue::IsJobQueuedOrRunning(
                     JOB_TRANSCODE,
                     ctx->m_playingInfo->GetChanID(),
                     ctx->m_playingInfo->GetRecordingStartTime()))
        {
            stop = true;
        }

        if (stop)
        {
            JobQueue::ChangeJobCmds(
                JOB_TRANSCODE,
                ctx->m_playingInfo->GetChanID(),
                ctx->m_playingInfo->GetRecordingStartTime(), JOB_STOP);
            m_queuedTranscode = false;
            SetOSDMessage(ctx, tr("Stopping Transcode"));
        }
        else
        {
            const RecordingInfo recinfo(*ctx->m_playingInfo);
            recinfo.ApplyTranscoderProfileChange(profile);
            QString jobHost = "";

            if (m_dbRunJobsOnRemote)
                jobHost = ctx->m_playingInfo->GetHostname();

            QString msg = tr("Try Again");
            if (JobQueue::QueueJob(JOB_TRANSCODE,
                       ctx->m_playingInfo->GetChanID(),
                       ctx->m_playingInfo->GetRecordingStartTime(),
                       jobHost, "", "", JOB_USE_CUTLIST))
            {
                m_queuedTranscode = true;
                msg = tr("Transcoding");
            }
            SetOSDMessage(ctx, msg);
        }
    }
    ctx->UnlockPlayingInfo(__FILE__, __LINE__);
}

int TV::GetNumChapters(const PlayerContext *ctx)
{
    int num_chapters = 0;
    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (ctx->m_player)
        num_chapters = ctx->m_player->GetNumChapters();
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);
    return num_chapters;
}

void TV::GetChapterTimes(const PlayerContext *ctx, QList<long long> &times)
{
    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (ctx->m_player)
        ctx->m_player->GetChapterTimes(times);
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);
}

int TV::GetCurrentChapter(const PlayerContext *ctx)
{
    int chapter = 0;
    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (ctx->m_player)
        chapter = ctx->m_player->GetCurrentChapter();
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);
    return chapter;
}

void TV::DoJumpChapter(PlayerContext *ctx, int chapter)
{
    NormalSpeed(ctx);
    StopFFRew(ctx);

    PauseAudioUntilBuffered(ctx);

    UpdateOSDSeekMessage(ctx, tr("Jump Chapter"), kOSDTimeout_Med);
    SetUpdateOSDPosition(true);

    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (ctx->m_player)
        ctx->m_player->JumpChapter(chapter);
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);
}

int TV::GetNumTitles(const PlayerContext *ctx)
{
    int num_titles = 0;
    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (ctx->m_player)
        num_titles = ctx->m_player->GetNumTitles();
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);
    return num_titles;
}

int TV::GetCurrentTitle(const PlayerContext *ctx)
{
    int currentTitle = 0;
    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (ctx->m_player)
        currentTitle = ctx->m_player->GetCurrentTitle();
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);
    return currentTitle;
}

int TV::GetNumAngles(const PlayerContext *ctx)
{
    int num_angles = 0;
    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (ctx->m_player)
        num_angles = ctx->m_player->GetNumAngles();
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);
    return num_angles;
}

int TV::GetCurrentAngle(const PlayerContext *ctx)
{
    int currentAngle = 0;
    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (ctx->m_player)
        currentAngle = ctx->m_player->GetCurrentAngle();
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);
    return currentAngle;
}

QString TV::GetAngleName(const PlayerContext *ctx, int angle)
{
    QString name;
    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (ctx->m_player)
        name = ctx->m_player->GetAngleName(angle);
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);
    return name;
}

int TV::GetTitleDuration(const PlayerContext *ctx, int title)
{
    int seconds = 0;
    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (ctx->m_player)
        seconds = ctx->m_player->GetTitleDuration(title);
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);
    return seconds;
}


QString TV::GetTitleName(const PlayerContext *ctx, int title)
{
    QString name;
    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (ctx->m_player)
        name = ctx->m_player->GetTitleName(title);
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);
    return name;
}

void TV::DoSwitchTitle(PlayerContext *ctx, int title)
{
    NormalSpeed(ctx);
    StopFFRew(ctx);

    PauseAudioUntilBuffered(ctx);

    UpdateOSDSeekMessage(ctx, tr("Switch Title"), kOSDTimeout_Med);
    SetUpdateOSDPosition(true);

    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (ctx->m_player)
        ctx->m_player->SwitchTitle(title);
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);
}

void TV::DoSwitchAngle(PlayerContext *ctx, int angle)
{
    NormalSpeed(ctx);
    StopFFRew(ctx);

    PauseAudioUntilBuffered(ctx);

    UpdateOSDSeekMessage(ctx, tr("Switch Angle"), kOSDTimeout_Med);
    SetUpdateOSDPosition(true);

    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (ctx->m_player)
        ctx->m_player->SwitchAngle(angle);
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);
}

void TV::DoSkipCommercials(PlayerContext *ctx, int direction)
{
    NormalSpeed(ctx);
    StopFFRew(ctx);

    if (StateIsLiveTV(GetState(ctx)))
        return;

    PauseAudioUntilBuffered(ctx);

    osdInfo info;
    ctx->CalcPlayerSliderPosition(info);
    info.text["title"] = tr("Skip");
    info.text["description"] = tr("Searching");
    UpdateOSDStatus(ctx, info, kOSDFunctionalType_Default, kOSDTimeout_Med);
    SetUpdateOSDPosition(true);

    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (ctx->m_player)
        ctx->m_player->SkipCommercials(direction);
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);
}

void TV::SwitchSource(PlayerContext *ctx, uint source_direction)
{
    QMap<uint,InputInfo> sources;
    uint         cardid  = ctx->GetCardID();

    InfoMap info;
    ctx->m_recorder->GetChannelInfo(info);
    uint sourceid = info["sourceid"].toUInt();

    vector<InputInfo> inputs = RemoteRequestFreeInputInfo(cardid);
    for (auto & input : inputs)
    {
        // prefer the current card's input in sources list
        if ((sources.find(input.m_sourceId) == sources.end()) ||
            ((cardid == input.m_inputId) &&
             (cardid != sources[input.m_sourceId].m_inputId)))
        {
            sources[input.m_sourceId] = input;
        }
    }

    // Source switching
    QMap<uint,InputInfo>::const_iterator beg = sources.find(sourceid);
    QMap<uint,InputInfo>::const_iterator sit = beg;

    if (sit == sources.end())
    {
        return;
    }

    if (kNextSource == source_direction)
    {
        ++sit;
        if (sit == sources.end())
            sit = sources.begin();
    }

    if (kPreviousSource == source_direction)
    {
        if (sit != sources.begin())
            --sit;
        else
        {
            QMap<uint,InputInfo>::const_iterator tmp = sources.begin();
            while (tmp != sources.end())
            {
                sit = tmp;
                ++tmp;
            }
        }
    }

    if (sit == beg)
    {
        return;
    }

    m_switchToInputId = (*sit).m_inputId;

    QMutexLocker locker(&m_timerIdLock);
    if (!m_switchToInputTimerId)
        m_switchToInputTimerId = StartTimer(1, __LINE__);
}

void TV::SwitchInputs(PlayerContext *ctx,
                      uint chanid, QString channum, uint inputid)
{
    if (!ctx->m_recorder)
        return;

    // this will re-create the player. Ensure any outstanding events are delivered
    // and processed before the player is deleted so that we don't confuse the
    // state of the new player e.g. when switching inputs from the guide grid,
    // "EPG_EXITING" may not be received until after the player is re-created
    // and we inadvertantly disable drawing...
    qApp->processEvents();

    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("(%1,'%2',%3)")
            .arg(chanid).arg(channum).arg(inputid));

    RemoteEncoder *testrec = nullptr;

    if (!StateIsLiveTV(GetState(ctx)))
        return;

    QStringList reclist;
    if (inputid)
    {
        reclist.push_back(QString::number(inputid));
    }
    else if (chanid || !channum.isEmpty())
    {
        // If we are switching to a channel not on the current recorder
        // we need to find the next free recorder with that channel.
        reclist = ChannelUtil::GetValidRecorderList(chanid, channum);
    }

    if (!reclist.empty())
        testrec = RemoteRequestFreeRecorderFromList(reclist, ctx->GetCardID());

    if (testrec && testrec->IsValidRecorder())
    {
        inputid = testrec->GetRecorderNumber();

        // We are switching to a specific channel...
        if (chanid && channum.isEmpty())
            channum = ChannelUtil::GetChanNum(chanid);

        if (!channum.isEmpty())
            CardUtil::SetStartChannel(inputid, channum);
    }

    // If we are just switching recorders find first available recorder.
    if (!testrec)
        testrec = RemoteRequestNextFreeRecorder(ctx->GetCardID());

    if (testrec && testrec->IsValidRecorder())
    {
        // Switching inputs so clear the pseudoLiveTVState.
        ctx->SetPseudoLiveTV(nullptr, kPseudoNormalLiveTV);

        PlayerContext *mctx = GetPlayer(ctx, 0);
        if (mctx != ctx)
            PIPRemovePlayer(mctx, ctx);

        bool muted = false;
        ctx->LockDeletePlayer(__FILE__, __LINE__);
        if (ctx->m_player && ctx->m_player->IsMuted())
            muted = true;
        ctx->UnlockDeletePlayer(__FILE__, __LINE__);

        // pause the decoder first, so we're not reading too close to the end.
        if (ctx->m_buffer) {
            ctx->m_buffer->IgnoreLiveEOF(true);
            ctx->m_buffer->StopReads();
        }
        if (ctx->m_player)
            ctx->m_player->PauseDecoder();

        // shutdown stuff
        if (ctx->m_buffer) {
            ctx->m_buffer->Pause();
            ctx->m_buffer->WaitForPause();
        }
        ctx->StopPlaying();
        ctx->m_recorder->StopLiveTV();
        ctx->SetPlayer(nullptr);

        // now restart stuff
        ctx->m_lastSignalUIInfo.clear();
        m_lockTimerOn = false;

        ctx->SetRecorder(testrec);
        ctx->m_recorder->Setup();
        // We need to set channum for SpawnLiveTV..
        if (channum.isEmpty() && chanid)
            channum = ChannelUtil::GetChanNum(chanid);
        if (channum.isEmpty() && inputid)
            channum = CardUtil::GetStartingChannel(inputid);
        ctx->m_recorder->SpawnLiveTV(ctx->m_tvchain->GetID(), false, channum);

        if (!ctx->ReloadTVChain())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "LiveTV not successfully restarted");
            RestoreScreenSaver(ctx);
            ctx->SetRecorder(nullptr);
            SetErrored(ctx);
            SetExitPlayer(true, false);
        }
        else
        {
            ctx->LockPlayingInfo(__FILE__, __LINE__);
            QString playbackURL = ctx->m_playingInfo->GetPlaybackURL(true);
            bool opennow = (ctx->m_tvchain->GetInputType(-1) != "DUMMY");
            ctx->SetRingBuffer(
                RingBuffer::Create(
                    playbackURL, false, true,
                    opennow ? RingBuffer::kLiveTVOpenTimeout : -1));

            ctx->m_tvchain->SetProgram(*ctx->m_playingInfo);
            if (ctx->m_buffer)
                ctx->m_buffer->SetLiveMode(ctx->m_tvchain);
            ctx->UnlockPlayingInfo(__FILE__, __LINE__);
        }

        bool ok = false;
        if (ctx->m_playingInfo && StartRecorder(ctx,-1))
        {
            PlayerContext *mctx2 = GetPlayer(ctx, 0);
            QRect dummy = QRect();
            if (ctx->CreatePlayer(
                    this, GetMythMainWindow(), ctx->GetState(),
                    false, dummy, muted))
            {
                ScheduleStateChange(ctx);
                ok = true;
                ctx->PushPreviousChannel();
                for (size_t i = 1; i < m_player.size(); i++)
                    PIPAddPlayer(mctx2, GetPlayer(ctx, i));

                SetSpeedChangeTimer(25, __LINE__);
            }
            else
                StopStuff(mctx2, ctx, true, true, true);
        }

        if (!ok)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "LiveTV not successfully started");
            RestoreScreenSaver(ctx);
            ctx->SetRecorder(nullptr);
            SetErrored(ctx);
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

    UnpauseLiveTV(ctx);
    UpdateOSDInput(ctx);

    ITVRestart(ctx, true);
}

void TV::ToggleChannelFavorite(PlayerContext */*ctx*/)
{
    // TOGGLEFAV was broken in [20523], this just prints something
    // out so as not to cause further confusion. See #8948.
    LOG(VB_GENERAL, LOG_ERR,
        "TV::ToggleChannelFavorite() -- currently disabled");
}

void TV::ToggleChannelFavorite(PlayerContext *ctx, const QString& changroup_name)
{
    if (ctx->m_recorder)
        ctx->m_recorder->ToggleChannelFavorite(changroup_name);
}

QString TV::GetQueuedInput(void) const
{
    QMutexLocker locker(&m_timerIdLock);
    return m_queuedInput;
}

int TV::GetQueuedInputAsInt(bool *ok, int base) const
{
    QMutexLocker locker(&m_timerIdLock);
    return m_queuedInput.toInt(ok, base);
}

QString TV::GetQueuedChanNum(void) const
{
    QMutexLocker locker(&m_timerIdLock);

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
void TV::ClearInputQueues(const PlayerContext *ctx, bool hideosd)
{
    if (hideosd)
        HideOSDWindow(ctx, "osd_input");

    QMutexLocker locker(&m_timerIdLock);
    m_queuedInput   = "";
    m_queuedChanNum = "";
    m_queuedChanID  = 0;
    if (m_queueInputTimerId)
    {
        KillTimer(m_queueInputTimerId);
        m_queueInputTimerId = 0;
    }
}

void TV::AddKeyToInputQueue(PlayerContext *ctx, char key)
{
    if (key)
    {
        QMutexLocker locker(&m_timerIdLock);
        m_queuedInput   = m_queuedInput.append(key).right(kInputKeysMax);
        m_queuedChanNum = m_queuedChanNum.append(key).right(kInputKeysMax);
        if (!m_queueInputTimerId)
            m_queueInputTimerId = StartTimer(10, __LINE__);
    }

    bool commitSmart = false;
    QString inputStr = GetQueuedInput();

    // Always use immediate channel change when channel numbers are entered
    // in browse mode because in browse mode space/enter exit browse
    // mode and change to the currently browsed channel.
    if (StateIsLiveTV(GetState(ctx)) && !m_ccInputMode && !m_asInputMode &&
        m_browseHelper->IsBrowsing())
    {
        commitSmart = ProcessSmartChannel(ctx, inputStr);
    }

    // Handle OSD...
    inputStr = inputStr.isEmpty() ? "?" : inputStr;
    if (m_ccInputMode)
    {
        QString entryStr = (m_vbimode==VBIMode::PAL_TT) ? tr("TXT:") : tr("CC:");
        inputStr = entryStr + " " + inputStr;
    }
    else if (m_asInputMode)
        inputStr = tr("Seek:", "seek to location") + " " + inputStr;
    SetOSDText(ctx, "osd_input", "osd_number_entry", inputStr,
               kOSDTimeout_Med);

    // Commit the channel if it is complete and smart changing is enabled.
    if (commitSmart)
        CommitQueuedInput(ctx);
}

static QString add_spacer(const QString &chan, const QString &spacer)
{
    if ((chan.length() >= 2) && !spacer.isEmpty())
        return chan.left(chan.length()-1) + spacer + chan.right(1);
    return chan;
}

bool TV::ProcessSmartChannel(const PlayerContext *ctx, QString &inputStr)
{
    QString chan = GetQueuedChanNum();

    if (chan.isEmpty())
        return false;

    // Check for and remove duplicate separator characters
    if ((chan.length() > 2) && (chan.right(1) == chan.right(2).left(1)))
    {
        bool ok = false;
        chan.right(1).toUInt(&ok);
        if (!ok)
        {
            chan = chan.left(chan.length()-1);

            QMutexLocker locker(&m_timerIdLock);
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
    if (ctx->m_recorder)
    {
        valid_prefix = ctx->m_recorder->CheckChannelPrefix(
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
        QMutexLocker locker(&m_timerIdLock);
        m_queuedChanNum = "";
    }
    else if (!needed_spacer.isEmpty())
    {
        // need a spacer..
        QMutexLocker locker(&m_timerIdLock);
        m_queuedChanNum = add_spacer(chan, needed_spacer);
    }

#if DEBUG_CHANNEL_PREFIX
    LOG(VB_GENERAL, LOG_DEBUG, QString(" ValidPref(%1) CardId(%2) Chan(%3) "
                                       " PrefCardId(%4) Complete(%5) Sp(%6)")
            .arg(valid_prefix).arg(0).arg(GetQueuedChanNum())
            .arg(pref_cardid).arg(is_not_complete).arg(needed_spacer));
#endif

    QMutexLocker locker(&m_timerIdLock);
    inputStr = m_queuedChanNum;
    if (!m_queueInputTimerId)
        m_queueInputTimerId = StartTimer(10, __LINE__);

    return !is_not_complete;
}

bool TV::CommitQueuedInput(PlayerContext *ctx)
{
    bool commited = false;

    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("livetv(%1) qchannum(%2) qchanid(%3)")
            .arg(StateIsLiveTV(GetState(ctx)))
            .arg(GetQueuedChanNum())
            .arg(GetQueuedChanID()));

    if (m_ccInputMode)
    {
        commited = true;
        if (HasQueuedInput())
            HandleTrackAction(ctx, ACTION_TOGGLESUBS);
    }
    else if (m_asInputMode)
    {
        commited = true;
        if (HasQueuedInput())
            // XXX Should the cutlist be honored?
            DoArbSeek(ctx, ARBSEEK_FORWARD, /*honorCutlist*/false);
    }
    else if (StateIsLiveTV(GetState(ctx)))
    {
        QString channum = GetQueuedChanNum();
        QString chaninput = GetQueuedInput();
        if (m_browseHelper->IsBrowsing())
        {
            uint sourceid = 0;
            ctx->LockPlayingInfo(__FILE__, __LINE__);
            if (ctx->m_playingInfo)
                sourceid = ctx->m_playingInfo->GetSourceID();
            ctx->UnlockPlayingInfo(__FILE__, __LINE__);

            commited = true;
            if (channum.isEmpty())
                channum = m_browseHelper->GetBrowsedInfo().m_chanNum;
            uint chanid = m_browseHelper->GetChanId(
                channum, ctx->GetCardID(), sourceid);
            if (chanid)
                m_browseHelper->BrowseChannel(ctx, channum);

            HideOSDWindow(ctx, "osd_input");
        }
        else if (GetQueuedChanID() || !channum.isEmpty())
        {
            commited = true;
            ChangeChannel(ctx, GetQueuedChanID(), channum);
        }
    }

    ClearInputQueues(ctx, true);
    return commited;
}

void TV::ChangeChannel(PlayerContext *ctx, ChannelChangeDirection direction)
{
    if (m_dbUseChannelGroups || (direction == CHANNEL_DIRECTION_FAVORITE))
    {
        uint old_chanid = 0;
        if (m_channelGroupId > -1)
        {
            ctx->LockPlayingInfo(__FILE__, __LINE__);
            if (!ctx->m_playingInfo)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "no active ctx playingInfo.");
                ctx->UnlockPlayingInfo(__FILE__, __LINE__);
                ReturnPlayerLock(ctx);
                return;
            }
            // Collect channel info
            old_chanid = ctx->m_playingInfo->GetChanID();
            ctx->UnlockPlayingInfo(__FILE__, __LINE__);
        }

        if (old_chanid)
        {
            QMutexLocker locker(&m_channelGroupLock);
            if (m_channelGroupId > -1)
            {
                uint chanid = ChannelUtil::GetNextChannel(
                    m_channelGroupChannelList, old_chanid, 0, 0, direction);
                if (chanid)
                    ChangeChannel(ctx, chanid, "");
                return;
            }
        }
    }

    if (direction == CHANNEL_DIRECTION_FAVORITE)
        direction = CHANNEL_DIRECTION_UP;

    QString oldinputname = ctx->m_recorder->GetInput();

    if (ContextIsPaused(ctx, __FILE__, __LINE__))
    {
        HideOSDWindow(ctx, "osd_status");
        MythUIHelper::DisableScreensaver();
    }

    // Save the current channel if this is the first time
    if (ctx->m_prevChan.empty())
        ctx->PushPreviousChannel();

    PauseAudioUntilBuffered(ctx);
    PauseLiveTV(ctx);

    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (ctx->m_player)
    {
        ctx->m_player->ResetCaptions();
        ctx->m_player->ResetTeletext();
    }
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);

    ctx->m_recorder->ChangeChannel(direction);
    ClearInputQueues(ctx, false);

    if (ctx->m_player)
        ctx->m_player->GetAudio()->Reset();

    UnpauseLiveTV(ctx);

    if (oldinputname != ctx->m_recorder->GetInput())
        UpdateOSDInput(ctx);
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
        chanid = max(ChannelUtil::GetChanID(cur_sourceid, channum), 0);
        if (chanid)
            return chanid;
    }
    // try to find channel on specified input

    uint sourceid = CardUtil::GetSourceID(cardid);
    if (cur_sourceid != sourceid && sourceid)
        chanid = max(ChannelUtil::GetChanID(sourceid, channum), 0);
    return chanid;
}

void TV::ChangeChannel(PlayerContext *ctx, uint chanid, const QString &chan)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("(%1, '%2')")
            .arg(chanid).arg(chan));

    if ((!chanid && chan.isEmpty()) || !ctx || !ctx->m_recorder)
        return;

    QString channum = chan;
    QStringList reclist;
    QSet<uint> tunable_on;

    QString oldinputname = ctx->m_recorder->GetInput();

    if (channum.isEmpty() && chanid)
    {
        channum = ChannelUtil::GetChanNum(chanid);
    }

    bool getit = false;
    if (ctx->m_recorder)
    {
        if (kPseudoChangeChannel == ctx->m_pseudoLiveTVState)
        {
            getit = false;
        }
        else if (kPseudoRecording == ctx->m_pseudoLiveTVState)
        {
            getit = true;
        }
        else if (chanid)
        {
            tunable_on = IsTunableOn(ctx, chanid);
            getit = !tunable_on.contains(ctx->GetCardID());
        }
        else
        {
            QString needed_spacer;
            uint pref_cardid = 0;
            uint cardid = ctx->GetCardID();
            bool dummy = false;

            ctx->m_recorder->CheckChannelPrefix(chan,  pref_cardid,
                                              dummy, needed_spacer);

            LOG(VB_CHANNEL, LOG_INFO, LOC +
                QString("CheckChannelPrefix(%1, pref_cardid %2, %3, '%4') "
                        "cardid %5")
                .arg(chan).arg(pref_cardid).arg(dummy).arg(needed_spacer)
                .arg(cardid));

            channum = add_spacer(chan, needed_spacer);
            if (pref_cardid != cardid)
            {
                getit = true;
            }
            else
            {
                if (!chanid)
                    chanid = get_chanid(ctx, cardid, chan);
                tunable_on = IsTunableOn(ctx, chanid);
                getit = !tunable_on.contains(cardid);
            }
        }

        if (getit)
        {
            QStringList tmp =
                ChannelUtil::GetValidRecorderList(chanid, channum);
            if (tunable_on.empty())
            {
                if (!chanid)
                    chanid = get_chanid(ctx, ctx->GetCardID(), chan);
                tunable_on = IsTunableOn(ctx, chanid);
            }
            foreach (const auto & rec, tmp)
            {
                if (!chanid || tunable_on.contains(rec.toUInt()))
                    reclist.push_back(rec);
            }
        }
    }

    if (!reclist.empty())
    {
        RemoteEncoder *testrec = RemoteRequestFreeRecorderFromList(reclist, ctx->GetCardID());
        if (!testrec || !testrec->IsValidRecorder())
        {
            ClearInputQueues(ctx, true);
            ShowNoRecorderDialog(ctx);
            delete testrec;
            return;
        }

        if (!ctx->m_prevChan.empty() && ctx->m_prevChan.back() == channum)
        {
            // need to remove it if the new channel is the same as the old.
            ctx->m_prevChan.pop_back();
        }

        uint new_cardid = testrec->GetRecorderNumber();
        uint inputid = new_cardid;

        // found the card on a different recorder.
        delete testrec;
        // Save the current channel if this is the first time
        if (ctx->m_prevChan.empty())
            ctx->PushPreviousChannel();
        SwitchInputs(ctx, chanid, channum, inputid);
        return;
    }

    if (getit || !ctx->m_recorder || !ctx->m_recorder->CheckChannel(channum))
        return;

    if (ContextIsPaused(ctx, __FILE__, __LINE__))
    {
        HideOSDWindow(ctx, "osd_status");
        MythUIHelper::DisableScreensaver();
    }

    // Save the current channel if this is the first time
    if (ctx->m_prevChan.empty())
        ctx->PushPreviousChannel();

    PauseAudioUntilBuffered(ctx);
    PauseLiveTV(ctx);

    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (ctx->m_player)
    {
        ctx->m_player->ResetCaptions();
        ctx->m_player->ResetTeletext();
    }
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);

    ctx->m_recorder->SetChannel(channum);

    if (ctx->m_player)
        ctx->m_player->GetAudio()->Reset();

    UnpauseLiveTV(ctx, chanid && GetQueuedChanID());

    if (oldinputname != ctx->m_recorder->GetInput())
        UpdateOSDInput(ctx);
}

void TV::ChangeChannel(const PlayerContext *ctx, const ChannelInfoList &options)
{
    for (const auto & option : options)
    {
        uint    chanid  = option.m_chanId;
        QString channum = option.m_chanNum;

        if (chanid && !channum.isEmpty() && IsTunable(ctx, chanid))
        {
            // hide the channel number, activated by certain signal monitors
            HideOSDWindow(ctx, "osd_input");

            QMutexLocker locker(&m_timerIdLock);
            m_queuedInput   = channum;
            m_queuedChanNum = channum;
            m_queuedChanID  = chanid;
            if (!m_queueInputTimerId)
                m_queueInputTimerId = StartTimer(10, __LINE__);
            break;
        }
    }
}

void TV::ShowPreviousChannel(PlayerContext *ctx)
{
    QString channum = ctx->GetPreviousChannel();

    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("Previous channel number '%1'")
            .arg(channum));

    if (channum.isEmpty())
        return;

    SetOSDText(ctx, "osd_input", "osd_number_entry", channum, kOSDTimeout_Med);
}

void TV::PopPreviousChannel(PlayerContext *ctx, bool immediate_change)
{
    if (!ctx->m_tvchain)
        return;

    if (!immediate_change)
        ShowPreviousChannel(ctx);

    QString prev_channum = ctx->PopPreviousChannel();
    QString cur_channum  = ctx->m_tvchain->GetChannelName(-1);

    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("'%1'->'%2'")
            .arg(cur_channum).arg(prev_channum));

    // Only change channel if previous channel != current channel
    if (cur_channum != prev_channum && !prev_channum.isEmpty())
    {
        QMutexLocker locker(&m_timerIdLock);
        m_queuedInput   = prev_channum;
        m_queuedChanNum = prev_channum;
        m_queuedChanID  = 0;
        if (!m_queueInputTimerId)
            m_queueInputTimerId = StartTimer(10, __LINE__);
    }

    if (immediate_change)
    {
        // Turn off OSD Channel Num so the channel changes right away
        HideOSDWindow(ctx, "osd_input");
    }
}

bool TV::ClearOSD(const PlayerContext *ctx)
{
    bool res = false;

    if (HasQueuedInput() || HasQueuedChannel())
    {
        ClearInputQueues(ctx, true);
        res = true;
    }

    OSD *osd = GetOSDLock(ctx);
    if (osd)
    {
        osd->DialogQuit();
        osd->HideAll(true, nullptr, true); // pop OSD screen
        res = true;
    }
    ReturnOSDLock(ctx, osd);

    if (m_browseHelper->IsBrowsing())
        m_browseHelper->BrowseEnd(nullptr, false);

    return res;
}

/** \fn TV::ToggleOSD(const PlayerContext*, bool includeStatus)
 *  \brief Cycle through the available Info OSDs.
 */
void TV::ToggleOSD(PlayerContext *ctx, bool includeStatusOSD)
{
    OSD *osd = GetOSDLock(ctx);
    if (!osd)
    {
        ReturnOSDLock(ctx, osd);
        return;
    }

    bool hideAll    = false;
    bool showStatus = false;
    bool paused     = ContextIsPaused(ctx, __FILE__, __LINE__);
    bool is_status_disp    = osd->IsWindowVisible("osd_status");
    bool has_prog_info     = osd->HasWindow("program_info");
    bool is_prog_info_disp = osd->IsWindowVisible("program_info");

    ReturnOSDLock(ctx, osd);

    if (is_status_disp)
    {
        if (has_prog_info)
            UpdateOSDProgInfo(ctx, "program_info");
        else
            hideAll = true;
    }
    else if (is_prog_info_disp && !paused)
    {
        hideAll = true;
    }
    else if (includeStatusOSD)
    {
        showStatus = true;
    }
    else
    {
        if (has_prog_info)
            UpdateOSDProgInfo(ctx, "program_info");
    }

    if (hideAll || showStatus)
    {
        OSD *osd2 = GetOSDLock(ctx);
        if (osd2)
            osd2->HideAll();
        ReturnOSDLock(ctx, osd2);
    }

    if (showStatus)
    {
        osdInfo info;
        if (ctx->CalcPlayerSliderPosition(info))
        {
            info.text["title"] = (paused ? tr("Paused") : tr("Position")) + GetLiveTVIndex(ctx);
            UpdateOSDStatus(ctx, info, kOSDFunctionalType_Default,
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

void TV::ToggleOSDDebug(PlayerContext *ctx)
{
    bool show = false;
    OSD *osd = GetOSDLock(ctx);
    if (osd && osd->IsWindowVisible("osd_debug"))
    {
        if (ctx->m_buffer)
            ctx->m_buffer->EnableBitrateMonitor(false);
        if (ctx->m_player)
            ctx->m_player->EnableFrameRateMonitor(false);
        osd->HideWindow("osd_debug");
    }
    else if (osd)
    {
        if (ctx->m_buffer)
            ctx->m_buffer->EnableBitrateMonitor(true);
        if (ctx->m_player)
            ctx->m_player->EnableFrameRateMonitor(true);
        show = true;
        QMutexLocker locker(&m_timerIdLock);
        if (!m_updateOSDDebugTimerId)
            m_updateOSDDebugTimerId = StartTimer(250, __LINE__);
    }
    ReturnOSDLock(ctx, osd);
    if (show)
        UpdateOSDDebug(ctx);
}

void TV::UpdateOSDDebug(const PlayerContext *ctx)
{
    OSD *osd = GetOSDLock(ctx);
    if (osd && ctx->m_player)
    {
        InfoMap infoMap;
        ctx->m_player->GetPlaybackData(infoMap);
        osd->ResetWindow("osd_debug");
        osd->SetText("osd_debug", infoMap, kOSDTimeout_None);
    }
    ReturnOSDLock(ctx, osd);
}

/** \fn TV::UpdateOSDProgInfo(const PlayerContext*, const char *whichInfo)
 *  \brief Update and display the passed OSD set with programinfo
 */
void TV::UpdateOSDProgInfo(const PlayerContext *ctx, const char *whichInfo)
{
    InfoMap infoMap;
    ctx->GetPlayingInfoMap(infoMap);

    QString nightmode = gCoreContext->GetBoolSetting("NightModeEnabled", false)
                            ? "yes" : "no";
    infoMap["nightmode"] = nightmode;

    // Clear previous osd and add new info
    OSD *osd = GetOSDLock(ctx);
    if (osd)
    {
        osd->HideAll();
        osd->SetText(whichInfo, infoMap, kOSDTimeout_Long);
    }
    ReturnOSDLock(ctx, osd);
}

void TV::UpdateOSDStatus(const PlayerContext *ctx, osdInfo &info,
                         int type, OSDTimeout timeout)
{
    OSD *osd = GetOSDLock(ctx);
    if (osd)
    {
        osd->ResetWindow("osd_status");
        QString nightmode = gCoreContext->GetBoolSetting("NightModeEnabled", false)
                                ? "yes" : "no";
        info.text.insert("nightmode", nightmode);
        osd->SetValues("osd_status", info.values, timeout);
        osd->SetText("osd_status",   info.text, timeout);
        if (type != kOSDFunctionalType_Default)
            osd->SetFunctionalWindow("osd_status", (OSDFunctionalType)type);
    }
    ReturnOSDLock(ctx, osd);
}

void TV::UpdateOSDStatus(const PlayerContext *ctx, const QString& title, const QString& desc,
                         const QString& value, int type, const QString& units,
                         int position, OSDTimeout timeout)
{
    osdInfo info;
    info.values.insert("position", position);
    info.values.insert("relposition", position);
    info.text.insert("title", title);
    info.text.insert("description", desc);
    info.text.insert("value", value);
    info.text.insert("units", units);
    UpdateOSDStatus(ctx, info, type, timeout);
}

void TV::UpdateOSDSeekMessage(const PlayerContext *ctx,
                              const QString &mesg, enum OSDTimeout timeout)
{
    LOG(VB_PLAYBACK, LOG_INFO, QString("UpdateOSDSeekMessage(%1, %2)")
            .arg(mesg).arg(timeout));

    osdInfo info;
    if (ctx->CalcPlayerSliderPosition(info))
    {
        int osdtype = (m_doSmartForward) ? kOSDFunctionalType_SmartForward :
            kOSDFunctionalType_Default;
        info.text["title"] = mesg + GetLiveTVIndex(ctx);
        UpdateOSDStatus(ctx, info, osdtype, timeout);
        SetUpdateOSDPosition(true);
    }
}

void TV::UpdateOSDInput(const PlayerContext *ctx)
{
    if (!ctx->m_recorder || !ctx->m_tvchain)
        return;

    int cardid = ctx->GetCardID();

    QString displayName = CardUtil::GetDisplayName(cardid);

    SetOSDMessage(ctx, displayName);
}

/** \fn TV::UpdateOSDSignal(const PlayerContext*, const QStringList&)
 *  \brief Updates Signal portion of OSD...
 */
void TV::UpdateOSDSignal(PlayerContext *ctx, const QStringList &strlist)
{
    OSD *osd = GetOSDLock(ctx);
    if (!osd || m_browseHelper->IsBrowsing() || !m_queuedChanNum.isEmpty())
    {
        if (&ctx->m_lastSignalMsg != &strlist)
            ctx->m_lastSignalMsg = strlist;
        ReturnOSDLock(ctx, osd);

        QMutexLocker locker(&m_timerIdLock);
        m_signalMonitorTimerId[StartTimer(1, __LINE__)] = ctx;
        return;
    }
    ReturnOSDLock(ctx, osd);

    SignalMonitorList slist = SignalMonitorValue::Parse(strlist);

    InfoMap infoMap = ctx->m_lastSignalUIInfo;
    if ((!ctx->m_lastSignalUIInfoTime.isRunning() ||
         (ctx->m_lastSignalUIInfoTime.elapsed() > 5000)) ||
        infoMap["callsign"].isEmpty())
    {
        ctx->m_lastSignalUIInfo.clear();
        ctx->GetPlayingInfoMap(ctx->m_lastSignalUIInfo);

        infoMap = ctx->m_lastSignalUIInfo;
        ctx->m_lastSignalUIInfoTime.start();
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

    uint  sig  = 0;
    float snr  = 0.0F;
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
            ber = it->GetValue();
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
    if (snr > 0.0F)
        sigDesc += " | " + tr("S/N %1dB").arg(log10f(snr), 3, 'f', 1);
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

    osd = GetOSDLock(ctx);
    if (osd)
    {
        infoMap["description"] = sigDesc;
        osd->SetText("program_info", infoMap, kOSDTimeout_Med);
    }
    ReturnOSDLock(ctx, osd);

    ctx->m_lastSignalMsg.clear();
    ctx->m_lastSignalMsgTime.start();

    // Turn off lock timer if we have an "All Good" or good PMT
    if (allGood || (pmt == "M"))
    {
        m_lockTimerOn = false;
        m_lastLockSeenTime = MythDate::current();
    }
}

void TV::UpdateOSDTimeoutMessage(PlayerContext *ctx)
{
    bool timed_out = false;

    if (ctx->m_recorder)
    {
        QString input = ctx->m_recorder->GetInput();
        uint timeout  = ctx->m_recorder->GetSignalLockTimeout(input);
        timed_out = m_lockTimerOn && m_lockTimer.hasExpired(timeout);
    }

    OSD *osd = GetOSDLock(ctx);

    if (!osd)
    {
        if (timed_out)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "You have no OSD, but tuning has already taken too long.");
        }
        ReturnOSDLock(ctx, osd);
        return;
    }

    bool showing = osd->DialogVisible(OSD_DLG_INFO);
    if (!timed_out)
    {
        if (showing)
            osd->DialogQuit();
        ReturnOSDLock(ctx, osd);
        return;
    }

    if (showing)
    {
        ReturnOSDLock(ctx, osd);
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

    ReturnOSDLock(ctx, osd);
}

void TV::UpdateLCD(void)
{
    // Make sure the LCD information gets updated shortly
    QMutexLocker locker(&m_timerIdLock);
    if (m_lcdTimerId)
        KillTimer(m_lcdTimerId);
    m_lcdTimerId = StartTimer(1, __LINE__);
}

void TV::ShowLCDChannelInfo(const PlayerContext *ctx)
{
    LCD *lcd = LCD::Get();
    ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (!lcd || !ctx->m_playingInfo)
    {
        ctx->UnlockPlayingInfo(__FILE__, __LINE__);
        return;
    }

    QString title    = ctx->m_playingInfo->GetTitle();
    QString subtitle = ctx->m_playingInfo->GetSubtitle();
    QString callsign = ctx->m_playingInfo->GetChannelSchedulingID();

    ctx->UnlockPlayingInfo(__FILE__, __LINE__);

    if ((callsign != m_lcdCallsign) || (title != m_lcdTitle) ||
        (subtitle != m_lcdSubtitle))
    {
        lcd->switchToChannel(callsign, title, subtitle);
        m_lcdCallsign = callsign;
        m_lcdTitle = title;
        m_lcdSubtitle = subtitle;
    }
}

static void format_time(int seconds, QString &tMin, QString &tHrsMin)
{
    int minutes     = seconds / 60;
    int hours       = minutes / 60;
    int min         = minutes % 60;

    tMin = TV::tr("%n minute(s)", "", minutes);
    tHrsMin = QString("%1:%2").arg(hours).arg(min, 2, 10, QChar('0'));
}


void TV::ShowLCDDVDInfo(const PlayerContext *ctx)
{
    LCD *lcd = LCD::Get();

    if (!lcd || !ctx->m_buffer || !ctx->m_buffer->IsDVD())
    {
        return;
    }

    DVDRingBuffer *dvd = ctx->m_buffer->DVD();
    QString dvdName;
    QString dvdSerial;
    QString mainStatus;
    QString subStatus;

    if (!dvd->GetNameAndSerialNum(dvdName, dvdSerial))
    {
        dvdName = tr("DVD");
    }

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
        QString timeMins;
        QString timeHrsMin;
        int playingTitle = 0;
        int playingPart = 0;

        dvd->GetPartAndTitle(playingPart, playingTitle);
        int totalParts = dvd->NumPartsInTitle();
        format_time(dvd->GetTotalTimeOfTitle(), timeMins, timeHrsMin);

        mainStatus = tr("Title: %1 (%2)").arg(playingTitle).arg(timeHrsMin);
        subStatus = tr("Chapter: %1/%2").arg(playingPart).arg(totalParts);
    }
    if ((dvdName != m_lcdCallsign) || (mainStatus != m_lcdTitle) ||
                                      (subStatus != m_lcdSubtitle))
    {
        lcd->switchToChannel(dvdName, mainStatus, subStatus);
        m_lcdCallsign = dvdName;
        m_lcdTitle    = mainStatus;
        m_lcdSubtitle = subStatus;
    }
}


bool TV::IsTunable(const PlayerContext *ctx, uint chanid)
{
    return !IsTunableOn(ctx, chanid).empty();
}

bool TV::IsTunable(uint chanid)
{
    return !IsTunableOn(nullptr, chanid).empty();
}

static QString toCommaList(const QSet<uint> &list)
{
    QString ret = "";
    foreach (uint i, list)
        ret += QString("%1,").arg(i);

    if (ret.length())
        return ret.left(ret.length()-1);

    return "";
}

QSet<uint> TV::IsTunableOn(
    const PlayerContext *ctx, uint chanid)
{
    QSet<uint> tunable_cards;

    if (!chanid)
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC +
            QString("ChanId (%1) - no").arg(chanid));

        return tunable_cards;
    }

    uint mplexid = ChannelUtil::GetMplexID(chanid);
    mplexid = (32767 == mplexid) ? 0 : mplexid;

    uint excluded_input = 0;
    if (ctx && ctx->m_recorder && ctx->m_pseudoLiveTVState == kPseudoNormalLiveTV)
        excluded_input = ctx->GetCardID();

    uint sourceid = ChannelUtil::GetSourceIDForChannel(chanid);

    vector<InputInfo> inputs = RemoteRequestFreeInputInfo(excluded_input);

    for (auto & input : inputs)
    {
        if (input.m_sourceId != sourceid)
            continue;

        if (input.m_mplexId &&
            input.m_mplexId != mplexid)
            continue;

        if (!input.m_mplexId && input.m_chanId &&
            input.m_chanId != chanid)
            continue;

        tunable_cards.insert(input.m_inputId);
    }

    if (tunable_cards.empty())
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC + QString("ChanId (%1) - no")
            .arg(chanid));
    }
    else
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC + QString("ChanId (%1) yes { %2 }")
            .arg(chanid).arg(toCommaList(tunable_cards)));
    }

    return tunable_cards;
}

bool TV::StartEmbedding(const QRect &embedRect)
{
    PlayerContext *ctx = GetPlayerReadLock(0, __FILE__, __LINE__);
    if (!ctx)
        return false;

    if (!ctx->IsNullVideoDesired())
        ctx->StartEmbedding(embedRect);
    else
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("Called with null video context #%1")
                .arg(find_player_index(ctx)));
        ctx->ResizePIPWindow(embedRect);
    }

    // Hide any PIP windows...
    PlayerContext *mctx = GetPlayer(ctx, 0);
    for (uint i = 1; (mctx == ctx) && (i < m_player.size()); i++)
    {
        GetPlayer(ctx,i)->LockDeletePlayer(__FILE__, __LINE__);
        if (GetPlayer(ctx,i)->m_player)
            GetPlayer(ctx,i)->m_player->SetPIPVisible(false);
        GetPlayer(ctx,i)->UnlockDeletePlayer(__FILE__, __LINE__);
    }

    // Start checking for end of file for embedded window..
    QMutexLocker locker(&m_timerIdLock);
    if (m_embedCheckTimerId)
        KillTimer(m_embedCheckTimerId);
    m_embedCheckTimerId = StartTimer(kEmbedCheckFrequency, __LINE__);

    bool embedding = ctx->IsEmbedding();
    ReturnPlayerLock(ctx);
    return embedding;
}

void TV::StopEmbedding(void)
{
    PlayerContext *ctx = GetPlayerReadLock(0, __FILE__, __LINE__);
    if (!ctx)
        return;

    if (ctx->IsEmbedding())
        ctx->StopEmbedding();

    // Undo any PIP hiding
    PlayerContext *mctx = GetPlayer(ctx, 0);
    for (uint i = 1; (mctx == ctx) && (i < m_player.size()); i++)
    {
        GetPlayer(ctx,i)->LockDeletePlayer(__FILE__, __LINE__);
        if (GetPlayer(ctx,i)->m_player)
            GetPlayer(ctx,i)->m_player->SetPIPVisible(true);
        GetPlayer(ctx,i)->UnlockDeletePlayer(__FILE__, __LINE__);
    }

    // Stop checking for end of file for embedded window..
    QMutexLocker locker(&m_timerIdLock);
    if (m_embedCheckTimerId)
        KillTimer(m_embedCheckTimerId);
    m_embedCheckTimerId = 0;

    ReturnPlayerLock(ctx);
}

vector<bool> TV::DoSetPauseState(PlayerContext *lctx, const vector<bool> &pause)
{
    vector<bool> was_paused;
    vector<float> times;
    for (uint i = 0; lctx && i < m_player.size() && i < pause.size(); i++)
    {
        PlayerContext *actx = GetPlayer(lctx, i);
        if (actx)
            was_paused.push_back(ContextIsPaused(actx, __FILE__, __LINE__));
        float time = 0.0F;
        if (pause[i] ^ was_paused.back())
            time = DoTogglePauseStart(GetPlayer(lctx,i));
        times.push_back(time);
    }

    for (uint i = 0; lctx && i < m_player.size() && i < pause.size(); i++)
    {
        if (pause[i] ^ was_paused[i])
            DoTogglePauseFinish(GetPlayer(lctx,i), times[i], false);
    }

    return was_paused;
}

void TV::DoEditSchedule(int editType)
{
    // Prevent nesting of the pop-up UI
    if (m_ignoreKeyPresses)
        return;

    if ((editType == kScheduleProgramGuide  && !RunProgramGuidePtr) ||
        (editType == kScheduleProgramFinder && !RunProgramFinderPtr) ||
        (editType == kScheduledRecording    && !RunScheduleEditorPtr) ||
        (editType == kViewSchedule          && !RunViewScheduledPtr))
    {
        return;
    }

    PlayerContext *actx = GetPlayerReadLock(0, __FILE__, __LINE__);

    actx->LockPlayingInfo(__FILE__, __LINE__);
    if (!actx->m_playingInfo)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "no active ctx playingInfo.");
        actx->UnlockPlayingInfo(__FILE__, __LINE__);
        ReturnPlayerLock(actx);
        return;
    }

    // Collect channel info
    const ProgramInfo pginfo(*actx->m_playingInfo);
    uint    chanid  = pginfo.GetChanID();
    QString channum = pginfo.GetChanNum();
    QDateTime starttime = MythDate::current();
    actx->UnlockPlayingInfo(__FILE__, __LINE__);

    ClearOSD(actx);

    // Pause playback as needed...
    bool pause_active = true;
    bool isNearEnd = false;
    bool isLiveTV = StateIsLiveTV(GetState(actx));
    bool paused = false;

    {
        actx->LockDeletePlayer(__FILE__, __LINE__);
        pause_active = !actx->m_player || !actx->m_player->GetVideoOutput();
        if (actx->m_player)
        {
            paused = actx->m_player->IsPaused();
            if (!pause_active)
                isNearEnd = actx->m_player->IsNearEnd();
        }
        actx->UnlockDeletePlayer(__FILE__, __LINE__);
    }

    pause_active |= kScheduledRecording == editType;
    pause_active |= kViewSchedule == editType;
    pause_active |= kScheduleProgramFinder == editType;
    pause_active |= !isLiveTV && (!m_dbContinueEmbedded || isNearEnd);
    pause_active |= paused;
    vector<bool> do_pause;
    do_pause.insert(do_pause.begin(), 1, !m_player.empty());
    int actx_index = find_player_index(actx);
    if (actx_index < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to find player index by context");
        return;
    }
    do_pause[static_cast<size_t>(actx_index)] = pause_active;
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("Pausing player: %1").arg(pause_active));

    m_savedPause = DoSetPauseState(actx, do_pause);

    // Resize window to the MythTV GUI size
    MythMainWindow *mwnd = GetMythMainWindow();
    MythDisplay* display = MythDisplay::AcquireRelease();
    if (display->UsingVideoModes())
    {
        bool hide = display->NextModeIsLarger(display->GetGUIResolution());
        if (hide)
            mwnd->hide();
        display->SwitchToGUI(true);
        if (hide)
            mwnd->Show();
    }
    MythDisplay::AcquireRelease(false);

    if (!m_dbUseGuiSizeForTv)
        mwnd->MoveResize(m_savedGuiBounds);
#ifdef Q_OS_ANDROID
    mwnd->Show();
#else
    mwnd->show();
#endif
    ReturnPlayerLock(actx);


    // Actually show the pop-up UI
    switch (editType)
    {
        case kScheduleProgramGuide:
        {
            m_isEmbedded = (isLiveTV && !pause_active);
            RunProgramGuidePtr(chanid, channum, starttime, this,
                               m_isEmbedded, true, m_channelGroupId);
            m_ignoreKeyPresses = true;
            break;
        }
        case kScheduleProgramFinder:
        {
            m_isEmbedded = (isLiveTV && !pause_active);
            RunProgramFinderPtr(this, m_isEmbedded, true);
            m_ignoreKeyPresses = true;
            break;
        }
        case kScheduledRecording:
        {
            RunScheduleEditorPtr(&pginfo, (void *)this);
            m_ignoreKeyPresses = true;
            break;
        }
        case kViewSchedule:
        {
            RunViewScheduledPtr((void *)this, !pause_active);
            m_ignoreKeyPresses = true;
            break;
        }
        case kPlaybackBox:
        {
            RunPlaybackBoxPtr((void *)this, !pause_active);
            m_ignoreKeyPresses = true;
            break;
        }
    }

    // We are embedding in a mythui window so assuming no one
    // else has disabled painting show the MythUI window again.
    if (GetMythMainWindow() && m_weDisabledGUI)
    {
        GetMythMainWindow()->PopDrawDisabled();
        m_weDisabledGUI = false;
    }
}

void TV::EditSchedule(const PlayerContext */*ctx*/, int editType)
{
    // post the request so the guide will be created in the UI thread
    QString message = QString("START_EPG %1").arg(editType);
    auto* me = new MythEvent(message);
    qApp->postEvent(this, me);
}

void TV::ChangeVolume(PlayerContext *ctx, bool up, int newvolume)
{
    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (!ctx->m_player || !ctx->m_player->PlayerControlsVolume())
    {
        ctx->UnlockDeletePlayer(__FILE__, __LINE__);
        return;
    }

    bool setabsolute = (newvolume >= 0 && newvolume <= 100);

    if (ctx->m_player->IsMuted() && (up || setabsolute))
        ToggleMute(ctx);

    uint curvol = setabsolute ?
                      ctx->m_player->SetVolume(newvolume) :
                      ctx->m_player->AdjustVolume((up) ? +2 : -2);

    ctx->UnlockDeletePlayer(__FILE__, __LINE__);

    if (!m_browseHelper->IsBrowsing())
    {
        UpdateOSDStatus(ctx, tr("Adjust Volume"), tr("Volume"),
                        QString::number(curvol),
                        kOSDFunctionalType_PictureAdjust, "%", curvol * 10,
                        kOSDTimeout_Med);
        SetUpdateOSDPosition(false);

        if (LCD *lcd = LCD::Get())
        {
            QString appName = tr("Video");

            if (StateIsLiveTV(GetState(ctx)))
                appName = tr("TV");

            if (ctx->m_buffer && ctx->m_buffer->IsDVD())
                appName = tr("DVD");

            lcd->switchToVolume(appName);
            lcd->setVolumeLevel((float)curvol / 100);

            QMutexLocker locker(&m_timerIdLock);
            if (m_lcdVolumeTimerId)
                KillTimer(m_lcdVolumeTimerId);

            m_lcdVolumeTimerId = StartTimer(2000, __LINE__);
        }
    }
}

void TV::ToggleTimeStretch(PlayerContext *ctx)
{
    if (ctx->m_tsNormal == 1.0F)
    {
        ctx->m_tsNormal = ctx->m_tsAlt;
    }
    else
    {
        ctx->m_tsAlt = ctx->m_tsNormal;
        ctx->m_tsNormal = 1.0F;
    }
    ChangeTimeStretch(ctx, 0, false);
}

void TV::ChangeTimeStretch(PlayerContext *ctx, int dir, bool allowEdit)
{
    const float kTimeStretchMin = 0.5;
    const float kTimeStretchMax = 2.0;
    const float kTimeStretchStep = 0.05;
    float new_ts_normal = ctx->m_tsNormal + (kTimeStretchStep * dir);
    m_stretchAdjustment = allowEdit;

    if (new_ts_normal > kTimeStretchMax &&
        ctx->m_tsNormal < kTimeStretchMax)
    {
        new_ts_normal = kTimeStretchMax;
    }
    else if (new_ts_normal < kTimeStretchMin &&
             ctx->m_tsNormal > kTimeStretchMin)
    {
        new_ts_normal = kTimeStretchMin;
    }

    if (new_ts_normal > kTimeStretchMax ||
        new_ts_normal < kTimeStretchMin)
    {
        return;
    }

    ctx->m_tsNormal = kTimeStretchStep * lroundf(new_ts_normal / kTimeStretchStep);

    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (ctx->m_player && !ctx->m_player->IsPaused())
            ctx->m_player->Play(ctx->m_tsNormal, true);
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);

    if (!m_browseHelper->IsBrowsing())
    {
        if (!allowEdit)
        {
            UpdateOSDSeekMessage(ctx, ctx->GetPlayMessage(), kOSDTimeout_Short);
        }
        else
        {
            UpdateOSDStatus(ctx, tr("Adjust Time Stretch"), tr("Time Stretch"),
                            QString::number(ctx->m_tsNormal,'f',2),
                            kOSDFunctionalType_TimeStretchAdjust, "X",
                            (int)(ctx->m_tsNormal*(1000/kTimeStretchMax)),
                            kOSDTimeout_None);
            SetUpdateOSDPosition(false);
        }
    }

    SetSpeedChangeTimer(0, __LINE__);
}

void TV::EnableUpmix(PlayerContext *ctx, bool enable, bool toggle)
{
    if (!ctx->m_player || !ctx->m_player->HasAudioOut())
        return;
    QString text;

    bool enabled = false;

    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (toggle)
        enabled = ctx->m_player->GetAudio()->EnableUpmix(false, true);
    else
        enabled = ctx->m_player->GetAudio()->EnableUpmix(enable);
    // May have to disable digital passthrough
    ctx->m_player->ForceSetupAudioStream();
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);

    if (!m_browseHelper->IsBrowsing())
        SetOSDMessage(ctx, enabled ? tr("Upmixer On") : tr("Upmixer Off"));
}

void TV::ChangeSubtitleZoom(PlayerContext *ctx, int dir)
{
    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (!ctx->m_player)
    {
        ctx->UnlockDeletePlayer(__FILE__, __LINE__);
        return;
    }

    OSD *osd = GetOSDLock(ctx);
    SubtitleScreen *subs = nullptr;
    if (osd)
        subs = osd->InitSubtitles();
    ReturnOSDLock(ctx, osd);
    m_subtitleZoomAdjustment = true;
    bool showing = ctx->m_player->GetCaptionsEnabled();
    int newval = (subs ? subs->GetZoom() : 100) + dir;
    newval = max(50, newval);
    newval = min(200, newval);
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);

    if (showing && !m_browseHelper->IsBrowsing())
    {
        UpdateOSDStatus(ctx, tr("Adjust Subtitle Zoom"), tr("Subtitle Zoom"),
                        QString::number(newval),
                        kOSDFunctionalType_SubtitleZoomAdjust,
                        "%", newval * 1000 / 200, kOSDTimeout_None);
        SetUpdateOSDPosition(false);
        if (subs)
            subs->SetZoom(newval);
    }
}

// dir in 10ms jumps
void TV::ChangeSubtitleDelay(PlayerContext *ctx, int dir)
{
    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (!ctx->m_player)
    {
        ctx->UnlockDeletePlayer(__FILE__, __LINE__);
        return;
    }

    OSD *osd = GetOSDLock(ctx);
    SubtitleScreen *subs = nullptr;
    if (osd)
        subs = osd->InitSubtitles();
    ReturnOSDLock(ctx, osd);
    m_subtitleDelayAdjustment = true;
    uint capmode = ctx->m_player->GetCaptionMode();
    bool showing = ctx->m_player->GetCaptionsEnabled() &&
        (capmode == kDisplayRawTextSubtitle ||
         capmode == kDisplayTextSubtitle);
    int newval = (subs ? subs->GetDelay() : 100) + dir * 10;
    newval = max(-5000, newval);
    newval = min(5000, newval);
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);

    if (showing && !m_browseHelper->IsBrowsing())
    {
        // range of -5000ms..+5000ms, scale to 0..1000
        UpdateOSDStatus(ctx, tr("Adjust Subtitle Delay"), tr("Subtitle Delay"),
                        QString::number(newval),
                        kOSDFunctionalType_SubtitleDelayAdjust,
                        "ms", newval / 10 + 500, kOSDTimeout_None);
        SetUpdateOSDPosition(false);
        if (subs)
            subs->SetDelay(newval);
    }
}

// dir in 10ms jumps
void TV::ChangeAudioSync(PlayerContext *ctx, int dir, int newsync)
{
    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (!ctx->m_player)
    {
        ctx->UnlockDeletePlayer(__FILE__, __LINE__);
        return;
    }

    m_audiosyncAdjustment = true;
    long long newval = ctx->m_player->AdjustAudioTimecodeOffset(dir * 10, newsync);
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);

    if (!m_browseHelper->IsBrowsing())
    {
        int val = (int)newval;
        UpdateOSDStatus(ctx, tr("Adjust Audio Sync"), tr("Audio Sync"),
                        QString::number(val),
                        kOSDFunctionalType_AudioSyncAdjust,
                        "ms", (val/2) + 500, kOSDTimeout_None);
        SetUpdateOSDPosition(false);
    }
}

void TV::ToggleMute(PlayerContext *ctx, const bool muteIndividualChannels)
{
    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (!ctx->m_player || !ctx->m_player->HasAudioOut() ||
        !ctx->m_player->PlayerControlsVolume())
    {
        ctx->UnlockDeletePlayer(__FILE__, __LINE__);
        return;
    }

    MuteState mute_status = kMuteOff;

    if (!muteIndividualChannels)
    {
        ctx->m_player->SetMuted(!ctx->m_player->IsMuted());
        mute_status = (ctx->m_player->IsMuted()) ? kMuteAll : kMuteOff;
    }
    else
    {
        mute_status = ctx->m_player->IncrMuteState();
    }
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);

    QString text;

    switch (mute_status)
    {
        case kMuteOff:   text = tr("Mute Off"); break;
        case kMuteAll:   text = tr("Mute On"); break;
        case kMuteLeft:  text = tr("Left Channel Muted"); break;
        case kMuteRight: text = tr("Right Channel Muted"); break;
    }

    UpdateNavDialog(ctx);
    SetOSDMessage(ctx, text);
}

void TV::ToggleSleepTimer(const PlayerContext *ctx)
{
    QString text;

    // increment sleep index, cycle through
    if (++m_sleepIndex == m_sleepTimes.size())
        m_sleepIndex = 0;

    // set sleep timer to next sleep_index timeout
    if (m_sleepTimerId)
    {
        KillTimer(m_sleepTimerId);
        m_sleepTimerId = 0;
        m_sleepTimerTimeout = 0;
    }

    if (m_sleepTimes[m_sleepIndex].seconds != 0)
    {
        m_sleepTimerTimeout = m_sleepTimes[m_sleepIndex].seconds * 1000;
        m_sleepTimerId = StartTimer(m_sleepTimerTimeout, __LINE__);
    }

    text = tr("Sleep ") + " " + m_sleepTimes[m_sleepIndex].dispString;

    if (!m_browseHelper->IsBrowsing())
        SetOSDMessage(ctx, text);
}

void TV::ShowOSDSleep(void)
{
    KillTimer(m_sleepTimerId);
    m_sleepTimerId = 0;

    PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
    OSD *osd = GetOSDLock(mctx);
    if (osd)
    {
        QString message = tr(
            "MythTV was set to sleep after %1 minutes and "
            "will exit in %d seconds.\n"
            "Do you wish to continue watching?")
            .arg(m_sleepTimerTimeout * (1.0F/60000.0F));

        osd->DialogShow(OSD_DLG_SLEEP, message, kSleepTimerDialogTimeout);
        osd->DialogAddButton(tr("Yes"), "DIALOG_SLEEP_YES_0");
        osd->DialogAddButton(tr("No"),  "DIALOG_SLEEP_NO_0");
    }
    ReturnOSDLock(mctx, osd);
    ReturnPlayerLock(mctx);

    m_sleepDialogTimerId = StartTimer(kSleepTimerDialogTimeout, __LINE__);
}

void TV::HandleOSDSleep(PlayerContext *ctx, const QString& action)
{
    if (!DialogIsVisible(ctx, OSD_DLG_SLEEP))
        return;

    if (action == "YES")
    {
        if (m_sleepDialogTimerId)
        {
            KillTimer(m_sleepDialogTimerId);
            m_sleepDialogTimerId = 0;
        }
        m_sleepTimerId = StartTimer(m_sleepTimerTimeout * 1000, __LINE__);
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "No longer watching TV, exiting");
        SetExitPlayer(true, true);
    }
}

void TV::SleepDialogTimeout(void)
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
void TV::ShowOSDIdle(void)
{
    KillTimer(m_idleTimerId);
    m_idleTimerId = 0;

    PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
    OSD *osd = GetOSDLock(mctx);
    if (osd)
    {
        QString message = tr(
            "MythTV has been idle for %1 minutes and "
            "will exit in %d seconds. Are you still watching?")
            .arg(m_dbIdleTimeout * (1.0F/60000.0F));

        osd->DialogShow(OSD_DLG_IDLE, message, kIdleTimerDialogTimeout);
        osd->DialogAddButton(tr("Yes"), "DIALOG_IDLE_YES_0");
        osd->DialogAddButton(tr("No"),  "DIALOG_IDLE_NO_0");
    }
    ReturnOSDLock(mctx, osd);
    ReturnPlayerLock(mctx);

    m_idleDialogTimerId = StartTimer(kIdleTimerDialogTimeout, __LINE__);
}

void TV::HandleOSDIdle(PlayerContext *ctx, const QString& action)
{
    if (!DialogIsVisible(ctx, OSD_DLG_IDLE))
        return;

    if (action == "YES")
    {
        if (m_idleDialogTimerId)
        {
            KillTimer(m_idleDialogTimerId);
            m_idleDialogTimerId = 0;
        }
        if (m_idleTimerId)
            KillTimer(m_idleTimerId);
        m_idleTimerId = StartTimer(m_dbIdleTimeout, __LINE__);
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "No longer watching LiveTV, exiting");
        SetExitPlayer(true, true);
    }
}

void TV::IdleDialogTimeout(void)
{
    KillTimer(m_idleDialogTimerId);
    m_idleDialogTimerId = 0;

    PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
    if (StateIsLiveTV(mctx->GetState()))
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "Idle timeout reached, leaving LiveTV");
        SetExitPlayer(true, true);
    }
    ReturnPlayerLock(mctx);
}

void TV::ToggleMoveBottomLine(PlayerContext *ctx)
{
    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (!ctx->m_player)
    {
        ctx->UnlockDeletePlayer(__FILE__, __LINE__);
        return;
    }

    ctx->m_player->ToggleMoveBottomLine();
    QString msg = ctx->m_player->GetVideoOutput()->GetZoomString();

    ctx->UnlockDeletePlayer(__FILE__, __LINE__);

    SetOSDMessage(ctx, msg);
}

void TV::SaveBottomLine(PlayerContext *ctx)
{
    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (!ctx->m_player)
    {
        ctx->UnlockDeletePlayer(__FILE__, __LINE__);
        return;
    }

    ctx->m_player->SaveBottomLine();

    ctx->UnlockDeletePlayer(__FILE__, __LINE__);

    SetOSDMessage(ctx, "Current 'Manual Zoom' saved for 'BottomLine'.");
}

void TV::ToggleAspectOverride(PlayerContext *ctx, AspectOverrideMode aspectMode)
{
    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (!ctx->m_player)
    {
        ctx->UnlockDeletePlayer(__FILE__, __LINE__);
        return;
    }
    ctx->m_player->ToggleAspectOverride(aspectMode);
    QString text = toString(ctx->m_player->GetAspectOverride());
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);

    SetOSDMessage(ctx, text);
}

void TV::ToggleAdjustFill(PlayerContext *ctx, AdjustFillMode adjustfillMode)
{
    if (ctx != GetPlayer(ctx,0) || ctx->IsPBP())
        return;

    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (!ctx->m_player)
    {
        ctx->UnlockDeletePlayer(__FILE__, __LINE__);
        return;
    }
    ctx->m_player->ToggleAdjustFill(adjustfillMode);
    QString text = toString(ctx->m_player->GetAdjustFill());
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);

    SetOSDMessage(ctx, text);
}

void TV::PauseAudioUntilBuffered(PlayerContext *ctx)
{
    if (!ctx)
        return;

    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (ctx->m_player)
        ctx->m_player->GetAudio()->PauseAudioUntilBuffered();
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);
}

/// This handles all custom events
void TV::customEvent(QEvent *e)
{
    if (e->type() == MythEvent::kUpdateTvProgressEventType && m_myWindow)
    {
        m_myWindow->UpdateProgress();
        return;
    }

    if (e->type() == MythEvent::MythUserMessage)
    {
        auto *me = dynamic_cast<MythEvent*>(e);
        if (me == nullptr)
            return;
        QString message = me->Message();

        if (message.isEmpty())
            return;

        uint timeout = 0;
        if (me->ExtraDataCount() == 1)
        {
            uint t = me->ExtraData(0).toUInt();
            if (t > 0 && t < 1000)
                timeout = t * 1000;
        }

        if (timeout > 0)
            message += " (%d)";

        PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
        OSD *osd = GetOSDLock(mctx);
        if (osd)
            osd->DialogShow(OSD_DLG_CONFIRM, message, timeout);
        ReturnOSDLock(mctx, osd);
        ReturnPlayerLock(mctx);

        return;
    }

    if (e->type() == MythEvent::kUpdateBrowseInfoEventType)
    {
        auto *b = reinterpret_cast<UpdateBrowseInfoEvent*>(e);
        PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
        OSD *osd = GetOSDLock(mctx);
        if (osd)
        {
            osd->SetText("browse_info", b->m_im, kOSDTimeout_None);
            osd->SetExpiry("browse_info", kOSDTimeout_None);
        }
        ReturnOSDLock(mctx, osd);
        ReturnPlayerLock(mctx);
        return;
    }

    if (e->type() == DialogCompletionEvent::kEventType)
    {
        auto *dce = reinterpret_cast<DialogCompletionEvent*>(e);
        if (dce->GetData().userType() == qMetaTypeId<MenuNodeTuple>())
        {
            MenuNodeTuple data = dce->GetData().value<MenuNodeTuple>();
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
            OSDDialogEvent(dce->GetResult(), dce->GetResultText(),
                           dce->GetData().toString());
        return;
    }

    if (e->type() == OSDHideEvent::kEventType)
    {
        auto *ce = reinterpret_cast<OSDHideEvent*>(e);
        HandleOSDClosed(ce->GetFunctionalType());
        return;
    }

    // Stop DVD playback cleanly when the DVD is ejected
    if (e->type() == MythMediaEvent::kEventType)
    {
        PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
        TVState state = mctx->GetState();
        if (state != kState_WatchingDVD)
        {
            ReturnPlayerLock(mctx);
            return;
        }

        auto *me = dynamic_cast<MythMediaEvent*>(e);
        if (me == nullptr)
            return;
        MythMediaDevice *device = me->getDevice();

        QString filename = mctx->m_buffer ? mctx->m_buffer->GetFilename() : "";

        if (device && filename.endsWith(device->getDevicePath()) &&
            (device->getStatus() == MEDIASTAT_OPEN))
        {
            LOG(VB_GENERAL, LOG_NOTICE,
                "DVD has been ejected, exiting playback");

            for (uint i = 0; mctx && (i < m_player.size()); i++)
            {
                PlayerContext *ctx = GetPlayer(mctx, i);
                PrepareToExitPlayer(ctx, __LINE__, kBookmarkAuto);
            }

            SetExitPlayer(true, true);
        }
        ReturnPlayerLock(mctx);
        return;
    }

    if (e->type() != MythEvent::MythEventMessage)
        return;

    uint cardnum   = 0;
    auto *me = dynamic_cast<MythEvent*>(e);
    if (me == nullptr)
        return;
    QString message = me->Message();

    // TODO Go through these and make sure they make sense...
    QStringList tokens = message.split(" ", QString::SkipEmptyParts);

    if (me->ExtraDataCount() == 1)
    {
        PlayerContext *ctx = GetPlayerReadLock(0, __FILE__, __LINE__);
        int value = me->ExtraData(0).toInt();
        if (message == ACTION_SETVOLUME)
            ChangeVolume(ctx, false, value);
        else if (message == ACTION_SETAUDIOSYNC)
            ChangeAudioSync(ctx, 0, value);
        else if (message == ACTION_SETBRIGHTNESS)
        {
            DoChangePictureAttribute(ctx, kAdjustingPicture_Playback,
                                     kPictureAttribute_Brightness,
                                     false, value);
        }
        else if (message == ACTION_SETCONTRAST)
        {
            DoChangePictureAttribute(ctx, kAdjustingPicture_Playback,
                                     kPictureAttribute_Contrast,
                                     false, value);
        }
        else if (message == ACTION_SETCOLOUR)
        {
            DoChangePictureAttribute(ctx, kAdjustingPicture_Playback,
                                     kPictureAttribute_Colour,
                                     false, value);
        }
        else if (message == ACTION_SETHUE)
        {
            DoChangePictureAttribute(ctx, kAdjustingPicture_Playback,
                                     kPictureAttribute_Hue,
                                     false, value);
        }
        else if (message == ACTION_JUMPCHAPTER)
        {
            DoJumpChapter(ctx, value);
        }
        else if (message == ACTION_SWITCHTITLE)
        {
            DoSwitchTitle(ctx, value - 1);
        }
        else if (message == ACTION_SWITCHANGLE)
        {
            DoSwitchAngle(ctx, value);
        }
        else if (message == ACTION_SEEKABSOLUTE)
        {
            DoSeekAbsolute(ctx, value, /*honorCutlist*/true);
        }
        ReturnPlayerLock(ctx);
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

        PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
        for (uint i = 0; mctx && (i < m_player.size()); i++)
        {
            PlayerContext *ctx = GetPlayer(mctx, i);
            if (ctx->GetState() == kState_WatchingRecording)
            {
                if (ctx->m_recorder && (cardnum == ctx->GetCardID()))
                {
                    ctx->LockDeletePlayer(__FILE__, __LINE__);
                    if (ctx->m_player)
                    {
                        ctx->m_player->SetWatchingRecording(false);
                        if (seconds > 0)
                            ctx->m_player->SetLength(seconds);
                    }
                    ctx->UnlockDeletePlayer(__FILE__, __LINE__);

                    ctx->ChangeState(kState_WatchingPreRecorded);
                    ScheduleStateChange(ctx);
                }
            }
            else if (StateIsLiveTV(ctx->GetState()))
            {
                if (ctx->m_recorder && cardnum == ctx->GetCardID() &&
                    ctx->m_tvchain && ctx->m_tvchain->HasNext())
                {
                    ctx->LockDeletePlayer(__FILE__, __LINE__);
                    if (ctx->m_player)
                    {
                        ctx->m_player->SetWatchingRecording(false);
                        if (seconds > 0)
                            ctx->m_player->SetLength(seconds);
                    }
                    ctx->UnlockDeletePlayer(__FILE__, __LINE__);
                }
            }
        }
        ReturnPlayerLock(mctx);
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

        PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
        if (mctx->m_recorder && cardnum == mctx->GetCardID())
        {
            AskAllowRecording(mctx, me->ExtraDataList(),
                              timeuntil, hasrec, haslater);
        }

        for (size_t i = 1; i < m_player.size(); i++)
        {
            PlayerContext *ctx = GetPlayer(mctx, i);
            if (ctx->m_recorder && ctx->GetCardID() == cardnum)
            {
                LOG(VB_GENERAL, LOG_INFO, LOC + "Disabling PxP for recording");
                QString type = ctx->IsPIP() ?
                    tr("PiP", "Picture-in-Picture") :
                    tr("PbP", "Picture-by-Picture");
                StopStuff(mctx, ctx, true, true, true);
                SetOSDMessage(mctx, tr("Disabling %1 for recording").arg(type));
            }
        }
        ReturnPlayerLock(mctx);
    }

    if (message.startsWith("QUIT_LIVETV"))
    {
        cardnum = (tokens.size() >= 2) ? tokens[1].toUInt() : 0;

        PlayerContext *mctx = GetPlayerReadLock(-1, __FILE__, __LINE__);
        int match = -1;
        for (uint i = 0; mctx && (i < m_player.size()); i++)
        {
            PlayerContext *ctx = GetPlayer(mctx, i);
            match = (ctx->GetCardID() == cardnum) ? i : match;
        }

        if (match >= 0 && GetPlayer(mctx, match)->m_recorder)
        {
            if (0 == match)
            {
                for (uint i = 1; mctx && (i < m_player.size()); i++)
                {
                    PlayerContext *ctx = GetPlayer(mctx, i);
                    if (ctx->m_recorder && (ctx->GetCardID() == cardnum))
                    {
                        LOG(VB_GENERAL, LOG_INFO, LOC +
                            "Disabling PiP for QUIT_LIVETV");
                        StopStuff(mctx, ctx, true, true, true);
                    }
                }
                SetLastProgram(nullptr);
                m_jumpToProgram = true;
                SetExitPlayer(true, false);
            }
            else
            {
                PlayerContext *ctx = GetPlayer(mctx, match);
                StopStuff(mctx, ctx, true, true, true);
            }
        }
        ReturnPlayerLock(mctx);
    }

    if (message.startsWith("LIVETV_WATCH"))
    {
        int watch = 0;
        if (tokens.size() >= 3)
        {
            cardnum = tokens[1].toUInt();
            watch   = tokens[2].toInt();
        }

        PlayerContext *mctx = GetPlayerWriteLock(0, __FILE__, __LINE__);
        int match = -1;
        for (uint i = 0; mctx && (i < m_player.size()); i++)
        {
            PlayerContext *ctx = GetPlayer(mctx, i);
            match = (ctx->GetCardID() == cardnum) ? i : match;
        }

        if (match >= 0)
        {
            if (watch)
            {
                ProgramInfo pi(me->ExtraDataList());
                if (pi.HasPathname() || pi.GetChanID())
                {
                    PlayerContext *ctx = GetPlayer(mctx, match);
                    ctx->SetPseudoLiveTV(&pi, kPseudoChangeChannel);

                    QMutexLocker locker(&m_timerIdLock);
                    if (!m_pseudoChangeChanTimerId)
                        m_pseudoChangeChanTimerId = StartTimer(0, __LINE__);
                }
            }
            else
            {
                PlayerContext *ctx = GetPlayer(mctx, match);
                ctx->SetPseudoLiveTV(nullptr, kPseudoNormalLiveTV);
            }
        }
        ReturnPlayerLock(mctx);
    }

    if (message.startsWith("LIVETV_CHAIN"))
    {
        QString id;
        if ((tokens.size() >= 2) && tokens[1] == "UPDATE")
            id = tokens[2];

        PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
        for (uint i = 0; mctx && (i < m_player.size()); i++)
        {
            PlayerContext *ctx = GetPlayer(mctx, i);
            if (ctx->m_tvchain && ctx->m_tvchain->GetID() == id &&
                find_player_index(ctx) >= 0)
            {
                ctx->UpdateTVChain(me->ExtraDataList());
                break;
            }
        }
        ReturnPlayerLock(mctx);
    }

    if (message.startsWith("EXIT_TO_MENU"))
    {
        PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
        for (uint i = 0; mctx && (i < m_player.size()); i++)
        {
            PlayerContext *ctx = GetPlayer(mctx, i);
            PrepareToExitPlayer(ctx, __LINE__);
        }

        SetExitPlayer(true, true);
        if (mctx && mctx->m_player)
            mctx->m_player->DisableEdit(-1);
        ReturnPlayerLock(mctx);
    }

    if (message.startsWith("SIGNAL"))
    {
        cardnum = (tokens.size() >= 2) ? tokens[1].toUInt() : 0;
        const QStringList& signalList = me->ExtraDataList();

        PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
        OSD *osd = GetOSDLock(mctx);
        if (osd)
        {
            for (uint i = 0; mctx && (i < m_player.size()); i++)
            {
                PlayerContext *ctx = GetPlayer(mctx, i);
                bool tc = ctx->m_recorder && (ctx->GetCardID() == cardnum);
                if (tc && !signalList.empty())
                {
                    UpdateOSDSignal(ctx, signalList);
                    UpdateOSDTimeoutMessage(ctx);
                }
            }
        }
        ReturnOSDLock(mctx, osd);
        ReturnPlayerLock(mctx);
    }

    if (message.startsWith("NETWORK_CONTROL"))
    {
        if ((tokens.size() >= 2) &&
            (tokens[1] != "ANSWER") && (tokens[1] != "RESPONSE"))
        {
            QStringList tokens2 = message.split(" ", QString::SkipEmptyParts);
            if ((tokens2.size() >= 2) &&
                (tokens2[1] != "ANSWER") && (tokens2[1] != "RESPONSE"))
            {
                QMutexLocker locker(&m_timerIdLock);
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

    if (message.startsWith("EPG_EXITING") ||
        message.startsWith("PROGFINDER_EXITING") ||
        message.startsWith("VIEWSCHEDULED_EXITING") ||
        message.startsWith("PLAYBACKBOX_EXITING") ||
        message.startsWith("SCHEDULEEDITOR_EXITING"))
    {
        // Resize the window back to the MythTV Player size
        PlayerContext *actx = GetPlayerReadLock(0, __FILE__, __LINE__);
        MythMainWindow *mwnd = GetMythMainWindow();

        StopEmbedding();
        MythPainter *painter = GetMythPainter();
        if (painter)
            painter->FreeResources();

        PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
        mctx->LockDeletePlayer(__FILE__, __LINE__);
        if (mctx->m_player && mctx->m_player->GetVideoOutput())
            mctx->m_player->GetVideoOutput()->ResizeForVideo();
        mctx->UnlockDeletePlayer(__FILE__, __LINE__);
        ReturnPlayerLock(mctx);

        // m_playerBounds is not applicable when switching modes so
        // skip this logic in that case.
        if (!m_dbUseVideoModes)
            mwnd->MoveResize(m_playerBounds);

        DoSetPauseState(actx, m_savedPause); // Restore pause states

        if (!m_weDisabledGUI)
        {
            m_weDisabledGUI = true;
            GetMythMainWindow()->PushDrawDisabled();
        }

        qApp->processEvents();

        m_isEmbedded = false;
        m_ignoreKeyPresses = false;

        if (message.startsWith("PLAYBACKBOX_EXITING"))
        {
            ProgramInfo pginfo(me->ExtraDataList());
            if (pginfo.HasPathname() || pginfo.GetChanID())
                PrepToSwitchToRecordedProgram(actx, pginfo);
        }

        ReturnPlayerLock(actx);

    }

    if (message.startsWith("COMMFLAG_START") && (tokens.size() >= 2))
    {
        uint evchanid = 0;
        QDateTime evrecstartts;
        ProgramInfo::ExtractKey(tokens[1], evchanid, evrecstartts);

        PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
        for (uint i = 0; mctx && (i < m_player.size()); i++)
        {
            PlayerContext *ctx = GetPlayer(mctx, i);
            ctx->LockPlayingInfo(__FILE__, __LINE__);
            bool doit =
                ((ctx->m_playingInfo) &&
                 (ctx->m_playingInfo->GetChanID()             == evchanid) &&
                 (ctx->m_playingInfo->GetRecordingStartTime() == evrecstartts));
            ctx->UnlockPlayingInfo(__FILE__, __LINE__);

            if (doit)
            {
                QString msg = "COMMFLAG_REQUEST ";
                msg += ProgramInfo::MakeUniqueKey(evchanid, evrecstartts);
                gCoreContext->SendMessage(msg);
            }
        }
        ReturnPlayerLock(mctx);
    }

    if (message.startsWith("COMMFLAG_UPDATE") && (tokens.size() >= 3))
    {
        uint evchanid = 0;
        QDateTime evrecstartts;
        ProgramInfo::ExtractKey(tokens[1], evchanid, evrecstartts);

        PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
        for (uint i = 0; mctx && evchanid && (i < m_player.size()); i++)
        {
            PlayerContext *ctx = GetPlayer(mctx, i);
            ctx->LockPlayingInfo(__FILE__, __LINE__);
            bool doit =
                ((ctx->m_playingInfo) &&
                 (ctx->m_playingInfo->GetChanID()             == evchanid) &&
                 (ctx->m_playingInfo->GetRecordingStartTime() == evrecstartts));
            ctx->UnlockPlayingInfo(__FILE__, __LINE__);

            if (doit)
            {
                frm_dir_map_t newMap;
                QStringList mark;
                QStringList marks =
                    tokens[2].split(",", QString::SkipEmptyParts);
                for (uint j = 0; j < (uint)marks.size(); j++)
                {
                    mark = marks[j].split(":", QString::SkipEmptyParts);
                    if (marks.size() >= 2)
                    {
                        newMap[mark[0].toLongLong()] =
                            (MarkTypes) mark[1].toInt();
                    }
                }
                ctx->LockDeletePlayer(__FILE__, __LINE__);
                if (ctx->m_player)
                    ctx->m_player->SetCommBreakMap(newMap);
                ctx->UnlockDeletePlayer(__FILE__, __LINE__);
            }
        }
        ReturnPlayerLock(mctx);
    }

    if (message == "NOTIFICATION")
    {
        if (!GetNotificationCenter())
            return;

        PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
        OSD *osd = GetOSDLock(mctx);

        MythNotification mn(*me);
        MythNotificationCenter::GetInstance()->Queue(mn);

        ReturnOSDLock(mctx, osd);
        ReturnPlayerLock(mctx);
    }
}

void TV::QuickRecord(PlayerContext *ctx)
{
    BrowseInfo bi = m_browseHelper->GetBrowsedInfo();
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

        OSD *osd = GetOSDLock(ctx);
        if (osd)
        {
            osd->SetText("browse_info", infoMap, kOSDTimeout_Med);
            InfoMap map;
            map.insert("message_text", tr("Record"));
            osd->SetText("osd_message", map, kOSDTimeout_Med);
        }
        ReturnOSDLock(ctx, osd);
        return;
    }

    ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (!ctx->m_playingInfo)
    {
        LOG(VB_GENERAL, LOG_CRIT, LOC + "Unknown recording during live tv.");
        ctx->UnlockPlayingInfo(__FILE__, __LINE__);
        return;
    }

    QString cmdmsg("");
    if (ctx->m_playingInfo->QueryAutoExpire() == kLiveTVAutoExpire)
    {
        RecordingInfo recInfo(*ctx->m_playingInfo);
        recInfo.SaveAutoExpire((AutoExpireType)m_dbAutoexpireDefault);
        recInfo.ApplyRecordRecGroupChange("Default");
        *ctx->m_playingInfo = recInfo;

        cmdmsg = tr("Record");
        ctx->SetPseudoLiveTV(ctx->m_playingInfo, kPseudoRecording);
        ctx->m_recorder->SetLiveRecording(true);
        LOG(VB_RECORD, LOG_INFO, LOC + "Toggling Record on");
    }
    else
    {
        RecordingInfo recInfo(*ctx->m_playingInfo);
        recInfo.SaveAutoExpire(kLiveTVAutoExpire);
        recInfo.ApplyRecordRecGroupChange("LiveTV");
        *ctx->m_playingInfo = recInfo;

        cmdmsg = tr("Cancel Record");
        ctx->SetPseudoLiveTV(ctx->m_playingInfo, kPseudoNormalLiveTV);
        ctx->m_recorder->SetLiveRecording(false);
        LOG(VB_RECORD, LOG_INFO, LOC + "Toggling Record off");
    }

    QString msg = cmdmsg + " \"" + ctx->m_playingInfo->GetTitle() + "\"";

    ctx->UnlockPlayingInfo(__FILE__, __LINE__);

    SetOSDMessage(ctx, msg);
}

void TV::HandleOSDClosed(int osdType)
{
    switch (osdType)
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
            PlayerContext *ctx = GetPlayerReadLock(0, __FILE__, __LINE__);
            ctx->LockDeletePlayer(__FILE__, __LINE__);
            if (ctx->m_player)
            {
                int64_t aoff = ctx->m_player->GetAudioTimecodeOffset();
                gCoreContext->SaveSetting("AudioSyncOffset", QString::number(aoff));
            }
            ctx->UnlockDeletePlayer(__FILE__, __LINE__);
            ReturnPlayerLock(ctx);
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

PictureAttribute TV::NextPictureAdjustType(
    PictureAdjustType type, MythPlayer *mp, PictureAttribute attr)
{
    if (!mp)
        return kPictureAttribute_None;

    uint sup = kPictureAttributeSupported_None;
    if ((kAdjustingPicture_Playback == type) && mp && mp->GetVideoOutput())
    {
        sup = mp->GetVideoOutput()->GetSupportedPictureAttributes();
        if (mp->HasAudioOut() && mp->PlayerControlsVolume())
            sup |= kPictureAttributeSupported_Volume;
    }
    else if ((kAdjustingPicture_Channel == type) ||
             (kAdjustingPicture_Recording == type))
    {
        sup = (kPictureAttributeSupported_Brightness |
               kPictureAttributeSupported_Contrast |
               kPictureAttributeSupported_Colour |
               kPictureAttributeSupported_Hue);
    }

    return ::next((PictureAttributeSupported)sup, attr);
}

void TV::DoToggleNightMode(const PlayerContext *ctx)
{
    ctx->LockDeletePlayer(__FILE__, __LINE__);
    ctx->m_player->ToggleNightMode();
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);
}

void TV::DoTogglePictureAttribute(const PlayerContext *ctx,
                                  PictureAdjustType type)
{
    ctx->LockDeletePlayer(__FILE__, __LINE__);
    PictureAttribute attr = NextPictureAdjustType(type, ctx->m_player,
                                                  m_adjustingPictureAttribute);
    if (kPictureAttribute_None == attr)
    {
        ctx->UnlockDeletePlayer(__FILE__, __LINE__);
        return;
    }

    m_adjustingPicture          = type;
    m_adjustingPictureAttribute = attr;

    QString title = toTitleString(type);

    int value = 99;
    if (kAdjustingPicture_Playback == type)
    {
        if (!ctx->m_player)
        {
            ctx->UnlockDeletePlayer(__FILE__, __LINE__);
            return;
        }
        if (kPictureAttribute_Volume != m_adjustingPictureAttribute)
        {
            value = ctx->m_player->GetVideoOutput()->GetPictureAttribute(attr);
        }
        else if (ctx->m_player->HasAudioOut())
        {
            value = ctx->m_player->GetVolume();
            title = tr("Adjust Volume");
        }
    }
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);

    if (ctx->m_recorder && (kAdjustingPicture_Playback != type))
    {
        value = ctx->m_recorder->GetPictureAttribute(attr);
    }

    QString text = toString(attr) + " " + toTypeString(type);

    UpdateOSDStatus(ctx, title, text, QString::number(value),
                    kOSDFunctionalType_PictureAdjust, "%",
                    value * 10, kOSDTimeout_Med);
    SetUpdateOSDPosition(false);
}

void TV::DoChangePictureAttribute(
    PlayerContext *ctx,
    PictureAdjustType type, PictureAttribute attr,
    bool up, int newvalue)
{
    int value = 99;

    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (kAdjustingPicture_Playback == type)
    {
        if (kPictureAttribute_Volume == attr)
        {
            ctx->UnlockDeletePlayer(__FILE__, __LINE__);
            ChangeVolume(ctx, up, newvalue);
            return;
        }
        if (!ctx->m_player)
        {
            ctx->UnlockDeletePlayer(__FILE__, __LINE__);
            return;
        }

        if (ctx->m_player->GetVideoOutput())
        {
            MythVideoOutput *vo = ctx->m_player->GetVideoOutput();
            if ((newvalue >= 0) && (newvalue <= 100))
                value = vo->SetPictureAttribute(attr, newvalue);
            else
                value = vo->ChangePictureAttribute(attr, up);
        }
    }
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);

    if (ctx->m_recorder && (kAdjustingPicture_Playback != type))
    {
        value = ctx->m_recorder->ChangePictureAttribute(type, attr, up);
    }

    QString text = toString(attr) + " " + toTypeString(type);

    UpdateOSDStatus(ctx, toTitleString(type), text, QString::number(value),
                    kOSDFunctionalType_PictureAdjust, "%",
                    value * 10, kOSDTimeout_Med);
    SetUpdateOSDPosition(false);
}

void TV::SetActive(PlayerContext *lctx, int index, bool osd_msg)
{
    if (!lctx)
        return;

    int new_index = (index < 0) ? (m_playerActive+1) % m_player.size() : index;
    new_index = ((uint)new_index >= m_player.size()) ? 0 : new_index;

    QString loc = LOC + QString("(%1,%2) %3 -> %4")
        .arg(index).arg((osd_msg) ? "with OSD" : "w/o OSD")
        .arg(m_playerActive).arg(new_index);

    LOG(VB_PLAYBACK, LOG_DEBUG, loc + " -- begin");

    for (size_t i = 0; i < m_player.size(); i++)
        ClearOSD(GetPlayer(lctx,i));

    m_playerActive = new_index;

    for (int i = 0; i < (int)m_player.size(); i++)
    {
        PlayerContext *ctx = GetPlayer(lctx, i);
        ctx->LockDeletePlayer(__FILE__, __LINE__);
        if (ctx->m_player)
            ctx->m_player->SetPIPActive(i == m_playerActive);
        ctx->UnlockDeletePlayer(__FILE__, __LINE__);
    }

    if (osd_msg && !GetPlayer(lctx, -1)->IsPIP() && m_player.size() > 1)
    {
        PlayerContext *actx = GetPlayer(lctx, -1);
        SetOSDMessage(actx, tr("Active Changed"));
    }

    LOG(VB_PLAYBACK, LOG_DEBUG, loc + " -- end");
}

void TV::ShowOSDCutpoint(PlayerContext *ctx, const QString &type)
{
    if (type == "EDIT_CUT_POINTS")
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
    else if (type == "EDIT_CUT_POINTS_COMPACT")
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
    else if (type == "EXIT_EDIT_MODE")
    {
        OSD *osd = GetOSDLock(ctx);
        if (!osd)
        {
            ReturnOSDLock(ctx, osd);
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
        ReturnOSDLock(ctx, osd);
    }
}

bool TV::HandleOSDCutpoint(PlayerContext *ctx, const QString& action)
{
    bool res = true;
    if (!DialogIsVisible(ctx, OSD_DLG_CUTPOINT))
        return res;

    OSD *osd = GetOSDLock(ctx);
    if (action == "DONOTHING" && osd)
    {
    }
    else if (osd)
    {
        QStringList actions(action);
        if (!ctx->m_player->HandleProgramEditorActions(actions))
            LOG(VB_GENERAL, LOG_ERR, LOC + "Unrecognised cutpoint action");
        else
            m_editMode = ctx->m_player->GetEditMode();
    }
    ReturnOSDLock(ctx, osd);
    return res;
}

/** \fn TV::StartProgramEditMode(PlayerContext *ctx)
 *  \brief Starts Program Cut Map Editing mode
 */
void TV::StartProgramEditMode(PlayerContext *ctx)
{
    ctx->LockPlayingInfo(__FILE__, __LINE__);
    bool isEditing = ctx->m_playingInfo->QueryIsEditing();
    ctx->UnlockPlayingInfo(__FILE__, __LINE__);

    if (isEditing)
    {
        ShowOSDAlreadyEditing(ctx);
        return;
    }

    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (ctx->m_player)
        m_editMode = ctx->m_player->EnableEdit();
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);
}

void TV::ShowOSDAlreadyEditing(PlayerContext *ctx)
{
    OSD *osd = GetOSDLock(ctx);
    if (osd)
    {
        osd->DialogQuit();
        bool was_paused = ContextIsPaused(ctx, __FILE__, __LINE__);
        if (!was_paused)
            DoTogglePause(ctx, true);

        QString message = tr("This program is currently being edited");
        osd->DialogShow(OSD_DLG_EDITING, message);
        QString def = QString("DIALOG_EDITING_CONTINUE_%1").arg(was_paused);
        osd->DialogAddButton(tr("Continue Editing"), def, false, true);
        osd->DialogAddButton(tr("Do not edit"),
                             QString("DIALOG_EDITING_STOP_%1").arg(was_paused));
        osd->DialogBack("", def, true);
    }
    ReturnOSDLock(ctx, osd);
}

void TV::HandleOSDAlreadyEditing(PlayerContext *ctx, const QString& action,
                                 bool was_paused)
{
    if (!DialogIsVisible(ctx, OSD_DLG_EDITING))
        return;

    bool paused = ContextIsPaused(ctx, __FILE__, __LINE__);

    if (action == "STOP")
    {
        ctx->LockPlayingInfo(__FILE__, __LINE__);
        if (ctx->m_playingInfo)
            ctx->m_playingInfo->SaveEditing(false);
        ctx->UnlockPlayingInfo(__FILE__, __LINE__);
        if (!was_paused && paused)
            DoTogglePause(ctx, true);
    }
    else // action == "CONTINUE"
    {
        ctx->LockDeletePlayer(__FILE__, __LINE__);
        if (ctx->m_player)
        {
            ctx->m_playingInfo->SaveEditing(false);
            m_editMode = ctx->m_player->EnableEdit();
            if (!m_editMode && !was_paused && paused)
                DoTogglePause(ctx, false);
        }
        ctx->UnlockDeletePlayer(__FILE__, __LINE__);
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
void TV::StartChannelEditMode(PlayerContext *ctx)
{
    OSD *osd = GetOSDLock(ctx);
    if (!ctx->m_recorder || !osd)
    {
        ReturnOSDLock(ctx, osd);
        return;
    }
    ReturnOSDLock(ctx, osd);

    QMutexLocker locker(&m_chanEditMapLock);

    // Get the info available from the backend
    m_chanEditMap.clear();
    ctx->m_recorder->GetChannelInfo(m_chanEditMap);

    // Update with XDS Info
    ChannelEditAutoFill(ctx, m_chanEditMap);

    // Set proper initial values for channel editor, and make it visible..
    osd = GetOSDLock(ctx);
    if (osd)
    {
        osd->DialogQuit();
        osd->DialogShow(OSD_DLG_EDITOR);
        osd->SetText(OSD_DLG_EDITOR, m_chanEditMap, kOSDTimeout_None);
    }
    ReturnOSDLock(ctx, osd);
}

void TV::StartOsdNavigation(PlayerContext *ctx)
{
    OSD *osd = GetOSDLock(ctx);
    if (osd)
    {
        osd->DialogQuit();
        osd->HideAll();
        ToggleOSD(ctx, true);
        osd->DialogShow(OSD_DLG_NAVIGATE);
    }
    ReturnOSDLock(ctx, osd);
    UpdateNavDialog(ctx);
}

/**
 *  \brief Processes channel editing key.
 */
bool TV::HandleOSDChannelEdit(PlayerContext *ctx, const QString& action)
{
    QMutexLocker locker(&m_chanEditMapLock);
    bool hide = false;

    if (!DialogIsVisible(ctx, OSD_DLG_EDITOR))
        return hide;

    OSD *osd = GetOSDLock(ctx);
    if (osd && action == "PROBE")
    {
        InfoMap infoMap;
        osd->DialogGetText(infoMap);
        ChannelEditAutoFill(ctx, infoMap);
        insert_map(m_chanEditMap, infoMap);
        osd->SetText(OSD_DLG_EDITOR, m_chanEditMap, kOSDTimeout_None);
    }
    else if (osd && action == "OK")
    {
        InfoMap infoMap;
        osd->DialogGetText(infoMap);
        insert_map(m_chanEditMap, infoMap);
        ctx->m_recorder->SetChannelInfo(m_chanEditMap);
        hide = true;
    }
    else if (osd && action == "QUIT")
    {
        hide = true;
    }
    ReturnOSDLock(ctx, osd);
    return hide;
}

/** \fn TV::ChannelEditAutoFill(const PlayerContext*,InfoMap&) const
 *  \brief Automatically fills in as much information as possible.
 */
void TV::ChannelEditAutoFill(const PlayerContext *ctx, InfoMap &infoMap)
{
#if 0
    const QString keys[4] = { "XMLTV", "callsign", "channame", "channum", };
#endif

    // fill in uninitialized and unchanged fields from XDS
    ChannelEditXDSFill(ctx, infoMap);
}

void TV::ChannelEditXDSFill(const PlayerContext *ctx, InfoMap &infoMap)
{
    QMap<QString,bool> modifiable;
    if (!(modifiable["callsign"] = infoMap["callsign"].isEmpty()))
    {
        QString unsetsign = tr("UNKNOWN%1", "Synthesized callsign");
        uint    unsetcmpl = unsetsign.length() - 2;
        unsetsign = unsetsign.left(unsetcmpl);
        if (infoMap["callsign"].left(unsetcmpl) == unsetsign) // was unsetcmpl????
            modifiable["callsign"] = true;
    }
    modifiable["channame"] = infoMap["channame"].isEmpty();

    const QString xds_keys[2] = { "callsign", "channame", };
    for (const auto & key : xds_keys)
    {
        if (!modifiable[key])
            continue;

        ctx->LockDeletePlayer(__FILE__, __LINE__);
        QString tmp = ctx->m_player->GetXDS(key).toUpper();
        ctx->UnlockDeletePlayer(__FILE__, __LINE__);

        if (tmp.isEmpty())
            continue;

        if ((key == "callsign") &&
            ((tmp.length() > 5) || (tmp.indexOf(" ") >= 0)))
        {
            continue;
        }

        infoMap[key] = tmp;
    }
}

void TV::OSDDialogEvent(int result, const QString& text, QString action)
{
    PlayerContext *actx = GetPlayerReadLock(-1, __FILE__, __LINE__);

    LOG(VB_GENERAL, LOG_DEBUG, LOC +
        QString("result %1 text %2 action %3")
            .arg(result).arg(text).arg(action));

    bool hide = true;
    if (result == 100)
        hide = false;

    bool handled = true;
    if (action.startsWith("DIALOG_"))
    {
        action.remove("DIALOG_");
        QStringList desc = action.split("_");
        bool valid = desc.size() == 3;
        if (valid && desc[0] == ACTION_JUMPREC)
        {
            FillOSDMenuJumpRec(actx, desc[1], desc[2].toInt(), text);
            hide = false;
        }
        else if (valid && desc[0] == "VIDEOEXIT")
        {
            hide = HandleOSDVideoExit(actx, desc[1]);
        }
        else if (valid && desc[0] == "SLEEP")
        {
            HandleOSDSleep(actx, desc[1]);
        }
        else if (valid && desc[0] == "IDLE")
        {
            HandleOSDIdle(actx, desc[1]);
        }
        else if (valid && desc[0] == "INFO")
        {
            HandleOSDInfo(actx, desc[1]);
        }
        else if (valid && desc[0] == "EDITING")
        {
            HandleOSDAlreadyEditing(actx, desc[1], desc[2].toInt() != 0);
        }
        else if (valid && desc[0] == "ASKALLOW")
        {
            HandleOSDAskAllow(actx, desc[1]);
        }
        else if (valid && desc[0] == "EDITOR")
        {
            hide = HandleOSDChannelEdit(actx, desc[1]);
        }
        else if (valid && desc[0] == "CUTPOINT")
        {
            hide = HandleOSDCutpoint(actx, desc[1]);
        }
        else if ((valid && desc[0] == "DELETE") ||
                 (valid && desc[0] == "CONFIRM"))
        {
        }
        else if (valid && desc[0] == ACTION_PLAY)
        {
            DoPlay(actx);
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, "Unrecognised dialog event.");
        }
    }
    else if (result < 0)
        ; // exit dialog // NOLINT(bugprone-branch-clone)
    else if (HandleTrackAction(actx, action))
        ;
    else if (action == ACTION_PAUSE)
        DoTogglePause(actx, true);
    else if (action == ACTION_STOP)
    {
        PrepareToExitPlayer(actx, __LINE__);
        SetExitPlayer(true, true);
    }
    else if (action == "CANCELPLAYLIST")
    {
        setInPlayList(false);
        MythEvent xe("CANCEL_PLAYLIST");
        gCoreContext->dispatch(xe);
    }
    else if (action == ACTION_JUMPFFWD)
        DoJumpFFWD(actx);
    else if (action == ACTION_JUMPRWND)
        DoJumpRWND(actx);
    else if (action == ACTION_SEEKFFWD)
        DoSeekFFWD(actx);
    else if (action == ACTION_SEEKRWND)
        DoSeekRWND(actx);
    else if (action.startsWith("DEINTERLACER"))
        HandleDeinterlacer(actx, action);
    else if (action == ACTION_TOGGLEOSDDEBUG)
        ToggleOSDDebug(actx);
    else if (action == "TOGGLEMANUALZOOM")
        SetManualZoom(actx, true, tr("Zoom Mode ON"));
    else if (action == ACTION_BOTTOMLINEMOVE)
        ToggleMoveBottomLine(actx);
    else if (action == ACTION_BOTTOMLINESAVE)
        SaveBottomLine(actx);
    else if (action == "TOGGLESTRETCH")
        ToggleTimeStretch(actx);
    else if (action == ACTION_ENABLEUPMIX)
        EnableUpmix(actx, true);
    else if (action == ACTION_DISABLEUPMIX)
        EnableUpmix(actx, false);
    else if (action.startsWith("ADJUSTSTRETCH"))
    {
        bool floatRead = false;
        float stretch = action.right(action.length() - 13).toFloat(&floatRead);
        if (floatRead &&
            stretch <= 2.0F &&
            stretch >= 0.48F)
        {
            actx->m_tsNormal = stretch;   // alter speed before display
        }

        StopFFRew(actx);

        if (ContextIsPaused(actx, __FILE__, __LINE__))
            DoTogglePause(actx, true);

        ChangeTimeStretch(actx, 0, !floatRead);   // just display
    }
    else if (action.startsWith("SELECTSCAN_"))
        OverrideScan(actx, static_cast<FrameScanType>(action.right(1).toInt()));
    else if (action.startsWith(ACTION_TOGGELAUDIOSYNC))
        ChangeAudioSync(actx, 0);
    else if (action == ACTION_TOGGLESUBTITLEZOOM)
        ChangeSubtitleZoom(actx, 0);
    else if (action == ACTION_TOGGLESUBTITLEDELAY)
        ChangeSubtitleDelay(actx, 0);
    else if (action == ACTION_TOGGLEVISUALISATION)
        EnableVisualisation(actx, false, true /*toggle*/);
    else if (action == ACTION_ENABLEVISUALISATION)
        EnableVisualisation(actx, true);
    else if (action == ACTION_DISABLEVISUALISATION)
        EnableVisualisation(actx, false);
    else if (action.startsWith(ACTION_TOGGLESLEEP))
    {
        ToggleSleepTimer(actx, action.left(13));
    }
    else if (action.startsWith("TOGGLEPICCONTROLS"))
    {
        m_adjustingPictureAttribute = (PictureAttribute)
            (action.right(1).toInt() - 1);
        DoTogglePictureAttribute(actx, kAdjustingPicture_Playback);
    }
    else if (action == ACTION_TOGGLENIGHTMODE)
    {
        DoToggleNightMode(actx);
    }
    else if (action == "TOGGLEASPECT")
        ToggleAspectOverride(actx);
    else if (action.startsWith("TOGGLEASPECT"))
    {
        ToggleAspectOverride(actx,
                             (AspectOverrideMode) action.right(1).toInt());
    }
    else if (action == "TOGGLEFILL")
        ToggleAdjustFill(actx);
    else if (action.startsWith("TOGGLEFILL"))
    {
        ToggleAdjustFill(actx, (AdjustFillMode) action.right(1).toInt());
    }
    else if (action == "MENU")
         ShowOSDMenu();
    else if (action == "AUTODETECT_FILL")
    {
        actx->m_player->m_detectLetterBox->SetDetectLetterbox(!actx->m_player->m_detectLetterBox->GetDetectLetterbox());
    }
    else if (action == ACTION_GUIDE)
        EditSchedule(actx, kScheduleProgramGuide);
    else if (action.startsWith("CHANGROUP_") && m_dbUseChannelGroups)
    {
        if (action == "CHANGROUP_ALL_CHANNELS")
        {
            UpdateChannelList(-1);
        }
        else
        {
            action.remove("CHANGROUP_");

            UpdateChannelList(action.toInt());

            // make sure the current channel is from the selected group
            // or tune to the first in the group
            QString cur_channum;
            QString new_channum;
            if (actx->m_tvchain)
            {
                QMutexLocker locker(&m_channelGroupLock);
                const ChannelInfoList &list = m_channelGroupChannelList;
                cur_channum = actx->m_tvchain->GetChannelName(-1);
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

            if (actx->m_tvchain)
            {
                // Only change channel if new channel != current channel
                if (cur_channum != new_channum && !new_channum.isEmpty())
                {
                    QMutexLocker locker(&m_timerIdLock);
                    m_queuedInput   = new_channum;
                    m_queuedChanNum = new_channum;
                    m_queuedChanID  = 0;
                    if (!m_queueInputTimerId)
                        m_queueInputTimerId = StartTimer(10, __LINE__);
                }

                // Turn off OSD Channel Num so the channel
                // changes right away
                HideOSDWindow(actx, "osd_input");
            }
        }
    }
    else if (action == ACTION_FINDER)
        EditSchedule(actx, kScheduleProgramFinder);
    else if (action == "SCHEDULE")
        EditSchedule(actx, kScheduledRecording);
    else if (action == ACTION_VIEWSCHEDULED)
        EditSchedule(actx, kViewSchedule);
    else if (action.startsWith("VISUALISER"))
        EnableVisualisation(actx, true, false, action);
    else if (action.startsWith("3D"))
        Handle3D(actx, action);
    else if (HandleJumpToProgramAction(actx, QStringList(action)))
    {
    }
    else if (PxPHandleAction(actx, QStringList(action)))
    {
        for (size_t i = 0; i < m_player.size(); i++)
            ClearOSD(GetPlayer(actx,i));
        actx = GetPlayer(actx,-1); // "NEXTPIPWINDOW" changes active context..
    }
    else if (StateIsLiveTV(GetState(actx)))
    {
        if (action == "TOGGLEBROWSE")
            m_browseHelper->BrowseStart(actx);
        else if (action == "PREVCHAN")
            PopPreviousChannel(actx, true);
        else if (action.startsWith("SWITCHTOINPUT_"))
        {
            m_switchToInputId = action.mid(14).toUInt();
            QMutexLocker locker(&m_timerIdLock);
            if (!m_switchToInputTimerId)
                m_switchToInputTimerId = StartTimer(1, __LINE__);
        }
        else if (action == "EDIT")
        {
            StartChannelEditMode(actx);
            hide = false;
        }
        else
            handled = false;
    }
    else
        handled = false;
    if (!handled && StateIsPlaying(actx->GetState()))
    {
        handled = true;
        if (action == ACTION_JUMPTODVDROOTMENU ||
            action == ACTION_JUMPTODVDCHAPTERMENU ||
            action == ACTION_JUMPTOPOPUPMENU ||
            action == ACTION_JUMPTODVDTITLEMENU)
        {
            QString menu = "root";
            if (action == ACTION_JUMPTODVDCHAPTERMENU)
                menu = "chapter";
            else if (action == ACTION_JUMPTODVDTITLEMENU)
                menu = "title";
            else if (action == ACTION_JUMPTOPOPUPMENU)
                menu = "popup";
            actx->LockDeletePlayer(__FILE__, __LINE__);
            if (actx->m_player)
                actx->m_player->GoToMenu(menu);
            actx->UnlockDeletePlayer(__FILE__, __LINE__);
        }
        else if (action.startsWith(ACTION_JUMPCHAPTER))
        {
            int chapter = action.right(3).toInt();
            DoJumpChapter(actx, chapter);
        }
        else if (action.startsWith(ACTION_SWITCHTITLE))
        {
            int title = action.right(3).toInt();
            DoSwitchTitle(actx, title);
        }
        else if (action.startsWith(ACTION_SWITCHANGLE))
        {
            int angle = action.right(3).toInt();
            DoSwitchAngle(actx, angle);
        }
        else if (action == "EDIT")
        {
            StartProgramEditMode(actx);
            hide = false;
        }
        else if (action == "TOGGLEAUTOEXPIRE")
            ToggleAutoExpire(actx);
        else if (action.startsWith("TOGGLECOMMSKIP"))
        {
            SetAutoCommercialSkip(
                actx, (CommSkipMode)(action.right(1).toInt()));
        }
        else if (action == "QUEUETRANSCODE")
        {
            DoQueueTranscode(actx, "Default");
        }
        else if (action == "QUEUETRANSCODE_AUTO")
        {
            DoQueueTranscode(actx, "Autodetect");
        }
        else if (action == "QUEUETRANSCODE_HIGH")
        {
            DoQueueTranscode(actx, "High Quality");
        }
        else if (action == "QUEUETRANSCODE_MEDIUM")
        {
            DoQueueTranscode(actx, "Medium Quality");
        }
        else if (action == "QUEUETRANSCODE_LOW")
        {
            DoQueueTranscode(actx, "Low Quality");
        }
        else
        {
            handled = false;
        }
    }
    if (!handled)
    {
        bool isDVD = actx->m_buffer && actx->m_buffer->IsDVD();
        bool isMenuOrStill = actx->m_buffer && actx->m_buffer->IsInDiscMenuOrStillFrame();
        handled = ActiveHandleAction(actx, QStringList(action), isDVD, isMenuOrStill);
    }
    if (!handled)
            handled = ActivePostQHandleAction(actx, QStringList(action));

    if (!handled)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Unknown menu action selected: " + action);
        hide = false;
    }

    if (hide)
    {
        OSD *osd = GetOSDLock(actx);
        if (osd)
            osd->DialogQuit();
        ReturnOSDLock(actx, osd);
    }

    ReturnPlayerLock(actx);
}

bool TV::DialogIsVisible(PlayerContext *ctx, const QString &dialog)
{
    bool visible = false;
    OSD *osd = GetOSDLock(ctx);
    if (osd)
        visible = osd->DialogVisible(dialog);
    ReturnOSDLock(ctx, osd);
    return visible;
}

void TV::HandleOSDInfo(PlayerContext *ctx, const QString& action)
{
    if (!DialogIsVisible(ctx, OSD_DLG_INFO))
        return;

    if (action == "CHANNELLOCK")
    {
        m_lockTimerOn = false;
    }
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

QDomElement MenuBase::GetRoot(void) const
{
    return m_document->documentElement();
}

QString MenuBase::Translate(const QString &text) const
{
    return qApp->translate(m_translationContext, text.toUtf8(), nullptr);
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
    addButton(c, osd, active, result, (action), (text), "", false, "")
#define BUTTON2(action, textActive, textInactive) \
    addButton(c, osd, active, result, (action), (textActive), \
              (textInactive), false, "")
#define BUTTON3(action, textActive, textInactive, isMenu)     \
    addButton(c, osd, active, result, (action), (textActive), \
              (textInactive), (isMenu), "")

bool TV::MenuItemDisplay(const MenuItemContext &c)
{
    if (&c.m_menu == &m_playbackMenu ||
        &c.m_menu == &m_playbackCompactMenu)
    {
        return MenuItemDisplayPlayback(c);
    }
    if (&c.m_menu == &m_cutlistMenu ||
        &c.m_menu == &m_cutlistCompactMenu)
    {
        return MenuItemDisplayCutlist(c);
    }
    return false;
}

bool TV::MenuItemDisplayCutlist(const MenuItemContext &c)
{
    MenuCategory category = c.m_category;
    const QString &actionName = c.m_action;

    bool result = false;
    PlayerContext *ctx = m_tvmCtx;
    OSD *osd = m_tvmOsd;
    if (!osd)
        return result;
    if (category == kMenuCategoryMenu)
    {
        result = c.m_menu.Show(c.m_node, QDomNode(), *this, false);
        if (result && c.m_doDisplay)
        {
            QVariant v;
            v.setValue(MenuNodeTuple(c.m_menu, c.m_node));
            osd->DialogAddButton(c.m_menuName, v, true,
                                 c.m_currentContext != kMenuCurrentDefault);
        }
        return result;
    }
    ctx->LockDeletePlayer(__FILE__, __LINE__);
    uint64_t frame   = ctx->m_player->GetFramesPlayed();
    uint64_t previous_cut = ctx->m_player->GetNearestMark(frame, false);
    uint64_t next_cut = ctx->m_player->GetNearestMark(frame, true);
    uint64_t total_frames = ctx->m_player->GetTotalFrameCount();
    bool is_in_delete = ctx->m_player->IsInDelete(frame);
    bool is_temporary_mark = ctx->m_player->IsTemporaryMark(frame);
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
            active = ctx->m_player->DeleteMapHasUndo();
            //: %1 is the undo message
            QString text = tr("Undo - %1");
            addButton(c, osd, active, result, actionName, text, "", false,
                      ctx->m_player->DeleteMapGetUndoMessage());
        }
        else if (actionName == "DIALOG_CUTPOINT_REDO_0")
        {
            active = ctx->m_player->DeleteMapHasRedo();
            //: %1 is the redo message
            QString text = tr("Redo - %1");
            addButton(c, osd, active, result, actionName, text, "", false,
                      ctx->m_player->DeleteMapGetRedoMessage());
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
            QString text = GetMythMainWindow()->
                GetActionText(c.m_menu.GetKeyBindingContext(), actionName);
            if (text.isEmpty())
                text = GetMythMainWindow()->
                    GetActionText("Global", actionName);
            if (!text.isEmpty())
                BUTTON(actionName, text);
        }
    }
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);
    return result;
}

// Returns true if at least one item should be displayed.
bool TV::MenuItemDisplayPlayback(const MenuItemContext &c)
{
    MenuCategory category = c.m_category;
    const QString &actionName = c.m_action;

    bool result = false;
    bool active = true;
    PlayerContext *ctx = m_tvmCtx;
    OSD *osd = m_tvmOsd;
    if (!osd)
        return result;
    if (category == kMenuCategoryMenu)
    {
        result = c.m_menu.Show(c.m_node, QDomNode(), *this, false);
        if (result && c.m_doDisplay)
        {
            QVariant v;
            v.setValue(MenuNodeTuple(c.m_menu, c.m_node));
            osd->DialogAddButton(c.m_menuName, v, true,
                                 c.m_currentContext != kMenuCurrentDefault);
        }
        return result;
    }
    ctx->LockDeletePlayer(__FILE__, __LINE__);
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
        if (ctx == GetPlayer(ctx, 0))
        {
            for (int j = kAspect_Off; j < kAspect_END; j++)
            {
                // swap 14:9 and 16:9
                int i = ((kAspect_14_9 == j) ? kAspect_16_9 :
                         ((kAspect_16_9 == j) ? kAspect_14_9 : j));
                QString action = prefix + QString::number(i);
                active = (m_tvmAspectOverride == i);
                BUTTON(action, toString((AspectOverrideMode) i));
            }
        }
    }
    else if (matchesGroup(actionName, "TOGGLEFILL", category, prefix))
    {
        if (ctx == GetPlayer(ctx, 0))
        {
            for (int i = kAdjustFill_Off; i < kAdjustFill_END; i++)
            {
                QString action = prefix + QString::number(i);
                active = (m_tvmAdjustFill == i);
                BUTTON(action, toString((AdjustFillMode) i));
            }
        }
    }
    else if (matchesGroup(actionName, "TOGGLEPICCONTROLS", category, prefix))
    {
        if (ctx == GetPlayer(ctx, 0))
        {
            for (int i = kPictureAttribute_MIN; i < kPictureAttribute_MAX; i++)
            {
                if (toMask((PictureAttribute)i) & m_tvmSup)
                {
                    QString action = prefix + QString::number(i - kPictureAttribute_MIN);
                    if ((PictureAttribute)i != kPictureAttribute_Range)
                        BUTTON(action, toString((PictureAttribute) i));
                }
            }
        }
    }
    else if (matchesGroup(actionName, "3D", category, prefix))
    {
        if (m_tvmStereoAllowed)
        {
            active = (m_tvmStereoMode == kStereoscopicModeNone);
            BUTTON(ACTION_3DNONE, tr("None"));
            active = (m_tvmStereoMode == kStereoscopicModeSideBySide);
            BUTTON(ACTION_3DSIDEBYSIDE, tr("Side by Side"));
            active = (m_tvmStereoMode == kStereoscopicModeSideBySideDiscard);
            BUTTON(ACTION_3DSIDEBYSIDEDISCARD, tr("Discard Side by Side"));
            active = (m_tvmStereoMode == kStereoscopicModeTopAndBottom);
            BUTTON(ACTION_3DTOPANDBOTTOM, tr("Top and Bottom"));
            active = (m_tvmStereoMode == kStereoscopicModeTopAndBottomDiscard);
            BUTTON(ACTION_3DTOPANDBOTTOMDISCARD, tr("Discard Top and Bottom"));
        }
    }
    else if (matchesGroup(actionName, "SELECTSCAN_", category, prefix) && ctx->m_player)
    {
        FrameScanType scan = ctx->m_player->GetScanType();
        active = (scan == kScan_Detect);
        BUTTON("SELECTSCAN_0", ScanTypeToString(kScan_Detect));
        active = (scan == kScan_Progressive);
        BUTTON("SELECTSCAN_3", ScanTypeToString(kScan_Progressive));
        active = (scan == kScan_Interlaced);
        BUTTON("SELECTSCAN_1", ScanTypeToString(kScan_Interlaced));
        active = (scan == kScan_Intr2ndField);
        BUTTON("SELECTSCAN_2", ScanTypeToString(kScan_Intr2ndField));
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
        static struct {
            int     m_speedX100;
            QString m_suffix;
            QString m_trans;
        } s_speeds[] = {
            {  0, "",    tr("Adjust")},
            { 50, "0.5", tr("0.5x")},
            { 90, "0.9", tr("0.9x")},
            {100, "1.0", tr("1.0x")},
            {110, "1.1", tr("1.1x")},
            {120, "1.2", tr("1.2x")},
            {130, "1.3", tr("1.3x")},
            {140, "1.4", tr("1.4x")},
            {150, "1.5", tr("1.5x")},
        };
        for (auto & speed : s_speeds)
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
                active = ((int)(it->m_grpId) == m_channelGroupId);
                BUTTON(action, it->m_name);
            }
        }
    }
    else if (matchesGroup(actionName, "TOGGLECOMMSKIP", category, prefix))
    {
        if (m_tvmIsRecording || m_tvmIsRecorded)
        {
            static constexpr uint kCasOrd[] = { 0, 2, 1 };
            for (uint csm : kCasOrd)
            {
                const auto mode = (CommSkipMode) csm;
                QString action = prefix + QString::number(csm);
                active = (mode == m_tvmCurSkip);
                BUTTON(action, toString((CommSkipMode) csm));
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
                int hours   =  m_tvmChapterTimes[i] / 60 / 60;
                int minutes = (m_tvmChapterTimes[i] / 60) - (hours * 60);
                int secs    =  m_tvmChapterTimes[i] % 60;
                QString chapter1 = QString("%1").arg(i+1, size, 10, QChar(48));
                QString chapter2 = QString("%1").arg(i+1, 3   , 10, QChar(48));
                QString desc = chapter1 + QString(" (%1:%2:%3)")
                    .arg(hours,   2, 10, QChar(48))
                    .arg(minutes, 2, 10, QChar(48))
                    .arg(secs,    2, 10, QChar(48));
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
                QString desc = GetAngleName(ctx, i);
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
            if (GetTitleDuration(ctx, i) < 120) // Ignore < 2 minutes long
                continue;

            QString titleIdx = QString("%1").arg(i, 3, 10, QChar(48));
            QString desc = GetTitleName(ctx, i);
            QString action = prefix + titleIdx;
            active = (m_tvmCurrentTitle == i);
            BUTTON(action, desc);
        }
    }
    else if (matchesGroup(actionName, "SWITCHTOINPUT_", category, prefix))
    {
        if (ctx->m_recorder)
        {
            uint inputid  = ctx->GetCardID();
            vector<InputInfo> inputs = RemoteRequestFreeInputInfo(inputid);
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
        if (ctx->m_recorder)
        {
            uint inputid  = ctx->GetCardID();
            InfoMap info;
            ctx->m_recorder->GetChannelInfo(info);
            uint sourceid = info["sourceid"].toUInt();
            QMap<uint, bool> sourceids;
            vector<InputInfo> inputs = RemoteRequestFreeInputInfo(inputid);
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
        else if (actionName == "CREATEPIPVIEW")
        {
            MenuLazyInit(&m_tvmFreeRecorderCount);
            if (m_tvmFreeRecorderCount &&
                m_player.size() <= kMaxPIPCount && !m_tvmHasPBP && m_tvmAllowPIP)
            {
                BUTTON(actionName, tr("Open Live TV PIP"));
            }
        }
        else if (actionName == "CREATEPBPVIEW")
        {
            MenuLazyInit(&m_tvmFreeRecorderCount);
            if (m_tvmFreeRecorderCount &&
                m_player.size() < kMaxPBPCount && !m_tvmHasPIP && m_tvmAllowPBP)
            {
                BUTTON(actionName, tr("Open Live TV PBP"));
            }
        }
        else if (actionName == "JUMPRECPIP")
        {
            if (m_player.size() <= kMaxPIPCount &&
                !m_tvmHasPBP && m_tvmAllowPIP)
            {
                BUTTON(actionName, tr("Open Recording PIP"));
            }
        }
        else if (actionName == "JUMPRECPBP")
        {
            if (m_player.size() < kMaxPBPCount &&
                !m_tvmHasPIP && m_tvmAllowPBP)
            {
                BUTTON(actionName, tr("Open Recording PBP"));
            }
        }
        else if (actionName == "NEXTPIPWINDOW")
        {
            if (m_player.size() > 1)
                BUTTON(actionName, tr("Change Active Window"));
        }
        else if (actionName == "TOGGLEPIPMODE")
        {
            if (m_player.size() > 1)
            {
                const PlayerContext *mctx = GetPlayer(ctx, 0);
                const PlayerContext *octx = GetPlayer(ctx, 1);
                if (mctx == ctx && octx->IsPIP())
                    BUTTON(actionName, tr("Close PIP(s)", nullptr, m_player.size() - 1));
            }
        }
        else if (actionName == "TOGGLEPBPMODE")
        {
            if (m_player.size() > 1)
            {
                const PlayerContext *mctx = GetPlayer(ctx, 0);
                const PlayerContext *octx = GetPlayer(ctx, 1);
                if (mctx == ctx && octx->IsPBP())
                    BUTTON(actionName, tr("Close PBP(s)", nullptr, m_player.size() - 1));
            }
        }
        else if (actionName == "SWAPPIP")
        {
            const PlayerContext *mctx = GetPlayer(ctx, 0);
            if (mctx != ctx || m_player.size() == 2)
                BUTTON(actionName, tr("Swap Windows"));
        }
        else if (actionName == "TOGGLEPIPSTATE")
        {
            uint max_cnt = min(kMaxPBPCount, kMaxPIPCount+1);
            if (m_player.size() <= max_cnt &&
                ((m_tvmHasPIP && m_tvmAllowPBP) ||
                    (m_tvmHasPBP && m_tvmAllowPIP)) )
            {
                active = !m_tvmHasPBP;
                BUTTON2(actionName, tr("Switch to PBP"), tr("Switch to PIP"));
            }
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
            v.setValue(MenuNodeTuple(c.m_menu, c.m_node));
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
            QString text = GetMythMainWindow()->
                GetActionText(c.m_menu.GetKeyBindingContext(), actionName);
            if (text.isEmpty())
                text = GetMythMainWindow()->
                    GetActionText("Global", actionName);
            if (!text.isEmpty())
                BUTTON(actionName, text);
        }
    }

    ctx->UnlockDeletePlayer(__FILE__, __LINE__);
    return result;
}

void TV::MenuLazyInit(void *field)
{
    if (field == &m_tvmFreeRecorderCount)
    {
        if (m_tvmFreeRecorderCount < 0)
            m_tvmFreeRecorderCount = RemoteGetFreeRecorderCount();
    }
}

void TV::PlaybackMenuInit(const MenuBase &menu)
{
    m_tvmCtx = GetPlayerReadLock(-1, __FILE__, __LINE__);
    m_tvmOsd = GetOSDLock(m_tvmCtx);
    if (&menu != &m_playbackMenu && &menu != &m_playbackCompactMenu)
        return;

    PlayerContext *ctx = m_tvmCtx;

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
    m_tvmStereoMode        = kStereoscopicModeNone;
    m_tvmDoubleRate        = false;

    m_tvmSpeedX100         = (int)(round(ctx->m_tsNormal * 100));
    m_tvmState             = ctx->GetState();
    m_tvmIsRecording       = (m_tvmState == kState_WatchingRecording);
    m_tvmIsRecorded        = (m_tvmState == kState_WatchingPreRecorded);
    m_tvmIsVideo           = (m_tvmState == kState_WatchingVideo);
    m_tvmCurSkip           = kCommSkipOff;
    m_tvmIsPaused          = false;
    m_tvmAllowPIP          = IsPIPSupported(ctx);
    m_tvmAllowPBP          = IsPBPSupported(ctx);
    m_tvmHasPBP            = (m_player.size() > 1) && GetPlayer(ctx,1)->IsPBP();
    m_tvmHasPIP            = (m_player.size() > 1) && GetPlayer(ctx,1)->IsPIP();
    m_tvmFreeRecorderCount = -1;
    m_tvmIsDvd             = (m_tvmState == kState_WatchingDVD);
    m_tvmIsBd              = (ctx->m_buffer && ctx->m_buffer->IsBD() &&
                              ctx->m_buffer->BD()->IsHDMVNavigation());
    m_tvmJump              = (!m_tvmNumChapters && !m_tvmIsDvd &&
                              !m_tvmIsBd && ctx->m_buffer &&
                              ctx->m_buffer->IsSeekingAllowed());
    m_tvmIsLiveTv          = StateIsLiveTV(m_tvmState);
    m_tvmPreviousChan      = false;

    m_tvmNumChapters    = GetNumChapters(ctx);
    m_tvmCurrentChapter = GetCurrentChapter(ctx);
    m_tvmNumAngles      = GetNumAngles(ctx);
    m_tvmCurrentAngle   = GetCurrentAngle(ctx);
    m_tvmNumTitles      = GetNumTitles(ctx);
    m_tvmCurrentTitle   = GetCurrentTitle(ctx);
    m_tvmChapterTimes.clear();
    GetChapterTimes(ctx, m_tvmChapterTimes);

    m_tvmSubsCapMode   = 0;
    m_tvmSubsHaveText  = false;
    m_tvmSubsForcedOn  = true;
    m_tvmSubsEnabled   = false;
    m_tvmSubsHaveSubs  = false;

    for (int i = kTrackTypeUnknown ; i < kTrackTypeCount ; ++i)
        m_tvmCurtrack[i] = -1;

    if (m_tvmIsLiveTv)
    {
        QString prev_channum = ctx->GetPreviousChannel();
        QString cur_channum  = QString();
        if (ctx->m_tvchain)
            cur_channum = ctx->m_tvchain->GetChannelName(-1);
        if (!prev_channum.isEmpty() && prev_channum != cur_channum)
            m_tvmPreviousChan = true;
    }

    ctx->LockDeletePlayer(__FILE__, __LINE__);

    if (ctx->m_player && ctx->m_player->GetVideoOutput())
    {
        for (int i = kTrackTypeUnknown ; i < kTrackTypeCount ; ++i)
        {
            m_tvmTracks[i] = ctx->m_player->GetTracks(i);
            if (!m_tvmTracks[i].empty())
                m_tvmCurtrack[i] = ctx->m_player->GetTrack(i);
        }
        m_tvmSubsHaveSubs =
            !m_tvmTracks[kTrackTypeSubtitle].empty() ||
            m_tvmSubsHaveText ||
            !m_tvmTracks[kTrackTypeCC708].empty() ||
            !m_tvmTracks[kTrackTypeCC608].empty() ||
            !m_tvmTracks[kTrackTypeTeletextCaptions].empty() ||
            !m_tvmTracks[kTrackTypeRawText].empty();
        m_tvmAvsync = (ctx->m_player->GetTrackCount(kTrackTypeVideo) > 0) &&
            !m_tvmTracks[kTrackTypeAudio].empty();
        m_tvmVisual           = ctx->m_player->CanVisualise();
        m_tvmActive           = ctx->m_player->GetVisualiserName();
        m_tvmUpmixing         = ctx->m_player->GetAudio()->IsUpmixing();
        m_tvmCanUpmix         = ctx->m_player->GetAudio()->CanUpmix();
        m_tvmAspectOverride   = ctx->m_player->GetAspectOverride();
        m_tvmAdjustFill       = ctx->m_player->GetAdjustFill();
        m_tvmDoubleRate       = ctx->m_player->CanSupportDoubleRate();
        m_tvmCurSkip          = ctx->m_player->GetAutoCommercialSkip();
        m_tvmIsPaused         = ctx->m_player->IsPaused();
        m_tvmSubsCapMode      = ctx->m_player->GetCaptionMode();
        m_tvmSubsEnabled      = ctx->m_player->GetCaptionsEnabled();
        m_tvmSubsHaveText     = ctx->m_player->HasTextSubtitles();
        m_tvmSubsForcedOn     = ctx->m_player->GetAllowForcedSubtitles();
        //ctx->m_player->GetVideoOutput()->GetDeinterlacers(m_tvmDeinterlacers);
        //QStringList decoderdeints = ctx->m_player->GetMythCodecContext()->GetDeinterlacers();
        //m_tvmDeinterlacers.append(decoderdeints);
        //m_tvmCurrentDeinterlacer = ctx->m_player->GetMythCodecContext()->getDeinterlacerName();
        //if (m_tvmCurrentDeinterlacer.isEmpty())
        //    m_tvmCurrentDeinterlacer =
        //        ctx->m_player->GetVideoOutput()->GetDeinterlacer();
        if (m_tvmVisual)
            m_tvmVisualisers = ctx->m_player->GetVisualiserList();
        MythVideoOutput *vo = ctx->m_player->GetVideoOutput();
        if (vo)
        {
            m_tvmSup            = vo->GetSupportedPictureAttributes();
            m_tvmStereoAllowed  = vo->StereoscopicModesAllowed();
            m_tvmStereoMode     = vo->GetStereoscopicMode();
            m_tvmFillAutoDetect = vo->HasSoftwareFrames();  
        }
    }
    ctx->LockPlayingInfo(__FILE__, __LINE__);
    m_tvmIsOn = ctx->m_playingInfo->QueryAutoExpire() != kDisableAutoExpire;
    m_tvmTranscoding = JobQueue::IsJobQueuedOrRunning
        (JOB_TRANSCODE, ctx->m_playingInfo->GetChanID(),
         ctx->m_playingInfo->GetScheduledStartTime());
    ctx->UnlockPlayingInfo(__FILE__, __LINE__);

    ctx->UnlockDeletePlayer(__FILE__, __LINE__);
}

void TV::PlaybackMenuDeinit(const MenuBase &/*menu*/)
{
    ReturnOSDLock(m_tvmCtx, m_tvmOsd);
    ReturnPlayerLock(m_tvmCtx);
    m_tvmOsd = nullptr;
    m_tvmOsd = nullptr;
}

void TV::PlaybackMenuShow(const MenuBase &menu,
                          const QDomNode &node, const QDomNode &selected)
{
    PlaybackMenuInit(menu);
    if (m_tvmOsd)
    {
        bool isPlayback = (&menu == &m_playbackMenu ||
                           &menu == &m_playbackCompactMenu);
        bool isCutlist = (&menu == &m_cutlistMenu ||
                          &menu == &m_cutlistCompactMenu);
        m_tvmOsd->DialogShow(isPlayback ? OSD_DLG_MENU :
                             isCutlist ? OSD_DLG_CUTPOINT :
                             "???",
                             menu.GetName());
        menu.Show(node, selected, *this);
        QString text =
            menu.Translate(node.toElement().attribute("text", menu.GetName()));
        m_tvmOsd->DialogSetText(text);
        QDomNode parent = node.parentNode();
        if (!parent.parentNode().isNull())
        {
            QVariant v;
            v.setValue(MenuNodeTuple(menu, node));
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
    PlaybackMenuDeinit(menu);
}

void TV::MenuStrings(void)
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

void TV::FillOSDMenuJumpRec(PlayerContext* ctx, const QString &category,
                            int level, const QString &selected)
{
    // bool in_recgroup = !category.isEmpty() && level > 0;
    if (level < 0 || level > 1)
    {
        level = 0;
        // in_recgroup = false;
    }

    OSD *osd = GetOSDLock(ctx);
    if (osd)
    {
        QString title = tr("Recorded Program");
        osd->DialogShow("osd_jumprec", title);

        QMutexLocker locker(&m_progListsLock);
        m_progLists.clear();
        vector<ProgramInfo*> *infoList = RemoteGetRecordedList(0);
        bool LiveTVInAllPrograms = gCoreContext->GetBoolSetting("LiveTVInAllPrograms",false);
        if (infoList)
        {
            QList<QString> titles_seen;

            ctx->LockPlayingInfo(__FILE__, __LINE__);
            QString currecgroup = ctx->m_playingInfo->GetRecordingGroup();
            ctx->UnlockPlayingInfo(__FILE__, __LINE__);

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
            for (Iprog = m_progLists.begin(); Iprog != m_progLists.end(); ++Iprog)
            {
                const ProgramList &plist = *Iprog;
                uint progIndex = (uint) plist.size();
                const QString& group = Iprog.key();

                if (plist[0]->GetRecordingGroup() != currecgroup)
                    SetLastProgram(plist[0]);

                if (progIndex == 1 && level == 0)
                {
                    osd->DialogAddButton(Iprog.key(),
                                         QString("JUMPPROG %1 0").arg(group));
                }
                else if (progIndex > 1 && level == 0)
                {
                    QString act = QString("DIALOG_%1_%2_1")
                                    .arg(ACTION_JUMPREC).arg(group);
                    osd->DialogAddButton(group, act,
                                         true, selected == group);
                }
                else if (level == 1 && Iprog.key() == category)
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

        if (!category.isEmpty())
        {
            if (level == 1)
                osd->DialogBack(category, "DIALOG_" + ACTION_JUMPREC + "_X_0");
            else if (level == 0)
            {
                if (m_tvmJumprecBackHack.isValid())
                    osd->DialogBack("", m_tvmJumprecBackHack);
                else
                    osd->DialogBack(ACTION_JUMPREC,
                                    "DIALOG_MENU_" + ACTION_JUMPREC +"_0");
            }
        }
    }
    ReturnOSDLock(ctx, osd);
}

void TV::HandleDeinterlacer(PlayerContext *ctx, const QString &action)
{
    if (!action.startsWith("DEINTERLACER"))
        return;

    QString deint = action.mid(13);
    ctx->LockDeletePlayer(__FILE__, __LINE__);
    //if (ctx->m_player)
    //    ctx->m_player->ForceDeinterlacer(deint);
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);
}

void TV::OverrideScan(PlayerContext *Context, FrameScanType Scan)
{
    QString message;
    Context->LockDeletePlayer(__FILE__, __LINE__);
    if (Context->m_player)
    {
        Context->m_player->SetScanOverride(Scan);
        message = ScanTypeToString(Scan == kScan_Detect ? kScan_Detect :
                    Context->m_player->GetScanType(), Scan > kScan_Detect);
    }
    Context->UnlockDeletePlayer(__FILE__, __LINE__);

    if (!message.isEmpty())
        SetOSDMessage(Context, message);
}

void TV::ToggleAutoExpire(PlayerContext *ctx)
{
    QString desc;

    ctx->LockPlayingInfo(__FILE__, __LINE__);

    if (ctx->m_playingInfo->QueryAutoExpire() != kDisableAutoExpire)
    {
        ctx->m_playingInfo->SaveAutoExpire(kDisableAutoExpire);
        desc = tr("Auto-Expire OFF");
    }
    else
    {
        ctx->m_playingInfo->SaveAutoExpire(kNormalAutoExpire);
        desc = tr("Auto-Expire ON");
    }

    ctx->UnlockPlayingInfo(__FILE__, __LINE__);

    if (!desc.isEmpty())
        UpdateOSDSeekMessage(ctx, desc, kOSDTimeout_Med);
}

void TV::SetAutoCommercialSkip(const PlayerContext *ctx,
                               CommSkipMode skipMode)
{
    QString desc;

    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (ctx->m_player)
    {
        ctx->m_player->SetAutoCommercialSkip(skipMode);
        desc = toString(ctx->m_player->GetAutoCommercialSkip());
    }
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);

    if (!desc.isEmpty())
        UpdateOSDSeekMessage(ctx, desc, kOSDTimeout_Med);
}

void TV::SetManualZoom(const PlayerContext *ctx, bool zoomON, const QString& desc)
{
    if (ctx->GetPIPState() != kPIPOff)
        return;

    m_zoomMode = zoomON;
    if (zoomON)
        ClearOSD(ctx);

    if (!desc.isEmpty())
        UpdateOSDSeekMessage(ctx, desc,
                             zoomON ? kOSDTimeout_None : kOSDTimeout_Med);
}

bool TV::HandleJumpToProgramAction(
    PlayerContext *ctx, const QStringList &actions)
{
    const PlayerContext *mctx = GetPlayer(ctx, 0);
    TVState s = ctx->GetState();
    if (has_action(ACTION_JUMPPREV, actions) ||
        (has_action("PREVCHAN", actions) && !StateIsLiveTV(s)))
    {
        if (mctx == ctx)
        {
            PrepareToExitPlayer(ctx, __LINE__);
            m_jumpToProgram = true;
            SetExitPlayer(true, true);
        }
        else
        {
            // TODO
        }
        return true;
    }

    foreach (const auto & action, actions)
    {
        if (!action.startsWith("JUMPPROG"))
            continue;

        bool ok = false;
        QString progKey   = action.section(" ",1,-2);
        uint    progIndex = action.section(" ",-1,-1).toUInt(&ok);
        ProgramInfo *p = nullptr;

        if (ok)
        {
            QMutexLocker locker(&m_progListsLock);
            auto pit = m_progLists.find(progKey);
            if (pit != m_progLists.end())
            {
                const ProgramInfo *tmp = (*pit)[progIndex];
                if (tmp)
                    p = new ProgramInfo(*tmp);
            }
        }

        if (!p)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Failed to locate jump to program '%1' @ %2")
                    .arg(progKey).arg(action.section(" ",-1,-1)));
            return true;
        }

        PIPState state = kPIPOff;
        {
            QMutexLocker locker(&m_timerIdLock);
            state = m_jumpToProgramPIPState;
        }

        if (kPIPOff == state)
        {
            if (mctx == ctx)
            {
                PrepToSwitchToRecordedProgram(ctx, *p);
            }
            else
            {
                // TODO
            }
        }
        else
        {
            QString type = (kPIPonTV == m_jumpToProgramPIPState) ? "PIP" : "PBP";
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("Creating %1 with program: %2")
                    .arg(type).arg(p->toString(ProgramInfo::kTitleSubtitle)));

            if (m_jumpToProgramPIPState == kPIPonTV)
                CreatePIP(ctx, p);
            else if (m_jumpToProgramPIPState == kPBPLeft)
                CreatePBP(ctx, p);
        }

        delete p;

        return true;
    }

    bool wants_jump = has_action(ACTION_JUMPREC, actions);
    bool wants_pip = !wants_jump && has_action("JUMPRECPIP", actions);
    bool wants_pbp = !wants_jump && !wants_pip &&
        has_action("JUMPRECPBP", actions);

    if (!wants_jump && !wants_pip && !wants_pbp)
        return false;

    {
        QMutexLocker locker(&m_timerIdLock);
        m_jumpToProgramPIPState = wants_pip ? kPIPonTV :
            (wants_pbp ? kPBPLeft : kPIPOff);
    }

    if ((wants_pbp || wants_pip || m_dbJumpPreferOsd) &&
        (StateIsPlaying(s) || StateIsLiveTV(s)))
    {
        QMutexLocker locker(&m_timerIdLock);
        if (m_jumpMenuTimerId)
            KillTimer(m_jumpMenuTimerId);
        m_jumpMenuTimerId = StartTimer(1, __LINE__);
    }
    else if (RunPlaybackBoxPtr)
        EditSchedule(ctx, kPlaybackBox);
    else
        LOG(VB_GENERAL, LOG_ERR, "Failed to open jump to program GUI");

    return true;
}

#define MINUTE (60*1000)

void TV::ToggleSleepTimer(const PlayerContext *ctx, const QString &time)
{
    int mins = 0;

    if (time == ACTION_TOGGLESLEEP + "ON")
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
            m_sleepTimerId = StartTimer(m_sleepTimerTimeout, __LINE__);
        }
    }
    else
    {
        if (m_sleepTimerId)
        {
            KillTimer(m_sleepTimerId);
            m_sleepTimerId = 0;
        }

        if (time.length() > 11)
        {
            bool intRead = false;
            mins = time.right(time.length() - 11).toInt(&intRead);

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
                LOG(VB_GENERAL, LOG_ERR, LOC + "Invalid time " + time);
            }
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Invalid time string " + time);
        }

        if (mins)
        {
            m_sleepTimerTimeout = mins * MINUTE;
            m_sleepTimerId = StartTimer(m_sleepTimerTimeout, __LINE__);
        }
    }

    QString out;
    if (mins != 0)
        out = tr("Sleep") + " " + QString::number(mins);
    else
        out = tr("Sleep") + " " + m_sleepTimes[0].dispString;
    SetOSDMessage(ctx, out);
}

void TV::ShowNoRecorderDialog(const PlayerContext *ctx, NoRecorderMsg msgType)
{
    QString errorText;

    switch (msgType)
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

    OSD *osd = GetOSDLock(ctx);
    if (osd)
    {
        osd->DialogShow(OSD_DLG_INFO, errorText);
        osd->DialogAddButton(tr("OK"), "DIALOG_INFO_X_X");
    }
    else
    {
        ShowOkPopup(errorText);
    }
    ReturnOSDLock(ctx, osd);
}

/**
 *  \brief Used in ChangeChannel() to temporarily stop video output.
 */
void TV::PauseLiveTV(PlayerContext *ctx)
{
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("player ctx %1")
            .arg(find_player_index(ctx)));

    m_lockTimerOn = false;

    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (ctx->m_player && ctx->m_buffer)
    {
        ctx->m_buffer->IgnoreLiveEOF(true);
        ctx->m_buffer->StopReads();
        ctx->m_player->PauseDecoder();
        ctx->m_buffer->StartReads();
    }
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);

    // XXX: Get rid of this?
    ctx->m_recorder->PauseRecorder();

    ctx->m_lastSignalMsg.clear();
    ctx->m_lastSignalUIInfo.clear();

    m_lockTimerOn = false;

    QString input = ctx->m_recorder->GetInput();
    uint timeout  = ctx->m_recorder->GetSignalLockTimeout(input);

    if (timeout < 0xffffffff && !ctx->IsPIP())
    {
        m_lockTimer.start();
        m_lockTimerOn = true;
    }

    SetSpeedChangeTimer(0, __LINE__);
}

/**
 *  \brief Used in ChangeChannel() to restart video output.
 */
void TV::UnpauseLiveTV(PlayerContext *ctx, bool bQuietly /*=false*/)
{
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("player ctx %1")
            .arg(find_player_index(ctx)));

    if (ctx->HasPlayer() && ctx->m_tvchain)
    {
        ctx->ReloadTVChain();
        ctx->m_tvchain->JumpTo(-1, 1);
        ctx->LockDeletePlayer(__FILE__, __LINE__);
        if (ctx->m_player)
            ctx->m_player->Play(ctx->m_tsNormal, true, false);
        ctx->UnlockDeletePlayer(__FILE__, __LINE__);
        if (ctx->m_buffer)
            ctx->m_buffer->IgnoreLiveEOF(false);

        SetSpeedChangeTimer(0, __LINE__);
    }

    ITVRestart(ctx, true);

    if (ctx->HasPlayer() && !bQuietly)
    {
        UpdateOSDProgInfo(ctx, "program_info");
        UpdateLCD();
        ctx->PushPreviousChannel();
    }
}

/**
 * \brief Restart the MHEG/MHP engine.
 */
void TV::ITVRestart(PlayerContext *ctx, bool isLive)
{
    int chanid = -1;
    int sourceid = -1;

    if (ContextIsPaused(ctx, __FILE__, __LINE__))
        return;

    ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (ctx->m_playingInfo)
    {
        chanid = ctx->m_playingInfo->GetChanID();
        sourceid = ChannelUtil::GetSourceIDForChannel(chanid);
    }
    ctx->UnlockPlayingInfo(__FILE__, __LINE__);

    ctx->LockDeletePlayer(__FILE__, __LINE__);
    if (ctx->m_player)
        ctx->m_player->ITVRestart(chanid, sourceid, isLive);
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);
}

void TV::DoJumpFFWD(PlayerContext *ctx)
{
    if (GetState(ctx) == kState_WatchingDVD)
        DVDJumpForward(ctx);
    else if (GetNumChapters(ctx) > 0)
        DoJumpChapter(ctx, 9999);
    else
    {
        DoSeek(ctx, ctx->m_jumptime * 60, tr("Jump Ahead"),
               /*timeIsOffset*/true,
               /*honorCutlist*/true);
    }
}

void TV::DoSeekFFWD(PlayerContext *ctx)
{
    DoSeek(ctx, ctx->m_fftime, tr("Skip Ahead"),
            /*timeIsOffset*/true,
            /*honorCutlist*/true);
}

void TV::DoJumpRWND(PlayerContext *ctx)
{
    if (GetState(ctx) == kState_WatchingDVD)
        DVDJumpBack(ctx);
    else if (GetNumChapters(ctx) > 0)
        DoJumpChapter(ctx, -1);
    else
    {
        DoSeek(ctx, -ctx->m_jumptime * 60, tr("Jump Back"),
               /*timeIsOffset*/true,
               /*honorCutlist*/true);
    }
}

void TV::DoSeekRWND(PlayerContext *ctx)
{
    DoSeek(ctx, -ctx->m_rewtime, tr("Jump Back"),
            /*timeIsOffset*/true,
            /*honorCutlist*/true);
}

/*  \fn TV::DVDJumpBack(PlayerContext*)
    \brief jump to the previous dvd title or chapter
*/
void TV::DVDJumpBack(PlayerContext *ctx)
{
    auto *dvdrb = dynamic_cast<DVDRingBuffer*>(ctx->m_buffer);
    if (!ctx->HasPlayer() || !dvdrb)
        return;

    if (ctx->m_buffer->IsInDiscMenuOrStillFrame())
    {
        UpdateOSDSeekMessage(ctx, tr("Skip Back Not Allowed"), kOSDTimeout_Med);
    }
    else if (!dvdrb->StartOfTitle())
    {
        DoJumpChapter(ctx, -1);
    }
    else
    {
        uint titleLength = dvdrb->GetTotalTimeOfTitle();
        uint chapterLength = dvdrb->GetChapterLength();
        if ((titleLength == chapterLength) && chapterLength > 300)
        {
            DoSeek(ctx, -ctx->m_jumptime * 60, tr("Jump Back"),
                   /*timeIsOffset*/true,
                   /*honorCutlist*/true);
        }
        else
        {
            ctx->LockDeletePlayer(__FILE__, __LINE__);
            if (ctx->m_player)
                ctx->m_player->GoToDVDProgram(false);
            ctx->UnlockDeletePlayer(__FILE__, __LINE__);

            UpdateOSDSeekMessage(ctx, tr("Previous Title"), kOSDTimeout_Med);
        }
    }
}

/* \fn TV::DVDJumpForward(PlayerContext*)
 * \brief jump to the next dvd title or chapter
 */
void TV::DVDJumpForward(PlayerContext *ctx)
{
    auto *dvdrb = dynamic_cast<DVDRingBuffer*>(ctx->m_buffer);
    if (!ctx->HasPlayer() || !dvdrb)
        return;

    bool in_still = dvdrb->IsInStillFrame();
    bool in_menu  = dvdrb->IsInMenu();
    if (in_still && !dvdrb->NumMenuButtons())
    {
        dvdrb->SkipStillFrame();
        UpdateOSDSeekMessage(ctx, tr("Skip Still Frame"), kOSDTimeout_Med);
    }
    else if (!dvdrb->EndOfTitle() && !in_still && !in_menu)
    {
        DoJumpChapter(ctx, 9999);
    }
    else if (!in_still && !in_menu)
    {
        uint titleLength = dvdrb->GetTotalTimeOfTitle();
        uint chapterLength = dvdrb->GetChapterLength();
        uint currentTime = (uint)dvdrb->GetCurrentTime();
        if ((titleLength == chapterLength) &&
             (currentTime < (chapterLength - (ctx->m_jumptime * 60))) &&
             chapterLength > 300)
        {
            DoSeek(ctx, ctx->m_jumptime * 60, tr("Jump Ahead"),
                   /*timeIsOffset*/true,
                   /*honorCutlist*/true);
        }
        else
        {
            ctx->LockDeletePlayer(__FILE__, __LINE__);
            if (ctx->m_player)
                ctx->m_player->GoToDVDProgram(true);
            ctx->UnlockDeletePlayer(__FILE__, __LINE__);

            UpdateOSDSeekMessage(ctx, tr("Next Title"), kOSDTimeout_Med);
        }
    }
}

/* \fn TV::IsBookmarkAllowed(const PlayerContext*) const
 * \brief Returns true if bookmarks are allowed for the current player.
 */
bool TV::IsBookmarkAllowed(const PlayerContext *ctx)
{
    ctx->LockPlayingInfo(__FILE__, __LINE__);

    // Allow bookmark of "Record current LiveTV program"
    if (StateIsLiveTV(GetState(ctx)) && ctx->m_playingInfo &&
        (ctx->m_playingInfo->QueryAutoExpire() == kLiveTVAutoExpire))
    {
        ctx->UnlockPlayingInfo(__FILE__, __LINE__);
        return false;
    }

    if (StateIsLiveTV(GetState(ctx)) && !ctx->m_playingInfo)
    {
        ctx->UnlockPlayingInfo(__FILE__, __LINE__);
        return false;
    }

    ctx->UnlockPlayingInfo(__FILE__, __LINE__);

    return ctx->m_buffer && ctx->m_buffer->IsBookmarkAllowed();
}

/* \fn TV::IsDeleteAllowed(const PlayerContext*) const
 * \brief Returns true if the delete menu option should be offered.
 */
bool TV::IsDeleteAllowed(const PlayerContext *ctx)
{
    bool allowed = false;

    if (!StateIsLiveTV(GetState(ctx)))
    {
        ctx->LockPlayingInfo(__FILE__, __LINE__);
        ProgramInfo *curProgram = ctx->m_playingInfo;
        allowed = curProgram && curProgram->QueryIsDeleteCandidate(true);
        ctx->UnlockPlayingInfo(__FILE__, __LINE__);
    }

    return allowed;
}

void TV::ShowOSDStopWatchingRecording(PlayerContext *ctx)
{
    ClearOSD(ctx);

    if ((ctx != GetPlayer(ctx, 0)))
        return;

    if (!ContextIsPaused(ctx, __FILE__, __LINE__))
        DoTogglePause(ctx, false);

    QString message;
    QString videotype;

    if (StateIsLiveTV(GetState(ctx)))
        videotype = tr("Live TV");
    else if (ctx->m_buffer && ctx->m_buffer->IsDVD())
        videotype = tr("this DVD");

    ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (videotype.isEmpty() && ctx->m_playingInfo->IsVideo())
        videotype = tr("this Video");
    ctx->UnlockPlayingInfo(__FILE__, __LINE__);

    if (videotype.isEmpty())
        videotype = tr("this recording");

    OSD *osd = GetOSDLock(ctx);
    if (osd)
    {
        osd->DialogShow(OSD_DLG_VIDEOEXIT,
                        tr("You are exiting %1").arg(videotype));

        if (IsBookmarkAllowed(ctx))
        {
            osd->DialogAddButton(tr("Save this position and go to the menu"),
                                 "DIALOG_VIDEOEXIT_SAVEPOSITIONANDEXIT_0");
            osd->DialogAddButton(tr("Do not save, just exit to the menu"),
                                 ACTION_STOP);
        }
        else
            osd->DialogAddButton(tr("Exit %1").arg(videotype),
                                 ACTION_STOP);

        if (IsDeleteAllowed(ctx))
            osd->DialogAddButton(tr("Delete this recording"),
                                 "DIALOG_VIDEOEXIT_CONFIRMDELETE_0");

        osd->DialogAddButton(tr("Keep watching"),
                             "DIALOG_VIDEOEXIT_KEEPWATCHING_0");
        osd->DialogBack("", "DIALOG_VIDEOEXIT_KEEPWATCHING_0", true);
    }
    ReturnOSDLock(ctx, osd);

    QMutexLocker locker(&m_timerIdLock);
    if (m_videoExitDialogTimerId)
        KillTimer(m_videoExitDialogTimerId);
    m_videoExitDialogTimerId = StartTimer(kVideoExitDialogTimeout, __LINE__);
}

void TV::ShowOSDPromptDeleteRecording(PlayerContext *ctx, const QString& title,
                                      bool force)
{
    ctx->LockPlayingInfo(__FILE__, __LINE__);

    if (ctx->m_ffRewState ||
        StateIsLiveTV(ctx->GetState()) ||
        m_exitPlayerTimerId)
    {
        // this should only occur when the cat walks on the keyboard.
        LOG(VB_GENERAL, LOG_ERR, "It is unsafe to delete at the moment");
        ctx->UnlockPlayingInfo(__FILE__, __LINE__);
        return;
    }

    if ((ctx != GetPlayer(ctx, 0)))
    {
        // just to avoid confusion...
        LOG(VB_GENERAL, LOG_ERR, "Only the main program may be deleted");
        ctx->UnlockPlayingInfo(__FILE__, __LINE__);
        return;
    }

    bool paused = ContextIsPaused(ctx, __FILE__, __LINE__);
    if (!ctx->m_playingInfo->QueryIsDeleteCandidate(true))
    {
        LOG(VB_GENERAL, LOG_ERR,
            "This program cannot be deleted at this time.");
        ProgramInfo pginfo(*ctx->m_playingInfo);
        ctx->UnlockPlayingInfo(__FILE__, __LINE__);

        OSD *osd = GetOSDLock(ctx);
        if (osd && !osd->DialogVisible())
        {
            QString message = tr("Cannot delete program ") +
                QString("%1 ")
                .arg(pginfo.GetTitle());

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
                for (uint i = 0; i+2 < (uint)byWho.size(); i+=3)
                {
                    if (byWho[i+1] == gCoreContext->GetHostName() &&
                        byWho[i+0].contains(kPlayerInUseID))
                        continue;
                    if (byWho[i+0].contains(kRecorderInUseID))
                        continue;
                    message += " " + byWho[i+2];
                }
            }
            osd->DialogShow(OSD_DLG_DELETE, message);
            QString action = "DIALOG_DELETE_OK_0";
            osd->DialogAddButton(tr("OK"), action);
            osd->DialogBack("", action, true);
        }
        ReturnOSDLock(ctx, osd);
        // If the delete prompt is to be displayed at the end of a
        // recording that ends in a final cut region, it will get into
        // a loop of popping up the OK button while the cut region
        // plays.  Avoid this.
        if (ctx->m_player->IsNearEnd() && !paused)
            SetExitPlayer(true, true);

        return;
    }
    ctx->UnlockPlayingInfo(__FILE__, __LINE__);

    ClearOSD(ctx);

    if (!paused)
        DoTogglePause(ctx, false);

    InfoMap infoMap;
    ctx->GetPlayingInfoMap(infoMap);
    QString message = QString("%1\n%2\n%3")
        .arg(title).arg(infoMap["title"]).arg(infoMap["timedate"]);

    OSD *osd = GetOSDLock(ctx);
    if (osd && (!osd->DialogVisible() || force))
    {
        osd->DialogShow(OSD_DLG_VIDEOEXIT, message);
        if (title == "End Of Recording")
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

        QMutexLocker locker(&m_timerIdLock);
        if (m_videoExitDialogTimerId)
            KillTimer(m_videoExitDialogTimerId);
        m_videoExitDialogTimerId = StartTimer(kVideoExitDialogTimeout, __LINE__);
    }
    ReturnOSDLock(ctx, osd);
}

bool TV::HandleOSDVideoExit(PlayerContext *ctx, const QString& action)
{
    if (!DialogIsVisible(ctx, OSD_DLG_VIDEOEXIT))
        return false;

    bool hide        = true;
    bool delete_ok   = IsDeleteAllowed(ctx);
    bool bookmark_ok = IsBookmarkAllowed(ctx);

    ctx->LockDeletePlayer(__FILE__, __LINE__);
    bool near_end = ctx->m_player && ctx->m_player->IsNearEnd();
    ctx->UnlockDeletePlayer(__FILE__, __LINE__);

    if (action == "DELETEANDRERECORD" && delete_ok)
    {
        m_allowRerecord = true;
        m_requestDelete = true;
        SetExitPlayer(true, true);
    }
    else if (action == "JUSTDELETE" && delete_ok)
    {
        m_requestDelete = true;
        SetExitPlayer(true, true);
    }
    else if (action == "CONFIRMDELETE")
    {
        hide = false;
        ShowOSDPromptDeleteRecording(ctx, tr("Are you sure you want to delete:"),
                                     true);
    }
    else if (action == "SAVEPOSITIONANDEXIT" && bookmark_ok)
    {
        PrepareToExitPlayer(ctx, __LINE__, kBookmarkAlways);
        SetExitPlayer(true, true);
    }
    else if (action == "KEEPWATCHING" && !near_end)
    {
        DoTogglePause(ctx, true);
    }

    return hide;
}

void TV::HandleSaveLastPlayPosEvent(void)
{
    // Helper class to save the latest playback position (in a background thread
    // to avoid playback glitches).  The ctor makes a copy of the ProgramInfo
    // struct to avoid race conditions if playback ends and deletes objects
    // before or while the background thread runs.
    class PositionSaver : public QRunnable
    {
    public:
        PositionSaver(const ProgramInfo &pginfo, uint64_t frame) :
            m_pginfo(pginfo), m_frame(frame) {}
        void run(void) override // QRunnable
        {
            LOG(VB_PLAYBACK, LOG_DEBUG,
                QString("PositionSaver frame=%1").arg(m_frame));
            frm_dir_map_t lastPlayPosMap;
            lastPlayPosMap[m_frame] = MARK_UTIL_LASTPLAYPOS;
            m_pginfo.ClearMarkupMap(MARK_UTIL_LASTPLAYPOS);
            m_pginfo.SaveMarkupMap(lastPlayPosMap, MARK_UTIL_LASTPLAYPOS);
        }
    private:
        const ProgramInfo m_pginfo;
        const uint64_t m_frame;
    };

    PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
    for (uint i = 0; mctx && i < m_player.size(); ++i)
    {
        PlayerContext *ctx = GetPlayer(mctx, i);
        ctx->LockDeletePlayer(__FILE__, __LINE__);
        bool playing = ctx->m_player && !ctx->m_player->IsPaused();
        if (playing) // Don't bother saving lastplaypos while paused
        {
            uint64_t framesPlayed = ctx->m_player->GetFramesPlayed();
            MThreadPool::globalInstance()->
                start(new PositionSaver(*ctx->m_playingInfo, framesPlayed),
                      "PositionSaver");
        }
        ctx->UnlockDeletePlayer(__FILE__, __LINE__);
    }
    ReturnPlayerLock(mctx);

    QMutexLocker locker(&m_timerIdLock);
    KillTimer(m_saveLastPlayPosTimerId);
    m_saveLastPlayPosTimerId = StartTimer(kSaveLastPlayPosTimeout, __LINE__);
}

void TV::SetLastProgram(const ProgramInfo *rcinfo)
{
    QMutexLocker locker(&m_lastProgramLock);

    delete m_lastProgram;

    if (rcinfo)
        m_lastProgram = new ProgramInfo(*rcinfo);
    else
        m_lastProgram = nullptr;
}

ProgramInfo *TV::GetLastProgram(void) const
{
    QMutexLocker locker(&m_lastProgramLock);
    if (m_lastProgram)
        return new ProgramInfo(*m_lastProgram);
    return nullptr;
}

QString TV::GetRecordingGroup(int player_idx) const
{
    QString ret;

    const PlayerContext *ctx = GetPlayerReadLock(player_idx, __FILE__, __LINE__);
    if (ctx)
    {
        if (StateIsPlaying(GetState(ctx)))
        {
            ctx->LockPlayingInfo(__FILE__, __LINE__);
            if (ctx->m_playingInfo)
                ret = ctx->m_playingInfo->GetRecordingGroup();
            ctx->UnlockPlayingInfo(__FILE__, __LINE__);
        }
    }
    ReturnPlayerLock(ctx);
    return ret;
}

bool TV::IsSameProgram(int player_idx, const ProgramInfo *rcinfo) const
{
    if (!rcinfo)
        return false;

    bool ret = false;
    const PlayerContext *ctx = GetPlayerReadLock(player_idx, __FILE__, __LINE__);
    if (ctx)
        ret = ctx->IsSameProgram(*rcinfo);
    ReturnPlayerLock(ctx);

    return ret;
}

void TV::RestoreScreenSaver(const PlayerContext *ctx)
{
    if (ctx == GetPlayer(ctx, 0))
        MythUIHelper::RestoreScreensaver();
}

bool TV::ContextIsPaused(PlayerContext *ctx, const char *file, int location)
{
    if (!ctx)
        return false;
    bool paused = false;
    ctx->LockDeletePlayer(file, location);
    if (ctx->m_player)
        paused = ctx->m_player->IsPaused();
    ctx->UnlockDeletePlayer(file, location);
    return paused;
}

OSD *TV::GetOSDL(const char *file, int location)
{
    PlayerContext *actx = GetPlayerReadLock(-1, file, location);

    OSD *osd = GetOSDL(actx, file, location);
    if (!osd)
        ReturnPlayerLock(actx);

    return osd;
}

OSD *TV::GetOSDL(const PlayerContext *ctx, const char *file, int location)
{
    if (!ctx)
        return nullptr;

    const PlayerContext *mctx = GetPlayer(ctx, 0);

    mctx->LockDeletePlayer(file, location);
    if (mctx->m_player && ctx->IsPIP())
    {
        mctx->LockOSD();
        OSD *osd = mctx->m_player->GetOSD();
        if (!osd)
        {
            mctx->UnlockOSD();
            mctx->UnlockDeletePlayer(file, location);
        }
        else
            m_osdLctx[osd] = mctx;
        return osd;
    }
    mctx->UnlockDeletePlayer(file, location);

    ctx->LockDeletePlayer(file, location);
    if (ctx->m_player && !ctx->IsPIP())
    {
        ctx->LockOSD();
        OSD *osd = ctx->m_player->GetOSD();
        if (!osd)
        {
            ctx->UnlockOSD();
            ctx->UnlockDeletePlayer(file, location);
        }
        else
            m_osdLctx[osd] = ctx;
        return osd;
    }
    ctx->UnlockDeletePlayer(file, location);

    return nullptr;
}

void TV::ReturnOSDLock(const PlayerContext *ctx, OSD *&osd)
{
    if (!ctx || !osd)
        return;

    m_osdLctx[osd]->UnlockOSD();
    m_osdLctx[osd]->UnlockDeletePlayer(__FILE__, __LINE__);

    osd = nullptr;
}

PlayerContext *TV::GetPlayerWriteLock(int which, const char *file, int location)
{
    m_playerLock.lockForWrite();

    if ((which >= (int)m_player.size()))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("(%1,%2,%3) returning nullptr size(%4)")
                .arg(which).arg(file).arg(location).arg(m_player.size()));
        return nullptr;
    }

    return (which < 0) ? m_player[m_playerActive] : m_player[which];
}

PlayerContext *TV::GetPlayerReadLock(int which, const char *file, int location)
{
    m_playerLock.lockForRead();

    if ((which >= (int)m_player.size()))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("(%1,%2,%3) returning nullptr size(%4)")
                .arg(which).arg(file).arg(location).arg(m_player.size()));
        return nullptr;
    }

    return (which < 0) ? m_player[m_playerActive] : m_player[which];
}

const PlayerContext *TV::GetPlayerReadLock(
    int which, const char *file, int location) const
{
    m_playerLock.lockForRead();

    if ((which >= (int)m_player.size()))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("(%1,%2,%3) returning nullptr size(%4)")
                .arg(which).arg(file).arg(location).arg(m_player.size()));
        return nullptr;
    }

    return (which < 0) ? m_player[m_playerActive] : m_player[which];
}

PlayerContext *TV::GetPlayerHaveLock(
    PlayerContext *locked_context,
    int which, const char *file, int location)
{
    if (!locked_context || (which >= (int)m_player.size()))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("(0x%1,%2,%3,%4) returning nullptr size(%5)")
                .arg((uint64_t)locked_context, 0, 16)
                .arg(which).arg(file).arg(location).arg(m_player.size()));
        return nullptr;
    }

    return (which < 0) ? m_player[m_playerActive] : m_player[which];
}

const PlayerContext *TV::GetPlayerHaveLock(
    const PlayerContext *locked_context,
    int which, const char *file, int location) const
{
    if (!locked_context || (which >= (int)m_player.size()))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("(0x%1,%2,%3,%4) returning nullptr size(%5)")
                .arg((uint64_t)locked_context, 0, 16)
                .arg(which).arg(file).arg(location).arg(m_player.size()));
        return nullptr;
    }

    return (which < 0) ? m_player[m_playerActive] : m_player[which];
}

void TV::ReturnPlayerLock(PlayerContext *&ctx)
{
    m_playerLock.unlock();
    ctx = nullptr;
}

void TV::ReturnPlayerLock(const PlayerContext *&ctx) const
{
    m_playerLock.unlock();
    ctx = nullptr;
}

QString TV::GetLiveTVIndex(const PlayerContext *ctx)
{
#ifdef DEBUG_LIVETV_TRANSITION
    return (ctx->m_tvchain ?
            QString(" (%1/%2)")
                .arg(ctx->m_tvchain->GetCurPos())
                .arg(ctx->m_tvchain->TotalSize()) : QString());
#else
    (void)ctx;
    return QString();
#endif
}

void TV::onApplicationStateChange(Qt::ApplicationState state)
{
    switch (state)
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

/* vim: set expandtab tabstop=4 shiftwidth=4: */
