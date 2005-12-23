#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "mythconfig.h"
#ifdef HAVE_SYS_SOUNDCARD_H
    #include <sys/soundcard.h>
#elif HAVE_SOUNDCARD_H
    #include <soundcard.h>
#endif
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <cerrno>
#include <cmath>

#include <qstringlist.h>

#include <iostream>
using namespace std;

#include "mythcontext.h"
#include "NuppelVideoRecorder.h"
#include "vbitext/cc.h"
#include "channelbase.h"
#include "filtermanager.h"
#include "recordingprofile.h"

extern "C" {
#include "vbitext/vbi.h"
}

#include "videodev_myth.h"
#include "go7007_myth.h"

#ifdef WORDS_BIGENDIAN
#include "bswap.h"
#endif

#ifndef MJPIOC_S_PARAMS
#include "videodev_mjpeg.h"
#endif

#define KEYFRAMEDIST   30

#include "RingBuffer.h"
#include "RTjpegN.h"

#include "programinfo.h"

#ifdef USING_FRONTEND
extern pthread_mutex_t avcodeclock;
#else
pthread_mutex_t avcodeclock = PTHREAD_MUTEX_INITIALIZER;
#endif

#define LOC QString("NVR(%1): ").arg(videodevice)
#define LOC_ERR QString("NVR(%1) Error: ").arg(videodevice)

NuppelVideoRecorder::NuppelVideoRecorder(TVRec *rec, ChannelBase *channel)
                   : RecorderBase(rec, "NuppelVideoRecorder")
{
    channelObj = channel;

    encoding = false;
    fd = -1;
    channelfd = -1;
    lf = tf = 0;
    M1 = 0, M2 = 0, Q = 255;
    pid = pid2 = 0;
    inputchannel = 1;
    compression = 1;
    compressaudio = 1;
    rawmode = 0;
    usebttv = 1;
    w = 352;
    h = 240;
    pip_mode = 0;
    video_aspect = 1.33333;

    skip_btaudio = false;

    framerate_multiplier = 1.0;
    height_multiplier = 1.0;

    mp3quality = 3;
    gf = NULL;
    rtjc = NULL;
    strm = NULL;   
    mp3buf = NULL;

    transcoding = false;

    act_video_encode = 0;
    act_video_buffer = 0;
    video_buffer_count = 0;
    video_buffer_size = 0;

    act_audio_encode = 0;
    act_audio_buffer = 0;
    audio_buffer_count = 0;
    audio_buffer_size = 0;

    act_text_encode = 0;
    act_text_buffer = 0;
    text_buffer_count = 0;
    text_buffer_size = 0;

    childrenLive = false;
    errored = false;

    recording = false;

    writepaused = false;
    audiopaused = false;
    mainpaused = false;

    keyframedist = KEYFRAMEDIST;

    audiobytes = 0;
    audio_bits = 16;
    audio_channels = 2;
    audio_samplerate = 44100;
    audio_bytes_per_sample = audio_channels * audio_bits / 8;

    picture_format = PIX_FMT_YUV420P;

    avcodec_init();
    avcodec_register_all();

    mpa_codec = 0;
    mpa_ctx = NULL;

    useavcodec = false;

    targetbitrate = 2200;
    scalebitrate = 1;
    maxquality = 2;
    minquality = 31;
    qualdiff = 3;
    mp4opts = 0;
    mb_decision = FF_MB_DECISION_SIMPLE;
    encoding_thread_count = 1;

    oldtc = 0;
    startnum = 0;
    frameofgop = 0;
    lasttimecode = 0;
    audio_behind = 0;

    extendeddataOffset = 0;
    seektable = new vector<struct seektable_entry>;

    hardware_encode = false;
    hmjpg_quality = 80;
    hmjpg_hdecimation = 2;
    hmjpg_vdecimation = 2;
    hmjpg_maxw = 640;
    
    videoFilterList = "";
    videoFilters = NULL;
    FiltMan = new FilterManager;
    inpixfmt = FMT_YV12;
    correct_bttv = false;

    usingv4l2 = false;

    prev_bframe_save_pos = -1;

    volume = 100;

    ccd = new CCDecoder(this);

    go7007 = false;
    resetcapture = false;
}

NuppelVideoRecorder::~NuppelVideoRecorder(void)
{
    if (weMadeBuffer && ringBuffer)
    {
        delete ringBuffer;
        ringBuffer = NULL;
    }
    if (rtjc)
        delete rtjc;
    if (mp3buf)
        delete [] mp3buf;
    if (gf)
        lame_close(gf);  
    if (strm)
        delete [] strm;
    if (fd >= 0)
        close(fd);
    if (seektable)
    {
        seektable->clear();
        delete seektable;  
    }  

    while (videobuffer.size() > 0)
    {
        struct vidbuffertype *vb = videobuffer.back();
        delete [] vb->buffer;
        delete vb;
        videobuffer.pop_back();
    }
    while (audiobuffer.size() > 0)
    {
        struct audbuffertype *ab = audiobuffer.back();
        delete [] ab->buffer;
        delete ab;
        audiobuffer.pop_back();
    }
    while (textbuffer.size() > 0)
    {
        struct txtbuffertype *tb = textbuffer.back();
        delete [] tb->buffer;
        delete tb;
        textbuffer.pop_back();
    }

    if (mpa_codec)
        avcodec_close(mpa_ctx);

    if (mpa_ctx)
        av_free(mpa_ctx);
    mpa_ctx = NULL;

    if (videoFilters)
        delete videoFilters;
    if (FiltMan)
        delete FiltMan;
    if (ccd)
        delete ccd;
}

void NuppelVideoRecorder::deleteLater(void)
{
    if (fd >= 0)
    {
        close(fd);
        fd = -1;
    }
    RecorderBase::deleteLater();
}

void NuppelVideoRecorder::SetOption(const QString &opt, int value)
{
    if (opt == "width")
        w_out = w = value;
    else if (opt == "height")
        h_out = h = value;
    else if (opt == "rtjpegchromafilter")
        M1 = value;
    else if (opt == "rtjpeglumafilter")
        M2 = value;
    else if (opt == "rtjpegquality")
        Q = value;
    else if (opt == "mpeg4bitrate")
        targetbitrate = value;
    else if (opt == "mpeg4scalebitrate")
        scalebitrate = value;
    else if (opt == "mpeg4maxquality")
        maxquality = value;
    else if (opt == "mpeg4minquality")
        minquality = value;
    else if (opt == "mpeg4qualdiff")
        qualdiff = value;
    else if (opt == "encodingthreadcount")
        encoding_thread_count = value;
    else if (opt == "mpeg4optionvhq")
    {
        if (value)
            mb_decision = FF_MB_DECISION_RD;
        else
            mb_decision = FF_MB_DECISION_SIMPLE;
    }
    else if (opt == "mpeg4option4mv")
    {
        if (value)
            mp4opts |= CODEC_FLAG_4MV;
        else
            mp4opts &= ~CODEC_FLAG_4MV;
    }
    else if (opt == "mpeg4optionidct")
    {
        if (value)
            mp4opts |= CODEC_FLAG_INTERLACED_DCT;
        else
            mp4opts &= ~CODEC_FLAG_INTERLACED_DCT;
    }
    else if (opt == "mpeg4optionime")
    {
        if (value)
            mp4opts |= CODEC_FLAG_INTERLACED_ME;
        else
            mp4opts &= ~CODEC_FLAG_INTERLACED_ME;
    }
    else if (opt == "hardwaremjpegquality")
        hmjpg_quality = value;
    else if (opt == "hardwaremjpeghdecimation")
        hmjpg_hdecimation = value;
    else if (opt == "hardwaremjpegvdecimation")
        hmjpg_vdecimation = value;
    else if (opt == "audiocompression")
        compressaudio = value;
    else if (opt == "mp3quality")
        mp3quality = value;
    else if (opt == "samplerate")
        audio_samplerate = value;
    else if (opt == "audioframesize")
        audio_buffer_size = value;
    else if (opt == "pip_mode")
        pip_mode = value;
    else if (opt == "inpixfmt")
        inpixfmt = (VideoFrameType)value;
    else if (opt == "skipbtaudio")
        skip_btaudio = value;
    else if (opt == "volume")
        volume = value;
    else
        RecorderBase::SetOption(opt, value);
}

void NuppelVideoRecorder::SetOptionsFromProfile(RecordingProfile *profile,
                                                const QString &videodev,
                                                const QString &audiodev,
                                                const QString &vbidev)
{
    SetOption("videodevice", videodev);
    SetOption("vbidevice", vbidev);
    SetOption("tvformat", gContext->GetSetting("TVFormat"));
    SetOption("vbiformat", gContext->GetSetting("VbiFormat"));
    SetOption("audiodevice", audiodev);

    QString setting = profile->byName("videocodec")->getValue();
    if (setting == "MPEG-4")
    {
        SetOption("codec", "mpeg4");

        SetIntOption(profile, "mpeg4bitrate");
        SetIntOption(profile, "mpeg4scalebitrate");
        SetIntOption(profile, "mpeg4maxquality");
        SetIntOption(profile, "mpeg4minquality");
        SetIntOption(profile, "mpeg4qualdiff");
#ifdef HAVE_PTHREADS
        SetIntOption(profile, "encodingthreadcount");
#endif
        SetIntOption(profile, "mpeg4optionvhq");
        SetIntOption(profile, "mpeg4option4mv");
    }
    else if (setting == "RTjpeg")
    {
        SetOption("codec", "rtjpeg");

        SetIntOption(profile, "rtjpegquality");
        SetIntOption(profile, "rtjpegchromafilter");
        SetIntOption(profile, "rtjpeglumafilter");
    }
    else if (setting == "Hardware MJPEG")
    {
        SetOption("codec", "hardware-mjpeg");

        SetIntOption(profile, "hardwaremjpegquality");
        SetIntOption(profile, "hardwaremjpeghdecimation");
        SetIntOption(profile, "hardwaremjpegvdecimation");
    }
    else
    {
        VERBOSE(VB_IMPORTANT, "Unknown video codec");
        VERBOSE(VB_IMPORTANT, "Please go into the TV Settings, Recording Profiles and");
        VERBOSE(VB_IMPORTANT, "setup the four 'Software Encoders' profiles.");
        VERBOSE(VB_IMPORTANT, "Assuming RTjpeg for now.");

        SetOption("codec", "rtjpeg");

        SetIntOption(profile, "rtjpegquality");
        SetIntOption(profile, "rtjpegchromafilter");
        SetIntOption(profile, "rtjpeglumafilter");
    }

    setting = profile->byName("audiocodec")->getValue();
    if (setting == "MP3")
    {
        SetOption("audiocompression", 1);
        SetIntOption(profile, "mp3quality");
        SetIntOption(profile, "samplerate");
    }
    else if (setting == "Uncompressed")
    {
        SetOption("audiocompression", 0);
        SetIntOption(profile, "samplerate");
    }
    else
    {
        VERBOSE(VB_IMPORTANT, "NVR: Error, unknown audio codec");
        SetOption("audiocompression", 0);
    }

    SetIntOption(profile, "volume");

    SetIntOption(profile, "width");
    SetIntOption(profile, "height");
}

void NuppelVideoRecorder::Pause(bool clear)
{
    cleartimeonpause = clear;
    writepaused = audiopaused = mainpaused = false;
    request_pause = true;

    // The wakeAll is to make sure [write|audio|main]paused are
    // set immediately, even if we were already paused previously.
    unpauseWait.wakeAll();
}

void NuppelVideoRecorder::Unpause(void)
{
    request_pause = false;
    unpauseWait.wakeAll();
}

bool NuppelVideoRecorder::IsPaused(void) const
{
    return (audiopaused && mainpaused && writepaused);
}

void NuppelVideoRecorder::SetVideoFilters(QString &filters)
{
    videoFilterList = filters;
    InitFilters();
}

bool NuppelVideoRecorder::IsRecording(void)
{
    return recording;
}

bool NuppelVideoRecorder::IsErrored(void)
{
    return errored;
}

