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
#include "livetvchain.h"
#include "playgroup.h"

#ifndef HAVE_ROUND
#define round(x) ((int) ((x) + 0.5))
#endif

const int TV::kInitFFRWSpeed  = 0;
const int TV::kMuteTimeout    = 800;
const int TV::kLCDTimeout     = 30000;
const int TV::kBrowseTimeout  = 30000;
const int TV::kSMExitTimeout  = 2000;
const int TV::kInputKeysMax   = 6;
const int TV::kInputModeTimeout=5000;

#define DEBUG_ACTIONS 0 /**< set to 1 to debug actions */
#define LOC QString("TV: ")
#define LOC_ERR QString("TV Error: ")

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
    REG_KEY("TV Playback", "ARBSEEK", "Arbitrary Seek", "*");
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
/*
  keys already used:

  Global:           I   M              0123456789
  Playback: ABCDEFGH JK  NOPQRSTUVWXYZ
  Frontend:    D          OP R  U  X   01 3   7 9
  Editing:    C E   I       Q        Z

  Playback: <>,.?/|[]{}\+-*
  Frontend: <>,.?/
  Editing:  <>,.

  Global:   PgDown, PgUp,  Right, Left, Home, End, Up, Down, 
  Playback: PgDown, PgUp,  Right, Left, Home, End, Up, Down, Backspace,
  Frontend:                Right, Left, Home, End
  Editing:  PgDown, PgUp,               Home, End

  Global:   Return, Enter, Space, Esc

  Global:          F1,
  Playback: Ctrl-B,   F7,F8,F9,F10,F11
 */
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
      MuteIndividualChannels(false), arrowAccel(false),
      osd_general_timeout(2), osd_prog_info_timeout(3),
      autoCommercialSkip(false), tryUnflaggedSkip(false),
      smartForward(false), stickykeys(0),
      ff_rew_repos(1.0f), ff_rew_reverse(false),
      vbimode(0),
      // State variables
      internalState(kState_None),
      menurunning(false), runMainLoop(false), wantsToQuit(true),
      exitPlayer(false), paused(false), errored(false),
      stretchAdjustment(false),
      audiosyncAdjustment(false), audiosyncBaseline(LONG_LONG_MIN),
      editmode(false), zoomMode(false), sigMonMode(false),
      update_osd_pos(false), endOfRecording(false), requestDelete(false),
      doSmartForward(false),
      queuedTranscode(false), getRecorderPlaybackInfo(false),
      picAdjustment(kPictureAttribute_None),
      recAdjustment(kPictureAttribute_None),
      ignoreKeys(false),
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
      // CC/Teletex input state variables
      ccInputMode(false), ccInputModeExpires(QTime::currentTime()),
      // Arbritary seek input state variables
      asInputMode(false), asInputModeExpires(QTime::currentTime()),
      // Channel changing state variables
      queuedChanNum(""),
      muteTimer(new QTimer(this)),
      lockTimerOn(false),
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
      switchToRec(NULL),
      // LiveTVChain
      tvchain(NULL), piptvchain(NULL),
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
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Init(): Could not open DB connection in player");
        errored = true;
        return false;
    }

    baseFilters         += gContext->GetSetting("CustomFilters");
    jumptime             = gContext->GetNumSetting("JumpAmount", 10);
    usePicControls       = gContext->GetNumSetting("UseOutputPictureControls",0);
    smartChannelChange   = gContext->GetNumSetting("SmartChannelChange", 0);
    MuteIndividualChannels=gContext->GetNumSetting("IndividualMuteControl", 0);
    arrowAccel           = gContext->GetNumSetting("UseArrowAccels", 1);
    persistentbrowsemode = gContext->GetNumSetting("PersistentBrowseMode", 0);
    osd_general_timeout  = gContext->GetNumSetting("OSDGeneralTimeout", 2);
    osd_prog_info_timeout= gContext->GetNumSetting("OSDProgramInfoTimeout", 3);
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

    QString vbiformat = gContext->GetSetting("VbiFormat");
    vbimode = (vbiformat.lower() == "pal teletext") ? 1 : 0;
    vbimode = (vbiformat.lower().left(4) == "ntsc") ? 2 : vbimode;

    if (createWindow)
    {
        MythMainWindow *mainWindow = gContext->GetMainWindow();
        bool fullscreen = !gContext->GetNumSetting("GuiSizeForTV", 0);
        bool switchMode = gContext->GetNumSetting("UseVideoModes", 0);

        saved_gui_bounds = QRect(mainWindow->geometry().topLeft(),
                                 mainWindow->size());

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

        player_bounds = saved_gui_bounds;
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
        if (!gContext->GetNumSetting("GuiSizeForTV", 0))
            mwnd->move(saved_gui_bounds.topLeft());
    }
    if (recorderPlaybackInfo)
        delete recorderPlaybackInfo;

    if (treeMenu)
        delete treeMenu;

    if (playbackinfo)
        delete playbackinfo;

    if (class LCD * lcd = LCD::Get())
        lcd->switchToTime();
}

TVState TV::GetState(void) const
{
    if (InStateChange())
        return kState_ChangingState;
    return internalState;
}

int TV::LiveTV(LiveTVChain *chain, bool showDialogs)
{
    if (internalState == kState_None && RequestNextRecorder(showDialogs))
    {
        tvchain = chain;
        ChangeState(kState_WatchingLiveTV);
        switchToRec = NULL;

        fftime       = PlayGroup::GetSetting("Default", "skipahead", 30);
        rewtime      = PlayGroup::GetSetting("Default", "skipback", 5);
        normal_speed = PlayGroup::GetSetting("Default", "timestretch", 
                                                 100) / 100.0;

        return 1;
    }
    return 0;
}

int TV::GetLastRecorderNum(void) const
{
    if (!recorder)
        return -1;
    return recorder->GetRecorderNumber();
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
    playbackinfo = new ProgramInfo(*rcinfo);

    int overrecordseconds = gContext->GetNumSetting("RecordOverTime");
    QDateTime curtime = QDateTime::currentDateTime();
    QDateTime recendts = rcinfo->recendts.addSecs(overrecordseconds);

    if (curtime < recendts && !rcinfo->isVideo)
        ChangeState(kState_WatchingRecording);
    else
        ChangeState(kState_WatchingPreRecorded);

    fftime = PlayGroup::GetSetting(playbackinfo->playgroup, "skipahead", 30);
    rewtime = PlayGroup::GetSetting(playbackinfo->playgroup, "skipback", 5);
    normal_speed = PlayGroup::GetSetting(playbackinfo->playgroup, 
                                         "timestretch", 100) / 100.0;

    if (class LCD * lcd = LCD::Get())
        lcd->switchToChannel(rcinfo->chansign, rcinfo->title, rcinfo->subtitle);

    return 1;
}

