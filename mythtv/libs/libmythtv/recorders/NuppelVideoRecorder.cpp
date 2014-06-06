#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "mythconfig.h"
#if HAVE_SYS_SOUNDCARD_H
    #include <sys/soundcard.h>
#elif HAVE_SOUNDCARD_H
    #include <soundcard.h>
#endif
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <cerrno>
#include <cmath>

#include <QStringList>

#include <iostream>
using namespace std;

#include "mythmiscutil.h"
#include "mythcontext.h"
#include "NuppelVideoRecorder.h"
#include "channelbase.h"
#include "filtermanager.h"
#include "recordingprofile.h"
#include "tv_rec.h"
#include "tv_play.h"
#include "audioinput.h"
#include "mythlogging.h"
#include "vbitext/cc.h"
#include "vbitext/vbi.h"
#include "mythavutil.h"

#if HAVE_BIGENDIAN
extern "C" {
#include "bswap.h"
}
#endif

extern "C" {
#include "libswscale/swscale.h"
}

#ifdef USING_V4L2
#include <linux/videodev2.h>

#include "go7007_myth.h"

#ifdef USING_V4L1
#include <linux/videodev.h>
#endif // USING_V4L1

#ifndef MJPIOC_S_PARAMS
#include "videodev_mjpeg.h"
#endif

#endif // USING_V4L2

#define KEYFRAMEDIST   30

#include "ringbuffer.h"
#include "RTjpegN.h"

#include "programinfo.h"

#define LOC QString("NVR(%1): ").arg(videodevice)

void NVRWriteThread::run(void)
{
    RunProlog();
    m_parent->doWriteThread();
    RunEpilog();
}

void NVRAudioThread::run(void)
{
    RunProlog();
    m_parent->doAudioThread();
    RunEpilog();
}

NuppelVideoRecorder::NuppelVideoRecorder(TVRec *rec, ChannelBase *channel) :
    V4LRecorder(rec), audio_device(NULL),
    write_thread(NULL), audio_thread(NULL)
{
    channelObj = channel;

    fd = -1;
    channelfd = -1;
    lf = tf = 0;
    M1 = 0, M2 = 0, Q = 255;
    pid = pid2 = 0;
    inputchannel = 1;
    compression = 1;
    compressaudio = 1;
    usebttv = 1;
    width  = 352;
    height = 240;
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
    v4l2_pixelformat = 0;

    avcodec_register_all();

    mpa_vidcodec = 0;
    mpa_vidctx = NULL;

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

    ccd = new CC608Decoder(this);

    go7007 = false;
    resetcapture = false;

    SetPositionMapType(MARK_KEYFRAME);
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
    if (audio_device)
    {
        delete audio_device;
        audio_device = NULL;
    }
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

    if (mpa_vidcodec)
    {
        QMutexLocker locker(avcodeclock);
        avcodec_close(mpa_vidctx);
    }

    if (mpa_vidctx)
        av_free(mpa_vidctx);
    mpa_vidctx = NULL;

    if (videoFilters)
        delete videoFilters;
    if (FiltMan)
        delete FiltMan;
    if (ccd)
        delete ccd;
}

void NuppelVideoRecorder::SetOption(const QString &opt, int value)
{
    if (opt == "width")
        w_out = width = value;
    else if (opt == "height")
        h_out = height = value;
    else if (opt == "rtjpegchromafilter")
        M1 = value;
    else if (opt == "rtjpeglumafilter")
        M2 = value;
    else if (opt == "rtjpegquality")
        Q = value;
    else if ((opt == "mpeg4bitrate") || (opt == "mpeg2bitrate"))
        targetbitrate = value;
    else if (opt == "scalebitrate")
        scalebitrate = value;
    else if (opt == "mpeg4maxquality")
    {
        if (value > 0)
            maxquality = value;
        else
            maxquality = 1;
    }
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
        V4LRecorder::SetOption(opt, value);
}

void NuppelVideoRecorder::SetOption(const QString &name, const QString &value)
{
    V4LRecorder::SetOption(name, value);
}

void NuppelVideoRecorder::SetOptionsFromProfile(RecordingProfile *profile,
                                                const QString &videodev,
                                                const QString &audiodev,
                                                const QString &vbidev)
{
    SetOption("videodevice", videodev);
    SetOption("vbidevice", vbidev);
    SetOption("tvformat", gCoreContext->GetSetting("TVFormat"));
    SetOption("vbiformat", gCoreContext->GetSetting("VbiFormat"));
    SetOption("audiodevice", audiodev);

    QString setting = QString::null;
    const Setting *tmp = profile->byName("videocodec");
    if (tmp)
        setting = tmp->getValue();

    if (setting == "MPEG-4")
    {
        SetOption("videocodec", "mpeg4");

        SetIntOption(profile, "mpeg4bitrate");
        SetIntOption(profile, "scalebitrate");
        SetIntOption(profile, "mpeg4maxquality");
        SetIntOption(profile, "mpeg4minquality");
        SetIntOption(profile, "mpeg4qualdiff");
#ifdef USING_FFMPEG_THREADS
        SetIntOption(profile, "encodingthreadcount");
#endif
        SetIntOption(profile, "mpeg4optionvhq");
        SetIntOption(profile, "mpeg4option4mv");
        SetIntOption(profile, "mpeg4optionidct");
        SetIntOption(profile, "mpeg4optionime");
    }
    else if (setting == "MPEG-2")
    {
        SetOption("videocodec", "mpeg2video");

        SetIntOption(profile, "mpeg2bitrate");
        SetIntOption(profile, "scalebitrate");
#ifdef USING_FFMPEG_THREADS
        SetIntOption(profile, "encodingthreadcount");
#endif
    }
    else if (setting == "RTjpeg")
    {
        SetOption("videocodec", "rtjpeg");

        SetIntOption(profile, "rtjpegquality");
        SetIntOption(profile, "rtjpegchromafilter");
        SetIntOption(profile, "rtjpeglumafilter");
    }
    else if (setting == "Hardware MJPEG")
    {
        SetOption("videocodec", "hardware-mjpeg");

        SetIntOption(profile, "hardwaremjpegquality");
        SetIntOption(profile, "hardwaremjpeghdecimation");
        SetIntOption(profile, "hardwaremjpegvdecimation");
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
                "Unknown video codec.  "
                "Please go into the TV Settings, Recording Profiles and "
                "setup the four 'Software Encoders' profiles.  "
                "Assuming RTjpeg for now.");

        SetOption("videocodec", "rtjpeg");

        SetIntOption(profile, "rtjpegquality");
        SetIntOption(profile, "rtjpegchromafilter");
        SetIntOption(profile, "rtjpeglumafilter");
    }

    setting = QString::null;
    if ((tmp = profile->byName("audiocodec")))
        setting = tmp->getValue();

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
        LOG(VB_GENERAL, LOG_ERR, LOC + "Unknown audio codec");
        SetOption("audiocompression", 0);
    }

    SetIntOption(profile, "volume");

    SetIntOption(profile, "width");
    SetIntOption(profile, "height");
}

void NuppelVideoRecorder::Pause(bool clear)
{
    QMutexLocker locker(&pauseLock);
    cleartimeonpause = clear;
    writepaused = audiopaused = mainpaused = false;
    request_pause = true;

    // The wakeAll is to make sure [write|audio|main]paused are
    // set immediately, even if we were already paused previously.
    unpauseWait.wakeAll();
}

