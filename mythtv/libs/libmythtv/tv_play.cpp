#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <qsqldatabase.h>
#include <qapplication.h>

#include <iostream>
using namespace std;

#include "tv.h"
#include "osd.h"
#include "mythcontext.h"
#include "dialogbox.h"
#include "remoteencoder.h"
#include "remoteutil.h"
#include "guidegrid.h"

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
    prbuffer = NULL;
    nvp = NULL;
    osd = NULL;
    requestDelete = false;
    endOfRecording = false;

    gContext->addListener(this);
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
}

TVState TV::LiveTV(void)
{
    if (internalState == kState_None)
    {
        RemoteEncoder *testrec = RemoteRequestRecorder();

        if (!testrec->IsValidRecorder())
        {
            QString title = "MythTV is already using all available inputs for "
                            "recording.  If you want to watch an in-progress "
                            "recording, select one from the playback menu.  If "
                            "you want to watch live TV, cancel one of the "
                            "in-progress recordings from the delete menu.";
    
            DialogBox diag(title);
            diag.AddButton("Cancel and go back to the TV menu");

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

int TV::AllowRecording(const QString &message, int timeuntil)
{
    if (internalState != kState_WatchingLiveTV)
    {
        return 1;
    }

    QString option1 = "Record and watch while it records";
    QString option2 = "Let it record and go back to the Main Menu";
    QString option3 = "Don't let it record, I want to watch TV";

    dialogname = "allowrecordingbox";

    while (!osd)
    {
        qApp->unlock();
        qApp->processEvents();
        usleep(1000);
        qApp->lock();
    }

    osd->NewDialogBox(dialogname, message, option1, option2, option3, 
                      "", timeuntil);

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

        if (rcinfo->conflicting)
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

    osd_display_time = gContext->GetNumSetting("OSDDisplayTime");

    if (gContext->GetNumSetting("Deinterlace"))
    {
        if (filters.length() > 1)
            filters += ",";
        filters += "linearblend";
    }

    nvp->SetVideoFilters(filters);

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
        prbuffer->StopReads();
        nvp->StopPlaying();
        pthread_join(decode, NULL);
        delete nvp;
    }

    paused = false;
    doing_ff = false;
    doing_rew = false;
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
        piprbuffer->StopReads();
        pipnvp->StopPlaying();
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

    int pausecheck = 0;

    channelqueued = false;
    channelKeys[0] = channelKeys[1] = channelKeys[2] = ' ';
    channelKeys[3] = 0;
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
                if (ff_rew_scaling > 0)
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

        case 's': case 'S': case 'p': case 'P': 
        {
            doing_ff = false;
            doing_rew = false;
            DoPause();
            break;
        }
        case wsRight: case 'd': case 'D': 
        {
            doing_ff = true;
            doing_rew = false;
            DoFF(); 
            break;
        }
        case wsLeft: case 'a': case 'A': 
        {
            doing_rew = true;
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

                QString message = QString("You are exiting this recording");

                QString option1 = "Save this position and go to the menu";
                QString option2 = "Do not save, just exit to the menu";
                QString option3 = "Keep watching";
                QString option4 = "Delete this recording";

                dialogname = "exitplayoptions";
                osd->NewDialogBox(dialogname, message, option1, option2, 
                                  option3, option4, 0);
            } 
            else 
            {
                exitPlayer = true;
                break;
            }
            break;
        }
        default: 
        {
            if (doing_ff || doing_rew)
            {
                switch (keypressed)
                {
                    case wsZero:   case '0': ff_rew_scaling =  0.00; break;
                    case wsOne:    case '1': ff_rew_scaling =  0.25; break;
                    case wsTwo:    case '2': ff_rew_scaling =  0.50; break;
                    case wsThree:  case '3': ff_rew_scaling =  1.00; break;
                    case wsFour:   case '4': ff_rew_scaling =  1.50; break;
                    case wsFive:   case '5': ff_rew_scaling =  2.24; break;
                    case wsSix:    case '6': ff_rew_scaling =  3.34; break;
                    case wsSeven:  case '7': ff_rew_scaling =  7.48; break;
                    case wsEight:  case '8': ff_rew_scaling = 11.18; break;
                    case wsNine:   case '9': ff_rew_scaling = 16.72; break;

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
            case 'i': case 'I': UpdateOSD(); break;

            case wsUp: ChangeChannel(true); break;

            case wsDown: ChangeChannel(false); break;

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

            default: break;
        }
    }
    else if (StateIsPlaying(internalState))
    {
        switch (keypressed)
        {
            case 'i': case 'I': DoPosition(); break;
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
                                dummy, dummy, pipchanname);
    recorder->GetChannelInfo(dummy, dummy, dummy, dummy, dummy, dummy,
                             dummy, dummy, bigchanname);

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

void TV::DoPosition(void)
{
    if (activenvp != nvp)
        return;

    QString desc = "";
    int pos = calcSliderPos(0, desc);
    osd->StartPause(pos, false, "Position", desc, osd_display_time);
}

void TV::DoFF(void)
{
    bool slidertype = false;
    if (internalState == kState_WatchingLiveTV)
        slidertype = true;

    if (activenvp == nvp)
    {
        QString desc = "";
        int pos = calcSliderPos((int)(fftime * ff_rew_scaling), desc);
        osd->StartPause(pos, slidertype, "Forward", desc, 2);
    }

    activenvp->FastForward(fftime * ff_rew_scaling);
}

void TV::DoRew(void)
{
    bool slidertype = false;
    if (internalState == kState_WatchingLiveTV)
        slidertype = true;

    if (activenvp == nvp)
    {
        QString desc = "";
        int pos = calcSliderPos(0 - (int)(rewtime * ff_rew_scaling), desc);
        osd->StartPause(pos, slidertype, "Rewind", desc, 2);
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

void TV::ChangeChannel(bool up)
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
    activerecorder->ChangeChannel(up);

    activenvp->ResetPlaying();
    while (!activenvp->ResetYet())
        usleep(5);

    usleep(300000);

    if (activenvp == nvp)
        UpdateOSD();

    activenvp->Unpause();

    channelqueued = false;
    channelKeys[0] = channelKeys[1] = channelKeys[2] = ' ';
    channelkeysstored = 0;
}

void TV::ChannelKey(int key)
{
    char thekey = key;

    if (key > 256)
        thekey = key - 256 - 0xb0 + '0';

    if (channelkeysstored == 3)
    {
        channelKeys[0] = channelKeys[1];
        channelKeys[1] = channelKeys[2];
        channelKeys[2] = thekey;
    }
    else
    {
        channelKeys[channelkeysstored] = thekey; 
        channelkeysstored++;
    }
    channelKeys[3] = 0;

    if (activenvp == nvp)
        osd->SetChannumText(channelKeys, 2);

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
    channelKeys[0] = channelKeys[1] = channelKeys[2] = ' ';
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

    activerecorder->Pause();
    activerbuffer->Reset();
    activerecorder->SetChannel(name);

    activenvp->ResetPlaying();
    while (!activenvp->ResetYet())
        usleep(5);

    usleep(300000);

    if (activenvp == nvp)
        UpdateOSD();

    activenvp->Unpause();
}

void TV::UpdateOSD(void)
{
    QString title, subtitle, desc, category, starttime, endtime;
    QString callsign, iconpath, channelname;

    GetChannelInfo(activerecorder, title, subtitle, desc, category, 
                   starttime, endtime, callsign, iconpath, channelname);

    osd->SetInfoText(title, subtitle, desc, category, starttime, endtime,
                     callsign, iconpath, osd_display_time);
    osd->SetChannumText(channelname, osd_display_time);
}

void TV::UpdateOSDInput(void)
{
    QString dummy = "";
    QString name = "";

    activerecorder->GetInputName(name);

    osd->SetInfoText(name, dummy, dummy, dummy, dummy, dummy, dummy, dummy, 
                     osd_display_time);
}

void TV::GetChannelInfo(RemoteEncoder *enc, QString &title, QString &subtitle, 
                        QString &desc, QString &category, QString &starttime, 
                        QString &endtime, QString &callsign, QString &iconpath,
                        QString &channelname)
{
    enc->GetChannelInfo(title, subtitle, desc, category, starttime, endtime, 
                        callsign, iconpath, channelname);
}

void TV::EmbedOutput(unsigned long wid, int x, int y, int w, int h)
{
    if (nvp)
        nvp->EmbedInWidget(wid, x, y, w, h);
}

void TV::StopEmbeddingOutput(void)
{
    if (nvp)
        nvp->StopEmbedding();
}

void TV::doLoadMenu(void)
{
    QString dummy;
    QString channame = "3";

    if (recorder)
        recorder->GetChannelInfo(dummy, dummy, dummy, dummy, dummy, dummy, 
                                 dummy, dummy, channame);

    QString chanstr = RunProgramGuide(channame, true, this);

    if (chanstr != "")
    {
        chanstr = chanstr.left(3);
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
    QString text;

    if (up)
        text = "Brightness +";
    else
        text = "Brightness -";

    activerecorder->ChangeBrightness(up);

    if (activenvp == nvp)
        osd->SetSettingsText(text, text.length() );
}

void TV::ChangeContrast(bool up)
{
    QString text;

    if (up)
        text = "Contrast +";
    else
        text = "Contrast -";

    activerecorder->ChangeContrast(up);

    if (activenvp == nvp)
        osd->SetSettingsText(text, text.length() );
}

void TV::ChangeColour(bool up)
{
    QString text;

    if (up)
        text = "Color +";
    else
        text = "Color -";

    activerecorder->ChangeColour(up);

    if (activenvp == nvp)
        osd->SetSettingsText(text, text.length() );
}

void TV::EPGChannelUpdate(QString chanstr)
{
    if (chanstr != "")
    {
        chanstr = chanstr.left(3);
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