int TV::PlayFromRecorder(int recordernum)
{
    int retval = 0;

    if (recorder)
    {
        VERBOSE(VB_IMPORTANT, LOC +
                QString("PlayFromRecorder(%1): Recorder already exists!")
                .arg(recordernum));
        return -1;
    }

    activerecorder = recorder = RemoteGetExistingRecorder(recordernum);
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
        VERBOSE(VB_IMPORTANT, LOC_ERR + "HandleStateChange(): "
                "Called after fatal error detected.");
        return;
    }

    bool changed = false;

    stateLock.lock();
    TVState nextState = internalState;
    if (!nextStates.size())
    {
        VERBOSE(VB_IMPORTANT, LOC + "HandleStateChange() Warning, "
                "called with no state to change to.");
        stateLock.unlock();
        return;
    }
    TVState desiredNextState = nextStates.dequeue();    
    VERBOSE(VB_GENERAL, LOC + QString("Attempting to change from %1 to %2")
            .arg(StateToString(nextState))
            .arg(StateToString(desiredNextState)));

    if (desiredNextState == kState_Error)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "HandleStateChange(): "
                "Attempting to set to an error state!");
        errored = true;
        stateLock.unlock();
        return;
    }

    if (TRANSITION(kState_None, kState_WatchingLiveTV))
    {
        QString name = "";

        lastSignalUIInfo.clear();
        activerecorder = recorder;
        recorder->Setup();
        
        lockTimerOn = false;

        SET_NEXT();
        recorder->SpawnLiveTV(tvchain->GetID(), false);

        tvchain->ReloadAll();

        playbackinfo = tvchain->GetProgramAt(-1);
        if (!playbackinfo)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "LiveTV not successfully started");
            gContext->RestoreScreensaver();
            DeleteRecorder();

            SET_LAST();
        }
        else
        {
            tvchain->SetProgram(playbackinfo);

            prbuffer = new RingBuffer(playbackinfo->pathname, false);
            prbuffer->SetLiveMode(tvchain);
        }

        gContext->DisableScreensaver();

        bool ok = false;
        if (playbackinfo && StartRecorder())
        {
            if (StartPlayer(false))
                ok = true;
            else
                StopStuff(true, true, true);
        }
        if (!ok)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "LiveTV not successfully started");
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
    else if (TRANSITION(kState_WatchingLiveTV, kState_None))
    {
        SET_NEXT();

        StopStuff(true, true, true);
        pbinfoLock.lock();
        if (playbackinfo)
            delete playbackinfo;
        playbackinfo = NULL;
        pbinfoLock.unlock();

        gContext->RestoreScreensaver();
    }
    else if (TRANSITION(kState_WatchingRecording, kState_WatchingPreRecorded))
    {
        SET_NEXT();
    }
    else if (TRANSITION(kState_None, kState_WatchingPreRecorded) ||
             TRANSITION(kState_None, kState_WatchingRecording))
    {
        prbuffer = new RingBuffer(inputFilename, false);
        if (prbuffer->IsOpen())
        {
            gContext->DisableScreensaver();
    
            if (desiredNextState == kState_WatchingRecording)
            {
                activerecorder = recorder =
                    RemoteGetExistingRecorder(playbackinfo);
                if (!recorder || !recorder->IsValidRecorder())
                {
                    VERBOSE(VB_IMPORTANT, LOC_ERR + "Couldn't find "
                            "recorder for in-progress recording");
                    desiredNextState = kState_WatchingPreRecorded;
                    DeleteRecorder();
                }
                else
                {
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
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Decoder not alive, and trying to play..");
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
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Unknown state transition: %1 to %2")
                .arg(StateToString(internalState))
                .arg(StateToString(desiredNextState)));
    }
    else if (internalState != nextState)
    {
        VERBOSE(VB_GENERAL, LOC + QString("Changing from %1 to %2")
                .arg(StateToString(internalState))
                .arg(StateToString(nextState)));
    }

    // update internal state variable
    TVState lastState = internalState;
    internalState = nextState;
    stateLock.unlock();

    if (StateIsLiveTV(internalState))
    {
        UpdateOSDInput();
        UpdateLCD();
    }
    else if (StateIsPlaying(internalState) && lastState == kState_None)
    {
        if (GetOSD() && (PlayGroup::GetCount() > 0))
            GetOSD()->SetSettingsText(tr("%1 Settings")
                                      .arg(playbackinfo->playgroup), 3);
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

uint TV::GetLockTimeout(uint cardid)
{
    uint timeout = 0xffffffff;
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT channel_timeout, cardtype "
                  "FROM capturecard "
                  "WHERE cardid = :CARDID");
    query.bindValue(":CARDID", cardid);
    if (!query.exec() || !query.isActive())
        MythContext::DBError("Getting timeout", query);
    else if (query.next() &&
             SignalMonitor::IsSupported(query.value(1).toString()))
        timeout = max(query.value(0).toInt(), 500);

    VERBOSE(VB_PLAYBACK, LOC + QString("GetLockTimeout(%1): "
                                       "Set lock timeout to %2 ms")
            .arg(cardid).arg(timeout));

    return timeout;
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
            VERBOSE(VB_IMPORTANT, LOC_ERR + "StartRecorder() -- "
                    "timed out waiting for recorder to start");
        return false;
    }

    VERBOSE(VB_PLAYBACK, LOC + "StartRecorder(): took "<<t.elapsed()
            <<" ms to start recorder.");

    // Get timeout for this recorder
    lockTimeout[recorder->GetRecorderNumber()] =
        GetLockTimeout(recorder->GetRecorderNumber());

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

    VERBOSE(VB_PLAYBACK, LOC + "StartPlayer(): took "<<t.elapsed()
            <<" ms to start player.");

    if (nvp->IsPlaying())
    {
        activenvp = nvp;
        activerbuffer = prbuffer;
        StartOSD();
        return true;
    }
    VERBOSE(VB_IMPORTANT, LOC_ERR +
            QString("StartPlayer(): NVP is not playing after %1 msec")
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
    VERBOSE(VB_PLAYBACK, LOC + "StopStuff() -- begin");
    if (stopRingBuffers)
    {
        VERBOSE(VB_PLAYBACK, LOC + "StopStuff(): stopping ring buffer[s]");
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
        VERBOSE(VB_PLAYBACK, LOC + "StopStuff(): stopping player[s] (1/2)");
        if (nvp)
            nvp->StopPlaying();

        if (pipnvp)
            pipnvp->StopPlaying();
    }

    if (stopRecorders)
    {
        VERBOSE(VB_PLAYBACK, LOC + "StopStuff(): stopping recorder[s]");
        if (recorder)
            recorder->StopLiveTV();

        if (piprecorder)
            piprecorder->StopLiveTV();
    }

    if (stopPlayers)
    {
        VERBOSE(VB_PLAYBACK, LOC + "StopStuff(): stopping player[s] (2/2)");
        if (nvp)
            TeardownPlayer();

        if (pipnvp)
            TeardownPipPlayer();
    }
    VERBOSE(VB_PLAYBACK, LOC + "StopStuff() -- end");
}

void TV::SetupPlayer(bool isWatchingRecording)
{
    if (nvp)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Attempting to setup a player, but it already exists.");
        return;
    }

    QString filters = "";
    
    
    nvp = new NuppelVideoPlayer("player", playbackinfo);
    nvp->SetParentWidget(myWindow);
    nvp->SetParentPlayer(this);
    nvp->SetRingBuffer(prbuffer);
    nvp->SetRecorder(recorder);
    nvp->SetAudioSampleRate(gContext->GetNumSetting("AudioSampleRate"));
    nvp->SetAudioDevice(gContext->GetSetting("AudioOutputDevice"));
    nvp->SetLength(playbackLen);
    nvp->SetExactSeeks(gContext->GetNumSetting("ExactSeeking"));
    nvp->SetAutoCommercialSkip(autoCommercialSkip);
    nvp->SetLiveTVChain(tvchain);

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

    pbinfoLock.lock();    
    if (playbackinfo) // Recordings have this info already.
        chanFilters = playbackinfo->chanOutputFilters;
    pbinfoLock.unlock();    

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
    
    VERBOSE(VB_CHANNEL, LOC +
            QString("Output filters for this channel are: '%1'").arg(filters));
    return filters;
}

void TV::SetupPipPlayer(void)
{
    if (pipnvp)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Attempting to setup a PiP player, but it already exists.");
        return;
    }

    pipnvp = new NuppelVideoPlayer("PIP player");
    pipnvp->SetAsPIP();
    pipnvp->SetRingBuffer(piprbuffer);
    pipnvp->SetRecorder(piprecorder);
    pipnvp->SetAudioSampleRate(gContext->GetNumSetting("AudioSampleRate"));
    pipnvp->SetAudioDevice(gContext->GetSetting("AudioOutputDevice"));
    pipnvp->SetExactSeeks(gContext->GetNumSetting("ExactSeeking"));
    pipnvp->SetLiveTVChain(piptvchain);

    pipnvp->SetLength(playbackLen);
}

