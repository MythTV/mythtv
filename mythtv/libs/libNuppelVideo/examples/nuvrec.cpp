// STOP: this code is a hack, don't read further
//
// the code, its structure and documentation is a mess and should be rewritten 
// from scratch, but i've no time for such a luxury
// but there will always be enough time for a bit of stupidity :-]
// 
// you should already have stopped reading, what are you doing?
// and still you are reading, why? 
// would you be so kind to stop reading immediately
//
// Success has many parents, Failure is an orphan ;-)
//
// this is open source, it's dangerous
// watch out! or the teeth of the open source monster could get ya
//
// :wq -- it's the best editor

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/soundcard.h>
#include <linux/videodev.h>
#include <linux/wait.h>
#include <errno.h>
#include "minilzo.h"
#include "RTjpegN.h"
#include "nuppelvideo.h"
#undef MMX
#include <lame/lame.h>

#ifdef NUVREC
int encoding;
#else
#include "tv.h"
#endif

// #define TESTINPUT 1
// #define TESTSPLIT 1
#define KEYFRAMEDIST 30

//#ifdef TESTSPLIT
//  #define MAXBYTES          20000000
//  #define MAXBYTESFORCE     21000000
//#else
//  #define MAXBYTES       2000000000
//  #define MAXBYTESFORCE  2100000000
//#endif

// we need the BTTV_FIELDNR, so we really know how many frames we lose
#define BTTV_FIELDNR            _IOR('v' , BASE_VIDIOCPRIVATE+2, unsigned int)

/* Globals */

int fd;     // output file haendle
int ostr=0;
__s8 *strm;
long fsize;
long long input_len=0;
long long ftot=0;
long stot=0;
long dropped=0;
long copied=0;
unsigned int lf=0, tf=0;
int M1,M2,Q;
int pid=0, pid2=0;
int inputchannel=1;
int compression=1;
int compressaudio=1;
int recordaudio=1;
unsigned int byteswritten;
unsigned long long audiobytes;
int effectivedsp;
int ntsc=1; // default to NTSC, this info is only for the video header
int quiet;
int rawmode=0;
int usebttv=1;
struct video_audio origaudio;

char *mp3buf;
int mp3buf_size;
lame_global_flags *gf;

RTjpeg_t *rtjc;

//#define DP(DSTRING) fprintf(stderr, "%s\n", DSTRING);
#define DP(DSTRING)

pid_t waitpid(pid_t pid, int *status, int options);


// lzo static vars -----------------/////////////////////////////////
#define IN_LEN          (1024*1024L)
#define OUT_LEN         (IN_LEN + IN_LEN / 64 + 16 + 3)

// static lzo_byte in  [ IN_LEN ];
static lzo_byte out [ OUT_LEN ];
#define HEAP_ALLOC(var,size) \
        long __LZO_MMODEL var [ ((size) + (sizeof(long) - 1)) / sizeof(long) ]
HEAP_ALLOC(wrkmem,LZO1X_1_MEM_COMPRESS);
// lzo static vars end -------------/////////////////////////////////

int shmid;
void *sharedbuffer;

#define ERROR(PARAM) { fprintf(stderr, "\n%s\n", PARAM); exit(1); }

// ----------------------------------------------------------------------

#include <signal.h>
#include <sys/ipc.h>
//#include <sys/sem.h>
#include <sys/shm.h>
// #include <sys/wait.h>

char *audiodevice = "/dev/dsp";

struct vidbuffertype *videobuffer;
struct audbuffertype *audiobuffer;

int act_video_encode=0;
int act_video_buffer=0;

int act_audio_encode=0;
int act_audio_buffer=0;

int video_buffer_count=0;   // should be a setting from command line  6.3MB 384x288x40
                            //                                       23.2MB 704x576x40
int audio_buffer_count=0;   // should be a setting from command line 

long video_buffer_size=0;
long audio_buffer_size=0; 

struct timeval now, stm;
struct timezone tzone;

// ----------------------------------------------------------------------

static void childdeathhandler(int i)
{
  int status;

  status = 0;

  fprintf(stderr, "\n%s\n", "child died - terminating");
  close(fd);
  kill(pid,  2);
  kill(pid2, 2);
  usleep(30);
  waitpid(pid, &status, WNOHANG);  // we don't want to have zombies
  if (recordaudio) waitpid(pid2, &status, WNOHANG);
  usleep(300000);

  kill(pid,  9);
  if (recordaudio) kill(pid2, 9);

  fprintf(stderr, "\n"); // preserve status line
  exit(0);
}

// ----------------------------------------------------------------------

static void fatherhandler(int i)
{
  int status;

  status = 0;

  // no more signals from our children
  signal(SIGCHLD, SIG_DFL); // ignore fails on linux?

  close(fd);
  kill(pid,  2);
  kill(pid2, 2);
  usleep(30);
  waitpid(pid, &status, WNOHANG);  // we don't want to have zombies
  if (recordaudio) waitpid(pid2, &status, WNOHANG);
  usleep(300000);

  kill(pid,  9);
  if (recordaudio) kill(pid2, 9);

  // reset audio settings
  if (ioctl(fd, VIDIOCSAUDIO, &origaudio)<0) perror("VIDIOCSAUDIO");
 
  if (!quiet) fprintf(stderr, "\n"); // preserve status line
  exit(i);
}

// ----------------------------------------------------------------------

static void sighandler(int i)
{
  if (ostr) {
    close(ostr);
  }
  if (!quiet) fprintf(stderr, "\n"); // preserve status line
  exit(0);
}

// ----------------------------------------------------------------------

