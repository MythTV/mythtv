/********************************************
 * the nuppelvideo reader lib for           *
 *     exportvideo, nuv2divx                *
 *     nuv2yuv and nuvplay                  *
 *                                          *
 * by  Roman Hochleitner                    *
 *     roman@mars.tuwien.ac.at              *
 *                                          *
 ********************************************
 * USE AT YOUR OWN RISK         NO WARRANTY *
 * (might crash your _________!)            *
 ********************************************
 *                                          *
 * parts borrowed from Justin Schoeman      *
 *                     and others           *
 *                                          *
 * This Software is under GPL version 2     *
 *                                          *
 * http://www.gnu.org/copyleft/gpl.html     *
 * ---------------------------------------- *
 * Fri May 30 01:17:49 CEST 2001            *
 ********************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include "nuppelvideo.h"
#undef MMX
#include "lame/lame.h"

#define RTJPEG_INTERNAL 1
 #include "rtjpeg_plugin.h"
#undef RTJPEG_INTERNAL

#ifdef __cplusplus
extern "C" {
#endif
#include "RTjpegN.h"
#ifdef __cplusplus
}
#endif
#include "minilzo.h"

RTjpeg_t *rtjd;
uint8_t *planes[3];

/* ------------------------------------------------- */

int rtjpeg_open(char *tplorg)
{
  unsigned long int tbls[128];
  struct rtframeheader frameheader;
  struct stat fstatistics;
  int    filesize;
  int    startpos;
  //int    seekdist;
  int    foundit=0;
  char   ftype;
  char   *space;
  int i;
  
  rtjpeg_file=open(tplorg, O_RDONLY|O_LARGEFILE);
  
  if (rtjpeg_file == -1) {
    fprintf(stderr, "File not found: %s\n", tplorg);
    exit(1);
  } 

  gf = lame_init();
  lame_set_decode_only(gf, 1);
  lame_decode_init();
  lame_init_params(gf);
  
  fstat(rtjpeg_file, &fstatistics);
  filesize = rtjpeg_filesize = fstatistics.st_size;
  read(rtjpeg_file, &rtjpeg_fileheader, FILEHEADERSIZE);
  
  rtjpeg_video_width      = rtjpeg_fileheader.width;
  rtjpeg_video_height     = rtjpeg_fileheader.height;
  rtjpeg_video_frame_rate = rtjpeg_fileheader.fps;
  rtjpeg_keyframedist     = rtjpeg_fileheader.keyframedist;
  rtjpeg_eof=0;

  // make sure we have enough even for a raw YUV frame
  space = (char *)malloc((int)(rtjpeg_video_width*rtjpeg_video_height*1.5)); 

  /* first frame has to be the Compression "D"ata frame */
  if (FRAMEHEADERSIZE != read(rtjpeg_file, &frameheader, FRAMEHEADERSIZE)) {
    fprintf(stderr, "Cant read Compression (D)ata frame header\n");
    exit(1);
  }
  if (frameheader.frametype != 'D') {
    fprintf(stderr, "\nIllegal File Format, no Compression (D)ata frame\n" );
    exit(1);
  }
  if (frameheader.packetlength != read(rtjpeg_file, tbls, frameheader.packetlength)) {
    fprintf(stderr, "Cant read Compression (D)ata packet, length=%d\n",
            frameheader.packetlength);
    exit(1);
  }

  rtjd = RTjpeg_init();
  i = RTJ_YUV420;
  RTjpeg_set_format(rtjd, &i);

  if ((rtjpeg_video_height & 1) == 1) {
    // this won't ever happen, since RTjpeg can only handle n*8 for w and h
    rtjpeg_video_height--; 
    fprintf(stderr, "\nIncompatible video height, reducing height to %d\n", rtjpeg_video_height);
  }

  /* init compression lzo ------------------------------ */
  if (lzo_init() != LZO_E_OK) {
    fprintf(stderr,"%s\n", "lzo_init() failed !!!");
    exit(3);
  }

  // how much video frames has the file?
  // what is the last resulting dsp frequency?

  rtjpeg_startpos = startpos = lseek(rtjpeg_file, 0, SEEK_CUR);

  read(rtjpeg_file, &frameheader, FRAMEHEADERSIZE);

  foundit = 0;
  rtjpeg_effdsp=44100;

  while (!foundit) {
    ftype = ' ';
    if (frameheader.frametype == 'S') {
      if (frameheader.comptype == 'V') {
      }
      if (frameheader.comptype == 'A') {
        rtjpeg_effdsp=frameheader.timecode;
	if (rtjpeg_effdsp > 0) {
           foundit = 1;
	   continue;
	}
      }
    } else {
      if (frameheader.frametype == 'V') {
        ftype = 'V';
      }
    }
    if (frameheader.frametype != 'R' && frameheader.packetlength!=0) {
      // we have to read to be sure to count the correct amount of frames
      if (frameheader.packetlength != read(rtjpeg_file, space, frameheader.packetlength)) {
        foundit=1;
        continue;
      }
    }
  
    foundit = FRAMEHEADERSIZE != read(rtjpeg_file, &frameheader, FRAMEHEADERSIZE); 
  }

  free(space); // give back memory

  lseek(rtjpeg_file, startpos, SEEK_SET);

  return(0);
}