void TV::TeardownPlayer(void)
{
    if (nvp)
    {
        QMutexLocker locker(&osdlock); // prevent UpdateOSDSignal using osd...
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

    pbinfoLock.lock();
    if (playbackinfo)
        delete playbackinfo;
    playbackinfo = NULL;
    pbinfoLock.unlock(); 

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

    if (activerecorder == piprecorder)
        activerecorder = recorder;

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
    
    ClearInputQueues();

    switchToRec = NULL;
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
                VERBOSE(VB_IMPORTANT, LOC + "Waiting for Valgrind...");
                sleep(1);
            }
#endif // USING_VALGRIND

            if (!nvp->IsPlaying())
            {
                ChangeState(RemovePlaying(internalState));
                endOfRecording = true;
                VERBOSE(VB_PLAYBACK, LOC_ERR + "nvp->IsPlaying() timed out");
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
            UpdateOSDSeekMessage(PlayMesg(), osd_general_timeout);
        }

        if (activenvp && (activenvp->GetNextPlaySpeed() != normal_speed) &&
            activenvp->AtNormalSpeed() &&
            !activenvp->PlayingSlowForPrebuffer())
        {
            // got changed in nvp due to close to end of file
            normal_speed = 1.0f;
            UpdateOSDSeekMessage(PlayMesg(), osd_general_timeout);
        }

        if (++updatecheck >= 100)
        {
            OSDSet *oset;
            if (GetOSD() && (oset = GetOSD()->GetSet("status")) &&
                oset->Displaying() && update_osd_pos &&
                (StateIsLiveTV(internalState) ||
                 StateIsPlaying(internalState)))
            {
                struct StatusPosInfo posInfo;
                nvp->calcSliderPos(posInfo);
                GetOSD()->UpdateStatus(posInfo);
            }

            updatecheck = 0;
        }

        // Commit input when the OSD fades away
        if (HasQueuedChannel() && GetOSD())
        {
            OSDSet *set = GetOSD()->GetSet("channel_number");
            if ((set && !set->Displaying()) || !set)
                CommitQueuedInput();
        }
        // Clear closed caption input mode when timer expires
        if (ccInputMode && (ccInputModeExpires < QTime::currentTime()))
        {
            ccInputMode = false;
            ClearInputQueues(true);
        }
        // Clear arbitrary seek input mode when timer expires
        if (asInputMode && (asInputModeExpires < QTime::currentTime()))
        {
            asInputMode = false;
            ClearInputQueues(true);
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
                struct StatusPosInfo posInfo;
                nvp->calcSliderPos(posInfo);
                progress = (float)posInfo.position / 1000;
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
    VERBOSE(VB_IMPORTANT, LOC + "ProcessKeypress()");
#endif // DEBUG_ACTIONS

    bool was_doing_ff_rew = false;
    bool redisplayBrowseInfo = false;

    if (ignoreKeys)
        return;

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

    // If text is already queued up, be more lax on what is ok.
    // This allows hex teletext entry and minor channel entry.
    const QString txt = e->text();
    if (HasQueuedInput() && (1 == txt.length()))
    {
        bool ok = false;
        txt.toInt(&ok, 16);
        if (ok || txt=="_" || txt=="-" || txt=="#" || txt==".")
        {
            AddKeyToInputQueue(txt.at(0).latin1());
            return;
        }
    }

    QStringList actions;
    if (!gContext->GetMainWindow()->TranslateKeyPress(
            "TV Playback", e, actions))
    {
        return;
    }

    bool handled = false;

    if (browsemode)
    {
        int passThru = 0;

        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
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
                AddKeyToInputQueue('0' + action.toInt());
            }
            else if (action == "TOGGLEBROWSE" || action == "ESCAPE" ||
                     action == "CLEAROSD")
            {
                CommitQueuedInput(); 
                BrowseEnd(false);
            }
            else if (action == "SELECT")
            {
                CommitQueuedInput(); 
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
            QString action = actions[i];
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
            QString action = actions[i];
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
            QString action = actions[i];
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
            QString action = actions[i];
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
            QString action = actions[i];
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
            QString action = actions[i];
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
        VERBOSE(VB_IMPORTANT, LOC + QString("handled(%1) actions[%2](%3)")
                .arg(handled).arg(i).arg(actions[i]));
#endif // DEBUG_ACTIONS

    if (handled)
        return;

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "TOGGLECC" && !browsemode)
        {
            if (ccInputMode)
            {
                bool valid = false;
                int page = GetQueuedInputAsInt(&valid, 16) << 16;
                page = (valid) ? page : 0;
                DoToggleCC(page);
                ccInputModeExpires.start(); // expire ccInputMode now...
                ClearInputQueues(true);
            }
            else
            {
                ccInputMode        = true;
                ccInputModeExpires = QTime::currentTime()
                    .addMSecs(kInputModeTimeout);
                asInputModeExpires = QTime::currentTime();
                ClearInputQueues(false);
                AddKeyToInputQueue(0);
            }
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
        else if (action == "ARBSEEK")
        {
            if (asInputMode)
            {
                asInputModeExpires.start();
                ClearInputQueues(true);
                UpdateOSDTextEntry(tr("Seek:"));
            }
            else
            {
                asInputMode        = true;
                asInputModeExpires = QTime::currentTime()
                    .addMSecs(kInputModeTimeout);
                ccInputModeExpires = QTime::currentTime();
                ClearInputQueues(false);
                AddKeyToInputQueue(0);
            }            
        }
        else if (action == "SEEKFFWD")
        {
            if (HasQueuedInput())
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
            if (HasQueuedInput())
                DoArbSeek(ARBSEEK_END);
            else if (paused)
                DoSeek(1.0, tr("Forward"));
            else
                ChangeFFRew(1);
        }
        else if (action == "SEEKRWND")
        {
            if (HasQueuedInput())
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
            if (HasQueuedInput())
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
                nvp->ChangeDVDTrack(0);
                UpdateOSDSeekMessage(tr("Previous Chapter"),
                                     osd_general_timeout);
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
                nvp->ChangeDVDTrack(1);
                UpdateOSDSeekMessage(tr("Next Chapter"), osd_general_timeout);
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
                QString action = actions[i];
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
                DoNVPSeek(StopFFRew());
                UpdateOSDSeekMessage(PlayMesg(), osd_general_timeout);
                handled = true;
            }
        }

        if (speed_index)
        {
            NormalSpeed();
            UpdateOSDSeekMessage(PlayMesg(), osd_general_timeout);
            handled = true;
        }
    }

    if (!handled)
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            bool ok = false;
            int val = action.toInt(&ok);

            if (ok)
            {
                AddKeyToInputQueue('0' + val);
                handled = true;
            }
        }
    }

    if (StateIsLiveTV(GetState()))
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;

            if (action == "INFO")
            {
                if (HasQueuedInput())
                    DoArbSeek(ARBSEEK_SET);
                else
                    ToggleOSD(true);
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
                CommitQueuedInput();
            else if (action == "GUIDE")
                LoadMenu();
            //else if (action == "TOGGLEPIPMODE")
            //    TogglePIPView();
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
            QString action = actions[i];
            handled = true;

            if (action == "INFO")
            {
                if (HasQueuedInput())
                    DoArbSeek(ARBSEEK_SET);
                else
                    ToggleOSD(true);
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
                    nvp->ChangeDVDTrack(0);
                    UpdateOSDSeekMessage(tr("Previous Chapter"),
                                         osd_general_timeout);
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
                    nvp->ChangeDVDTrack(1);
                    UpdateOSDSeekMessage(tr("Next Chapter"),
                                         osd_general_timeout);
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
#if 0
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
            VERBOSE(VB_IMPORTANT, LOC + "HandleStateChange() Error, failed "
                    "to start RingBuffer for PiP on backend. Aborting.");
            delete testrec;
            piprecorder = NULL;
            return;
        }

        piprbuffer = new RingBuffer(name, filesize, smudge, piprecorder);

        // Get timeout for this recorder
        lockTimeout[piprecorder->GetRecorderNumber()] =
            GetLockTimeout(piprecorder->GetRecorderNumber());

        piprecorder->SpawnLiveTV( , true);

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
#endif
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
    if (!pipnvp || !piptvchain || !tvchain) 
        return;

    QString dummy;
    QString pipchanname;
    QString bigchanname;

    pipchanname = piptvchain->GetChannelName(-1);
    bigchanname = tvchain->GetChannelName(-1);

    if (activenvp != nvp)
        ToggleActiveWindow();

    ChangeChannel(0, pipchanname);

    ToggleActiveWindow();

    ChangeChannel(0, bigchanname);

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

    DoNVPSeek(time);
    UpdateOSDSeekMessage(PlayMesg(), osd_general_timeout);

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
        DoNVPSeek(time);
        UpdateOSDSeekMessage(tr("Paused"), -1);
        gContext->RestoreScreensaver();
    }
    else
    {
        DoNVPSeek(time);
        UpdateOSDSeekMessage(PlayMesg(), osd_general_timeout);
        gContext->DisableScreensaver();
    }
}

