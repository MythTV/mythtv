#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "NuppelVideoRecorder.h"
#include "NuppelVideoPlayer.h"
#include "RingBuffer.h"

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

  RingBuffer *rbuffer = new RingBuffer("ringbuf.nuv", 1024 * 1024 * 10);
  
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

  usleep(800000);

  pthread_create(&decode, NULL, SpawnDecode, nvp);

  while (!nvp->IsPlaying())
      usleep(50);

  while (nvp->IsPlaying())
      usleep(50);
  
  nvr->StopRecording();
  
  pthread_join(encode, NULL);
  pthread_join(decode, NULL);

  delete nvr;
  delete nvp;
  return 0;
}
