#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <qsqldatabase.h>

#include "XJ.h"
#include "tv.h"

char theprefix[] = "/usr/local";

void *SpawnEncode(void *param)
{
    NuppelVideoRecorder *nvr = (NuppelVideoRecorder *)param;

    nvr->StartRecording();
  
    return NULL;
}

void *SpawnDecode(void *param)
{
    NuppelVideoPlayer *nvp = (NuppelVideoPlayer *)param;

    nvp->StartPlaying();

    return NULL;
}

TV::TV(const string &startchannel)
{
    string settingsfile = string(theprefix) + 
                          string("/share/mythtv/settings.txt");
    settings = new Settings(settingsfile);

    settings->ReadSettings("settings.txt");

    db_conn = NULL;
    ConnectDB();
    
    channel = new Channel(this, settings->GetSetting("V4LDevice"));

    channel->Open();

    channel->SetFormat(settings->GetSetting("TVFormat"));
    channel->SetFreqTable(settings->GetSetting("FreqTable"));

    channel->SetChannelByString(startchannel);

    channel->Close();  

    nvr = NULL;
    nvp = NULL;
    prbuffer = rbuffer = NULL;

    menurunning = false;

    internalState = nextState = kState_None; 

    runMainLoop = false;
    changeState = false;

    watchingLiveTV = false;

    pthread_create(&event, NULL, EventThread, this);

    while (!runMainLoop)
        usleep(50);

    curRecording = NULL;

    tvtorecording = 1;
}

TV::~TV(void)
{
    runMainLoop = false;
    pthread_join(event, NULL);

    if (channel)
        delete channel;
    if (settings)
        delete settings;
    if (rbuffer)
        delete rbuffer;
    if (prbuffer && prbuffer != rbuffer)
        delete prbuffer;
    if (nvp)
        delete nvp;
    if (nvr)
        delete nvr;
    if (db_conn)
        DisconnectDB();
}

void TV::LiveTV(void)
{
    if (internalState == kState_RecordingOnly)
    {
        char cmdline[4096];

        sprintf(cmdline, "mythdialog \"MythTV is already recording '%s' on "
                         "channel %s.  Do you want to:\" \"Stop recording and "
                         "watch TV\" \"Watch the in-progress recording\" "
                         "\"Cancel\"", curRecording->title.ascii(), 
                         curRecording->channum.ascii());

        int result = system(cmdline);

        if (result > 0)
            result = WEXITSTATUS(result);

        if (result == 1)
        {
            nextState = kState_WatchingLiveTV;
            changeState = true;
        }
        else if (result == 2)
        {
            nextState = kState_WatchingRecording;
            inputFilename = outputFilename;
            changeState = true;
        }
    }
    else if (internalState == kState_None)
    {
        nextState = kState_WatchingLiveTV;
        changeState = true;
    }
}

int TV::AllowRecording(ProgramInfo *rcinfo, int timeuntil)
{
    if (internalState != kState_WatchingLiveTV)
    {
        return 1;
    }

    string channame = rcinfo->channum.ascii();
    string title = rcinfo->title.ascii();

    string message = "MythTV wants to record \"";
    message += title + "\" on Channel " + channame;
    message += " in %d seconds.  Do you want to:";

    string option1 = "Record and watch while it records";
    string option2 = "Let it record and go back to the Main Menu";
    string option3 = "Don't let it record, I want to watch TV";

    nvp->SetDialogBox(message, option1, option2, option3, timeuntil);

    while (nvp->DialogVisible())
        usleep(50);

    int result = nvp->GetDialogSelection();

    tvtorecording = result;

    return result;
}