bool TV::DoNVPSeek(float time)
{
    if (time > -0.001f && time < +0.001f)
        return false;

    bool muted = false;

    AudioOutput *aud = nvp->getAudioOutput(); 
    if (aud && !aud->GetMute())
    {
        aud->ToggleMute();
        muted = true;
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
    DoNVPSeek(time);
    UpdateOSDSeekMessage(mesg, osd_general_timeout);

    if (activenvp->GetLimitKeyRepeat())
    {
        keyRepeat = false;
        keyrepeatTimer->start(300, true);
    }
}

void TV::DoArbSeek(ArbSeekWhence whence)
{
    bool ok = false;
    int seek = GetQueuedInputAsInt(&ok);
    ClearInputQueues(true);
    if (!ok)
        return;

    float time = ((seek / 100) * 3600) + ((seek % 100) * 60);

    if (whence == ARBSEEK_FORWARD)
        DoSeek(time, tr("Jump Ahead"));
    else if (whence == ARBSEEK_REWIND)
        DoSeek(-time, tr("Jump Back"));
    else
    {
        if (whence == ARBSEEK_END)
            time = (activenvp->CalcMaxFFTime(LONG_MAX, false) / frameRate) - time;
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
    DoNVPSeek(time);
    UpdateOSDSeekMessage(mesg, osd_general_timeout);
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
            DoNVPSeek(time);
            UpdateOSDSeekMessage(PlayMesg(), osd_general_timeout);
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
    UpdateOSDSeekMessage(mesg, -1);
}

void TV::DoQueueTranscode(void)
{
    QMutexLocker lock(&pbinfoLock);

    if (internalState == kState_WatchingPreRecorded)
    {
        bool stop = false;
        if (queuedTranscode)
            stop = true;
        else if (JobQueue::IsJobQueuedOrRunning(JOB_TRANSCODE,
                                                playbackinfo->chanid,
                                                playbackinfo->recstartts))
        {
            stop = true;
        }

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
        struct StatusPosInfo posInfo;
        nvp->calcSliderPos(posInfo);
        posInfo.desc = tr("Searching...");
        GetOSD()->ShowStatus(posInfo, slidertype, tr("Skip"), 6);
        update_osd_pos = true;
    }

    if (activenvp)
        activenvp->SkipCommercials(direction);

    if (muted) 
        muteTimer->start(kMuteTimeout, true);
}

static int get_cardinputid(uint cardid, const QString &channum)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT cardinputid "
        "FROM channel, capturecard, cardinput "
        "WHERE channel.channum      = :CHANNUM           AND "
        "      channel.sourceid     = cardinput.sourceid AND "
        "      cardinput.cardid     = capturecard.cardid AND "
        "      capturecard.cardid   = :CARDID");
    query.bindValue(":CHANNUM", channum);
    query.bindValue(":CARDID", cardid);

    if (!query.exec() || !query.isActive())
        MythContext::DBError("get_cardinputid", query);
    else if (query.next())
        return query.value(0).toInt();

    return -1;    
}

static void set_startchan(uint cardinputid, const QString &channum)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE cardinput "
                  "SET startchan = :CHANNUM "
                  "WHERE cardinputid = :INPUTID");
    query.bindValue(":CHANNUM", channum);
    query.bindValue(":INPUTID", cardinputid);
    query.exec();
    if (!query.exec())
        MythContext::DBError("set_startchan", query);
}

void TV::SwitchCards(uint chanid, QString channum)
{
    VERBOSE(VB_PLAYBACK, LOC +
            QString("SwitchCards(%1,'%2')").arg(chanid).arg(channum));

    RemoteEncoder *testrec = NULL;

    if (!StateIsLiveTV(GetState()))
        return;

    if (/*chanid || */!channum.isEmpty())
    {
        // If we are switching to a channel not on the current recorder
        // we need to find the next free recorder with that channel.
        QStringList reclist = GetValidRecorderList(chanid, channum);
        testrec = RemoteRequestFreeRecorderFromList(reclist);
        // now we need to set our channel as the starting channel..
        if (testrec && testrec->IsValidRecorder())
        {
            int cardid = testrec->GetRecorderNumber();
            int cardinputid = get_cardinputid(cardid, channum);
            VERBOSE(VB_IMPORTANT, LOC + "setting startchan: " +
                    QString("cardid(%1) cardinputid(%2) channum(%3)")
                    .arg(cardid).arg(cardinputid).arg(channum));
            set_startchan(cardinputid, channum);
        }
    }

    // If we are just switching recorders find first available recorder.
    if (!testrec)
        testrec = RemoteRequestNextFreeRecorder(recorder->GetRecorderNumber());

    if (testrec && testrec->IsValidRecorder())
    {
        switchToRec = testrec;
        exitPlayer  = true;
    }
    else
    {
        VERBOSE(VB_GENERAL, LOC + "No recorder to switch to...");
        // If activenvp is main nvp, show old input in on screen display
        if (nvp && activenvp == nvp)
            UpdateOSDInput();
        delete testrec;
    }
}

void TV::ToggleInputs(void)
{
    // If main Nuppel Video Player is paused, unpause it
    if (activenvp == nvp && paused)
    {
        if (GetOSD())
            GetOSD()->EndStatus();
        gContext->DisableScreensaver();
        paused = false;
    }

    // Pause the backend recorder, send command, and then unpause..
    PauseLiveTV();
    activerecorder->ToggleInputs();
    UnpauseLiveTV();

    // If activenvp is main nvp, show new input in on screen display
    if (nvp && activenvp == nvp)
        UpdateOSDInput();
}

void TV::ToggleChannelFavorite(void)
{
    activerecorder->ToggleChannelFavorite();
}

void TV::ChangeChannel(int direction)
{
    bool muted = false;

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
            GetOSD()->EndStatus();
        gContext->DisableScreensaver();
        paused = false;
    }

    // Save the current channel if this is the first time
    if (nvp && (activenvp == nvp) && prevChan.size() == 0)
        AddPreviousChannel();

    PauseLiveTV();

    activerecorder->ChangeChannel(direction);
    ClearInputQueues(false);

    if (muted)
        muteTimer->start(kMuteTimeout * 2, true);

    UnpauseLiveTV();
}