long long NuppelVideoRecorder::GetFramesWritten(void)
{
    return framesWritten;
}

int NuppelVideoRecorder::GetVideoFd(void)
{
    return channelfd;
}

bool NuppelVideoRecorder::SetupAVCodec(void)
{
    if (!useavcodec)
        useavcodec = true;

    if (mpa_codec)
        avcodec_close(mpa_ctx);
    
    if (mpa_ctx)
        av_free(mpa_ctx);
    mpa_ctx = NULL;

    mpa_codec = avcodec_find_encoder_by_name(codec.ascii());

    if (!mpa_codec)
    {
        VERBOSE(VB_IMPORTANT, QString("NVR: Error finding codec: %1").arg(codec.ascii()));
        return false;
    }

    mpa_ctx = avcodec_alloc_context();
 
    switch (picture_format)
    {
        case PIX_FMT_YUV420P:
        case PIX_FMT_YUV422P:
        case PIX_FMT_YUVJ420P:
            mpa_ctx->pix_fmt = picture_format; 
            mpa_picture.linesize[0] = w_out;
            mpa_picture.linesize[1] = w_out / 2;
            mpa_picture.linesize[2] = w_out / 2;
            break;
        default:
            VERBOSE(VB_IMPORTANT, QString("NVR: Unknown picture format: %1").arg(picture_format));
    }
 
    mpa_ctx->width = w_out;
    mpa_ctx->height = (int)(h * height_multiplier);

    int usebitrate = targetbitrate * 1000;
    if (scalebitrate)
    {
        float diff = (w_out * h_out) / (640.0 * 480.0);
        usebitrate = (int)(diff * usebitrate);
    }

    if (targetbitrate == -1)
        usebitrate = -1;

    mpa_ctx->time_base.den = (int)ceil(video_frame_rate * 100 *
                                    framerate_multiplier);
    mpa_ctx->time_base.num = 100;
    mpa_ctx->bit_rate = usebitrate;
    mpa_ctx->bit_rate_tolerance = usebitrate * 100;
    mpa_ctx->qmin = maxquality;
    mpa_ctx->qmax = minquality;
    mpa_ctx->mb_qmin = maxquality;
    mpa_ctx->mb_qmax = minquality;
    mpa_ctx->max_qdiff = qualdiff;
    mpa_ctx->flags = mp4opts;
    mpa_ctx->mb_decision = mb_decision;

    mpa_ctx->qblur = 0.5;
    mpa_ctx->max_b_frames = 0;
    mpa_ctx->b_quant_factor = 0;
    mpa_ctx->rc_strategy = 2;
    mpa_ctx->b_frame_strategy = 0;
    mpa_ctx->gop_size = 30;
    mpa_ctx->rc_max_rate = 0;
    mpa_ctx->rc_min_rate = 0;
    mpa_ctx->rc_buffer_size = 0;
    mpa_ctx->rc_buffer_aggressivity = 1.0;
    mpa_ctx->rc_override_count = 0;
    mpa_ctx->rc_initial_cplx = 0;
    mpa_ctx->dct_algo = FF_DCT_AUTO;
    mpa_ctx->idct_algo = FF_IDCT_AUTO;
    mpa_ctx->prediction_method = FF_PRED_LEFT;
    if (codec.lower() == "huffyuv" || codec.lower() == "mjpeg")
        mpa_ctx->strict_std_compliance = FF_COMPLIANCE_INOFFICIAL;

    pthread_mutex_lock(&avcodeclock);

#ifdef HAVE_PTHREADS
    if ((encoding_thread_count > 1) &&
        avcodec_thread_init(mpa_ctx, encoding_thread_count))
    {
        VERBOSE(VB_IMPORTANT, LOC + "FFMPEG couldn't start threading...");
    }
#endif

    if (avcodec_open(mpa_ctx, mpa_codec) < 0)
    {
        pthread_mutex_unlock(&avcodeclock);
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Unable to open FFMPEG/%1 codec").arg(codec));
        return false;
    }

    pthread_mutex_unlock(&avcodeclock);
    return true;
}

void NuppelVideoRecorder::SetupRTjpeg(void)
{
    picture_format = PIX_FMT_YUV420P;

    int setval;
    rtjc = new RTjpeg();
    setval = RTJ_YUV420;
    rtjc->SetFormat(&setval);
    setval = (int)(h_out * height_multiplier);
    rtjc->SetSize(&w_out, &setval);
    rtjc->SetQuality(&Q);
    setval = 2;
    rtjc->SetIntra(&setval, &M1, &M2);
}

void NuppelVideoRecorder::Initialize(void)
{
    if (AudioInit() != 0)   
    {
        VERBOSE(VB_IMPORTANT, "NVR: Could not detect audio blocksize");
    }
 
    if (codec == "hardware-mjpeg")
    {
        codec = "mjpeg";
        hardware_encode = true;

        MJPEGInit();
 
        w = hmjpg_maxw / hmjpg_hdecimation;

        if (ntsc)
        {
            switch (hmjpg_vdecimation)
            {
                case 2: h = 240; break;
                case 4: h = 120; break;
                default: h = 480; break;
            }
        }
        else
        {
            switch (hmjpg_vdecimation)
            {
                case 2: h = 288; break;
                case 4: h = 144; break;
                default: h = 576; break;
            }
        }
    }

    if (!ringBuffer)
    {
        VERBOSE(VB_IMPORTANT, "NVR: Warning, old ringbuf creation");
        ringBuffer = new RingBuffer("output.nuv", true);
        weMadeBuffer = true;
        livetv = false;
        if (!ringBuffer->IsOpen())
        {
            VERBOSE(VB_IMPORTANT, "NVR: Could not open RingBuffer");
            errored = true;
            return;
        }
    }
    else
        livetv = ringBuffer->LiveMode();

    audiobytes = 0;

    InitFilters();
    InitBuffers();
}

int NuppelVideoRecorder::AudioInit(bool skipdevice)
{
    int afmt, afd;
    int frag, blocksize = 4096;
    int tmp;

#if !defined (HAVE_SYS_SOUNDCARD_H) && !defined(HAVE_SOUNDCARD_H)
    VERBOSE(VB_IMPORTANT, QString("NVR::AudioInit() This Unix doesn't support"
                                  " device files for audio access. Skipping"));
    return 1;
#else
    if (!skipdevice)
    {
        if (-1 == (afd = open(audiodevice.ascii(), O_RDONLY | O_NONBLOCK)))
        {
            VERBOSE(VB_IMPORTANT, QString("NVR: Error, cannot open DSP '%1'").
                    arg(audiodevice));
            perror("open");
            return 1;
        }
 
        fcntl(afd, F_SETFL, fcntl(afd, F_GETFL) & ~O_NONBLOCK);
 
        //ioctl(afd, SNDCTL_DSP_RESET, 0);
   
        frag = (8 << 16) | (10); //8 buffers, 1024 bytes each
        ioctl(afd, SNDCTL_DSP_SETFRAGMENT, &frag);
 
        afmt = AFMT_S16_LE;
        ioctl(afd, SNDCTL_DSP_SETFMT, &afmt);
        if (afmt != AFMT_S16_LE) 
        {
            close(afd);
            VERBOSE(VB_IMPORTANT, "NVR: Error, can't get 16 bit DSP");
            return 1;
        }

        if (ioctl(afd, SNDCTL_DSP_SAMPLESIZE, &audio_bits) < 0 ||
            ioctl(afd, SNDCTL_DSP_CHANNELS, &audio_channels) < 0 ||
            ioctl(afd, SNDCTL_DSP_SPEED, &audio_samplerate) < 0)
        {
            close(afd);
            QString msg = 
                QString("NVR: AudioInit(): %1 : error setting audio input device"
                        " to %2kHz/%3bits/%4channel").arg(audiodevice).
                arg(audio_samplerate).arg(audio_bits).arg(audio_channels);
            VERBOSE(VB_IMPORTANT, msg);
            return 1;
        }

        if (-1 == ioctl(afd, SNDCTL_DSP_GETBLKSIZE, &blocksize)) 
        {
            close(afd);
            VERBOSE(VB_IMPORTANT, "NVR: AudioInit(): Can't get DSP blocksize");
            return(1);
        }

        close(afd);
    }
#endif

    audio_bytes_per_sample = audio_channels * audio_bits / 8;
    blocksize *= 4;

    audio_buffer_size = blocksize;

    if (compressaudio)
    {
        gf = lame_init();
        lame_set_bWriteVbrTag(gf, 0);
        lame_set_quality(gf, mp3quality);
        lame_set_compression_ratio(gf, 11);
        lame_set_mode(gf, audio_channels == 2 ? STEREO : MONO);
        lame_set_num_channels(gf, audio_channels);
        lame_set_in_samplerate(gf, audio_samplerate);
        if ((tmp = lame_init_params(gf)) != 0)
        {
            VERBOSE(VB_IMPORTANT, QString("NVR: AudioInit(): lame_init_params error %1").arg(tmp));
            compressaudio = false; 
        }

        if (audio_bits != 16) 
        {
            VERBOSE(VB_IMPORTANT, "NVR: AudioInit(): lame support requires 16bit audio");
            compressaudio = false;
        }
    }
    mp3buf_size = (int)(1.25 * 16384 + 7200);
    mp3buf = new char[mp3buf_size];

    return 0; 
}

/** \fn NuppelVideoRecorder::MJPEGInit(void)
 *  \brief Determines MJPEG capture resolution.
 *
 *   This function requires an file descriptor for the device, which
 *   means Channel can not be open when NVR::Initialize() is called.
 *   It is safe for the recorder to be open.
 *
 *  \return true on success
 */
bool NuppelVideoRecorder::MJPEGInit(void)
{
    bool we_opened_fd = false;
    int init_fd = fd;
    if (init_fd < 0)
    {
        init_fd = open(videodevice.ascii(), O_RDWR);
        we_opened_fd = true;

        if (init_fd < 0)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Can't open video device" + ENO);
            return false;
        }
    }

    struct video_capability vc;
    bzero(&vc, sizeof(vc));
    int ret = ioctl(init_fd, VIDIOCGCAP, &vc);

    if (ret < 0)
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Can't query V4L capabilities" + ENO);

    if (we_opened_fd)
        close(init_fd);

    if (ret < 0)
        return false;

    if (vc.maxwidth != 768 && vc.maxwidth != 640)
        vc.maxwidth = 720;

    if (vc.type & VID_TYPE_MJPEG_ENCODER)
    {
        if (vc.maxwidth >= 768)
            hmjpg_maxw = 768;
        else if (vc.maxwidth >= 704)
            hmjpg_maxw = 704;
        else
            hmjpg_maxw = 640;
        return true;
    }

    VERBOSE(VB_IMPORTANT, LOC_ERR + "MJPEG not supported by device");
    return false;
}

void NuppelVideoRecorder::InitFilters(void)
{
    int btmp;
    if (videoFilters)
        delete videoFilters;

    QString tmpVideoFilterList;

    w_out = w;
    h_out = h;
    VideoFrameType tmp = FMT_YV12;

    if (correct_bttv && !videoFilterList.contains("adjust"))
    {
        if (videoFilterList.isEmpty())
            tmpVideoFilterList = "adjust";
        else
            tmpVideoFilterList = "adjust," + videoFilterList;
    }
    else
        tmpVideoFilterList = videoFilterList;

    videoFilters = FiltMan->LoadFilters(tmpVideoFilterList, inpixfmt, tmp, 
                                        w_out, h_out, btmp);
    if (video_buffer_size && btmp != video_buffer_size)
    {
        video_buffer_size = btmp;
        ResizeVideoBuffers();
    }
}

