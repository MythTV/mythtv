#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/soundcard.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>

#include "NuppelVideoPlayer.h"
#include "minilzo.h"
#include "XJ.h"
#include "linearBlend.h"

NuppelVideoPlayer::NuppelVideoPlayer(void)
{
    playing = false;
    audiofd = -1;
    filename = "output.nuv";

    eof = 0;
    buf = NULL;
    buf2 = NULL;
    lastct = '1';

    gf = NULL;
    rtjd = NULL;
 
    lastaudiolen = 0;
    strm = NULL;
    wpos = rpos = 0;
    audiolen = 0;
    fafterseek = 0;

    paused = 0;
    deinterlace = 0;

    audiodevice = "/dev/dsp";

    ringBuffer = NULL;
    weMadeBuffer = false;
}

NuppelVideoPlayer::~NuppelVideoPlayer(void)
{
    if (gf)
        lame_close(gf);
    if (rtjd)
        delete rtjd;
    if (weMadeBuffer)
        delete ringBuffer;
}

void NuppelVideoPlayer::InitSound(void)
{
    int bits = 16, stereo = 1, speed = 44100;

    audiofd = open(audiodevice.c_str(), O_WRONLY);
    if (audiofd == -1)
    {
        fprintf(stderr, "Can't open audio device\n");
	return;
    }

    if (ioctl(audiofd, SNDCTL_DSP_SAMPLESIZE, &bits) < 0)
    {
        fprintf(stderr, "problem setting sample size\n");
        close(audiofd);
        audiofd = -1;
        return;
    }

    if (ioctl(audiofd, SNDCTL_DSP_STEREO, &stereo) < 0) 
    {
        fprintf(stderr, "problem setting to stereo\n");
        close(audiofd);
        audiofd = -1;
        return;
    }

    if (ioctl(audiofd, SNDCTL_DSP_SPEED, &speed) < 0) 
    {
        fprintf(stderr,"problem setting sample rate\n");
        close(audiofd);
        audiofd = -1;
        return;
    }
}

void NuppelVideoPlayer::WriteAudio(unsigned char *aubuf, int size)
{
    if (audiofd <= 0)
        return;

    unsigned char *tmpbuf;
    int written = 0, lw = 0;

    tmpbuf = aubuf;

    while ((written < size) && 
           ((lw = write(audiofd, tmpbuf, size - written)) > 0))
    {
        if (lw == -1)
        {
            fprintf(stderr, "Error writing to audio device\n");
            close(audiofd);
            audiofd = -1;
            return;
        }
        written += lw;
        tmpbuf += lw;
    }
}

int NuppelVideoPlayer::InitSubs(void)
{
    gf = lame_init();
    lame_set_decode_only(gf, 1);
    lame_decode_init();
    lame_init_params(gf);

    int i;
    rtjd = new RTjpeg();
    i = RTJ_YUV420;
    rtjd->SetFormat(&i);

    if (lzo_init() != LZO_E_OK)
    {
        fprintf(stderr, "lzo_init() failed");
        return -1;
    }

    return 0;
}

int NuppelVideoPlayer::OpenFile(void)
{
    if (InitSubs() < 0)
        return -1;

    struct rtframeheader frameheader;
    int    startpos;
    int    foundit=0;
    char   ftype;
    char   *space;

    if (!ringBuffer)
    {
        ringBuffer = new RingBuffer(filename.c_str(), true, false);
        weMadeBuffer = true;
    }

    if (!ringBuffer->IsOpen())
    {
        fprintf(stderr, "File not found: %s\n", filename.c_str());
        return -1;
    }

    ringBuffer->Read(&fileheader, FILEHEADERSIZE);

    video_width = fileheader.width;
    video_height = fileheader.height;
    video_frame_rate = fileheader.fps;
    eof = 0;

    space = new char[video_width * video_height * 3 / 2];

    if (FRAMEHEADERSIZE != ringBuffer->Read(&frameheader, FRAMEHEADERSIZE))
    {
        fprintf(stderr, "File not big enough for a header\n");
        return -1;
    }
    if (frameheader.frametype != 'D') 
    {
        fprintf(stderr, "Illegal file format\n");
        return -1;
    }
    if (frameheader.packetlength != ringBuffer->Read(space, 
                                                     frameheader.packetlength))
    {
        fprintf(stderr, "File not big enough for first frame data\n");
        return -1;
    }

    if ((video_height & 1) == 1)
    {
        video_height--;
        fprintf(stderr, "Incompatible video height, reducing to %d\n", 
                video_height);
    }

    startpos = ringBuffer->Seek(0, SEEK_CUR);

    ringBuffer->Read(&frameheader, FRAMEHEADERSIZE);

    foundit = 0;
    effdsp = 44100;

    
    while (!foundit) 
    {
        ftype = ' ';
        if (frameheader.frametype == 'S') 
        {
            if (frameheader.comptype == 'A')
            {
                effdsp = frameheader.timecode;
                if (effdsp > 0)
                {
                    foundit = 1;
                    continue;
                }
            }
        }
        if (frameheader.frametype != 'R' && frameheader.packetlength != 0)
        {
            if (frameheader.packetlength != ringBuffer->Read(space, 
                                                 frameheader.packetlength))
            {
                foundit = 1;
                continue;
            }
        }

        foundit = (FRAMEHEADERSIZE != ringBuffer->Read(&frameheader, 
                                                       FRAMEHEADERSIZE));
    }

    delete [] space;

    ringBuffer->Seek(startpos, SEEK_SET);

    return 0;
}
      