QString TV::GetQueuedChanNum(void) const
{
    // strip initial zeros.
    int nzi = queuedChanNum.find(QRegExp("([1-9]|\\w)"));
    if (nzi > 0)
        queuedChanNum = queuedChanNum.right(queuedChanNum.length() - nzi);

    return queuedChanNum.stripWhiteSpace();
}

/** \fn TV::ClearInputQueues(bool)
 *  \brief Clear channel key buffer of input keys.
 *  \param hideosd, if true hides "channel_number" OSDSet.
 */
void TV::ClearInputQueues(bool hideosd)
{
    if (hideosd && GetOSD()) 
        GetOSD()->HideSet("channel_number");

    queuedInput   = "";
    queuedChanNum = "";
    queuedChanID  = 0;
}

void TV::AddKeyToInputQueue(char key)
{
    static char *spacers[5] = { "_", "-", "#", ".", NULL };

    if (key)
    {
        queuedInput   = queuedInput.append(key).right(kInputKeysMax);
        queuedChanNum = queuedChanNum.append(key).right(kInputKeysMax);
    }

    /* 
     * Always use smartChannelChange when channel numbers are entered in
     * browse mode because in browse mode space/enter exit browse mode and
     * change to the currently browsed channel. This makes smartChannelChange
     * the only way to enter a channel number to browse without waiting for the
     * OSD to fadeout after entering numbers.
     */
    bool do_smart = StateIsLiveTV(GetState()) &&
        !ccInputMode && !asInputMode &&
        (smartChannelChange || browsemode);
    QString chan = GetQueuedChanNum();
    if (!chan.isEmpty() && do_smart)
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
        queuedChanNum = (ok) ? mod : chan.right(1);
        do_smart &= unique;
    }

    QString inputStr = (do_smart) ? queuedChanNum : queuedInput;
    inputStr = inputStr.isEmpty() ? "?" : inputStr;
    if (ccInputMode)
        inputStr = tr("CC:", "closed caption, teletext page") + " " + inputStr;
    else if (asInputMode)
        inputStr = tr("Seek:", "seek to location") + " " + inputStr;
    UpdateOSDTextEntry(inputStr);

    if (do_smart)
        CommitQueuedInput();
}

void TV::UpdateOSDTextEntry(const QString &message)
{
    if (!GetOSD())
        return;

    InfoMap infoMap;

    infoMap["channum"]  = message;
    infoMap["callsign"] = "";

    GetOSD()->ClearAllText("channel_number");
    GetOSD()->SetText("channel_number", infoMap, 2);
}

void TV::CommitQueuedInput(void)
{
    VERBOSE(VB_PLAYBACK, LOC + "CommitQueuedInput() " + 
            QString("livetv(%1) qchannum(%2) qchanid(%3)")
            .arg(StateIsLiveTV(GetState()))
            .arg(GetQueuedChanNum())
            .arg(GetQueuedChanID()));

    if (ccInputMode)
    {
        bool valid = false;
        int page = GetQueuedInputAsInt(&valid, 16) << 16;
        if (valid && page)
            DoToggleCC(page);
        ccInputModeExpires.start(); // expire ccInputMode
    }
    else if (asInputMode)
    {
        if (HasQueuedInput())
            DoArbSeek(ARBSEEK_FORWARD);
    }
    else if (StateIsLiveTV(GetState()))
    {
        QString channum = GetQueuedChanNum();
        if (browsemode)
        {
            BrowseChannel(channum);
            if (activenvp == nvp && GetOSD())
                GetOSD()->HideSet("channel_number");
        }
        else if (activerecorder->CheckChannel(channum))
            ChangeChannel(GetQueuedChanID(), channum);
        else
            ChangeChannel(GetQueuedChanID(), GetQueuedInput());
    }

    ClearInputQueues(true);
}

void TV::ChangeChannel(uint chanid, const QString &channum)
{
    VERBOSE(VB_PLAYBACK, LOC + QString("ChangeChannel(%1, '%2') ")
            .arg(chanid).arg(channum));

    QStringList reclist;
    bool muted = false;

    if (activerecorder)
    {
        bool getit = false, unique = false;
        if (chanid)
            getit = activerecorder->ShouldSwitchToAnotherCard(
                QString::number(chanid));
        else
            getit = !activerecorder->CheckChannelPrefix(channum, unique);

        if (getit)
            reclist = GetValidRecorderList(chanid, channum);
    }

    if (reclist.size())
    {
        RemoteEncoder *testrec = NULL;
        testrec = RemoteRequestFreeRecorderFromList(reclist);
        if (!testrec || !testrec->IsValidRecorder())
        {
            ClearInputQueues(true);
            ShowNoRecorderDialog();
            if (testrec)
                delete testrec;
            return;
        }

        if (!prevChan.empty() && prevChan.back() == channum)
        {
            // need to remove it if the new channel is the same as the old.
            prevChan.pop_back();
        }

        // found the card on a different recorder.
        delete testrec;
        SwitchCards(chanid, channum);
        return;
    }

    if (!prevChan.empty() && prevChan.back() == channum)
        return;

    if (!activerecorder->CheckChannel(channum))
        return;

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
        GetOSD()->EndStatus();
        gContext->DisableScreensaver();
        paused = false;
    }

    // Save the current channel if this is the first time
    if (prevChan.size() == 0)
        AddPreviousChannel();

    PauseLiveTV();

    activerecorder->SetChannel(channum);

    if (muted)
        muteTimer->start(kMuteTimeout * 2, true);

    UnpauseLiveTV();
}

void TV::AddPreviousChannel(void)
{
    if (!tvchain)
        return;

    // Don't store more than thirty channels.  Remove the first item
    if (prevChan.size() > 29)
        prevChan.erase(prevChan.begin());

    // This method builds the stack of previous channels
    prevChan.push_back(tvchain->GetChannelName(-1));
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
    if (!tvchain)
        return;

    // Stop the timer
    prevChanTimer->stop();

    // Figure out the vector the desired channel is in
    int i = (prevChan.size() - prevChanKeyCnt - 1) % prevChan.size();

    // Reset the prevChanKeyCnt counter
    prevChanKeyCnt = 0;

    // Only change channel if prevChan[vector] != current channel
    QString chan_name = tvchain->GetChannelName(-1);

    if (chan_name != prevChan[i])
    {
        queuedInput   = prevChan[i];
        queuedChanNum = prevChan[i];
        queuedChanID  = 0;
    }

    //Turn off the OSD Channel Num so the channel changes right away
    if (activenvp == nvp && GetOSD())
        GetOSD()->HideSet("channel_number");
}

bool TV::ClearOSD(void)
{
    bool res = false;

    if (HasQueuedInput() || HasQueuedChannel())
    {
        ClearInputQueues(true);
        res = true;
    }

    if (GetOSD() && GetOSD()->HideAll())
        res = true;

    while (res && GetOSD() && GetOSD()->HideAll())
        usleep(1000);

    return res;
}

/** \fn TV::ToggleOSD(bool includeStatus)
 *  \brief Cycle through the available Info OSDs. 
 */
