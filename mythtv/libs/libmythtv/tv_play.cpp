#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <qsqldatabase.h>
#include <qapplication.h>
#include <qregexp.h>
#include <qfile.h>
#include <qtimer.h>

#include <iostream>
using namespace std;

#include "tv.h"
#include "osd.h"
#include "osdtypes.h"
#include "mythcontext.h"
#include "dialogbox.h"
#include "remoteencoder.h"
#include "remoteutil.h"
#include "guidegrid.h"
#include "volumecontrol.h"
#include "NuppelVideoPlayer.h"
#include "programinfo.h"

// cheat and put the keycodes here
#define wsUp            0x52 + 256
#define wsDown          0x54 + 256
#define wsLeft          0x51 + 256
#define wsRight         0x53 + 256
#define wsPageUp        0x55 + 256
#define wsPageDown      0x56 + 256
#define wsEnd           0x57 + 256
#define wsBegin         0x58 + 256
#define wsEscape        0x1b + 256
#define wsZero          0xb0 + 256
#define wsOne           0xb1 + 256
#define wsTwo           0xb2 + 256
#define wsThree         0xb3 + 256
#define wsFour          0xb4 + 256
#define wsFive          0xb5 + 256
#define wsSix           0xb6 + 256
#define wsSeven         0xb7 + 256
#define wsEight         0xb8 + 256
#define wsNine          0xb9 + 256
#define wsEnter         0x8d + 256
#define wsReturn        0x0d + 256

enum SeekSpeeds {
  SSPEED_NORMAL_WITH_DISPLAY = 0,
  SSPEED_SLOW_1,
  SSPEED_SLOW_2,
  SSPEED_NORMAL,
  SSPEED_FAST_1,
  SSPEED_FAST_2,
  SSPEED_FAST_3,
  SSPEED_FAST_4,
  SSPEED_FAST_5,
  SSPEED_FAST_6,
  SSPEED_MAX
};

struct SeekSpeedInfo {
  QString   dispString;
  float  scaling;
};

SeekSpeedInfo seek_speed_array[] =
  {{"", 0.0},
   {"1/4X", 0.25},
   {"1/2X", 0.50},
   {"1X", 1.00},
   {"1.5X", 1.50},
   {"2X", 2.24},
   {"3X", 3.34},
   {"8X", 7.48},
   {"10X", 11.18},
   {"16X", 16.72}};

enum DeinterlaceMode {
  DEINTERLACE_NONE =0,
  DEINTERLACE_BOB,
  DEINTERLACE_BOB_FULLHEIGHT_COPY,
  DEINTERLACE_BOB_FULLHEIGHT_LINEAR_INTERPOLATION,
  DEINTERLACE_DISCARD_TOP,
  DEINTERLACE_DISCARD_BOTTOM,
  DEINTERLACE_AREA,
  DEINTERLACE_LAST
};

char *deinterlacer_names[] =
{ "None",
  "Field Bob",
  "Line Doubler",
  "Interpolated",
  "Discard Top",
  "Discard Bottom",
  "Area Based"
};

void *SpawnDecode(void *param)
{
    NuppelVideoPlayer *nvp = (NuppelVideoPlayer *)param;
    nvp->StartPlaying();
    return NULL;
}

TV::TV(QSqlDatabase *db)
  : QObject()
{
    m_db = db;

    dialogname = "";
    playbackinfo = NULL;
    editmode = false;
    browsemode = false;
    prbuffer = NULL;
    nvp = NULL;
    osd = NULL;
    requestDelete = false;
    endOfRecording = false;
    volumeControl = NULL;
    embedid = 0;
    times_pressed = 0;
    last_channel = "";

    deinterlace_mode = DEINTERLACE_NONE;
    gContext->addListener(this);

    PrevChannelVector channame_vector(30);

    prevChannelTimer = new QTimer(this);
    connect(prevChannelTimer, SIGNAL(timeout()), SLOT(SetPreviousChannel()));
}

void TV::Init(void)
{
    fftime = gContext->GetNumSetting("FastForwardAmount", 30);
    rewtime = gContext->GetNumSetting("RewindAmount", 5);
    jumptime = gContext->GetNumSetting("JumpAmount", 10);

    recorder = piprecorder = activerecorder = NULL;
    nvp = pipnvp = activenvp = NULL;
    prbuffer = piprbuffer = activerbuffer = NULL;

    menurunning = false;

    internalState = nextState = kState_None; 

    runMainLoop = false;
    changeState = false;

    watchingLiveTV = false;

    if (gContext->GetNumSetting("MythControlsVolume", 1))
        volumeControl = new VolumeControl(true);

    pthread_create(&event, NULL, EventThread, this);

    while (!runMainLoop)
        usleep(50);
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
    if (volumeControl)
        delete volumeControl;
}

TVState TV::LiveTV(void)
{
    if (internalState == kState_None)
    {
        RemoteEncoder *testrec = RemoteRequestRecorder();

        if (!testrec->IsValidRecorder())
        {
            QString title = tr("MythTV is already using all available inputs "
                               "for recording.  If you want to watch an "
                               "in-progress recording, select one from the "
                               "playback menu.  If you want to watch live TV, "
                               "cancel one of the in-progress recordings from "
                               "the delete menu.");
    
            DialogBox diag(title);
            diag.AddButton(tr("Cancel and go back to the TV menu"));

            diag.Show();
            diag.exec();
        
            nextState = kState_None;
            changeState = false;
       
            delete testrec;
 
            return nextState;
        }

        activerecorder = recorder = testrec;
        nextState = kState_WatchingLiveTV;
        changeState = true;
    }

    TVState retval = kState_None;
    
    if (nextState != internalState)
    {
        retval = nextState;
        changeState = true;
    }

    return retval;
}

void TV::FinishRecording(void)
{
    if (!IsRecording())
        return;

    activerecorder->FinishRecording();
}