/* ------------------------------------------------- */

unsigned char *decode_frame(struct rtframeheader *frameheader,unsigned char *strm)
{
  int r;
  int keyframe;
  unsigned int out_len;
  static unsigned char *buf2 = 0;
  static char lastct='1';
  int compoff = 0;


  if (buf2==NULL) {
    buf2       = (unsigned char *) malloc( rtjpeg_video_width*rtjpeg_video_height +
                        (rtjpeg_video_width*rtjpeg_video_height)/2);
    planes[0] = rtjpeg_buf;
    planes[1] = planes[0] + rtjpeg_video_width*rtjpeg_video_height;
    planes[2] = planes[1] + (rtjpeg_video_width*rtjpeg_video_height)/4;
  }

  // now everything is initialized 

  /* fprintf(stderr, "%s\n", "after reading frame"); */

  // now check if we have a (N)ull or (L)ast in comprtype -- empty data packet!
  if (frameheader->frametype == 'V') {
    if (frameheader->comptype == 'N') {
      memset(rtjpeg_buf,   0,  rtjpeg_video_width*rtjpeg_video_height);
      memset(rtjpeg_buf+rtjpeg_video_width*rtjpeg_video_height, 
                         127, (rtjpeg_video_width*rtjpeg_video_height)/2);
      return(rtjpeg_buf);
    }
    if (frameheader->comptype == 'L') {
      switch(lastct) {
        case '0': 
        case '3': return(buf2); break;
        case '1': 
        case '2': return(rtjpeg_buf); break;
        default: return(rtjpeg_buf);
      }
    }
  }

  // look for the keyframe flag and if set clear it
  keyframe = frameheader->keyframe==0;
  if (keyframe) {
    // normally there would be nothing to do, but for testing we
    // we will reset the buffer before decompression
    //memset(rtjpeg_buf,   0,  rtjpeg_video_width*rtjpeg_video_height);
    //memset(rtjpeg_buf+rtjpeg_video_width*rtjpeg_video_height, 
    //                   127, (rtjpeg_video_width*rtjpeg_video_height)/2);
    // rtjpeg_reset_old(); // this might be used if i/we make real
    // predicted frames (but with no motion compensation)
  }

  if (frameheader->comptype == '0') compoff=1;
  if (frameheader->comptype == '1') compoff=1;
  if (frameheader->comptype == '2') compoff=0;
  if (frameheader->comptype == '3') compoff=0;

  lastct = frameheader->comptype; // we need that for the 'L' ctype

  // lzo decompression ------------------
  if (!compoff) {
    r = lzo1x_decompress(strm,frameheader->packetlength,buf2,&out_len,NULL);
    if (r != LZO_E_OK) {
      // if decompression fails try raw format :-)
      fprintf(stderr,"\nminilzo: can't decompress illegal data, ft='%c' ct='%c' len=%d tc=%d\n",
                    frameheader->frametype, frameheader->comptype, 
                    frameheader->packetlength, frameheader->timecode);
    }
  }

  // the raw modes

  // raw YUV420 (I420, YCrCb) uncompressed
  if (frameheader->frametype=='V' && frameheader->comptype == '0') {
    memcpy(buf2, strm, (int)(rtjpeg_video_width*rtjpeg_video_height*1.5)); // save for 'L'
    return(buf2);
  }
 
  // raw YUV420 (I420, YCrCb) but compressed
  if (frameheader->frametype=='V' && frameheader->comptype == '3') {
    return(buf2);
  }
 
  // rtjpeg decompression

  if (compoff) {
    RTjpeg_decompress(rtjd, strm, planes); 
  } else {
    RTjpeg_decompress(rtjd, buf2, planes);
  }

  return(rtjpeg_buf);
}