void NuppelVideoRecorder::InitBuffers(void)
{
    int videomegs;
    int audiomegs = 2;

    if (!video_buffer_size)
    {
        if (picture_format == PIX_FMT_YUV422P)
            video_buffer_size = w_out * h_out * 2;
        else
            video_buffer_size = w_out * h_out * 3 / 2;
    }

    if (w >= 480 || h > 288)
        videomegs = 20;
    else
        videomegs = 12;

    video_buffer_count = (videomegs * 1000 * 1000) / video_buffer_size;

    if (audio_buffer_size != 0)
        audio_buffer_count = (audiomegs * 1000 * 1000) / audio_buffer_size;
    else
        audio_buffer_count = 0;

    text_buffer_size = 8 * (sizeof(teletextsubtitle) + VT_WIDTH);
    text_buffer_count = video_buffer_count;

    for (int i = 0; i < video_buffer_count; i++)
    {
        vidbuffertype *vidbuf = new vidbuffertype;
        vidbuf->buffer = new unsigned char[video_buffer_size];
        vidbuf->sample = 0;
        vidbuf->freeToEncode = 0;
        vidbuf->freeToBuffer = 1;
        vidbuf->bufferlen = 0;
        vidbuf->forcekey = 0;
       
        videobuffer.push_back(vidbuf);
    }

    for (int i = 0; i < audio_buffer_count; i++)
    {
        audbuffertype *audbuf = new audbuffertype;
        audbuf->buffer = new unsigned char[audio_buffer_size];
        audbuf->sample = 0;
        audbuf->freeToEncode = 0;
        audbuf->freeToBuffer = 1;
      
        audiobuffer.push_back(audbuf);
    }

    for (int i = 0; i < text_buffer_count; i++)
    {
        txtbuffertype *txtbuf = new txtbuffertype;
        txtbuf->buffer = new unsigned char[text_buffer_size];
        txtbuf->freeToEncode = 0;
        txtbuf->freeToBuffer = 1;

        textbuffer.push_back(txtbuf);
    }
}

void NuppelVideoRecorder::ResizeVideoBuffers(void)
{
    for (unsigned int i = 0; i < videobuffer.size(); i++)
    {
        delete [] (videobuffer[i]->buffer);
        videobuffer[i]->buffer = new unsigned char[video_buffer_size];
    }
}

void NuppelVideoRecorder::StopRecording(void)
{
    encoding = false;
}

void NuppelVideoRecorder::StreamAllocate(void)
{
    strm = new signed char[w * h * 2 + 10];
}