unsigned char *NuppelVideoPlayer::DecodeFrame(struct rtframeheader *frameheader,
                                              unsigned char *lstrm)
{
    int r;
    unsigned int out_len;
    int compoff = 0;

    if (!buf2)
    {
        buf2 = new unsigned char[video_width * video_height * 3 / 2];
        planes[0] = buf;
        planes[1] = planes[0] + video_width * video_height;
        planes[2] = planes[1] + (video_width * video_height) / 4;
    }

    if (frameheader->frametype == 'V') {
        if (frameheader->comptype == 'N') {
            memset(buf, 0,  video_width * video_height);
            memset(buf + video_width * video_height, 127,
                   (video_width * video_height)/2);
            return(buf);
        }
        if (frameheader->comptype == 'L') {
            switch(lastct) {
                case '0':
                case '3': return(buf2); break;
                case '1':
                case '2': return(buf); break;
                default: return(buf);
            }
        }
    }

    //keyframe = frameheader->keyframe==0;
    //if (keyframe) {
    //}

    if (frameheader->comptype == '0' || frameheader->comptype == '1') 
        compoff=1;
    else if (frameheader->comptype == '2' || frameheader->comptype == '3') 
        compoff=0;

    lastct = frameheader->comptype;

    if (!compoff) 
    {
        r = lzo1x_decompress(lstrm, frameheader->packetlength, buf2, &out_len,
                              NULL);
        if (r != LZO_E_OK) 
        {
            fprintf(stderr,"\nminilzo: can't decompress illegal data, "
                    "ft='%c' ct='%c' len=%d tc=%d\n",
                    frameheader->frametype, frameheader->comptype,
                    frameheader->packetlength, frameheader->timecode);
        }
    }

    if (frameheader->frametype=='V' && frameheader->comptype == '0') 
    {
        memcpy(buf2, lstrm, (int)(video_width * video_height*1.5));
        return(buf2);
    }

    if (frameheader->frametype=='V' && frameheader->comptype == '3') 
        return(buf2);

    if (compoff) 
        rtjd->Decompress((int8_t*)lstrm, planes);
    else
        rtjd->Decompress((int8_t*)buf2, planes);

    return(buf);
}