void init_shm(int init_shm)
{
    int i;
    unsigned char *startaudiodesc;
    unsigned char *startvideo;
    unsigned char *startaudio;

    if (init_shm) {
      shmid = shmget(IPC_PRIVATE, video_buffer_size*video_buffer_count +
                                  audio_buffer_size*audio_buffer_count +
                                  video_buffer_count*sizeof(vidbuffertyp) +
                                  audio_buffer_count*sizeof(audbuffertyp),
  	             IPC_EXCL | IPC_CREAT | 0600);
      if (shmid == -1)
  	  ERROR("shmget");
    } 
    sharedbuffer = shmat(shmid, IPC_RMID, SHM_RND);
    if (sharedbuffer == (char*)-1)
    {
  	perror("shmat");
	if(shmctl(shmid, IPC_RMID, NULL))
		perror("shmctl");
	exit(-1);
    }
    if(shmctl(shmid, IPC_RMID, NULL))
	ERROR("shmctl");

    videobuffer    = (struct vidbuffertype *)sharedbuffer;
    startaudiodesc = (char *)(sharedbuffer + video_buffer_count*sizeof(vidbuffertyp));
    audiobuffer    = (struct audbuffertype *)startaudiodesc;

    // start addr of raw video data THESE ARE OFFSETS!!!
    startvideo = (unsigned char *)(video_buffer_count*sizeof(vidbuffertyp) + 
                                   audio_buffer_count*sizeof(audbuffertyp));

    // start addr of raw audio data THESE ARE OFFSETS!!!
    startaudio = startvideo + (unsigned int)(video_buffer_size*video_buffer_count);

    // the capture        process may set freeToEncode and clear freeToBuffer and nothing more
    // the encoder/writer process may set freeToBuffer and clear freeToEncode and nothing more
    // the resources therefore is only used by max 1 process in a useable state

    if (init_shm) {
      for (i=0; i<video_buffer_count; i++) {
        videobuffer[i].sample=0;              // not relevant as long freeToEncode==0
        videobuffer[i].freeToEncode=0;        // empty at first
        videobuffer[i].freeToBuffer=1;        // fillable == empty 2vars --> no locking!!
        videobuffer[i].buffer_offset = startvideo + (unsigned int)(i*video_buffer_size);
      }
      for (i=0; i<audio_buffer_count; i++) {
        audiobuffer[i].sample=0;              // not relevant as long freeToEncode==0
        audiobuffer[i].freeToEncode=0;        // empty at first
        audiobuffer[i].freeToBuffer=1;        // fillable == empty 2vars --> no locking!!
        audiobuffer[i].buffer_offset = startaudio + (unsigned int)(i*audio_buffer_size);
      }
    }
}

// ----------------------------------------------------------------------


void writeit(unsigned char *buf, int fnum, int timecode, int w, int h)
{
  int tmp, r=0, out_len;
  struct rtframeheader frameheader;
  int xaa, freecount=0, compressthis; 
  static int startnum=0;
  static int frameofgop=0;
  static int lasttimecode=0;
  int raw=0;
  int timeperframe=40; 
  uint8_t *planes[3];
  
  planes[0] = buf;
  planes[1] = planes[0] + w*h;
  planes[2] = planes[1] + (w*h)/4;
  compressthis = compression;

  if (lf==0) { // this will be triggered every new file
    lf=fnum-2;
    startnum=fnum;
  }

  // count free buffers -- FIXME this can be done with less CPU time!!
  for (xaa=0; xaa < video_buffer_count; xaa++) {
    if (videobuffer[xaa].freeToBuffer) freecount++;
  }

  if (freecount < ((2*video_buffer_count)/3)) compressthis = 0; // speed up the encode process

  if (freecount < 5 || rawmode) raw = 1; // speed up the encode process

  // see if it's time for a seeker header, sync information and a keyframe
  
  frameheader.keyframe  = frameofgop;             // no keyframe defaulted

  if (((fnum-startnum)>>1) % KEYFRAMEDIST == 0) {
    frameheader.keyframe=0;
    frameofgop=0;
    write(ostr, "RTjjjjjjjjjjjjjjjjjjjjjjjj", FRAMEHEADERSIZE);
    frameheader.frametype    = 'S';           // sync frame
    frameheader.comptype     = 'V';           // video sync information
    frameheader.filters      = 0;             // no filters applied
    frameheader.packetlength = 0;             // no data packet
    frameheader.timecode     = (fnum-startnum)>>1; // frame number of following frame
    // write video sync info
    write(ostr, &frameheader, FRAMEHEADERSIZE);
    frameheader.frametype    = 'S';           // sync frame
    frameheader.comptype     = 'A';           // video sync information
    frameheader.filters      = 0;             // no filters applied
    frameheader.packetlength = 0;             // no data packet
    frameheader.timecode     = effectivedsp;  // effective audio dsp frequency
    // write audio sync info
    write(ostr, &frameheader, FRAMEHEADERSIZE);
    byteswritten += 3*FRAMEHEADERSIZE;
  }

  if (!raw) {
    tmp=RTjpeg_compress(rtjc, strm, planes);
  } else {
    tmp = video_buffer_size;
  }

  // here is lzo compression afterwards
  if (compressthis) {
    if (raw) {
      // compress raw yuv420 probably not very fast nor effective
      //r = lzo1x_1_compress(buf,video_buffer_size,out,&out_len,wrkmem);
   printf("compressing raw!!\n\n");
    } else {
      printf("ori %d\n", tmp);
      printf("bz2 %d\n", out_len);
      r = lzo1x_1_compress(strm,tmp,out,&out_len,wrkmem);
      printf("lzo %d\n", out_len);
    }

    //if (r!=LZO_E_OK) {
    //  fprintf(stderr,"%s\n","lzo compression failed");
    //  exit(3);
    //}
  }

  // fprintf(stderr, "inputlen: %6d    outputlen: %6d  \n",tmp,out_len);

  // if (r == LZO_E_OK)
  //    printf("compressed %lu bytes into %lu bytes\n",
  //    (long) in_len, (long) out_len);

  frameheader.frametype = 'V'; // video frame
  frameheader.timecode  = timecode;
  frameheader.filters   = 0;             // no filters applied

  //if (compressthis && tmp < out_len) {
  //  fprintf(stderr, "\rinputlen: %6d    outputlen: %6d",tmp,out_len);
  //}

  // now we check if we lost frames and if we did we have
  // to adjust the timecode of the current real frame to
  // the past and later add timeperframe to each "copied" frame

  // dropped+= fnum-(lf+2); // should be += 0 ;-)
  // this was bullshit, this bug caused many videos not to sync if someone lost more
  // than one frame e.g 2 or 3 there were more frames copied than were really dropped :-(

  dropped = (((fnum-lf)>>1) - 1); // should be += 0 ;-)

  // examples: (10 - 6)/2 -1 = 2-1 = 1 frame to copy  (8)
  //           (10 - 8)/2 -1 = 1-1 = 0 => OK
  //           (10 - 4)/2 -1 = 3-1 = 2 frames to copy (6,8)
  //           (10 - 2)/2 -1 = 4-1 = 3 frames to copy (4,6,8)
  // odd ex.   ( 9 - 5)/2 -1 = 2-1 = 1 frame  to copy (7)
  //           ( 9 - 1)/2 -1 = 4-1 = 3 frames to copy (3,5,7)

#ifdef DEBUG_FTYPE
if (fnum==lf || fnum-lf==1)
fprintf(stderr,"\nWARNING lf=%d fnum=%d lasttimecode=%d timecode=%d\n",
                lf, fnum, lasttimecode, timecode);
#endif

  if (dropped>0) {
#ifdef DEBUG_FTYPE
fprintf(stderr,"\n lf=%d fnum=%d dropped=%ld lasttimecode=%d timecode=%d\n",
                lf, fnum, dropped, lasttimecode, timecode);
#endif
    timeperframe = (timecode - lasttimecode)/(dropped + 1); // the one we got => +1
    // this won't ever happen
    if (timeperframe < 0) { 
      if (ntsc) {
        timeperframe = 1000/30;
      } else {
        timeperframe = 1000/25;
      }
    }
    frameheader.timecode = lasttimecode + timeperframe;
    lasttimecode = frameheader.timecode;
  }

  // compr ends here
  if (compressthis==0 || (tmp<out_len)) {
    if (!raw) {
      frameheader.comptype  = '1'; // video compression: RTjpeg only
      frameheader.packetlength = tmp;
      write(ostr, &frameheader, FRAMEHEADERSIZE);
      write(ostr, strm, tmp);
      byteswritten += tmp + FRAMEHEADERSIZE;
      stot+=tmp;
      input_len += tmp;
    } else {
      frameheader.comptype  = '0'; // raw YUV420 for people with BIG FAST disks :]
      frameheader.packetlength = video_buffer_size;
      write(ostr, &frameheader, FRAMEHEADERSIZE);
      write(ostr, buf, video_buffer_size); // we write buf directly
      byteswritten += video_buffer_size + FRAMEHEADERSIZE;
      stot+=video_buffer_size;
      input_len += video_buffer_size;
    }
  } else {
    if (!raw) {
      frameheader.comptype  = '2'; // video compression: RTjpeg with lzo
    } else {
      frameheader.comptype  = '3'; // raw YUV420 with lzo (whoever wants that ;)
    }
    frameheader.packetlength = out_len;
    write(ostr, &frameheader, FRAMEHEADERSIZE);
    write(ostr, out, out_len);
    byteswritten += out_len + FRAMEHEADERSIZE;
    stot+=out_len;
    input_len += tmp;
  }

  frameofgop++;
 
  ftot+=(fsize+FRAMEHEADERSIZE); // actual framesize + sizeof header

  // if we have lost frames we insert "copied" frames until we have the
  // exact count because of that we should have no problems with audio 
  // sync, as long as we don't loose audio samples :-/

  while (dropped > 0) {
    frameheader.timecode = lasttimecode + timeperframe;
    lasttimecode = frameheader.timecode;
    frameheader.keyframe  = frameofgop;             // no keyframe defaulted
    frameheader.packetlength =  0;   // no additional data needed
    frameheader.frametype    = 'V';  // last frame (or nullframe if it is first)
    frameheader.comptype    = 'L';   
    write(ostr, &frameheader, FRAMEHEADERSIZE);
    byteswritten += FRAMEHEADERSIZE;
    // we don't calculate sizes for lost frames for compression computation
    dropped--;
    copied++;
    frameofgop++;
  }

  // now we reset the last frame number so that we can find out
  // how many frames we didn't get next time

  lf=fnum;
  lasttimecode = timecode;

  if (!quiet) {
    if ((fnum % 17) == 0) {
      if (compression==0) {
        fprintf(stderr, "copied=%ld f#=%u buf=%d ratio=%3.2f effdsp=%d  \r", 
                copied, (fnum-startnum)>>1, freecount, 
                (float)((double)ftot)/((double)stot), effectivedsp);
      } else {
        fprintf(stderr, 
                "\rcopied=%ld buf=%d f#=%u ratio=%3.2f lzor=%3.2f effdsp=%d  ", 
                copied, freecount, (fnum-startnum)>>1,  
                (float)((double)ftot)/((double)stot),
                (float)((double)input_len)/((double)stot), effectivedsp);
      }
    }
  }
}