bool NuppelVideoRecorder::IsPaused(bool holding_lock) const
{
    if (!holding_lock)
        pauseLock.lock();
    bool ret = audiopaused && mainpaused && writepaused;
    if (!holding_lock)
        pauseLock.unlock();
    return ret;
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

long long NuppelVideoRecorder::GetFramesWritten(void)
{
    return framesWritten;
}

int NuppelVideoRecorder::GetVideoFd(void)
{
    return channelfd;
}

bool NuppelVideoRecorder::SetupAVCodecVideo(void)
{
    if (!useavcodec)
        useavcodec = true;

    if (mpa_vidcodec)
    {
        QMutexLocker locker(avcodeclock);
        avcodec_close(mpa_vidctx);
    }

    if (mpa_vidctx)
        av_free(mpa_vidctx);
    mpa_vidctx = NULL;

    QByteArray vcodec = videocodec.toLatin1();
    mpa_vidcodec = avcodec_find_encoder_by_name(vcodec.constData());

    if (!mpa_vidcodec)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Video Codec not found: %1")
                .arg(vcodec.constData()));
        return false;
    }

    mpa_vidctx = avcodec_alloc_context3(NULL);

    switch (picture_format)
    {
        case PIX_FMT_YUV420P:
        case PIX_FMT_YUV422P:
        case PIX_FMT_YUVJ420P:
            mpa_vidctx->pix_fmt = picture_format;
            break;
        default:
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("Unknown picture format: %1")
                    .arg(picture_format));
    }

    mpa_vidctx->width = w_out;
    mpa_vidctx->height = (int)(height * height_multiplier);

    int usebitrate = targetbitrate * 1000;
    if (scalebitrate)
    {
        float diff = (w_out * h_out) / (640.0 * 480.0);
        usebitrate = (int)(diff * usebitrate);
    }

    if (targetbitrate == -1)
        usebitrate = -1;

    mpa_vidctx->time_base.den = (int)ceil(video_frame_rate * 100 *
                                    framerate_multiplier);
    mpa_vidctx->time_base.num = 100;

    // avcodec needs specific settings for mpeg2 compression
    switch (mpa_vidctx->time_base.den)
    {
        case 2397:
        case 2398: mpa_vidctx->time_base.den = 24000;
                   mpa_vidctx->time_base.num = 1001;
                   break;
        case 2997:
        case 2998: mpa_vidctx->time_base.den = 30000;
                   mpa_vidctx->time_base.num = 1001;
                   break;
        case 5994:
        case 5995: mpa_vidctx->time_base.den = 60000;
                   mpa_vidctx->time_base.num = 1001;
                   break;
    }

    mpa_vidctx->bit_rate = usebitrate;
    mpa_vidctx->bit_rate_tolerance = usebitrate * 100;
    mpa_vidctx->qmin = maxquality;
    mpa_vidctx->qmax = minquality;
    mpa_vidctx->max_qdiff = qualdiff;
    mpa_vidctx->flags = mp4opts;
    mpa_vidctx->mb_decision = mb_decision;

    mpa_vidctx->qblur = 0.5;
    mpa_vidctx->max_b_frames = 0;
    mpa_vidctx->b_quant_factor = 0;
    mpa_vidctx->rc_strategy = 2;
    mpa_vidctx->b_frame_strategy = 0;
    mpa_vidctx->gop_size = 30;
    mpa_vidctx->rc_max_rate = 0;
    mpa_vidctx->rc_min_rate = 0;
    mpa_vidctx->rc_buffer_size = 0;
    mpa_vidctx->rc_buffer_aggressivity = 1.0;
    mpa_vidctx->rc_override_count = 0;
    mpa_vidctx->rc_initial_cplx = 0;
    mpa_vidctx->dct_algo = FF_DCT_AUTO;
    mpa_vidctx->idct_algo = FF_IDCT_AUTO;
    mpa_vidctx->prediction_method = FF_PRED_LEFT;
    if (videocodec.toLower() == "huffyuv" || videocodec.toLower() == "mjpeg")
        mpa_vidctx->strict_std_compliance = FF_COMPLIANCE_UNOFFICIAL;
    mpa_vidctx->thread_count = encoding_thread_count;

    QMutexLocker locker(avcodeclock);

    if (avcodec_open2(mpa_vidctx, mpa_vidcodec, NULL) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Unable to open FFMPEG/%1 codec")
                .arg(videocodec));
        return false;
    }


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


void NuppelVideoRecorder::UpdateResolutions(void)
{
    int tot_height = (int)(height * height_multiplier);
    double aspectnum = w_out / (double)tot_height;
    uint aspect;

    if (aspectnum == 0.0)
        aspect = 0;
    else if (fabs(aspectnum - 1.3333333333333333) < 0.001)
        aspect = 2;
    else if (fabs(aspectnum - 1.7777777777777777) < 0.001)
        aspect = 3;
    else if (fabs(aspectnum - 2.21) < 0.001)
        aspect = 4;
    else
        aspect = aspectnum * 1000000;

    if ((aspect > 0) && (aspect != m_videoAspect))
    {
        m_videoAspect = aspect;
        AspectChange((AspectRatio)aspect, 0);
    }

    if (w_out && tot_height &&
        ((uint)tot_height != m_videoHeight ||
         (uint)w_out      != m_videoWidth))
    {
        m_videoHeight = tot_height;
        m_videoWidth = w_out;
        ResolutionChange(w_out, tot_height, 0);
    }

    int den = (int)ceil(video_frame_rate * 100 * framerate_multiplier);
    int num = 100;

    // avcodec needs specific settings for mpeg2 compression
    switch (den)
    {
        case 2397:
        case 2398: den = 24000;
                   num = 1001;
                   break;
        case 2997:
        case 2998: den = 30000;
                   num = 1001;
                   break;
        case 5994:
        case 5995: den = 60000;
                   num = 1001;
                   break;
    }

    FrameRate frameRate(den, num);
    if (frameRate.isNonzero() && frameRate != m_frameRate)
    {
        m_frameRate = frameRate;
        LOG(VB_RECORD, LOG_INFO, LOC + QString("NVR: frame rate = %1")
            .arg(frameRate.toDouble() * 1000));
        FrameRateChange(frameRate.toDouble() * 1000, 0);
    }
}

void NuppelVideoRecorder::Initialize(void)
{
    if (AudioInit() != 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to init audio input device");
    }

    if (videocodec == "hardware-mjpeg")
    {
        videocodec = "mjpeg";
        hardware_encode = true;

        MJPEGInit();

        width = hmjpg_maxw / hmjpg_hdecimation;

        if (ntsc)
        {
            switch (hmjpg_vdecimation)
            {
                case 2: height = 240; break;
                case 4: height = 120; break;
                default: height = 480; break;
            }
        }
        else
        {
            switch (hmjpg_vdecimation)
            {
                case 2: height = 288; break;
                case 4: height = 144; break;
                default: height = 576; break;
            }
        }
    }

    if (!ringBuffer)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Warning, old RingBuffer creation");
        ringBuffer = RingBuffer::Create("output.nuv", true);
        weMadeBuffer = true;
        livetv = false;
        if (!ringBuffer->IsOpen())
        {
            _error = "Could not open RingBuffer";
            LOG(VB_GENERAL, LOG_ERR, LOC + _error);
            return;
        }
    }
    else
        livetv = ringBuffer->LiveMode();

    audiobytes = 0;

    InitBuffers();
    InitFilters();
}

int NuppelVideoRecorder::AudioInit(bool skipdevice)
{
    int blocksize;
    int tmp;

    if (!skipdevice)
    {
        audio_device = AudioInput::CreateDevice(audiodevice.toLatin1());
        if (!audio_device)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Failed to create audio device: %1") .arg(audiodevice));
            return 1;
        }

        if (!audio_device->Open(audio_bits, audio_samplerate, audio_channels))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Failed to open audio device %1").arg(audiodevice));
            return 1;
        }

        if ((blocksize = audio_device->GetBlockSize()) <= 0)
        {
            blocksize = 1024;
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Failed to determine audio block size on %1,"
                        "using default 1024 bytes").arg(audiodevice));
        }

        audio_device->Close();
        audio_buffer_size = blocksize;
    }

    audio_bytes_per_sample = audio_channels * audio_bits / 8;
    LOG(VB_AUDIO, LOG_INFO, LOC +
        QString("Audio device %1 buffer size: %1 bytes")
            .arg(audio_buffer_size));

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
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("AudioInit(): lame_init_params error %1").arg(tmp));
            compressaudio = false;
        }

        if (audio_bits != 16)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "AudioInit(): lame support requires 16bit audio");
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
 *   means Channel cannot be open when NVR::Initialize() is called.
 *   It is safe for the recorder to be open.
 *
 *  \return true on success
 */
