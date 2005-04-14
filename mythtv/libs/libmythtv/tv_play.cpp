#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <unistd.h>
#include <pthread.h>

#include <iostream>
using namespace std;

#include <qapplication.h>
#include <qregexp.h>
#include <qfile.h>
#include <qtimer.h>

#include "mythdbcon.h"
#include "tv.h"
#include "osd.h"
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
#include "commercial_skip.h"
#include "vsync.h"
#include "lcddevice.h"
#include "jobqueue.h"
#include "audiooutput.h"
#include "DisplayRes.h"

struct SeekSpeedInfo {
    QString   dispString;
    float  scaling;
    float ff_repos;
    float rew_repos;
};

// uncomment define to use MythTV with fluxbox as the window manager
//#define FLUXBOX_HACK

#define MAX_REPO_LEVEL 3
SeekSpeedInfo seek_speed_array[MAX_REPO_LEVEL][9] =
{
    // Less adjustment overall, no adjustment on the low end
    {
     {  "3X",    3.0,   1.50,   2.50},
     {  "5X",    5.0,   2.50,   2.50},
     { "10X",   10.0,   5.00,   5.00},
     { "20X",   20.0,  10.00,  10.00},
     { "30X",   30.0,  15.00,  15.00},
     { "60X",   60.0,  30.00,  30.00},
     {"120X",  120.0,  60.00,  60.00},
     {"180X",  180.0,  90.00,  90.00}},
    
    // Less adjustment overall
    {
     {  "3X",    3.0,   2.50,   2.50},
     {  "5X",    5.0,   3.75,   3.75},
     { "10X",   10.0,   7.50,   7.50},
     { "20X",   20.0,  15.00,  15.00},
     { "30X",   30.0,  22.50,  22.50},
     { "60X",   60.0,  45.00,  45.00},
     {"120X",  120.0,  90.00,  90.00},
     {"180X",  180.0, 135.00, 135.00}},
    
    // More adjustment (this is the default)
    {
     {  "3X",    3.0,   3.00,   3.00},
     {  "5X",    5.0,   5.00,   5.00},
     { "10X",   10.0,  10.00,  10.00},
     { "20X",   20.0,  20.00,  20.00},
     { "30X",   30.0,  30.00,  30.00},
     { "60X",   60.0,  60.00,  60.00},
     {"120X",  120.0, 120.00, 120.00},
     {"180X",  180.0, 180.00, 180.00}},
};

const int SSPEED_NORMAL = 0;
const int SSPEED_MAX = sizeof seek_speed_array[0] / sizeof seek_speed_array[0][0];

struct SleepTimer {
    QString   dispString;
    unsigned long seconds;
};

SleepTimer sleep_timer_array[] =
{
    {QObject::tr("Off"),       0},
    {QObject::tr("30m"),   30*60},
    {QObject::tr("1h"),    60*60},
    {QObject::tr("1h30m"), 90*60},
    {QObject::tr("2h"),   120*60},
};

const int SSLEEP_MAX = sizeof sleep_timer_array / sizeof sleep_timer_array[0];

const int kMuteTimeout = 800;
const int kLCDTimeout = 30000;

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
    REG_KEY("TV Playback", "SUBCHANNELSEP", "Separator for HDTV subchannels", "_");
    
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

TV::TV(void)
    : QObject(),
      // Configuration variables from database
      repoLevel(0), baseFilters(""), fftime(0), rewtime(0),
      jumptime(0), usePicControls(false), smartChannelChange(false),
      showBufferedWarnings(false), bufferedChannelThreshold(0),
      MuteIndividualChannels(false), arrowAccel(false),
      vbimode(0), 
      // Configuretion variables from DB in RunTV
      stickykeys(0), ff_rew_repos(0), ff_rew_reverse(false),
      smartForward(false),
      // Configuration varibles from DB just before playback
      autoCommercialSkip(false), tryUnflaggedSkip(false),
      osd_display_time(0),
      // State variables
      internalState(kState_None), nextState(kState_None), changeState(false),
      menurunning(false), runMainLoop(false), wantsToQuit(true), 
      exitPlayer(false), paused(false), errored(false),
      stretchAdjustment(false), editmode(false), zoomMode(false),
      update_osd_pos(false), endOfRecording(false), requestDelete(false),
      doSmartForward(false), switchingCards(false), lastRecorderNum(-1),
      queuedTranscode(false), getRecorderPlaybackInfo(false), 
      sleep_index(0), sleepTimer(new QTimer(this)), 
      picAdjustment(kPictureAttribute_None),
      recAdjustment(kPictureAttribute_None),
      // Key processing buffer, lock, and state
      keyRepeat(true), keyrepeatTimer(new QTimer(this)),
      // Fast forward state
      doing_ff_rew(0), ff_rew_index(0), speed_index(0), normal_speed(1.0f),
      // Channel changing state variables
      channelqueued(false), channelkeysstored(0), 
      lastCC(""), lastCCDir(0), muteTimer(new QTimer(this)),
      // previous channel functionality state variables
      times_pressed(0), prevChannelTimer(new QTimer(this)),
      // channel browsing state variables
      browsemode(false), persistentbrowsemode(false),
      browseTimer(new QTimer(this)),
      browsechannum(""), browsechanid(""), browsestarttime(""),
      // Program Info for currently playing video
      playbackinfo(NULL), inputFilename(""), playbackLen(0),
      recorderPlaybackInfo(NULL),
      // estimated framerate from recorder
      frameRate(30.0f),
      // Video Players
      nvp(NULL), pipnvp(NULL), activenvp(NULL),
      // Remote Encoders
      recorder(NULL), piprecorder(NULL), activerecorder(NULL),
      // RingBuffers
      prbuffer(NULL), piprbuffer(NULL), activerbuffer(NULL), 
      // OSD info
      osd(NULL), dialogname(""), treeMenu(NULL), udpnotify(NULL),
      // LCD Info
      lcdTitle(""), lcdSubtitle(""), lcdCallsign(""),
      // Window info (GUI is optional, transcoding, preview img, etc)
      myWindow(NULL), embedid(0), embx(0), emby(0), embw(0), embh(0)
{
    bzero(channelKeys, sizeof(channelKeys));
    lastLcdUpdate = QDateTime::currentDateTime();
    lastLcdUpdate.addYears(-1); // make last LCD update last year..

    gContext->addListener(this);

    connect(prevChannelTimer, SIGNAL(timeout()), SLOT(SetPreviousChannel()));
    connect(browseTimer,      SIGNAL(timeout()), SLOT(BrowseEndTimer()));
    connect(muteTimer,        SIGNAL(timeout()), SLOT(UnMute()));
    connect(keyrepeatTimer,   SIGNAL(timeout()), SLOT(KeyRepeatOK()));
    connect(sleepTimer,       SIGNAL(timeout()), SLOT(SleepEndTimer()));
}

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

    repoLevel = gContext->GetNumSetting("TVRepositionLevel", 2);
    if ((repoLevel >= MAX_REPO_LEVEL) || (repoLevel < 0))
        repoLevel = MAX_REPO_LEVEL - 1;

    baseFilters += gContext->GetSetting("CustomFilters");
    fftime = gContext->GetNumSetting("FastForwardAmount", 30);
    rewtime = gContext->GetNumSetting("RewindAmount", 5);
    jumptime = gContext->GetNumSetting("JumpAmount", 10);
    usePicControls = gContext->GetNumSetting("UseOutputPictureControls", 0);
    smartChannelChange = gContext->GetNumSetting("SmartChannelChange", 0);
    showBufferedWarnings = gContext->GetNumSetting("CCBufferWarnings", 0);
    bufferedChannelThreshold = gContext->GetNumSetting("CCWarnThresh", 10);
    MuteIndividualChannels = gContext->GetNumSetting("IndividualMuteControl",0);
    arrowAccel = gContext->GetNumSetting("UseArrowAccels", 1);

    QString vbiformat = gContext->GetSetting("VbiFormat");
    vbimode = (vbiformat.lower() == "pal teletext") ? 1 : 0;
    vbimode = (vbiformat.lower().left(4) == "ntsc") ? 2 : vbimode;

    if (createWindow)
    {
        MythMainWindow *mainWindow = gContext->GetMainWindow();
        bool fullscreen = !gContext->GetNumSetting("GuiSizeForTV", 0);
        bool switchMode = gContext->GetNumSetting("UseVideoModes", 0);

        saved_gui_bounds = QRect(mainWindow->geometry().topLeft(), mainWindow->size());

#ifdef FLUXBOX_HACK
        // Qt doesn't work correctly when fluxbox is the window manager
        // Try to undo the 'off by one' error that results.
        if (!fullscreen && !gContext->GetNumSetting("RunFrontendInWindow", 0))
        {
            VERBOSE(VB_PLAYBACK, QString("Fluxbox: %1x%2")
                    .arg(saved_gui_bounds.x()).arg(saved_gui_bounds.y()));
            saved_gui_bounds.setX(saved_gui_bounds.x()-1);
            saved_gui_bounds.setY(saved_gui_bounds.y()-1);
        }
#endif
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
            mainWindow->resize(QSize(maxWidth, maxHeight));
            mainWindow->setFixedSize(QSize(maxWidth, maxHeight));

            // bit of a hack, but it's ok if the window is too
            // big in fullscreen mode
            if (fullscreen)
                player_bounds.setSize(QSize(maxWidth, maxHeight));
        }

        // player window sizing
        int flags = Qt::WStyle_Customize | Qt::WStyle_NormalBorder;
        if (gContext->GetNumSetting("RunFrontendInWindow", 0))
            flags = Qt::WStyle_Customize | Qt::WStyle_NoBorder;
        myWindow = new MythDialog(mainWindow, "video playback window", flags);

        myWindow->installEventFilter(this);
        myWindow->setNoErase();
        if (switchMode && display_res)
            myWindow->resize(QSize(maxWidth, maxHeight));
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
        if (!gContext->GetNumSetting("GuiSizeForTV", 0))
        {
            gContext->GetMainWindow()->resize(saved_gui_bounds.size());
            gContext->GetMainWindow()->setFixedSize(saved_gui_bounds.size());
            gContext->GetMainWindow()->show();
            gContext->GetMainWindow()->move(saved_gui_bounds.topLeft());
        }
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
    if (changeState)
        return kState_ChangingState;
    return internalState;
}