// ---------------------------------------------------------------

void writeitaudio(unsigned char *buf, int fnum, int timecode)
{
  struct rtframeheader frameheader;
  static int last_block   = 0;
  static int audio_behind = 0;
  static int firsttc      = -1;
  double mt;
  double eff;
  double abytes;

  if (last_block != 0) {
    if (fnum != (last_block+1)) {
      audio_behind = fnum - (last_block+1);
    }
  }

  frameheader.frametype = 'A'; // audio frame

  if (compressaudio) {
     frameheader.comptype  = '3'; // audio is compressed
  } else {
      frameheader.comptype  = '0'; // audio is uncompressed
      frameheader.packetlength = audio_buffer_size;
  }
  frameheader.timecode = timecode;
      
  if (firsttc==-1) {
    firsttc = timecode;
    fprintf(stderr, "first timecode=%d\n", firsttc);
  } else {
    timecode -= firsttc; // this is to avoid the lack between the beginning
                         // of recording and the first timestamp, maybe we
                         // can calculate the audio-video +-lack at the 
			 // beginning too

    abytes = (double)audiobytes; // - (double)audio_buffer_size; 
                                 // wrong guess ;-)

    // need seconds instead of msec's
    //mt = (double)timecode/1000.0;
    mt = (double)timecode;
    if (mt > 0.0) {
      //eff = (abytes/4.0)/mt;
      //effectivedsp=(int)(100.0*eff);
      eff = (abytes/mt)*((double)100000.0/(double)4.0);
      effectivedsp=(int)eff;
      //fprintf(stderr,"timecode = %d  audiobutes=%d  effectivedsp = %d\n", 
      //                timecode, audiobytes, effectivedsp);
    }
  }

  if (compressaudio) {
    char mp3gapless[7200];
    int compressedsize = 0;
    int gaplesssize = 0;
    int lameret = 0;
    
    lameret = lame_encode_buffer_interleaved(gf, (short int *)buf, 
                                             audio_buffer_size / 4, mp3buf, 
                                             mp3buf_size);
    compressedsize = lameret;

    lameret = lame_encode_flush_nogap(gf, mp3gapless, 7200);
    gaplesssize = lameret;
    
    frameheader.packetlength = compressedsize + gaplesssize;

    write(ostr, &frameheader, FRAMEHEADERSIZE);
    write(ostr, mp3buf, compressedsize);
    write(ostr, mp3gapless, gaplesssize);

    byteswritten += compressedsize + gaplesssize + FRAMEHEADERSIZE;
    audiobytes += audio_buffer_size;
  } else {
    write(ostr, &frameheader, FRAMEHEADERSIZE);
    write(ostr, buf, audio_buffer_size);
    byteswritten += audio_buffer_size + FRAMEHEADERSIZE;
    audiobytes += audio_buffer_size; // only audio no header!!
  }
    
  // this will probably never happen and if there would be a 
  // 'uncountable' video frame drop -> material==worthless

  if (audio_behind > 0) {
    fprintf(stderr, "audio behind!!!!!\n\n\n");
    frameheader.frametype = 'A'; // audio frame
    frameheader.comptype  = 'N'; // output a nullframe with
    frameheader.packetlength = 0;
    write(ostr, &frameheader, FRAMEHEADERSIZE);
    byteswritten += FRAMEHEADERSIZE;
    audiobytes += audio_buffer_size;
    audio_behind --;
  }
  
  last_block = fnum;
}
 