bool NuppelVideoRecorder::MJPEGInit(void)
{
#ifdef USING_V4L1
    bool we_opened_fd = false;
    int init_fd = fd;
    if (init_fd < 0)
    {
        QByteArray vdevice = videodevice.toLatin1();
        init_fd = open(vdevice.constData(), O_RDWR);
        we_opened_fd = true;

        if (init_fd < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Can't open video device" + ENO);
            return false;
        }
    }

    struct video_capability vc;
    memset(&vc, 0, sizeof(vc));
    int ret = ioctl(init_fd, VIDIOCGCAP, &vc);

    if (ret < 0)
        LOG(VB_GENERAL, LOG_ERR, LOC + "Can't query V4L capabilities" + ENO);

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
#endif // USING_V4L1

    LOG(VB_GENERAL, LOG_ERR, LOC + "MJPEG not supported by device");
    return false;
}

void NuppelVideoRecorder::InitFilters(void)
{
    int btmp = video_buffer_size;
    if (videoFilters)
        delete videoFilters;

    QString tmpVideoFilterList;

    w_out = width;
    h_out = height;
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

    if (width >= 480 || height > 288)
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

void NuppelVideoRecorder::StreamAllocate(void)
{
    strm = new signed char[width * height * 2 + 10];
}

bool NuppelVideoRecorder::Open(void)
{
    if (channelfd>0)
        return true;

    int retries = 0;
    QByteArray vdevice = videodevice.toLatin1();
    fd = open(vdevice.constData(), O_RDWR);
    while (fd < 0)
    {
        usleep(30000);
        fd = open(vdevice.constData(), O_RDWR);
        if (retries++ > 5)
        {
            _error = QString("Can't open video device: %1").arg(videodevice);
            LOG(VB_GENERAL, LOG_ERR, LOC + _error + ENO);
            KillChildren();
            return false;
        }
    }

    channelfd = fd;
    return true;
}

void NuppelVideoRecorder::ProbeV4L2(void)
{
#ifdef USING_V4L2
    usingv4l2 = true;

    struct v4l2_capability vcap;
    memset(&vcap, 0, sizeof(vcap));

    if (ioctl(channelfd, VIDIOC_QUERYCAP, &vcap) < 0)
    {
        usingv4l2 = false;
    }

    if (usingv4l2 && !(vcap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Not a v4l2 capture device, falling back to v4l");
        usingv4l2 = false;
    }

    if (usingv4l2 && !(vcap.capabilities & V4L2_CAP_STREAMING))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Won't work with the streaming interface, falling back");
        usingv4l2 = false;
    }

    if (vcap.card[0] == 'B' && vcap.card[1] == 'T' &&
        vcap.card[2] == '8' && vcap.card[4] == '8')
        correct_bttv = true;

    QString driver = (char *)vcap.driver;
    if (driver == "go7007")
        go7007 = true;
#endif // USING_V4L2
}

void NuppelVideoRecorder::run(void)
{
    if (lzo_init() != LZO_E_OK)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "lzo_init() failed, exiting");
        _error = "lzo_init() failed, exiting";
        LOG(VB_GENERAL, LOG_ERR, LOC + _error);
        return;
    }

    if (!Open())
    {
        _error = "Failed to open device";
        LOG(VB_GENERAL, LOG_ERR, LOC + _error);
        return;
    }

    ProbeV4L2();

    if (usingv4l2 && !SetFormatV4L2())
    {
        _error = "Failed to set V4L2 format";
        LOG(VB_GENERAL, LOG_ERR, LOC + _error);
        return;
    }

    StreamAllocate();

    positionMapLock.lock();
    positionMap.clear();
    positionMapDelta.clear();
    positionMapLock.unlock();

    if (videocodec.toLower() == "rtjpeg")
        useavcodec = false;
    else
        useavcodec = true;

    if (useavcodec)
        useavcodec = SetupAVCodecVideo();

    if (!useavcodec)
        SetupRTjpeg();

    UpdateResolutions();

    if (CreateNuppelFile() != 0)
    {
        _error = QString("Cannot open '%1' for writing")
            .arg(ringBuffer->GetFilename());
        LOG(VB_GENERAL, LOG_ERR, LOC + _error);
        return;
    }

    if (IsHelperRequested())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Children are already alive");
        _error = "Children are already alive";
        return;
    }

    {
        QMutexLocker locker(&pauseLock);
        request_recording = true;
        request_helper = true;
        recording = true;
        recordingWait.wakeAll();
    }

    write_thread = new NVRWriteThread(this);
    write_thread->start();

    audio_thread = new NVRAudioThread(this);
    audio_thread->start();

    if ((vbimode != VBIMode::None) && (OpenVBIDevice() >= 0))
        vbi_thread = new VBIThread(this);

    // save the start time
    gettimeofday(&stm, &tzone);

    // try to get run at higher scheduling priority, ignore failure
    myth_nice(-10);

    if (usingv4l2)
    {
        inpixfmt = FMT_NONE;
        InitFilters();
        DoV4L2();
    }
    else
        DoV4L1();

    {
        QMutexLocker locker(&pauseLock);
        request_recording = false;
        request_helper = false;
        recording = false;
        recordingWait.wakeAll();
    }
}