bool NuppelVideoRecorder::Open(void)
{
    if (channelfd>0)
        return true;

    int retries = 0;
    fd = open(videodevice.ascii(), O_RDWR);
    while (fd < 0)
    {
        usleep(30000);
        fd = open(videodevice.ascii(), O_RDWR);
        if (retries++ > 5)
        {
            VERBOSE(VB_IMPORTANT, QString("NVR: Can't open video device: %1").
                    arg(videodevice));
            perror("open video:");
            KillChildren();
            errored = true;
            return false;
        }
    }

    usingv4l2 = true;

    struct v4l2_capability vcap;
    memset(&vcap, 0, sizeof(vcap));

    if (ioctl(fd, VIDIOC_QUERYCAP, &vcap) < 0)
    {
        usingv4l2 = false;
    }

    if (usingv4l2 && !(vcap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
    {
        VERBOSE(VB_IMPORTANT, "NVR: Not a v4l2 capture device, falling back to v4l");
        usingv4l2 = false;
    }

    if (usingv4l2 && !(vcap.capabilities & V4L2_CAP_STREAMING))
    {
        VERBOSE(VB_IMPORTANT, "NVR: Won't work with the streaming interface, falling back");
        usingv4l2 = false;
    }

    if (usingv4l2)
    {
        if (vcap.card[0] == 'B' && vcap.card[1] == 'T' &&
            vcap.card[2] == '8' && vcap.card[4] == '8')
            correct_bttv = true;

        QString driver = (char *)vcap.driver;
        if (driver == "cx8800" || driver == "go7007")
        {
            channelfd = open(videodevice.ascii(), O_RDWR);
            if (channelfd < 0)
            {
                VERBOSE(VB_IMPORTANT, QString("NVR: Can't open video device: %1").arg(videodevice));
                perror("open video:");
                KillChildren();
                return false;
            }

            if (driver == "go7007")
            {
                go7007 = true;
            }
            
            inpixfmt = FMT_NONE;
            InitFilters();
            DoV4L2();
            return false;
        }
    }

    channelfd = fd;
    return true;
}

void NuppelVideoRecorder::StartRecording(void)
{
    if (lzo_init() != LZO_E_OK)
    {
        VERBOSE(VB_IMPORTANT, "NVR: lzo_init() failed, exiting");
        errored = true;
        return;
    }

    StreamAllocate();

    positionMapLock.lock();
    positionMap.clear();
    positionMapDelta.clear();
    positionMapLock.unlock();

    if (codec.lower() == "rtjpeg")
        useavcodec = false;
    else
        useavcodec = true;

    if (useavcodec)
        useavcodec = SetupAVCodec();

    if (!useavcodec)
        SetupRTjpeg();

    if (CreateNuppelFile() != 0)
    {
        VERBOSE(VB_IMPORTANT, QString("NVR: Error, cannot open '%1' for writing").
                arg(ringBuffer->GetFilename()));
        errored = true;
        return;
    }

    if (childrenLive)
    {
        VERBOSE(VB_IMPORTANT, "NVR: Error, children are already alive");
        errored = true;
        return;
    }

    if (SpawnChildren() < 0)
    {
        VERBOSE(VB_IMPORTANT, "NVR: Error, couldn't spawn children");
        errored = true;
        return;
    }

    // save the start time
    gettimeofday(&stm, &tzone);

    if (getuid() == 0)
        nice(-10);

    if (!Open())
    {
        errored = true;
        return;
    }

    struct video_capability vc;
    struct video_mmap mm;
    struct video_mbuf vm;
    struct video_channel vchan;
    struct video_audio va;
    struct video_tuner vt;

    memset(&mm, 0, sizeof(mm));
    memset(&vm, 0, sizeof(vm));
    memset(&vchan, 0, sizeof(vchan));
    memset(&va, 0, sizeof(va));
    memset(&vt, 0, sizeof(vt));
    memset(&vc, 0, sizeof(vc));

    if (ioctl(fd, VIDIOCGCAP, &vc) < 0)
    {
        perror("VIDIOCGCAP:");
        KillChildren(); 
        errored = true;
        return;
    }

    if (vc.name[0] == 'B' && vc.name[1] == 'T' && vc.name[2] == '8' &&
        vc.name[4] == '8')
        correct_bttv = true;

    int channelinput = 0;

    if (channelObj)
        channelinput = channelObj->GetCurrentInputNum();

    vchan.channel = channelinput;

    if (ioctl(fd, VIDIOCGCHAN, &vchan) < 0)
        perror("VIDIOCGCHAN");

    // if channel has a audio then activate it
    if (!skip_btaudio && (vchan.flags & VIDEO_VC_AUDIO) == VIDEO_VC_AUDIO) {
        if (ioctl(fd, VIDIOCGAUDIO, &va)<0)
            perror("VIDIOCGAUDIO");

        va.flags &= ~VIDEO_AUDIO_MUTE; // now this really has to work

        va.volume = volume * 65535 / 100;

        if (ioctl(fd, VIDIOCSAUDIO, &va) < 0)
            perror("VIDIOCSAUDIO");
        //if (ioctl(fd, VIDIOCSCHAN, &vchan) < 0)
        //    perror("VIDIOCSCHAN");
    }

    if ((vc.type & VID_TYPE_MJPEG_ENCODER) && hardware_encode)
    {
        DoMJPEG();
        errored = true;
        return;
    }

    inpixfmt = FMT_NONE;
    InitFilters();

    if (ioctl(fd, VIDIOCGMBUF, &vm) < 0)
    {
        perror("VIDIOCGMBUF:");
        KillChildren();
        errored = true;
        return;
    }

    if (vm.frames < 2)
    {
        fprintf(stderr, "need a minimum of 2 capture buffers\n");
        KillChildren();
        errored = true;
        return;
    }

    int frame;

    unsigned char *buf = (unsigned char *)mmap(0, vm.size, 
                                               PROT_READ|PROT_WRITE, 
                                               MAP_SHARED, 
                                               fd, 0);
    if (buf <= 0)
    {
        perror("mmap");
        KillChildren();
        errored = true;
        return;
    }

    mm.height = h;
    mm.width  = w;
    if (inpixfmt == FMT_YUV422P)
        mm.format = VIDEO_PALETTE_YUV422P;
    else
        mm.format = VIDEO_PALETTE_YUV420P;  

    mm.frame  = 0;
    if (ioctl(fd, VIDIOCMCAPTURE, &mm)<0) 
        perror("VIDIOCMCAPTUREi0");
    mm.frame  = 1;
    if (ioctl(fd, VIDIOCMCAPTURE, &mm)<0) 
        perror("VIDIOCMCAPTUREi1");
    
    encoding = true;
    recording = true;

    int syncerrors = 0;

    while (encoding) 
    {
        if (request_pause)
        {
           mainpaused = true;
           pauseWait.wakeAll();
           if (IsPaused() && tvrec)
               tvrec->RecorderPaused();

           unpauseWait.wait(100);
           if (cleartimeonpause)
               gettimeofday(&stm, &tzone);
           continue;
        }
        mainpaused = false;

        frame = 0;
        mm.frame = 0;
        if (ioctl(fd, VIDIOCSYNC, &frame)<0) 
        {
            syncerrors++;
            if (syncerrors == 10)
                VERBOSE(VB_IMPORTANT, "NVR: Multiple bttv errors, "
                        "further messages supressed");
            else if (syncerrors < 10)
                perror("VIDIOCSYNC");
        }
        else 
        {
            BufferIt(buf+vm.offsets[0], video_buffer_size);
            //memset(buf+vm.offsets[0], 0, video_buffer_size);
        }

        if (ioctl(fd, VIDIOCMCAPTURE, &mm)<0) 
            perror("VIDIOCMCAPTURE0");

        frame = 1;
        mm.frame = 1;
        if (ioctl(fd, VIDIOCSYNC, &frame)<0) 
        {
            syncerrors++;
            if (syncerrors == 10)
                VERBOSE(VB_IMPORTANT, "NVR: Multiple bttv errors, further messages supressed");
            else if (syncerrors < 10)
                perror("VIDIOCSYNC");
        }
        else 
        {
            BufferIt(buf+vm.offsets[1], video_buffer_size);
            //memset(buf+vm.offsets[1], 0, video_buffer_size);
        }
        if (ioctl(fd, VIDIOCMCAPTURE, &mm)<0) 
            perror("VIDIOCMCAPTURE1");
    }

    munmap(buf, vm.size);

    KillChildren();

    FinishRecording();

    recording = false;
    close(fd);
}

void NuppelVideoRecorder::DoV4L2(void)
{
    struct v4l2_format     vfmt;
    struct v4l2_buffer     vbuf;
    struct v4l2_requestbuffers vrbuf;
    struct v4l2_control    vc;

    memset(&vfmt, 0, sizeof(vfmt));
    memset(&vbuf, 0, sizeof(vbuf));
    memset(&vrbuf, 0, sizeof(vrbuf));
    memset(&vc, 0, sizeof(vc));

    vc.id = V4L2_CID_AUDIO_MUTE;
    vc.value = 0;

    if (ioctl(fd, VIDIOC_S_CTRL, &vc) < 0)
        perror("VIDIOC_S_CTRL:V4L2_CID_AUDIO_MUTE");

    vfmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    vfmt.fmt.pix.width = w;
    vfmt.fmt.pix.height = h;
    vfmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

    if (go7007)
        vfmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MPEG;
    else if (inpixfmt == FMT_YUV422P)
        vfmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV422P;
    else
        vfmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;

    if (ioctl(fd, VIDIOC_S_FMT, &vfmt) < 0)
    {
        // this is supported by the cx88 and various ati cards.
        vfmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;

        if (ioctl(fd, VIDIOC_S_FMT, &vfmt) < 0)
        {
            VERBOSE(VB_IMPORTANT, "NVR: v4l2: Unable to set desired format");
            errored = true;
            return;
        }
        else
        {
            // we need to convert the buffer - we can't deal with yuyv directly.
            if (inpixfmt == FMT_YUV422P)
            {
                VERBOSE(VB_IMPORTANT, "NVR: v4l2: yuyv format supported, but yuv422 requested.");
                VERBOSE(VB_IMPORTANT, "NVR: v4l2: unfortunately, this converter hasn't been written yet, exiting");
                errored = true;
                return;
            }
            VERBOSE(VB_RECORD, "NVR: v4l2: format set, getting yuyv from v4l, converting");
        }
    }
    else // cool, we can do our preferred format, most likely running on bttv.
        VERBOSE(VB_RECORD, "NVR: v4l2: format set, getting yuv420 from v4l");

    if (go7007)
    {
        struct go7007_comp_params comp;
        struct go7007_mpeg_params mpeg;

        memset(&comp, 0, sizeof(comp));
        comp.gop_size = keyframedist;
        comp.max_b_frames = 0;
        comp.aspect_ratio = GO7007_ASPECT_RATIO_1_1;
        comp.flags |= GO7007_COMP_CLOSED_GOP;
        if (ioctl(fd, GO7007IOC_S_COMP_PARAMS, &comp) < 0) 
        {
            VERBOSE(VB_IMPORTANT, "Unable to set compression params\n");
            errored = true;
            return;
        }

        memset(&mpeg, 0, sizeof(mpeg));
        mpeg.mpeg_video_standard = GO7007_MPEG_VIDEO_MPEG4;
        if (ioctl(fd, GO7007IOC_S_MPEG_PARAMS, &mpeg) < 0)
        {
            VERBOSE(VB_IMPORTANT, "Unable to set MPEG params\n");
            errored = true;
            return;
        }

        int usebitrate = targetbitrate * 1000;
        if (scalebitrate)
        {
            float diff = (w * h) / (640.0 * 480.0);
            usebitrate = (int)(diff * usebitrate);
        }

        if (ioctl(fd, GO7007IOC_S_BITRATE, &usebitrate) < 0) 
        {
            VERBOSE(VB_IMPORTANT, "Unable to set bitrate\n");
            errored = true;
            return;
        }

        hardware_encode = true;
    }

    int numbuffers = 5;

    vrbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    vrbuf.memory = V4L2_MEMORY_MMAP;
    vrbuf.count = numbuffers;

    if (ioctl(fd, VIDIOC_REQBUFS, &vrbuf) < 0)
    {
        VERBOSE(VB_IMPORTANT, "NVR: Not able to get any capture buffers, exiting");
        errored = true;
        return;
    }

    if (vrbuf.count < 5)
    {
        VERBOSE(VB_IMPORTANT, "NVR: Not enough buffer memory, exiting");
        errored = true;
        return;
    }

    numbuffers = vrbuf.count;

    unsigned char *buffers[numbuffers];
    int bufferlen[numbuffers];

    for (int i = 0; i < numbuffers; i++)
    {
        vbuf.type = vrbuf.type;
        vbuf.index = i;

        if (ioctl(fd, VIDIOC_QUERYBUF, &vbuf) < 0)
        {
            VERBOSE(VB_IMPORTANT, QString("NVR: unable to query capture buffer %1").arg(i));
            errored = true;
            return;
        }

        buffers[i] = (unsigned char *)mmap(NULL, vbuf.length,
                                           PROT_READ|PROT_WRITE, MAP_SHARED,
                                           fd, vbuf.m.offset);

        if (buffers[i] == MAP_FAILED)
        {
            perror("mmap");
            VERBOSE(VB_IMPORTANT, QString("NVR: memory map error"));
            errored = true;
            return;
        }
        bufferlen[i] = vbuf.length;
    }

    for (int i = 0; i < numbuffers; i++)
    {
        memset(buffers[i], 0, bufferlen[i]);
        vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        vbuf.index = i;
        ioctl(fd, VIDIOC_QBUF, &vbuf);
    }

    int turnon = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(fd, VIDIOC_STREAMON, &turnon);

    struct timeval tv;
    fd_set rdset;
    int frame = 0;
    bool forcekey = false;

    encoding = true;
    recording = true;

    while (encoding) {
again:
        if (request_pause)
        {
            mainpaused = true;
            pauseWait.wakeAll();
            if (IsPaused() && tvrec)
                tvrec->RecorderPaused();

            unpauseWait.wait(100);
            if (cleartimeonpause)
                gettimeofday(&stm, &tzone);
            continue;
        }
        mainpaused = false;

        if (resetcapture && go7007)
        {
            int turnon = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            ioctl(fd, VIDIOC_STREAMOFF, &turnon);

            for (int i = 0; i < numbuffers; i++)
            {
                memset(buffers[i], 0, bufferlen[i]);
                vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                vbuf.index = i;
                ioctl(fd, VIDIOC_QBUF, &vbuf);
             }

             ioctl(fd, VIDIOC_STREAMON, &turnon);
             resetcapture = false;
        }

        tv.tv_sec = 5;
        tv.tv_usec = 0;
        FD_ZERO(&rdset);
        FD_SET(fd, &rdset);

        switch (select(fd+1, &rdset, NULL, NULL, &tv))
        {
            case -1:
                  if (errno == EINTR)
                      goto again;
                  perror("select");
                  continue;
            case 0:
                  printf("select timeout\n");
                  continue;
           default: break;
        }

        memset(&vbuf, 0, sizeof(vbuf));
        vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        vbuf.memory = V4L2_MEMORY_MMAP;
        if (ioctl(fd, VIDIOC_DQBUF, &vbuf) < 0)
        {
            perror("VIDIOC_DQBUF");
            if (errno == -EINVAL)
            {
                for (int i = 0; i < numbuffers; i++)
                {
                    vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                    vbuf.index = i;
                    ioctl(fd, VIDIOC_QBUF, &vbuf);
                }
                continue;
            }
        }

        frame = vbuf.index;
        if (go7007)
            forcekey = vbuf.flags & V4L2_BUF_FLAG_KEYFRAME;

        if (!request_pause)
        {
            if (vfmt.fmt.pix.pixelformat == V4L2_PIX_FMT_YUYV)
            {
                // Convert YUYV to YUV420P
                unsigned conversion_buffer_size = h * w * 3 / 2;
                uint8_t conversion_buffer[conversion_buffer_size];

                uint8_t *y_plane = conversion_buffer;
                uint8_t *cb_plane = y_plane + w * h;
                uint8_t *cr_plane = cb_plane + w * h / 4;

                uint8_t *src = buffers[frame];

                // Round height to multiple of two.
                unsigned height = (h / 2) * 2;

                // Treat lines in batches of two, first use color information, then don't
                unsigned line_size = w * 2;

                for (unsigned line = 0; line < height; line += 2)
                {
                    uint8_t *src_endline = src + line_size;

                    // convert first line, use color information
                    while (src < src_endline)
                    {
                        *y_plane++ = *src++;
                        *cb_plane++ = *src++;
                        *y_plane++ = *src++;
                        *cr_plane++ = *src++;
                    }

                    src_endline = src + line_size;

                    // convert second line, don't use color information
                    while (src < src_endline)
                    {
                        *y_plane++ = *src++;
                        src++;
                        *y_plane++ = *src++;
                        src++;
                    }
                }

                BufferIt(conversion_buffer, video_buffer_size);
            }
            else
            {
                // buffer the frame directly
                BufferIt(buffers[frame], vbuf.bytesused, forcekey);
            }
        }

        vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(fd, VIDIOC_QBUF, &vbuf);
    }

    KillChildren();

    ioctl(fd, VIDIOC_STREAMOFF, &turnon);

    for (int i = 0; i < numbuffers; i++)
    {
        munmap(buffers[i], bufferlen[i]);
    }

    FinishRecording();

    recording = false;
    close(fd);
    close(channelfd);
}

void NuppelVideoRecorder::DoMJPEG(void)
{
    struct mjpeg_params bparm;

    if (ioctl(fd, MJPIOC_G_PARAMS, &bparm) < 0)
    {
        perror("MJPIOC_G_PARAMS:");
        return;
    }

    //bparm.input = 2;
    //bparm.norm = 1;
    bparm.quality = hmjpg_quality;

    if (hmjpg_hdecimation == hmjpg_vdecimation)
    {
        bparm.decimation = hmjpg_hdecimation;
    }
    else
    {
        bparm.decimation = 0;
        bparm.HorDcm = hmjpg_hdecimation;
        bparm.VerDcm = (hmjpg_vdecimation + 1) / 2;

        if (hmjpg_vdecimation == 1)
        {
            bparm.TmpDcm = 1;
            bparm.field_per_buff = 2;
        }
        else
        {
            bparm.TmpDcm = 2;
            bparm.field_per_buff = 1;
        }

        bparm.img_width = hmjpg_maxw;
      
        if (ntsc)
            bparm.img_height = 240;
        else 
            bparm.img_height = 288;

        bparm.img_x = 0;
        bparm.img_y = 0;
    }

    bparm.APPn = 0;

    if (hmjpg_vdecimation == 1)
        bparm.APP_len = 14;
    else
        bparm.APP_len = 0;

    bparm.odd_even = !(hmjpg_vdecimation > 1);

    for (int n = 0; n < bparm.APP_len; n++)
        bparm.APP_data[n] = 0;

    if (ioctl(fd, MJPIOC_S_PARAMS, &bparm) < 0)
    {
        perror("MJPIOC_S_PARAMS:");
        return;
    }

    struct mjpeg_requestbuffers breq;

    breq.count = 64;
    breq.size = 256 * 1024;

    if (ioctl(fd, MJPIOC_REQBUFS, &breq) < 0)
    {
        perror("MJPIOC_REQBUFS:");
        return;
    }

    uint8_t *MJPG_buff = (uint8_t *)mmap(0, breq.count * breq.size, 
                                         PROT_READ|PROT_WRITE, MAP_SHARED, fd, 
                                         0);

    if (MJPG_buff == MAP_FAILED)
    {
        VERBOSE(VB_IMPORTANT, "NVR: error mapping mjpeg buffers");
        return;
    } 
  
    struct mjpeg_sync bsync;
 
    for (unsigned int count = 0; count < breq.count; count++)
    {
        if (ioctl(fd, MJPIOC_QBUF_CAPT, &count) < 0)
            perror("MJPIOC_QBUF_CAPT:");
    }

    encoding = true;
    recording = true;

    while (encoding)
    {
        if (request_pause)
        {
           mainpaused = true;
           pauseWait.wakeAll();
           if (IsPaused() && tvrec)
               tvrec->RecorderPaused();

           unpauseWait.wait(100);
           if (cleartimeonpause)
               gettimeofday(&stm, &tzone);
           continue;
        }
        mainpaused = false;

        if (ioctl(fd, MJPIOC_SYNC, &bsync) < 0)
            encoding = false;

        BufferIt((unsigned char *)(MJPG_buff + bsync.frame * breq.size),
                 bsync.length);

        if (ioctl(fd, MJPIOC_QBUF_CAPT, &(bsync.frame)) < 0)
            encoding = false;
    }

    munmap(MJPG_buff, breq.count * breq.size);
    KillChildren();
        
    FinishRecording();    
            
    recording = false;
    close(fd);
}

int NuppelVideoRecorder::SpawnChildren(void)
{
    int result;

    childrenLive = true;
    
    result = pthread_create(&write_tid, NULL, 
                            NuppelVideoRecorder::WriteThread, this);

    if (result)
    {
        VERBOSE(VB_IMPORTANT, "NVR: Couldn't spawn writer thread, exiting");
        return -1;
    }

    result = pthread_create(&audio_tid, NULL,
                            NuppelVideoRecorder::AudioThread, this);

    if (result)
    {
        VERBOSE(VB_IMPORTANT, "NVR: Couldn't spawn audio thread, exiting");
        return -1;
    }

    if (vbimode)
    {
        result = pthread_create(&vbi_tid, NULL,
                                NuppelVideoRecorder::VbiThread, this);

        if (result)
        {
            VERBOSE(VB_IMPORTANT, "NVR: Couldn't spawn vbi thread, exiting");
            return -1;
        }
    }

    return 0;
}

void NuppelVideoRecorder::KillChildren(void)
{
    childrenLive = false;
   
    pthread_join(write_tid, NULL);
    pthread_join(audio_tid, NULL);
    if (vbimode)
        pthread_join(vbi_tid, NULL);
#ifdef HAVE_PTHREADS
    if (useavcodec && encoding_thread_count > 1)
        avcodec_thread_free(mpa_ctx); 
#endif
}

void NuppelVideoRecorder::BufferIt(unsigned char *buf, int len, bool forcekey)
{
    int act;
    long tcres;
    int fn;
    struct timeval now;

    act = act_video_buffer;
 
    if (!videobuffer[act]->freeToBuffer) {
        return;
    }

    gettimeofday(&now, &tzone);
   
    tcres = (now.tv_sec-stm.tv_sec)*1000 + now.tv_usec/1000 - stm.tv_usec/1000;

    usebttv = 0;
    // here is the non preferable timecode - drop algorithm - fallback
    if (!usebttv) 
    {
        if (tf==0)
            tf = 2;
        else 
        {
            fn = tcres - oldtc;

     // the difference should be less than 1,5*timeperframe or we have 
     // missed at least one frame, this code might be inaccurate!
     
            if (ntsc_framerate)
                fn = (fn+16)/33;
            else 
                fn = (fn+20)/40;
            if (fn<1)
                fn=1;
            tf += 2*fn; // two fields
        }
    }

    oldtc = tcres;

    if (!videobuffer[act]->freeToBuffer) 
    {
        printf("DROPPED frame due to full buffer in the recorder.\n");
        return; // we can't buffer the current frame
    }
    
    videobuffer[act]->sample = tf;

    // record the time at the start of this frame.
    // 'tcres' is at the end of the frame, so subtract the right # of ms
    videobuffer[act]->timecode = (ntsc_framerate) ? (tcres - 33) : (tcres - 40);

    memcpy(videobuffer[act]->buffer, buf, len);
    videobuffer[act]->bufferlen = len;
    videobuffer[act]->forcekey = forcekey;

    videobuffer[act]->freeToBuffer = 0;
    act_video_buffer++;
    if (act_video_buffer >= video_buffer_count) 
        act_video_buffer = 0; // cycle to begin of buffer
    videobuffer[act]->freeToEncode = 1; // set last to prevent race
    return;
}

inline void NuppelVideoRecorder::WriteFrameheader(rtframeheader *fh)
{
#ifdef WORDS_BIGENDIAN
    fh->timecode     = bswap_32(fh->timecode);
    fh->packetlength = bswap_32(fh->packetlength);
#endif
    ringBuffer->Write(fh, FRAMEHEADERSIZE);
}

void NuppelVideoRecorder::WriteHeader(void)
{
    struct rtfileheader fileheader;
    struct rtframeheader frameheader;
    static unsigned long int tbls[128];
    static const char finfo[12] = "MythTVVideo";
    static const char vers[5]   = "0.07";
    
    if (!videoFilters)
        InitFilters();
    memset(&fileheader, 0, sizeof(fileheader));
    memcpy(fileheader.finfo, finfo, sizeof(fileheader.finfo));
    memcpy(fileheader.version, vers, sizeof(fileheader.version));
    fileheader.width  = w_out;
    fileheader.height = (int)(h_out * height_multiplier);
    fileheader.desiredwidth  = 0;
    fileheader.desiredheight = 0;
    fileheader.pimode = 'P';
    fileheader.aspect = video_aspect;
    fileheader.fps = video_frame_rate;
    fileheader.fps *= framerate_multiplier;
    fileheader.videoblocks = -1;
    fileheader.audioblocks = -1;
    fileheader.textsblocks = -1; // TODO: make only -1 if VBI support active?
    fileheader.keyframedist = KEYFRAMEDIST;

#ifdef WORDS_BIGENDIAN
    fileheader.width         = bswap_32(fileheader.width);
    fileheader.height        = bswap_32(fileheader.height);
    fileheader.desiredwidth  = bswap_32(fileheader.desiredwidth);
    fileheader.desiredheight = bswap_32(fileheader.desiredheight);
    fileheader.aspect        = bswap_dbl(fileheader.aspect);
    fileheader.fps           = bswap_dbl(fileheader.fps);
    fileheader.videoblocks   = bswap_32(fileheader.videoblocks);
    fileheader.audioblocks   = bswap_32(fileheader.audioblocks);
    fileheader.textsblocks   = bswap_32(fileheader.textsblocks);
    fileheader.keyframedist  = bswap_32(fileheader.keyframedist);
#endif
    ringBuffer->Write(&fileheader, FILEHEADERSIZE);

    memset(&frameheader, 0, sizeof(frameheader));
    frameheader.frametype = 'D'; // compressor data

    if (useavcodec)
    {
        frameheader.comptype = 'F';
        frameheader.packetlength = mpa_ctx->extradata_size;

        WriteFrameheader(&frameheader);
        ringBuffer->Write(mpa_ctx->extradata, frameheader.packetlength);
    }
    else
    {
        frameheader.comptype = 'R'; // compressor data for RTjpeg
        frameheader.packetlength = sizeof(tbls);

        // compression configuration header
        WriteFrameheader(&frameheader);

        memset(tbls, 0, sizeof(tbls));
        ringBuffer->Write(tbls, sizeof(tbls));
    }

    memset(&frameheader, 0, sizeof(frameheader));
    frameheader.frametype = 'X'; // extended data
    frameheader.packetlength = sizeof(extendeddata);

    // extended data header
    WriteFrameheader(&frameheader);
    
    struct extendeddata moredata;
    memset(&moredata, 0, sizeof(extendeddata));
    
    moredata.version = 1;
    if (useavcodec)
    {
        int vidfcc = 0;
        switch(mpa_codec->id)
        {
            case CODEC_ID_MPEG4: vidfcc = MKTAG('D', 'I', 'V', 'X'); break;
            case CODEC_ID_WMV1: vidfcc = MKTAG('W', 'M', 'V', '1'); break;
            case CODEC_ID_MSMPEG4V3: vidfcc = MKTAG('D', 'I', 'V', '3'); break;
            case CODEC_ID_MSMPEG4V2: vidfcc = MKTAG('M', 'P', '4', '2'); break;
            case CODEC_ID_MSMPEG4V1: vidfcc = MKTAG('M', 'P', 'G', '4'); break;
            case CODEC_ID_MJPEG: vidfcc = MKTAG('M', 'J', 'P', 'G'); break;
            case CODEC_ID_H263: vidfcc = MKTAG('H', '2', '6', '3'); break;
            case CODEC_ID_H263P: vidfcc = MKTAG('H', '2', '6', '3'); break;
            case CODEC_ID_H263I: vidfcc = MKTAG('I', '2', '6', '3'); break;
            case CODEC_ID_MPEG1VIDEO: vidfcc = MKTAG('M', 'P', 'E', 'G'); break;
            case CODEC_ID_HUFFYUV: vidfcc = MKTAG('H', 'F', 'Y', 'U'); break;
            default: break;
        }
        moredata.video_fourcc = vidfcc;
        moredata.lavc_bitrate = mpa_ctx->bit_rate;
        moredata.lavc_qmin = mpa_ctx->qmin;
        moredata.lavc_qmax = mpa_ctx->qmax;
        moredata.lavc_maxqdiff = mpa_ctx->max_qdiff;
    }
    else
    {
        moredata.video_fourcc = MKTAG('R', 'J', 'P', 'G');
        moredata.rtjpeg_quality = Q;
        moredata.rtjpeg_luma_filter = M1;
        moredata.rtjpeg_chroma_filter = M2;
    }

    if (compressaudio)
    {
        moredata.audio_fourcc = MKTAG('L', 'A', 'M', 'E');
        moredata.audio_compression_ratio = 11;
        moredata.audio_quality = mp3quality;
    }
    else
    {
        moredata.audio_fourcc = MKTAG('R', 'A', 'W', 'A');
    }

    moredata.audio_sample_rate = audio_samplerate;
    moredata.audio_channels = audio_channels;
    moredata.audio_bits_per_sample = audio_bits;

    extendeddataOffset = ringBuffer->GetWritePosition();

#ifdef WORDS_BIGENDIAN
    moredata.version                 = bswap_32(moredata.version);
    moredata.video_fourcc            = bswap_32(moredata.video_fourcc);
    moredata.audio_fourcc            = bswap_32(moredata.audio_fourcc);
    moredata.audio_sample_rate       = bswap_32(moredata.audio_sample_rate);
    moredata.audio_bits_per_sample   = bswap_32(moredata.audio_bits_per_sample);
    moredata.audio_channels          = bswap_32(moredata.audio_channels);
    moredata.audio_compression_ratio = bswap_32(moredata.audio_compression_ratio);
    moredata.audio_quality           = bswap_32(moredata.audio_quality);
    moredata.rtjpeg_quality          = bswap_32(moredata.rtjpeg_quality);
    moredata.rtjpeg_luma_filter      = bswap_32(moredata.rtjpeg_luma_filter);
    moredata.rtjpeg_chroma_filter    = bswap_32(moredata.rtjpeg_chroma_filter);
    moredata.lavc_bitrate            = bswap_32(moredata.lavc_bitrate);
    moredata.lavc_qmin               = bswap_32(moredata.lavc_qmin);
    moredata.lavc_qmax               = bswap_32(moredata.lavc_qmax);
    moredata.lavc_maxqdiff           = bswap_32(moredata.lavc_maxqdiff);
    moredata.seektable_offset        = bswap_64(moredata.seektable_offset);
    moredata.keyframeadjust_offset   = bswap_64(moredata.keyframeadjust_offset);
#endif
    ringBuffer->Write(&moredata, sizeof(moredata));

    last_block = 0;
    lf = 0; // that resets framenumber so that seeking in the
            // continues parts works too
}

void NuppelVideoRecorder::WriteSeekTable(void)
{
    int numentries = seektable->size();

    struct rtframeheader frameheader;
    memset(&frameheader, 0, sizeof(frameheader));
    frameheader.frametype = 'Q'; // SeekTable
    frameheader.packetlength = sizeof(struct seektable_entry) * numentries;

    long long currentpos = ringBuffer->GetWritePosition();

    ringBuffer->Write(&frameheader, sizeof(frameheader));    

    char *seekbuf = new char[frameheader.packetlength];
    int offset = 0;

    vector<struct seektable_entry>::iterator i = seektable->begin();
    for (; i != seektable->end(); i++)
    {
        memcpy(seekbuf + offset, (const void *)&(*i), 
               sizeof(struct seektable_entry));
        offset += sizeof(struct seektable_entry);
    }

    ringBuffer->Write(seekbuf, frameheader.packetlength);

    ringBuffer->WriterSeek(extendeddataOffset + 
                           offsetof(struct extendeddata, seektable_offset),
                           SEEK_SET);

    ringBuffer->Write(&currentpos, sizeof(long long));

    ringBuffer->WriterSeek(0, SEEK_END);

    delete [] seekbuf;
}

void NuppelVideoRecorder::WriteKeyFrameAdjustTable(
                                     QPtrList<struct kfatable_entry> *kfa_table)
{
    int numentries = kfa_table->count();

    struct rtframeheader frameheader;
    memset(&frameheader, 0, sizeof(frameheader));
    frameheader.frametype = 'K'; // KFA Table
    frameheader.packetlength = sizeof(struct kfatable_entry) * numentries;

    long long currentpos = ringBuffer->GetWritePosition();

    ringBuffer->Write(&frameheader, sizeof(frameheader));

    char *kfa_buf = new char[frameheader.packetlength];
    int offset = 0;

    struct kfatable_entry *i = kfa_table->first();
    for (; i ; i = kfa_table->next())
    {
        memcpy(kfa_buf + offset, (const void *)&(*i),
               sizeof(struct kfatable_entry));
        offset += sizeof(struct kfatable_entry);
    }

    ringBuffer->Write(kfa_buf, frameheader.packetlength);


    ringBuffer->WriterSeek(extendeddataOffset +
                           offsetof(struct extendeddata, keyframeadjust_offset),
                           SEEK_SET);

    ringBuffer->Write(&currentpos, sizeof(long long));

    ringBuffer->WriterSeek(0, SEEK_END);

    delete [] kfa_buf;
}

void NuppelVideoRecorder::UpdateSeekTable(int frame_num, bool update_db,
                                          long offset)
{
    long long position = ringBuffer->GetWritePosition() + offset;
    struct seektable_entry ste;
    ste.file_offset = position;
    ste.keyframe_number = frame_num;
    seektable->push_back(ste);

    bool save_map = false;

    positionMapLock.lock();
    if (!positionMap.contains(ste.keyframe_number))
    {
        positionMapDelta[ste.keyframe_number] = position;
        positionMap[ste.keyframe_number] = position;
        lastPositionMapPos = position;
        save_map = true;
    }
    positionMapLock.unlock();

    if (save_map && update_db)
        SavePositionMap(false);
}

void NuppelVideoRecorder::SavePositionMap(bool force)
{
    QMutexLocker locker(&positionMapLock);

    force |= (positionMap.size() < 30) && (positionMap.size() % 5 == 1);
    force |= positionMapDelta.size() >= 30;

    if (curRecording && force)
    {
        curRecording->SetPositionMapDelta(positionMapDelta, MARK_KEYFRAME);
        curRecording->SetFilesize(lastPositionMapPos);
        positionMapDelta.clear();
    }
}

int NuppelVideoRecorder::CreateNuppelFile(void)
{
    framesWritten = 0;
    
    if (!ringBuffer)
    {
        VERBOSE(VB_IMPORTANT, "NVR: Error, no ringbuffer, recorder wasn't initialized.");
        return -1;
    }

    if (!ringBuffer->IsOpen())
    {
        VERBOSE(VB_IMPORTANT, "NVR: Ringbuffer isn't open");
        return -1;
    }

    WriteHeader();

    return 0;
}

void NuppelVideoRecorder::Reset(void)
{
    ResetForNewFile();

    for (int i = 0; i < video_buffer_count; i++)
    {
        vidbuffertype *vidbuf = videobuffer[i];
        vidbuf->sample = 0;
        vidbuf->timecode = 0;
        vidbuf->freeToEncode = 0;
        vidbuf->freeToBuffer = 1;
        vidbuf->forcekey = 0;
    }

    for (int i = 0; i < audio_buffer_count; i++)
    {
        audbuffertype *audbuf = audiobuffer[i];
        audbuf->sample = 0;
        audbuf->timecode = 0;
        audbuf->freeToEncode = 0;
        audbuf->freeToBuffer = 1;
    }

    for (int i = 0; i < text_buffer_count; i++)
    {
        txtbuffertype *txtbuf = textbuffer[i];
        txtbuf->buffer = new unsigned char[text_buffer_size];
        txtbuf->freeToEncode = 0;
        txtbuf->freeToBuffer = 1;
    }

    act_video_encode = 0;
    act_video_buffer = 0;
    act_audio_encode = 0;
    act_audio_buffer = 0;
    act_audio_sample = 0;
    act_text_encode = 0;
    act_text_buffer = 0;

    audiobytes = 0;
    effectivedsp = 0;

    if (useavcodec)
        SetupAVCodec();

    if (curRecording)
        curRecording->ClearPositionMap(MARK_KEYFRAME);
}

void *NuppelVideoRecorder::WriteThread(void *param)
{
    NuppelVideoRecorder *nvr = (NuppelVideoRecorder *)param;

    nvr->doWriteThread();

    return NULL;
}

void *NuppelVideoRecorder::AudioThread(void *param)
{
    NuppelVideoRecorder *nvr = (NuppelVideoRecorder *)param;

    nvr->doAudioThread();

    return NULL;
}

void *NuppelVideoRecorder::VbiThread(void *param)
{
    NuppelVideoRecorder *nvr = (NuppelVideoRecorder *)param;

    nvr->doVbiThread();

    return NULL;
}

void NuppelVideoRecorder::doAudioThread(void)
{
#if !defined (HAVE_SYS_SOUNDCARD_H) && !defined(HAVE_SOUNDCARD_H)
    VERBOSE(VB_IMPORTANT,
            QString("NVR::doAudioThread() This Unix doesn't support"
                    " device files for audio access. Skipping"));
    return;
#else
    int afmt = 0, trigger = 0;
    int afd = 0, act = 0, lastread = 0;
    int frag = 0, blocksize = 0;
    unsigned char *buffer;
    audio_buf_info ispace;
    struct timeval anow;

    act_audio_sample = 0;

    if (-1 == (afd = open(audiodevice.ascii(), O_RDONLY | O_NONBLOCK))) 
    {
        VERBOSE(VB_IMPORTANT, QString("NVR: Cannot open DSP '%1', exiting").
                arg(audiodevice));
        perror("open");
        return;
    }

    fcntl(afd, F_SETFL, fcntl(afd, F_GETFL) & ~O_NONBLOCK);
    //ioctl(afd, SNDCTL_DSP_RESET, 0);

    frag = (8 << 16) | (10); //8 buffers, 1024 bytes each
    ioctl(afd, SNDCTL_DSP_SETFRAGMENT, &frag);

    afmt = AFMT_S16_LE;
    ioctl(afd, SNDCTL_DSP_SETFMT, &afmt);
    if (afmt != AFMT_S16_LE) 
    {
        VERBOSE(VB_IMPORTANT, "NVR: Can't get 16 bit DSP, exiting");
        close(afd);
        return;
    }

    if (ioctl(afd, SNDCTL_DSP_SAMPLESIZE, &audio_bits) < 0 ||
        ioctl(afd, SNDCTL_DSP_CHANNELS, &audio_channels) < 0 ||
        ioctl(afd, SNDCTL_DSP_SPEED, &audio_samplerate) < 0)
    {
        VERBOSE(VB_IMPORTANT, QString("NVR: %1: error setting audio input device to "
                                      "%2 kHz/%3 bits/%4 channel").
                arg(audiodevice).arg(audio_samplerate).
                arg(audio_bits).arg(audio_channels));
        close(afd);
        return;
    }

    audio_bytes_per_sample = audio_channels * audio_bits / 8;

    if (-1 == ioctl(afd, SNDCTL_DSP_GETBLKSIZE,  &blocksize)) 
    {
        VERBOSE(VB_IMPORTANT, "NVR: Can't get DSP blocksize, exiting");
        close(afd);
        return;
    }

    blocksize *= 4;  // allways read 4*blocksize

    if (blocksize != audio_buffer_size) 
    {
        VERBOSE(VB_IMPORTANT, 
                QString("NVR: Warning, audio blocksize = '%1' while audio_buffer_size='%2'").
                arg(blocksize).arg(audio_buffer_size));
    }

    buffer = new unsigned char[audio_buffer_size];

    /* trigger record */
    trigger = 0;
    ioctl(afd,SNDCTL_DSP_SETTRIGGER,&trigger);

    trigger = PCM_ENABLE_INPUT;
    ioctl(afd,SNDCTL_DSP_SETTRIGGER,&trigger);

    audiopaused = false;
    while (childrenLive) 
    {
        if (request_pause)
        {
            audiopaused = true;
            pauseWait.wakeAll();
            if (IsPaused() && tvrec)
                tvrec->RecorderPaused();

            unpauseWait.wait(100);
            act = act_audio_buffer;
            continue;
        }
        audiopaused = false;

        if (audio_buffer_size != (lastread = read(afd, buffer,
                                                  audio_buffer_size))) 
        {
            VERBOSE(VB_IMPORTANT, 
                    QString("NVR: Only read %1 bytes of %2 bytes from '%3").
                    arg(lastread).arg(audio_buffer_size).arg(audiodevice));
            perror("read audio");
        }

        /* record the current time */
        /* Don't assume that the sound device's record buffer is empty
           (like we used to.) Measure to see how much stuff is in there,
           and correct for it when calculating the timestamp */
        gettimeofday(&anow, &tzone);
        ioctl( afd, SNDCTL_DSP_GETISPACE, &ispace );

        act = act_audio_buffer;

        if (!audiobuffer[act]->freeToBuffer) 
        {
            VERBOSE(VB_IMPORTANT, "NVR: Ran out of free AUDIO buffers :-(");
            act_audio_sample++;
            continue;
        }

        audiobuffer[act]->sample = act_audio_sample;

        /* calculate timecode. First compute the difference
           between now and stm (start time) */
        audiobuffer[act]->timecode = (anow.tv_sec - stm.tv_sec) * 1000 + 
                                     anow.tv_usec / 1000 - stm.tv_usec / 1000;
        /* We want the timestamp to point to the start of this
           audio chunk. So, subtract off the length of the chunk
           and the length of audio still in the capture buffer. */
        audiobuffer[act]->timecode -= (int)( 
                (ispace.fragments * ispace.fragsize + audio_buffer_size)
                 * 1000.0 / (audio_samplerate * audio_bytes_per_sample));

        memcpy(audiobuffer[act]->buffer, buffer, audio_buffer_size);

        audiobuffer[act]->freeToBuffer = 0;
        act_audio_buffer++;
        if (act_audio_buffer >= audio_buffer_count) 
            act_audio_buffer = 0; 
        audiobuffer[act]->freeToEncode = 1; 

        act_audio_sample++; 
    }

    delete [] buffer;
    close(afd);
#endif
}

struct VBIData
{
    NuppelVideoRecorder *nvr;
    vt_page teletextpage;
    bool foundteletextpage;
};

void NuppelVideoRecorder::FormatTeletextSubtitles(struct VBIData *vbidata)
{
    struct timeval tnow;
    gettimeofday(&tnow, &tzone);

    int act = act_text_buffer;
    if (!textbuffer[act]->freeToBuffer)
    {
        VERBOSE(VB_IMPORTANT, QString("NVR: Teletext #%1: ran out of free TEXT buffers :-(").arg(act));
        return;
    }

    // calculate timecode:
    // compute the difference  between now and stm (start time)
    textbuffer[act]->timecode = (tnow.tv_sec-stm.tv_sec) * 1000 +
                                tnow.tv_usec/1000 - stm.tv_usec/1000;
    textbuffer[act]->pagenr = (vbidata->teletextpage.pgno << 16) + 
                              vbidata->teletextpage.subno;

    unsigned char *inpos = vbidata->teletextpage.data[0];
    unsigned char *outpos = textbuffer[act]->buffer;
    *outpos = 0;
    struct teletextsubtitle st;
    unsigned char linebuf[VT_WIDTH + 1];
    unsigned char *linebufpos = linebuf;

    for (int y = 0; y < VT_HEIGHT; y++)
    {
        char c = ' ';
        char last_c = ' ';
        int hid = 0;
        int gfx = 0;
        int dbl = 0;
        int box = 0;
        int sep = 0;
        int hold = 0;
        int visible = 0;
        int fg = 7;
        int bg = 0;

        for (int x = 0; x < VT_WIDTH; ++x)
        {
            c = *inpos++;
            switch (c)
            {
                case 0x00 ... 0x07:     /* alpha + fg color */
                    fg = c & 7;
                    gfx = 0;
                    sep = 0;
                    hid = 0;
                    goto ctrl;
                case 0x08:              /* flash */
                    goto ctrl;
                case 0x09:              /* steady */
                    goto ctrl;
                case 0x0a:              /* end box */
                    box = 0;
                    goto ctrl;
                case 0x0b:              /* start box */
                    box = 1;
                    goto ctrl;
                case 0x0c:              /* normal height */
                    dbl = 0;
                    goto ctrl;
                case 0x0d:              /* double height */
                    if (y < VT_HEIGHT-2)        /* ignored on last 2 lines */
                    {
                        dbl = 1;
                    }
                    goto ctrl;
                case 0x10 ... 0x17:     /* gfx + fg color */
                    fg = c & 7;
                    gfx = 1;
                    hid = 0;
                    goto ctrl;
                case 0x18:              /* conceal */
                    hid = 1;
                    goto ctrl;
                case 0x19:              /* contiguous gfx */
                    hid = 0;
                    sep = 0;
                    goto ctrl;
                case 0x1a:              /* separate gfx */
                    sep = 1;
                    goto ctrl;
                case 0x1c:              /* black bf */
                    bg = 0;
                    goto ctrl;
                case 0x1d:              /* new bg */
                    bg = fg;
                    goto ctrl;
                case 0x1e:              /* hold gfx */
                    hold = 1;
                    goto ctrl;
                case 0x1f:              /* release gfx */
                    hold = 0;
                    goto ctrl;
                case 0x0e:              /* SO */
                    goto ctrl;
                case 0x0f:              /* SI */
                    goto ctrl;
                case 0x1b:              /* ESC */
                    goto ctrl;
 
                ctrl:
                    c = ' ';
                    if (hold && gfx)
                        c = last_c;
                    break;
            }
            if (gfx)
                if ((c & 0xa0) == 0x20)
                {
                    last_c = c;
                    c += (c & 0x40) ? 32 : -32;
                }
            if (hid)
                c = ' ';

            if (visible || (c != ' '))
            {
                if (!visible)
                {
                    st.row = y;
                    st.col = x;
                    st.dbl = dbl;
                    st.fg  = fg;
                    st.bg  = bg;
                    linebufpos = linebuf;
                    *linebufpos = 0;
                }
                *linebufpos++ = c;
                *linebufpos = 0;
                visible = 1;
            }
        }
        if (visible)
        {
            st.len = linebufpos - linebuf + 1;;
            int max = 200;
            int bufsize = ((outpos - textbuffer[act]->buffer + 1) + st.len);
            if (bufsize > max)
                break;
            memcpy(outpos, &st, sizeof(st));
            outpos += sizeof(st);
            if (st.len < 42)
            {
                memcpy(outpos, linebuf, st.len);
                outpos += st.len;
            }
            else
            {
                memcpy(outpos, linebuf, 41);
                outpos += 41;
            }
            *outpos = 0;
        }
    }

    textbuffer[act]->bufferlen = outpos - textbuffer[act]->buffer + 1;
    textbuffer[act]->freeToBuffer = 0;
    act_text_buffer++;
    if (act_text_buffer >= text_buffer_count)
        act_text_buffer = 0;
    textbuffer[act]->freeToEncode = 1;
}

void NuppelVideoRecorder::FormatCC(struct cc *cc)
{
    struct timeval tnow;
    gettimeofday (&tnow, &tzone);

    // calculate timecode:
    // compute the difference  between now and stm (start time)
    int tc = (tnow.tv_sec - stm.tv_sec) * 1000 +
             tnow.tv_usec / 1000 - stm.tv_usec / 1000;

    ccd->FormatCC(tc, cc->code1, cc->code2);
}

void NuppelVideoRecorder::AddTextData(unsigned char *buf, int len,
                                      long long timecode, char /*type*/)
{
    int act = act_text_buffer;
    if (!textbuffer[act]->freeToBuffer)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + QString("Teletext#%1").arg(act) +
                " ran out of free TEXT buffers :-(");
        return;
    }

    textbuffer[act]->timecode = timecode;
    memcpy(textbuffer[act]->buffer, buf, len);
    textbuffer[act]->bufferlen = len + sizeof(ccsubtitle);

    textbuffer[act]->freeToBuffer = 0;
    act_text_buffer++;
    if (act_text_buffer >= text_buffer_count)
        act_text_buffer = 0;
    textbuffer[act]->freeToEncode = 1;
}

