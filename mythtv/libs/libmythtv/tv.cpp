#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <qsqldatabase.h>

#include "tv.h"
#include "osd.h"

// cheat and put the keycodes here
#define wsUp            0x52 + 256
#define wsDown          0x54 + 256
#define wsLeft          0x51 + 256
#define wsRight         0x53 + 256
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

TV::TV(const QString &startchannel, int capturecardnum, int pipcardnum)
{
    settings = new Settings();

    settings->LoadSettingsFiles("settings.txt", theprefix);
    settings->LoadSettingsFiles("mysql.txt", theprefix);
    settings->LoadSettingsFiles("theme.txt", theprefix);

    db_conn = NULL;
    ConnectDB(capturecardnum);
 
    bool orderbychanid = false;
    QString chanorder = settings->GetSetting("ChannelOrdering");
    if (chanorder != "" && chanorder != "channum")
        orderbychanid = true;
 
    audiosamplerate = pipaudiosamplerate = -1;
 
    GetDevices(capturecardnum, videodev, audiodev, audiosamplerate);

    pipvideodev = "";
    pipaudiodev = "";

    if (pipcardnum > 0)
        GetDevices(pipcardnum, pipvideodev, pipaudiodev, pipaudiosamplerate);

    channel = new Channel(this, videodev);
    channel->Open();
    channel->SetFormat(settings->GetSetting("TVFormat"));
    channel->SetFreqTable(settings->GetSetting("FreqTable"));
    channel->SetChannelByString(startchannel);
    channel->SetChannelOrdering(orderbychanid, chanorder);
    channel->Close();  

    pipchannel = new Channel(this, pipvideodev);
    if (pipchannel->Open())
    {
        pipchannel->SetFormat(settings->GetSetting("TVFormat"));
        pipchannel->SetFreqTable(settings->GetSetting("FreqTable"));
        pipchannel->SetChannelByString(startchannel);
        pipchannel->SetChannelOrdering(orderbychanid, chanorder);
        pipchannel->Close();
    }

    fftime = settings->GetNumSetting("FastForwardAmount");
    if (fftime <= 0)
        fftime = 5;

    rewtime = settings->GetNumSetting("RewindAmount");
    if (rewtime <= 0)
        rewtime = 5;

    nvr = pipnvr = activenvr = NULL;
    nvp = pipnvp = activenvp = NULL;
    prbuffer = rbuffer = piprbuffer = activerbuffer = NULL;
    activechannel = channel;

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

QString TV::GetInstallPrefix(void)
{
    return QString(theprefix); 
}

TVState TV::LiveTV(void)
{
    if (internalState == kState_RecordingOnly)
    {
        QString cmdline = QString("mythdialog \"MythTV is already recording "
                                  "'%1' on ").arg(curRecording->title);

        if (settings->GetNumSetting("DisplayChanNum") == 0)
            cmdline += QString("%1 [%2].").arg(curRecording->channame)
                                          .arg(curRecording->chansign);
        else
            cmdline += QString("channel %1.").arg(curRecording->chanstr);

        cmdline += QString("  Do you want to:\" \"Stop recording and watch "
                           "TV\" \"Watch the in-progress recording\" "
                           "\"Cancel and go back to the menu\"");

        int result = system(cmdline.ascii());

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
        else
        {
            nextState = kState_RecordingOnly;
            changeState = false;
        }
    }
    else if (internalState == kState_None)
    {
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

int TV::AllowRecording(ProgramInfo *rcinfo, int timeuntil)
{
    if (internalState != kState_WatchingLiveTV)
    {
        return 1;
    }

    QString message = QString("MythTV wants to record \"") + rcinfo->title;
    if (settings->GetNumSetting("DisplayChanNum") == 0)
        message += QString("\" on ") + rcinfo->channame + " [" +
                   rcinfo->chansign + "]";
    else
        message += "\" on Channel " + rcinfo->chanstr;
    message += " in %d seconds.  Do you want to:";

    QString option1 = "Record and watch while it records";
    QString option2 = "Let it record and go back to the Main Menu";
    QString option3 = "Don't let it record, I want to watch TV";

    osd->SetDialogBox(message, option1, option2, option3, timeuntil);

    while (osd->DialogShowing())
        usleep(50);

    int result = osd->GetDialogSelection();

    tvtorecording = result;

    return result;
}

void TV::StartRecording(ProgramInfo *rcinfo)
{  
    QString recprefix = settings->GetSetting("RecordFilePrefix");

    if (internalState == kState_None || 
        internalState == kState_WatchingPreRecorded)
    {
        outputFilename = rcinfo->GetRecordFilename(recprefix);
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

            outputFilename = rcinfo->GetRecordFilename(recprefix);
            recordEndTime = rcinfo->endts;
            curRecording = rcinfo;

            nextState = kState_RecordingOnly;
            changeState = true;
        }
        else if (tvtorecording == 1)
        {
            outputFilename = rcinfo->GetRecordFilename(recprefix);
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
        QString recprefix = settings->GetSetting("RecordFilePrefix");
        inputFilename = rcinfo->GetRecordFilename(recprefix);
        playbackLen = rcinfo->CalculateLength();

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

void TV::StateToString(TVState state, QString &statestr)
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
    else if ((internalState == kState_None && 
              nextState == kState_RecordingOnly) || 
             (internalState == kState_WatchingPreRecorded &&
              nextState == kState_WatchingOtherRecording))
    {   
        channel->Open();
        channel->SetChannelByString(curRecording->chanstr);
        channel->Close();

        rbuffer = new RingBuffer(outputFilename, true);

        internalState = nextState;
        nextState = kState_None;
        changed = true;

        startRecorder = true;
    }   
    else if ((internalState == kState_RecordingOnly && 
              nextState == kState_None) ||
             (internalState == kState_WatchingRecording &&
              nextState == kState_WatchingPreRecorded) ||
             (internalState == kState_WatchingOtherRecording &&
              nextState == kState_WatchingPreRecorded))
    {
        closeRecorder = true;

        if (internalState == kState_WatchingRecording)
            nvp->SetWatchingRecording(false);

        internalState = nextState;
        changed = true;

        watchingLiveTV = false;
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

        nvr->Pause();
        while (!nvr->GetPause())
            usleep(5);

        rbuffer->Reset();

        channel->SetChannelByString(curRecording->chanstr);

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
    }
    else if ((internalState == kState_WatchingRecording &&
              nextState == kState_WatchingLiveTV) ||
             (internalState == kState_WatchingPreRecorded &&
              nextState == kState_WatchingLiveTV))
    {
        if (paused)
            osd->EndPause();
        paused = false;

        nvp->Pause();
        while (!nvp->GetPause())
            usleep(50);

        rbuffer->TransitionToRing();

        nvp->Unpause();
 
        WriteRecordedRecord();
 
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
    changeState = false;
 
    if (startRecorder)
    {
        SetupRecorder();
        pthread_create(&encode, NULL, SpawnEncode, nvr);

        while (!nvr->IsRecording())
            usleep(50);

        activenvr = nvr;
        activerbuffer = rbuffer;
        activechannel = channel;

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

        activenvp = nvp;
        frameRate = nvp->GetFrameRate();
	osd = nvp->GetOSD();
    }

    if (closeRecorder)
    {
        TeardownRecorder(killRecordingFile);
        if (pipnvr)
            TeardownPipRecorder();
    }

    if (closePlayer)
    {
        TeardownPlayer();
        if (pipnvp)
            TeardownPipPlayer();
    }
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
    nvr->SetVideoDevice(videodev);
    nvr->SetResolution(settings->GetNumSetting("Width"),
                       settings->GetNumSetting("Height"));
    nvr->SetTVFormat(settings->GetSetting("TVFormat"));
    nvr->SetCodec(settings->GetSetting("Codec"));
    
    nvr->SetRTJpegMotionLevels(settings->GetNumSetting("LumaFilter"),
                               settings->GetNumSetting("ChromaFilter"));
    nvr->SetRTJpegQuality(settings->GetNumSetting("Quality"));

    nvr->SetMP4TargetBitrate(settings->GetNumSetting("TargetBitrate"));
    nvr->SetMP4ScaleBitrate(settings->GetNumSetting("ScaleBitrate"));
    nvr->SetMP4Quality(settings->GetNumSetting("MaxQuality"),
                       settings->GetNumSetting("MinQuality"),
                       settings->GetNumSetting("QualDiff"));
    
    nvr->SetMP3Quality(settings->GetNumSetting("MP3Quality"));

    if (audiosamplerate > 0)
        nvr->SetAudioSampleRate(audiosamplerate);
    else
        nvr->SetAudioSampleRate(settings->GetNumSetting("AudioSampleRate"));

    nvr->SetAudioDevice(audiodev);
    nvr->SetAudioCompression(!settings->GetNumSetting("DontCompressAudio"));

    nvr->Initialize();
}

void TV::SetupPipRecorder(void)
{
    if (pipnvr)
    {
        printf("Attempting to setup a recorder, but it already exists\n");
        return;
    }

    pipnvr = new NuppelVideoRecorder();
    pipnvr->SetRingBuffer(piprbuffer);
    pipnvr->SetAsPIP();
    pipnvr->SetVideoDevice(pipvideodev);
    pipnvr->SetResolution(160, 128);
    pipnvr->SetTVFormat(settings->GetSetting("TVFormat"));

    pipnvr->SetCodec("rtjpeg");
    pipnvr->SetRTJpegMotionLevels(0, 0);
    pipnvr->SetRTJpegQuality(255);

    pipnvr->SetMP3Quality(9);

    if (pipaudiosamplerate > 0)
        pipnvr->SetAudioSampleRate(pipaudiosamplerate);
    else
        pipnvr->SetAudioSampleRate(settings->GetNumSetting("AudioSampleRate"));

    pipnvr->SetAudioDevice(pipaudiodev);
    pipnvr->SetAudioCompression(!settings->GetNumSetting("DontCompressAudio"));

    pipnvr->Initialize();
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
        unlink(outputFilename.ascii());
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

void TV::TeardownPipRecorder(void)
{
    if (pipnvr)
    {
        pipnvr->StopRecording();
        pthread_join(pipencode, NULL);
        delete pipnvr;
    }
    pipnvr = NULL;
}

void TV::SetupPlayer(void)
{
    if (nvp)
    { 
        printf("Attempting to setup a player, but it already exists.\n");
        return;
    }

    QString filters = "";
    nvp = new NuppelVideoPlayer();
    nvp->SetRingBuffer(prbuffer);
    nvp->SetRecorder(nvr);
    nvp->SetOSDFontName(settings->GetSetting("OSDFont"), theprefix);
    nvp->SetOSDThemeName(settings->GetSetting("OSDTheme"));
    nvp->SetAudioSampleRate(settings->GetNumSetting("AudioSampleRate"));
    nvp->SetAudioDevice(settings->GetSetting("AudioDevice"));
    nvp->SetLength(playbackLen);
    osd_display_time = settings->GetNumSetting("OSDDisplayTime");

    if (settings->GetNumSetting("Deinterlace"))
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
        printf("Attempting to setup a player, but it already exists.\n");
        return;
    }

    QString filters = "";

    pipnvp = new NuppelVideoPlayer();
    pipnvp->SetAsPIP();
    pipnvp->SetRingBuffer(piprbuffer);
    pipnvp->SetRecorder(pipnvr);
    pipnvp->SetOSDFontName(settings->GetSetting("OSDFont"), theprefix);
    pipnvp->SetOSDThemeName(settings->GetSetting("OSDTheme"));
    pipnvp->SetAudioSampleRate(settings->GetNumSetting("AudioSampleRate"));
    pipnvp->SetAudioDevice(settings->GetSetting("AudioDevice"));
    pipnvp->SetLength(playbackLen);

    if (settings->GetNumSetting("Deinterlace"))
    {
        if (filters.length() > 1)
            filters += ",";
        filters += "linearblend";
    }
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
    osd = NULL;
    
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

    delete piprbuffer;
    piprbuffer = NULL;
}

char *TV::GetScreenGrab(ProgramInfo *rcinfo, int secondsin, int &bufferlen,
                        int &video_width, int &video_height)
{
    QString recprefix = settings->GetSetting("RecordFilePrefix");
    QString filename = rcinfo->GetRecordFilename(recprefix);

    RingBuffer *tmprbuf = new RingBuffer(filename, false);

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
    paused = false;
    int keypressed;

    stickykeys = settings->GetNumSetting("StickyKeys");
    doing_ff = false;
    doing_rew = false;

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
                usleep(50000);
            }
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
                QString desc = "";
                int pos = calcSliderPos(0, desc);
                osd->UpdatePause(pos, desc);
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
    if (nvp->GetOSD() && osd->DialogShowing())
    {
        switch (keypressed)
        {
            case wsUp: osd->DialogUp(); break;
            case wsDown: osd->DialogDown(); break;
            case ' ': case wsEnter: case wsReturn: osd->TurnOff(); break;
            default: break;
        }
        
        return;
    }

    switch (keypressed) 
    {
        case 's': case 'S': case 'p': case 'P': 
        {
            doing_ff = false;
            doing_rew = false;
            DoPause();
            break;
        }
        case wsRight: case 'd': case 'D': 
        {
            if (doing_ff && stickykeys)
                doing_ff = false;
            else
            {
                doing_ff = true;
                doing_rew = false;
                DoFF(); 
            }
            break;
        }
        case wsLeft: case 'a': case 'A': 
        {
            if (doing_rew && stickykeys)
                doing_rew = false;
            else
            {
                doing_rew = true;
                doing_ff = false;
                DoRew(); 
            }
            break;
        }
        case wsEscape: exitPlayer = true; break;

//        case 'e': case 'E': nvp->ToggleEdit(); break;
//        case ' ': nvp->AdvanceOneFrame(); break;
        default: 
        {
            doing_ff = false;
            doing_rew = false;
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

            case wsEnter: ChannelCommit(); break;

            case 'M': case 'm': LoadMenu(); break;

            case 'V': case 'v': TogglePIPView(); break;
            case 'B': case 'b': ToggleActiveWindow(); break;
            case 'N': case 'n': SwapPIP(); break;

            default: break;
        }
    }
}

void TV::TogglePIPView(void)
{
    if (!pipnvp)
    {
        if (!pipchannel->Open())
            return;

        pipchannel->Close();

        long long filesize = settings->GetNumSetting("PIPBufferSize");
        filesize = filesize * 1024 * 1024 * 1024;
        long long smudge = settings->GetNumSetting("PIPMaxBufferFill");
        smudge = smudge * 1024 * 1024;

        piprbuffer = new RingBuffer(settings->GetSetting("PIPBufferName"),
                                    filesize, smudge);

        SetupPipRecorder();
        pthread_create(&pipencode, NULL, SpawnEncode, pipnvr);

        while (!pipnvr->IsRecording())
            usleep(50);
    
        pipchannel->SetFd(pipnvr->GetVideoFd());

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
	
        TeardownPipRecorder();
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
        activenvr = pipnvr;
        activerbuffer = piprbuffer;
        activechannel = pipchannel;
    }
    else
    {
        activenvp = nvp;
        activenvr = nvr;
        activerbuffer = rbuffer;
        activechannel = channel;
    }
}

void TV::SwapPIP(void)
{
    if (!pipnvp)
        return;

    QString pipchanname = pipchannel->GetCurrentName();
    QString bigchanname = channel->GetCurrentName();

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
        ret = (float)rbuffer->GetFreeSpace() / 
              ((float)rbuffer->GetFileSize() - rbuffer->GetSmudgeSize());
        ret *= 1000.0;

        long long written = nvr->GetFramesWritten();
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
            sprintf(text, "%02d:%02d:%02d behind  --  %.2f%% full", hours, mins,
                    secs, (1000 - ret) / 10);
        else
            sprintf(text, "%02d:%02d behind  --  %.2f%% full", mins, secs,
                    (1000 - ret) / 10);

        desc = text;
        return (int)(1000 - ret);
    }

    if (internalState == kState_WatchingRecording)
        playbackLen = (int)(((float)nvr->GetFramesWritten() / frameRate));

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

void TV::DoFF(void)
{
    if (paused)
        return;

    bool slidertype = false;
    if (internalState == kState_WatchingLiveTV)
        slidertype = true;

    if (activenvp == nvp)
    {
        QString desc = "";
        int pos = calcSliderPos(fftime, desc);
        osd->StartPause(pos, slidertype, "Forward", desc, 1);
    }

    activenvp->FastForward(fftime);
}

void TV::DoRew(void)
{
    if (paused)
        return;

    bool slidertype = false;
    if (internalState == kState_WatchingLiveTV)
        slidertype = true;

    if (activenvp == nvp)
    {
        QString desc = "";
        int pos = calcSliderPos(0 - rewtime, desc);
        osd->StartPause(pos, slidertype, "Rewind", desc, 1);
    }

    activenvp->Rewind(rewtime);
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

    activenvr->Pause();
    while (!activenvr->GetPause())
        usleep(5);

    activerbuffer->Reset();

    activechannel->ToggleInputs();

    activenvr->Reset();
    activenvr->Unpause();

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

    activenvr->Pause();
    while (!activenvr->GetPause())
        usleep(5);

    activerbuffer->Reset();

    if (up)
        activechannel->ChannelUp();
    else
        activechannel->ChannelDown();

    activenvr->Reset();
    activenvr->Unpause();

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
    int finetuning = 0;
    if (!CheckChannel(name, finetuning))
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

    activenvr->Pause();
    while (!activenvr->GetPause())
        usleep(5);

    activerbuffer->Reset();

    QString chan = name.stripWhiteSpace();
    QString prevchan = activechannel->GetCurrentName();

    if (!activechannel->SetChannelByString(chan))
        activechannel->SetChannelByString(prevchan);

    activenvr->Reset();
    activenvr->Unpause();

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
    QString callsign, iconpath;
    GetChannelInfo(title, subtitle, desc, category, starttime, endtime, 
                   callsign, iconpath);

    osd->SetInfoText(title, subtitle, desc, category, starttime, endtime,
                     callsign, iconpath, osd_display_time);
    osd->SetChannumText(channel->GetCurrentName(), osd_display_time);
}

void TV::UpdateOSDInput(void)
{
    QString dummy = "";
    osd->SetInfoText(channel->GetCurrentInput(), dummy, dummy, dummy, dummy, 
                     dummy, dummy, dummy, osd_display_time);
}

void TV::GetChannelInfo(QString &title, QString &subtitle, QString &desc, 
                        QString &category, QString &starttime, QString &endtime,
                        QString &callsign, QString &iconpath)
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
   
    QString channelname = channel->GetCurrentName();
    QString channelinput = channel->GetCurrentInput();
    QString device = channel->GetDevice();
 
    QString thequery = QString("SELECT starttime,endtime,title,subtitle,"
                               "description,category,callsign,icon "
                               "FROM program,channel,capturecard,cardinput "
                               "WHERE channel.channum = \"%1\" "
                               "AND starttime < %2 AND endtime > %3 AND "
                               "program.chanid = channel.chanid AND "
                               "channel.sourceid = cardinput.sourceid AND "
                               "cardinput.inputname = \"%4\" AND "
                               "cardinput.cardid = capturecard.cardid AND "
                               "capturecard.videodevice = \"%5\";")
                               .arg(channelname).arg(curtimestr).arg(curtimestr)
                               .arg(channelinput).arg(device);

    QSqlQuery query = db_conn->exec(thequery);

    QString test;
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        starttime = query.value(0).toString();
        endtime = query.value(1).toString();
        test = query.value(2).toString();
        if (test != QString::null)
            title = QString::fromUtf8(test);
        test = query.value(3).toString();
        if (test != QString::null)
            subtitle = QString::fromUtf8(test);
        test = query.value(4).toString();
        if (test != QString::null)
            desc = QString::fromUtf8(test);
        test = query.value(5).toString();
        if (test != QString::null)
            category = QString::fromUtf8(test);
        test = query.value(6).toString();
        if (test != QString::null)
            callsign = test;
        test = query.value(7).toString();
        if (test != QString::null)
            iconpath = test;
    }
}

void TV::ConnectDB(int cardnum)
{
    QString name = QString("TV%1%2").arg(cardnum).arg(rand());

    db_conn = QSqlDatabase::addDatabase("QMYSQL3", name);
    if (!db_conn)
    {
        printf("Couldn't initialize mysql connection\n");
        return;
    }
    db_conn->setDatabaseName(settings->GetSetting("DBName"));
    db_conn->setUserName(settings->GetSetting("DBUserName"));
    db_conn->setPassword(settings->GetSetting("DBPassword"));
    db_conn->setHostName(settings->GetSetting("DBHostName"));

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

void TV::GetDevices(int cardnum, QString &video, QString &audio, int &rate)
{
    video = "";
    audio = "";

    QString thequery = QString("SELECT videodevice,audiodevice,audioratelimit "
                               "FROM capturecard WHERE cardid = %1;")
                              .arg(cardnum);

    QSqlQuery query = db_conn->exec(thequery);

    int testnum = 0;

    QString test;
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        test = query.value(0).toString();
        if (test != QString::null)
            video = QString::fromUtf8(test);
        test = query.value(1).toString();
        if (test != QString::null)
            audio = QString::fromUtf8(test);
        testnum = query.value(2).toInt();

        if (testnum > 0)
            rate = testnum;
        else
            rate = -1;
    }
}

bool TV::CheckChannel(QString &channum, int &finetuning)
{
    finetuning = 0;

    if (!db_conn)
        return true;

    bool ret = false;

    QString channelinput = channel->GetCurrentInput();
    QString device = channel->GetDevice();

    QString thequery = QString("SELECT channel.finetune FROM "
                               "channel,capturecard,cardinput "
                               "WHERE channel.channum = \"%1\" AND "
                               "channel.sourceid = cardinput.sourceid AND "
                               "cardinput.inputname = \"%2\" AND "
                               "cardinput.cardid = capturecard.cardid AND "
                               "capturecard.videodevice = \"%3\";")
                               .arg(channum).arg(channelinput).arg(device);

    QSqlQuery query = db_conn->exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        finetuning = query.value(0).toInt();
        return true;
    }

    thequery = "SELECT NULL FROM channel;";
    query = db_conn->exec(thequery);

    if (query.numRowsAffected() == 0)
        ret = true;

    return ret;
}

QString TV::GetNextChannel(bool direction)
{
    QString ret = "";

    QString channum = channel->GetCurrentName();
    QString channelinput = channel->GetCurrentInput();
    QString device = channel->GetDevice();

    QString channelorder = channel->GetOrdering();

    QString thequery = QString("SELECT %1 FROM "
                               "channel,capturecard,cardinput "
                               "WHERE channel.channum = \"%1\" AND "
                               "channel.sourceid = cardinput.sourceid AND "
                               "cardinput.inputname = \"%2\" AND "
                               "cardinput.cardid = capturecard.cardid AND "
                               "capturecard.videodevice = \"%3\";")
                               .arg(channelorder).arg(channum)
                               .arg(channelinput).arg(device);

    QSqlQuery query = db_conn->exec(thequery);

    QString id = QString::null;

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        id = query.value(0).toString();
    }

    if (id == QString::null)
        return ret;

    QString comp = ">";
    QString ordering = "";

    if (direction == false)
    {
        comp = "<";
        ordering = " DESC ";
    }

    thequery = QString("SELECT channel.channum FROM channel,capturecard,"
                       "cardinput WHERE channel.%1 %2 \"%3\" AND "
                       "channel.sourceid = cardinput.sourceid AND "
                       "cardinput.inputname = \"%4\" AND "
                       "cardinput.cardid = capturecard.cardid AND "
                       "capturecard.videodevice = \"%5\" ORDER BY %6 %7 "
                       "LIMIT 1;")
                       .arg(channelorder).arg(comp).arg(id)
                       .arg(channelinput).arg(device)
                       .arg(channelorder).arg(ordering);

    query = db_conn->exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        ret = query.value(0).toString();
    }
    else
    {
        if (direction)        
            comp = "<";
        else
            comp = ">";

        thequery = QString("SELECT channel.channum FROM channel,capturecard,"
                           "cardinput WHERE channel.%1 %2 \"%3\" AND "
                           "channel.sourceid = cardinput.sourceid AND "
                           "cardinput.inputname = \"%4\" AND "
                           "cardinput.cardid = capturecard.cardid AND "
                           "capturecard.videodevice = \"%5\" ORDER BY %6 %7 "
                           "LIMIT 1;")
                           .arg(channelorder).arg(comp).arg(id)
                           .arg(channelinput).arg(device)
                           .arg(channelorder).arg(ordering);

        query = db_conn->exec(thequery);
 
        if (query.isActive() && query.numRowsAffected() > 0)
        { 
            query.next();

            ret = query.value(0).toString();
        }
    }

    return ret;
}

bool TV::ChangeExternalChannel(QString &channum)
{
    QString command = settings->GetSetting("ExternalChannelCommand");

    command += QString(" ") + QString(channum);

    int ret = system(command.ascii());

    if (ret > 0)
        return true;
    return true;
}

void TV::doLoadMenu(void)
{
    QString epgname = "mythepg";

    char runname[512];

    sprintf(runname, "%s %s", epgname.ascii(), 
            channel->GetCurrentName().ascii());
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