#ifdef USING_V4L1
void NuppelVideoRecorder::DoV4L1(void)
{
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
        QString tmp = "VIDIOCGCAP: " + ENO;
        KillChildren();
        LOG(VB_GENERAL, LOG_ERR, tmp);
        _error = tmp;
        return;
    }

    int channelinput = 0;

    if (channelObj)
        channelinput = channelObj->GetCurrentInputNum();

    vchan.channel = channelinput;

    if (ioctl(fd, VIDIOCGCHAN, &vchan) < 0)
        LOG(VB_GENERAL, LOG_ERR, LOC + "VIDIOCGCHAN: " + ENO);

    // Set volume level for audio recording (unless feature is disabled).
    if (!skip_btaudio)
    {
        // v4l1 compat in Linux 2.6.18 does not set VIDEO_VC_AUDIO,
        // so we just use VIDIOCGAUDIO unconditionally.. then only
        // report a get failure as an error if VIDEO_VC_AUDIO is set.
        if (ioctl(fd, VIDIOCGAUDIO, &va) < 0)
        {
            bool reports_audio = vchan.flags & VIDEO_VC_AUDIO;
            uint err_level = reports_audio ? VB_GENERAL : VB_AUDIO;
            // print at VB_GENERAL if driver reports audio.
            LOG(err_level, LOG_ERR, LOC + "Failed to get audio" + ENO);
        }
        else
        {
            // if channel has a audio then activate it
            va.flags &= ~VIDEO_AUDIO_MUTE; // now this really has to work
            va.volume = volume * 65535 / 100;
            if (ioctl(fd, VIDIOCSAUDIO, &va) < 0)
                LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to set audio" + ENO);
        }
    }

    if ((vc.type & VID_TYPE_MJPEG_ENCODER) && hardware_encode)
    {
        DoMJPEG();
        _error = "MJPEG requested but not available.";
        LOG(VB_GENERAL, LOG_ERR, LOC + _error);
        return;
    }

    inpixfmt = FMT_NONE;
    InitFilters();

    if (ioctl(fd, VIDIOCGMBUF, &vm) < 0)
    {
        QString tmp = "VIDIOCGMBUF: " + ENO;
        KillChildren();
        LOG(VB_GENERAL, LOG_ERR, LOC + tmp);
        _error = tmp;
        return;
    }

    if (vm.frames < 2)
    {
        QString tmp = "need a minimum of 2 capture buffers";
        KillChildren();
        LOG(VB_GENERAL, LOG_ERR, LOC + tmp);
        _error = tmp;
        return;
    }

    int frame;

    unsigned char *buf = (unsigned char *)mmap(0, vm.size,
                                               PROT_READ|PROT_WRITE,
                                               MAP_SHARED,
                                               fd, 0);
    if (buf <= 0)
    {
        QString tmp = "mmap: " + ENO;
        KillChildren();
        LOG(VB_GENERAL, LOG_ERR, LOC + tmp);
        _error = tmp;
        return;
    }

    mm.height = height;
    mm.width  = width;
    if (inpixfmt == FMT_YUV422P)
        mm.format = VIDEO_PALETTE_YUV422P;
    else
        mm.format = VIDEO_PALETTE_YUV420P;

    mm.frame  = 0;
    if (ioctl(fd, VIDIOCMCAPTURE, &mm)<0)
        LOG(VB_GENERAL, LOG_ERR, LOC + "VIDIOCMCAPTUREi0: " + ENO);
    mm.frame  = 1;
    if (ioctl(fd, VIDIOCMCAPTURE, &mm)<0)
        LOG(VB_GENERAL, LOG_ERR, LOC + "VIDIOCMCAPTUREi1: " + ENO);

    int syncerrors = 0;

    while (IsRecordingRequested() && !IsErrored())
    {
        {
            QMutexLocker locker(&pauseLock);
            if (request_pause)
            {
                if (!mainpaused)
                {
                    mainpaused = true;
                    pauseWait.wakeAll();
                    if (IsPaused(true) && tvrec)
                        tvrec->RecorderPaused();
                }
                unpauseWait.wait(&pauseLock, 100);
                if (cleartimeonpause)
                    gettimeofday(&stm, &tzone);
                continue;
            }

            if (!request_pause && mainpaused)
            {
                mainpaused = false;
                unpauseWait.wakeAll();
            }
        }

        frame = 0;
        mm.frame = 0;
        if (ioctl(fd, VIDIOCSYNC, &frame)<0)
        {
            syncerrors++;
            if (syncerrors == 10)
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "Multiple bttv errors, further messages supressed");
            else if (syncerrors < 10)
                LOG(VB_GENERAL, LOG_ERR, LOC + "VIDIOCSYNC: " + ENO);
        }
        else
        {
            BufferIt(buf+vm.offsets[0], video_buffer_size);
            //memset(buf+vm.offsets[0], 0, video_buffer_size);
        }

        if (ioctl(fd, VIDIOCMCAPTURE, &mm)<0)
            LOG(VB_GENERAL, LOG_ERR, LOC + "VIDIOCMCAPTURE0: " + ENO);

        frame = 1;
        mm.frame = 1;
        if (ioctl(fd, VIDIOCSYNC, &frame)<0)
        {
            syncerrors++;
            if (syncerrors == 10)
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "Multiple bttv errors, further messages supressed");
            else if (syncerrors < 10)
                LOG(VB_GENERAL, LOG_ERR, LOC + "VIDIOCSYNC: " + ENO);
        }
        else
        {
            BufferIt(buf+vm.offsets[1], video_buffer_size);
            //memset(buf+vm.offsets[1], 0, video_buffer_size);
        }
        if (ioctl(fd, VIDIOCMCAPTURE, &mm)<0)
            LOG(VB_GENERAL, LOG_ERR, LOC + "VIDIOCMCAPTURE1: " + ENO);
    }

    munmap(buf, vm.size);

    KillChildren();

    FinishRecording();

    close(fd);
}
#else // if !USING_V4L1
void NuppelVideoRecorder::DoV4L1(void) {}
#endif // !USING_V4L1

#ifdef USING_V4L2
bool NuppelVideoRecorder::SetFormatV4L2(void)
{
    struct v4l2_format     vfmt;
    memset(&vfmt, 0, sizeof(vfmt));

    vfmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    vfmt.fmt.pix.width = width;
    vfmt.fmt.pix.height = height;
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
            // this is supported by the HVR-950q
            vfmt.fmt.pix.pixelformat = V4L2_PIX_FMT_UYVY;
            if (ioctl(fd, VIDIOC_S_FMT, &vfmt) < 0)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "v4l2: Unable to set desired format");
                return false;
            }
            else
            {
                // we need to convert the buffer - we can't deal with uyvy
                // directly.
                if (inpixfmt == FMT_YUV422P)
                {
                    LOG(VB_GENERAL, LOG_ERR, LOC +
                        "v4l2: uyvy format supported, but yuv422 requested.");
                    LOG(VB_GENERAL, LOG_ERR, LOC +
                        "v4l2: unfortunately, this converter hasn't been "
                        "written yet, exiting");
                    return false;
                }
                LOG(VB_RECORD, LOG_INFO, LOC +
                    "v4l2: format set, getting uyvy from v4l, converting");
            }
        }
        else
        {
            // we need to convert the buffer - we can't deal with yuyv directly.
            if (inpixfmt == FMT_YUV422P)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "v4l2: yuyv format supported, but yuv422 requested.");
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "v4l2: unfortunately, this converter hasn't been written "
                    "yet, exiting");
                return false;
            }
            LOG(VB_RECORD, LOG_INFO, LOC +
                "v4l2: format set, getting yuyv from v4l, converting");
        }
    }
    else // cool, we can do our preferred format, most likely running on bttv.
        LOG(VB_RECORD, LOG_INFO, LOC +
            "v4l2: format set, getting yuv420 from v4l");

    // VIDIOC_S_FMT might change the format, check it
    if (width  != (int)vfmt.fmt.pix.width ||
        height != (int)vfmt.fmt.pix.height)
    {
        LOG(VB_RECORD, LOG_INFO, LOC +
            QString("v4l2: resolution changed. requested %1x%2, using "
                    "%3x%4 now")
                .arg(width).arg(height)
                .arg(vfmt.fmt.pix.width) .arg(vfmt.fmt.pix.height));
        w_out = width  = vfmt.fmt.pix.width;
        h_out = height = vfmt.fmt.pix.height;
    }

    v4l2_pixelformat = vfmt.fmt.pix.pixelformat;

    return true;
}
#else // if !USING_V4L2
bool NuppelVideoRecorder::SetFormatV4L2(void) { return false; }
#endif // !USING_V4L2