static void vbi_event(struct VBIData *data, struct vt_event *ev)
{
    switch (ev->type)
    {
       case EV_PAGE:
       {
            struct vt_page *vtp = (struct vt_page *) ev->p1;
            if (vtp->flags & PG_SUBTITLE)
            {
                //printf("subtitle page %x.%x\n", vtp->pgno, vtp->subno);
                data->foundteletextpage = true;
                memcpy(&(data->teletextpage), vtp, sizeof(vt_page));
            }
       }

       case EV_HEADER:
       case EV_XPACKET:
           break;
    }
}

void NuppelVideoRecorder::doVbiThread(void)
{
    struct vbi *vbi = NULL;
    struct cc *cc = NULL;
    int vbifd;

    switch (vbimode)
    {
        case 1:
            vbi = vbi_open(vbidevice.ascii(), NULL, 99, -1);
            if (!vbi)
            {
                VERBOSE(VB_IMPORTANT, QString("NVR: Can't open vbi device: %1").arg(vbidevice));
                return;
            }
            vbifd = vbi->fd;
            break;
        case 2:
            cc = cc_open(vbidevice.ascii());
            if (!cc)
            {
                VERBOSE(VB_IMPORTANT, QString("NVR: Can't open vbi device: %1").arg(vbidevice));
                return; 
            }
            vbifd = cc->fd;
            break;
        case 3:
        default:
            return;
    }

    struct VBIData vbicallbackdata;
    vbicallbackdata.nvr = this;

    if (vbimode == 1)
        vbi_add_handler(vbi, (void*) vbi_event, &vbicallbackdata);

    while (childrenLive) 
    {
        if (request_pause)
        {
            unpauseWait.wait(100);
            continue;
        }

        struct timeval tv;
        fd_set rdset;

        tv.tv_sec = 5;
        tv.tv_usec = 0;
        FD_ZERO(&rdset);
        FD_SET(vbifd, &rdset);

        switch (select(vbifd + 1, &rdset, 0, 0, &tv))
        {
            case -1:
                  perror("vbi select");
                  continue;
            case 0:
                  //printf("vbi select timeout\n");
                  continue;
        }

        switch (vbimode)
        {
            case 1:
                vbicallbackdata.foundteletextpage = false;
                vbi_handler(vbi, vbi->fd);
                if (vbicallbackdata.foundteletextpage)
                {
                    // decode VBI as teletext subtitles
                    FormatTeletextSubtitles(&vbicallbackdata);
                }
                break;
            case 2:
                cc_handler(cc);
                FormatCC(cc);
                break;
        }
    }

    switch (vbimode)
    {
        case 1:
            vbi_del_handler(vbi, (void*) vbi_event, &vbicallbackdata);
            vbi_close(vbi);
            vbi = NULL;
            break;
        case 2:
             cc_close(cc);
             cc = NULL;
             break;
        default:
             return;
    }
}


