#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <errno.h>
#include "RTjpegN.h"
#include "XJ.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <termios.h>
#include <sys/time.h>
#include "minilzo.h"
#include "nuppelvideo.h"
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include "linearBlend.h"
#include "rtjpeg_plugin.h"

#define MAXPREBUFFERS 60

// -- lzo static vars ------------------------------------
#define IN_LEN          (128*1024L)
#define OUT_LEN         (IN_LEN + IN_LEN / 64 + 16 + 3)
// -- lzo static vars end --------------------------------

int compoff = 0;


static void hup(int foo)
{
 XJ_exit();
 exit(0);
}

int initsound()
{
  int audio;
// int fmt;
  int bits=16, stereo=1, speed=44100;

  audio = open("/dev/dsp", O_WRONLY);
  if (audio == -1) {
    fprintf(stderr,"%s\n", "can't open audio device!");
    return(0);
  }

  // linux audio seems to have allways 64k buffer
  
  if (ioctl(audio, SNDCTL_DSP_SAMPLESIZE, &bits) < 0) {
    fprintf(stderr,"%s\n", "can't open audio device(SNDCTL_DSP_SAMPLESIZE)!");
    return(0);
  }

  if (ioctl(audio, SNDCTL_DSP_STEREO, &stereo) < 0) {
    fprintf(stderr,"%s\n", "can't open audio device(SNDCTL_DSP_STEREO)!");
    return(0);
  }

  if (ioctl(audio, SNDCTL_DSP_SPEED, &speed) < 0) {
    fprintf(stderr,"%s\n", "can't open audio device(SNDCTL_DSP_SPEED)!");
    return(0);
  }

  return(audio);
}

void write_audio(int audio, unsigned char *aubuf, int size)
{
  unsigned char *tmpbuf;
  int written=0, lw=0;

  tmpbuf = aubuf;

  while ( (written < size) && ((lw = write(audio, tmpbuf, size - written)) > 0) )
  {
    if (lw == -1) {
      fprintf(stderr,"%s\n", "can't write to audio device!");
      exit(1);
    }
    written += lw;
    tmpbuf += lw;
  }

  // now sync audio
  // if (ioctl (audio, SNDCTL_DSP_SYNC, NULL) < 0) {
  //   fprintf(stderr,"%s\n", "can't sync audio device!");
  //   return;
  // }
}

int paused;
// Controls

void PauseVideo(void)
{
   if (paused)
   {
     paused = 0;
   }
   else
   {
     paused = 1;
   }
}

void FFVideo(void)
{
}

void RewVideo(void)
{
}


// ----------------------------------------------------------
// -- USAGE -------------------------------------------------

void usage()
{
   fprintf(stderr, "\nNuppelVideo Player v0.52         (c)Roman HOCHLEITNER\n");
   fprintf(stderr, "Usage: nuvplay [options] filename \n\n");
   fprintf(stderr, "options: -w n .......... wait n miliseconds (per frame)\n");
   fprintf(stderr, "         -a n [msec].... audio-video delay (experimental) [0]\n");
   fprintf(stderr, "         -F n [Hz]...... force eff-DSP frequency [calc.]\n");
   fprintf(stderr, "         -b n .......... videoframes delayed (sync) 0..60 [8]\n");
   fprintf(stderr, "         -f ............ play fast (every 7th fr. no audio)\n");
   fprintf(stderr, "         -d ............ don't deinterlace\n");
   fprintf(stderr, "         -s ............ do not skip frames\n");
   fprintf(stderr, "         -V ............ video only (i.e. no audio)\n");
   fprintf(stderr, "         -h ............ this help\n\n");
   fprintf(stderr, "Note: this 'player' is far from perfect and if video/audio is not\n");
   fprintf(stderr, "      in sync it's rather more a problem of the player and not of the\n"); 
   fprintf(stderr, "      recordtool, in this case try -b 1 to 30 or -a -200 .. +200\n");
   fprintf(stderr, "      to compensate the delay\n");
   fprintf(stderr, "\n");
   exit(-1);
}


