#include <cstdlib>
#include <cstring>
#include <cmath>
#include <unistd.h>
#include <pthread.h>

#include <qapplication.h>
#include <qregexp.h>
#include <qfile.h>
#include <qtimer.h>

#include "mythdbcon.h"
#include "tv.h"
#include "osd.h"
#include "osdsurface.h"
#include "osdtypes.h"
#include "osdlistbtntype.h"
#include "mythcontext.h"
#include "dialogbox.h"
#include "remoteencoder.h"
#include "remoteutil.h"
#include "guidegrid.h"
#include "NuppelVideoPlayer.h"
#include "programinfo.h"
#include "udpnotify.h"
#include "vsync.h"
#include "lcddevice.h"
#include "jobqueue.h"
#include "audiooutput.h"
#include "DisplayRes.h"
#include "signalmonitor.h"
#include "scheduledrecording.h"
#include "config.h"

#ifndef HAVE_ROUND
#define round(x) ((int) ((x) + 0.5))
#endif

const int TV::kInitFFRWSpeed  = 0;
const int TV::kMuteTimeout    = 800;   
const int TV::kLCDTimeout     = 30000;
const int TV::kBrowseTimeout  = 30000;
const int TV::kSMExitTimeout  = 2000;
const int TV::kChannelKeysMax = 6;

#define DEBUG_ACTIONS 0 /**< set to 1 to debug actions */

void TV::InitKeys(void)
{
    REG_KEY("TV Frontend", "PAGEUP", "Page Up", "3");
    REG_KEY("TV Frontend", "PAGEDOWN", "Page Down", "9");
    REG_KEY("TV Frontend", "DELETE", "Delete Program", "D");
    REG_KEY("TV Frontend", "PLAYBACK", "Play Program", "P");
    REG_KEY("TV Frontend", "TOGGLERECORD", "Toggle recording status of current "
            "program", "R");
    REG_KEY("TV Frontend", "DAYLEFT", "Page the program guide back one day", 
            "Home,7");
    REG_KEY("TV Frontend", "DAYRIGHT", "Page the program guide forward one day",
            "End,1");
    REG_KEY("TV Frontend", "PAGELEFT", "Page the program guide left",
            ",,<");
    REG_KEY("TV Frontend", "PAGERIGHT", "Page the program guide right",
            ">,.");
    REG_KEY("TV Frontend", "TOGGLEFAV", "Toggle the current channel as a "
            "favorite", "?");
    REG_KEY("TV Frontend", "NEXTFAV", "Toggle showing all channels or just "
            "favorites in the program guide.", "/");
    REG_KEY("TV Frontend", "CHANUPDATE", "Switch channels without exiting "
            "guide in Live TV mode.", "X");
    REG_KEY("TV Frontend", "RANKINC", "Increase program or channel rank",
            "Right");
    REG_KEY("TV Frontend", "RANKDEC", "Decrease program or channel rank",
            "Left");
    REG_KEY("TV Frontend", "UPCOMING", "List upcoming episodes", "O");
    REG_KEY("TV Frontend", "DETAILS", "Show program details", "U");

    REG_KEY("TV Playback", "CLEAROSD", "Clear OSD", "Backspace");
    REG_KEY("TV Playback", "PAUSE", "Pause", "P");
    REG_KEY("TV Playback", "DELETE", "Delete Program", "D");
    REG_KEY("TV Playback", "SEEKFFWD", "Fast Forward", "Right");
    REG_KEY("TV Playback", "SEEKRWND", "Rewind", "Left");
    REG_KEY("TV Playback", "CHANNELUP", "Channel up", "Up");
    REG_KEY("TV Playback", "CHANNELDOWN", "Channel down", "Down");
    REG_KEY("TV Playback", "NEXTFAV", "Switch to the next favorite channel",
            "/");
    REG_KEY("TV Playback", "PREVCHAN", "Switch to the previous channel", "H");
    REG_KEY("TV Playback", "JUMPFFWD", "Jump ahead", "PgDown");
    REG_KEY("TV Playback", "JUMPRWND", "Jump back", "PgUp");
    REG_KEY("TV Playback", "JUMPBKMRK", "Jump to bookmark", "K");
    REG_KEY("TV Playback", "FFWDSTICKY", "Fast Forward (Sticky) or Forward one "
            "frame while paused", ">,.");
    REG_KEY("TV Playback", "RWNDSTICKY", "Rewind (Sticky) or Rewind one frame "
            "while paused", ",,<");
    REG_KEY("TV Playback", "TOGGLEINPUTS", "Toggle Inputs", "C");
    REG_KEY("TV Playback", "SWITCHCARDS", "Switch Capture Cards", "Y");
    REG_KEY("TV Playback", "SKIPCOMMERCIAL", "Skip Commercial", "Z,End");
    REG_KEY("TV Playback", "SKIPCOMMBACK", "Skip Commercial (Reverse)",
            "Q,Home");
    REG_KEY("TV Playback", "JUMPSTART", "Jump to the start of the recording.", "Ctrl+B");            
    REG_KEY("TV Playback", "TOGGLEBROWSE", "Toggle channel browse mode", "O");
    REG_KEY("TV Playback", "TOGGLERECORD", "Toggle recording status of current "
            "program", "R");
    REG_KEY("TV Playback", "TOGGLEFAV", "Toggle the current channel as a "
            "favorite", "?");
    REG_KEY("TV Playback", "VOLUMEDOWN", "Volume down", "[,{,F10");
    REG_KEY("TV Playback", "VOLUMEUP", "Volume up", "],},F11");
    REG_KEY("TV Playback", "MUTE", "Mute", "|,\\,F9");
    REG_KEY("TV Playback", "TOGGLEPIPMODE", "Toggle Picture-in-Picture mode",
            "V");
    REG_KEY("TV Playback", "TOGGLEPIPWINDOW", "Toggle active PiP window", "B");
    REG_KEY("TV Playback", "SWAPPIP", "Swap the PiP window channels", "N");
    REG_KEY("TV Playback", "TOGGLEASPECT", "Toggle the display aspect ratio",
            "W");
    REG_KEY("TV Playback", "TOGGLECC", "Toggle Closed Captioning/Teletext",
            "T");
    REG_KEY("TV Playback", "DISPCC1", "Display CC1", "");
    REG_KEY("TV Playback", "DISPCC2", "Display CC2", "");
    REG_KEY("TV Playback", "DISPCC3", "Display CC3", "");
    REG_KEY("TV Playback", "DISPCC4", "Display CC4", "");
    REG_KEY("TV Playback", "DISPTXT1", "Display TXT1", "");
    REG_KEY("TV Playback", "DISPTXT2", "Display TXT2", "");
    REG_KEY("TV Playback", "DISPTXT3", "Display TXT3", "");
    REG_KEY("TV Playback", "DISPTXT4", "Display TXT4", "");
    REG_KEY("TV Playback", "QUEUETRANSCODE", "Queue the current recording for "
            "transcoding", "X");
    REG_KEY("TV Playback", "SPEEDINC", "Increase the playback speed", "U");
    REG_KEY("TV Playback", "SPEEDDEC", "Decrease the playback speed", "J");
    REG_KEY("TV Playback", "TOGGLESTRETCH", "Turn on time stretch control", "A");
    REG_KEY("TV Playback", "TOGGLEPICCONTROLS", "Turn on the playback picture "
            "adjustment controls", "F");
    REG_KEY("TV Playback", "TOGGLERECCONTROLS", "Turn on the recording picture "
            "adjustment controls", "G");
    REG_KEY("TV Playback", "TOGGLEEDIT", "Start Edit Mode", "E");
    REG_KEY("TV Playback", "GUIDE", "Show the Program Guide", "S");
    REG_KEY("TV Playback", "TOGGLESLEEP", "Toggle the Sleep Timer", "F8");
    REG_KEY("TV Playback", "PLAY", "Play", "Ctrl+P");
    REG_KEY("TV Playback", "NEXTAUDIO", "Switch to the next audio track", "+");
    REG_KEY("TV Playback", "PREVAUDIO", "Switch to the previous audio track", "-");
    REG_KEY("TV Playback", "NEXTSUBTITLE", "Switch to the next subtitle track", "");
    REG_KEY("TV Playback", "PREVSUBTITLE", "Switch to the previous subtitle track", "");
    REG_KEY("TV Playback", "SUBCHANNELSEP", "Separator for HDTV subchannels", "_");
    REG_KEY("TV Playback", "JUMPPREV", "Jump to previously played recording", "");
    REG_KEY("TV Playback", "SIGNALMON", "Monitor Signal Quality", "F7");
    
    REG_KEY("TV Editing", "CLEARMAP", "Clear editing cut points", "C,Q,Home");
    REG_KEY("TV Editing", "INVERTMAP", "Invert Begin/End cut points", "I");
    REG_KEY("TV Editing", "LOADCOMMSKIP", "Load cut list from commercial skips",
            "Z,End");
    REG_KEY("TV Editing", "NEXTCUT", "Jump to the next cut point", "PgDown");
    REG_KEY("TV Editing", "PREVCUT", "Jump to the previous cut point", "PgUp");
    REG_KEY("TV Editing", "BIGJUMPREW", "Jump back 10x the normal amount",
            ",,<");
    REG_KEY("TV Editing", "BIGJUMPFWD", "Jump forward 10x the normal amount",
            ">,.");
    REG_KEY("TV Editing", "TOGGLEEDIT", "Exit out of Edit Mode", "E");
}

void *SpawnDecode(void *param)
{
    NuppelVideoPlayer *nvp = (NuppelVideoPlayer *)param;
    nvp->StartPlaying();
    nvp->StopPlaying();
    return NULL;
}

/** \fn TV:TV()
 *  \brief Performs instance initialiation not requiring access to database.
 *  \sa Init()
 */
TV::TV(void)
    : QObject(NULL, "TV"),
      // Configuration variables from database
      baseFilters(""), fftime(0), rewtime(0),
      jumptime(0), usePicControls(false), smartChannelChange(false),
      MuteIndividualChannels(false), arrowAccel(false), osd_display_time(0),
      autoCommercialSkip(false), tryUnflaggedSkip(false),
      smartForward(false), stickykeys(0),
      ff_rew_repos(1.0f), ff_rew_reverse(false),
      showBufferedWarnings(false), bufferedChannelThreshold(0),
      vbimode(0),
      // State variables
      internalState(kState_None),
      menurunning(false), runMainLoop(false), wantsToQuit(true),
      exitPlayer(false), paused(false), errored(false),
      stretchAdjustment(false),
      audiosyncAdjustment(false), audiosyncBaseline(0),
      editmode(false), zoomMode(false), sigMonMode(false),
      update_osd_pos(false), endOfRecording(false), requestDelete(false),
      doSmartForward(false), switchingCards(false), lastRecorderNum(-1),
      queuedTranscode(false), getRecorderPlaybackInfo(false),
      picAdjustment(kPictureAttribute_None),
      recAdjustment(kPictureAttribute_None),
      // Sleep Timer
      sleep_index(0), sleepTimer(new QTimer(this)),
      // Key processing buffer, lock, and state
      keyRepeat(true), keyrepeatTimer(new QTimer(this)),
      // Fast forward state
      doing_ff_rew(0), ff_rew_index(0), speed_index(0),
      // Time Stretch state
      normal_speed(1.0f),
      // Estimated framerate from recorder
      frameRate(30.0f),
      // Channel changing state variables
      channelqueued(false), channelKeys(""), lookForChannel(false),
      lastCC(""), lastCCDir(0), muteTimer(new QTimer(this)),
      lockTimerOn(false), lockTimeout(3000),
      // previous channel functionality state variables
      prevChanKeyCnt(0), prevChanTimer(new QTimer(this)),
      // channel browsing state variables
      browsemode(false), persistentbrowsemode(false),
      browseTimer(new QTimer(this)),
      browsechannum(""), browsechanid(""), browsestarttime(""),
      // Program Info for currently playing video
      recorderPlaybackInfo(NULL),
      playbackinfo(NULL), inputFilename(""), playbackLen(0),
      // Video Players
      nvp(NULL), pipnvp(NULL), activenvp(NULL),
      // Remote Encoders
      recorder(NULL), piprecorder(NULL), activerecorder(NULL),
      // RingBuffers
      prbuffer(NULL), piprbuffer(NULL), activerbuffer(NULL),
      // OSD info
      dialogname(""), treeMenu(NULL), udpnotify(NULL), osdlock(true),
      // LCD Info
      lcdTitle(""), lcdSubtitle(""), lcdCallsign(""),
      // Window info (GUI is optional, transcoding, preview img, etc)
      myWindow(NULL), embedWinID(0), embedBounds(0,0,0,0)
{
    lastLcdUpdate = QDateTime::currentDateTime();
    lastLcdUpdate.addYears(-1); // make last LCD update last year..
    lastSignalMsgTime.start();
    lastSignalMsgTime.addMSecs(-2 * kSMExitTimeout);

    sleep_times.push_back(SleepTimerInfo(QObject::tr("Off"),       0));
    sleep_times.push_back(SleepTimerInfo(QObject::tr("30m"),   30*60));
    sleep_times.push_back(SleepTimerInfo(QObject::tr("1h"),    60*60));
    sleep_times.push_back(SleepTimerInfo(QObject::tr("1h30m"), 90*60));
    sleep_times.push_back(SleepTimerInfo(QObject::tr("2h"),   120*60));

    gContext->addListener(this);

    connect(prevChanTimer,    SIGNAL(timeout()), SLOT(SetPreviousChannel()));
    connect(browseTimer,      SIGNAL(timeout()), SLOT(BrowseEndTimer()));
    connect(muteTimer,        SIGNAL(timeout()), SLOT(UnMute()));
    connect(keyrepeatTimer,   SIGNAL(timeout()), SLOT(KeyRepeatOK()));
    connect(sleepTimer,       SIGNAL(timeout()), SLOT(SleepEndTimer()));
}

/** \fn TV::Init(bool)
 *  \brief Performs instance initialization, returns true on success.
 *
 *  \param createWindow If true a MythDialog is created for display.
 *  \return Returns true on success, false on failure.
 */
bool TV::Init(bool createWindow)
{
    MSqlQuery query(MSqlQuery::InitCon());
    if (!query.isConnected())
    {
        VERBOSE(VB_IMPORTANT, "TV::Init(): Error, could "
                "not open DB connection in player");
        errored = true;
        return false;
    }

    baseFilters         += gContext->GetSetting("CustomFilters");
    fftime               = gContext->GetNumSetting("FastForwardAmount", 30);
    rewtime              = gContext->GetNumSetting("RewindAmount", 5);
    jumptime             = gContext->GetNumSetting("JumpAmount", 10);
    usePicControls       = gContext->GetNumSetting("UseOutputPictureControls",0);
    smartChannelChange   = gContext->GetNumSetting("SmartChannelChange", 0);
    MuteIndividualChannels=gContext->GetNumSetting("IndividualMuteControl", 0);
    arrowAccel           = gContext->GetNumSetting("UseArrowAccels", 1);
    persistentbrowsemode = gContext->GetNumSetting("PersistentBrowseMode", 0);
    osd_display_time     = gContext->GetNumSetting("OSDDisplayTime");
    autoCommercialSkip   = gContext->GetNumSetting("AutoCommercialSkip", 0);
    tryUnflaggedSkip     = gContext->GetNumSetting("TryUnflaggedSkip", 0);
    smartForward         = gContext->GetNumSetting("SmartForward", 0);
    stickykeys           = gContext->GetNumSetting("StickyKeys");
    ff_rew_repos         = gContext->GetNumSetting("FFRewReposTime", 100)/100.0;
    ff_rew_reverse       = gContext->GetNumSetting("FFRewReverse", 1);
    int def[8] = { 3, 5, 10, 20, 30, 60, 120, 180 };
    for (uint i = 0; i < sizeof(def)/sizeof(def[0]); i++)
        ff_rew_speeds.push_back(
            gContext->GetNumSetting(QString("FFRewSpeed%1").arg(i), def[i]));

    showBufferedWarnings     = gContext->GetNumSetting("CCBufferWarnings", 0);
    bufferedChannelThreshold = gContext->GetNumSetting("CCWarnThresh", 10);
    QString vbiformat = gContext->GetSetting("VbiFormat");
    vbimode = (vbiformat.lower() == "pal teletext") ? 1 : 0;
    vbimode = (vbiformat.lower().left(4) == "ntsc") ? 2 : vbimode;

    if (createWindow)
    {
        MythMainWindow *mainWindow = gContext->GetMainWindow();
        bool fullscreen = !gContext->GetNumSetting("GuiSizeForTV", 0);
        bool switchMode = gContext->GetNumSetting("UseVideoModes", 0);

        saved_gui_bounds = QRect(mainWindow->geometry().topLeft(), mainWindow->size());

        // adjust for window manager wierdness.
        {
            int xbase, width, ybase, height;
            float wmult, hmult;
            gContext->GetScreenSettings(xbase, width, wmult,
                                        ybase, height, hmult);
            if (abs(saved_gui_bounds.x()-xbase < 3) &&
                abs(saved_gui_bounds.y()-ybase < 3))
            {
                saved_gui_bounds.setX(xbase);
                saved_gui_bounds.setY(ybase);
            }
        }

        // if width && height are zero users expect fullscreen playback
        if (!fullscreen)
        {
            int gui_width = 0, gui_height = 0;
            gContext->GetResolutionSetting("Gui", gui_width, gui_height);
            fullscreen |= (0 == gui_width && 0 == gui_height);
        }

        QRect player_bounds = saved_gui_bounds;
        if (fullscreen)
        {
            int xbase, width, ybase, height;
            gContext->GetScreenBounds(xbase, ybase, width, height);
            player_bounds = QRect(xbase, ybase, width, height);
        }

        // main window sizing
        DisplayRes *display_res = DisplayRes::GetDisplayRes();
        int maxWidth = 1920, maxHeight = 1440;
        if (switchMode && display_res)
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
                mainWindow->resize(player_bounds.size());
            }
        }

        // player window sizing
        myWindow = new MythDialog(mainWindow, "video playback window");

        myWindow->installEventFilter(this);
        myWindow->setNoErase();
        QRect win_bounds(0, 0, player_bounds.width(), player_bounds.height());
        myWindow->setGeometry(win_bounds);
        myWindow->setFixedSize(win_bounds.size());

        // resize main window
        mainWindow->setGeometry(player_bounds);
        mainWindow->setFixedSize(player_bounds.size());

        // finally we put the player window on screen...
        myWindow->show();
        myWindow->setBackgroundColor(Qt::black);
        qApp->processEvents();
    }

    pthread_create(&event, NULL, EventThread, this);

    while (!runMainLoop && !IsErrored())
        usleep(50);

    return !IsErrored();
}

TV::~TV(void)
{
    QMutexLocker locker(&osdlock); // prevent UpdateOSDSignal from continuing.
    gContext->removeListener(this);

    runMainLoop = false;
    pthread_join(event, NULL);

    if (prbuffer)
        delete prbuffer;
    if (nvp)
        delete nvp;
    if (myWindow)
    {
        delete myWindow;
        MythMainWindow* mwnd = gContext->GetMainWindow();
        mwnd->resize(saved_gui_bounds.size());
        mwnd->setFixedSize(saved_gui_bounds.size());
        mwnd->show();
        mwnd->move(saved_gui_bounds.topLeft());
    }
    if (recorderPlaybackInfo)
        delete recorderPlaybackInfo;

    if (treeMenu)
        delete treeMenu;

    if (class LCD * lcd = LCD::Get())
        lcd->switchToTime();
}