void NuppelVideoRecorder::doWriteThread(void)
{
    writepaused = false;
    while (childrenLive && !IsErrored())
    {
        if (request_pause)
        {
            writepaused = true;
            pauseWait.wakeAll();
            if (IsPaused() && tvrec)
                tvrec->RecorderPaused();

            unpauseWait.wait(100);
            continue;
        }
        writepaused = false;

        CheckForRingBufferSwitch();
 
        enum 
        { ACTION_NONE, 
          ACTION_VIDEO, 
          ACTION_AUDIO, 
          ACTION_TEXT 
        } action = ACTION_NONE;
        int firsttimecode = -1;

        if (videobuffer[act_video_encode]->freeToEncode)
        {
            action = ACTION_VIDEO;
            firsttimecode = videobuffer[act_video_encode]->timecode;
        }

        if (audio_buffer_count && 
            audiobuffer[act_audio_encode]->freeToEncode &&
            (action == ACTION_NONE ||
             (audiobuffer[act_audio_encode]->timecode < firsttimecode)))
        {
            action = ACTION_AUDIO;
            firsttimecode = audiobuffer[act_audio_encode]->timecode;
        }

        if (text_buffer_count &&
            textbuffer[act_text_encode]->freeToEncode &&
            (action == ACTION_NONE ||
             (textbuffer[act_text_encode]->timecode < firsttimecode)))
        {
            action = ACTION_TEXT;
        }

        switch (action)
        {
            case ACTION_VIDEO:
            {
                VideoFrame frame;
                frame.codec = FMT_YV12;
                frame.width = w;
                frame.height = h;
                frame.buf = videobuffer[act_video_encode]->buffer;
                frame.size = videobuffer[act_video_encode]->bufferlen;
                frame.frameNumber = videobuffer[act_video_encode]->sample;
                frame.timecode = videobuffer[act_video_encode]->timecode;
                frame.forcekey = videobuffer[act_video_encode]->forcekey;  
 
                WriteVideo(&frame);

                videobuffer[act_video_encode]->sample = 0;
                videobuffer[act_video_encode]->freeToEncode = 0;
                videobuffer[act_video_encode]->freeToBuffer = 1;
                videobuffer[act_video_encode]->forcekey = 0;
                act_video_encode++;
                if (act_video_encode >= video_buffer_count)
                    act_video_encode = 0;
                break;
            }
            case ACTION_AUDIO:
            {
                WriteAudio(audiobuffer[act_audio_encode]->buffer,
                           audiobuffer[act_audio_encode]->sample,
                           audiobuffer[act_audio_encode]->timecode);
                if (IsErrored()) {
                    VERBOSE(VB_IMPORTANT, "NVR: ACTION_AUDIO can not be completed due to error.");
                    StopRecording();
                    break;
                }
                audiobuffer[act_audio_encode]->sample = 0;
                audiobuffer[act_audio_encode]->freeToEncode = 0;
                audiobuffer[act_audio_encode]->freeToBuffer = 1; 
                act_audio_encode++;
                if (act_audio_encode >= audio_buffer_count) 
                    act_audio_encode = 0; 
                break;
            }
            case ACTION_TEXT:
            {
                WriteText(textbuffer[act_text_encode]->buffer,
                          textbuffer[act_text_encode]->bufferlen,
                          textbuffer[act_text_encode]->timecode,
                          textbuffer[act_text_encode]->pagenr);
                textbuffer[act_text_encode]->freeToEncode = 0;
                textbuffer[act_text_encode]->freeToBuffer = 1;
                act_text_encode++;
                if (act_text_encode >= text_buffer_count)
                    act_text_encode = 0;
                break;
            }
            default:
            {
                usleep(100);
                break;
            }
        }
    }
}

long long NuppelVideoRecorder::GetKeyframePosition(long long desired)
{
    QMutexLocker lock(&positionMapLock);

    long long ret = -1;

    if (positionMap.find(desired) != positionMap.end())
        ret = positionMap[desired];

    return ret;
}

void NuppelVideoRecorder::SetNextRecording(const ProgramInfo *progInf, 
                                           RingBuffer *rb)
{
    // First we do some of the time consuming stuff we can do now
    SavePositionMap(true);
    ringBuffer->WriterFlush();

    // Then we set the next info
    QMutexLocker locker(&nextRingBufferLock);
    nextRecording = NULL;
    if (progInf)
        nextRecording = new ProgramInfo(*progInf);
    nextRingBuffer = rb;
}

void NuppelVideoRecorder::ResetForNewFile(void)
{
    framesWritten = 0;
    lf = 0;
    last_block = 0;

    seektable->clear();

    positionMapLock.lock();
    positionMap.clear();
    positionMapDelta.clear();
    positionMapLock.unlock();

    resetcapture = true;
}

void NuppelVideoRecorder::StartNewFile(void)
{
    CreateNuppelFile();
}

void NuppelVideoRecorder::FinishRecording(void)
{
    ringBuffer->WriterFlush();

    WriteSeekTable();

    if (curRecording)
    {
        curRecording->SetFilesize(ringBuffer->GetRealFileSize());
        SavePositionMap(true);
    }
    positionMapLock.lock();
    positionMap.clear();
    positionMapDelta.clear();
    positionMapLock.unlock();
}

