#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/soundcard.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <time.h>
#include <math.h>
#include <qstringlist.h>

#include <iostream>
using namespace std;

#include "NuppelVideoPlayer.h"
#include "NuppelVideoRecorder.h"
#include "minilzo.h"
#include "XJ.h"
#include "effects.h"
#include "yuv2rgb.h"
#include "osdtypes.h"

extern pthread_mutex_t avcodeclock;

#define wsUp            0x52 + 256
#define wsDown          0x54 + 256
#define wsLeft          0x51 + 256
#define wsRight         0x53 + 256
#define wsEscape        0x1b + 256
#define wsZero          0xb0 + 256
#define wsOne           0xb1 + 256
#define wsTwo           0xb2 + 256
#define wsThree         0xb3 + 256
#define wsFour          0xb4 + 256
#define wsFive          0xb5 + 256
#define wsSix           0xb6 + 256
#define wsSeven         0xb7 + 256
#define wsEight         0xb8 + 256
#define wsNine          0xb9 + 256
#define wsEnter         0x8d + 256
#define wsReturn        0x0d + 256


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
    positionMap = NULL; 

    keyframedist = 30;

    lastaudiolen = 0;
    strm = NULL;
    wpos = rpos = 0;
    waud = raud = 0;

    paused = 0;

    audiodevice = "/dev/dsp";

    ringBuffer = NULL;
    weMadeBuffer = false;

    osd = NULL;
    audio_samplerate = 44100;
    editmode = false;
    advancevideo = resetvideo = advancedecoder = false;

    totalLength = 0;
    totalFrames = 0;

    avcodec_init();
    avcodec_register_all();

    mpa_codec = 0;
    mpa_ctx = NULL;
    osdtheme = "none";

    for (int i = 0; i < MAXVBUFFER; i++)
    {
        vbuffer[i] = NULL;
    }

    disablevideo = disableaudio = false;

    setpipplayer = pipplayer = NULL;
    needsetpipplayer = false;

    videoFilterList = "";

    usingextradata = false;
    memset(&extradata, 0, sizeof(extendeddata));

    videoOutput = NULL;
    watchingrecording = false;

    haspositionmap = false;

    exactseeks = false;

    eventvalid = false;

    timedisplay = NULL;
    seekamount = 30;
    seekamountpos = 2;

    dialogname = "";

    pthread_mutex_init(&eventLock, NULL);
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
    if (buf)
        delete [] buf;
    if (buf2)
        delete [] buf2;
    if (strm)
        delete [] strm;
    if (positionMap)
        delete positionMap;

    for (int i = 0; i < MAXVBUFFER; i++)
    {
        if (vbuffer[i])
            delete [] vbuffer[i];
    }

    CloseAVCodec();

    if (videoFilters.size() > 0)
    {
        filters_cleanup(&videoFilters[0], videoFilters.size());
        videoFilters.clear();
    }
}

bool NuppelVideoPlayer::GetPause(void)
{
    if (disableaudio)
    {
        return (actuallypaused && video_actually_paused);
    }
    return (actuallypaused && audio_actually_paused && video_actually_paused);
}
 