TVState TV::GetState(void)
{
    if (InStateChange())
        return kState_ChangingState;
    return internalState;
}

int TV::LiveTV(bool showDialogs)
{
    if (internalState == kState_None && RequestNextRecorder(showDialogs))
    {
        ChangeState(kState_WatchingLiveTV);
        switchingCards = false;
        return 1;
    }
    return 0;
}

void TV::DeleteRecorder()
{
    RemoteEncoder *rec = recorder;
    activerecorder = recorder = NULL;
    if (rec)
        delete rec;
}

bool TV::RequestNextRecorder(bool showDialogs)
{
    DeleteRecorder();

    RemoteEncoder *testrec = NULL;
    if (lookForChannel)
    {
        QStringList reclist;
        GetValidRecorderList(channelid, reclist);
        testrec = RemoteRequestFreeRecorderFromList(reclist);
        lookForChannel = false;
        channelqueued = true;
        channelid = "";
    }
    else
    {
        // The default behavior for starting live tv is to get the next
        // free recorder.  This is also how switching to the next recorder
        // works
        testrec = RemoteRequestNextFreeRecorder(lastRecorderNum);
    }

    if (!testrec)
        return false;
    
    if (!testrec->IsValidRecorder())
    {
        if (showDialogs)
        {
            QString title = tr("MythTV is already using all available "
                               "inputs for recording.  If you want to "
                               "watch an in-progress recording, select one "
                               "from the playback menu.  If you want to "
                               "watch live TV, cancel one of the "
                               "in-progress recordings from the delete "
                               "menu.");
            
            DialogBox diag(gContext->GetMainWindow(), title);
            diag.AddButton(tr("Cancel and go back to the TV menu"));
            diag.exec();
        }        
        
        delete testrec;
        
        return false;
    }
    
    activerecorder = recorder = testrec;
    lastRecorderNum = recorder->GetRecorderNumber();
    return true;
}

void TV::FinishRecording(void)
{
    if (!IsRecording())
        return;

    activerecorder->FinishRecording();
}

void TV::AskAllowRecording(const QStringList &messages, int timeuntil)
{
    if (!StateIsLiveTV(GetState()))
       return;

    QString title = messages[0];
    QString chanstr = messages[1];
    QString chansign = messages[2];
    QString channame = messages[3];

    QString channel = gContext->GetSetting("ChannelFormat", "<num> <sign>");
    channel.replace("<num>", chanstr)
        .replace("<sign>", chansign)
        .replace("<name>", channame);
    
    QString message = QObject::tr(
        "MythTV wants to record \"%1\" on %2 in %3 seconds. Do you want to:")
        .arg(title).arg(channel).arg(" %d ");
    
    while (!GetOSD())
    {
        qApp->unlock();
        qApp->processEvents();
        usleep(1000);
        qApp->lock();
    }

    QStringList options;
    options += tr("Record and watch while it records");
    options += tr("Let it record and go back to the Main Menu");
    options += tr("Don't let it record, I want to watch TV");

    dialogname = "allowrecordingbox";
    GetOSD()->NewDialogBox(dialogname, message, options, timeuntil); 
}

void TV::setLastProgram(ProgramInfo *rcinfo)
{
    lastProgram = rcinfo;
}

int TV::Playback(ProgramInfo *rcinfo)
{
    jumpToProgram = false;

    if (internalState != kState_None)
        return 0;

    inputFilename = rcinfo->pathname;

    playbackLen = rcinfo->CalculateLength();
    playbackinfo = rcinfo;

    int overrecordseconds = gContext->GetNumSetting("RecordOverTime");
    QDateTime curtime = QDateTime::currentDateTime();
    QDateTime recendts = rcinfo->recendts.addSecs(overrecordseconds);

    if (curtime < recendts && !rcinfo->isVideo)
        ChangeState(kState_WatchingRecording);
    else
        ChangeState(kState_WatchingPreRecorded);

    normal_speed = playbackinfo->timestretch;

    if (class LCD * lcd = LCD::Get())
        lcd->switchToChannel(rcinfo->chansign, rcinfo->title, rcinfo->subtitle);

    return 1;
}

int TV::PlayFromRecorder(int recordernum)
{
    int retval = 0;

    if (recorder)
    {
        VERBOSE(VB_IMPORTANT,
                QString("TV::PlayFromRecorder(%1): Recorder already exists!")
                .arg(recordernum));
        return -1;
    }

    recorder = RemoteGetExistingRecorder(recordernum);
    if (!recorder)
        return -1;

    if (recorder->IsValidRecorder())
    {
        // let the mainloop get the programinfo from encoder,
        // connecting to encoder won't work from here
        getRecorderPlaybackInfo = true;
        while (getRecorderPlaybackInfo)
        {
            qApp->unlock();
            qApp->processEvents();
            usleep(1000);
            qApp->lock();
        }
    }

    DeleteRecorder();

    if (recorderPlaybackInfo)
    {
        bool fileexists = false;
        if (recorderPlaybackInfo->pathname.left(7) == "myth://")
            fileexists = RemoteCheckFile(recorderPlaybackInfo);
        else
        {
            QFile checkFile(recorderPlaybackInfo->pathname);
            fileexists = checkFile.exists();
        }

        if (fileexists)
        {
            Playback(recorderPlaybackInfo);
            retval = 1;
        }
    }

    return retval;
}

bool TV::StateIsRecording(TVState state)
{
    return (state == kState_RecordingOnly || 
            state == kState_WatchingRecording);
}

bool TV::StateIsPlaying(TVState state)
{
    return (state == kState_WatchingPreRecorded || 
            state == kState_WatchingRecording);
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

TVState TV::RemovePlaying(TVState state)
{
    if (StateIsPlaying(state))
        return kState_None;
    return kState_Error;
}

#define TRANSITION(ASTATE,BSTATE) \
   ((internalState == ASTATE) && (desiredNextState == BSTATE))

#define SET_NEXT() do { nextState = desiredNextState; changed = true; } while(0);
#define SET_LAST() do { nextState = internalState; changed = true; } while(0);

/** \fn TV::HandleStateChange(void)
 *  \brief Changes the state to the state on the front of the 
 *         state change queue.
 *
 *   Note: There must exist a state transition from any state we can enter
 *   to  the kState_None state, as this is used to shutdown TV in RunTV.
 *
 */
void TV::HandleStateChange(void)
{
    if (IsErrored())
    {
        VERBOSE(VB_IMPORTANT, "TV::HandleStateChange() Error, "
                "called after fatal error detected.");
        return;
    }

    bool changed = false;

    stateLock.lock();
    TVState nextState = internalState;
    if (!nextStates.size())
    {
        VERBOSE(VB_IMPORTANT, "TV::HandleStateChange() Warning, "
                "called with no state to change to.");
        stateLock.unlock();
        return;
    }
    TVState desiredNextState = nextStates.dequeue();    
    VERBOSE(VB_GENERAL, QString("Attempting to change from %1 to %2")
            .arg(StateToString(nextState))
            .arg(StateToString(desiredNextState)));

    if (desiredNextState == kState_Error)
    {
        VERBOSE(VB_IMPORTANT, "TV::HandleStateChange() Error, "
                "attempting to set to an error state!");
        errored = true;
        stateLock.unlock();
        return;
    }

    if (TRANSITION(kState_None, kState_WatchingLiveTV))
    {
        long long filesize = 0;
        long long smudge = 0;
        QString name = "";

        lastSignalUIInfo.clear();
        recorder->Setup();
        if (!recorder->SetupRingBuffer(name, filesize, smudge))
        {
            VERBOSE(VB_IMPORTANT, "TV::HandleStateChange() Error, "
                    "failed to start RingBuffer on backend. Aborting.");
            DeleteRecorder();

            SET_LAST();
        }
        else
        {
            lockTimerOn = false;
            prbuffer = new RingBuffer(name, filesize, smudge, recorder);

            SET_NEXT();

            recorder->SpawnLiveTV();

            gContext->DisableScreensaver();

            bool ok = false;
            if (StartRecorder())
            {
                if (StartPlayer(false))
                    ok = true;
                else
                    StopStuff(true, true, true);
            }
            if (!ok)
            {
                VERBOSE(VB_IMPORTANT, "LiveTV not successfully started");
                gContext->RestoreScreensaver();
                DeleteRecorder();

                SET_LAST();
            }
            else
            {
                lockTimer.start();
                lockTimerOn = true;
            }
        }
    }
    else if (TRANSITION(kState_WatchingLiveTV, kState_None))
    {
        SET_NEXT();

        StopStuff(true, true, true);
        gContext->RestoreScreensaver();
    }
    else if (TRANSITION(kState_WatchingRecording, kState_WatchingPreRecorded))
    {
        SET_NEXT();
    }
    else if (TRANSITION(kState_None, kState_WatchingPreRecorded) ||
             TRANSITION(kState_None, kState_WatchingRecording))
    {
        QString tmpFilename;
        if ((inputFilename.left(7) == "myth://") &&
            (inputFilename.length() > 7))
        {
            tmpFilename = gContext->GetSettingOnHost("RecordFilePrefix",
                                                     playbackinfo->hostname);

            int pathLen = inputFilename.find(QRegExp("/"), 7);
            if (pathLen != -1)
            {
                tmpFilename += inputFilename.right(inputFilename.length() -
                                                   pathLen);

                QFile checkFile(tmpFilename);

                if (!checkFile.exists())
                    tmpFilename = inputFilename;
            }
        }
        else
        {
            tmpFilename = inputFilename;
        }

        prbuffer = new RingBuffer(tmpFilename, false);
        if (prbuffer->IsOpen())
        {

            gContext->DisableScreensaver();
    
            if (desiredNextState == kState_WatchingRecording)
            {
                recorder = RemoteGetExistingRecorder(playbackinfo);
                if (!recorder || !recorder->IsValidRecorder())
                {
                    VERBOSE(VB_IMPORTANT, "ERROR: couldn't find "
                            "recorder for in-progress recording");
                    desiredNextState = kState_WatchingPreRecorded;
                    DeleteRecorder();
                }
                else
                {
                    activerecorder = recorder;
                    recorder->Setup();
                }
            }

            StartPlayer(desiredNextState == kState_WatchingRecording);
            
            
            SET_NEXT();
            if (!playbackinfo->isVideo)
            {
                QString message = "COMMFLAG_REQUEST ";
                message += playbackinfo->chanid + " " +
                           playbackinfo->startts.toString(Qt::ISODate);
                RemoteSendMessage(message);
            }                
        }
        else
        {
            SET_LAST();
        }
    }
    else if (TRANSITION(kState_WatchingPreRecorded, kState_None) ||
             TRANSITION(kState_WatchingRecording, kState_None))
    {
        if (internalState == kState_WatchingRecording)
            recorder->StopPlaying();

        SET_NEXT();

        StopStuff(true, true, false);
        gContext->RestoreScreensaver();
    }
    else if (TRANSITION(kState_None, kState_None))
    {
        SET_NEXT();
    }

    // Check that new state is valid
    if (kState_None != nextState &&
        activenvp && !activenvp->IsDecoderThreadAlive())
    {
        VERBOSE(VB_IMPORTANT, "Error, decoder not alive, and trying to play..");
        if (nextState == kState_WatchingLiveTV)
        {
            StopStuff(true, true, true);
        }

        nextState = kState_None;
        changed = true;
    }

    // Print state changed message...
    if (!changed)
    {
        VERBOSE(VB_IMPORTANT, QString("Unknown state transition: %1 to %2")
                .arg(StateToString(internalState))
                .arg(StateToString(desiredNextState)));
    }
    else if (internalState != nextState)
    {
        VERBOSE(VB_GENERAL, QString("Changing from %1 to %2")
                .arg(StateToString(internalState))
                .arg(StateToString(nextState)));
    }

    // update internal state variable
    internalState = nextState;
    stateLock.unlock();

    if (StateIsLiveTV(internalState))
    {
        UpdateOSDInput();
        UpdateLCD();
    }
    if (recorder)
        recorder->FrontendReady();
}
#undef TRANSITION
#undef SET_NEXT
#undef SET_LAST

/** \fn TV::InStateChange() const
 *  \brief Returns true if there is a state change queued up.
 */
bool TV::InStateChange() const
{
    if (stateLock.locked())
        return true;
    stateLock.lock();
    bool inStateChange = nextStates.size() > 0;
    stateLock.unlock();
    return inStateChange;
}

/** \fn TV::ChangeState(TVState nextState)
 *  \brief Puts a state change on the nextState queue.
 */
void TV::ChangeState(TVState nextState)
{
    stateLock.lock();
    nextStates.enqueue(nextState);
    stateLock.unlock();
}

/** \fn TV::ForceNextStateNone()
 *  \brief Removes any pending state changes, and puts kState_None on the queue.
 */
void TV::ForceNextStateNone()
{
    stateLock.lock();
    nextStates.clear();
    nextStates.push_back(kState_None);
    stateLock.unlock();
}


/** \fn TV::StartRecorder(int)
 *  \brief Starts recorder, must be called before StartPlayer().
 *  \param maxWait How long to wait for RecorderBase to start recording.
 *  \return true when successful, false otherwise.
 */
bool TV::StartRecorder(int maxWait)
{
    maxWait = (maxWait <= 0) ? 20000 : maxWait;
    MythTimer t;
    t.start();
    while (!recorder->IsRecording() && !exitPlayer && t.elapsed() < maxWait)
        usleep(50);
    if (!recorder->IsRecording() || exitPlayer)
    {
        if (!exitPlayer)
            VERBOSE(VB_IMPORTANT, "StartRecorder() -- "
                    "timed out waiting for recorder to start");
        return false;
    }

    VERBOSE(VB_PLAYBACK, "TV::StartRecorder: took "<<t.elapsed()
            <<" ms to start recorder.");

    // Get timeout for this recorder
    lockTimeout = 0xffffffff;
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT channel_timeout, cardtype "
                  "FROM capturecard "
                  "WHERE cardid = :CARDID");
    query.bindValue(":CARDID", recorder->GetRecorderNumber());
    if (!query.exec() || !query.isActive())
        MythContext::DBError("Getting timeout", query);
    else if (query.next() &&
             SignalMonitor::IsSupported(query.value(1).toString()))
        lockTimeout = max(query.value(0).toInt(), 500);

    VERBOSE(VB_PLAYBACK, "TV::StartRecorder: " +
            QString("Set lock timeout to %1 ms for rec #%2")
            .arg(lockTimeout).arg(recorder->GetRecorderNumber()));

    // Cache starting frame rate for this recorder
    frameRate = recorder->GetFrameRate();
    return true;
}

/** \fn TV::StartPlayer(bool, int)
 *  \brief Starts player, must be called after StartRecorder().
 *  \param maxWait How long to wait for NuppelVideoPlayer to start playing.
 *  \return true when successful, false otherwise.
 */
bool TV::StartPlayer(bool isWatchingRecording, int maxWait)
{ 
    SetupPlayer(isWatchingRecording);
    pthread_create(&decode, NULL, SpawnDecode, nvp);

    maxWait = (maxWait <= 0) ? 20000 : maxWait;
#ifdef USING_VALGRIND
    maxWait = (1<<30);
#endif // USING_VALGRIND
    MythTimer t;
    t.start();
    while (!nvp->IsPlaying() && nvp->IsDecoderThreadAlive() &&
           (t.elapsed() < maxWait))
        usleep(50);

    VERBOSE(VB_PLAYBACK, "TV::StartPlayer: took "<<t.elapsed()
            <<" ms to start player.");

    if (nvp->IsPlaying())
    {
        activenvp = nvp;
        activerbuffer = prbuffer;
        activerecorder = recorder;
        StartOSD();
        return true;
    }
    VERBOSE(VB_IMPORTANT,
            QString("TV::StartPlayer: Error, NVP is not playing after %1 msec")
            .arg(maxWait));
    return false;
}

/** \fn TV::StartOSD()
 *  \brief Initializes the on screen display.
 *
 *   If the NuppelVideoPlayer already exists we grab it's OSD via
 *   NuppelVideoPlayer::GetOSD().
 */
void TV::StartOSD()
{
    if (nvp)
    {
        frameRate = nvp->GetFrameRate();
        if (nvp->GetOSD())
            GetOSD()->SetUpOSDClosedHandler(this);
    }
}

/** \fn TV::StopStuff(bool, bool, bool)
 *  \brief Can shut down the ringbuffers, the players, and in LiveTV it can 
 *         shut down the recorders.
 *
 *   The player needs to be partially shutdown before the recorder,
 *   and partially shutdown after the recorder. Hence these are shutdown
 *   from within the same method. Also, shutting down things in the right
 *   order avoids spewing error messages...
 *
 *  \param stopRingBuffers Set to true if ringbuffer must be shut down.
 *  \param stopPlayers     Set to true if player must be shut down.
 *  \param stopRecorders   Set to true if recorder must be shut down.
 */
void TV::StopStuff(bool stopRingBuffers, bool stopPlayers, bool stopRecorders)
{
    VERBOSE(VB_PLAYBACK, "TV::StopStuff() -- begin");
    if (stopRingBuffers)
    {
        VERBOSE(VB_PLAYBACK, "TV::StopStuff(): stopping ring buffer[s]");
        if (prbuffer)
        {
            prbuffer->StopReads();
            prbuffer->Pause();
            prbuffer->WaitForPause();
        }

        if (piprbuffer)
        {
            piprbuffer->StopReads();
            piprbuffer->Pause();
            piprbuffer->WaitForPause();
        }
    }

    if (stopPlayers)
    {
        VERBOSE(VB_PLAYBACK, "TV::StopStuff(): stopping player[s] (1/2)");
        if (nvp)
            nvp->StopPlaying();

        if (pipnvp)
            pipnvp->StopPlaying();
    }

    if (stopRecorders)
    {
        VERBOSE(VB_PLAYBACK, "TV::StopStuff(): stopping recorder[s]");
        if (recorder)
            recorder->StopLiveTV();

        if (piprecorder)
            piprecorder->StopLiveTV();
    }

    if (stopPlayers)
    {
        VERBOSE(VB_PLAYBACK, "TV::StopStuff(): stopping player[s] (2/2)");
        if (nvp)
            TeardownPlayer();

        if (pipnvp)
            TeardownPipPlayer();
    }
    VERBOSE(VB_PLAYBACK, "TV::StopStuff() -- end");
}

void TV::SetupPlayer(bool isWatchingRecording)
{
    if (nvp)
    {
        VERBOSE(VB_IMPORTANT,
                "Attempting to setup a player, but it already exists.");
        return;
    }

    QString filters = "";
    
    
    nvp = new NuppelVideoPlayer(playbackinfo);
    nvp->SetParentWidget(myWindow);
    nvp->SetRingBuffer(prbuffer);
    nvp->SetRecorder(recorder);
    nvp->SetAudioSampleRate(gContext->GetNumSetting("AudioSampleRate"));
    nvp->SetAudioDevice(gContext->GetSetting("AudioOutputDevice"));
    nvp->SetLength(playbackLen);
    nvp->SetExactSeeks(gContext->GetNumSetting("ExactSeeking"));
    nvp->SetAutoCommercialSkip(autoCommercialSkip);

    nvp->SetAudioStretchFactor(normal_speed);

    if (gContext->GetNumSetting("DefaultCCMode"))
        nvp->ToggleCC(vbimode, 0);

    filters = GetFiltersForChannel();
    nvp->SetVideoFilters(filters);

    if (embedWinID > 0)
        nvp->EmbedInWidget(embedWinID, embedBounds.x(), embedBounds.y(),
                           embedBounds.width(), embedBounds.height());

    if (isWatchingRecording)
        nvp->SetWatchingRecording(true);

    int udp_port = gContext->GetNumSetting("UDPNotifyPort");
     if (udp_port > 0)
         udpnotify = new UDPNotify(this, udp_port);
     else
         udpnotify = NULL;
}


