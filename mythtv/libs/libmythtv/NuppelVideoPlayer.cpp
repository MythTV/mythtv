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
#include <math.h>

#include "NuppelVideoPlayer.h"
#include "NuppelVideoRecorder.h"
#include "minilzo.h"
#include "XJ.h"
#include "effects.h"

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
    waud = raud = 0;

    paused = 0;
    deinterlace = 0;

    audiodevice = "/dev/dsp";

    ringBuffer = NULL;
    weMadeBuffer = false;

    osd = NULL;

    audio_samplerate = 44100;
}

NuppelVideoPlayer::~NuppelVideoPlayer(void)
{
    if (gf)
        lame_close(gf);
    if (rtjd)
        delete rtjd;
    if (weMadeBuffer)
        delete ringBuffer;
    if (osd)
        delete osd;
}

void NuppelVideoPlayer::InitSound(void)
{
    int bits = 16, stereo = 1, speed = audio_samplerate;

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

    positionMap = new map<long long, long long>;
    return 0;
}

int NuppelVideoPlayer::OpenFile(bool skipDsp)
{
    struct rtframeheader frameheader;
    int    startpos;
    int    foundit=0;
    char   ftype;
    char   *space;

    if (!skipDsp)
    {
        if (!ringBuffer)
        {
            ringBuffer = new RingBuffer(filename, false);
            weMadeBuffer = true;
	    livetv = false;
        }
        else 
            livetv = true;

        if (!ringBuffer->IsOpen())
        {
            fprintf(stderr, "File not found: %s\n", filename.c_str());
            return -1;
        }
    }
    ringBuffer->Read(&fileheader, FILEHEADERSIZE);

    if (!skipDsp)
    {
        video_width = fileheader.width;
        video_height = fileheader.height;
        video_frame_rate = fileheader.fps;
        eof = 0;
    }

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

    if (skipDsp)
    {
        delete [] space;
        return 0;
    }

    startpos = ringBuffer->Seek(0, SEEK_CUR);

    ringBuffer->Read(&frameheader, FRAMEHEADERSIZE);

    foundit = 0;
    effdsp = audio_samplerate;

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

int NuppelVideoPlayer::audiolen(bool use_lock)
{
    /* Thread safe, returns the number of valid bytes in the audio buffer */
    int ret;
    
    if (use_lock) 
        pthread_mutex_lock(&audio_buflock);

    if (waud >= raud)
        ret = waud - raud;
    else
        ret = AUDBUFSIZE - (raud - waud);

    if (use_lock)
        pthread_mutex_unlock(&audio_buflock);

    return ret;
}

int NuppelVideoPlayer::audiofree(bool use_lock)
{
    return AUDBUFSIZE - audiolen(use_lock) - 1;
    /* There is one wasted byte in the buffer. The case where waud = raud is
       interpreted as an empty buffer, so the fullest the buffer can ever
       be is AUDBUFSIZE - 1. */
}

int NuppelVideoPlayer::vbuffer_numvalid(void)
{
    /* thread safe, returns number of valid slots in the video buffer */
    int ret;
    pthread_mutex_lock(&video_buflock);

    if (wpos >= rpos)
        ret = wpos - rpos;
    else
        ret = MAXVBUFFER - (rpos - wpos);

    pthread_mutex_unlock(&video_buflock);
    return ret;
}

int NuppelVideoPlayer::vbuffer_numfree(void)
{
    return MAXVBUFFER - vbuffer_numvalid() - 1;
    /* There's one wasted slot, because the case when rpos = wpos is 
       interpreted as an empty buffer. So the fullest the buffer can be is
       MAXVBUFFER - 1. */
}

int NuppelVideoPlayer::GetAudiotime(void)
{
    /* Returns the current timecode of audio leaving the soundcard, based
       on the 'audiotime' computed earlier, and the delay since it was computed.

       This is a little roundabout...

       The reason is that computing 'audiotime' requires acquiring the audio 
       lock, which the video thread should not do. So, we call 'SetAudioTime()'
       from the audio thread, and then call this from the video thread. */
    int ret;
    struct timeval now;
    gettimeofday(&now, NULL);

    if (audiotime == 0)
        return 0;

    pthread_mutex_lock(&avsync_lock);

    ret = audiotime;
 
    ret += (now.tv_sec - audiotime_updated.tv_sec) * 1000;
    ret += (now.tv_usec - audiotime_updated.tv_usec) / 1000;

    pthread_mutex_unlock(&avsync_lock);
    return ret;
}

void NuppelVideoPlayer::SetAudiotime(void)
{
    int soundcard_buffer;
    int totalbuffer;

    /* We want to calculate 'audiotime', which is the timestamp of the audio
       which is leaving the sound card at this instant.

       We use these variables:

       'effdsp' is samples/sec, multiplied by 100.
       Bytes per sample is assumed to be 4.

       'audiotimecode' is the timecode of the audio that has just been 
       written into the buffer.

       'totalbuffer' is the total # of bytes in our audio buffer, and the
       sound card's buffer.

       'ms/byte' is given by '25000/effdsp'...
     */
 
    pthread_mutex_lock(&audio_buflock);
    pthread_mutex_lock(&avsync_lock);
 
    ioctl(audiofd, SNDCTL_DSP_GETODELAY, &soundcard_buffer); // bytes
    totalbuffer = audiolen(false) + soundcard_buffer;
               
    audiotime = audbuf_timecode - (int)((double)totalbuffer * 25000.0 /
                                        (double)effdsp);
 
    gettimeofday(&audiotime_updated, NULL);

    pthread_mutex_unlock(&avsync_lock);
    pthread_mutex_unlock(&audio_buflock);
}

void NuppelVideoPlayer::GetFrame(int onlyvideo)
{
    int gotvideo = 0;
    int seeked = 0;

    if (weseeked)
    {
        seeked = 1;
        weseeked = 0;
    }

    while (!gotvideo)
    {
	long long currentposition = ringBuffer->GetReadPosition();
        if (ringBuffer->Read(&frameheader, FRAMEHEADERSIZE) != FRAMEHEADERSIZE)
        {
            eof = 1;
            return;
        }

        if (frameheader.frametype == 'R') 
            continue; // the R-frame has no data packet

        if (frameheader.frametype == 'S') 
	{
            if (frameheader.comptype == 'A')
	    {
                if (frameheader.timecode > 0 && frameheader.timecode < 5500000)
		{
                    effdsp = frameheader.timecode;
		}
	    }
	    else if (frameheader.comptype == 'V')
            {
		(*positionMap)[framesPlayed / 30] = currentposition;
		lastKey = framesPlayed;
	    }
        }
	    
        if (frameheader.packetlength!=0) {
            if (ringBuffer->Read(strm, frameheader.packetlength) != 
                frameheader.packetlength) 
            {
                eof = 1;
                return;
            }
        }

        if (frameheader.frametype=='V') 
        {
            unsigned char *ret = DecodeFrame(&frameheader, strm);

            while (vbuffer_numfree() == 0)
            {
                //printf("waiting for video buffer to drain.\n");
                prebuffering = false;
                usleep(2000);
            }

            pthread_mutex_lock(&video_buflock);
            memcpy(vbuffer[wpos], ret, (int)(video_width*video_height * 1.5));
            timecodes[wpos] = frameheader.timecode;
            if (deinterlace)
                linearBlendYUV420(vbuffer[wpos], video_width, video_height);

            wpos = (wpos+1) % MAXVBUFFER;
            //printf("wpos now '%d'\n", wpos);
            pthread_mutex_unlock(&video_buflock);

            if (vbuffer_numvalid() >= usepre)
                prebuffering = false;
            gotvideo = 1;
            framesPlayed++;
            continue;
        }

        if (frameheader.frametype=='A' && !onlyvideo) 
        {
            if (frameheader.comptype=='N' && lastaudiolen!=0) 
            {
                memset(strm, 0, lastaudiolen);
            }
            else if (frameheader.comptype=='3') 
            {
                int lameret = 0;
                int len = 0;
                short int pcmlbuffer[audio_samplerate]; 
                short int pcmrbuffer[audio_samplerate];
                int packetlen = frameheader.packetlength;

                pthread_mutex_lock(&audio_buflock); // begin critical section

                do 
                {
                    lameret = lame_decode(strm, packetlen, pcmlbuffer, 
                                          pcmrbuffer);

                    if (lameret > 0) 
                    {
                        int itemp = 0;
                        int afree = audiofree(false);

                        if (lameret * 4 > afree)
                        {
                            lameret = afree / 4;
                            printf("Audio buffer overflow, audio data lost!\n");
                        }

                        short int *saudbuffer = (short int *)audiobuffer;

                        for (itemp = 0; itemp < lameret; itemp++)
                        {
                            saudbuffer[waud / 2] = pcmlbuffer[itemp];
                            saudbuffer[waud / 2 + 1] = pcmrbuffer[itemp];
                           
                            waud += 4;
                            len += 4;
                            if (waud >= AUDBUFSIZE)
                                waud -= AUDBUFSIZE;
                        }
                    }
                    else if (lameret < 0)
                    {
                        printf("lame decode error\n");
                        exit(-1);
                    }
                    packetlen = 0;
                } while (lameret > 0);

                audbuf_timecode = frameheader.timecode + (int)((double)len *
                                  25000.0 / (double)effdsp); // time at end 
                lastaudiolen = audiolen(false);
               
                pthread_mutex_unlock(&audio_buflock); // end critical section
            } 
            else 
            {
                int bdiff, len = frameheader.packetlength;
                int afree = audiofree(true);
                  
                if (len > afree)
                {
                    printf("Audio buffer overflow, audio data lost!\n");
                    len = afree;
                }

                pthread_mutex_lock(&audio_buflock); // begin critical section
                bdiff = AUDBUFSIZE - waud;
                if (bdiff < len)
                {
                    memcpy(audiobuffer + waud, strm, bdiff);
                    memcpy(audiobuffer, strm + bdiff, len - bdiff);
                    waud = len - bdiff;
                }
                else
                {
                    memcpy(audiobuffer + waud, strm, len);
                    waud += len;
                }
                lastaudiolen = audiolen(false);
                audbuf_timecode = frameheader.timecode + (int)((double)len *
                                  25000.0 / (double)effdsp); // time at end

                pthread_mutex_unlock(&audio_buflock); // end critical section
            }
        }
    }
}

void NuppelVideoPlayer::OutputVideoLoop(void)
{
    unsigned char *X11videobuf;
    int videosize;
    int laudiotime;
    int delay, avsync_delay;

    struct timeval nexttrigger, now; 

    gettimeofday(&nexttrigger, NULL);
  
    Jitterometer *output_jmeter = new Jitterometer("video_output", 100);

    videosize = video_width * video_height * 3 / 2;
 
    X11videobuf = XJ_init(video_width, video_height, "Mythical Convergence",
                          "MythTV");

    while (!eof && !killplayer)
    {
        if (paused)
        {
            video_actually_paused = true;
            if (livetv && ringBuffer->GetFreeSpace() < -1000)
            {
                paused = false;
                printf("forced unpause\n");
            }
            else
            {
                //printf("video waiting for unpause\n");
                usleep(50);
                continue;
            }
        }

        if (prebuffering)
        {
            //printf("prebuffering...\n");
            usleep(2000);
            continue;
        }

        if (vbuffer_numvalid() == 0)
        {
           prebuffering = true;
           continue;
        }

        /* if we get here, we're actually going to do video output */
  
        // calculate 'delay', that we need to get from 'now' to 'nexttrigger'
        gettimeofday(&now, NULL);

        delay = (nexttrigger.tv_sec - now.tv_sec) * 1000000 +
                (nexttrigger.tv_usec - now.tv_usec); // uSecs
        /* trigger */
        if (delay > 0)
            usleep(delay);

        memcpy(X11videobuf, vbuffer[rpos], videosize);
        osd->Display(X11videobuf);
        XJ_show(video_width, video_height);
        /* a/v sync assumes that when 'XJ_show' returns, that is the instant
           the frame has become visible on screen */

        output_jmeter->RecordCycleTime();

        /* compute new value of nexttrigger */
        nexttrigger.tv_usec += (int)(1000000 / video_frame_rate);

        /* Apply just a little feedback. The ComputeAudiotime() function is
           jittery, so if we try to correct our entire A/V drift on each frame,
           video output is jerky. Instead, correct 1/12 of the computed drift
           on each frame.
 
           This value (1/12) was arrived at through trial and error, it's a 
           tradeoff between a/v sync and video jitter */
        if (audiofd > 0)
        {
            laudiotime = GetAudiotime(); // ms, same scale as timecodes

            if (laudiotime != 0) // laudiotime = 0 after a seek
            {
                /* if we were perfect, timecodes[rpos] and laudiotime would
                   match and this adjustment wouldn't do anything */
               avsync_delay = (timecodes[rpos] - laudiotime) * 1000; // uSecs
               nexttrigger.tv_usec += avsync_delay / 12;
            }
        }

        /* normalize nexttrigger */
        while (nexttrigger.tv_usec > 999999)
        {
            nexttrigger.tv_sec++;
            nexttrigger.tv_usec -= 1000000;
        }
        while (nexttrigger.tv_usec < 0)
        {
            nexttrigger.tv_sec--;
            nexttrigger.tv_usec += 1000000;
        }

        /* update rpos */
        pthread_mutex_lock(&video_buflock);
        if (rpos != wpos) // if a seek occurred, rpos == wpos, in this case do
                          // nothing
            rpos = (rpos + 1) % MAXVBUFFER;
        pthread_mutex_unlock(&video_buflock);
    }

    XJ_exit();             
}

void NuppelVideoPlayer::OutputAudioLoop(void)
{
    int bytesperframe;
    int space_on_soundcard;

    while (!eof && !killplayer)
    {
        if (paused)
        {
            audio_actually_paused = true;
            ioctl(audiofd, SNDCTL_DSP_RESET, NULL);
            //printf("audio waiting for unpause\n");
            usleep(50);
            continue;
        }    

        if (prebuffering)
        {
            //printf("audio thread waiting for prebuffer\n");
            usleep(200);
            continue;
        }

        if (audiofd <= 0) 
            break;

        SetAudiotime(); // once per loop, calculate stuff for a/v sync

        /* do audio output */
 
        /* approximate # of audio bytes for each frame. */
        bytesperframe = 4 * (int)((1.0/video_frame_rate) *
                                  ((double)effdsp/100.0) + 0.5);

        // wait for the buffer to fill with enough to play
        if (bytesperframe > audiolen(true))
        { 
            //printf("audio thread waiting for buffer to fill\n");
            usleep(200);
            continue;
        }

        // wait for there to be free space on the sound card so we can write
        // without blocking.  We don't want to block while holding audio_buflock

        audio_buf_info info;
        ioctl(audiofd, SNDCTL_DSP_GETOSPACE, &info);
        space_on_soundcard = info.bytes;

        if (bytesperframe > space_on_soundcard)
        {
            //printf("waiting for space on soundcard\n");
            usleep(200);
            continue;
        }

        pthread_mutex_lock(&audio_buflock); // begin critical section

        // re-check audiolen() in case things changed.
        // for example, ClearAfterSeek() might have run
        if (bytesperframe <= audiolen(false))
        {
            int bdiff = AUDBUFSIZE - raud;
            if (bytesperframe > bdiff)
            {
                WriteAudio(audiobuffer + raud, bdiff);
                WriteAudio(audiobuffer, bytesperframe - bdiff);
            }
            else
            {
                WriteAudio(audiobuffer + raud, bytesperframe);
            }

            /* update raud */
            raud = (raud + bytesperframe) % AUDBUFSIZE;
        }
        pthread_mutex_unlock(&audio_buflock); // end critical section
    }
    ioctl(audiofd, SNDCTL_DSP_RESET, NULL);
}

void *NuppelVideoPlayer::kickoffOutputAudioLoop(void *player)
{
    ((NuppelVideoPlayer *)player)->OutputAudioLoop();
    return NULL;
}

void *NuppelVideoPlayer::kickoffOutputVideoLoop(void *player)
{
    ((NuppelVideoPlayer *)player)->OutputVideoLoop();
    return NULL;
}

void NuppelVideoPlayer::StartPlaying(void)
{
    usepre = 2;

    InitSubs();
    OpenFile();

    if (fileheader.audioblocks != 0)
        InitSound();
  
    osd = new OSD(video_width, video_height, osdfilename);

    playing = true;
    killplayer = false;
  
    framesPlayed = 0;

    audbuf_timecode = 0;
    audiotime = 0;
    gettimeofday(&audiotime_updated, NULL);

    weseeked = 0;
    rewindtime = 0;
    fftime = 0;

    resetplaying = false;
    
    if (buf == NULL)
    {
        int i;
        buf = new unsigned char[video_width * video_height * 3 / 2];
        strm = new unsigned char[video_width * video_height * 2];
 
        pthread_mutex_init(&audio_buflock, NULL);
        pthread_mutex_init(&video_buflock, NULL);
        pthread_mutex_init(&avsync_lock, NULL);

        // initialize and purge buffers
        for (i = 0; i < MAXVBUFFER; i++)
        {
            vbuffer[i] = new unsigned char[video_width * video_height * 3 / 2];
        }
        ClearAfterSeek();
    }

    /* This thread will fill the video and audio buffers, it does all CPU
       intensive operations. We fork two other threads which do nothing but
       write to the audio and video output devices.  These should use a 
       minimum of CPU. */
    pthread_t output_audio, output_video;
    pthread_create(&output_audio, NULL, kickoffOutputAudioLoop, this);
    pthread_create(&output_video, NULL, kickoffOutputVideoLoop, this);

    while (!eof && !killplayer)
    {
	if (resetplaying)
	{
            ClearAfterSeek();

	    framesPlayed = 0;

	    //OpenFile(true);
	    delete positionMap;
	    positionMap = new map<long long, long long>;

	    resetplaying = false;
	    actuallyreset = true;
        }
	    
        if (paused)
	{ 
            actuallypaused = true;
            if (livetv && ringBuffer->GetFreeSpace() < -1000)
            {
                paused = false;
		printf("forced unpause\n");
	    }
	    else
            {
                //printf("startplaying waiting for unpause\n");
                usleep(50);
                continue;
            }
	}
	
	if (rewindtime > 0)
	{
	    rewindtime *= video_frame_rate;

            DoRewind();

            rewindtime = 0;
	}
	if (fftime > 0)
	{
            CalcMaxFFTime();
	    fftime *= video_frame_rate;

            DoFastForward();

            fftime = 0;
	}

        GetFrame(audiofd <= 0); // reads next compressed video frame from the
                                // ringbuffer, decompresses it, and stores it
                                // in our local buffer. Also reads and
                                // decompresses any audio frames it encounters
    }

    // these threads will also exit when killplayer or eof is true
    pthread_join(output_video, NULL);
    pthread_join(output_audio, NULL);

    playing = false;
}

bool NuppelVideoPlayer::DoRewind(void)
{
    if (rewindtime < 5)
        return false;

    int number = (int)rewindtime;

    long long desiredFrame = framesPlayed - number;

    if (desiredFrame < 0)
        desiredFrame = 0;

    long long storelastKey = lastKey;
    while (lastKey > desiredFrame)
    {
        lastKey -= 30;
    }
    if (lastKey < 1)
        lastKey = 1;

    int normalframes = desiredFrame - lastKey;
    long long keyPos = (*positionMap)[lastKey / 30];
    long long curPosition = ringBuffer->GetReadPosition();
    long long diff = keyPos - curPosition;
  
    while (ringBuffer->GetFreeSpaceWithReadChange(diff) < 0)
    {
        lastKey += 30;
        keyPos = (*positionMap)[lastKey / 30];
        if (keyPos == 0)
            continue;
        diff = keyPos - curPosition;
        normalframes = 0;
        if (lastKey > storelastKey)
        {
            lastKey = storelastKey;
            diff = 0;
            normalframes = 0;
            return false;
        }
    }

    if (keyPos == 0)
        return false;

    ringBuffer->Seek(diff, SEEK_CUR);
    framesPlayed = lastKey;

    int fileend = 0;

    while (normalframes > 0)
    {
        fileend = (FRAMEHEADERSIZE != ringBuffer->Read(&frameheader,
                                                       FRAMEHEADERSIZE));

        if (fileend)
            continue;
        if (frameheader.frametype == 'R')
            continue;

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

        fileend = (ringBuffer->Read(strm, frameheader.packetlength) !=
                                    frameheader.packetlength);

        if (fileend)
            continue;
        if (frameheader.frametype == 'V')
        {
            framesPlayed++;
            normalframes--;

            DecodeFrame(&frameheader, strm);
        }
    }

    ClearAfterSeek();
    return true;
}


void NuppelVideoPlayer::CalcMaxFFTime(void)
{
    if (livetv)
    {
        float behind = (float)(nvr->GetFramesWritten() - framesPlayed) / 
                       video_frame_rate;
	if (behind < 1.0) // if we're close, do nothing
	    fftime = 0.0;
	else if (behind - fftime <= 1.0)
	{
            fftime = behind - 1.0;
	}
    }
    else
    {
    }
}

bool NuppelVideoPlayer::DoFastForward(void)
{
    if (fftime < 5)
        return false;

    int number = (int)fftime;

    long long desiredFrame = framesPlayed + number;
    long long desiredKey = lastKey;

    while (desiredKey < desiredFrame)
    {
        desiredKey += 30;
    }
    desiredKey -= 30;

    int normalframes = desiredFrame - desiredKey;
    int fileend = 0;

    if (positionMap->find(desiredKey / 30) != positionMap->end())
    {
        lastKey = desiredKey;
        long long keyPos = (*positionMap)[lastKey / 30];
        long long diff = keyPos - ringBuffer->GetReadPosition();

        ringBuffer->Seek(diff, SEEK_CUR);
        framesPlayed = lastKey;
    }
    else  
    {
        while (lastKey < desiredKey && !fileend)
        {
            fileend = (FRAMEHEADERSIZE != ringBuffer->Read(&frameheader,
                                                       FRAMEHEADERSIZE));

            if (frameheader.frametype == 'S')
            {
                if (frameheader.comptype == 'V')
                {
                    (*positionMap)[framesPlayed / 30] = 
                                                 ringBuffer->GetReadPosition();
                    lastKey = framesPlayed;
                }
                if (frameheader.comptype == 'A')
                    if (frameheader.timecode > 0)
                    {
                        effdsp = frameheader.timecode;
                    }
            }
            else if (frameheader.frametype == 'V')
            {
                framesPlayed++;
            }

            if (frameheader.frametype != 'R' && frameheader.packetlength > 0)
            {
                fileend = (ringBuffer->Read(strm, frameheader.packetlength) !=
                           frameheader.packetlength);
            }
        }
    } 

    while (normalframes > 0 && !fileend)
    {
        fileend = (FRAMEHEADERSIZE != ringBuffer->Read(&frameheader,
                                                       FRAMEHEADERSIZE));

        if (fileend)
            continue;
        if (frameheader.frametype == 'R')
            continue;
        else if (frameheader.frametype == 'S') 
        {
            if (frameheader.comptype == 'A')
            {
                if (frameheader.timecode > 0)
                {
                    effdsp = frameheader.timecode;
                }
            }
        }

        fileend = (ringBuffer->Read(strm, frameheader.packetlength) !=
                                    frameheader.packetlength);

        if (frameheader.frametype == 'V')
        {
            framesPlayed++;
            normalframes--;
            DecodeFrame(&frameheader, strm);
        }
    }

    ClearAfterSeek();
    return true;
}

void NuppelVideoPlayer::ClearAfterSeek(void)
{
    /* caller to this function should not hold any locks, we acquire all three
       right here */

    pthread_mutex_lock(&audio_buflock);
    pthread_mutex_lock(&video_buflock);
    pthread_mutex_lock(&avsync_lock);

    for (int i = 0; i < MAXVBUFFER; i++)
    {
        timecodes[i] = 0;
    }
    wpos = 0;
    rpos = 0;
    raud = waud = 0;
    weseeked = 1;
    audbuf_timecode = 0;
    audiotime = 0;
    gettimeofday(&audiotime_updated, NULL);
    prebuffering = true;

    if (audiofd)
        ioctl(audiofd, SNDCTL_DSP_RESET, NULL);

    pthread_mutex_unlock(&avsync_lock);
    pthread_mutex_unlock(&video_buflock);
    pthread_mutex_unlock(&audio_buflock);
}

void NuppelVideoPlayer::SetInfoText(const string &text, const string &subtitle,
                                    const string &desc, const string &category,
                                    const string &start, const string &end,
                                    int secs)
{
    osd->SetInfoText(text, subtitle, desc, category, start, end, 
                     (int)(secs * ceil(video_frame_rate)));
}

void NuppelVideoPlayer::SetChannelText(const string &text, int secs)
{
    osd->SetChannumText(text, (int)(secs * ceil(video_frame_rate)));
}

void NuppelVideoPlayer::ShowLastOSD(int secs)
{
    osd->ShowLast((int)(secs * ceil(video_frame_rate)));
}