void NuppelVideoPlayer::InitSound(void)
{
    int bits = 16, stereo = 1, speed = audio_samplerate, caps;

    if (usingextradata)
    {
        bits = extradata.audio_bits_per_sample;
        stereo = (extradata.audio_channels == 2);
        speed = extradata.audio_sample_rate;
    }

    if (disableaudio)
    {
        audiofd = -1;
        return;
    }

    audiofd = open(audiodevice.ascii(), O_WRONLY);
    if (audiofd == -1)
    {
        cerr << "player: Can't open audio device: " << audiodevice << endl;
        perror("open audio:");
	return;
    }

    if (ioctl(audiofd, SNDCTL_DSP_SAMPLESIZE, &bits) < 0)
    {
        cerr << "problem setting sample size, exiting\n";
        close(audiofd);
        audiofd = -1;
        return;
    }

    if (ioctl(audiofd, SNDCTL_DSP_STEREO, &stereo) < 0) 
    {
        cerr << "problem setting to stereo, exiting\n";
        close(audiofd);
        audiofd = -1;
        return;
    }

    if (ioctl(audiofd, SNDCTL_DSP_SPEED, &speed) < 0) 
    {
        cerr << "problem setting sample rate, exiting\n";
        close(audiofd);
        audiofd = -1;
        return;
    }

    if (ioctl(audiofd, SNDCTL_DSP_GETCAPS, &caps) >= 0 && 
        !(caps & DSP_CAP_REALTIME))
    {
        cerr << "audio device cannot report buffer state accurately,\n"
             << "audio/video sync will be bad, continuing anyway\n";
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
            cerr << "Error writing to audio device, exiting\n";
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
        cerr << "lzo_init() failed, exiting\n";
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
            livetv = ringBuffer->LiveMode();

        if (!ringBuffer->IsOpen())
        {
            fprintf(stderr, "File not found: %s\n", 
                    ringBuffer->GetFilename().ascii());
            return -1;
        }
    }
    startpos = ringBuffer->Seek(0, SEEK_CUR);
    
    if (ringBuffer->Read(&fileheader, FILEHEADERSIZE) != FILEHEADERSIZE)
    {
        cerr << "Error reading file: " << ringBuffer->GetFilename() << endl;
        return -1;
    }

    while ((QString(fileheader.finfo) != "NuppelVideo") && 
           (QString(fileheader.finfo) != "MythTVVideo"))
    {
        ringBuffer->Seek(startpos, SEEK_SET);
        char dummychar;
        ringBuffer->Read(&dummychar, 1);

        startpos = ringBuffer->Seek(0, SEEK_CUR);
 
        if (ringBuffer->Read(&fileheader, FILEHEADERSIZE) != FILEHEADERSIZE)
        {
            cerr << "Error reading file: " << ringBuffer->GetFilename() << endl;
            return -1;
        }

        if (startpos > 20000)
        {
            cerr << "Bad file: " << ringBuffer->GetFilename().ascii() << endl;
            return -1;
        }
    }

    if (!skipDsp)
    {
        video_width = fileheader.width;
        video_height = fileheader.height;
        video_frame_rate = fileheader.fps;
        eof = 0;
    }

    keyframedist = fileheader.keyframedist;

    space = new char[video_width * video_height * 3 / 2];

    if (FRAMEHEADERSIZE != ringBuffer->Read(&frameheader, FRAMEHEADERSIZE))
    {
        cerr << "File not big enough for a header\n";
        return -1;
    }
    if (frameheader.frametype != 'D') 
    {
        cerr << "Illegal file format\n";
        return -1;
    }
    if (frameheader.packetlength != ringBuffer->Read(space, 
                                                     frameheader.packetlength))
    {
        cerr << "File not big enough for first frame data\n";
        return -1;
    }

    if ((video_height & 1) == 1)
    {
        video_height--;
        cerr << "Incompatible video height, reducing to " << video_height 
             << endl; 
    }

    if (skipDsp)
    {
        delete [] space;
        return 0;
    }

    startpos = ringBuffer->Seek(0, SEEK_CUR);

    ringBuffer->Read(&frameheader, FRAMEHEADERSIZE);
    
    if (frameheader.frametype == 'X')
    {
        if (frameheader.packetlength != EXTENDEDSIZE)
        {
            cerr << "Corrupt file.  Bad extended frame.\n";
        }
        else
        {
            ringBuffer->Read(&extradata, frameheader.packetlength);
            usingextradata = true;
            ringBuffer->Read(&frameheader, FRAMEHEADERSIZE);    
        }
    }

    if (usingextradata && extradata.seektable_offset > 0 && !disablevideo)
    {
        long long currentpos = ringBuffer->Seek(0, SEEK_CUR);
        struct rtframeheader seek_frameheader;

        ringBuffer->Seek(extradata.seektable_offset, SEEK_SET);

        ringBuffer->Read(&seek_frameheader, FRAMEHEADERSIZE);
    
        if (seek_frameheader.frametype != 'Q')
        {
            cerr << "Invalid seektable\n";
        }
        else
        {
            if (seek_frameheader.packetlength > 0)
            {
                char *seekbuf = new char[seek_frameheader.packetlength];
                ringBuffer->Read(seekbuf, seek_frameheader.packetlength);

                int numentries = seek_frameheader.packetlength / 
                                 sizeof(struct seektable_entry);
                struct seektable_entry ste;
                int offset = 0;

                for (int z = 0; z < numentries; z++)
                {
                    memcpy(&ste, seekbuf + offset, 
                           sizeof(struct seektable_entry));
                    offset += sizeof(struct seektable_entry);

                    (*positionMap)[ste.keyframe_number] = ste.file_offset;
                }
                haspositionmap = true;
                totalLength = (int)((ste.keyframe_number * keyframedist * 1.0) /
                                     video_frame_rate);
                totalFrames = (long long)ste.keyframe_number * keyframedist;
            }
        }

        ringBuffer->Seek(currentpos, SEEK_SET);
    }

    while (frameheader.frametype != 'A' && frameheader.frametype != 'V' &&
           frameheader.frametype != 'S' && frameheader.frametype != 'T' &&
           frameheader.frametype != 'R')
    {
        ringBuffer->Seek(startpos, SEEK_SET);
        
        char dummychar;
        ringBuffer->Read(&dummychar, 1);

        startpos = ringBuffer->Seek(0, SEEK_CUR);

        if (ringBuffer->Read(&frameheader, FRAMEHEADERSIZE) != FRAMEHEADERSIZE)
        {
            delete [] space;
            return -1;
        }

        if (startpos > 20000)
        {
            delete [] space;
            return -1;
        }
    }

    foundit = 0;
    effdsp = audio_samplerate;
    if (usingextradata)
        effdsp = extradata.audio_sample_rate;

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

        long long startpos2 = ringBuffer->Seek(0, SEEK_CUR);

        foundit = (FRAMEHEADERSIZE != ringBuffer->Read(&frameheader, 
                                                       FRAMEHEADERSIZE));

        while (frameheader.frametype != 'A' && frameheader.frametype != 'V' &&
               frameheader.frametype != 'S' && frameheader.frametype != 'T' &&
               frameheader.frametype != 'R' && frameheader.frametype != 'X')
        {      
            ringBuffer->Seek(startpos2, SEEK_SET);
        
            char dummychar;
            ringBuffer->Read(&dummychar, 1);
        
            startpos2 = ringBuffer->Seek(0, SEEK_CUR);
        
            foundit = (FRAMEHEADERSIZE != ringBuffer->Read(&frameheader, 
                                                           FRAMEHEADERSIZE));

            if (foundit)
                break;
        }
    }

    delete [] space;

    ringBuffer->Seek(startpos, SEEK_SET);

    if (haspositionmap)
    {
        QString bookmarkname = ringBuffer->GetFilename();
        bookmarkname += ".bookmark";
        FILE *bookmarkfile = fopen(bookmarkname.ascii(), "r");
        if (bookmarkfile)
        {
            long long pos = 0;
            fscanf(bookmarkfile, "%lld", &pos);
            fclose(bookmarkfile);
            unlink(bookmarkname);
            
            bool seeks = exactseeks;
            exactseeks = false;

            fftime = pos;
            DoFastForward(); 
            fftime = 0;

            exactseeks = seeks;
        }
    }

    return 0;
}

bool NuppelVideoPlayer::InitAVCodec(int codec)
{
    if (mpa_codec)
        CloseAVCodec();

    if (usingextradata)
    {
        switch(extradata.video_fourcc) 
        {
            case MKTAG('D', 'I', 'V', 'X'): codec = CODEC_ID_MPEG4; break;
            case MKTAG('W', 'M', 'V', '1'): codec = CODEC_ID_WMV1; break;
            case MKTAG('D', 'I', 'V', '3'): codec = CODEC_ID_MSMPEG4V3; break;
            case MKTAG('M', 'P', '4', '2'): codec = CODEC_ID_MSMPEG4V2; break;
            case MKTAG('M', 'P', 'G', '4'): codec = CODEC_ID_MSMPEG4V1; break;
            case MKTAG('M', 'J', 'P', 'G'): codec = CODEC_ID_MJPEG; break;
            case MKTAG('H', '2', '6', '3'): codec = CODEC_ID_H263; break;
            case MKTAG('I', '2', '6', '3'): codec = CODEC_ID_H263I; break;
            case MKTAG('M', 'P', 'E', 'G'): codec = CODEC_ID_MPEG1VIDEO; break;
            default: codec = -1;
        }
    }
    mpa_codec = avcodec_find_decoder((enum CodecID)codec);

    if (!mpa_codec)
    {
        cerr << "couldn't find codec " << codec;
        if (usingextradata)
            cerr << " (" << extradata.video_fourcc << ")";
        cerr << endl;
        return false;
    }

    if (mpa_ctx)
        free(mpa_ctx);

    mpa_ctx = avcodec_alloc_context();

    mpa_ctx->width = video_width;
    mpa_ctx->height = video_height;
    mpa_ctx->error_resilience = 2;
    
    if (avcodec_open(mpa_ctx, mpa_codec) < 0)
    {
        cerr << "Couldn't find lavc codec\n";
        return false;
    }

    return true;
}
    
void NuppelVideoPlayer::CloseAVCodec(void)
{
    if (!mpa_codec)
        return;

    avcodec_close(mpa_ctx);

    if (mpa_ctx)
        free(mpa_ctx);
    mpa_ctx = NULL;
}

void NuppelVideoPlayer::InitFilters(void)
{
    QStringList filters = QStringList::split(",", videoFilterList);
    for (QStringList::Iterator i = filters.begin(); i != filters.end(); i++)
    {
        VideoFilter *filter = load_videoFilter((char *)((*i).ascii()),
                                               NULL);
        if (filter != NULL)
            videoFilters.push_back(filter);
    }   
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

    if (frameheader->comptype == 'N') {
        memset(buf, 0,  video_width * video_height);
        memset(buf + video_width * video_height, 127,
                (video_width * video_height)/2);
        return(buf);
    }
    if (frameheader->comptype == 'L') {
        switch(lastct) {
            case '0': case '3': return buf2;
            case '1': case '2': return buf;
            default: return buf;
        }
    }

    compoff = 1;
    if (frameheader->comptype == '2' || frameheader->comptype == '3') 
        compoff=0;

    lastct = frameheader->comptype;

    if (!compoff) 
    {
        r = lzo1x_decompress(lstrm, frameheader->packetlength, buf2, &out_len,
                              NULL);
        if (r != LZO_E_OK) 
        {
            cerr << "minilzo: can't decompress illegal data\n";
        }
    }

    if (frameheader->comptype == '0') 
    {
        memcpy(buf2, lstrm, (int)(video_width * video_height*1.5));
        return buf2;
    }

    if (frameheader->comptype == '3') 
        return buf2;

    if (frameheader->comptype == '2' || frameheader->comptype == '1')
    {
        if (compoff) 
            rtjd->Decompress((int8_t*)lstrm, planes);
        else
            rtjd->Decompress((int8_t*)buf2, planes);
    }
    else
    {
        if (!mpa_codec)
            InitAVCodec(frameheader->comptype - '3');

        int gotpicture = 0;
#ifdef EXTRA_LOCKING
        pthread_mutex_lock(&avcodeclock);
#endif
        int ret = avcodec_decode_video(mpa_ctx, &mpa_picture, &gotpicture,
                                       lstrm, frameheader->packetlength);
#ifdef EXTRA_LOCKING
        pthread_mutex_unlock(&avcodeclock);
#endif

        if (ret < 0)
        {
            cout << "decoding error\n";
            return buf;
        }

        if (!gotpicture)
        {
            return NULL;
        }

        if (mpa_picture.linesize[0] != video_width)
        {
            for (int i = 0; i < video_height; i++)
            {
                memcpy(planes[0] + i * video_width, 
                       mpa_picture.data[0] + i * mpa_picture.linesize[0], 
                       video_width);
            }

            for (int i = 0; i < video_height / 2; i++)
            {
                memcpy(planes[1] + i * video_width / 2, 
                       mpa_picture.data[1] + i * mpa_picture.linesize[1],
                       video_width / 2);
                memcpy(planes[2] + i * video_width / 2,
                       mpa_picture.data[2] + i * mpa_picture.linesize[2],
                       video_width / 2);
            }
        }
        else
        {
            memcpy(planes[0], mpa_picture.data[0], video_width * video_height);
            memcpy(planes[1], mpa_picture.data[1], 
                   video_width * video_height / 4);
            memcpy(planes[2], mpa_picture.data[2], 
                   video_width * video_height / 4);
        }
    }

    return buf;
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

    if (audiotime == 0)
        return 0;

    pthread_mutex_lock(&avsync_lock);

    gettimeofday(&now, NULL);

    ret = audiotime;
 
    ret += (now.tv_sec - audiotime_updated.tv_sec) * 1000;
    ret += (now.tv_usec - audiotime_updated.tv_usec) / 1000;

    pthread_mutex_unlock(&avsync_lock);
    return ret;
}