QString TV::GetFiltersForChannel()
{
    QString filters;
    QString chanFilters;
    
    QString chan_name;
    
    if (playbackinfo) // Recordings have this info already.
        chanFilters = playbackinfo->chanOutputFilters;
    else if (activerecorder)
    {
        // Live TV requires a lookup
        activerecorder->GetOutputFilters(chanFilters);
    }
    
    if ((chanFilters.length() > 1) && (chanFilters[0] != '+'))
    {
        filters = chanFilters;
    }
    else
    {
        filters = baseFilters;

        if ((filters.length() > 1) && (filters.right(1) != ","))
            filters += ",";

        filters += chanFilters.mid(1);
    }
    
    VERBOSE(VB_CHANNEL, QString("Output filters for this channel are: '%1'").arg(filters));
    return filters;
}

void TV::SetupPipPlayer(void)
{
    if (pipnvp)
    {
        VERBOSE(VB_IMPORTANT,
                "Attempting to setup a PiP player, but it already exists.");
        return;
    }

    pipnvp = new NuppelVideoPlayer();
    pipnvp->SetAsPIP();
    pipnvp->SetRingBuffer(piprbuffer);
    pipnvp->SetRecorder(piprecorder);
    pipnvp->SetAudioSampleRate(gContext->GetNumSetting("AudioSampleRate"));
    pipnvp->SetAudioDevice(gContext->GetSetting("AudioOutputDevice"));
    pipnvp->SetExactSeeks(gContext->GetNumSetting("ExactSeeking"));

    pipnvp->SetLength(playbackLen);
}

void TV::TeardownPlayer(void)
{
    if (nvp)
    {
        QMutexLocker locker(&osdlock); // prevent UpdateOSDSignal using osd...
        // Stop the player's video sync method.  Do so from this
        // main thread to work around a potential OpenGL bug.
        VideoSync *vs = nvp->getVideoSync();
        if (vs != NULL)
            vs->Stop();
        pthread_join(decode, NULL);
        delete nvp;
        nvp = NULL;
    }

    if (udpnotify)
        delete udpnotify;

    paused = false;
    doing_ff_rew = 0;
    ff_rew_index = kInitFFRWSpeed;
    speed_index = 0;
    sleep_index = 0;
    normal_speed = 1.0f;

    nvp = activenvp = NULL;

    playbackinfo = NULL;
 
    DeleteRecorder();

    if (prbuffer)
    {
        delete prbuffer;
        prbuffer = activerbuffer = NULL;
    }

    if (piprbuffer)
    {
        delete piprbuffer;
        piprbuffer = NULL;
    }
}

void TV::TeardownPipPlayer(void)
{
    if (pipnvp)
    {
        pthread_join(pipdecode, NULL);
        delete pipnvp;
    }
    pipnvp = NULL;

    if (piprecorder)
        delete piprecorder;
    piprecorder = NULL;

    delete piprbuffer;
    piprbuffer = NULL;
}

void *TV::EventThread(void *param)
{
    TV *thetv = (TV *)param;
    thetv->RunTV();

    return NULL;
}

void TV::RunTV(void)
{ 
    paused = false;
    QKeyEvent *keypressed;

    if (gContext->GetNumSetting("WatchTVGuide", 0))
    {
        MSqlQuery query(MSqlQuery::InitCon()); 
        query.prepare("SELECT keylist FROM keybindings WHERE "
                      "context = 'TV Playback' AND action = 'GUIDE' AND "
                      "hostname = :HOSTNAME ;");
        query.bindValue(":HOSTNAME", gContext->GetHostName());

        if (query.exec() && query.isActive() && query.size() > 0)
        {
            query.next();

            QKeySequence keyseq(query.value(0).toString());

            int keynum = keyseq[0];
            keynum &= ~Qt::UNICODE_ACCEL;
   
            keyList.prepend(new QKeyEvent(QEvent::KeyPress, keynum, 0, 0));
        }
    }

    doing_ff_rew = 0;
    ff_rew_index = kInitFFRWSpeed;
    speed_index = 0;
    normal_speed = 1.0f;
    sleep_index = 0;

    int updatecheck = 0;
    update_osd_pos = false;

    lastLcdUpdate = QDateTime::currentDateTime();
    UpdateLCD();
    
    ChannelClear();

    switchingCards = false;
    runMainLoop = true;
    exitPlayer = false;

    while (runMainLoop)
    {
        stateLock.lock();
        bool doHandle = nextStates.size() > 0;
        stateLock.unlock();
        if (doHandle)
            HandleStateChange();

        if (osdlock.tryLock())
        {
            if (lastSignalMsg.size())
            {   // set last signal msg, so we get some feedback...
                UpdateOSDSignal(lastSignalMsg);
                lastSignalMsg.clear();
            }
            UpdateOSDTimeoutMessage();
            osdlock.unlock();
        }

        usleep(1000);

        if (getRecorderPlaybackInfo)
        {
            if (recorderPlaybackInfo)
            {
                delete recorderPlaybackInfo;
                recorderPlaybackInfo = NULL;
            }

            recorder->Setup();

            if (recorder->IsRecording())
            {
                recorderPlaybackInfo = recorder->GetRecording();
                RemoteFillProginfo(recorderPlaybackInfo, 
                                   gContext->GetHostName());
            }

            getRecorderPlaybackInfo = false;
        }

        if (nvp && (keyList.count() > 0))
        { 
            keyListLock.lock();
            keypressed = keyList.first();
            keyList.removeFirst();
            keyListLock.unlock();

            ProcessKeypress(keypressed);
            delete keypressed;
        }

        if ((recorder && recorder->GetErrorStatus()) || IsErrored())
        {
            exitPlayer = true;
            wantsToQuit = false;
        }

        if (StateIsPlaying(internalState))
        {
#ifdef USING_VALGRIND
            while (!nvp->IsPlaying())
            {
                VERBOSE(VB_IMPORTANT, "Waiting for Valgrind...");
                sleep(1);
            }
#endif // USING_VALGRIND

            if (!nvp->IsPlaying())
            {
                ChangeState(RemovePlaying(internalState));
                endOfRecording = true;
                VERBOSE(VB_PLAYBACK, ">> Player timeout");
            }
        }

        if (exitPlayer)
        {
            while (GetOSD() && GetOSD()->DialogShowing(dialogname))
            {
                GetOSD()->DialogAbort(dialogname);
                GetOSD()->TurnDialogOff(dialogname);
                usleep(1000);
            }
            ChangeState(kState_None);
            exitPlayer = false;
        }

        if ((doing_ff_rew || speed_index) && 
            activenvp && activenvp->AtNormalSpeed())
        {
            speed_index = 0;
            doing_ff_rew = 0;
            ff_rew_index = kInitFFRWSpeed;
            UpdatePosOSD(0.0, PlayMesg());
        }

        if (activenvp && (activenvp->GetNextPlaySpeed() != normal_speed) &&
            activenvp->AtNormalSpeed() && !activenvp->PlayingSlowForPrebuffer())
        {
            normal_speed = 1.0;     // got changed in nvp due to close to end of file
            UpdatePosOSD(0.0, PlayMesg());
        }

        if (++updatecheck >= 20)
        {
            OSDSet *oset;
            if (GetOSD() && (oset = GetOSD()->GetSet("status")) &&
                oset->Displaying() && update_osd_pos &&
                (StateIsLiveTV(internalState) ||
                 StateIsPlaying(internalState)))
            {
                QString desc = "";
                int pos = nvp->calcSliderPos(desc);
                GetOSD()->UpdatePause(pos, desc);
            }

            updatecheck = 0;
        }

        if (channelqueued && GetOSD())
        {
            OSDSet *set = GetOSD()->GetSet("channel_number");
            if ((set && !set->Displaying()) || !set)
            {
                if (StateIsLiveTV(GetState()))
                    ChannelCommit();
                else
                    ChannelClear();
            }
        }

        if (class LCD * lcd = LCD::Get())
        {
            QDateTime curTime = QDateTime::currentDateTime();
            if (lastLcdUpdate.secsTo(curTime) < 60)
                continue;

            float progress = 0.0;

            if (StateIsLiveTV(GetState()))
                ShowLCDChannelInfo();

            if (activenvp)
            {
                QString dummy;
                int pos = activenvp->calcSliderPos(dummy);
                progress = (float)pos / 1000;
            }
            lcd->setChannelProgress(progress);

            lastLcdUpdate = QDateTime::currentDateTime();
        }
    }
  
    if (!IsErrored() && (GetState() != kState_None))
    {
        ForceNextStateNone();
        HandleStateChange();
    }
}

bool TV::eventFilter(QObject *o, QEvent *e)
{
    (void)o;

    switch (e->type())
    {
        case QEvent::KeyPress:
        {
            QKeyEvent *k = new QKeyEvent(*(QKeyEvent *)e);
  
            // can't process these events in the Qt event loop. 
            keyListLock.lock();
            keyList.append(k);
            keyListLock.unlock();

            return true;
        }
        case QEvent::Paint:
        {
            if (nvp)
                nvp->ExposeEvent();
            return true;
        }
        case MythEvent::MythEventMessage:
        {
            customEvent((QCustomEvent *)e);
            return true;
        }
        default:
            return false;
    }
}

void TV::ProcessKeypress(QKeyEvent *e)
{
#if DEBUG_ACTIONS
    VERBOSE(VB_IMPORTANT, "TV::ProcessKeypress()");
#endif // DEBUG_ACTIONS

    bool was_doing_ff_rew = false;
    bool redisplayBrowseInfo = false;

    if (editmode)
    {   
        if (!nvp->DoKeypress(e))
            editmode = nvp->GetEditMode();
        if (!editmode)
        {
            paused = !paused;
            DoPause();
        }
        return;
    }

    if (GetOSD() && GetOSD()->IsRunningTreeMenu())
    {
        GetOSD()->TreeMenuHandleKeypress(e);
        return;
    }

    QStringList actions;
    QString action;
    if (!gContext->GetMainWindow()->TranslateKeyPress("TV Playback", e, 
                                                      actions))
        return;

    bool handled = false;

    if (browsemode)
    {
        int passThru = 0;

        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            action = actions[i];
            handled = true;

            if (action == "UP" || action == "CHANNELUP")
                BrowseDispInfo(BROWSE_UP);
            else if (action == "DOWN" || action == "CHANNELDOWN")
                BrowseDispInfo(BROWSE_DOWN);
            else if (action == "LEFT")
                BrowseDispInfo(BROWSE_LEFT);
            else if (action == "RIGHT")
                BrowseDispInfo(BROWSE_RIGHT);
            else if (action == "NEXTFAV")
                BrowseDispInfo(BROWSE_FAVORITE);
            else if (action == "0" || action == "1" || action == "2" ||
                     action == "3" || action == "4" || action == "5" ||
                     action == "6" || action == "7" || action == "8" ||
                     action == "9")
            {
                ChannelKey('0' + action.toInt());
            }
            else if (action == "SUBCHANNELSEP")
            {
                ChannelKey('_');
            }
            else if (action == "TOGGLEBROWSE" || action == "ESCAPE" ||
                     action == "CLEAROSD")
            {
                ChannelCommit(); 
                BrowseEnd(false);
            }
            else if (action == "SELECT")
            {
                ChannelCommit(); 
                BrowseEnd(true);
            }
            else if (action == "TOGGLERECORD")
                ToggleRecord();
            else if (action == "VOLUMEDOWN" || action == "VOLUMEUP" ||
                     action == "STRETCHINC" || action == "STRETCHDEC" ||
                     action == "MUTE" || action == "TOGGLEASPECT")
            {
                passThru = 1;
                handled = false;
            }
            else if (action == "TOGGLEPIPWINDOW" || action == "TOGGLEPIPMODE" ||
                     action == "SWAPPIP")
            {
                passThru = 1;
                handled = false;
                redisplayBrowseInfo = true;
            }
            else
                handled = false;
        }

        if (!passThru)
            return;
    }

    if (zoomMode)
    {
        int passThru = 0;

        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            action = actions[i];
            handled = true;

            if (action == "UP" || action == "CHANNELUP")
                nvp->Zoom(kZoomUp);
            else if (action == "DOWN" || action == "CHANNELDOWN")
                nvp->Zoom(kZoomDown);
            else if (action == "LEFT")
                nvp->Zoom(kZoomLeft);
            else if (action == "RIGHT")
                nvp->Zoom(kZoomRight);
            else if (action == "ESCAPE")
            {
                nvp->Zoom(kZoomHome);
                SetManualZoom(false);
            }
            else if (action == "SELECT")
                SetManualZoom(false);
            else if (action == "JUMPFFWD")
                nvp->Zoom(kZoomIn);
            else if (action == "JUMPRWND")
                nvp->Zoom(kZoomOut);
            else if (action == "VOLUMEDOWN" || action == "VOLUMEUP" ||
                     action == "STRETCHINC" || action == "STRETCHDEC" ||
                     action == "MUTE" || action == "PAUSE" ||
                     action == "CLEAROSD")
            {
                passThru = 1;
                handled = false;
            }
            else
                handled = false;
        }

        if (!passThru)
            return;
    }

    if (dialogname != "" && GetOSD() && GetOSD()->DialogShowing(dialogname))
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            action = actions[i];
            handled = true;

            if (action == "UP")
                GetOSD()->DialogUp(dialogname); 
            else if (action == "DOWN")
                GetOSD()->DialogDown(dialogname);
            else if (action == "SELECT" || action == "ESCAPE" || 
                     action == "CLEAROSD" ||
                     ((arrowAccel) && (action == "LEFT" || action == "RIGHT")))
            {
                if (action == "ESCAPE" || action == "CLEAROSD" ||
                    (arrowAccel && action == "LEFT"))
                    GetOSD()->DialogAbort(dialogname);
                GetOSD()->TurnDialogOff(dialogname);
                if (dialogname == "alreadybeingedited")
                {
                    int result = GetOSD()->GetDialogResponse(dialogname);
                    if (result == 1) 
                    {
                       playbackinfo->SetEditing(false);
                       editmode = nvp->EnableEdit();
                    }
                    else
                    {
                        paused = !paused;
                        DoPause();
                    }
                }
                else if (dialogname == "exitplayoptions") 
                {
                    int result = GetOSD()->GetDialogResponse(dialogname);

                    switch (result)
                    {
                        case 1:
                            nvp->SetBookmark();
                            wantsToQuit = true;
                            exitPlayer = true;
                            break;
                        case 3: case 0:
                            paused = !paused;
                            DoPause();
                            break;
                        case 4:
                            wantsToQuit = true;
                            exitPlayer = true;
                            requestDelete = true;
                            break;
                        default:
                            wantsToQuit = true;
                            exitPlayer = true;
                            break;
                    }
                }
                else if (dialogname == "videoexitplayoptions") 
                {
                    int result = GetOSD()->GetDialogResponse(dialogname);

                    switch (result)
                    {
                        case 0: case 2:
                            paused = !paused;
                            DoPause();
                            break;
                        case 1:
                            wantsToQuit = true;
                            exitPlayer = true;
                            requestDelete = true;
                            break;
                        default:
                            wantsToQuit = true;
                            exitPlayer = true;
                            break;
                    }
                }
                else if (dialogname == "ccwarningdirection")
                {
                    if (GetOSD()->GetDialogResponse(dialogname) == 1)
                        ChangeChannel(lastCCDir, true);
                    else if (!paused)
                        activenvp->Play(normal_speed, true);
                }     
                else if (dialogname == "ccwarningstring")
                {
                    if (GetOSD()->GetDialogResponse(dialogname) == 1)
                        ChangeChannelByString(lastCC, true);
                    else if (!paused)
                        activenvp->Play(normal_speed, true);
                }
                else if (dialogname == "allowrecordingbox")
                {
                    int result = GetOSD()->GetDialogResponse(dialogname);
    
                    if (result == 2)
                        StopLiveTV();
                    else if (result == 3)
                        recorder->CancelNextRecording();
                }
                else if (dialogname == "channel_timed_out")
                {
                    lockTimerOn = false;
                }

                while (GetOSD()->DialogShowing(dialogname))
                {
                    usleep(1000);
                }

                dialogname = "";
            }
            else
                handled = false;
        }
        return;
    }

    if (picAdjustment != kPictureAttribute_None)
    {
        for (unsigned int i = 0; i < actions.size(); i++)
        {
            action = actions[i];
            handled = true;

            if (action == "LEFT")
                DoChangePictureAttribute(picAdjustment, false, false);
            else if (action == "RIGHT")
                DoChangePictureAttribute(picAdjustment, true, false);
            else
                handled = false;
        }
    }
    
    if (recAdjustment)
    {
        for (unsigned int i = 0; i < actions.size(); i++)
        {
            action = actions[i];
            handled = true;

            if (action == "LEFT")
                DoChangePictureAttribute(recAdjustment, false, true);
            else if (action == "RIGHT")
                DoChangePictureAttribute(recAdjustment, true, true);
            else
                handled = false;
        }
    }
   
    if (stretchAdjustment)
    {
        for (unsigned int i = 0; i < actions.size(); i++)
        {
            action = actions[i];
            handled = true;

            if (action == "LEFT")
                ChangeTimeStretch(-1);
            else if (action == "RIGHT")
                ChangeTimeStretch(1);
            else if (action == "DOWN")
                ChangeTimeStretch(-5);
            else if (action == "UP")
                ChangeTimeStretch(5);
            else if (action == "TOGGLESTRETCH")
                ClearOSD();
            else
                handled = false;
        }
    }
   
    if (audiosyncAdjustment)
    {
        for (unsigned int i = 0; i < actions.size(); i++)
        {
            action = actions[i];
            handled = true;

            if (action == "LEFT")
                ChangeAudioSync(-1);
            else if (action == "RIGHT")
                ChangeAudioSync(1);
            else if (action == "UP")
                ChangeAudioSync(-10);
            else if (action == "DOWN")
                ChangeAudioSync(10);
            else if (action == "1")
                ChangeAudioSync(1000000);
            else if (action == "0")
                ChangeAudioSync(-1000000);
            else if (action == "TOGGLEAUDIOSYNC")
                ClearOSD();
            else
                handled = false;
        }
    }

#if DEBUG_ACTIONS
    for (uint i = 0; i < actions.size(); ++i)
        VERBOSE(VB_IMPORTANT, QString("handled(%1) actions[%2](%3)")
                .arg(handled).arg(i).arg(actions[i]));