int TV::LiveTV(bool showDialogs)
{
    if (internalState == kState_None)
    {
        RemoteEncoder *testrec = RemoteRequestNextFreeRecorder(lastRecorderNum);

        if (!testrec)
            return 0;

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
 
            return 0;
        }

        activerecorder = recorder = testrec;
        lastRecorderNum = recorder->GetRecorderNumber();
        nextState = kState_WatchingLiveTV;
        changeState = true;
        switchingCards = false;
    }

    return 1;
}

void TV::FinishRecording(void)
{
    if (!IsRecording())
        return;

    activerecorder->FinishRecording();
}

void TV::AskAllowRecording(const QStringList &messages, int timeuntil)
{
    if (GetState() != kState_WatchingLiveTV)
       return;

    QString title = messages[0];
    QString chanstr = messages[1];
    QString chansign = messages[2];
    QString channame = messages[3];

    QString channel = gContext->GetSetting("ChannelFormat", "<num> <sign>");
    channel.replace("<num>", chanstr)
        .replace("<sign>", chansign)
        .replace("<name>", channame);
    
    QString message = QObject::tr("MythTV wants to record \"%1\" on %2"
                                  " in %3 seconds. Do you want to:")
                                 .arg(title)
                                 .arg(channel)
                                 .arg(" %d ");
    
    while (!osd)
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
    osd->NewDialogBox(dialogname, message, options, timeuntil); 

}

int TV::Playback(ProgramInfo *rcinfo)
{
    if (internalState != kState_None)
        return 0;

    inputFilename = rcinfo->pathname;

    playbackLen = rcinfo->CalculateLength();
    playbackinfo = rcinfo;

    int overrecordseconds = gContext->GetNumSetting("RecordOverTime");
    QDateTime curtime = QDateTime::currentDateTime();
    QDateTime endts = rcinfo->endts.addSecs(overrecordseconds);

    if (curtime < endts && !rcinfo->isVideo)
        nextState = kState_WatchingRecording;
    else
        nextState = kState_WatchingPreRecorded;

    changeState = true;

    if (class LCD * lcd = LCD::Get())
        lcd->switchToChannel(rcinfo->chansign, rcinfo->title, rcinfo->subtitle);

    return 1;
}