// ---------------------------------------------------------------------
// -- AUDIO INIT (detects blocksize and sets audio_buffer_size)  -------

int audio_init() 
{
  int afmt, afd;
  int frag, channels, rate, blocksize;

  if (-1 == (afd = open(audiodevice, O_RDONLY))) {
    fprintf(stderr, "\n%s\n", "Cannot open DSP, exiting");
    return(1);
  }

  ioctl(afd, SNDCTL_DSP_RESET, 0);
   
  frag=(8<<16)|(10);//8 buffers, 1024 bytes each
  ioctl(afd, SNDCTL_DSP_SETFRAGMENT, &frag);

  afmt = AFMT_S16_LE;
  ioctl(afd, SNDCTL_DSP_SETFMT, &afmt);
  if (afmt != AFMT_S16_LE) {
    fprintf(stderr, "\n%s\n", "Can't get 16 bit DSP, exiting");
    return(1);
  }

  channels = 2;
  ioctl(afd, SNDCTL_DSP_CHANNELS, &channels);

  /* sample rate */
  rate = 44100;
  ioctl(afd, SNDCTL_DSP_SPEED,    &rate);

  if (-1 == ioctl(afd, SNDCTL_DSP_GETBLKSIZE,  &blocksize)) {
    fprintf(stderr, "\n%s\n", "Can't get DSP blocksize, exiting");
    return(1);
  }
  blocksize *= 4;
  fprintf(stderr, "\naudio blocksize = '%d'\n",blocksize);

  // close audio, audio_capture_process opens again later
  close(afd);

  audio_buffer_size = blocksize;
  
  return(0); // everything is ok
}

// ---------------------------------------------------------------------
// -- AUDIO CAPTURE PROCESS --------------------------------------------

void audio_capture_process() 
{
  int afmt,trigger;
  int afd, act, lastread;
  int frag, channels, rate, blocksize;
  unsigned char *buffer;
  long tcres;
 
  long long act_audio_sample=0;

  signal(SIGINT, sighandler); // install sighaendler

  if (-1 == (afd = open(audiodevice, O_RDONLY))) {
    fprintf(stderr, "\n%s\n", "Cannot open DSP, exiting");
    exit(1);
  }

  ioctl(afd, SNDCTL_DSP_RESET, 0);
   
  frag=(8<<16)|(10);//8 buffers, 1024 bytes each
  ioctl(afd, SNDCTL_DSP_SETFRAGMENT, &frag);

  afmt = AFMT_S16_LE;
  ioctl(afd, SNDCTL_DSP_SETFMT, &afmt);
  if (afmt != AFMT_S16_LE) {
    fprintf(stderr, "\n%s\n", "Can't get 16 bit DSP, exiting");
    exit;
  }

  channels = 2;
  ioctl(afd, SNDCTL_DSP_CHANNELS, &channels);

  /* sample rate */
  rate = 44100;
  ioctl(afd, SNDCTL_DSP_SPEED,    &rate);

  if (-1 == ioctl(afd, SNDCTL_DSP_GETBLKSIZE,  &blocksize)) {
    fprintf(stderr, "\n%s\n", "Can't get DSP blocksize, exiting");
    exit;
  }

  blocksize*=4;  // allways read 4*blocksize

  if (blocksize != audio_buffer_size) {
    fprintf(stderr, "\nwarning: audio blocksize = '%d' audio_buffer_size='%ld'\n",
                    blocksize, audio_buffer_size);
   //FIXME exit(1);
  } 


  buffer = (char *)malloc(audio_buffer_size);
  /* trigger record */
  trigger = ~PCM_ENABLE_INPUT;
  ioctl(afd,SNDCTL_DSP_SETTRIGGER,&trigger);

  trigger = PCM_ENABLE_INPUT;
  ioctl(afd,SNDCTL_DSP_SETTRIGGER,&trigger);

  // now we can record AUDIO

  while(1) {
    // get current time for timecode before the samples are 'delivered'
    // so we don't have to calculate the time for recording it, which
    // depends on the audio_buffer_size
    // FIXME? if we are late and the kernel 64k audio buffer is almost
    // full, the timestimp will be wrong

    gettimeofday(&now, &tzone);

    if (audio_buffer_size != (lastread = read(afd,buffer,audio_buffer_size))) {
      fprintf(stderr, "only read %d from %ld bytes from '%s'\n", lastread, audio_buffer_size,
                      audiodevice);
      perror("read /dev*audiodevice");
      //return buffer;
      // storage old buffer ?
    }
    // fprintf(stderr, "*");

    act = act_audio_buffer;

    if (! audiobuffer[act].freeToBuffer) {
      // we have to skip the current frame :-(
      fprintf(stderr, "ran out of free AUDIO buffers :-(\n");
      act_audio_sample ++;
      continue;
    }
DP("buffered audio frame");

    tcres = (now.tv_sec-stm.tv_sec)*1000 + now.tv_usec/1000 - stm.tv_usec/1000;
    audiobuffer[act].sample = act_audio_sample;
    audiobuffer[act].timecode = tcres;

    memcpy((unsigned char *)((unsigned int)sharedbuffer + 
                             (unsigned int)audiobuffer[act].buffer_offset), 
                             buffer, audio_buffer_size);
 
    audiobuffer[act].freeToBuffer = 0;
    act_audio_buffer++;
    if (act_audio_buffer >= audio_buffer_count) act_audio_buffer = 0; // cycle to begin of buffer
    audiobuffer[act].freeToEncode = 1; // last setting to prevent race conditions

    act_audio_sample ++; // we should get this from the kernel
                         // then we would know when we have lost audio FIXME TODO

    //usleep(10); // dont need to sleep (read is blocking)
    // exit(0); // this stupid process doesn't die!!!!!!!!!!!!!!!!!!!
  }
}

