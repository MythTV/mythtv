#include <unistd.h>

#include "NuppelVideoPlayer.h"

// ----------------------------------------------------------
// -- USAGE -------------------------------------------------

void usage()
{
   fprintf(stderr, "\nNuppelVideo Player v0.52         (c)Roman HOCHLEITNER\n");
   fprintf(stderr, "Usage: nuvplay [options] filename \n\n");
   fprintf(stderr, "options: -w n .......... wait n miliseconds (per frame)\n");
   fprintf(stderr, "         -a n [msec].... audio-video delay (experimental) [0]\n");
   fprintf(stderr, "         -b n .......... videoframes delayed (sync) 0..60 [8]\n");
   fprintf(stderr, "         -d ............ don't deinterlace\n");
   fprintf(stderr, "         -V ............ video only (i.e. no audio)\n");
   fprintf(stderr, "         -h ............ this help\n\n");
   fprintf(stderr, "\n");
   exit(-1);
}


int main(int argc, char *argv[])
{
  char c;
  int usepre=8;
  int playaudio=1;
  int playfast=0;
  int deinterlace=1;
  char *filename = "";
  int audiodelay = 0;

  while ((c=getopt(argc,argv,"w:b:da:VZh")) != -1) {
    switch(c) {
      case 'w': playfast=atoi(optarg); if(playfast<=1) playfast=2;  playaudio=usepre=0; break;
      case 'b': usepre=atoi(optarg); playaudio=1; break;
      case 'a': audiodelay=atoi(optarg); break;
      case 'd': deinterlace=0; break;
      case 'V': playaudio=0; break;
      case 'h': usage();         break;
 
      default: usage();
    }
  }

  if (optind==argc) usage();
  else filename=argv[optind];

  NuppelVideoPlayer *nvp = new NuppelVideoPlayer();
  nvp->SetFileName(filename);
  nvp->StartPlaying(); 
 
  delete nvp;

  return 0;
}