#endif // DEBUG_ACTIONS

    if (handled)
        return;

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        action = actions[i];
        handled = true;

        if (action == "TOGGLECC")
        {
            bool valid = false;
            int page = inputKeys.toInt(&valid, 16) << 16;
            if (!valid)
                page = 0;
            ChannelClear();
            DoToggleCC(page);
        }
        else if (action == "SKIPCOMMERCIAL")
            DoSkipCommercials(1);
        else if (action == "SKIPCOMMBACK")
            DoSkipCommercials(-1);
        else if (action == "QUEUETRANSCODE")
            DoQueueTranscode();
        else if (action == "PLAY")
            DoPlay();
        else if (action == "NEXTAUDIO")
            ChangeAudioTrack(1);
        else if (action == "PREVAUDIO")
            ChangeAudioTrack(-1);
        else if (action == "NEXTSUBTITLE")
            ChangeSubtitleTrack(1);
        else if (action == "PREVSUBTITLE")
            ChangeSubtitleTrack(-1);
        else if (action == "PAUSE") 
            DoPause();
        else if (action == "SPEEDINC")
            ChangeSpeed(1);
        else if (action == "SPEEDDEC")
            ChangeSpeed(-1);
        else if (action == "TOGGLESTRETCH")
        {
            ChangeTimeStretch(0);   // just display
        }
        else if (action == "TOGGLEAUDIOSYNC")
            ChangeAudioSync(0);   // just display
        else if (action == "TOGGLEPICCONTROLS")
        {
            if (usePicControls)
            {
                picAdjustment += 1;
                if (picAdjustment >= kPictureAttribute_MAX)
                    picAdjustment = kPictureAttribute_MIN;
                DoTogglePictureAttribute();
            }
        }
        else if (action == "SEEKFFWD")
        {
            if (channelqueued)
                DoArbSeek(ARBSEEK_FORWARD);
            else if (paused)
                DoSeek(1.001 / frameRate, tr("Forward"));
            else if (!stickykeys)
            {
                if (smartForward && doSmartForward)
                    DoSeek(rewtime, tr("Skip Ahead"));
                else
                    DoSeek(fftime, tr("Skip Ahead"));
            }
            else
                ChangeFFRew(1);
        }
        else if (action == "FFWDSTICKY")
        {
            if (channelqueued)
                DoArbSeek(ARBSEEK_END);
            else if (paused)
                DoSeek(1.0, tr("Forward"));
            else
                ChangeFFRew(1);
        }
        else if (action == "SEEKRWND")
        {
            if (channelqueued)
                DoArbSeek(ARBSEEK_REWIND);
            else if (paused)
                DoSeek(-1.001 / frameRate, tr("Rewind"));
            else if (!stickykeys)
            {
                DoSeek(-rewtime, tr("Skip Back"));
                if (smartForward)
                    doSmartForward = true;
            }
            else
                ChangeFFRew(-1);
        }
        else if (action == "RWNDSTICKY")
        {
            if (channelqueued)
                DoArbSeek(ARBSEEK_SET);
            else if (paused)
                DoSeek(-1.0, tr("Rewind"));
            else
                ChangeFFRew(-1);
        }
        else if (action == "JUMPRWND")
        {
            if (prbuffer->isDVD())
            {
                prbuffer->prevTrack();
                UpdatePosOSD(0.0, tr("Previous Chapter"));
            }
            else
            {
                DoSeek(-jumptime * 60, tr("Jump Back"));
            }
        }
        else if (action == "JUMPFFWD")
        {
            if (prbuffer->isDVD())
            {
                prbuffer->nextTrack();
                UpdatePosOSD(0.0, tr("Next Chapter"));
            }
            else
            {
                DoSeek(jumptime * 60, tr("Jump Ahead"));
            }
        }
        else if (action == "JUMPBKMRK")
        {
            int bookmark = activenvp->GetBookmark();
            if (bookmark > frameRate)
                DoSeek((bookmark - activenvp->GetFramesPlayed()) / frameRate,
                       tr("Jump to Bookmark"));
        }
        else if (action == "JUMPSTART" && activenvp)
        {
            DoSeek(-activenvp->GetFramesPlayed() / frameRate,
                   tr("Jump to Beginning"));
        }
        else if (action == "CLEAROSD")
        {
            ClearOSD();
        }
        else if (action == "JUMPPREV")
        {
            nvp->SetBookmark();
            exitPlayer = true;
            wantsToQuit = true;
            jumpToProgram = true;
        }
        else if (action == "SIGNALMON")
        {
            if ((GetState() == kState_WatchingLiveTV) && activerecorder)
            {
                int rate   = sigMonMode ? 0 : 100;
                int notify = sigMonMode ? 0 : 1;

                PauseLiveTV();
                activerecorder->SetSignalMonitoringRate(rate,notify);
                UnpauseLiveTV();

                lockTimerOn = false;
                sigMonMode  = !sigMonMode;
            }
        }
        else if (action == "ESCAPE")
        {
            if (StateIsLiveTV(internalState) &&
                (lastSignalMsgTime.elapsed() < kSMExitTimeout))
                ClearOSD();
            else if (GetOSD() && ClearOSD())
                return;

            NormalSpeed();
            StopFFRew();

            if (StateIsPlaying(internalState) &&
                gContext->GetNumSetting("PlaybackExitPrompt") == 1 && 
                (!playbackinfo || !playbackinfo->isVideo)  )
            {
                nvp->Pause();

                QString message = tr("You are exiting this recording");

                QStringList options;
                options += tr("Save this position and go to the menu");
                options += tr("Do not save, just exit to the menu");
                options += tr("Keep watching");
                options += tr("Delete this recording");

                dialogname = "exitplayoptions";
                if (GetOSD())
                    GetOSD()->NewDialogBox(dialogname, message, options, 0);
            }
            else if (StateIsPlaying(internalState) &&
                     gContext->GetNumSetting("PlaybackExitPrompt") == 1 && 
                     playbackinfo && playbackinfo->isVideo)
            {
                nvp->Pause();

                QString vmessage = tr("You are exiting this video");

                QStringList voptions;
                voptions += tr("Exit to the menu");
                voptions += tr("Keep watching");
                dialogname = "videoexitplayoptions";
                if (GetOSD())
                    GetOSD()->NewDialogBox(dialogname, vmessage, voptions, 0);
            }
            else if (StateIsLiveTV(GetState()))
            {
                ChangeState(kState_None);
                exitPlayer = true;
                wantsToQuit = true;
            }
            else 
            {
                if (nvp && gContext->GetNumSetting("PlaybackExitPrompt") == 2)
                    nvp->SetBookmark();
                exitPlayer = true;
                wantsToQuit = true;
            }
            break;
        }
        else if (action == "VOLUMEDOWN")
            ChangeVolume(false);
        else if (action == "VOLUMEUP")
            ChangeVolume(true);
        else if (action == "MUTE")
            ToggleMute();
        else if (action == "STRETCHINC")
            ChangeTimeStretch(1);
        else if (action == "STRETCHDEC")
            ChangeTimeStretch(-1);
        else if (action == "TOGGLEASPECT")
            ToggleLetterbox();
        else if (action == "MENU")
            ShowOSDTreeMenu();
        else
            handled = false;
    }

    if (!handled)
    {
        if (doing_ff_rew)
        {
            for (unsigned int i = 0; i < actions.size() && !handled; i++)
            {
                action = actions[i];
                bool ok = false;
                int val = action.toInt(&ok);

                if (ok && val < (int)ff_rew_speeds.size())
                {
                    SetFFRew(val);
                    handled = true;
                }
            }

            if (!handled)
            {
                float time = StopFFRew();
                UpdatePosOSD(time, PlayMesg());
                handled = true;
            }
        }

        if (speed_index)
        {
            NormalSpeed();
            UpdatePosOSD(0.0, PlayMesg());
            handled = true;
        }
    }

    if (!handled)
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            action = actions[i];
            bool ok = false;
            int val = action.toInt(&ok);

            if (ok)
            {
                ChannelKey('0' + val);
                handled = true;
            }
            if (action == "SUBCHANNELSEP")
            {
                ChannelKey('_');
                handled = true;
            }
        }
    }

    if (StateIsLiveTV(GetState()))
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            action = actions[i];
            handled = true;

            if (action == "INFO")
            {
                if (channelqueued)
                    DoArbSeek(ARBSEEK_SET);
                else
                    ToggleOSD();
            }
            else if (action == "CHANNELUP")
            {
                if (persistentbrowsemode)
                    BrowseDispInfo(BROWSE_UP);
                else
                    ChangeChannel(CHANNEL_DIRECTION_UP);
            }
            else if (action == "CHANNELDOWN")
            {
                if (persistentbrowsemode)
                    BrowseDispInfo(BROWSE_DOWN);
                else
                    ChangeChannel(CHANNEL_DIRECTION_DOWN);
            }
            else if (action == "TOGGLERECORD")
                ToggleRecord();
            else if (action == "NEXTFAV")
                ChangeChannel(CHANNEL_DIRECTION_FAVORITE);
            else if (action == "TOGGLEFAV")
                ToggleChannelFavorite();
            else if (action == "TOGGLEINPUTS")
                ToggleInputs();
            else if (action == "SWITCHCARDS")
                SwitchCards();
            else if (action == "SELECT")
                ChannelCommit();
            else if (action == "GUIDE")
                LoadMenu();
            else if (action == "TOGGLEPIPMODE")
                TogglePIPView();
            else if (action == "TOGGLEPIPWINDOW")
                ToggleActiveWindow();
            else if (action == "SWAPPIP")
                SwapPIP();
            else if (action == "TOGGLERECCONTROLS")
                DoToggleRecPictureAttribute();
            else if (action == "TOGGLEBROWSE")
                BrowseStart();
            else if (action == "PREVCHAN")
                PreviousChannel();
            else if (action == "TOGGLESLEEP")
                ToggleSleepTimer();
            else
                handled = false;
        }

        if (redisplayBrowseInfo)
            BrowseStart();
    }
    else if (StateIsPlaying(internalState))
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            action = actions[i];
            handled = true;

            if (action == "INFO")
            {
                if (channelqueued)
                    DoArbSeek(ARBSEEK_SET);
                else
                    DoInfo();
            }
            else if (action == "SELECT")
            {
                if (!was_doing_ff_rew)
                {
                    if (gContext->GetNumSetting("AltClearSavedPosition", 1)
                        && nvp->GetBookmark())
                        nvp->ClearBookmark(); 
                    else
                        nvp->SetBookmark(); 
                }
                else
                    handled = false;
            }
            else if (action == "DELETE")
            {
                NormalSpeed();
                StopFFRew();
                nvp->SetBookmark(); 

                requestDelete = true;
                exitPlayer = true;
                wantsToQuit = true;
            }
            else if (action == "TOGGLEEDIT")
                DoEditMode();
            else if (action == "TOGGLEBROWSE")
                ShowOSDTreeMenu();
            else if (action == "CHANNELUP")
            {
                if (prbuffer->isDVD())
                {
                    prbuffer->prevTrack();
                    UpdatePosOSD(0.0, tr("Previous Chapter"));
                }
                else
                {
                    DoSeek(-jumptime * 60, tr("Jump Back"));
                }
            }    
            else if (action == "CHANNELDOWN")
            {
                if (prbuffer->isDVD())
                {
                    prbuffer->nextTrack();
                    UpdatePosOSD(0.0, tr("Next Chapter"));
                }
                else
                {
                    DoSeek(jumptime * 60, tr("Jump Ahead"));
                }
            }
            else if (action == "TOGGLESLEEP")
                ToggleSleepTimer();
            else
                handled = false;
        }
    }
}

void TV::TogglePIPView(void)
{
    if (!pipnvp)
    {
        RemoteEncoder *testrec = RemoteRequestRecorder();
        
        if (!testrec)
            return;

        if (!testrec->IsValidRecorder())
        {
            delete testrec;
            return;
        }

        piprecorder = testrec;

        QString name = "";
        long long filesize = 0;
        long long smudge = 0;

        piprecorder->Setup();
        if (!piprecorder->SetupRingBuffer(name, filesize, smudge, true))
        {
            VERBOSE(VB_IMPORTANT, "TV::HandleStateChange() Error, failed "
                    "to start RingBuffer for PiP on backend. Aborting.");
            delete testrec;
            piprecorder = NULL;
            return;
        }

        piprbuffer = new RingBuffer(name, filesize, smudge, piprecorder);

        piprecorder->SpawnLiveTV();

        while (!piprecorder->IsRecording())
            usleep(50);
    
        SetupPipPlayer();
        pthread_create(&pipdecode, NULL, SpawnDecode, pipnvp);

        while (!pipnvp->IsPlaying())
            usleep(50);

        nvp->SetPipPlayer(pipnvp);        
    }
    else
    {
        if (activenvp != nvp)
            ToggleActiveWindow();

        nvp->SetPipPlayer(NULL);
        while (!nvp->PipPlayerSet())
            usleep(50);

        piprbuffer->StopReads();
        piprbuffer->Pause();
        while (!piprbuffer->isPaused())
            usleep(50);

        pipnvp->StopPlaying();

        piprecorder->StopLiveTV();

        TeardownPipPlayer();        
    }
}

void TV::ToggleActiveWindow(void)
{
    if (!pipnvp)
        return;

    if (activenvp == nvp)
    {
        activenvp = pipnvp;
        activerbuffer = piprbuffer;
        activerecorder = piprecorder;
    }
    else
    {
        activenvp = nvp;
        activerbuffer = prbuffer;
        activerecorder = recorder;
    }
}

void TV::SwapPIP(void)
{
    if (!pipnvp)
        return;

    QString dummy;
    QString pipchanname;
    QString bigchanname;

    pipchanname = piprecorder->GetCurrentChannel();
    bigchanname = recorder->GetCurrentChannel();

    if (activenvp != nvp)
        ToggleActiveWindow();

    ChangeChannelByString(pipchanname);

    ToggleActiveWindow();

    ChangeChannelByString(bigchanname);

    ToggleActiveWindow();
}

void TV::DoPlay(void)
{
    float time = 0.0;

    if (doing_ff_rew)
    {
        time = StopFFRew();
        activenvp->Play(normal_speed, true);
        speed_index = 0;
    }
    else if (paused || (speed_index != 0))
    {
        activenvp->Play(normal_speed, true);
        paused = false;
        speed_index = 0;
    }


    if (activenvp != nvp)
        return;

    UpdatePosOSD(time, PlayMesg());

    gContext->DisableScreensaver();
}

QString TV::PlayMesg()
{
    QString mesg = QString(tr("Play"));
    if (normal_speed != 1.0)
    {
        mesg += " %1X";
        mesg = mesg.arg(normal_speed);
    }
    return mesg;
}

void TV::DoPause(void)
{
    speed_index = 0;
    float time = 0.0;

    if (paused)
        activenvp->Play(normal_speed, true);
    else 
    {
        if (doing_ff_rew)
        {
            time = StopFFRew();
            activenvp->Play(normal_speed, true);
            usleep(1000);
        }
        
        activenvp->Pause();
    }

    paused = !paused;

    if (activenvp != nvp)
        return;

    if (paused)
    {
        activerbuffer->WaitForPause();
        UpdatePosOSD(time, tr("Paused"), -1);
        gContext->RestoreScreensaver();
    }
    else
    {
        UpdatePosOSD(time, PlayMesg());
        gContext->DisableScreensaver();
    }
}

void TV::DoInfo(void)
{
    QString title, subtitle, description, category, starttime, endtime;
    QString callsign, iconpath;
    OSDSet *oset;

    if (paused || !GetOSD())
        return;

    oset = GetOSD()->GetSet("status");
    if ((oset) && (oset->Displaying()) && !prbuffer->isDVD())
    {
        InfoMap infoMap;
        playbackinfo->ToMap(infoMap);
        GetOSD()->HideSet("status");
        GetOSD()->ClearAllText("program_info");
        GetOSD()->SetText("program_info", infoMap, osd_display_time);
    }
    else
    {
        QString desc = "";
        int pos = nvp->calcSliderPos(desc);
        GetOSD()->HideSet("program_info");
        GetOSD()->StartPause(pos, false, tr("Position"), desc, osd_display_time);
        update_osd_pos = true;
    }
}

bool TV::UpdatePosOSD(float time, const QString &mesg, int disptime)
{
    bool muted = false;

    AudioOutput *aud = nvp->getAudioOutput(); 
    if (aud && !aud->GetMute())
    {
        aud->ToggleMute();
        muted = true;
    }

    if (activenvp == nvp)
    {
        QString desc = "";
        int pos = nvp->calcSliderPos(desc);
        bool slidertype = StateIsLiveTV(GetState());
        int osdtype = (doSmartForward) ? kOSDFunctionalType_SmartForward :
                                         kOSDFunctionalType_Default;
        if (GetOSD())
            GetOSD()->StartPause(pos, slidertype, mesg, desc, disptime, 
                                 osdtype);
        update_osd_pos = true;
    }

    bool res = false;

    if (time > 0.0)
        res = activenvp->FastForward(time);
    else if (time < 0.0)
        res = activenvp->Rewind(-time);

    if (muted) 
        muteTimer->start(kMuteTimeout, true);

    return res;
}

void TV::DoSeek(float time, const QString &mesg)
{
    if (!keyRepeat)
        return;

    NormalSpeed();
    time += StopFFRew();
    UpdatePosOSD(time, mesg);

    if (activenvp->GetLimitKeyRepeat())
    {
        keyRepeat = false;
        keyrepeatTimer->start(300, true);
    }
}

void TV::DoArbSeek(ArbSeekWhence whence)
{
    int seek = inputKeys.toInt();
    ChannelClear(true);

    float time = ((seek / 100) * 3600) + ((seek % 100) * 60);

    if (whence == ARBSEEK_FORWARD)
        DoSeek(time, tr("Jump Ahead"));
    else if (whence == ARBSEEK_REWIND)
        DoSeek(-time, tr("Jump Back"));
    else
    {
        if (whence == ARBSEEK_END)
            time = (activenvp->CalcMaxFFTime(LONG_MAX) / frameRate) - time;
        else
            time = time - (activenvp->GetFramesPlayed() - 1) / frameRate;
        DoSeek(time, tr("Jump To"));
    }
}

void TV::NormalSpeed(void)
{
    if (!speed_index)
        return;

    speed_index = 0;
    activenvp->Play(normal_speed, true);
}

void TV::ChangeSpeed(int direction)
{
    int old_speed = speed_index;

    if (paused)
        speed_index = -4;

    speed_index += direction;

    float time = StopFFRew();
    float speed;
    QString mesg;

    switch (speed_index)
    {
        case  2: speed = 3.0;      mesg = QString(tr("Speed 3X"));    break;
        case  1: speed = 2.0;      mesg = QString(tr("Speed 2X"));    break;
        case  0: speed = 1.0;      mesg = PlayMesg();                 break;
        case -1: speed = 1.0 / 3;  mesg = QString(tr("Speed 1/3X"));  break;
        case -2: speed = 1.0 / 8;  mesg = QString(tr("Speed 1/8X"));  break;
        case -3: speed = 1.0 / 16; mesg = QString(tr("Speed 1/16X")); break;
        case -4: DoPause(); return; break;
        default: speed_index = old_speed; return; break;
    }

    if (!activenvp->Play((speed_index==0)?normal_speed:speed, speed_index==0))
    {
        speed_index = old_speed;
        return;
    }

    paused = false;
    UpdatePosOSD(time, mesg);
}

float TV::StopFFRew(void)
{
    float time = 0.0;

    if (!doing_ff_rew)
        return time;

    if (doing_ff_rew > 0)
        time = -ff_rew_speeds[ff_rew_index] * ff_rew_repos;
    else
        time = ff_rew_speeds[ff_rew_index] * ff_rew_repos;

    doing_ff_rew = 0;
    ff_rew_index = kInitFFRWSpeed;

    activenvp->Play(normal_speed, true);

    return time;
}