// ----------------------------------------------------------------------

int create_nuppelfile(char *fname, int number, int w, int h) 
{
  struct rtfileheader fileheader;
  struct rtframeheader frameheader;
  char realfname[255];
  static unsigned long int tbls[128];
  static const char finfo[12] = "NuppelVideo";
  static const char vers[5]   = "0.05";

  // fprintf(stderr, "sizeof(fileheader)=%d  sizeof(frameheader)=%d\n", 
  //                  sizeof(fileheader),sizeof(frameheader));

  if (number==0) { 
    int i;
    rtjc = RTjpeg_init();
    i = RTJ_YUV420;
    RTjpeg_set_format(rtjc, &i);
    RTjpeg_set_size(rtjc, &w, &h);
    RTjpeg_set_quality(rtjc, &Q);
    i = 15;
    RTjpeg_set_intra(rtjc, &i, &M1, &M2);
    snprintf(realfname, 250, "%s.nuv", fname);
  } else {
    snprintf(realfname, 250, "%s-%d.nuv", fname, number);
  }
  ostr=open(realfname,O_WRONLY|O_TRUNC|O_CREAT|O_LARGEFILE, 0644);

  if (ostr==-1) return(-1);

  memcpy(fileheader.finfo, finfo, sizeof(fileheader.finfo));
  memcpy(fileheader.version, vers, sizeof(fileheader.version));
  fileheader.width  = w;
  fileheader.height = h;
  fileheader.desiredwidth  = 0;
  fileheader.desiredheight = 0;
  fileheader.pimode = 'P';
  fileheader.aspect = 1.0;
  if (ntsc) fileheader.fps = 29.97;
      else  fileheader.fps = 25.0;
  fileheader.videoblocks = -1;
  if (recordaudio) {
    fileheader.audioblocks = -1;
  } else {
    fileheader.audioblocks = 0;
  }
  fileheader.textsblocks = 0;
  fileheader.keyframedist = KEYFRAMEDIST;
  // write the fileheader
  write(ostr, &fileheader, FILEHEADERSIZE);

  frameheader.frametype = 'D'; // compressor data
  frameheader.comptype  = 'R'; // compressor data for RTjpeg
  frameheader.packetlength = sizeof(tbls);


  // compression configuration header
  write(ostr, &frameheader, FRAMEHEADERSIZE);

  // compression configuration data
  write(ostr, tbls, sizeof(tbls));
  fsync(ostr);

  byteswritten = FILEHEADERSIZE + sizeof(tbls) + FRAMEHEADERSIZE;
  lf = 0; // that resets framenumber so that seeking in the
          // continues parts works too

  ftot = stot = input_len = 1; // reset the values for compression ratio calculations
                               
  return(0);
}

// ---------------------------------------------------------------------
// -- WRITER PROCESS ---------------------------------------------------


void write_process(char *fname, int w, int h) 
{
  int act;
  int videofirst;

  // init compression lzo ------------------------------
  if (lzo_init() != LZO_E_OK)
  {
    fprintf(stderr,"%s\n", "lzo_init() failed !!!");
    exit(3);
  }

  init_shm(0); // only attach to the existing shm
  strm=malloc(w*h+(w*h)/2+10);

  if (0 != create_nuppelfile(fname, 0, w, h)) {
    fprintf(stderr, "cannot open %s.nuv for writing\n", fname);
    exit(1);
  }

  signal(SIGINT, sighandler); // install sighaendler to flush and close ostr
   
  while(1) {
    act = act_video_encode;
    if (!videobuffer[act].freeToEncode && !audiobuffer[act_audio_encode].freeToEncode) {
      // we have no frames in our cycle buffer
//fprintf(stderr,"*");

      usleep(5); // wait a little for next frame and give time to other processes
      continue;   // check for next frame
    }

// unluckily i didn't find any information at the kernel interface level that 
// reports for lost audio :-( -- didn't happen right now, anyway

    if (videobuffer[act].freeToEncode) {
      if (audiobuffer[act_audio_encode].freeToEncode) {
        // check which first 
        videofirst = (videobuffer[act].timecode <= audiobuffer[act_audio_encode].timecode);
      } else {
        videofirst = 1;
      }
    } else {
      videofirst = 0;
    }

    if (videofirst) {
      if (videobuffer[act].freeToEncode) {
        DP("before write frame");
        // we have at least 1 frame --> encode and write it :-)
        writeit((unsigned char *)((unsigned int)sharedbuffer + 
                                  (unsigned int)videobuffer[act].buffer_offset), 
                                  videobuffer[act].sample,
                                  videobuffer[act].timecode, w, h);
        DP("after write frame");
        videobuffer[act].sample = 0;
        act_video_encode++;
        if (act_video_encode >= video_buffer_count) act_video_encode = 0; // cycle to begin of buffer
        videobuffer[act].freeToEncode = 0; 
        videobuffer[act].freeToBuffer = 1; // last setting to prevent race conditions
      }
    } else {

      // check audio afterward FIXME buffercount for audio/video buffers
      // to work against bufferoverflows!!
      // FIXME check for lost audiobuffers!!!!!!!!!!!

      if (audiobuffer[act_audio_encode].freeToEncode) {
        DP("before write audio frame");
        // we have at least 1 frame --> write it :-)
        writeitaudio((unsigned char *)((unsigned int)sharedbuffer + 
                                      (unsigned int)audiobuffer[act_audio_encode].buffer_offset), 
                                      audiobuffer[act_audio_encode].sample, 
                                      audiobuffer[act_audio_encode].timecode);
        DP("after write audio frame");
        audiobuffer[act_audio_encode].sample = 0;
        audiobuffer[act_audio_encode].freeToEncode = 0; 
        audiobuffer[act_audio_encode].freeToBuffer = 1; // last setting to prevent race conditions
        act_audio_encode++;
        if (act_audio_encode >= audio_buffer_count) act_audio_encode = 0; // cycle to begin of buffer
      }

    }
  }
}

// ----------------------------------------------------------
// -- BUFFER PROCESS ----------------------------------------