unsigned char *NuppelVideoPlayer::GetFrame(int *timecode, int onlyvideo,
                                           unsigned char **audiodata, int *alen)
{
    unsigned char *ret;
    int gotvideo = 0;
    int gotaudio = 0;
    int seeked = 0;
    int bytesperframe;

    int tcshift;
    int shiftcorrected = 0;
    int ashift;
    int i;

    if (buf==NULL) {
        buf = new unsigned char[video_width *video_height * 3 / 2];
        strm = new unsigned char[video_width * video_height * 2];

        // initialize and purge buffers
        for (i = 0; i < MAXVBUFFER; i++) {
            vbuffer[i] = new unsigned char[video_width * video_height * 3 / 2];
            bufstat[i]=0;
            timecodes[i]=0;
        }
        wpos = 0;
        rpos = 0;
        audiolen=0;
        seeked = 1;
    }

    bytesperframe = 4 * (int)((1.0/video_frame_rate) * 
                    ((double)effdsp/100.0)+0.5);

    gotvideo = 0;
    if (onlyvideo > 0)
        gotaudio = 1;
    else
        gotaudio = 0;

    while (!gotvideo || !gotaudio)
    {
        if (!gotvideo && bufstat[rpos] == 1)
            gotvideo = 1;
 
        if (!gotaudio && (audiolen >= bytesperframe))
            gotaudio = 1;
    
        if (gotvideo && gotaudio)
        {
            if (shiftcorrected || onlyvideo > 0)
                continue;
            if (!seeked)
            {
                  if (!seeked) 
                  {
                      tcshift = (int)(((double)(audiotimecode - 
                                timecodes[rpos]) * (double)effdsp)/100000)*4;
                      if (tcshift > 1000) 
                          tcshift= 1000;
                      if (tcshift < -1000) 
                          tcshift=-1000;
                      bytesperframe -= tcshift;
                      if (bytesperframe < 100) 
                          bytesperframe=100;
                  } 
                  else 
                  {
                      if (timecodes[rpos] > audiotimecode) {
                          ashift = (int)(((double)(audiotimecode - 
                                   timecodes[rpos]) * (double)effdsp) / 
                                   100000)*4;
                          if (ashift>audiolen)
                              audiolen = 0;
                          else 
                          {
                              memcpy(tmpaudio, audiobuffer, audiolen);
                              memcpy(audiobuffer, tmpaudio + ashift, audiolen);
                              audiolen -= ashift;
                          }
                      }
                      else if (timecodes[rpos] < audiotimecode) 
                      {
                          ashift = (int)(((double)(audiotimecode - 
                                   timecodes[rpos]) * (double)effdsp) / 
                                   100000)*4;
                          if (ashift > (30 * bytesperframe)) 
                          {
                              fprintf(stderr, "Warning: should never happen, "
                                      "huge timecode gap gap=%d atc=%d "
                                      "vtc=%d\n",
                                      ashift, audiotimecode, timecodes[rpos]);
                          } 
                          else 
                          {
                              memcpy(tmpaudio, audiobuffer, audiolen);
                              bzero(audiobuffer, ashift); // silence!
                              memcpy(audiobuffer + ashift, tmpaudio, audiolen);
                              audiolen += ashift;
                          }
                      }
                  }
                  shiftcorrected=1;  
                  if (audiolen >= bytesperframe) 
                      continue;
                  else
                      gotaudio = 0;
              }
        }

        if (ringBuffer->Read(&frameheader, FRAMEHEADERSIZE) != FRAMEHEADERSIZE)
        {
            eof=1;
            return (buf);
        }

        if (frameheader.frametype == 'R') 
            continue; // the R-frame has no data packet

        if (frameheader.frametype == 'S') 
	{
            if (frameheader.comptype == 'A')
	    {
                if (frameheader.timecode > 0)
		{
                    effdsp = frameheader.timecode;
		}
	    }
	}

        if (frameheader.packetlength!=0) {
            if (ringBuffer->Read(strm, frameheader.packetlength) != 
                frameheader.packetlength) 
            {
                eof = 1;
                return (buf);
            }
        }

        if (frameheader.frametype=='V') 
        {
            if (onlyvideo>= 0) 
            {
                ret = DecodeFrame(&frameheader, strm);
            } 
            else 
            {
                ret = vbuffer[0];
            }
            memcpy(vbuffer[wpos], ret, (int)(video_width*video_height * 1.5));
            timecodes[wpos] = frameheader.timecode;
            bufstat[wpos]=1;
            wpos = (wpos+1) % MAXVBUFFER;
            continue;
        }

        if (frameheader.frametype=='A' && onlyvideo<=0) 
        {
            if (frameheader.comptype=='N' && lastaudiolen!=0) 
            {
                memset(strm, 0, lastaudiolen);
            }
            else if (frameheader.comptype=='3') 
            {
                int lameret = 0;
                short pcmlbuffer[44100]; 
                short pcmrbuffer[44100];
                int sampleswritten = 0;
                int audiobufferpos = audiolen;
                int packetlen = frameheader.packetlength;

                do 
                {
                    lameret = lame_decode(strm, packetlen, pcmlbuffer, 
                                          pcmrbuffer);

                    if (lameret > 0) 
                    {
                        int itemp = 0;

                        for (itemp = 0; itemp < lameret; itemp++)
                        {
                            memcpy(&audiobuffer[audiobufferpos], 
                                   &pcmlbuffer[itemp], sizeof(short int));
                            audiobufferpos += 2;

                            memcpy(&audiobuffer[audiobufferpos], 
                                   &pcmrbuffer[itemp], sizeof(short int));
                            audiobufferpos += 2;
                        }
                        sampleswritten += lameret;
                    }
                    packetlen = 0;
                } while (lameret > 0);

                audiotimecode = frameheader.timecode + audiodelay; // untested
                if (audiolen > 0) 
                {
                    audiotimecode -= (int)(1000 * (((double)audiolen * 25.0) / 
                                     (double)effdsp));
                    if (audiotimecode < 0)
                        audiotimecode = 0;
                }

                audiolen += sampleswritten * 4;
                lastaudiolen = audiolen;
            } 
            else 
            {
                memcpy(audiobuffer+audiolen, strm, frameheader.packetlength);
                audiotimecode = frameheader.timecode + audiodelay;
                if (audiolen>0) 
                {
                    audiotimecode -= (int)(1000 * (((double)audiolen * 25.0) / 
                                     (double)effdsp));
                    if (audiotimecode<0) 
                    audiotimecode = 0;
                }
                audiolen += frameheader.packetlength;
                lastaudiolen = audiolen;
            }
        }
    }

    if (onlyvideo>0) 
        *alen = 0;
    else 
    {
        *alen = bytesperframe;
        memcpy(tmpaudio, audiobuffer, audiolen);
        memcpy(audiobuffer, tmpaudio+bytesperframe, audiolen);
        audiolen -= bytesperframe;
    }

    *audiodata = tmpaudio;

    fafterseek++;

    ret = vbuffer[rpos];
    bufstat[rpos] = 0; // clear flag
    rpos = (rpos+1) % MAXVBUFFER;

    return (ret);
}

