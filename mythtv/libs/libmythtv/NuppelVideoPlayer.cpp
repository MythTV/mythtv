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
#include <qsqldatabase.h>
#include <qurl.h>

#include <iostream>
using namespace std;

#include "NuppelVideoPlayer.h"
#include "NuppelVideoRecorder.h"
#include "minilzo.h"
#include "XJ.h"
#include "yuv2rgb.h"
#include "osdtypes.h"
#include "remoteutil.h"
#include "programinfo.h"
#include "mythcontext.h"

extern "C" {
#include "../libavcodec/mythav.h"
#include "../libvbitext/vbi.h"
}

#include "remoteencoder.h"

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

NuppelVideoPlayer::NuppelVideoPlayer(QSqlDatabase *ldb,
                                     ProgramInfo *info)
{
    m_db = ldb;
    m_playbackinfo = NULL;

    if (info)
        m_playbackinfo = new ProgramInfo(*info);

    playing = false;
    audiofd = -1;
    filename = "output.nuv";

    vbimode = ' ';
    vbipagenr = 0x08880000;
    video_height = 0;
    video_width = 0;
    video_size = 0;
    text_size = 0;

    bookmarkseek = 0;

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
    wtxt = rtxt = 0;

    embedid = 0;
    embx = emby = embw = embh = -1;

    nvr_enc = NULL;

    paused = 0;

    ffmpeg_extradata = NULL;
    ffmpeg_extradatasize = 0;

    audiodevice = "/dev/dsp";

    ringBuffer = NULL;
    weMadeBuffer = false;
    osd = NULL;

    audio_bits = 16;
    audio_channels = 2;
    audio_samplerate = 44100;
    audio_bytes_per_sample = audio_channels * audio_bits / 8;
    audio_buffer_unused = 0;

    editmode = false;
    advancevideo = resetvideo = advancedecoder = false;

    totalLength = 0;
    totalFrames = 0;

    avcodec_init();
    avcodec_register_all();

    mpa_codec = 0;
    mpa_ctx = NULL;
    mpa_pic = NULL;
    osdtheme = "none";
    directrendering = false;

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

    autocommercialskip = 0;
    commercialskipmethod = COMMERCIAL_SKIP_BLANKS;

    eventvalid = false;

    timedisplay = NULL;
    seekamount = 30;
    seekamountpos = 4;
    deleteframe = 0;
    hasdeletetable = false;

    dialogname = "";

    for (int i = 0; i <= MAXVBUFFER; i++)
        vbuffer[i] = NULL;

    for (int i = 0; i <= MAXTBUFFER; i++)
        tbuffer[i] = NULL;

    own_vidbufs = false;

    killvideo = false;
    pausevideo = false;

    killaudio = false;
    pauseaudio = false;

    cc = false;

    numbadioctls = 0;
    numlowbuffer = 0;

    pthread_mutex_init(&eventLock, NULL);
}

NuppelVideoPlayer::~NuppelVideoPlayer(void)
{
    if (m_playbackinfo)
        delete m_playbackinfo;
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
    if (ffmpeg_extradata)
        delete [] ffmpeg_extradata;

    if (own_vidbufs)
    {
        for (int i = 0; i <= MAXVBUFFER; i++)
        {
            if (vbuffer[i])
                delete [] vbuffer[i];
        }
    }

    for (int i = 0; i < MAXTBUFFER; i++)
    {
        if (tbuffer[i])
            delete [] tbuffer[i];
    }

    CloseAVCodec();

    if (videoFilters.size() > 0)
    {
        filters_cleanup(&videoFilters[0], videoFilters.size());
        videoFilters.clear();
    }
}

void NuppelVideoPlayer::Pause(void)
{
    actuallypaused = false;

    PauseAudio();
    PauseVideo();
    paused = true;
}

void NuppelVideoPlayer::Unpause(void)
{
    paused = false;
    UnpauseVideo();
    UnpauseAudio();
}

bool NuppelVideoPlayer::GetPause(void)
{
    if (disableaudio)
    {
        return (actuallypaused && GetVideoPause());
    }
    return (actuallypaused && GetAudioPause() && GetVideoPause());
}

inline bool NuppelVideoPlayer::GetVideoPause(void)
{
    return video_actually_paused;
}

void NuppelVideoPlayer::PauseVideo(void)
{
    video_actually_paused = false;
    pausevideo = true;
}

void NuppelVideoPlayer::UnpauseVideo(void)
{
    pausevideo = false;
}

inline bool NuppelVideoPlayer::GetAudioPause(void)
{
    return audio_actually_paused;
}

void NuppelVideoPlayer::PauseAudio(void)
{
    audio_actually_paused = false;
    pauseaudio = true;
}

void NuppelVideoPlayer::UnpauseAudio(void)
{
    pauseaudio = false;
}

void NuppelVideoPlayer::InitVideo(void)
{
    char name[] = "MythTV"; 
    videoOutput = new XvVideoOutput();
    videoOutput->Init(video_width, video_height, name, name, MAXVBUFFER + 1, 
                      vbuffer, embedid);
    if (embedid > 0)
    {
        videoOutput->EmbedInWidget(embedid, embx, emby, embw, embh);
    }
}

