#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "XJ.h"
#include "tv.h"

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
    settings = new Settings("settings.txt");

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
    secsToRecord = -1;

    runMainLoop = false;
    changeState = false;

    pthread_create(&event, NULL, EventThread, this);

    while (!runMainLoop)
        usleep(50);
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
    if (internalState != kState_None)
    {
        printf("Error, attempting to watch live tv when there's something "
               "already going on.\n");
        return;
    }
    else
    {
        nextState = kState_WatchingLiveTV;
        changeState = true;
    }
}

void TV::StartRecording(const string &channelName, int duration,
                        const string &outputFileName)
{   
    if (internalState == kState_None || 
        internalState == kState_WatchingPreRecorded)
    {
        recordChannel = channelName;
        outputFilename = outputFileName;
        secsToRecord = duration;
        if (internalState == kState_None)
            nextState = kState_RecordingOnly;
        else if (internalState == kState_WatchingPreRecorded)
            nextState = kState_WatchingOtherRecording;
        changeState = true;
    }   
}

void TV::Playback(const string &inputFileName)
{
    if (internalState == kState_None || internalState == kState_RecordingOnly)
    {
        inputFilename = inputFileName;

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

void TV::HandleStateChange(void)
{
    bool changed = false;
    bool startPlayer = false;
    bool startRecorder = false;
    bool pauseBetween = false;
    bool closePlayer = false;
    bool closeRecorder = false;

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

        startPlayer = startRecorder = pauseBetween = true;

        printf("Changing from %s to %s\n", origname.c_str(), statename.c_str());
    }
    else if (internalState == kState_WatchingLiveTV && 
             nextState == kState_None)
    {
        closePlayer = true;
        closeRecorder = true;

        internalState = nextState;
        changed = true;

        printf("Changing from %s to %s\n", origname.c_str(), statename.c_str());
    }
    else if ((internalState == kState_None && 
              nextState == kState_RecordingOnly) || 
             (internalState == kState_WatchingPreRecorded &&
              nextState == kState_WatchingOtherRecording))
    {   
        channel->Open();
        channel->SetChannelByString(recordChannel);
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

    if (pauseBetween)
    {
        usleep(500000);
    }

    if (startPlayer)
    {
        SetupPlayer();
        pthread_create(&decode, NULL, SpawnDecode, nvp);

        while (!nvp->IsPlaying())
            usleep(50);

        frameRate = nvp->GetFrameRate();
    }

    if (closeRecorder)
        TeardownRecorder();

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
    nvr->Initialize();
}

void TV::TeardownRecorder(void)
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
    nvp->SetOSDFontName((char *)settings->GetSetting("OSDFont").c_str());
    nvp->SetAudioSampleRate(settings->GetNumSetting("AudioSampleRate"));
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
        delete prbuffer;
        prbuffer = rbuffer = NULL;
    }
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
            if ((float)nvr->GetFramesWritten() >
                (float)secsToRecord * frameRate)
            {
                nextState = RemoveRecording(internalState);
                changeState = true;
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

    usleep(500000);
    nvp->ResetPlaying();
    while (!nvp->ResetYet())
        usleep(5);

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
    if (!CheckChannel(atoi(name)))
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

    usleep(500000);
    nvp->ResetPlaying();
    while (!nvp->ResetYet())
        usleep(5);

    UpdateOSD();
    nvp->Unpause();
}

void TV::UpdateOSD(void)
{
    string title, subtitle, desc, category, starttime, endtime;
    GetChannelInfo(channel->GetCurrent(), title, subtitle, desc, category, 
                   starttime, endtime);

    nvp->SetInfoText(title, subtitle, desc, category, starttime, endtime,
                     osd_display_time);
    nvp->SetChannelText(channel->GetCurrentName(), osd_display_time);
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

    char query[1024];
    sprintf(query, "SELECT * FROM program WHERE channum = %d AND starttime < %s AND endtime > %s;", lchannel, curtimestr, curtimestr);

    MYSQL_RES *res_set;

    if (mysql_query(db_conn, query) == 0)
    {
        res_set = mysql_store_result(db_conn);
  
        MYSQL_ROW row;

        row = mysql_fetch_row(res_set);

        if (row != NULL)
        {
            for (unsigned int i = 0; i < mysql_num_fields(res_set); i++)
            {
                switch (i) 
                {
                    case 0: break;
                    case 1: starttime = row[i]; break;
                    case 2: endtime = row[i]; break;
                    case 3: if (row[i]) title = row[i]; break;
                    case 4: if (row[i]) subtitle = row[i]; break;
                    case 5: if (row[i]) desc = row[i]; break;
                    case 6: if (row[i]) category = row[i]; break;
                    default: break;
                }
            }
        }

        mysql_free_result(res_set);
    }
}

void TV::ConnectDB(void)
{
    db_conn = mysql_init(NULL);
    if (!db_conn)
    {
        printf("Couldn't init mysql connection\n");
    }
    else
    {
        if (mysql_real_connect(db_conn, "localhost", "mythtv", "mythtv",
                               "mythconverg", 0, NULL, 0) == NULL)
        {
            printf("Couldn't connect to mysql database, no channel info\n");
            mysql_close(db_conn);
            db_conn = NULL;
        }
    }
}

void TV::DisconnectDB(void)
{
    if (db_conn)
        mysql_close(db_conn);
}

bool TV::CheckChannel(int channum)
{
    if (!db_conn)
        return true;

    bool ret = false;
    char query[1024];
    sprintf(query, "SELECT * FROM channel WHERE channum = %d", channum);

    MYSQL_RES *res_set;

    if (mysql_query(db_conn, query) == 0)
    {
        res_set = mysql_store_result(db_conn);

        MYSQL_ROW row;

        row = mysql_fetch_row(res_set);

        if (row != NULL)
        {
            ret = true;
        }
            
        mysql_free_result(res_set);
    }

    return ret;
}

void TV::doLoadMenu(void)
{
    string epgname = settings->GetSetting("ProgramGuidePath");

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