#define MAXPREBUFFERS 60
void NuppelVideoPlayer::StartPlaying(void)
{
    char *X11videobuf;
    unsigned char *videobuf, *videobuf3;

    struct timeval now, last;
    struct timezone tzone;

    long tf;
    long usecs;
    int usepre = 8;
    unsigned char *audiodata;
    int audiodatalen;

    int timecode;

    int prebuffered = 0;
    int actpre = 0;
    int fnum = 0;

    unsigned char *prebuf[MAXPREBUFFERS];
    
    int videosize; 
    int key;
    
    now.tv_sec = 0;

    OpenFile();

    videosize = video_width * video_height * 3 / 2;

    if (fileheader.audioblocks != 0)
        InitSound();
  
    X11videobuf = XJ_init(video_width, video_height, "NuppelVideo Player", 
                          "NuppelVideo");
    videobuf3 = new unsigned char[videosize];

    playing = true;
   
    while (!eof && playing)
    {
        if (paused)
            goto endofloop;

        videobuf = GetFrame(&timecode, audiofd <= 0, &audiodata, &audiodatalen);
        if (eof)
            continue;
        if (audiodatalen != 0)
            WriteAudio(audiodata, audiodatalen);

        if (usepre != 0)
        {
            if (prebuffered < usepre)
            {
                prebuf[prebuffered] = new unsigned char[videosize];
                memcpy(prebuf[prebuffered], videobuf, videosize);
                prebuffered++;
                fnum++;
                continue;
            }
            else
            {
                memcpy(videobuf3, prebuf[actpre], videosize);
                memcpy(prebuf[actpre], videobuf, videosize);
                actpre++;
                if (actpre >= usepre)
                    actpre = 0;
            }
        }
        else
        {
            memcpy(videobuf3, videobuf, videosize);
        }

        if (now.tv_sec == 0)
        {
            gettimeofday(&now, &tzone);
            last.tv_sec = now.tv_sec;
            last.tv_usec = now.tv_usec;
            usecs = (int)(1000000.0/video_frame_rate)/2;
            if (usecs>0) 
                usleep(usecs);
        } 
        else 
        {
            gettimeofday(&now, &tzone);
            usecs = (long int)(1000000.0/video_frame_rate -
                 ((now.tv_sec-last.tv_sec)*1000000 + now.tv_usec-last.tv_usec));
            last.tv_sec = now.tv_sec;
            last.tv_usec = now.tv_usec;
            if (usecs > (1000000/video_frame_rate)/2) 
                usecs = (long int)((1000000/video_frame_rate)/2);
        }

        if (usecs > -1000000/video_frame_rate) 
        {
            //if (deinterlace)
            //    linearBlendYUV420(videobuf3, video_width, video_height);
            memcpy(X11videobuf, videobuf3, video_width * video_height);
            memcpy(X11videobuf + video_width * video_height, videobuf3 +
                   video_width * video_height * 5 / 4, video_width * 
                   video_height / 4);
            memcpy(X11videobuf + video_width * video_height * 5 / 4, 
                   videobuf3 + video_width * video_height, video_width * 
                   video_height / 4);

            XJ_show(video_width, video_height);
        }

        if (usecs > 0)
        {
            //usleep(usecs/4);
        }

        tf++;
        fnum++;

endofloop:
        key = XJ_CheckEvents();

        if (key > 0)
        {
            /*if (key == 'p' || key == 'P')
                PauseVideo();
            else if (key == wsRight)
                FFVideo();
            else if (key == wsLeft)
                RewVideo();*/
            if (key == wsEscape)
                eof = 1;
        }
    }

    playing = false;
    XJ_exit();
}