void TV::ToggleOSD(bool includeStatusOSD)
{
    OSD *osd = GetOSD();
    bool showStatus = false;
    if (paused || !osd)
        return;
        
    // DVD toggles between status and nothing
    if (prbuffer->isDVD())
    {
        if (osd->IsSetDisplaying("status"))
            osd->HideAll();
        else
            showStatus = true;
    }
    else if (osd->IsSetDisplaying("status"))
    {
        if (osd->HasSet("program_info_small"))
            UpdateOSDProgInfo("program_info_small");
        else
            UpdateOSDProgInfo("program_info");
    }
    // If small is displaying, show long if we have it, else hide info
    else if (osd->IsSetDisplaying("program_info_small")) 
    {
        if (osd->HasSet("program_info"))
            UpdateOSDProgInfo("program_info");
        else
            osd->HideAll();
    }
    // If long is displaying, hide info
    else if (osd->IsSetDisplaying("program_info"))
    {
        osd->HideAll();
    }
    // If no program_info displaying, show status if we want it
    else if (includeStatusOSD)
    {
        showStatus = true;
    }
    // No status desired? Nothing is up, Display small if we have, else display long
    else
    {   
        if (osd->HasSet("program_info_small"))
            UpdateOSDProgInfo("program_info_small");
        else
            UpdateOSDProgInfo("program_info");
    }

    if (showStatus)
    {
        osd->HideAll();
        if (nvp)
        {
            struct StatusPosInfo posInfo;
            nvp->calcSliderPos(posInfo);
            osd->ShowStatus(posInfo, false, tr("Position"),
                              osd_prog_info_timeout);
            update_osd_pos = true;
        }
        else
            update_osd_pos = false;
    }
    else
        update_osd_pos = false;
}

/** \fn TV::UpdateOSDProgInfo(const char *whichInfo)
 *  \brief Update and display the passed OSD set with programinfo
 */
void TV::UpdateOSDProgInfo(const char *whichInfo)
{
    InfoMap infoMap;
    OSD *osd = GetOSD();
    if (!osd)
        return;

    pbinfoLock.lock();
    if (playbackinfo)
        playbackinfo->ToMap(infoMap);
    pbinfoLock.unlock();

    // Clear previous osd and add new info
    osd->ClearAllText(whichInfo);
    osd->HideAll();
    osd->SetText(whichInfo, infoMap, osd_prog_info_timeout);
}

void TV::UpdateOSDSeekMessage(const QString &mesg, int disptime)
{
    if (activenvp != nvp)
        return;

    struct StatusPosInfo posInfo;
    nvp->calcSliderPos(posInfo);
    bool slidertype = StateIsLiveTV(GetState());
    int osdtype = (doSmartForward) ? kOSDFunctionalType_SmartForward :
                                     kOSDFunctionalType_Default;
    if (GetOSD())
        GetOSD()->ShowStatus(posInfo, slidertype, mesg, disptime, osdtype);
    update_osd_pos = true;
}

void TV::UpdateOSDInput(void)
{
    if (!activerecorder || !tvchain)
        return;

    QString name = tvchain->GetInputName(-1);
    QString displayName = "";
    QString msg;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT displayname FROM cardinput "
                  "WHERE cardid = :CARDID AND inputname = :INPUTNAME");
    query.bindValue(":CARDID", activerecorder->GetRecorderNumber());
    query.bindValue(":INPUTNAME", name);
    query.exec();
    if (query.isActive() && query.size() > 0)
    {
        query.next();
        displayName = query.value(0).toString();
    }

    if ( displayName == "")
        msg = QString("%1: %2")
                .arg(activerecorder->GetRecorderNumber()).arg(name);
    else
        msg = displayName;

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
        pbinfoLock.lock();
        if (playbackinfo)
            playbackinfo->ToMap(lastSignalUIInfo);
        pbinfoLock.unlock();
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

    uint  sig  = 0;
    float snr  = 0.0f;
    uint  ber  = 0xffffffff;
    QString pat(""), pmt(""), mgt(""), vct(""), nit(""), sdt("");
    for (it = slist.begin(); it != slist.end(); ++it)
    {
        if ("error" == it->GetShortName() || "message" == it->GetShortName())
            continue;

        infoMap[it->GetShortName()] = QString::number(it->GetValue());
        if ("signal" == it->GetShortName())
            sig = it->GetNormalizedValue(0, 100);
        else if ("snr" == it->GetShortName())
            snr = it->GetValue();
        else if ("ber" == it->GetShortName())
            ber = it->GetValue();
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

    sigDesc = sigDesc + QString(" | (%1%2%3%4%5%6%7) %8")
        .arg(slock).arg(pat).arg(pmt).arg(mgt).arg(vct)
        .arg(nit).arg(sdt).arg(sigMsg);

    //GetOSD()->ClearAllText("signal_info");
    //GetOSD()->SetText("signal_info", infoMap, -1);

    GetOSD()->ClearAllText("channel_number");
    GetOSD()->SetText("channel_number", infoMap, osd_prog_info_timeout);

    infoMap["description"] = sigDesc;
    GetOSD()->ClearAllText("program_info");
    GetOSD()->SetText("program_info", infoMap, osd_prog_info_timeout);

    lastSignalMsg.clear();
    lastSignalMsgTime.start();

    // Turn off lock timer if we have an "All Good" or good PMT
    if (allGood || (pmt == "M"))
        lockTimerOn = false;
}