#ifdef USING_V4L2
#define MAX_VIDEO_BUFFERS 5
void NuppelVideoRecorder::DoV4L2(void)
{
    struct v4l2_buffer     vbuf;
    struct v4l2_requestbuffers vrbuf;
    struct v4l2_control    vc;

    memset(&vbuf, 0, sizeof(vbuf));
    memset(&vrbuf, 0, sizeof(vrbuf));
    memset(&vc, 0, sizeof(vc));

    vc.id = V4L2_CID_AUDIO_MUTE;
    vc.value = 0;

    if (ioctl(fd, VIDIOC_S_CTRL, &vc) < 0)
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "VIDIOC_S_CTRL:V4L2_CID_AUDIO_MUTE: " + ENO);

    if (go7007)
    {
        struct go7007_comp_params comp;
        struct go7007_mpeg_params mpeg;

        memset(&comp, 0, sizeof(comp));
        comp.gop_size = keyframedist;
        comp.max_b_frames = 0;

        if (fabs(video_aspect - 1.33333) < 0.01f)
        {
            if (ntsc)
                comp.aspect_ratio = GO7007_ASPECT_RATIO_4_3_NTSC;
            else
                comp.aspect_ratio = GO7007_ASPECT_RATIO_4_3_PAL;
        }
        else if (fabs(video_aspect - 1.77777) < 0.01f)
        {
            if (ntsc)
                comp.aspect_ratio = GO7007_ASPECT_RATIO_16_9_NTSC;
            else
                comp.aspect_ratio = GO7007_ASPECT_RATIO_16_9_PAL;
        }
        else
        {
            comp.aspect_ratio = GO7007_ASPECT_RATIO_1_1;
        }

        comp.flags |= GO7007_COMP_CLOSED_GOP;
        if (ioctl(fd, GO7007IOC_S_COMP_PARAMS, &comp) < 0)
        {
            _error = "Unable to set compression params";
            LOG(VB_GENERAL, LOG_ERR, LOC + _error);
            return;
        }

        memset(&mpeg, 0, sizeof(mpeg));

        if (videocodec == "mpeg2video")
            mpeg.mpeg_video_standard = GO7007_MPEG_VIDEO_MPEG2;
        else
            mpeg.mpeg_video_standard = GO7007_MPEG_VIDEO_MPEG4;

        if (ioctl(fd, GO7007IOC_S_MPEG_PARAMS, &mpeg) < 0)
        {
            _error = "Unable to set MPEG params";
            LOG(VB_GENERAL, LOG_ERR, LOC + _error);
            return;
        }

        int usebitrate = targetbitrate * 1000;
        if (scalebitrate)
        {
            float diff = (width * height) / (640.0 * 480.0);
            usebitrate = (int)(diff * usebitrate);
        }

        if (ioctl(fd, GO7007IOC_S_BITRATE, &usebitrate) < 0)
        {
            _error = "Unable to set bitrate";
            LOG(VB_GENERAL, LOG_ERR, LOC + _error);
            return;
        }

        hardware_encode = true;
    }

    uint numbuffers = MAX_VIDEO_BUFFERS;

    vrbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    vrbuf.memory = V4L2_MEMORY_MMAP;
    vrbuf.count = numbuffers;

    if (ioctl(fd, VIDIOC_REQBUFS, &vrbuf) < 0)
    {
        _error = "Not able to get any capture buffers, exiting";
        LOG(VB_GENERAL, LOG_ERR, LOC + _error);
        return;
    }

    if (vrbuf.count < numbuffers)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Requested %1 buffers, but only %2 are available. "
                    "Proceeding anyway").arg(numbuffers).arg(vrbuf.count));
    }

    numbuffers = vrbuf.count;

    unsigned char *buffers[MAX_VIDEO_BUFFERS];
    int bufferlen[MAX_VIDEO_BUFFERS];

    for (uint i = 0; i < numbuffers; i++)
    {
        vbuf.type = vrbuf.type;
        vbuf.index = i;

        if (ioctl(fd, VIDIOC_QUERYBUF, &vbuf) < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("unable to query capture buffer %1").arg(i));
            _error = "Unable to query capture buffer";
            return;
        }

        buffers[i] = (unsigned char *)mmap(NULL, vbuf.length,
                                           PROT_READ|PROT_WRITE, MAP_SHARED,
                                           fd, vbuf.m.offset);

        if (buffers[i] == MAP_FAILED)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "mmap: " + ENO);
            LOG(VB_GENERAL, LOG_ERR, LOC + "Memory map failed");
            _error = "Memory map failed";
            return;
        }
        bufferlen[i] = vbuf.length;
    }

    for (uint i = 0; i < numbuffers; i++)
    {
        memset(buffers[i], 0, bufferlen[i]);
        vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        vbuf.index = i;
        if (ioctl(fd, VIDIOC_QBUF, &vbuf) < 0)
            LOG(VB_GENERAL, LOG_ERR, LOC + "unable to enqueue capture buffer (VIDIOC_QBUF failed) " + ENO);
    }

    int turnon = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMON, &turnon) < 0)
        LOG(VB_GENERAL, LOG_ERR, LOC + "unable to start capture (VIDIOC_STREAMON failed) " + ENO);

    struct timeval tv;
    fd_set rdset;
    int frame = 0;
    bool forcekey = false;

    resetcapture = false;

    // setup pixel format conversions for YUYV and UYVY
    uint8_t *output_buffer = NULL;
    struct SwsContext *convert_ctx = NULL;
    AVPicture img_out;
    if (v4l2_pixelformat == V4L2_PIX_FMT_YUYV ||
        v4l2_pixelformat == V4L2_PIX_FMT_UYVY)
    {
        PixelFormat in_pixfmt = v4l2_pixelformat == V4L2_PIX_FMT_YUYV ?
                                PIX_FMT_YUYV422 :
                                PIX_FMT_UYVY422;

        output_buffer = (uint8_t*)av_malloc(height * width * 3 / 2);
        if (!output_buffer)
        {
            _error = "Cannot initialize image conversion buffer";
            LOG(VB_GENERAL, LOG_ERR, LOC + _error);
            return;
        }

        convert_ctx = sws_getCachedContext(convert_ctx, width, height, in_pixfmt,
                                           width, height, PIX_FMT_YUV420P,
                                           SWS_FAST_BILINEAR, NULL, NULL, NULL);
        if (!convert_ctx)
        {
            _error = "Cannot initialize image conversion context";
            av_free(output_buffer);
            LOG(VB_GENERAL, LOG_ERR, LOC + _error);
            return;
        }

        avpicture_fill(&img_out, output_buffer, PIX_FMT_YUV420P, width, height);
    }

    while (IsRecordingRequested() && !IsErrored())
    {
again:
        {
            QMutexLocker locker(&pauseLock);
            if (request_pause)
            {
                if (!mainpaused)
                {
                    mainpaused = true;
                    pauseWait.wakeAll();
                    if (IsPaused(true) && tvrec)
                        tvrec->RecorderPaused();
                }
                unpauseWait.wait(&pauseLock, 100);
                if (cleartimeonpause)
                    gettimeofday(&stm, &tzone);
                continue;
            }

            if (!request_pause && mainpaused)
            {
                mainpaused = false;
                unpauseWait.wakeAll();
            }
        }

        if (resetcapture)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Resetting and re-queueing");
            turnon = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if (ioctl(fd, VIDIOC_STREAMOFF, &turnon) < 0)
                LOG(VB_GENERAL, LOG_ERR, LOC + "unable to stop capture (VIDIOC_STREAMOFF failed) " + ENO);

            for (uint i = 0; i < numbuffers; i++)
            {
                memset(buffers[i], 0, bufferlen[i]);
                vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                vbuf.index = i;
                if (ioctl(fd, VIDIOC_QBUF, &vbuf) < 0)
                    LOG(VB_GENERAL, LOG_ERR, LOC + "unable to enqueue capture buffer (VIDIOC_QBUF failed) " + ENO);
             }

             if (ioctl(fd, VIDIOC_STREAMON, &turnon) < 0)
                LOG(VB_GENERAL, LOG_ERR, LOC + "unable to start capture (VIDIOC_STREAMON failed) " + ENO);
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
                  LOG(VB_GENERAL, LOG_ERR, LOC + "select: " + ENO);
                  continue;
            case 0:
                  LOG(VB_GENERAL, LOG_INFO, LOC + "select timeout");
                  continue;
           default: break;
        }

        memset(&vbuf, 0, sizeof(vbuf));
        vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        vbuf.memory = V4L2_MEMORY_MMAP;
        if (ioctl(fd, VIDIOC_DQBUF, &vbuf) < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "DQBUF ioctl failed." + ENO);

            // EIO failed DQBUF de-tunes post 2.6.15.3 for cx88
            // EIO or EINVAL on bttv means we need to reset the buffers..
            if (errno == EIO && channelObj)
            {
                channelObj->Retune();
                resetcapture = true;
                continue;
            }

            if (errno == EIO || errno == EINVAL)
            {
                resetcapture = true;
                continue;
            }

            if (errno == EAGAIN)
                continue;
        }

        frame = vbuf.index;
        if (go7007)
            forcekey = vbuf.flags & V4L2_BUF_FLAG_KEYFRAME;

        if (!request_pause)
        {
            if (v4l2_pixelformat == V4L2_PIX_FMT_YUYV)
            {
                AVPicture img_in;
                avpicture_fill(&img_in, buffers[frame], PIX_FMT_YUYV422, width, height);
                sws_scale(convert_ctx, img_in.data, img_in.linesize,
                          0, height, img_out.data, img_out.linesize);
                BufferIt(output_buffer, video_buffer_size);
            }
            else if (v4l2_pixelformat == V4L2_PIX_FMT_UYVY)
            {
                AVPicture img_in;
                avpicture_fill(&img_in, buffers[frame], PIX_FMT_UYVY422, width, height);
                sws_scale(convert_ctx, img_in.data, img_in.linesize,
                          0, height, img_out.data, img_out.linesize);
                BufferIt(output_buffer, video_buffer_size);
            }
            else
            {
                // buffer the frame directly
                BufferIt(buffers[frame], vbuf.bytesused, forcekey);
            }
        }

        vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(fd, VIDIOC_QBUF, &vbuf) < 0)
            LOG(VB_GENERAL, LOG_ERR, LOC + "unable to enqueue capture buffer (VIDIOC_QBUF failed) " + ENO);
    }

    KillChildren();

    if (ioctl(fd, VIDIOC_STREAMOFF, &turnon) < 0)
        LOG(VB_GENERAL, LOG_ERR, LOC + "unable to stop capture (VIDIOC_STREAMOFF failed) " + ENO);

    for (uint i = 0; i < numbuffers; i++)
    {
        munmap(buffers[i], bufferlen[i]);
    }

    FinishRecording();

    av_free(output_buffer);
    sws_freeContext(convert_ctx);

    close(fd);
    close(channelfd);
}
#else // if !USING_V4L2
void NuppelVideoRecorder::DoV4L2(void) {}
#endif // !USING_V4L2

