#include <stdio.h>
#include <unistd.h>

#include "NuppelVideoRecorder.h"

// ----------------------------------------------------------
// -- USAGE -------------------------------------------------

void RecordUsage()
{
   fprintf(stderr, "\nNuppelVideo Recording Tool v0.52    (c)Roman HOCHLEITNER\n");
   fprintf(stderr, "Usage: nuvrec [options] filename \n\n");
   fprintf(stderr, "    -q n .......... Quality 3..255  (75%%JPEG=)[255]\n");
   fprintf(stderr, "    -l n .......... Luminance Threshold   0..20 [1]\n");
   fprintf(stderr, "    -c n .......... Chrominance Threshold 0..20 [1]\n");
   fprintf(stderr, "    -W n .......... Width       [352 PAL, 352 NTSC]\n");
   fprintf(stderr, "    -H n .......... Height      [288 PAL, 240 NTSC]\n");
   fprintf(stderr, "    -t min ........ Length (3.5 = 3m 30s) [forever]\n");
   fprintf(stderr, "    -V dev ........ Videodevice       [/dev/video0]\n");
   fprintf(stderr, "    -A dev ........ Audiodevice          [/dev/dsp]\n");
   fprintf(stderr, "    -a n .......... Volume (bttv-volume) [no chg.]\n");
   fprintf(stderr, "    -p ............ PAL\n");
   fprintf(stderr, "    -n ............ NTSC [default]\n");
   fprintf(stderr, "    -s ............ SECAM\n");
   fprintf(stderr, "    -Q ............ shut up\n");
   fprintf(stderr, "    -h ............ this help\n");
   fprintf(stderr, "\n");
   exit(-1);
}

// ----------------------------------------------------------
// -- MAIN --------------------------------------------------

int main(int argc, char** argv)
{
  char c;
  int secam;
  int ntsc = 1;
  int volume = -1;
  int reclength= -1; // reclength in secs
  double drec= -1.0;
  int Q = 255, M1 = 1, M2 = 1;
  
  int w = 352;    //  192x144 640x480 608x448 512x384 480x368 384x288 
  int h = 240;    //  352x256 352x240 320x240 720x576

  char *outfilename = "outfile";
  char *audiodevice = "/dev/dsp";
  char *videodevice = "/dev/video";

  /////////////////////////////////////////////////////
  //  CHECKING AND INTERPRETING COMMAND LINE SWITCHES 
  /////////////////////////////////////////////////////

  int quiet = 0;

  while ((c=getopt(argc,argv,"q:l:c:W:H:t:pnsA:V:Qa:h")) != -1) {
    switch(c) {
      case 'q': Q  = atoi(optarg);  break;
      case 'l': M1 = atoi(optarg); break;
      case 'c': M2 = atoi(optarg); break;
      case 'W': w = atoi(optarg);  break;
      case 'H': h = atoi(optarg);  break;
      case 't': drec = atof(optarg);  break;
      case 'p': ntsc = 0;  break;
      case 'n': ntsc = 1;  break;
      case 's': ntsc = 0; secam=1;  break;
      case 'A': audiodevice = optarg;   break;
      case 'V': videodevice = optarg;   break;
      case 'Q': quiet = 1;   break;
      case 'a': volume = atoi(optarg);   break;
      case 'h': RecordUsage();  break;
  
      default: RecordUsage();
    }
  }
  
  if (optind==argc) RecordUsage();
  else outfilename=argv[optind];

  if (drec != -1.0) {
    reclength = (int)(drec*60);
  }
  
  if (reclength != -1) {
    //signal(SIGALRM, fatherhandler);
    //alarm(reclength);
  }

  NuppelVideoRecorder *nvr = new NuppelVideoRecorder();
  nvr->SetMotionLevels(M1, M2);
  nvr->SetQuality(Q);
  nvr->SetResolution(w, h);

  nvr->SetFilename(outfilename);
  nvr->SetAudioDevice(audiodevice);
  nvr->SetVideoDevice(videodevice);

  nvr->Initialize();
  nvr->StartRecording();

  delete nvr;
  
  return 0;
}
