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

TV::TV(void)
{
    settings = new Settings("settings.txt");

    channel = new Channel(settings->GetSetting("V4LDevice"));

    channel->Open();

    channel->SetFormat(settings->GetSetting("TVFormat"));
    channel->SetFreqTable(settings->GetSetting("FreqTable"));

    channel->SetChannelByString("3");

    channel->Close();  

    nvr = NULL;
    nvp = NULL;
    rbuffer = NULL;
}

TV::~TV(void)
{
    if (channel)
        delete channel;
    if (settings)
        delete settings;
    if (rbuffer)
        delete rbuffer;
    if (nvp)
        delete nvp;
    if (nvr)
        delete nvr;
}

void TV::LiveTV(void)
{
    pthread_t encode, decode; 
  
    long long filesize = settings->GetNumSetting("BufferSize");
    filesize = filesize * 1024 * 1024 * 1024;
    long long smudge = settings->GetNumSetting("MaxBufferFill");
    smudge = smudge * 1024 * 1024; 

    osd_display_time = settings->GetNumSetting("OSDDisplayTime");
  
    rbuffer = new RingBuffer(settings->GetSetting("BufferName"), filesize, 
                             smudge);
  
    nvr = new NuppelVideoRecorder();
    nvr->SetRingBuffer(rbuffer);
    nvr->SetMotionLevels(settings->GetNumSetting("LumaFilter"), 
                         settings->GetNumSetting("ChromaFilter"));
    nvr->SetQuality(settings->GetNumSetting("Quality"));
    nvr->SetResolution(settings->GetNumSetting("Width"),
                     settings->GetNumSetting("Height"));
    nvr->SetMP3Quality(settings->GetNumSetting("MP3Quality"));
    nvr->Initialize();
 
    nvp = new NuppelVideoPlayer();
    nvp->SetRingBuffer(rbuffer);
    nvp->SetRecorder(nvr);
    nvp->SetDeinterlace((bool)settings->GetNumSetting("Deinterlace"));
    nvp->SetOSDFontName((char *)settings->GetSetting("OSDFont").c_str());
    
    pthread_create(&encode, NULL, SpawnEncode, nvr);

    while (!nvr->IsRecording())
        usleep(50);

    // evil.
    channel->SetFd(nvr->GetVideoFd());

    sleep(1);
    pthread_create(&decode, NULL, SpawnDecode, nvp);

    while (!nvp->IsPlaying())
        usleep(50);

    int keypressed;
    bool paused = false;
    float frameRate = nvp->GetFrameRate();
  
    cout << endl;

    while (nvp->IsPlaying())
    {
        usleep(50);
        if ((keypressed = XJ_CheckEvents()))
        {
           switch (keypressed) {
	       case 'p': case 'P': paused = nvp->TogglePause(); break;
               case wsRight: nvp->FastForward(5); break;
               case wsLeft: nvp->Rewind(5); break;
               case wsEscape: nvp->StopPlaying(); break;
               case wsUp: ChangeChannel(true); break; 
               case wsDown: ChangeChannel(false); break;
               default: break;
           }
        }
        if (paused)
        {
            fprintf(stderr, "\r Paused: %f seconds behind realtime (%f%% buffer left)", (float)(nvr->GetFramesWritten() - nvp->GetFramesPlayed()) / frameRate, (float)rbuffer->GetFreeSpace() / (float)rbuffer->GetFileSize() * 100.0);
        }
        else
        {
            fprintf(stderr, "\r                                                                      ");
            fprintf(stderr, "\r Playing: %f seconds behind realtime (%lld skipped frames)", (float)(nvr->GetFramesWritten() - nvp->GetFramesPlayed()) / frameRate, nvp->GetFramesSkipped());
        }
    }

    printf("exited for some reason\n");
  
    nvr->StopRecording();
  
    pthread_join(encode, NULL);
    pthread_join(decode, NULL);

    delete nvr;
    delete nvp;
    delete rbuffer;

    nvr = NULL;
    nvp = NULL;
    rbuffer = NULL;
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

    nvp->SetInfoText("Channel Info Placeholder", osd_display_time);
    nvp->SetChannelText(channel->GetCurrentName(), osd_display_time);

    nvp->Unpause();
}