void TV::StartRecording(ProgramInfo *rcinfo)
{  
    string recprefix = settings->GetSetting("RecordFilePrefix");

    if (internalState == kState_None || 
        internalState == kState_WatchingPreRecorded)
    {
        outputFilename = rcinfo->GetRecordFilename(recprefix.c_str());
        recordEndTime = rcinfo->endts;
        curRecording = rcinfo;

        if (internalState == kState_None)
            nextState = kState_RecordingOnly;
        else if (internalState == kState_WatchingPreRecorded)
            nextState = kState_WatchingOtherRecording;
        changeState = true;
    }
    else if (internalState == kState_WatchingLiveTV)
    {
        if (tvtorecording == 2)
        {
            nextState = kState_None;
            changeState = true;

            while (GetState() != kState_None)
                usleep(50);

            outputFilename = rcinfo->GetRecordFilename(recprefix.c_str());
            recordEndTime = rcinfo->endts;
            curRecording = rcinfo;

            nextState = kState_RecordingOnly;
            changeState = true;
        }
        else if (tvtorecording == 1)
        {
            outputFilename = rcinfo->GetRecordFilename(recprefix.c_str());
            recordEndTime = rcinfo->endts;
            curRecording = rcinfo;

            nextState = kState_WatchingRecording;
            changeState = true;
        }
        tvtorecording = 1;
    }  
}

void TV::StopRecording(void)
{
    if (StateIsRecording(internalState))
    {
        nextState = RemoveRecording(internalState);
        changeState = true;
    }
}

void TV::Playback(ProgramInfo *rcinfo)
{
    if (internalState == kState_None || internalState == kState_RecordingOnly)
    {
        string recprefix = settings->GetSetting("RecordFilePrefix");
        inputFilename = rcinfo->GetRecordFilename(recprefix.c_str());

        if (internalState == kState_None)
            nextState = kState_WatchingPreRecorded;
        else if (internalState == kState_RecordingOnly)
        {
            if (inputFilename == outputFilename)
                nextState = kState_WatchingRecording;
            else
                nextState = kState_WatchingOtherRecording;
        }
        changeState = true;
    }
}

void TV::StateToString(TVState state, string &statestr)
{
    switch (state) {
        case kState_None: statestr = "None"; break;
        case kState_WatchingLiveTV: statestr = "WatchingLiveTV"; break;
        case kState_WatchingPreRecorded: statestr = "WatchingPreRecorded";
                                         break;
        case kState_WatchingRecording: statestr = "WatchingRecording"; break;
        case kState_WatchingOtherRecording: statestr = "WatchingOtherRecording";
                                            break;
        case kState_RecordingOnly: statestr = "RecordingOnly"; break;
        default: statestr = "Unknown"; break;
    }
}

bool TV::StateIsRecording(TVState state)
{
    bool retval = false;

    if (state == kState_RecordingOnly || 
        state == kState_WatchingRecording ||
        state == kState_WatchingOtherRecording)
    {
        retval = true;
    }

    return retval;
}

bool TV::StateIsPlaying(TVState state)
{
    bool retval = false;

    if (state == kState_WatchingPreRecorded || 
        state == kState_WatchingRecording ||
        state == kState_WatchingOtherRecording)
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
    {
        if (state == kState_WatchingPreRecorded)
            return kState_None;
        return kState_RecordingOnly;
    }
    return kState_Error;
}

void TV::WriteRecordedRecord(void)
{
    if (!curRecording)
        return;

    curRecording->WriteRecordedToDB(db_conn);
    delete curRecording;
    curRecording = NULL;
}