#ifdef NUVPLAY
int main(int argc, char *argv[])
#else
int PlayVideo(int argc, char *argv[])
#endif
{
  int key = 0;
  int w=384, h=288;
  struct timeval now, last;
  struct timezone tzone;
  //struct rtframeheader *frameheader;
  // int r, out_len;
  int audf=0;
  unsigned char *buf, *buf3;
  // unsigned char *buf2, *orig;
  char *sbuf=NULL;
  char c;
  // unsigned long int tbls[128];
  // __s8 *strm;
  long tf=0;
  int usecs=0;
  // use count prebuffers to adjust audio/video sync
  int usepre=8;
  int playaudio=1;
  int playfast=0;
  int deinterlace=1;
  char *filename = "";
  unsigned char *audiodata;
  int timecode; 
 
  int prebuffered=0;
  int actpre=0;
  int fnum=0;
  int audiolen;
  int skipframes=0;
  //struct termios termios;

  unsigned char *prebuf[MAXPREBUFFERS];

#ifdef NUVPLAY  
  while ((c=getopt(argc,argv,"w:b:da:fsVZh")) != -1) {
    switch(c) {
      case 'w': playfast=atoi(optarg); if(playfast<=1) playfast=2;  playaudio=usepre=0; break;
      case 'b': usepre=atoi(optarg); playaudio=1; break;
      case 'a': rtjpeg_audiodelay=atoi(optarg); break;
      case 'f': playfast=1; playaudio=usepre=0; break;
      case 'd': deinterlace=0; break;
      case 's': skipframes=0; break;
      case 'V': playaudio=0; break;
      case 'h': usage();         break;
 
      default: usage();
    }
  }

  if (optind==argc) usage();
  else filename=argv[optind];
#else
 filename = argv[1];
#endif
 
 tzone.tz_minuteswest=-60;
 tzone.tz_dsttime=0;
 now.tv_sec=0; // reset

 // init compression lzo ------------------------------
 if (lzo_init() != LZO_E_OK)
 {
   fprintf(stderr,"%s\n", "lzo_init() failed !!!");
   exit(3);
 }

 signal(SIGINT, &hup);
 
 // now we open the file

 rtjpeg_open(filename);

 w = rtjpeg_video_width;
 h = rtjpeg_video_height;

 fprintf(stderr, "nuv file with w=%d and h=%d and fps=%f effdsp=%5.2f\n",
                  rtjpeg_video_width,
                  rtjpeg_video_height,
                  rtjpeg_video_frame_rate,
                  (double)rtjpeg_effdsp/100.0);

 if (rtjpeg_fileheader.audioblocks!=0 && playaudio!=0) {
   // write to sound card
   audf=initsound();
   if (audf==0) {
     fprintf(stderr, "Cannot write to audio device, no audio is played\n" );
     playaudio=0; // reset flag
   }
 } else {
   playaudio=0;
 }

 sbuf=XJ_init(w, h, "NuppelVideo Player (experimental)", "NuppelVideo");

 buf3=malloc(w*h+(w*h)/2);

 while(!rtjpeg_eof)
 {
  if (paused)
     goto endofloop;
  
  buf = rtjpeg_get_frame(&timecode, playaudio==0, &audiodata, &audiolen);
  if (rtjpeg_eof) continue;
  if (audiolen!=0) {
    write_audio(audf, audiodata, audiolen);
  }

  // now we have to use/fill the pic-cache for syncing
  // because the soundcard needs 64k before playing --> ca 13pics

  if (usepre!=0) {
    if (prebuffered<usepre) {
      prebuf[prebuffered]=malloc(w*h+(w*h)/2);
      memcpy(prebuf[prebuffered], buf, w*h+(w*h)/2);
      prebuffered++;
      fnum++;
      continue;
    } else {
      // place the neweset buffer in the oldest
      memcpy(buf3, prebuf[actpre], w*h+(w*h)/2);
      memcpy(prebuf[actpre], buf, w*h+(w*h)/2);
      actpre++;
      if (actpre>=usepre) actpre=0;
    }
  } else {
    // if we play no audio we don;t need picbuffers for sync
    memcpy(buf3, buf, w*h+(w*h)/2);
  }

  // calaculate the delay

  if (!playfast) {
    if (now.tv_sec==0) {
      gettimeofday(&now, &tzone);
      last=now;
      usecs= (1000000.0/rtjpeg_video_frame_rate)/2;
      if (usecs>0) usleep(usecs); 
    } else {
      gettimeofday(&now, &tzone);
      usecs = 1000000.0/rtjpeg_video_frame_rate - 
              ((now.tv_sec-last.tv_sec)*1000000 + now.tv_usec-last.tv_usec);
      last=now;
      //fprintf(stderr, "\nwaiting %d usecs    ", usecs);
      if (usecs > (1000000/rtjpeg_video_frame_rate)/2) usecs= (1000000/rtjpeg_video_frame_rate)/2;
    }
  }

  if (playfast!=1 || tf%7==0) {
    if (!skipframes || usecs > (-1000000/rtjpeg_video_frame_rate)) {

      if (deinterlace) 
         linearBlendYUV420(buf3, w, h); 

      // Need to flip the U + V parts for Xv (to be proper YV12)
      memcpy(sbuf, buf3, w*h);
      memcpy(sbuf+w*h, buf3 + w*h*5/4, w*h/4);
      memcpy(sbuf+w*h*5/4, buf3 + w*h, w*h/4);

      XJ_show(w, h); // display the frame
    } 
  } else {
    tf++;
    fnum++;
    printf("skipped!\n");
    continue;
  }

  if (!playfast) {
  //    if (usecs>0) { fprintf(stderr, "sleeping %d\n", usecs); usleep(usecs/2); }
  }
 
  if (playfast >1) {
    usleep(playfast*1000);
  }  
  tf++;
  fnum++;

endofloop:  
  key = XJ_CheckEvents();

  if (key > 0)
  {
    if (key == 'p' || key == 'P')
        PauseVideo();
    else if (key == wsRight)
        FFVideo();
    else if (key == wsLeft)
        RewVideo();
    else if (key == wsEscape)
	rtjpeg_eof = 1;
//    else if (key == wsUp)
//        ChannelUp();
//    else if (key == wsDown)
//        ChannelDown();
  }
 }
 
 XJ_exit();

 return 0;
}