#ifdef USING_V4L1
void NuppelVideoRecorder::DoMJPEG(void)
{
    struct mjpeg_params bparm;

    if (ioctl(fd, MJPIOC_G_PARAMS, &bparm) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "MJPIOC_G_PARAMS: " + ENO);
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
        LOG(VB_GENERAL, LOG_DEBUG, LOC + "MJPIOC_S_PARAMS: " + ENO);
        return;
    }

    struct mjpeg_requestbuffers breq;

    breq.count = 64;
    breq.size = 256 * 1024;

    if (ioctl(fd, MJPIOC_REQBUFS, &breq) < 0)
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC + "MJPIOC_REQBUFS: " + ENO);
        return;
    }

    uint8_t *MJPG_buff = (uint8_t *)mmap(0, breq.count * breq.size,
                                         PROT_READ|PROT_WRITE, MAP_SHARED, fd,
                                         0);

    if (MJPG_buff == MAP_FAILED)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "mapping mjpeg buffers");
        return;
    }

    struct mjpeg_sync bsync;

    for (unsigned int count = 0; count < breq.count; count++)
    {
        if (ioctl(fd, MJPIOC_QBUF_CAPT, &count) < 0)
            LOG(VB_GENERAL, LOG_ERR, LOC + "MJPIOC_QBUF_CAPT: " + ENO);
    }

    while (IsRecordingRequested() && !IsErrored())
    {
        {
            QMutexLocker locker(&pauseLock);
            if (request_pause)
            {
                if (!mainpaused)
                {
                    mainpaused = true;
                    pauseWait.wakeAll();
                    if (IsPaused(true) && tvrec)
                        tvrec->RecorderPaused();
                }
                unpauseWait.wait(&pauseLock, 100);
                if (cleartimeonpause)
                    gettimeofday(&stm, &tzone);
                continue;
            }

            if (!request_pause && mainpaused)
            {
                mainpaused = false;
                unpauseWait.wakeAll();
            }
        }

        if (ioctl(fd, MJPIOC_SYNC, &bsync) < 0)
        {
            _error = "MJPEG sync error";
            LOG(VB_GENERAL, LOG_ERR, LOC + _error + ENO);
            break;
        }

        BufferIt((unsigned char *)(MJPG_buff + bsync.frame * breq.size),
                 bsync.length);

        if (ioctl(fd, MJPIOC_QBUF_CAPT, &(bsync.frame)) < 0)
        {
            _error = "MJPEG Capture error";
            LOG(VB_GENERAL, LOG_ERR, LOC + _error + ENO);
        }
    }

    munmap(MJPG_buff, breq.count * breq.size);
    KillChildren();

    FinishRecording();

    close(fd);
}
#else // if !USING_V4L1
void NuppelVideoRecorder::DoMJPEG(void) {}
#endif // !USING_V4L1

void NuppelVideoRecorder::KillChildren(void)
{
    {
        QMutexLocker locker(&pauseLock);
        request_helper = false;
        unpauseWait.wakeAll();
    }

    if (write_thread)
    {
        write_thread->wait();
        delete write_thread;
        write_thread = NULL;
    }

    if (audio_thread)
    {
        audio_thread->wait();
        delete audio_thread;
        audio_thread = NULL;
    }

    if (vbi_thread)
    {
        vbi_thread->wait();
        delete vbi_thread;
        vbi_thread = NULL;
        CloseVBIDevice();
    }
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
        LOG(VB_GENERAL, LOG_INFO, LOC +
            "DROPPED frame due to full buffer in the recorder.");
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
#if HAVE_BIGENDIAN
    fh->timecode     = bswap_32(fh->timecode);
    fh->packetlength = bswap_32(fh->packetlength);
#endif
    ringBuffer->Write(fh, FRAMEHEADERSIZE);
}

void NuppelVideoRecorder::SetNewVideoParams(double newaspect)
{
    if (newaspect == video_aspect)
        return;

    video_aspect = newaspect;

    struct rtframeheader frameheader;
    memset(&frameheader, 0, sizeof(frameheader));

    frameheader.frametype = 'S';
    frameheader.comptype = 'M';
    frameheader.packetlength = sizeof(struct rtfileheader);

    WriteFrameheader(&frameheader);

    WriteFileHeader();
}

void NuppelVideoRecorder::WriteFileHeader(void)
{
    struct rtfileheader fileheader;
    static const char finfo[12] = "MythTVVideo";
    static const char vers[5]   = "0.07";

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

#if HAVE_BIGENDIAN
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
}