void bufferit(unsigned char *buf)
{
 int act;
 long tcres;
 //static int firsttc=0;
 static int oldtc=0;
 int fn;

 act = act_video_buffer; 

#ifdef TESTINPUT
  while (!videobuffer[act].freeToBuffer) usleep(100);
  //usleep(500000); // test dsp measuring
#else
 if (! videobuffer[act].freeToBuffer) {
   // we have to skip the current frame :-(
   // fprintf(stderr, "\rran out of free VIDEO framepages :-(");
   // the fprint only made things slower
   return;
 }
#endif

 // get current time for timecode
 gettimeofday(&now, &tzone);

 tcres = (now.tv_sec-stm.tv_sec)*1000 + now.tv_usec/1000 - stm.tv_usec/1000;

#ifdef TESTINPUT
  tf+=2; // when reading from files we won't lose frames ;)
#else
 if (usebttv) {
   // i hate it when interfaces changes and a non existent ioctl doesn't make an error
   // and doesn't return -1, returning 0 instead and making no error is really weird
   if (ioctl(fd, BTTV_FIELDNR, &tf)) {
     perror("BTTV_FIELDNR");
     usebttv = 0;
     fprintf(stderr, "\nbttv_fieldnr not supported by bttv-driver" 
                     "\nuse insmod/modprobe bttv card=YOURCARD field_nr=1 to activate f.n."
                     "\nfalling back to timecode routine to determine lost frames\n");
   }
   if (tf==0) {
     usebttv = 0;
     fprintf(stderr, "\nbttv_fieldnr not supported by bttv-driver" 
                     "\nuse insmod/modprobe bttv card=YOURCARD field_nr=1 to activate f.n."
                     "\nfalling back to timecode routine to determine lost frames\n");
   }
 }

 // here is the non preferable timecode - drop algorithm - fallback
 if (!usebttv) {

   if (tf==0) {
     tf = 2; 
   } else {
     fn = tcres - oldtc;
  
     // the difference should be less than 1,5*timeperframe or we have 
     // missed at least one frame, this code might be inaccurate!

     if (ntsc) {
       fn = fn/33;
     } else {
       fn = fn/40;
     }
     if (fn==0) fn=1;
     tf+= 2*fn; // two fields
   }
 }
#endif

 oldtc = tcres;

 if (! videobuffer[act].freeToBuffer) {
   return; // we can't buffer the current frame
 }

 videobuffer[act].sample = tf;
 videobuffer[act].timecode = tcres;

DP("buffered frame");

 memcpy((unsigned char *)((unsigned int)sharedbuffer + 
                          (unsigned int)videobuffer[act].buffer_offset), 
                          buf, video_buffer_size);
 
 videobuffer[act].freeToBuffer = 0;
 act_video_buffer++;
 if (act_video_buffer >= video_buffer_count) act_video_buffer = 0; // cycle to begin of buffer
 videobuffer[act].freeToEncode = 1; // last setting to prevent race conditions

 return;
}

// ----------------------------------------------------------
// -- TESTINPUT ---------------------------------------------

#ifdef TESTINPUT

void testinput()
{
  int cc;
  char *tmpbuffer; 
  int hsize, vsize, frame_rate_code;
  FILE *ifile;

// thats borrowed code from mpeg2enc by Andrew Stevens
// much smoother written than mine ;)

  const int PARAM_LINE_MAX=256;
  int n;
  char param_line[PARAM_LINE_MAX];

  //ifile = fopen("test.yuv", "rb");
  ifile = stdin;

  for(n=0;n<PARAM_LINE_MAX;n++) {
    if(!fread(param_line+n,1,1,ifile)) {
            fprintf(stderr,"Error reading header from input stream\n");
            exit(1);
    }
    if(param_line[n]=='\n') break;
  }
  if(n==PARAM_LINE_MAX) {
    fprintf(stderr,"Didn't get linefeed in first %d characters of input stream\n",
                    PARAM_LINE_MAX);
    exit(1);
  }
  param_line[n] = 0; /* Replace linefeed by end of string */

  if(strncmp(param_line,"YUV4MPEG",8)) {
    fprintf(stderr,"Input starts not with \"YUV4MPEG\"\n");
    fprintf(stderr,"This is not a valid input for me\n");
    exit(1);
  }

  if( sscanf(param_line+8,"%d %d %d",&hsize,&vsize,&frame_rate_code) != 3) {
    fprintf(stderr,"Could not get image dimenstion and frame rate from input stream\n");
    exit(1);
  }

// this is my code style again, bugs begin here ;)

  fprintf(stderr,"importing video for testing purposes only\n width=%d height=%d\n", hsize, vsize);

  if ( ((int)(hsize*vsize*1.5)) != video_buffer_size) {
    fprintf(stderr,"-W -H params do not match the imported video size -- aborting\n");
    fatherhandler(0); // stops the children and then exits itself
  }

  tmpbuffer = malloc(video_buffer_size);
  while (1) {
//for (cc=1; cc < video_buffer_count-2; cc++) {
    // FRAME backslash n
    if (1 != fread(tmpbuffer, 6, 1, ifile)) { 
      fprintf(stderr,"no FRAME\\n found\n");
     //break;
      fatherhandler(0);
    }
    if (1 != fread(tmpbuffer, video_buffer_size, 1, ifile)) {
      fprintf(stderr,"couldn't read frame, assuming end of file\n");
     //break;
      fatherhandler(0);
    }
    bufferit(tmpbuffer);
  }
//write_process("test.nuv",hsize,vsize); // never returns
  exit(0);
}

#endif

// ----------------------------------------------------------
// -- USAGE -------------------------------------------------

void RecordUsage()
{
   fprintf(stderr, "\nNuppelVideo Recording Tool v0.52    (c)Roman HOCHLEITNER\n");
   fprintf(stderr, "Usage: nuvrec [options] filename \n\n");
   fprintf(stderr, "options: -q n .......... Quality 3..255  (75%%JPEG=)[255]\n");
   fprintf(stderr, "         -l n .......... Luminance Threshold   0..20 [1]\n");
   fprintf(stderr, "         -c n .......... Chrominance Threshold 0..20 [1]\n");
   fprintf(stderr, "         -W n .......... Width       [352 PAL, 352 NTSC]\n");
   fprintf(stderr, "         -H n .......... Height      [288 PAL, 240 NTSC]\n");
   fprintf(stderr, "         -t min ........ Length (3.5 = 3m 30s) [forever]\n");
   fprintf(stderr, "         -S n .......... Source (0 can be Televison) [0]\n");
   fprintf(stderr, "         -f n.n ........ Tunerfrequency      [no change]\n");
   fprintf(stderr, "         -x n .......... Video buffers in mb  [l.8/b.14]\n");
   fprintf(stderr, "         -y n .......... Audio buffers in mb         [2]\n");
   fprintf(stderr, "         -V dev ........ Videodevice       [/dev/video0]\n");
   fprintf(stderr, "         -A dev ........ Audiodevice          [/dev/dsp]\n");
   fprintf(stderr, "         -a n .......... Volume (bttv-volume) [no chg.]\n");
   fprintf(stderr, "         -T ............ use time for drop calculation\n");
   fprintf(stderr, "         -p ............ PAL\n");
   fprintf(stderr, "         -n ............ NTSC [default]\n");
   fprintf(stderr, "         -s ............ SECAM\n");
   fprintf(stderr, "         -r ............ raw YUV I420 (with lzo)\n");
   fprintf(stderr, "         -Q ............ shut up\n");
   fprintf(stderr, "         -N ............ no (lzo) compression\n");
   fprintf(stderr, "         -z ............ video only (i.e. no audio)\n");
   fprintf(stderr, "         -D ............ no audio compression\n");
   fprintf(stderr, "         -h ............ this help\n");
   fprintf(stderr, "\n");
   exit(-1);
}

// ----------------------------------------------------------
// -- MAIN --------------------------------------------------