/* ------------------------------------------------- */

#define MAXVBUFFER 20

unsigned char *rtjpeg_get_frame(int *timecode, int onlyvideo, 
                                unsigned char **audiodata, int *alen)
{
  static int lastaudiolen=0;
  static unsigned char *strm = 0;
  static struct rtframeheader frameheader;

  static int wpos = 0;
  static int rpos = 0;
  static int            bufstat[MAXVBUFFER]; // 0 .. free,  1 .. filled
  static int          timecodes[MAXVBUFFER];
  static unsigned char *vbuffer[MAXVBUFFER];
  static unsigned char audiobuffer[512000]; // big to have enough buffer for
  static unsigned char    tmpaudio[512000]; // audio delays
  static int              audiolen=0;
  static int              fafterseek=0;
  static int              audiobytes=0;
  static int              audiotimecode;

  unsigned char *ret;
  int gotvideo=0;
  int gotaudio=0;
  int seeked=0;
  int bytesperframe;
//  int calcerror;
  int tcshift;
  int shiftcorrected=0;
  int ashift;
  int i;
  //int cnt=0;
 
  if (rtjpeg_buf==NULL) {
    rtjpeg_buf = (unsigned char *) malloc( rtjpeg_video_width*rtjpeg_video_height +
                        (rtjpeg_video_width*rtjpeg_video_height)/2);
    strm       = (unsigned char *) malloc( rtjpeg_video_width*rtjpeg_video_height +
                        (rtjpeg_video_width*rtjpeg_video_height)); // a bit more

    // initialize and purge buffers
    for (i=0; i< MAXVBUFFER; i++) {
      vbuffer[i] = (unsigned char *) malloc( rtjpeg_video_width*rtjpeg_video_height +
                                            (rtjpeg_video_width*rtjpeg_video_height)/2);
      bufstat[i]=0;
      timecodes[i]=0;
    }
    wpos = 0;
    rpos = 0;
    audiolen=0;
    seeked = 1;
  }

  // now everything is initialized 
  
  // now we have to read as many video frames and audio blocks 
  // until we have enough audio for the current video frame
  // that is: PAL  1/25*rtjpeg_effdsp*4    where rtjpeg_effdsp should be 44100
  //          NTSC 1/29.97*rtjpeg_effdsp*4

  bytesperframe = 4*(int)((1.0/rtjpeg_video_frame_rate)*((double)rtjpeg_effdsp/100.0)+0.5);

  // if seeked and the audio we get has an erlier timecode then the folowing
  // video frame we have to cut off (vid.tc-aud.tc)*rtjpeg_effdsp*4 bytes
  // from the head of the audioblock

  gotvideo = 0;
  if (onlyvideo>0)  gotaudio=1;
     else           gotaudio=0;

  // if (onlyvideo<0) gotvideo=1; // do not decode 

  while (!gotvideo || !gotaudio) {

    // now check if we already have a video frame in a buffer
    if (!gotvideo && bufstat[rpos]==1) {
      gotvideo = 1;
    } 

    // now check if we already have enough audio for the video frame in the audiobuffer

    if (!gotaudio && (audiolen >= bytesperframe)) {
      gotaudio = 1;
    }

    // check if we have both and correct audiolen if seeked
    if (gotvideo && gotaudio) {
      if (shiftcorrected || onlyvideo>0) continue;
      //fprintf(stderr, "we have both video and audio, audiolen=%d\n", audiolen);
      if (!seeked) {
        tcshift=(int)(((double)(audiotimecode-timecodes[rpos])*(double)rtjpeg_effdsp)/100000)*4;
        //fprintf(stderr, "\nbytesperframe was %d and now is %d shifted with %d\n", bytesperframe,
        //        bytesperframe-tcshift, -tcshift);
        if (tcshift> 1000) tcshift= 1000;
        if (tcshift<-1000) tcshift=-1000;
        bytesperframe -= tcshift;
        if (bytesperframe<100) { 
        //  fprintf(stderr, "bytesperframe was %d < 100 and now is forced to 100\n", bytesperframe);
          bytesperframe=100;
        }
      } else {
        //fprintf(stderr, "we have seeked audio will be adjusted, audiolen=%d\n", audiolen);
        //fprintf(stderr, "timecodes atc=%d vtc=%d\n", audiotimecode, timecodes[rpos]);
        // we have seeked and have now to correct audiolen due to timecode differences

        // video later than audio 
        // we have to cut off (vid.tc-aud.tc)*rtjpeg_effdsp*4 bytes
        if (timecodes[rpos] > audiotimecode) {
          ashift = (int)(((double)(audiotimecode-timecodes[rpos])*(double)rtjpeg_effdsp)/100000)*4;
          //ashift = (((timecodes[rpos]-audiotimecode)*rtjpeg_effdsp)/100)*4;
          if (ashift>audiolen) {
            audiolen = 0;
          } else {
            //fprintf(stderr, "timecode shift=%d atc=%d vtc=%d\n", ashift, audiotimecode, 
            //                timecodes[rpos]);
            memcpy(tmpaudio, audiobuffer, audiolen);
            memcpy(audiobuffer, tmpaudio+ashift, audiolen);
            audiolen -= ashift;
          }
        }
  
        // audio is later than video
        // we have to insert blank audio (aud.tc-vid.tc)*rtjpeg_effdsp* bytes
        if (timecodes[rpos] < audiotimecode) {
          ashift = (int)(((double)(audiotimecode-timecodes[rpos])*(double)rtjpeg_effdsp)/100000)*4;
          if (ashift>(30*bytesperframe)) {
            fprintf(stderr, "Warning: should never happen, huge timecode gap gap=%d atc=%d vtc=%d\n",
                    ashift, audiotimecode, timecodes[rpos]);
          } else {
            //fprintf(stderr, "timecode shift=%d atc=%d vtc=%d\n", -ashift, audiotimecode, 
            //                timecodes[rpos]);
            memcpy(tmpaudio, audiobuffer, audiolen);
            bzero(audiobuffer, ashift); // silence!
            memcpy(audiobuffer+ashift, tmpaudio, audiolen);
            audiolen += ashift;
          }
        }
      }
      shiftcorrected=1; // mark so that we don't loop forever if audiobytes was less than
                        // bytesperframe at least once
      // now we check if we still have enough audio
      if (audiolen >= bytesperframe) {
        continue; // we are finished for this video frame + adequate audio
      } else {
        gotaudio = 0;
      }
    }

    if (read(rtjpeg_file, &frameheader, FRAMEHEADERSIZE)!=FRAMEHEADERSIZE) {
      rtjpeg_eof=1;
      return(rtjpeg_buf);
    }

#ifdef DEBUG_FTYPE
     fprintf(stderr,"\ntype='%c' ctype='%c' length=%d  timecode=%d  f-gop=%d", 
							frameheader.frametype,
                                                        frameheader.comptype,
                                                        frameheader.packetlength,
                                                        frameheader.timecode,
							frameheader.keyframe);
#endif

    if (frameheader.frametype == 'R') continue; // the R-frame has no data packet!!

    if (frameheader.frametype == 'S') {
      if (frameheader.comptype == 'A') {
          if (frameheader.timecode > 0)
            rtjpeg_effdsp = frameheader.timecode;
      }
    }
    
    if (frameheader.packetlength!=0) {
      if(read(rtjpeg_file, strm, frameheader.packetlength)!=frameheader.packetlength) {
        rtjpeg_eof=1;
        return(rtjpeg_buf);
      } 
    }

    if (frameheader.frametype=='V') {
      // decode the videobuffer and buffer it
      if (onlyvideo>= 0) {
        ret = decode_frame(&frameheader, strm);
      } else {
        ret = vbuffer[0]; // we don't decode video for exporting audio only
      }
      // now buffer it
      memcpy(vbuffer[wpos], ret, (int)(rtjpeg_video_width*rtjpeg_video_height*1.5));
      timecodes[wpos] = frameheader.timecode;
      bufstat[wpos]=1;
      //lastwpos=wpos;
      wpos = (wpos+1) % MAXVBUFFER;
      continue;
    }

    if (frameheader.frametype=='A' && onlyvideo<=0) {
      // buffer the audio
      if (frameheader.comptype=='N' && lastaudiolen!=0) {
        // this won't happen too often, if ever!
        memset(strm,   0, lastaudiolen);
      } 
      if (frameheader.comptype=='3') {
	  int lameret = 0;
	  short pcmlbuffer[11025]; // these should be big enough for anything
	  short pcmrbuffer[11025];
	  int sampleswritten = 0;
	  int audiobufferpos = audiolen;
	  int packetlen = frameheader.packetlength;

	  do {
              lameret = lame_decode(strm, packetlen, pcmlbuffer, pcmrbuffer);

	      if (lameret > 0) {
		 int itemp = 0;
		 
		 for (itemp = 0; itemp < lameret; itemp++)
                 {
                     memcpy(&audiobuffer[audiobufferpos], &pcmlbuffer[itemp], 
                            sizeof(short int));
		     audiobufferpos += 2;
		     
                     memcpy(&audiobuffer[audiobufferpos], &pcmrbuffer[itemp],
                            sizeof(short int));
		     audiobufferpos += 2;
		 }
		 sampleswritten += lameret;
              }	    
	      packetlen = 0;
          } while (lameret > 0);
	  
	  audiotimecode = frameheader.timecode + rtjpeg_audiodelay; // untested
	  if (audiolen > 0) {
            audiotimecode -= (int)(1000*(((double)audiolen*25.0)/(double)rtjpeg_effdsp));
	    if (audiotimecode<0) {
              audiotimecode = 0;
	    }
	  }

	  audiolen += sampleswritten * 4;
	  lastaudiolen = audiolen;
      } else {
        // now buffer it
        memcpy(audiobuffer+audiolen, strm, frameheader.packetlength);
        audiotimecode = frameheader.timecode + rtjpeg_audiodelay; // untested !!!! possible FIXME
        if (audiolen>0) {
          // now we take the new timecode and calculate the shift
          // we have to subtract from it - (audiolen*100/(effdsp*4))*1000 [msec]
          audiotimecode -= (int)(1000*(((double)audiolen*25.0)/(double)rtjpeg_effdsp));
          if (audiotimecode<0) {
            //fprintf(stderr, "WARNING audiotimecode < 0, at=%d\n", audiotimecode);
            audiotimecode = 0;
          }
        }
        audiolen += frameheader.packetlength;
        lastaudiolen = audiolen;
        // we do not know now if it is enough for the current video frame
      }
    } 

  }

  if (onlyvideo>0) {
    *alen = 0;
  } else {
    *alen = bytesperframe;
    memcpy(tmpaudio, audiobuffer, audiolen);
    memcpy(audiobuffer, tmpaudio+bytesperframe, audiolen);
    audiolen -= bytesperframe;
    audiobytes += bytesperframe;
  }
  
  *audiodata = tmpaudio;

  fafterseek ++;

  // now we have to return the frame and free it!!
  ret = vbuffer[rpos];
  bufstat[rpos] = 0; // clear flag
  rpos = (rpos+1) % MAXVBUFFER;

  return(ret);
}

/* ------------------------------------------------- */

int rtjpeg_close()
{
  close(rtjpeg_file);
  lame_close(gf);
  return(0);
}


/* ------------------------------------------------- */

int rtjpeg_get_video_width()
{
  return(rtjpeg_video_width);
}



/* ------------------------------------------------- */

int rtjpeg_get_video_height()
{
  return(rtjpeg_video_height);
}



/* ------------------------------------------------- */

double rtjpeg_get_video_frame_rate()
{
  return(rtjpeg_video_frame_rate);
}



/* ------------------------------------------------- */
/* we don't really check sig now, just the extension     */

int rtjpeg_check_sig(char *fname)
{
  int len;

  len=strlen(fname);
  if (len < 4)
    return(0);
  if ((0 == strcmp(fname+len-4,".nuv")) || 
      (0 == strcmp(fname+len-4,".NUV")))   return(1);
  return(0);
}



/* ------------------------------------------------- */

int rtjpeg_end_of_video()
{
   return (rtjpeg_eof);
}