void TV::ChangeFFRew(int direction)
{
    if (doing_ff_rew == direction)
    {
        while (++ff_rew_index < (int)ff_rew_speeds.size())
            if (ff_rew_speeds[ff_rew_index])
                break;
        if (ff_rew_index >= (int)ff_rew_speeds.size())
            ff_rew_index = kInitFFRWSpeed;
        SetFFRew(ff_rew_index);
    }
    else if (!ff_rew_reverse && doing_ff_rew == -direction)
    {
        while (--ff_rew_index >= kInitFFRWSpeed)
            if (ff_rew_speeds[ff_rew_index])
                break;
        if (ff_rew_index >= kInitFFRWSpeed)
            SetFFRew(ff_rew_index);
        else
        {
            float time = StopFFRew();
            UpdatePosOSD(time, PlayMesg());
        }
    }
    else
    {
        NormalSpeed();
        paused = false;
        doing_ff_rew = direction;
        SetFFRew(kInitFFRWSpeed);
    }
}

void TV::SetFFRew(int index)
{
    if (!doing_ff_rew)
        return;

    if (!ff_rew_speeds[index])
        return;

    ff_rew_index = index;
    float speed;

    QString mesg;
    if (doing_ff_rew > 0)
    {
        mesg = tr("Forward %1X").arg(ff_rew_speeds[ff_rew_index]);
        speed = ff_rew_speeds[ff_rew_index];
    }
    else
    {
        mesg = tr("Rewind %1X").arg(ff_rew_speeds[ff_rew_index]);
        speed = -ff_rew_speeds[ff_rew_index];
    }

    activenvp->Play(speed, false);
    UpdatePosOSD(0.0, mesg, -1);
}

void TV::DoQueueTranscode(void)
{
    if (internalState == kState_WatchingPreRecorded)
    {
        bool stop = false;
        if (queuedTranscode)
            stop = true;
        else if (JobQueue::IsJobQueuedOrRunning(JOB_TRANSCODE,
                                        playbackinfo->chanid,
                                        playbackinfo->recstartts))
            stop = true;
        if (stop)
        {
            JobQueue::ChangeJobCmds(JOB_TRANSCODE,
                                    playbackinfo->chanid,
                                    playbackinfo->recstartts, JOB_STOP);
            queuedTranscode = false;
            if (activenvp == nvp && GetOSD())
                GetOSD()->SetSettingsText(tr("Stopping Transcode"), 3);
        }
        else
        {
            QString jobHost = "";

            if (gContext->GetNumSetting("JobsRunOnRecordHost", 0))
                jobHost = playbackinfo->hostname;

            if (JobQueue::QueueJob(JOB_TRANSCODE,
                               playbackinfo->chanid, playbackinfo->recstartts,
                               jobHost, "", "", JOB_USE_CUTLIST))
            {
                queuedTranscode = true;
                if (activenvp == nvp && GetOSD())
                    GetOSD()->SetSettingsText(tr("Transcoding"), 3);
            } else {
                if (activenvp == nvp && GetOSD())
                    GetOSD()->SetSettingsText(tr("Try Again"), 3);
            }
        }
    }
}

void TV::DoToggleCC(int arg)
{
    nvp->ToggleCC(vbimode, arg);
}

void TV::DoSkipCommercials(int direction)
{
    NormalSpeed();
    StopFFRew();
    
    if (StateIsLiveTV(GetState()))
        return;
        
    bool muted = false;

    AudioOutput *aud = nvp->getAudioOutput();
    if (aud && !aud->GetMute())
    {
        aud->ToggleMute();
        muted = true;
    }

    bool slidertype = false;

    if (activenvp == nvp && GetOSD())
    {
        QString dummy = "";
        QString desc = tr("Searching...");
        int pos = nvp->calcSliderPos(dummy);
        GetOSD()->StartPause(pos, slidertype, tr("Skip"), desc, 6);
        update_osd_pos = true;
    }

    if (activenvp)
        activenvp->SkipCommercials(direction);

    if (muted) 
        muteTimer->start(kMuteTimeout, true);
}

void TV::SwitchCards(void)
{
    if (StateIsLiveTV(GetState()))
    {
        switchingCards = true;
        exitPlayer = true;
    }
}

void TV::ToggleInputs(void)
{
    if (activenvp == nvp && paused)
    {
        if (GetOSD())
            GetOSD()->EndPause();
        gContext->DisableScreensaver();
        paused = false;
    }

    PauseLiveTV();

    activerecorder->ToggleInputs();

    UnpauseLiveTV();
}

void TV::ToggleChannelFavorite(void)
{
    activerecorder->ToggleChannelFavorite();
}

void TV::ChangeChannel(int direction, bool force)
{
    bool muted = false;

    if (!force && GetOSD() && showBufferedWarnings && 
        (nvp && (activenvp == nvp)))
    {
        int behind = activenvp->GetSecondsBehind();
        if (behind > bufferedChannelThreshold)
        {
            VERBOSE(VB_GENERAL, QString("Channel change requested when the "
                                        "user is %1 seconds behind.")
                                        .arg(behind));

            if (!paused)
                nvp->Pause();

            QString message = tr("You are currently behind real time. If you "
                                 "change channels now, you will lose any "
                                 "unwatched video.");
            QStringList options;
            options += tr("Change the channel anyway");
            options += tr("Keep watching");

            lastCCDir = direction;
            dialogname = "ccwarningdirection";

            GetOSD()->NewDialogBox(dialogname, message, options, 0);
            return;
        }
    }

    if (nvp)
    {
        AudioOutput *aud = nvp->getAudioOutput();
        if (aud && !aud->GetMute() && activenvp == nvp)
        {
            aud->ToggleMute();
            muted = true;
        }
    }

    if (nvp && (activenvp == nvp) && paused)
    {
        if (GetOSD())
            GetOSD()->EndPause();
        gContext->DisableScreensaver();
        paused = false;
    }

    // Save the current channel if this is the first time
    if (nvp && (activenvp == nvp) && prevChan.size() == 0)
        AddPreviousChannel();

    PauseLiveTV();

    activerecorder->ChangeChannel(direction);
    ChannelClear();

    if (muted)
        muteTimer->start(kMuteTimeout * 2, true);

    UnpauseLiveTV();
}

QString TV::QueuedChannel(void)
{
    if (!channelqueued)
        return "";

    // strip initial zeros.
    int nzi = channelKeys.find(QRegExp("([1-9]|\\w)"));
    if (nzi > 0)
        channelKeys = channelKeys.right(channelKeys.length() - nzi);

    return channelKeys.stripWhiteSpace();
}

/** \fn TV::ChannelClear(bool)
 *  \brief Clear channel key buffer of input keys.
 *  \param hideosd, if true hides "channel_number" OSDSet.
 */
void TV::ChannelClear(bool hideosd)
{
    if (lookForChannel)
        return;

    if (hideosd && GetOSD()) 
        GetOSD()->HideSet("channel_number");

    channelqueued = false;

    channelKeys = "";
    inputKeys = "";
    channelid = "";
}

void TV::ChannelKey(char key)
{
    static char *spacers[5] = { "_", "-", "#", ".", NULL };

    channelKeys   = channelKeys.append(key).right(kChannelKeysMax);
    inputKeys     = inputKeys.append(key).right(kChannelKeysMax);
    channelqueued = true;

    /* 
     * Always use smartChannelChange when channel numbers are entered in
     * browse mode because in browse mode space/enter exit browse mode and
     * change to the currently browsed channel. This makes smartChannelChange
     * the only way to enter a channel number to browse without waiting for the
     * OSD to fadeout after entering numbers.
     */
    bool do_smart = (smartChannelChange || browsemode);
    QString chan = QueuedChannel();
    if (StateIsLiveTV(GetState()) && !chan.isEmpty() && do_smart)
    {
        // Look for channel in line-up
        bool unique = false;
        bool ok = activerecorder->CheckChannelPrefix(chan, unique);

        // If pure channel not in line-up, try adding a spacer
        QString mod = chan;
        for (uint i=0; (spacers[i] != NULL) && !ok; ++i)
        {
            mod = chan.left(chan.length()-1) + spacers[i] + chan.right(1);
            ok = activerecorder->CheckChannelPrefix(mod, unique);
        }

        // Use valid channel if it is there, otherwise reset...
        channelKeys = (ok) ? mod : QString("%1").arg(key);
        do_smart &= unique;
    }

    if (activenvp == nvp && GetOSD())
    {
        InfoMap infoMap;

        infoMap["channum"] = (do_smart) ? channelKeys : inputKeys;
        infoMap["callsign"] = "";
        GetOSD()->ClearAllText("channel_number");
        GetOSD()->SetText("channel_number", infoMap, 2);
    }

    if (do_smart)
        ChannelCommit();
}

void TV::ChannelCommit(void)
{
    if (!channelqueued)
        return;

    QString chan = QueuedChannel();

    if (browsemode)
    {
        BrowseChannel(chan);
        if (activenvp == nvp && GetOSD())
            GetOSD()->HideSet("channel_number");
    }
    else
        ChangeChannelByString(chan);

    ChannelClear();
}

void TV::ChangeChannelByString(QString &name, bool force)
{
    bool muted = false;

    if (!channelid.isEmpty() && activerecorder &&
        activerecorder->ShouldSwitchToAnotherCard(channelid))
    {
        RemoteEncoder *testrec = NULL;
        QStringList reclist;
        GetValidRecorderList(channelid, reclist);
        testrec = RemoteRequestFreeRecorderFromList(reclist);
        if (!testrec || !testrec->IsValidRecorder())
        {
            ChannelClear();
            ShowNoRecorderDialog();
            if (testrec)
                delete testrec;
            return;
        }

        if (!prevChan.empty() && prevChan.back() == name)
        {
            // need to remove it if the new channel is the same as the old.
            prevChan.pop_back();
        }

        // found the card on a different recorder.
        delete testrec;
        lookForChannel = true;
        channelqueued = true;
        SwitchCards();
        return;
    }

    if (!prevChan.empty() && prevChan.back() == name)
        return;

    if (!activerecorder->CheckChannel(name))
        return;

    if (!force && GetOSD() && nvp && (activenvp == nvp) && showBufferedWarnings)
    {
        int behind = activenvp->GetSecondsBehind();
        if (behind > bufferedChannelThreshold)
        {
            VERBOSE(VB_GENERAL, QString("Channel change requested when the "
                                        "user is %1 seconds behind.")
                                        .arg(behind));

            if (!paused)
                nvp->Pause();

            QString message = tr("You are currently behind real time. If you "
                                 "change channels now, you will lose any "
                                 "unwatched video.");
            QStringList options;
            options += tr("Change the channel anyway");
            options += tr("Keep watching");

            lastCC = name;
            dialogname = "ccwarningstring";

            GetOSD()->NewDialogBox(dialogname, message, options, 0);
            return;
        }
    }

    if (nvp)
    {
        AudioOutput *aud = nvp->getAudioOutput();
        if (aud && !aud->GetMute() && activenvp == nvp)
        {
            aud->ToggleMute();
            muted = true;
        }
    }

    if (nvp && (activenvp == nvp) && paused && GetOSD())
    {
        GetOSD()->EndPause();
        gContext->DisableScreensaver();
        paused = false;
    }

    // Save the current channel if this is the first time
    if (prevChan.size() == 0)
        AddPreviousChannel();

    PauseLiveTV();

    activerecorder->SetChannel(name);

    if (muted)
        muteTimer->start(kMuteTimeout * 2, true);

    UnpauseLiveTV();
}

void TV::AddPreviousChannel(void)
{
    // Don't store more than thirty channels.  Remove the first item
    if (prevChan.size() > 29)
        prevChan.erase(prevChan.begin());

    // This method builds the stack of previous channels
    prevChan.push_back(activerecorder->GetCurrentChannel());
}

void TV::PreviousChannel(void)
{
    // Save the channel if this is the first time, and return so we don't
    // change chan to the current chan
    if (prevChan.size() == 0)
        return;

    // Increment the prevChanKeyCnt counter so we know how far to jump
    prevChanKeyCnt++;

    // Figure out the vector the desired channel is in
    int i = (prevChan.size() - prevChanKeyCnt - 1) % prevChan.size();

    // Display channel name in the OSD for up to 1 second.
    if (activenvp == nvp && GetOSD())
    {
        GetOSD()->HideSet("program_info");

        InfoMap infoMap;

        infoMap["channum"] = prevChan[i];
        infoMap["callsign"] = "";
        GetOSD()->ClearAllText("channel_number");
        GetOSD()->SetText("channel_number", infoMap, 1);
    }

    // Reset the timer
    prevChanTimer->stop();
    prevChanTimer->start(750);
}

void TV::SetPreviousChannel()
{
    // Stop the timer
    prevChanTimer->stop();

    // Figure out the vector the desired channel is in
    int i = (prevChan.size() - prevChanKeyCnt - 1) % prevChan.size();

    // Reset the prevChanKeyCnt counter
    prevChanKeyCnt = 0;

    // Only change channel if prevChan[vector] != current channel
    QString chan_name = activerecorder->GetCurrentChannel();

    if (chan_name != prevChan[i].latin1())
    {
        channelKeys = prevChan[i];
        channelqueued = true;
    }

    //Turn off the OSD Channel Num so the channel changes right away
    if (activenvp == nvp && GetOSD())
        GetOSD()->HideSet("channel_number");
}

bool TV::ClearOSD(void)
{
    bool res = false;

    if (channelqueued)
    {
        ChannelClear(true);
        res = true;
    }

    if (GetOSD() && GetOSD()->HideAll())
        res = true;

    while (res && GetOSD() && GetOSD()->HideAll())
        usleep(1000);

    return res;
}

void TV::ToggleOSD(void)
{
    if (!GetOSD())
        return;

    OSDSet *oset = GetOSD()->GetSet("program_info");
    if ((oset) && (oset->Displaying() || prbuffer->isDVD()))
        GetOSD()->HideAll();
    else if (!prbuffer->isDVD())
        UpdateOSD();
}

static void GetProgramInfo(NuppelVideoPlayer *activenvp,
                           InfoMap &infoMap)
{
    ProgramInfo *progInfo = NULL;
    QDateTime now = QDateTime::currentDateTime(), cur, prev;
    QString prevTimeStr = infoMap["startts"];
    int behind = 0;

    // Current playback time
    if (activenvp)
        behind = activenvp->GetSecondsBehind();
    cur = now.addSecs(0-behind);

    // Previous osd time
    if ((prevTimeStr == QString::null) || (prevTimeStr.length()<2))
        prev = now.addSecs(1);
    else 
        prev = QDateTime::fromString(infoMap["startts"], Qt::ISODate);

    // If we need it, try to set progInfo,
    if (cur < prev)
        progInfo = ProgramInfo::GetProgramAtDateTime(infoMap["chanid"], cur);

    // If there is a progInfo, use it
    if (progInfo)
        progInfo->ToMap(infoMap);
}

void TV::UpdateOSD(void)
{
    InfoMap infoMap;

    // Collect channel info
    GetChannelInfo(activerecorder, infoMap);

    // Collect program info, if we are out of date
    GetProgramInfo(activenvp, infoMap);

    if (GetOSD())
    {
        // Clear previous osd and add new info
        GetOSD()->ClearAllText("program_info");
        GetOSD()->SetText("program_info", infoMap, osd_display_time);
        GetOSD()->ClearAllText("channel_number");
        GetOSD()->SetText("channel_number", infoMap, osd_display_time);
    }
}

void TV::UpdateOSDInput(void)
{
    if (!activerecorder)
        return;

    QString name = "";
    activerecorder->GetInputName(name);
    QString msg = QString ("%1: %2")
        .arg(activerecorder->GetRecorderNumber()).arg(name);

    if (GetOSD())
        GetOSD()->SetSettingsText(msg, 3);
}

/** \fn TV::UpdateOSDSignal(const QStringList&)
 *  \brief Updates Signal portion of OSD...
 */