int TV::AllowRecording(const QString &message, int timeuntil)
{
    if (internalState != kState_WatchingLiveTV)
    {
        return 1;
    }

    dialogname = "allowrecordingbox";

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

    osd->NewDialogBox(dialogname, message, options, timeuntil); 

    while (osd->DialogShowing(dialogname))
    {
        qApp->unlock();
        qApp->processEvents();
        usleep(1000);
        qApp->lock();
    }

    int result = osd->GetDialogResponse(dialogname);
    dialogname = "";

    if (result == 2)
        StopLiveTV();

    return result;
}

void TV::Playback(ProgramInfo *rcinfo)
{
    if (internalState == kState_None)
    {
        inputFilename = rcinfo->pathname;

        playbackLen = rcinfo->CalculateLength();
        playbackinfo = rcinfo;

        QDateTime curtime = QDateTime::currentDateTime();

        if (curtime < rcinfo->endts && curtime > rcinfo->startts)
            nextState = kState_WatchingRecording;
        else
            nextState = kState_WatchingPreRecorded;

        changeState = true;
    }
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
    bool retval = false;

    if (state == kState_RecordingOnly || 
        state == kState_WatchingRecording)
    {
        retval = true;
    }

    return retval;
}

bool TV::StateIsPlaying(TVState state)
{
    bool retval = false;

    if (state == kState_WatchingPreRecorded || 
        state == kState_WatchingRecording)
    {
        retval = true;
    }

    return retval;
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
    bool changed = false;
    bool startPlayer = false;
    bool startRecorder = false;
    bool closePlayer = false;
    bool closeRecorder = false;
    bool sleepBetween = false;

    QString statename;
    StateToString(nextState, statename);
    QString origname;
    StateToString(internalState, origname);

    if (nextState == kState_Error)
    {
        printf("ERROR: Attempting to set to an error state.\n");
        exit(-1);
    }

    if (internalState == kState_None && nextState == kState_WatchingLiveTV)
    {
        long long filesize = 0;
        long long smudge = 0;
        QString name = "";

        recorder->Setup();
        recorder->SetupRingBuffer(name, filesize, smudge);

        prbuffer = new RingBuffer(name, filesize, smudge, recorder);

        internalState = nextState;
        changed = true;

        startPlayer = startRecorder = true;

        recorder->SpawnLiveTV();

        watchingLiveTV = true;
        sleepBetween = true;
    }
    else if (internalState == kState_WatchingLiveTV && 
             nextState == kState_None)
    {
        closePlayer = true;
        closeRecorder = true;

        internalState = nextState;
        changed = true;

        watchingLiveTV = false;
    }
    else if (internalState == kState_WatchingRecording &&
             nextState == kState_WatchingPreRecorded)
    {
        internalState = nextState;
        changed = true;

        watchingLiveTV = false;
    }
    else if ((internalState == kState_None && 
              nextState == kState_WatchingPreRecorded) ||
             (internalState == kState_None &&
              nextState == kState_WatchingRecording))
    {
        prbuffer = new RingBuffer(inputFilename, false);

        if (nextState == kState_WatchingRecording)
        {
            recorder = RemoteGetExistingRecorder(playbackinfo);
            if (!recorder->IsValidRecorder())
            {
                cout << "ERROR: couldn't find recorder for in-progress "
                     << "recording\n";
                nextState = kState_WatchingPreRecorded;
                delete recorder;
                activerecorder = recorder = NULL;
            }
            else
            {
                activerecorder = recorder;
                recorder->Setup();
            }
        }

        internalState = nextState;
        changed = true;

        startPlayer = true;
    }
    else if ((internalState == kState_WatchingPreRecorded && 
              nextState == kState_None) || 
             (internalState == kState_WatchingRecording &&
              nextState == kState_None))
    {
        if (internalState == kState_WatchingRecording)
            recorder->StopPlaying();

        closePlayer = true;
        
        internalState = nextState;
        changed = true;

        watchingLiveTV = false;
    }
    else if (internalState == kState_WatchingLiveTV &&  
             nextState == kState_WatchingRecording)
    {
        if (paused)
            osd->EndPause();
        paused = false;

        nvp->Pause();
        while (!nvp->GetPause())
            usleep(5);

        recorder->TriggerRecordingTransition();

        prbuffer->Reset();

        nvp->ResetPlaying();
        while (!nvp->ResetYet())
            usleep(5);

        usleep(300000);

        nvp->Unpause();

        internalState = nextState;
        changed = true;
    }
    else if (internalState == kState_WatchingRecording &&
             nextState == kState_WatchingLiveTV)
    {
        internalState = nextState;
        changed = true;
    }
    else if (internalState == kState_None && 
             nextState == kState_None)
    {
        changed = true;
    }

    if (!changed)
    {
        printf("Unknown state transition: %d to %d\n", internalState,
                                                       nextState);
    }
    else
    {
        printf("Changing from %s to %s\n", origname.ascii(), statename.ascii());
    }
 
    if (startRecorder)
    {
        while (!recorder->IsRecording())
            usleep(50);

        frameRate = recorder->GetFrameRate();
    }

    if (sleepBetween) 
        usleep(300000);

    if (startPlayer)
    {
        SetupPlayer();
        pthread_create(&decode, NULL, SpawnDecode, nvp);

        while (!nvp->IsPlaying())
            usleep(50);

        activenvp = nvp;
        activerbuffer = prbuffer;
        activerecorder = recorder;

        frameRate = nvp->GetFrameRate();
	osd = nvp->GetOSD();
    }

    if (closePlayer)
    {
        if (prbuffer)
        {
            prbuffer->StopReads();
            prbuffer->Pause();
            while (!prbuffer->isPaused())
                usleep(50);
        }

        if (nvp)
            nvp->StopPlaying();

        if (piprbuffer)
        {
            piprbuffer->StopReads();
            piprbuffer->Pause();
            while (!piprbuffer->isPaused())
                usleep(50);
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

    changeState = false;
}

void TV::SetupPlayer(void)
{
    if (nvp)
    { 
        printf("Attempting to setup a player, but it already exists.\n");
        return;
    }

    QString filters = "";
    nvp = new NuppelVideoPlayer(m_db, playbackinfo);
    nvp->SetRingBuffer(prbuffer);
    nvp->SetRecorder(recorder);
    nvp->SetOSDFontName(gContext->GetSetting("OSDFont"),
                        gContext->GetSetting("OSDCCFont"),
                        gContext->GetInstallPrefix()); 
    nvp->SetOSDThemeName(gContext->GetSetting("OSDTheme"));
    nvp->SetAudioSampleRate(gContext->GetNumSetting("AudioSampleRate"));
    nvp->SetAudioDevice(gContext->GetSetting("AudioOutputDevice"));
    nvp->SetLength(playbackLen);
    nvp->SetExactSeeks(gContext->GetNumSetting("ExactSeeking"));
    nvp->SetAutoCommercialSkip(gContext->GetNumSetting("AutoCommercialSkip"));
    nvp->SetCommercialSkipMethod(gContext->GetNumSetting("CommercialSkipMethod"));

    osd_display_time = gContext->GetNumSetting("OSDDisplayTime");

    if (gContext->GetNumSetting("Deinterlace"))
    {
        if (filters.length() > 1)
            filters += ",";
        filters += "linearblend";
    }

    nvp->SetVideoFilters(filters);

    if (embedid > 0)
        nvp->EmbedInWidget(embedid, embx, emby, embw, embh);

    if (internalState == kState_WatchingRecording)
        nvp->SetWatchingRecording(true);

    osd = NULL;
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
    pipnvp->SetOSDFontName(gContext->GetSetting("OSDFont"),
                           gContext->GetSetting("OSDCCFont"),
                           gContext->GetInstallPrefix());
    pipnvp->SetOSDThemeName(gContext->GetSetting("OSDTheme"));
    pipnvp->SetAudioSampleRate(gContext->GetNumSetting("AudioSampleRate"));
    pipnvp->SetAudioDevice(gContext->GetSetting("AudioOutputDevice"));
    pipnvp->SetExactSeeks(gContext->GetNumSetting("ExactSeeking"));

    pipnvp->SetLength(playbackLen);
}

void TV::TeardownPlayer(void)
{
    if (nvp)
    {
        pthread_join(decode, NULL);
        delete nvp;
    }

    paused = false;
    doing_ff = false;
    doing_rew = false;
    ff_rew_index = SSPEED_NORMAL;
    ff_rew_scaling = 1.0;

    nvp = NULL;
    osd = NULL;
  
    if (recorder) 
        delete recorder; 
    recorder = activerecorder = NULL;
 
    if (prbuffer)
    {
        delete prbuffer;
        prbuffer = NULL;
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
    int keypressed;

    stickykeys = gContext->GetNumSetting("StickyKeys");
    doing_ff = false;
    doing_rew = false;
    ff_rew_scaling = 1.0;
    ff_rew_index = SSPEED_NORMAL;

    int pausecheck = 0;

    channelqueued = false;
    channelKeys[0] = channelKeys[1] = channelKeys[2] = channelKeys[3] = ' ';
    channelKeys[4] = 0;
    channelkeysstored = 0;    

    runMainLoop = true;
    exitPlayer = false;

    while (runMainLoop)
    {
        if (changeState)
            HandleStateChange();

        usleep(1000);

        if (nvp)
        {
            if ((keypressed = nvp->CheckEvents()))
                ProcessKeypress(keypressed);
            else if (stickykeys)
            {
                if (doing_ff)
                    DoFF();
                else if (doing_rew)
                    DoRew();
                if (ff_rew_index > SSPEED_NORMAL)
                    usleep(50000);
            }
        }

        if (StateIsPlaying(internalState))
        {
            if (!nvp->IsPlaying())
            {
                nextState = RemovePlaying(internalState);
                changeState = true;
                endOfRecording = true;
            }
        }

        if (exitPlayer)
        {
            if (internalState == kState_WatchingLiveTV)
            {
                nextState = kState_None;
                changeState = true;
            }
            else if (StateIsPlaying(internalState))
            {
                nextState = RemovePlaying(internalState);
                changeState = true;
            }
            exitPlayer = false;
        }

        if (internalState == kState_WatchingLiveTV || 
            internalState == kState_WatchingRecording)
        {
            pausecheck++;
            if (paused && !(pausecheck % 20))
            {
                QString desc = "";
                int pos = calcSliderPos(0, desc);
                osd->UpdatePause(pos, desc);
                pausecheck = 0;
            }

            if (channelqueued && nvp->GetOSD() && !osd->Visible())
            {
                ChannelCommit();
            }
        }
    }
  
    nextState = kState_None;
    HandleStateChange();
}

void TV::ProcessKeypress(int keypressed)
{
    bool was_doing_ff_rew = false;

    if (editmode)
    {   
        nvp->DoKeypress(keypressed);
        if (keypressed == wsEscape || 
            keypressed == 'e' || keypressed == 'E' ||
            keypressed == 'm' || keypressed == 'M')
            editmode = nvp->GetEditMode();
        return;
    }

    if (browsemode)
    {
        int passThru = 0;
        switch (keypressed)
        {
            case wsUp: BrowseDispInfo(BROWSE_UP); break;
            case wsDown: BrowseDispInfo(BROWSE_DOWN); break;
            case wsLeft: BrowseDispInfo(BROWSE_LEFT); break;
            case wsRight: BrowseDispInfo(BROWSE_RIGHT); break;
            case wsEscape: BrowseEnd(false); break;
            case ' ': case wsEnter: case wsReturn: BrowseEnd(true); break;
            case 'r': case 'R': BrowseToggleRecord(); break;
            case '[': case ']': case '|': passThru = 1; break;
        }

        if (!passThru)
            return;
    }

    if (nvp->GetOSD() && osd->DialogShowing(dialogname))
    {
        switch (keypressed)
        {
            case wsUp: osd->DialogUp(dialogname); break;
            case wsDown: osd->DialogDown(dialogname); break;
            case ' ': case wsEnter: case wsReturn: 
            {
                osd->TurnDialogOff(dialogname);
                if (dialogname == "exitplayoptions") 
                {
                    int result = osd->GetDialogResponse(dialogname);
                    dialogname = "";

                    if (result == 3)
                    {
                        nvp->Unpause();
                    }
                    else if (result == 1)
                    {
                        nvp->SetBookmark();
                        exitPlayer = true;
                    }
                    else if (result == 4)
                    {
                        exitPlayer = true;
                        requestDelete = true;
                    }
                    else
                    {
                        exitPlayer = true;
                    }
                }
                break;
            }
            default: break;
        }
        
        return;
    }

    switch (keypressed) 
    {
        case 'f': case 'F':
        {
            nvp->ToggleFullScreen();
            break;
        }
 
        case 't': case 'T':
        {
            nvp->ToggleCC();
            break;
        }
        case 'z': case 'Z':
        {
            doing_ff = false;
            doing_rew = false;
            DoSkipCommercials(1);
            break;
        }
        case 'q': case 'Q':
        {
            doing_ff = false;
            doing_rew = false;
            DoSkipCommercials(-1);
            break;
        }
        case 's': case 'S': case 'p': case 'P': 
        {
            doing_ff = false;
            doing_rew = false;
            DoPause();
            break;
        }
        case wsRight: case 'd': case 'D': 
        {
            if (stickykeys)
            {
                if (doing_ff)
                    ff_rew_index = (++ff_rew_index % SSPEED_MAX);
                else
                {
                    doing_ff = true;
                    ff_rew_index = SSPEED_NORMAL;
                }
            }
            else
                ff_rew_index = SSPEED_NORMAL;

            doing_rew = false;
            DoFF(); 
            break;
        }
        case wsLeft: case 'a': case 'A': 
        {
            if (stickykeys)
            {
                if (doing_rew)
                    ff_rew_index = (++ff_rew_index % SSPEED_MAX);
                else
                {
                    doing_rew = true;
                    ff_rew_index = SSPEED_NORMAL;
                }
            }
            else
                ff_rew_index = SSPEED_NORMAL;

            doing_ff = false;
            DoRew(); 
            break;
        }
        case wsPageUp:
        {
            DoJumpAhead(); 
            break;
        }
        case wsPageDown:
        {
            DoJumpBack(); 
            break;
        }
        case wsEscape:
        {
            if (StateIsPlaying(internalState) && 
                gContext->GetNumSetting("PlaybackExitPrompt")) 
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
            else 
            {
                exitPlayer = true;
                break;
            }
            break;
        }
        case '[':  ChangeVolume(false); break;
        case ']':  ChangeVolume(true); break;
        case '|':  ToggleMute(); break;

        default: 
        {
            if (doing_ff || doing_rew)
            {
                switch (keypressed)
                {
                    case wsZero:  case '0': ff_rew_index = SSPEED_NORMAL_WITH_DISPLAY; break;
                    case wsOne:   case '1': ff_rew_index = SSPEED_SLOW_1; break;
                    case wsTwo:   case '2': ff_rew_index = SSPEED_SLOW_2; break;
                    case wsThree: case '3': ff_rew_index = SSPEED_NORMAL; break;
                    case wsFour:  case '4': ff_rew_index = SSPEED_FAST_1; break;
                    case wsFive:  case '5': ff_rew_index = SSPEED_FAST_2; break;
                    case wsSix:   case '6': ff_rew_index = SSPEED_FAST_3; break;
                    case wsSeven: case '7': ff_rew_index = SSPEED_FAST_4; break;
                    case wsEight: case '8': ff_rew_index = SSPEED_FAST_5; break;
                    case wsNine:  case '9': ff_rew_index = SSPEED_FAST_6; break;

                    default:
                       doing_ff = false;
                       doing_rew = false;
                       was_doing_ff_rew = true;
                       break;
                }
            }
            break;
        }
    }

    if (internalState == kState_WatchingLiveTV)
    {
        switch (keypressed)
        {
            case 'i': case 'I': ToggleOSD(); break;

            case wsUp: ChangeChannel(CHANNEL_DIRECTION_UP); break;
            case wsDown: ChangeChannel(CHANNEL_DIRECTION_DOWN); break;
            case '/': ChangeChannel(CHANNEL_DIRECTION_FAVORITE); break;

            case '?': ToggleChannelFavorite(); break;

            case 'c': case 'C': ToggleInputs(); break;

            case wsZero: case wsOne: case wsTwo: case wsThree: case wsFour:
            case wsFive: case wsSix: case wsSeven: case wsEight:
            case wsNine: case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                     ChannelKey(keypressed); break;

            case ' ': case wsEnter: case wsReturn: ChannelCommit(); break;

            case 'M': case 'm': LoadMenu(); break;

            case 'V': case 'v': TogglePIPView(); break;
            case 'B': case 'b': ToggleActiveWindow(); break;
            case 'N': case 'n': SwapPIP(); break;

            // Contrast, brightness, colour of the input source
            case 'j': ChangeContrast(false); break;
            case 'J': ChangeContrast(true); break;
            case 'k': ChangeBrightness(false); break;
            case 'K': ChangeBrightness(true); break;
            case 'l': ChangeColour(false); break;
            case 'L': ChangeColour(true); break;

            case 'x': ChangeDeinterlacer(); break;
            case 'o': case 'O': BrowseStart(); break;

            case 'H': case 'h': PreviousChannel(); break;

            default: break;
        }
    }
    else if (StateIsPlaying(internalState))
    {
        switch (keypressed)
        {
            case 'i': case 'I': DoInfo(); break;
            case ' ': case wsEnter: case wsReturn: 
            {
                if (!was_doing_ff_rew)
                    nvp->SetBookmark(); 
                break;
            }
            case 'e': case 'E': 
            case 'm': case 'M': editmode = nvp->EnableEdit(); break;
            default: break;
        }
    }
}

void TV::TogglePIPView(void)
{
    if (!pipnvp)
    {
        RemoteEncoder *testrec = RemoteRequestRecorder();

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

    piprecorder->GetChannelInfo(dummy, dummy, dummy, dummy, dummy, dummy,
                                dummy, dummy, pipchanname, dummy);
    recorder->GetChannelInfo(dummy, dummy, dummy, dummy, dummy, dummy,
                             dummy, dummy, bigchanname, dummy);

    if (activenvp != nvp)
        ToggleActiveWindow();

    ChangeChannelByString(pipchanname);

    ToggleActiveWindow();

    ChangeChannelByString(bigchanname);

    ToggleActiveWindow();
}

int TV::calcSliderPos(int offset, QString &desc)
{
    float ret;

    char text[512];

    if (internalState == kState_WatchingLiveTV)
    {
        ret = prbuffer->GetFreeSpace() / 
              ((float)prbuffer->GetFileSize() - prbuffer->GetSmudgeSize());
        ret *= 1000.0;

        long long written = recorder->GetFramesWritten();
        long long played = nvp->GetFramesPlayed();

        played += (int)(offset * frameRate);
        if (played > written)
            played = written;
        if (played < 0)
            played = 0;
	
        int secsbehind = (int)((float)(written - played) / frameRate);

        if (secsbehind < 0)
            secsbehind = 0;

        int hours = (int)secsbehind / 3600;
        int mins = ((int)secsbehind - hours * 3600) / 60;
        int secs = ((int)secsbehind - hours * 3600 - mins * 60);

        if (hours > 0)
        {
            if (osd->getTimeType() == 0)
            {
                sprintf(text, "%02d:%02d:%02d behind  --  %.2f%% full", hours, 
                        mins, secs, (1000 - ret) / 10);
            }
            else
            {
                sprintf(text, "%02d:%02d:%02d behind", hours, mins, secs);
            }
        }
        else
        {
            if (osd->getTimeType() == 0)
            {
                sprintf(text, "%02d:%02d behind  --  %.2f%% full", mins, secs,
                        (1000 - ret) / 10);
            }
            else
            {
                sprintf(text, "%02d:%02d behind", mins, secs);
            }
        }

        desc = text;
        return (int)(1000 - ret);
    }

    if (internalState == kState_WatchingRecording)
        playbackLen = (int)(((float)recorder->GetFramesWritten() / frameRate));
    else
        playbackLen = nvp->GetLength();

    float secsplayed = ((float)nvp->GetFramesPlayed() / frameRate) + offset;
    if (secsplayed < 0)
        secsplayed = 0;
    if (secsplayed > playbackLen)
        secsplayed = playbackLen;

    ret = secsplayed / (float)playbackLen;
    ret *= 1000.0;

    int phours = (int)secsplayed / 3600;
    int pmins = ((int)secsplayed - phours * 3600) / 60;
    int psecs = ((int)secsplayed - phours * 3600 - pmins * 60);

    int shours = playbackLen / 3600;
    int smins = (playbackLen - shours * 3600) / 60;
    int ssecs = (playbackLen - shours * 3600 - smins * 60);

    if (shours > 0)
        sprintf(text, "%02d:%02d:%02d of %02d:%02d:%02d", phours, pmins, psecs,
                shours, smins, ssecs);
    else
        sprintf(text, "%02d:%02d of %02d:%02d", pmins, psecs, smins, ssecs);

    desc = text;

    return (int)(ret);
}

void TV::DoPause(void)
{
    paused = activenvp->TogglePause();

    if (activenvp != nvp)
        return;

    if (paused)
    {
        QString desc = "";
        int pos = calcSliderPos(0, desc);
        if (internalState == kState_WatchingLiveTV)
            osd->StartPause(pos, true, "Paused", desc, -1);
	else
            osd->StartPause(pos, false, "Paused", desc, -1);
    }
    else
        osd->EndPause();
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
        oset->Display(false);

        QMap<QString, QString> regexpMap;
        playbackinfo->ToMap(m_db, regexpMap);
        osd->ClearAllText("program_info");
        osd->SetTextByRegexp("program_info", regexpMap, osd_display_time);
    }
    else
    {
        oset = osd->GetSet("program_info");
        if ((oset) && (oset->Displaying()))
            oset->Display(false);

        QString desc = "";
        int pos = calcSliderPos(0, desc);
        osd->StartPause(pos, false, "Position", desc, osd_display_time);
    }
}

void TV::DoFF(void)
{
    bool slidertype = false;
    if (internalState == kState_WatchingLiveTV)
        slidertype = true;

    ff_rew_scaling = seek_speed_array[ff_rew_index].scaling;
    QString scaleString = "Forward ";
    scaleString += seek_speed_array[ff_rew_index].dispString;

    if (activenvp == nvp)
    {
        QString desc = "";
        int pos = calcSliderPos((int)(fftime * ff_rew_scaling), desc);
        osd->StartPause(pos, slidertype, scaleString, desc, 2);
    }

    activenvp->FastForward(fftime * ff_rew_scaling);
}

void TV::DoRew(void)
{
    bool slidertype = false;
    if (internalState == kState_WatchingLiveTV)
        slidertype = true;

    ff_rew_scaling = seek_speed_array[ff_rew_index].scaling;
    QString scaleString = "Rewind ";
    scaleString += seek_speed_array[ff_rew_index].dispString;

    if (activenvp == nvp)
    {
        QString desc = "";
        int pos = calcSliderPos(0 - (int)(rewtime * ff_rew_scaling), desc);
        osd->StartPause(pos, slidertype, scaleString, desc, 2);
    }

    activenvp->Rewind(rewtime * ff_rew_scaling);
}

void TV::DoJumpAhead(void)
{
    bool slidertype = false;
    if (internalState == kState_WatchingLiveTV)
        slidertype = true;

    if (activenvp == nvp)
    {
        QString desc = "";
        int pos = calcSliderPos((int)(jumptime * 60), desc);
        osd->StartPause(pos, slidertype, "Jump Ahead", desc, 2);
    }

    activenvp->FastForward(jumptime * 60);
}

void TV::DoJumpBack(void)
{
    bool slidertype = false;
    if (internalState == kState_WatchingLiveTV)
        slidertype = true;

    if (activenvp == nvp)
    {
        QString desc = "";
        int pos = calcSliderPos(0 - (int)(jumptime * 60), desc);
        osd->StartPause(pos, slidertype, "Jump Back", desc, 2);
    }

    activenvp->Rewind(jumptime * 60);
}

void TV::DoSkipCommercials(int direction)
{
    bool slidertype = false;
    if (internalState == kState_WatchingLiveTV)
        slidertype = true;

    if (activenvp == nvp)
    {
        QString dummy = "";
        QString desc = "Searching...";
        int pos = calcSliderPos(0, dummy);
        osd->StartPause(pos, slidertype, "SKIP", desc, 6);
    }

    activenvp->SkipCommercials(direction);
}

void TV::ToggleInputs(void)
{
    if (activenvp == nvp)
    {
        if (paused)
            osd->EndPause();
        paused = false;
    }

    activenvp->Pause();
    while (!activenvp->GetPause())
        usleep(5);

    activerecorder->Pause();
    activerbuffer->Reset();
    activerecorder->ToggleInputs();

    activenvp->ResetPlaying();
    while (!activenvp->ResetYet())
        usleep(5);

    usleep(300000);

    if (activenvp == nvp)
        UpdateOSDInput();

    activenvp->Unpause();
}

void TV::ToggleChannelFavorite(void)
{
    activerecorder->ToggleChannelFavorite();
}

void TV::ChangeChannel(int direction)
{
    bool muted = false;

    if (volumeControl && !volumeControl->GetMute())
    {
        volumeControl->ToggleMute();
        muted = true;
    }

    if (activenvp == nvp)
    {
        if (paused)
            osd->EndPause();
        paused = false;
    }

    activenvp->Pause();
    while (!activenvp->GetPause())
        usleep(5);

    // Save the current channel if this is the first time
    if (channame_vector.size() == 0)
        AddPreviousChannel();

    activerecorder->Pause();
    activerbuffer->Reset();
    activerecorder->ChangeChannel(direction);

    AddPreviousChannel();

    activenvp->ResetPlaying();
    while (!activenvp->ResetYet())
        usleep(5);

    usleep(300000);

    if (activenvp == nvp)
        UpdateOSD();

    activenvp->Unpause();

    channelqueued = false;
    channelKeys[0] = channelKeys[1] = channelKeys[2] = channelKeys[3] = ' ';
    channelkeysstored = 0;

    if (muted)
        volumeControl->ToggleMute();
}

void TV::ChannelKey(int key)
{
    char thekey = key;

    if (key > 256)
        thekey = key - 256 - 0xb0 + '0';

    if (channelkeysstored == 4)
    {
        channelKeys[0] = channelKeys[1];
        channelKeys[1] = channelKeys[2];
        channelKeys[2] = channelKeys[3];
        channelKeys[3] = thekey;
    }
    else
    {
        channelKeys[channelkeysstored] = thekey; 
        channelkeysstored++;
    }
    channelKeys[4] = 0;

    if (activenvp == nvp && osd)
    {
        QMap<QString, QString> regexpMap;

        regexpMap["channum"] = channelKeys;
        regexpMap["callsign"] = "";
        osd->ClearAllText("channel_number");
        osd->SetTextByRegexp("channel_number", regexpMap, 2);
    }

    channelqueued = true;
}

void TV::ChannelCommit(void)
{
    if (!channelqueued)
        return;

    for (int i = 0; i < channelkeysstored; i++)
    {
        if (channelKeys[i] == '0')
            channelKeys[i] = ' ';
        else
            break;
    }

    QString chan = QString(channelKeys).stripWhiteSpace();
    ChangeChannelByString(chan);

    channelqueued = false;
    channelKeys[0] = channelKeys[1] = channelKeys[2] = channelKeys[3] = ' ';
    channelkeysstored = 0;
}

void TV::ChangeChannelByString(QString &name)
{
    if (!activerecorder->CheckChannel(name))
        return;

    if (activenvp == nvp)
    {
        if (paused)
            osd->EndPause();
        paused = false;
    }

    activenvp->Pause();
    while (!activenvp->GetPause())
        usleep(5);

    // Save the current channel if this is the first time
    if (channame_vector.size() == 0)
        AddPreviousChannel();

    activerecorder->Pause();
    activerbuffer->Reset();
    activerecorder->SetChannel(name);

    AddPreviousChannel();

    activenvp->ResetPlaying();
    while (!activenvp->ResetYet())
        usleep(5);

    usleep(300000);

    if (activenvp == nvp)
        UpdateOSD();

    activenvp->Unpause();
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

    // Get the current channel and add it to the vector
    QString dummy = "";
    QString chan_name = "";
    activerecorder->GetChannelInfo(dummy, dummy, dummy, dummy, dummy,
                                   dummy, dummy, dummy, chan_name, dummy);

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

        QMap<QString, QString> regexpMap;

        regexpMap["channum"] = channame_vector[vector];
        regexpMap["callsign"] = "";
        osd->ClearAllText("channel_number");
        osd->SetTextByRegexp("channel_number", regexpMap, 1);
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
    QString dummy = "";
    QString chan_name = "";
    activerecorder->GetChannelInfo(dummy, dummy, dummy, dummy, dummy,
                                   dummy, dummy, dummy, chan_name, dummy);

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

void TV::ToggleOSD(void)
{
    if (osd->Visible())
    {
        osd->HideSet("program_info");
        osd->HideSet("channel_number");
    }
    else
        UpdateOSD();
}

void TV::UpdateOSD(void)
{
    QMap<QString, QString> regexpMap;

    GetChannelInfo(activerecorder, regexpMap);

    osd->ClearAllText("program_info");
    osd->SetTextByRegexp("program_info", regexpMap, osd_display_time);
    osd->ClearAllText("channel_number");
    osd->SetTextByRegexp("channel_number", regexpMap, osd_display_time);
}

void TV::UpdateOSDInput(void)
{
    QString dummy = "";
    QString name = "";

    activerecorder->GetInputName(name);

    osd->SetInfoText(name, dummy, dummy, dummy, dummy, dummy, dummy, dummy, 
                     osd_display_time);
}

void TV::GetNextProgram(RemoteEncoder *enc, int direction,
                        QMap<QString, QString> &regexpMap)
{
    if (!enc)
        enc = activerecorder;

    QString title, subtitle, description, category, starttime, endtime;
    QString callsign, iconpath, channum, chanid;

    starttime = regexpMap["dbstarttime"];
    chanid = regexpMap["chanid"];
    channum = regexpMap["channum"];

    enc->GetNextProgram(direction,
                        title, subtitle, description, category, starttime,
                        endtime, callsign, iconpath, channum, chanid);

    QString tmFmt = gContext->GetSetting("TimeFormat");
    QString dtFmt = gContext->GetSetting("ShortDateFormat");
    QDateTime startts = QDateTime::fromString(starttime, Qt::ISODate);
    QDateTime endts = QDateTime::fromString(endtime, Qt::ISODate);
    QString length;
    int hours, minutes, seconds;

    regexpMap["dbstarttime"] = starttime;
    regexpMap["dbendtime"] = endtime;
    regexpMap["title"] = title;
    regexpMap["subtitle"] = subtitle;
    regexpMap["description"] = description;
    regexpMap["category"] = category;
    regexpMap["callsign"] = callsign;
    regexpMap["starttime"] = startts.toString(tmFmt);
    regexpMap["startdate"] = startts.toString(dtFmt);
    regexpMap["endtime"] = endts.toString(tmFmt);
    regexpMap["enddate"] = endts.toString(dtFmt);
    regexpMap["channum"] = channum;
    regexpMap["chanid"] = chanid;
    regexpMap["iconpath"] = iconpath;

    seconds = startts.secsTo(endts);
    minutes = seconds / 60;
    regexpMap["lenmins"] = QString("%1").arg(minutes);
    hours   = minutes / 60;
    minutes = minutes % 60;
    length.sprintf("%d:%02d", hours, minutes);
    regexpMap["lentime"] = length;
}

void TV::GetNextProgram(RemoteEncoder *enc, int direction,
                        QString &title, QString &subtitle, 
                        QString &desc, QString &category, QString &starttime, 
                        QString &endtime, QString &callsign, QString &iconpath,
                        QString &channelname, QString &chanid)
{
    if (!enc)
        enc = activerecorder;

    enc->GetNextProgram(direction,
                        title, subtitle, desc, category, starttime, endtime, 
                        callsign, iconpath, channelname, chanid);
}

void TV::GetChannelInfo(RemoteEncoder *enc, QMap<QString, QString> &regexpMap)
{
    if (!enc)
        enc = activerecorder;

    QString title, subtitle, description, category, starttime, endtime;
    QString callsign, iconpath, channum, chanid;

    enc->GetChannelInfo(title, subtitle, description, category, starttime,
                        endtime, callsign, iconpath, channum, chanid);

    QString tmFmt = gContext->GetSetting("TimeFormat");
    QString dtFmt = gContext->GetSetting("ShortDateFormat");
    QDateTime startts = QDateTime::fromString(starttime, Qt::ISODate);
    QDateTime endts = QDateTime::fromString(endtime, Qt::ISODate);
    QString length;
    int hours, minutes, seconds;

    regexpMap["dbstarttime"] = starttime;
    regexpMap["dbendtime"] = endtime;
    regexpMap["title"] = title;
    regexpMap["subtitle"] = subtitle;
    regexpMap["description"] = description;
    regexpMap["category"] = category;
    regexpMap["callsign"] = callsign;
    regexpMap["starttime"] = startts.toString(tmFmt);
    regexpMap["startdate"] = startts.toString(dtFmt);
    regexpMap["endtime"] = endts.toString(tmFmt);
    regexpMap["enddate"] = endts.toString(dtFmt);
    regexpMap["channum"] = channum;
    regexpMap["chanid"] = chanid;
    regexpMap["iconpath"] = iconpath;

    seconds = startts.secsTo(endts);
    minutes = seconds / 60;
    regexpMap["lenmins"] = QString("%1").arg(minutes);
    hours   = minutes / 60;
    minutes = minutes % 60;
    length.sprintf("%d:%02d", hours, minutes);
    regexpMap["lentime"] = length;
}

void TV::GetChannelInfo(RemoteEncoder *enc, QString &title, QString &subtitle, 
                        QString &desc, QString &category, QString &starttime, 
                        QString &endtime, QString &callsign, QString &iconpath,
                        QString &channelname, QString &chanid)
{
    if (!enc)
        enc = activerecorder;

    enc->GetChannelInfo(title, subtitle, desc, category, starttime, endtime, 
                        callsign, iconpath, channelname, chanid);
}

void TV::EmbedOutput(unsigned long wid, int x, int y, int w, int h)
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

    if (activerecorder)
        activerecorder->GetChannelInfo(dummy, dummy, dummy, dummy, dummy, dummy,
                                       dummy, dummy, channame, dummy);

    QString chanstr = RunProgramGuide(channame, true, this);

    if (chanstr != "")
    {
        chanstr = chanstr.left(4);
        sprintf(channelKeys, "%s", chanstr.ascii());
        channelqueued = true; 
    }

    StopEmbeddingOutput();

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

void TV::ChangeBrightness(bool up)
{
    int brightness = activerecorder->ChangeBrightness(up);

    QString text = QString("Brightness %1 %").arg(brightness);

    if (osd)
        osd->StartPause(brightness * 10, true, "Adjust Picture", text, 5);
}

void TV::ChangeContrast(bool up)
{
    int contrast = activerecorder->ChangeContrast(up);

    QString text = QString("Contrast %1 %").arg(contrast);

    if (osd)
        osd->StartPause(contrast * 10, true, "Adjust Picture", text, 5);
}

void TV::ChangeColour(bool up)
{
    int colour = activerecorder->ChangeColour(up);

    QString text = QString("Colour %1 %").arg(colour);

    if (osd)
        osd->StartPause(colour * 10, true, "Adjust Picture", text, 5);
}

void TV::ChangeDeinterlacer()
{
    QString text;

    deinterlace_mode++;
    if (deinterlace_mode == DEINTERLACE_LAST)
        deinterlace_mode = 0;

    activerecorder->ChangeDeinterlacer(deinterlace_mode);

    text = QString("Deint %1").arg(deinterlacer_names[deinterlace_mode]);

    if (activenvp == nvp)
        osd->SetSettingsText(text, text.length());
}

void TV::ChangeVolume(bool up)
{
    if (!volumeControl)
        return;

    if (up)
        volumeControl->AdjustCurrentVolume(2);
    else 
        volumeControl->AdjustCurrentVolume(-2);

    int curvol = volumeControl->GetCurrentVolume();
    QString text = QString("Volume %1 %").arg(curvol);

    if (osd && !browsemode)
        osd->StartPause(curvol * 10, true, "Adjust Volume", text, 5);
}

void TV::ToggleMute(void)
{
    if (!volumeControl)
        return;

    volumeControl->ToggleMute();
    bool muted = volumeControl->GetMute();

    QString text;

    if (muted)
        text = "Mute On";
    else
        text = "Mute Off";
 
    if (osd && !browsemode)
        osd->SetSettingsText(text, 5);
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

void TV::customEvent(QCustomEvent *e)
{
    if ((MythEvent::Type)(e->type()) == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent *)e;
        QString message = me->Message();

        if (internalState == kState_WatchingRecording &&
            message.left(14) == "DONE_RECORDING")
        {
            message = message.simplifyWhiteSpace();
            QStringList tokens = QStringList::split(" ", message);
            int cardnum = tokens[1].toInt();
            int filelen = tokens[2].toInt();

            if (cardnum == recorder->GetRecorderNumber())
            {
                if (!watchingLiveTV)
                {
                    nvp->SetWatchingRecording(false);
                    nvp->SetLength(filelen);
                    nextState = kState_WatchingPreRecorded;
                }
                else
                    nextState = kState_WatchingLiveTV;
                changeState = true;
            }
        }
        else if (internalState == kState_WatchingLiveTV && 
                 message.left(14) == "ASK_RECORDING ")
        {
            message = message.simplifyWhiteSpace();
            QStringList tokens = QStringList::split(" ", message);
            int cardnum = tokens[1].toInt();
            int timeuntil = tokens[2].toInt();

            if (cardnum == recorder->GetRecorderNumber())
            {
                int retval = AllowRecording(me->ExtraData(), timeuntil);

                QString resp = QString("ASK_RECORDING_RESPONSE %1 %2")
                                    .arg(cardnum)
                                    .arg(retval);
                RemoteSendMessage(resp);
            }
        }
        else if (internalState == kState_WatchingLiveTV &&
                 message.left(13) == "LIVE_TV_READY")
        {
            message = message.simplifyWhiteSpace();
            QStringList tokens = QStringList::split(" ", message);
            int cardnum = tokens[1].toInt();

            if (cardnum == recorder->GetRecorderNumber())
            {
                nextState = kState_WatchingRecording;
                changeState = true;
            }
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

    browsemode = true;

    QString title, subtitle, desc, category, starttime, endtime;
    QString callsign, iconpath, channum, chanid;

    GetChannelInfo(activerecorder, title, subtitle, desc, category, 
                   starttime, endtime, callsign, iconpath, channum, chanid);

    browsechannum = channum;
    browsechanid = chanid;
    browsestarttime = starttime;

    BrowseDispInfo(BROWSE_SAME);
}

void TV::BrowseEnd(bool change)
{
    osd->HideSet("browse_info");

    if (change)
    {
        ChangeChannelByString(browsechannum);
    }

    browsemode = false;
}

void TV::BrowseDispInfo(int direction)
{
    QDateTime curtime = QDateTime::currentDateTime();
    QDateTime maxtime = curtime.addSecs(60 * 60 * 4);
    QDateTime lastprogtime =
                  QDateTime::fromString(browsestarttime, Qt::ISODate);
    QMap<QString, QString> regexpMap;

    if (paused)
        return;

    if (lastprogtime < curtime)
        browsestarttime = curtime.toString("yyyyMMddhhmm") + "00";

    if ((lastprogtime > maxtime) &&
        (direction == BROWSE_RIGHT))
        return;

    regexpMap["channum"] = browsechannum;
    regexpMap["dbstarttime"] = browsestarttime;
    regexpMap["chanid"] = browsechanid;

    if (direction != BROWSE_SAME)
        GetNextProgram(activerecorder, direction, regexpMap);
    else
        GetChannelInfo(activerecorder, regexpMap);

    browsechannum = regexpMap["channum"];
    browsechanid = regexpMap["chanid"];
    browsestarttime = regexpMap["dbstarttime"];

    QDateTime startts = QDateTime::fromString(browsestarttime, Qt::ISODate);
    ProgramInfo *program_info = ProgramInfo::GetProgramAtDateTime(
                                      browsechanid, startts );
    if (program_info)
        program_info->ToMap(m_db, regexpMap);

    osd->ClearAllText("browse_info");
    osd->SetTextByRegexp("browse_info", regexpMap, -1);

    delete program_info;
}

void TV::BrowseToggleRecord(void)
{
    QMap<QString, QString> regexpMap;
    QDateTime startts = QDateTime::fromString(browsestarttime, Qt::ISODate);

    ProgramInfo *program_info = ProgramInfo::GetProgramAtDateTime(
                                      browsechanid, startts );
    program_info->ToggleRecord(m_db);

    program_info->ToMap(m_db, regexpMap);

    osd->ClearAllText("browse_info");
    osd->SetTextByRegexp("browse_info", regexpMap, -1);

    delete program_info;
}