int TV::PlayFromRecorder(int recordernum)
{
    int retval = 0;

    if (recorder)
    {
        cerr << "PlayFromRecorder (" << recordernum << ") : recorder already exists!";
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

    delete recorder;
    recorder = NULL;

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

void TV::StateToString(TVState state, QString &statestr)
{
    switch (state) {
        case kState_None: statestr = "None"; break;
        case kState_WatchingLiveTV: statestr = "WatchingLiveTV"; break;
        case kState_WatchingPreRecorded: statestr = "WatchingPreRecorded";
                                         break;
        case kState_WatchingRecording: statestr = "WatchingRecording"; break;
        case kState_RecordingOnly: statestr = "RecordingOnly"; break;
        default: statestr = "Unknown"; break;
    }
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

void TV::HandleStateChange(void)
{
    if (IsErrored())
    {
        VERBOSE(VB_IMPORTANT, "TV::HandleStateChange() Error, "
                "called after fatal error detected.");
        return;
    }

    bool changed = false;

    TVState tmpInternalState = internalState;

    QString statename;
    StateToString(nextState, statename);
    QString origname;
    StateToString(tmpInternalState, origname);

    if (nextState == kState_Error)
    {
        VERBOSE(VB_IMPORTANT, "TV::HandleStateChange() Error, "
                "attempting to set to an error state, exiting");
        errored = true;
        return;
    }

    if (internalState == kState_None && nextState == kState_WatchingLiveTV)
    {
        long long filesize = 0;
        long long smudge = 0;
        QString name = "";

        recorder->Setup();
        recorder->SetupRingBuffer(name, filesize, smudge);

        prbuffer = new RingBuffer(name, filesize, smudge, recorder);

        tmpInternalState = nextState;
        changed = true;

        persistentbrowsemode =
            gContext->GetNumSetting("PersistentBrowseMode", 0);

        recorder->SpawnLiveTV();

        gContext->DisableScreensaver();
        StartPlayerAndRecorder(true, false);
        if (recorder->IsRecording())
            StartPlayerAndRecorder(false, true);
        else
        {
            VERBOSE(VB_IMPORTANT, "LiveTV not successfully started");
            tmpInternalState = internalState;
            nextState = internalState;
            StopPlayerAndRecorder(true, false);
            gContext->RestoreScreensaver();
            recorder = NULL;
        }
    }
    else if (internalState == kState_WatchingLiveTV && 
             nextState == kState_None)
    {
        tmpInternalState = nextState;
        changed = true;

        StopPlayerAndRecorder(true, true);
        gContext->RestoreScreensaver();
    }
    else if (internalState == kState_WatchingRecording &&
             nextState == kState_WatchingPreRecorded)
    {
        tmpInternalState = nextState;
        changed = true;
    }
    else if ((internalState == kState_None && 
              nextState == kState_WatchingPreRecorded) ||
             (internalState == kState_None &&
              nextState == kState_WatchingRecording))
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
    
            if (nextState == kState_WatchingRecording)
            {
                recorder = RemoteGetExistingRecorder(playbackinfo);
                if (!recorder || !recorder->IsValidRecorder())
                {
                    cerr << "ERROR: couldn't find recorder for in-progress "
                         << "recording\n";
                    nextState = kState_WatchingPreRecorded;
                    if (recorder)
                        delete recorder;
                    activerecorder = recorder = NULL;
                }
                else
                {
                    activerecorder = recorder;
                    recorder->Setup();
                }
            }
    
            tmpInternalState = nextState;
            changed = true;
    
            StartPlayerAndRecorder(true, false);
        }
        else
        {
            nextState = kState_None;
        }
    }
    else if ((internalState == kState_WatchingPreRecorded && 
              nextState == kState_None) || 
             (internalState == kState_WatchingRecording &&
              nextState == kState_None))
    {
        if (internalState == kState_WatchingRecording)
            recorder->StopPlaying();

        tmpInternalState = nextState;
        changed = true;

        StopPlayerAndRecorder(true, false);
        gContext->RestoreScreensaver();
    }
    else if (internalState == kState_None && 
             nextState == kState_None)
    {
        changed = true;
    }

    if (!changed)
    {
        VERBOSE(VB_IMPORTANT, QString("Unknown state transition: %1 to %2")
                .arg(internalState).arg(nextState));
    }
    else
    {
        VERBOSE(VB_GENERAL, QString("Changing from %1 to %2")
                .arg(origname).arg(statename));
    }

    if (kState_None != nextState && activenvp && 
        !activenvp->IsDecoderThreadAlive())
    {
        VERBOSE(VB_IMPORTANT, "Decoder not alive, and trying to play..");
        if (nextState == kState_WatchingLiveTV)
        {
            StopPlayerAndRecorder(false, true);
            recorder = NULL;
        }

        tmpInternalState = kState_None;
    }

    internalState = tmpInternalState;
    changeState = false;

    if (internalState == kState_WatchingLiveTV)
    {
        UpdateOSDInput();
        UpdateLCD();
    }
    if (recorder)
        recorder->FrontendReady();
}

void TV::StartPlayerAndRecorder(bool startPlayer, bool startRecorder)
{ 
    if (startRecorder)
    {
        while (!recorder->IsRecording() && !exitPlayer)
            usleep(50);
        if (exitPlayer)
            return;
        frameRate = recorder->GetFrameRate();
    }

    if (startPlayer)
    {
        SetupPlayer();
        pthread_create(&decode, NULL, SpawnDecode, nvp);

        while (!nvp->IsPlaying() && nvp->IsDecoderThreadAlive())
            usleep(50);

        activenvp = nvp;
        activerbuffer = prbuffer;
        activerecorder = recorder;

        frameRate = nvp->GetFrameRate();
        osd = nvp->GetOSD();
        if (osd)
            osd->SetUpOSDClosedHandler(this);
    }
}

void TV::StopPlayerAndRecorder(bool closePlayer, bool closeRecorder)
{
    if (closePlayer)
    {
        if (prbuffer)
        {
            prbuffer->StopReads();
            prbuffer->Pause();
            prbuffer->WaitForPause();
        }

        if (nvp)
            nvp->StopPlaying();

        if (piprbuffer)
        {
            piprbuffer->StopReads();
            piprbuffer->Pause();
            piprbuffer->WaitForPause();
        }

        if (pipnvp)
            pipnvp->StopPlaying();
    }

    if (closeRecorder)
    {
        recorder->StopLiveTV();
        if (piprecorder > 0)
            piprecorder->StopLiveTV();
    }

    if (closePlayer)
    {
        TeardownPlayer();
        if (pipnvp)
            TeardownPipPlayer();
    }
}

void TV::SetupPlayer(void)
{
    if (nvp)
    { 
        printf("Attempting to setup a player, but it already exists.\n");
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

    autoCommercialSkip = gContext->GetNumSetting("AutoCommercialSkip", 0);
    nvp->SetAutoCommercialSkip(autoCommercialSkip);
    nvp->SetCommercialSkipMethod(gContext->GetNumSetting("CommercialSkipMethod",
                                                         COMM_DETECT_BLANKS));

    tryUnflaggedSkip = gContext->GetNumSetting("TryUnflaggedSkip", 0);
    nvp->SetTryUnflaggedSkip(tryUnflaggedSkip);

    osd_display_time = gContext->GetNumSetting("OSDDisplayTime");

    if (gContext->GetNumSetting("DefaultCCMode"))
        nvp->ToggleCC(vbimode, 0);

    filters = getFiltersForChannel();
    nvp->SetVideoFilters(filters);

    if (embedid > 0)
        nvp->EmbedInWidget(embedid, embx, emby, embw, embh);

    if (nextState == kState_WatchingRecording)
        nvp->SetWatchingRecording(true);

    osd = NULL;

    int udp_port = gContext->GetNumSetting("UDPNotifyPort");
    if (udp_port > 0)
        udpnotify = new UDPNotify(this, udp_port);
    else
        udpnotify = NULL;
}


QString TV::getFiltersForChannel()
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
        printf("Attempting to setup a pip player, but it already exists.\n");
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
        // Stop the player's video sync method.  Do so from this
        // main thread to work around a potential OpenGL bug.
        VideoSync *vs = nvp->getVideoSync();
        if (vs != NULL)
            vs->Stop();
        pthread_join(decode, NULL);
        delete nvp;
    }

    if (udpnotify)
        delete udpnotify;

    paused = false;
    doing_ff_rew = 0;
    ff_rew_index = SSPEED_NORMAL;
    speed_index = 0;
    sleep_index = 0;
    normal_speed = 1.0;

    nvp = activenvp = NULL;
    osd = NULL;
 
    playbackinfo = NULL;
 
    if (recorder) 
        delete recorder; 

    recorder = activerecorder = NULL;
 
    if (prbuffer)
    {
        delete prbuffer;
        prbuffer = activerbuffer = NULL;
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
                      "context = \"TV Playback\" AND action = \"GUIDE\" AND "
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

    stickykeys = gContext->GetNumSetting("StickyKeys");
    ff_rew_repos = gContext->GetNumSetting("FFRewRepos", 1);
    ff_rew_reverse = gContext->GetNumSetting("FFRewReverse", 1);
    smartForward = gContext->GetNumSetting("SmartForward", 0);

    doing_ff_rew = 0;
    ff_rew_index = SSPEED_NORMAL;
    speed_index = 0;
    normal_speed = 1.0;
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
        if (changeState)
            HandleStateChange();

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

        if (nvp)
        {
            if (keyList.count() > 0)
            { 
                keyListLock.lock();
                keypressed = keyList.first();
                keyList.removeFirst();
                keyListLock.unlock();

                ProcessKeypress(keypressed);
                delete keypressed;
            }
        }

        if ((recorder && recorder->GetErrorStatus()) || IsErrored())
        {
            exitPlayer = true;
            wantsToQuit = true;
        }

        if (StateIsPlaying(internalState))
        {
            if (!nvp->IsPlaying())
            {
                nextState = RemovePlaying(internalState);
                changeState = true;
                endOfRecording = true;
                VERBOSE(VB_PLAYBACK, ">> Player timeout");
            }
        }

        if (exitPlayer)
        {
            while (osd->DialogShowing(dialogname))
            {
                osd->DialogAbort(dialogname);
                osd->TurnDialogOff(dialogname);
                usleep(1000);
            }
            nextState = kState_None;
            changeState = true;
            exitPlayer = false;
        }

        if ((doing_ff_rew || speed_index) && 
            activenvp->AtNormalSpeed())
        {
            speed_index = 0;
            doing_ff_rew = 0;
            ff_rew_index = SSPEED_NORMAL;
            UpdatePosOSD(0.0, tr("Play"));
        }

        if (++updatecheck >= 20)
        {
            OSDSet *oset;
            if (osd && (oset = osd->GetSet("status")) &&
                oset->Displaying() && update_osd_pos &&
                (internalState == kState_WatchingLiveTV || 
                 internalState == kState_WatchingRecording ||
                 internalState == kState_WatchingPreRecorded))
            {
                QString desc = "";
                int pos = nvp->calcSliderPos(desc);
                osd->UpdatePause(pos, desc);
            }

            updatecheck = 0;
        }

        if (channelqueued && nvp && nvp->GetOSD())
        {
            OSDSet *set = osd->GetSet("channel_number");
            if ((set && !set->Displaying()) || !set)
            {
                if (internalState == kState_WatchingLiveTV)
                    ChannelCommit();
                else
                    ChannelClear();
            }
        }

        if (class LCD * lcd = LCD::Get())
        {
            if (lastLcdUpdate.secsTo(QDateTime::currentDateTime()) < 60)
                continue;

            float progress = 0.0;

            if (internalState == kState_WatchingLiveTV)
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
  
    if (!IsErrored())
    {
        nextState = kState_None;
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

    if (nvp->GetOSD() && osd->IsRunningTreeMenu())
    {
        osd->TreeMenuHandleKeypress(e);
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

    if (nvp->GetOSD() && dialogname != "" && osd->DialogShowing(dialogname))
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            action = actions[i];
            handled = true;

            if (action == "UP")
                osd->DialogUp(dialogname); 
            else if (action == "DOWN")
                osd->DialogDown(dialogname);
            else if (action == "SELECT" || action == "ESCAPE" || 
                     action == "CLEAROSD" ||
                     ((arrowAccel) && (action == "LEFT" || action == "RIGHT")))
            {
                if (action == "ESCAPE" || action == "CLEAROSD" ||
                    (arrowAccel && action == "LEFT"))
                    osd->DialogAbort(dialogname);
                osd->TurnDialogOff(dialogname);
                if (dialogname == "alreadybeingedited")
                {
                    int result = osd->GetDialogResponse(dialogname);
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
                    int result = osd->GetDialogResponse(dialogname);

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
                    int result = osd->GetDialogResponse(dialogname);

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
                    if (osd->GetDialogResponse(dialogname) == 1)
                        ChangeChannel(lastCCDir, true);
                    else if (!paused)
                        activenvp->Play(normal_speed, true);
                }     
                else if (dialogname == "ccwarningstring")
                {
                    if (osd->GetDialogResponse(dialogname) == 1)
                        ChangeChannelByString(lastCC, true);
                    else if (!paused)
                        activenvp->Play(normal_speed, true);
                }
                else if (dialogname == "allowrecordingbox")
                {
                    int result = osd->GetDialogResponse(dialogname);
    
                    if (result == 2)
                        StopLiveTV();
                    else if (result == 3)
                        recorder->CancelNextRecording();
                }

                while (osd->DialogShowing(dialogname))
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
            else if (action == "TOGGLESTRETCH")
                ClearOSD();
            else
                handled = false;
        }
    }
   
    if (handled)
        return;

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        action = actions[i];
        handled = true;

        if (action == "TOGGLECC")
            DoToggleCC(0);
        else if (action == "SKIPCOMMERCIAL")
            DoSkipCommercials(1);
        else if (action == "SKIPCOMMBACK")
            DoSkipCommercials(-1);
        else if (action == "QUEUETRANSCODE")
            DoQueueTranscode();
        else if (action == "PLAY")
            DoPlay();
        else if (action == "NEXTAUDIO")
        {
            if(activenvp)
            {
                activenvp->incCurrentAudioTrack();
                if ( activenvp->getCurrentAudioTrack() )
                {
                    QString msg = QString("%1 %2")
                                  .arg(tr("Audio track"))
                                  .arg(activenvp->getCurrentAudioTrack());

                    osd->SetSettingsText(msg, 3);
                }
            }
        }
        else if (action == "PREVAUDIO")
        {
            if(activenvp)
            {
                activenvp->decCurrentAudioTrack();
                if ( activenvp->getCurrentAudioTrack() )
                {
                    QString msg = QString("%1 %2")
                                  .arg(tr("Audio track"))
                                  .arg(activenvp->getCurrentAudioTrack());

                    osd->SetSettingsText(msg, 3);
                }

            }
        }
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
                DoArbSeek(1);
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
            if (paused)
                DoSeek(1.0, tr("Forward"));
            else
                ChangeFFRew(1);
        }
        else if (action == "SEEKRWND")
        {
            if (channelqueued)
                DoArbSeek(-1);
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
            if (paused)
                DoSeek(-1.0, tr("Rewind"));
            else
                ChangeFFRew(-1);
        }
        else if (action == "JUMPRWND")
            DoSeek(-jumptime * 60, tr("Jump Back"));
        else if (action == "JUMPFFWD")
            DoSeek(jumptime * 60, tr("Jump Ahead"));
        else if (action == "JUMPSTART" && activenvp)
        {
            DoSeek(-activenvp->GetFramesPlayed(), tr("Jump to Beginning"));
        }
        else if (action == "CLEAROSD")
        {
            ClearOSD();
        }
        else if (action == "ESCAPE")
        {
            if (osd && ClearOSD())
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
                osd->NewDialogBox(dialogname, message, options, 0);
            }
            else if(StateIsPlaying(internalState) &&
                  gContext->GetNumSetting("PlaybackExitPrompt") == 1 && 
                  playbackinfo && playbackinfo->isVideo)
            {
                nvp->Pause();

                QString vmessage = tr("You are exiting this video");

                QStringList voptions;
                voptions += tr("Exit to the menu");
                voptions += tr("Keep watching");
                dialogname = "videoexitplayoptions";
                osd->NewDialogBox(dialogname, vmessage, voptions, 0);
            } 
            else 
            {
                if (gContext->GetNumSetting("PlaybackExitPrompt") == 2)
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

                if (ok && val < SSPEED_MAX)
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

    if (internalState == kState_WatchingLiveTV)
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            action = actions[i];
            handled = true;

            if (action == "INFO")
            {
                if (channelqueued)
                    DoArbSeek(0);
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
                    DoArbSeek(0);
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
                DoSeek(-jumptime * 60, tr("Jump Back"));
            else if (action == "CHANNELDOWN")
                DoSeek(jumptime * 60, tr("Jump Ahead"));
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
        piprecorder->SetupRingBuffer(name, filesize, smudge, true);

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

    if (paused)
        return;

    oset = osd->GetSet("status");
    if ((oset) && (oset->Displaying()))
    {
        QMap<QString, QString> infoMap;
        playbackinfo->ToMap(infoMap);
        osd->HideSet("status");
        osd->ClearAllText("program_info");
        osd->SetText("program_info", infoMap, osd_display_time);
    }
    else
    {
        QString desc = "";
        int pos = nvp->calcSliderPos(desc);
        osd->HideSet("program_info");
        osd->StartPause(pos, false, tr("Position"), desc, osd_display_time);
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
        bool slidertype = (internalState == kState_WatchingLiveTV);
        int osdtype = (doSmartForward) ? kOSDFunctionalType_SmartForward :
                                         kOSDFunctionalType_Default;
        osd->StartPause(pos, slidertype, mesg, desc, disptime, osdtype);
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

void TV::DoArbSeek(int dir)
{
    int chan = QueuedChannel().toInt();
    ChannelClear(true);

    float time = ((chan / 100) * 3600) + ((chan % 100) * 60);

    if (dir > 0)
        DoSeek(time, tr("Jump Ahead"));
    else if (dir < 0)
        DoSeek(-time, tr("Jump Back"));
    else
    {
        time -= (activenvp->GetFramesPlayed() - 1) / frameRate;
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
        case  0: speed = 1.0;      mesg = QString(tr("Play"));        break;
        case -1: speed = 1.0 / 3;  mesg = QString(tr("Speed 1/3X"));  break;
        case -2: speed = 1.0 / 8;  mesg = QString(tr("Speed 1/8X"));  break;
        case -3: speed = 1.0 / 16; mesg = QString(tr("Speed 1/16X")); break;
        case -4: DoPause(); return; break;
        default: speed_index = old_speed; return; break;
    }

    if (!activenvp->Play(speed, (speed == 1.0)))
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

    if (ff_rew_repos)
    {
        if (doing_ff_rew > 0)
            time = -seek_speed_array[repoLevel][ff_rew_index].ff_repos;
        else
            time = seek_speed_array[repoLevel][ff_rew_index].rew_repos;
    }

    doing_ff_rew = 0;
    ff_rew_index = SSPEED_NORMAL;

    activenvp->Play(normal_speed, true);

    return time;
}

void TV::ChangeFFRew(int direction)
{
    if (doing_ff_rew == direction)
        SetFFRew((ff_rew_index + 1) % SSPEED_MAX);
    else if (!ff_rew_reverse && doing_ff_rew == -direction)
    {
        if (ff_rew_index > SSPEED_NORMAL)
            SetFFRew((ff_rew_index - 1) % SSPEED_MAX);
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
        SetFFRew(SSPEED_NORMAL);
    }
}

void TV::SetFFRew(int index)
{
    if (!doing_ff_rew)
        return;

    ff_rew_index = index;
    float speed;

    QString mesg;
    if (doing_ff_rew > 0)
    {
        mesg = tr("Forward ");
        speed = 1.0;
    }
    else
    {
        mesg = tr("Rewind ");
        speed = -1.0;
    }

    mesg += seek_speed_array[repoLevel][ff_rew_index].dispString;
    speed *= seek_speed_array[repoLevel][ff_rew_index].scaling;

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
                                        playbackinfo->startts))
            stop = true;
        if (stop)
        {
            JobQueue::ChangeJobCmds(JOB_TRANSCODE,
                                    playbackinfo->chanid,
                                    playbackinfo->startts, JOB_STOP);
            queuedTranscode = false;
            if (activenvp == nvp)
                osd->SetSettingsText(tr("Stopping Transcode"), 3);
        }
        else
        {
            QString jobHost = "";

            if (gContext->GetNumSetting("JobsRunOnRecordHost", 0))
                jobHost = playbackinfo->hostname;

            if (JobQueue::QueueJob(JOB_TRANSCODE,
                               playbackinfo->chanid, playbackinfo->startts,
                               jobHost, "", "", JOB_USE_CUTLIST))
            {
                queuedTranscode = true;
                if (activenvp == nvp)
                    osd->SetSettingsText(tr("Transcoding"), 3);
            } else {
                if (activenvp == nvp)
                    osd->SetSettingsText(tr("Try Again"), 3);
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

    bool muted = false;

    AudioOutput *aud = nvp->getAudioOutput();
    if (aud && !aud->GetMute())
    {
        aud->ToggleMute();
        muted = true;
    }

    bool slidertype = false;
    if (internalState == kState_WatchingLiveTV)
        slidertype = true;

    if (activenvp == nvp)
    {
        QString dummy = "";
        QString desc = tr("Searching...");
        int pos = nvp->calcSliderPos(dummy);
        osd->StartPause(pos, slidertype, tr("Skip"), desc, 6);
        update_osd_pos = true;
    }

    activenvp->SkipCommercials(direction);

    if (muted) 
        muteTimer->start(kMuteTimeout, true);
}

void TV::SwitchCards(void)
{
    if (internalState == kState_WatchingLiveTV)
    {
        switchingCards = true;
        exitPlayer = true;
    }
}

void TV::ToggleInputs(void)
{
    if (activenvp == nvp)
    {
        if (paused)
        {
            osd->EndPause();
            gContext->DisableScreensaver();
            paused = false;
        }
    }

    activenvp->Pause(false);

    // all we care about is the ringbuffer being paused, here..
    activerbuffer->WaitForPause();

    activerecorder->Pause();
    activerbuffer->Reset();
    activerecorder->ToggleInputs();

    activenvp->ResetPlaying();
    activenvp->Play(normal_speed, true, false);

    if (activenvp == nvp)
    {
        UpdateOSDInput();
        UpdateLCD();
    }
}

void TV::ToggleChannelFavorite(void)
{
    activerecorder->ToggleChannelFavorite();
}

void TV::ChangeChannel(int direction, bool force)
{
    bool muted = false;

    if (!force && (activenvp == nvp) && showBufferedWarnings)
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

            osd->NewDialogBox(dialogname, message, options, 0);
            return;
        }
    }

    AudioOutput *aud = nvp->getAudioOutput();
    if (aud && !aud->GetMute() && activenvp == nvp)
    {
        aud->ToggleMute();
        muted = true;
    }

    if (activenvp == nvp)
    {
        if (paused)
        {
            osd->EndPause();
            gContext->DisableScreensaver();
            paused = false;
        }
    }

    activenvp->Pause(false);
    // all we care about is the ringbuffer being paused, here..
    activerbuffer->WaitForPause();    

    // Save the current channel if this is the first time
    if (channame_vector.size() == 0 && activenvp == nvp)
        AddPreviousChannel();

    activerecorder->Pause();
    activerbuffer->Reset();
    activerecorder->ChangeChannel(direction);

    activenvp->ResetPlaying();
    
    QString filters = getFiltersForChannel();
    activenvp->SetVideoFilters(filters);
    
    activenvp->Play(normal_speed, true, false);

    if (activenvp == nvp)
    {
        UpdateOSD();
        UpdateLCD();
        AddPreviousChannel();
    }

    ChannelClear();

    if (muted)
        muteTimer->start(kMuteTimeout * 2, true);
}

QString TV::QueuedChannel(void)
{
    if (!channelqueued)
        return "";

    for (int i = 0; i < channelkeysstored; i++)
    {
        if (channelKeys[i] == '0')
            channelKeys[i] = ' ';
        else
            break;
    }

    return QString(channelKeys).stripWhiteSpace();
}

void TV::ChannelClear(bool hideosd)
{
    if (hideosd && osd) 
        osd->HideSet("channel_number");

    channelqueued = false;
    channelKeys[0] = channelKeys[1] = channelKeys[2] = channelKeys[3] = ' ';
    channelKeys[4] = 0;
    channelkeysstored = 0;
}

void TV::ChannelKey(char key)
{

    if (channelkeysstored == 4)
    {
        channelKeys[0] = channelKeys[1];
        channelKeys[1] = channelKeys[2];
        channelKeys[2] = channelKeys[3];
        channelKeys[3] = key;
    }
    else
    {
        channelKeys[channelkeysstored] = key; 
        channelkeysstored++;
    }
    channelKeys[4] = 0;

    channelqueued = true;

    bool unique = false;

    /* 
     * Always use smartChannelChange when channel numbers are entered in
     * browse mode because in browse mode space/enter exit browse mode and
     * change to the currently browsed channel. This makes smartChannelChange
     * the only way to enter a channel number to browse without waiting for the
     * OSD to fadeout after entering numbers.
     */
    if (internalState == kState_WatchingLiveTV && 
        (smartChannelChange || browsemode))
    {
        char *chan_no_leading_zero = NULL;
        for (int i = 0; i < channelkeysstored; i++)
        {
            if (channelKeys[i] != '0') 
            {
                chan_no_leading_zero = &channelKeys[i];
                break;
            }
        }

        if (chan_no_leading_zero)
        {
            QString chan = QString(chan_no_leading_zero).stripWhiteSpace();
            if (!activerecorder->CheckChannelPrefix(chan, unique))
            {
                channelKeys[0] = key;
                channelKeys[1] = channelKeys[2] = channelKeys[3] = ' ';
                channelkeysstored = 1;
            }
        }
    }

    if (activenvp == nvp && osd)
    {
        QMap<QString, QString> infoMap;

        infoMap["channum"] = channelKeys;
        infoMap["callsign"] = "";
        osd->ClearAllText("channel_number");
        osd->SetText("channel_number", infoMap, 2);
    }

    if (unique)
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
        if (activenvp == nvp && osd)
            osd->HideSet("channel_number");
    }
    else
        ChangeChannelByString(chan);

    ChannelClear();
}

void TV::ChangeChannelByString(QString &name, bool force)
{
    bool muted = false;

    if (!channame_vector.empty() && channame_vector.back() == name)
        return;

    if (!activerecorder->CheckChannel(name))
        return;

    if (!force && (activenvp == nvp) && showBufferedWarnings)
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

            osd->NewDialogBox(dialogname, message, options, 0);
            return;
        }
    }

    AudioOutput *aud = nvp->getAudioOutput();
    if (aud && !aud->GetMute() && activenvp == nvp)
    {
        aud->ToggleMute();
        muted = true;
    }

    if (activenvp == nvp)
    {
        if (paused)
        {
            osd->EndPause();
            gContext->DisableScreensaver();
            paused = false;
        }
    }

    activenvp->Pause(false);
    // all we care about is the ringbuffer being paused, here..
    activerbuffer->WaitForPause();

    // Save the current channel if this is the first time
    if (channame_vector.size() == 0)
        AddPreviousChannel();

    activerecorder->Pause();
    activerbuffer->Reset();
    activerecorder->SetChannel(name);

    activenvp->ResetPlaying();
    
    QString filters = getFiltersForChannel();
    activenvp->SetVideoFilters(filters);
    
    activenvp->Play(normal_speed, true, false);

    if (activenvp == nvp)
    {
        UpdateOSD();
        UpdateLCD();
        AddPreviousChannel();
    }

    if (muted)
        muteTimer->start(kMuteTimeout * 2, true);
}

void TV::AddPreviousChannel(void)
{
    // Don't store more than thirty channels.  Remove the first item
    if (channame_vector.size() > 29)
    {
        PrevChannelVector::iterator it;
        it = channame_vector.begin();
        channame_vector.erase(it);
    }

    QString chan_name = activerecorder->GetCurrentChannel();

    // This method builds the stack of previous channels
    channame_vector.push_back(chan_name);
}

void TV::PreviousChannel(void)
{
    // Save the channel if this is the first time, and return so we don't
    // change chan to the current chan
    if (channame_vector.size() == 0)
        return;

    // Increment the times_pressed counter so we know how far to jump
    times_pressed++;

    //Figure out the vector the desired channel is in
    int vector = (channame_vector.size() - times_pressed - 1) % 
                 channame_vector.size();

    // Display channel name in the OSD for up to 1 second.
    if (activenvp == nvp && osd)
    {
        osd->HideSet("program_info");

        QMap<QString, QString> infoMap;

        infoMap["channum"] = channame_vector[vector];
        infoMap["callsign"] = "";
        osd->ClearAllText("channel_number");
        osd->SetText("channel_number", infoMap, 1);
    }

    // Reset the timer
    prevChannelTimer->stop();
    prevChannelTimer->start(750);
}

void TV::SetPreviousChannel()
{
    // Stop the timer
    prevChannelTimer->stop();

    // Figure out the vector the desired channel is in
    int vector = (channame_vector.size() - times_pressed - 1) % 
                 channame_vector.size();

    // Reset the times_pressed counter
    times_pressed = 0;

    // Only change channel if channame_vector[vector] != current channel
    QString chan_name = activerecorder->GetCurrentChannel();

    if (chan_name != channame_vector[vector].latin1())
    {
        // Populate the array with the channel
        for(uint i = 0; i < channame_vector[vector].length(); i++)
        {
            channelKeys[i] = (int)*channame_vector[vector].mid(i, 1).latin1();
        }

        channelqueued = true;
    }

    //Turn off the OSD Channel Num so the channel changes right away
    if (activenvp == nvp && osd)
        osd->HideSet("channel_number");
}

bool TV::ClearOSD(void)
{
    bool res = false;

    if (channelqueued)
    {
        ChannelClear(true);
        res = true;
    }

    if (osd->HideAll())
        res = true;

    while (res && osd->HideAll())
        usleep(1000);

    return res;
}

void TV::ToggleOSD(void)
{
    OSDSet *oset = osd->GetSet("program_info");
    if (osd && (oset) && oset->Displaying())
        osd->HideAll();
    else
        UpdateOSD();
}

void TV::UpdateOSD(void)
{
    QMap<QString, QString> infoMap;

    GetChannelInfo(activerecorder, infoMap);

    int behind = activenvp->GetSecondsBehind();
    QDateTime now = QDateTime::currentDateTime();
    QDateTime infoMapTime = QDateTime::fromString(infoMap["startts"],
                                                  Qt::ISODate);
    QDateTime curPlaybackTime = now.addSecs(0-behind);
    ProgramInfo *curPlaybackInfo;

    if (curPlaybackTime < infoMapTime)
    {
        curPlaybackInfo = ProgramInfo::GetProgramAtDateTime(infoMap["chanid"], 
                                                            curPlaybackTime);
        if (curPlaybackInfo)
            curPlaybackInfo->ToMap(infoMap);
    }

    osd->ClearAllText("program_info");
    osd->SetText("program_info", infoMap, osd_display_time);
    osd->ClearAllText("channel_number");
    osd->SetText("channel_number", infoMap, osd_display_time);
}

void TV::UpdateOSDInput(void)
{
    QString name = "";

    activerecorder->GetInputName(name);
    QString msg = QString ("%1: %2").arg(activerecorder->GetRecorderNumber())
                                    .arg(name);
    if (osd)
        osd->SetSettingsText(msg, 3);
    else
        VERBOSE(VB_OSD, QString("TV: Unable to display input ID (%1) on OSD.")
                                .arg(msg));
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

void TV::GetNextProgram(RemoteEncoder *enc, int direction,
                        QMap<QString, QString> &infoMap)
{
    if (!enc)
        enc = activerecorder;

    QString title, subtitle, description, category, starttime, endtime;
    QString callsign, iconpath, channum, chanid, seriesid, programid;

    starttime = infoMap["dbstarttime"];
    chanid = infoMap["chanid"];
    channum = infoMap["channum"];
    seriesid = infoMap["seriesid"];
    programid = infoMap["programid"];

    enc->GetNextProgram(direction,
                        title, subtitle, description, category, starttime,
                        endtime, callsign, iconpath, channum, chanid,
                        seriesid, programid);
    
          
    QString tmFmt = gContext->GetSetting("TimeFormat");
    QString dtFmt = gContext->GetSetting("ShortDateFormat");
    QDateTime startts = QDateTime::fromString(starttime, Qt::ISODate);
    QDateTime endts = QDateTime::fromString(endtime, Qt::ISODate);
    QString length;
    int hours, minutes, seconds;

    infoMap["dbstarttime"] = starttime;
    infoMap["dbendtime"] = endtime;
    infoMap["title"] = title;
    infoMap["subtitle"] = subtitle;
    infoMap["description"] = description;
    infoMap["category"] = category;
    infoMap["callsign"] = callsign;
    infoMap["starttime"] = startts.toString(tmFmt);
    infoMap["startdate"] = startts.toString(dtFmt);
    infoMap["endtime"] = endts.toString(tmFmt);
    infoMap["enddate"] = endts.toString(dtFmt);
    infoMap["channum"] = channum;
    infoMap["chanid"] = chanid;
    infoMap["iconpath"] = iconpath;
    infoMap["seriesid"] = seriesid;
    infoMap["programid"] = programid;

    seconds = startts.secsTo(endts);
    minutes = seconds / 60;
    infoMap["lenmins"] = QString("%1 %2").arg(minutes).arg(tr("minutes"));
    hours   = minutes / 60;
    minutes = minutes % 60;
    length.sprintf("%d:%02d", hours, minutes);
    infoMap["lentime"] = length;
    
}

void TV::GetNextProgram(RemoteEncoder *enc, int direction,
                        QString &title, QString &subtitle, 
                        QString &desc, QString &category, QString &starttime, 
                        QString &endtime, QString &callsign, QString &iconpath,
                        QString &channelname, QString &chanid,
                        QString &seriesid, QString &programid)
{
    if (!enc)
        enc = activerecorder;

    enc->GetNextProgram(direction,
                        title, subtitle, desc, category, starttime, endtime, 
                        callsign, iconpath, channelname, chanid,
                        seriesid, programid);
}

void TV::GetChannelInfo(RemoteEncoder *enc, QMap<QString, QString> &infoMap)
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
    QDateTime startts = QDateTime::fromString(starttime, Qt::ISODate);
    QDateTime endts = QDateTime::fromString(endtime, Qt::ISODate);
    QString length;
    int hours, minutes, seconds;

    infoMap["startts"] = starttime;
    infoMap["endts"] = endtime;
    infoMap["dbstarttime"] = starttime;
    infoMap["dbendtime"] = endtime;
    infoMap["title"] = title;
    infoMap["subtitle"] = subtitle;
    infoMap["description"] = description;
    infoMap["category"] = category;
    infoMap["callsign"] = callsign;
    infoMap["starttime"] = startts.toString(tmFmt);
    infoMap["startdate"] = startts.toString(dtFmt);
    infoMap["endtime"] = endts.toString(tmFmt);
    infoMap["enddate"] = endts.toString(dtFmt);
    infoMap["channum"] = channum;
    infoMap["chanid"] = chanid;
    infoMap["iconpath"] = iconpath;
    infoMap["seriesid"] = seriesid;
    infoMap["programid"] = programid;

    seconds = startts.secsTo(endts);
    minutes = seconds / 60;
    infoMap["lenmins"] = QString("%1 %2").arg(minutes).arg(tr("minutes"));
    hours   = minutes / 60;
    minutes = minutes % 60;
    length.sprintf("%d:%02d", hours, minutes);
    
    infoMap["lentime"] = length;
    
    
    if (stars.toFloat())
        infoMap["stars"] = QString("(%1 %2) ").arg(4.0 * stars.toFloat()).arg(QObject::tr("stars"));
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

    enc->GetChannelInfo(title, subtitle, desc, category, starttime, endtime, 
                        callsign, iconpath, channelname, chanid,
                        seriesid, programid, outFilters, repeat, airdate,
                        stars);
}

void TV::EmbedOutput(WId wid, int x, int y, int w, int h)
{
    embedid = wid;
    embx = x;
    emby = y;
    embw = w;
    embh = h;

    if (nvp)
        nvp->EmbedInWidget(wid, x, y, w, h);
}

void TV::StopEmbeddingOutput(void)
{
    if (nvp)
        nvp->StopEmbedding();
    embedid = 0;
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

    QString chanstr = RunProgramGuide(channame, true, this, allowsecondary);

    if (chanstr != "")
    {
        chanstr = chanstr.left(4);
        sprintf(channelKeys, "%s", chanstr.ascii());
        channelqueued = true; 
    }

    StopEmbeddingOutput();

    if (fullscreen) 
    {
        mwnd->setGeometry(0, 0, QApplication::desktop()->width(),
                          QApplication::desktop()->height());
        mwnd->setFixedSize(QSize(QApplication::desktop()->width(),
                                 QApplication::desktop()->height()));
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

    if (osd)
    {
        if (recorder)
        {
            brightness = activerecorder->ChangeBrightness(up);
            text = QString(tr("Brightness (REC) %1 %")).arg(brightness);
            osd->StartPause(brightness * 10, true, tr("Adjust Recording"),
                text, 5, kOSDFunctionalType_RecPictureAdjust);
        }
        else
        {
            brightness = nvp->getVideoOutput()->ChangeBrightness(up);
            gContext->SaveSetting("PlaybackBrightness", brightness);
            text = QString(tr("Brightness %1 %")).arg(brightness);
            osd->StartPause(brightness * 10, true, tr("Adjust Picture"),
                text, 5, kOSDFunctionalType_PictureAdjust);
        }

        update_osd_pos = false;
    }
}

void TV::ChangeContrast(bool up, bool recorder)
{
    int contrast;
    QString text;

    if (osd)
    {
        if (recorder)
        {
            contrast = activerecorder->ChangeContrast(up);
            text = QString(tr("Contrast (REC) %1 %")).arg(contrast);
            osd->StartPause(contrast * 10, true, tr("Adjust Recording"),
                text, 5, kOSDFunctionalType_RecPictureAdjust);
        }
        else
        {
            contrast = nvp->getVideoOutput()->ChangeContrast(up);
            gContext->SaveSetting("PlaybackContrast", contrast);
            text = QString(tr("Contrast %1 %")).arg(contrast);
            osd->StartPause(contrast * 10, true, tr("Adjust Picture"),
                text, 5, kOSDFunctionalType_PictureAdjust);
        }

        update_osd_pos = false;
    }
}

void TV::ChangeColour(bool up, bool recorder)
{
    int colour;
    QString text;

    if (osd)
    {
        if (recorder)
        {
            colour = activerecorder->ChangeColour(up);
            text = QString(tr("Colour (REC) %1 %")).arg(colour);
            osd->StartPause(colour * 10, true, tr("Adjust Recording"),
                text, 5, kOSDFunctionalType_RecPictureAdjust);
        }
        else
        {
            colour = nvp->getVideoOutput()->ChangeColour(up);
            gContext->SaveSetting("PlaybackColour", colour);
            text = QString(tr("Colour %1 %")).arg(colour);
            osd->StartPause(colour * 10, true, tr("Adjust Picture"),
                text, 5, kOSDFunctionalType_PictureAdjust);
        }

        update_osd_pos = false;
    }
}

void TV::ChangeHue(bool up, bool recorder)
{
    int hue;
    QString text;

    if (osd)
    {
        if (recorder)
        {
            hue = activerecorder->ChangeHue(up);
            text = QString(tr("Hue (REC) %1 %")).arg(hue);
            osd->StartPause(hue * 10, true, tr("Adjust Recording"),
                text, 5, kOSDFunctionalType_RecPictureAdjust);
        }
        else
        {
            hue = nvp->getVideoOutput()->ChangeHue(up);
            gContext->SaveSetting("PlaybackHue", hue);
            text = QString(tr("Hue %1 %")).arg(hue);
            osd->StartPause(hue * 10, true, tr("Adjust Picture"),
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

    if (osd && !browsemode)
    {
        osd->StartPause(curvol * 10, true, tr("Adjust Volume"), text, 5, 
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

    if (osd && !browsemode)
    {
        int val = (int)(normal_speed*500);
        osd->StartPause(val, false, tr("Adjust Time Stretch"), text, 10, 
                        kOSDFunctionalType_TimeStretchAdjust);
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
 
    if (osd && !browsemode)
        osd->SetSettingsText(text, 5);
}

void TV::ToggleSleepTimer(void)
{
    QString text;

    // increment sleep index, cycle through
    if (++sleep_index == SSLEEP_MAX) 
        sleep_index = 0;

    // turn sleep timer off
    if (sleep_timer_array[sleep_index].seconds == 0)
        sleepTimer->stop();
    else
    {
        if (sleepTimer->isActive())
            // sleep timer is active, adjust interval
            sleepTimer->changeInterval(sleep_timer_array[sleep_index].seconds *
                                       1000);
        else
            // sleep timer is not active, start it
            sleepTimer->start(sleep_timer_array[sleep_index].seconds * 1000, 
                              TRUE);
    }

    text = tr("Sleep ") + " " + sleep_timer_array[sleep_index].dispString;

    // display OSD
    if (osd && !browsemode)
        osd->SetSettingsText(text, 3);
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

    if (osd && !browsemode && !osd->IsRunningTreeMenu())
        osd->SetSettingsText(text, 3);
}

void TV::EPGChannelUpdate(QString chanstr)
{
    if (chanstr != "")
    {
        chanstr = chanstr.left(4);
        sprintf(channelKeys, "%s", chanstr.ascii());
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
                nextState = kState_WatchingPreRecorded;
                changeState = true;
            }
        }
        else if (GetState() == kState_WatchingLiveTV && 
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
        else if (message.left(11) == "QUIT_LIVETV")
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
        else if (message.left(12) == "EXIT_TO_MENU")
        {
            int exitprompt = gContext->GetNumSetting("PlaybackExitPrompt");
            if (exitprompt == 1 || exitprompt == 2)
                nvp->SetBookmark();
            wantsToQuit = true;
            exitPlayer = true;
        }
    }
}

void TV::BrowseStart(void)
{
    if (activenvp != nvp)
        return;

    if (paused)
        return;

    OSDSet *oset = osd->GetSet("browse_info");
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

    browseTimer->start(30000, true);
}

void TV::BrowseEnd(bool change)
{
    if (!browsemode)
        return;

    browseTimer->stop();

    osd->HideSet("browse_info");

    if (change)
    {
        ChangeChannelByString(browsechannum);
    }

    browsemode = false;
}

void TV::BrowseDispInfo(int direction)
{
    if (!browsemode)
        BrowseStart();
    QDateTime curtime = QDateTime::currentDateTime();
    QDateTime maxtime = curtime.addSecs(60 * 60 * 4);
    QDateTime lastprogtime =
                  QDateTime::fromString(browsestarttime, Qt::ISODate);
    QMap<QString, QString> infoMap;

    if (paused)
        return;

    browseTimer->changeInterval(30000);
    
    
    if (lastprogtime < curtime)
        browsestarttime = curtime.toString(Qt::ISODate);
        //browsestarttime = curtime.toString("yyyyMMddhhmm") + "00";
    
    
    if ((lastprogtime > maxtime) &&
        (direction == BROWSE_RIGHT))
        return;

    infoMap["channum"] = browsechannum;
    infoMap["dbstarttime"] = browsestarttime;
    infoMap["chanid"] = browsechanid;
    
    GetNextProgram(activerecorder, direction, infoMap);
    
    browsechannum = infoMap["channum"];
    browsechanid = infoMap["chanid"];

    if ((direction == BROWSE_LEFT) ||
        (direction == BROWSE_RIGHT))
        browsestarttime = infoMap["dbstarttime"];

    QDateTime startts = QDateTime::fromString(browsestarttime, Qt::ISODate);
    ProgramInfo *program_info = ProgramInfo::GetProgramAtDateTime(browsechanid,
                                                                  startts);
    
    if (program_info)
        program_info->ToMap(infoMap);

    osd->ClearAllText("browse_info");
    osd->SetText("browse_info", infoMap, -1);

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
                       starttime, endtime, callsign, iconpath, channum, chanid,
                       seriesid, programid, outFilters, repeat, airdate, stars);

        QDateTime startts = QDateTime::fromString(starttime, Qt::ISODate);

        ProgramInfo *program_info = ProgramInfo::GetProgramAtDateTime(chanid,
                                                                      startts);

        if (program_info)
        {
            program_info->ToggleRecord();

            QString msg = QString("%1 \"%2\"").arg(tr("Record")).arg(title);

            if (activenvp == nvp)
                osd->SetSettingsText(msg, 3);

            delete program_info;
        }

        return;
    }

    QMap<QString, QString> infoMap;
    QDateTime startts = QDateTime::fromString(browsestarttime, Qt::ISODate);

    ProgramInfo *program_info = ProgramInfo::GetProgramAtDateTime(browsechanid,
                                                                  startts);
    program_info->ToggleRecord();

    program_info->ToMap(infoMap);

    osd->ClearAllText("browse_info");
    osd->SetText("browse_info", infoMap, -1);

    if (activenvp == nvp)
        osd->SetSettingsText(tr("Record"), 3);

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
        case kOSDFunctionalType_Default:
            break;
    }
}

void TV::DoTogglePictureAttribute(void)
{
    OSDSet *oset;
    int value = 0;
    oset = osd->GetSet("status");

    if (osd)
    {
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
        osd->StartPause(value*10, true, title, picName, 5, 
                        kOSDFunctionalType_PictureAdjust);
        update_osd_pos = false;
    }
}

void TV::DoToggleRecPictureAttribute(void)
{
    OSDSet *oset;
    int value = 0;
    oset = osd->GetSet("status");
   
    recAdjustment = (recAdjustment % 4) + 1;
   
    if (osd)
    {
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
        osd->StartPause(value * 10, true, title, recName, 5,
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
    return nvp->GetOSD();
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

    if (isEditing)
    {
        nvp->Pause();

        dialogname = "alreadybeingedited";

        QString message = tr("This program is currently being edited");

        QStringList options;
        options += tr("Continue Editing");
        options += tr("Do not edit");

        osd->NewDialogBox(dialogname, message, options, 0);
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
    else if (internalState == kState_WatchingLiveTV)
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

    if (nvp->GetOSD())
    {
        ClearOSD();

        OSDListTreeType *tree = osd->ShowTreeMenu("menu", treeMenu);
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

    if (internalState == kState_WatchingLiveTV)
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
        subitem = new OSDGenericTree(item, tr("Auto-Skip Pre-Notify"),
                                     "TOGGLECOMMSKIP3",
                                     (autoCommercialSkip == 3) ? 1 : 0, NULL,
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

    int speedX10 = (int)(ceil(normal_speed * 10));
    item = new OSDGenericTree(treeMenu, tr("Adjust Time Stretch"), "TOGGLESTRETCH");
    subitem = new OSDGenericTree(item, tr("Adjust"), "TOGGLESTRETCH");
    subitem = new OSDGenericTree(item, tr("0.5X"), "TOGGLESTRETCH0.5",
                                 (speedX10 == 5) ? 1 : 0, NULL,
                                 "STRETCHGROUP");
    subitem = new OSDGenericTree(item, tr("0.9X"), "TOGGLESTRETCH0.9",
                                 (speedX10 == 9) ? 1 : 0, NULL,
                                 "STRETCHGROUP");
    subitem = new OSDGenericTree(item, tr("1.0X"), "TOGGLESTRETCH1.0",
                                 (speedX10 == 10) ? 1 : 0, NULL,
                                 "STRETCHGROUP");
    subitem = new OSDGenericTree(item, tr("1.1X"), "TOGGLESTRETCH1.1",
                                 (speedX10 == 11) ? 1 : 0, NULL,
                                 "STRETCHGROUP");
    subitem = new OSDGenericTree(item, tr("1.2X"), "TOGGLESTRETCH1.2",
                                 (speedX10 == 12) ? 1 : 0, NULL,
                                 "STRETCHGROUP");
    subitem = new OSDGenericTree(item, tr("1.3X"), "TOGGLESTRETCH1.3",
                                 (speedX10 == 13) ? 1 : 0, NULL,
                                 "STRETCHGROUP");
    subitem = new OSDGenericTree(item, tr("1.4X"), "TOGGLESTRETCH1.4",
                                 (speedX10 == 14) ? 1 : 0, NULL,
                                 "STRETCHGROUP");
    subitem = new OSDGenericTree(item, tr("1.5X"), "TOGGLESTRETCH1.5",
                                 (speedX10 == 15) ? 1 : 0, NULL,
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

    if (activenvp == nvp && desc != "" )
    {
        QString curTime = "";
        int pos = nvp->calcSliderPos(curTime);
        osd->StartPause(pos, false, desc, curTime, 1);
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
    else if (autoCommercialSkip == 3)
        desc = tr("Auto-Skip Pre-Notify");

    nvp->SetAutoCommercialSkip(autoCommercialSkip);

    if (activenvp == nvp && desc != "" )
    {
        QString curTime = "";
        int pos = nvp->calcSliderPos(curTime);
        osd->StartPause(pos, false, desc, curTime, 1);
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

    if (activenvp == nvp && desc != "" )
    {
        QString curTime = "";
        int pos = nvp->calcSliderPos(curTime);
        osd->StartPause(pos, false, desc, curTime, 1);
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
        if (osd && !browsemode)
        {
            QString out;

            if (mins != 0)
                out = tr("Sleep") + " " + QString::number(mins);
            else
                out = tr("Sleep") + " " + sleep_timer_array[0].dispString;

            osd->SetSettingsText(out, 3);
        }
    }
    else
    {
        cout << "No sleep timer?";
    }
}