void TV::UpdateOSDSignal(const QStringList& strlist)
{
    QMutexLocker locker(&osdlock);

    if (!GetOSD())
    {
        if (&lastSignalMsg != &strlist)
            lastSignalMsg = strlist;
        return;
    }

    SignalMonitorList slist = SignalMonitorValue::Parse(strlist);

    InfoMap infoMap = lastSignalUIInfo;
    if (lastSignalUIInfoTime.elapsed() > 5000 || infoMap["callsign"].isEmpty())
    {
        //lastSignalUIInfo["name"]  = "signalmonitor";
        //lastSignalUIInfo["title"] = "Signal Monitor";
        lastSignalUIInfo.clear();
        GetChannelInfo(activerecorder, lastSignalUIInfo);
        GetProgramInfo(activenvp, lastSignalUIInfo);
        infoMap = lastSignalUIInfo;
        lastSignalUIInfoTime.start();
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

    int   sig = 100;
    float snr  = 70000.0f, Usnr = 0.0f;
    uint  ber  = 0xffffffff;
    QString pat(""), pmt(""), mgt(""), vct(""), nit(""), sdt("");
    for (it = slist.begin(); it != slist.end(); ++it)
    {
        if ("error" == it->GetShortName() || "message" == it->GetShortName())
            continue;

        infoMap[it->GetShortName()] = QString::number(it->GetValue());
        if ("signal" == it->GetShortName())
        {
            sig = it->GetNormalizedValue(0, 100);
            infoMap["signal"] = QString::number(sig);
        }
        else if ("snr" == it->GetShortName())
        {
            snr = it->GetValue();
            Usnr = (uint) it->GetValue();
        }
        else if ("ber" == it->GetShortName())
            ber = it->GetValue();
        else if ("seen_pat" == it->GetShortName())
            pat = it->IsGood() ? "a" : " ";
        else if ("matching_pat" == it->GetShortName())
            pat = it->IsGood() ? "A" : pat;
        else if ("seen_pmt" == it->GetShortName())
            pmt = it->IsGood() ? "m" : " ";
        else if ("matching_pmt" == it->GetShortName())
            pmt = it->IsGood() ? "M" : pmt;
        else if ("seen_mgt" == it->GetShortName())
            mgt = it->IsGood() ? "g" : " ";
        else if ("matching_mgt" == it->GetShortName())
            mgt = it->IsGood() ? "G" : mgt;
        else if ("seen_vct" == it->GetShortName())
            vct = it->IsGood() ? "v" : " ";
        else if ("matching_vct" == it->GetShortName())
            vct = it->IsGood() ? "V" : vct;
        else if ("seen_nit" == it->GetShortName())
            nit = it->IsGood() ? "n" : " ";
        else if ("matching_nit" == it->GetShortName())
            nit = it->IsGood() ? "N" : nit;
        else if ("seen_sdt" == it->GetShortName())
            sdt = it->IsGood() ? "s" : " ";
        else if ("matching_sdt" == it->GetShortName())
            sdt = it->IsGood() ? "S" : sdt;
    }

    bool    allGood = SignalMonitorValue::AllGood(slist);
    QString slock   = ("1" == infoMap["slock"]) ? "L" : "l";
    QString lockMsg = (slock=="L") ? tr("Partial Lock") : tr("No Lock");
    QString sigMsg  = allGood ? tr("Lock") : lockMsg;

    QString sigDesc = tr("Signal %1\%").arg(sig);
    if (sig != 0 && sig != 100);
    else if (snr < 32766.0f)
        sigDesc = tr("S/N %1 dB")
            .arg(log10f((snr < -16384) ? Usnr: snr), 4, 'f', 1);
    else if (ber < 65535)
        sigDesc = tr("Bit Errors %1").arg(ber, 4);

    sigDesc = sigDesc + QString(" (%1%2%3%4%5%6%7) %8")
        .arg(slock).arg(pat).arg(pmt).arg(mgt).arg(vct)
        .arg(nit).arg(sdt).arg(sigMsg);

    //GetOSD()->ClearAllText("signal_info");
    //GetOSD()->SetText("signal_info", infoMap, -1);

    GetOSD()->ClearAllText("channel_number");
    GetOSD()->SetText("channel_number", infoMap, osd_display_time);

    infoMap["description"] = sigDesc;
    GetOSD()->ClearAllText("program_info");
    GetOSD()->SetText("program_info", infoMap, osd_display_time);

    lastSignalMsg.clear();
    lastSignalMsgTime.start();

    // Turn off lock timer if we have an "All Good" or good PMT
    if (allGood || (pmt == "M"))
        lockTimerOn = false;
}

void TV::UpdateOSDTimeoutMessage(void)
{
    QString dlg_name("channel_timed_out");
    bool timed_out = lockTimerOn && ((uint)lockTimer.elapsed() > lockTimeout);
    OSD *osd = GetOSD();

    if (!osd)
    {
        if (timed_out)
        {
            VERBOSE(VB_IMPORTANT, "Error: You have no OSD, "
                    "but tuning has already taken too long.");
        }
        return;
    }

    if (!timed_out)
    {
        if (osd->DialogShowing(dlg_name))
            osd->TurnDialogOff(dlg_name);
        return;
    }

    if (osd->DialogShowing(dlg_name))
        return;

    // create dialog...
    static QString chan_up   = GET_KEY("TV Playback", "CHANNELUP");
    static QString chan_down = GET_KEY("TV Playback", "CHANNELDOWN");
    static QString tog_in    = GET_KEY("TV Playback", "TOGGLEINPUTS");
    static QString tog_cards = GET_KEY("TV Playback", "SWITCHCARDS");

    QString message = tr(
        "You should have gotten a channel lock by now. "
        "You can continue to wait for a signal, or you "
        "can change the channels with %1 or %2, change "
        "input's (%3), capture cards (%4), etc.")
        .arg(chan_up).arg(chan_down).arg(tog_in).arg(tog_cards);

    QStringList options;
    options += tr("OK");

    dialogname = dlg_name;
    osd->NewDialogBox(dialogname, message, options, 0);
}

void TV::UpdateLCD(void)
{
    // Make sure the LCD information gets updated shortly
    lastLcdUpdate = lastLcdUpdate.addSecs(-120);
}

void TV::ShowLCDChannelInfo(void)
{
    class LCD * lcd = LCD::Get();
    if (lcd == NULL)
        return;

    QString title, subtitle, callsign, dummy;
    GetChannelInfo(recorder, title, subtitle, dummy, dummy, dummy, dummy, 
                   callsign, dummy, dummy, dummy, dummy, dummy, dummy,
                   dummy, dummy, dummy);

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

    tMin = QString("%1 %2").arg(minutes).arg(TV::tr("minutes"));
    tHrsMin.sprintf("%d:%02d", hours, min);
}

/** \fn TV::GetNextProgram(RemoteEncoder*,int,InfoMap&)
 *  \brief Fetches information on the desired program from the backend.
 *  \param enc RemoteEncoder to query, if null query the activerecorder.
 *  \param direction BrowseDirection to get information on.
 */
void TV::GetNextProgram(RemoteEncoder *enc, int direction,
                        InfoMap &infoMap)
{
    QString title, subtitle, desc, category, endtime, callsign, iconpath;
    QString lenM, lenHM;

    QString starttime = infoMap["dbstarttime"];
    QString chanid    = infoMap["chanid"];
    QString channum   = infoMap["channum"];
    QString seriesid  = infoMap["seriesid"];
    QString programid = infoMap["programid"];

    GetNextProgram(enc, direction,
                   title,     subtitle, desc,      category,
                   starttime, endtime,  callsign,  iconpath,
                   channum,   chanid,   seriesid,  programid);

    QString timeFmt = gContext->GetSetting("TimeFormat");
    QString dateFmt = gContext->GetSetting("ShortDateFormat");
    QDateTime begts = QDateTime::fromString(starttime, Qt::ISODate);
    QDateTime endts = QDateTime::fromString(endtime,   Qt::ISODate);
    format_time(begts.secsTo(endts), lenM, lenHM);

    infoMap["dbstarttime"] = starttime;
    infoMap["dbendtime"]   = endtime;
    infoMap["title"]       = title;
    infoMap["subtitle"]    = subtitle;
    infoMap["description"] = desc;
    infoMap["category"]    = category;
    infoMap["callsign"]    = callsign;
    infoMap["starttime"]   = begts.toString(timeFmt);
    infoMap["startdate"]   = begts.toString(dateFmt);
    infoMap["endtime"]     = endts.toString(timeFmt);
    infoMap["enddate"]     = endts.toString(dateFmt);
    infoMap["channum"]     = channum;
    infoMap["chanid"]      = chanid;
    infoMap["iconpath"]    = iconpath;
    infoMap["seriesid"]    = seriesid;
    infoMap["programid"]   = programid;
    infoMap["lenmins"]     = lenM;
    infoMap["lentime"]     = lenHM;
}

/** \fn TV::GetNextProgram(RemoteEncoder*,int,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&)
 *  \brief Fetches information on the desired program from the backend.
 *  \param enc RemoteEncoder to query, if null query the activerecorder.
 *  \param direction BrowseDirection to get information on.
 */
void TV::GetNextProgram(RemoteEncoder *enc, int direction,
                        QString &title,     QString &subtitle,
                        QString &desc,      QString &category,
                        QString &starttime, QString &endtime,
                        QString &callsign,  QString &iconpath,
                        QString &channame,  QString &chanid,
                        QString &seriesid,  QString &programid)
{
    if (!enc)
        enc = activerecorder;

    enc->GetNextProgram(direction,
                        title,     subtitle,  desc,      category,
                        starttime, endtime,   callsign,  iconpath,
                        channame,  chanid,    seriesid,  programid);
}

void TV::GetChannelInfo(RemoteEncoder *enc, InfoMap &infoMap)
{
    if (!enc)
        enc = activerecorder;

    QString title, subtitle, description, category, starttime, endtime;
    QString callsign, iconpath, channum, chanid, seriesid, programid;
    QString outputFilters;
    QString repeat, airdate, stars;

    enc->GetChannelInfo(title, subtitle, description, category, starttime,
                        endtime, callsign, iconpath, channum, chanid,
                        seriesid, programid, outputFilters,
                        repeat, airdate, stars);
    
    QString dateFormat = gContext->GetSetting("DateFormat", "ddd MMMM d");
    QString oldDateFormat = gContext->GetSetting("OldDateFormat", "M/d/yyyy");
    QString tmFmt = gContext->GetSetting("TimeFormat");
    QString dtFmt = gContext->GetSetting("ShortDateFormat");

    infoMap["startts"] = starttime;
    infoMap["endts"] = endtime;
    infoMap["dbstarttime"] = starttime;
    infoMap["dbendtime"] = endtime;
    infoMap["title"] = title;
    infoMap["subtitle"] = subtitle;
    infoMap["description"] = description;
    infoMap["category"] = category;
    infoMap["callsign"] = callsign;
    
    QDateTime startts, endts;
    if ("" == starttime)
        infoMap["starttime"] = infoMap["startdate"] = "";
    else
    {
        startts = QDateTime::fromString(starttime, Qt::ISODate);
        infoMap["starttime"] = startts.toString(tmFmt);
        infoMap["startdate"] = startts.toString(dtFmt);
    }
    if ("" == endtime)
        infoMap["endtime"] = infoMap["enddate"] = "";
    else
    {
        endts = QDateTime::fromString(endtime, Qt::ISODate);
        infoMap["endtime"] = endts.toString(tmFmt);
        infoMap["enddate"] = endts.toString(dtFmt);
    }
    QString lenM(""), lenHM("");
    if ((starttime != "") && (endtime != ""))
        format_time(startts.secsTo(endts), lenM, lenHM);

    infoMap["channum"]   = channum;
    infoMap["chanid"]    = chanid;
    infoMap["iconpath"]  = iconpath;
    infoMap["seriesid"]  = seriesid;
    infoMap["programid"] = programid;
    infoMap["lenmins"]   = lenM;
    infoMap["lentime"]   = lenHM;

    bool ok;
    double fstars = stars.toFloat(&ok);
    if (ok)
        infoMap["stars"] = QString("(%1 %2) ")
            .arg(4.0 * fstars).arg(QObject::tr("stars"));
    else
        infoMap["stars"] = "";

    QDate originalAirDate;       
    
    if (airdate.isEmpty())
    {
        originalAirDate = startts.date();
    }
    else
    {
        originalAirDate = QDate::fromString(airdate, Qt::ISODate);
    }        

    if (!repeat.isEmpty() && repeat.toInt())
    {
        infoMap["REPEAT"] = QString("(%1) ").arg(QObject::tr("Repeat"));
        infoMap["LONGREPEAT"] = QString("(%1 %2) ")
                                .arg(QObject::tr("Repeat"))
                                .arg(originalAirDate.toString(oldDateFormat));
    }
    else
    {
        infoMap["REPEAT"] = "";
        infoMap["LONGREPEAT"] = "";
    }
                
    infoMap["originalairdate"]= originalAirDate.toString(dateFormat);
    infoMap["shortoriginalairdate"]= originalAirDate.toString(dtFmt);

}

void TV::GetChannelInfo(RemoteEncoder *enc, QString &title, QString &subtitle, 
                        QString &desc, QString &category, QString &starttime, 
                        QString &endtime, QString &callsign, QString &iconpath,
                        QString &channelname, QString &chanid,
                        QString &seriesid, QString &programid, QString &outFilters,
                        QString &repeat, QString &airdate, QString &stars)
{
    if (!enc)
        enc = activerecorder;
    if (!enc)
        return;

    enc->GetChannelInfo(title, subtitle, desc, category, starttime, endtime, 
                        callsign, iconpath, channelname, chanid,
                        seriesid, programid, outFilters, repeat, airdate,
                        stars);
}

void TV::EmbedOutput(WId wid, int x, int y, int w, int h)
{
    embedWinID = wid;
    embedBounds = QRect(x,y,w,h);

    if (nvp)
        nvp->EmbedInWidget(wid, x, y, w, h);
}

void TV::StopEmbeddingOutput(void)
{
    if (nvp)
        nvp->StopEmbedding();
    embedWinID = 0;
}

void TV::doLoadMenu(void)
{
    QString dummy;
    QString channame = "3";

    bool fullscreen = !gContext->GetNumSetting("GuiSizeForTV", 0);
    MythMainWindow* mwnd = gContext->GetMainWindow();
    
    if (fullscreen)
    {
        int xbase, ybase, screenwidth, screenheight;
        float wmult, hmult;
        gContext->GetScreenSettings(xbase, screenwidth, wmult,
                                    ybase, screenheight, hmult);
        mwnd->setGeometry(xbase, ybase, screenwidth, screenheight);
        mwnd->setFixedSize(QSize(screenwidth, screenheight));
    }

    if (activerecorder)
        channame = activerecorder->GetCurrentChannel();

    bool allowsecondary = true;

    if (nvp && nvp->getVideoOutput())
        allowsecondary = nvp->getVideoOutput()->AllowPreviewEPG();

    QString chanid = RunProgramGuide(channame, true, this, allowsecondary);

    if (channame != "" && chanid != "")
    {
        channelKeys = channame.left(4);
        channelid = chanid;
        channelqueued = true; 
    }

    StopEmbeddingOutput();

    if (fullscreen) 
    {
        int xbase, ybase, screenwidth, screenheight;
        float wmult, hmult;
        gContext->GetScreenSettings(xbase, screenwidth, wmult,
                                    ybase, screenheight, hmult);
        mwnd->setGeometry(xbase, ybase, screenwidth, screenheight);
        mwnd->setFixedSize(QSize(screenwidth, screenheight));
    }

    menurunning = false;
}

void *TV::MenuHandler(void *param)
{
    TV *obj = (TV *)param;
    obj->doLoadMenu();

    return NULL;
}

void TV::LoadMenu(void)
{
    if (menurunning == true)
        return;
    menurunning = true;
    pthread_t tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    pthread_create(&tid, &attr, TV::MenuHandler, this);
}

void TV::ChangeBrightness(bool up, bool recorder)
{
    int brightness;
    QString text;

    if (GetOSD())
    {
        if (recorder)
        {
            brightness = activerecorder->ChangeBrightness(up);
            text = QString(tr("Brightness (REC) %1 %")).arg(brightness);
            GetOSD()->StartPause(brightness * 10, true, tr("Adjust Recording"),
                text, 5, kOSDFunctionalType_RecPictureAdjust);
        }
        else
        {
            brightness = nvp->getVideoOutput()->ChangeBrightness(up);
            gContext->SaveSetting("PlaybackBrightness", brightness);
            text = QString(tr("Brightness %1 %")).arg(brightness);
            GetOSD()->StartPause(brightness * 10, true, tr("Adjust Picture"),
                text, 5, kOSDFunctionalType_PictureAdjust);
        }

        update_osd_pos = false;
    }
}

void TV::ChangeContrast(bool up, bool recorder)
{
    int contrast;
    QString text;

    if (GetOSD())
    {
        if (recorder)
        {
            contrast = activerecorder->ChangeContrast(up);
            text = QString(tr("Contrast (REC) %1 %")).arg(contrast);
            GetOSD()->StartPause(contrast * 10, true, tr("Adjust Recording"),
                text, 5, kOSDFunctionalType_RecPictureAdjust);
        }
        else
        {
            contrast = nvp->getVideoOutput()->ChangeContrast(up);
            gContext->SaveSetting("PlaybackContrast", contrast);
            text = QString(tr("Contrast %1 %")).arg(contrast);
            GetOSD()->StartPause(contrast * 10, true, tr("Adjust Picture"),
                text, 5, kOSDFunctionalType_PictureAdjust);
        }

        update_osd_pos = false;
    }
}

void TV::ChangeColour(bool up, bool recorder)
{
    int colour;
    QString text;

    if (GetOSD())
    {
        if (recorder)
        {
            colour = activerecorder->ChangeColour(up);
            text = QString(tr("Colour (REC) %1 %")).arg(colour);
            GetOSD()->StartPause(colour * 10, true, tr("Adjust Recording"),
                text, 5, kOSDFunctionalType_RecPictureAdjust);
        }
        else
        {
            colour = nvp->getVideoOutput()->ChangeColour(up);
            gContext->SaveSetting("PlaybackColour", colour);
            text = QString(tr("Colour %1 %")).arg(colour);
            GetOSD()->StartPause(colour * 10, true, tr("Adjust Picture"),
                text, 5, kOSDFunctionalType_PictureAdjust);
        }

        update_osd_pos = false;
    }
}

void TV::ChangeHue(bool up, bool recorder)
{
    int hue;
    QString text;

    if (GetOSD())
    {
        if (recorder)
        {
            hue = activerecorder->ChangeHue(up);
            text = QString(tr("Hue (REC) %1 %")).arg(hue);
            GetOSD()->StartPause(hue * 10, true, tr("Adjust Recording"),
                text, 5, kOSDFunctionalType_RecPictureAdjust);
        }
        else
        {
            hue = nvp->getVideoOutput()->ChangeHue(up);
            gContext->SaveSetting("PlaybackHue", hue);
            text = QString(tr("Hue %1 %")).arg(hue);
            GetOSD()->StartPause(hue * 10, true, tr("Adjust Picture"),
                text, 5, kOSDFunctionalType_PictureAdjust);
        }

        update_osd_pos = false;
    }
}

void TV::ChangeVolume(bool up)
{
    AudioOutput *aud = nvp->getAudioOutput();
    if (!aud)
        return;

    if (up)
        aud->AdjustCurrentVolume(2);
    else 
        aud->AdjustCurrentVolume(-2);

    int curvol = aud->GetCurrentVolume();
    QString text = QString(tr("Volume %1 %")).arg(curvol);

    if (GetOSD() && !browsemode)
    {
        GetOSD()->StartPause(curvol * 10, true, tr("Adjust Volume"), text, 5, 
                        kOSDFunctionalType_PictureAdjust);
        update_osd_pos = false;
    }
}

void TV::ChangeTimeStretch(int dir, bool allowEdit)
{
    float new_normal_speed = normal_speed + 0.05*dir;
    stretchAdjustment = allowEdit;

    if (new_normal_speed > 2.0 || new_normal_speed < 0.48)
        return;

    normal_speed = new_normal_speed;

    activenvp->Play(normal_speed, true);

    QString text = QString(tr("Time Stretch %1X")).arg(normal_speed);

    if (GetOSD() && !browsemode)
    {
        int val = (int)(normal_speed*500);
        GetOSD()->StartPause(val, false, tr("Adjust Time Stretch"), text, 10, 
                        kOSDFunctionalType_TimeStretchAdjust);
        update_osd_pos = false;
    }
}

// dir in 10ms jumps
void TV::ChangeAudioSync(int dir, bool allowEdit)
{
    long long newval;

    if (!audiosyncAdjustment)
        audiosyncBaseline = activenvp->GetAudioTimecodeOffset();

    audiosyncAdjustment = allowEdit;

    if (dir == 1000000)
    {
        newval = activenvp->ResyncAudioTimecodeOffset() - 
                 audiosyncBaseline;
        audiosyncBaseline = activenvp->GetAudioTimecodeOffset();
    }
    else if (dir == -1000000)
    {
        newval = activenvp->ResetAudioTimecodeOffset() - 
                 audiosyncBaseline;
        audiosyncBaseline = activenvp->GetAudioTimecodeOffset();
    }
    else
        newval = activenvp->AdjustAudioTimecodeOffset(dir*10) - 
                 audiosyncBaseline;

    if (GetOSD() && !browsemode)
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

        GetOSD()->StartPause((val/2)+500, false, tr("Adjust Audio Sync"), text, 10, 
                        kOSDFunctionalType_AudioSyncAdjust);
        update_osd_pos = false;
    }
}

void TV::ToggleMute(void)
{
    kMuteState mute_status;

    AudioOutput *aud = nvp->getAudioOutput();
    if (!aud)
        return;

    if (!MuteIndividualChannels)
    {
        aud->ToggleMute();
        bool muted = aud->GetMute();
        if (muted) 
            mute_status = MUTE_BOTH;
        else
            mute_status = MUTE_OFF;
    }
    else mute_status = aud->IterateMutedChannels();

    QString text;

    switch (mute_status)
    {
       case MUTE_OFF: text = tr("Mute Off"); break;
       case MUTE_BOTH:  text = tr("Mute On"); break;
       case MUTE_LEFT: text = tr("Left Channel Muted"); break;
       case MUTE_RIGHT: text = tr("Right Channel Muted"); break;
    }
 
    if (GetOSD() && !browsemode)
        GetOSD()->SetSettingsText(text, 5);
}

void TV::ToggleSleepTimer(void)
{
    QString text;

    // increment sleep index, cycle through
    if (++sleep_index == sleep_times.size()) 
        sleep_index = 0;

    // turn sleep timer off
    if (sleep_times[sleep_index].seconds == 0)
        sleepTimer->stop();
    else
    {
        if (sleepTimer->isActive())
            // sleep timer is active, adjust interval
            sleepTimer->changeInterval(sleep_times[sleep_index].seconds *
                                       1000);
        else
            // sleep timer is not active, start it
            sleepTimer->start(sleep_times[sleep_index].seconds * 1000, 
                              TRUE);
    }

    text = tr("Sleep ") + " " + sleep_times[sleep_index].dispString;

    // display OSD
    if (GetOSD() && !browsemode)
        GetOSD()->SetSettingsText(text, 3);
}

void TV::SleepEndTimer(void)
{
    exitPlayer = true;
    wantsToQuit = true;
}

