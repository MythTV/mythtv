#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "NuppelVideoRecorder.h"
#include "NuppelVideoPlayer.h"
#include "RingBuffer.h"
#include "XJ.h"

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

int main(int argc, char *argv[])
{
  pthread_t encode, decode;
 
  long long filesize = (long long)(5) * 1024 * 1024 * 1024;
  long long smudge = (long long)(50) * 1024 * 1024; 

  RingBuffer *rbuffer = new RingBuffer("/mnt/store/ringbuf.nuv", 
		                       filesize, smudge);
  
  NuppelVideoRecorder *nvr = new NuppelVideoRecorder();
  nvr->SetRingBuffer(rbuffer);
  nvr->SetMotionLevels(0, 0);
  nvr->SetQuality(120);
  nvr->SetResolution(640, 480);
  nvr->Initialize();
  
  NuppelVideoPlayer *nvp = new NuppelVideoPlayer();
  nvp->SetRingBuffer(rbuffer);
  
  pthread_create(&encode, NULL, SpawnEncode, nvr);

  while (!nvr->IsRecording())
      usleep(50);

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
	       case 'p':
               case 'P': paused = nvp->TogglePause(); break;
               case wsRight: nvp->FastForward(5); break;
               case wsLeft: nvp->Rewind(5); break;
               case wsEscape: nvp->StopPlaying(); break;
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
          fprintf(stderr, "\r Playing: %f seconds behind realtime", (float)(nvr->GetFramesWritten() - nvp->GetFramesPlayed()) / frameRate);
      }
  }
      
  nvr->StopRecording();
  
  pthread_join(encode, NULL);
  pthread_join(decode, NULL);

  delete nvr;
  delete nvp;
  return 0;
}
