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

#define PAUSEBUFFER (1024 * 1024 * 50)
long long CalcMaxPausePosition(RingBuffer *rbuffer)
{
    long long readpos = rbuffer->GetTotalReadPosition();
    long long maxwrite = readpos - PAUSEBUFFER + rbuffer->GetFileSize();

    printf("calced: %lld %lld\n", readpos, maxwrite);
    return maxwrite;
}

int main(int argc, char *argv[])
{
  pthread_t encode, decode;
  
  RingBuffer *rbuffer = new RingBuffer("/mnt/store/ringbuf.nuv", 
		                       1024 * 1024 * 1024 * 5);
  
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
  long long readpos, writepos, maxwritepos = 0;

  cout << endl;

  while (nvp->IsPlaying())
  {
      usleep(50);
      if ((keypressed = XJ_CheckEvents()))
      {
           switch (keypressed) {
	       case 'p':
               case 'P': { 
                             paused = nvp->TogglePause(); 
			     if (paused)
                                 maxwritepos = CalcMaxPausePosition(rbuffer);
                         } break;
               case wsRight: nvp->FastForward(5); break;
               case wsLeft: nvp->Rewind(5); break;
               case wsEscape: nvp->StopPlaying(); break;
               default: break;
           }
      }
      if (paused)
      {
          writepos = rbuffer->GetTotalWritePosition();
          if (writepos > maxwritepos)
          {
              fprintf(stderr, "\r%f left -- Forced unpause.", (float)(maxwritepos - writepos) / 1024 / 1024);
              paused = nvp->TogglePause();
          }
          else 
              fprintf(stderr, "\rPaused: %f MB (%f%%) left", (float)(maxwritepos - writepos) / 1024 / 1024, (float)(maxwritepos - writepos) / (float)(rbuffer->GetFileSize() - PAUSEBUFFER) * 100);
      }
      else
      {
          readpos = rbuffer->GetTotalReadPosition();
          writepos = rbuffer->GetTotalWritePosition();

          fprintf(stderr, "\rPlaying: %f MB behind realtime", (float)(writepos - readpos) / 1024 / 1024);
      }
  }
      
  nvr->StopRecording();
  
  pthread_join(encode, NULL);
  pthread_join(decode, NULL);

  delete nvr;
  delete nvp;
  return 0;
}