void TV::ToggleLetterbox(int letterboxMode)
{
    nvp->ToggleLetterbox(letterboxMode);
    int letterbox = nvp->GetLetterbox();
    QString text;

    switch (letterbox)
    {
        case kLetterbox_4_3:          text = tr("4:3"); break;
        case kLetterbox_16_9:         text = tr("16:9"); break;
        case kLetterbox_4_3_Zoom:     text = tr("4:3 Zoom"); break;
        case kLetterbox_16_9_Zoom:    text = tr("16:9 Zoom"); break;
        case kLetterbox_16_9_Stretch: text = tr("16:9 Stretch"); break;
        case kLetterbox_Fill:         text = tr("Fill"); break;
        default:                      text = tr("Off"); break;
    }

    if (GetOSD() && !browsemode && !GetOSD()->IsRunningTreeMenu())
        GetOSD()->SetSettingsText(text, 3);
}

void TV::EPGChannelUpdate(QString chanid, QString chanstr)
{
    if (chanid != "" && chanstr != "")
    {
        channelKeys = chanstr.left(4);
        channelid = chanid;
        channelqueued = true; 
    }
}

void TV::KeyRepeatOK(void)
{
    keyRepeat = true;
}

void TV::UnMute(void)
{
    // If muted, unmute
    AudioOutput *aud = nvp->getAudioOutput();
    if (aud && aud->GetMute())
        aud->ToggleMute();
}

void TV::customEvent(QCustomEvent *e)
{
    if ((MythEvent::Type)(e->type()) == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent *)e;
        QString message = me->Message();

        if (GetState() == kState_WatchingRecording &&
            message.left(14) == "DONE_RECORDING")
        {
            message = message.simplifyWhiteSpace();
            QStringList tokens = QStringList::split(" ", message);
            int cardnum = tokens[1].toInt();
            int filelen = tokens[2].toInt();

            if (cardnum == recorder->GetRecorderNumber())
            {
                nvp->SetWatchingRecording(false);
                nvp->SetLength(filelen);
                ChangeState(kState_WatchingPreRecorded);
            }
        }
        else if (StateIsLiveTV(GetState()) &&
                 message.left(14) == "ASK_RECORDING ")
        {
            message = message.simplifyWhiteSpace();
            QStringList tokens = QStringList::split(" ", message);
            int cardnum = tokens[1].toInt();
            int timeuntil = tokens[2].toInt();

            if (cardnum == recorder->GetRecorderNumber())
            {
                menurunning = false;
                AskAllowRecording(me->ExtraDataList(), timeuntil);
            }
        }
        else if (recorder && message.left(11) == "QUIT_LIVETV")
        {
            message = message.simplifyWhiteSpace();
            QStringList tokens = QStringList::split(" ", message);
            int cardnum = tokens[1].toInt();

            if (cardnum == recorder->GetRecorderNumber())
            {
                menurunning = false;
                wantsToQuit = false;
                exitPlayer = true;
            }
        }
        else if (nvp && message.left(12) == "EXIT_TO_MENU")
        {
            int exitprompt = gContext->GetNumSetting("PlaybackExitPrompt");
            if (exitprompt == 1 || exitprompt == 2)
                nvp->SetBookmark();
            wantsToQuit = true;
            exitPlayer = true;
        }
        else if (message.left(6) == "SIGNAL")
        {
            int cardnum = (QStringList::split(" ", message))[1].toInt();
            QStringList signalList = me->ExtraDataList();
            bool tc = recorder && (recorder->GetRecorderNumber() == cardnum);
            if (tc && signalList.size())
            {
                UpdateOSDSignal(signalList);
            }
        }
        else if (message.left(7) == "SKIP_TO")
        {
            int cardnum = (QStringList::split(" ", message))[1].toInt();
            QStringList keyframe = me->ExtraDataList();
            VERBOSE(VB_IMPORTANT, "Got SKIP_TO message. Keyframe: "
                    <<stringToLongLong(keyframe[0]));
            bool tc = recorder && (recorder->GetRecorderNumber() == cardnum);
            (void)tc;
        }
        else if (playbackinfo && message.left(14) == "COMMFLAG_START")
        {
            message = message.simplifyWhiteSpace();
            QStringList tokens = QStringList::split(" ", message);
            QString evchanid = tokens[1];
            QDateTime evstartts = QDateTime::fromString(tokens[2], Qt::ISODate);

            if ((playbackinfo->chanid == evchanid) &&
                (playbackinfo->startts == evstartts))
            {
                QString msg = "COMMFLAG_REQUEST ";
                msg += tokens[1] + " " + tokens[2];
                RemoteSendMessage(msg);
            }
        }
        else if (playbackinfo && message.left(15) == "COMMFLAG_UPDATE")
        {
            message = message.simplifyWhiteSpace();
            QStringList tokens = QStringList::split(" ", message);
            QString evchanid = tokens[1];
            QDateTime evstartts = QDateTime::fromString(tokens[2], Qt::ISODate);

            if ((playbackinfo->chanid == evchanid) &&
                (playbackinfo->startts == evstartts))
            {
                QMap<long long, int> newMap;
                QStringList mark;
                QStringList marks = QStringList::split(",", tokens[3]);
                for (unsigned int i = 0; i < marks.size(); i++)
                {
                    mark = QStringList::split(":", marks[i]);
                    newMap[mark[0].toInt()] = mark[1].toInt();
                }

                nvp->SetCommBreakMap(newMap);
            }
        }
    }
}

/** \fn TV::BrowseStart()
 *  \brief Begins channel browsing.
 */
void TV::BrowseStart(void)
{
    if (activenvp != nvp)
        return;

    if (paused || !GetOSD())
        return;

    OSDSet *oset = GetOSD()->GetSet("browse_info");
    if (!oset)
        return;

    ClearOSD();

    browsemode = true;

    QString title, subtitle, desc, category, starttime, endtime;
    QString callsign, iconpath, channum, chanid, seriesid, programid;
    QString chanFilters, repeat, airdate, stars;

    
    GetChannelInfo(activerecorder, title, subtitle, desc, category, 
                   starttime, endtime, callsign, iconpath, channum, chanid,
                   seriesid, programid, chanFilters, repeat, airdate, stars);

    browsechannum = channum;
    browsechanid = chanid;
    browsestarttime = starttime;
    
    BrowseDispInfo(BROWSE_SAME);

    browseTimer->start(kBrowseTimeout, true);
}

/** \fn TV::BrowseEnd(bool)
 *  \brief Ends channel browsing. Changing the channel if change is true.
 *  \param change, iff true we call ChangeChannelByString(const QString&)
 */
void TV::BrowseEnd(bool change)
{
    if (!browsemode || !GetOSD())
        return;

    browseTimer->stop();

    GetOSD()->HideSet("browse_info");

    if (change)
    {
        ChangeChannelByString(browsechannum);
    }

    browsemode = false;
}

/** \fn TV::BrowseDispInfo(int)
 *  \brief Fetches browse info from backend and sends it to the OSD.
 *  \param direction BrowseDirection to get information on.
 */
void TV::BrowseDispInfo(int direction)
{
    if (!browsemode)
        BrowseStart();

    InfoMap infoMap;
    QDateTime curtime  = QDateTime::currentDateTime();
    QDateTime maxtime  = curtime.addSecs(60 * 60 * 4);
    QDateTime lasttime = QDateTime::fromString(browsestarttime, Qt::ISODate);

    if (paused || !GetOSD())
        return;

    browseTimer->changeInterval(kBrowseTimeout);

    if (lasttime < curtime)
        browsestarttime = curtime.toString(Qt::ISODate);

    if ((lasttime > maxtime) && (direction == BROWSE_RIGHT))
        return;

    infoMap["dbstarttime"] = browsestarttime;
    infoMap["channum"]     = browsechannum;
    infoMap["chanid"]      = browsechanid;
    
    GetNextProgram(activerecorder, direction, infoMap);
    
    browsechannum = infoMap["channum"];
    browsechanid  = infoMap["chanid"];

    if ((direction == BROWSE_LEFT) || (direction == BROWSE_RIGHT))
        browsestarttime = infoMap["dbstarttime"];

    QDateTime startts = QDateTime::fromString(browsestarttime, Qt::ISODate);
    ProgramInfo *program_info = ProgramInfo::GetProgramAtDateTime(
        browsechanid, startts);
    
    if (program_info)
        program_info->ToMap(infoMap);

    GetOSD()->ClearAllText("browse_info");
    GetOSD()->SetText("browse_info", infoMap, -1);

    delete program_info;
}

void TV::ToggleRecord(void)
{
    if (!browsemode)
    {
        QString title, subtitle, desc, category, starttime, endtime;
        QString callsign, iconpath, channum, chanid, seriesid, programid;
        QString outFilters, repeat, airdate, stars;

        GetChannelInfo(activerecorder, title, subtitle, desc, category, 
                       starttime, endtime, callsign, iconpath, channum,
                       chanid, seriesid, programid, outFilters, repeat,
                       airdate, stars);

        if (starttime > "")
        {
            QDateTime startts = QDateTime::fromString(starttime, Qt::ISODate);

            ProgramInfo *program_info = 
                    ProgramInfo::GetProgramAtDateTime(chanid, startts);

            if (program_info)
            {
                program_info->ToggleRecord();

                QString msg = QString("%1 \"%2\"")
                                      .arg(tr("Record")).arg(title);

                if (activenvp == nvp && GetOSD())
                    GetOSD()->SetSettingsText(msg, 3);

                delete program_info;
            }
        }
        else
        {
            VERBOSE(VB_GENERAL, "Creating a manual recording");

            QString timeformat = gContext->GetSetting("TimeFormat", "h:mm AP");
            QString channelFormat =
                    gContext->GetSetting("ChannelFormat", "<num> <sign>");

            ProgramInfo p;

            p.chanid = chanid;

            MSqlQuery query(MSqlQuery::InitCon());
            query.prepare("SELECT chanid, channum, callsign, name "
                          "FROM channel WHERE chanid=:CHANID");
            query.bindValue(":CHANID", p.chanid);

            query.exec();

            if (query.isActive() && query.numRowsAffected()) 
            {
                query.next();
                p.chanstr = query.value(1).toString();
                p.chansign = QString::fromUtf8(query.value(2).toString());
                p.channame = QString::fromUtf8(query.value(3).toString());
            }
            else
            {
                MythContext::DBError("ToggleRecord channel info", query);
                return;
            }

            p.startts = QDateTime::currentDateTime();
            QTime tmptime = p.startts.time();
            tmptime = tmptime.addSecs(-(tmptime.second()));
            p.startts.setTime(tmptime);
            p.endts = p.startts.addSecs(3600);

            p.title = p.ChannelText(channelFormat) + " - " + 
                      p.startts.toString(timeformat) +
                      " (" + tr("Manual Record") + ")";

            ScheduledRecording record;
            record.loadByProgram(&p);
            record.setSearchType(kManualSearch);
            record.setRecordingType(kSingleRecord);
            record.save();

            QString msg = QString("%1 \"%2\"")
                                  .arg(tr("Record")).arg(p.title);

            if (activenvp == nvp && GetOSD())
                GetOSD()->SetSettingsText(msg, 3);
        }
        return;
    }

    InfoMap infoMap;
    QDateTime startts = QDateTime::fromString(browsestarttime, Qt::ISODate);

    ProgramInfo *program_info = ProgramInfo::GetProgramAtDateTime(browsechanid,
                                                                  startts);
    program_info->ToggleRecord();

    program_info->ToMap(infoMap);

    if (GetOSD())
    {
        GetOSD()->ClearAllText("browse_info");
        GetOSD()->SetText("browse_info", infoMap, -1);

        if (activenvp == nvp)
            GetOSD()->SetSettingsText(tr("Record"), 3);
    }

    delete program_info;
}

void TV::BrowseChannel(QString &chan)
{
    if (!activerecorder->CheckChannel(chan))
        return;

    browsechannum = chan;
    BrowseDispInfo(BROWSE_SAME);
}

void TV::HandleOSDClosed(int osdType)
{
    switch (osdType)
    {
        case kOSDFunctionalType_RecPictureAdjust:
            recAdjustment = kPictureAttribute_None;
            break;
        case kOSDFunctionalType_PictureAdjust:
            picAdjustment = kPictureAttribute_None;
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

void TV::DoTogglePictureAttribute(void)
{
    OSDSet *oset;
    int value = 0;

    if (GetOSD())
    {
        oset = GetOSD()->GetSet("status");
        QString title = tr("Adjust Picture");
        QString picName;

        AudioOutput *aud = NULL;
        switch (picAdjustment)
        {
            case kPictureAttribute_Brightness:
                value = nvp->getVideoOutput()->GetCurrentBrightness();
                picName = QString("%1 %2 %").arg(tr("Brightness")).arg(value);
                break;
            case kPictureAttribute_Contrast:
                value = nvp->getVideoOutput()->GetCurrentContrast();
                picName = QString("%1 %2 %").arg(tr("Contrast")).arg(value);
                break;
            case kPictureAttribute_Colour:
                value = nvp->getVideoOutput()->GetCurrentColour();
                picName = QString("%1 %2 %").arg(tr("Colour")).arg(value);
                break;
            case kPictureAttribute_Hue:
                value = nvp->getVideoOutput()->GetCurrentHue();
                picName = QString("%1 %2 %").arg(tr("Hue")).arg(value);
                break;
            case kPictureAttribute_Volume:
                aud = nvp->getAudioOutput();
                value = (aud) ? (aud->GetCurrentVolume()) 
                        : 99; 
                title = tr("Adjust Volume");
                picName = QString("%1 %2 %").arg(tr("Volume")).arg(value);
                break;
        }
        GetOSD()->StartPause(value*10, true, title, picName, 5, 
                        kOSDFunctionalType_PictureAdjust);
        update_osd_pos = false;
    }
}

void TV::DoToggleRecPictureAttribute(void)
{
    OSDSet *oset;
    int value = 0;
   
    recAdjustment = (recAdjustment % 4) + 1;
   
    if (GetOSD())
    {
        oset = GetOSD()->GetSet("status");
        QString title = tr("Adjust Recording");
        QString recName;
      
        switch (recAdjustment)
        {
            case kPictureAttribute_Brightness:
                activerecorder->ChangeBrightness(true);
                value = activerecorder->ChangeBrightness(false);
                recName = QString("%1 %2 %3 %").arg(tr("Brightness"))
                                  .arg(tr("(REC)")).arg(value);
                break;
            case kPictureAttribute_Contrast:
                activerecorder->ChangeContrast(true);
                value = activerecorder->ChangeContrast(false);
                recName = QString("%1 %2 %3 %").arg(tr("Contrast"))
                                  .arg(tr("(REC)")).arg(value);
                break;
            case kPictureAttribute_Colour:
                activerecorder->ChangeColour(true);
                value = activerecorder->ChangeColour(false);
                recName = QString("%1 %2 %3 %").arg(tr("Colour"))
                                  .arg(tr("(REC)")).arg(value);
                break;
            case kPictureAttribute_Hue:
                activerecorder->ChangeHue(true);
                value = activerecorder->ChangeHue(false);
                recName = QString("%1 %2 %3 %").arg(tr("Hue"))
                                  .arg(tr("(REC)")).arg(value);
                break;
        }
        GetOSD()->StartPause(value * 10, true, title, recName, 5,
                        kOSDFunctionalType_RecPictureAdjust);
        update_osd_pos = false;
    }
}
   
void TV::DoChangePictureAttribute(int control, bool up, bool rec)
{
    switch (control)
    {
        case kPictureAttribute_Brightness:
            ChangeBrightness(up, rec);
            break;
        case kPictureAttribute_Contrast:
            ChangeContrast(up, rec);
            break;
        case kPictureAttribute_Colour:
            ChangeColour(up, rec);
            break;
        case kPictureAttribute_Hue:
            ChangeHue(up, rec);
            break;
        case kPictureAttribute_Volume:
            ChangeVolume(up);
            break;
    }
}

OSD *TV::GetOSD(void)
{
    if (nvp)
        return nvp->GetOSD();
    return NULL;
}

void TV::TreeMenuEntered(OSDListTreeType *tree, OSDGenericTree *item)
{
    // show help text somewhere, perhaps?
    (void)tree;
    (void)item;
}

void TV::DoEditMode(void)
{
    bool isEditing = playbackinfo->IsEditing();

    if (isEditing && GetOSD())
    {
        nvp->Pause();

        dialogname = "alreadybeingedited";

        QString message = tr("This program is currently being edited");

        QStringList options;
        options += tr("Continue Editing");
        options += tr("Do not edit");

        GetOSD()->NewDialogBox(dialogname, message, options, 0);
        return;
    }

    editmode = nvp->EnableEdit();
}

void TV::TreeMenuSelected(OSDListTreeType *tree, OSDGenericTree *item)
{
    if (!tree || !item)
        return;

    bool hidetree = true;

    QString action = item->getAction();

    if (action == "TOGGLECC")
        DoToggleCC(0);
    else if (action.left(6) == "DISPCC")
        DoToggleCC(action.right(1).toInt());
    else if (action.left(7) == "DISPTXT")
        DoToggleCC(action.right(1).toInt() + 4);
    else if (action == "TOGGLEMANUALZOOM")
        SetManualZoom(true);
    else if (action.left(13) == "TOGGLESTRETCH")
    {
        bool floatRead;
        float stretch = action.right(action.length() - 13).toFloat(&floatRead);
        if (floatRead &&
            stretch <= 2.0 &&
            stretch >= 0.48)
        {
            normal_speed = stretch;   // alter speed before display
        }

        ChangeTimeStretch(0, !floatRead);   // just display
    }
    else if (action.left(15) == "TOGGLEAUDIOSYNC")
        ChangeAudioSync(0);
    else if (action.left(11) == "TOGGLESLEEP")
    {
        ToggleSleepTimer(action.left(13));
    }
    else if (action.left(17) == "TOGGLEPICCONTROLS")
    {
        picAdjustment = action.right(1).toInt();
        DoTogglePictureAttribute();
    }
    else if (action.left(12) == "TOGGLEASPECT")
    {
        ToggleLetterbox(action.right(1).toInt());
        hidetree = false;
    }
    else if (action == "NEXTAUDIO")
        ChangeAudioTrack(1);
    else if (action == "PREVAUDIO")
        ChangeAudioTrack(-1);
    else if (action.left(11) == "SELECTAUDIO")
        SetAudioTrack(action.mid(11).toInt());
    else if (action == "NEXTSUBTITLE")
        ChangeSubtitleTrack(1);
    else if (action == "PREVSUBTITLE")
        ChangeSubtitleTrack(-1);
    else if (action.left(14) == "SELECTSUBTITLE")
        SetSubtitleTrack(action.mid(14).toInt());
    else if (StateIsLiveTV(GetState()))
    {
        if (action == "GUIDE")
            LoadMenu();
        else if (action == "TOGGLEPIPMODE")
            TogglePIPView();
        else if (action == "TOGGLEPIPWINDOW")
            ToggleActiveWindow();
        else if (action == "SWAPPIP")
            SwapPIP();
        else if (action == "TOGGLEBROWSE")
            BrowseStart();
        else if (action == "PREVCHAN")
            PreviousChannel();
        else
        {
            cout << "unknown menu action selected: " << action << endl;
            hidetree = false;
        }
    }
    else if (StateIsPlaying(internalState))
    {
        if (action == "TOGGLEEDIT")
            DoEditMode();
        else if (action == "TOGGLEAUTOEXPIRE")
            ToggleAutoExpire();
        else if (action.left(14) == "TOGGLECOMMSKIP")
            SetAutoCommercialSkip(action.right(1).toInt());
        else if (action == "QUEUETRANSCODE")
            DoQueueTranscode();
        else if (action == "JUMPPREV")
        {
            nvp->SetBookmark();
            exitPlayer = true;
            wantsToQuit = true;
            jumpToProgram = true;
        }
        else
        {
            cout << "unknown menu action selected: " << action << endl;
            hidetree = false;
        }
    }
    else
    {
        cout << "unknown menu action selected: " << action << endl;
        hidetree = false;
    }

    if (hidetree)
    {
        tree->SetVisible(false);
        tree->disconnect();
    }
}

void TV::ShowOSDTreeMenu(void)
{
    BuildOSDTreeMenu();

    if (GetOSD())
    {
        ClearOSD();

        OSDListTreeType *tree = GetOSD()->ShowTreeMenu("menu", treeMenu);
        if (tree)
        {
            connect(tree, SIGNAL(itemSelected(OSDListTreeType *,OSDGenericTree *)), 
                    this, SLOT(TreeMenuSelected(OSDListTreeType *, OSDGenericTree *)));

            connect(tree, SIGNAL(itemEntered(OSDListTreeType *, OSDGenericTree *)),
                    this, SLOT(TreeMenuEntered(OSDListTreeType *, OSDGenericTree *)));
        }
    }
}

void TV::BuildOSDTreeMenu(void)
{
    if (treeMenu)
        delete treeMenu;

    treeMenu = new OSDGenericTree(NULL, "treeMenu");
    OSDGenericTree *item, *subitem;

    if (StateIsLiveTV(GetState()))
    {
        bool freeRecorders = (pipnvp != NULL);
        if (!freeRecorders)
            freeRecorders = RemoteGetFreeRecorderCount();

        item = new OSDGenericTree(treeMenu, tr("Program Guide"), "GUIDE");

        if (freeRecorders)
        {
            item = new OSDGenericTree(treeMenu, tr("Picture-in-Picture"));
            subitem = new OSDGenericTree(item, tr("Enable/Disable"), 
                                         "TOGGLEPIPMODE");
            subitem = new OSDGenericTree(item, tr("Swap Channels"), "SWAPPIP");
            subitem = new OSDGenericTree(item, tr("Change Active Window"),
                                         "TOGGLEPIPWINDOW");
        }

        item = new OSDGenericTree(treeMenu, tr("Enable Browse Mode"),
                                  "TOGGLEBROWSE");

        item = new OSDGenericTree(treeMenu, tr("Previous Channel"),
                                  "PREVCHAN");
    }
    else if (StateIsPlaying(internalState))
    {
        item = new OSDGenericTree(treeMenu, tr("Edit Recording"), "TOGGLEEDIT");

        if (lastProgram != NULL)
            item = new OSDGenericTree(treeMenu, tr("Previous Recording"), "JUMPPREV");

        if (JobQueue::IsJobQueuedOrRunning(JOB_TRANSCODE,
                                   playbackinfo->chanid, playbackinfo->startts))
            item = new OSDGenericTree(treeMenu, tr("Stop Transcoding"), "QUEUETRANSCODE");
        else
            item = new OSDGenericTree(treeMenu, tr("Begin Transcoding"), "QUEUETRANSCODE");

        item = new OSDGenericTree(treeMenu, tr("Commercial Auto-Skip"));
        subitem = new OSDGenericTree(item, tr("Auto-Skip OFF"),
                                     "TOGGLECOMMSKIP0",
                                     (autoCommercialSkip == 0) ? 1 : 0, NULL,
                                     "COMMSKIPGROUP");
        subitem = new OSDGenericTree(item, tr("Auto-Skip Notify"),
                                     "TOGGLECOMMSKIP2",
                                     (autoCommercialSkip == 2) ? 1 : 0, NULL,
                                     "COMMSKIPGROUP");
        subitem = new OSDGenericTree(item, tr("Auto-Skip ON"),
                                     "TOGGLECOMMSKIP1",
                                     (autoCommercialSkip == 1) ? 1 : 0, NULL,
                                     "COMMSKIPGROUP");

        if (playbackinfo->GetAutoExpireFromRecorded())
            item = new OSDGenericTree(treeMenu, tr("Turn Auto-Expire OFF"),
                                      "TOGGLEAUTOEXPIRE");
        else
            item = new OSDGenericTree(treeMenu, tr("Turn Auto-Expire ON"),
                                      "TOGGLEAUTOEXPIRE");
    }
    
    item = new OSDGenericTree(treeMenu, tr("Select Audio Track"), "NEXTAUDIO");

    const QStringList atracks = activenvp->listAudioTracks();
    int atrack = activenvp->getCurrentAudioTrack();
    int i = 1;

    for (QStringList::const_iterator it = atracks.begin(); it != atracks.end(); it++)
    {
        subitem = new OSDGenericTree(item, *it, "SELECTAUDIO" + QString("%1").arg(i),
                                     (i == atrack) ? 1 : 0, NULL, "AUDIOGROUP");
        i++;
    }

    const QStringList stracks = activenvp->listSubtitleTracks();
    
    if (stracks.size() > 0)
    {
        item = new OSDGenericTree(treeMenu, tr("Select Subtitles"),
                                      "TOGGLECC");
        int strack = activenvp->getCurrentSubtitleTrack();
        int i = 1;

        subitem = new OSDGenericTree(item, "None", "SELECTSUBTITLE" + QString("%1").arg(0),
                                     (0 == strack) ? 1 : 0, NULL, "SUBTITLEGROUP");

        for (QStringList::const_iterator it = stracks.begin(); it != stracks.end(); it++)
        {
            subitem = new OSDGenericTree(item, *it, "SELECTSUBTITLE" + QString("%1").arg(i),
                                         (i == strack) ? 1 : 0, NULL, "SUBTITLEGROUP");
            i++;
        }
    } else {
        if (vbimode == 1)
            item = new OSDGenericTree(treeMenu, tr("Toggle Teletext"),
                                      "TOGGLECC");
        else if (vbimode == 2)
        {
            item = new OSDGenericTree(treeMenu, tr("Closed Captioning"));
            subitem = new OSDGenericTree(item, tr("Toggle CC"), "TOGGLECC");
            for (int i = 1; i <= 4; i++)
                subitem = new OSDGenericTree(item, QString("%1%2").arg(tr("CC")).arg(i),
                                             QString("DISPCC%1").arg(i));
            for (int i = 1; i <= 4; i++)
                subitem = new OSDGenericTree(item, QString("%1%2").arg(tr("TXT")).arg(i),
                                             QString("DISPTXT%1").arg(i));
        }
    }

    int letterbox = nvp->GetLetterbox();
    item = new OSDGenericTree(treeMenu, tr("Change Aspect Ratio"));
    subitem = new OSDGenericTree(item, tr("4:3"), "TOGGLEASPECT" +
                                 QString("%1").arg(kLetterbox_4_3),
                                 (letterbox == kLetterbox_4_3) ? 1 : 0,
                                 NULL, "ASPECTGROUP");
    subitem = new OSDGenericTree(item, tr("16:9"), "TOGGLEASPECT" +
                                 QString("%1").arg(kLetterbox_16_9),
                                 (letterbox == kLetterbox_16_9) ? 1 : 0,
                                 NULL, "ASPECTGROUP");
    subitem = new OSDGenericTree(item, tr("4:3 Zoom"), "TOGGLEASPECT" +
                                 QString("%1").arg(kLetterbox_4_3_Zoom),
                                 (letterbox == kLetterbox_4_3_Zoom) ? 1 : 0,
                                 NULL, "ASPECTGROUP");
    subitem = new OSDGenericTree(item, tr("16:9 Zoom"), "TOGGLEASPECT" +
                                 QString("%1").arg(kLetterbox_16_9_Zoom),
                                 (letterbox == kLetterbox_16_9_Zoom) ? 1 : 0,
                                 NULL, "ASPECTGROUP");
    subitem = new OSDGenericTree(item, tr("16:9 Stretch"), "TOGGLEASPECT" +
                                 QString("%1").arg(kLetterbox_16_9_Stretch),
                                 (letterbox == kLetterbox_16_9_Stretch) ? 1 : 0,
                                 NULL, "ASPECTGROUP");

    if (usePicControls)
    {
        item = new OSDGenericTree(treeMenu, tr("Adjust Picture"));
        subitem = new OSDGenericTree(item, tr("Brightness"),
                                     "TOGGLEPICCONTROLS" +
                                     QString("%1")
                                           .arg(kPictureAttribute_Brightness));
        subitem = new OSDGenericTree(item, tr("Contrast"),
                                     "TOGGLEPICCONTROLS" +
                                     QString("%1")
                                           .arg(kPictureAttribute_Contrast));
        subitem = new OSDGenericTree(item, tr("Colour"),
                                     "TOGGLEPICCONTROLS" +
                                         QString("%1")
                                           .arg(kPictureAttribute_Colour));
        subitem = new OSDGenericTree(item, tr("Hue"),
                                     "TOGGLEPICCONTROLS" +
                                         QString("%1")
                                           .arg(kPictureAttribute_Hue));
    }

    item = new OSDGenericTree(treeMenu, tr("Manual Zoom Mode"), 
                             "TOGGLEMANUALZOOM");

    item = new OSDGenericTree(treeMenu, tr("Adjust Audio Sync"), "TOGGLEAUDIOSYNC");

    int speedX100 = (int)(round(normal_speed * 100));

    item = new OSDGenericTree(treeMenu, tr("Adjust Time Stretch"), "TOGGLESTRETCH");
    subitem = new OSDGenericTree(item, tr("Adjust"), "TOGGLESTRETCH");
    subitem = new OSDGenericTree(item, tr("0.5X"), "TOGGLESTRETCH0.5",
                                 (speedX100 == 50) ? 1 : 0, NULL,
                                 "STRETCHGROUP");
    subitem = new OSDGenericTree(item, tr("0.9X"), "TOGGLESTRETCH0.9",
                                 (speedX100 == 90) ? 1 : 0, NULL,
                                 "STRETCHGROUP");
    subitem = new OSDGenericTree(item, tr("1.0X"), "TOGGLESTRETCH1.0",
                                 (speedX100 == 100) ? 1 : 0, NULL,
                                 "STRETCHGROUP");
    subitem = new OSDGenericTree(item, tr("1.1X"), "TOGGLESTRETCH1.1",
                                 (speedX100 == 110) ? 1 : 0, NULL,
                                 "STRETCHGROUP");
    subitem = new OSDGenericTree(item, tr("1.2X"), "TOGGLESTRETCH1.2",
                                 (speedX100 == 120) ? 1 : 0, NULL,
                                 "STRETCHGROUP");
    subitem = new OSDGenericTree(item, tr("1.3X"), "TOGGLESTRETCH1.3",
                                 (speedX100 == 130) ? 1 : 0, NULL,
                                 "STRETCHGROUP");
    subitem = new OSDGenericTree(item, tr("1.4X"), "TOGGLESTRETCH1.4",
                                 (speedX100 == 140) ? 1 : 0, NULL,
                                 "STRETCHGROUP");
    subitem = new OSDGenericTree(item, tr("1.5X"), "TOGGLESTRETCH1.5",
                                 (speedX100 == 150) ? 1 : 0, NULL,
                                 "STRETCHGROUP");

    // add sleep items to menu

    item = new OSDGenericTree(treeMenu, tr("Sleep"), "TOGGLESLEEPON");
    if (sleepTimer->isActive())
        subitem = new OSDGenericTree(item, tr("Sleep Off"), "TOGGLESLEEPON");
    subitem = new OSDGenericTree(item, "30 " + tr("minutes"), "TOGGLESLEEP30");
    subitem = new OSDGenericTree(item, "60 " + tr("minutes"), "TOGGLESLEEP60");
    subitem = new OSDGenericTree(item, "90 " + tr("minutes"), "TOGGLESLEEP90");
    subitem = new OSDGenericTree(item, "120 " + tr("minutes"), "TOGGLESLEEP120");
}

void TV::ChangeAudioTrack(int dir)
{
    if (activenvp)
    {
        if (dir > 0)
            activenvp->incCurrentAudioTrack(); 
        else
            activenvp->decCurrentAudioTrack();

        if (activenvp->getCurrentAudioTrack())
        {
            QString msg = QString("%1 %2")
                          .arg(tr("Audio track"))
                          .arg(activenvp->getCurrentAudioTrack());

            if (GetOSD())
                GetOSD()->SetSettingsText(msg, 3);
        }
    }
}

void TV::SetAudioTrack(int track)
{
    if (activenvp)
    {
        activenvp->setCurrentAudioTrack(track - 1); 

        if (activenvp->getCurrentAudioTrack())
        {
            QString msg = QString("%1 %2")
                          .arg(tr("Audio track"))
                          .arg(activenvp->getCurrentAudioTrack());

            if (GetOSD())
                GetOSD()->SetSettingsText(msg, 3);
        }
    }
}

void TV::ChangeSubtitleTrack(int dir)
{
    if (activenvp)
    {
        if (dir > 0)
            activenvp->incCurrentSubtitleTrack(); 
        else
            activenvp->decCurrentSubtitleTrack();

        if (activenvp->getCurrentSubtitleTrack())
        {
            QString msg = QString("%1 %2")
                          .arg(tr("Subtitle track"))
                          .arg(activenvp->getCurrentSubtitleTrack());

            if (GetOSD())
                GetOSD()->SetSettingsText(msg, 3);
        }
    }
}

void TV::SetSubtitleTrack(int track)
{
    if (activenvp)
    {
        activenvp->setCurrentSubtitleTrack(track - 1); 

        if (activenvp->getCurrentSubtitleTrack())
        {
            QString msg = QString("%1 %2")
                          .arg(tr("Subtitle track"))
                          .arg(activenvp->getCurrentSubtitleTrack());

            if (GetOSD())
                GetOSD()->SetSettingsText(msg, 3);
        }
    }
}

void TV::ToggleAutoExpire(void)
{
    QString desc = "";

    if (playbackinfo->GetAutoExpireFromRecorded())
    {
        playbackinfo->SetAutoExpire(false);
        desc = tr("Auto-Expire OFF");
    }
    else
    {
        playbackinfo->SetAutoExpire(true);
        desc = tr("Auto-Expire ON");
    }

    if (GetOSD() && activenvp == nvp && desc != "" )
    {
        QString curTime = "";
        int pos = nvp->calcSliderPos(curTime);
        GetOSD()->StartPause(pos, false, desc, curTime, 1);
        update_osd_pos = false;
    }
}

void TV::SetAutoCommercialSkip(int skipMode)
{
    QString desc = "";

    autoCommercialSkip = skipMode;

    if (autoCommercialSkip == 0)
        desc = tr("Auto-Skip OFF");
    else if (autoCommercialSkip == 1)
        desc = tr("Auto-Skip ON");
    else if (autoCommercialSkip == 2)
        desc = tr("Auto-Skip Notify");

    nvp->SetAutoCommercialSkip(autoCommercialSkip);

    if (GetOSD() && activenvp == nvp && desc != "" )
    {
        QString curTime = "";
        int pos = nvp->calcSliderPos(curTime);
        GetOSD()->StartPause(pos, false, desc, curTime, 1);
        update_osd_pos = false;
    }
}

void TV::SetManualZoom(bool zoomON)
{
    QString desc = "";

    zoomMode = zoomON;
    if (zoomON)
    {
        ClearOSD();
        desc = tr("Zoom Mode ON");
    }
    else
        desc = tr("Zoom Mode OFF");

    if (GetOSD() && activenvp == nvp && desc != "" )
    {
        QString curTime = "";
        int pos = nvp->calcSliderPos(curTime);
        GetOSD()->StartPause(pos, false, desc, curTime, 1);
        update_osd_pos = false;
    }
}

void TV::ToggleSleepTimer(const QString time)
{
    const int minute(60*1000);
    int mins(0);

    if (sleepTimer)
    {
        if (time == "TOGGLESLEEPON")
        {
            if (sleepTimer->isActive())
                sleepTimer->stop();
            else
            {
                mins = 60;
                sleepTimer->start(mins *minute);
            }
        }
        else
        {
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
                    cout << "Invalid time " << time << endl;
                }
            }
            else
            {
                cout << "Invalid time string " << time << endl;
            }

            if (sleepTimer->isActive())
            {
                sleepTimer->stop();
            }

            if (mins)
                sleepTimer->start(mins * minute);
        }
               
        // display OSD
        if (GetOSD() && !browsemode)
        {
            QString out;

            if (mins != 0)
                out = tr("Sleep") + " " + QString::number(mins);
            else
                out = tr("Sleep") + " " + sleep_times[0].dispString;

            GetOSD()->SetSettingsText(out, 3);
        }
    }
    else
    {
        cout << "No sleep timer?";
    }
}

void TV::GetValidRecorderList (const QString & chanid, QStringList & reclist)
{
    reclist.clear();
    // Query the database to determine which source is being used currently.
    // set the EPG so that it only displays the channels of the current source
    MSqlQuery query(MSqlQuery::InitCon());
    // We want to get the current source id for this recorder
    QString queryChanid = "SELECT I.cardid FROM channel C LEFT JOIN "
                          "cardinput I ON C.sourceid = I.sourceid "
                          "WHERE C.chanid = " + chanid + ";";
    query.prepare(queryChanid);
    query.exec();
    if (query.isActive() && query.size() > 0) {
        while (query.next ()) {
            reclist << query.value(0).toString();
        }
    }
}

void TV::ShowNoRecorderDialog(void)
{
    QString errorText = tr("MythTV is already using all available "
                           "inputs for the channel you selected. "
                           "If you want to watch an in-progress recording, "
                           "select one from the playback menu.  If you "
                           "want to watch live TV, cancel one of the "
                           "in-progress recordings from the delete "
                           "menu.");

    MythPopupBox::showOkPopup(
            gContext->GetMainWindow(), QObject::tr("Channel Change Error"),
            errorText);
}

/** \fn TV::PauseLiveTV()
 *  \brief Used in ChangeChannel(), ChangeChannelByString(),
 *         and ToggleInputs() to temporarily stop video output.
 */
void TV::PauseLiveTV(void)
{
    VERBOSE(VB_PLAYBACK, "PauseLiveTV()");
    lockTimerOn = false;

    if (activenvp)
        activenvp->Pause(false);

    if (activerbuffer)
        activerbuffer->WaitForPause();

    activerecorder->PauseRecorder();
    if (activerbuffer)
        activerbuffer->Reset();

    osdlock.lock();
    lastSignalMsg.clear();
    lastSignalUIInfo.clear();
    osdlock.unlock();

    lockTimerOn = false;
    if (lockTimeout < 0xffffffff)
    {
        lockTimer.start();
        lockTimerOn = true;
    }
}

/** \fn TV::UnpauseLiveTV()
 *  \brief Used in ChangeChannel(), ChangeChannelByString(),
 *         and ToggleInputs() to restart video output.
 */
void TV::UnpauseLiveTV(void)
{
    VERBOSE(VB_PLAYBACK, "UnpauseLiveTV()");

    if (activenvp)
    {
        activenvp->ResetPlaying();
        if (activenvp->IsErrored())
        {
            VERBOSE(VB_IMPORTANT,
                    "TVPlay::UnpauseLiveTV(): Unable to reset playing.");
            wantsToQuit = false;
            exitPlayer = true;
            return;
        }
        QString filters = GetFiltersForChannel();
        activenvp->SetVideoFilters(filters);
        activenvp->Play(normal_speed, true, false);
    }

    if (!nvp || (nvp && activenvp == nvp))
    {
        UpdateOSD();
        UpdateLCD();
        AddPreviousChannel();
    }
}