void TV::HandleStateChange(void)
{
    bool changed = false;
    bool startPlayer = false;
    bool startRecorder = false;
    bool closePlayer = false;
    bool closeRecorder = false;
    bool killRecordingFile = false;
    bool sleepBetween = false;

    string statename;
    StateToString(nextState, statename);
    string origname;
    StateToString(internalState, origname);

    if (nextState == kState_Error)
    {
        printf("ERROR: Attempting to set to an error state.\n");
        exit(-1);
    }

    if (internalState == kState_None && nextState == kState_WatchingLiveTV)
    {
        long long filesize = settings->GetNumSetting("BufferSize");
        filesize = filesize * 1024 * 1024 * 1024;
        long long smudge = settings->GetNumSetting("MaxBufferFill");
        smudge = smudge * 1024 * 1024; 

        rbuffer = new RingBuffer(settings->GetSetting("BufferName"), filesize, 
                                 smudge);
        prbuffer = rbuffer;

        internalState = nextState;
        changed = true;

        startPlayer = startRecorder = true;

        watchingLiveTV = true;
        sleepBetween = true;

        printf("Changing from %s to %s\n", origname.c_str(), statename.c_str());
    }
    else if (internalState == kState_WatchingLiveTV && 
             nextState == kState_None)
    {
        closePlayer = true;
        closeRecorder = true;

        internalState = nextState;
        changed = true;

        watchingLiveTV = false;

        printf("Changing from %s to %s\n", origname.c_str(), statename.c_str());
    }
    else if ((internalState == kState_None && 
              nextState == kState_RecordingOnly) || 
             (internalState == kState_WatchingPreRecorded &&
              nextState == kState_WatchingOtherRecording))
    {   
        string channame = curRecording->channum.ascii();

        channel->Open();
        channel->SetChannelByString(channame);
        channel->Close();

        rbuffer = new RingBuffer(outputFilename, true);

        internalState = nextState;
        nextState = kState_None;
        changed = true;

        startRecorder = true;

        printf("Changing from %s to %s\n", origname.c_str(), statename.c_str());
    }   
    else if ((internalState == kState_RecordingOnly && 
              nextState == kState_None) ||
             (internalState == kState_WatchingRecording &&
              nextState == kState_WatchingPreRecorded) ||
             (internalState == kState_WatchingOtherRecording &&
              nextState == kState_WatchingPreRecorded))
    {
        closeRecorder = true;

        internalState = nextState;
        changed = true;

        watchingLiveTV = false;

        printf("Changing from %s to %s\n", origname.c_str(), statename.c_str());
    }
    else if ((internalState == kState_None && 
              nextState == kState_WatchingPreRecorded) ||
             (internalState == kState_RecordingOnly &&
              nextState == kState_WatchingOtherRecording) ||
             (internalState == kState_RecordingOnly &&
              nextState == kState_WatchingRecording))
    {
        prbuffer = new RingBuffer(inputFilename, false);

        internalState = nextState;
        changed = true;

        startPlayer = true;
    
        printf("Changing from %s to %s\n", origname.c_str(), statename.c_str());
    }
    else if ((internalState == kState_WatchingPreRecorded && 
              nextState == kState_None) || 
             (internalState == kState_WatchingOtherRecording &&
              nextState == kState_RecordingOnly) ||
             (internalState == kState_WatchingRecording &&
              nextState == kState_RecordingOnly))
    {
        closePlayer = true;
        
        internalState = nextState;
        changed = true;

        watchingLiveTV = false;

        printf("Changing from %s to %s\n", origname.c_str(), statename.c_str());
    }
    else if (internalState == kState_RecordingOnly && 
             nextState == kState_WatchingLiveTV)
    {
        TeardownRecorder(true);
        
        long long filesize = settings->GetNumSetting("BufferSize");
        filesize = filesize * 1024 * 1024 * 1024;
        long long smudge = settings->GetNumSetting("MaxBufferFill");
        smudge = smudge * 1024 * 1024;

        rbuffer = new RingBuffer(settings->GetSetting("BufferName"), filesize,
                                 smudge);
        prbuffer = rbuffer;

        internalState = nextState;
        changed = true;

        startPlayer = startRecorder = true;

        watchingLiveTV = true;
        sleepBetween = true;

        printf("Changing from %s to %s\n", origname.c_str(), statename.c_str());
    }
    else if (internalState == kState_WatchingLiveTV &&  
             nextState == kState_WatchingRecording)
    {
        nvp->Pause();
        while (!nvp->GetPause())
            usleep(5);

        nvr->Pause();
        while (!nvr->GetPause())
            usleep(5);

        rbuffer->Reset();

        string channame = curRecording->channum.ascii();

        channel->SetChannelByString(channame);

        rbuffer->TransitionToFile(outputFilename);
        nvr->WriteHeader(true);
        nvr->Reset();
        nvr->Unpause();

        nvp->ResetPlaying();
        while (!nvp->ResetYet())
            usleep(5);

        usleep(300000);

        nvp->Unpause();
        internalState = nextState;
        changed = true;

        printf("Changing from %s to %s\n", origname.c_str(), statename.c_str());
    }
    else if ((internalState == kState_WatchingRecording &&
              nextState == kState_WatchingLiveTV) ||
             (internalState == kState_WatchingPreRecorded &&
              nextState == kState_WatchingLiveTV))
    {
        nvp->Pause();
        while (!nvp->GetPause())
            usleep(50);

        rbuffer->TransitionToRing();

        nvp->Unpause();
 
        WriteRecordedRecord();
 
        internalState = nextState;
        changed = true;

        printf("Changing from %s to %s\n", origname.c_str(), statename.c_str());
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
    changeState = false;
 
    if (startRecorder)
    {
        SetupRecorder();
        pthread_create(&encode, NULL, SpawnEncode, nvr);

        while (!nvr->IsRecording())
            usleep(50);

        // evil.
        channel->SetFd(nvr->GetVideoFd());

        frameRate = nvr->GetFrameRate();
    }

    if (sleepBetween) 
        usleep(300000);

    if (startPlayer)
    {
        SetupPlayer();
        pthread_create(&decode, NULL, SpawnDecode, nvp);

        while (!nvp->IsPlaying())
            usleep(50);

        frameRate = nvp->GetFrameRate();
    }

    if (closeRecorder)
        TeardownRecorder(killRecordingFile);

    if (closePlayer)
        TeardownPlayer();
}

void TV::SetupRecorder(void)
{
    if (nvr)
    {  
        printf("Attempting to setup a recorder, but it already exists\n");
        return;
    }

    nvr = new NuppelVideoRecorder();
    nvr->SetRingBuffer(rbuffer);
    nvr->SetMotionLevels(settings->GetNumSetting("LumaFilter"),
                         settings->GetNumSetting("ChromaFilter"));
    nvr->SetQuality(settings->GetNumSetting("Quality"));
    nvr->SetResolution(settings->GetNumSetting("Width"),
                     settings->GetNumSetting("Height"));
    nvr->SetMP3Quality(settings->GetNumSetting("MP3Quality"));
    nvr->SetAudioSampleRate(settings->GetNumSetting("AudioSampleRate"));
    nvr->SetVideoDevice((char *)settings->GetSetting("V4LDevice").c_str());
    nvr->SetAudioDevice((char *)settings->GetSetting("AudioDevice").c_str());
    nvr->Initialize();
}

void TV::TeardownRecorder(bool killFile)
{
    if (nvr)
    {
        nvr->StopRecording();
        pthread_join(encode, NULL);
        delete nvr;
    }
    nvr = NULL;

    if (rbuffer && rbuffer != prbuffer)
    {
        delete rbuffer;
        rbuffer = NULL;
    }

    if (killFile)
    {
        unlink(outputFilename.c_str());
        if (curRecording)
        {
            delete curRecording;
            curRecording = NULL;
        }
    }
    else if (curRecording)
    {
        WriteRecordedRecord();
    }
}    

void TV::SetupPlayer(void)
{
    if (nvp)
    { 
        printf("Attempting to setup a player, but it already exists.\n");
        return;
    }

    nvp = new NuppelVideoPlayer();
    nvp->SetRingBuffer(prbuffer);
    nvp->SetRecorder(nvr);
    nvp->SetDeinterlace((bool)settings->GetNumSetting("Deinterlace"));
    nvp->SetOSDFontName((char *)settings->GetSetting("OSDFont").c_str(),
                        theprefix);
    nvp->SetAudioSampleRate(settings->GetNumSetting("AudioSampleRate"));
    nvp->SetAudioDevice((char *)settings->GetSetting("AudioDevice").c_str());
    osd_display_time = settings->GetNumSetting("OSDDisplayTime");
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
    nvp = NULL;

    if (prbuffer && prbuffer != rbuffer)
    {
        delete prbuffer;
        prbuffer = NULL;
    }

    if (prbuffer && prbuffer == rbuffer)
    {
        if (internalState == kState_RecordingOnly)
        {
            prbuffer = NULL;
        }
        else
        {
            delete prbuffer;
            prbuffer = rbuffer = NULL;
        }
    }
}

char *TV::GetScreenGrab(ProgramInfo *rcinfo, int secondsin, int &bufferlen,
                        int &video_width, int &video_height)
{
    string recprefix = settings->GetSetting("RecordFilePrefix");
    QString filename = rcinfo->GetRecordFilename(recprefix.c_str());

    RingBuffer *tmprbuf = new RingBuffer(filename.ascii(), false);

    NuppelVideoPlayer *nupvidplay = new NuppelVideoPlayer();
    nupvidplay->SetRingBuffer(tmprbuf);
    nupvidplay->SetAudioSampleRate(settings->GetNumSetting("AudioSampleRate"));
    char *retbuf = nupvidplay->GetScreenGrab(secondsin, bufferlen, video_width,
                                             video_height);

    delete nupvidplay;
    delete tmprbuf;

    return retbuf;
}

void *TV::EventThread(void *param)
{
    TV *thetv = (TV *)param;
    thetv->RunTV();

    return NULL;
}

void TV::RunTV(void)
{ 
    frameRate = 29.97; // the default's not used, but give it a default anyway.

    paused = false;
    int keypressed;
 
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

        if ((keypressed = XJ_CheckEvents()))
        {
           ProcessKeypress(keypressed);
        }

        if (StateIsRecording(internalState))
        {
            if (QDateTime::currentDateTime() > recordEndTime)
            {
                if (watchingLiveTV)
                {
                    nextState = kState_WatchingLiveTV;
                    changeState = true;
                }
                else
                {
                    nextState = RemoveRecording(internalState);
                    changeState = true;
                }
            }
        }

        if (StateIsPlaying(internalState))
        {
            if (!nvp->IsPlaying())
            {
                nextState = RemovePlaying(internalState);
                changeState = true;
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

        if (internalState == kState_WatchingLiveTV)
        {
            if (paused)
            {
                //fprintf(stderr, "\r Paused: %f seconds behind realtime (%f%% buffer left)", (float)(nvr->GetFramesWritten() - nvp->GetFramesPlayed()) / frameRate, (float)rbuffer->GetFreeSpace() / (float)rbuffer->GetFileSize() * 100.0);
            }
            else
            {
                //fprintf(stderr, "\r                                                                      ");
                //fprintf(stderr, "\r Playing: %f seconds behind realtime", (float)(nvr->GetFramesWritten() - nvp->GetFramesPlayed()) / frameRate);
            }

            if (channelqueued && !nvp->OSDVisible())
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
    if (nvp && nvp->DialogVisible())
    {
        switch (keypressed)
        {
            case wsUp: nvp->DialogUp(); break;
            case wsDown: nvp->DialogDown(); break;
            case ' ': case wsEnter: case wsReturn: nvp->TurnOffOSD(); break;
            default: break;
        }
        
        return;
    }

    switch (keypressed) 
    {
        case 's': case 'S':
        case 'p': case 'P': paused = nvp->TogglePause(); break;

        case wsRight: case 'd': case 'D': nvp->FastForward(5); break;

        case wsLeft: case 'a': case 'A': nvp->Rewind(5); break;

        case wsEscape: exitPlayer = true; break;

        default: break;
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

            case wsEnter: ChannelCommit(); break;

            case 'M': case 'm': LoadMenu(); break;

            default: break;
        }
    }
}

void TV::ToggleInputs(void)
{
    nvp->Pause();
    while (!nvp->GetPause())
        usleep(5);

    nvr->Pause();
    while (!nvr->GetPause())
        usleep(5);

    rbuffer->Reset();

    channel->ToggleInputs();

    nvr->Reset();
    nvr->Unpause();

    nvp->ResetPlaying();
    while (!nvp->ResetYet())
        usleep(5);

    usleep(300000);

    UpdateOSD();

    nvp->Unpause();
}

void TV::ChangeChannel(bool up)
{
    nvp->Pause();
    while (!nvp->GetPause())
        usleep(5);

    nvr->Pause();
    while (!nvr->GetPause())
        usleep(5);

    rbuffer->Reset();

    if (up)
        channel->ChannelUp();
    else
        channel->ChannelDown();

    nvr->Reset();
    nvr->Unpause();

    nvp->ResetPlaying();
    while (!nvp->ResetYet())
        usleep(5);

    usleep(300000);

    UpdateOSD();

    nvp->Unpause();

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
    nvp->SetChannelText(channelKeys, 2);

    channelqueued = true;
}

void TV::ChannelCommit(void)
{
    if (!channelqueued)
        return;

    ChangeChannel(channelKeys);

    channelqueued = false;
    channelKeys[0] = channelKeys[1] = channelKeys[2] = ' ';
    channelkeysstored = 0;
}

void TV::ChangeChannel(char *name)
{
    if (!CheckChannel(name))
        return;

    nvp->Pause();
    while (!nvp->GetPause())
        usleep(5);

    nvr->Pause();
    while (!nvr->GetPause())
        usleep(5);

    rbuffer->Reset();

    int channum = atoi(name) - 1;
    int prevchannel = channel->GetCurrent() - 1;
    
    if (!channel->SetChannel(channum))
        channel->SetChannel(prevchannel);

    nvr->Reset();
    nvr->Unpause();

    nvp->ResetPlaying();
    while (!nvp->ResetYet())
        usleep(5);

    usleep(300000);

    UpdateOSD();

    nvp->Unpause();
}

void TV::UpdateOSD(void)
{
    string channelname = channel->GetCurrentName();
    if (channelname.size() > 6)
    {
        string dummy = "";
        nvp->SetInfoText(channelname, dummy, dummy, dummy, dummy, dummy,
                         osd_display_time);
    }
    else
    {
        string title, subtitle, desc, category, starttime, endtime;
        GetChannelInfo(channel->GetCurrent(), title, subtitle, desc, category, 
                       starttime, endtime);

        nvp->SetInfoText(title, subtitle, desc, category, starttime, endtime,
                         osd_display_time);
        nvp->SetChannelText(channel->GetCurrentName(), osd_display_time);
    }
}

void TV::GetChannelInfo(int lchannel, string &title, string &subtitle,
                        string &desc, string &category, string &starttime,
                        string &endtime)
{
    title = "";
    subtitle = "";
    desc = "";
    category = "";
    starttime = "";
    endtime = "";

    if (!db_conn)
        return;

    char curtimestr[128];
    time_t curtime;
    struct tm *loctime;

    curtime = time(NULL);
    loctime = localtime(&curtime);

    strftime(curtimestr, 128, "%Y%m%d%H%M%S", loctime);

    char thequery[1024];
    sprintf(thequery, "SELECT channum,starttime,endtime,title,subtitle,description,category FROM program WHERE channum = %d AND starttime < %s AND endtime > %s;", lchannel, curtimestr, curtimestr);

    QSqlQuery query = db_conn->exec(thequery);

    QString test;
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        starttime = query.value(1).toString().ascii();
        endtime = query.value(2).toString().ascii();
        test = query.value(3).toString();
        if (test != QString::null)
            title = test.ascii();
        test = query.value(4).toString();
        if (test != QString::null)
            subtitle = test.ascii();
        test = query.value(5).toString();
        if (test != QString::null)
            desc = test.ascii();
        test = query.value(6).toString();
        if (test != QString::null)
            category = test.ascii();
    }
}

void TV::ConnectDB(void)
{
    db_conn = QSqlDatabase::addDatabase("QMYSQL3", "TV");
    if (!db_conn)
    {
        printf("Couldn't initialize mysql connection\n");
        return;
    }
    db_conn->setDatabaseName("mythconverg");
    db_conn->setUserName("mythtv");
    db_conn->setPassword("mythtv");
    db_conn->setHostName("localhost");

    if (!db_conn->open())
    {
        printf("Couldn't open database\n");
    }
}

void TV::DisconnectDB(void)
{
    if (db_conn)
    {
        db_conn->close();
        delete db_conn;
    }
}

bool TV::CheckChannel(char *channum)
{
    if (!db_conn)
        return true;

    bool ret = false;
    char thequery[1024];
    sprintf(thequery, "SELECT * FROM channel WHERE channum = %s", channum);

    QSqlQuery query = db_conn->exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
        ret = true;

    return ret;
}

bool TV::ChangeExternalChannel(char *channum)
{
    string command = settings->GetSetting("ExternalChannelCommand");

    command += string(" ") + string(channum);

    int ret = system(command.c_str());

    if (ret > 0)
        return true;
    return false;
}

void TV::doLoadMenu(void)
{
    string epgname = "mythepg";

    char runname[512];

    sprintf(runname, "%s %d", epgname.c_str(), channel->GetCurrent());
    int ret = system(runname);

    if (ret > 0)
    {
        ret = WEXITSTATUS(ret);
        sprintf(channelKeys, "%d", ret);
        channelqueued = true; 
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