void NuppelVideoPlayer::SetAudiotime(void)
{
    if (audbuf_timecode == 0)
        return;

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

    Frame frame;

    frame.codec = CODEC_YUV;
    frame.width = video_width;
    frame.height = video_height;
    frame.bpp = -1;
    frame.frameNumber = framesPlayed;

    if (weseeked)
    {
        seeked = 1;
        weseeked = 0;
    }

    while (!gotvideo)
    {
	long long currentposition = ringBuffer->GetReadPosition();
        long long startpos = ringBuffer->Seek(0, SEEK_CUR);
        if (ringBuffer->Read(&frameheader, FRAMEHEADERSIZE) != FRAMEHEADERSIZE)
        {
            eof = 1;
            return;
        }

        while (frameheader.frametype != 'A' && frameheader.frametype != 'V' &&
               frameheader.frametype != 'S' && frameheader.frametype != 'T' &&
               frameheader.frametype != 'R' && frameheader.frametype != 'X')
        {
            ringBuffer->Seek(startpos, SEEK_SET);
            char dummychar;
            ringBuffer->Read(&dummychar, 1);

            startpos = ringBuffer->Seek(0, SEEK_CUR);

            if (ringBuffer->Read(&frameheader, FRAMEHEADERSIZE) 
                != FRAMEHEADERSIZE)
            {
                eof = 1;
                return;
            }
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
                if (!haspositionmap)
		    (*positionMap)[framesPlayed/keyframedist] = currentposition;
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

            if (!ret)
                continue;
	    
            while (vbuffer_numfree() == 0)
            {
                //cout << "waiting for video buffer to drain.\n";
                prebuffering = false;
                usleep(2000);
            }

            pthread_mutex_lock(&video_buflock);
            memcpy(vbuffer[wpos], ret, (int)(video_width*video_height * 1.5));
            timecodes[wpos] = frameheader.timecode;

            frame.buf = vbuffer[wpos];

            if (videoFilters.size() > 0)
                process_video_filters(&frame, &videoFilters[0],
                                      videoFilters.size());

            wpos = (wpos+1) % MAXVBUFFER;
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
                            cout << "Audio buffer overflow, audio data lost!\n";
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
                        cout << "lame decode error, exiting\n";
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
                    cout << "Audio buffer overflow, audio data lost!\n";
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

static void NormalizeTimeval(struct timeval *tv)
{
    while (tv->tv_usec > 999999)
    {
	tv->tv_sec++;
	tv->tv_usec -= 1000000;
    }
    while (tv->tv_usec < 0)
    {
	tv->tv_sec--;
	tv->tv_usec += 1000000;
    }
}

void NuppelVideoPlayer::ResetNexttrigger(struct timeval *nexttrigger)
{
    /* when we're paused or prebuffering, we need to update
       'nexttrigger' before we start playing again. Otherwise,
       the value of 'nexttrigger' will be far in the past, and
       the video will play really fast for a while.*/
    
    gettimeofday(nexttrigger, NULL);
    nexttrigger->tv_usec += (int)(1000000 / video_frame_rate);
    NormalizeTimeval(nexttrigger);
}

unsigned char *NuppelVideoPlayer::GetCurrentFrame(int &w, int &h)
{
    w = video_width;
    h = video_height;
    return vbuffer[rpos];
}

void NuppelVideoPlayer::ShowPip(unsigned char *xvidbuf)
{
    int pipw, piph;

    unsigned char *pipimage = pipplayer->GetCurrentFrame(pipw, piph);

    if (!pipimage)
        return;

    int xoff = 50;
    int yoff = 50;
    
    for (int i = 0; i < piph; i++)
    {
        memcpy(xvidbuf + (i + yoff) * video_width + xoff,
               pipimage + i * pipw, pipw);
    }

    xoff /= 2;
    yoff /= 2;

    unsigned char *uptr = xvidbuf + video_width * video_height;
    unsigned char *vptr = xvidbuf + video_width * video_height * 5 / 4;
    int vidw = video_width / 2;

    unsigned char *pipuptr = pipimage + pipw * piph;
    unsigned char *pipvptr = pipimage + pipw * piph * 5 / 4;
    pipw /= 2;

    for (int i = 0; i < piph / 2; i ++)
    {
        memcpy(uptr + (i + yoff) * vidw + xoff, pipuptr + i * pipw, pipw);
        memcpy(vptr + (i + yoff) * vidw + xoff, pipvptr + i * pipw, pipw);
    }
}

int NuppelVideoPlayer::CheckEvents(void)
{
    int ret = 0;
    if (videoOutput && eventvalid)
    {
        pthread_mutex_lock(&eventLock);
        if (videoOutput && eventvalid)
            ret = videoOutput->CheckEvents();
        pthread_mutex_unlock(&eventLock);
    } 
    return ret;
}

void NuppelVideoPlayer::ToggleFullScreen(void)
{
    if (videoOutput)
        videoOutput->ToggleFullScreen();
}

void NuppelVideoPlayer::OutputVideoLoop(void)
{
    unsigned char *X11videobuf = NULL;
    int videosize;
    int laudiotime;
    int delay, avsync_delay;

    struct timeval nexttrigger, now; 

    gettimeofday(&nexttrigger, NULL);
  
    //Jitterometer *output_jmeter = new Jitterometer("video_output", 100);

    videosize = video_width * video_height * 3 / 2;

    char name[] = "MythTV"; 
    if (!disablevideo)
    {
        videoOutput = new XvVideoOutput();
        X11videobuf = videoOutput->Init(video_width, video_height, name, name);
        eventvalid = true;
    }

    int pause_rpos = 0;
    while (!eof && !killplayer)
    {
        if (needsetpipplayer)
        {
            pipplayer = setpipplayer;
            needsetpipplayer = false;
        }

        if (paused)
        {
            if (!video_actually_paused)
                pause_rpos = rpos;

            if (advancevideo)
            {
                rpos = (rpos + 1) % MAXVBUFFER;
                pause_rpos = rpos;
                advancevideo = false;
            }
            if (resetvideo)
            {
                resetvideo = false;
                pause_rpos = 0;
            }

            video_actually_paused = true;
            if (livetv && ringBuffer->GetFreeSpace() < -1000)
            {
                paused = false;
                cout << "forced unpause\n";
            }
            else
            {
                //printf("video waiting for unpause\n");
                usleep(50); 
                if (editmode)
                    UpdateTimeDisplay();

                if (!disablevideo)
                {
                    memcpy(X11videobuf, vbuffer[pause_rpos], videosize);
                    if (pipplayer)
                        ShowPip(X11videobuf);
                    osd->Display(X11videobuf);
                    videoOutput->Show(video_width, video_height);
                    ResetNexttrigger(&nexttrigger);
                }
                continue;
            }
        }
	video_actually_paused = false;

        if (prebuffering)
        {
            //printf("prebuffering...\n");
            usleep(2000);
	    ResetNexttrigger(&nexttrigger);
            continue;
        }

        if (vbuffer_numvalid() == 0)
        {
           prebuffering = true;
           continue;
        }

        /* if we get here, we're actually going to do video output */

        if (!disablevideo)
        {
            memcpy(X11videobuf, vbuffer[rpos], videosize);
            if (pipplayer)
                ShowPip(X11videobuf);
            osd->Display(X11videobuf);
        }
	
        // calculate 'delay', that we need to get from 'now' to 'nexttrigger'
        gettimeofday(&now, NULL);

        delay = (nexttrigger.tv_sec - now.tv_sec) * 1000000 +
                (nexttrigger.tv_usec - now.tv_usec); // uSecs

        if (delay > 100000)
        {
            cout << "Delaying to next trigger: " << delay << endl;
            delay = 100000;
        }

        /* trigger */
        if (delay > 0)
            usleep(delay);

        if (!disablevideo)
        {
            videoOutput->Show(video_width, video_height);
        }
        /* a/v sync assumes that when 'XJ_show' returns, that is the instant
           the frame has become visible on screen */

        //output_jmeter->RecordCycleTime();

        /* compute new value of nexttrigger */
        /* doing the gettimeofday here helps resync faster when it's somehow 
           got a huge delay */
        if (!disablevideo)
            gettimeofday(&nexttrigger, NULL);
        nexttrigger.tv_usec += (int)(1000000 / video_frame_rate);

        /* Apply just a little feedback. The ComputeAudiotime() function is
           jittery, so if we try to correct our entire A/V drift on each frame,
           video output is jerky. Instead, correct a fraction of the computed
	   drift on each frame.

	   In steady state, very little feedback is needed. However, if we are
	   far out of sync, we need more feedback. So, we case on this. */
        if (audiofd > 0)
        {
            laudiotime = GetAudiotime(); // ms, same scale as timecodes

            if (laudiotime != 0) // laudiotime = 0 after a seek
            {
                /* if we were perfect, timecodes[rpos] and laudiotime would
                   match and this adjustment wouldn't do anything */
               avsync_delay = (timecodes[rpos] - laudiotime) * 1000; // uSecs

	       if(avsync_delay < -100000 || avsync_delay > 100000)
		 nexttrigger.tv_usec += avsync_delay / 10; // re-syncing
	       else
		 nexttrigger.tv_usec += avsync_delay / 50; // steady state
            }
        }

	NormalizeTimeval(&nexttrigger);

        /* update rpos */
        pthread_mutex_lock(&video_buflock);
        if (rpos != wpos) // if a seek occurred, rpos == wpos, in this case do
                          // nothing
            rpos = (rpos + 1) % MAXVBUFFER;
        pthread_mutex_unlock(&video_buflock);
    }

    if (!disablevideo)
    {
        pthread_mutex_lock(&eventLock);
        delete videoOutput;
        videoOutput = NULL;
        eventvalid = false;
        pthread_mutex_unlock(&eventLock);
    }
}

void NuppelVideoPlayer::OutputAudioLoop(void)
{
    int bytesperframe;
    int space_on_soundcard;
    unsigned char zeros[1024];
    
    bzero(zeros, 1024);

    while (!eof && !killplayer)
    {
	if (audiofd <= 0) 
	    break;

	if (paused)
	{
            audio_actually_paused = true;
            //usleep(50);
            audiotime = 0; // mark 'audiotime' as invalid.
            WriteAudio(zeros, 1024);
            continue;
	}    
	
        if (prebuffering)
        {
	    audiotime = 0; // mark 'audiotime' as invalid
	    WriteAudio(zeros, 1024);

	    //printf("audio thread waiting for prebuffer\n");
	    continue;
        }

        SetAudiotime(); // once per loop, calculate stuff for a/v sync

        /* do audio output */
	
        /* approximate # of audio bytes for each frame. */
        bytesperframe = 4 * (int)((1.0/video_frame_rate) *
                                  ((double)effdsp/100.0) + 0.5);
	
        // wait for the buffer to fill with enough to play
        if (bytesperframe >= audiolen(true))
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
        if (bytesperframe < audiolen(false))
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
    //ioctl(audiofd, SNDCTL_DSP_RESET, NULL);
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
    killplayer = false;
    usepre = 2;

    InitSubs();
    if (OpenFile() < 0)
        return;

    if (fileheader.audioblocks != 0)
        InitSound();

    InitFilters();

    if (!disablevideo)
        osd = new OSD(video_width, video_height, (int)ceil(video_frame_rate),
                      osdfilename, osdprefix, osdtheme);

    playing = true;
  
    framesPlayed = 0;

    audbuf_timecode = 0;
    audiotime = 0;
    gettimeofday(&audiotime_updated, NULL);

    weseeked = 0;
    rewindtime = fftime = 0;

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
            else if (advancedecoder)
            {
                if (vbuffer_numvalid() <= 1)
                {
                    fftime = 1;
                    DoFastForward();
                    fftime = 0;

                    GetFrame(audiofd <= 0);
                    resetvideo = true;
                }
                else
                    advancevideo = true;
                advancedecoder = false;
                continue;
            }
            else if (rewindtime > 0)
            {   
                DoRewind();
                    
                rewindtime = 0;
                GetFrame(audiofd <= 0);
                resetvideo = true;
                continue;
            }       
            else if (fftime > 0)
            {
                fftime = CalcMaxFFTime(fftime);

                DoFastForward();

                fftime = 0;
                GetFrame(audiofd <= 0);
                resetvideo = true;
                continue;
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
            if (rewindtime >= 5)
                DoRewind();

            rewindtime = 0;
	}
	if (fftime > 0)
	{
            fftime = CalcMaxFFTime(fftime);

            if (fftime >= 5)
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

    close(audiofd);
    playing = false;
}

void NuppelVideoPlayer::SetBookmark(void)
{
    if (!haspositionmap)
        return;

    if (livetv)
        return;

    long long framenum = framesPlayed;
    QString filename = ringBuffer->GetFilename();
    filename += ".bookmark";

    FILE *bookmarkfile = fopen(filename.ascii(), "w");
    if (!bookmarkfile)
    {
        cerr << "Unable to open bookmark file: " << bookmarkfile << endl;
        return;
    }

    fprintf(bookmarkfile, "%lld\n", framenum);
    fclose(bookmarkfile);

    osd->ShowText("bookmark", "Position Saved", video_width * 1 / 8, 
                  video_height * 1 / 8, video_width * 7 / 8, video_height / 2, 
                  1);
}

bool NuppelVideoPlayer::DoRewind(void)
{
    long long number = rewindtime + 1;

    long long desiredFrame = framesPlayed - number;

    if (desiredFrame < 0)
        desiredFrame = 0;

    long long storelastKey = lastKey;
    while (lastKey > desiredFrame)
    {
        lastKey -= keyframedist;
    }
    if (lastKey < 1)
        lastKey = 1;

    int normalframes = desiredFrame - lastKey;
    long long keyPos = (*positionMap)[lastKey / keyframedist];
    long long curPosition = ringBuffer->GetReadPosition();
    long long diff = keyPos - curPosition;

    while (ringBuffer->GetFreeSpaceWithReadChange(diff) < 0)
    {
        lastKey += keyframedist;
        keyPos = (*positionMap)[lastKey / keyframedist];
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

    if (!exactseeks)
        normalframes = 0;

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

long long NuppelVideoPlayer::CalcMaxFFTime(long long ff)
{
    long long maxtime = (long long)(1.0 * video_frame_rate);
    if (watchingrecording && nvr)
        maxtime = (long long)(3.0 * video_frame_rate);
    
    long long ret = ff;

    if (livetv || (watchingrecording && nvr))
    {
        long long behind = nvr->GetFramesWritten() - framesPlayed;
	if (behind < maxtime) // if we're close, do nothing
	    ret = 0;
	else if (behind - fftime <= maxtime)
            ret = behind - maxtime;
    }
    else
    {
        if (totalFrames > 0)
        {
            long long behind = totalFrames - framesPlayed;
            if (behind < maxtime)
                ret = 0;
            else if (behind - fftime <= maxtime)
                ret = behind - maxtime;
        }
    }

    return ret;
}

bool NuppelVideoPlayer::DoFastForward(void)
{
    long long number = fftime - 1;

    long long desiredFrame = framesPlayed + number;
    long long desiredKey = lastKey;

    while (desiredKey < desiredFrame)
    {
        desiredKey += keyframedist;
    }
    desiredKey -= keyframedist;

    int normalframes = desiredFrame - desiredKey;
    int fileend = 0;

    if (desiredKey == lastKey)
        normalframes = number;

    if (positionMap->find(desiredKey / keyframedist) != positionMap->end() &&
        desiredKey != lastKey)
    {
        lastKey = desiredKey;
        long long keyPos = (*positionMap)[lastKey / keyframedist];
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
                    if (!haspositionmap)
                        (*positionMap)[framesPlayed / keyframedist] = 
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

    if (!exactseeks)
        normalframes = 0;

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

    //if (audiofd)
    //    ioctl(audiofd, SNDCTL_DSP_RESET, NULL);

    pthread_mutex_unlock(&avsync_lock);
    pthread_mutex_unlock(&video_buflock);
    pthread_mutex_unlock(&audio_buflock);
}

bool NuppelVideoPlayer::EnableEdit(void)
{
    editmode = false;
    if (haspositionmap)
    {
        editmode = true;
        Pause();
        while (!GetPause())
            usleep(50);
        seekamount = keyframedist;
        seekamountpos = 2;

        dialogname = "";
        deleteMap.clear();
        UpdateEditSlider();
    }
    return editmode;   
}

void NuppelVideoPlayer::DoKeypress(int keypress)
{
    if (dialogname != "")
    {
        switch(keypress)
        {
            case wsUp: osd->DialogUp(dialogname); break;
            case wsDown: osd->DialogDown(dialogname); break;
            case ' ': case wsEnter: case wsReturn: 
            {
                osd->TurnDialogOff(dialogname); 
                HandleResponse();
                break;
            }
            default: break;
        }
        return;
    }

    bool exactstore = exactseeks;
    exactseeks = true;
    switch (keypress)
    {
        case ' ': case wsEnter: case wsReturn:
        {
            HandleSelect();
            break;
        }
        case wsLeft: case 'a': case 'A': 
        {
            if (seekamount > 0)
            {
                rewindtime = seekamount;
                while (rewindtime != 0)
                    usleep(50);
                UpdateEditSlider();
            }
            else
                HandleArbSeek(false);
            break;           
        }
        case wsRight: case 'd': case 'D':
        {
            if (seekamount > 0)
            {
                fftime = seekamount;
                while (fftime != 0)
                    usleep(50);
                UpdateEditSlider();
            }
            else
                HandleArbSeek(true);
            break;
        }
        case wsUp: 
        {
            UpdateSeekAmount(true);
            break;
        }
        case wsDown:
        {
            UpdateSeekAmount(false);
            break; 
        }
    }
    exactseeks = exactstore;
}

void NuppelVideoPlayer::UpdateSeekAmount(bool up)
{
    QRect rect;
    rect.setTop(video_height * 3 / 16);
    rect.setBottom(video_height * 5 / 16);
    rect.setLeft(video_width * 3 / 8);
    rect.setRight(video_width * 15 / 16);

    if (seekamountpos > 0 && !up)
        seekamountpos--;
    if (seekamountpos < 10 && up) 
        seekamountpos++;

    QString text = "";

    int fps = (int)ceil(video_frame_rate);

    switch (seekamountpos)
    {
        case 0: text = "cut point"; seekamount = -2; break;
        case 1: text = "keyframe"; seekamount = -1; break;
        case 2: text = "1 frame"; seekamount = 1; break;
        case 3: text = "0.5 seconds"; seekamount = fps / 2; break;
        case 4: text = "1 second"; seekamount = fps; break;
        case 5: text = "5 seconds"; seekamount = fps * 5; break;
        case 6: text = "20 seconds"; seekamount = fps * 20; break;
        case 7: text = "1 minute"; seekamount = fps * 60; break;
        case 8: text = "5 mintues"; seekamount = fps * 300; break;
        case 9: text = "10 minutes"; seekamount = fps * 600; break;
        default: text = "error"; seekamount = fps; break;
    }

    osd->ShowText("seek_desc", text, rect.left(), rect.top(), rect.width(),
                  rect.height(), 2);
}

void NuppelVideoPlayer::UpdateTimeDisplay(void)
{
    if (!timedisplay)
    {
        timedisplay = new OSDSet("edittime_display", true, video_width, 
                                 video_height, video_width / 640.0, 
                                 video_height / 480.0);

        TTFFont *font = osd->GetFont("channel_font");
        QRect rect;
        rect.setTop(video_height * 1 / 16);
        rect.setBottom(video_height * 2 / 8);
        rect.setLeft(video_width / 2 - 50);
        rect.setRight(video_width * 15 / 16);

        OSDTypeText *text = new OSDTypeText("timedisp", font, "", rect);
        timedisplay->AddType(text);

        osd->AddSet(timedisplay, "edittime_display");
    }

    if (!timedisplay)
        return;

    OSDTypeText *text = (OSDTypeText *)timedisplay->GetType("timedisp");
    if (text)
    {
        int hours = 0;
        int mins = 0;
        int secs = 0;
        int frames = 0;

        int fps = (int)ceil(video_frame_rate);

        hours = (framesPlayed / fps) / 60 / 60;
        mins = (framesPlayed / fps) / 60 - (hours * 60);
        secs = (framesPlayed / fps) - (mins * 60) - (hours * 60 * 60);
        frames = framesPlayed - ((secs * fps) + (mins * 60 * fps) + 
                                 (hours * 60 * 60 * fps));

        char timestr[128];
        sprintf(timestr, "%1d:%02d:%02d.%02d", hours, mins, secs, frames);
        text->SetText(timestr);

        osd->SetVisible(timedisplay, -1);
    }
}

void NuppelVideoPlayer::HandleSelect(void)
{
    bool deletepoint = false;
    QMap<long long, int>::Iterator i;
    int direction = 0;

    for (i = deleteMap.begin(); i != deleteMap.end(); ++i)
    {
        if (llabs(framesPlayed - i.key()) < (int)ceil(20 * video_frame_rate))
        {
            deletepoint = true;
            deleteframe = i.key();
            direction = i.data();
            break;
        }
    }

    if (deletepoint)
    {
        QString message = "You are close to an existing cut point.  Would you "
                          "like to:";
        QString option1 = "Delete this cut point";
        QString option2 = "Move this cut point to the current position";
        QString option3 = "Filp directions - delete to the ";
        if (direction == 0)
            option3 += "right";
        else
            option3 += "left";
        QString option4 = "Cancel";

        dialogname = "deletemark";
        dialogtype = 0;
        osd->NewDialogBox(dialogname, message, option1, option2, option3, 
                          option4, -1);
    }
    else
    {
        QString message = "Insert a new cut point?";
        QString option1 = "Delete before this frame";
        QString option2 = "Delete after this frame";
        QString option3 = "Cancel";

        dialogname = "addmark";
        dialogtype = 1;
        osd->NewDialogBox(dialogname, message, option1, option2, option3,
                          "", -1);
    }
}

void NuppelVideoPlayer::HandleResponse(void)
{
    int result = osd->GetDialogResponse(dialogname);
    dialogname = "";

    if (dialogtype == 0)
    {
        int type = deleteMap[deleteframe];
        if (result == 1)
            DeleteMark(deleteframe);
        if (result == 2)
        {
            DeleteMark(deleteframe);
            AddMark(framesPlayed, type);
        }
        else if (result == 3)
        {
            DeleteMark(deleteframe);
            AddMark(deleteframe, 1 - type);
        }
    }
    else if (dialogtype == 1)
    {
        if (result == 1)
            AddMark(framesPlayed, 0);
        else if (result == 2)
            AddMark(framesPlayed, 1);
    }
}

void NuppelVideoPlayer::UpdateEditSlider(void)
{
    osd->DoEditSlider(deleteMap, framesPlayed, totalFrames);
}

void NuppelVideoPlayer::AddMark(long long frames, int type)
{
    deleteMap[frames] = type;
    osd->ShowEditArrow(frames, totalFrames, type);
    UpdateEditSlider();
}

void NuppelVideoPlayer::DeleteMark(long long frames)
{
    deleteMap.remove(frames);
    osd->HideEditArrow(frames);
    UpdateEditSlider();
}

void NuppelVideoPlayer::HandleArbSeek(bool right)
{
    if (seekamount == -2)
    {
        QMap<long long, int>::Iterator i = deleteMap.begin();
        long long framenum = -1;
        if (right)
        {
            for (; i != deleteMap.end(); ++i)
            {
                if (i.key() > framesPlayed)
                {
                    framenum = i.key();
                    break;
                }
            }
            if (framenum == -1)
                framenum = totalFrames;

            fftime = framenum - framesPlayed;
            while (fftime > 0)
                usleep(50);
        }
        else
        {
            for (; i != deleteMap.end(); ++i)
            {
                if (i.key() >= framesPlayed)
                    break;
                framenum = i.key();
            }
            if (framenum == -1)
                framenum = 0;
            
            rewindtime = framesPlayed - framenum;
            while (rewindtime > 0)
                usleep(50);
        }
    }
    else
    {
        if (right)
        {
            exactseeks = false;
            fftime = keyframedist * 3 / 2;
            while (fftime > 0)
                usleep(50);
        }
        else
        {
            exactseeks = false;
            rewindtime = 2;
            while (rewindtime > 0)
                usleep(50);
        }
    }

    UpdateEditSlider();
}

char *NuppelVideoPlayer::GetScreenGrab(int secondsin, int &bufflen, int &vw,
                                       int &vh)
{
    InitSubs();
    OpenFile(false);

    int number = (int)(secondsin * video_frame_rate);

    long long desiredFrame = number;
    long long desiredKey = 0;

    while (desiredKey < desiredFrame)
    {
        desiredKey += keyframedist;
    }
    desiredKey -= keyframedist;

    int normalframes = number - desiredKey;

    if (normalframes < 3)
        normalframes = 3;

    int fileend = 0;

    buf = new unsigned char[video_width * video_height * 3 / 2];
    strm = new unsigned char[video_width * video_height * 2];

    long long int maxRead = 200000000;

    unsigned char *frame = NULL;

    while (lastKey < desiredKey && !fileend)
    {
        fileend = (FRAMEHEADERSIZE != ringBuffer->Read(&frameheader,
                                                   FRAMEHEADERSIZE));

        if (frameheader.frametype == 'S')
        {
            if (frameheader.comptype == 'V')
            {
                (*positionMap)[framesPlayed / keyframedist] =
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

        if (ringBuffer->GetReadPosition() > maxRead)
            break;
    }

    int decodedframes = 0;
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
            decodedframes++;
            frame = DecodeFrame(&frameheader, strm);

            if (ringBuffer->GetReadPosition() > maxRead && decodedframes > 2)
                break;
        }
    }

    if (!frame)
    {
        bufflen = 0;
        vw = vh = 0;
        return NULL;
    }

    linearBlendYUV420(frame, video_width, video_height);

    bufflen = video_width * video_height * 4;
    unsigned char *outputbuf = new unsigned char[bufflen];

    yuv2rgb_fun convert = yuv2rgb_init_mmx(32, MODE_RGB);
    
    convert(outputbuf, frame, frame + (video_width * video_height), 
            frame + (video_width * video_height * 5 / 4), video_width,
            video_height);

    vw = video_width;
    vh = video_height;

    return (char *)outputbuf;
}

void NuppelVideoPlayer::ReencodeFile(char *inputname, char *outputname)
{ 
    inputname = inputname;
    outputname = outputname;
/*
    filename = inputname;
     
    InitSubs();
    OpenFile(false);

    mpa_codec = avcodec_find_encoder(CODEC_ID_MPEG4);

    if (!mpa_codec)
    {
        cout << "error finding codec\n";
        return;
    }
    mpa_ctx->pix_fmt = PIX_FMT_YUV420P;

    mpa_picture.linesize[0] = video_width;
    mpa_picture.linesize[1] = video_width / 2;
    mpa_picture.linesize[2] = video_width / 2;

    mpa_ctx->width = video_width;
    mpa_ctx->height = video_height;
 
    mpa_ctx->frame_rate = (int)(video_frame_rate * FRAME_RATE_BASE);
    mpa_ctx.bit_rate = 1800 * 1000;
    mpa_ctx.bit_rate_tolerance = 1024 * 8 * 1000;
    mpa_ctx.qmin = 2;
    mpa_ctx.qmax = 15;
    mpa_ctx.max_qdiff = 3;
    mpa_ctx.qcompress = 0.5;
    mpa_ctx.qblur = 0.5;
    mpa_ctx.max_b_frames = 3;
    mpa_ctx.b_quant_factor = 2.0;
    mpa_ctx.rc_strategy = 2;
    mpa_ctx.b_frame_strategy = 0;
    mpa_ctx.gop_size = 30; 
    mpa_ctx.flags = CODEC_FLAG_HQ; // | CODEC_FLAG_TYPE; 
    mpa_ctx.me_method = 5;
    mpa_ctx.key_frame = -1; 

    if (avcodec_open(&mpa_ctx, mpa_codec) < 0)
    {
        cerr << "Unable to open FFMPEG/MPEG4 codex\n" << endl;
        return;
    }

    FILE *out = fopen(outputname, "w+");

    int fileend = 0;

    buf = new unsigned char[video_width * video_height * 3 / 2];
    strm = new unsigned char[video_width * video_height * 2];

    unsigned char *frame = NULL;

    static unsigned long int tbls[128];

    fwrite(&fileheader, FILEHEADERSIZE, 1, out);
    frameheader.frametype = 'D';
    frameheader.comptype = 'R';
    frameheader.packetlength = sizeof(tbls);

    fwrite(&frameheader, FRAMEHEADERSIZE, 1, out);
    fwrite(tbls, sizeof(tbls), 1, out);

    int outsize;
    unsigned char *outbuffer = new unsigned char[1000 * 1000 * 3];
    bool nextiskey = true;

    while (!fileend)
    {
        fileend = (FRAMEHEADERSIZE != ringBuffer->Read(&frameheader,
                                                       FRAMEHEADERSIZE));

        if (fileend)
            continue;
        if (frameheader.frametype == 'R')
        {
            fwrite("RTjjjjjjjjjjjjjjjjjjjjjjjj", FRAMEHEADERSIZE, 1, out);
            continue;
        }
        else if (frameheader.frametype == 'S')
        {
            fwrite(&frameheader, FRAMEHEADERSIZE, 1, out);
            nextiskey = true;
            continue;
        }

        fileend = (ringBuffer->Read(strm, frameheader.packetlength) !=
                                    frameheader.packetlength);

        if (frameheader.frametype == 'V')
        {
            framesPlayed++;
            frame = DecodeFrame(&frameheader, strm);

            mpa_picture.data[0] = frame;
            mpa_picture.data[1] = frame + (video_width * video_height);
            mpa_picture.data[2] = frame + (video_width * video_height * 5 / 4);
        
            mpa_ctx.key_frame = nextiskey;

            outsize = avcodec_encode_video(&mpa_ctx, outbuffer, 
                                           1000 * 1000 * 3, &mpa_picture);

            frameheader.comptype = '3' + CODEC_ID_MPEG4;
            frameheader.packetlength = outsize;

            fwrite(&frameheader, FRAMEHEADERSIZE, 1, out);
            fwrite(outbuffer, frameheader.packetlength, 1, out);
            cout << framesPlayed << endl; 
            nextiskey = false;
        }
        else
        {
            fwrite(&frameheader, FRAMEHEADERSIZE, 1, out);
            fwrite(strm, frameheader.packetlength, 1, out);
        }
    }

    delete [] outbuffer;

    fclose(out);
    avcodec_close(&mpa_ctx);
*/
}