#ifdef NUVREC
int main(int argc, char** argv)
#else
int RecordVideo(int argc, char** argv)
#endif
{
  struct video_mmap mm;
  struct video_mbuf vm;
  struct video_channel vchan;
  struct video_audio va;
  struct video_tuner vt;

  char *videodevice = "/dev/video0";
  char c;
  int secam;
  int channel=0;
  double  frequency=0.0;
  long v4lfrequency=0;
  int volume = -1;
  int reclength= -1; // reclength in secs
  double drec= -1.0;

  int w = -1;    //  192x144 640x480 608x448 512x384 480x368 384x288 
  int h = -1;    //  352x256 352x240 320x240 720x576

  int videomegs = -1;
  int audiomegs = -1;

  unsigned char *buf;
  int frame;
  char *outfilename = "outfile";

  /////////////////////////////////////////////////////
  //  CHECKING AND INTERPRETING COMMAND LINE SWITCHES 
  /////////////////////////////////////////////////////

  quiet = 0;

  Q  = 255; // nice defaults
  M1 =   1;
  M2 =   1;
  compression = 1;
  recordaudio = 1;
  compressaudio = 1;
  ntsc  = 1;
  secam = 0;
  tzone.tz_minuteswest=-60; // whatever
  tzone.tz_dsttime=0;
  now.tv_sec=0; // reset
  now.tv_usec=0; // reset

  while ((c=getopt(argc,argv,"q:l:c:S:W:H:t:NTV:A:a:srf:pnb:x:y:zQD")) != -1) {
    switch(c) {
      case 'q': Q  = atoi(optarg);  break;
      case 'l': M1 = atoi(optarg); break;
      case 'c': M2 = atoi(optarg); break;
      case 'S': channel = atoi(optarg); break;
      case 'W': w = atoi(optarg);  break;
      case 'H': h = atoi(optarg);  break;
      case 't': drec = atof(optarg);  break;
      case 'x': videomegs = atoi(optarg);  break;
      case 'y': audiomegs = atoi(optarg);  break;
      case 'p': ntsc = 0;  break;
      case 'n': ntsc = 1;  break;
      case 's': ntsc = 0; secam=1;  break;
      case 'f': frequency = atof(optarg); break;
      case 'N': compression = 0;   break;
      case 'z': recordaudio = 0;   break;
      case 'T': usebttv = 0;   break;
      case 'A': audiodevice = optarg;   break;
      case 'V': videodevice = optarg;   break;
      case 'Q': quiet = 1;   break;
      case 'r': rawmode = 1;   break;
      case 'a': volume = atoi(optarg);   break;
      case 'D': compressaudio = 0;   break;
      case 'h': RecordUsage();  break;
  
      default: RecordUsage();
    }
  }
  
  if (optind==argc) RecordUsage();
  else outfilename=argv[optind];

  if (drec != -1.0) {
    reclength = (int)(drec*60);
  }

  if (ntsc && w==-1) {
    w = 352;
  }
  if (ntsc && h==-1) {
    h = 240;
  }

  if (!ntsc && w==-1) {
    w = 352;
  }
  if (!ntsc && h==-1) {
    h = 288;
  }

  if (compressaudio) {
    gf = lame_init();
    lame_set_out_samplerate(gf, 44100);
    lame_set_bWriteVbrTag(gf, 0);
    lame_set_quality(gf, 8);
    lame_set_compression_ratio(gf, 11);
    lame_init_params(gf);
  } else {
    gf = NULL; 
  }
  /////////////////////////////////////////////
  //  CALCULATE BUFFER SIZES
  /////////////////////////////////////////////

  video_buffer_size=w*h + ((w*h)>>1);
  fsize=video_buffer_size;

  if (videomegs == -1) {
    if (w>=480 || h>288) { 
      videomegs = 14; // 14 megs for big ones
    } else {
      videomegs = 8;  // normally we don't need more than that
    }
  }
  video_buffer_count = (videomegs*1000*1000)/video_buffer_size; 

  // we have to detect audio_buffer_size, too before initshm()
  // or we crash horribly later, if no audio is recorded we 
  // don't change the value of 16384

  if (recordaudio) {
    if (0!=audio_init()) { 
      fprintf(stderr,"error: could not detect audio blocksize, audio recording disabled\n");
      recordaudio = 0;
    }
  }
 
  if (audiomegs==-1) audiomegs=2;

  if (audio_buffer_size!=0) 
    audio_buffer_count = (audiomegs*1000*1000)/audio_buffer_size;
  else 
    audio_buffer_count = 0;

  mp3buf_size = 1.25 * 8192 + 7200;
  mp3buf = malloc(mp3buf_size * sizeof(char));
 
  fprintf(stderr, 
          "we are using %dx%ldB video (frame-) buffers and %dx%ldB audio blocks\n", 
                  video_buffer_count, video_buffer_size, 
                  audio_buffer_count, audio_buffer_size );

  /////////////////////////////////////////////
  //  NOW FIRST INIT OF SHARED MEMORY
  /////////////////////////////////////////////

  init_shm(1); // init the fields in shm


  /////////////////////////////////////////////
  //  NOW START WRITER/ENCODER PROCESS
  /////////////////////////////////////////////

  pid = fork();
  if (!pid) {
    // pid is our compression/writer process
    if (getuid()==0) nice(-6);
    init_shm(0); // only new process to shm
    write_process(outfilename,w,h); // never returns
    fprintf(stderr, "%s\n", "child returned from write_process!!!");
    _exit(1); // should never happen
  }

  ///////////////////////////
  // now save the start time
  gettimeofday(&stm, &tzone);
 
// #ifndef TESTINPUT
  /////////////////////////////////////////////
  //  NOW START AUDIO RECORDER
  /////////////////////////////////////////////

  if (recordaudio) {
    pid2 = fork();
    if (!pid2) {
      // pid2 is our audio capturing process
      if (getuid()==0) nice(-10); 
      init_shm(0); // only new process to shm
      audio_capture_process(); // never returns
      fprintf(stderr, "%s\n", "child returned from audio capturing process!!!");
      exit(1); // should never happen
    }
  }

// #endif

  /////////////////////////////////////////////
  //  NOW START VIDEO RECORDER
  /////////////////////////////////////////////

  // only root can do this
  if (getuid()==0) nice(-10);

  init_shm(0); // only attach to the existing shm

  signal(SIGINT, fatherhandler);      // install sighaendler to wait for children pid and pid2
  signal(SIGCHLD, childdeathhandler); // in case the audiorecorder kills itself again :-(
                                      //             finally fixed by faz (DI Franz Ackermann)

  if (reclength != -1) {
    signal(SIGALRM, fatherhandler);
    alarm(reclength);
  }

  fd = open(videodevice, O_RDWR|O_CREAT);
  if(fd<=0){
    perror("open");
    fatherhandler(-1);
  }
    
  if(ioctl(fd, VIDIOCGMBUF, &vm)<0)
  {
  	perror("VIDIOCMCAPTUREi0");
  	fatherhandler(-1);
  }
  if(vm.frames<2)
  {
  	fprintf(stderr, "stoopid prog want min 2 cap buffs!\n");
  	fatherhandler(-1);
  }

  // fprintf(stderr, "We have vm.frames=%d\n", vm.frames);
    
  buf = (unsigned char*)mmap(0, vm.size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
  if (buf<=0)
  {
   perror("mmap");
   fatherhandler(-1);
  }	


  vchan.channel = channel;
  if(ioctl(fd, VIDIOCGCHAN, &vchan)<0) perror("VIDIOCGCHAN");

  // choose the right input
  if(ioctl(fd, VIDIOCSCHAN, &vchan)<0) perror("VIDIOCSCHAN");
  
  // if channel has a audio then activate it
  if ((vchan.flags & VIDEO_VC_AUDIO)==VIDEO_VC_AUDIO) {
    // we assume only a channel with audio can have a tuner therefore
    // we only tune here if we are supposed to
    if (frequency != 0.0) {
      v4lfrequency  = ((unsigned long)frequency)*16; 
      v4lfrequency |= ((unsigned long)( (frequency-(v4lfrequency/16))*100 )*16)/100; // ??????
      if (ioctl(fd, VIDIOCSFREQ, &v4lfrequency)<0) perror("VIDIOCSFREQ");
      if (!quiet) fprintf(stderr, "tuner frequency set to '%5.4f' MHz.\n", frequency);
    }
    if (!quiet) fprintf(stderr, "%s\n", "unmuting tv-audio");
    // audio hack, to enable audio from tvcard, in case we use a tuner
    va.audio = 0; // use audio channel 0
    if (ioctl(fd, VIDIOCGAUDIO, &va)<0) perror("VIDIOCGAUDIO"); 
    origaudio = va;
    if (!quiet) fprintf(stderr, "audio volume was '%d'\n", va.volume);
    va.audio = 0;
    va.flags &= ~VIDEO_AUDIO_MUTE; // now this really has to work

    if ((volume==-1 && va.volume<32768) || volume!=-1) {
      if (volume==-1) {
        va.volume = 32768;            // no more silence 8-)
      } else {
        va.volume = volume;
      }
      if (!quiet) fprintf(stderr, "audio volume set to '%d'\n", va.volume);
    }
    if (ioctl(fd, VIDIOCSAUDIO, &va)<0) perror("VIDIOCSAUDIO");
  } else {
    if (!quiet) fprintf(stderr, "channel '%d' has no tuner (composite)\n", channel);
  }

  // setting video mode
  vt.tuner = 0;
  if(ioctl(fd, VIDIOCGTUNER, &vt)<0) perror("VIDIOCGTUNER");
  if (ntsc)         { vt.flags |= VIDEO_TUNER_NTSC;  vt.mode |= VIDEO_MODE_NTSC; }
    else if (secam) { vt.flags |= VIDEO_TUNER_SECAM; vt.mode |= VIDEO_MODE_SECAM; }
      else          { vt.flags |= VIDEO_TUNER_PAL;   vt.mode |= VIDEO_MODE_PAL; }
  vt.tuner = 0;
  if(ioctl(fd, VIDIOCSTUNER, &vt)<0) perror("VIDIOCSTUNER");

  // make sure we use the right input
  if(ioctl(fd, VIDIOCSCHAN, &vchan)<0) perror("VIDIOCSCHAN");

  mm.height = h;
  mm.width  = w;
  mm.format = VIDEO_PALETTE_YUV420P    ; /* YCrCb422 */

  mm.frame  = 0;
  if(ioctl(fd, VIDIOCMCAPTURE, &mm)<0) perror("VIDIOCMCAPTUREi0");
  mm.frame  = 1;
  if(ioctl(fd, VIDIOCMCAPTURE, &mm)<0) perror("VIDIOCMCAPTUREi1");

  encoding = 1;

  while(encoding) {
    frame=0;
    mm.frame  = 0;
    if(ioctl(fd, VIDIOCSYNC, &frame)<0) perror("VIDIOCSYNC0");
    else {
      if(ioctl(fd, VIDIOCMCAPTURE, &mm)<0) perror("VIDIOCMCAPTURE0");
      DP("Captured 0er");
      bufferit(buf+vm.offsets[0]);
    }
    frame=1;
    mm.frame  = 1;
    if(ioctl(fd, VIDIOCSYNC, &frame)<0) perror("VIDIOCSYNC1");
    else {
      if(ioctl(fd, VIDIOCMCAPTURE, &mm)<0) perror("VIDIOCMCAPTURE1");
      DP("Captured 1er");
      bufferit(buf+vm.offsets[1]);
    }
  }

  free(mp3buf);

  RTjpeg_close(rtjc);

  return 0;
}