void NuppelVideoPlayer::InitSound(void)
{
    int caps;

    if (usingextradata)
    {
        audio_bits = extradata.audio_bits_per_sample;
        audio_channels = extradata.audio_channels;
        audio_samplerate = extradata.audio_sample_rate;
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

    if (ioctl(audiofd, SNDCTL_DSP_SAMPLESIZE, &audio_bits) < 0 ||
        ioctl(audiofd, SNDCTL_DSP_CHANNELS, &audio_channels) < 0 ||
        ioctl(audiofd, SNDCTL_DSP_SPEED, &audio_samplerate) < 0)
    {
        cerr << "player: " << audiodevice 
             << ": error setting audio output device to "
             << audio_samplerate << "kHz/" 
             << audio_bits << "bits/"
             << audio_channels << "channel\n";
        close(audiofd);
        audiofd = -1;
        return;
    }

    audio_bytes_per_sample = audio_channels * audio_bits / 8;

    audio_buf_info info;
    ioctl(audiofd, SNDCTL_DSP_GETOSPACE, &info);

    audio_buffer_unused = info.bytes - audio_bytes_per_sample * 
                          audio_samplerate / 10;
    if(audio_buffer_unused < 0)
       audio_buffer_unused = 0;

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
            cerr << "Warning: Old player ringbuf creation\n";
            ringBuffer = new RingBuffer(NULL, filename, false);
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

    ringBuffer->Start();
    startpos = ringBuffer->GetTotalReadPosition();
    
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

        startpos = ringBuffer->GetTotalReadPosition();
 
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
        video_size = video_height * video_width * 3 / 2;
        text_size = 8 * (sizeof(teletextsubtitle) + VT_WIDTH);
        eof = 0;
    }

    keyframedist = fileheader.keyframedist;

    space = new char[video_size];

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

    if (frameheader.comptype == 'F')
    {
        ffmpeg_extradatasize = frameheader.packetlength;
        if (ffmpeg_extradatasize > 0)
        {
            ffmpeg_extradata = new char[ffmpeg_extradatasize];
            if (frameheader.packetlength != ringBuffer->Read(ffmpeg_extradata,
                                                     frameheader.packetlength))
            {
                cerr << "File not big enough for first frame data\n";
                delete [] ffmpeg_extradata;
                ffmpeg_extradata = NULL;
            }
        }
    }
    else
    {
        if (frameheader.packetlength != ringBuffer->Read(space, 
                                                     frameheader.packetlength))
        {
            cerr << "File not big enough for first frame data\n";
            return -1;
        }
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

    startpos = ringBuffer->GetTotalReadPosition();

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
        long long currentpos = ringBuffer->GetTotalReadPosition();
        struct rtframeheader seek_frameheader;

        int seekret = ringBuffer->Seek(extradata.seektable_offset, SEEK_SET);
        if (seekret == -1) {
          perror("seek");
          // XXX, will hopefully blow up soon enough
        }

        ringBuffer->Read(&seek_frameheader, FRAMEHEADERSIZE);
    
        if (seek_frameheader.frametype != 'Q')
        {
          cerr << "Invalid seektable (frametype " 
               << (int)seek_frameheader.frametype << ")\n";
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
            else
                cerr << "0 length seek table\n";
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

        startpos = ringBuffer->GetTotalReadPosition();

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
    {
        effdsp = extradata.audio_sample_rate;
        audio_channels = extradata.audio_channels;
        audio_bits = extradata.audio_bits_per_sample;
        audio_bytes_per_sample = audio_channels * audio_bits / 8;
        foundit = 1;
    }

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

        long long startpos2 = ringBuffer->GetTotalReadPosition();

        foundit = (FRAMEHEADERSIZE != ringBuffer->Read(&frameheader, 
                                                       FRAMEHEADERSIZE));

        while (frameheader.frametype != 'A' && frameheader.frametype != 'V' &&
               frameheader.frametype != 'S' && frameheader.frametype != 'T' &&
               frameheader.frametype != 'R' && frameheader.frametype != 'X')
        {      
            ringBuffer->Seek(startpos2, SEEK_SET);
        
            char dummychar;
            ringBuffer->Read(&dummychar, 1);
        
            startpos2 = ringBuffer->GetTotalReadPosition();
        
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
        LoadCutList();
    
        if (!deleteMap.isEmpty())
        {
            hasdeletetable = true;
            deleteIter = deleteMap.begin();
        }

	bookmarkseek = GetBookmark();
    }

    return 0;
}

int get_buffer(struct AVCodecContext *c, AVFrame *pic)
{
    NuppelVideoPlayer *nvp = (NuppelVideoPlayer *)(c->opaque);

    int width = c->width;
    int height = c->height;

    pic->data[0] = nvp->directbuf;
    pic->data[1] = pic->data[0] + width * height;
    pic->data[2] = pic->data[1] + width * height / 4;

    pic->linesize[0] = width;
    pic->linesize[1] = width / 2;
    pic->linesize[2] = width / 2;

    pic->opaque = nvp;
    pic->type = FF_BUFFER_TYPE_USER;

    pic->age = 256 * 256 * 256 * 64;

    return 1;
}

void release_buffer(struct AVCodecContext *c, AVFrame *pic)
{
    (void)c;
    assert(pic->type == FF_BUFFER_TYPE_USER);

    int i;
    for (i = 0; i < 4; i++)
        pic->data[i] = NULL;
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
            case MKTAG('H', 'F', 'Y', 'U'): codec = CODEC_ID_HUFFYUV; break;
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

    if (mpa_codec->capabilities & CODEC_CAP_DR1)
        directrendering = true;

    if (mpa_ctx)
        free(mpa_ctx);

    if (mpa_pic)
        free(mpa_pic);

    mpa_pic = avcodec_alloc_frame();

    mpa_ctx = avcodec_alloc_context();

    mpa_ctx->codec_id = (enum CodecID)codec;
    mpa_ctx->width = video_width;
    mpa_ctx->height = video_height;
    mpa_ctx->error_resilience = 2;
    mpa_ctx->bits_per_sample = 12;   

    if (directrendering) 
    {
        mpa_ctx->flags |= CODEC_FLAG_EMU_EDGE; 
        mpa_ctx->draw_horiz_band = NULL;
        mpa_ctx->get_buffer = get_buffer;
        mpa_ctx->release_buffer = release_buffer;
        mpa_ctx->opaque = (void *)this;
    }

    if (ffmpeg_extradatasize > 0)
    {
        mpa_ctx->flags |= CODEC_FLAG_EXTERN_HUFF;
        mpa_ctx->extradata = ffmpeg_extradata;
        mpa_ctx->extradata_size = ffmpeg_extradatasize;
    }

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
    if (mpa_pic)
        free(mpa_pic);
    mpa_ctx = NULL;
    mpa_pic = NULL;
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

bool NuppelVideoPlayer::DecodeFrame(struct rtframeheader *frameheader,
                                    unsigned char *lstrm, 
                                    unsigned char *outbuf)
{
    int r;
    unsigned int out_len;
    int compoff = 0;

    if (!buf2)
    {
        buf2 = new unsigned char[video_size];
        planes[0] = buf;
        planes[1] = planes[0] + video_width * video_height;
        planes[2] = planes[1] + (video_width * video_height) / 4;
    }

    if (frameheader->comptype == 'N') {
        memset(outbuf, 0, video_width * video_height);
        memset(outbuf + video_width * video_height, 127,
               (video_width * video_height)/2);
        return true;
    }

    if (frameheader->comptype == 'L') {
        switch(lastct) {
            case '0': case '3': 
                memcpy(outbuf, buf2, video_size);
                break;
            case '1': case '2': 
            default:
                memcpy(outbuf, buf, video_size);
                break;
        }
        return true;
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
        memcpy(outbuf, lstrm, video_size);
        return true;
    }

    if (frameheader->comptype == '3')
    {
        memcpy(outbuf, buf2, video_size);
        return true;
    }

    if (frameheader->comptype == '2' || frameheader->comptype == '1')
    {
        if (compoff) 
            rtjd->Decompress((int8_t*)lstrm, planes);
        else
            rtjd->Decompress((int8_t*)buf2, planes);

        memcpy(outbuf, buf, video_size);
    }
    else
    {
        if (!mpa_codec)
            InitAVCodec(frameheader->comptype - '3');

        int gotpicture = 0;
#ifdef EXTRA_LOCKING
        pthread_mutex_lock(&avcodeclock);
#endif
        // if directrendering, writes into buf
        directbuf = outbuf;
        int ret = avcodec_decode_video(mpa_ctx, mpa_pic, &gotpicture,
                                       lstrm, frameheader->packetlength);
#ifdef EXTRA_LOCKING
        pthread_mutex_unlock(&avcodeclock);
#endif
        if (ret < 0)
        {
            cout << "decoding error\n";
            return false;
        }

        if (!gotpicture)
        {
            return false;
        }

        if (directrendering)
            return true;

        avpicture_fill(&tmppicture, outbuf, PIX_FMT_YUV420P, video_width,
                       video_height);
        for (int i = 0; i < 4; i++)
        {
            mpa_pic_tmp.data[i] = mpa_pic->data[i];
            mpa_pic_tmp.linesize[i] = mpa_pic->linesize[i];
        }

        img_convert(&tmppicture, PIX_FMT_YUV420P, &mpa_pic_tmp,
                    mpa_ctx->pix_fmt, video_width, video_height);
    }

    return true;
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

int NuppelVideoPlayer::tbuffer_numvalid(void)
{
    /* thread safe, returns number of valid slots in the text buffer */
    int ret;
    pthread_mutex_lock(&text_buflock);

    if (wtxt >= rtxt)
        ret = wtxt - rtxt;
    else
        ret = MAXTBUFFER - (rtxt - wtxt);

    pthread_mutex_unlock(&text_buflock);
    return ret;
}

int NuppelVideoPlayer::tbuffer_numfree(void)
{
    return MAXTBUFFER - tbuffer_numvalid() - 1;
    /* There's one wasted slot, because the case when rtxt = wtxt is
       interpreted as an empty buffer. So the fullest the buffer can be is
       MAXTBUFFER - 1. */
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

    int soundcard_buffer = 0;
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
               
    audiotime = audbuf_timecode - (int)(totalbuffer * 100000.0 /
                                        (audio_bytes_per_sample * effdsp));
 
    gettimeofday(&audiotime_updated, NULL);

    pthread_mutex_unlock(&avsync_lock);
    pthread_mutex_unlock(&audio_buflock);
}

void NuppelVideoPlayer::GetFrame(int onlyvideo, bool unsafe)
{
    int gotvideo = 0;
    int seeked = 0;
    bool ret = false;
    int seeklen = 0;

    if (weseeked)
    {
        seeked = 1;
        weseeked = 0;
    }

    while (!gotvideo)
    {
	long long currentposition = ringBuffer->GetTotalReadPosition();

        if ((ringBuffer->Read(&frameheader, FRAMEHEADERSIZE) != FRAMEHEADERSIZE)
            || (frameheader.frametype == 'Q'))
        {
            eof = 1;
            return;
        }

        while (frameheader.frametype != 'A' && frameheader.frametype != 'V' &&
               frameheader.frametype != 'S' && frameheader.frametype != 'T' &&
               frameheader.frametype != 'R' && frameheader.frametype != 'X')
        {
            ringBuffer->Seek((long long)seeklen-FRAMEHEADERSIZE, SEEK_CUR);

            if (ringBuffer->Read(&frameheader, FRAMEHEADERSIZE)
                != FRAMEHEADERSIZE)
            {
                eof = 1;
                return;
            }
            seeklen = 1;
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
	  
        if (frameheader.packetlength > 0) 
        {
            if (ringBuffer->Read(strm, frameheader.packetlength) != 
                frameheader.packetlength) 
            {
                eof = 1;
                return;
            }
        }
        else
            continue;

        if (frameheader.frametype == 'V') 
        {
            while (vbuffer_numfree() == 0 && !unsafe)
            {
                //cout << "waiting for video buffer to drain.\n";
                prebuffering = false;
                usleep(2000);
            }

            pthread_mutex_lock(&video_buflock);

            ret = DecodeFrame(&frameheader, strm, vbuffer[wpos]);
            if (!ret)
            {
                pthread_mutex_unlock(&video_buflock);
                continue;
            }

            timecodes[wpos] = frameheader.timecode;

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
		int new_audio_samps = 0;
                int lameret = 0;
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

                        if (lameret * audio_bytes_per_sample > afree)
                        {
                            lameret = afree / audio_bytes_per_sample;
                            cout << "Audio buffer overflow, audio data lost!\n";
                        }

                        short int *saudbuffer = (short int *)audiobuffer;

                        for (itemp = 0; itemp < lameret; itemp++)
                        {
                            saudbuffer[waud / 2] = pcmlbuffer[itemp];
                            if (audio_channels == 2)
                                saudbuffer[waud / 2 + 1] = pcmrbuffer[itemp];

                            waud += audio_bytes_per_sample;
                            if (waud >= AUDBUFSIZE)
                                waud -= AUDBUFSIZE;
                        }
			new_audio_samps += lameret;
                    }
                    else if (lameret < 0)
                    {
                        cout << "lame decode error, exiting\n";
                        exit(-1);
                    }
                    packetlen = 0;
                } while (lameret > 0);

		/* we want the time at the end -- but the file format stores
		   time at the start of the chunk. */
                audbuf_timecode = frameheader.timecode + 
		    (int)( (new_audio_samps*100000.0) / effdsp );

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
                }
                else
                {
                    memcpy(audiobuffer + waud, strm, len);
                }

		waud = (waud + len) % AUDBUFSIZE;

                lastaudiolen = audiolen(false);

		/* we want the time at the end -- but the file format stores
		   time at the start of the chunk. */
                audbuf_timecode = frameheader.timecode + 
		    (int)( (len*100000.0) / (audio_bytes_per_sample*effdsp) ); 

                pthread_mutex_unlock(&audio_buflock); // end critical section
            }
        }
        if (frameheader.frametype == 'T')
        {
            if (!tbuffer_numfree())
            {
                cerr << "text buffer overflow\n";
            }
            else
            {
                memcpy(tbuffer[wtxt], strm, frameheader.packetlength);
                txttimecodes[wtxt] = frameheader.timecode;
                txttype[wtxt] = frameheader.comptype;

                pthread_mutex_lock(&text_buflock);
                wtxt = (wtxt+1) % MAXTBUFFER;
                pthread_mutex_unlock(&text_buflock);
            }
        }
    }
}

void NuppelVideoPlayer::ReduceJitter(struct timeval *nexttrigger)
{
  /* Comments and debug variables will be trimmed later */

  /* usleep() will relinqusish the CPU. The scheduler won't start
     a new time slice until 0-10000usec (or 10000-20000usec)
     later. Without a realtime scheduling policy, there will
     always be jitter. This strives to smooth the time spacing
     between frames. */

  /* Half the frames are delayed until half as late as the
     previous frame. */

    static int cheat = 5000;
    static int fudge = 0;

    struct timeval now; 
    int delay, miss;
    int cnt = 0;

    cheat += 100;

    gettimeofday(&now, NULL);

    delay = (nexttrigger->tv_sec - now.tv_sec) * 1000000 +
            (nexttrigger->tv_usec - now.tv_usec); // uSecs

    /* The usleep() is shortened by "cheat" so that this process
       gets the CPU early for about half the frames. Also, late
       frames won't be as late. */

    if (delay > (cheat - fudge))
        usleep(delay - (cheat - fudge));

    /* if late, draw the frame ASAP. If a little early, the frame
       is due during this time slice so hold the CPU until half as
       late as the previous frame (fudge) */

    while (delay + fudge > 0)
    {
        gettimeofday(&now, NULL);
        
        delay = (nexttrigger->tv_sec - now.tv_sec) * 1000000 +
                (nexttrigger->tv_usec - now.tv_usec); // uSecs

        if (cnt == 0)
            miss = delay + fudge;

        cnt++;
    }

    // cerr << cheat - fudge << '\t' << miss << '\t';
    // cerr << 0 - fudge << '\t' << delay << endl;
    
    fudge = abs(delay / 2);

    if (cnt > 1)
        cheat -= 200;
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

void NuppelVideoPlayer::EmbedInWidget(unsigned long wid, int x, int y, int w, 
                                      int h)
{
    if (videoOutput)
    {
        videoOutput->EmbedInWidget(wid, x, y, w, h);
    }
    else
    {
        embedid = wid;
        embx = x; 
        emby = y; 
        embw = w; 
        embh = h;
    }
}

void NuppelVideoPlayer::StopEmbedding(void)
{
    if (videoOutput)
    {
        videoOutput->StopEmbedding();
    }
}

void NuppelVideoPlayer::ToggleFullScreen(void)
{
    if (videoOutput)
        videoOutput->ToggleFullScreen();
}

void NuppelVideoPlayer::ToggleCC(void)
{
    if (cc)
    {
        printf("turn tt off\n");
        cc = false;
    }
    else
    {
        printf("turn tt on\n");
        cc = true;
    }
}

void NuppelVideoPlayer::ShowText(void)
{
    // check if subtitles need to be updated on the OSD
    if (tbuffer_numvalid() && txttimecodes[rtxt] && 
        (txttimecodes[rtxt] <= timecodes[rpos]))
    {
        if (txttype[rtxt] == 'T')
        {
            // display full page of teletext
            //
            // all formatting is always defined in the page itself,
            // if scrolling is needed for live closed captions this
            // is handled by the broadcaster:
            // the pages are then very often transmitted (sometimes as often as
            // every 2 frames) with small differences between them
            unsigned char *inpos = tbuffer[rtxt];
            int pagenr;
            memcpy(&pagenr, inpos, sizeof(int));
            inpos += sizeof(int);

            if (pagenr == vbipagenr)
            {
                // show teletext subtitles
                osd->ClearAllCCText();
                while (*inpos)
                {
                    struct teletextsubtitle st;
                    memcpy(&st, inpos, sizeof(st));
                    inpos += sizeof(st);
                    printf("%s\n", (char*) inpos);
                    QString s((const char*) inpos);
                    osd->AddCCText(s, st.row, st.col, 1, true);
                    inpos += st.len;
                }
            }
        }
        else if (txttype[rtxt] == 'C')
        {
            // TODO: show US close caption
            //
            // as far as I understand this is a stream of characters,
            // which upon display is broken into words and lines and scrolled,
            // vbipagenr could be used to select the streamnr like
            // the pagenr for teletext
        }

        /* update rtxt */
        pthread_mutex_lock(&text_buflock);
        if (rtxt != wtxt) // if a seek occurred, rtxt == wtxt, in this case do
                          // nothing
            rtxt = (rtxt + 1) % MAXTBUFFER;
        pthread_mutex_unlock(&text_buflock);
    }
}

void NuppelVideoPlayer::OutputVideoLoop(void)
{
    int laudiotime;
    int delay, avsync_delay;

    struct timeval nexttrigger, now; 
  
    //Jitterometer *output_jmeter = new Jitterometer("video_output", 100);

    bool reducejitter = gContext->GetNumSetting("ReduceJitter", 0);

    if (!disablevideo)
    {
        eventvalid = true;
    }

    Frame frame;
    
    frame.codec = CODEC_YUV;
    frame.width = video_width;
    frame.height = video_height;
    frame.bpp = -1;
    frame.frameNumber = framesPlayed;

    int pause_rpos = 0;
    unsigned char *pause_buf = new unsigned char[video_size];

    gettimeofday(&nexttrigger, NULL);

    while (!eof && !killvideo)
    {
        if (needsetpipplayer)
        {
            pipplayer = setpipplayer;
            needsetpipplayer = false;
        }

        if (pausevideo)
        {
            if (!video_actually_paused)
            {
                pause_rpos = rpos;
                memcpy(pause_buf, vbuffer[pause_rpos], video_size);
            }

            if (advancevideo)
            {
                rpos = (rpos + 1) % MAXVBUFFER;
                pause_rpos = rpos;
                memcpy(pause_buf, vbuffer[pause_rpos], video_size);
                advancevideo = false;
            }
            if (resetvideo)
            {
                resetvideo = false;
                pause_rpos = 0;
                memcpy(pause_buf, vbuffer[pause_rpos], video_size);
            }

            video_actually_paused = true;

            //printf("video waiting for unpause\n");
            usleep(5000);

            if (!disablevideo)
            {
                memcpy(vbuffer[MAXVBUFFER], pause_buf, video_size);
                frame.buf = vbuffer[MAXVBUFFER];
                if (videoFilters.size() > 0)
                    process_video_filters(&frame, &videoFilters[0],
                                          videoFilters.size());
                if (pipplayer)
                    ShowPip(vbuffer[MAXVBUFFER]);
                osd->Display(vbuffer[MAXVBUFFER]);
                videoOutput->Show(vbuffer[MAXVBUFFER], video_width, 
                                  video_height);
                ResetNexttrigger(&nexttrigger);
            }
            continue;
        }
	video_actually_paused = false;

        if (prebuffering)
        {
            //printf("prebuffering...\n");
            usleep(200);
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
            frame.buf = vbuffer[rpos];
            if (videoFilters.size() > 0)
                process_video_filters(&frame, &videoFilters[0],
                                      videoFilters.size());

            if (pipplayer)
                ShowPip(vbuffer[rpos]);

            ShowText();

            osd->Display(vbuffer[rpos]);
        }

        // calculate 'delay', that we need to get from 'now' to 'nexttrigger'
        gettimeofday(&now, NULL);

        delay = (nexttrigger.tv_sec - now.tv_sec) * 1000000 +
                (nexttrigger.tv_usec - now.tv_usec); // uSecs

        /* If delay is sometwhat more than a frame or < 0ms, 
           we clip it to these amounts and reset nexttrigger */
        if ( delay > 40000 )
        {
            // cerr << "Delaying to next trigger: " << delay << endl;
            delay = 40000;
            usleep(delay);

            gettimeofday(&nexttrigger, NULL);
        }
        else if (delay <= 0)
        {
            // cerr << "clipped negative delay " << delay << endl;
            gettimeofday(&nexttrigger, NULL);
        }
        else
        {
            if (!disablevideo && reducejitter)
                ReduceJitter(&nexttrigger);
            else
                usleep(delay);
        }

        if (!disablevideo)
        {
            videoOutput->Show(vbuffer[rpos], video_width, video_height);
        }
        /* a/v sync assumes that when 'Show' returns, that is the instant
           the frame has become visible on screen */

        //output_jmeter->RecordCycleTime();

	/* The value of nexttrigger is perfect -- we calculated it to
	   be exactly one frame time after the previous frame,
	   plus just enough feedback to stay synchronized with audio. */

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
		/* if we were perfect, (timecodes[rpos] - frame_time) 
		   and laudiotime would match, and this adjustment 
		   wouldn't do anything */

		/* The time at the start of this frame (ie, now) is
		   given by timecodes[rpos] */

		avsync_delay = (timecodes[rpos] - laudiotime) * 1000; // uSecs

		if(avsync_delay < -100000 || avsync_delay > 100000)
		    nexttrigger.tv_usec += avsync_delay / 3; // re-syncing
		else
		    nexttrigger.tv_usec += avsync_delay / 30; // steady state
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

inline int NuppelVideoPlayer::getSpaceOnSoundcard(void)
{
    audio_buf_info info;
    int space = 0;

    ioctl(audiofd, SNDCTL_DSP_GETOSPACE, &info);
    space = info.bytes - audio_buffer_unused;

    if (space < 0)
    {
        numbadioctls++;
        if (numbadioctls > 2 || space < -5000)
        {
            cerr << "Your soundcard is not reporting free space correctly.\n"
                 << "Falling back to old method...\n";
            audio_buffer_unused = 0;
            space = info.bytes;
        }
    }
    else
        numbadioctls = 0;

    return space;
}

void NuppelVideoPlayer::OutputAudioLoop(void)
{
    int bytesperframe;
    int space_on_soundcard;
    unsigned char zeros[1024];
 
    bzero(zeros, 1024);

    while (!eof && !killaudio)
    {
	if (audiofd <= 0) 
	    break;

	if (pauseaudio)
	{
            audio_actually_paused = true;
            //usleep(50);
            audiotime = 0; // mark 'audiotime' as invalid.

            space_on_soundcard = getSpaceOnSoundcard();
            if (1024 < space_on_soundcard)
            {
                WriteAudio(zeros, 1024);
            }
            else
            {
             //printf("waiting for space to write 1024 zeros on soundcard which has %d bytes free\n",space_on_soundcard);
                usleep(50);
            }

            continue;
	}    
	
        if (prebuffering)
        {
	    audiotime = 0; // mark 'audiotime' as invalid

            space_on_soundcard = getSpaceOnSoundcard();
            if (1024 < space_on_soundcard)
            {
                WriteAudio(zeros, 1024);
            }
            else
            {
             //printf("waiting for space to write 1024 zeros on soundcard which has %d bytes free\n",space_on_soundcard);
                usleep(50);
            }

	    //printf("audio thread waiting for prebuffer\n");
	    continue;
        }

        SetAudiotime(); // once per loop, calculate stuff for a/v sync

        /* do audio output */
	
        /* approximate # of audio bytes for each frame. */
        bytesperframe = audio_bytes_per_sample * 
                        (int)(effdsp / 100.0 / video_frame_rate + 0.5);
	
        // wait for the buffer to fill with enough to play
        if (bytesperframe >= audiolen(true))
        { 
            //printf("audio thread waiting for buffer to fill\n");
            usleep(200);
            continue;
        }
	
        // wait for there to be free space on the sound card so we can write
        // without blocking.  We don't want to block while holding audio_buflock
	
        space_on_soundcard = getSpaceOnSoundcard();
        if (bytesperframe > space_on_soundcard)
        {
            //printf("waiting for space to write %d bytes on soundcard whish has %d bytes free\n", bytesperframe, space_on_soundcard);
            numlowbuffer++;
            if (numlowbuffer > 5 && audio_buffer_unused)
            {
                cerr << "dropping back audio_buffer_unused\n";
                audio_buffer_unused /= 2;
            }

            usleep(200);
            continue;
        }
        else
            numlowbuffer = 0;

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

void NuppelVideoPlayer::FastForward(float seconds)
{
    if (fftime == 0)
        fftime = (int)(seconds * video_frame_rate);
}

void NuppelVideoPlayer::Rewind(float seconds)
{
    if (rewindtime == 0)
        rewindtime = (int)(seconds * video_frame_rate);
}

void NuppelVideoPlayer::SkipCommercials(void)
{
    if (!skipcommercials)
        skipcommercials = 1;
}

void NuppelVideoPlayer::StartPlaying(void)
{
    consecutive_blanks = 0;

    killplayer = false;
    usepre = 3;

    framesPlayed = 0;

    InitSubs();
    if (OpenFile() < 0)
        return;

    if (fileheader.audioblocks != 0)
        InitSound();

    InitFilters();

    if (!disablevideo)
    {
        InitVideo();
        osd = new OSD(video_width, video_height, (int)ceil(video_frame_rate),
                      osdfontname, osdccfontname, osdprefix, osdtheme);
    }
    else
        own_vidbufs = true;

    playing = true;
  
    audbuf_timecode = 0;
    audiotime = 0;
    gettimeofday(&audiotime_updated, NULL);

    weseeked = 0;
    rewindtime = fftime = 0;
    skipcommercials = 0;

    resetplaying = false;
    
    if (buf == NULL)
    {
        buf = new unsigned char[video_size];
        strm = new unsigned char[video_width * video_height * 2];
 
        pthread_mutex_init(&audio_buflock, NULL);
        pthread_mutex_init(&video_buflock, NULL);
        pthread_mutex_init(&avsync_lock, NULL);

        if (own_vidbufs)
        {
            // initialize and purge buffers
            for (int i = 0; i <= MAXVBUFFER; i++)
                vbuffer[i] = new unsigned char[video_size];
        }

        for (int i = 0; i < MAXTBUFFER; i++)
            tbuffer[i] = new unsigned char[text_size];

        ClearAfterSeek();
    }

    /* This thread will fill the video and audio buffers, it does all CPU
       intensive operations. We fork two other threads which do nothing but
       write to the audio and video output devices.  These should use a 
       minimum of CPU. */

    pthread_t output_audio, output_video;
    pthread_create(&output_audio, NULL, kickoffOutputAudioLoop, this);
    pthread_create(&output_video, NULL, kickoffOutputVideoLoop, this);

    int pausecheck = 0;

    if (bookmarkseek > 30)
    {
        GetFrame(audiofd <= 0);

        bool seeks = exactseeks;
        exactseeks = false;

        fftime = bookmarkseek;
        DoFastForward();
        fftime = 0;

        exactseeks = seeks;
    }

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
            pausecheck++;

            if (!(pausecheck % 20))
            {
                if (livetv && ringBuffer->GetFreeSpace() < -1000)
                {
                    Unpause();
		    printf("forced unpause\n");
	        }
                pausecheck = 0;
            }
            if (advancedecoder)
            {
                if (vbuffer_numvalid() <= 1)
                {
                    fftime = 1;
                    DoFastForward();

                    GetFrame(audiofd <= 0);
                    resetvideo = true;
                    while (resetvideo)
                        usleep(50);
 
                    fftime = 0;
                }
                else
                    advancevideo = true;
                advancedecoder = false;
                continue;
            }
            else if (rewindtime > 0)
            {   
                DoRewind();
                    
                GetFrame(audiofd <= 0);
                resetvideo = true;
                while (resetvideo)
                    usleep(50);

                rewindtime = 0;
                continue;
            }       
            else if (fftime > 0)
            {
                fftime = CalcMaxFFTime(fftime);
                DoFastForward();

                GetFrame(audiofd <= 0);
                resetvideo = true;
                while (resetvideo)
                    usleep(50);

                fftime = 0;
                continue;
            }
            else
            {
                //printf("startplaying waiting for unpause\n");
                usleep(500);
                continue;
            }
	}
	
	if (rewindtime > 0)
	{
            PauseVideo();

            while (!GetVideoPause())
                usleep(50);

            if (rewindtime >= 1)
                DoRewind();

            UnpauseVideo();
            rewindtime = 0;
	}

	if (fftime > 0)
	{
            fftime = CalcMaxFFTime(fftime);

            PauseVideo();
            while (!GetVideoPause())
                usleep(50);

            if (fftime >= 5)
                DoFastForward();

            UnpauseVideo();
            while (GetVideoPause())
                usleep(50);
            fftime = 0;
	}

        if ( skipcommercials )
        {
            PauseVideo();

            while (!GetVideoPause())
                usleep(50);

            DoSkipCommercials();
            UnpauseVideo();

            skipcommercials = 0;
            continue;
        }

        GetFrame(audiofd <= 0);

        if (autocommercialskip)
            AutoCommercialSkip();

        if (hasdeletetable && deleteIter.data() == 1 && 
            framesPlayed >= deleteIter.key())
        {
            ++deleteIter;
            if (deleteIter.key() == totalFrames)
                eof = 1;
            else
                fftime = deleteIter.key() - framesPlayed;
            ++deleteIter;
        }
    }

    killvideo = killaudio = true;

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
    if (!m_db || !m_playbackinfo)
        return;

    m_playbackinfo->SetBookmark(framesPlayed, m_db);

    osd->ShowText("bookmark", "Position Saved", video_width * 1 / 8, 
                  video_height * 1 / 8, video_width * 7 / 8, video_height / 2, 
                  1);
}

long long NuppelVideoPlayer::GetBookmark(void)
{
    if (!m_db || !m_playbackinfo)
        return 0;

    return m_playbackinfo->GetBookmark(m_db);
}

bool NuppelVideoPlayer::DoRewind(void)
{
    long long number = rewindtime + 1;

    long long desiredFrame = framesPlayed - number;

    if (!editmode && hasdeletetable && IsInDelete(desiredFrame))
    { 
        QMap<long long, int>::Iterator it = deleteMap.begin();
        while (it != deleteMap.end())
        {
            if (desiredFrame > it.key())
                ++it;
            else
                break;
        }

        if (it != deleteMap.begin() && it != deleteMap.end())
        {
            long long over = it.key() - desiredFrame;
            --it;
            desiredFrame = it.key() - over;
        }
    }

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
    long long curPosition = ringBuffer->GetTotalReadPosition();
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
    {
        cout << "unknown position\n";
        return false;
    }

    ringBuffer->Seek(diff, SEEK_CUR);
    framesPlayed = lastKey;

    int fileend = 0;

    if (!exactseeks)
        normalframes = 0;

    if (mpa_codec)
        avcodec_flush_buffers(mpa_ctx);
 
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

	    pthread_mutex_lock(&video_buflock);
            DecodeFrame(&frameheader, strm, vbuffer[wpos]);
            wpos = (wpos + 1) % MAXVBUFFER;
	    pthread_mutex_unlock(&video_buflock);
        }
    }

    if (directrendering)
    {
        int pos = wpos - 1;
        if (pos < 0)
            pos = MAXVBUFFER - 1;

        memcpy(buf, vbuffer[pos], video_size);
        mythav_set_last_picture(mpa_ctx, buf, video_width, video_height);
    }

    ClearAfterSeek();
    return true;
}

long long NuppelVideoPlayer::CalcMaxFFTime(long long ff)
{
    long long maxtime = (long long)(1.0 * video_frame_rate);
    if (watchingrecording && nvr_enc && nvr_enc->IsValidRecorder())
        maxtime = (long long)(3.0 * video_frame_rate);
    
    long long ret = ff;

    if (livetv || (watchingrecording && nvr_enc && nvr_enc->IsValidRecorder()))
    {
        long long behind = nvr_enc->GetFramesWritten() - framesPlayed;
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

    long long keyPos = -1;

    if (positionMap->find(desiredKey / keyframedist) != positionMap->end() &&
        desiredKey != lastKey)
    {
        keyPos = (*positionMap)[desiredKey / keyframedist];
    }
    else if (livetv || (watchingrecording && nvr_enc && 
                        nvr_enc->IsValidRecorder()))
    {
        keyPos = nvr_enc->GetKeyframePosition(desiredKey / keyframedist);
        for (int i = lastKey / keyframedist; i < desiredKey / keyframedist; 
             i++)
        {
            if (positionMap->find(i) == positionMap->end())
                (*positionMap)[i] = nvr_enc->GetKeyframePosition(i);
        }
    }

    if (keyPos != -1)
    {
        lastKey = desiredKey;
        long long diff = keyPos - ringBuffer->GetTotalReadPosition();

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
                                             ringBuffer->GetTotalReadPosition();
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

    if (mpa_codec && desiredKey != lastKey)
        avcodec_flush_buffers(mpa_ctx);

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

        if (fileend)
            continue;
        if (frameheader.frametype == 'V')
        {
            framesPlayed++;
            normalframes--;

	    pthread_mutex_lock(&video_buflock);
            DecodeFrame(&frameheader, strm, vbuffer[wpos]);
            wpos = (wpos + 1) % MAXVBUFFER;
	    pthread_mutex_unlock(&video_buflock);
        }
    }

    if (directrendering)
    {
        int pos = wpos - 1;
        if (pos < 0)
            pos = MAXVBUFFER - 1;

        memcpy(buf, vbuffer[pos], video_size);
        mythav_set_last_picture(mpa_ctx, buf, video_width, video_height);
    }

    ClearAfterSeek();
    return true;
}

void NuppelVideoPlayer::JumpToFrame(long long frame)
{
    bool exactstore = exactseeks;

    exactseeks = true;
    fftime = rewindtime = 0;

    if (frame > framesPlayed)
    {
        fftime = frame - framesPlayed;
        DoFastForward();
        fftime = 0;
    }
    else if (frame < framesPlayed)
    {
        rewindtime = framesPlayed - frame;
        DoRewind();
        rewindtime = 0;
    }

    exactseeks = exactstore;
}

int NuppelVideoPlayer::SkipTooCloseToEnd(int frames)
{
    if ((livetv) ||
        (watchingrecording && nvr_enc && nvr_enc->IsValidRecorder()))
    {
        if (nvr_enc->GetFramesWritten() < (framesPlayed + frames))
            return 1;
    }
    else if ((totalFrames) && (totalFrames < (framesPlayed + frames)))
    {
        return 1;
    }
    return 0;
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
    for (int i = 0; i < MAXTBUFFER; i++)
    {
        txttimecodes[i] = 0;
    }
    wtxt = 0;
    rtxt = 0;
    raud = waud = 0;
    weseeked = 1;
    audbuf_timecode = 0;
    audiotime = 0;
    gettimeofday(&audiotime_updated, NULL);
    prebuffering = true;

    if (osd)
        osd->ClearAllCCText();

    //if (audiofd)
    //    ioctl(audiofd, SNDCTL_DSP_RESET, NULL);

    pthread_mutex_unlock(&avsync_lock);
    pthread_mutex_unlock(&video_buflock);
    pthread_mutex_unlock(&audio_buflock);

    SetDeleteIter();
}

void NuppelVideoPlayer::SetDeleteIter(void)
{
    deleteIter = deleteMap.begin();
    if (hasdeletetable)  
    {
        while (deleteIter != deleteMap.end())
        {
            if (framesPlayed > deleteIter.key())
            {
                ++deleteIter;
            }
            else
                break;
        }

        if (deleteIter != deleteMap.begin())
            --deleteIter;
        if (deleteIter.data() == 0)
            ++deleteIter;
    }
}

bool NuppelVideoPlayer::EnableEdit(void)
{
    editmode = false;

    if (!haspositionmap || !m_playbackinfo || !m_db)
        return false;

    if (m_playbackinfo->IsEditing(m_db))
        return false;

    editmode = true;
    Pause();
    while (!GetPause())
        usleep(50);
    seekamount = keyframedist;
    seekamountpos = 4;

    dialogname = "";
    UpdateEditSlider();
    UpdateTimeDisplay();

    if (hasdeletetable)
    {
        if (deleteMap.contains(0))
            deleteMap.erase(0);
        if (deleteMap.contains(totalFrames))
            deleteMap.erase(totalFrames);

        QMap<long long, int>::Iterator it;
        for (it = deleteMap.begin(); it != deleteMap.end(); ++it)
             AddMark(it.key(), it.data());
    }

    m_playbackinfo->SetEditing(true, m_db);

    return editmode;   
}

void NuppelVideoPlayer::DisableEdit(void)
{
    editmode = false;

    QMap<long long, int>::Iterator i = deleteMap.begin();
    for (; i != deleteMap.end(); ++i)
        osd->HideEditArrow(i.key());
    osd->HideText("seek_desc");
    osd->HideText("deletemarker");
    osd->HideText("edittime_display");
    osd->HideText("editslider");

    timedisplay = NULL;

    SaveCutList();

    LoadCutList();
    if (!deleteMap.isEmpty())
    {
        hasdeletetable = true;
        SetDeleteIter();
    }
   
    m_playbackinfo->SetEditing(false, m_db);

    Unpause();
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
            UpdateTimeDisplay();
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
            UpdateTimeDisplay();
            break;
        }
        case wsUp: 
        {
            UpdateSeekAmount(true);
            UpdateTimeDisplay();
            break;
        }
        case wsDown:
        {
            UpdateSeekAmount(false);
            UpdateTimeDisplay();
            break; 
        }
        case wsEscape: case 'e': case 'E':
        {
            DisableEdit();
            break;
        }
        default: break;
    }
    exactseeks = exactstore;
}

void NuppelVideoPlayer::UpdateSeekAmount(bool up)
{
    QRect rect;
    rect.setTop(video_height * 3 / 16);
    rect.setBottom(video_height * 6 / 16);
    rect.setLeft(video_width * 5 / 16);
    rect.setRight(video_width * 15 / 16);

    if (seekamountpos > 0 && !up)
        seekamountpos--;
    if (seekamountpos < 9 && up) 
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
        case 8: text = "5 minutes"; seekamount = fps * 300; break;
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
        timedisplay = new OSDSet("edittime_display", false, video_width, 
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

        timedisplay->SetAllowFade(false);
        osd->SetVisible(timedisplay, -1);
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

    if (IsInDelete(framesPlayed))
    {
        osd->ShowText("deletemarker", "cut", video_width / 8, 
                      video_height / 16, video_width / 2, 
                      video_height / 8, -1, 2);
    }
    else
        osd->HideText("deletemarker");
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

bool NuppelVideoPlayer::IsInDelete(long long testframe)
{
    long long startpos = 0;
    long long endpos = 0;
    bool first = true;
    bool indelete = false;
    bool ret = false;

    QMap<long long, int>::Iterator i;
    for (i = deleteMap.begin(); i != deleteMap.end(); ++i)
    {
        if (ret)
            break;

        long long frame = i.key();
        int direction = i.data();

        if (direction == 0 && !indelete && first)
        {
            startpos = 0;
            endpos = frame;
            first = false;
            if (startpos <= testframe && endpos >= testframe)
                ret = true;
        }
        else if (direction == 0)
        {
            endpos = frame;
            indelete = false;
            first = false;
            if (startpos <= testframe && endpos >= testframe)
                ret = true;
        }
        else if (direction == 1 && !indelete)
        {
            startpos = frame;
            indelete = true;
            first = false;
        }
        first = false;
    }

    if (indelete && testframe >= startpos) 
        ret = true;

    return ret;
}

void NuppelVideoPlayer::SaveCutList(void)
{
    long long startpos = 0;
    long long endpos = 0;
    bool first = true;
    bool indelete = false;

    long long lastpos = -1;
    int lasttype = -1;

    QMap<long long, int>::Iterator i;

    for (i = deleteMap.begin(); i != deleteMap.end();)
    {
        long long frame = i.key();
        int direction = i.data();

        if (direction == 0 && !indelete && first)
        {
            deleteMap[0] = 1;
            startpos = 0;
            endpos = frame;
        }
        else if (direction == 0)
        {
            endpos = frame;
            indelete = false;
            first = false;
        }
        else if (direction == 1 && !indelete)
        {
            startpos = frame;
            indelete = true;
            first = false;
        }

        if (direction == lasttype)
        {
            if (direction == 0)
            {
                deleteMap.remove(lastpos);
                ++i;
            }
            else
            {
                ++i;
                deleteMap.remove(frame);
            }
        }
        else
            ++i;

        lastpos = frame;
        lasttype = direction;
    }
    
    if (indelete)
        deleteMap[totalFrames] = 0;

    m_playbackinfo->SetCutList(deleteMap, m_db);
}

void NuppelVideoPlayer::LoadCutList(void)
{
    if (!m_playbackinfo || !m_db)
        return;

    m_playbackinfo->GetCutList(deleteMap, m_db);
}

char *NuppelVideoPlayer::GetScreenGrab(int secondsin, int &bufflen, int &vw,
                                       int &vh)
{
    InitSubs();
    if (OpenFile() < 0)
        return NULL;

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

    buf = new unsigned char[video_size];
    strm = new unsigned char[video_width * video_height * 2];

    pthread_mutex_init(&audio_buflock, NULL);
    pthread_mutex_init(&video_buflock, NULL);
    pthread_mutex_init(&avsync_lock, NULL);

    own_vidbufs = true;

    for (int i = 0; i <= MAXVBUFFER; i++)
        vbuffer[i] = new unsigned char[video_size];

    for (int i = 0; i < MAXTBUFFER; i++)
        tbuffer[i] = new unsigned char[text_size];

    ClearAfterSeek();

    long long int maxRead = 200000000;

    bool frame = false;

    long long keyPos = -1;

    if (positionMap->find(desiredKey / keyframedist) != positionMap->end())
        keyPos = (*positionMap)[desiredKey / keyframedist];

    GetFrame(1);

    if (mpa_codec)
        avcodec_flush_buffers(mpa_ctx);

    if (keyPos != -1)
    {
        long long diff = keyPos - ringBuffer->GetTotalReadPosition();
  
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
                    (*positionMap)[framesPlayed / keyframedist] =
                                             ringBuffer->GetTotalReadPosition();
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

            if (ringBuffer->GetTotalReadPosition() > maxRead)
                break;
        }
    }

    normalframes = 1;
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

        if (fileend)
            continue;
        if (frameheader.frametype == 'V')
        {
            framesPlayed++;
            normalframes--;
            frame = DecodeFrame(&frameheader, strm, buf);

            if (ringBuffer->GetTotalReadPosition() > maxRead)
                break;
        }
    }

    if (!frame)
    {
        bufflen = 0;
        vw = vh = 0;
        return NULL;
    }

    AVPicture orig, retbuf;
    avpicture_fill(&orig, buf, PIX_FMT_YUV420P, video_width, video_height);
 
    avpicture_deinterlace(&orig, &orig, PIX_FMT_YUV420P, video_width,
                          video_height);

    bufflen = video_width * video_height * 4;
    unsigned char *outputbuf = new unsigned char[bufflen];

    avpicture_fill(&retbuf, outputbuf, PIX_FMT_RGBA32, video_width,
                   video_height);

    img_convert(&retbuf, PIX_FMT_RGBA32, &orig, PIX_FMT_YUV420P,
                video_width, video_height);

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

#define VARIANCE_PIXELS 200
unsigned int NuppelVideoPlayer::GetFrameVariance(int vposition)
{
    unsigned int average_Y;
    unsigned int pixel_Y[VARIANCE_PIXELS];
    unsigned int variance, temp;
    int i,x,y,top,bottom;

    average_Y = variance = temp = 0;
    i = x = y = top = bottom = 0;

    top = (int)(video_height * .10);
    bottom = (int)(video_height * .90);

    // get the pixel values
    for (i = 0; i < VARIANCE_PIXELS; i++)
    {
        x = 10 + (rand() % (video_width - 10));  // Get away from the edges.
        y = top + (rand() % (bottom - top));
        pixel_Y[i] = vbuffer[vposition][y * video_width + x];
    }

    // get the average
    for (i = 0; i < VARIANCE_PIXELS; i++)
        average_Y += pixel_Y[i];
    average_Y /= VARIANCE_PIXELS;

    // get the sum of the squared differences
    for (i = 0; i < VARIANCE_PIXELS; i++)
        temp += (pixel_Y[i] - average_Y) * (pixel_Y[i] - average_Y);

    // get the variance
    variance = (unsigned int)(temp / VARIANCE_PIXELS);

    return variance;
}

void NuppelVideoPlayer::AutoCommercialSkip(void)
{
    PauseVideo();

    while (!GetVideoPause())
        usleep(50);

    if (autocommercialskip == COMMERCIAL_SKIP_BLANKS)
    {
        int variance = GetFrameVariance(wpos);
        if (variance < 20)
        {
            consecutive_blanks++;
            if (consecutive_blanks >= 3)
            {
                int saved_position = framesPlayed;
                JumpToNetFrame((long long int)(30 * video_frame_rate - 3));

                // search a 10-frame window for another blank
                int tries = 10;
                GetFrame(1, true);
                while ((tries > 0) && (GetFrameVariance(wpos) >= 20))
                {
                     GetFrame(1, true);
                     tries--;
                }

                if (tries)
                {
                    // found another blank
 
                    consecutive_blanks = 0;
                    UnpauseVideo();
                    return;
                }
                else
                {
                    // no blank found so exit auto-skip
                    JumpToFrame(saved_position + 1);
                    UnpauseVideo();
                    return;
                }
            }
        }
        else if (consecutive_blanks)
        {
            consecutive_blanks = 0;
        }
    }

    UnpauseVideo();
}

void NuppelVideoPlayer::SkipCommercialsByBlanks(void)
{
    int scanned_frames;
    int blanks_found;
    int found_blank_seq;
    int first_blank_frame;
    int saved_position;
    int min_blank_frame_seq = 1;

    // rewind 2 seconds in case user hit Skip right after a break
    JumpToNetFrame((long long int)(-2 * video_frame_rate));

    // scan through up to 64 seconds to find first break
    // 64 == 2 seconds we rewound, plus up to 62 seconds to find first break
    scanned_frames = blanks_found = found_blank_seq = first_blank_frame = 0;
    saved_position = framesPlayed;

    while (scanned_frames < (64 * video_frame_rate))
    {
        GetFrame(1, true);
        if (GetFrameVariance(wpos) < 20)
        {
            blanks_found++;
            if (!first_blank_frame)
                first_blank_frame = framesPlayed;
            if (blanks_found >= min_blank_frame_seq)
                break;
        }
        else if (blanks_found)
        {
            blanks_found = 0;
            first_blank_frame = 0;
        }
        if (SkipTooCloseToEnd(min_blank_frame_seq - blanks_found))
        {
            JumpToFrame(saved_position);
            return;
        }

        scanned_frames++;
    }

    if (!first_blank_frame)
    {
        JumpToFrame(saved_position);
        return;
    }

    // if we make it here, then a blank was found
    int blank_seq_found = 0;
    do
    {
        int jump_seconds = 14;

        blank_seq_found = 0;
        saved_position = framesPlayed;
        while ((framesPlayed - saved_position) < (61 * video_frame_rate))
        {
            JumpToNetFrame((long long int)(jump_seconds * video_frame_rate));
            jump_seconds = 12;

            scanned_frames = blanks_found = found_blank_seq = 0;
            first_blank_frame = 0;
            while (scanned_frames < (3 * video_frame_rate))
            {
                GetFrame(1, true);
                if (GetFrameVariance(wpos) < 20)
                {
                    blanks_found++;
                    if (!first_blank_frame)
                        first_blank_frame = framesPlayed;
                    if (blanks_found >= min_blank_frame_seq)
                        break;
                }
                else if (blanks_found)
                {
                    blanks_found = 0;
                    first_blank_frame = 0;
                }

                if (SkipTooCloseToEnd(min_blank_frame_seq - blanks_found))
                {
                    JumpToFrame(saved_position);
                    return;
                }

                scanned_frames++;
            }

            if (blanks_found >= min_blank_frame_seq)
            {
                blank_seq_found = 1;
                break;
            }
        }
    } while (blank_seq_found);

    JumpToFrame(saved_position);
}

bool NuppelVideoPlayer::DoSkipCommercials(void)
{
    switch (commercialskipmethod)
    {
        case COMMERCIAL_SKIP_BLANKS:
        default                    : SkipCommercialsByBlanks();
                                     break;
    }

    return true;
}