void TV::UpdateOSDTimeoutMessage(void)
{
    QString dlg_name("channel_timed_out");
    bool timed_out = false;
    if (activerecorder)
    {
        uint timeout = lockTimeout[activerecorder->GetRecorderNumber()];
        timed_out = lockTimerOn && ((uint)lockTimer.elapsed() > timeout);
    }
    OSD *osd = GetOSD();

    if (!osd)
    {
        if (timed_out)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "You have no OSD, "
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
    if (lcd == NULL || playbackinfo == NULL)
        return;

    QString title, subtitle, callsign;

    pbinfoLock.lock();
    title = playbackinfo->title;
    subtitle = playbackinfo->subtitle;
    callsign = playbackinfo->chansign;
    pbinfoLock.unlock();

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
    if (!playbackinfo)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "doLoadMenu(): no active playbackinfo.");
        return;
    }

    // Resize window to the MythTV GUI size
    MythMainWindow *mwnd = gContext->GetMainWindow();
    bool using_gui_size_for_tv = gContext->GetNumSetting("GuiSizeForTV", 0);
    if (!using_gui_size_for_tv)
    {
        mwnd->setGeometry(saved_gui_bounds.left(), saved_gui_bounds.top(),
                          saved_gui_bounds.width(), saved_gui_bounds.height());
        mwnd->setFixedSize(saved_gui_bounds.size());
    }

    // Collect channel info
    pbinfoLock.lock();
    uint    chanid  = playbackinfo->chanid.toUInt();
    QString channum = playbackinfo->chanstr;
    pbinfoLock.unlock();

    // See if we can provide a channel preview in EPG
    bool allowsecondary = true;
    if (nvp && nvp->getVideoOutput())
        allowsecondary = nvp->getVideoOutput()->AllowPreviewEPG();

    // Start up EPG
    bool cc = RunProgramGuide(chanid, channum, true, this, allowsecondary);

    StopEmbeddingOutput();

    // Resize the window back to the MythTV Player size
    if (!using_gui_size_for_tv)
    {
        mwnd->setGeometry(player_bounds.left(), player_bounds.top(),
                          player_bounds.width(), player_bounds.height());
        mwnd->setFixedSize(player_bounds.size());
    }

    // If user selected a new channel in the EPG, change to that channel
    if (cc)
        EPGChannelUpdate(chanid, channum);

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
            GetOSD()->ShowStatus(brightness * 10, true, tr("Adjust Recording"),
                text, 5, kOSDFunctionalType_RecPictureAdjust);
        }
        else
        {
            brightness = nvp->getVideoOutput()->ChangeBrightness(up);
            gContext->SaveSetting("PlaybackBrightness", brightness);
            text = QString(tr("Brightness %1 %")).arg(brightness);
            GetOSD()->ShowStatus(brightness * 10, true, tr("Adjust Picture"),
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
            GetOSD()->ShowStatus(contrast * 10, true, tr("Adjust Recording"),
                text, 5, kOSDFunctionalType_RecPictureAdjust);
        }
        else
        {
            contrast = nvp->getVideoOutput()->ChangeContrast(up);
            gContext->SaveSetting("PlaybackContrast", contrast);
            text = QString(tr("Contrast %1 %")).arg(contrast);
            GetOSD()->ShowStatus(contrast * 10, true, tr("Adjust Picture"),
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
            GetOSD()->ShowStatus(colour * 10, true, tr("Adjust Recording"),
                text, 5, kOSDFunctionalType_RecPictureAdjust);
        }
        else
        {
            colour = nvp->getVideoOutput()->ChangeColour(up);
            gContext->SaveSetting("PlaybackColour", colour);
            text = QString(tr("Colour %1 %")).arg(colour);
            GetOSD()->ShowStatus(colour * 10, true, tr("Adjust Picture"),
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
            GetOSD()->ShowStatus(hue * 10, true, tr("Adjust Recording"),
                text, 5, kOSDFunctionalType_RecPictureAdjust);
        }
        else
        {
            hue = nvp->getVideoOutput()->ChangeHue(up);
            gContext->SaveSetting("PlaybackHue", hue);
            text = QString(tr("Hue %1 %")).arg(hue);
            GetOSD()->ShowStatus(hue * 10, true, tr("Adjust Picture"),
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
        GetOSD()->ShowStatus(curvol * 10, true, tr("Adjust Volume"), text, 5, 
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
        GetOSD()->ShowStatus(val, false, tr("Adjust Time Stretch"), text, 10, 
                        kOSDFunctionalType_TimeStretchAdjust);
        update_osd_pos = false;
    }
}

// dir in 10ms jumps
void TV::ChangeAudioSync(int dir, bool allowEdit)
{
    long long newval;

    if (!audiosyncAdjustment && LONG_LONG_MIN == audiosyncBaseline)
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

        GetOSD()->ShowStatus((val/2)+500, false, tr("Adjust Audio Sync"), text,
                             10, kOSDFunctionalType_AudioSyncAdjust);
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

void TV::EPGChannelUpdate(uint chanid, QString channum)
{
    if (chanid && !channum.isEmpty())
    {
        // we need to use deep copy bcs this can be called from another thread.
        queuedInput   = QDeepCopy<QString>(channum);
        queuedChanNum = queuedInput;
        queuedChanID  = chanid;
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

        if (message.left(14) == "DONE_RECORDING")
        {
            if (GetState() == kState_WatchingRecording)
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
            else if (StateIsLiveTV(GetState()))
            {
                message = message.simplifyWhiteSpace();
                QStringList tokens = QStringList::split(" ", message);
                int cardnum = tokens[1].toInt();
                int filelen = tokens[2].toInt();

                if (cardnum == recorder->GetRecorderNumber() &&
                    tvchain && tvchain->HasNext())
                {
                    nvp->SetWatchingRecording(false);
                    nvp->SetLength(filelen);
                }
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
        else if (tvchain && message.left(12) == "LIVETV_CHAIN")
        {
            message = message.simplifyWhiteSpace();
            QStringList tokens = QStringList::split(" ", message);
            if (tokens[1] == "UPDATE")
            {
                if (tokens[2] == tvchain->GetID())
                {
                    tvchain->ReloadAll();
                    if (nvp && nvp->GetTVChain())
                        nvp->CheckTVChain();
                }
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
            VERBOSE(VB_IMPORTANT, LOC + "Got SKIP_TO message. Keyframe: "
                    <<stringToLongLong(keyframe[0]));
            bool tc = recorder && (recorder->GetRecorderNumber() == cardnum);
            (void)tc;
        }

        pbinfoLock.lock();
        if (playbackinfo && message.left(14) == "COMMFLAG_START")
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
                pbinfoLock.unlock();
                RemoteSendMessage(msg);
                pbinfoLock.lock();
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
        pbinfoLock.unlock();
    }
}

/** \fn TV::BrowseStart()
 *  \brief Begins channel browsing.
 */
void TV::BrowseStart(void)
{
    if (paused || !GetOSD())
        return;

    OSDSet *oset = GetOSD()->GetSet("browse_info");
    if (!oset)
        return;

    ClearOSD();

    pbinfoLock.lock();
    if (playbackinfo)
    {
        browsemode = true;
        browsechannum = playbackinfo->chanstr;
        browsechanid = playbackinfo->chanid;
        browsestarttime = playbackinfo->startts.toString();

        BrowseDispInfo(BROWSE_SAME);

        browseTimer->start(kBrowseTimeout, true);
    }
    pbinfoLock.unlock();
}

/** \fn TV::BrowseEnd(bool)
 *  \brief Ends channel browsing. Changing the channel if change is true.
 *  \param change, iff true we call ChangeChannel()
 */
void TV::BrowseEnd(bool change)
{
    if (!browsemode || !GetOSD())
        return;

    browseTimer->stop();

    GetOSD()->HideSet("browse_info");

    if (change)
    {
        ChangeChannel(0, browsechannum);
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
    ProgramInfo *program_info = 
        ProgramInfo::GetProgramAtDateTime(browsechanid, startts);
    
    if (program_info)
        program_info->ToMap(infoMap);

    GetOSD()->ClearAllText("browse_info");
    GetOSD()->SetText("browse_info", infoMap, -1);

    delete program_info;
}

void TV::ToggleRecord(void)
{
    if (browsemode)
    {
        InfoMap infoMap;
        QDateTime startts = QDateTime::fromString(
            browsestarttime, Qt::ISODate);

        ProgramInfo *program_info = 
            ProgramInfo::GetProgramAtDateTime(browsechanid, startts);
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
        return;
    }

    QMutexLocker lock(&pbinfoLock);

    if (!playbackinfo)
    {
        VERBOSE(VB_GENERAL, LOC + "Unknown recording during live tv.");
        return;
    }

    QString cmdmsg("");
    if (playbackinfo->GetAutoExpireFromRecorded() == kLiveTVAutoExpire)
    {
        int autoexpiredef = gContext->GetNumSetting("AutoExpireDefault", 0);
        playbackinfo->SetAutoExpire(autoexpiredef);
        playbackinfo->ApplyRecordRecGroupChange("Default");
        cmdmsg = tr("Record");
    }
    else
    {
        playbackinfo->SetAutoExpire(kLiveTVAutoExpire);
        playbackinfo->ApplyRecordRecGroupChange("LiveTV");
        cmdmsg = tr("Cancel Record");
    }

    QString msg = cmdmsg + "\"" + playbackinfo->title + "\"";
    if (activenvp == nvp && GetOSD())
        GetOSD()->SetSettingsText(msg, 3);
}

void TV::BrowseChannel(const QString &chan)
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
        GetOSD()->ShowStatus(value*10, true, title, picName, 5, 
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
        GetOSD()->ShowStatus(value * 10, true, title, recName, 5,
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
    pbinfoLock.lock();
    bool isEditing = playbackinfo->IsEditing();
    pbinfoLock.unlock();

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
        //else if (action == "TOGGLEPIPMODE")
        //    TogglePIPView();
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
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Unknown menu action selected: " + action);
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
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Unknown menu action selected: " + action);
            hidetree = false;
        }
    }
    else
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Unknown menu action selected: " + action);
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

        if (!persistentbrowsemode)
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

        pbinfoLock.lock();

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

        pbinfoLock.unlock();
    }
    
    const QStringList atracks = activenvp->listAudioTracks();
    if (atracks.count() > 1)
    {
        item = new OSDGenericTree(treeMenu, tr("Select Audio Track"),
                                  "NEXTAUDIO");

        int atrack = activenvp->getCurrentAudioTrack();
        QStringList::const_iterator it = atracks.begin();
        for (uint i = 1; it != atracks.end(); ++it, ++i)
        {
            subitem = new OSDGenericTree(
                item, *it, "SELECTAUDIO" + QString("%1").arg(i),
                ((int)i == atrack) ? 1 : 0, NULL, "AUDIOGROUP");
        }
    }

    const QStringList stracks = activenvp->listSubtitleTracks();
    if (!stracks.empty())
    {
        item = new OSDGenericTree(
            treeMenu, tr("Select Subtitles"), "TOGGLECC");

        int strack = activenvp->getCurrentSubtitleTrack();

        subitem = new OSDGenericTree(
            item, "None", "SELECTSUBTITLE" + QString("%1").arg(0),
            (0 == strack) ? 1 : 0, NULL, "SUBTITLEGROUP");

        
        QStringList::const_iterator it = stracks.begin();
        for (uint i = 1; it != stracks.end(); ++it, ++i)
        {
            subitem = new OSDGenericTree(
                item, *it, "SELECTSUBTITLE" + QString("%1").arg(i),
                ((int)i == strack) ? 1 : 0, NULL, "SUBTITLEGROUP");
        }
    }
    else if (vbimode == 1)
    {
        item = new OSDGenericTree(
            treeMenu, tr("Toggle Teletext"), "TOGGLECC");
    }
    else if (vbimode == 2)
    {
        item    = new OSDGenericTree(treeMenu, tr("Closed Captioning"));
        subitem = new OSDGenericTree(item, tr("Toggle CC"), "TOGGLECC");
        for (uint i = 1; i <= 4; i++)
        {
            subitem = new OSDGenericTree(
                item, QString("%1%2").arg(tr("CC")).arg(i),
                QString("DISPCC%1").arg(i));
        }
        for (uint i = 1; i <= 4; i++)
        {
            subitem = new OSDGenericTree(
                item, QString("%1%2").arg(tr("TXT")).arg(i),
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

    pbinfoLock.lock();

    if (playbackinfo->GetAutoExpireFromRecorded())
    {
        playbackinfo->SetAutoExpire(0);
        desc = tr("Auto-Expire OFF");
    }
    else
    {
        playbackinfo->SetAutoExpire(1);
        desc = tr("Auto-Expire ON");
    }

    pbinfoLock.unlock();

    if (GetOSD() && activenvp == nvp && desc != "" )
    {
        struct StatusPosInfo posInfo;
        nvp->calcSliderPos(posInfo);
        GetOSD()->ShowStatus(posInfo, false, desc, 1);
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
        struct StatusPosInfo posInfo;
        nvp->calcSliderPos(posInfo);
        GetOSD()->ShowStatus(posInfo, false, desc, 1);
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
        struct StatusPosInfo posInfo;
        nvp->calcSliderPos(posInfo);
        GetOSD()->ShowStatus(posInfo, false, desc, 1);
        update_osd_pos = false;
    }
}

void TV::ToggleSleepTimer(const QString time)
{
    const int minute = 60*1000; /* milliseconds in a minute */
    int mins = 0;

    if (!sleepTimer)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "No sleep timer?");
        return;
    }

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
                VERBOSE(VB_IMPORTANT, LOC_ERR + "Invalid time "<<time);
            }
        }
        else
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Invalid time string "<<time);
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

/** \fn TV::GetValidRecorderList(uint)
 *  \brief Returns list of the recorders that have chanid in their sources.
 *  \param chanid  Channel ID of channel we are querying recorders for.
 *  \return List of cardid's for recorders with channel.
 */
QStringList TV::GetValidRecorderList(uint chanid)
{
    QStringList reclist;

    // Query the database to determine which source is being used currently.
    // set the EPG so that it only displays the channels of the current source
    MSqlQuery query(MSqlQuery::InitCon());
    // We want to get the current source id for this recorder
    query.prepare(
        "SELECT cardinput.cardid "
        "FROM channel "
        "LEFT JOIN cardinput ON channel.sourceid = cardinput.sourceid "
        "WHERE channel.chanid = :CHANID");
    query.bindValue(":CHANID", chanid);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("GetValidRecorderList ChanID", query);
        return reclist;
    }

    while (query.next())
        reclist << query.value(0).toString();

    return reclist;
}

/** \fn TV::GetValidRecorderList(const QString&)
 *  \brief Returns list of the recorders that have channum in their sources.
 *  \param channum  Channel "number" we are querying recorders for.
 *  \return List of cardid's for recorders with channel.
 */
QStringList TV::GetValidRecorderList(const QString &channum)
{
    QStringList reclist;

    // Query the database to determine which source is being used currently.
    // set the EPG so that it only displays the channels of the current source
    MSqlQuery query(MSqlQuery::InitCon());
    // We want to get the current source id for this recorder
    query.prepare(
        "SELECT cardinput.cardid "
        "FROM channel "
        "LEFT JOIN cardinput ON channel.sourceid = cardinput.sourceid "
        "WHERE channel.channum = :CHANNUM");
    query.bindValue(":CHANNUM", channum);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("GetValidRecorderList ChanNum", query);
        return reclist;
    }

    while (query.next())
        reclist << query.value(0).toString();

    return reclist;
}

/** \fn TV::GetValidRecorderList(uint, const QString&)
 *  \brief Returns list of the recorders that have chanid or channum
 *         in their sources.
 *  \param chanid   Channel ID of channel we are querying recorders for.
 *  \param channum  Channel "number" we are querying recorders for.
 *  \return List of cardid's for recorders with channel.
 */
QStringList TV::GetValidRecorderList(uint chanid, const QString &channum)
{
    if (chanid)
        return GetValidRecorderList(chanid);
    else if (!channum.isEmpty())
        return GetValidRecorderList(channum);
    return QStringList();
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
 *  \brief Used in ChangeChannel(), ChangeChannel(),
 *         and ToggleInputs() to temporarily stop video output.
 */
void TV::PauseLiveTV(void)
{
    VERBOSE(VB_PLAYBACK, LOC + "PauseLiveTV()");
    lockTimerOn = false;

    if (activenvp && activerbuffer)
    {
        activerbuffer->StopReads();
        activenvp->PauseDecoder();
        activerbuffer->IgnoreLiveEOF(true);
        activerbuffer->StartReads();
    }

    // XXX: Get rid of this?
    activerecorder->PauseRecorder();

    osdlock.lock();
    lastSignalMsg.clear();
    lastSignalUIInfo.clear();
    osdlock.unlock();

    lockTimerOn = false;
    uint timeout = lockTimeout[activerecorder->GetRecorderNumber()];
    if (timeout < 0xffffffff)
    {
        lockTimer.start();
        lockTimerOn = true;
    }
}

/** \fn TV::UnpauseLiveTV()
 *  \brief Used in ChangeChannel(), ChangeChannel(),
 *         and ToggleInputs() to restart video output.
 */
void TV::UnpauseLiveTV(void)
{
    VERBOSE(VB_PLAYBACK, LOC + "UnpauseLiveTV()");

    if (activenvp && tvchain)
    {
        tvchain->ReloadAll();
        ProgramInfo *pginfo = tvchain->GetProgramAt(-1);
        if (pginfo)
        {
            SetCurrentlyPlaying(pginfo);
            delete pginfo;
        }

        tvchain->JumpTo(-1, 1);

        QString filters = GetFiltersForChannel();
        activenvp->SetVideoFilters(filters);
        activenvp->Play(normal_speed, true, false);
    }

    if (!nvp || (nvp && activenvp == nvp))
    {
        UpdateOSDProgInfo("program_info");
        UpdateLCD();
        AddPreviousChannel();
    }
}

void TV::SetCurrentlyPlaying(ProgramInfo *pginfo)
{
    pbinfoLock.lock();

    if (playbackinfo)
        delete playbackinfo;
    playbackinfo = NULL;

    if (pginfo)
        playbackinfo = new ProgramInfo(*pginfo);

    pbinfoLock.unlock();
}

