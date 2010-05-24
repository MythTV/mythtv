#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <cmath>
#include <unistd.h>
#include <pthread.h>

#include <algorithm>
using namespace std;

#include <QCoreApplication>
#include <QRegExp>
#include <QFile>
#include <QTimer>
#include <QDir>
#include <QKeyEvent>
#include <QEvent>

#include "mythdb.h"
#include "tv_play.h"
#include "tv_rec.h"
#include "osd.h"
#include "osdsurface.h"
#include "osdtypes.h"
#include "osdlistbtntype.h"
#include "mythcorecontext.h"
#include "dialogbox.h"
#include "remoteencoder.h"
#include "remoteutil.h"
#include "tvremoteutil.h"
#include "NuppelVideoPlayer.h"
#include "DetectLetterbox.h"
#include "programinfo.h"
#include "udpnotify.h"
#include "vsync.h"
#include "lcddevice.h"
#include "jobqueue.h"
#include "audiooutput.h"
#include "DisplayRes.h"
#include "signalmonitorvalue.h"
#include "scheduledrecording.h"
#include "previewgenerator.h"
#include "mythconfig.h"
#include "livetvchain.h"
#include "playgroup.h"
#include "DVDRingBuffer.h"
#include "datadirect.h"
#include "sourceutil.h"
#include "cardutil.h"
#include "channelutil.h"
#include "util-osx-cocoa.h"
#include "compat.h"
#include "mythverbose.h"
#include "mythuihelper.h"
#include "mythdialogbox.h"
#include "mythmainwindow.h"
#include "mythscreenstack.h"
#include "mythscreentype.h"
#include "tvosdmenuentry.h"
#include "tv_play_win.h"
#include "recordinginfo.h"
#include "mythsystemevent.h"
#include "videometadatautil.h"

#if ! HAVE_ROUND
#define round(x) ((int) ((x) + 0.5))
#endif

#define DEBUG_CHANNEL_PREFIX 0 /**< set to 1 to debug channel prefixing */
#define DEBUG_ACTIONS        0 /**< set to 1 to debug actions           */

#define LOC      QString("TV: ")
#define LOC_WARN QString("TV Warning: ")
#define LOC_ERR  QString("TV Error: ")

#define GetPlayer(X,Y) GetPlayerHaveLock(X, Y, __FILE__ , __LINE__)
#define GetOSDLock(X) GetOSDL(X, __FILE__, __LINE__)

const int  TV::kInitFFRWSpeed                = 0;
const uint TV::kInputKeysMax                 = 6;
const uint TV::kNextSource                   = 1;
const uint TV::kPreviousSource               = 2;
const uint TV::kMaxPIPCount                  = 4;
const uint TV::kMaxPBPCount                  = 2;


const uint TV::kInputModeTimeout             = 5000;
const uint TV::kMuteTimeout                  = 800;
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

/**
 * \brief stores last program info. maintains info so long as
 * mythfrontend is active
 */
QStringList TV::lastProgramStringList = QStringList();

/**
 * \brief function pointer for RunPlaybackBox in playbackbox.cpp
 */
EMBEDRETURNVOID TV::RunPlaybackBoxPtr = NULL;

/**
 * \brief function pointer for RunViewScheduled in viewscheduled.cpp
 */
EMBEDRETURNVOID TV::RunViewScheduledPtr = NULL;

/**
 * \brief function pointer for RunScheduleEditor in scheduleeditor.cpp
 */
EMBEDRETURNVOIDSCHEDIT TV::RunScheduleEditorPtr = NULL;

/**
 * \brief function pointer for RunProgramGuide in guidegrid.cpp
 */
EMBEDRETURNVOIDEPG TV::RunProgramGuidePtr = NULL;

/**
 * \brief function pointer for RunProgramFinder in progfind.cpp
 */
EMBEDRETURNVOIDFINDER TV::RunProgramFinderPtr = NULL;


/**
 * \brief If any cards are configured, return the number.
 */
static int ConfiguredTunerCards(void)
{
    int count = 0;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT COUNT(cardid) FROM capturecard;");
    if (query.exec() && query.isActive() && query.size() && query.next())
        count = query.value(0).toInt();

    VERBOSE(VB_RECORD, "ConfiguredTunerCards() = " + QString::number(count));

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
                usleep(25 * 1000);
            }
        }
    }
}

/**
 * \brief returns true if the recording completed when exiting.
 */
bool TV::StartTV(ProgramInfo *tvrec, uint flags)
{
    VERBOSE(VB_PLAYBACK, LOC + "StartTV() -- begin");
    bool startInGuide = flags & kStartTVInGuide;
    bool inPlaylist = flags & kStartTVInPlayList;
    bool initByNetworkCommand = flags & kStartTVByNetworkCommand;
    TV *tv = new TV();
    bool quitAll = false;
    bool showDialogs = true;
    bool playCompleted = false;
    ProgramInfo *curProgram = NULL;
    bool startSysEventSent = false;


    if (tvrec)
    {
        curProgram = new ProgramInfo(*tvrec);

        curProgram->SetIgnoreBookmark(flags & kStartTVIgnoreBookmark);
    }

    // Initialize TV
    if (!tv->Init())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed initializing TV");
        delete tv;
        delete curProgram;
        return false;
    }

    if (!lastProgramStringList.empty())
    {
        ProgramInfo pginfo(lastProgramStringList);
        if (pginfo.HasPathname() || pginfo.GetChanID())
            tv->SetLastProgram(&pginfo);
    }

    sendPlaybackStart();

    if (curProgram)
    {
        startSysEventSent = true;
        SendMythSystemPlayEvent("PLAY_STARTED", curProgram);
    }

    QString nvpError = QString::null;
    while (!quitAll)
    {
        if (curProgram)
        {
            VERBOSE(VB_PLAYBACK, LOC + "tv->Playback() -- begin");
            if (!tv->Playback(*curProgram))
            {
                quitAll = true;
            }
            else if (!startSysEventSent)
            {
                startSysEventSent = true;
                SendMythSystemPlayEvent("PLAY_STARTED", curProgram);
            }

            VERBOSE(VB_PLAYBACK, LOC + "tv->Playback() -- end");
        }
        else if (RemoteGetFreeRecorderCount())
        {
            VERBOSE(VB_PLAYBACK, LOC + "tv->LiveTV() -- begin");
            if (!tv->LiveTV(showDialogs, startInGuide))
            {
                tv->SetExitPlayer(true, true);
                quitAll = true;
            }
            else if (!startSysEventSent)
            {
                startSysEventSent = true;
                SendMythSystemEvent("LIVETV_STARTED");
            }

            VERBOSE(VB_PLAYBACK, LOC + "tv->LiveTV() -- end");
        }
        else if (!ConfiguredTunerCards())
        {
            // (cannot watch Live TV without a card :-)
            PlayerContext *mctx = tv->GetPlayerReadLock(0, __FILE__, __LINE__);
            tv->ShowNoRecorderDialog(mctx, kNoTuners);
            tv->ReturnPlayerLock(mctx);
            quitAll = true;
            continue;
        }
        else
        {
            vector<ProgramInfo *> *reclist;
            reclist = RemoteGetCurrentlyRecordingList();
            if (reclist->empty())
            {
                VERBOSE(VB_PLAYBACK, LOC_ERR +
                        "Failed to get recording show list");
                PlayerContext *mctx = tv->GetPlayerReadLock(0, __FILE__, __LINE__);
                tv->ShowNoRecorderDialog(mctx, kNoCurrRec);
                tv->ReturnPlayerLock(mctx);
                quitAll = true;
                delete reclist;
                continue;
            }

            ProgramInfo *p = NULL;
            QStringList recTitles;
            QString buttonTitle;
            vector<ProgramInfo *>::iterator it = reclist->begin();
            recTitles.append(tr("Exit"));
            while (it != reclist->end())
            {
                p = *it;

                if (p->GetRecordingStatus() == rsRecorded &&
                    p->GetRecordingGroup()  == "LiveTV")
                {
                    buttonTitle = tr("LiveTV, chan %1: %2")
                        .arg(p->GetChanNum()).arg(p->GetTitle());
                }
                else
                {
                    buttonTitle = tr("Chan %1: %2")
                        .arg(p->GetChanNum()).arg(p->GetTitle());
                }

                recTitles.append(buttonTitle);
                it++;
            }
            DialogCode ret = MythPopupBox::ShowButtonPopup(
                GetMythMainWindow(), "",
                tr("All Tuners are Busy.\nSelect a Current Recording"),
                recTitles, kDialogCodeButton1);

            int idx = MythDialog::CalcItemIndex(ret) - 1;
            if ((0 <= idx) && (idx < (int)reclist->size()))
            {
                p = reclist->at(idx);
                if (curProgram)
                    delete curProgram;
                curProgram = new ProgramInfo(*p);
            }
            else
            {
                quitAll = true;
            }

            while (!reclist->empty())
            {
                delete reclist->back();
                reclist->pop_back();
            }
            delete reclist;
            continue;
        }

        tv->setInPlayList(inPlaylist);
        tv->setUnderNetworkControl(initByNetworkCommand);

        // Process Events
        VERBOSE(VB_PLAYBACK, LOC + "StartTV -- process events begin");
        MythTimer st; st.start();
        bool is_started = false;
        while (true)
        {
            qApp->processEvents();

            QMutexLocker locker(&tv->stateChangeCondLock);
            TVState state   = tv->GetState(0);
            bool is_err     = kState_Error == state;
            bool is_none    = kState_None  == state;
            is_started = is_started ||
                (st.elapsed() > (int) TV::kEndOfPlaybackFirstCheckTimer) ||
                (!is_none && !is_err && kState_ChangingState != state);
            if (is_err || (is_none && is_started))
                break;

            // timeout needs to be low enough to process keyboard input
            unsigned long timeout = 20; // milliseconds
            tv->stateChangeCond.wait(&tv->stateChangeCondLock, timeout);
        }
        VERBOSE(VB_PLAYBACK, LOC + "StartTV -- process events end");

        if (tv->getJumpToProgram())
        {
            ProgramInfo *nextProgram = tv->GetLastProgram();

            tv->SetLastProgram(curProgram);
            if (curProgram)
                delete curProgram;

            curProgram = nextProgram;

            SendMythSystemPlayEvent("PLAY_CHANGED", curProgram);
            continue;
        }

        const PlayerContext *mctx =
            tv->GetPlayerReadLock(0, __FILE__, __LINE__);
        quitAll = tv->wantsToQuit || (mctx && mctx->errored);
        if (mctx)
        {
            mctx->LockDeleteNVP(__FILE__, __LINE__);
            if (mctx->nvp && mctx->nvp->IsErrored())
                nvpError = mctx->nvp->GetError();
            mctx->UnlockDeleteNVP(__FILE__, __LINE__);
        }
        tv->ReturnPlayerLock(mctx);
    }

    VERBOSE(VB_PLAYBACK, LOC + "StartTV -- process events 2 begin");
    qApp->processEvents();
    VERBOSE(VB_PLAYBACK, LOC + "StartTV -- process events 2 end");

    // check if the show has reached the end.
    if (tvrec && tv->getEndOfRecording())
        playCompleted = true;

    bool allowrerecord = tv->getAllowRerecord();
    bool deleterecording = tv->requestDelete;

    tv->SaveChannelGroup();

    delete tv;

    if (curProgram)
    {
        SendMythSystemPlayEvent("PLAY_STOPPED", curProgram);

        if (deleterecording)
        {
            QStringList list;
            list.push_back(QString::number(curProgram->GetChanID()));
            list.push_back(curProgram->GetRecordingStartTime(ISODate));
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
    else
        SendMythSystemEvent("PLAY_STOPPED");

    if (!nvpError.isEmpty())
    {
        MythScreenStack *ss = GetMythMainWindow()->GetStack("popup stack");
        MythConfirmationDialog *dlg = new MythConfirmationDialog(
            ss, nvpError, false);
        if (!dlg->Create())
            delete dlg;
        else
            ss->AddScreen(dlg);
    }

    sendPlaybackEnd();

    VERBOSE(VB_PLAYBACK, LOC + "StartTV -- end");

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
    REG_KEY("TV Frontend", "PAGEUP", QT_TRANSLATE_NOOP("MythControls",
            "Page Up"), "3");
    REG_KEY("TV Frontend", "PAGEDOWN", QT_TRANSLATE_NOOP("MythControls",
            "Page Down"), "9");
    REG_KEY("TV Frontend", "PLAYBACK", QT_TRANSLATE_NOOP("MythControls",
            "Play Program"), "P");
    REG_KEY("TV Frontend", "TOGGLERECORD", QT_TRANSLATE_NOOP("MythControls",
            "Toggle recording status of current program"), "R");
    REG_KEY("TV Frontend", "DAYLEFT", QT_TRANSLATE_NOOP("MythControls",
            "Page the program guide back one day"), "Home,7");
    REG_KEY("TV Frontend", "DAYRIGHT", QT_TRANSLATE_NOOP("MythControls",
            "Page the program guide forward one day"), "End,1");
    REG_KEY("TV Frontend", "PAGELEFT", QT_TRANSLATE_NOOP("MythControls",
            "Page the program guide left"), ",,<");
    REG_KEY("TV Frontend", "PAGERIGHT", QT_TRANSLATE_NOOP("MythControls",
            "Page the program guide right"), ">,.");
    REG_KEY("TV Frontend", "TOGGLEFAV", QT_TRANSLATE_NOOP("MythControls",
            "Toggle the current channel as a favorite"), "?");
    REG_KEY("TV Frontend", "TOGGLEEPGORDER", QT_TRANSLATE_NOOP("MythControls",
            "Reverse the channel order in the program guide"), "0");
    REG_KEY("TV Frontend", "GUIDE", QT_TRANSLATE_NOOP("MythControls",
            "Show the Program Guide"), "S");
    REG_KEY("TV Frontend", "FINDER", QT_TRANSLATE_NOOP("MythControls",
            "Show the Program Finder"), "#");
    REG_KEY("TV Frontend", "NEXTFAV", QT_TRANSLATE_NOOP("MythControls",
            "Cycle through channel groups and all channels in the "
            "program guide."), "/");
    REG_KEY("TV Frontend", "CHANUPDATE", QT_TRANSLATE_NOOP("MythControls",
            "Switch channels without exiting guide in Live TV mode."), "X");
    REG_KEY("TV Frontend", "VOLUMEDOWN", QT_TRANSLATE_NOOP("MythControls",
            "Volume down"), "[,{,F10,Volume Down");
    REG_KEY("TV Frontend", "VOLUMEUP", QT_TRANSLATE_NOOP("MythControls",
            "Volume up"), "],},F11,Volume Up");
    REG_KEY("TV Frontend", "MUTE", QT_TRANSLATE_NOOP("MythControls", "Mute"),
            "|,\\,F9,Volume Mute");
    REG_KEY("TV Frontend", "RANKINC", QT_TRANSLATE_NOOP("MythControls",
            "Increase program or channel rank"), "Right");
    REG_KEY("TV Frontend", "RANKDEC", QT_TRANSLATE_NOOP("MythControls",
            "Decrease program or channel rank"), "Left");
    REG_KEY("TV Frontend", "UPCOMING", QT_TRANSLATE_NOOP("MythControls",
            "List upcoming episodes"), "O");
    REG_KEY("TV Frontend", "DETAILS", QT_TRANSLATE_NOOP("MythControls",
            "Show program details"), "U");
    REG_KEY("TV Frontend", "VIEWCARD", QT_TRANSLATE_NOOP("MythControls",
            "Switch Capture Card view"), "Y");
    REG_KEY("TV Frontend", "VIEWINPUT", QT_TRANSLATE_NOOP("MythControls",
            "Switch Capture Card view"), "C");
    REG_KEY("TV Frontend", "CUSTOMEDIT", QT_TRANSLATE_NOOP("MythControls",
            "Edit Custom Record Rule"), "");
    REG_KEY("TV Frontend", "CHANGERECGROUP", QT_TRANSLATE_NOOP("MythControls",
            "Change Recording Group"), "");
    REG_KEY("TV Frontend", "CHANGEGROUPVIEW", QT_TRANSLATE_NOOP("MythControls",
            "Change Group View"), "");

    REG_KEY("TV Playback", "CLEAROSD", QT_TRANSLATE_NOOP("MythControls",
            "Clear OSD"), "Backspace");
    REG_KEY("TV Playback", "PAUSE", QT_TRANSLATE_NOOP("MythControls",
            "Pause"), "P");
    REG_KEY("TV Playback", "SEEKFFWD", QT_TRANSLATE_NOOP("MythControls",
            "Fast Forward"), "Right");
    REG_KEY("TV Playback", "SEEKRWND", QT_TRANSLATE_NOOP("MythControls",
            "Rewind"), "Left");
    REG_KEY("TV Playback", "ARBSEEK", QT_TRANSLATE_NOOP("MythControls",
            "Arbitrary Seek"), "*");
    REG_KEY("TV Playback", "CHANNELUP", QT_TRANSLATE_NOOP("MythControls",
            "Channel up"), "Up");
    REG_KEY("TV Playback", "CHANNELDOWN", QT_TRANSLATE_NOOP("MythControls",
            "Channel down"), "Down");
    REG_KEY("TV Playback", "NEXTFAV", QT_TRANSLATE_NOOP("MythControls",
            "Switch to the next favorite channel"), "/");
    REG_KEY("TV Playback", "PREVCHAN", QT_TRANSLATE_NOOP("MythControls",
            "Switch to the previous channel"), "H");
    REG_KEY("TV Playback", "JUMPFFWD", QT_TRANSLATE_NOOP("MythControls",
            "Jump ahead"), "PgDown");
    REG_KEY("TV Playback", "JUMPRWND", QT_TRANSLATE_NOOP("MythControls",
            "Jump back"), "PgUp");
    REG_KEY("TV Playback", "JUMPBKMRK", QT_TRANSLATE_NOOP("MythControls",
            "Jump to bookmark"), "K");
    REG_KEY("TV Playback", "FFWDSTICKY", QT_TRANSLATE_NOOP("MythControls",
            "Fast Forward (Sticky) or Forward one frame while paused"), ">,.");
    REG_KEY("TV Playback", "RWNDSTICKY", QT_TRANSLATE_NOOP("MythControls",
            "Rewind (Sticky) or Rewind one frame while paused"), ",,<");
    REG_KEY("TV Playback", "NEXTSOURCE", QT_TRANSLATE_NOOP("MythControls",
            "Next Video Source"), "Y");
    REG_KEY("TV Playback", "PREVSOURCE", QT_TRANSLATE_NOOP("MythControls",
            "Previous Video Source"), "Ctrl+Y");
    REG_KEY("TV Playback", "NEXTINPUT", QT_TRANSLATE_NOOP("MythControls",
            "Next Input"), "C");
    REG_KEY("TV Playback", "NEXTCARD", QT_TRANSLATE_NOOP("MythControls",
            "Next Card"), "");
    REG_KEY("TV Playback", "SKIPCOMMERCIAL", QT_TRANSLATE_NOOP("MythControls",
            "Skip Commercial"), "Z,End");
    REG_KEY("TV Playback", "SKIPCOMMBACK", QT_TRANSLATE_NOOP("MythControls",
            "Skip Commercial (Reverse)"), "Q,Home");
    REG_KEY("TV Playback", "JUMPSTART", QT_TRANSLATE_NOOP("MythControls",
            "Jump to the start of the recording."), "Ctrl+B");
    REG_KEY("TV Playback", "TOGGLEBROWSE", QT_TRANSLATE_NOOP("MythControls",
            "Toggle channel browse mode"), "O");
    REG_KEY("TV Playback", "TOGGLERECORD", QT_TRANSLATE_NOOP("MythControls",
            "Toggle recording status of current program"), "R");
    REG_KEY("TV Playback", "TOGGLEFAV", QT_TRANSLATE_NOOP("MythControls",
            "Toggle the current channel as a favorite"), "?");
    REG_KEY("TV Playback", "VOLUMEDOWN", QT_TRANSLATE_NOOP("MythControls",
            "Volume down"), "[,{,F10,Volume Down");
    REG_KEY("TV Playback", "VOLUMEUP", QT_TRANSLATE_NOOP("MythControls",
            "Volume up"), "],},F11,Volume Up");
    REG_KEY("TV Playback", "MUTE", QT_TRANSLATE_NOOP("MythControls", "Mute"),
            "|,\\,F9,Volume Mute");
    REG_KEY("TV Playback", "TOGGLEUPMIX", QT_TRANSLATE_NOOP("MythControls",
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
    REG_KEY("TV Playback", "TOGGLEASPECT", QT_TRANSLATE_NOOP("MythControls",
            "Toggle the video aspect ratio"), "Ctrl+W");
    REG_KEY("TV Playback", "TOGGLEFILL", QT_TRANSLATE_NOOP("MythControls",
            "Next Preconfigured Zoom mode"), "W");
    REG_KEY("TV Playback", "TOGGLECC", QT_TRANSLATE_NOOP("MythControls",
            "Toggle any captions"), "T");
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

    REG_KEY("TV Playback", "SELECTAUDIO_0", QT_TRANSLATE_NOOP("MythControls",
            "Play audio track 1"), "");
    REG_KEY("TV Playback", "SELECTAUDIO_1", QT_TRANSLATE_NOOP("MythControls",
            "Play audio track 2"), "");
    REG_KEY("TV Playback", "SELECTSUBTITLE_0",QT_TRANSLATE_NOOP("MythControls",
            "Display subtitle 1"), "");
    REG_KEY("TV Playback", "SELECTSUBTITLE_1",QT_TRANSLATE_NOOP("MythControls",
            "Display subtitle 2"), "");
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

    REG_KEY("TV Playback", "NEXTAUDIO", QT_TRANSLATE_NOOP("MythControls",
            "Next audio track"), "+");
    REG_KEY("TV Playback", "PREVAUDIO", QT_TRANSLATE_NOOP("MythControls",
            "Previous audio track"), "-");
    REG_KEY("TV Playback", "NEXTSUBTITLE", QT_TRANSLATE_NOOP("MythControls",
            "Next subtitle track"), "");
    REG_KEY("TV Playback", "PREVSUBTITLE", QT_TRANSLATE_NOOP("MythControls",
            "Previous subtitle track"), "");
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
    REG_KEY("TV Playback", "TOGGLEAUDIOSYNC", QT_TRANSLATE_NOOP("MythControls",
            "Turn on audio sync adjustment controls"), "");
    REG_KEY("TV Playback", "TOGGLEPICCONTROLS",
            QT_TRANSLATE_NOOP("MythControls", "Playback picture adjustments"),
             "F");
    REG_KEY("TV Playback", "TOGGLECHANCONTROLS",
            QT_TRANSLATE_NOOP("MythControls", "Recording picture adjustments "
            "for this channel"), "Ctrl+G");
    REG_KEY("TV Playback", "TOGGLERECCONTROLS",
            QT_TRANSLATE_NOOP("MythControls", "Recording picture adjustments "
            "for this recorder"), "G");
    REG_KEY("TV Playback", "CYCLECOMMSKIPMODE",
            QT_TRANSLATE_NOOP("MythControls", "Cycle Commercial Skip mode"),
            "");
    REG_KEY("TV Playback", "GUIDE", QT_TRANSLATE_NOOP("MythControls",
            "Show the Program Guide"), "S");
    REG_KEY("TV Playback", "FINDER", QT_TRANSLATE_NOOP("MythControls",
            "Show the Program Finder"), "#");
    REG_KEY("TV Playback", "TOGGLESLEEP", QT_TRANSLATE_NOOP("MythControls",
            "Toggle the Sleep Timer"), "F8");
    REG_KEY("TV Playback", "PLAY", QT_TRANSLATE_NOOP("MythControls", "Play"),
            "Ctrl+P");
    REG_KEY("TV Playback", "JUMPPREV", QT_TRANSLATE_NOOP("MythControls",
            "Jump to previously played recording"), "");
    REG_KEY("TV Playback", "JUMPREC", QT_TRANSLATE_NOOP("MythControls",
            "Display menu of recorded programs to jump to"), "");
    REG_KEY("TV Playback", "VIEWSCHEDULED", QT_TRANSLATE_NOOP("MythControls",
            "Display scheduled recording list"), "");
    REG_KEY("TV Playback", "SIGNALMON", QT_TRANSLATE_NOOP("MythControls",
            "Monitor Signal Quality"), "Alt+F7");
    REG_KEY("TV Playback", "JUMPTODVDROOTMENU",
            QT_TRANSLATE_NOOP("MythControls", "Jump to the DVD Root Menu"), "");
    REG_KEY("TV Playback", "EXITSHOWNOPROMPTS",
            QT_TRANSLATE_NOOP("MythControls", "Exit Show without any prompts"),
            "");
    REG_KEY("TV Playback", "SCREENSHOT",
            QT_TRANSLATE_NOOP("MythControls", "Save screenshot of current "
            "video frame"), "");

    /* Interactive Television keys */
    REG_KEY("TV Playback", "MENURED",    QT_TRANSLATE_NOOP("MythControls",
            "Menu Red"),    "F2");
    REG_KEY("TV Playback", "MENUGREEN",  QT_TRANSLATE_NOOP("MythControls",
            "Menu Green"),  "F3");
    REG_KEY("TV Playback", "MENUYELLOW", QT_TRANSLATE_NOOP("MythControls",
            "Menu Yellow"), "F4");
    REG_KEY("TV Playback", "MENUBLUE",   QT_TRANSLATE_NOOP("MythControls",
            "Menu Blue"),   "F5");
    REG_KEY("TV Playback", "TEXTEXIT",   QT_TRANSLATE_NOOP("MythControls",
            "Menu Exit"),   "F6");
    REG_KEY("TV Playback", "MENUTEXT",   QT_TRANSLATE_NOOP("MythControls",
            "Menu Text"),   "F7");
    REG_KEY("TV Playback", "MENUEPG",    QT_TRANSLATE_NOOP("MythControls",
            "Menu EPG"),    "F12");

    /* Editing keys */
    REG_KEY("TV Editing", "CLEARMAP",    QT_TRANSLATE_NOOP("MythControls",
            "Clear editing cut points"), "C,Q,Home");
    REG_KEY("TV Editing", "INVERTMAP",   QT_TRANSLATE_NOOP("MythControls",
            "Invert Begin/End cut points"),"I");
    REG_KEY("TV Editing", "LOADCOMMSKIP",QT_TRANSLATE_NOOP("MythControls",
            "Load cut list from commercial skips"), "Z,End");
    REG_KEY("TV Editing", "NEXTCUT",     QT_TRANSLATE_NOOP("MythControls",
            "Jump to the next cut point"), "PgDown");
    REG_KEY("TV Editing", "PREVCUT",     QT_TRANSLATE_NOOP("MythControls",
            "Jump to the previous cut point"), "PgUp");
    REG_KEY("TV Editing", "BIGJUMPREW",  QT_TRANSLATE_NOOP("MythControls",
            "Jump back 10x the normal amount"), ",,<");
    REG_KEY("TV Editing", "BIGJUMPFWD",  QT_TRANSLATE_NOOP("MythControls",
            "Jump forward 10x the normal amount"), ">,.");

    /* Teletext keys */
    REG_KEY("Teletext Menu", "NEXTPAGE",    QT_TRANSLATE_NOOP("MythControls",
            "Next Page"),             "Down");
    REG_KEY("Teletext Menu", "PREVPAGE",    QT_TRANSLATE_NOOP("MythControls",
            "Previous Page"),         "Up");
    REG_KEY("Teletext Menu", "NEXTSUBPAGE", QT_TRANSLATE_NOOP("MythControls",
            "Next Subpage"),          "Right");
    REG_KEY("Teletext Menu", "PREVSUBPAGE", QT_TRANSLATE_NOOP("MythControls",
            "Previous Subpage"),      "Left");
    REG_KEY("Teletext Menu", "TOGGLETT",    QT_TRANSLATE_NOOP("MythControls",
            "Toggle Teletext"),       "T");
    REG_KEY("Teletext Menu", "MENURED",     QT_TRANSLATE_NOOP("MythControls",
            "Menu Red"),              "F2");
    REG_KEY("Teletext Menu", "MENUGREEN",   QT_TRANSLATE_NOOP("MythControls",
            "Menu Green"),            "F3");
    REG_KEY("Teletext Menu", "MENUYELLOW",  QT_TRANSLATE_NOOP("MythControls",
            "Menu Yellow"),           "F4");
    REG_KEY("Teletext Menu", "MENUBLUE",    QT_TRANSLATE_NOOP("MythControls",
            "Menu Blue"),             "F5");
    REG_KEY("Teletext Menu", "MENUWHITE",   QT_TRANSLATE_NOOP("MythControls",
            "Menu White"),            "F6");
    REG_KEY("Teletext Menu", "TOGGLEBACKGROUND",
            QT_TRANSLATE_NOOP("MythControls", "Toggle Background"), "F7");
    REG_KEY("Teletext Menu", "REVEAL",      QT_TRANSLATE_NOOP("MythControls",
            "Reveal hidden Text"),    "F8");

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

  Playback: Ctrl-B,Ctrl-G,Ctrl-Y,Ctrl-U
*/
}

/** \fn TV::TV(void)
 *  \brief Performs instance initialiation not requiring access to database.
 *  \sa Init(void)
 */
TV::TV(void)
    : // Configuration variables from database
      baseFilters(""),
      db_channel_format("<num> <sign>"),
      db_time_format("h:mm AP"), db_short_date_format("M/d"),
      db_idle_timeout(0),           db_udpnotify_port(0),
      db_browse_max_forward(14400),
      db_playback_exit_prompt(0),   db_autoexpire_default(0),
      db_auto_set_watched(false),   db_end_of_rec_exit_prompt(false),
      db_jump_prefer_osd(true),     db_use_gui_size_for_tv(false),
      db_start_in_guide(false),     db_toggle_bookmark(false),
      db_run_jobs_on_remote(false), db_continue_embedded(false),
      db_use_fixed_size(true),      db_browse_always(false),
      db_browse_all_tuners(false),

      smartChannelChange(false),
      MuteIndividualChannels(false), arrowAccel(false),
      osd_general_timeout(2), osd_prog_info_timeout(3),
      tryUnflaggedSkip(false),
      smartForward(false),
      ff_rew_repos(1.0f), ff_rew_reverse(false),
      jumped_back(false),
      vbimode(VBIMode::None),
      // State variables
      switchToInputId(0),
      wantsToQuit(true),
      stretchAdjustment(false),
      audiosyncAdjustment(false), audiosyncBaseline(LONG_LONG_MIN),
      editmode(false),     zoomMode(false),
      sigMonMode(false),
      endOfRecording(false),
      requestDelete(false),  allowRerecord(false),
      doSmartForward(false),
      queuedTranscode(false),
      adjustingPicture(kAdjustingPicture_None),
      adjustingPictureAttribute(kPictureAttribute_None),
      askAllowType(kAskAllowCancel), askAllowLock(QMutex::Recursive),
      // Channel Editing
      chanEditMapLock(QMutex::Recursive),
      ddMapSourceId(0), ddMapLoaderRunning(false),
      // Sleep Timer
      sleep_index(0), sleepTimerId(0), sleepDialogTimerId(0),
      // Idle Timer
      idleTimerId(0), idleDialogTimerId(0),
      // CC/Teletext input state variables
      ccInputMode(false),
      // Arbritary seek input state variables
      asInputMode(false),
      // Channel changing state variables
      queuedChanNum(""),
      lockTimerOn(false),
      // channel browsing state variables
      browsemode(false),
      browsechannum(""), browsechanid(0), browsestarttime(""),
      // Program Info for currently playing video
      lastProgram(NULL),
      inPlaylist(false), underNetworkControl(false),
      // Jump to program stuff
      jumpToProgramPIPState(kPIPOff),
      jumpToProgram(false),
      // Video Player currently receiving UI input
      playerActive(-1),
      noHardwareDecoders(false),
      //Recorder switching info
      switchToRec(NULL),
      // OSD info
      treeMenu(NULL), osdMenuEntries(NULL), udpnotify(NULL),
      // LCD Info
      lcdTitle(""), lcdSubtitle(""), lcdCallsign(""),
      // Window info (GUI is optional, transcoding, preview img, etc)
      myWindow(NULL),               isEmbedded(false),
      ignoreKeyPresses(false),
      // Timers
      lcdTimerId(0),                keyListTimerId(0),
      networkControlTimerId(0),     jumpMenuTimerId(0),
      pipChangeTimerId(0),          udpNotifyTimerId(0),
      switchToInputTimerId(0),      ccInputTimerId(0),
      asInputTimerId(0),            queueInputTimerId(0),
      browseTimerId(0),             updateOSDPosTimerId(0),
      endOfPlaybackTimerId(0),      embedCheckTimerId(0),
      endOfRecPromptTimerId(0),     videoExitDialogTimerId(0),
      pseudoChangeChanTimerId(0),   speedChangeTimerId(0),
      errorRecoveryTimerId(0),      exitPlayerTimerId(0)
{
    VERBOSE(VB_PLAYBACK, LOC + "ctor -- begin");
    ctorTime.start();

    setObjectName("TV");
    keyRepeatTimer.start();

    QMap<QString,QString> kv;
    kv["LiveTVIdleTimeout"]        = "0";
    kv["UDPNotifyPort"]            = "0";
    kv["BrowseMaxForward"]         = "240";
    kv["PlaybackExitPrompt"]       = "0";
    kv["AutoExpireDefault"]        = "0";
    kv["AutomaticSetWatched"]      = "0";
    kv["EndOfRecordingExitPrompt"] = "0";
    kv["JumpToProgramOSD"]         = "1";
    kv["GuiSizeForTV"]             = "0";
    kv["WatchTVGuide"]             = "0";
    kv["AltClearSavedPosition"]    = "1";
    kv["JobsRunOnRecordHost"]      = "0";
    kv["ContinueEmbeddedTVPlay"]   = "0";
    kv["UseFixedWindowSize"]       = "1";
    kv["PersistentBrowseMode"]     = "0";
    kv["BrowseAllTuners"]          = "0";
    kv["ChannelOrdering"]          = "channum";

    kv["CustomFilters"]            = "";
    kv["ChannelFormat"]            = "<num> <sign>";
    kv["TimeFormat"]               = "h:mm AP";
    kv["ShortDateFormat"]          = "M/d";
    kv["SmartChannelChange"]       = "0";

    kv["IndividualMuteControl"]    = "0";
    kv["UseArrowAccels"]           = "1";
    kv["OSDGeneralTimeout"]        = "2";
    kv["OSDProgramInfoTimeout"]    = "3";
    kv["TryUnflaggedSkip"]         = "0";

    kv["ChannelGroupDefault"]      = "-1";
    kv["BrowseChannelGroup"]       = "0";
    kv["SmartForward"]             = "0";
    kv["FFRewReposTime"]           = "100";
    kv["FFRewReverse"]             = "1";

    kv["ChannelGroupDefault"]      = "-1";
    kv["BrowseChannelGroup"]       = "0";
    kv["VbiFormat"]                = "";
    kv["DecodeVBIFormat"]          = "";

    int ff_rew_def[8] = { 3, 5, 10, 20, 30, 60, 120, 180 };
    for (uint i = 0; i < sizeof(ff_rew_def)/sizeof(ff_rew_def[0]); i++)
        kv[QString("FFRewSpeed%1").arg(i)] = QString::number(ff_rew_def[i]);

    MythDB::getMythDB()->GetSettings(kv);

    // convert from minutes to ms.
    db_idle_timeout        = kv["LiveTVIdleTimeout"].toInt() * 60 * 1000;
    db_udpnotify_port      = kv["UDPNotifyPort"].toInt();
    db_browse_max_forward  = kv["BrowseMaxForward"].toInt() * 60;
    db_playback_exit_prompt= kv["PlaybackExitPrompt"].toInt();
    db_autoexpire_default  = kv["AutoExpireDefault"].toInt();
    db_auto_set_watched    = kv["AutomaticSetWatched"].toInt();
    db_end_of_rec_exit_prompt = kv["EndOfRecordingExitPrompt"].toInt();
    db_jump_prefer_osd     = kv["JumpToProgramOSD"].toInt();
    db_use_gui_size_for_tv = kv["GuiSizeForTV"].toInt();
    db_start_in_guide      = kv["WatchTVGuide"].toInt();
    db_toggle_bookmark     = kv["AltClearSavedPosition"].toInt();
    db_run_jobs_on_remote  = kv["JobsRunOnRecordHost"].toInt();
    db_continue_embedded   = kv["ContinueEmbeddedTVPlay"].toInt();
    db_use_fixed_size      = kv["UseFixedWindowSize"].toInt();
    db_browse_always       = kv["PersistentBrowseMode"].toInt();
    db_browse_all_tuners   = kv["BrowseAllTuners"].toInt();
    QString channelOrdering= kv["ChannelOrdering"];
    baseFilters           += kv["CustomFilters"];
    db_channel_format      = kv["ChannelFormat"];
    db_time_format         = kv["TimeFormat"];
    db_short_date_format   = kv["ShortDateFormat"];
    smartChannelChange     = kv["SmartChannelChange"].toInt();
    MuteIndividualChannels = kv["IndividualMuteControl"].toInt();
    arrowAccel             = kv["UseArrowAccels"].toInt();
    osd_general_timeout    = kv["OSDGeneralTimeout"].toInt();
    osd_prog_info_timeout  = kv["OSDProgramInfoTimeout"].toInt();
    tryUnflaggedSkip       = kv["TryUnflaggedSkip"].toInt();
    channel_group_id       = kv["ChannelGroupDefault"].toInt();
    browse_changrp         = kv["BrowseChannelGroup"].toInt();
    smartForward           = kv["SmartForward"].toInt();
    ff_rew_repos           = kv["FFRewReposTime"].toFloat() * 0.01f;
    ff_rew_reverse         = kv["FFRewReverse"].toInt();
    channel_group_id       = kv["ChannelGroupDefault"].toInt();
    browse_changrp         = kv["BrowseChannelGroup"].toInt();
    QString beVBI          = kv["VbiFormat"];
    QString feVBI          = kv["DecodeVBIFormat"];

    for (uint i = 0; i < sizeof(ff_rew_def)/sizeof(ff_rew_def[0]); i++)
        ff_rew_speeds.push_back(kv[QString("FFRewSpeed%1").arg(i)].toInt());

    // process it..

    if (db_browse_all_tuners)
    {
        QMutexLocker locker(&browseLock);
        db_browse_all_channels = ChannelUtil::GetChannels(
            0, true, "channum, callsign");
        ChannelUtil::SortChannels(
            db_browse_all_channels, channelOrdering, true);
    }

    vbimode = VBIMode::Parse(!feVBI.isEmpty() ? feVBI : beVBI);

    sleep_times.push_back(SleepTimerInfo(QObject::tr("Off"),       0));
    sleep_times.push_back(SleepTimerInfo(QObject::tr("30m"),   30*60));
    sleep_times.push_back(SleepTimerInfo(QObject::tr("1h"),    60*60));
    sleep_times.push_back(SleepTimerInfo(QObject::tr("1h30m"), 90*60));
    sleep_times.push_back(SleepTimerInfo(QObject::tr("2h"),   120*60));

    osdMenuEntries = new TVOSDMenuEntryList();

    gCoreContext->addListener(this);

    playerLock.lockForWrite();
    player.push_back(new PlayerContext(kPlayerInUseID));
    playerActive = 0;
    playerLock.unlock();
    VERBOSE(VB_PLAYBACK, LOC + "ctor -- end");
}

/** \fn TV::Init(bool)
 *  \brief Performs instance initialization, returns true on success.
 *
 *  \param createWindow If true a MythDialog is created for display.
 *  \return Returns true on success, false on failure.
 */
bool TV::Init(bool createWindow)
{
    VERBOSE(VB_PLAYBACK, LOC + "Init -- begin");

    if (browse_changrp && (channel_group_id > -1))
    {
      m_channellist = ChannelUtil::GetChannels(0, true, "channum, callsign", channel_group_id);
      ChannelUtil::SortChannels(m_channellist, "channum", true);
    }

    m_changrplist  = ChannelGroup::GetChannelGroups();

    VERBOSE(VB_PLAYBACK, LOC + "Init -- end channel groups");

    if (createWindow)
    {
        bool fullscreen = !gCoreContext->GetNumSetting("GuiSizeForTV", 0);
        bool switchMode = gCoreContext->GetNumSetting("UseVideoModes", 0);

        saved_gui_bounds = QRect(GetMythMainWindow()->geometry().topLeft(),
                                 GetMythMainWindow()->size());

        // adjust for window manager wierdness.
        {
            int xbase, width, ybase, height;
            float wmult, hmult;
            GetMythUI()->GetScreenSettings(xbase, width, wmult,
                                           ybase, height, hmult);
            if ((abs(saved_gui_bounds.x()-xbase) < 3) &&
                (abs(saved_gui_bounds.y()-ybase) < 3))
            {
                saved_gui_bounds = QRect(QPoint(xbase, ybase),
                                         GetMythMainWindow()->size());
            }
        }

        // if width && height are zero users expect fullscreen playback
        if (!fullscreen)
        {
            int gui_width = 0, gui_height = 0;
            gCoreContext->GetResolutionSetting("Gui", gui_width, gui_height);
            fullscreen |= (0 == gui_width && 0 == gui_height);
        }

        player_bounds = saved_gui_bounds;
        if (fullscreen)
        {
            int xbase, width, ybase, height;
            GetMythUI()->GetScreenBounds(xbase, ybase, width, height);
            player_bounds = QRect(xbase, ybase, width, height);
        }

        // main window sizing
        int maxWidth = 1920, maxHeight = 1440;
        if (switchMode)
        {
            DisplayRes *display_res = DisplayRes::GetDisplayRes();
            if(display_res)
            {
                // The very first Resize needs to be the maximum possible
                // desired res, because X will mask off anything outside
                // the initial dimensions
                maxWidth = display_res->GetMaxWidth();
                maxHeight = display_res->GetMaxHeight();

                // bit of a hack, but it's ok if the window is too
                // big in fullscreen mode
                if (fullscreen)
                {
                    player_bounds.setSize(QSize(maxWidth, maxHeight));

                    // resize possibly avoids a bug on some systems
                    GetMythMainWindow()->setGeometry(player_bounds);
                }
            }
        }

        // player window sizing
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

        myWindow = new TvPlayWindow(mainStack, "Playback");

        if (myWindow->Create())
            mainStack->AddScreen(myWindow, false);
        else
        {
            delete myWindow;
            myWindow = NULL;
        }

        MythMainWindow *mainWindow = GetMythMainWindow();
        //QPalette p = mainWindow->palette();
        //p.setColor(mainWindow->backgroundRole(), Qt::black);
        //mainWindow->setPalette(p);
        mainWindow->installEventFilter(this);
        qApp->processEvents();
    }

    mainLoopCondLock.lock();
    start();
    mainLoopCond.wait(&mainLoopCondLock);
    errorRecoveryTimerId = StartTimer(kErrorRecoveryCheckFrequency, __LINE__);
    lcdTimerId           = StartTimer(1, __LINE__);
    speedChangeTimerId   = StartTimer(kSpeedChangeCheckFrequency, __LINE__);

    mainLoopCondLock.unlock();

    VERBOSE(VB_PLAYBACK, LOC + "Init -- end");
    return true;
}

TV::~TV(void)
{
    VERBOSE(VB_PLAYBACK, "TV::~TV() -- begin");

    if (udpnotify)
    {
        udpnotify->deleteLater();
        udpnotify = NULL;
    }

    gCoreContext->removeListener(this);

    GetMythMainWindow()->SetDrawEnabled(true);

    if (myWindow)
    {
        myWindow->Close();
        myWindow = NULL;
    }

    TV::exit(0);
    TV::wait();

    VERBOSE(VB_PLAYBACK, "TV::~TV() -- lock");

    // restore window to gui size and position
    MythMainWindow* mwnd = GetMythMainWindow();
    mwnd->setGeometry(saved_gui_bounds);
    mwnd->setFixedSize(saved_gui_bounds.size());
    mwnd->show();
    if (!db_use_gui_size_for_tv)
        mwnd->move(saved_gui_bounds.topLeft());

    if (treeMenu)
        delete treeMenu;

    if (lastProgram)
        delete lastProgram;

    if (class LCD * lcd = LCD::Get())
    {
        lcd->setFunctionLEDs(FUNC_TV, false);
        lcd->setFunctionLEDs(FUNC_MOVIE, false);
        lcd->switchToTime();
    }

    if (ddMapLoaderRunning)
    {
        pthread_join(ddMapLoader, NULL);
        ddMapLoaderRunning = false;

        if (ddMapSourceId)
        {
            int *src = new int;
            *src = ddMapSourceId;
            pthread_create(&ddMapLoader, NULL, load_dd_map_post_thunk, src);
            pthread_detach(ddMapLoader);
        }
    }

    if (osdMenuEntries)
        delete osdMenuEntries;

    PlayerContext *mctx = GetPlayerWriteLock(0, __FILE__, __LINE__);
    while (!player.empty())
    {
        delete player.back();
        player.pop_back();
    }
    ReturnPlayerLock(mctx);

    GetMythMainWindow()->GetPaintWindow()->show();

    VERBOSE(VB_PLAYBACK, "TV::~TV() -- end");
}

/**
 * \brief save channel group setting to database
 */
void TV::SaveChannelGroup(void)
{
    int remember_last_changrp = gCoreContext->GetNumSetting("ChannelGroupRememberLast", 0);

    if (remember_last_changrp)
       gCoreContext->SaveSetting("ChannelGroupDefault", channel_group_id);
}

/**
 * \brief update the channel list with channels from the selected channel group
 */
void TV::UpdateChannelList(int groupID)
{
    if (groupID == channel_group_id)
        return;

    channel_group_id = groupID;

    if (browse_changrp)
    {
        m_channellist = ChannelUtil::GetChannels(0, true, "channum, callsign", channel_group_id);
        ChannelUtil::SortChannels(m_channellist, "channum", true);
    }
}

/**
 * \brief get tv state of active player context
 */
TVState TV::GetState(int player_idx) const
{
    TVState ret = kState_ChangingState;
    const PlayerContext *ctx = GetPlayerReadLock(player_idx, __FILE__, __LINE__);
    ret = GetState(ctx);
    ReturnPlayerLock(ctx);
    return ret;
}

/**
 * \brief get tv state of active player context
 */
TVState TV::GetState(const PlayerContext *actx) const
{
    TVState ret = kState_ChangingState;
    if (!actx->InStateChange())
        ret = actx->GetState();
    return ret;
}

/** \fn TV::LiveTV(bool,bool)
 *  \brief Starts LiveTV
 *  \param showDialogs if true error dialogs are shown, if false they are not
 *  \param startInGuide if true the EPG will be shown upon entering LiveTV
 */
bool TV::LiveTV(bool showDialogs, bool startInGuide)
{
    requestDelete = false;
    allowRerecord = false;
    jumpToProgram = false;

    PlayerContext *actx = GetPlayerReadLock(-1, __FILE__, __LINE__);
    if (actx->GetState() == kState_None &&
        RequestNextRecorder(actx, showDialogs))
    {
        actx->SetInitialTVState(true);
        ScheduleStateChange(actx);
        switchToRec = NULL;

        // Start Idle Timer
        if (db_idle_timeout > 0)
        {
            idleTimerId = StartTimer(db_idle_timeout, __LINE__);
            VERBOSE(VB_GENERAL, QString("Using Idle Timer. %1 minutes")
                    .arg(db_idle_timeout*(1.0f/60000.0f)));
        }

        if (startInGuide || db_start_in_guide)
        {
            MSqlQuery query(MSqlQuery::InitCon());
            query.prepare("SELECT keylist FROM keybindings WHERE "
                          "context = 'TV Playback' AND action = 'GUIDE' AND "
                          "hostname = :HOSTNAME ;");
            query.bindValue(":HOSTNAME", gCoreContext->GetHostName());

            if (query.exec() && query.isActive() && query.size() > 0)
            {
                query.next();

                QKeySequence keyseq(query.value(0).toString());

                int keynum = keyseq[0];
                keynum &= ~Qt::UNICODE_ACCEL;

                QMutexLocker locker(&timerIdLock);
                keyList.push_front(
                    new QKeyEvent(QEvent::KeyPress, keynum, 0, 0));
                if (!keyListTimerId)
                    keyListTimerId = StartTimer(1, __LINE__);
            }
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

bool TV::RequestNextRecorder(PlayerContext *ctx, bool showDialogs)
{
    if (!ctx)
        return false;

    ctx->SetRecorder(NULL);

    RemoteEncoder *testrec = NULL;
    if (switchToRec)
    {
        // If this is set we, already got a new recorder in SwitchCards()
        testrec = switchToRec;
        switchToRec = NULL;
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
    if (StateIsRecording(GetState(ctx)) && ctx->recorder)
        ctx->recorder->FinishRecording();
    ReturnPlayerLock(ctx);
}

void TV::AskAllowRecording(PlayerContext *ctx,
                           const QStringList &msg, int timeuntil,
                           bool hasrec, bool haslater)
{
    //VERBOSE(VB_IMPORTANT, LOC + "AskAllowRecording");
    if (!StateIsLiveTV(GetState(ctx)))
       return;

    ProgramInfo *info = new ProgramInfo(msg);
    if (!info->GetChanID())
    {
        delete info;
        return;
    }

    QMutexLocker locker(&askAllowLock);
    QString key = info->MakeUniqueKey();
    if (timeuntil > 0)
    {
        // add program to list
        //VERBOSE(VB_IMPORTANT, LOC + "AskAllowRecording -- " +
        //        QString("adding '%1'").arg(info->title));
        QDateTime expiry = QDateTime::currentDateTime().addSecs(timeuntil);
        askAllowPrograms[key] = AskProgramInfo(expiry, hasrec, haslater, info);
    }
    else
    {
        // remove program from list
        VERBOSE(VB_IMPORTANT, LOC + "AskAllowRecording -- " +
                QString("removing '%1'").arg(info->GetTitle()));
        QMap<QString,AskProgramInfo>::iterator it = askAllowPrograms.find(key);
        if (it != askAllowPrograms.end())
        {
            delete (*it).info;
            askAllowPrograms.erase(it);
        }
        delete info;
    }

    UpdateOSDAskAllowDialog(ctx);
}

void TV::UpdateOSDAskAllowDialog(PlayerContext *ctx)
{
    QMutexLocker locker(&askAllowLock);
    if (!ctx->recorder)
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
    QDateTime timeNow = QDateTime::currentDateTime();
    QMap<QString,AskProgramInfo>::iterator it = askAllowPrograms.begin();
    QMap<QString,AskProgramInfo>::iterator next = it;
    while (it != askAllowPrograms.end())
    {
        next = it; next++;
        if ((*it).expiry <= timeNow)
        {
            //VERBOSE(VB_IMPORTANT, LOC + "UpdateOSDAskAllowDialog -- " +
            //        QString("removing '%1'").arg((*it).info->title));
            delete (*it).info;
            askAllowPrograms.erase(it);
        }
        it = next;
    }

    AskAllowType type      = kAskAllowCancel;
    int          sel       = 0;
    int          timeuntil = 0;
    QString      message   = QString::null;
    QStringList  options;

    uint conflict_count = askAllowPrograms.size();

    it = askAllowPrograms.begin();
    if ((1 == askAllowPrograms.size()) && ((*it).info->GetCardID() == cardid))
    {
        (*it).is_in_same_input_group = (*it).is_conflicting = true;
    }
    else if (!askAllowPrograms.empty())
    {
        // get the currently used input on our card
        bool busy_input_grps_loaded = false;
        vector<uint> busy_input_grps;
        TunedInputInfo busy_input;
        RemoteIsBusy(cardid, busy_input);

        // check if current input can conflict
        it = askAllowPrograms.begin();
        for (; it != askAllowPrograms.end(); ++it)
        {
            (*it).is_in_same_input_group =
                (cardid == (*it).info->GetCardID());

            if ((*it).is_in_same_input_group)
                continue;

            // is busy_input in same input group as recording
            if (!busy_input_grps_loaded)
            {
                busy_input_grps = CardUtil::GetInputGroups(busy_input.inputid);
                busy_input_grps_loaded = true;
            }

            vector<uint> input_grps =
                CardUtil::GetInputGroups((*it).info->GetInputID());

            for (uint i = 0; i < input_grps.size(); i++)
            {
                if (find(busy_input_grps.begin(), busy_input_grps.end(),
                         input_grps[i]) !=  busy_input_grps.end())
                {
                    (*it).is_in_same_input_group = true;
                    break;
                }
            }
        }

        // check if inputs that can conflict are ok
        conflict_count = 0;
        it = askAllowPrograms.begin();
        for (; it != askAllowPrograms.end(); ++it)
        {
            if (!(*it).is_in_same_input_group)
                (*it).is_conflicting = false;
            else if ((cardid == (uint)(*it).info->GetCardID()))
                (*it).is_conflicting = true;
            else if (!CardUtil::IsTunerShared(cardid, (*it).info->GetCardID()))
                (*it).is_conflicting = true;
            else if ((busy_input.sourceid == (uint)(*it).info->GetSourceID()) &&
                     (busy_input.mplexid  == (uint)(*it).info->QueryMplexID()))
                (*it).is_conflicting = false;
            else
                (*it).is_conflicting = true;

            conflict_count += (*it).is_conflicting ? 1 : 0;
        }
    }

    it = askAllowPrograms.begin();
    for (; it != askAllowPrograms.end() && !(*it).is_conflicting; ++it);

    if (conflict_count == 0)
    {
        VERBOSE(VB_GENERAL, LOC + "The scheduler wants to make "
                "a non-conflicting recording.");
        // TODO take down mplexid and inform user of problem
        // on channel changes.
    }
    else if (conflict_count == 1 && ((*it).info->GetCardID() == cardid))
    {
        //VERBOSE(VB_IMPORTANT, LOC + "UpdateOSDAskAllowDialog -- " +
        //        "kAskAllowOneRec");

        type = kAskAllowOneRec;
        it = askAllowPrograms.begin();

        QString channel = db_channel_format;
        channel
            .replace("<num>",  (*it).info->GetChanNum())
            .replace("<sign>", (*it).info->GetChannelSchedulingID())
            .replace("<name>", (*it).info->GetChannelName());

        message = single_rec.arg((*it).info->GetTitle()).arg(channel);

        options += record_watch;
        options += let_record1;
        options += ((*it).has_later) ? record_later1 : do_not_record1;

        sel = ((*it).has_rec) ? 2 : 0;
        timeuntil = QDateTime::currentDateTime().secsTo((*it).expiry);
    }
    else
    {
        type = kAskAllowMultiRec;

        if (conflict_count > 1)
        {
            message = QObject::tr(
                "MythTV wants to record these programs in %d seconds:");
            message += "\n";
        }

        bool has_rec = false;
        it = askAllowPrograms.begin();
        for (; it != askAllowPrograms.end(); ++it)
        {
            if (!(*it).is_conflicting)
                continue;

            QString title = (*it).info->GetTitle();
            if ((title.length() < 10) && !(*it).info->GetSubtitle().isEmpty())
                title += ": " + (*it).info->GetSubtitle();
            if (title.length() > 20)
                title = title.left(17) + "...";

            QString channel = db_channel_format;
            channel
                .replace("<num>",  (*it).info->GetChanNum())
                .replace("<sign>", (*it).info->GetChannelSchedulingID())
                .replace("<name>", (*it).info->GetChannelName());

            if (conflict_count > 1)
            {
                message += QObject::tr("\"%1\" on %2").arg(title).arg(channel);
                message += "\n";
            }
            else
            {
                message = single_rec.arg((*it).info->GetTitle()).arg(channel);
                has_rec = (*it).has_rec;
            }
        }

        if (conflict_count > 1)
        {
            message += "\n";
            message += QObject::tr("Do you want to:");
        }

        bool all_have_later = true;
        it = askAllowPrograms.begin();
        for (; it != askAllowPrograms.end(); ++it)
        {
            if ((*it).is_conflicting)
                all_have_later &= (*it).has_later;
        }

        if (conflict_count > 1)
        {
            options += let_recordm;
            options += (all_have_later) ? record_laterm : do_not_recordm;
            sel = 0;
        }
        else
        {
            options += let_record1;
            options += (all_have_later) ? record_later1 : do_not_record1;
            sel = (has_rec) ? 1 : 0;
        }

        it = askAllowPrograms.begin();
        timeuntil = 9999;
        for (; it != askAllowPrograms.end(); ++it)
        {
            if (!(*it).is_conflicting)
                continue;
            int tmp = QDateTime::currentDateTime().secsTo((*it).expiry);
            timeuntil = min(timeuntil, max(tmp, 0));
        }
        timeuntil = (9999 == timeuntil) ? 0 : timeuntil;
    }


    OSD *osd = GetOSDLock(ctx);
    if (!osd)
    {
        ReturnOSDLock(ctx, osd);
        return;
    }

    // Only allow one of these dialogs at a time...
    if (osd->DialogShowing(kOSDDialogAllowRecording))
    {
        askAllowType = kAskAllowCancel;
        osd->HideSet(kOSDDialogAllowRecording, true);
    }

    askAllowType = type;

    if (kAskAllowCancel != askAllowType)
    {
        BrowseEnd(ctx, false);
        osd->DialogAbortAndHideAll();
        osd->HideTreeMenu();
        osd->NewDialogBox(
            kOSDDialogAllowRecording, message, options, timeuntil, sel);
    }

    ReturnOSDLock(ctx, osd);
    //VERBOSE(VB_IMPORTANT, LOC + "UpdateOSDAskAllowDialog -- done");
}

void TV::HandleOSDAskAllowResponse(PlayerContext *ctx, int dialog_result)
{
    if (!askAllowLock.tryLock())
    {
        VERBOSE(VB_IMPORTANT, "allowrecordingbox : askAllowLock is locked");
        return;
    }

    if (kAskAllowOneRec == askAllowType)
    {
        //VERBOSE(VB_IMPORTANT, "allowrecordingbox : one : "<<result);
        switch (dialog_result)
        {
            default:
            case 1:
                // watch while it records
                if (ctx->recorder)
                    ctx->recorder->CancelNextRecording(false);
                break;
            case 2:
                // return to main menu
                PrepareToExitPlayer(ctx, __LINE__);
                SetExitPlayer(true, true);
                break;
            case 3:
                // cancel scheduled recording
                if (ctx->recorder)
                    ctx->recorder->CancelNextRecording(true);
                break;
        }
    }
    else if (kAskAllowMultiRec == askAllowType)
    {
        //VERBOSE(VB_IMPORTANT, "allowrecordingbox : multi : "<<result);
        switch (dialog_result)
        {
            default:
            case 1:
                // return to main menu
                PrepareToExitPlayer(ctx, __LINE__);
                SetExitPlayer(true, true);
                break;
            case 2:
            {
                // cancel conflicting scheduled recordings
                QMap<QString,AskProgramInfo>::iterator it =
                    askAllowPrograms.begin();
                for (; it != askAllowPrograms.end(); ++it)
                {
                    if ((*it).is_conflicting)
                    {
                        RemoteCancelNextRecording(
                            (*it).info->GetCardID(), true);
                    }
                }
                break;
            }
        }
    }
    else
    {
        //VERBOSE(VB_IMPORTANT,
        //        "allowrecordingbox : cancel : "<<result);
    }

    askAllowLock.unlock();
}


int TV::Playback(const ProgramInfo &rcinfo)
{
    wantsToQuit   = false;
    jumpToProgram = false;
    allowRerecord = false;
    requestDelete = false;

    PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
    if (mctx->GetState() != kState_None)
    {
        ReturnPlayerLock(mctx);
        return 0;
    }

    mctx->SetPlayingInfo(&rcinfo);
    mctx->SetInitialTVState(false);
    ScheduleStateChange(mctx);

    ReturnPlayerLock(mctx);

    if (class LCD * lcd = LCD::Get())
    {
        lcd->switchToChannel(rcinfo.GetChannelSchedulingID(),
                             rcinfo.GetTitle(), rcinfo.GetSubtitle());
        lcd->setFunctionLEDs((rcinfo.IsRecording())?FUNC_TV:FUNC_MOVIE, true);
    }

    return 1;
}

#ifdef PLAY_FROM_RECORDER
int TV::PlayFromRecorder(int recordernum)
{
    int retval = 0;

    PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);

    if (mctx->recorder)
    {
        VERBOSE(VB_IMPORTANT, LOC +
                QString("PlayFromRecorder(%1): Recorder already exists!")
                .arg(recordernum));
        ReturnPlayerLock(mctx);
        return -1;
    }

    mctx->SetRecorder(RemoteGetExistingRecorder(recordernum));
    if (!mctx->recorder)
    {
        ReturnPlayerLock(mctx);
        return -1;
    }

    ProgramInfo pginfo;

    if (mctx->recorder->IsValidRecorder())
    {
        ReturnPlayerLock(mctx);

        // let the mainloop get the programinfo from encoder,
        // connecting to encoder won't work from here
        recorderPlaybackInfoLock.lock();
        int my_timer = StartTimer(1, __LINE__);
        recorderPlaybackInfoTimerId[my_timer] = my_timer;

        bool done = false;
        while (!recorderPlaybackInfoWaitCond
               .wait(&recorderPlaybackInfoLock, 100) && !done)
        {
            QMap<int,ProgramInfo>::iterator it =
                recorderPlaybackInfo.find(my_timer);
            if (it != recorderPlaybackInfo.end())
            {
                pginfo = *it;
                recorderPlaybackInfo.erase(it);
                done = true;
            }
        }
        recorderPlaybackInfoLock.unlock();

        mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
    }

    mctx->SetRecorder(NULL);
    ReturnPlayerLock(mctx);

    bool fileexists = false;
    if (pginfo.IsMythStream())
        fileexists = RemoteCheckFile(&pginfo);
    else
    {
        QFile checkFile(pginfo.GetPlaybackURL(true));
        fileexists = checkFile.exists();
    }

    if (fileexists)
    {
        Playback(pginfo);
        retval = 1;
    }

    return retval;
}
#endif // PLAY_FROM_RECORDER

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
            state == kState_WatchingDVD);
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
   ((ctxState == ASTATE) && (desiredNextState == BSTATE))

#define SET_NEXT() do { nextState = desiredNextState; changed = true; } while(0)
#define SET_LAST() do { nextState = ctxState; changed = true; } while(0)

static QString tv_i18n(const QString &msg)
{
    QByteArray msg_arr = msg.toLatin1();
    QString msg_i18n = QObject::tr(msg_arr.constData());
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
    VERBOSE(VB_PLAYBACK, LOC +
            QString("HandleStateChange(%1) -- begin")
            .arg(find_player_index(ctx)));

    if (ctx->IsErrored())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "HandleStateChange(): "
                "Called after fatal error detected.");
        return;
    }

    bool changed = false;

    ctx->LockState();
    TVState nextState = ctx->GetState();
    if (ctx->nextState.empty())
    {
        VERBOSE(VB_IMPORTANT, LOC + "HandleStateChange() Warning, "
                "called with no state to change to.");
        ctx->UnlockState();

        QMutexLocker locker(&stateChangeCondLock);
        stateChangeCond.wakeAll();
        return;
    }

    TVState ctxState = ctx->GetState();
    TVState desiredNextState = ctx->DequeueNextState();

    VERBOSE(VB_GENERAL, LOC + QString("Attempting to change from %1 to %2")
            .arg(StateToString(nextState))
            .arg(StateToString(desiredNextState)));

    if (desiredNextState == kState_Error)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "HandleStateChange(): "
                "Attempting to set to an error state!");
        SetErrored(ctx);
        ctx->UnlockState();

        QMutexLocker locker(&stateChangeCondLock);
        stateChangeCond.wakeAll();
        return;
    }

    bool ok = false;
    if (TRANSITION(kState_None, kState_WatchingLiveTV))
    {
        QString name = "";

        ctx->lastSignalUIInfo.clear();

        ctx->recorder->Setup();

        QDateTime timerOffTime = QDateTime::currentDateTime();
        lockTimerOn = false;

        SET_NEXT();
        VERBOSE(VB_IMPORTANT, "Spawning LiveTV Recorder -- begin");
        ctx->recorder->SpawnLiveTV(ctx->tvchain->GetID(), false, "");
        VERBOSE(VB_IMPORTANT, "Spawning LiveTV Recorder -- end");

        if (!ctx->ReloadTVChain())
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "HandleStateChange(): LiveTV not successfully started");
            RestoreScreenSaver(ctx);
            ctx->SetRecorder(NULL);
            SetErrored(ctx);
            SET_LAST();
        }
        else
        {
            ctx->LockPlayingInfo(__FILE__, __LINE__);
            QString playbackURL = ctx->playingInfo->GetPlaybackURL(true);
            ctx->UnlockPlayingInfo(__FILE__, __LINE__);

            bool opennow = (ctx->tvchain->GetCardType(-1) != "DUMMY");

            VERBOSE(VB_IMPORTANT, QString("We have a playbackURL(%1) & "
                                          "cardtype(%2)")
                    .arg(playbackURL).arg(ctx->tvchain->GetCardType(-1)));

            ctx->SetRingBuffer(new RingBuffer(playbackURL, false, true,
                                      opennow ? 12 : (uint)-1));
            ctx->buffer->SetLiveMode(ctx->tvchain);
        }

        VERBOSE(VB_IMPORTANT, "We have a RingBuffer");

        GetMythUI()->DisableScreensaver();
        GetMythMainWindow()->SetDrawEnabled(false);

        if (ctx->playingInfo && StartRecorder(ctx,-1))
        {
            // Cache starting frame rate for this recorder
            ctx->last_framerate = ctx->recorder->GetFrameRate();
            ok = StartPlayer(mctx, ctx, desiredNextState);
        }
        if (!ok)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "LiveTV not successfully started");
            RestoreScreenSaver(ctx);
            ctx->SetRecorder(NULL);
            SetErrored(ctx);
            SET_LAST();
        }
        else if (!ctx->IsPIP())
        {
            if (!lastLockSeenTime.isValid() ||
                (lastLockSeenTime < timerOffTime))
            {
                lockTimer.start();
                lockTimerOn = true;
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
    else if (TRANSITION(kState_WatchingRecording, kState_WatchingPreRecorded))
    {
        SET_NEXT();
    }
    else if (TRANSITION(kState_None, kState_WatchingPreRecorded) ||
             TRANSITION(kState_None, kState_WatchingVideo) ||
             TRANSITION(kState_None, kState_WatchingDVD)   ||
             TRANSITION(kState_None, kState_WatchingRecording))
    {
        ctx->LockPlayingInfo(__FILE__, __LINE__);
        QString playbackURL;
        if (ctx->playingInfo->IsRecording())
        {
            playbackURL = ctx->playingInfo->GetPlaybackURL(true);
        }
        else
        {
            playbackURL = ctx->playingInfo->GetPathname();
            playbackURL.detach();
        }
        ctx->UnlockPlayingInfo(__FILE__, __LINE__);

        ctx->SetRingBuffer(new RingBuffer(playbackURL, false));

        if (ctx->buffer && ctx->buffer->IsOpen())
        {
            GetMythMainWindow()->SetDrawEnabled(false);
            GetMythUI()->DisableScreensaver();

            if (desiredNextState == kState_WatchingRecording)
            {
                ctx->LockPlayingInfo(__FILE__, __LINE__);
                RemoteEncoder *rec = RemoteGetExistingRecorder(
                    ctx->playingInfo);
                ctx->UnlockPlayingInfo(__FILE__, __LINE__);

                ctx->SetRecorder(rec);

                if (!ctx->recorder ||
                    !ctx->recorder->IsValidRecorder())
                {
                    VERBOSE(VB_IMPORTANT, LOC_ERR + "Couldn't find "
                            "recorder for in-progress recording");
                    desiredNextState = kState_WatchingPreRecorded;
                    ctx->SetRecorder(NULL);
                }
                else
                {
                    ctx->recorder->Setup();
                }
            }

            ok = StartPlayer(mctx, ctx, desiredNextState);

            if (ok)
            {
                SET_NEXT();

                ctx->LockPlayingInfo(__FILE__, __LINE__);
                if (ctx->playingInfo->IsRecording())
                {
                    QString message = "COMMFLAG_REQUEST ";
                    message += ctx->playingInfo->MakeUniqueKey();
                    RemoteSendMessage(message);
                }
                ctx->UnlockPlayingInfo(__FILE__, __LINE__);
            }
        }

        if (!ok)
        {
            SET_LAST();
            SetErrored(ctx);
        }
        else if (mctx != ctx)
        {
            SetActive(ctx, find_player_index(ctx), false);
        }
    }
    else if (TRANSITION(kState_WatchingPreRecorded, kState_None) ||
             TRANSITION(kState_WatchingVideo, kState_None)       ||
             TRANSITION(kState_WatchingDVD, kState_None)         ||
             TRANSITION(kState_WatchingRecording, kState_None))
    {
        SET_NEXT();

        RestoreScreenSaver(ctx);
        StopStuff(mctx, ctx, true, true, false);

        if ((mctx != ctx) && (GetPlayer(ctx,-1) == ctx))
            SetActive(mctx, 0, true);
    }
    else if (TRANSITION(kState_None, kState_None))
    {
        SET_NEXT();
    }

    // Check that new state is valid
    ctx->LockDeleteNVP(__FILE__, __LINE__);
    bool decoder_died_unexpectedly = (kState_None != nextState) &&
        ctx->nvp && !ctx->nvp->IsDecoderThreadAlive();
    ctx->UnlockDeleteNVP(__FILE__, __LINE__);
    if (decoder_died_unexpectedly)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Decoder not alive, and trying to play..");
        if (nextState == kState_WatchingLiveTV)
        {
            StopStuff(mctx, ctx, true, true, true);
        }

        nextState = kState_None;
        changed = true;

        if ((mctx != ctx) && (GetPlayer(ctx,-1) == ctx))
            SetActive(mctx, 0, true);
    }

    // Print state changed message...
    if (!changed)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Unknown state transition: %1 to %2")
                .arg(StateToString(ctx->GetState()))
                .arg(StateToString(desiredNextState)));
    }
    else if (ctx->GetState() != nextState)
    {
        VERBOSE(VB_GENERAL, LOC + QString("Changing from %1 to %2")
                .arg(StateToString(ctx->GetState()))
                .arg(StateToString(nextState)));
    }

    // update internal state variable
    TVState lastState = ctx->GetState();
    ctx->playingState = nextState;
    ctx->UnlockState();

    if (mctx == ctx)
    {
        if (StateIsLiveTV(ctx->GetState()))
        {
            VERBOSE(VB_IMPORTANT, LOC + "State is LiveTV & mctx == ctx");
            UpdateOSDInput(ctx);
            VERBOSE(VB_IMPORTANT, LOC + "UpdateOSDInput done");
            UpdateLCD();
            VERBOSE(VB_IMPORTANT, LOC + "UpdateLCD done");
            ITVRestart(ctx, true);
            VERBOSE(VB_IMPORTANT, LOC + "ITVRestart done");
        }
        else if (StateIsPlaying(ctx->GetState()) && lastState == kState_None)
        {
            OSD *osd = GetOSDLock(ctx);
            ctx->LockPlayingInfo(__FILE__, __LINE__);
            if (osd && (PlayGroup::GetCount() > 0))
                osd->SetSettingsText(tr("%1 Settings")
                    .arg(tv_i18n(ctx->playingInfo->GetPlaybackGroup())), 3);
            ctx->UnlockPlayingInfo(__FILE__, __LINE__);
            ReturnOSDLock(ctx, osd);
            ITVRestart(ctx, false);
        }

        if (ctx->buffer && ctx->buffer->isDVD())
        {
            UpdateLCD();
        }

        if (ctx->recorder)
            ctx->recorder->FrontendReady();

        QMutexLocker locker(&timerIdLock);
        if (endOfRecPromptTimerId)
            KillTimer(endOfRecPromptTimerId);
        endOfRecPromptTimerId = 0;
        if (db_end_of_rec_exit_prompt && !inPlaylist && !underNetworkControl)
        {
            endOfRecPromptTimerId =
                StartTimer(kEndOfRecPromptCheckFrequency, __LINE__);
        }

        if (endOfPlaybackTimerId)
            KillTimer(endOfPlaybackTimerId);
        endOfPlaybackTimerId = 0;

        if (StateIsPlaying(ctx->GetState()))
        {
            endOfPlaybackTimerId =
                StartTimer(kEndOfPlaybackFirstCheckTimer, __LINE__);

        }

    }

    if (TRANSITION(kState_None, kState_WatchingPreRecorded) ||
             TRANSITION(kState_None, kState_WatchingVideo) ||
             TRANSITION(kState_None, kState_WatchingDVD)   ||
             TRANSITION(kState_None, kState_WatchingRecording) ||
             TRANSITION(kState_None, kState_WatchingLiveTV))
    {
        MythMainWindow *mainWindow = GetMythMainWindow();
        mainWindow->setBaseSize(player_bounds.size());
        mainWindow->setMinimumSize(
            (db_use_fixed_size) ? player_bounds.size() : QSize(16, 16));
        mainWindow->setMaximumSize(
            (db_use_fixed_size) ? player_bounds.size() :
            QSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX));
        mainWindow->setGeometry(player_bounds);

        // hide the GUI paint window
        GetMythMainWindow()->GetPaintWindow()->hide();
    }

    VERBOSE(VB_PLAYBACK, LOC +
            QString("HandleStateChange(%1) -- end")
            .arg(find_player_index(ctx)));

    QMutexLocker locker(&stateChangeCondLock);
    stateChangeCond.wakeAll();
}
#undef TRANSITION
#undef SET_NEXT
#undef SET_LAST

/** \fn TV::StartRecorder(PlayerContext*, int)
 *  \brief Starts recorder, must be called before StartPlayer().
 *  \param maxWait How long to wait for RecorderBase to start recording.
 *  \return true when successful, false otherwise.
 */
bool TV::StartRecorder(PlayerContext *ctx, int maxWait)
{
    RemoteEncoder *rec = ctx->recorder;
    maxWait = (maxWait <= 0) ? 40000 : maxWait;
    MythTimer t;
    t.start();
    bool recording = false, ok = true;
    if (!rec) {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Invalid Remote Encoder");
        SetErrored(ctx);
        return false;
    }
    while (!(recording = rec->IsRecording(&ok)) &&
           !exitPlayerTimerId && t.elapsed() < maxWait)
    {
        if (!ok)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "StartRecorder() -- "
                    "lost contact with backend");
            SetErrored(ctx);
            return false;
        }
        usleep(5000);
    }

    if (!recording || exitPlayerTimerId)
    {
        if (!exitPlayerTimerId)
            VERBOSE(VB_IMPORTANT, LOC_ERR + "StartRecorder() -- "
                    "timed out waiting for recorder to start");
        return false;
    }

    VERBOSE(VB_PLAYBACK, LOC + "StartRecorder(): took "<<t.elapsed()
            <<" ms to start recorder.");

    return true;
}

/** \fn TV::StopStuff(PlayerContext*,PlayerContext*, bool, bool, bool)
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
void TV::StopStuff(PlayerContext *mctx, PlayerContext *ctx,
                   bool stopRingBuffer, bool stopPlayer, bool stopRecorder)
{
    VERBOSE(VB_PLAYBACK,
            LOC + QString("StopStuff() for player ctx %1 -- begin")
            .arg(find_player_index(ctx)));

    SetActive(mctx, 0, false);

    if (ctx->buffer && ctx->buffer->isDVD())
    {
        VERBOSE(VB_PLAYBACK,LOC + " StopStuff() -- "
                "get dvd player out of still frame or wait status");
        ctx->buffer->DVD()->IgnoreStillOrWait(true);
    }

    ctx->LockDeleteNVP(__FILE__, __LINE__);
    if (stopPlayer && ctx->nvp)
        ctx->nvp->StopPlaying();
    ctx->UnlockDeleteNVP(__FILE__, __LINE__);

    if (stopRingBuffer)
    {
        VERBOSE(VB_PLAYBACK, LOC + "StopStuff(): stopping ring buffer");
        if (ctx->buffer)
        {
            ctx->buffer->StopReads();
            ctx->buffer->Pause();
            ctx->buffer->WaitForPause();
        }
    }

    if (stopPlayer)
    {
        VERBOSE(VB_PLAYBACK, LOC + "StopStuff(): stopping player");
        if (ctx == mctx)
        {
            for (uint i = 1; mctx && (i < player.size()); i++)
                StopStuff(mctx, GetPlayer(mctx,i), true, true, true);
        }
    }

    if (stopRecorder)
    {
        VERBOSE(VB_PLAYBACK, LOC + "StopStuff(): stopping recorder");
        if (ctx->recorder)
            ctx->recorder->StopLiveTV();
    }

    VERBOSE(VB_PLAYBACK, LOC + "StopStuff() -- end");
}

void TV::TeardownPlayer(PlayerContext *mctx, PlayerContext *ctx)
{
    int ctx_index = find_player_index(ctx);

    QString loc = LOC + QString("TeardownPlayer() player ctx %1")
        .arg(ctx_index);

    if (!mctx || !ctx || ctx_index < 0)
    {
        VERBOSE(VB_IMPORTANT, loc + "-- error");
        return;
    }

    VERBOSE(VB_PLAYBACK, loc);

    if (mctx != ctx)
    {
        if (ctx->HasNVP())
        {
            PIPRemovePlayer(mctx, ctx);
            ctx->SetNVP(NULL);
        }

        player.erase(player.begin() + ctx_index);
        delete ctx;
        if (mctx->IsPBP())
            PBPRestartMainNVP(mctx);
        SetActive(mctx, playerActive, false);
        return;
    }

    ctx->TeardownPlayer();

    if ((mctx == ctx) && udpnotify)
    {
        udpnotify->deleteLater();
        udpnotify = NULL;
    }
}

void TV::run(void)
{
    PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
    mctx->paused = false;
    mctx->ff_rew_state = 0;
    mctx->ff_rew_index = kInitFFRWSpeed;
    mctx->ff_rew_speed = 0;
    mctx->ts_normal    = 1.0f;
    ReturnPlayerLock(mctx);

    sleep_index = 0;

    SetUpdateOSDPosition(false);

    const PlayerContext *ctx = GetPlayerReadLock(0, __FILE__, __LINE__);
    ClearInputQueues(ctx, false);
    ReturnPlayerLock(ctx);

    switchToRec = NULL;
    SetExitPlayer(false, false);

    mainLoopCondLock.lock();
    mainLoopCond.wakeAll();
    mainLoopCondLock.unlock();

    exec();

    mctx = GetPlayerWriteLock(0, __FILE__, __LINE__);
    if (!mctx->IsErrored() && (GetState(mctx) != kState_None))
    {
        mctx->ForceNextStateNone();
        HandleStateChange(mctx, mctx);
        if (jumpToProgram)
            TeardownPlayer(mctx, mctx);
    }
    ReturnPlayerLock(mctx);
}

void TV::timerEvent(QTimerEvent *te)
{
    const int timer_id = te->timerId();

    PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
    if (mctx->IsErrored())
    {
        ReturnPlayerLock(mctx);
        QThread::exit(1);
        return;
    }
    ReturnPlayerLock(mctx);

    bool ignore = false;
    {
        QMutexLocker locker(&timerIdLock);
        ignore =
            (stateChangeTimerId.size() &&
             stateChangeTimerId.find(timer_id) == stateChangeTimerId.end());
    }
    if (ignore)
        return; // Always handle state changes first...

    bool handled = true;
    if (timer_id == lcdTimerId)
        HandleLCDTimerEvent();
    else if (timer_id == sleepTimerId)
        SleepDialogCreate();
    else if (timer_id == sleepDialogTimerId)
        SleepDialogTimeout();
    else if (timer_id == idleTimerId)
        IdleDialogCreate();
    else if (timer_id == idleDialogTimerId)
        IdleDialogTimeout();
    else if (timer_id == endOfPlaybackTimerId)
        HandleEndOfPlaybackTimerEvent();
    else if (timer_id == embedCheckTimerId)
        HandleIsNearEndWhenEmbeddingTimerEvent();
    else if (timer_id == endOfRecPromptTimerId)
        HandleEndOfRecordingExitPromptTimerEvent();
    else if (timer_id == videoExitDialogTimerId)
        HandleVideoExitDialogTimerEvent();
    else if (timer_id == pseudoChangeChanTimerId)
        HandlePseudoLiveTVTimerEvent();
    else if (timer_id == speedChangeTimerId)
        HandleSpeedChangeTimerEvent();
    else if (timer_id == pipChangeTimerId)
        HandlePxPTimerEvent();
    else
        handled = false;

    if (handled)
        return;

    // Check if it matches a stateChangeTimerId
    PlayerContext *ctx = NULL;
    {
        QMutexLocker locker(&timerIdLock);
        TimerContextMap::iterator it = stateChangeTimerId.find(timer_id);
        if (it != stateChangeTimerId.end())
        {
            KillTimer(timer_id);
            ctx = *it;
            stateChangeTimerId.erase(it);
        }
    }

    if (ctx)
    {
        PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
        bool still_exists = find_player_index(ctx) >= 0;

        while (still_exists && !ctx->nextState.empty())
        {
            HandleStateChange(mctx, ctx);
            if ((kState_None  == ctx->GetState() ||
                 kState_Error == ctx->GetState()) &&
                ((mctx != ctx) || jumpToProgram))
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

#ifdef PLAY_FROM_RECORDER
    // Check if it matches a recorderPlaybackInfoTimerId
    bool do_pbinfo_fetch = false;
    {
        QMutexLocker locker(&recorderPlaybackInfoLock);
        QMap<int,int>::iterator it = recorderPlaybackInfoTimerId.find(timer_id);
        if (it != recorderPlaybackInfoTimerId.end())
        {
            KillTimer(timer_id);
            recorderPlaybackInfoTimerId.erase(it);
            do_pbinfo_fetch = true;
        }
    }

    if (do_pbinfo_fetch)
    {
        PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
        mctx->recorder->Setup();

        ProgramInfo *pbinfo = NULL;
        bool ok = true;
        if (mctx->recorder->IsRecording(&ok))
        {
            pbinfo = mctx->recorder->GetRecording();
            if (pbinfo)
                RemoteFillProgramInfo(*pbinfo, gCoreContext->GetHostName());
        }
        if (!ok || !pbinfo)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "lost contact with backend");
            SetErrored(ctx);
        }

        ReturnPlayerLock(mctx);

        QMutexLocker locker(&recorderPlaybackInfoLock);
        if (pbinfo)
        {
            recorderPlaybackInfo[timer_id] = *pbinfo;
            delete pbinfo;
        }
        recorderPlaybackInfo[timer_id] = ProgramInfo();
        recorderPlaybackInfoWaitCond.wakeAll();
        handled = true;
    }
#endif // PLAY_FROM_RECORDER

    if (handled)
        return;

    // Check if it matches a signalMonitorTimerId
    ctx = NULL;
    {
        QMutexLocker locker(&timerIdLock);
        TimerContextMap::iterator it = signalMonitorTimerId.find(timer_id);
        if (it != signalMonitorTimerId.end())
        {
            KillTimer(timer_id);
            ctx = *it;
            signalMonitorTimerId.erase(it);
        }
    }

    if (ctx)
    {
        PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
        bool still_exists = find_player_index(ctx) >= 0;

        if (still_exists && !ctx->lastSignalMsg.empty())
        {   // set last signal msg, so we get some feedback...
            UpdateOSDSignal(ctx, ctx->lastSignalMsg);
            ctx->lastSignalMsg.clear();
        }
        UpdateOSDTimeoutMessage(ctx);

        ReturnPlayerLock(mctx);
        handled = true;
    }

    if (handled)
        return;

    // Check if it matches a tvchainUpdateTimerId
    ctx = NULL;
    {
        QMutexLocker locker(&timerIdLock);
        TimerContextMap::iterator it = tvchainUpdateTimerId.find(timer_id);
        if (it != tvchainUpdateTimerId.end())
        {
            KillTimer(timer_id);
            ctx = *it;
            tvchainUpdateTimerId.erase(it);
        }
    }

    if (ctx)
    {
        PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
        bool still_exists = find_player_index(ctx) >= 0;

        if (still_exists)
            ctx->UpdateTVChain();

        ReturnPlayerLock(mctx);
        handled = true;
    }

    if (handled)
        return;

    // Check if it matches keyListTimerId
    QKeyEvent *keyEvent = NULL;
    {
        QMutexLocker locker(&timerIdLock);

        if (timer_id == keyListTimerId)
        {
            keyEvent = keyList.dequeue();
            if (keyList.empty())
            {
                KillTimer(keyListTimerId);
                keyListTimerId = 0;
            }
        }
    }

    if (keyEvent)
    {
        PlayerContext *actx = GetPlayerWriteLock(-1, __FILE__, __LINE__);
        if (actx->HasNVP())
        {
            ProcessKeypress(actx, keyEvent);

            delete keyEvent;
        }
        else
        {
            VERBOSE(VB_IMPORTANT,
                    "Ignoring key event for now because nvp is not set");

            QMutexLocker locker(&timerIdLock);
            keyList.push_front(keyEvent);
            if (keyListTimerId)
                KillTimer(keyListTimerId);
            keyListTimerId = StartTimer(20, __LINE__);
        }
        ReturnPlayerLock(actx);
        handled = true;
    }

    if (handled)
        return;

    // Check if it matches networkControlTimerId
    QString netCmd = QString::null;
    {
        QMutexLocker locker(&timerIdLock);
        if (timer_id == networkControlTimerId)
        {
            if (networkControlCommands.size())
                netCmd = networkControlCommands.dequeue();
            if (networkControlCommands.empty())
            {
                KillTimer(networkControlTimerId);
                networkControlTimerId = 0;
            }
        }
    }

    if (!netCmd.isEmpty())
    {
        PlayerContext *actx = GetPlayerWriteLock(-1, __FILE__, __LINE__);
        ProcessNetworkControlCommand(actx, netCmd);
        ReturnPlayerLock(actx);
        handled = true;
    }

    if (handled)
        return;

    // Check if it matches exitPlayerTimerId
    if (timer_id == exitPlayerTimerId)
    {
        PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);

        OSD *osd = GetOSDLock(mctx);
        if (osd)
            osd->DialogAbortAndHideAll();
        ReturnOSDLock(mctx, osd);

        if (jumpToProgram && lastProgram)
        {
            if (!lastProgram->IsFileReadable())
            {
                OSD *osd = GetOSDLock(mctx);
                if (osd)
                {
                    QString msg = tr("Last Program: %1 Doesn't Exist")
                                    .arg(lastProgram->GetTitle());
                    osd->SetSettingsText(msg, 3);
                }
                ReturnOSDLock(mctx, osd);
                lastProgramStringList.clear();
                SetLastProgram(NULL);
                VERBOSE(VB_PLAYBACK, LOC_ERR +
                            "Last Program File does not exist");
                jumpToProgram = false;
            }
            else
                ForceNextStateNone(mctx);
        }
        else
            ForceNextStateNone(mctx);

        ReturnPlayerLock(mctx);

        QMutexLocker locker(&timerIdLock);
        KillTimer(exitPlayerTimerId);
        exitPlayerTimerId = 0;
        handled = true;
    }

    if (handled)
        return;

    if (timer_id == jumpMenuTimerId)
    {
        DoDisplayJumpMenu();

        QMutexLocker locker(&timerIdLock);
        KillTimer(jumpMenuTimerId);
        jumpMenuTimerId = 0;
        handled = true;
    }

    if (handled)
        return;

    if (timer_id == udpNotifyTimerId)
    {
        while (HasUDPNotifyEvent())
            HandleUDPNotifyEvent();

        QMutexLocker locker(&timerIdLock);
        KillTimer(udpNotifyTimerId);
        udpNotifyTimerId = 0;
        handled = true;
    }

    if (handled)
        return;

    if (timer_id == switchToInputTimerId)
    {
        PlayerContext *actx = GetPlayerReadLock(-1, __FILE__, __LINE__);
        if (switchToInputId)
        {
            uint tmp = switchToInputId;
            switchToInputId = 0;
            SwitchInputs(actx, tmp);
        }
        ReturnPlayerLock(actx);

        QMutexLocker locker(&timerIdLock);
        KillTimer(switchToInputTimerId);
        switchToInputTimerId = 0;
        handled = true;
    }

    if (handled)
        return;

    if (timer_id == ccInputTimerId)
    {
        PlayerContext *actx = GetPlayerReadLock(-1, __FILE__, __LINE__);
        // Clear closed caption input mode when timer expires
        if (ccInputMode)
        {
            ccInputMode = false;
            ClearInputQueues(actx, true);
        }
        ReturnPlayerLock(actx);

        QMutexLocker locker(&timerIdLock);
        KillTimer(ccInputTimerId);
        ccInputTimerId = 0;
        handled = true;
    }

    if (handled)
        return;

    if (timer_id == asInputTimerId)
    {
        PlayerContext *actx = GetPlayerReadLock(-1, __FILE__, __LINE__);
        // Clear closed caption input mode when timer expires
        if (asInputMode)
        {
            asInputMode = false;
            ClearInputQueues(actx, true);
        }
        ReturnPlayerLock(actx);

        QMutexLocker locker(&timerIdLock);
        KillTimer(asInputTimerId);
        asInputTimerId = 0;
        handled = true;
    }

    if (handled)
        return;

    if (timer_id == queueInputTimerId)
    {
        PlayerContext *actx = GetPlayerReadLock(-1, __FILE__, __LINE__);
        // Commit input when the OSD fades away
        if (HasQueuedChannel())
        {
            OSD *osd = GetOSDLock(actx);
            if (osd)
            {
                OSDSet *oset = osd->GetSet("channel_number");
                if ((oset && !oset->Displaying()) || !oset)
                    CommitQueuedInput(actx);
            }
            ReturnOSDLock(actx, osd);
        }
        ReturnPlayerLock(actx);

        QMutexLocker locker(&timerIdLock);
        if (!queuedChanID && queuedChanNum.isEmpty() && queueInputTimerId)
        {
            KillTimer(queueInputTimerId);
            queueInputTimerId = 0;
        }
        handled = true;
    }

    if (handled)
        return;

    if (timer_id == browseTimerId)
    {
        PlayerContext *actx = GetPlayerReadLock(-1, __FILE__, __LINE__);
        BrowseEnd(actx, false);
        ReturnPlayerLock(actx);
        handled = true;
    }

    if (handled)
        return;

    // Check unmute..
    ctx = NULL;
    {
        QMutexLocker locker(&timerIdLock);
        TimerContextMap::iterator it = unmuteTimerId.find(timer_id);
        if (it != unmuteTimerId.end())
        {
            KillTimer(timer_id);
            ctx = *it;
            unmuteTimerId.erase(it);
        }
    }

    if (ctx)
    {
        PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
        if (find_player_index(ctx) >= 0)
        {
            ctx->LockDeleteNVP(__FILE__, __LINE__);
            if (ctx->nvp && ctx->nvp->IsMuted())
                ctx->nvp->SetMuted(false);
            ctx->UnlockDeleteNVP(__FILE__, __LINE__);
        }
        ReturnPlayerLock(mctx);
        handled = true;
    }

    if (handled)
        return;

    if (timer_id == updateOSDPosTimerId)
    {
        PlayerContext *actx = GetPlayerReadLock(-1, __FILE__, __LINE__);
        OSD *osd = GetOSDLock(actx);
        OSDSet *osdset = NULL;
        if (osd && (osdset = osd->GetSet("status")) &&
            osdset->Displaying() &&
            (StateIsLiveTV(actx->GetState()) ||
             StateIsPlaying(actx->GetState())))
        {
            ReturnOSDLock(actx, osd);
            struct StatusPosInfo posInfo;
            if (actx->CalcNVPSliderPosition(posInfo))
            {
                OSD *osd = GetOSDLock(actx);
                if (osd)
                    osd->UpdateStatus(posInfo);
                ReturnOSDLock(actx, osd);
            }
        }
        else
        {
            ReturnOSDLock(actx, osd);
        }
        ReturnPlayerLock(actx);
        handled = true;
    }

    if (handled)
        return;

    if (timer_id == errorRecoveryTimerId)
    {
        bool error = false;
        PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);

        if (mctx->IsNVPErrored())
        {
            if (mctx->IsNVPRecoverable())
                RestartMainNVP(mctx);

            if (mctx->IsNVPDecoderErrored())
            {
                VERBOSE(VB_IMPORTANT, LOC +
                    QString("Serious hardware decoder error detected. "
                            "Disabling hardware decoders."));
                noHardwareDecoders = true;
                for (uint i = 0; i < player.size(); i++)
                    player[i]->SetNoHardwareDecoders();
                RestartMainNVP(mctx);
            }
        }

        if (mctx->IsRecorderErrored() ||
            mctx->IsNVPErrored() ||
            mctx->IsErrored())
        {
            SetExitPlayer(true, false);
            ForceNextStateNone(mctx);
            error = true;
        }

        for (uint i = 0; i < player.size(); i++)
        {
            PlayerContext *ctx = GetPlayer(mctx, i);
            if (error || ctx->IsErrored())
                ForceNextStateNone(ctx);
        }
        ReturnPlayerLock(mctx);

        QMutexLocker locker(&timerIdLock);
        if (errorRecoveryTimerId)
            KillTimer(errorRecoveryTimerId);
        errorRecoveryTimerId =
            StartTimer(kErrorRecoveryCheckFrequency, __LINE__);
    }
}

bool TV::HandlePxPTimerEvent(void)
{
    QString cmd = QString::null;

    {
        QMutexLocker locker(&timerIdLock);
        if (changePxP.empty())
        {
            if (pipChangeTimerId)
                KillTimer(pipChangeTimerId);
            pipChangeTimerId = 0;
            return true;
        }
        cmd = changePxP.dequeue();
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
        else if (mctx && player.size() == 2)
            PxPSwap(mctx, GetPlayer(mctx,1));
    }
    else if (cmd == "TOGGLEPIPSTATE")
        PxPToggleType(mctx, !mctx->IsPBP());

    ReturnPlayerLock(mctx);

    QMutexLocker locker(&timerIdLock);

    if (pipChangeTimerId)
        KillTimer(pipChangeTimerId);

    if (changePxP.empty())
        pipChangeTimerId = 0;
    else
        pipChangeTimerId = StartTimer(20, __LINE__);

    return true;
}

bool TV::HandleLCDTimerEvent(void)
{
    PlayerContext *actx = GetPlayerReadLock(-1, __FILE__, __LINE__);
    LCD *lcd = LCD::Get();
    if (lcd)
    {
        float progress = 0.0f;
        bool showProgress = true;

        if (StateIsLiveTV(GetState(actx)))
            ShowLCDChannelInfo(actx);

        if (actx->buffer && actx->buffer->isDVD())
        {
            ShowLCDDVDInfo(actx);
            showProgress = !actx->buffer->InDVDMenuOrStillFrame();
        }

        if (showProgress)
        {
            struct StatusPosInfo posInfo;
            if (actx->CalcNVPSliderPosition(posInfo))
                progress = posInfo.position * 0.001f;
        }
        lcd->setChannelProgress(progress);
    }
    ReturnPlayerLock(actx);

    QMutexLocker locker(&timerIdLock);
    KillTimer(lcdTimerId);
    lcdTimerId = StartTimer(kLCDTimeout, __LINE__);

    return true;
}

int  TV::StartTimer(int interval, int line)
{
    int x = QObject::startTimer(interval);
    if (!x)
    {
        VERBOSE(VB_IMPORTANT,
                LOC_ERR + QString("Failed to start timer on line %1 of %2")
                .arg(line).arg(__FILE__));
    }
    return x;
}

#include <cassert>
void TV::KillTimer(int id)
{
    assert(id);
    QObject::killTimer(id);
}

void TV::ForceNextStateNone(PlayerContext *ctx)
{
    ctx->ForceNextStateNone();
    ScheduleStateChange(ctx);
}

void TV::ScheduleStateChange(PlayerContext *ctx)
{
    QMutexLocker locker(&timerIdLock);
    stateChangeTimerId[StartTimer(1, __LINE__)] = ctx;
}

void TV::SetErrored(PlayerContext *ctx)
{
    if (!ctx)
        return;
    QMutexLocker locker(&timerIdLock);
    ctx->errored = true;
    KillTimer(errorRecoveryTimerId);
    errorRecoveryTimerId = StartTimer(1, __LINE__);
}

void TV::PrepToSwitchToRecordedProgram(PlayerContext *ctx,
                                       const ProgramInfo &p)
{
    VERBOSE(VB_GENERAL, LOC + QString("Switching to program: %1")
            .arg(p.toString(ProgramInfo::kTitleSubtitle)));
    SetLastProgram(&p);
    PrepareToExitPlayer(ctx,__LINE__);
    jumpToProgram = true;
    SetExitPlayer(true, true);
}

void TV::PrepareToExitPlayer(PlayerContext *ctx, int line, bool bookmark) const
{
    bool bookmark_it = bookmark && IsBookmarkAllowed(ctx);
    ctx->LockDeleteNVP(__FILE__, line);
    if (ctx->nvp)
    {
        if (bookmark_it && !(ctx->nvp->IsNearEnd()))
            ctx->nvp->SetBookmark();
        if (db_auto_set_watched)
            ctx->nvp->SetWatched();
    }
    ctx->UnlockDeleteNVP(__FILE__, line);
}

void TV::SetExitPlayer(bool set_it, bool wants_to) const
{
    QMutexLocker locker(&timerIdLock);
    if (set_it)
    {
        wantsToQuit = wants_to;
        if (!exitPlayerTimerId)
            exitPlayerTimerId = ((TV*)this)->StartTimer(1, __LINE__);
    }
    else
    {
        if (exitPlayerTimerId)
            ((TV*)this)->KillTimer(exitPlayerTimerId);
        exitPlayerTimerId = 0;
        wantsToQuit = wants_to;
    }
}

void TV::SetUpdateOSDPosition(bool set_it)
{
    QMutexLocker locker(&timerIdLock);
    if (set_it)
    {
        if (!updateOSDPosTimerId)
            updateOSDPosTimerId = StartTimer(500, __LINE__);
    }
    else
    {
        if (updateOSDPosTimerId)
            KillTimer(updateOSDPosTimerId);
        updateOSDPosTimerId = 0;
    }
}

void TV::HandleEndOfPlaybackTimerEvent(void)
{
    {
        QMutexLocker locker(&timerIdLock);
        if (endOfPlaybackTimerId)
            KillTimer(endOfPlaybackTimerId);
        endOfPlaybackTimerId = 0;
    }

    bool is_playing = false;
    PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
    for (uint i = 0; mctx && (i < player.size()); i++)
    {
        PlayerContext *ctx = GetPlayer(mctx, i);
        if (!StateIsPlaying(ctx->GetState()))
            continue;

        if (ctx->IsNVPPlaying())
        {
            is_playing = true;
            continue;
        }

        ForceNextStateNone(ctx);
        if (mctx == ctx)
        {
            endOfRecording = true;
            PrepareToExitPlayer(mctx, __LINE__, false);
            SetExitPlayer(true, true);
        }
    }
    ReturnPlayerLock(mctx);

    if (is_playing)
    {
        QMutexLocker locker(&timerIdLock);
        endOfPlaybackTimerId =
            StartTimer(kEndOfPlaybackCheckFrequency, __LINE__);
    }
}

void TV::HandleIsNearEndWhenEmbeddingTimerEvent(void)
{
    PlayerContext *actx = GetPlayerReadLock(-1, __FILE__, __LINE__);

    bool toggle = !actx->paused && !StateIsLiveTV(GetState(actx));

    if (toggle)
    {
        actx->LockDeleteNVP(__FILE__, __LINE__);

        toggle = actx->nvp && actx->nvp->IsEmbedding()
                           && actx->nvp->IsNearEnd();

        actx->UnlockDeleteNVP(__FILE__, __LINE__);

        if (toggle)
            DoTogglePause(actx, true);
    }
    ReturnPlayerLock(actx);
}

void TV::HandleEndOfRecordingExitPromptTimerEvent(void)
{
    if (endOfRecording || inPlaylist || editmode || underNetworkControl ||
        exitPlayerTimerId)
    {
        return;
    }

    PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
    OSD *osd = GetOSDLock(mctx);
    if (osd && !osd->GetDialogActive().isEmpty())
    {
        ReturnOSDLock(mctx, osd);
        ReturnPlayerLock(mctx);
        return;
    }
    ReturnOSDLock(mctx, osd);

    bool do_prompt = false;
    mctx->LockDeleteNVP(__FILE__, __LINE__);
    if (mctx->GetState() == kState_WatchingPreRecorded && mctx->nvp)
    {
        if (!mctx->nvp->IsNearEnd())
            jumped_back = false;

        do_prompt = mctx->nvp->IsNearEnd() && !jumped_back &&
            !mctx->nvp->IsEmbedding() && !mctx->paused;
    }
    mctx->UnlockDeleteNVP(__FILE__, __LINE__);

    if (do_prompt)
        PromptDeleteRecording(mctx, tr("End Of Recording"));

    ReturnPlayerLock(mctx);
}

void TV::HandleVideoExitDialogTimerEvent(void)
{
    {
        QMutexLocker locker(&timerIdLock);
        if (videoExitDialogTimerId)
            KillTimer(videoExitDialogTimerId);
        videoExitDialogTimerId = 0;
    }

    // disable dialog and exit playback after timeout
    PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
    OSD *osd = GetOSDLock(mctx);
    if (!osd || !IsVideoExitDialog(osd->GetDialogActive()))
    {
        ReturnOSDLock(mctx, osd);
        ReturnPlayerLock(mctx);
        return;
    }
    if (osd)
        osd->TurnDialogOff(kOSDDialogActive);
    ReturnOSDLock(mctx, osd);
    DoTogglePause(mctx, true);
    ClearOSD(mctx);
    PrepareToExitPlayer(mctx, __LINE__);
    ReturnPlayerLock(mctx);

    requestDelete = false;
    SetExitPlayer(true, true);
}

void TV::HandlePseudoLiveTVTimerEvent(void)
{
    {
        QMutexLocker locker(&timerIdLock);
        KillTimer(pseudoChangeChanTimerId);
        pseudoChangeChanTimerId = 0;
    }

    bool restartTimer = false;
    PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
    for (uint i = 0; mctx && (i < player.size()); i++)
    {
        PlayerContext *ctx = GetPlayer(mctx, i);
        if (kPseudoChangeChannel != ctx->pseudoLiveTVState)
            continue;

        if (ctx->InStateChange())
        {
            restartTimer = true;
            continue;
        }

        VERBOSE(VB_PLAYBACK, "REC_PROGRAM -- channel change "<<i);

        uint        chanid  = ctx->pseudoLiveTVRec->GetChanID();
        QString     channum = ctx->pseudoLiveTVRec->GetChanNum();
        StringDeque tmp     = ctx->prevChan;

        ctx->prevChan.clear();
        ChangeChannel(ctx, chanid, channum);
        ctx->prevChan = tmp;
        ctx->pseudoLiveTVState = kPseudoRecording;
    }
    ReturnPlayerLock(mctx);

    if (restartTimer)
    {
        QMutexLocker locker(&timerIdLock);
        if (!pseudoChangeChanTimerId)
            pseudoChangeChanTimerId = StartTimer(25, __LINE__);
    }
}

void TV::SetSpeedChangeTimer(uint when, int line)
{
    QMutexLocker locker(&timerIdLock);
    if (speedChangeTimerId)
        KillTimer(speedChangeTimerId);
    speedChangeTimerId = StartTimer(when, line);
}

void TV::HandleSpeedChangeTimerEvent(void)
{
    {
        QMutexLocker locker(&timerIdLock);
        if (speedChangeTimerId)
            KillTimer(speedChangeTimerId);
        speedChangeTimerId = StartTimer(kSpeedChangeCheckFrequency, __LINE__);
    }

    bool update_msg = false;
    PlayerContext *actx = GetPlayerReadLock(-1, __FILE__, __LINE__);
    for (uint i = 0; actx && (i < player.size()); i++)
    {
        PlayerContext *ctx = GetPlayer(actx, i);
        update_msg |= ctx->HandleNVPSpeedChangeFFRew() && (ctx == actx);
    }
    ReturnPlayerLock(actx);

    actx = GetPlayerReadLock(-1, __FILE__, __LINE__);
    for (uint i = 0; actx && (i < player.size()); i++)
    {
        PlayerContext *ctx = GetPlayer(actx, i);
        update_msg |= ctx->HandleNVPSpeedChangeEOF() && (ctx == actx);
    }

    if (update_msg)
    {
        UpdateOSDSeekMessage(actx, actx->GetPlayMessage(),
                             osd_general_timeout);
    }
    ReturnPlayerLock(actx);
}

bool TV::eventFilter(QObject *o, QEvent *e)
{
    const MythMainWindow *mainWindow = GetMythMainWindow();
    if (mainWindow == o)
    {
        if (e->type() == QEvent::Resize)
        {
            PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
            mctx->LockDeleteNVP(__FILE__, __LINE__);
            if (mctx->nvp)
                mctx->nvp->WindowResized(((const QResizeEvent*) e)->size());
            mctx->UnlockDeleteNVP(__FILE__, __LINE__);
            ReturnPlayerLock(mctx);
        }
    }

    if (e->type() == MythEvent::MythEventMessage)
    {
        customEvent(e);
        return true;
    }

    switch (e->type())
    {
        case QEvent::KeyPress:
        {
            // we ignore keypresses if the epg is running for example
            if (!ignoreKeyPresses)
            {
                QKeyEvent *k = new QKeyEvent(*(QKeyEvent *)e);
                QMutexLocker locker(&timerIdLock);
                keyList.enqueue(k);
                if (!keyListTimerId)
                    keyListTimerId = StartTimer(1, __LINE__);

                return true;
             }
             else
             {
                return false;
             }
        }

        case QEvent::Paint:
        case QEvent::UpdateRequest:
        case QEvent::Enter:
        {
            DrawUnusedRects();
            return false;
        }

        default:
            return false;
    }
}

bool TV::HandleTrackAction(PlayerContext *ctx, const QString &action)
{
    ctx->LockDeleteNVP(__FILE__, __LINE__);
    if (!ctx->nvp)
    {
        ctx->UnlockDeleteNVP(__FILE__, __LINE__);
        return false;
    }

    bool handled = false;

    if (action == "TOGGLECC" && !browsemode)
    {
        handled = true;
        if (ccInputMode)
        {
            bool valid = false;
            int page = GetQueuedInputAsInt(&valid, 16);
            if (vbimode == VBIMode::PAL_TT && valid)
                ctx->nvp->SetTeletextPage(page);
            else if (vbimode == VBIMode::NTSC_CC)
                ctx->nvp->SetTrack(kTrackTypeCC608,
                                   max(min(page - 1, 1), 0));

            ClearInputQueues(ctx, true);

            QMutexLocker locker(&timerIdLock);
            ccInputMode = false;
            if (ccInputTimerId)
            {
                KillTimer(ccInputTimerId);
                ccInputTimerId = 0;
            }
        }
        else if (ctx->nvp->GetCaptionMode() & kDisplayNUVTeletextCaptions)
        {
            ClearInputQueues(ctx, false);
            AddKeyToInputQueue(ctx, 0);

            QMutexLocker locker(&timerIdLock);
            ccInputMode        = true;
            asInputMode        = false;
            ccInputTimerId = StartTimer(kInputModeTimeout, __LINE__);
            if (asInputTimerId)
            {
                KillTimer(asInputTimerId);
                asInputTimerId = 0;
            }
        }
        else
        {
            ctx->nvp->ToggleCaptions();
        }
    }
    else if (action.left(6) == "TOGGLE")
    {
        int type = to_track_type(action.mid(6));
        if (type == kTrackTypeTeletextMenu)
        {
            handled = true;
            ctx->nvp->EnableTeletext();
        }
        else if (type >= kTrackTypeSubtitle)
        {
            handled = true;
            ctx->nvp->ToggleCaptions(type);
        }
    }
    else if (action.left(6) == "SELECT")
    {
        int type = to_track_type(action.mid(6));
        int mid  = (kTrackTypeSubtitle == type) ? 15 : 12;
        if (type >= kTrackTypeAudio)
        {
            handled = true;
            ctx->nvp->SetTrack(type, action.mid(mid).toInt());
        }
    }
    else if (action.left(4) == "NEXT" || action.left(4) == "PREV")
    {
        int dir = (action.left(4) == "NEXT") ? +1 : -1;
        int type = to_track_type(action.mid(4));
        if (type >= kTrackTypeAudio)
        {
            handled = true;
            ctx->nvp->ChangeTrack(type, dir);
        }
        else if (action.right(2) == "CC")
        {
            handled = true;
            ctx->nvp->ChangeCaptionTrack(dir);
        }
    }

    ctx->UnlockDeleteNVP(__FILE__, __LINE__);

    return handled;
}

static bool has_action(QString action, const QStringList &actions)
{
    QStringList::const_iterator it;
    for (it = actions.begin(); it != actions.end(); ++it)
    {
        if (action == *it)
            return true;
    }
    return false;
}

void TV::ProcessKeypress(PlayerContext *actx, QKeyEvent *e)
{
    bool ignoreKeys = actx->IsNVPChangingBuffers();
#if DEBUG_ACTIONS
    VERBOSE(VB_IMPORTANT, LOC + QString("ProcessKeypress() ignoreKeys: %1")
            .arg(ignoreKeys));
#endif // DEBUG_ACTIONS

    if (idleTimerId)
    {
        KillTimer(idleTimerId);
        idleTimerId = StartTimer(db_idle_timeout, __LINE__);
    }

    QStringList actions;
    bool handled = false;

    if (ignoreKeys)
    {
        handled = GetMythMainWindow()->TranslateKeyPress(
                  "TV Playback", e, actions);

        if (handled || actions.isEmpty())
            return;

        bool esc   = has_action("ESCAPE", actions);
        bool pause = has_action("PAUSE",  actions);
        bool play  = has_action("PLAY",   actions);

        if ((!esc || browsemode) && !pause && !play)
            return;
    }

    if (editmode)
    {
        actx->LockDeleteNVP(__FILE__, __LINE__);
        if (actx->nvp && !actx->nvp->DoKeypress(e))
            editmode = actx->nvp->GetEditMode();
        actx->UnlockDeleteNVP(__FILE__, __LINE__);

        if (!editmode)
        {
            actx->paused = !actx->paused;
            DoTogglePause(actx, true);
        }

        return;
    }

    OSD *osd = GetOSDLock(actx);
    if (osd && osd->IsRunningTreeMenu())
    {
        osd->TreeMenuHandleKeypress(e);
        ReturnOSDLock(actx, osd);
        return;
    }
    ReturnOSDLock(actx, osd);

    // If text is already queued up, be more lax on what is ok.
    // This allows hex teletext entry and minor channel entry.
    const QString txt = e->text();
    if (HasQueuedInput() && (1 == txt.length()))
    {
        bool ok = false;
        txt.toInt(&ok, 16);
        if (ok || txt=="_" || txt=="-" || txt=="#" || txt==".")
        {
            AddKeyToInputQueue(actx, txt.at(0).toLatin1());
            return;
        }
    }

    osd = GetOSDLock(actx);
    if (osd && osd->IsDialogActive("channel_editor"))
    {
        ChannelEditKey(actx, e);
        ReturnOSDLock(actx, osd);
        return;
    }
    ReturnOSDLock(actx, osd);

    // Teletext menu
    actx->LockDeleteNVP(__FILE__, __LINE__);
    if (actx->nvp && (actx->nvp->GetCaptionMode() == kDisplayTeletextMenu))
    {
        QStringList tt_actions;
        handled = GetMythMainWindow()->TranslateKeyPress(
                  "Teletext Menu", e, tt_actions);

        if (!handled && !tt_actions.isEmpty())
        {
            for (int i = 0; i < tt_actions.size(); i++)
            {
                if (actx->nvp->HandleTeletextAction(tt_actions[i]))
                {
                    actx->UnlockDeleteNVP(__FILE__, __LINE__);
                    return;
                }
            }
        }
    }

    // Interactive television
    if (actx->nvp && actx->nvp->GetInteractiveTV())
    {
        QStringList itv_actions;
        handled = GetMythMainWindow()->TranslateKeyPress(
                  "TV Playback", e, itv_actions);

        if (!handled && !itv_actions.isEmpty())
        {
            for (int i = 0; i < itv_actions.size(); i++)
            {
                if (actx->nvp->ITVHandleAction(itv_actions[i]))
                {
                    actx->UnlockDeleteNVP(__FILE__, __LINE__);
                    return;
                }
            }
        }
    }
    actx->UnlockDeleteNVP(__FILE__, __LINE__);

    handled = GetMythMainWindow()->TranslateKeyPress(
              "TV Playback", e, actions);

    if (handled || actions.isEmpty())
        return;

    handled = false;

    bool isDVD = actx->buffer && actx->buffer->isDVD();
    bool isDVDStill = isDVD && actx->buffer->InDVDMenuOrStillFrame();

    handled = handled || OSDDialogHandleAction(actx, actions);
    handled = handled || BrowseHandleAction(actx, actions);
    handled = handled || ManualZoomHandleAction(actx, actions);
    handled = handled || PictureAttributeHandleAction(actx, actions);
    handled = handled || TimeStretchHandleAction(actx, actions);
    handled = handled || AudioSyncHandleAction(actx, actions);
    handled = handled || DVDMenuHandleAction(actx, actions, isDVD, isDVDStill);
    handled = handled || ActiveHandleAction(actx, actions, isDVD, isDVDStill);
    handled = handled || ToggleHandleAction(actx, actions, isDVD);
    handled = handled || PxPHandleAction(actx, actions);
    handled = handled || FFRewHandleAction(actx, actions);
    handled = handled || ActivePostQHandleAction(actx, actions, isDVD);

#if DEBUG_ACTIONS
    for (uint i = 0; i < actions.size(); ++i)
        VERBOSE(VB_IMPORTANT, LOC + QString("handled(%1) actions[%2](%3)")
                .arg(handled).arg(i).arg(actions[i]));
#endif // DEBUG_ACTIONS

    if (handled)
        return;

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
}

bool TV::BrowseHandleAction(PlayerContext *ctx, const QStringList &actions)
{
    if (!browsemode)
        return false;

    bool handled = true;

    if (has_action("UP", actions) || has_action("CHANNELUP", actions))
        BrowseDispInfo(ctx, BROWSE_UP);
    else if (has_action("DOWN", actions) || has_action("CHANNELDOWN", actions))
        BrowseDispInfo(ctx, BROWSE_DOWN);
    else if (has_action("LEFT", actions))
        BrowseDispInfo(ctx, BROWSE_LEFT);
    else if (has_action("RIGHT", actions))
        BrowseDispInfo(ctx, BROWSE_RIGHT);
    else if (has_action("NEXTFAV", actions))
        BrowseDispInfo(ctx, BROWSE_FAVORITE);
    else if (has_action("SELECT", actions))
    {
        BrowseEnd(ctx, true);
    }
    else if (has_action("CLEAROSD",     actions) ||
             has_action("ESCAPE",       actions) ||
             has_action("TOGGLEBROWSE", actions))
    {
        BrowseEnd(ctx, false);
    }
    else if (has_action("TOGGLERECORD", actions))
        ToggleRecord(ctx);
    else
    {
        handled = false;
        QStringList::const_iterator it = actions.begin();
        for (; it != actions.end(); ++it)
        {
            if ((*it).length() == 1 && (*it)[0].isDigit())
            {
                AddKeyToInputQueue(ctx, (*it)[0].toLatin1());
                handled = true;
            }
        }
    }

    // only pass-through actions listed below
    return handled ||
        !(has_action("VOLUMEDOWN",      actions) ||
          has_action("VOLUMEUP",        actions) ||
          has_action("STRETCHINC",      actions) ||
          has_action("STRETCHDEC",      actions) ||
          has_action("MUTE",            actions) ||
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
    if (!zoomMode)
        return false;

    actx->LockDeleteNVP(__FILE__, __LINE__);
    if (!actx->nvp)
    {
        actx->UnlockDeleteNVP(__FILE__, __LINE__);
        return false;
    }

    bool end_manual_zoom = false;
    bool handled = true;
    if (has_action("UP", actions) ||
        has_action("CHANNELUP", actions))
    {
        actx->nvp->Zoom(kZoomUp);
    }
    else if (has_action("DOWN", actions) ||
             has_action("CHANNELDOWN", actions))
    {
        actx->nvp->Zoom(kZoomDown);
    }
    else if (has_action("LEFT", actions))
        actx->nvp->Zoom(kZoomLeft);
    else if (has_action("RIGHT", actions))
        actx->nvp->Zoom(kZoomRight);
    else if (has_action("VOLUMEUP", actions))
        actx->nvp->Zoom(kZoomAspectUp);
    else if (has_action("VOLUMEDOWN", actions))
        actx->nvp->Zoom(kZoomAspectDown);
    else if (has_action("ESCAPE", actions))
    {
        actx->nvp->Zoom(kZoomHome);
        end_manual_zoom = true;
    }
    else if (has_action("SELECT", actions))
        SetManualZoom(actx, false, tr("Zoom Committed"));
    else if (has_action("JUMPFFWD", actions))
        actx->nvp->Zoom(kZoomIn);
    else if (has_action("JUMPRWND", actions))
        actx->nvp->Zoom(kZoomOut);
    else
    {
        // only pass-through actions listed below
        handled = !(has_action("STRETCHINC", actions) ||
                    has_action("STRETCHDEC", actions) ||
                    has_action("MUTE",       actions) ||
                    has_action("PAUSE",      actions) ||
                    has_action("CLEAROSD",   actions));
    }
    actx->UnlockDeleteNVP(__FILE__, __LINE__);

    if (end_manual_zoom)
        SetManualZoom(actx, false, tr("Zoom Ignored"));

    return handled;
}

bool TV::OSDDialogHandleAction(PlayerContext *actx, const QStringList &actions)
{
    QString dialogname = QString::null;
    {
        OSD *osd = GetOSDLock(actx);

        if (!osd || !osd->DialogShowing(kOSDDialogActive))
        {
            ReturnOSDLock(actx, osd);
            return false;
        }

        if (has_action("UP", actions))
        {
            osd->DialogUp(kOSDDialogActive);
            ReturnOSDLock(actx, osd);
            return true;
        }
        else if (has_action("DOWN", actions))
        {
            osd->DialogDown(kOSDDialogActive);
            ReturnOSDLock(actx, osd);
            return true;
        }

        dialogname = osd->GetDialogActive();

        ReturnOSDLock(actx, osd);
    }

    bool handled = true;
    actx->LockDeleteNVP(__FILE__, __LINE__);
    bool isnearend = actx->nvp && actx->nvp->IsNearEnd();
    actx->UnlockDeleteNVP(__FILE__, __LINE__);
    if (((has_action("RWNDSTICKY", actions) ||
          has_action("SEEKRWND", actions) ||
          has_action("JUMPRWND", actions))) &&
        IsVideoExitDialog(dialogname) &&
        !has_action("UP", actions) &&
        !has_action("DOWN", actions) &&
        !(has_action("LEFT", actions) && arrowAccel))
    {
        DoTogglePause(actx, false);

        OSD *osd = GetOSDLock(actx);
        if (osd)
            osd->TurnDialogOff(kOSDDialogActive);
        ReturnOSDLock(actx, osd);

        ClearInputQueues(actx, true);
        requestDelete = false;
        jumped_back = true;
        if (has_action("RWNDSTICKY", actions))
            ChangeFFRew(actx, -3);
        else if (has_action("JUMPRWND", actions))
            DoSeek(actx, -actx->jumptime * 60, tr("Jump Back"));
        else
            DoSeek(actx, -actx->rewtime, tr("Skip Back"));
    }
    else if (has_action("ESCAPE", actions) && isnearend)
    {
        requestDelete = false;
        PrepareToExitPlayer(actx, __LINE__, false);
        SetExitPlayer(true, true);
    }
    else if (has_action("SELECT", actions)  ||
             has_action("ESCAPE", actions)   ||
             has_action("CLEAROSD", actions) ||
             (arrowAccel &&
              (has_action("LEFT",  actions) ||
               has_action("RIGHT", actions))))
    {
        dialogname = QString::null;
        int result = -1;
        {
            OSD *osd = GetOSDLock(actx);
            if (osd)
            {
                dialogname = osd->GetDialogActive();
                if (has_action("ESCAPE", actions)   ||
                    has_action("CLEAROSD", actions) ||
                    (arrowAccel && has_action("LEFT", actions)))
                {
                    osd->DialogAbort(dialogname);
                }
                osd->TurnDialogOff(dialogname);
                result = osd->GetDialogResponse(dialogname);
            }
            ReturnOSDLock(actx, osd);
        }

        if (dialogname == kOSDDialogAlreadyEditing)
        {
            if (result == 1)
            {
                actx->LockPlayingInfo(__FILE__, __LINE__);
                if (actx->playingInfo)
                    actx->playingInfo->SaveEditing(false);
                actx->UnlockPlayingInfo(__FILE__, __LINE__);

                actx->LockDeleteNVP(__FILE__, __LINE__);
                if (actx->nvp)
                    editmode = actx->nvp->EnableEdit();
                actx->UnlockDeleteNVP(__FILE__, __LINE__);
            }
            else
            {
                actx->paused = !actx->paused;
                DoTogglePause(actx, true);
            }
        }
        else if (dialogname == kOSDDialogExitOptions)
        {
            if (result > 0)
            {
                if (!IsBookmarkAllowed(actx))
                    result++;
                if (result > 2 && !IsDeleteAllowed(actx))
                    result++;
            }

            switch (result)
            {
                case 0: case 4:
                    DoTogglePause(actx, true);
                    break;
                case 1:
                    PrepareToExitPlayer(actx, __LINE__);
                    SetExitPlayer(true, true);
                    break;
                case 3:
                    DoTogglePause(actx, true);
                    PromptDeleteRecording(
                        actx, tr("Delete this recording?"));
                    return handled;
                default:
                    PrepareToExitPlayer(actx, __LINE__, false);
                    SetExitPlayer(true, true);
                    break;
            }
        }
        else if (dialogname == kOSDDialogAskDelete)
        {
            switch (result)
            {
                case 1:
                    allowRerecord = true;
                    requestDelete = true;
                    SetExitPlayer(true, true);
                    break;
                case 2:
                    requestDelete = true;
                    SetExitPlayer(true, true);
                    break;
                case 3:
                    PrepareToExitPlayer(actx, __LINE__);
                    SetExitPlayer(true, true);
                    break;
                default:
                    if (isnearend)
                    {
                        PrepareToExitPlayer(actx, __LINE__, false);
                        SetExitPlayer(true, true);
                    }
                    else
                        DoTogglePause(actx, true);
                    break;
            }
        }
        else if (dialogname == kOSDDialogAllowRecording)
        {
            HandleOSDAskAllowResponse(GetPlayer(actx,0), result);
        }
        else if (dialogname == kOSDDialogIdleTimeout)
        {
            if (result == 1)
            {
                if (idleDialogTimerId)
                {
                    KillTimer(idleDialogTimerId);
                    idleDialogTimerId = 0;
                }

                if (idleTimerId)
                    KillTimer(idleTimerId);
                idleTimerId = StartTimer(db_idle_timeout, __LINE__);
            }
            else
            {
                VERBOSE(VB_GENERAL, LOC +
                        "No longer watching LiveTV, exiting");
                SetExitPlayer(true, true);
            }
        }
        else if (dialogname == kOSDDialogSleepTimeout)
        {
            if (result == 1)
            {
                if (sleepDialogTimerId)
                {
                    KillTimer(sleepDialogTimerId);
                    sleepDialogTimerId = 0;
                }
                sleepTimerId = StartTimer(sleepTimerTimeout * 1000, __LINE__);
            }
            else
            {
                VERBOSE(VB_GENERAL, LOC +
                        "No longer watching TV, exiting");
                SetExitPlayer(true, true);
            }
        }
        else if (dialogname == kOSDDialogChannelTimeout)
        {
            lockTimerOn = false;
        }

        OSD *osd = GetOSDLock(actx);
        if (osd)
            osd->HideSet(dialogname, true);
        ReturnOSDLock(actx, osd);
    }
    else
        handled = false;

    return handled;
}

bool TV::PictureAttributeHandleAction(PlayerContext *ctx,
                                      const QStringList &actions)
{
    if (!adjustingPicture)
        return false;

    bool handled = true;
    if (has_action("LEFT", actions))
    {
        DoChangePictureAttribute(ctx, adjustingPicture,
                                 adjustingPictureAttribute, false);
    }
    else if (has_action("RIGHT", actions))
    {
        DoChangePictureAttribute(ctx, adjustingPicture,
                                 adjustingPictureAttribute, true);
    }
    else
        handled = false;

    return handled;
}

bool TV::TimeStretchHandleAction(PlayerContext *ctx,
                                 const QStringList &actions)
{
    if (!stretchAdjustment)
        return false;

    bool handled = true;

    if (has_action("LEFT", actions))
        ChangeTimeStretch(ctx, -1);
    else if (has_action("RIGHT", actions))
        ChangeTimeStretch(ctx, 1);
    else if (has_action("DOWN", actions))
        ChangeTimeStretch(ctx, -5);
    else if (has_action("UP", actions))
        ChangeTimeStretch(ctx, 5);
    else if (has_action("ADJUSTSTRETCH", actions))
        ClearOSD(ctx);
    else
        handled = false;

    return handled;
}

bool TV::AudioSyncHandleAction(PlayerContext *ctx,
                               const QStringList &actions)
{
    if (!audiosyncAdjustment)
        return false;

    bool handled = true;

    if (has_action("LEFT", actions))
        ChangeAudioSync(ctx, -1);
    else if (has_action("RIGHT", actions))
        ChangeAudioSync(ctx, 1);
    else if (has_action("UP", actions))
        ChangeAudioSync(ctx, -10);
    else if (has_action("DOWN", actions))
        ChangeAudioSync(ctx, 10);
    else if (has_action("1", actions))
        ChangeAudioSync(ctx, 1000000);
    else if (has_action("0", actions))
        ChangeAudioSync(ctx, -1000000);
    else if (has_action("TOGGLEAUDIOSYNC", actions))
        ClearOSD(ctx);
    else
        handled = false;

    return handled;
}

bool TV::DVDMenuHandleAction(PlayerContext *ctx,
                             const QStringList &actions,
                             bool isDVD, bool isDVDStill)
{
    bool handled = false;

    if (isDVD)
    {
        int nb_buttons = ctx->buffer->DVD()->NumMenuButtons();
        if (nb_buttons == 0)
            return false;

        handled = true;
        if (has_action("UP", actions) ||
            has_action("CHANNELUP", actions))
        {
            ctx->buffer->DVD()->MoveButtonUp();
        }
        else if (has_action("DOWN", actions) ||
                 has_action("CHANNELDOWN", actions))
        {
            ctx->buffer->DVD()->MoveButtonDown();
        }
        else if (has_action("LEFT", actions) ||
                 has_action("SEEKRWND", actions))
        {
            ctx->buffer->DVD()->MoveButtonLeft();
        }
        else if (has_action("RIGHT", actions) ||
                 has_action("SEEKFFWD", actions))
        {
            ctx->buffer->DVD()->MoveButtonRight();
        }
        else if (has_action("SELECT", actions))
        {
            ctx->LockDeleteNVP(__FILE__, __LINE__);
            ctx->nvp->ActivateDVDButton();
            ctx->UnlockDeleteNVP(__FILE__, __LINE__);
        }
        else
            handled = false;
    }

    return handled;
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
    else if (has_action("PLAY", actions))
        DoPlay(ctx);
    else if (has_action("PAUSE", actions))
    {
        if (ctx->paused)
            SendMythSystemPlayEvent("PLAY_UNPAUSED", ctx->playingInfo);
        else
            SendMythSystemPlayEvent("PLAY_PAUSED", ctx->playingInfo);
        DoTogglePause(ctx, true);
    }
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
        QString msg = QString::null;
        ctx->LockDeleteNVP(__FILE__, __LINE__);
        if (ctx->nvp)
        {
            ctx->nvp->NextScanType();
            msg = toString(ctx->nvp->GetScanType());
        }
        ctx->UnlockDeleteNVP(__FILE__, __LINE__);

        if (!msg.isEmpty())
        {
            OSD *osd = GetOSDLock(ctx);
            if (osd)
                osd->SetSettingsText(msg, 3);
            ReturnOSDLock(ctx, osd);
        }
    }
    else if (has_action("ARBSEEK", actions) && !isDVD)
    {
        if (asInputMode)
        {
            ClearInputQueues(ctx, true);
            UpdateOSDTextEntry(ctx, tr("Seek:"));

            QMutexLocker locker(&timerIdLock);
            asInputMode = false;
            if (asInputTimerId)
            {
                KillTimer(asInputTimerId);
                asInputTimerId = 0;
            }
        }
        else
        {
            ClearInputQueues(ctx, false);
            AddKeyToInputQueue(ctx, 0);

            QMutexLocker locker(&timerIdLock);
            asInputMode        = true;
            ccInputMode        = false;
            asInputTimerId = StartTimer(kInputModeTimeout, __LINE__);
            if (ccInputTimerId)
            {
                KillTimer(ccInputTimerId);
                ccInputTimerId = 0;
            }
        }
    }
    else if (has_action("JUMPRWND", actions))
    {
        if (isDVD)
            DVDJumpBack(ctx);
        else if (GetNumChapters(ctx) > 0)
            DoJumpChapter(ctx, -1);
        else
            DoSeek(ctx, -ctx->jumptime * 60, tr("Jump Back"));
    }
    else if (has_action("JUMPFFWD", actions))
    {
        if (isDVD)
            DVDJumpForward(ctx);
        else if (GetNumChapters(ctx) > 0)
            DoJumpChapter(ctx, 9999);
        else
            DoSeek(ctx, ctx->jumptime * 60, tr("Jump Ahead"));
    }
    else if (has_action("JUMPBKMRK", actions))
    {
        ctx->LockDeleteNVP(__FILE__, __LINE__);
        long long bookmark = ctx->nvp->GetBookmark();
        long long curloc   = ctx->nvp->GetFramesPlayed();
        float mult = 1.0f;
        if (ctx->last_framerate)
            mult = 1.0f / ctx->last_framerate;
        long long seekloc = (long long) ((bookmark - curloc) * mult);
        ctx->UnlockDeleteNVP(__FILE__, __LINE__);

        if (bookmark > ctx->last_framerate)
        {
            DoSeek(ctx, seekloc, tr("Jump to Bookmark"));
        }
    }
    else if (has_action("JUMPSTART",actions))
    {
        long long seekloc = +1;
        ctx->LockDeleteNVP(__FILE__, __LINE__);
        if (ctx->nvp && ctx->last_framerate >= 0.0001f)
        {
            seekloc = (int64_t) (-1.0 * ctx->nvp->GetFramesPlayed() /
                                 ctx->last_framerate);
        }
        ctx->UnlockDeleteNVP(__FILE__, __LINE__);

        if (seekloc <= 0)
            DoSeek(ctx, seekloc, tr("Jump to Beginning"));
    }
    else if (has_action("CLEAROSD", actions))
    {
        ClearOSD(ctx);
    }
    else if (has_action("VIEWSCHEDULED", actions))
        EditSchedule(ctx, kViewSchedule);
    else if (HandleJumpToProgramAction(ctx, actions))
    {
    }
    else if (has_action("SIGNALMON", actions))
    {
        if ((GetState(ctx) == kState_WatchingLiveTV) && ctx->recorder)
        {
            QString input = ctx->recorder->GetInput();
            uint timeout  = ctx->recorder->GetSignalLockTimeout(input);

            if (timeout == 0xffffffff)
            {
                OSD *osd = GetOSDLock(ctx);
                if (osd)
                    osd->SetSettingsText("No Signal Monitor", 2);
                ReturnOSDLock(ctx, osd);
                return false;
            }

            int rate   = sigMonMode ? 0 : 100;
            int notify = sigMonMode ? 0 : 1;

            PauseLiveTV(ctx);
            ctx->recorder->SetSignalMonitoringRate(rate, notify);
            UnpauseLiveTV(ctx);

            lockTimerOn = false;
            sigMonMode  = !sigMonMode;
        }
    }
    else if (has_action("SCREENSHOT", actions) && !isDVD)
    {
        long long caploc = -1;
        ctx->LockDeleteNVP(__FILE__, __LINE__);
        if (ctx->nvp)
            caploc = ctx->nvp->GetFramesPlayed();
        ctx->UnlockDeleteNVP(__FILE__, __LINE__);

        if (caploc >= 0)
            ScreenShot(ctx, caploc);
    }
    else if (has_action("EXITSHOWNOPROMPTS", actions))
    {
        requestDelete = false;
        PrepareToExitPlayer(ctx, __LINE__);
        SetExitPlayer(true, true);
    }
    else if (has_action("ESCAPE", actions))
    {
        if (StateIsLiveTV(ctx->GetState()) &&
            (ctx->lastSignalMsgTime.elapsed() <
             (int)PlayerContext::kSMExitTimeout))
        {
            ClearOSD(ctx);
        }
        else
        {
            OSD *osd = GetOSDLock(ctx);
            if (osd && ClearOSD(ctx))
            {
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
            if (ctx->HasNVP() && (12 & db_playback_exit_prompt))
            {
                PromptStopWatchingRecording(ctx);
                return handled;
            }
            else
            {
                do_exit = true;
            }
        }
        else
        {
            if (ctx->HasNVP() && (5 & db_playback_exit_prompt) &&
                !underNetworkControl && !isDVDStill)
            {
                PromptStopWatchingRecording(ctx);
                return handled;
            }
            PrepareToExitPlayer(ctx, __LINE__, db_playback_exit_prompt == 2);
            requestDelete = false;
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
            else
            {
                SetExitPlayer(true, true);
            }
        }

        SetActive(ctx, 0, false);
    }
    else if (has_action("VOLUMEDOWN", actions))
        ChangeVolume(ctx, false);
    else if (has_action("VOLUMEUP", actions))
        ChangeVolume(ctx, true);
    else if (has_action("MUTE", actions))
        ToggleMute(ctx);
    else if (has_action("STRETCHINC", actions))
        ChangeTimeStretch(ctx, 1);
    else if (has_action("STRETCHDEC", actions))
        ChangeTimeStretch(ctx, -1);
    else if (has_action("MENU", actions))
        ShowOSDTreeMenu(ctx);
    else if (has_action("INFO", actions))
    {
        if (HasQueuedInput())
        {
            DoArbSeek(ctx, ARBSEEK_SET);
        }
        else
            ToggleOSD(ctx, true);
    }
    else if (!isDVDStill && SeekHandleAction(ctx, actions, isDVD))
    {
    }
    else
    {
        handled = false;
        QStringList::const_iterator it = actions.begin();
        for (; it != actions.end() && !handled; ++it)
            handled = HandleTrackAction(ctx, *it);
    }

    return handled;
}

bool TV::FFRewHandleAction(PlayerContext *ctx, const QStringList &actions)
{
    bool handled = false;

    if (ctx->ff_rew_state)
    {
        for (int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            bool ok = false;
            int val = action.toInt(&ok);

            if (ok && val < (int)ff_rew_speeds.size())
            {
                SetFFRew(ctx, val);
                handled = true;
            }
        }

        if (!handled)
        {
            DoNVPSeek(ctx, StopFFRew(ctx));
            UpdateOSDSeekMessage(ctx, ctx->GetPlayMessage(),
                                 osd_general_timeout);
            handled = true;
        }
    }

    if (ctx->ff_rew_speed)
    {
        NormalSpeed(ctx);
        UpdateOSDSeekMessage(ctx, ctx->GetPlayMessage(), osd_general_timeout);
        handled = true;
    }

    return handled;
}

bool TV::ToggleHandleAction(PlayerContext *ctx,
                            const QStringList &actions, bool isDVD)
{
    bool handled = true;
    bool islivetv = StateIsLiveTV(GetState(ctx));

    if (has_action("TOGGLEASPECT", actions))
        ToggleAspectOverride(ctx);
    else if (has_action("TOGGLEFILL", actions))
        ToggleAdjustFill(ctx);
    else if (has_action("TOGGLEAUDIOSYNC", actions))
        ChangeAudioSync(ctx, 0);   // just display
    else if (has_action("TOGGLEPICCONTROLS", actions))
        DoTogglePictureAttribute(ctx, kAdjustingPicture_Playback);
    else if (has_action("TOGGLESTRETCH", actions))
        ToggleTimeStretch(ctx);
    else if (has_action("TOGGLEUPMIX", actions))
        ToggleUpmix(ctx);
    else if (has_action("TOGGLESLEEP", actions))
        ToggleSleepTimer(ctx);
    else if (has_action("TOGGLERECORD", actions) && islivetv)
        ToggleRecord(ctx);
//    else if (has_action("TOGGLEFAV", actions) && islivetv)
//        ToggleChannelFavorite(ctx);
    else if (has_action("TOGGLECHANCONTROLS", actions) && islivetv)
        DoTogglePictureAttribute(ctx, kAdjustingPicture_Channel);
    else if (has_action("TOGGLERECCONTROLS", actions) && islivetv)
        DoTogglePictureAttribute(ctx, kAdjustingPicture_Recording);
    else if (has_action("TOGGLEINPUTS", actions) &&
             islivetv && !ctx->pseudoLiveTVState)
    {
        ToggleInputs(ctx);
    }
    else if (has_action("TOGGLEBROWSE", actions))
    {
        if (islivetv)
            BrowseStart(ctx);
        else if (!isDVD)
            ShowOSDTreeMenu(ctx);
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
    else
        handled = false;

    return handled;
}

bool TV::PxPHandleAction(PlayerContext *ctx, const QStringList &actions)
{
    if (!IsPIPSupported(ctx) && !IsPBPSupported(ctx))
        return false;

    bool handled = true;
    {
        QMutexLocker locker(&timerIdLock);

        if (has_action("TOGGLEPIPMODE", actions))
            changePxP.enqueue("TOGGLEPIPMODE");
        else if (has_action("TOGGLEPBPMODE", actions))
            changePxP.enqueue("TOGGLEPBPMODE");
        else if (has_action("CREATEPIPVIEW", actions))
            changePxP.enqueue("CREATEPIPVIEW");
        else if (has_action("CREATEPBPVIEW", actions))
            changePxP.enqueue("CREATEPBPVIEW");
        else if (has_action("SWAPPIP", actions))
            changePxP.enqueue("SWAPPIP");
        else if (has_action("TOGGLEPIPSTATE", actions))
            changePxP.enqueue("TOGGLEPIPSTATE");
        else
            handled = false;

        if (!changePxP.empty() && !pipChangeTimerId)
            pipChangeTimerId = StartTimer(1, __LINE__);
    }

    if (has_action("NEXTPIPWINDOW", actions))
    {
        SetActive(ctx, -1, true);
        handled = true;
    }

    return handled;
}

bool TV::ActivePostQHandleAction(PlayerContext *ctx,
                                 const QStringList &actions, bool isDVD)
{
    bool handled = true;
    bool islivetv = StateIsLiveTV(GetState(ctx));

    if (has_action("SELECT", actions))
    {
        if (!islivetv || !CommitQueuedInput(ctx))
        {
            if (isDVD && ctx->buffer && ctx->buffer->DVD())
                ctx->buffer->DVD()->JumpToTitle(false);

            ctx->LockDeleteNVP(__FILE__, __LINE__);
            if (ctx->nvp)
            {
                if (db_toggle_bookmark && ctx->nvp->GetBookmark())
                    ctx->nvp->ClearBookmark();
                else
                    ctx->nvp->SetBookmark();
            }
            ctx->UnlockDeleteNVP(__FILE__, __LINE__);
        }
    }
    else if (has_action("NEXTFAV", actions) && islivetv)
        ChangeChannel(ctx, CHANNEL_DIRECTION_FAVORITE);
    else if (has_action("NEXTSOURCE", actions) && islivetv)
        SwitchSource(ctx, kNextSource);
    else if (has_action("PREVSOURCE", actions) && islivetv)
        SwitchSource(ctx, kPreviousSource);
    else if (has_action("NEXTINPUT", actions) && islivetv)
        ToggleInputs(ctx);
    else if (has_action("NEXTCARD", actions) && islivetv)
        SwitchCards(ctx);
    else if (has_action("GUIDE", actions))
        EditSchedule(ctx, kScheduleProgramGuide);
    else if (has_action("PREVCHAN", actions) && islivetv)
        PopPreviousChannel(ctx, false);
    else if (has_action("CHANNELUP", actions))
    {
        if (islivetv)
        {
            if (db_browse_always)
                BrowseDispInfo(ctx, BROWSE_UP);
            else
                ChangeChannel(ctx, CHANNEL_DIRECTION_UP);
        }
        else if (isDVD)
            DVDJumpBack(ctx);
        else if (GetNumChapters(ctx) > 0)
            DoJumpChapter(ctx, -1);
        else
            DoSeek(ctx, -ctx->jumptime * 60, tr("Jump Back"));
    }
    else if (has_action("CHANNELDOWN", actions))
    {
        if (islivetv)
        {
            if (db_browse_always)
                BrowseDispInfo(ctx, BROWSE_DOWN);
            else
                ChangeChannel(ctx, CHANNEL_DIRECTION_DOWN);
        }
        else if (isDVD)
            DVDJumpForward(ctx);
        else if (GetNumChapters(ctx) > 0)
            DoJumpChapter(ctx, 9999);
        else
            DoSeek(ctx, ctx->jumptime * 60, tr("Jump Ahead"));
    }
    else if (has_action("DELETE", actions) && !islivetv)
    {
        NormalSpeed(ctx);
        StopFFRew(ctx);
        ctx->LockDeleteNVP(__FILE__, __LINE__);
        if (ctx->nvp)
            ctx->nvp->SetBookmark();
        ctx->UnlockDeleteNVP(__FILE__, __LINE__);
        PromptDeleteRecording(ctx, tr("Delete this recording?"));
    }
    else if (has_action("JUMPTODVDROOTMENU", actions) && isDVD)
    {
        ctx->LockDeleteNVP(__FILE__, __LINE__);
        if (ctx->nvp)
            ctx->nvp->GoToDVDMenu("root");
        ctx->UnlockDeleteNVP(__FILE__, __LINE__);
    }
    else if (has_action("FINDER", actions))
        EditSchedule(ctx, kScheduleProgramFinder);
    else
        handled = false;

    return handled;
}


void TV::ProcessNetworkControlCommand(PlayerContext *ctx,
                                      const QString &command)
{
    bool ignoreKeys = ctx->IsNVPChangingBuffers();
#ifdef DEBUG_ACTIONS
    VERBOSE(VB_IMPORTANT, LOC + "ProcessNetworkControlCommand(" +
            QString("%1) ignoreKeys: %2").arg(command).arg(ignoreKeys));
#endif

    if (ignoreKeys)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                "Ignoring network control command"
                "\n\t\t\tbecause ignoreKeys is set");
        return;
    }

    QStringList tokens = command.split(" ", QString::SkipEmptyParts);
    if (tokens.size() < 2)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Not enough tokens"
                "in network control command" + "\n\t\t\t" +
                QString("'%1'").arg(command));
        return;
    }

    OSD *osd = GetOSDLock(ctx);
    QString dlg;
    if (osd)
        dlg = osd->GetDialogActive();
    ReturnOSDLock(ctx, osd);

    if (!dlg.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "Ignoring network "
                "control command\n\t\t\t" +
                QString("because dialog '%1'").arg(dlg) +
                "is waiting for a response");
        return;
    }

    if (tokens[1] != "QUERY")
        ClearOSD(ctx);

    if (tokens.size() == 3 && tokens[1] == "CHANID")
    {
        queuedChanID = tokens[2].toUInt();
        queuedChanNum = QString::null;
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
        if (tokens[2] == "0x")
        {
            NormalSpeed(ctx);
            StopFFRew(ctx);

            if (!ctx->paused)
                DoTogglePause(ctx, true);
        }
        else if (tokens[2].contains(QRegExp("^\\-*\\d+x$")))
        {
            QString speed = tokens[2].left(tokens[2].length()-1);
            bool ok = false;
            int tmpSpeed = speed.toInt(&ok);
            int searchSpeed = abs(tmpSpeed);
            unsigned int index;

            if (ctx->paused)
                DoTogglePause(ctx, true);

            if (tmpSpeed == 1)
            {
                StopFFRew(ctx);
                ctx->ts_normal = 1.0f;
                ChangeTimeStretch(ctx, 0, false);

                ReturnPlayerLock(ctx);
                return;
            }

            NormalSpeed(ctx);

            for (index = 0; index < ff_rew_speeds.size(); index++)
                if (ff_rew_speeds[index] == searchSpeed)
                    break;

            if ((index < ff_rew_speeds.size()) &&
                (ff_rew_speeds[index] == searchSpeed))
            {
                if (tmpSpeed < 0)
                    ctx->ff_rew_state = -1;
                else if (tmpSpeed > 1)
                    ctx->ff_rew_state = 1;
                else
                    StopFFRew(ctx);

                if (ctx->ff_rew_state)
                    SetFFRew(ctx, index);
            }
            else
            {
                VERBOSE(VB_IMPORTANT, QString("Couldn't find %1 speed in "
                    "FFRewSpeed settings array, changing to default speed "
                    "of 1x").arg(searchSpeed));

                ctx->ff_rew_state = 0;
                SetFFRew(ctx, kInitFFRWSpeed);
            }
        }
        else if (tokens[2].contains(QRegExp("^\\d*\\.\\d+x$")))
        {
            QString tmpSpeed = tokens[2].left(tokens[2].length() - 1);

            if (ctx->paused)
                DoTogglePause(ctx, true);

            StopFFRew(ctx);

            bool floatRead;
            float stretch = tmpSpeed.toFloat(&floatRead);
            if (floatRead &&
                stretch <= 2.0 &&
                stretch >= 0.48)
            {
                ctx->ts_normal = stretch;   // alter speed before display
                ChangeTimeStretch(ctx, 0, false);
            }
        }
        else if (tokens[2].contains(QRegExp("^\\d+\\/\\d+x$")))
        {
            if (ctx->buffer && ctx->buffer->InDVDMenuOrStillFrame())
            {
                ReturnPlayerLock(ctx);
                return;
            }

            if (ctx->paused)
                DoTogglePause(ctx, true);

            if (tokens[2] == "16x")
                ChangeSpeed(ctx, 5 - ctx->ff_rew_speed);
            else if (tokens[2] == "8x")
                ChangeSpeed(ctx, 4 - ctx->ff_rew_speed);
            else if (tokens[2] == "4x")
                ChangeSpeed(ctx, 3 - ctx->ff_rew_speed);
            else if (tokens[2] == "3x")
                ChangeSpeed(ctx, 2 - ctx->ff_rew_speed);
            else if (tokens[2] == "2x")
                ChangeSpeed(ctx, 1 - ctx->ff_rew_speed);
            else if (tokens[2] == "1x")
                ChangeSpeed(ctx, 0 - ctx->ff_rew_speed);
            else if (tokens[2] == "1/2x")
                ChangeSpeed(ctx, -1 - ctx->ff_rew_speed);
            else if (tokens[2] == "1/3x")
                ChangeSpeed(ctx, -2 - ctx->ff_rew_speed);
            else if (tokens[2] == "1/4x")
                ChangeSpeed(ctx, -3 - ctx->ff_rew_speed);
            else if (tokens[2] == "1/8x")
                ChangeSpeed(ctx, -4 - ctx->ff_rew_speed);
            else if (tokens[2] == "1/16x")
                ChangeSpeed(ctx, -5 - ctx->ff_rew_speed);
        }
        else
            VERBOSE(VB_IMPORTANT,
                QString("Found an unknown speed of %1").arg(tokens[2]));
    }
    else if (tokens.size() == 2 && tokens[1] == "STOP")
    {
        ctx->LockDeleteNVP(__FILE__, __LINE__);
        if (ctx->nvp)
            ctx->nvp->SetBookmark();
        if (ctx->nvp && db_auto_set_watched)
            ctx->nvp->SetWatched();
        ctx->UnlockDeleteNVP(__FILE__, __LINE__);
        SetExitPlayer(true, true);
    }
    else if (tokens.size() >= 3 && tokens[1] == "SEEK" && ctx->HasNVP())
    {
        if (ctx->buffer && ctx->buffer->InDVDMenuOrStillFrame())
            return;

        ctx->LockDeleteNVP(__FILE__, __LINE__);
        long long fplay = 0;
        if (ctx->nvp && (tokens[2] == "BEGINNING" || tokens[2] == "POSITION"))
        {
            fplay = ctx->nvp->GetFramesPlayed();
        }
        ctx->UnlockDeleteNVP(__FILE__, __LINE__);

        if (tokens[2] == "BEGINNING")
            DoSeek(ctx, -fplay, tr("Jump to Beginning"));
        else if (tokens[2] == "FORWARD")
            DoSeek(ctx, ctx->fftime, tr("Skip Ahead"));
        else if (tokens[2] == "BACKWARD")
            DoSeek(ctx, -ctx->rewtime, tr("Skip Back"));
        else if ((tokens[2] == "POSITION") && (tokens.size() == 4) &&
                 (tokens[3].contains(QRegExp("^\\d+$"))) &&
                 ctx->last_framerate)
        {
            long long rel_frame = tokens[3].toInt();
            rel_frame -= (long long) (fplay * (1.0 / ctx->last_framerate)),
            DoSeek(ctx, rel_frame, tr("Jump To"));
        }
    }
    else if (tokens.size() >= 3 && tokens[1] == "QUERY")
    {
        if (tokens[2] == "POSITION")
        {
            QString speedStr;

            switch (ctx->ff_rew_speed)
            {
                case  4: speedStr = "16x"; break;
                case  3: speedStr = "8x";  break;
                case  2: speedStr = "3x";  break;
                case  1: speedStr = "2x";  break;
                case  0: speedStr = "1x";  break;
                case -1: speedStr = "1/3x"; break;
                case -2: speedStr = "1/8x"; break;
                case -3: speedStr = "1/16x"; break;
                case -4: speedStr = "0x"; break;
                default: speedStr = "1x"; break;
            }

            if (ctx->ff_rew_state == -1)
                speedStr = QString("-%1").arg(speedStr);
            else if (ctx->ts_normal != 1.0)
                speedStr = QString("%1X").arg(ctx->ts_normal);

            struct StatusPosInfo posInfo;
            ctx->CalcNVPSliderPosition(posInfo, true);

            QDateTime respDate = mythCurrentDateTime();
            QString infoStr = "";

            ctx->LockDeleteNVP(__FILE__, __LINE__);
            long long fplay = 0;
            if (ctx->nvp)
                fplay = ctx->nvp->GetFramesPlayed();
            ctx->UnlockDeleteNVP(__FILE__, __LINE__);

            ctx->LockPlayingInfo(__FILE__, __LINE__);
            if (ctx->GetState() == kState_WatchingLiveTV)
            {
                infoStr = "LiveTV";
                if (ctx->playingInfo)
                    respDate = ctx->playingInfo->GetScheduledStartTime();
            }
            else
            {
                if (ctx->buffer->isDVD())
                    infoStr = "DVD";
                else if (ctx->playingInfo->IsRecording())
                    infoStr = "Recorded";
                else
                    infoStr = "Video";

                if (ctx->playingInfo)
                    respDate = ctx->playingInfo->GetRecordingStartTime();
            }

            if ((infoStr == "Recorded") || (infoStr == "LiveTV"))
            {
                infoStr += QString(" %1 %2 %3 %4 %5 %6 %7")
                    .arg(posInfo.desc)
                    .arg(speedStr)
                    .arg(ctx->playingInfo != NULL ?
                         ctx->playingInfo->GetChanID() : 0)
                    .arg(respDate.toString(Qt::ISODate))
                    .arg(fplay)
                    .arg(ctx->buffer->GetFilename())
                    .arg(ctx->last_framerate);
            }
            else
            {
                QString position = posInfo.desc.section(" ",0,0);
                infoStr += QString(" %1 %2 %3 %4 %5")
                    .arg(position)
                    .arg(speedStr)
                    .arg(ctx->buffer->GetFilename())
                    .arg(fplay)
                    .arg(ctx->last_framerate);
            }

            ctx->UnlockPlayingInfo(__FILE__, __LINE__);

            QString message = QString("NETWORK_CONTROL ANSWER %1")
                                .arg(infoStr);
            MythEvent me(message);
            gCoreContext->dispatch(me);
        }
    }
}

/**
 * \brief Setup Picture by Picture. right side will be the current video.
 * \param info programinfo for PBP to use for left Picture. is NULL for Live TV
 */
bool TV::CreatePBP(PlayerContext *ctx, const ProgramInfo *info)
{
    VERBOSE(VB_PLAYBACK, LOC + "CreatePBP() -- begin");

    if (player.size() > 1)
    {
        VERBOSE(VB_IMPORTANT, LOC + "CreatePBP() -- end : "
                "only allowed when player.size() == 1");
        return false;
    }

    PlayerContext *mctx = GetPlayer(ctx, 0);
    if (!IsPBPSupported(mctx))
    {
        VERBOSE(VB_IMPORTANT, LOC + "CreatePBP() -- end : "
                "PBP not supported by video method.");
        return false;
    }

    if (!mctx->nvp)
        return false;
    mctx->LockDeleteNVP(__FILE__, __LINE__);
    long long mctx_frame = mctx->nvp->GetFramesPlayed();
    mctx->UnlockDeleteNVP(__FILE__, __LINE__);

    // This is safe because we are already holding lock for a ctx
    player.push_back(new PlayerContext(kPBPPlayerInUseID));
    PlayerContext *pbpctx = player.back();
    if (noHardwareDecoders)
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
        delete player.back();
        player.pop_back();
        return false;
    }

    mctx->PIPTeardown();
    mctx->SetPIPState(kPBPLeft);
    mctx->buffer->Seek(0, SEEK_SET);

    if (StateIsLiveTV(mctx->GetState()))
        mctx->buffer->Unpause();

    bool ok = mctx->CreateNVP(
        this, GetMythMainWindow(), mctx->GetState(), 0, &mctx->embedBounds);

    if (ok)
    {
        ScheduleStateChange(mctx);
        mctx->StartOSD(this);
        mctx->LockDeleteNVP(__FILE__, __LINE__);
        if (mctx->nvp)
            mctx->nvp->JumpToFrame(mctx_frame);
        mctx->UnlockDeleteNVP(__FILE__, __LINE__);
        SetSpeedChangeTimer(25, __LINE__);
    }
    else
    {
        VERBOSE(VB_IMPORTANT, LOC + "Failed to restart new main context");
        // Make putative PBP context the main context
        swap(player[0],player[1]);
        player[0]->SetPIPState(kPIPOff);
        // End the old main context..
        ForceNextStateNone(mctx);
    }

    VERBOSE(VB_PLAYBACK, LOC + "CreatePBP() -- end : "<<ok);
    return ok;
}

/**
 * \brief create PIP.
 * \param info programinfo for PIP to create. is NULL for LiveTV PIP
 */
bool TV::CreatePIP(PlayerContext *ctx, const ProgramInfo *info)
{
    PlayerContext *mctx = GetPlayer(ctx, 0);
    if (!mctx)
        return false;

    VERBOSE(VB_PLAYBACK, LOC + "CreatePIP -- begin");

    if (mctx->IsPBP())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "CreatePIP called, but we're in PBP mode already, ignoring.");
        return false;
    }

    /* TODO implement PIP solution for Xvmc playback */
    if (!IsPIPSupported(mctx))
    {
        VERBOSE(VB_IMPORTANT, LOC + "PiP not supported by video method.");
        return false;
    }

    PlayerContext *pipctx = new PlayerContext(kPIPPlayerInUseID);
    if (noHardwareDecoders)
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
    player.push_back(pipctx);

    return true;
}

int TV::find_player_index(const PlayerContext *ctx) const
{
    for (uint i = 0; i < player.size(); i++)
        if (GetPlayer(ctx, i) == ctx)
            return i;
    return -1;
}

bool TV::StartPlayer(PlayerContext *mctx, PlayerContext *ctx,
                     TVState desiredState)
{
    bool wantPiP = ctx->IsPIP();

    VERBOSE(VB_PLAYBACK, LOC + QString("StartPlayer(%1, %2, %3) -- begin")
            .arg(find_player_index(ctx)).arg(StateToString(desiredState))
            .arg((wantPiP) ? "PiP" : "main"));

    VERBOSE(VB_PLAYBACK, LOC +
            QString("Elapsed time since TV constructor was called: %1 ms")
            .arg(ctorTime.elapsed()));

    if (wantPiP)
    {
        if (mctx->HasNVP() && ctx->StartPIPPlayer(this, desiredState) &&
            ctx->HasNVP() && PIPAddPlayer(mctx, ctx))
        {
            ScheduleStateChange(ctx);
            VERBOSE(VB_IMPORTANT, "StartPlayer PiP -- end : ok");
            return true;
        }

        ForceNextStateNone(ctx);
        VERBOSE(VB_IMPORTANT, "StartPlayer PiP -- end : !ok");
        return false;
    }

    InitUDPNotifyEvent();
    bool ok = false;
    if (ctx->IsNullVideoDesired())
    {
        ok = ctx->CreateNVP(this, NULL, desiredState, 0, NULL);
        ScheduleStateChange(ctx);
        if (ok)
            ok = PIPAddPlayer(mctx, ctx);
    }
    else
    {
        ok = ctx->CreateNVP(this, GetMythMainWindow(), desiredState,
                            mctx->embedWinID, &mctx->embedBounds);
        ScheduleStateChange(ctx);
    }

    if (ok)
    {
        ctx->StartOSD(this);
        SetSpeedChangeTimer(25, __LINE__);
    }

    VERBOSE(VB_PLAYBACK, LOC + QString("StartPlayer(%1, %2, %3) -- end %4")
            .arg(find_player_index(ctx)).arg(StateToString(desiredState))
            .arg((wantPiP) ? "PiP" : "main").arg((ok) ? "ok" : "error"));

    return ok;
}

/// \brief Maps NVP of software scaled PIP to the NVP of the main player
bool TV::PIPAddPlayer(PlayerContext *mctx, PlayerContext *pipctx)
{
    if (!mctx || !pipctx)
        return false;

    if (!mctx->IsNVPPlaying())
        return false;

    bool ok = false, addCondition = false;
    pipctx->LockDeleteNVP(__FILE__, __LINE__);
    if (pipctx->nvp)
    {
        bool is_using_null = pipctx->nvp->UsingNullVideo();
        pipctx->UnlockDeleteNVP(__FILE__, __LINE__);

        if (is_using_null)
        {
            addCondition = true;
            multi_lock(&mctx->deleteNVPLock, &pipctx->deleteNVPLock, NULL);
            if (mctx->nvp && pipctx->nvp)
            {
                PIPLocation loc = mctx->nvp->GetNextPIPLocation();
                ok = mctx->nvp->AddPIPPlayer(pipctx->nvp, loc, 4000);
            }
            mctx->deleteNVPLock.unlock();
            pipctx->deleteNVPLock.unlock();
        }
        else if (pipctx->IsPIP())
        {
            ok = ResizePIPWindow(pipctx);
        }
    }
    else
        pipctx->UnlockDeleteNVP(__FILE__, __LINE__);

    VERBOSE(VB_IMPORTANT,
            QString("AddPIPPlayer null: %1 IsPIP: %2 addCond: %3 ok: %4")
            .arg(pipctx->nvp->UsingNullVideo())
            .arg(pipctx->IsPIP()).arg(addCondition).arg(ok));

    return ok;
}

/// \brief Unmaps NVP of software scaled PIP from the NVP of the main player
bool TV::PIPRemovePlayer(PlayerContext *mctx, PlayerContext *pipctx)
{
    if (!mctx || !pipctx)
        return false;

    bool ok = false;
    multi_lock(&mctx->deleteNVPLock, &pipctx->deleteNVPLock, NULL);
    if (mctx->nvp && pipctx->nvp)
        ok = mctx->nvp->RemovePIPPlayer(pipctx->nvp, 4000);
    mctx->deleteNVPLock.unlock();
    pipctx->deleteNVPLock.unlock();

    VERBOSE(VB_IMPORTANT, QString("PIPRemovePlayer ok: %1").arg(ok));

    return ok;
}

/// \brief start/stop PIP/PBP
void TV::PxPToggleView(PlayerContext *actx, bool wantPBP)
{
    if (wantPBP && !IsPBPSupported(actx))
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                "PxPToggleView() -- end: PBP not supported by video method.");
        return;
    }

    if (player.size() <= 1)
        PxPCreateView(actx, wantPBP);
    else
        PxPTeardownView(actx);
}

/// \brief start PIP/PBP
void TV::PxPCreateView(PlayerContext *actx, bool wantPBP)
{
    if (!actx)
        return;

    QString err_msg = QString::null;
    if ((player.size() > kMaxPBPCount) && (wantPBP || actx->IsPBP()))
    {
        err_msg = tr("Sorry, PBP only supports %n video stream(s)", "",
                     kMaxPBPCount);
    }

    if ((player.size() > kMaxPIPCount) &&
        (!wantPBP || GetPlayer(actx,1)->IsPIP()))
    {
        err_msg = tr("Sorry, PIP only supports %n video stream(s)", "",
                     kMaxPIPCount);
    }

    if ((player.size() > 1) && (wantPBP ^ actx->IsPBP()))
        err_msg = tr("Sorry, can not mix PBP and PIP views");

    if (!err_msg.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + err_msg);
        OSD *osd = GetOSDLock(actx);
        if (osd)
            osd->SetSettingsText(err_msg, 3);
        ReturnOSDLock(actx, osd);
        return;
    }

    bool ok = false;
    if (wantPBP)
        ok = CreatePBP(actx, NULL);
    else
        ok = CreatePIP(actx, NULL);
    actx = GetPlayer(actx, -1); // CreatePBP/PIP mess with ctx's

    QString msg = (ok) ?
        ((wantPBP) ? tr("Creating PBP")      : tr("Creating PIP")) :
        ((wantPBP) ? tr("Cannot create PBP") : tr("Cannot create PIP"));

    OSD *osd = GetOSDLock(actx);
    if (osd)
        osd->SetSettingsText(msg, 3);
    ReturnOSDLock(actx, osd);
}

/// \brief stop PIP/PBP
void TV::PxPTeardownView(PlayerContext *actx)
{
    VERBOSE(VB_IMPORTANT, "PxPTeardownView()");

    QString msg;
    PlayerContext *mctx = GetPlayer(actx, 0);
    PlayerContext *dctx = NULL;
    dctx = (mctx != actx)       ? actx               : dctx;
    dctx = (2 == player.size()) ? GetPlayer(actx, 1) : dctx;

    SetActive(actx, 0, false);

    PlayerContext *ctx1 = GetPlayer(actx, 1);
    msg = (ctx1->IsPIP()) ? tr("Stopping PIP") : tr("Stopping PBP");
    if (dctx)
    {
        ForceNextStateNone(dctx);
    }
    else
    {
        if (player.size() > 2)
        {
            msg = (ctx1->IsPIP()) ?
                tr("Stopping all PIPs") : tr("Stopping all PBPs");
        }

        for (uint i = player.size() - 1; i > 0; i--)
            ForceNextStateNone(GetPlayer(actx,i));
    }

    OSD *osd = GetOSDLock(mctx);
    if (osd)
        osd->SetSettingsText(msg, 3);
    ReturnOSDLock(mctx, osd);
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
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                "PxPToggleType() -- end: PBP not supported by video method.");
        return;
    }


    VERBOSE(VB_PLAYBACK, LOC +
            QString("PxPToggleType() converting from %1 to %2 -- begin")
            .arg(before).arg(after));

    if (mctx->IsPBP() == wantPBP)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                "PxPToggleType() -- end: already in desired mode");
        return;
    }

    uint max_cnt = min(kMaxPBPCount, kMaxPIPCount+1);
    if (player.size() > max_cnt)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("PxPToggleType() -- end: "
                        "# player contexts must be %1 or less, "
                        "but it is currently %1")
                .arg(max_cnt).arg(player.size()));

        QString err_msg = tr("Too many views to switch");

        PlayerContext *actx = GetPlayer(mctx, -1);
        OSD *osd = GetOSDLock(actx);
        if (osd)
            osd->SetSettingsText(err_msg, 3);
        ReturnOSDLock(actx, osd);
        return;
    }

    for (uint i = 0; i < player.size(); i++)
    {
        PlayerContext *ctx = GetPlayer(mctx, i);
        if (!ctx->IsNVPPlaying())
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "PxPToggleType() -- end: " +
                    QString("nvp #%1 is not active, exiting without "
                            "doing anything to avoid danger").arg(i));
            return;
        }
    }

    MuteState mctx_mute = kMuteOff;
    mctx->LockDeleteNVP(__FILE__, __LINE__);
    if (mctx->nvp)
        mctx_mute = mctx->nvp->GetMuteState();
    mctx->UnlockDeleteNVP(__FILE__, __LINE__);

    vector<long long> pos = TeardownAllNVPs(mctx);

    if (wantPBP)
    {
        GetPlayer(mctx, 0)->SetPIPState(kPBPLeft);
        GetPlayer(mctx, 1)->SetPIPState(kPBPRight);
    }
    else
    {
        GetPlayer(mctx, 0)->SetPIPState(kPIPOff);
        for (uint i = 1; i < player.size(); i++)
        {
            GetPlayer(mctx, i)->SetPIPState(kPIPonTV);
            GetPlayer(mctx, i)->SetNullVideo(true);
        }
    }

    RestartAllNVPs(mctx, pos, mctx_mute);

    VERBOSE(VB_PLAYBACK, LOC +
            QString("PxPToggleType() converting from %1 to %2 -- end")
            .arg(before).arg(after));
}

/**
 * \brief resize PIP Window. done when changing channels or swapping PIP
 */
bool TV::ResizePIPWindow(PlayerContext *ctx)
{
    VERBOSE(VB_PLAYBACK, LOC + "ResizePIPWindow -- begin");
    PlayerContext *mctx = GetPlayer(ctx, 0);
    if (mctx->HasNVP() && ctx->HasNVP())
    {
        QRect rect;

        multi_lock(&mctx->deleteNVPLock, &ctx->deleteNVPLock, (QMutex*)NULL);
        if (mctx->nvp && ctx->nvp)
        {
            PIPLocation loc = mctx->nvp->GetNextPIPLocation();
            VERBOSE(VB_PLAYBACK, LOC + "ResizePIPWindow -- loc "<<loc);
            if (loc != kPIP_END)
            {
                rect = mctx->nvp->getVideoOutput()->GetPIPRect(
                    loc, ctx->nvp, false);
            }
        }
        mctx->UnlockDeleteNVP(__FILE__, __LINE__);
        ctx->UnlockDeleteNVP(__FILE__, __LINE__);

        if (rect.isValid())
        {
            ctx->ResizePIPWindow(rect);
            VERBOSE(VB_PLAYBACK, LOC + "ResizePIPWindow -- end : ok");
            return true;
        }
    }
    VERBOSE(VB_PLAYBACK, LOC + "ResizePIPWindow -- end : !ok");
    return false;
}

bool TV::IsPBPSupported(const PlayerContext *ctx) const
{
    const PlayerContext *mctx = NULL;
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
    const PlayerContext *mctx = NULL;
    if (ctx)
        mctx = GetPlayer(ctx, 0);
    else
        mctx = GetPlayerReadLock(0, __FILE__, __LINE__);

    bool yes = mctx->IsPIPSupported();

    if (!ctx)
        ReturnPlayerLock(mctx);

    return yes;
}

/** \brief Teardown all NVP's in preparation for PxP Swap or
 *         change from PIP -> PBP or PBP -> PIP
 */
vector<long long> TV::TeardownAllNVPs(PlayerContext *lctx)
{
    vector<long long> pos;
    for (uint i = 0; i < player.size(); i++)
    {
        const PlayerContext *ctx = GetPlayer(lctx, i);
        ctx->LockDeleteNVP(__FILE__, __LINE__);
        pos.push_back((ctx->nvp) ? ctx->nvp->GetFramesPlayed() : 0);
        ctx->UnlockDeleteNVP(__FILE__, __LINE__);
    }

    for (uint i = 0; i < player.size(); i++)
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
void TV::PBPRestartMainNVP(PlayerContext *mctx)
{
    VERBOSE(VB_PLAYBACK, LOC  + "PBPRestartMainNVP -- begin");

    if (!mctx->IsNVPPlaying() ||
        mctx->GetPIPState() != kPBPLeft || exitPlayerTimerId)
    {
        VERBOSE(VB_PLAYBACK, LOC  + "PBPRestartMainNVP -- end !ok !valid");
        return;
    }

    mctx->LockDeleteNVP(__FILE__, __LINE__);
    long long mctx_frame = (mctx->nvp) ? mctx->nvp->GetFramesPlayed() : 0;
    mctx->UnlockDeleteNVP(__FILE__, __LINE__);

    mctx->PIPTeardown();
    mctx->SetPIPState(kPIPOff);
    mctx->buffer->Seek(0, SEEK_SET);

    if (mctx->CreateNVP(this, GetMythMainWindow(), mctx->GetState(),
                        mctx->embedWinID, &mctx->embedBounds))
    {
        ScheduleStateChange(mctx);
        mctx->StartOSD(this);
        mctx->LockDeleteNVP(__FILE__, __LINE__);
        if (mctx->nvp)
            mctx->nvp->JumpToFrame(mctx_frame);
        mctx->UnlockDeleteNVP(__FILE__, __LINE__);
        SetSpeedChangeTimer(25, __LINE__);
        VERBOSE(VB_PLAYBACK, LOC + "PBPRestartMainNVP -- end ok");
        return;
    }

    ForceNextStateNone(mctx);
    VERBOSE(VB_PLAYBACK, LOC +
            "PBPRestartMainNVP -- end !ok NVP did not restart");
}

/**
* \brief Recreate Main and PIP windows. Could be either PIP or PBP views.
*/
void TV::RestartAllNVPs(PlayerContext *lctx,
                        const vector<long long> &pos,
                        MuteState mctx_mute)
{
    QString loc = LOC + QString("RestartAllNVPs(): ");

    PlayerContext *mctx = GetPlayer(lctx, 0);

    if (!mctx)
        return;

    mctx->buffer->Seek(0, SEEK_SET);

    if (StateIsLiveTV(mctx->GetState()))
        mctx->buffer->Unpause();

    bool ok = StartPlayer(mctx, mctx, mctx->GetState());

    if (ok)
    {
        mctx->LockDeleteNVP(__FILE__, __LINE__);
        if (mctx->nvp)
            mctx->nvp->JumpToFrame(pos[0]);
        mctx->UnlockDeleteNVP(__FILE__, __LINE__);
    }
    else
    {
        VERBOSE(VB_IMPORTANT, loc +
                "Failed to restart new main context (was pip context)");
        ForceNextStateNone(mctx);
        return;
    }

    for (uint i = 1; i < player.size(); i++)
    {
        PlayerContext *pipctx = GetPlayer(lctx, i);

        pipctx->buffer->Seek(0, SEEK_SET);

        if (StateIsLiveTV(pipctx->GetState()))
            pipctx->buffer->Unpause();

        ok = StartPlayer(mctx, pipctx, pipctx->GetState());

        if (ok)
        {
            pipctx->LockDeleteNVP(__FILE__, __LINE__);
            if (pipctx->nvp)
            {
                pipctx->nvp->SetMuted(true);
                pipctx->nvp->JumpToFrame(pos[i]);
            }
            pipctx->UnlockDeleteNVP(__FILE__, __LINE__);
        }
        else
        { // TODO print OSD informing user of Swap failure ?
            VERBOSE(VB_IMPORTANT, loc +
                    "Failed to restart new pip context (was main context)");
            ForceNextStateNone(pipctx);
        }
    }

    // If old main player had a kMuteAll | kMuteOff setting,
    // apply old main player's mute setting to new main player.
    mctx->LockDeleteNVP(__FILE__, __LINE__);
    if (mctx->nvp && ((kMuteAll == mctx_mute) || (kMuteOff == mctx_mute)))
        mctx->nvp->SetMuteState(mctx_mute);
    mctx->UnlockDeleteNVP(__FILE__, __LINE__);
}

void TV::PxPSwap(PlayerContext *mctx, PlayerContext *pipctx)
{
    if (!mctx || !pipctx)
        return;

    VERBOSE(VB_PLAYBACK, LOC + "PxPSwap -- begin");
    if (mctx == pipctx)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "PxPSwap -- need two contexts");
        return;
    }

    lockTimerOn = false;

    multi_lock(&mctx->deleteNVPLock, &pipctx->deleteNVPLock, NULL);
    if (!mctx->nvp   || !mctx->nvp->IsPlaying() ||
        !pipctx->nvp || !pipctx->nvp->IsPlaying())
    {
        mctx->deleteNVPLock.unlock();
        pipctx->deleteNVPLock.unlock();
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "PxPSwap -- an nvp is not playing");
        return;
    }

    MuteState mctx_mute = mctx->nvp->GetMuteState();
    mctx->deleteNVPLock.unlock();
    pipctx->deleteNVPLock.unlock();

    int ctx_index = find_player_index(pipctx);

    vector<long long> pos = TeardownAllNVPs(mctx);

    swap(player[0],           player[ctx_index]);
    swap(pos[0],              pos[ctx_index]);
    swap(player[0]->pipState, player[ctx_index]->pipState);
    playerActive = (ctx_index == playerActive) ?
        0 : ((ctx_index == 0) ? ctx_index : playerActive);

    RestartAllNVPs(mctx, pos, mctx_mute);

    SetActive(mctx, playerActive, false);

    VERBOSE(VB_PLAYBACK, LOC + "PxPSwap -- end");
}

void TV::RestartMainNVP(PlayerContext *mctx)
{
    if (!mctx)
        return;

    VERBOSE(VB_PLAYBACK, LOC + "Restart main player -- begin");
    lockTimerOn = false;

    mctx->LockDeleteNVP(__FILE__, __LINE__);
    if (!mctx->nvp)
    {
        mctx->deleteNVPLock.unlock();
        return;
    }

    MuteState mctx_mute = mctx->nvp->GetMuteState();

    // HACK - FIXME
    // workaround muted audio when NVP is re-created
    mctx_mute = kMuteOff;
    // FIXME - end
    mctx->deleteNVPLock.unlock();

    vector<long long> pos = TeardownAllNVPs(mctx);
    RestartAllNVPs(mctx, pos, mctx_mute);
    SetActive(mctx, playerActive, false);

    VERBOSE(VB_PLAYBACK, LOC + "Restart main player -- end");
}

void TV::DoPlay(PlayerContext *ctx)
{
    ctx->LockDeleteNVP(__FILE__, __LINE__);
    if (!ctx->nvp)
    {
        ctx->UnlockDeleteNVP(__FILE__, __LINE__);
        return;
    }

    float time = 0.0;

    if (ctx->ff_rew_state)
    {
        time = StopFFRew(ctx);
        ctx->nvp->Play(ctx->ts_normal, true);
        ctx->ff_rew_speed = 0;
    }
    else if (ctx->paused || (ctx->ff_rew_speed != 0))
    {
        ctx->nvp->Play(ctx->ts_normal, true);
        ctx->paused = false;
        ctx->ff_rew_speed = 0;
    }
    ctx->UnlockDeleteNVP(__FILE__, __LINE__);

    DoNVPSeek(ctx, time);
    UpdateOSDSeekMessage(ctx, ctx->GetPlayMessage(), osd_general_timeout);

    GetMythUI()->DisableScreensaver();

    SetSpeedChangeTimer(0, __LINE__);
}

float TV::DoTogglePauseStart(PlayerContext *ctx)
{
    if (!ctx)
        return 0.0f;

    if (ctx->buffer && ctx->buffer->InDVDMenuOrStillFrame())
        return 0.0f;

    ctx->ff_rew_speed = 0;
    float time = 0.0f;

    ctx->LockDeleteNVP(__FILE__, __LINE__);
    if (!ctx->nvp)
    {
        ctx->UnlockDeleteNVP(__FILE__, __LINE__);
        return 0.0f;
    }
    if (ctx->paused)
    {
        ctx->nvp->Play(ctx->ts_normal, true);
    }
    else
    {
        if (ctx->ff_rew_state)
        {
            time = StopFFRew(ctx);
            ctx->nvp->Play(ctx->ts_normal, true);
            usleep(1000);
        }

        ctx->nvp->Pause();
    }
    ctx->UnlockDeleteNVP(__FILE__, __LINE__);

    ctx->paused = !ctx->paused;

    return time;
}

void TV::DoTogglePauseFinish(PlayerContext *ctx, float time, bool showOSD)
{
    if (!ctx || !ctx->HasNVP())
        return;

    if (ctx->buffer && ctx->buffer->InDVDMenuOrStillFrame())
        return;

    if (ctx->paused)
    {
        if (ctx->buffer)
            ctx->buffer->WaitForPause();

        DoNVPSeek(ctx, time);

        if (showOSD && ctx == player[0])
            UpdateOSDSeekMessage(ctx, tr("Paused"), -1);
        else if (showOSD)
            UpdateOSDSeekMessage(ctx, tr("Aux Paused"), -1);

        RestoreScreenSaver(ctx);
    }
    else
    {
        DoNVPSeek(ctx, time);
        if (showOSD)
            UpdateOSDSeekMessage(ctx,
                                 ctx->GetPlayMessage(),
                                 osd_general_timeout);
        GetMythUI()->DisableScreensaver();
    }

    SetSpeedChangeTimer(0, __LINE__);
}

void TV::DoTogglePause(PlayerContext *ctx, bool showOSD)
{
    DoTogglePauseFinish(ctx, DoTogglePauseStart(ctx), showOSD);
}

bool TV::DoNVPSeek(PlayerContext *ctx, float time)
{
    if (time > -0.001f && time < +0.001f)
        return false;

    VERBOSE(VB_PLAYBACK, LOC + "DoNVPSeek() -- begin");

    bool muted = false;

    ctx->LockDeleteNVP(__FILE__, __LINE__);
    if (!ctx->nvp)
    {
        ctx->UnlockDeleteNVP(__FILE__, __LINE__);
        return false;
    }

    if (ctx == GetPlayer(ctx, 0))
        muted = MuteChannelChange(ctx);

    bool res = false;

    if (LONG_LONG_MIN != audiosyncBaseline)
    {
        long long aud_tc = ctx->nvp->GetAudioTimecodeOffset();
        ctx->nvp->SaveAudioTimecodeOffset(aud_tc - audiosyncBaseline);
    }

    if (time > 0.0f)
    {
        VERBOSE(VB_PLAYBACK, LOC + "DoNVPSeek() -- ff");
        res = ctx->nvp->FastForward(time);
    }
    else if (time < 0.0)
    {
        VERBOSE(VB_PLAYBACK, LOC + "DoNVPSeek() -- rew");
        res = ctx->nvp->Rewind(-time);
    }
    ctx->UnlockDeleteNVP(__FILE__, __LINE__);

    if (muted)
        SetMuteTimer(ctx, kMuteTimeout);

    VERBOSE(VB_PLAYBACK, LOC + "DoNVPSeek() -- end");

    return res;
}

bool TV::SeekHandleAction(PlayerContext *actx, const QStringList &actions,
                          const bool isDVD)
{
    const int kRewind = 4, kForward = 8, kSticky = 16, kSlippery = 32,
              kRelative = 64, kAbsolute = 128, kWhenceMask = 3;
    int flags = 0;
    if (has_action("SEEKFFWD", actions))
        flags = ARBSEEK_FORWARD | kForward | kSlippery | kRelative;
    else if (has_action("FFWDSTICKY", actions))
        flags = ARBSEEK_END     | kForward | kSticky   | kAbsolute;
    else if (has_action("RIGHT", actions))
        flags = ARBSEEK_FORWARD | kForward | kSticky   | kRelative;
    else if (has_action("SEEKRWND", actions))
        flags = ARBSEEK_REWIND  | kRewind  | kSlippery | kRelative;
    else if (has_action("RWNDSTICKY", actions))
        flags = ARBSEEK_SET     | kRewind  | kSticky   | kAbsolute;
    else if (has_action("LEFT", actions))
        flags = ARBSEEK_REWIND  | kRewind  | kSticky   | kRelative;
    else
        return false;

    int direction = (flags & kRewind) ? -1 : 1;
    if (HasQueuedInput())
    {
        DoArbSeek(actx, static_cast<ArbSeekWhence>(flags & kWhenceMask));
    }
    else if (actx->paused)
    {
        if (!isDVD)
        {
            float time = (flags & kAbsolute) ?  direction :
                             direction * (1.001 / actx->last_framerate);
            QString message = (flags & kRewind) ? QString(tr("Rewind")) :
                                                 QString(tr("Forward"));
            DoSeek(actx, time, message);
        }
    }
    else if (flags & kSticky)
    {
        ChangeFFRew(actx, direction);
    }
    else if (flags & kRewind)
    {
            if (smartForward)
                doSmartForward = true;
            DoSeek(actx, -actx->rewtime, tr("Skip Back"));
    }
    else
    {
        if (smartForward & doSmartForward)
            DoSeek(actx, actx->rewtime, tr("Skip Ahead"));
        else
            DoSeek(actx, actx->fftime, tr("Skip Ahead"));
    }
    return true;
}

void TV::DoSeek(PlayerContext *ctx, float time, const QString &mesg)
{
    bool limitkeys = false;

    ctx->LockDeleteNVP(__FILE__, __LINE__);
    if (ctx->nvp && ctx->nvp->GetLimitKeyRepeat())
        limitkeys = true;
    ctx->UnlockDeleteNVP(__FILE__, __LINE__);

    if (!limitkeys || (keyRepeatTimer.elapsed() > (int)kKeyRepeatTimeout))
    {
        keyRepeatTimer.start();
        NormalSpeed(ctx);
        time += StopFFRew(ctx);
        DoNVPSeek(ctx, time);
        UpdateOSDSeekMessage(ctx, mesg, osd_general_timeout);
    }
}

void TV::DoArbSeek(PlayerContext *ctx, ArbSeekWhence whence)
{
    bool ok = false;
    int seek = GetQueuedInputAsInt(&ok);
    ClearInputQueues(ctx, true);
    if (!ok)
        return;

    float time = ((seek / 100) * 3600) + ((seek % 100) * 60);

    if (whence == ARBSEEK_FORWARD)
        DoSeek(ctx, time, tr("Jump Ahead"));
    else if (whence == ARBSEEK_REWIND)
        DoSeek(ctx, -time, tr("Jump Back"));
    else
    {
        ctx->LockDeleteNVP(__FILE__, __LINE__);
        if (!ctx->nvp)
        {
            ctx->UnlockDeleteNVP(__FILE__, __LINE__);
            return;
        }
        if (whence == ARBSEEK_END)
            time = (ctx->nvp->CalcMaxFFTime(LONG_MAX, false) /
                    ctx->last_framerate) - time;
        else
            time = time - (ctx->nvp->GetFramesPlayed() - 1) /
                    ctx->last_framerate;
        ctx->UnlockDeleteNVP(__FILE__, __LINE__);
        DoSeek(ctx, time, tr("Jump To"));
    }
}

void TV::NormalSpeed(PlayerContext *ctx)
{
    if (!ctx->ff_rew_speed)
        return;

    ctx->ff_rew_speed = 0;

    ctx->LockDeleteNVP(__FILE__, __LINE__);
    if (ctx->nvp)
        ctx->nvp->Play(ctx->ts_normal, true);
    ctx->UnlockDeleteNVP(__FILE__, __LINE__);

    SetSpeedChangeTimer(0, __LINE__);
}

void TV::ChangeSpeed(PlayerContext *ctx, int direction)
{
    int old_speed = ctx->ff_rew_speed;

    if (ctx->paused)
        ctx->ff_rew_speed = -4;

    ctx->ff_rew_speed += direction;

    float time = StopFFRew(ctx);
    float speed;
    QString mesg;

    switch (ctx->ff_rew_speed)
    {
        case  4: speed = 16.0;     mesg = QString(tr("Speed 16X"));   break;
        case  3: speed = 8.0;      mesg = QString(tr("Speed 8X"));    break;
        case  2: speed = 3.0;      mesg = QString(tr("Speed 3X"));    break;
        case  1: speed = 2.0;      mesg = QString(tr("Speed 2X"));    break;
        case  0: speed = 1.0;      mesg = ctx->GetPlayMessage();      break;
        case -1: speed = 1.0 / 3;  mesg = QString(tr("Speed 1/3X"));  break;
        case -2: speed = 1.0 / 8;  mesg = QString(tr("Speed 1/8X"));  break;
        case -3: speed = 1.0 / 16; mesg = QString(tr("Speed 1/16X")); break;
        case -4:
            DoTogglePause(ctx, true);
            return;
        default:
            ctx->ff_rew_speed = old_speed;
            return;
    }

    ctx->LockDeleteNVP(__FILE__, __LINE__);
    if (ctx->nvp && !ctx->nvp->Play(
            (!ctx->ff_rew_speed) ? ctx->ts_normal: speed, !ctx->ff_rew_speed))
    {
        ctx->ff_rew_speed = old_speed;
        ctx->UnlockDeleteNVP(__FILE__, __LINE__);
        return;
    }
    ctx->UnlockDeleteNVP(__FILE__, __LINE__);

    ctx->paused = false;
    DoNVPSeek(ctx, time);
    UpdateOSDSeekMessage(ctx, mesg, osd_general_timeout);

    SetSpeedChangeTimer(0, __LINE__);
}

float TV::StopFFRew(PlayerContext *ctx)
{
    float time = 0.0;

    if (!ctx->ff_rew_state)
        return time;

    if (ctx->ff_rew_state > 0)
        time = -ff_rew_speeds[ctx->ff_rew_index] * ff_rew_repos;
    else
        time = ff_rew_speeds[ctx->ff_rew_index] * ff_rew_repos;

    ctx->ff_rew_state = 0;
    ctx->ff_rew_index = kInitFFRWSpeed;

    ctx->LockDeleteNVP(__FILE__, __LINE__);
    if (ctx->nvp)
        ctx->nvp->Play(ctx->ts_normal, true);
    ctx->UnlockDeleteNVP(__FILE__, __LINE__);

    SetSpeedChangeTimer(0, __LINE__);

    return time;
}

void TV::ChangeFFRew(PlayerContext *ctx, int direction)
{
    if (ctx->ff_rew_state == direction)
    {
        while (++ctx->ff_rew_index < (int)ff_rew_speeds.size())
            if (ff_rew_speeds[ctx->ff_rew_index])
                break;
        if (ctx->ff_rew_index >= (int)ff_rew_speeds.size())
            ctx->ff_rew_index = kInitFFRWSpeed;
        SetFFRew(ctx, ctx->ff_rew_index);
    }
    else if (!ff_rew_reverse && ctx->ff_rew_state == -direction)
    {
        while (--ctx->ff_rew_index >= kInitFFRWSpeed)
            if (ff_rew_speeds[ctx->ff_rew_index])
                break;
        if (ctx->ff_rew_index >= kInitFFRWSpeed)
            SetFFRew(ctx, ctx->ff_rew_index);
        else
        {
            float time = StopFFRew(ctx);
            DoNVPSeek(ctx, time);
            UpdateOSDSeekMessage(ctx, ctx->GetPlayMessage(),
                                 osd_general_timeout);
        }
    }
    else
    {
        NormalSpeed(ctx);
        ctx->paused = false;
        ctx->ff_rew_state = direction;
        SetFFRew(ctx, kInitFFRWSpeed);
    }
}

void TV::SetFFRew(PlayerContext *ctx, int index)
{
    if (!ctx->ff_rew_state)
    {
        return;
    }

    if (!ff_rew_speeds[index])
    {
        return;
    }

    ctx->ff_rew_index = index;
    int speed;

    QString mesg;
    if (ctx->ff_rew_state > 0)
    {
        mesg = tr("Forward %1X").arg(ff_rew_speeds[ctx->ff_rew_index]);
        speed = ff_rew_speeds[ctx->ff_rew_index];
    }
    else
    {
        mesg = tr("Rewind %1X").arg(ff_rew_speeds[ctx->ff_rew_index]);
        speed = -ff_rew_speeds[ctx->ff_rew_index];
    }

    ctx->LockDeleteNVP(__FILE__, __LINE__);
    if (ctx->nvp)
        ctx->nvp->Play((float)speed, (speed == 1) && (ctx->ff_rew_state > 0));
    ctx->UnlockDeleteNVP(__FILE__, __LINE__);

    UpdateOSDSeekMessage(ctx, mesg, -1);

    SetSpeedChangeTimer(0, __LINE__);
}

void TV::DoQueueTranscode(PlayerContext *ctx, QString profile)
{
    ctx->LockPlayingInfo(__FILE__, __LINE__);

    if (ctx->GetState() == kState_WatchingPreRecorded)
    {
        bool stop = false;
        if (queuedTranscode)
            stop = true;
        else if (JobQueue::IsJobQueuedOrRunning(
                     JOB_TRANSCODE,
                     ctx->playingInfo->GetChanID(),
                     ctx->playingInfo->GetRecordingStartTime()))
        {
            stop = true;
        }

        if (stop)
        {
            JobQueue::ChangeJobCmds(
                JOB_TRANSCODE,
                ctx->playingInfo->GetChanID(),
                ctx->playingInfo->GetRecordingStartTime(), JOB_STOP);
            queuedTranscode = false;
            OSD *osd = GetOSDLock(ctx);
            if (osd)
                osd->SetSettingsText(tr("Stopping Transcode"), 3);
            ReturnOSDLock(ctx, osd);
        }
        else
        {
            const RecordingInfo recinfo(*ctx->playingInfo);
            recinfo.ApplyTranscoderProfileChange(profile);
            QString jobHost = "";

            if (db_run_jobs_on_remote)
                jobHost = ctx->playingInfo->GetHostname();

            OSD *osd = GetOSDLock(ctx);
            if (JobQueue::QueueJob(
                    JOB_TRANSCODE,
                    ctx->playingInfo->GetChanID(),
                    ctx->playingInfo->GetRecordingStartTime(),
                    jobHost, "", "", JOB_USE_CUTLIST))
            {
                queuedTranscode = true;
                if (osd)
                    osd->SetSettingsText(tr("Transcoding"), 3);
            } else {
                if (osd)
                    osd->SetSettingsText(tr("Try Again"), 3);
            }
            ReturnOSDLock(ctx, osd);
        }
    }
    ctx->UnlockPlayingInfo(__FILE__, __LINE__);
}

int TV::GetNumChapters(const PlayerContext *ctx) const
{
    int num_chapters = 0;
    ctx->LockDeleteNVP(__FILE__, __LINE__);
    if (ctx->nvp)
        num_chapters = ctx->nvp->GetNumChapters();
    ctx->UnlockDeleteNVP(__FILE__, __LINE__);
    return num_chapters;
}

void TV::GetChapterTimes(const PlayerContext *ctx, QList<long long> &times) const
{
    ctx->LockDeleteNVP(__FILE__, __LINE__);
    if (ctx->nvp)
        ctx->nvp->GetChapterTimes(times);
    ctx->UnlockDeleteNVP(__FILE__, __LINE__);
}

int TV::GetCurrentChapter(const PlayerContext *ctx) const
{
    int chapter = 0;
    ctx->LockDeleteNVP(__FILE__, __LINE__);
    if (ctx->nvp)
        chapter = ctx->nvp->GetCurrentChapter();
    ctx->UnlockDeleteNVP(__FILE__, __LINE__);
    return chapter;
}

void TV::DoJumpChapter(PlayerContext *ctx, int chapter)
{
    NormalSpeed(ctx);
    StopFFRew(ctx);

    ctx->LockDeleteNVP(__FILE__, __LINE__);
    bool muted = MuteChannelChange(ctx);
    ctx->UnlockDeleteNVP(__FILE__, __LINE__);

    struct StatusPosInfo posInfo;
    ctx->CalcNVPSliderPosition(posInfo);

    bool slidertype = false;

    OSD *osd = GetOSDLock(ctx);
    if (osd)
    {
        posInfo.desc = tr("Searching...");
        osd->ShowStatus(posInfo, slidertype, tr("Jump Chapter"), 3);
        SetUpdateOSDPosition(true);
    }
    ReturnOSDLock(ctx, osd);

    ctx->LockDeleteNVP(__FILE__, __LINE__);
    if (ctx->nvp)
        ctx->nvp->JumpChapter(chapter);
    ctx->UnlockDeleteNVP(__FILE__, __LINE__);

    if (muted)
        SetMuteTimer(ctx, kMuteTimeout);
}

void TV::DoSkipCommercials(PlayerContext *ctx, int direction)
{
    NormalSpeed(ctx);
    StopFFRew(ctx);

    if (StateIsLiveTV(GetState(ctx)))
        return;

    ctx->LockDeleteNVP(__FILE__, __LINE__);
    bool muted = MuteChannelChange(ctx);
    ctx->UnlockDeleteNVP(__FILE__, __LINE__);

    struct StatusPosInfo posInfo;
    ctx->CalcNVPSliderPosition(posInfo);

    bool slidertype = false;

    OSD *osd = GetOSDLock(ctx);
    if (osd)
    {
        posInfo.desc = tr("Searching...");
        osd->ShowStatus(posInfo, slidertype, tr("Skip"), 6);
        SetUpdateOSDPosition(true);
    }
    ReturnOSDLock(ctx, osd);

    ctx->LockDeleteNVP(__FILE__, __LINE__);
    if (ctx->nvp)
        ctx->nvp->SkipCommercials(direction);
    ctx->UnlockDeleteNVP(__FILE__, __LINE__);

    if (muted)
        SetMuteTimer(ctx, kMuteTimeout);
}

void TV::SwitchSource(PlayerContext *ctx, uint source_direction)
{
    QMap<uint,InputInfo> sources;
    vector<uint> cardids = RemoteRequestFreeRecorderList();
    uint         cardid  = ctx->GetCardID();
    cardids.push_back(cardid);
    stable_sort(cardids.begin(), cardids.end());

    vector<uint> excluded_cardids;
    excluded_cardids.push_back(cardid);

    InfoMap info;
    ctx->recorder->GetChannelInfo(info);
    uint sourceid = info["sourceid"].toUInt();

    vector<uint>::const_iterator it = cardids.begin();
    for (; it != cardids.end(); ++it)
    {
        vector<InputInfo> inputs = RemoteRequestFreeInputList(
            *it, excluded_cardids);

        if (inputs.empty())
            continue;

        for (uint i = 0; i < inputs.size(); i++)
        {
            // prefer the current card's input in sources list
            if ((sources.find(inputs[i].sourceid) == sources.end()) ||
                ((cardid == inputs[i].cardid) &&
                 (cardid != sources[inputs[i].sourceid].cardid)))
            {
                sources[inputs[i].sourceid] = inputs[i];
            }
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
        sit++;
        if (sit == sources.end())
            sit = sources.begin();
    }

    if (kPreviousSource == source_direction)
    {
        if (sit != sources.begin())
            sit--;
        else
        {
            QMap<uint,InputInfo>::const_iterator tmp = sources.begin();
            while (tmp != sources.end())
            {
                sit = tmp;
                tmp++;
            }
        }
    }

    if (sit == beg)
    {
        return;
    }

    switchToInputId = (*sit).inputid;

    QMutexLocker locker(&timerIdLock);
    if (!switchToInputTimerId)
        switchToInputTimerId = StartTimer(1, __LINE__);
}

void TV::SwitchInputs(PlayerContext *ctx, uint inputid)
{
    if (!ctx->recorder)
    {
        return;
    }

    VERBOSE(VB_PLAYBACK, LOC + QString("SwitchInputd(%1)").arg(inputid));

    if ((uint)ctx->GetCardID() == CardUtil::GetCardID(inputid))
    {
        ToggleInputs(ctx, inputid);
    }
    else
    {
        SwitchCards(ctx, 0, QString::null, inputid);
    }
}

void TV::SwitchCards(PlayerContext *ctx,
                     uint chanid, QString channum, uint inputid)
{
    VERBOSE(VB_PLAYBACK, LOC +
            QString("SwitchCards(%1,'%2',%3)")
            .arg(chanid).arg(channum).arg(inputid));

    RemoteEncoder *testrec = NULL;

    if (!StateIsLiveTV(GetState(ctx)))
    {
        return;
    }

    // If we are switching to a channel not on the current recorder
    // we need to find the next free recorder with that channel.
    QStringList reclist;
    if (!channum.isEmpty())
        reclist = ChannelUtil::GetValidRecorderList(chanid, channum);
    else if (inputid)
    {
        uint cardid = CardUtil::GetCardID(inputid);
        if (cardid)
            reclist.push_back(QString::number(cardid));

        // now we need to set our channel as the starting channel..
        if (testrec && testrec->IsValidRecorder())
        {
            QString inputname("");
            int cardid = testrec->GetRecorderNumber();
            int cardinputid = CardUtil::GetCardInputID(
                    cardid, channum, inputname);

            VERBOSE(VB_CHANNEL, LOC + "Setting startchan: " +
                    QString("cardid(%1) cardinputid(%2) channum(%3)")
                    .arg(cardid).arg(cardinputid).arg(channum));

            if (cardid >= 0 && cardinputid >=0 && !inputname.isEmpty())
            {
                CardUtil::SetStartChannel(cardinputid, channum);
                CardUtil::SetStartInput(cardid, inputname);
            }
        }
    }
    if (!reclist.empty())
        testrec = RemoteRequestFreeRecorderFromList(reclist);

    // If we are just switching recorders find first available recorder.
    if (!testrec)
        testrec = RemoteRequestNextFreeRecorder(ctx->GetCardID());

    if (testrec && testrec->IsValidRecorder())
    {
        // Switching cards so clear the pseudoLiveTVState.
        ctx->SetPseudoLiveTV(NULL, kPseudoNormalLiveTV);

        PlayerContext *mctx = GetPlayer(ctx, 0);
        if (mctx != ctx)
            PIPRemovePlayer(mctx, ctx);

        bool muted = false;
        ctx->LockDeleteNVP(__FILE__, __LINE__);
        if (ctx->nvp && ctx->nvp->IsMuted())
            muted = true;
        ctx->UnlockDeleteNVP(__FILE__, __LINE__);

        // pause the decoder first, so we're not reading too close to the end.
        ctx->buffer->IgnoreLiveEOF(true);
        ctx->buffer->StopReads();
        ctx->nvp->PauseDecoder();

        // shutdown stuff
        ctx->buffer->Pause();
        ctx->buffer->WaitForPause();
        ctx->nvp->StopPlaying();
        ctx->recorder->StopLiveTV();
        ctx->SetNVP(NULL);

        // now restart stuff
        ctx->lastSignalUIInfo.clear();
        lockTimerOn = false;

        ctx->SetRecorder(testrec);
        ctx->recorder->Setup();
        ctx->recorder->SpawnLiveTV(ctx->tvchain->GetID(), false, channum);

        if (!ctx->ReloadTVChain())
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "LiveTV not successfully restarted");
            RestoreScreenSaver(ctx);
            ctx->SetRecorder(NULL);
            SetErrored(ctx);
            SetExitPlayer(true, false);
        }
        else
        {
            ctx->LockPlayingInfo(__FILE__, __LINE__);
            QString playbackURL = ctx->playingInfo->GetPlaybackURL(true);
            bool opennow = (ctx->tvchain->GetCardType(-1) != "DUMMY");
            ctx->SetRingBuffer(new RingBuffer(playbackURL, false, true,
                                              opennow ? 12 : (uint)-1));

            ctx->tvchain->SetProgram(*ctx->playingInfo);
            ctx->buffer->SetLiveMode(ctx->tvchain);
            ctx->UnlockPlayingInfo(__FILE__, __LINE__);
        }

        bool ok = false;
        if (ctx->playingInfo && StartRecorder(ctx,-1))
        {
            // Cache starting frame rate for this recorder
            ctx->last_framerate = ctx->recorder->GetFrameRate();
            PlayerContext *mctx = GetPlayer(ctx, 0);

            if (ctx->CreateNVP(
                    this, GetMythMainWindow(), ctx->GetState(),
                    mctx->embedWinID, &mctx->embedBounds, muted))
            {
                ScheduleStateChange(ctx);
                ok = true;
                ctx->StartOSD(this);
                ctx->PushPreviousChannel();
                for (uint i = 1; i < player.size(); i++)
                    PIPAddPlayer(mctx, GetPlayer(ctx, i));

                SetSpeedChangeTimer(25, __LINE__);
            }
            else
                StopStuff(mctx, ctx, true, true, true);
        }

        if (!ok)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "LiveTV not successfully started");
            RestoreScreenSaver(ctx);
            ctx->SetRecorder(NULL);
            SetErrored(ctx);
            SetExitPlayer(true, false);
        }
        else
        {
            lockTimer.start();
            lockTimerOn = true;
        }
    }
    else
    {
        VERBOSE(VB_GENERAL, LOC + "No recorder to switch to...");
        delete testrec;
    }

    UpdateOSDInput(ctx);
    UnpauseLiveTV(ctx);

    ITVRestart(ctx, true);
}

void TV::ToggleInputs(PlayerContext *ctx, uint inputid)
{
    if (!ctx->recorder)
    {
        return;
    }

    // If Nuppel Video Player is paused, unpause it
    if (ctx->paused)
    {
        OSD *osd = GetOSDLock(ctx);
        if (osd)
            osd->EndStatus();
        ReturnOSDLock(ctx, osd);

        GetMythUI()->DisableScreensaver();
        ctx->paused = false;
    }

    const QString curinputname = ctx->recorder->GetInput();
    QString inputname = curinputname;

    uint cardid = ctx->GetCardID();
    vector<uint> excluded_cardids;
    excluded_cardids.push_back(cardid);
    vector<InputInfo> inputs = RemoteRequestFreeInputList(
        cardid, excluded_cardids);

    vector<InputInfo>::const_iterator it = inputs.end();

    if (inputid)
    {
        it = find(inputs.begin(), inputs.end(), inputid);
    }
    else
    {
        it = find(inputs.begin(), inputs.end(), inputname);
        if (it != inputs.end())
            it++;
    }

    if (it == inputs.end())
        it = inputs.begin();

    if (it != inputs.end())
        inputname = (*it).name;

    if (curinputname != inputname)
    {
        // Pause the backend recorder, send command, and then unpause..
        PauseLiveTV(ctx);
        lockTimerOn = false;
        inputname = ctx->recorder->SetInput(inputname);
        UnpauseLiveTV(ctx);
    }

    UpdateOSDInput(ctx, inputname);
}

void TV::ToggleChannelFavorite(PlayerContext *ctx, QString changroup_name)
{
    if (ctx->recorder)
        ctx->recorder->ToggleChannelFavorite(changroup_name);
}

QString TV::GetQueuedInput(void) const
{
    QMutexLocker locker(&timerIdLock);
    QString ret = queuedInput;
    ret.detach();
    return ret;
}

int TV::GetQueuedInputAsInt(bool *ok, int base) const
{
    QMutexLocker locker(&timerIdLock);
    return queuedInput.toInt(ok, base);
}

QString TV::GetQueuedChanNum(void) const
{
    QMutexLocker locker(&timerIdLock);

    if (queuedChanNum.isEmpty())
        return "";

    // strip initial zeros and other undesirable characters
    int i = 0;
    for (; i < queuedChanNum.length(); i++)
    {
        if ((queuedChanNum[i] > '0') && (queuedChanNum[i] <= '9'))
            break;
    }
    queuedChanNum = queuedChanNum.right(queuedChanNum.length() - i);

    // strip whitespace at end of string
    queuedChanNum = queuedChanNum.trimmed();

    QString ret = queuedChanNum;
    ret.detach();
    return ret;
}

/** \fn TV::ClearInputQueues(const PlayerContext*,bool)
 *  \brief Clear channel key buffer of input keys.
 *  \param hideosd if true, hides "channel_number" OSDSet.
 */
void TV::ClearInputQueues(const PlayerContext *ctx, bool hideosd)
{
    OSD *osd = GetOSDLock(ctx);
    if (hideosd && osd)
        osd->HideSet("channel_number");
    ReturnOSDLock(ctx, osd);

    QMutexLocker locker(&timerIdLock);
    queuedInput   = "";
    queuedChanNum = "";
    queuedChanID  = 0;
    if (queueInputTimerId)
    {
        KillTimer(queueInputTimerId);
        queueInputTimerId = 0;
    }
}

void TV::AddKeyToInputQueue(PlayerContext *ctx, char key)
{
    if (key)
    {
        QMutexLocker locker(&timerIdLock);
        queuedInput   = queuedInput.append(key).right(kInputKeysMax);
        queuedChanNum = queuedChanNum.append(key).right(kInputKeysMax);
        if (!queueInputTimerId)
            queueInputTimerId = StartTimer(10, __LINE__);
    }

    bool commitSmart = false;
    QString inputStr = GetQueuedInput();

    // Always use smartChannelChange when channel numbers are entered
    // in browse mode because in browse mode space/enter exit browse
    // mode and change to the currently browsed channel.
    if (StateIsLiveTV(GetState(ctx)) && !ccInputMode && !asInputMode &&
        (smartChannelChange || browsemode))
    {
        commitSmart = ProcessSmartChannel(ctx, inputStr);
    }

    // Handle OSD...
    inputStr = inputStr.isEmpty() ? "?" : inputStr;
    if (ccInputMode)
    {
        QString entryStr = (vbimode==VBIMode::PAL_TT) ? tr("TXT:") : tr("CC:");
        inputStr = entryStr + " " + inputStr;
    }
    else if (asInputMode)
        inputStr = tr("Seek:", "seek to location") + " " + inputStr;
    UpdateOSDTextEntry(ctx, inputStr);

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
        bool ok;
        chan.right(1).toUInt(&ok);
        if (!ok)
        {
            chan = chan.left(chan.length()-1);

            QMutexLocker locker(&timerIdLock);
            queuedChanNum = chan;
            if (!queueInputTimerId)
                queueInputTimerId = StartTimer(10, __LINE__);
        }
    }

    // Look for channel in line-up
    QString needed_spacer;
    uint    pref_cardid;
    bool    is_not_complete = true;

    bool valid_prefix = false;
    if (ctx->recorder)
    {
        valid_prefix = ctx->recorder->CheckChannelPrefix(
            chan, pref_cardid, is_not_complete, needed_spacer);
    }

#if DEBUG_CHANNEL_PREFIX
    VERBOSE(VB_IMPORTANT, QString("valid_pref(%1) cardid(%2) chan(%3) "
                                  "pref_cardid(%4) complete(%5) sp(%6)")
            .arg(valid_prefix).arg(0).arg(chan)
            .arg(pref_cardid).arg(is_not_complete).arg(needed_spacer));
#endif

    if (!valid_prefix)
    {
        // not a valid prefix.. reset...
        QMutexLocker locker(&timerIdLock);
        queuedChanNum = "";
    }
    else if (!needed_spacer.isEmpty())
    {
        // need a spacer..
        QMutexLocker locker(&timerIdLock);
        queuedChanNum = add_spacer(chan, needed_spacer);
    }

#if DEBUG_CHANNEL_PREFIX
    VERBOSE(VB_IMPORTANT, QString(" ValidPref(%1) CardId(%2) Chan(%3) "
                                  " PrefCardId(%4) Complete(%5) Sp(%6)")
            .arg(valid_prefix).arg(0).arg(GetQueuedChanNum())
            .arg(pref_cardid).arg(is_not_complete).arg(needed_spacer));
#endif

    QMutexLocker locker(&timerIdLock);
    inputStr = queuedChanNum;
    inputStr.detach();
    if (!queueInputTimerId)
        queueInputTimerId = StartTimer(10, __LINE__);

    return !is_not_complete;
}

void TV::UpdateOSDTextEntry(const PlayerContext *ctx, const QString &message)
{
    OSD *osd = GetOSDLock(ctx);
    if (!osd)
    {
        ReturnOSDLock(ctx, osd);
        return;
    }

    InfoMap infoMap;

    infoMap["channum"]  = message;
    infoMap["callsign"] = "";

    osd->ClearAllText("channel_number");
    osd->SetText("channel_number", infoMap, 2);

    ReturnOSDLock(ctx, osd);
}

bool TV::CommitQueuedInput(PlayerContext *ctx)
{
    bool commited = false;

    VERBOSE(VB_PLAYBACK, LOC + "CommitQueuedInput() " +
            QString("livetv(%1) qchannum(%2) qchanid(%3)")
            .arg(StateIsLiveTV(GetState(ctx)))
            .arg(GetQueuedChanNum())
            .arg(GetQueuedChanID()));

    if (ccInputMode)
    {
        commited = true;
        if (HasQueuedInput())
            HandleTrackAction(ctx, "TOGGLECC");
    }
    else if (asInputMode)
    {
        commited = true;
        if (HasQueuedInput())
            DoArbSeek(ctx, ARBSEEK_FORWARD);
    }
    else if (StateIsLiveTV(GetState(ctx)))
    {
        QString channum = GetQueuedChanNum();
        QString chaninput = GetQueuedInput();
        if (browsemode)
        {
            commited = true;
            if (channum.isEmpty())
            {
                channum = browsechannum;
                channum.detach();
            }
            BrowseChannel(ctx, channum);
            OSD *osd = GetOSDLock(ctx);
            if (osd)
                osd->HideSet("channel_number");
            ReturnOSDLock(ctx, osd);
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

void TV::ChangeChannel(PlayerContext *ctx, int direction)
{
    bool muted = false;

    if ((browse_changrp || (direction == CHANNEL_DIRECTION_FAVORITE)) &&
        (channel_group_id > -1))
    {
       uint    chanid;

       ctx->LockPlayingInfo(__FILE__, __LINE__);
       if (!ctx->playingInfo)
       {
           VERBOSE(VB_IMPORTANT,
                   LOC_ERR + "ChangeChannel(): no active ctx playingInfo.");
           ctx->UnlockPlayingInfo(__FILE__, __LINE__);
           ReturnPlayerLock(ctx);
           return;
       }

       // Collect channel info
       const ProgramInfo pginfo(*ctx->playingInfo);
       uint    old_chanid  = pginfo.GetChanID();
       ctx->UnlockPlayingInfo(__FILE__, __LINE__);

       chanid = ChannelUtil::GetNextChannel(m_channellist, old_chanid, 0, direction);

       ChangeChannel(ctx, chanid, "");
       return;
    }
    else if (direction == CHANNEL_DIRECTION_FAVORITE)
        direction = CHANNEL_DIRECTION_UP;

    QString oldinputname = ctx->recorder->GetInput();

    muted = MuteChannelChange(ctx);

    if (ctx->paused)
    {
        OSD *osd = GetOSDLock(ctx);
        if (osd)
            osd->EndStatus();
        GetMythUI()->DisableScreensaver();
        ctx->paused = false;
        ReturnOSDLock(ctx, osd);
    }

    // Save the current channel if this is the first time
    if (ctx->prevChan.empty())
        ctx->PushPreviousChannel();

    PauseLiveTV(ctx);

    ctx->LockDeleteNVP(__FILE__, __LINE__);
    if (ctx->nvp)
    {
        ctx->nvp->ResetCaptions();
        ctx->nvp->ResetTeletext();
    }
    ctx->UnlockDeleteNVP(__FILE__, __LINE__);

    ctx->recorder->ChangeChannel(direction);
    ClearInputQueues(ctx, false);

    if (muted)
        SetMuteTimer(ctx, kMuteTimeout * 2);

    UnpauseLiveTV(ctx);

    if (oldinputname != ctx->recorder->GetInput())
        UpdateOSDInput(ctx);
}

void TV::ChangeChannel(PlayerContext *ctx, uint chanid, const QString &chan)
{
    VERBOSE(VB_PLAYBACK, LOC + QString("ChangeChannel(%1, '%2') ")
            .arg(chanid).arg(chan));

    if ((!chanid && chan.isEmpty()) || !ctx || !ctx->recorder)
        return;

    QString channum = chan;
    QStringList reclist;
    bool muted = false;

    QString oldinputname = ctx->recorder->GetInput();

    if (channum.isEmpty() && chanid)
    {
        channum = ChannelUtil::GetChanNum(chanid);
    }

    bool getit = false;
    if (ctx->recorder)
    {
        if (ctx->pseudoLiveTVState == kPseudoRecording)
        {
            getit = true;
        }
        else if (chanid)
        {
            getit = ctx->recorder->ShouldSwitchToAnotherCard(
                QString::number(chanid));
        }
        else
        {
            QString needed_spacer;
            uint pref_cardid;
            uint cardid = ctx->GetCardID();
            bool dummy;

            ctx->recorder->CheckChannelPrefix(chan,  pref_cardid,
                                              dummy, needed_spacer);

            channum = add_spacer(chan, needed_spacer);
            getit = (pref_cardid != cardid);
        }

        if (getit)
            reclist = ChannelUtil::GetValidRecorderList(chanid, channum);
    }

    if (reclist.size())
    {
        RemoteEncoder *testrec = NULL;
        testrec = RemoteRequestFreeRecorderFromList(reclist);
        if (!testrec || !testrec->IsValidRecorder())
        {
            ClearInputQueues(ctx, true);
            ShowNoRecorderDialog(ctx);
            if (testrec)
                delete testrec;
            return;
        }

        if (!ctx->prevChan.empty() && ctx->prevChan.back() == channum)
        {
            // need to remove it if the new channel is the same as the old.
            ctx->prevChan.pop_back();
        }

        // found the card on a different recorder.
        delete testrec;
        // Save the current channel if this is the first time
        if (ctx->prevChan.empty())
            ctx->PushPreviousChannel();
        SwitchCards(ctx, chanid, channum);
        return;
    }

    if (getit || !ctx->recorder || !ctx->recorder->CheckChannel(channum))
        return;

    muted = MuteChannelChange(ctx);

    OSD *osd = GetOSDLock(ctx);
    if (osd && ctx->paused)
    {
        osd->EndStatus();
        GetMythUI()->DisableScreensaver();
        ctx->paused = false;
    }
    ReturnOSDLock(ctx, osd);

    // Save the current channel if this is the first time
    if (ctx->prevChan.empty())
        ctx->PushPreviousChannel();

    PauseLiveTV(ctx);

    ctx->LockDeleteNVP(__FILE__, __LINE__);
    if (ctx->nvp)
    {
        ctx->nvp->ResetCaptions();
        ctx->nvp->ResetTeletext();
    }
    ctx->UnlockDeleteNVP(__FILE__, __LINE__);

    ctx->recorder->SetChannel(channum);

    if (muted)
        SetMuteTimer(ctx, kMuteTimeout * 2);

    UnpauseLiveTV(ctx);

    if (oldinputname != ctx->recorder->GetInput())
        UpdateOSDInput(ctx);
}

void TV::ChangeChannel(const PlayerContext *ctx, const DBChanList &options)
{
    for (uint i = 0; i < options.size(); i++)
    {
        uint    chanid  = options[i].chanid;
        QString channum = options[i].channum;

        if (chanid && !channum.isEmpty() && IsTunable(ctx, chanid))
        {
            // hide the channel number, activated by certain signal monitors
            OSD *osd = GetOSDLock(ctx);
            if (osd)
                osd->HideSet("channel_number");
            ReturnOSDLock(ctx, osd);

            QMutexLocker locker(&timerIdLock);
            queuedInput   = channum; queuedInput.detach();
            queuedChanNum = channum; queuedChanNum.detach();
            queuedChanID  = chanid;
            if (!queueInputTimerId)
                queueInputTimerId = StartTimer(10, __LINE__);
            break;
        }
    }
}

void TV::ShowPreviousChannel(PlayerContext *ctx)
{
    QString channum = ctx->GetPreviousChannel();

    VERBOSE(VB_PLAYBACK, LOC + QString("ShowPreviousChannel: '%1'")
            .arg(channum));

    if (channum.isEmpty())
        return;

    // Display channel name in the OSD for up to 1 second.
    OSD *osd = GetOSDLock(ctx);
    if (osd)
    {
        osd->HideSet("program_info");

        InfoMap infoMap;

        infoMap["channum"] = channum;
        infoMap["callsign"] = "";
        osd->ClearAllText("channel_number");
        osd->SetText("channel_number", infoMap, 1);
    }
    ReturnOSDLock(ctx, osd);
}

void TV::PopPreviousChannel(PlayerContext *ctx, bool immediate_change)
{
    if (!ctx->tvchain)
        return;

    if (!immediate_change)
        ShowPreviousChannel(ctx);

    QString prev_channum = ctx->PopPreviousChannel();
    QString cur_channum  = ctx->tvchain->GetChannelName(-1);

    VERBOSE(VB_PLAYBACK, LOC + QString("PopPreviousChannel: '%1'->'%2'")
            .arg(cur_channum).arg(prev_channum));

    // Only change channel if previous channel != current channel
    if (cur_channum != prev_channum && !prev_channum.isEmpty())
    {
        QMutexLocker locker(&timerIdLock);
        queuedInput   = prev_channum; queuedInput.detach();
        queuedChanNum = prev_channum; queuedChanNum.detach();
        queuedChanID  = 0;
        if (!queueInputTimerId)
            queueInputTimerId = StartTimer(10, __LINE__);
    }

    if (immediate_change)
    {
        // Turn off OSD Channel Num so the channel changes right away
        OSD *osd = GetOSDLock(ctx);
        if (osd)
            osd->HideSet("channel_number");
        ReturnOSDLock(ctx, osd);
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
    if (osd && osd->HideAll())
        res = true;

    while (res && osd && osd->HideAll())
        usleep(1000);

    while (osd && osd->DialogShowing(kOSDDialogActive))
        osd->TurnDialogOff(kOSDDialogActive);

    ReturnOSDLock(ctx, osd);

    if (browsemode)
    {
        QMutexLocker locker(&timerIdLock);
        if (browseTimerId)
        {
            KillTimer(browseTimerId);
            browseTimerId = 0;
        }
        browsemode = false;
    }

    return res;
}

/** \fn TV::ToggleOSD(const PlayerContext*, bool includeStatus)
 *  \brief Cycle through the available Info OSDs.
 */
void TV::ToggleOSD(const PlayerContext *ctx, bool includeStatusOSD)
{
    OSD *osd = GetOSDLock(ctx);
    bool hideAll    = false;
    bool showStatus = false;
    if (ctx->paused || !osd)
    {
        ReturnOSDLock(ctx, osd);
        return;
    }
    bool is_status_disp          = osd->IsSetDisplaying("status");
    bool has_prog_info_small     = osd->HasSet("program_info_small");
    bool is_prog_info_small_disp = osd->IsSetDisplaying("program_info_small");
    bool has_prog_info           = osd->HasSet("program_info");
    bool is_prog_info_disp       = osd->IsSetDisplaying("program_info");
    bool has_prog_video          = osd->HasSet("program_info_video");
    bool has_prog_small_video    = osd->HasSet("program_info_small_video");
    bool has_prog_rec            = osd->HasSet("program_info_recording");
    bool has_prog_small_rec      = osd->HasSet("program_info_small_recording");

    ReturnOSDLock(ctx, osd);

    ctx->LockPlayingInfo(__FILE__, __LINE__);
    QString desc  = ctx->playingInfo->GetDescription();
    QString title = ctx->playingInfo->GetTitle();
    ctx->UnlockPlayingInfo(__FILE__, __LINE__);

    bool is_dvd = (ctx->buffer && ctx->buffer->isDVD()) ||
        ctx->playingInfo->IsVideoDVD();
    bool is_video     = ctx->playingInfo->IsVideo();
    bool is_recording = StateIsPlaying(ctx->GetState()) && !is_video;

    if (is_dvd && desc.isEmpty() && title.isEmpty())
    {
        // DVD toggles between status and nothing
        if (is_status_disp)
            hideAll = true;
        else
            showStatus = true;
    }
    else if (is_status_disp)
    {
        if (is_video && has_prog_small_video)
            UpdateOSDProgInfo(ctx, "program_info_small_video");
        else if (is_video && has_prog_video)
            UpdateOSDProgInfo(ctx, "program_info_video");
        else if (is_recording && has_prog_small_rec)
            UpdateOSDProgInfo(ctx, "program_info_small_recording");
        else if (is_recording && has_prog_rec)
            UpdateOSDProgInfo(ctx, "program_info_recording");
        else if (has_prog_info_small)
            UpdateOSDProgInfo(ctx, "program_info_small");
        else
            UpdateOSDProgInfo(ctx, "program_info");
    }
    else if (is_prog_info_small_disp)
    {
        // If small is displaying, show long if we have it, else hide info
        if (is_video && has_prog_video)
            UpdateOSDProgInfo(ctx, "program_info_video");
        if (is_recording && has_prog_rec)
            UpdateOSDProgInfo(ctx, "program_info_recording");
        else if (has_prog_info)
            UpdateOSDProgInfo(ctx, "program_info");
        else
            hideAll = true;
    }
    else if (is_prog_info_disp)
    {
        // If long is displaying, hide info
        hideAll = true;
    }
    else if (includeStatusOSD)
    {
        // If no program_info displaying, show status if we want it
        showStatus = true;
    }
    else
    {
        // No status desired? Nothing is up, Display small if we have,
        // else display long
        if (is_video && has_prog_small_video)
            UpdateOSDProgInfo(ctx, "program_info_small_video");
        if (is_video && has_prog_video)
            UpdateOSDProgInfo(ctx, "program_info_video");
        if (is_recording && has_prog_small_rec)
            UpdateOSDProgInfo(ctx, "program_info_small_recording");
        if (is_recording && has_prog_rec)
            UpdateOSDProgInfo(ctx, "program_info_recording");
        else if (has_prog_info_small)
            UpdateOSDProgInfo(ctx, "program_info_small");
        else
            UpdateOSDProgInfo(ctx, "program_info");
    }

    if (hideAll || showStatus)
    {
        OSD *osd = GetOSDLock(ctx);
        if (osd)
            osd->HideAll();
        ReturnOSDLock(ctx, osd);
    }

    if (showStatus)
    {
        struct StatusPosInfo posInfo;
        if (ctx->CalcNVPSliderPosition(posInfo))
        {
            osd = GetOSDLock(ctx);
            if (osd)
            {
                osd->ShowStatus(posInfo, false, tr("Position"),
                                osd_prog_info_timeout);
            }
            ReturnOSDLock(ctx, osd);
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

/** \fn TV::UpdateOSDProgInfo(const PlayerContext*, const char *whichInfo)
 *  \brief Update and display the passed OSD set with programinfo
 */
void TV::UpdateOSDProgInfo(const PlayerContext *ctx, const char *whichInfo)
{
    InfoMap infoMap;
    ctx->GetPlayingInfoMap(infoMap);

    // Clear previous osd and add new info
    OSD *osd = GetOSDLock(ctx);
    if (osd)
    {
        osd->ClearAllText(whichInfo);
        osd->HideAll();
        osd->SetText(whichInfo, infoMap, osd_prog_info_timeout);
    }
    ReturnOSDLock(ctx, osd);
}

void TV::UpdateOSDSeekMessage(const PlayerContext *ctx,
                              const QString &mesg, int disptime)
{
    VERBOSE(VB_PLAYBACK, QString("UpdateOSDSeekMessage(%1, %2)")
            .arg(mesg).arg(disptime));

    struct StatusPosInfo posInfo;
    if (ctx->CalcNVPSliderPosition(posInfo))
    {
        bool slidertype = StateIsLiveTV(GetState(ctx));
        int osdtype = (doSmartForward) ? kOSDFunctionalType_SmartForward :
            kOSDFunctionalType_Default;
        OSD *osd = GetOSDLock(ctx);
        if (osd)
            osd->ShowStatus(posInfo, slidertype, mesg, disptime, osdtype);
        ReturnOSDLock(ctx, osd);
        SetUpdateOSDPosition(true);
    }
}

void TV::UpdateOSDInput(const PlayerContext *ctx, QString inputname)
{
    if (!ctx->recorder || !ctx->tvchain)
        return;

    int cardid = ctx->GetCardID();

    if (inputname.isEmpty())
        inputname = ctx->tvchain->GetInputName(-1);

    QString displayName = CardUtil::GetDisplayName(cardid, inputname);
    // If a display name doesn't exist use cardid and inputname
    if (displayName.isEmpty())
        displayName = QString("%1: %2").arg(cardid).arg(inputname);

    OSD *osd = GetOSDLock(ctx);
    if (osd)
        osd->SetSettingsText(displayName, 3);
    ReturnOSDLock(ctx, osd);
}

/** \fn TV::UpdateOSDSignal(const PlayerContext*, const QStringList&)
 *  \brief Updates Signal portion of OSD...
 */
void TV::UpdateOSDSignal(const PlayerContext *ctx, const QStringList &strlist)
{
    OSD *osd = GetOSDLock(ctx);
    if (!osd || browsemode || !queuedChanNum.isEmpty())
    {
        if (&ctx->lastSignalMsg != &strlist)
        {
            ctx->lastSignalMsg = strlist;
            ctx->lastSignalMsg.detach();
        }
        ReturnOSDLock(ctx, osd);

        QMutexLocker locker(&timerIdLock);
        signalMonitorTimerId[StartTimer(1, __LINE__)] = (PlayerContext*)(ctx);
        return;
    }
    ReturnOSDLock(ctx, osd);

    SignalMonitorList slist = SignalMonitorValue::Parse(strlist);

    InfoMap infoMap = ctx->lastSignalUIInfo;
    if (ctx->lastSignalUIInfoTime.elapsed() > 5000 ||
        infoMap["callsign"].isEmpty())
    {
        //ctx->lastSignalUIInfo["name"]  = "signalmonitor";
        //ctx->lastSignalUIInfo["title"] = "Signal Monitor";
        ctx->lastSignalUIInfo.clear();
        ctx->GetPlayingInfoMap(ctx->lastSignalUIInfo);

        infoMap = ctx->lastSignalUIInfo;
        ctx->lastSignalUIInfoTime.start();
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
    float snr  = 0.0f;
    uint  ber  = 0xffffffff;
    int   pos  = -1;
    QString pat(""), pmt(""), mgt(""), vct(""), nit(""), sdt(""), crypt("");
    QString err = QString::null, msg = QString::null;
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
            VERBOSE(VB_IMPORTANT, "msg: "<<msg);
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
    QString slock   = ("1" == infoMap["slock"]) ? "L" : "l";
    QString lockMsg = (slock=="L") ? tr("Partial Lock") : tr("No Lock");
    QString sigMsg  = allGood ? tr("Lock") : lockMsg;

    QString sigDesc = tr("Signal %1\%").arg(sig,2);
    if (snr > 0.0f)
        sigDesc += " | " + tr("S/N %1dB").arg(log10f(snr), 3, 'f', 1);
    if (ber != 0xffffffff)
        sigDesc += " | " + tr("BE %1", "Bit Errors").arg(ber, 2);
    if ((pos >= 0) && (pos < 100))
        sigDesc += " | " + tr("Rotor %1\%").arg(pos,2);

    sigDesc = sigDesc + QString(" | (%1%2%3%4%5%6%7%8) %9")
        .arg(slock).arg(pat).arg(pmt).arg(mgt).arg(vct)
        .arg(nit).arg(sdt).arg(crypt).arg(sigMsg);

    if (!err.isEmpty())
        sigDesc = err;
    else if (!msg.isEmpty())
        sigDesc = msg;

    //osd->ClearAllText("signal_info");
    //osd->SetText("signal_info", infoMap, -1);

    infoMap["description"] = sigDesc;

    osd = GetOSDLock(ctx);
    if (osd)
    {
        osd->ClearAllText("channel_number");
        osd->SetText("channel_number", infoMap, osd_prog_info_timeout);
        osd->ClearAllText("program_info");
        osd->SetText("program_info", infoMap, osd_prog_info_timeout);
    }
    ReturnOSDLock(ctx, osd);

    ctx->lastSignalMsg.clear();
    ctx->lastSignalMsgTime.start();

    // Turn off lock timer if we have an "All Good" or good PMT
    if (allGood || (pmt == "M"))
    {
        lockTimerOn = false;
        lastLockSeenTime = QDateTime::currentDateTime();
    }
}

void TV::UpdateOSDTimeoutMessage(PlayerContext *ctx)
{
    bool timed_out = false;

    if (ctx->recorder)
    {
        QString input = ctx->recorder->GetInput();
        uint timeout  = ctx->recorder->GetSignalLockTimeout(input);
        timed_out = lockTimerOn && ((uint)lockTimer.elapsed() > timeout);
    }

    OSD *osd = GetOSDLock(ctx);

    if (!osd)
    {
        if (timed_out)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "You have no OSD, "
                    "but tuning has already taken too long.");
        }
        ReturnOSDLock(ctx, osd);
        return;
    }

    if (!timed_out)
    {
        if (osd->DialogShowing(kOSDDialogChannelTimeout))
            osd->TurnDialogOff(kOSDDialogChannelTimeout);
        ReturnOSDLock(ctx, osd);
        return;
    }

    if (osd->DialogShowing(kOSDDialogChannelTimeout))
    {
        ReturnOSDLock(ctx, osd);
        return;
    }

    // create dialog...
    static QString chan_up   = GET_KEY("TV Playback", "CHANNELUP");
    static QString chan_down = GET_KEY("TV Playback", "CHANNELDOWN");
    static QString next_src  = GET_KEY("TV Playback", "NEXTSOURCE");
    static QString tog_cards = GET_KEY("TV Playback", "NEXTINPUT");

    QString message = tr(
        "You should have received a channel lock by now. "
        "You can continue to wait for a signal, or you "
        "can change the channel with %1 or %2, change "
        "video source (%3), inputs (%4), etc.")
        .arg(chan_up).arg(chan_down).arg(next_src).arg(tog_cards);

    QStringList options;
    options += tr("OK");

    osd->NewDialogBox(kOSDDialogChannelTimeout, message, options, 0);

    ReturnOSDLock(ctx, osd);
}

void TV::UpdateLCD(void)
{
    // Make sure the LCD information gets updated shortly
    QMutexLocker locker(&timerIdLock);
    if (lcdTimerId)
        KillTimer(lcdTimerId);
    lcdTimerId = StartTimer(1, __LINE__);
}

void TV::ShowLCDChannelInfo(const PlayerContext *ctx)
{
    class LCD * lcd = LCD::Get();
    ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (!lcd || !ctx->playingInfo)
    {
        ctx->UnlockPlayingInfo(__FILE__, __LINE__);
        return;
    }

    QString title    = ctx->playingInfo->GetTitle();
    QString subtitle = ctx->playingInfo->GetSubtitle();
    QString callsign = ctx->playingInfo->GetChannelSchedulingID();

    ctx->UnlockPlayingInfo(__FILE__, __LINE__);

    if ((callsign != lcdCallsign) || (title != lcdTitle) ||
        (subtitle != lcdSubtitle))
    {
        lcd->switchToChannel(callsign, title, subtitle);
        lcdCallsign = callsign;
        lcdTitle = title;
        lcdSubtitle = subtitle;
    }
}

static void format_time(int seconds, QString &tMin, QString &tHrsMin)
{
    int minutes     = seconds / 60;
    int hours       = minutes / 60;
    int min         = minutes % 60;

    tMin = TV::tr("%n minute(s)", "", minutes);
    tHrsMin.sprintf("%d:%02d", hours, min);
}


void TV::ShowLCDDVDInfo(const PlayerContext *ctx)
{
    class LCD * lcd = LCD::Get();

    if (!lcd || !ctx->buffer || !ctx->buffer->isDVD())
    {
        return;
    }

    DVDRingBufferPriv *dvd = ctx->buffer->DVD();
    QString dvdName, dvdSerial;
    QString mainStatus, subStatus;

    if (!dvd->GetNameAndSerialNum(dvdName, dvdSerial))
    {
        dvdName = tr("DVD");
    }
    if (dvd->IsInMenu())
        mainStatus = tr("Menu");
    else if (dvd->InStillFrame())
        mainStatus = tr("Still Frame");
    else
    {
        QString timeMins, timeHrsMin;
        int playingTitle, playingPart, totalParts;

        dvd->GetPartAndTitle(playingPart, playingTitle);
        totalParts = dvd->NumPartsInTitle();
        format_time(dvd->GetTotalTimeOfTitle(), timeMins, timeHrsMin);

        mainStatus = tr("Title: %1 (%2)").arg(playingTitle).arg(timeHrsMin);
        subStatus = tr("Chapter: %1/%2").arg(playingPart).arg(totalParts);
    }
    if ((dvdName != lcdCallsign) || (mainStatus != lcdTitle) ||
                                    (subStatus != lcdSubtitle))
    {
        lcd->switchToChannel(dvdName, mainStatus, subStatus);
        lcdCallsign = dvdName;
        lcdTitle    = mainStatus;
        lcdSubtitle = subStatus;
    }
}


/**
 *  \brief Fetches information on the desired program from the backend.
 *  \param enc RemoteEncoder to query, if null query the actx->recorder.
 *  \param direction BrowseDirection to get information on.
 *  \param infoMap InfoMap to fill in with returned data
 */
void TV::GetNextProgram(RemoteEncoder *enc, BrowseDirection direction,
                        InfoMap &infoMap) const
{
    QString title, subtitle, desc, category, endtime, callsign, iconpath;
    QDateTime begts, endts;

    QString starttime = infoMap["dbstarttime"];
    QString chanid    = infoMap["chanid"];
    QString channum   = infoMap["channum"];
    QString seriesid  = infoMap["seriesid"];
    QString programid = infoMap["programid"];

    const PlayerContext *actx = NULL;
    if (!enc)
    {
        actx = GetPlayerReadLock(-1, __FILE__, __LINE__);
        enc  = actx->recorder;
    }

    enc->GetNextProgram(direction,
                        title,     subtitle,  desc,      category,
                        starttime, endtime,   callsign,  iconpath,
                        channum,   chanid,    seriesid,  programid);

    if (actx)
    {
        enc = NULL; // once we've returned the lock, using enc is unsafe
        ReturnPlayerLock(actx);
    }

    if (!starttime.isEmpty())
        begts = QDateTime::fromString(starttime, Qt::ISODate);
    else
        begts = QDateTime::fromString(infoMap["dbstarttime"], Qt::ISODate);

    infoMap["starttime"] = begts.toString(db_time_format);
    infoMap["startdate"] = begts.toString(db_short_date_format);

    infoMap["endtime"] = infoMap["enddate"] = "";
    if (!endtime.isEmpty())
    {
        endts = QDateTime::fromString(endtime, Qt::ISODate);
        infoMap["endtime"] = endts.toString(db_time_format);
        infoMap["enddate"] = endts.toString(db_short_date_format);
    }

    infoMap["lenmins"] = tr("%n minute(s)", "", 0);
    infoMap["lentime"] = "0:00";
    if (begts.isValid() && endts.isValid())
    {
        QString lenM, lenHM;
        format_time(begts.secsTo(endts), lenM, lenHM);
        infoMap["lenmins"] = lenM;
        infoMap["lentime"] = lenHM;
    }

    infoMap["dbstarttime"] = starttime;
    infoMap["dbendtime"]   = endtime;
    infoMap["title"]       = title;
    infoMap["subtitle"]    = subtitle;
    infoMap["description"] = desc;
    infoMap["category"]    = category;
    infoMap["callsign"]    = callsign;
    infoMap["channum"]     = channum;
    infoMap["chanid"]      = chanid;
    infoMap["iconpath"]    = iconpath;
    infoMap["seriesid"]    = seriesid;
    infoMap["programid"]   = programid;
}

void TV::GetNextProgram(BrowseDirection direction, InfoMap &infoMap) const
{
    uint chanid = infoMap["chanid"].toUInt();
    if (!chanid)
        chanid = BrowseAllGetChanId(infoMap["channum"]);

    int chandir = -1;
    switch (direction)
    {
        case BROWSE_UP:       chandir = CHANNEL_DIRECTION_UP;       break;
        case BROWSE_DOWN:     chandir = CHANNEL_DIRECTION_DOWN;     break;
        case BROWSE_FAVORITE: chandir = CHANNEL_DIRECTION_FAVORITE; break;
    }
    if (direction != BROWSE_INVALID)
    {
        QMutexLocker locker(&browseLock);
        chanid = ChannelUtil::GetNextChannel(
            db_browse_all_channels, chanid, 0, chandir);
    }

    infoMap["chanid"] = QString::number(chanid);

    {
        QMutexLocker locker(&browseLock);
        DBChanList::const_iterator it = db_browse_all_channels.begin();
        for (; it != db_browse_all_channels.end(); ++it)
        {
            if ((*it).chanid == chanid)
            {
                QString tmp = (*it).channum;
                tmp.detach();
                infoMap["channum"] = tmp;
                break;
            }
        }
    }

    QDateTime nowtime = QDateTime::currentDateTime();
    QDateTime latesttime = nowtime.addSecs(6*60*60);
    QDateTime browsetime = QDateTime::fromString(
        infoMap["dbstarttime"], Qt::ISODate);

    MSqlBindings bindings;
    bindings[":CHANID"] = chanid;
    bindings[":NOWTS"] = nowtime.toString("yyyy-MM-ddThh:mm:ss");
    bindings[":LATESTTS"] = latesttime.toString("yyyy-MM-ddThh:mm:ss");
    bindings[":BROWSETS"] = browsetime.toString("yyyy-MM-ddThh:mm:ss");
    bindings[":BROWSETS2"] = browsetime.toString("yyyy-MM-ddThh:mm:ss");

    QString querystr = " WHERE program.chanid = :CHANID ";
    switch (direction)
    {
        case BROWSE_LEFT:
            querystr += " AND program.endtime <= :BROWSETS "
                " AND program.endtime > :NOWTS ";
            break;

        case BROWSE_RIGHT:
            querystr += " AND program.starttime > :BROWSETS "
                " AND program.starttime < :LATESTTS ";
            break;

        default:
            querystr += " AND program.starttime <= :BROWSETS "
                " AND program.endtime > :BROWSETS2 ";
    };

    ProgramList progList;
    ProgramList dummySched;
    LoadFromProgram(progList, querystr, bindings, dummySched, false);

    if (progList.empty())
    {
        infoMap["dbstarttime"] = "";
        return;
    }

    const ProgramInfo *prog = (direction == BROWSE_LEFT) ?
        progList[progList.size() - 1] : progList[0];

    infoMap["dbstarttime"] = prog->GetScheduledStartTime(ISODate);
}

bool TV::IsTunable(const PlayerContext *ctx, uint chanid, bool use_cache)
{
    VERBOSE(VB_PLAYBACK, QString("IsTunable(%1)").arg(chanid));

    if (!chanid)
        return false;

    uint mplexid = ChannelUtil::GetMplexID(chanid);
    mplexid = (32767 == mplexid) ? 0 : mplexid;

    vector<uint> excluded_cards;
    if (ctx->recorder && ctx->pseudoLiveTVState == kPseudoNormalLiveTV)
        excluded_cards.push_back(ctx->GetCardID());

    uint sourceid = ChannelUtil::GetSourceIDForChannel(chanid);
    vector<uint> connected   = RemoteRequestFreeRecorderList();
    vector<uint> interesting = CardUtil::GetCardIDs(sourceid);

    // filter disconnected cards
    vector<uint> cardids = excluded_cards;
    for (uint i = 0; i < connected.size(); i++)
    {
        for (uint j = 0; j < interesting.size(); j++)
        {
            if (connected[i] == interesting[j])
            {
                cardids.push_back(interesting[j]);
                break;
            }
        }
    }

#if 0
    cout << "cardids[" << sourceid << "]: ";
    for (uint i = 0; i < cardids.size(); i++)
        cout << cardids[i] << ", ";
    cout << endl;
#endif

    for (uint i = 0; i < cardids.size(); i++)
    {
        vector<InputInfo> inputs;

        bool used_cache = false;
        if (use_cache)
        {
            QMutexLocker locker(&is_tunable_cache_lock);
            if (is_tunable_cache_inputs.contains(cardids[i]))
            {
                inputs = is_tunable_cache_inputs[cardids[i]];
                used_cache = true;
            }
        }

        if (!used_cache)
        {
            inputs = RemoteRequestFreeInputList(cardids[i], excluded_cards);
            QMutexLocker locker(&is_tunable_cache_lock);
            is_tunable_cache_inputs[cardids[i]] = inputs;
        }

#if 0
        cout << "inputs[" << cardids[i] << "]: ";
        for (uint j = 0; j < inputs.size(); j++)
            cout << inputs[j].inputid << ", ";
        cout << endl;
#endif

        for (uint j = 0; j < inputs.size(); j++)
        {
            if (inputs[j].sourceid != sourceid)
                continue;

            if (inputs[j].mplexid &&
                inputs[j].mplexid != mplexid)
                continue;

            VERBOSE(VB_PLAYBACK, QString("IsTunable(%1) -> true\n")
                    .arg(chanid));

            return true;
        }
    }

    VERBOSE(VB_PLAYBACK, QString("IsTunable(%1) -> false\n").arg(chanid));

    return false;
}

void TV::ClearTunableCache(void)
{
    QMutexLocker locker(&is_tunable_cache_lock);
    is_tunable_cache_inputs.clear();
}

bool TV::StartEmbedding(PlayerContext *ctx, WId wid, const QRect &embedRect)
{
    if (!ctx->IsNullVideoDesired())
        ctx->StartEmbedding(wid, embedRect);
    else
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                QString("StartEmbedding called with null video context #%1")
                .arg(find_player_index(ctx)));
        ctx->ResizePIPWindow(embedRect);
    }

    // Hide any PIP windows...
    PlayerContext *mctx = GetPlayer(ctx, 0);
    for (uint i = 1; (mctx == ctx) && (i < player.size()); i++)
    {
        GetPlayer(ctx,i)->LockDeleteNVP(__FILE__, __LINE__);
        if (GetPlayer(ctx,i)->nvp)
            GetPlayer(ctx,i)->nvp->SetPIPVisible(false);
        GetPlayer(ctx,i)->UnlockDeleteNVP(__FILE__, __LINE__);
    }

    // Start checking for end of file for embedded window..
    QMutexLocker locker(&timerIdLock);
    if (embedCheckTimerId)
        KillTimer(embedCheckTimerId);
    embedCheckTimerId = StartTimer(kEmbedCheckFrequency, __LINE__);

    return ctx->IsEmbedding();
}

void TV::StopEmbedding(PlayerContext *ctx)
{
    if (!ctx->IsEmbedding())
        return;

    ctx->StopEmbedding();

    // Undo any PIP hiding
    PlayerContext *mctx = GetPlayer(ctx, 0);
    for (uint i = 1; (mctx == ctx) && (i < player.size()); i++)
    {
        GetPlayer(ctx,i)->LockDeleteNVP(__FILE__, __LINE__);
        if (GetPlayer(ctx,i)->nvp)
            GetPlayer(ctx,i)->nvp->SetPIPVisible(true);
        GetPlayer(ctx,i)->UnlockDeleteNVP(__FILE__, __LINE__);
    }

    // Stop checking for end of file for embedded window..
    QMutexLocker locker(&timerIdLock);
    if (embedCheckTimerId)
        KillTimer(embedCheckTimerId);
    embedCheckTimerId = 0;
}

void TV::DrawUnusedRects(void)
{
    VERBOSE(VB_PLAYBACK, LOC + "DrawUnusedRects() -- begin");

    PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
    for (uint i = 0; mctx && (i < player.size()); i++)
    {
        PlayerContext *ctx = GetPlayer(mctx, i);
        ctx->LockDeleteNVP(__FILE__, __LINE__);
        if (ctx->nvp)
            ctx->nvp->ExposeEvent();
        ctx->UnlockDeleteNVP(__FILE__, __LINE__);
    }
    ReturnPlayerLock(mctx);

    VERBOSE(VB_PLAYBACK, LOC + "DrawUnusedRects() -- end");
}

vector<bool> TV::DoSetPauseState(PlayerContext *lctx, const vector<bool> &pause)
{
    vector<bool> was_paused;
    vector<float> times;
    for (uint i = 0; lctx && i < player.size() && i < pause.size(); i++)
    {
        was_paused.push_back(GetPlayer(lctx,i)->paused);
        float time = 0.0f;
        if (pause[i] ^ was_paused.back())
            time = DoTogglePauseStart(GetPlayer(lctx,i));
        times.push_back(time);
    }

    for (uint i = 0; lctx && i < player.size() && i < pause.size(); i++)
    {
        if (pause[i] ^ was_paused[i])
            DoTogglePauseFinish(GetPlayer(lctx,i), times[i], false);
    }

    return was_paused;
}

void TV::DoEditSchedule(int editType)
{
    if ((editType == kScheduleProgramGuide  && !RunProgramGuidePtr) ||
        (editType == kScheduleProgramFinder && !RunProgramFinderPtr) ||
        (editType == kScheduledRecording    && !RunScheduleEditorPtr) ||
        (editType == kViewSchedule          && !RunViewScheduledPtr))
    {
        return;
    }

    PlayerContext *actx = GetPlayerReadLock(-1, __FILE__, __LINE__);

    actx->LockPlayingInfo(__FILE__, __LINE__);
    if (!actx->playingInfo)
    {
        VERBOSE(VB_IMPORTANT,
                LOC_ERR + "doEditSchedule(): no active ctx playingInfo.");
        actx->UnlockPlayingInfo(__FILE__, __LINE__);
        ReturnPlayerLock(actx);
        return;
    }

    // Collect channel info
    const ProgramInfo pginfo(*actx->playingInfo);
    uint    chanid  = pginfo.GetChanID();
    QString channum = pginfo.GetChanNum();
    int changrpid   = channel_group_id;
    actx->UnlockPlayingInfo(__FILE__, __LINE__);

    ClearOSD(actx);

    // Pause playback as needed...
    bool pause_active = true;
    bool isNearEnd = false;
    bool isLiveTV = StateIsLiveTV(GetState(actx));
    bool allowEmbedding = false;

    {
        actx->LockDeleteNVP(__FILE__, __LINE__);
        pause_active = !actx->nvp || !actx->nvp->getVideoOutput();
        if (actx->nvp && actx->nvp->getVideoOutput())
            allowEmbedding = actx->nvp->getVideoOutput()->AllowPreviewEPG();

        if (!pause_active)
        {
            long long margin = (long long)
                (actx->last_framerate * actx->nvp->GetAudioStretchFactor());
            isNearEnd = actx->nvp->IsNearEnd(margin);
        }
        actx->UnlockDeleteNVP(__FILE__, __LINE__);
    }

    pause_active |= kScheduledRecording == editType;
    pause_active |= kViewSchedule == editType;
    pause_active |=
        !isLiveTV && (!db_continue_embedded || isNearEnd);
    pause_active |= actx->paused;
    vector<bool> do_pause;
    do_pause.insert(do_pause.begin(), true, player.size());
    do_pause[find_player_index(actx)] = pause_active;

    saved_pause = DoSetPauseState(actx, do_pause);

    // Resize window to the MythTV GUI size
    PlayerContext *mctx = GetPlayer(actx,0);
    mctx->LockDeleteNVP(__FILE__, __LINE__);
    if (mctx->nvp && mctx->nvp->getVideoOutput())
        mctx->nvp->getVideoOutput()->ResizeForGui();
    mctx->UnlockDeleteNVP(__FILE__, __LINE__);
    ReturnPlayerLock(actx);
    MythMainWindow *mwnd = GetMythMainWindow();
    if (!db_use_gui_size_for_tv || !db_use_fixed_size)
    {
        mwnd->setGeometry(saved_gui_bounds.left(), saved_gui_bounds.top(),
                          saved_gui_bounds.width(), saved_gui_bounds.height());
        mwnd->setFixedSize(saved_gui_bounds.size());
    }

    // Actually show the pop-up UI
    switch (editType)
    {
        case kScheduleProgramGuide:
        {
            isEmbedded = (isLiveTV && !pause_active && allowEmbedding);
            RunProgramGuidePtr(chanid, channum, this, isEmbedded, true, changrpid);
            ignoreKeyPresses = true;
            break;
        }
        case kScheduleProgramFinder:
        {
            isEmbedded = (isLiveTV && !pause_active && allowEmbedding);
            RunProgramFinderPtr(this, isEmbedded, true);
            ignoreKeyPresses = true;
            break;
        }
        case kScheduledRecording:
        {
            RunScheduleEditorPtr(&pginfo, (void *)this);
            ignoreKeyPresses = true;
            break;
        }
        case kViewSchedule:
        {
            RunViewScheduledPtr((void *)this, !pause_active);
            ignoreKeyPresses = true;
            break;
        }
        case kPlaybackBox:
        {
            RunPlaybackBoxPtr((void *)this, !pause_active);
            ignoreKeyPresses = true;
            break;
        }
    }

    //we are embedding in a mythui window so show the gui paint window again
    GetMythMainWindow()->SetDrawEnabled(true);
    GetMythMainWindow()->GetPaintWindow()->show();
}

void TV::EditSchedule(const PlayerContext *ctx, int editType)
{
    // post the request to the main UI thread
    // it will be caught in eventFilter and processed as CustomEvent
    // this will create the program guide window (widget)
    // on the main thread and avoid a deadlock on Win32
    QString message = QString("START_EPG %1").arg(editType);
    MythEvent* me = new MythEvent(message);
    qApp->postEvent(GetMythMainWindow(), me);
}

void TV::ChangeVolume(PlayerContext *ctx, bool up)
{
    ctx->LockDeleteNVP(__FILE__, __LINE__);
    if (!ctx->nvp)
    {
        ctx->UnlockDeleteNVP(__FILE__, __LINE__);
        return;
    }
    uint curvol = ctx->nvp->AdjustVolume((up) ? +2 : -2);
    ctx->UnlockDeleteNVP(__FILE__, __LINE__);

    QString text = tr("Volume %1 %").arg(curvol);

    OSD *osd = GetOSDLock(ctx);
    if (osd && !browsemode)
    {
        osd->ShowStatus(curvol * 10, true, tr("Adjust Volume"), text, 5,
                        kOSDFunctionalType_PictureAdjust);
        SetUpdateOSDPosition(false);
    }
    ReturnOSDLock(ctx, osd);
}

void TV::ToggleTimeStretch(PlayerContext *ctx)
{
    if (ctx->ts_normal == 1.0f)
    {
        ctx->ts_normal = ctx->ts_alt;
    }
    else
    {
        ctx->ts_alt = ctx->ts_normal;
        ctx->ts_normal = 1.0f;
    }
    ChangeTimeStretch(ctx, 0, false);
}

void TV::ChangeTimeStretch(PlayerContext *ctx, int dir, bool allowEdit)
{
    const float kTimeStretchMin = 0.5;
    const float kTimeStretchMax = 2.0;
    float new_ts_normal = ctx->ts_normal + 0.05*dir;
    stretchAdjustment = allowEdit;

    if (new_ts_normal > kTimeStretchMax &&
        ctx->ts_normal < kTimeStretchMax)
    {
        new_ts_normal = kTimeStretchMax;
    }
    else if (new_ts_normal < kTimeStretchMin &&
             ctx->ts_normal > kTimeStretchMin)
    {
        new_ts_normal = kTimeStretchMin;
    }

    if (new_ts_normal > kTimeStretchMax ||
        new_ts_normal < kTimeStretchMin)
    {
        return;
    }

    ctx->ts_normal = new_ts_normal;

    if (!ctx->paused)
    {
        ctx->LockDeleteNVP(__FILE__, __LINE__);
        if (ctx->nvp)
            ctx->nvp->Play(ctx->ts_normal, true);
        ctx->UnlockDeleteNVP(__FILE__, __LINE__);
    }

    QString text = QString(tr("Time Stretch %1X")).arg(ctx->ts_normal);

    if (!browsemode)
    {
        if (!allowEdit)
        {
            UpdateOSDSeekMessage(ctx, ctx->GetPlayMessage(),
                                 osd_general_timeout);
        }
        else
        {
            int val = (int)(ctx->ts_normal*(1000/kTimeStretchMax));
            OSD *osd = GetOSDLock(ctx);
            if (osd)
            {
                osd->ShowStatus(val, false, tr("Adjust Time Stretch"), text,
                                10, kOSDFunctionalType_TimeStretchAdjust);
            }
            ReturnOSDLock(ctx, osd);
            SetUpdateOSDPosition(false);
        }
    }

    SetSpeedChangeTimer(0, __LINE__);
}

void TV::ToggleUpmix(PlayerContext *ctx)
{
    if (!ctx->nvp || !ctx->nvp->HasAudioOut())
        return;
    QString text;
    if (ctx->nvp->ToggleUpmix())
        text = tr("Upmixer On");
    else
        text = tr("Upmixer Off");

    if (ctx->nvp->GetOSD() && !browsemode)
        ctx->nvp->GetOSD()->SetSettingsText(text, 5);
}

// dir in 10ms jumps
void TV::ChangeAudioSync(PlayerContext *ctx, int dir, bool allowEdit)
{
    long long newval;

    ctx->LockDeleteNVP(__FILE__, __LINE__);
    if (!ctx->nvp)
    {
        ctx->UnlockDeleteNVP(__FILE__, __LINE__);
        return;
    }

    if (!audiosyncAdjustment && LONG_LONG_MIN == audiosyncBaseline)
        audiosyncBaseline = ctx->nvp->GetAudioTimecodeOffset();

    audiosyncAdjustment = allowEdit;

    if (dir == 1000000)
    {
        newval = ctx->nvp->ResyncAudioTimecodeOffset() -
                 audiosyncBaseline;
        audiosyncBaseline = ctx->nvp->GetAudioTimecodeOffset();
    }
    else if (dir == -1000000)
    {
        newval = ctx->nvp->ResetAudioTimecodeOffset() -
                 audiosyncBaseline;
        audiosyncBaseline = ctx->nvp->GetAudioTimecodeOffset();
    }
    else
    {
        newval = ctx->nvp->AdjustAudioTimecodeOffset(dir*10) -
                 audiosyncBaseline;
    }
    ctx->UnlockDeleteNVP(__FILE__, __LINE__);

    OSD *osd = GetOSDLock(ctx);
    if (osd && !browsemode)
    {
        QString text = QString(" %1 ms").arg(newval);
        int val = (int)newval;
        if (dir == 1000000 || dir == -1000000)
        {
            text = tr("Audio Resync") + text;
            val = 0;
        }
        else
            text = tr("Audio Sync") + text;

        osd->ShowStatus((val/2)+500, false, tr("Adjust Audio Sync"), text,
                             10, kOSDFunctionalType_AudioSyncAdjust);
        SetUpdateOSDPosition(false);
    }
    ReturnOSDLock(ctx, osd);
}

void TV::ToggleMute(PlayerContext *ctx)
{
    ctx->LockDeleteNVP(__FILE__, __LINE__);
    if (!ctx->nvp || !ctx->nvp->HasAudioOut())
    {
        ctx->UnlockDeleteNVP(__FILE__, __LINE__);
        return;
    }

    MuteState mute_status;

    if (!MuteIndividualChannels)
    {
        ctx->nvp->SetMuted(!ctx->nvp->IsMuted());
        mute_status = (ctx->nvp->IsMuted()) ? kMuteAll : kMuteOff;
    }
    else
    {
        mute_status = ctx->nvp->IncrMuteState();
    }
    ctx->UnlockDeleteNVP(__FILE__, __LINE__);

    QString text;

    switch (mute_status)
    {
        case kMuteOff:   text = tr("Mute Off"); break;
        case kMuteAll:   text = tr("Mute On"); break;
        case kMuteLeft:  text = tr("Left Channel Muted"); break;
        case kMuteRight: text = tr("Right Channel Muted"); break;
    }

    OSD *osd = GetOSDLock(ctx);
    if (osd && !browsemode)
        osd->SetSettingsText(text, 5);
    ReturnOSDLock(ctx, osd);
}

void TV::ToggleSleepTimer(const PlayerContext *ctx)
{
    QString text;

    // increment sleep index, cycle through
    if (++sleep_index == sleep_times.size())
        sleep_index = 0;

    // set sleep timer to next sleep_index timeout
    if (sleepTimerId)
    {
        KillTimer(sleepTimerId);
        sleepTimerId = 0;
        sleepTimerTimeout = 0;
    }

    if (sleep_times[sleep_index].seconds != 0)
    {
        sleepTimerTimeout = sleep_times[sleep_index].seconds * 1000;
        sleepTimerId = StartTimer(sleepTimerTimeout, __LINE__);
    }

    text = tr("Sleep ") + " " + sleep_times[sleep_index].dispString;

    // display OSD
    OSD *osd = GetOSDLock(ctx);
    if (osd && !browsemode)
        osd->SetSettingsText(text, 3);
    ReturnOSDLock(ctx, osd);
}

void TV::SleepDialogCreate(void)
{
    KillTimer(sleepTimerId);
    sleepTimerId = 0;

    QString message = QObject::tr(
        "MythTV was set to sleep after %1 minutes and "
        "will exit in %2 seconds.\n"
        "Do you wish to continue watching?")
        .arg(sleepTimerTimeout * (1.0f/60000.0f))
        .arg("%d");

    QStringList options;
    options += tr("Yes");
    options += tr("No");

    PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
    OSD *osd = GetOSDLock(mctx);
    if (osd)
        osd->NewDialogBox(kOSDDialogSleepTimeout, message, options,
                          (int)(kSleepTimerDialogTimeout * 0.001), 0);
    ReturnOSDLock(mctx, osd);
    ReturnPlayerLock(mctx);

    sleepDialogTimerId = StartTimer(kSleepTimerDialogTimeout, __LINE__);
}

void TV::SleepDialogTimeout(void)
{
    KillTimer(sleepDialogTimerId);
    sleepDialogTimerId = 0;

    VERBOSE(VB_GENERAL, LOC +
            "Sleep timeout reached, exiting player.");

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
void TV::IdleDialogCreate(void)
{
    KillTimer(idleTimerId);
    idleTimerId = 0;

    QString message = QObject::tr(
        "MythTV has been idle for %1 minutes and "
        "will exit in %2 seconds. Are you still watching?")
        .arg(db_idle_timeout * (1.0f/60000.0f))
        .arg("%d");

    QStringList options;
    options += tr("Yes");
    options += tr("No");

    PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
    OSD *osd = GetOSDLock(mctx);
    if (osd)
        osd->NewDialogBox(kOSDDialogIdleTimeout, message, options,
                          (int)(kIdleTimerDialogTimeout * 0.001f), 0);
    ReturnOSDLock(mctx, osd);
    ReturnPlayerLock(mctx);

    idleDialogTimerId = StartTimer(kIdleTimerDialogTimeout, __LINE__);
}

void TV::IdleDialogTimeout(void)
{
    KillTimer(idleDialogTimerId);
    idleDialogTimerId = 0;

    PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
    if (StateIsLiveTV(mctx->GetState()))
    {
        VERBOSE(VB_GENERAL, LOC +
                "Idle timeout reached, leaving LiveTV");
        SetExitPlayer(true, true);
    }
    ReturnPlayerLock(mctx);
}

void TV::ToggleAspectOverride(PlayerContext *ctx, AspectOverrideMode aspectMode)
{
    ctx->LockDeleteNVP(__FILE__, __LINE__);
    if (!ctx->nvp)
    {
        ctx->UnlockDeleteNVP(__FILE__, __LINE__);
        return;
    }
    ctx->nvp->ToggleAspectOverride(aspectMode);
    QString text = toString(ctx->nvp->GetAspectOverride());
    ctx->UnlockDeleteNVP(__FILE__, __LINE__);

    OSD *osd = GetOSDLock(ctx);
    if (osd && !browsemode && !osd->IsRunningTreeMenu())
        osd->SetSettingsText(text, 3);
    ReturnOSDLock(ctx, osd);
}

void TV::ToggleAdjustFill(PlayerContext *ctx, AdjustFillMode adjustfillMode)
{
    if (ctx != GetPlayer(ctx,0) || ctx->IsPBP())
        return;

    ctx->LockDeleteNVP(__FILE__, __LINE__);
    if (!ctx->nvp)
    {
        ctx->UnlockDeleteNVP(__FILE__, __LINE__);
        return;
    }
    ctx->nvp->ToggleAdjustFill(adjustfillMode);
    QString text = toString(ctx->nvp->GetAdjustFill());
    ctx->UnlockDeleteNVP(__FILE__, __LINE__);

    OSD *osd = GetOSDLock(ctx);
    if (osd && !browsemode && !osd->IsRunningTreeMenu())
        osd->SetSettingsText(text, 3);
    ReturnOSDLock(ctx, osd);
}

void TV::SetMuteTimer(PlayerContext *ctx, int timeout)
{
    // message to set the timer will be posted to the main UI thread
    // where it will be caught in eventFilter and processed as CustomEvent
    // this will properly set the mute timer
    // otherwise it never fires on Win32
    QString message = QString("UNMUTE %1 %2").arg((long long)ctx).arg(timeout);
    qApp->postEvent(GetMythMainWindow(), new MythEvent(message));
}

bool TV::MuteChannelChange(PlayerContext *ctx)
{
    if (!ctx)
        return false;

    bool muted = false;
    ctx->LockDeleteNVP(__FILE__, __LINE__);
    if (ctx->nvp && !ctx->nvp->IsMuted())
        muted = ctx->nvp->SetMuted(true);
    ctx->UnlockDeleteNVP(__FILE__, __LINE__);

    return muted;
}

void TV::customEvent(QEvent *e)
{
    if (e->type() == OSDCloseEvent::kEventType)
    {
        OSDCloseEvent *ce = (OSDCloseEvent *)e;
        HandleOSDClosed(ce->GetFunctionType());
        return;
    }

    if (e->type() == OSDListTreeItemEnteredEvent::kEventType)
    {
        OSDListTreeItemEnteredEvent *ee = (OSDListTreeItemEnteredEvent *)e;
        TreeMenuEntered(ee);
        return;
    }

    if (e->type() == OSDListTreeItemSelectedEvent::kEventType)
    {
        OSDListTreeItemSelectedEvent *se = (OSDListTreeItemSelectedEvent *)e;
        TreeMenuSelected(se);
        return;
    }

    if (e->type() != MythEvent::MythEventMessage)
        return;

    uint cardnum   = 0;
    MythEvent *me = (MythEvent *)e;
    QString message = me->Message();

    // TODO Go through these and make sure they make sense...
    QStringList tokens = message.split(" ", QString::SkipEmptyParts);

    if (message.left(14) == "DONE_RECORDING")
    {
        int seconds = 0;
        long long frames = 0;
        if (tokens.size() >= 4)
        {
            cardnum = tokens[1].toUInt();
            seconds = tokens[2].toInt();
            frames = tokens[3].toLongLong();
        }

        PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
        for (uint i = 0; mctx && (i < player.size()); i++)
        {
            PlayerContext *ctx = GetPlayer(mctx, i);
            if (ctx->GetState() == kState_WatchingRecording)
            {
                if (ctx->recorder && (cardnum == ctx->GetCardID()))
                {
                    ctx->LockDeleteNVP(__FILE__, __LINE__);
                    if (ctx->nvp)
                    {
                        ctx->nvp->SetWatchingRecording(false);
                        ctx->nvp->SetLength((int)(frames / ctx->nvp->GetFrameRate()));
                    }
                    ctx->UnlockDeleteNVP(__FILE__, __LINE__);

                    ctx->ChangeState(kState_WatchingPreRecorded);
                    ScheduleStateChange(ctx);
                }
            }
            else if (StateIsLiveTV(ctx->GetState()))
            {
                if (ctx->recorder && cardnum == ctx->GetCardID() &&
                    ctx->tvchain && ctx->tvchain->HasNext())
                {
                    ctx->LockDeleteNVP(__FILE__, __LINE__);
                    if (ctx->nvp)
                    {
                        ctx->nvp->SetWatchingRecording(false);
                        ctx->nvp->SetLength((int)(frames / ctx->nvp->GetFrameRate()));
                    }
                    ctx->UnlockDeleteNVP(__FILE__, __LINE__);
                }
            }
        }
        ReturnPlayerLock(mctx);
    }

    if (message.left(14) == "ASK_RECORDING ")
    {
        int timeuntil = 0, hasrec = 0, haslater = 0;
        if (tokens.size() >= 5)
        {
            cardnum   = tokens[1].toUInt();
            timeuntil = tokens[2].toInt();
            hasrec    = tokens[3].toInt();
            haslater  = tokens[4].toInt();
        }
        VERBOSE(VB_GENERAL, LOC + message << " hasrec: "<<hasrec
                << " haslater: " << haslater);

        PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
        if (mctx->recorder && cardnum == mctx->GetCardID())
        {
            AskAllowRecording(mctx, me->ExtraDataList(),
                              timeuntil, hasrec, haslater);
        }

        for (uint i = 1; i < player.size(); i++)
        {
            PlayerContext *ctx = GetPlayer(mctx, i);
            if (ctx->recorder && ctx->GetCardID() == cardnum)
            {
                VERBOSE(VB_GENERAL, LOC + "Disabling PxP for recording");
                QString type = ctx->IsPIP() ?
                    tr("PiP", "Picture-in-Picture") :
                    tr("PbP", "Picture-by-Picture");
                StopStuff(mctx, ctx, true, true, true);
                OSD *osd = GetOSDLock(mctx);
                if (osd)
                {
                    osd->SetSettingsText(
                        tr("Disabling %1 for recording").arg(type), 3);
                }
                ReturnOSDLock(mctx, osd);
            }
        }
        ReturnPlayerLock(mctx);
    }

    if (message.left(11) == "QUIT_LIVETV")
    {
        cardnum = (tokens.size() >= 2) ? tokens[1].toUInt() : 0;

        PlayerContext *mctx = GetPlayerReadLock(-1, __FILE__, __LINE__);
        int match = -1;
        for (uint i = 0; mctx && (i < player.size()); i++)
        {
            PlayerContext *ctx = GetPlayer(mctx, i);
            match = (ctx->GetCardID() == cardnum) ? i : match;
        }

        if (match >= 0 && GetPlayer(mctx, match)->recorder)
        {
            if (0 == match)
            {
                for (uint i = 1; mctx && (i < player.size()); i++)
                {
                    PlayerContext *ctx = GetPlayer(mctx, i);
                    if (ctx->recorder && (ctx->GetCardID() == cardnum))
                    {
                        VERBOSE(VB_GENERAL, LOC +
                                "Disabling PiP for QUIT_LIVETV");
                        StopStuff(mctx, ctx, true, true, true);
                    }
                }
                SetLastProgram(NULL);
                jumpToProgram = true;
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

    if (message.left(12) == "LIVETV_WATCH")
    {
        int watch = 0;
        if (tokens.size() >= 3)
        {
            cardnum = tokens[1].toUInt();
            watch   = tokens[2].toInt();
        }

        PlayerContext *mctx = GetPlayerWriteLock(0, __FILE__, __LINE__);
        int match = -1;
        for (uint i = 0; mctx && (i < player.size()); i++)
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

                    QMutexLocker locker(&timerIdLock);
                    if (!pseudoChangeChanTimerId)
                        pseudoChangeChanTimerId = StartTimer(0, __LINE__);
                }
            }
            else
            {
                PlayerContext *ctx = GetPlayer(mctx, match);
                ctx->SetPseudoLiveTV(NULL, kPseudoNormalLiveTV);
            }
        }
        ReturnPlayerLock(mctx);
    }

    if (message.left(12) == "LIVETV_CHAIN")
    {
        QString id = QString::null;
        if ((tokens.size() >= 2) && tokens[1] == "UPDATE")
            id = tokens[2];

        PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
        for (uint i = 0; mctx && (i < player.size()); i++)
        {
            PlayerContext *ctx = GetPlayer(mctx, i);
            if (ctx->tvchain && ctx->tvchain->GetID() == id)
            {
                QMutexLocker locker(&timerIdLock);
                tvchainUpdateTimerId[StartTimer(1, __LINE__)] = ctx;
                break;
            }
        }
        ReturnPlayerLock(mctx);
    }

    if (message.left(12) == "EXIT_TO_MENU")
    {
        PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
        for (uint i = 0; mctx && (i < player.size()); i++)
        {
            PlayerContext *ctx = GetPlayer(mctx, i);
            PrepareToExitPlayer(ctx, __LINE__, db_playback_exit_prompt == 1 ||
                                               db_playback_exit_prompt == 2);
        }

        SetExitPlayer(true, true);
        ReturnPlayerLock(mctx);
    }

    if (message.left(6) == "SIGNAL")
    {
        cardnum = (tokens.size() >= 2) ? tokens[1].toUInt() : 0;
        QStringList signalList = me->ExtraDataList();

        PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
        for (uint i = 0; mctx && (i < player.size()); i++)
        {
            PlayerContext *ctx = GetPlayer(mctx, i);
            bool tc = ctx->recorder && (ctx->GetCardID() == cardnum);
            if (tc && !signalList.empty())
            {
                UpdateOSDSignal(ctx, signalList);
            }
        }
        ReturnPlayerLock(mctx);
    }

    if (message.left(15) == "NETWORK_CONTROL")
    {
        if ((tokens.size() >= 2) &&
            (tokens[1] != "ANSWER") && (tokens[1] != "RESPONSE"))
        {
            QStringList tokens = message.split(" ", QString::SkipEmptyParts);
            if ((tokens.size() >= 2) &&
                (tokens[1] != "ANSWER") && (tokens[1] != "RESPONSE"))
            {
                QMutexLocker locker(&timerIdLock);
                message.detach();
                networkControlCommands.enqueue(message);
                if (!networkControlTimerId)
                    networkControlTimerId = StartTimer(1, __LINE__);
            }
        }
    }

    if (message.left(6) == "UNMUTE")
    {
        if (tokens.size() >= 3)
        {
            long long target = tokens[1].toLongLong();
            PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
            for (uint i = 0; i < player.size(); i++)
            {
                PlayerContext *ctx = GetPlayer(mctx, i);
                if (((long long)ctx) == target)
                {
                    QMutexLocker locker(&timerIdLock);
                    unmuteTimerId[StartTimer(tokens[2].toUInt(), __LINE__)] = ctx;
                }
            }
            ReturnPlayerLock(mctx);
        }
    }

    if (message.left(9) == "START_EPG")
    {
        int editType = tokens[1].toInt();
        DoEditSchedule(editType);
    }

    if (message.left(11) == "EPG_EXITING" ||
        message.left(18) == "PROGFINDER_EXITING" ||
        message.left(21) == "VIEWSCHEDULED_EXITING" ||
        message.left(19)   == "PLAYBACKBOX_EXITING" ||
        message.left(22) == "SCHEDULEEDITOR_EXITING")
    {
        GetMythMainWindow()->SetDrawEnabled(false);
        // Resize the window back to the MythTV Player size
        PlayerContext *actx = GetPlayerReadLock(-1, __FILE__, __LINE__);
        PlayerContext *mctx;
        MythMainWindow *mwnd = GetMythMainWindow();

        StopEmbedding(actx);                // Undo any embedding

        mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
        mctx->LockDeleteNVP(__FILE__, __LINE__);
        if (mctx->nvp && mctx->nvp->getVideoOutput())
            mctx->nvp->getVideoOutput()->ResizeForVideo();
        mctx->UnlockDeleteNVP(__FILE__, __LINE__);
        ReturnPlayerLock(mctx);

        if (!db_use_gui_size_for_tv || !db_use_fixed_size)
        {
            mwnd->setMinimumSize(QSize(16, 16));
            mwnd->setMaximumSize(QSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX));
            mwnd->setGeometry(player_bounds.left(), player_bounds.top(),
                              player_bounds.width(), player_bounds.height());
        }

        DoSetPauseState(actx, saved_pause); // Restore pause states

        GetMythMainWindow()->GetPaintWindow()->hide();
        GetMythMainWindow()->GetPaintWindow()->clearMask();

        qApp->processEvents();
        DrawUnusedRects();

        isEmbedded = false;
        ignoreKeyPresses = false;

        if (message.left(19) == "PLAYBACKBOX_EXITING")
        {
            ProgramInfo pginfo(me->ExtraDataList());
            if (pginfo.HasPathname() || pginfo.GetChanID())
                PrepToSwitchToRecordedProgram(actx, pginfo);
        }

        ReturnPlayerLock(actx);

    }

    if (message.left(14) == "COMMFLAG_START" && (tokens.size() >= 2))
    {
        uint evchanid = 0;
        QDateTime evrecstartts;
        ProgramInfo::ExtractKey(tokens[1], evchanid, evrecstartts);

        PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
        for (uint i = 0; mctx && (i < player.size()); i++)
        {
            PlayerContext *ctx = GetPlayer(mctx, i);
            ctx->LockPlayingInfo(__FILE__, __LINE__);
            bool doit =
                ((ctx->playingInfo) &&
                 (ctx->playingInfo->GetChanID()             == evchanid) &&
                 (ctx->playingInfo->GetRecordingStartTime() == evrecstartts));
            ctx->UnlockPlayingInfo(__FILE__, __LINE__);

            if (doit)
            {
                QString msg = "COMMFLAG_REQUEST ";
                msg += ProgramInfo::MakeUniqueKey(evchanid, evrecstartts);
                RemoteSendMessage(msg);
            }
        }
        ReturnPlayerLock(mctx);
    }

    if (message.left(15) == "COMMFLAG_UPDATE" && (tokens.size() >= 3))
    {
        uint evchanid = 0;
        QDateTime evrecstartts;
        ProgramInfo::ExtractKey(tokens[1], evchanid, evrecstartts);

        PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
        for (uint i = 0; mctx && evchanid && (i < player.size()); i++)
        {
            PlayerContext *ctx = GetPlayer(mctx, i);
            ctx->LockPlayingInfo(__FILE__, __LINE__);
            bool doit =
                ((ctx->playingInfo) &&
                 (ctx->playingInfo->GetChanID()             == evchanid) &&
                 (ctx->playingInfo->GetRecordingStartTime() == evrecstartts));
            ctx->UnlockPlayingInfo(__FILE__, __LINE__);

            if (doit)
            {
                frm_dir_map_t newMap;
                QStringList mark;
                QStringList marks =
                    tokens[2].split(",", QString::SkipEmptyParts);
                for (uint i = 0; i < (uint)marks.size(); i++)
                {
                    mark = marks[i].split(":", QString::SkipEmptyParts);
                    if (marks.size() >= 2)
                    {
                        newMap[mark[0].toLongLong()] =
                            (MarkTypes) mark[1].toInt();
                    }
                }
                ctx->LockDeleteNVP(__FILE__, __LINE__);
                if (ctx->nvp)
                    ctx->nvp->SetCommBreakMap(newMap);
                ctx->UnlockDeleteNVP(__FILE__, __LINE__);
            }
        }
        ReturnPlayerLock(mctx);
    }
}

/** \fn TV::BrowseStart(PlayerContext*)
 *  \brief Begins channel browsing.
 */
void TV::BrowseStart(PlayerContext *ctx)
{
    VERBOSE(VB_IMPORTANT, "BrowseStart()");

    if (ctx->paused)
        return;

    OSD *osd = GetOSDLock(ctx);
    if (osd)
    {
        OSDSet *oset = osd->GetSet("browse_info");
        if (!oset)
        {
            ReturnOSDLock(ctx, osd);
            return;
        }
    }
    ReturnOSDLock(ctx, osd);

    ClearOSD(ctx);

    ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (ctx->playingInfo)
    {
        browsemode      = true;
        browsechannum   = ctx->playingInfo->GetChanNum();
        browsechannum.detach();
        browsechanid    = ctx->playingInfo->GetChanID();
        browsestarttime = ctx->playingInfo->GetScheduledStartTime(ISODate);
        ctx->UnlockPlayingInfo(__FILE__, __LINE__);

        BrowseDispInfo(ctx, BROWSE_SAME);

        QMutexLocker locker(&timerIdLock);
        if (browseTimerId)
            KillTimer(browseTimerId);
        browseTimerId = StartTimer(kBrowseTimeout, __LINE__);
    }
    else
    {
        ctx->UnlockPlayingInfo(__FILE__, __LINE__);
    }
}

/** \fn TV::BrowseEnd(PlayerContext*, bool)
 *  \brief Ends channel browsing. Changing the channel if change is true.
 *  \param change_channel iff true we call ChangeChannel()
 */
void TV::BrowseEnd(PlayerContext *ctx, bool change_channel)
{
    VERBOSE(VB_IMPORTANT, "BrowseEnd()");
    {
        QMutexLocker locker(&timerIdLock);
        if (browseTimerId)
        {
            KillTimer(browseTimerId);
            browseTimerId = 0;
        }
    }

    if (!browsemode)
        return;

    OSD *osd = GetOSDLock(ctx);
    if (osd)
        osd->HideSet("browse_info");

    if (change_channel)
        ChangeChannel(ctx, 0, browsechannum);

    browsemode = false;
}

/** \brief Fetches browse info from backend and sends it to the OSD.
 *  \param direction BrowseDirection to get information on.
 */
void TV::BrowseDispInfo(PlayerContext *ctx, BrowseDirection direction)
{
    VERBOSE(VB_IMPORTANT, "BrowseDispInfo()");

    if (!browsemode)
        BrowseStart(ctx);

    // if browsing channel groups is enabled or direction if BROWSE_FAVORITES
    // Then pick the next channel in the channel group list to browse
    // If channel group is ALL CHANNELS (-1), then bypass picking from
    // the channel group list
    if ((browse_changrp || (direction == BROWSE_FAVORITE)) &&
        (channel_group_id > -1) && (direction != BROWSE_SAME) &&
        (direction != BROWSE_RIGHT) && (direction != BROWSE_LEFT))
    {
        uint chanid;
        int  dir;

        if ( (direction == BROWSE_UP) || (direction == BROWSE_FAVORITE) )
            dir = CHANNEL_DIRECTION_UP;
        else if (direction == BROWSE_DOWN)
            dir = CHANNEL_DIRECTION_DOWN;
        else // this should never happen, but just in case
            dir = direction;

        chanid = ChannelUtil::GetNextChannel(
            m_channellist, browsechanid, 0, dir);
        VERBOSE(VB_IMPORTANT, QString("Get channel: %1").arg(chanid));
        browsechanid  = chanid;
        browsechannum = QString::null;
        direction     = BROWSE_SAME;
    }
    else if ((channel_group_id == -1) && (direction == BROWSE_FAVORITE))
        direction = BROWSE_UP;

    OSD *osd = GetOSDLock(ctx);
    if (ctx->paused || !osd)
    {
        ReturnOSDLock(ctx, osd);
        return;
    }

    {
        QMutexLocker locker(&timerIdLock);
        if (browseTimerId)
            KillTimer(browseTimerId);
        browseTimerId = StartTimer(kBrowseTimeout, __LINE__);
    }

    QDateTime lasttime = QDateTime::fromString(browsestarttime, Qt::ISODate);
    QDateTime curtime  = QDateTime::currentDateTime();
    if (lasttime < curtime)
        browsestarttime = curtime.toString(Qt::ISODate);

    QDateTime maxtime  = curtime.addSecs(db_browse_max_forward);
    if ((lasttime > maxtime) && (direction == BROWSE_RIGHT))
    {
        ReturnOSDLock(ctx, osd);
        return;
    }

    InfoMap infoMap;
    infoMap["dbstarttime"] = browsestarttime;
    infoMap["channum"]     = browsechannum;
    infoMap["chanid"]      = QString::number(browsechanid);

    if (ctx->recorder && !db_browse_all_tuners)
    {
        GetNextProgram(ctx->recorder, direction, infoMap);
    }
    else
    {
        GetNextProgram(direction, infoMap);
        while (!IsTunable(ctx, infoMap["chanid"].toUInt()) &&
               (infoMap["channum"] != browsechannum))
        {
            GetNextProgram(direction, infoMap);
        }
    }

    VERBOSE(VB_IMPORTANT, QString("browsechanid: %1 -> %2")
            .arg(browsechanid).arg(infoMap["chanid"].toUInt()));

    browsechannum = infoMap["channum"];
    browsechanid  = infoMap["chanid"].toUInt();

    if (((direction == BROWSE_LEFT) || (direction == BROWSE_RIGHT)) &&
        !infoMap["dbstarttime"].isEmpty())
    {
        browsestarttime = infoMap["dbstarttime"];
    }

    // pull in additional data from the DB...
    QDateTime startts = QDateTime::fromString(browsestarttime, Qt::ISODate);
    RecordingInfo recinfo(browsechanid, startts, false);
    recinfo.ToMap(infoMap);
    infoMap["iconpath"] = ChannelUtil::GetIcon(recinfo.GetChanID());

    osd->ClearAllText("browse_info");
    osd->SetText("browse_info", infoMap, -1);

    ReturnOSDLock(ctx, osd);
}

uint TV::BrowseAllGetChanId(const QString &chan) const
{
    QMutexLocker locker(&browseLock);
    DBChanList::const_iterator it = db_browse_all_channels.begin();
    for (; it != db_browse_all_channels.end(); ++it)
    {
        if ((*it).channum == chan)
            return (*it).chanid;
    }
    return 0;
}

void TV::ToggleRecord(PlayerContext *ctx)
{
    if (browsemode)
    {
        InfoMap infoMap;
        QDateTime startts = QDateTime::fromString(
            browsestarttime, Qt::ISODate);

        RecordingInfo::LoadStatus status;
        RecordingInfo recinfo(browsechanid, startts, false, 0, &status);
        if (RecordingInfo::kFoundProgram == status)
            recinfo.ToggleRecord();
        recinfo.ToMap(infoMap);
        infoMap["iconpath"] = ChannelUtil::GetIcon(recinfo.GetChanID());
        if (recinfo.IsVideoFile() &&
            recinfo.GetPathname() != recinfo.GetBasename())
        {
            infoMap["coverartpath"] = VideoMetaDataUtil::GetCoverArtPath(
                recinfo.GetPathname());
        }

        OSD *osd = GetOSDLock(ctx);
        if (osd)
        {
            osd->ClearAllText("browse_info");
            osd->SetText("browse_info", infoMap, -1);

            osd->SetSettingsText(tr("Record"), 3);
        }
        ReturnOSDLock(ctx, osd);
        return;
    }

    ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (!ctx->playingInfo)
    {
        VERBOSE(VB_GENERAL, LOC + "Unknown recording during live tv.");
        ctx->UnlockPlayingInfo(__FILE__, __LINE__);
        return;
    }

    QString cmdmsg("");
    if (ctx->playingInfo->QueryAutoExpire() == kLiveTVAutoExpire)
    {
        RecordingInfo recInfo(*ctx->playingInfo);
        recInfo.SaveAutoExpire((AutoExpireType)db_autoexpire_default);
        recInfo.ApplyRecordRecGroupChange("Default");
        *ctx->playingInfo = recInfo;

        cmdmsg = tr("Record");
        ctx->SetPseudoLiveTV(ctx->playingInfo, kPseudoRecording);
        ctx->recorder->SetLiveRecording(true);
        VERBOSE(VB_RECORD, LOC + "Toggling Record on");
    }
    else
    {
        RecordingInfo recInfo(*ctx->playingInfo);
        recInfo.SaveAutoExpire(kLiveTVAutoExpire);
        recInfo.ApplyRecordRecGroupChange("LiveTV");
        *ctx->playingInfo = recInfo;

        cmdmsg = tr("Cancel Record");
        ctx->SetPseudoLiveTV(ctx->playingInfo, kPseudoNormalLiveTV);
        ctx->recorder->SetLiveRecording(false);
        VERBOSE(VB_RECORD, LOC + "Toggling Record off");
    }

    QString msg = cmdmsg + " \"" + ctx->playingInfo->GetTitle() + "\"";

    ctx->UnlockPlayingInfo(__FILE__, __LINE__);

    OSD *osd = GetOSDLock(ctx);
    if (osd)
        osd->SetSettingsText(msg, 3);
    ReturnOSDLock(ctx, osd);
}

void TV::BrowseChannel(PlayerContext *ctx, const QString &chan)
{
    if (!ctx->recorder || !ctx->recorder->CheckChannel(chan))
    {
        if (!db_browse_all_tuners || !BrowseAllGetChanId(chan))
            return;
    }

    browsechannum = chan;
    browsechannum.detach();
    browsechanid = 0;
    BrowseDispInfo(ctx, BROWSE_SAME);
}

void TV::HandleOSDClosed(int osdType)
{
    switch (osdType)
    {
        case kOSDFunctionalType_PictureAdjust:
            adjustingPicture = kAdjustingPicture_None;
            adjustingPictureAttribute = kPictureAttribute_None;
            break;
        case kOSDFunctionalType_SmartForward:
            doSmartForward = false;
            break;
        case kOSDFunctionalType_TimeStretchAdjust:
            stretchAdjustment = false;
            break;
        case kOSDFunctionalType_AudioSyncAdjust:
            audiosyncAdjustment = false;
            break;
        case kOSDFunctionalType_Default:
            break;
    }
}

static PictureAttribute next(
    PictureAdjustType type, NuppelVideoPlayer *nvp, PictureAttribute attr)
{
    if (!nvp)
        return kPictureAttribute_None;

    uint sup = kPictureAttributeSupported_None;
    if ((kAdjustingPicture_Playback == type) && nvp && nvp->getVideoOutput())
    {
        sup = nvp->getVideoOutput()->GetSupportedPictureAttributes();
        if (nvp->HasAudioOut())
            sup |= kPictureAttributeSupported_Volume;
    }
    else if (kAdjustingPicture_Channel == type)
    {
        sup = (kPictureAttributeSupported_Brightness |
               kPictureAttributeSupported_Contrast |
               kPictureAttributeSupported_Colour |
               kPictureAttributeSupported_Hue);
    }
    else if (kAdjustingPicture_Recording == type)
    {
        sup = (kPictureAttributeSupported_Brightness |
               kPictureAttributeSupported_Contrast |
               kPictureAttributeSupported_Colour |
               kPictureAttributeSupported_Hue);
    }

    return next((PictureAttributeSupported)sup, attr);
}

void TV::DoTogglePictureAttribute(const PlayerContext *ctx,
                                  PictureAdjustType type)
{
    ctx->LockDeleteNVP(__FILE__, __LINE__);
    PictureAttribute attr = next(type, ctx->nvp, adjustingPictureAttribute);
    if (kPictureAttribute_None == attr)
    {
        ctx->UnlockDeleteNVP(__FILE__, __LINE__);
        return;
    }

    adjustingPicture          = type;
    adjustingPictureAttribute = attr;

    QString title = toTitleString(type);

    int value = 99;
    if (kAdjustingPicture_Playback == type)
    {
        if (!ctx->nvp)
        {
            ctx->UnlockDeleteNVP(__FILE__, __LINE__);
            return;
        }
        if (kPictureAttribute_Volume != adjustingPictureAttribute)
        {
            value = ctx->nvp->getVideoOutput()->GetPictureAttribute(attr);
        }
        else if (ctx->nvp->HasAudioOut())
        {
            value = ctx->nvp->GetVolume();
            title = tr("Adjust Volume");
        }
    }
    ctx->UnlockDeleteNVP(__FILE__, __LINE__);

    if (ctx->recorder && (kAdjustingPicture_Playback != type))
    {
        value = ctx->recorder->GetPictureAttribute(attr);
    }

    QString text = toString(attr) + " " + toTypeString(type) +
        QString(" %1 %").arg(value);

    OSD *osd = GetOSDLock(ctx);
    if (osd)
    {
        osd->GetSet("status");
        osd->ShowStatus(value * 10, true, title, text, 5,
                        kOSDFunctionalType_PictureAdjust);
    }
    ReturnOSDLock(ctx, osd);

    SetUpdateOSDPosition(false);
}

void TV::DoChangePictureAttribute(
    PlayerContext *ctx,
    PictureAdjustType type, PictureAttribute attr, bool up)
{
    int value = 99;

    ctx->LockDeleteNVP(__FILE__, __LINE__);
    if (kAdjustingPicture_Playback == type)
    {
        if (kPictureAttribute_Volume == attr)
        {
            ctx->UnlockDeleteNVP(__FILE__, __LINE__);
            ChangeVolume(ctx, up);
            return;
        }
        if (!ctx->nvp)
        {
            ctx->UnlockDeleteNVP(__FILE__, __LINE__);
            return;
        }
        value = ctx->nvp->getVideoOutput()->ChangePictureAttribute(attr, up);
    }
    ctx->UnlockDeleteNVP(__FILE__, __LINE__);

    if (ctx->recorder && (kAdjustingPicture_Playback != type))
    {
        value = ctx->recorder->ChangePictureAttribute(type, attr, up);
    }

    QString text = toString(attr) + " " + toTypeString(type) +
        QString(" %1 %").arg(value);

    OSD *osd = GetOSDLock(ctx);
    if (osd)
    {
        osd->ShowStatus(value * 10, true, toTitleString(type), text, 5,
                        kOSDFunctionalType_PictureAdjust);
    }
    ReturnOSDLock(ctx, osd);

    SetUpdateOSDPosition(false);
}

void TV::SetActive(PlayerContext *lctx, int index, bool osd_msg)
{
    if (!lctx)
        return;

    int new_index = (index < 0) ? (playerActive+1) % player.size() : index;
    new_index = ((uint)new_index >= player.size()) ? 0 : new_index;

    QString loc = LOC + QString("SetActive(%1,%2) %3 -> %4")
        .arg(index).arg((osd_msg) ? "with OSD" : "w/o OSD")
        .arg(playerActive).arg(new_index);

    VERBOSE(VB_PLAYBACK, loc + " -- begin");

    for (uint i = 0; i < player.size(); i++)
        ClearOSD(GetPlayer(lctx,i));

    playerActive = new_index;

    for (int i = 0; i < (int)player.size(); i++)
    {
        PlayerContext *ctx = GetPlayer(lctx, i);
        ctx->LockDeleteNVP(__FILE__, __LINE__);
        if (ctx->nvp)
            ctx->nvp->SetPIPActive(i == playerActive);
        ctx->UnlockDeleteNVP(__FILE__, __LINE__);
    }

    if (osd_msg && !GetPlayer(lctx, -1)->IsPIP() && player.size() > 1)
    {
        PlayerContext *actx = GetPlayer(lctx, -1);
        OSD *osd = GetOSDLock(actx);
        if (osd)
            osd->SetSettingsText(tr("Active Changed"), 3);
        ReturnOSDLock(actx, osd);
    }

    VERBOSE(VB_PLAYBACK, loc + " -- end");
}

void TV::TreeMenuEntered(OSDListTreeItemEnteredEvent *e)
{
    // show help text somewhere, perhaps?
    (void)e;
}

/** \fn TV::StartProgramEditMode(PlayerContext *ctx)
 *  \brief Starts Program Cut Map Editing mode
 */
void TV::StartProgramEditMode(PlayerContext *ctx)
{
    ctx->LockPlayingInfo(__FILE__, __LINE__);
    bool isEditing = ctx->playingInfo->QueryIsEditing();
    ctx->UnlockPlayingInfo(__FILE__, __LINE__);

    OSD *osd = GetOSDLock(ctx);
    if (isEditing && osd)
    {
        DoTogglePause(ctx, true);

        QString message = tr("This program is currently being edited");

        QStringList options;
        options += tr("Continue Editing");
        options += tr("Do not edit");

        osd->NewDialogBox(kOSDDialogAlreadyEditing, message, options, 0);

        ReturnOSDLock(ctx, osd);
        ReturnPlayerLock(ctx);

        return;
    }

    ReturnOSDLock(ctx, osd);

    ctx->LockDeleteNVP(__FILE__, __LINE__);
    if (ctx->nvp)
        editmode = ctx->nvp->EnableEdit();
    ctx->UnlockDeleteNVP(__FILE__, __LINE__);
}

static void insert_map(InfoMap &infoMap, const InfoMap &newMap)
{
    InfoMap::const_iterator it = newMap.begin();
    for (; it != newMap.end(); ++it)
        infoMap.insert(it.key(), *it);
}

class load_dd_map
{
  public:
    load_dd_map(TV *t, uint s) : tv(t), sourceid(s) {}
    TV   *tv;
    uint  sourceid;
};

void *TV::load_dd_map_thunk(void *param)
{
    load_dd_map *x = (load_dd_map*) param;
    x->tv->RunLoadDDMap(x->sourceid);
    delete x;
    return NULL;
}

void *TV::load_dd_map_post_thunk(void *param)
{
    uint *sourceid = (uint*) param;
    SourceUtil::UpdateChannelsFromListings(*sourceid);
    delete sourceid;
    return NULL;
}

/** \fn TV::StartChannelEditMode(PlayerContext*)
 *  \brief Starts channel editing mode.
 */
void TV::StartChannelEditMode(PlayerContext *ctx)
{
    OSD *osd = GetOSDLock(ctx);
    if (!ctx->recorder || !osd)
    {
        ReturnOSDLock(ctx, osd);
        return;
    }
    ReturnOSDLock(ctx, osd);

    QMutexLocker locker(&chanEditMapLock);
    if (ddMapLoaderRunning)
    {
        pthread_join(ddMapLoader, NULL);
        ddMapLoaderRunning = false;
    }

    // Get the info available from the backend
    chanEditMap.clear();
    ctx->recorder->GetChannelInfo(chanEditMap);
    chanEditMap["dialog_label"]   = tr("Channel Editor");
    chanEditMap["callsign_label"] = tr("Callsign");
    chanEditMap["channum_label"]  = tr("Channel #");
    chanEditMap["channame_label"] = tr("Channel Name");
    chanEditMap["XMLTV_label"]    = tr("XMLTV ID");
    chanEditMap["probe_all"]      = tr("[P]robe");
    chanEditMap["ok"]             = tr("[O]k");

    // Assuming the data is valid, try to load DataDirect listings.
    uint sourceid = chanEditMap["sourceid"].toUInt();
    if (sourceid && (sourceid != ddMapSourceId))
    {
        ddMapLoaderRunning = true;
        if (!pthread_create(&ddMapLoader, NULL, load_dd_map_thunk,
                            new load_dd_map(this, sourceid)))
        {
            ddMapLoaderRunning = false;
        }
        return;
    }

    // Update with XDS and DataDirect Info
    ChannelEditAutoFill(ctx, chanEditMap);

    // Set proper initial values for channel editor, and make it visible..
    osd = GetOSDLock(ctx);
    if (osd)
    {
        osd->SetText(kOSDDialogEditChannel, chanEditMap, -1);
        osd->PushDialog(kOSDDialogEditChannel);
    }
    ReturnOSDLock(ctx, osd);
}

/** \fn TV::ChannelEditKey(const PlayerContext*, const QKeyEvent *e)
 *  \brief Processes channel editing key.
 */
void TV::ChannelEditKey(const PlayerContext *ctx, const QKeyEvent *e)
{
    QMutexLocker locker(&chanEditMapLock);

    bool     focus_change   = false;
    QString  button_pressed = "";
    OSDSet  *osdset         = NULL;

    OSD *osd = GetOSDLock(ctx);
    if (osd && osd->IsDialogActive(kOSDDialogEditChannel))
        osdset = osd->GetSet(kOSDDialogEditChannel);

    if (!osdset || !osdset->HandleKey(e, &focus_change, &button_pressed))
    {
        ReturnOSDLock(ctx, osd);
        return;
    }

    if (button_pressed == "probe_all")
    {
        InfoMap infoMap;
        osdset->GetText(infoMap);
        ChannelEditAutoFill(ctx, infoMap);
        insert_map(chanEditMap, infoMap);
        osdset->SetText(chanEditMap);
    }
    else if (button_pressed == "ok")
    {
        InfoMap infoMap;
        osdset->GetText(infoMap);
        insert_map(chanEditMap, infoMap);
        ctx->recorder->SetChannelInfo(chanEditMap);
    }

    if (!osdset->Displaying())
    {
        VERBOSE(VB_IMPORTANT, "hiding channel_editor");
        osd->TurnDialogOff(kOSDDialogEditChannel);
    }

    ReturnOSDLock(ctx, osd);
}

/** \fn TV::ChannelEditAutoFill(const PlayerContext*,InfoMap&) const
 *  \brief Automatically fills in as much information as possible.
 */
void TV::ChannelEditAutoFill(const PlayerContext *ctx, InfoMap &infoMap) const
{
    QMap<QString,bool> dummy;
    ChannelEditAutoFill(ctx, infoMap, dummy);
}

/** \fn TV::ChannelEditAutoFill(const PlayerContext*,InfoMap&,const QMap<QString,bool>&) const
 *  \brief Automatically fills in as much information as possible.
 */
void TV::ChannelEditAutoFill(const PlayerContext *ctx, InfoMap &infoMap,
                             const QMap<QString,bool> &changed) const
{
    const QString keys[4] = { "XMLTV", "callsign", "channame", "channum", };

    // fill in uninitialized and unchanged fields from XDS
    ChannelEditXDSFill(ctx, infoMap);

    // if no data direct info we're done..
    if (!ddMapSourceId)
        return;

    if (changed.size())
    {
        ChannelEditDDFill(infoMap, changed, false);
    }
    else
    {
        QMutexLocker locker(&chanEditMapLock);
        QMap<QString,bool> chg;
        // check if anything changed
        for (uint i = 0; i < 4; i++)
            chg[keys[i]] = infoMap[keys[i]] != chanEditMap[keys[i]];

        // clean up case and extra spaces
        infoMap["callsign"] = infoMap["callsign"].toUpper().trimmed();
        infoMap["channum"]  = infoMap["channum"].trimmed();
        infoMap["channame"] = infoMap["channame"].trimmed();
        infoMap["XMLTV"]    = infoMap["XMLTV"].trimmed();

        // make sure changes weren't just chaff
        for (uint i = 0; i < 4; i++)
            chg[keys[i]] &= infoMap[keys[i]] != chanEditMap[keys[i]];

        ChannelEditDDFill(infoMap, chg, true);
    }
}

void TV::ChannelEditXDSFill(const PlayerContext *ctx, InfoMap &infoMap) const
{
    QMap<QString,bool> modifiable;
    if (!(modifiable["callsign"] = infoMap["callsign"].isEmpty()))
    {
        QString unsetsign = QObject::tr("UNKNOWN%1", "Synthesized callsign");
        uint    unsetcmpl = unsetsign.length() - 2;
        unsetsign = unsetsign.left(unsetcmpl);
        if (infoMap["callsign"].left(unsetcmpl) == unsetsign) // was unsetcmpl????
            modifiable["callsign"] = true;
    }
    modifiable["channame"] = infoMap["channame"].isEmpty();

    const QString xds_keys[2] = { "callsign", "channame", };
    for (uint i = 0; i < 2; i++)
    {
        if (!modifiable[xds_keys[i]])
            continue;

        ctx->LockDeleteNVP(__FILE__, __LINE__);
        QString tmp = ctx->nvp->GetXDS(xds_keys[i]).toUpper();
        ctx->UnlockDeleteNVP(__FILE__, __LINE__);

        if (tmp.isEmpty())
            continue;

        if ((xds_keys[i] == "callsign") &&
            ((tmp.length() > 5) || (tmp.indexOf(" ") >= 0)))
        {
            continue;
        }

        infoMap[xds_keys[i]] = tmp;
    }
}

void TV::ChannelEditDDFill(InfoMap &infoMap,
                           const QMap<QString,bool> &changed,
                           bool check_unchanged) const
{
    if (!ddMapSourceId)
        return;

    QMutexLocker locker(&chanEditMapLock);
    const QString keys[4] = { "XMLTV", "callsign", "channame", "channum", };

    // First check changed keys for availability in our listings source.
    // Then, if check_unchanged is set, check unchanged fields.
    QString key = "", dd_xmltv = "";
    uint endj = (check_unchanged) ? 2 : 1;
    for (uint j = 0; (j < endj) && dd_xmltv.isEmpty(); j++)
    {
        for (uint i = 0; (i < 4) && dd_xmltv.isEmpty(); i++)
        {
            key = keys[i];
            if (((j == 1) ^ changed[key]) && !infoMap[key].isEmpty())
                dd_xmltv = GetDataDirect(key, infoMap[key], "XMLTV");
        }
    }

    // If we found the channel in the listings, fill in all the data we have
    if (!dd_xmltv.isEmpty())
    {
        infoMap[keys[0]] = dd_xmltv;
        for (uint i = 1; i < 4; i++)
        {
            QString tmp = GetDataDirect(key, infoMap[key], keys[i]);
            if (!tmp.isEmpty())
                infoMap[keys[i]] = tmp;
        }
        return;
    }

    // If we failed to find an exact match, try partial matches.
    // But only fill the current field since this data is dodgy.
    key = "callsign";
    if (!infoMap[key].isEmpty())
    {
        dd_xmltv = GetDataDirect(key, infoMap[key], "XMLTV", true);
        VERBOSE(VB_IMPORTANT, QString("xmltv: %1 for key %2")
                .arg(dd_xmltv).arg(key));
        if (!dd_xmltv.isEmpty())
            infoMap[key] = GetDataDirect("XMLTV", dd_xmltv, key);
    }

    key = "channame";
    if (!infoMap[key].isEmpty())
    {
        dd_xmltv = GetDataDirect(key, infoMap[key], "XMLTV", true);
        VERBOSE(VB_IMPORTANT, QString("xmltv: %1 for key %2")
                .arg(dd_xmltv).arg(key));
        if (!dd_xmltv.isEmpty())
            infoMap[key] = GetDataDirect("XMLTV", dd_xmltv, key);
    }
}

QString TV::GetDataDirect(QString key, QString value, QString field,
                          bool allow_partial_match) const
{
    QMutexLocker locker(&chanEditMapLock);

    uint sourceid = chanEditMap["sourceid"].toUInt();
    if (!sourceid)
        return QString::null;

    if (sourceid != ddMapSourceId)
        return QString::null;

    DDKeyMap::const_iterator it_key = ddMap.find(key);
    if (it_key == ddMap.end())
        return QString::null;

    DDValueMap::const_iterator it_val = (*it_key).find(value);
    if (it_val != (*it_key).end())
    {
        InfoMap::const_iterator it_field = (*it_val).find(field);
        if (it_field != (*it_val).end())
        {
            QString ret = *it_field;
            ret.detach();
            return ret;
        }
    }

    if (!allow_partial_match || value.isEmpty())
        return QString::null;

    // Check for partial matches.. prefer early match, then short string
    DDValueMap::const_iterator best_match = (*it_key).end();
    int best_match_idx = INT_MAX, best_match_len = INT_MAX;
    for (it_val = (*it_key).begin(); it_val != (*it_key).end(); ++it_val)
    {
        int match_idx = it_val.key().indexOf(value);
        if (match_idx < 0)
            continue;

        int match_len = it_val.key().length();
        if ((match_idx < best_match_idx) && (match_len < best_match_len))
        {
            best_match     = it_val;
            best_match_idx = match_idx;
            best_match_len = match_len;
        }
    }

    if (best_match != (*it_key).end())
    {
        InfoMap::const_iterator it_field = (*best_match).find(field);
        if (it_field != (*it_val).end())
        {
            QString ret = *it_field;
            ret.detach();
            return ret;
        }
    }

    return QString::null;
}

void TV::RunLoadDDMap(uint sourceid)
{
    QMutexLocker locker(&chanEditMapLock);
    const QString keys[4] = { "XMLTV", "callsign", "channame", "channum", };

    // Startup channel editor gui early, with "Loading..." text
    const PlayerContext *actx = GetPlayerReadLock(-1, __FILE__, __LINE__);
    OSD *osd = GetOSDLock(actx);
    if (osd)
    {
        InfoMap tmp;
        insert_map(tmp, chanEditMap);
        for (uint i = 0; i < 4; i++)
            tmp[keys[i]] = "Loading...";
        osd->SetText(kOSDDialogEditChannel, tmp, -1);
        if (!osd->IsDialogExisting(kOSDDialogEditChannel))
            osd->PushDialog(kOSDDialogEditChannel);
    }
    ReturnOSDLock(actx, osd);
    ReturnPlayerLock(actx);

    // Load DataDirect info
    LoadDDMap(sourceid);

    // Update with XDS and DataDirect Info
    actx = GetPlayerReadLock(-1, __FILE__, __LINE__);
    ChannelEditAutoFill(actx, chanEditMap);

    // Set proper initial values for channel editor, and make it visible..
    osd = GetOSDLock(actx);
    if (osd)
    {
        osd->SetText(kOSDDialogEditChannel, chanEditMap, -1);
        if (!osd->IsDialogExisting(kOSDDialogEditChannel))
            osd->PushDialog(kOSDDialogEditChannel);
    }
    ReturnOSDLock(actx, osd);
    ReturnPlayerLock(actx);
}

bool TV::LoadDDMap(uint sourceid)
{
    QMutexLocker locker(&chanEditMapLock);
    const QString keys[4] = { "XMLTV", "callsign", "channame", "channum", };

    ddMap.clear();
    ddMapSourceId = 0;

    QString grabber, userid, passwd, lineupid;
    bool ok = SourceUtil::GetListingsLoginData(sourceid, grabber, userid,
                                               passwd, lineupid);
    if (!ok || (grabber != "datadirect"))
    {
        VERBOSE(VB_PLAYBACK, LOC + QString("LoadDDMap() g(%1)").arg(grabber));
        return false;
    }

    DataDirectProcessor ddp(DD_ZAP2IT, userid, passwd);
    ddp.GrabFullLineup(lineupid, true, false, 36*60*60);
    const DDLineupChannels channels = ddp.GetDDLineup(lineupid);

    InfoMap tmp;
    DDLineupChannels::const_iterator it;
    for (it = channels.begin(); it != channels.end(); ++it)
    {
        DDStation station = ddp.GetDDStation((*it).stationid);
        tmp["XMLTV"]    = (*it).stationid;
        tmp["callsign"] = station.callsign;
        tmp["channame"] = station.stationname;
        tmp["channum"]  = (*it).channel;
        if (!(*it).channelMinor.isEmpty())
        {
            tmp["channum"] += SourceUtil::GetChannelSeparator(sourceid);
            tmp["channum"] += (*it).channelMinor;
        }

#if 0
        VERBOSE(VB_GENERAL, QString("Adding channel: %1 -- %2 -- %3 -- %4")
                .arg(tmp["channum"],4).arg(tmp["callsign"],7)
                .arg(tmp["XMLTV"]).arg(tmp["channame"]));
#endif

        for (uint j = 0; j < 4; j++)
            for (uint i = 0; i < 4; i++)
                ddMap[keys[j]][tmp[keys[j]]][keys[i]] = tmp[keys[i]];
    }

    if (!ddMap.empty())
        ddMapSourceId = sourceid;

    return !ddMap.empty();
}

void TV::TreeMenuSelected(OSDListTreeItemSelectedEvent *e)
{
    bool hidetree = true;
    bool handled  = true;

    QString action = e->GetAction();

    PlayerContext *actx = GetPlayerReadLock(-1, __FILE__, __LINE__);

    if (HandleTrackAction(actx, action))
        ;
    else if (action == "TOGGLEMANUALZOOM")
        SetManualZoom(actx, true, tr("Zoom Mode ON"));
    else if (action == "TOGGLESTRETCH")
        ToggleTimeStretch(actx);
    else if (action == "TOGGLEUPMIX")
        ToggleUpmix(actx);
    else if (action.left(13) == "ADJUSTSTRETCH")
    {
        bool floatRead;
        float stretch = action.right(action.length() - 13).toFloat(&floatRead);
        if (floatRead &&
            stretch <= 2.0 &&
            stretch >= 0.48)
        {
            actx->ts_normal = stretch;   // alter speed before display
        }

        StopFFRew(actx);

        if (actx->paused)
            DoTogglePause(actx, true);

        ChangeTimeStretch(actx, 0, !floatRead);   // just display
    }
    else if (action.left(11) == "SELECTSCAN_")
    {
        QString msg = QString::null;
        actx->LockDeleteNVP(__FILE__, __LINE__);
        actx->nvp->SetScanType((FrameScanType) action.right(1).toInt());
        actx->UnlockDeleteNVP(__FILE__, __LINE__);
        msg = toString(actx->nvp->GetScanType());

        if (!msg.isEmpty())
        {
            OSD *osd = GetOSDLock(actx);
            if (osd)
                osd->SetSettingsText(msg, 3);
            ReturnOSDLock(actx, osd);
        }
    }
    else if (action.left(15) == "TOGGLEAUDIOSYNC")
        ChangeAudioSync(actx, 0);
    else if (action.left(11) == "TOGGLESLEEP")
    {
        ToggleSleepTimer(actx, action.left(13));
    }
    else if (action.left(17) == "TOGGLEPICCONTROLS")
    {
        adjustingPictureAttribute = (PictureAttribute)
            (action.right(1).toInt() - 1);
        DoTogglePictureAttribute(actx, kAdjustingPicture_Playback);
    }
    else if (action.left(12) == "TOGGLEASPECT")
    {
        ToggleAspectOverride(actx,
                             (AspectOverrideMode) action.right(1).toInt());
    }
    else if (action.left(10) == "TOGGLEFILL")
    {
        ToggleAdjustFill(actx, (AdjustFillMode) action.right(1).toInt());
    }
    else if (action == "AUTODETECT_FILL")
    {
        actx->nvp->detect_letter_box->SetDetectLetterbox(!actx->nvp->detect_letter_box->GetDetectLetterbox());
    }
    else if (action == "GUIDE")
        EditSchedule(actx, kScheduleProgramGuide);
    else if (action.left(10) == "CHANGROUP_")
    {
        if (action == "CHANGROUP_ALL_CHANNELS")
            channel_group_id = -1;
        else
        {
            action.remove("CHANGROUP_");

            channel_group_id = action.toInt();

            if (browse_changrp)
            {
                m_channellist = ChannelUtil::GetChannels(0, true, "channum, callsign", channel_group_id);
                ChannelUtil::SortChannels(m_channellist, "channum", true);

                // make sure the current channel is from the selected group
                // or tune to the first in the group
                if (actx->tvchain)
                {
                    QString cur_channum = actx->tvchain->GetChannelName(-1);
                    QString new_channum = cur_channum;

                    DBChanList::const_iterator it = m_channellist.begin();
                    for (; it != m_channellist.end(); ++it)
                    {
                        if ((*it).channum == cur_channum)
                        {
                            break;
                        }
                    }

                    if (it == m_channellist.end())
                    {
                        // current channel not found so switch to the
                        // first channel in the group
                        it = m_channellist.begin();
                        if (it != m_channellist.end())
                            new_channum = (*it).channum;
                    }

                    VERBOSE(VB_IMPORTANT, LOC + QString("Channel Group: '%1'->'%2'")
                            .arg(cur_channum).arg(new_channum));

                    // Only change channel if new channel != current channel
                    if (cur_channum != new_channum && !new_channum.isEmpty())
                    {
                        QMutexLocker locker(&timerIdLock);
                        queuedInput   = new_channum; queuedInput.detach();
                        queuedChanNum = new_channum; queuedChanNum.detach();
                        queuedChanID  = 0;
                        if (!queueInputTimerId)
                            queueInputTimerId = StartTimer(10, __LINE__);
                    }

                    // Turn off OSD Channel Num so the channel changes right away
                    OSD *osd = GetOSDLock(actx);
                    if (osd)
                        osd->HideSet("channel_number");
                    ReturnOSDLock(actx, osd);
                }
            }
        }
    }
    else if (action == "FINDER")
        EditSchedule(actx, kScheduleProgramFinder);
    else if (action == "SCHEDULE")
        EditSchedule(actx, kScheduledRecording);
    else if (action == "VIEWSCHEDULED")
        EditSchedule(actx, kViewSchedule);
    else if (HandleJumpToProgramAction(actx, QStringList(action)))
    {
    }
    else if (PxPHandleAction(actx, QStringList(action)))
    {
        // Hide the tree on old active context..
        for (uint i = 0; i < player.size(); i++)
        {
            OSD *osd = GetOSDLock(GetPlayer(actx,i));
            if (osd)
                osd->HideTreeMenu(true);
            ReturnOSDLock(GetPlayer(actx,i), osd);
            ClearOSD(GetPlayer(actx,i));
        }
        actx = GetPlayer(actx,-1); // "NEXTPIPWINDOW" changes active context..
    }
    else if (StateIsLiveTV(GetState(actx)))
    {
        if (action == "TOGGLEBROWSE")
            BrowseStart(actx);
        else if (action == "PREVCHAN")
            PopPreviousChannel(actx, true);
        else if (action.left(14) == "SWITCHTOINPUT_")
        {
            switchToInputId = action.mid(14).toUInt();
            QMutexLocker locker(&timerIdLock);
            if (!switchToInputTimerId)
                switchToInputTimerId = StartTimer(1, __LINE__);
        }
        else if (action == "EDIT")
            StartChannelEditMode(actx);
        else
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Unknown menu action selected: " + action);
            hidetree = false;
        }
    }
    else if (StateIsPlaying(actx->GetState()))
    {
        if (action == "JUMPTODVDROOTMENU" ||
            action == "JUMPTODVDCHAPTERMENU" ||
            action == "JUMPTODVDTITLEMENU")
        {
            QString menu = "root";
            if (action == "JUMPTODVDCHAPTERMENU")
                menu = "chapter";
            else if (action == "JUMPTODVDTITLEMENU")
                menu = "title";
            actx->LockDeleteNVP(__FILE__, __LINE__);
            if (actx->nvp)
                actx->nvp->GoToDVDMenu(menu);
            actx->UnlockDeleteNVP(__FILE__, __LINE__);
        }
        else if (action.left(13) == "JUMPTOCHAPTER")
        {
            int chapter = action.right(3).toInt();
            DoJumpChapter(actx, chapter);
        }
        else if (action == "EDIT")
            StartProgramEditMode(actx);
        else if (action == "TOGGLEAUTOEXPIRE")
            ToggleAutoExpire(actx);
        else if (action.left(14) == "TOGGLECOMMSKIP")
            SetAutoCommercialSkip(
                actx, (CommSkipMode)(action.right(1).toInt()));
        else if (action == "QUEUETRANSCODE")
            DoQueueTranscode(actx, "Default");
        else if (action == "QUEUETRANSCODE_AUTO")
            DoQueueTranscode(actx, "Autodetect");
        else if (action == "QUEUETRANSCODE_HIGH")
            DoQueueTranscode(actx, "High Quality");
        else if (action == "QUEUETRANSCODE_MEDIUM")
            DoQueueTranscode(actx, "Medium Quality");
        else if (action == "QUEUETRANSCODE_LOW")
            DoQueueTranscode(actx, "Low Quality");
        else
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Unknown menu action selected: " + action);
            hidetree = false;
        }
    }

    if (!handled)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Unknown menu action selected: " + action);
        hidetree = false;
    }

    if (hidetree)
    {
        OSD *osd = GetOSDLock(actx);
        if (osd)
            osd->HideTreeMenu(true);
        ReturnOSDLock(actx, osd);
    }
    ReturnPlayerLock(actx);
}

void TV::ShowOSDTreeMenu(const PlayerContext *ctx)
{
    int osdMenuCount = osdMenuEntries->GetCount();

    if (treeMenu)
    {
        for (uint i = 0; i < player.size(); i++)
        {
            OSD *osd = GetOSDLock(GetPlayer(ctx,i));
            if (osd)
                osd->HideTreeMenu();
            ReturnOSDLock(GetPlayer(ctx,i), osd);
        }
        delete treeMenu;
    }

    treeMenu = new OSDGenericTree(NULL, "treeMenu");

    QListIterator<TVOSDMenuEntry*> cm = osdMenuEntries->GetIterator();
    while(cm.hasNext())
    {
        TVOSDMenuEntry *entry = cm.next();
        if (entry->GetEntry(GetState(ctx)) > 0)
            FillOSDTreeMenu(ctx, treeMenu, entry->GetCategory());
    }

    OSDListTreeType *tree = NULL;

    ClearOSD(ctx);

    OSD *osd = GetOSDLock(ctx);
    if (osd)
    {
        int treeMenuDepth = treeMenu->calculateDepth(0) - 1;
        // something is wrong with reading osd menu DB table
        if (osdMenuCount == 0)
            osd->SetSettingsText("Cannot Display OSD Menu", 5);
        else if (treeMenuDepth <= 0)
            osd->SetSettingsText("No OSD Menu Entries", 3);
        else
            tree = osd->ShowTreeMenu("menu", treeMenu);
    }
    ReturnOSDLock(ctx, osd);

    if (tree)
        tree->SetListener(this);
}

void TV::FillOSDTreeMenu(
    const PlayerContext *ctx, OSDGenericTree *treeMenu,
    QString category) const
{
    bool    mainCtx    = ctx == GetPlayer(ctx, 0);
    // DVD menu stuff first
    if (category == "DVD")
    {
        new OSDGenericTree(
            treeMenu, tr("DVD Root Menu"),    "JUMPTODVDROOTMENU");
        new OSDGenericTree(
            treeMenu, tr("DVD Title Menu"),   "JUMPTODVDTITLEMENU");
        new OSDGenericTree(
            treeMenu, tr("DVD Chapter Menu"), "JUMPTODVDCHAPTERMENU");
    }
    else if (category == "AVCHAPTER")
        FillMenuAVChapter(ctx, treeMenu);
    else if (category == "GUIDE")
        new OSDGenericTree(treeMenu, tr("Program Guide"), "GUIDE");
    else if (category ==  "PIP")
        FillMenuPxP(ctx, treeMenu);
    else if (category == "INPUTSWITCHING")
        FillMenuInputSwitching(ctx, treeMenu);
    else if (category == "EDITCHANNEL")
        new OSDGenericTree(treeMenu, tr("Edit Channel"), "EDIT");
    else if (category == "EDITRECORDING")
        new OSDGenericTree(treeMenu, tr("Edit Recording"), "EDIT");
    else if (category == "JUMPREC")
    {
        OSDGenericTree *jtp_item =
        new OSDGenericTree(treeMenu, tr("Jump to Program"));
        new OSDGenericTree(jtp_item, tr("Recorded Program"), "JUMPREC");
        if (lastProgram != NULL)
        {
            if (lastProgram->GetSubtitle().isEmpty())
                new OSDGenericTree(jtp_item,
                                   lastProgram->GetTitle(), "JUMPPREV");
            else
                new OSDGenericTree(jtp_item,
                                   QString("%1: %2")
                                   .arg(lastProgram->GetTitle())
                                   .arg(lastProgram->GetSubtitle()),
                                   "JUMPPREV");
        }
    }
    else if (category == "CHANNELGROUP")
    {
        FillMenuChanGroups(ctx, treeMenu);
    }
    else if (category == "TOGGLEBROWSE")
    {
        if (!db_browse_always)
        {
            new OSDGenericTree(
                treeMenu, tr("Enable Browse Mode"), "TOGGLEBROWSE");
        }
    }
    else if ( category == "PREVCHAN")
        new OSDGenericTree(treeMenu, tr("Previous Channel"), "PREVCHAN");

    FillMenuPlaying(ctx, treeMenu, category);

    if (category == "AUDIOTRACKS")
        FillMenuTracks(ctx, treeMenu, kTrackTypeAudio);
    else if (category == "SUBTITLETRACKS")
        FillMenuTracks(ctx, treeMenu, kTrackTypeSubtitle);
    else if (category == "CCTRACKS")
    {
        FillMenuTracks(ctx, treeMenu, kTrackTypeCC708);
        if (VBIMode::NTSC_CC == vbimode)
            FillMenuTracks(ctx, treeMenu, kTrackTypeCC608);
        else if (VBIMode::PAL_TT == vbimode)
        {
            new OSDGenericTree(
                treeMenu, tr("Toggle Teletext Captions"), "TOGGLETTC");
            new OSDGenericTree(
                treeMenu, tr("Toggle Teletext Menu"), "TOGGLETTM");
            FillMenuTracks(ctx, treeMenu, kTrackTypeTeletextCaptions);
        }
    }

    if (mainCtx)
    {
        if (category == "VIDEOASPECT")
            FillMenuVideoAspect(ctx, treeMenu);
        else if (category == "ADJUSTFILL")
            FillMenuAdjustFill(ctx, treeMenu);
        else if (category ==  "MANUALZOOM")
            new OSDGenericTree(treeMenu, tr("Manual Zoom Mode"), "TOGGLEMANUALZOOM");
        else if (category == "ADJUSTPICTURE")
            FillMenuAdjustPicture(ctx, treeMenu);
    }

    if (category == "AUDIOSYNC")
        new OSDGenericTree(treeMenu, tr("Adjust Audio Sync"), "TOGGLEAUDIOSYNC");
    else if (category == "TOGGLEUPMIX")
        new OSDGenericTree(treeMenu, tr("Toggle Audio Upmixer"), "TOGGLEUPMIX");
    else if (category == "TIMESTRETCH")
        FillMenuTimeStretch(ctx, treeMenu);
    else if (category == "VIDEOSCAN")
        FillMenuVideoScan(ctx, treeMenu);
    else if (category == "SLEEP")
        FillMenuSleepMode(ctx, treeMenu);
}

void TV::FillMenuChanGroups(
    const PlayerContext *ctx, OSDGenericTree *treeMenu) const
{
    if (!browse_changrp)
        return;

    OSDGenericTree *cg_item = new OSDGenericTree(treeMenu, tr("Channel Groups"),
                                                 "CHANGROUP");
    new OSDGenericTree(cg_item, tr("All Channels"), "CHANGROUP_ALL_CHANNELS",
                                (channel_group_id == -1) ? 1 : 0,
                                NULL, "CHANNELGROUP");

    ChannelGroupList::const_iterator it;

    for (it = m_changrplist.begin(); it != m_changrplist.end(); ++it)
    {
        QString name = QString("CHANGROUP_%1").arg(it->grpid);
        new OSDGenericTree(cg_item, it->name, name,
                           ((int)(it->grpid) == channel_group_id) ? 1 : 0,
                           NULL, "CHANNELGROUP");
    }
}

void TV::FillMenuPlaying(
    const PlayerContext *ctx, OSDGenericTree *treeMenu,
    QString category) const
{
    ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (category == "TRANSCODE")
    {
        if (JobQueue::IsJobQueuedOrRunning(
            JOB_TRANSCODE,
            ctx->playingInfo->GetChanID(),
            ctx->playingInfo->GetScheduledStartTime()))
        {
            new OSDGenericTree(treeMenu, tr("Stop Transcoding"), "QUEUETRANSCODE");
        }
        else
        {
            OSDGenericTree *bt_item =
                new OSDGenericTree(treeMenu, tr("Begin Transcoding"));
            new OSDGenericTree(bt_item, tr("Default"),
                            "QUEUETRANSCODE");
            new OSDGenericTree(bt_item, tr("Autodetect"),
                           "QUEUETRANSCODE_AUTO");
            new OSDGenericTree(bt_item, tr("High Quality"),
                           "QUEUETRANSCODE_HIGH");
            new OSDGenericTree(bt_item, tr("Medium Quality"),
                           "QUEUETRANSCODE_MEDIUM");
            new OSDGenericTree(bt_item, tr("Low Quality"),
                           "QUEUETRANSCODE_LOW");
        }
    }

    else if (category == "COMMSKIP")
    {
        OSDGenericTree *cas_item =
            new OSDGenericTree(treeMenu, tr("Commercial Auto-Skip"));
        uint cas_ord[] = { 0, 2, 1 };

        ctx->LockDeleteNVP(__FILE__, __LINE__);
        CommSkipMode cur = kCommSkipOff;
        if (ctx->nvp)
            cur = ctx->nvp->GetAutoCommercialSkip();
        ctx->UnlockDeleteNVP(__FILE__, __LINE__);

        for (uint i = 0; i < sizeof(cas_ord)/sizeof(uint); i++)
        {
            const CommSkipMode mode = (CommSkipMode) cas_ord[i];
            new OSDGenericTree(
                cas_item, toString(mode),
                QString("TOGGLECOMMSKIP%1").arg(cas_ord[i]),
                (mode == cur) ? 1 : 0, NULL, "COMMSKIPGROUP");
        }
    }

    else if (category == "TOGGLEEXPIRE")
    {
        if (ctx->playingInfo->QueryAutoExpire() != kDisableAutoExpire)
        {
            new OSDGenericTree(
                treeMenu, tr("Turn Auto-Expire OFF"), "TOGGLEAUTOEXPIRE");
        }
        else
        {
            new OSDGenericTree(
                treeMenu, tr("Turn Auto-Expire ON"), "TOGGLEAUTOEXPIRE");
        }
    }

    ctx->UnlockPlayingInfo(__FILE__, __LINE__);

    if (category == "SCHEDULERECORDING")
    {
        OSDGenericTree *sr_item =
            new OSDGenericTree(treeMenu, tr("Schedule Recordings"));
         new OSDGenericTree(
            sr_item, tr("Upcoming Recordings"),     "VIEWSCHEDULED");
        new OSDGenericTree(
            sr_item, tr("Program Finder"),          "FINDER");
        new OSDGenericTree(
            sr_item, tr("Edit Recording Schedule"), "SCHEDULE");
    }
}

/// \brief Adds AVChapter menu if chapters available
void TV::FillMenuAVChapter(
    const PlayerContext *ctx, OSDGenericTree *treeMenu) const
{
    int num_chapters = GetNumChapters(ctx);
    if (!num_chapters)
        return;

    int current_chapter =  GetCurrentChapter(ctx);
    QList<long long> times;
    GetChapterTimes(ctx, times);
    if (num_chapters != times.size())
        return;

    OSDGenericTree *item =
        new OSDGenericTree(treeMenu, tr("Chapter"));

    int size = QString::number(num_chapters).size();
    for (int i = 0; i < num_chapters; i++)
    {
        int hours   = times[i] / 60 / 60;
        int minutes = (times[i] / 60) - (hours * 60);
        int secs    = times[i] % 60;
        QString chapter1 = QString("%1").arg(i+1, size, 10, QChar(48));
        QString chapter2 = QString("%1").arg(i+1, 3   , 10, QChar(48));
        QString desc = chapter1 + QString(" (%1:%2:%3)")
            .arg(hours, 2, 10, QChar(48)).arg(minutes, 2, 10, QChar(48))
            .arg(secs, 2, 10, QChar(48));
        new OSDGenericTree(item, desc, QString("JUMPTOCHAPTER%1").arg(chapter2),
                           (current_chapter == i) ? 1 : 0);
    }
}

/// \brief Constructs Picture-X-Picture portion of menu
void TV::FillMenuPxP(
    const PlayerContext *ctx, OSDGenericTree *treeMenu) const

{
    bool allowPIP = IsPIPSupported(ctx);
    bool allowPBP = IsPBPSupported(ctx);
    if (!(allowPIP || allowPBP))
        return;

    OSDGenericTree *item =
        new OSDGenericTree(treeMenu, tr("Picture-in-Picture"));

    bool hasPBP = (player.size()>1) && GetPlayer(ctx,1)->IsPBP();
    bool hasPIP = (player.size()>1) && GetPlayer(ctx,1)->IsPIP();

    if (RemoteGetFreeRecorderCount())
    {
        if (player.size() <= kMaxPIPCount && !hasPBP && allowPIP)
            new OSDGenericTree(item, tr("Open Live TV PIP"), "CREATEPIPVIEW");
        if (player.size() < kMaxPBPCount && !hasPIP && allowPBP)
            new OSDGenericTree(item, tr("Open Live TV PBP"), "CREATEPBPVIEW");
    }

    if (player.size() <= kMaxPIPCount && !hasPBP && allowPIP)
        new OSDGenericTree(item, tr("Open Recording PIP"), "JUMPRECPIP");
    if (player.size() < kMaxPBPCount && !hasPIP && allowPBP)
        new OSDGenericTree(item, tr("Open Recording PBP"), "JUMPRECPBP");

    if (player.size() <= 1)
        return;

    new OSDGenericTree(item, tr("Change Active Window"), "NEXTPIPWINDOW");

    QString pipType  = (ctx->IsPBP()) ? "PBP" : "PIP";
    QString toggleMode = QString("TOGGLE%1MODE").arg(pipType);

    bool isPBP = ctx->IsPBP();
    const PlayerContext *mctx = GetPlayer(ctx, 0);
    QString pipClose = (isPBP) ? tr("Close PBP") : tr("Close PIP");
    if (mctx == ctx)
    {
        if (player.size() > 2)
            pipClose = (isPBP) ? tr("Close PBPs") : tr("Close PIPs");

        new OSDGenericTree(item, pipClose, toggleMode);

        if (player.size() == 2)
            new OSDGenericTree(item, tr("Swap Windows"), "SWAPPIP");
    }
    else
    {
        new OSDGenericTree(item, pipClose, toggleMode);
        new OSDGenericTree(item, tr("Swap Windows"), "SWAPPIP");
    }

    uint max_cnt = min(kMaxPBPCount, kMaxPIPCount+1);
    if (player.size() <= max_cnt &&
        !(hasPIP && !allowPBP) &&
        !(hasPBP && !allowPIP))
    {
        QString switchTo = (isPBP) ? tr("Switch to PIP") : tr("Switch to PBP");
        new OSDGenericTree(item, switchTo, "TOGGLEPIPSTATE");
    }
}

void TV::FillMenuInputSwitching(
    const PlayerContext *ctx, OSDGenericTree *treeMenu) const
{
    if (!ctx->recorder)
        return;

    // Input switching
    QMap<uint,InputInfo> sources;
    vector<uint> cardids = RemoteRequestFreeRecorderList();
    uint         cardid  = ctx->GetCardID();
    cardids.push_back(cardid);
    stable_sort(cardids.begin(), cardids.end());

    vector<uint> excluded_cardids;
    excluded_cardids.push_back(cardid);

    InfoMap info;
    ctx->recorder->GetChannelInfo(info);
    uint sourceid = info["sourceid"].toUInt();

    OSDGenericTree *si_item = NULL;
    vector<uint>::const_iterator it = cardids.begin();
    for (; it != cardids.end(); ++it)
    {
        vector<InputInfo> inputs = RemoteRequestFreeInputList(
            *it, excluded_cardids);

        if (inputs.empty())
            continue;

        if (!si_item)
            si_item = new OSDGenericTree(treeMenu, tr("Switch Input"));

        for (uint i = 0; i < inputs.size(); i++)
        {
            // prefer the current card's input in sources list
            if ((sources.find(inputs[i].sourceid) == sources.end()) ||
                ((cardid == inputs[i].cardid) &&
                 (cardid != sources[inputs[i].sourceid].cardid)))
            {
                sources[inputs[i].sourceid] = inputs[i];
            }

            // don't add current input to list
            if ((inputs[i].cardid   == cardid) &&
                (inputs[i].sourceid == sourceid))
            {
                continue;
            }

            QString name = CardUtil::GetDisplayName(inputs[i].inputid);
            if (name.isEmpty())
            {
                name = tr("C", "Card") + ":" + QString::number(*it) + " " +
                    tr("I", "Input") + ":" + inputs[i].name;
            }

            new OSDGenericTree(
                si_item, name,
                QString("SWITCHTOINPUT_%1").arg(inputs[i].inputid));
        }
    }

    // Source switching
    // delete current source from list
    sources.remove(sourceid);

    // create menu if we have any sources left
    OSDGenericTree *sit_item = NULL;
    QMap<uint,InputInfo>::const_iterator sit = sources.begin();
    if (sit != sources.end())
        sit_item = new OSDGenericTree(treeMenu, tr("Switch Source"));
    for (; sit != sources.end(); ++sit)
    {
        new OSDGenericTree(
            sit_item, SourceUtil::GetSourceName((*sit).sourceid),
            QString("SWITCHTOINPUT_%1").arg((*sit).inputid));
    }
}

void TV::FillMenuVideoAspect(
    const PlayerContext *ctx, OSDGenericTree *treeMenu) const
{
    AspectOverrideMode aspectoverride = kAspect_Off;
    ctx->LockDeleteNVP(__FILE__, __LINE__);
    if (ctx->nvp)
        aspectoverride = ctx->nvp->GetAspectOverride();
    ctx->UnlockDeleteNVP(__FILE__, __LINE__);

    OSDGenericTree *car_item = new OSDGenericTree(
        treeMenu, tr("Change Aspect Ratio"));
    for (int j = kAspect_Off; j < kAspect_END; j++)
    {
        // swap 14:9 and 16:9
        int i = ((kAspect_14_9 == j) ? kAspect_16_9 :
                 ((kAspect_16_9 == j) ? kAspect_14_9 : j));

        bool sel = (i != kAspect_Off) ? (aspectoverride == i) :
            (aspectoverride <= kAspect_Off) || (aspectoverride >= kAspect_END);
        new OSDGenericTree(car_item, toString((AspectOverrideMode) i),
                           QString("TOGGLEASPECT%1").arg(i),
                           (sel) ? 1 : 0, NULL, "ASPECTGROUP");
    }
}

void TV::FillMenuAdjustFill(
    const PlayerContext *ctx, OSDGenericTree *treeMenu) const
{
    AdjustFillMode adjustfill = kAdjustFill_Off;
    ctx->LockDeleteNVP(__FILE__, __LINE__);
    if (ctx->nvp)
        adjustfill = ctx->nvp->GetAdjustFill();
    ctx->UnlockDeleteNVP(__FILE__, __LINE__);

    OSDGenericTree *af_item = new OSDGenericTree(treeMenu, tr("Adjust Fill"));
    new OSDGenericTree(af_item, tr("Auto Detect"),
                       "AUTODETECT_FILL",
                       (ctx->nvp->detect_letter_box->GetDetectLetterbox()) ? 1 : 0,
                       NULL, "ADJUSTFILLGROUP");

    for (int i = kAdjustFill_Off; i < kAdjustFill_END; i++)
    {
        bool sel = (i != kAdjustFill_Off) ? (adjustfill == i) :
            (adjustfill <= kAdjustFill_Off) || (adjustfill >= kAdjustFill_END);
        new OSDGenericTree(af_item, toString((AdjustFillMode) i),
                           QString("TOGGLEFILL%1").arg(i),
                           (sel) ? 1 : 0, NULL, "ADJUSTFILLGROUP");
    }
}

void TV::FillMenuAdjustPicture(
    const PlayerContext *ctx, OSDGenericTree *treeMenu) const
{
    uint sup = kPictureAttributeSupported_None;

    ctx->LockDeleteNVP(__FILE__, __LINE__);
    if (ctx->nvp && ctx->nvp->getVideoOutput())
        sup = ctx->nvp->getVideoOutput()->GetSupportedPictureAttributes();
    ctx->UnlockDeleteNVP(__FILE__, __LINE__);

    OSDGenericTree *ap_item = NULL;
    for (int i = kPictureAttribute_MIN; i < kPictureAttribute_MAX; i++)
    {
        if (toMask((PictureAttribute)i) & sup)
        {
            if (!ap_item)
                ap_item = new OSDGenericTree(treeMenu, tr("Adjust Picture"));
            new OSDGenericTree(
                ap_item, toString((PictureAttribute) i),
                QString("TOGGLEPICCONTROLS%1").arg(i));
        }
    }
}

void TV::FillMenuTimeStretch(
    const PlayerContext *ctx, OSDGenericTree *treeMenu) const
{
    int speedX100 = (int)(round(ctx->ts_normal * 100));

    OSDGenericTree *ats_item = new OSDGenericTree(
        treeMenu, tr("Adjust Time Stretch"), "ADJUSTSTRETCH");

    new OSDGenericTree(ats_item, tr("Toggle"), "TOGGLESTRETCH");
    new OSDGenericTree(ats_item, tr("Adjust"), "ADJUSTSTRETCH");
    new OSDGenericTree(ats_item, tr("0.5X"), "ADJUSTSTRETCH0.5",
                       (speedX100 == 50) ? 1 : 0, NULL,
                       "STRETCHGROUP");
    new OSDGenericTree(ats_item, tr("0.9X"), "ADJUSTSTRETCH0.9",
                       (speedX100 == 90) ? 1 : 0, NULL,
                       "STRETCHGROUP");
    new OSDGenericTree(ats_item, tr("1.0X"), "ADJUSTSTRETCH1.0",
                       (speedX100 == 100) ? 1 : 0, NULL,
                       "STRETCHGROUP");
    new OSDGenericTree(ats_item, tr("1.1X"), "ADJUSTSTRETCH1.1",
                       (speedX100 == 110) ? 1 : 0, NULL,
                       "STRETCHGROUP");
    new OSDGenericTree(ats_item, tr("1.2X"), "ADJUSTSTRETCH1.2",
                       (speedX100 == 120) ? 1 : 0, NULL,
                       "STRETCHGROUP");
    new OSDGenericTree(ats_item, tr("1.3X"), "ADJUSTSTRETCH1.3",
                       (speedX100 == 130) ? 1 : 0, NULL,
                       "STRETCHGROUP");
    new OSDGenericTree(ats_item, tr("1.4X"), "ADJUSTSTRETCH1.4",
                       (speedX100 == 140) ? 1 : 0, NULL,
                       "STRETCHGROUP");
    new OSDGenericTree(ats_item, tr("1.5X"), "ADJUSTSTRETCH1.5",
                       (speedX100 == 150) ? 1 : 0, NULL,
                       "STRETCHGROUP");
}

/// \brief Add scan mode override settings to menu.
void TV::FillMenuVideoScan(
    const PlayerContext *ctx, OSDGenericTree *treeMenu) const
{
    FrameScanType scan_type = kScan_Ignore;
    bool scan_type_locked = false;
    QString cur_mode = "";

    ctx->LockDeleteNVP(__FILE__, __LINE__);
    if (ctx->nvp)
    {
        scan_type = ctx->nvp->GetScanType();
        scan_type_locked = ctx->nvp->IsScanTypeLocked();
        if (!scan_type_locked)
        {
            if (kScan_Interlaced == scan_type)
                cur_mode = tr("(I)", "Interlaced (Normal)");
            else if (kScan_Intr2ndField == scan_type)
                cur_mode = tr("(i)", "Interlaced (Reversed)");
            else if (kScan_Progressive == scan_type)
                cur_mode = tr("(P)", "Progressive");
            cur_mode = " " + cur_mode;
            scan_type = kScan_Detect;
        }
    }
    ctx->UnlockDeleteNVP(__FILE__, __LINE__);

    OSDGenericTree *vs_item = new OSDGenericTree(
        treeMenu, tr("Video Scan"), "SCANMODE");
    new OSDGenericTree(
        vs_item, tr("Detect") + cur_mode, "SELECTSCAN_0",
        (scan_type == kScan_Detect) ? 1 : 0, NULL, "SCANGROUP");
    new OSDGenericTree(
        vs_item, tr("Progressive"), "SELECTSCAN_3",
        (scan_type == kScan_Progressive) ? 1 : 0, NULL, "SCANGROUP");
    new OSDGenericTree(
        vs_item, tr("Interlaced (Normal)"), "SELECTSCAN_1",
        (scan_type == kScan_Interlaced) ? 1 : 0, NULL, "SCANGROUP");
    new OSDGenericTree(
        vs_item, tr("Interlaced (Reversed)"), "SELECTSCAN_2",
        (scan_type == kScan_Intr2ndField) ? 1 : 0, NULL, "SCANGROUP");
}

/// \brief Add sleep items to menu.
void TV::FillMenuSleepMode(
    const PlayerContext *ctx, OSDGenericTree *treeMenu) const
{
    OSDGenericTree *s_item = new OSDGenericTree(
        treeMenu, tr("Sleep"), "TOGGLESLEEPON");
    if (sleepTimerId)
        new OSDGenericTree(s_item, tr("Sleep Off"), "TOGGLESLEEPON");
    new OSDGenericTree(s_item, tr("%n minute(s)", "", 30), "TOGGLESLEEP30");
    new OSDGenericTree(s_item, tr("%n minute(s)", "", 60), "TOGGLESLEEP60");
    new OSDGenericTree(s_item, tr("%n minute(s)", "", 90), "TOGGLESLEEP90");
    new OSDGenericTree(s_item, tr("%n minute(s)", "", 120), "TOGGLESLEEP120");
}

bool TV::FillMenuTracks(const PlayerContext *ctx,
                        OSDGenericTree *treeMenu, uint type) const
{
    QString mainMsg = QString::null;
    QString selStr  = QString::null;
    QString grpStr  = QString::null;
    QString typeStr = QString::null;
    bool    sel     = true;
    uint    capmode = 0;

    ctx->LockDeleteNVP(__FILE__, __LINE__);
    if (ctx->nvp)
        capmode = ctx->nvp->GetCaptionMode();
    ctx->UnlockDeleteNVP(__FILE__, __LINE__);

    if (kTrackTypeAudio == type)
    {
        mainMsg = tr("Select Audio Track");
        typeStr = "AUDIO";
        selStr  = "SELECTAUDIO_";
        grpStr  = "AUDIOGROUP";
    }
    else if (kTrackTypeSubtitle == type)
    {
        mainMsg = tr("Select Subtitle");
        typeStr = "SUBTITLE";
        selStr  = "SELECTSUBTITLE_";
        grpStr  = "SUBTITLEGROUP";
        sel     = capmode & kDisplayAVSubtitle;
    }
    else if (kTrackTypeCC608 == type)
    {
        mainMsg = tr("Select VBI CC");
        typeStr = "CC608";
        selStr  = "SELECTCC608_";
        grpStr  = "CC608GROUP";
        sel     = capmode & kDisplayCC608;
    }
    else if (kTrackTypeCC708 == type)
    {
        mainMsg = tr("Select ATSC CC");
        typeStr = "CC708";
        selStr  = "SELECTCC708_";
        grpStr  = "CC608GROUP";
        sel     = capmode & kDisplayCC708;
    }
    else if (kTrackTypeTeletextCaptions == type)
    {
        mainMsg = tr("Select DVB CC");
        typeStr = "TTC";
        selStr  = "SELECTTTC_";
        grpStr  = "TTCGROUP";
        sel     = capmode & kTrackTypeTeletextCaptions;
    }
    else
    {
        return false;
    }

    QStringList tracks;
    uint curtrack = ~0;

    ctx->LockDeleteNVP(__FILE__, __LINE__);
    if (ctx->nvp)
    {
        tracks = ctx->nvp->GetTracks(type);
        if (!tracks.empty())
            curtrack = (uint) ctx->nvp->GetTrack(type);
    }
    ctx->UnlockDeleteNVP(__FILE__, __LINE__);

    if (tracks.empty())
        return false;

    if ((kTrackTypeAudio == type) && tracks.size() <= 1)
        return false;

    OSDGenericTree *tm_item = new OSDGenericTree(
        treeMenu, mainMsg, "DUMMY" + QString::number(type));

    if (kTrackTypeAudio != type)
        new OSDGenericTree(tm_item, tr("Toggle On/Off"), "TOGGLE"+typeStr);

    for (uint i = 0; i < (uint)tracks.size(); i++)
    {
        new OSDGenericTree(
            tm_item, tracks[i], selStr + QString::number(i),
            (sel && (i == curtrack)) ? 1 : 0, NULL, grpStr);
    }

    return true;
}

void TV::ToggleAutoExpire(PlayerContext *ctx)
{
    QString desc = QString::null;

    ctx->LockPlayingInfo(__FILE__, __LINE__);

    if (ctx->playingInfo->QueryAutoExpire() != kDisableAutoExpire)
    {
        ctx->playingInfo->SaveAutoExpire(kDisableAutoExpire);
        desc = tr("Auto-Expire OFF");
    }
    else
    {
        ctx->playingInfo->SaveAutoExpire(kNormalAutoExpire);
        desc = tr("Auto-Expire ON");
    }

    ctx->UnlockPlayingInfo(__FILE__, __LINE__);

    if (!desc.isEmpty())
    {
        struct StatusPosInfo posInfo;
        if (ctx->CalcNVPSliderPosition(posInfo))
        {
            OSD *osd = GetOSDLock(ctx);
            if (osd)
                osd->ShowStatus(posInfo, false, desc, 1);
            ReturnOSDLock(ctx, osd);
        }
        SetUpdateOSDPosition(false);
    }
}

void TV::SetAutoCommercialSkip(const PlayerContext *ctx,
                               CommSkipMode skipMode)
{
    QString desc = QString::null;

    ctx->LockDeleteNVP(__FILE__, __LINE__);
    if (ctx->nvp)
    {
        ctx->nvp->SetAutoCommercialSkip(skipMode);
        desc = toString(ctx->nvp->GetAutoCommercialSkip());
    }
    ctx->UnlockDeleteNVP(__FILE__, __LINE__);

    if (!desc.isEmpty())
    {
        struct StatusPosInfo posInfo;
        if (ctx->CalcNVPSliderPosition(posInfo))
        {
            OSD *osd = GetOSDLock(ctx);
            if (osd)
                osd->ShowStatus(posInfo, false, desc, 1);
            ReturnOSDLock(ctx, osd);
        }
        SetUpdateOSDPosition(false);
    }
}

void TV::SetManualZoom(const PlayerContext *ctx, bool zoomON, QString desc)
{
    if (ctx->GetPIPState() != kPIPOff)
        return;

    zoomMode = zoomON;
    if (zoomON)
        ClearOSD(ctx);

    if (!desc.isEmpty())
    {
        struct StatusPosInfo posInfo;
        if (ctx->CalcNVPSliderPosition(posInfo))
        {
            OSD *osd = GetOSDLock(ctx);
            if (osd)
                osd->ShowStatus(posInfo, false, desc, 1);
            ReturnOSDLock(ctx, osd);
        }
        SetUpdateOSDPosition(false);
    }
}

bool TV::HandleJumpToProgramAction(
    PlayerContext *ctx, const QStringList &actions)
{
    const PlayerContext *mctx = GetPlayer(ctx, 0);
    TVState s = ctx->GetState();
    if (has_action("JUMPPREV", actions) ||
        (has_action("PREVCHAN", actions) && !StateIsLiveTV(s)))
    {
        if (PromptRecGroupPassword(ctx))
        {
            if (mctx == ctx)
            {
                PrepareToExitPlayer(ctx, __LINE__);
                jumpToProgram = true;
                SetExitPlayer(true, true);
            }
            else
            {
                // TODO
            }
        }
        return true;
    }

    QStringList::const_iterator it = actions.begin();
    for (; it != actions.end(); ++it)
    {
        if ((*it).left(8) != "JUMPPROG")
            continue;

        const QString &action = *it;

        bool ok;
        QString progKey   = action.section(" ",1,-2);
        uint    progIndex = action.section(" ",-1,-1).toUInt(&ok);
        ProgramInfo *p = NULL;

        if (ok)
        {
            QMutexLocker locker(&progListsLock);
            QMap<QString,ProgramList>::const_iterator it =
                progLists.find(progKey);
            if (it != progLists.end())
            {
                const ProgramInfo *tmp = (*it)[progIndex];
                if (tmp)
                    p = new ProgramInfo(*tmp);
            }
        }

        if (!p)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("Failed to locate jump to program '%1' @ %2")
                    .arg(progKey).arg(action.section(" ",-1,-1)));
            return true;
        }

        PIPState state = kPIPOff;
        {
            QMutexLocker locker(&timerIdLock);
            state = jumpToProgramPIPState;
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
            QString type = (kPIPonTV == jumpToProgramPIPState) ? "PIP" : "PBP";
            VERBOSE(VB_GENERAL, LOC + QString("Creating %1 with program: %2")
                    .arg(type).arg(p->toString(ProgramInfo::kTitleSubtitle)));

            if (jumpToProgramPIPState == kPIPonTV)
                CreatePIP(ctx, p);
            else if (jumpToProgramPIPState == kPBPLeft)
                CreatePBP(ctx, p);
        }

        delete p;

        return true;
    }

    bool wants_jump = has_action("JUMPREC", actions);
    bool wants_pip = !wants_jump && has_action("JUMPRECPIP", actions);
    bool wants_pbp = !wants_jump && !wants_pip &&
        has_action("JUMPRECPBP", actions);

    if (!wants_jump && !wants_pip && !wants_pbp)
        return false;

    {
        QMutexLocker locker(&timerIdLock);
        jumpToProgramPIPState = wants_pip ? kPIPonTV :
            (wants_pbp ? kPBPLeft : kPIPOff);
    }

    if ((wants_pbp || wants_pip || db_jump_prefer_osd) &&
        (StateIsPlaying(s) || StateIsLiveTV(s)))
    {
        QMutexLocker locker(&timerIdLock);
        if (jumpMenuTimerId)
            KillTimer(jumpMenuTimerId);
        jumpMenuTimerId = StartTimer(1, __LINE__);
    }
    else if (RunPlaybackBoxPtr)
        EditSchedule(ctx, kPlaybackBox);
    else
        VERBOSE(VB_IMPORTANT, "Failed to open jump to program GUI");

    return true;
}

void TV::ToggleSleepTimer(const PlayerContext *ctx, const QString &time)
{
    const int minute = 60*1000; /* milliseconds in a minute */
    int mins = 0;

    if (time == "TOGGLESLEEPON")
    {
        if (sleepTimerId)
        {
            KillTimer(sleepTimerId);
            sleepTimerId = 0;
        }
        else
        {
            mins = 60;
            sleepTimerTimeout = mins * minute;
            sleepTimerId = StartTimer(sleepTimerTimeout, __LINE__);
        }
    }
    else
    {
        if (sleepTimerId)
        {
            KillTimer(sleepTimerId);
            sleepTimerId = 0;
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
                VERBOSE(VB_IMPORTANT, LOC_ERR + "Invalid time "<<time);
            }
        }
        else
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Invalid time string "<<time);
        }

        if (mins)
        {
            sleepTimerTimeout = mins * minute;
            sleepTimerId = StartTimer(sleepTimerTimeout, __LINE__);
        }
    }

    // display OSD
    OSD *osd = GetOSDLock(ctx);
    if (osd && !browsemode)
    {
        QString out;

        if (mins != 0)
            out = tr("Sleep") + " " + QString::number(mins);
        else
            out = tr("Sleep") + " " + sleep_times[0].dispString;

        osd->SetSettingsText(out, 3);
    }
    ReturnOSDLock(ctx, osd);
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
                           "want to watch live TV, cancel one of the "
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
    if (ctx->embedWinID)
    {
        VERBOSE(VB_IMPORTANT, errorText);
    }
    else if (osd)
    {
        QStringList options("OK");
        osd->NewDialogBox(kOSDDialogInfo, errorText, options, 0);
    }
    else
    {
        MythPopupBox::showOkPopup(
            GetMythMainWindow(), QObject::tr("Channel Change Error"),
            errorText);
    }
    ReturnOSDLock(ctx, osd);
}

/**
 *  \brief Used in ChangeChannel(), ChangeChannel(),
 *         and ToggleInputs() to temporarily stop video output.
 */
void TV::PauseLiveTV(PlayerContext *ctx)
{
    VERBOSE(VB_PLAYBACK, LOC + QString("PauseLiveTV() player ctx %1")
            .arg(find_player_index(ctx)));

    lockTimerOn = false;

    ctx->LockDeleteNVP(__FILE__, __LINE__);
    if (ctx->nvp && ctx->buffer)
    {
        ctx->buffer->IgnoreLiveEOF(true);
        ctx->buffer->StopReads();
        ctx->nvp->PauseDecoder();
        ctx->buffer->StartReads();
    }
    ctx->UnlockDeleteNVP(__FILE__, __LINE__);

    // XXX: Get rid of this?
    ctx->recorder->PauseRecorder();

    ctx->lastSignalMsg.clear();
    ctx->lastSignalUIInfo.clear();

    lockTimerOn = false;

    QString input = ctx->recorder->GetInput();
    uint timeout  = ctx->recorder->GetSignalLockTimeout(input);

    if (timeout < 0xffffffff && !ctx->IsPIP())
    {
        lockTimer.start();
        lockTimerOn = true;
    }

    SetSpeedChangeTimer(0, __LINE__);
}

/**
 *  \brief Used in ChangeChannel(), ChangeChannel(),
 *         and ToggleInputs() to restart video output.
 */
void TV::UnpauseLiveTV(PlayerContext *ctx)
{
    VERBOSE(VB_PLAYBACK, LOC + QString("UnpauseLiveTV() player ctx %1")
            .arg(find_player_index(ctx)));

    if (ctx->HasNVP() && ctx->tvchain)
    {
        ctx->ReloadTVChain();
        ctx->tvchain->JumpTo(-1, 1);
        ctx->LockDeleteNVP(__FILE__, __LINE__);
        if (ctx->nvp)
            ctx->nvp->Play(ctx->ts_normal, true, false);
        ctx->UnlockDeleteNVP(__FILE__, __LINE__);
        ctx->buffer->IgnoreLiveEOF(false);

        SetSpeedChangeTimer(0, __LINE__);
    }

    ITVRestart(ctx, true);

    if (ctx->HasNVP())
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
    uint chanid = 0;
    uint cardid = 0;

    if (ctx->paused)
        return;

    ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (ctx->playingInfo)
        chanid = ctx->playingInfo->GetChanID();
    ctx->UnlockPlayingInfo(__FILE__, __LINE__);

    cardid = ctx->GetCardID();

    ctx->LockDeleteNVP(__FILE__, __LINE__);
    if (ctx->nvp)
        ctx->nvp->ITVRestart(chanid, cardid, isLive);
    ctx->UnlockDeleteNVP(__FILE__, __LINE__);
}

/* \fn TV::ScreenShot(PlayerContext*, long long)
 * \brief Creates an image of a particular frame from the current
 *        playbackinfo recording.
 */
bool TV::ScreenShot(PlayerContext *ctx, long long frameNumber)
{
    ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (!ctx->playingInfo)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "ScreenShot called with NULL playingInfo");
        ctx->UnlockPlayingInfo(__FILE__, __LINE__);
        return false;
    }

    // TODO FIXME .mythtv isn't guaranteed to exist, and may
    // very well belong to another frontend.
    QString outFile =
        QString("%1/.mythtv/%2_%3_%4.png")
        .arg(QDir::homePath()).arg(ctx->playingInfo->GetChanID())
        .arg(ctx->playingInfo->GetRecordingStartTime(MythDate))
        .arg(frameNumber);

    PreviewGenerator *previewgen = new PreviewGenerator(
        ctx->playingInfo, PreviewGenerator::kLocalAndRemote);
    ctx->UnlockPlayingInfo(__FILE__, __LINE__);

    previewgen->SetPreviewTimeAsFrameNumber(frameNumber);
    previewgen->SetOutputSize(QSize(-1,-1));
    previewgen->SetOutputFilename(outFile);
    bool ok = previewgen->Run();
    previewgen->deleteLater();

    QString msg = tr("Screen Shot") + " " + ((ok) ? tr("OK") : tr("Error"));

    OSD *osd = GetOSDLock(ctx);
    if (osd)
        osd->SetSettingsText(msg, 3);
    ReturnOSDLock(ctx, osd);

    return ok;
}

/*  \fn TV::DVDJumpBack(PlayerContext*)
    \brief jump to the previous dvd title or chapter
*/
void TV::DVDJumpBack(PlayerContext *ctx)
{
    if (!ctx->HasNVP() || !ctx->buffer || !ctx->buffer->isDVD())
        return;

    if (ctx->buffer->InDVDMenuOrStillFrame())
    {
        UpdateOSDSeekMessage(ctx, tr("Skip Back Not Allowed"),
                             osd_general_timeout);
    }
    else if (!ctx->buffer->DVD()->StartOfTitle())
    {
        ctx->LockDeleteNVP(__FILE__, __LINE__);
        if (ctx->nvp)
            ctx->nvp->ChangeDVDTrack(0);
        ctx->UnlockDeleteNVP(__FILE__, __LINE__);
        UpdateOSDSeekMessage(ctx, tr("Previous Chapter"),
                             osd_general_timeout);
    }
    else
    {
        uint titleLength = ctx->buffer->DVD()->GetTotalTimeOfTitle();
        uint chapterLength = ctx->buffer->DVD()->GetChapterLength();
        if ((titleLength == chapterLength) && chapterLength > 300)
        {
            DoSeek(ctx, -ctx->jumptime * 60, tr("Jump Back"));
        }
        else
        {
            ctx->LockDeleteNVP(__FILE__, __LINE__);
            if (ctx->nvp)
                ctx->nvp->GoToDVDProgram(0);
            ctx->UnlockDeleteNVP(__FILE__, __LINE__);

            UpdateOSDSeekMessage(ctx, tr("Previous Title"),
                                 osd_general_timeout);
        }
    }
}

/* \fn TV::DVDJumpForward(PlayerContext*)
 * \brief jump to the next dvd title or chapter
 */
void TV::DVDJumpForward(PlayerContext *ctx)
{
    if (!ctx->HasNVP() || !ctx->buffer || !ctx->buffer->isDVD())
        return;

    if (ctx->buffer->DVD()->InStillFrame())
    {
        ctx->buffer->DVD()->SkipStillFrame();
        UpdateOSDSeekMessage(ctx, tr("Skip Still Frame"),
                             osd_general_timeout);
    }
    else if (!ctx->buffer->DVD()->EndOfTitle())
    {
        ctx->LockDeleteNVP(__FILE__, __LINE__);
        if (ctx->nvp)
            ctx->nvp->ChangeDVDTrack(1);
        ctx->UnlockDeleteNVP(__FILE__, __LINE__);

        UpdateOSDSeekMessage(ctx, tr("Next Chapter"),
                             osd_general_timeout);
    }
    else if (!ctx->buffer->DVD()->NumMenuButtons())
    {
        uint titleLength = ctx->buffer->DVD()->GetTotalTimeOfTitle();
        uint chapterLength = ctx->buffer->DVD()->GetChapterLength();
        uint currentTime = ctx->buffer->DVD()->GetCurrentTime();
        if ((titleLength == chapterLength) &&
             (currentTime < (chapterLength - (ctx->jumptime * 60))) &&
             chapterLength > 300)
        {
            DoSeek(ctx, ctx->jumptime * 60, tr("Jump Ahead"));
        }
        else
        {
            ctx->LockDeleteNVP(__FILE__, __LINE__);
            if (ctx->nvp)
                ctx->nvp->GoToDVDProgram(1);
            ctx->UnlockDeleteNVP(__FILE__, __LINE__);

            UpdateOSDSeekMessage(ctx, tr("Next Title"),
                                 osd_general_timeout);
        }
    }
}

/* \fn TV::IsBookmarkAllowed(const PlayerContext*) const
 * \brief Returns true if bookmarks are allowed for the current player.
 */
bool TV::IsBookmarkAllowed(const PlayerContext *ctx) const
{
    bool isDVD = ctx->buffer && ctx->buffer->isDVD();

    ctx->LockPlayingInfo(__FILE__, __LINE__);

    // Allow bookmark of "Record current LiveTV program"
    if (StateIsLiveTV(GetState(ctx)) && ctx->playingInfo &&
        (ctx->playingInfo->QueryAutoExpire() == kLiveTVAutoExpire))
    {
        ctx->UnlockPlayingInfo(__FILE__, __LINE__);
        return false;
    }

    if (StateIsLiveTV(GetState(ctx)) && !ctx->playingInfo)
    {
        ctx->UnlockPlayingInfo(__FILE__, __LINE__);
        return false;
    }

    ctx->UnlockPlayingInfo(__FILE__, __LINE__);

    return (!isDVD || (ctx->buffer->DVD()->GetTotalTimeOfTitle() >= 120));
}

/* \fn TV::IsDeleteAllowed(const PlayerContext*) const
 * \brief Returns true if the delete menu option should be offered.
 */
bool TV::IsDeleteAllowed(const PlayerContext *ctx) const
{
    bool allowed = false;

    if (!StateIsLiveTV(GetState(ctx)))
    {
        ctx->LockPlayingInfo(__FILE__, __LINE__);
        ProgramInfo *curProgram = ctx->playingInfo;
        allowed = curProgram && curProgram->QueryIsDeleteCandidate(true);
        ctx->UnlockPlayingInfo(__FILE__, __LINE__);
    }

    return allowed;
}

void TV::PromptStopWatchingRecording(PlayerContext *ctx)
{
    ClearOSD(ctx);

    if ((ctx != GetPlayer(ctx, 0)))
        return;

    if (!ctx->paused)
        DoTogglePause(ctx, false);

    QString message;
    QString videotype = QString::null;
    QStringList options;

    if (StateIsLiveTV(GetState(ctx)))
        videotype = tr("Live TV");
    else if (ctx->buffer->isDVD())
        videotype = tr("this DVD");

    ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (videotype.isEmpty() && ctx->playingInfo->IsVideo())
        videotype = tr("this Video");
    ctx->UnlockPlayingInfo(__FILE__, __LINE__);

    if (videotype.isEmpty())
        videotype = tr("this recording");

    message = tr("You are exiting %1").arg(videotype);

    if (IsBookmarkAllowed(ctx))
    {
        options += tr("Save this position and go to the menu");
        options += tr("Do not save, just exit to the menu");
    }
    else
        options += tr("Exit %1").arg(videotype);

    if (IsDeleteAllowed(ctx))
        options += tr("Delete this recording");

    options += tr("Keep watching");

    OSD *osd = GetOSDLock(ctx);
    if (osd)
        osd->NewDialogBox(kOSDDialogExitOptions, message, options, 0);
    ReturnOSDLock(ctx, osd);

    QMutexLocker locker(&timerIdLock);
    if (videoExitDialogTimerId)
        KillTimer(videoExitDialogTimerId);
    videoExitDialogTimerId = StartTimer(kVideoExitDialogTimeout, __LINE__);
}

void TV::PromptDeleteRecording(PlayerContext *ctx, QString title)
{
    ctx->LockPlayingInfo(__FILE__, __LINE__);

    if (ctx->ff_rew_state ||
        StateIsLiveTV(ctx->GetState()) ||
        exitPlayerTimerId)
    {
        // this should only occur when the cat walks on the keyboard.
        VERBOSE(VB_IMPORTANT, "It is unsafe to delete at the moment");
        ctx->UnlockPlayingInfo(__FILE__, __LINE__);
        return;
    }

    if ((ctx != GetPlayer(ctx, 0)))
    {
        // just to avoid confusion...
        VERBOSE(VB_IMPORTANT, "Only the main program may be deleted");
        ctx->UnlockPlayingInfo(__FILE__, __LINE__);
        return;
    }

    if (!ctx->playingInfo->QueryIsDeleteCandidate(true))
    {
        VERBOSE(VB_IMPORTANT, "This program can not be deleted at this time.");
        ProgramInfo pginfo(*ctx->playingInfo);
        ctx->UnlockPlayingInfo(__FILE__, __LINE__);

        OSD *osd = GetOSDLock(ctx);
        bool has_dialog = osd && !osd->GetDialogActive().isEmpty();

        if (!has_dialog)
        {
            QStringList options(QObject::tr("OK"));
            QString message = QObject::tr("Can not delete program") +
                QString("\n%1\n%2\n\n")
                .arg(pginfo.GetTitle()).arg(pginfo.GetSubtitle());
            if (!pginfo.IsRecording())
            {
                message += QObject::tr("because it not a recording.");
            }
            else
            {
                message += QObject::tr("because it is in use by");
                QStringList byWho;
                pginfo.QueryIsInUse(byWho);
                for (uint i = 0; i+2 < (uint)byWho.size(); i+=3)
                {
                    if (byWho[i+1] == gCoreContext->GetHostName() &&
                        byWho[i+0].contains(kPlayerInUseID))
                        continue;
                    if (byWho[i+0].contains(kRecorderInUseID))
                        continue;
                    message += "\n" + byWho[i+2];
                }
            }
            if (osd && !has_dialog)
                osd->NewDialogBox(kOSDDialogCanNotDelete, message, options, 0);
        }
        ReturnOSDLock(ctx, osd);

        return;
    }
    ctx->UnlockPlayingInfo(__FILE__, __LINE__);

    ClearOSD(ctx);

    if (!ctx->paused)
        DoTogglePause(ctx, false);

    OSD *osd = GetOSDLock(ctx);
    bool has_dialog = osd && !osd->GetDialogActive().isEmpty();
    ReturnOSDLock(ctx, osd);

    if (!has_dialog)
    {
        InfoMap infoMap;
        ctx->GetPlayingInfoMap(infoMap);

        QString message = QString("%1\n%2\n%3")
            .arg(title).arg(infoMap["title"]).arg(infoMap["timedate"]);

        QStringList options;
        if (title == "End Of Recording")
        {
            options += tr("Delete it, but allow it to re-record");
            options += tr("Delete it");
            options += tr("Save it so I can watch it again");
        }
        else
        {
            options += tr("Yes, and allow re-record");
            options += tr("Yes, delete it");
            options += tr("No, keep it, I changed my mind");
        }

        OSD *osd = GetOSDLock(ctx);
        has_dialog = osd && !osd->GetDialogActive().isEmpty();
        if (osd && !has_dialog)
            osd->NewDialogBox(kOSDDialogAskDelete, message, options, 0, 2);
        ReturnOSDLock(ctx, osd);
    }

    if (!has_dialog)
    {
        QMutexLocker locker(&timerIdLock);
        if (videoExitDialogTimerId)
            KillTimer(videoExitDialogTimerId);
        videoExitDialogTimerId = StartTimer(kVideoExitDialogTimeout, __LINE__);
    }
}

bool TV::IsVideoExitDialog(const QString &dialogname)
{
    return (dialogname == kOSDDialogAskDelete ||
            dialogname == kOSDDialogExitOptions);
}

void TV::SetLastProgram(const ProgramInfo *rcinfo)
{
    QMutexLocker locker(&lastProgramLock);

    if (lastProgram)
        delete lastProgram;

    if (rcinfo)
        lastProgram = new ProgramInfo(*rcinfo);
    else
        lastProgram = NULL;
}

ProgramInfo *TV::GetLastProgram(void) const
{
    QMutexLocker locker(&lastProgramLock);
    if (lastProgram)
        return new ProgramInfo(*lastProgram);
    return NULL;
}

QString TV::GetRecordingGroup(int player_idx) const
{
    QString ret = QString::null;

    const PlayerContext *ctx = GetPlayerReadLock(player_idx, __FILE__, __LINE__);
    if (ctx)
    {
        if (StateIsPlaying(GetState(ctx)))
        {
            ctx->LockPlayingInfo(__FILE__, __LINE__);
            if (ctx->playingInfo)
                ret = ctx->playingInfo->GetRecordingGroup();
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

bool TV::PromptRecGroupPassword(PlayerContext *ctx)
{
    QMutexLocker locker(&lastProgramLock);

    if (!lastProgram)
        return false;

    bool stayPaused = ctx->paused;
    if (!ctx->paused)
        DoTogglePause(ctx, false);
    QString recGroupPassword;
    lastProgram->SetRecordingGroup(lastProgram->QueryRecordingGroup());
    recGroupPassword = ProgramInfo::QueryRecordingGroupPassword(
        lastProgram->GetRecordingGroup());

    if (recGroupPassword.size())
    {
        //qApp->lock();
        bool ok = false;
        QString text = tr("'%1' Group Password:")
            .arg(lastProgram->GetRecordingGroup());
        MythPasswordDialog *pwd = new MythPasswordDialog(text, &ok,
                                                recGroupPassword,
                                                GetMythMainWindow());
        pwd->exec();
        pwd->deleteLater();
        pwd = NULL;

        //qApp->unlock();
        if (!ok)
        {
            OSD *osd = GetOSDLock(ctx);
            if (osd)
            {
                QString msg = tr("Password Failed");
                osd->SetSettingsText(msg, 3);
            }
            ReturnOSDLock(ctx, osd);

            if (ctx->paused && !stayPaused)
                DoTogglePause(ctx, false);

            return false;
        }
    }

    if (ctx->paused && !stayPaused)
        DoTogglePause(ctx, false);

    return true;
}

void TV::DoDisplayJumpMenu(void)
{
    if (treeMenu)
        delete treeMenu;

    treeMenu = new OSDGenericTree(NULL, "treeMenu");

    PlayerContext *actx = GetPlayerReadLock(-1, __FILE__, __LINE__);

    // Build jumpMenu of recorded program titles
    QMutexLocker locker(&progListsLock);
    progLists.clear();
    vector<ProgramInfo*> *infoList = RemoteGetRecordedList(false);

    bool LiveTVInAllPrograms = gCoreContext->GetNumSetting("LiveTVInAllPrograms",0);
    if (infoList)
    {
        QList<QString> titles_seen;

        actx->LockPlayingInfo(__FILE__, __LINE__);
        QString currecgroup = actx->playingInfo->GetRecordingGroup();
        actx->UnlockPlayingInfo(__FILE__, __LINE__);

        vector<ProgramInfo *>::const_iterator it = infoList->begin();
        for ( ; it != infoList->end(); it++)
        {
            if ((*it)->GetRecordingGroup() != "LiveTV" || LiveTVInAllPrograms ||
                (*it)->GetRecordingGroup() == currecgroup)
            {
                progLists[(*it)->GetRecordingGroup()].push_front(
                    new ProgramInfo(*(*it)));
            }
        }

        ProgramInfo *lastprog = GetLastProgram();
        QMap<QString,ProgramList>::const_iterator Iprog;
        for (Iprog = progLists.begin(); Iprog != progLists.end(); Iprog++)
        {
            const ProgramList &plist = *Iprog;
            uint progIndex = (uint) plist.size();

            if (plist[0]->GetRecordingGroup() != currecgroup)
            {
                SetLastProgram(plist[0]);
                if (!PromptRecGroupPassword(actx))
                    continue;
            }

            if (progIndex == 1)
            {
                new OSDGenericTree(
                    treeMenu, Iprog.key(),
                    QString("JUMPPROG %1 0").arg(Iprog.key()));
            }
            else
            {
                OSDGenericTree *j_item =
                    new OSDGenericTree(treeMenu, Iprog.key());

                ProgramList::const_iterator it = plist.begin();
                for (; it != plist.end(); ++it)
                {
                    const ProgramInfo *p = *it;

                    if (titles_seen.contains(p->GetTitle()))
                        continue;
                    titles_seen.push_back(p->GetTitle());

                    OSDGenericTree *jsub_item =
                        new OSDGenericTree(j_item, p->GetTitle());
                    ProgramList::const_iterator it2 = plist.begin();
                    int j = -1;
                    for (; it2 != plist.end(); ++it2)
                    {
                        const ProgramInfo *q = *it2;
                        j++;

                        if (q->GetTitle() != p->GetTitle())
                            continue;

                        new OSDGenericTree(
                            jsub_item,
                            q->GetSubtitle().isEmpty() ?
                            q->GetTitle() : q->GetSubtitle(),
                            QString("JUMPPROG %1 %2").arg(Iprog.key()).arg(j));
                    }
                }
            }
        }
        SetLastProgram(lastprog);
        if (lastprog)
            delete lastprog;

        while (!infoList->empty())
        {
            delete infoList->back();
            infoList->pop_back();
        }
        delete infoList;
    }
    else
    {
        ReturnPlayerLock(actx);
        return;
    }

    OSD *osd = GetOSDLock(actx);
    if (osd)
    {
        ClearOSD(actx);
        OSDListTreeType *tree = osd->ShowTreeMenu("menu", treeMenu);
        if (tree)
            tree->SetListener(this);
    }
    ReturnOSDLock(actx, osd);
    ReturnPlayerLock(actx);
}

void TV::RestoreScreenSaver(const PlayerContext *ctx)
{
    if (ctx == GetPlayer(ctx, 0))
        GetMythUI()->RestoreScreensaver();
}

void TV::InitUDPNotifyEvent(void)
{
    if (db_udpnotify_port && !udpnotify)
    {
        udpnotify = new UDPNotify(db_udpnotify_port);
        connect(udpnotify,
                SIGNAL(AddUDPNotifyEvent(
                        const QString&,const UDPNotifyOSDSet*)),
                this,
                SLOT(AddUDPNotifyEvent(
                        const QString&,const UDPNotifyOSDSet*)),
                Qt::DirectConnection);
        connect(udpnotify, SIGNAL(ClearUDPNotifyEvents()),
                this,      SLOT(ClearUDPNotifyEvents()),
                Qt::DirectConnection);
    }
}

void TV::AddUDPNotifyEvent(const QString &name, const UDPNotifyOSDSet *set)
{
    QMutexLocker locker(&timerIdLock);
    udpnotifyEventName.enqueue(name);
    udpnotifyEventSet.enqueue(set);
    if (!udpNotifyTimerId)
        udpNotifyTimerId = StartTimer(1, __LINE__);
}

void TV::ClearUDPNotifyEvents(void)
{
    QMutexLocker locker(&timerIdLock);
    udpnotifyEventName.clear();
    udpnotifyEventSet.clear();
    if (udpNotifyTimerId)
    {
        KillTimer(udpNotifyTimerId);
        udpNotifyTimerId = 0;
    }
}

bool TV::HasUDPNotifyEvent(void) const
{
    QMutexLocker locker(&timerIdLock);
    return !udpnotifyEventName.empty();
}

void TV::HandleUDPNotifyEvent(void)
{
    QString                name = QString::null;
    const UDPNotifyOSDSet *set  = NULL;

    timerIdLock.lock();
    if (!udpnotifyEventName.empty())
    {
        name = udpnotifyEventName.dequeue();
        set  = udpnotifyEventSet.dequeue();
    }
    timerIdLock.unlock();

    PlayerContext *mctx = GetPlayerReadLock(0, __FILE__, __LINE__);
    OSD *osd = GetOSDLock(mctx);
    if (osd && set)
        osd->StartNotify(set);
    else if (osd && !name.isEmpty())
        osd->ClearNotify(name);
    ReturnOSDLock(mctx, osd);
    ReturnPlayerLock(mctx);
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
        return NULL;

    const PlayerContext *mctx = GetPlayer(ctx, 0);

    mctx->LockDeleteNVP(file, location);
    if (mctx->nvp && (ctx->IsPIP() || mctx->IsOSDFullScreen()))
    {
        OSD *osd = mctx->nvp->GetOSD();
        if (!osd)
            mctx->UnlockDeleteNVP(file, location);
        else
            osd_lctx[osd] = mctx;
        return osd;
    }
    mctx->UnlockDeleteNVP(file, location);

    ctx->LockDeleteNVP(file, location);
    if (ctx->nvp && !ctx->IsPIP())
    {
        OSD *osd = ctx->nvp->GetOSD();
        if (!osd)
            ctx->UnlockDeleteNVP(file, location);
        else
            osd_lctx[osd] = ctx;
        return osd;
    }
    ctx->UnlockDeleteNVP(file, location);

    return NULL;
}

void TV::ReturnOSDLock(OSD *&osd)
{
    if (!osd)
        return;

    // we already have the player lock because we have an osd pointer..
    osd_lctx[osd]->UnlockDeleteNVP(__FILE__, __LINE__);
    ReturnPlayerLock(osd_lctx[osd]);

    osd = NULL;
}

void TV::ReturnOSDLock(const PlayerContext *ctx, OSD *&osd)
{
    if (!ctx || !osd)
        return;

    osd_lctx[osd]->UnlockDeleteNVP(__FILE__, __LINE__);

    osd = NULL;
}

PlayerContext *TV::GetPlayerWriteLock(int which, const char *file, int location)
{
    playerLock.lockForWrite();

    if ((which >= (int)player.size()))
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                QString("GetPlayerWriteLock(%1,%2,%3) returning NULL size(%4)")
                .arg(which).arg(file).arg(location).arg(player.size()));
        return NULL;
    }

    return (which < 0) ? player[playerActive] : player[which];
}

PlayerContext *TV::GetPlayerReadLock(int which, const char *file, int location)
{
    playerLock.lockForRead();

    if ((which >= (int)player.size()))
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                QString("GetPlayerReadLock(%1,%2,%3) returning NULL size(%4)")
                .arg(which).arg(file).arg(location).arg(player.size()));
        return NULL;
    }

    return (which < 0) ? player[playerActive] : player[which];
}

const PlayerContext *TV::GetPlayerReadLock(
    int which, const char *file, int location) const
{
    playerLock.lockForRead();

    if ((which >= (int)player.size()))
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                QString("GetPlayerReadLock(%1,%2,%3) returning NULL size(%4)")
                .arg(which).arg(file).arg(location).arg(player.size()));
        return NULL;
    }

    return (which < 0) ? player[playerActive] : player[which];
}

PlayerContext *TV::GetPlayerHaveLock(
    PlayerContext *locked_context,
    int which, const char *file, int location)
{
    if (!locked_context || (which >= (int)player.size()))
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                QString("GetPlayerHaveLock(0x%1,%2,%3,%4) "
                        "returning NULL size(%5)")
                .arg((long long)locked_context, 0, 16)
                .arg(which).arg(file).arg(location).arg(player.size()));
        return NULL;
    }

    return (which < 0) ? player[playerActive] : player[which];
}

const PlayerContext *TV::GetPlayerHaveLock(
    const PlayerContext *locked_context,
    int which, const char *file, int location) const
{
    if (!locked_context || (which >= (int)player.size()))
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                QString("GetPlayerHaveLock(0x%1,%2,%3,%4) "
                        "returning NULL size(%5)")
                .arg((long long)locked_context, 0, 16)
                .arg(which).arg(file).arg(location).arg(player.size()));
        return NULL;
    }

    return (which < 0) ? player[playerActive] : player[which];
}

void TV::ReturnPlayerLock(PlayerContext *&ctx)
{
    playerLock.unlock();
    ctx = NULL;
}

void TV::ReturnPlayerLock(const PlayerContext *&ctx) const
{
    playerLock.unlock();
    ctx = NULL;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