void NuppelVideoRecorder::WriteHeader(void)
{
    struct rtframeheader frameheader;
    static unsigned long int tbls[128];

    if (!videoFilters)
        InitFilters();

    WriteFileHeader();

    memset(&frameheader, 0, sizeof(frameheader));
    frameheader.frametype = 'D'; // compressor data

    if (useavcodec)
    {
        frameheader.comptype = 'F';
        frameheader.packetlength = mpa_vidctx->extradata_size;

        WriteFrameheader(&frameheader);
        ringBuffer->Write(mpa_vidctx->extradata, frameheader.packetlength);
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
        switch(mpa_vidcodec->id)
        {
            case AV_CODEC_ID_MPEG4:      vidfcc = FOURCC_DIVX; break;
            case AV_CODEC_ID_WMV1:       vidfcc = FOURCC_WMV1; break;
            case AV_CODEC_ID_MSMPEG4V3:  vidfcc = FOURCC_DIV3; break;
            case AV_CODEC_ID_MSMPEG4V2:  vidfcc = FOURCC_MP42; break;
            case AV_CODEC_ID_MSMPEG4V1:  vidfcc = FOURCC_MPG4; break;
            case AV_CODEC_ID_MJPEG:      vidfcc = FOURCC_MJPG; break;
            case AV_CODEC_ID_H263:       vidfcc = FOURCC_H263; break;
            case AV_CODEC_ID_H263P:      vidfcc = FOURCC_H263; break;
            case AV_CODEC_ID_H263I:      vidfcc = FOURCC_I263; break;
            case AV_CODEC_ID_MPEG1VIDEO: vidfcc = FOURCC_MPEG; break;
            case AV_CODEC_ID_MPEG2VIDEO: vidfcc = FOURCC_MPG2; break;
            case AV_CODEC_ID_HUFFYUV:    vidfcc = FOURCC_HFYU; break;
            default: break;
        }
        moredata.video_fourcc = vidfcc;
        moredata.lavc_bitrate = mpa_vidctx->bit_rate;
        moredata.lavc_qmin = mpa_vidctx->qmin;
        moredata.lavc_qmax = mpa_vidctx->qmax;
        moredata.lavc_maxqdiff = mpa_vidctx->max_qdiff;
    }
    else
    {
        moredata.video_fourcc = FOURCC_RJPG;
        moredata.rtjpeg_quality = Q;
        moredata.rtjpeg_luma_filter = M1;
        moredata.rtjpeg_chroma_filter = M2;
    }

    if (compressaudio)
    {
        moredata.audio_fourcc = FOURCC_LAME;
        moredata.audio_compression_ratio = 11;
        moredata.audio_quality = mp3quality;
    }
    else
    {
        moredata.audio_fourcc = FOURCC_RAWA;
    }

    moredata.audio_sample_rate = audio_samplerate;
    moredata.audio_channels = audio_channels;
    moredata.audio_bits_per_sample = audio_bits;

    extendeddataOffset = ringBuffer->GetWritePosition();

#if HAVE_BIGENDIAN
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

    vector<struct seektable_entry>::iterator it = seektable->begin();
    for (; it != seektable->end(); ++it)
    {
        memcpy(seekbuf + offset, (const void *)&(*it),
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
    const vector<struct kfatable_entry> &kfa_table)
{
    int numentries = kfa_table.size();

    struct rtframeheader frameheader;
    memset(&frameheader, 0, sizeof(frameheader));
    frameheader.frametype = 'K'; // KFA Table
    frameheader.packetlength = sizeof(struct kfatable_entry) * numentries;

    long long currentpos = ringBuffer->GetWritePosition();

    ringBuffer->Write(&frameheader, sizeof(frameheader));

    char *kfa_buf = new char[frameheader.packetlength];
    uint offset = 0;

    vector<struct kfatable_entry>::const_iterator it = kfa_table.begin();
    for (; it != kfa_table.end() ; ++it)
    {
        memcpy(kfa_buf + offset, &(*it),
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

void NuppelVideoRecorder::UpdateSeekTable(int frame_num, long offset)
{
    long long position = ringBuffer->GetWritePosition() + offset;
    struct seektable_entry ste;
    ste.file_offset = position;
    ste.keyframe_number = frame_num;
    seektable->push_back(ste);

    positionMapLock.lock();
    if (!positionMap.contains(ste.keyframe_number))
    {
        positionMapDelta[ste.keyframe_number] = position;
        positionMap[ste.keyframe_number] = position;
        lastPositionMapPos = position;
    }
    positionMapLock.unlock();
}

int NuppelVideoRecorder::CreateNuppelFile(void)
{
    framesWritten = 0;

    if (!ringBuffer)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "No ringbuffer, recorder wasn't initialized.");
        return -1;
    }

    if (!ringBuffer->IsOpen())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Ringbuffer isn't open");
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
        SetupAVCodecVideo();

    if (curRecording)
        curRecording->ClearPositionMap(MARK_KEYFRAME);
}

void NuppelVideoRecorder::doAudioThread(void)
{
    if (!audio_device)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Invalid audio device (%1), exiting").arg(audiodevice));
        return;
    }

    if (!audio_device->Open(audio_bits, audio_samplerate, audio_channels))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Failed to open audio device %1").arg(audiodevice));
        return;
    }

    if (!audio_device->Start())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Failed to start audio capture on %1").arg(audiodevice));
        return;
    }

    struct timeval anow;
    unsigned char *buffer = new unsigned char[audio_buffer_size];
    int act = 0, lastread = 0;
    audio_bytes_per_sample = audio_channels * audio_bits / 8;

    while (IsHelperRequested() && !IsErrored())
    {
        {
            QMutexLocker locker(&pauseLock);
            if (request_pause)
            {
                if (!audiopaused)
                {
                    audiopaused = true;
                    pauseWait.wakeAll();
                    if (IsPaused(true) && tvrec)
                        tvrec->RecorderPaused();
                }
                unpauseWait.wait(&pauseLock, 100);
                continue;
            }

            if (!request_pause && audiopaused)
            {
                audiopaused = false;
                unpauseWait.wakeAll();
            }
        }

        if (!IsHelperRequested() || IsErrored())
            break;

        lastread = audio_device->GetSamples(buffer, audio_buffer_size);
        if (audio_buffer_size != lastread)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Short read, %1 of %2 bytes from ")
                    .arg(lastread).arg(audio_buffer_size) + audiodevice);
        }

        /* record the current time */
        /* Don't assume that the sound device's record buffer is empty
           (like we used to.) Measure to see how much stuff is in there,
           and correct for it when calculating the timestamp */
        gettimeofday(&anow, &tzone);
        int bytes_read = max(audio_device->GetNumReadyBytes(), 0);

        act = act_audio_buffer;

        if (!audiobuffer[act]->freeToBuffer)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Ran out of free AUDIO buffers :-(");
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
                (bytes_read + audio_buffer_size)
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

    if (audio_device->IsOpen())
        audio_device->Close();
}