void NuppelVideoRecorder::WriteVideo(VideoFrame *frame, bool skipsync, 
                                     bool forcekey)
{
    int tmp = 0, r = 0, out_len = OUT_LEN;
    struct rtframeheader frameheader;
    int xaa, freecount = 0, compressthis;
    int raw = 0;
    int timeperframe = 40;
    uint8_t *planes[3];
    int len = frame->size;
    int fnum = frame->frameNumber;
    long long timecode = frame->timecode;
    unsigned char *buf = frame->buf;

    memset(&frameheader, 0, sizeof(frameheader));

    planes[0] = buf;
    planes[1] = planes[0] + frame->width * frame->height;
    if (picture_format == PIX_FMT_YUV422P)
        planes[2] = planes[1] + (frame->width * frame->height) / 2;
    else
        planes[2] = planes[1] + (frame->width * frame->height) / 4;
    compressthis = compression;

    if (lf == 0) 
    {   // this will be triggered every new file
        lf = fnum;
        startnum = fnum;
        lasttimecode = 0;
        frameofgop = 0;
        forcekey = true;
    }

    // count free buffers -- FIXME this can be done with less CPU time!!
    for (xaa = 0; xaa < video_buffer_count; xaa++) 
    {
        if (videobuffer[xaa]->freeToBuffer) 
            freecount++;
    }

    if (freecount < (video_buffer_count / 3)) 
        compressthis = 0; // speed up the encode process
    
    if (freecount < 5 || rawmode)
        raw = 1; // speed up the encode process
    
    if (raw==1 || compressthis==0) 
    {
        if (ringBuffer->IsIOBound())
        {
            /* need to compress, the disk can't handle any more bandwidth*/
            raw=0;
            compressthis=1;
        }
    }

    if (transcoding)
    {
        raw = 0;
        compressthis = 1;
    }

    // see if it's time for a seeker header, sync information and a keyframe
    frameheader.keyframe  = frameofgop;             // no keyframe defaulted

    bool wantkeyframe = forcekey;

    bool writesync = false;

    if (!go7007 && (((fnum-startnum)>>1) % keyframedist == 0 && !skipsync))
        writesync = true;
    else if (go7007 && frame->forcekey)
        writesync = true;

    if (writesync) 
    {
        frameheader.keyframe=0;
        frameofgop=0;
        ringBuffer->Write("RTjjjjjjjjjjjjjjjjjjjjjjjj", FRAMEHEADERSIZE);

        UpdateSeekTable(((fnum - startnum) >> 1) / keyframedist);

        frameheader.frametype    = 'S';           // sync frame
        frameheader.comptype     = 'V';           // video sync information
        frameheader.filters      = 0;             // no filters applied
        frameheader.packetlength = 0;             // no data packet
        frameheader.timecode     = (fnum-startnum)>>1;  
        // write video sync info
        WriteFrameheader(&frameheader);
        frameheader.frametype    = 'S';           // sync frame
        frameheader.comptype     = 'A';           // video sync information
        frameheader.filters      = 0;             // no filters applied
        frameheader.packetlength = 0;             // no data packet
        frameheader.timecode     = effectivedsp;  // effective dsp frequency
        // write audio sync info
        WriteFrameheader(&frameheader);

        wantkeyframe = true;
        //ringBuffer->Sync();
    }

    if (videoFilters)
        videoFilters->ProcessFrame(frame);

    if (useavcodec)
    {
        mpa_picture.data[0] = planes[0];
        mpa_picture.data[1] = planes[1];
        mpa_picture.data[2] = planes[2];
        mpa_picture.linesize[0] = frame->width;
        mpa_picture.linesize[1] = frame->width / 2;
        mpa_picture.linesize[2] = frame->width / 2;
        mpa_picture.pts = frame->frameNumber;
        mpa_picture.type = FF_BUFFER_TYPE_SHARED;

        if (wantkeyframe)
            mpa_picture.pict_type = FF_I_TYPE;
        else
            mpa_picture.pict_type = 0;

        if (!hardware_encode)
        {
            pthread_mutex_lock(&avcodeclock);
            tmp = avcodec_encode_video(mpa_ctx, (unsigned char *)strm, 
                                       len, &mpa_picture); 
            pthread_mutex_unlock(&avcodeclock);
        }
    }
    else
    {
        if (!raw) 
        {
            if (wantkeyframe)
                rtjc->SetNextKey();
            tmp = rtjc->Compress(strm, planes);
        }
        else 
            tmp = len;

        // here is lzo compression afterwards
        if (compressthis) {
            if (raw) 
                r = lzo1x_1_compress((unsigned char*)buf, len, 
                                     out, (lzo_uint *)&out_len, wrkmem);
            else
                r = lzo1x_1_compress((unsigned char *)strm, tmp, out,
                                     (lzo_uint *)&out_len, wrkmem);
            if (r != LZO_E_OK) 
            {
                VERBOSE(VB_IMPORTANT, "NVR: lzo compression failed");
                return;
            }
        }
    }

    dropped = (((fnum-lf)>>1) - 1); // should be += 0 ;-)
    
    if (dropped>0)
    {
        if (ntsc_framerate)
            timeperframe = (int)(1000 / (30 * framerate_multiplier));
        else
            timeperframe = (int)(1000 / (25 * framerate_multiplier));
    }
   
    // if we have lost frames we insert "copied" frames until we have the
    // exact count because of that we should have no problems with audio 
    // sync, as long as we don't loose audio samples :-/
  
    while (0 && dropped > 0) 
    {
        frameheader.timecode = lasttimecode + timeperframe;
        lasttimecode = frameheader.timecode;
        frameheader.keyframe  = frameofgop;             // no keyframe defaulted
        frameheader.packetlength =  0;   // no additional data needed
        frameheader.frametype    = 'V';  // last frame (or nullframe if first)
        frameheader.comptype    = 'L';
        WriteFrameheader(&frameheader);
        // we don't calculate sizes for lost frames for compression computation
        dropped--;
        frameofgop++;
    }

    frameheader.frametype = 'V'; // video frame
    frameheader.timecode  = timecode;
    lasttimecode = frameheader.timecode;
    frameheader.filters   = 0;             // no filters applied

    // compr ends here
    if (useavcodec)
    {
        if (mpa_codec->id == CODEC_ID_RAWVIDEO)
        {
            frameheader.comptype = '0';
            frameheader.packetlength = len;
            WriteFrameheader(&frameheader);
            ringBuffer->Write(buf, len);
        }
        else if (hardware_encode)
        {
            frameheader.comptype = '4';
            frameheader.packetlength = len;
            WriteFrameheader(&frameheader);
            ringBuffer->Write(buf, len);
        }
        else
        {
            frameheader.comptype = '4';
            frameheader.packetlength = tmp;
            WriteFrameheader(&frameheader);
            ringBuffer->Write(strm, tmp);
        }
    }
    else if (compressthis == 0 || (tmp < out_len)) 
    {
        if (!raw) 
        {
            frameheader.comptype  = '1'; // video compression: RTjpeg only
            frameheader.packetlength = tmp;
            WriteFrameheader(&frameheader);
            ringBuffer->Write(strm, tmp);
        } 
        else 
        {
            frameheader.comptype  = '0'; // raw YUV420
            frameheader.packetlength = len;
            WriteFrameheader(&frameheader);
            ringBuffer->Write(buf, len); // we write buf directly
        }
    } 
    else 
    {
        if (!raw) 
            frameheader.comptype  = '2'; // video compression: RTjpeg with lzo
        else
            frameheader.comptype  = '3'; // raw YUV420 with lzo
        frameheader.packetlength = out_len;
        WriteFrameheader(&frameheader);
        ringBuffer->Write(out, out_len);
    }

    frameofgop++;
    framesWritten++;

    // now we reset the last frame number so that we can find out
    // how many frames we didn't get next time
    lf = fnum;
}

void NuppelVideoRecorder::WriteAudio(unsigned char *buf, int fnum, int timecode)
{
    struct rtframeheader frameheader;
    double mt;
    double eff;
    double abytes;

    if (last_block == 0)
    {
        firsttc = -1;
    }

    if (last_block != 0) 
    {
        if (fnum != (last_block+1)) 
        {
            audio_behind = fnum - (last_block+1);
            VERBOSE(VB_RECORD, QString("NVR: audio behind %1 %2").
                    arg(last_block).arg(fnum));
        }
    }

    frameheader.frametype = 'A'; // audio frame
    frameheader.timecode = timecode;

    if (firsttc == -1) 
    {
        firsttc = timecode;
        //fprintf(stderr, "first timecode=%d\n", firsttc);
    } 
    else 
    {
        timecode -= firsttc; // this is to avoid the lack between the beginning
                             // of recording and the first timestamp, maybe we
                             // can calculate the audio-video +-lack at the 
                            // beginning too
        abytes = (double)audiobytes; // - (double)audio_buffer_size; 
                                     // wrong guess ;-)
        // need seconds instead of msec's
        mt = (double)timecode;
        if (mt > 0.0) 
        {
            eff = (abytes / mt) * (100000.0 / audio_bytes_per_sample);
            effectivedsp = (int)eff;
        }
    }

    if (compressaudio) 
    {
        char mp3gapless[7200];
        int compressedsize = 0;
        int gaplesssize = 0;
        int lameret = 0;

        if (audio_channels == 2)
        {
            lameret = lame_encode_buffer_interleaved(gf, (short int *)buf,
                                                     audio_buffer_size / 
                                                     audio_bytes_per_sample,
                                                     (unsigned char *)mp3buf,
                                                     mp3buf_size);
        }
        else
        {
            lameret = lame_encode_buffer(gf, (short int *)buf, (short int *)buf,
                                         audio_buffer_size / 
                                         audio_bytes_per_sample,
                                         (unsigned char *)mp3buf,
                                         mp3buf_size);
        }

        if (lameret < 0)
        {
            VERBOSE(VB_IMPORTANT, QString("NVR: lame error '%1'").arg(lameret));
            errored = true;
            return;
        }
        compressedsize = lameret;

        lameret = lame_encode_flush_nogap(gf, (unsigned char *)mp3gapless, 
                                          7200);
        if (lameret < 0)
        {
            VERBOSE(VB_IMPORTANT, QString("NVR: lame error '%1'").arg(lameret));
            errored = true;
            return;
        }
        gaplesssize = lameret;

        frameheader.comptype = '3'; // audio is compressed
        frameheader.packetlength = compressedsize + gaplesssize;

        if (frameheader.packetlength > 0)
        {
            WriteFrameheader(&frameheader);
            ringBuffer->Write(mp3buf, compressedsize);
            ringBuffer->Write(mp3gapless, gaplesssize);
        }
        audiobytes += audio_buffer_size;
    } 
    else 
    {
        frameheader.comptype = '0'; // uncompressed audio
        frameheader.packetlength = audio_buffer_size;

        WriteFrameheader(&frameheader);
        ringBuffer->Write(buf, audio_buffer_size);
        audiobytes += audio_buffer_size; // only audio no header!!
    }

    // this will probably never happen and if there would be a 
    // 'uncountable' video frame drop -> material==worthless
    if (audio_behind > 0) 
    {
        VERBOSE(VB_RECORD, "audio behind");
        frameheader.frametype = 'A'; // audio frame
        frameheader.comptype  = 'N'; // output a nullframe with
        frameheader.packetlength = 0;
        WriteFrameheader(&frameheader);
        audiobytes += audio_buffer_size;
        audio_behind--;
    }

    last_block = fnum;
}

void NuppelVideoRecorder::WriteText(unsigned char *buf, int len, int timecode, 
                                    int pagenr)
{
    struct rtframeheader frameheader;

    frameheader.frametype = 'T'; // text frame
    frameheader.timecode = timecode;

    if (vbimode == 1)
    {
        frameheader.comptype = 'T'; // european teletext
        frameheader.packetlength = sizeof(int) + len;

        WriteFrameheader(&frameheader);
        ringBuffer->Write(&pagenr, sizeof(int));
        ringBuffer->Write(buf, len);
    }
    else if (vbimode == 2)
    {
        frameheader.comptype = 'C';      // NTSC CC
        frameheader.packetlength = len;

        WriteFrameheader(&frameheader);
        ringBuffer->Write(buf, len);
    }
}