#ifdef USING_V4L2
void NuppelVideoRecorder::FormatTT(struct VBIData *vbidata)
{
    struct timeval tnow;
    gettimeofday(&tnow, &tzone);

    int act = act_text_buffer;
    if (!textbuffer[act]->freeToBuffer)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Teletext #%1: ran out of free TEXT buffers :-(").arg(act));
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
    struct teletextsubtitle st = { 0 };
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

            (void) box;
            (void) sep;
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
#else  // USING_V4L2
void NuppelVideoRecorder::FormatTT(struct VBIData*) {}
#endif // USING_V4L2

void NuppelVideoRecorder::FormatCC(uint code1, uint code2)
{
    struct timeval tnow;
    gettimeofday (&tnow, &tzone);

    // calculate timecode:
    // compute the difference  between now and stm (start time)
    int tc = (tnow.tv_sec - stm.tv_sec) * 1000 +
             tnow.tv_usec / 1000 - stm.tv_usec / 1000;

    ccd->FormatCC(tc, code1, code2);
}

void NuppelVideoRecorder::AddTextData(unsigned char *buf, int len,
                                      int64_t timecode, char /*type*/)
{
    int act = act_text_buffer;
    if (!textbuffer[act]->freeToBuffer)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Teletext#%1").arg(act) +
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

void NuppelVideoRecorder::doWriteThread(void)
{
    while (IsHelperRequested() && !IsErrored())
    {
        {
            QMutexLocker locker(&pauseLock);
            if (request_pause)
            {
                if (!writepaused)
                {
                    writepaused = true;
                    pauseWait.wakeAll();
                    if (IsPaused(true) && tvrec)
                        tvrec->RecorderPaused();
                }
                unpauseWait.wait(&pauseLock, 100);
                continue;
            }

            if (!request_pause && writepaused)
            {
                writepaused = false;
                unpauseWait.wakeAll();
            }
        }

        if (!IsHelperRequested() || IsErrored())
            break;

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
                init(&frame,
                     FMT_YV12, videobuffer[act_video_encode]->buffer,
                     width, height, videobuffer[act_video_encode]->bufferlen);

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
                    LOG(VB_GENERAL, LOG_ERR, LOC +
                        "ACTION_AUDIO cannot be completed due to error.");
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

void NuppelVideoRecorder::ResetForNewFile(void)
{
    framesWritten = 0;
    lf = 0;
    last_block = 0;

    seektable->clear();

    ClearStatistics();

    positionMapLock.lock();
    positionMap.clear();
    positionMapDelta.clear();
    positionMapLock.unlock();

    if (go7007)
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
        curRecording->SaveFilesize(ringBuffer->GetRealFileSize());
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
    int tmp = 0;
    lzo_uint out_len = OUT_LEN;
    struct rtframeheader frameheader;
    int raw = 0, compressthis = compression;
    uint8_t *planes[3];
    int len = frame->size;
    int fnum = frame->frameNumber;
    long long timecode = frame->timecode;
    unsigned char *buf = frame->buf;

    memset(&frameheader, 0, sizeof(frameheader));

    planes[0] = buf;
    planes[1] = planes[0] + frame->width * frame->height;
    planes[2] = planes[1] + (frame->width * frame->height) /
                            (picture_format == PIX_FMT_YUV422P ? 2 : 4);

    if (lf == 0)
    {   // this will be triggered every new file
        lf = fnum;
        startnum = fnum;
        lasttimecode = 0;
        frameofgop = 0;
        forcekey = true;
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

    if (wantkeyframe)
    {
        frameheader.keyframe=0;
        frameofgop=0;
    }

    if (videoFilters)
        videoFilters->ProcessFrame(frame);

    if (useavcodec)
    {
        MythAVFrame mpa_picture;

        switch (picture_format)
        {
            case PIX_FMT_YUV420P:
            case PIX_FMT_YUV422P:
            case PIX_FMT_YUVJ420P:
                mpa_picture->linesize[0] = w_out;
                mpa_picture->linesize[1] = w_out / 2;
                mpa_picture->linesize[2] = w_out / 2;
                break;
        }
        mpa_picture->data[0] = planes[0];
        mpa_picture->data[1] = planes[1];
        mpa_picture->data[2] = planes[2];
        mpa_picture->linesize[0] = frame->width;
        mpa_picture->linesize[1] = frame->width / 2;
        mpa_picture->linesize[2] = frame->width / 2;
        mpa_picture->type = FF_BUFFER_TYPE_SHARED;

        if (wantkeyframe)
            mpa_picture->pict_type = AV_PICTURE_TYPE_I;
        else
            mpa_picture->pict_type = AV_PICTURE_TYPE_NONE;

        if (!hardware_encode)
        {
            AVPacket packet;
            av_init_packet(&packet);
            packet.data = (uint8_t *)strm;
            packet.size = len;

            int got_packet = 0;

            QMutexLocker locker(avcodeclock);
            tmp = avcodec_encode_video2(mpa_vidctx, &packet, mpa_picture,
                                        &got_packet);

            if (tmp < 0 || !got_packet)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "WriteVideo : avcodec_encode_video() failed");
                return;
            }

            tmp = packet.size;
        }
    }
    else
    {
        int freecount = 0;
        freecount = act_video_buffer > act_video_encode ?
                    video_buffer_count - (act_video_buffer - act_video_encode) :
                    act_video_encode - act_video_buffer;

        if (freecount < (video_buffer_count / 3))
            compressthis = 0; // speed up the encode process

        if (freecount < 5)
            raw = 1; // speed up the encode process

        if (transcoding)
        {
            raw = 0;
            compressthis = 1;
        }

        if (!raw)
        {
            if (wantkeyframe)
                rtjc->SetNextKey();
            tmp = rtjc->Compress(strm, planes);
        }
        else
            tmp = len;

        // here is lzo compression afterwards
        if (compressthis)
        {
            int r = 0;
            if (raw)
                r = lzo1x_1_compress((unsigned char*)buf, len,
                                     out, &out_len, wrkmem);
            else
                r = lzo1x_1_compress((unsigned char *)strm, tmp, out,
                                     &out_len, wrkmem);
            if (r != LZO_E_OK)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + "lzo compression failed");
                return;
            }
        }
    }

    frameheader.frametype = 'V'; // video frame
    frameheader.timecode  = timecode;
    lasttimecode = frameheader.timecode;
    frameheader.filters   = 0;             // no filters applied

    // compr ends here
    if (useavcodec)
    {
        if (mpa_vidcodec->id == AV_CODEC_ID_RAWVIDEO)
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
    else if (compressthis == 0 || (tmp < (int)out_len))
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

#if HAVE_BIGENDIAN
static void bswap_16_buf(short int *buf, int buf_cnt, int audio_channels)
    __attribute__ ((unused)); /* <- suppress compiler warning */

static void bswap_16_buf(short int *buf, int buf_cnt, int audio_channels)
{
    for (int i = 0; i < audio_channels * buf_cnt; i++)
        buf[i] = bswap_16(buf[i]);
}
#endif

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
            LOG(VB_RECORD, LOG_INFO, LOC + QString("audio behind %1 %2").
                    arg(last_block).arg(fnum));
        }
    }

    frameheader.frametype = 'A'; // audio frame
    frameheader.timecode = timecode;

    if (firsttc == -1)
    {
        firsttc = timecode;
#if 0
        LOG(VB_GENERAL, LOG_DEBUG, LOC +
            QString("first timecode=%1").arg(firsttc));
#endif
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

        int sample_cnt = audio_buffer_size / audio_bytes_per_sample;

#if HAVE_BIGENDIAN
        bswap_16_buf((short int*) buf, sample_cnt, audio_channels);
#endif

        if (audio_channels == 2)
        {
            lameret = lame_encode_buffer_interleaved(
                gf, (short int*) buf, sample_cnt,
                (unsigned char*) mp3buf, mp3buf_size);
        }
        else
        {
            lameret = lame_encode_buffer(
                gf, (short int*) buf, (short int*) buf, sample_cnt,
                (unsigned char*) mp3buf, mp3buf_size);
        }

        if (lameret < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("lame error '%1'").arg(lameret));
            _error = QString("Audio Encoding Error '%1'")
                .arg(lameret);
            return;
        }
        compressedsize = lameret;

        lameret = lame_encode_flush_nogap(gf, (unsigned char *)mp3gapless,
                                          7200);
        if (lameret < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("lame error '%1'").arg(lameret));
            _error = QString("Audio Encoding Error '%1'")
                .arg(lameret);
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
        LOG(VB_RECORD, LOG_INFO, LOC + "audio behind");
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

    if (VBIMode::PAL_TT == vbimode)
    {
        frameheader.comptype = 'T'; // european teletext
        frameheader.packetlength = len + 4;
        WriteFrameheader(&frameheader);
        union page_t {
            int32_t val32;
            struct { int8_t a,b,c,d; } val8;
        } v;
        v.val32 = pagenr;
        ringBuffer->Write(&v.val8.d, sizeof(int8_t));
        ringBuffer->Write(&v.val8.c, sizeof(int8_t));
        ringBuffer->Write(&v.val8.b, sizeof(int8_t));
        ringBuffer->Write(&v.val8.a, sizeof(int8_t));
        ringBuffer->Write(buf, len);
    }
    else if (VBIMode::NTSC_CC == vbimode)
    {
        frameheader.comptype = 'C'; // NTSC CC
        frameheader.packetlength = len;

        WriteFrameheader(&frameheader);
        ringBuffer->Write(buf, len);
    }
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */

