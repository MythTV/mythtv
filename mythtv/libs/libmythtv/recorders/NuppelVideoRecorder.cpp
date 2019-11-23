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
#include "fourcc.h"

#if HAVE_BIGENDIAN
extern "C" {
#include "bswap.h"
}
#endif

extern "C" {
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
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

#include "ringbuffer.h"
#include "RTjpegN.h"

#include "programinfo.h"
#include "mythsystemevent.h"

#define LOC QString("NVR(%1): ").arg(m_videodevice)

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
    V4LRecorder(rec)
{
    m_channelObj = channel;
    m_seektable = new vector<struct seektable_entry>;
    m_filtMan = new FilterManager;
    m_ccd = new CC608Decoder(this);

    SetPositionMapType(MARK_KEYFRAME);

    m_containerFormat = formatNUV;
}

NuppelVideoRecorder::~NuppelVideoRecorder(void)
{
    if (m_weMadeBuffer && m_ringBuffer)
    {
        delete m_ringBuffer;
        m_ringBuffer = nullptr;
    }
    delete m_rtjc;
    delete [] m_mp3buf;
    if (m_gf)
        lame_close(m_gf);
    delete [] m_strm;
    if (m_audio_device)
    {
        delete m_audio_device;
        m_audio_device = nullptr;
    }
    if (m_fd >= 0)
        close(m_fd);
    if (m_seektable)
    {
        m_seektable->clear();
        delete m_seektable;
    }

    while (!videobuffer.empty())
    {
        struct vidbuffertype *vb = videobuffer.back();
        delete [] vb->buffer;
        delete vb;
        videobuffer.pop_back();
    }
    while (!audiobuffer.empty())
    {
        struct audbuffertype *ab = audiobuffer.back();
        delete [] ab->buffer;
        delete ab;
        audiobuffer.pop_back();
    }
    while (!textbuffer.empty())
    {
        struct txtbuffertype *tb = textbuffer.back();
        delete [] tb->buffer;
        delete tb;
        textbuffer.pop_back();
    }

    if (m_mpa_vidcodec)
    {
        QMutexLocker locker(avcodeclock);
        avcodec_free_context(&m_mpa_vidctx);
    }

    delete m_videoFilters;
    delete m_filtMan;
    delete m_ccd;
}

void NuppelVideoRecorder::SetOption(const QString &opt, int value)
{
    if (opt == "width")
        m_w_out = m_width = value;
    else if (opt == "height")
        m_h_out = m_height = value;
    else if (opt == "rtjpegchromafilter")
        m_M1 = value;
    else if (opt == "rtjpeglumafilter")
        m_M2 = value;
    else if (opt == "rtjpegquality")
        m_Q = value;
    else if ((opt == "mpeg4bitrate") || (opt == "mpeg2bitrate"))
        m_targetbitrate = value;
    else if (opt == "scalebitrate")
        m_scalebitrate = value;
    else if (opt == "mpeg4maxquality")
    {
        if (value > 0)
            m_maxquality = value;
        else
            m_maxquality = 1;
    }
    else if (opt == "mpeg4minquality")
        m_minquality = value;
    else if (opt == "mpeg4qualdiff")
        m_qualdiff = value;
    else if (opt == "encodingthreadcount")
        m_encoding_thread_count = value;
    else if (opt == "mpeg4optionvhq")
    {
        if (value)
            m_mb_decision = FF_MB_DECISION_RD;
        else
            m_mb_decision = FF_MB_DECISION_SIMPLE;
    }
    else if (opt == "mpeg4option4mv")
    {
        if (value)
            m_mp4opts |= AV_CODEC_FLAG_4MV;
        else
            m_mp4opts &= ~AV_CODEC_FLAG_4MV;
    }
    else if (opt == "mpeg4optionidct")
    {
        if (value)
            m_mp4opts |= AV_CODEC_FLAG_INTERLACED_DCT;
        else
            m_mp4opts &= ~AV_CODEC_FLAG_INTERLACED_DCT;
    }
    else if (opt == "mpeg4optionime")
    {
        if (value)
            m_mp4opts |= AV_CODEC_FLAG_INTERLACED_ME;
        else
            m_mp4opts &= ~AV_CODEC_FLAG_INTERLACED_ME;
    }
    else if (opt == "hardwaremjpegquality")
        m_hmjpg_quality = value;
    else if (opt == "hardwaremjpeghdecimation")
        m_hmjpg_hdecimation = value;
    else if (opt == "hardwaremjpegvdecimation")
        m_hmjpg_vdecimation = value;
    else if (opt == "audiocompression")
        m_compressaudio = (value != 0);
    else if (opt == "mp3quality")
        m_mp3quality = value;
    else if (opt == "samplerate")
        m_audio_samplerate = value;
    else if (opt == "audioframesize")
        m_audio_buffer_size = value;
    else if (opt == "pip_mode")
        m_pip_mode = value;
    else if (opt == "inpixfmt")
        m_inpixfmt = (VideoFrameType)value;
    else if (opt == "skipbtaudio")
        m_skip_btaudio = (value != 0);
    else if (opt == "volume")
        m_volume = value;
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

    QString setting;
    const StandardSetting *tmp = profile->byName("videocodec");
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

    setting.clear();
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
    QMutexLocker locker(&m_pauseLock);
    m_cleartimeonpause = clear;
    m_writepaused = m_audiopaused = m_mainpaused = false;
    m_request_pause = true;

    // The wakeAll is to make sure [write|audio|main]paused are
    // set immediately, even if we were already paused previously.
    m_unpauseWait.wakeAll();
}

bool NuppelVideoRecorder::IsPaused(bool holding_lock) const
{
    if (!holding_lock)
        m_pauseLock.lock();
    bool ret = m_audiopaused && m_mainpaused && m_writepaused;
    if (!holding_lock)
        m_pauseLock.unlock();
    return ret;
}

void NuppelVideoRecorder::SetVideoFilters(QString &filters)
{
    m_videoFilterList = filters;
    InitFilters();
}

bool NuppelVideoRecorder::IsRecording(void)
{
    return m_recording;
}

long long NuppelVideoRecorder::GetFramesWritten(void)
{
    return m_framesWritten;
}

int NuppelVideoRecorder::GetVideoFd(void)
{
    return m_channelfd;
}

bool NuppelVideoRecorder::SetupAVCodecVideo(void)
{
    if (!m_useavcodec)
        m_useavcodec = true;

    if (m_mpa_vidcodec)
    {
        QMutexLocker locker(avcodeclock);
        avcodec_free_context(&m_mpa_vidctx);
    }

    QByteArray vcodec = m_videocodec.toLatin1();
    m_mpa_vidcodec = avcodec_find_encoder_by_name(vcodec.constData());

    if (!m_mpa_vidcodec)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Video Codec not found: %1")
                .arg(vcodec.constData()));
        return false;
    }

    m_mpa_vidctx = avcodec_alloc_context3(nullptr);

    switch (m_picture_format)
    {
        case AV_PIX_FMT_YUV420P:
        case AV_PIX_FMT_YUV422P:
        case AV_PIX_FMT_YUVJ420P:
            m_mpa_vidctx->pix_fmt = m_picture_format;
            break;
        default:
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("Unknown picture format: %1")
                    .arg(m_picture_format));
    }

    m_mpa_vidctx->width = m_w_out;
    m_mpa_vidctx->height = (int)(m_height * m_height_multiplier);

    int usebitrate = m_targetbitrate * 1000;
    if (m_scalebitrate)
    {
        float diff = (m_w_out * m_h_out) / (640.0 * 480.0);
        usebitrate = (int)(diff * usebitrate);
    }

    if (m_targetbitrate == -1)
        usebitrate = -1;

    m_mpa_vidctx->time_base.den = (int)ceil(m_video_frame_rate * 100 *
                                    m_framerate_multiplier);
    m_mpa_vidctx->time_base.num = 100;

    // avcodec needs specific settings for mpeg2 compression
    switch (m_mpa_vidctx->time_base.den)
    {
        case 2397:
        case 2398: m_mpa_vidctx->time_base.den = 24000;
                   m_mpa_vidctx->time_base.num = 1001;
                   break;
        case 2997:
        case 2998: m_mpa_vidctx->time_base.den = 30000;
                   m_mpa_vidctx->time_base.num = 1001;
                   break;
        case 5994:
        case 5995: m_mpa_vidctx->time_base.den = 60000;
                   m_mpa_vidctx->time_base.num = 1001;
                   break;
    }

    AVDictionary *opts = nullptr;

    m_mpa_vidctx->bit_rate = usebitrate;
    m_mpa_vidctx->bit_rate_tolerance = usebitrate * 100;
    m_mpa_vidctx->qmin = m_maxquality;
    m_mpa_vidctx->qmax = m_minquality;
    m_mpa_vidctx->max_qdiff = m_qualdiff;
    m_mpa_vidctx->flags = m_mp4opts;
    m_mpa_vidctx->mb_decision = m_mb_decision;

    m_mpa_vidctx->qblur = 0.5;
    m_mpa_vidctx->max_b_frames = 0;
    m_mpa_vidctx->b_quant_factor = 0;
    av_dict_set(&opts, "rc_strategy", "2", 0);
    av_dict_set(&opts, "b_strategy", "0", 0);
    m_mpa_vidctx->gop_size = 30;
    m_mpa_vidctx->rc_max_rate = 0;
    m_mpa_vidctx->rc_min_rate = 0;
    m_mpa_vidctx->rc_buffer_size = 0;
    m_mpa_vidctx->rc_override_count = 0;
    av_dict_set(&opts, "rc_init_cplx", "0", 0);
    m_mpa_vidctx->dct_algo = FF_DCT_AUTO;
    m_mpa_vidctx->idct_algo = FF_IDCT_AUTO;
    av_dict_set_int(&opts, "pred", FF_PRED_LEFT, 0);
    if (m_videocodec.toLower() == "huffyuv" || m_videocodec.toLower() == "mjpeg")
        m_mpa_vidctx->strict_std_compliance = FF_COMPLIANCE_UNOFFICIAL;
    m_mpa_vidctx->thread_count = m_encoding_thread_count;

    QMutexLocker locker(avcodeclock);

    if (avcodec_open2(m_mpa_vidctx, m_mpa_vidcodec, &opts) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Unable to open FFMPEG/%1 codec")
                .arg(m_videocodec));
        return false;
    }


    return true;
}

void NuppelVideoRecorder::SetupRTjpeg(void)
{
    m_picture_format = AV_PIX_FMT_YUV420P;

    int setval;
    m_rtjc = new RTjpeg();
    setval = RTJ_YUV420;
    m_rtjc->SetFormat(&setval);
    setval = (int)(m_h_out * m_height_multiplier);
    m_rtjc->SetSize(&m_w_out, &setval);
    m_rtjc->SetQuality(&m_Q);
    setval = 2;
    m_rtjc->SetIntra(&setval, &m_M1, &m_M2);
}


void NuppelVideoRecorder::UpdateResolutions(void)
{
    int tot_height = (int)(m_height * m_height_multiplier);
    double aspectnum = m_w_out / (double)tot_height;
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

    if (m_w_out && tot_height &&
        ((uint)tot_height != m_videoHeight ||
         (uint)m_w_out      != m_videoWidth))
    {
        m_videoHeight = tot_height;
        m_videoWidth = m_w_out;
        ResolutionChange(m_w_out, tot_height, 0);
    }

    int den = (int)ceil(m_video_frame_rate * 100 * m_framerate_multiplier);
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

    if (m_videocodec == "hardware-mjpeg")
    {
        m_videocodec = "mjpeg";
        m_hardware_encode = true;

        MJPEGInit();

        m_width = m_hmjpg_maxw / m_hmjpg_hdecimation;

        if (m_ntsc)
        {
            switch (m_hmjpg_vdecimation)
            {
                case 2: m_height = 240; break;
                case 4: m_height = 120; break;
                default: m_height = 480; break;
            }
        }
        else
        {
            switch (m_hmjpg_vdecimation)
            {
                case 2: m_height = 288; break;
                case 4: m_height = 144; break;
                default: m_height = 576; break;
            }
        }
    }

    if (!m_ringBuffer)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Warning, old RingBuffer creation");
        m_ringBuffer = RingBuffer::Create("output.nuv", true);
        m_weMadeBuffer = true;
        m_livetv = false;
        if (!m_ringBuffer || !m_ringBuffer->IsOpen())
        {
            m_error = "Could not open RingBuffer";
            LOG(VB_GENERAL, LOG_ERR, LOC + m_error);
            return;
        }
    }
    else
        m_livetv = m_ringBuffer->LiveMode();

    m_audiobytes = 0;

    InitBuffers();
    InitFilters();
}

int NuppelVideoRecorder::AudioInit(bool skipdevice)
{
    if (!skipdevice)
    {
        int blocksize;
        m_audio_device = AudioInput::CreateDevice(m_audiodevice.toLatin1());
        if (!m_audio_device)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Failed to create audio device: %1") .arg(m_audiodevice));
            return 1;
        }

        if (!m_audio_device->Open(m_audio_bits, m_audio_samplerate, m_audio_channels))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Failed to open audio device %1").arg(m_audiodevice));
            return 1;
        }

        if ((blocksize = m_audio_device->GetBlockSize()) <= 0)
        {
            blocksize = 1024;
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Failed to determine audio block size on %1,"
                        "using default 1024 bytes").arg(m_audiodevice));
        }

        m_audio_device->Close();
        m_audio_buffer_size = blocksize;
    }

    m_audio_bytes_per_sample = m_audio_channels * m_audio_bits / 8;
    LOG(VB_AUDIO, LOG_INFO, LOC +
        QString("Audio device %1 buffer size: %1 bytes")
            .arg(m_audio_buffer_size));

    if (m_compressaudio)
    {
        int tmp;
        m_gf = lame_init();
        lame_set_bWriteVbrTag(m_gf, 0);
        lame_set_quality(m_gf, m_mp3quality);
        lame_set_compression_ratio(m_gf, 11);
        lame_set_mode(m_gf, m_audio_channels == 2 ? STEREO : MONO);
        lame_set_num_channels(m_gf, m_audio_channels);
        lame_set_in_samplerate(m_gf, m_audio_samplerate);
        if ((tmp = lame_init_params(m_gf)) != 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("AudioInit(): lame_init_params error %1").arg(tmp));
            m_compressaudio = false;
        }

        if (m_audio_bits != 16)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "AudioInit(): lame support requires 16bit audio");
            m_compressaudio = false;
        }
    }
    m_mp3buf_size = (int)(1.25 * 16384 + 7200);
    m_mp3buf = new char[m_mp3buf_size];

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
    int init_fd = m_fd;
    if (init_fd < 0)
    {
        QByteArray vdevice = m_videodevice.toLatin1();
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
            m_hmjpg_maxw = 768;
        else if (vc.maxwidth >= 704)
            m_hmjpg_maxw = 704;
        else
            m_hmjpg_maxw = 640;
        return true;
    }
#endif // USING_V4L1

    LOG(VB_GENERAL, LOG_ERR, LOC + "MJPEG not supported by device");
    return false;
}

void NuppelVideoRecorder::InitFilters(void)
{
    int btmp = m_video_buffer_size;
    delete m_videoFilters;

    QString tmpVideoFilterList;

    m_w_out = m_width;
    m_h_out = m_height;
    VideoFrameType tmp = FMT_YV12;

    if (m_correct_bttv && !m_videoFilterList.contains("adjust"))
    {
        if (m_videoFilterList.isEmpty())
            tmpVideoFilterList = "adjust";
        else
            tmpVideoFilterList = "adjust," + m_videoFilterList;
    }
    else
        tmpVideoFilterList = m_videoFilterList;

    m_videoFilters = m_filtMan->LoadFilters(tmpVideoFilterList, m_inpixfmt, tmp,
                                        m_w_out, m_h_out, btmp);
    if (m_video_buffer_size && btmp != m_video_buffer_size)
    {
        m_video_buffer_size = btmp;
        ResizeVideoBuffers();
    }
}

void NuppelVideoRecorder::InitBuffers(void)
{
    int videomegs;
    // cppcheck-suppress variableScope
    int audiomegs = 2;

    if (!m_video_buffer_size)
    {
        m_video_buffer_size =
            buffersize(m_picture_format == AV_PIX_FMT_YUV422P ? FMT_YUV422P : FMT_YV12,
                       m_w_out, m_h_out);
    }

    if (m_width >= 480 || m_height > 288)
        videomegs = 20;
    else
        videomegs = 12;

    m_video_buffer_count = (videomegs * 1000 * 1000) / m_video_buffer_size;

    if (m_audio_buffer_size != 0)
        m_audio_buffer_count = (audiomegs * 1000 * 1000) / m_audio_buffer_size;
    else
        m_audio_buffer_count = 0;

    m_text_buffer_size = 8 * (sizeof(teletextsubtitle) + VT_WIDTH);
    m_text_buffer_count = m_video_buffer_count;

    for (int i = 0; i < m_video_buffer_count; i++)
    {
        vidbuffertype *vidbuf = new vidbuffertype;
        vidbuf->buffer = new unsigned char[m_video_buffer_size];
        vidbuf->sample = 0;
        vidbuf->freeToEncode = 0;
        vidbuf->freeToBuffer = 1;
        vidbuf->bufferlen = 0;
        vidbuf->forcekey = 0;

        videobuffer.push_back(vidbuf);
    }

    for (int i = 0; i < m_audio_buffer_count; i++)
    {
        audbuffertype *audbuf = new audbuffertype;
        audbuf->buffer = new unsigned char[m_audio_buffer_size];
        audbuf->sample = 0;
        audbuf->freeToEncode = 0;
        audbuf->freeToBuffer = 1;

        audiobuffer.push_back(audbuf);
    }

    for (int i = 0; i < m_text_buffer_count; i++)
    {
        txtbuffertype *txtbuf = new txtbuffertype;
        txtbuf->buffer = new unsigned char[m_text_buffer_size];
        txtbuf->freeToEncode = 0;
        txtbuf->freeToBuffer = 1;

        textbuffer.push_back(txtbuf);
    }
}

void NuppelVideoRecorder::ResizeVideoBuffers(void)
{
    for (size_t i = 0; i < videobuffer.size(); i++)
    {
        delete [] (videobuffer[i]->buffer);
        videobuffer[i]->buffer = new unsigned char[m_video_buffer_size];
    }
}

void NuppelVideoRecorder::StreamAllocate(void)
{
    delete [] m_strm;
    m_strm = new signed char[m_width * m_height * 2 + 10];
}

bool NuppelVideoRecorder::Open(void)
{
    if (m_channelfd>0)
        return true;

    int retries = 0;
    QByteArray vdevice = m_videodevice.toLatin1();
    m_fd = open(vdevice.constData(), O_RDWR);
    while (m_fd < 0)
    {
        usleep(30000);
        m_fd = open(vdevice.constData(), O_RDWR);
        if (retries++ > 5)
        {
            m_error = QString("Can't open video device: %1").arg(m_videodevice);
            LOG(VB_GENERAL, LOG_ERR, LOC + m_error + ENO);
            KillChildren();
            return false;
        }
    }

    m_channelfd = m_fd;
    return true;
}

void NuppelVideoRecorder::ProbeV4L2(void)
{
#ifdef USING_V4L2
    m_usingv4l2 = true;

    struct v4l2_capability vcap {};

    if (ioctl(m_channelfd, VIDIOC_QUERYCAP, &vcap) < 0)
    {
        m_usingv4l2 = false;
    }

    if (m_usingv4l2 && !(vcap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Not a v4l2 capture device, falling back to v4l");
        m_usingv4l2 = false;
    }

    if (m_usingv4l2 && !(vcap.capabilities & V4L2_CAP_STREAMING))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Won't work with the streaming interface, falling back");
        m_usingv4l2 = false;
    }

    if (vcap.card[0] == 'B' && vcap.card[1] == 'T' &&
        vcap.card[2] == '8' && vcap.card[4] == '8')
        m_correct_bttv = true;

    QString driver = (char *)vcap.driver;
    if (driver == "go7007")
        m_go7007 = true;
#endif // USING_V4L2
}

void NuppelVideoRecorder::run(void)
{
    if (lzo_init() != LZO_E_OK)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "lzo_init() failed, exiting");
        m_error = "lzo_init() failed, exiting";
        LOG(VB_GENERAL, LOG_ERR, LOC + m_error);
        return;
    }

    if (!Open())
    {
        m_error = "Failed to open device";
        LOG(VB_GENERAL, LOG_ERR, LOC + m_error);
        return;
    }

    ProbeV4L2();

    if (m_usingv4l2 && !SetFormatV4L2())
    {
        m_error = "Failed to set V4L2 format";
        LOG(VB_GENERAL, LOG_ERR, LOC + m_error);
        return;
    }

    StreamAllocate();

    m_positionMapLock.lock();
    m_positionMap.clear();
    m_positionMapDelta.clear();
    m_positionMapLock.unlock();

    m_useavcodec = (m_videocodec.toLower() != "rtjpeg");
    if (m_useavcodec)
        m_useavcodec = SetupAVCodecVideo();

    if (!m_useavcodec)
        SetupRTjpeg();

    UpdateResolutions();

    if (CreateNuppelFile() != 0)
    {
        m_error = QString("Cannot open '%1' for writing")
            .arg(m_ringBuffer->GetFilename());
        LOG(VB_GENERAL, LOG_ERR, LOC + m_error);
        return;
    }

    if (IsHelperRequested())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Children are already alive");
        m_error = "Children are already alive";
        return;
    }

    {
        QMutexLocker locker(&m_pauseLock);
        m_request_recording = true;
        m_request_helper = true;
        m_recording = true;
        m_recordingWait.wakeAll();
    }

    m_write_thread = new NVRWriteThread(this);
    m_write_thread->start();

    m_audio_thread = new NVRAudioThread(this);
    m_audio_thread->start();

    if ((m_vbimode != VBIMode::None) && (OpenVBIDevice() >= 0))
        m_vbi_thread = new VBIThread(this);

    // save the start time
    gettimeofday(&m_stm, &m_tzone);

    // try to get run at higher scheduling priority, ignore failure
    myth_nice(-10);

    if (m_usingv4l2)
    {
        m_inpixfmt = FMT_NONE;
        InitFilters();
        DoV4L2();
    }
    else
        DoV4L1();

    {
        QMutexLocker locker(&m_pauseLock);
        m_request_recording = false;
        m_request_helper = false;
        m_recording = false;
        m_recordingWait.wakeAll();
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

    if (ioctl(m_fd, VIDIOCGCAP, &vc) < 0)
    {
        QString tmp = "VIDIOCGCAP: " + ENO;
        KillChildren();
        LOG(VB_GENERAL, LOG_ERR, tmp);
        m_error = tmp;
        return;
    }

    int channelinput = 0;

    if (m_channelObj)
        channelinput = m_channelObj->GetCurrentInputNum();

    vchan.channel = channelinput;

    if (ioctl(m_fd, VIDIOCGCHAN, &vchan) < 0)
        LOG(VB_GENERAL, LOG_ERR, LOC + "VIDIOCGCHAN: " + ENO);

    // Set volume level for audio recording (unless feature is disabled).
    if (!m_skip_btaudio)
    {
        // v4l1 compat in Linux 2.6.18 does not set VIDEO_VC_AUDIO,
        // so we just use VIDIOCGAUDIO unconditionally.. then only
        // report a get failure as an error if VIDEO_VC_AUDIO is set.
        if (ioctl(m_fd, VIDIOCGAUDIO, &va) < 0)
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
            va.volume = m_volume * 65535 / 100;
            if (ioctl(m_fd, VIDIOCSAUDIO, &va) < 0)
                LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to set audio" + ENO);
        }
    }

    if ((vc.type & VID_TYPE_MJPEG_ENCODER) && m_hardware_encode)
    {
        DoMJPEG();
        m_error = "MJPEG requested but not available.";
        LOG(VB_GENERAL, LOG_ERR, LOC + m_error);
        return;
    }

    m_inpixfmt = FMT_NONE;
    InitFilters();

    if (ioctl(m_fd, VIDIOCGMBUF, &vm) < 0)
    {
        QString tmp = "VIDIOCGMBUF: " + ENO;
        KillChildren();
        LOG(VB_GENERAL, LOG_ERR, LOC + tmp);
        m_error = tmp;
        return;
    }

    if (vm.frames < 2)
    {
        QString tmp = "need a minimum of 2 capture buffers";
        KillChildren();
        LOG(VB_GENERAL, LOG_ERR, LOC + tmp);
        m_error = tmp;
        return;
    }

    int frame;

    unsigned char *buf = (unsigned char *)mmap(0, vm.size,
                                               PROT_READ|PROT_WRITE,
                                               MAP_SHARED,
                                               m_fd, 0);
    if (buf == MAP_FAILED)
    {
        QString tmp = "mmap: " + ENO;
        KillChildren();
        LOG(VB_GENERAL, LOG_ERR, LOC + tmp);
        m_error = tmp;
        return;
    }

    mm.height = m_height;
    mm.width  = m_width;
    if (m_inpixfmt == FMT_YUV422P)
        mm.format = VIDEO_PALETTE_YUV422P;
    else
        mm.format = VIDEO_PALETTE_YUV420P;

    mm.frame  = 0;
    if (ioctl(m_fd, VIDIOCMCAPTURE, &mm)<0)
        LOG(VB_GENERAL, LOG_ERR, LOC + "VIDIOCMCAPTUREi0: " + ENO);
    mm.frame  = 1;
    if (ioctl(m_fd, VIDIOCMCAPTURE, &mm)<0)
        LOG(VB_GENERAL, LOG_ERR, LOC + "VIDIOCMCAPTUREi1: " + ENO);

    int syncerrors = 0;

    while (IsRecordingRequested() && !IsErrored())
    {
        {
            QMutexLocker locker(&m_pauseLock);
            if (m_request_pause)
            {
                if (!m_mainpaused)
                {
                    m_mainpaused = true;
                    m_pauseWait.wakeAll();
                    if (IsPaused(true) && m_tvrec)
                        m_tvrec->RecorderPaused();
                }
                m_unpauseWait.wait(&m_pauseLock, 100);
                if (m_cleartimeonpause)
                    gettimeofday(&m_stm, &m_tzone);
                continue;
            }

            if (!m_request_pause && m_mainpaused)
            {
                m_mainpaused = false;
                m_unpauseWait.wakeAll();
            }
        }

        frame = 0;
        mm.frame = 0;
        if (ioctl(m_fd, VIDIOCSYNC, &frame)<0)
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
            BufferIt(buf+vm.offsets[0], m_video_buffer_size);
            //memset(buf+vm.offsets[0], 0, m_video_buffer_size);
        }

        if (ioctl(m_fd, VIDIOCMCAPTURE, &mm)<0)
            LOG(VB_GENERAL, LOG_ERR, LOC + "VIDIOCMCAPTURE0: " + ENO);

        frame = 1;
        mm.frame = 1;
        if (ioctl(m_fd, VIDIOCSYNC, &frame)<0)
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
            BufferIt(buf+vm.offsets[1], m_video_buffer_size);
            //memset(buf+vm.offsets[1], 0, m_video_buffer_size);
        }
        if (ioctl(m_fd, VIDIOCMCAPTURE, &mm)<0)
            LOG(VB_GENERAL, LOG_ERR, LOC + "VIDIOCMCAPTURE1: " + ENO);
    }

    munmap(buf, vm.size);

    KillChildren();

    FinishRecording();

    close(m_fd);
}
#else // if !USING_V4L1
void NuppelVideoRecorder::DoV4L1(void) {}
#endif // !USING_V4L1

#ifdef USING_V4L2
bool NuppelVideoRecorder::SetFormatV4L2(void)
{
    struct v4l2_format     vfmt {};

    vfmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    vfmt.fmt.pix.width = m_width;
    vfmt.fmt.pix.height = m_height;
    vfmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

    if (m_go7007)
        vfmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MPEG;
    else if (m_inpixfmt == FMT_YUV422P)
        vfmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV422P;
    else
        vfmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;

    if (ioctl(m_fd, VIDIOC_S_FMT, &vfmt) < 0)
    {
        // this is supported by the cx88 and various ati cards.
        vfmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;

        if (ioctl(m_fd, VIDIOC_S_FMT, &vfmt) < 0)
        {
            // this is supported by the HVR-950q
            vfmt.fmt.pix.pixelformat = V4L2_PIX_FMT_UYVY;
            if (ioctl(m_fd, VIDIOC_S_FMT, &vfmt) < 0)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "v4l2: Unable to set desired format");
                return false;
            }

            // we need to convert the buffer - we can't deal with uyvy
            // directly.
            if (m_inpixfmt == FMT_YUV422P)
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
        else
        {
            // we need to convert the buffer - we can't deal with yuyv directly.
            if (m_inpixfmt == FMT_YUV422P)
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
    if (m_width  != (int)vfmt.fmt.pix.width ||
        m_height != (int)vfmt.fmt.pix.height)
    {
        LOG(VB_RECORD, LOG_INFO, LOC +
            QString("v4l2: resolution changed. requested %1x%2, using "
                    "%3x%4 now")
                .arg(m_width).arg(m_height)
                .arg(vfmt.fmt.pix.width) .arg(vfmt.fmt.pix.height));
        m_w_out = m_width  = vfmt.fmt.pix.width;
        m_h_out = m_height = vfmt.fmt.pix.height;
    }

    m_v4l2_pixelformat = vfmt.fmt.pix.pixelformat;

    return true;
}
#else // if !USING_V4L2
bool NuppelVideoRecorder::SetFormatV4L2(void) { return false; }
#endif // !USING_V4L2

#ifdef USING_V4L2
#define MAX_VIDEO_BUFFERS 5
void NuppelVideoRecorder::DoV4L2(void)
{
    struct v4l2_buffer     vbuf {};
    struct v4l2_requestbuffers vrbuf {};
    struct v4l2_control    vc {};

    vc.id = V4L2_CID_AUDIO_MUTE;
    vc.value = 0;

    if (ioctl(m_fd, VIDIOC_S_CTRL, &vc) < 0)
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "VIDIOC_S_CTRL:V4L2_CID_AUDIO_MUTE: " + ENO);

    if (m_go7007)
    {
        struct go7007_comp_params comp {};
        struct go7007_mpeg_params mpeg {};

        comp.gop_size = m_keyframedist;
        comp.max_b_frames = 0;

        if (fabs(m_video_aspect - 1.33333F) < 0.01F)
        {
            if (m_ntsc)
                comp.aspect_ratio = GO7007_ASPECT_RATIO_4_3_NTSC;
            else
                comp.aspect_ratio = GO7007_ASPECT_RATIO_4_3_PAL;
        }
        else if (fabs(m_video_aspect - 1.77777F) < 0.01F)
        {
            if (m_ntsc)
                comp.aspect_ratio = GO7007_ASPECT_RATIO_16_9_NTSC;
            else
                comp.aspect_ratio = GO7007_ASPECT_RATIO_16_9_PAL;
        }
        else
        {
            comp.aspect_ratio = GO7007_ASPECT_RATIO_1_1;
        }

        comp.flags |= GO7007_COMP_CLOSED_GOP;
        if (ioctl(m_fd, GO7007IOC_S_COMP_PARAMS, &comp) < 0)
        {
            m_error = "Unable to set compression params";
            LOG(VB_GENERAL, LOG_ERR, LOC + m_error);
            return;
        }

        if (m_videocodec == "mpeg2video")
            mpeg.mpeg_video_standard = GO7007_MPEG_VIDEO_MPEG2;
        else
            mpeg.mpeg_video_standard = GO7007_MPEG_VIDEO_MPEG4;

        if (ioctl(m_fd, GO7007IOC_S_MPEG_PARAMS, &mpeg) < 0)
        {
            m_error = "Unable to set MPEG params";
            LOG(VB_GENERAL, LOG_ERR, LOC + m_error);
            return;
        }

        int usebitrate = m_targetbitrate * 1000;
        if (m_scalebitrate)
        {
            float diff = (m_width * m_height) / (640.0 * 480.0);
            usebitrate = (int)(diff * usebitrate);
        }

        if (ioctl(m_fd, GO7007IOC_S_BITRATE, &usebitrate) < 0)
        {
            m_error = "Unable to set bitrate";
            LOG(VB_GENERAL, LOG_ERR, LOC + m_error);
            return;
        }

        m_hardware_encode = true;
    }

    uint numbuffers = MAX_VIDEO_BUFFERS;

    vrbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    vrbuf.memory = V4L2_MEMORY_MMAP;
    vrbuf.count = numbuffers;

    if (ioctl(m_fd, VIDIOC_REQBUFS, &vrbuf) < 0)
    {
        m_error = "Not able to get any capture buffers, exiting";
        LOG(VB_GENERAL, LOG_ERR, LOC + m_error);
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

        if (ioctl(m_fd, VIDIOC_QUERYBUF, &vbuf) < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("unable to query capture buffer %1").arg(i));
            m_error = "Unable to query capture buffer";
            return;
        }

        buffers[i] = (unsigned char *)mmap(nullptr, vbuf.length,
                                           PROT_READ|PROT_WRITE, MAP_SHARED,
                                           m_fd, vbuf.m.offset);

        if (buffers[i] == MAP_FAILED)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "mmap: " + ENO);
            LOG(VB_GENERAL, LOG_ERR, LOC + "Memory map failed");
            m_error = "Memory map failed";
            return;
        }
        bufferlen[i] = vbuf.length;
    }

    for (uint i = 0; i < numbuffers; i++)
    {
        memset(buffers[i], 0, bufferlen[i]);
        vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        vbuf.index = i;
        if (ioctl(m_fd, VIDIOC_QBUF, &vbuf) < 0)
            LOG(VB_GENERAL, LOG_ERR, LOC + "unable to enqueue capture buffer (VIDIOC_QBUF failed) " + ENO);
    }

    int turnon = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(m_fd, VIDIOC_STREAMON, &turnon) < 0)
        LOG(VB_GENERAL, LOG_ERR, LOC + "unable to start capture (VIDIOC_STREAMON failed) " + ENO);

    struct timeval tv;
    fd_set rdset {};
    int frame = 0;
    bool forcekey = false;

    m_resetcapture = false;

    // setup pixel format conversions for YUYV and UYVY
    uint8_t *output_buffer = nullptr;
    struct SwsContext *convert_ctx = nullptr;
    AVFrame img_out;
    if (m_v4l2_pixelformat == V4L2_PIX_FMT_YUYV ||
        m_v4l2_pixelformat == V4L2_PIX_FMT_UYVY)
    {
        AVPixelFormat in_pixfmt = m_v4l2_pixelformat == V4L2_PIX_FMT_YUYV ?
                                    AV_PIX_FMT_YUYV422 :
                                    AV_PIX_FMT_UYVY422;

        output_buffer = (uint8_t*)av_malloc(m_height * m_width * 3 / 2);
        if (!output_buffer)
        {
            m_error = "Cannot initialize image conversion buffer";
            LOG(VB_GENERAL, LOG_ERR, LOC + m_error);
            return;
        }

        convert_ctx = sws_getCachedContext(convert_ctx, m_width, m_height, in_pixfmt,
                                           m_width, m_height, AV_PIX_FMT_YUV420P,
                                           SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
        if (!convert_ctx)
        {
            m_error = "Cannot initialize image conversion context";
            av_free(output_buffer);
            LOG(VB_GENERAL, LOG_ERR, LOC + m_error);
            return;
        }

        av_image_fill_arrays(img_out.data, img_out.linesize,
            output_buffer, AV_PIX_FMT_YUV420P, m_width, m_height, IMAGE_ALIGN);
    }

    while (IsRecordingRequested() && !IsErrored())
    {
again:
        {
            QMutexLocker locker(&m_pauseLock);
            if (m_request_pause)
            {
                if (!m_mainpaused)
                {
                    m_mainpaused = true;
                    m_pauseWait.wakeAll();
                    if (IsPaused(true) && m_tvrec)
                        m_tvrec->RecorderPaused();
                }
                m_unpauseWait.wait(&m_pauseLock, 100);
                if (m_cleartimeonpause)
                    gettimeofday(&m_stm, &m_tzone);
                continue;
            }

            if (!m_request_pause && m_mainpaused)
            {
                m_mainpaused = false;
                m_unpauseWait.wakeAll();
            }
        }

        if (m_resetcapture)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Resetting and re-queueing");
            turnon = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if (ioctl(m_fd, VIDIOC_STREAMOFF, &turnon) < 0)
                LOG(VB_GENERAL, LOG_ERR, LOC + "unable to stop capture (VIDIOC_STREAMOFF failed) " + ENO);

            for (uint i = 0; i < numbuffers; i++)
            {
                memset(buffers[i], 0, bufferlen[i]);
                vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                vbuf.index = i;
                if (ioctl(m_fd, VIDIOC_QBUF, &vbuf) < 0)
                    LOG(VB_GENERAL, LOG_ERR, LOC + "unable to enqueue capture buffer (VIDIOC_QBUF failed) " + ENO);
             }

             if (ioctl(m_fd, VIDIOC_STREAMON, &turnon) < 0)
                LOG(VB_GENERAL, LOG_ERR, LOC + "unable to start capture (VIDIOC_STREAMON failed) " + ENO);
             m_resetcapture = false;
        }

        tv.tv_sec = 5;
        tv.tv_usec = 0;
        FD_ZERO(&rdset);
        FD_SET(m_fd, &rdset);

        switch (select(m_fd+1, &rdset, nullptr, nullptr, &tv))
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
        if (ioctl(m_fd, VIDIOC_DQBUF, &vbuf) < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "DQBUF ioctl failed." + ENO);

            // EIO failed DQBUF de-tunes post 2.6.15.3 for cx88
            // EIO or EINVAL on bttv means we need to reset the buffers..
            if (errno == EIO && m_channelObj)
            {
                m_channelObj->Retune();
                m_resetcapture = true;
                continue;
            }

            if (errno == EIO || errno == EINVAL)
            {
                m_resetcapture = true;
                continue;
            }

            if (errno == EAGAIN)
                continue;
        }

        frame = vbuf.index;
        if (m_go7007)
            forcekey = ((vbuf.flags & V4L2_BUF_FLAG_KEYFRAME) != 0U);

        if (!m_request_pause)
        {
            if (m_v4l2_pixelformat == V4L2_PIX_FMT_YUYV)
            {
                AVFrame img_in;
                av_image_fill_arrays(img_in.data, img_in.linesize,
                    buffers[frame], AV_PIX_FMT_YUYV422, m_width, m_height,
                    IMAGE_ALIGN);
                sws_scale(convert_ctx, img_in.data, img_in.linesize,
                          0, m_height, img_out.data, img_out.linesize);
                BufferIt(output_buffer, m_video_buffer_size);
            }
            else if (m_v4l2_pixelformat == V4L2_PIX_FMT_UYVY)
            {
                AVFrame img_in;
                av_image_fill_arrays(img_in.data, img_in.linesize,
                    buffers[frame], AV_PIX_FMT_UYVY422, m_width, m_height,
                    IMAGE_ALIGN);
                sws_scale(convert_ctx, img_in.data, img_in.linesize,
                          0, m_height, img_out.data, img_out.linesize);
                BufferIt(output_buffer, m_video_buffer_size);
            }
            else
            {
                // buffer the frame directly
                BufferIt(buffers[frame], vbuf.bytesused, forcekey);
            }
        }

        vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(m_fd, VIDIOC_QBUF, &vbuf) < 0)
            LOG(VB_GENERAL, LOG_ERR, LOC + "unable to enqueue capture buffer (VIDIOC_QBUF failed) " + ENO);
    }

    KillChildren();

    if (ioctl(m_fd, VIDIOC_STREAMOFF, &turnon) < 0)
        LOG(VB_GENERAL, LOG_ERR, LOC + "unable to stop capture (VIDIOC_STREAMOFF failed) " + ENO);

    for (uint i = 0; i < numbuffers; i++)
    {
        munmap(buffers[i], bufferlen[i]);
    }

    FinishRecording();

    av_free(output_buffer);
    sws_freeContext(convert_ctx);

    close(m_fd);
    close(m_channelfd);
}
#else // if !USING_V4L2
void NuppelVideoRecorder::DoV4L2(void) {}
#endif // !USING_V4L2

#ifdef USING_V4L1
void NuppelVideoRecorder::DoMJPEG(void)
{
    struct mjpeg_params bparm;

    if (ioctl(m_fd, MJPIOC_G_PARAMS, &bparm) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "MJPIOC_G_PARAMS: " + ENO);
        return;
    }

    //bparm.input = 2;
    //bparm.norm = 1;
    bparm.quality = m_hmjpg_quality;

    if (m_hmjpg_hdecimation == m_hmjpg_vdecimation)
    {
        bparm.decimation = m_hmjpg_hdecimation;
    }
    else
    {
        bparm.decimation = 0;
        bparm.HorDcm = m_hmjpg_hdecimation;
        bparm.VerDcm = (m_hmjpg_vdecimation + 1) / 2;

        if (m_hmjpg_vdecimation == 1)
        {
            bparm.TmpDcm = 1;
            bparm.field_per_buff = 2;
        }
        else
        {
            bparm.TmpDcm = 2;
            bparm.field_per_buff = 1;
        }

        bparm.img_width = m_hmjpg_maxw;

        if (m_ntsc)
            bparm.img_height = 240;
        else
            bparm.img_height = 288;

        bparm.img_x = 0;
        bparm.img_y = 0;
    }

    bparm.APPn = 0;

    if (m_hmjpg_vdecimation == 1)
        bparm.APP_len = 14;
    else
        bparm.APP_len = 0;

    bparm.odd_even = !(m_hmjpg_vdecimation > 1);

    for (int n = 0; n < bparm.APP_len; n++)
        bparm.APP_data[n] = 0;

    if (ioctl(m_fd, MJPIOC_S_PARAMS, &bparm) < 0)
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC + "MJPIOC_S_PARAMS: " + ENO);
        return;
    }

    struct mjpeg_requestbuffers breq;

    breq.count = 64;
    breq.size = 256 * 1024;

    if (ioctl(m_fd, MJPIOC_REQBUFS, &breq) < 0)
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC + "MJPIOC_REQBUFS: " + ENO);
        return;
    }

    uint8_t *MJPG_buff = (uint8_t *)mmap(0, breq.count * breq.size,
                                         PROT_READ|PROT_WRITE, MAP_SHARED, m_fd,
                                         0);

    if (MJPG_buff == MAP_FAILED)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "mapping mjpeg buffers");
        return;
    }

    struct mjpeg_sync bsync;

    for (unsigned int count = 0; count < breq.count; count++)
    {
        if (ioctl(m_fd, MJPIOC_QBUF_CAPT, &count) < 0)
            LOG(VB_GENERAL, LOG_ERR, LOC + "MJPIOC_QBUF_CAPT: " + ENO);
    }

    while (IsRecordingRequested() && !IsErrored())
    {
        {
            QMutexLocker locker(&m_pauseLock);
            if (m_request_pause)
            {
                if (!m_mainpaused)
                {
                    m_mainpaused = true;
                    m_pauseWait.wakeAll();
                    if (IsPaused(true) && m_tvrec)
                        m_tvrec->RecorderPaused();
                }
                m_unpauseWait.wait(&m_pauseLock, 100);
                if (m_cleartimeonpause)
                    gettimeofday(&m_stm, &m_tzone);
                continue;
            }

            if (!m_request_pause && m_mainpaused)
            {
                m_mainpaused = false;
                m_unpauseWait.wakeAll();
            }
        }

        if (ioctl(m_fd, MJPIOC_SYNC, &bsync) < 0)
        {
            m_error = "MJPEG sync error";
            LOG(VB_GENERAL, LOG_ERR, LOC + m_error + ENO);
            break;
        }

        BufferIt((unsigned char *)(MJPG_buff + bsync.frame * breq.size),
                 bsync.length);

        if (ioctl(m_fd, MJPIOC_QBUF_CAPT, &(bsync.frame)) < 0)
        {
            m_error = "MJPEG Capture error";
            LOG(VB_GENERAL, LOG_ERR, LOC + m_error + ENO);
        }
    }

    munmap(MJPG_buff, breq.count * breq.size);
    KillChildren();

    FinishRecording();

    close(m_fd);
}
#else // if !USING_V4L1
void NuppelVideoRecorder::DoMJPEG(void) {}
#endif // !USING_V4L1

void NuppelVideoRecorder::KillChildren(void)
{
    {
        QMutexLocker locker(&m_pauseLock);
        m_request_helper = false;
        m_unpauseWait.wakeAll();
    }

    if (m_write_thread)
    {
        m_write_thread->wait();
        delete m_write_thread;
        m_write_thread = nullptr;
    }

    if (m_audio_thread)
    {
        m_audio_thread->wait();
        delete m_audio_thread;
        m_audio_thread = nullptr;
    }

    if (m_vbi_thread)
    {
        m_vbi_thread->wait();
        delete m_vbi_thread;
        m_vbi_thread = nullptr;
        CloseVBIDevice();
    }
}

void NuppelVideoRecorder::BufferIt(unsigned char *buf, int len, bool forcekey)
{
    int act;
    long tcres;
    struct timeval now {};

    act = m_act_video_buffer;

    if (!videobuffer[act]->freeToBuffer) {
        return;
    }

    gettimeofday(&now, &m_tzone);

    tcres = (now.tv_sec-m_stm.tv_sec)*1000 + now.tv_usec/1000 - m_stm.tv_usec/1000;

    m_usebttv = 0;
    // here is the non preferable timecode - drop algorithm - fallback
    if (!m_usebttv)
    {
        if (m_tf==0)
            m_tf = 2;
        else
        {
            int fn = tcres - m_oldtc;

     // the difference should be less than 1,5*timeperframe or we have
     // missed at least one frame, this code might be inaccurate!

            if (m_ntsc_framerate)
                fn = (fn+16)/33;
            else
                fn = (fn+20)/40;
            if (fn<1)
                fn=1;
            m_tf += 2*fn; // two fields
        }
    }

    m_oldtc = tcres;

    if (!videobuffer[act]->freeToBuffer)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            "DROPPED frame due to full buffer in the recorder.");
        return; // we can't buffer the current frame
    }

    videobuffer[act]->sample = m_tf;

    // record the time at the start of this frame.
    // 'tcres' is at the end of the frame, so subtract the right # of ms
    videobuffer[act]->timecode = (m_ntsc_framerate) ? (tcres - 33) : (tcres - 40);

    memcpy(videobuffer[act]->buffer, buf, len);
    videobuffer[act]->bufferlen = len;
    videobuffer[act]->forcekey = forcekey;

    videobuffer[act]->freeToBuffer = 0;
    m_act_video_buffer++;
    if (m_act_video_buffer >= m_video_buffer_count)
        m_act_video_buffer = 0; // cycle to begin of buffer
    videobuffer[act]->freeToEncode = 1; // set last to prevent race
}

inline void NuppelVideoRecorder::WriteFrameheader(rtframeheader *fh)
{
#if HAVE_BIGENDIAN
    fh->timecode     = bswap_32(fh->timecode);
    fh->packetlength = bswap_32(fh->packetlength);
#endif
    m_ringBuffer->Write(fh, FRAMEHEADERSIZE);
}

void NuppelVideoRecorder::SetNewVideoParams(double newaspect)
{
    if (newaspect == static_cast<double>(m_video_aspect))
        return;

    m_video_aspect = newaspect;

    struct rtframeheader frameheader {};

    frameheader.frametype = 'S';
    frameheader.comptype = 'M';
    frameheader.packetlength = sizeof(struct rtfileheader);

    WriteFrameheader(&frameheader);

    WriteFileHeader();
}

void NuppelVideoRecorder::WriteFileHeader(void)
{
    struct rtfileheader fileheader {};
    static const char finfo[12] = "MythTVVideo";
    static const char vers[5]   = "0.07";

    memcpy(fileheader.finfo, finfo, sizeof(fileheader.finfo));
    memcpy(fileheader.version, vers, sizeof(fileheader.version));
    fileheader.width  = m_w_out;
    fileheader.height = (int)(m_h_out * m_height_multiplier);
    fileheader.desiredwidth  = 0;
    fileheader.desiredheight = 0;
    fileheader.pimode = 'P';
    fileheader.aspect = m_video_aspect;
    fileheader.fps = m_video_frame_rate;
    fileheader.fps *= m_framerate_multiplier;
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
    m_ringBuffer->Write(&fileheader, FILEHEADERSIZE);
}

void NuppelVideoRecorder::WriteHeader(void)
{
    struct rtframeheader frameheader {};

    if (!m_videoFilters)
        InitFilters();

    WriteFileHeader();

    frameheader.frametype = 'D'; // compressor data

    if (m_useavcodec)
    {
        frameheader.comptype = 'F';
        frameheader.packetlength = m_mpa_vidctx->extradata_size;

        WriteFrameheader(&frameheader);
        m_ringBuffer->Write(m_mpa_vidctx->extradata, frameheader.packetlength);
    }
    else
    {
        static unsigned long int tbls[128];

        frameheader.comptype = 'R'; // compressor data for RTjpeg
        frameheader.packetlength = sizeof(tbls);

        // compression configuration header
        WriteFrameheader(&frameheader);

        memset(tbls, 0, sizeof(tbls));
        m_ringBuffer->Write(tbls, sizeof(tbls));
    }

    memset(&frameheader, 0, sizeof(frameheader));
    frameheader.frametype = 'X'; // extended data
    frameheader.packetlength = sizeof(extendeddata);

    // extended data header
    WriteFrameheader(&frameheader);

    struct extendeddata moredata {};

    moredata.version = 1;
    if (m_useavcodec)
    {
        int vidfcc = 0;
        switch(m_mpa_vidcodec->id)
        {
            case AV_CODEC_ID_MPEG4:      vidfcc = FOURCC_DIVX; break;
            case AV_CODEC_ID_WMV1:       vidfcc = FOURCC_WMV1; break;
            case AV_CODEC_ID_MSMPEG4V3:  vidfcc = FOURCC_DIV3; break;
            case AV_CODEC_ID_MSMPEG4V2:  vidfcc = FOURCC_MP42; break;
            case AV_CODEC_ID_MSMPEG4V1:  vidfcc = FOURCC_MPG4; break;
            case AV_CODEC_ID_MJPEG:      vidfcc = FOURCC_MJPG; break;
            case AV_CODEC_ID_H263:
            case AV_CODEC_ID_H263P:      vidfcc = FOURCC_H263; break;
            case AV_CODEC_ID_H263I:      vidfcc = FOURCC_I263; break;
            case AV_CODEC_ID_MPEG1VIDEO: vidfcc = FOURCC_MPEG; break;
            case AV_CODEC_ID_MPEG2VIDEO: vidfcc = FOURCC_MPG2; break;
            case AV_CODEC_ID_HUFFYUV:    vidfcc = FOURCC_HFYU; break;
            default: break;
        }
        moredata.video_fourcc = vidfcc;
        moredata.lavc_bitrate = m_mpa_vidctx->bit_rate;
        moredata.lavc_qmin = m_mpa_vidctx->qmin;
        moredata.lavc_qmax = m_mpa_vidctx->qmax;
        moredata.lavc_maxqdiff = m_mpa_vidctx->max_qdiff;
    }
    else
    {
        moredata.video_fourcc = FOURCC_RJPG;
        moredata.rtjpeg_quality = m_Q;
        moredata.rtjpeg_luma_filter = m_M1;
        moredata.rtjpeg_chroma_filter = m_M2;
    }

    if (m_compressaudio)
    {
        moredata.audio_fourcc = FOURCC_LAME;
        moredata.audio_compression_ratio = 11;
        moredata.audio_quality = m_mp3quality;
    }
    else
    {
        moredata.audio_fourcc = FOURCC_RAWA;
    }

    moredata.audio_sample_rate = m_audio_samplerate;
    moredata.audio_channels = m_audio_channels;
    moredata.audio_bits_per_sample = m_audio_bits;

    m_extendeddataOffset = m_ringBuffer->GetWritePosition();

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
    m_ringBuffer->Write(&moredata, sizeof(moredata));

    m_last_block = 0;
    m_lf = 0; // that resets framenumber so that seeking in the
              // continues parts works too
}

void NuppelVideoRecorder::WriteSeekTable(void)
{
    int numentries = m_seektable->size();

    struct rtframeheader frameheader {};
    frameheader.frametype = 'Q'; // SeekTable
    frameheader.packetlength = sizeof(struct seektable_entry) * numentries;

    long long currentpos = m_ringBuffer->GetWritePosition();

    m_ringBuffer->Write(&frameheader, sizeof(frameheader));

    char *seekbuf = new char[frameheader.packetlength];
    int offset = 0;

    vector<struct seektable_entry>::iterator it = m_seektable->begin();
    for (; it != m_seektable->end(); ++it)
    {
        memcpy(seekbuf + offset, (const void *)&(*it),
               sizeof(struct seektable_entry));
        offset += sizeof(struct seektable_entry);
    }

    m_ringBuffer->Write(seekbuf, frameheader.packetlength);

    m_ringBuffer->WriterSeek(m_extendeddataOffset +
                           offsetof(struct extendeddata, seektable_offset),
                           SEEK_SET);

    m_ringBuffer->Write(&currentpos, sizeof(long long));

    m_ringBuffer->WriterSeek(0, SEEK_END);

    delete [] seekbuf;
}

void NuppelVideoRecorder::WriteKeyFrameAdjustTable(
    const vector<struct kfatable_entry> &kfa_table)
{
    int numentries = kfa_table.size();

    struct rtframeheader frameheader {};
    frameheader.frametype = 'K'; // KFA Table
    frameheader.packetlength = sizeof(struct kfatable_entry) * numentries;

    long long currentpos = m_ringBuffer->GetWritePosition();

    m_ringBuffer->Write(&frameheader, sizeof(frameheader));

    char *kfa_buf = new char[frameheader.packetlength];
    uint offset = 0;

    vector<struct kfatable_entry>::const_iterator it = kfa_table.begin();
    for (; it != kfa_table.end() ; ++it)
    {
        memcpy(kfa_buf + offset, &(*it),
               sizeof(struct kfatable_entry));
        offset += sizeof(struct kfatable_entry);
    }

    m_ringBuffer->Write(kfa_buf, frameheader.packetlength);


    m_ringBuffer->WriterSeek(m_extendeddataOffset +
                           offsetof(struct extendeddata, keyframeadjust_offset),
                           SEEK_SET);

    m_ringBuffer->Write(&currentpos, sizeof(long long));

    m_ringBuffer->WriterSeek(0, SEEK_END);

    delete [] kfa_buf;
}

void NuppelVideoRecorder::UpdateSeekTable(int frame_num, long offset)
{
    long long position = m_ringBuffer->GetWritePosition() + offset;
    struct seektable_entry ste { position, frame_num};
    m_seektable->push_back(ste);

    m_positionMapLock.lock();
    if (!m_positionMap.contains(ste.keyframe_number))
    {
        m_positionMapDelta[ste.keyframe_number] = position;
        m_positionMap[ste.keyframe_number] = position;
        m_lastPositionMapPos = position;
    }
    m_positionMapLock.unlock();
}

int NuppelVideoRecorder::CreateNuppelFile(void)
{
    m_framesWritten = 0;

    if (!m_ringBuffer)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "No ringbuffer, recorder wasn't initialized.");
        return -1;
    }

    if (!m_ringBuffer->IsOpen())
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

    for (int i = 0; i < m_video_buffer_count; i++)
    {
        vidbuffertype *vidbuf = videobuffer[i];
        vidbuf->sample = 0;
        vidbuf->timecode = 0;
        vidbuf->freeToEncode = 0;
        vidbuf->freeToBuffer = 1;
        vidbuf->forcekey = 0;
    }

    for (int i = 0; i < m_audio_buffer_count; i++)
    {
        audbuffertype *audbuf = audiobuffer[i];
        audbuf->sample = 0;
        audbuf->timecode = 0;
        audbuf->freeToEncode = 0;
        audbuf->freeToBuffer = 1;
    }

    for (int i = 0; i < m_text_buffer_count; i++)
    {
        txtbuffertype *txtbuf = textbuffer[i];
        txtbuf->freeToEncode = 0;
        txtbuf->freeToBuffer = 1;
    }

    m_act_video_encode = 0;
    m_act_video_buffer = 0;
    m_act_audio_encode = 0;
    m_act_audio_buffer = 0;
    m_act_audio_sample = 0;
    m_act_text_encode = 0;
    m_act_text_buffer = 0;

    m_audiobytes = 0;
    m_effectivedsp = 0;

    if (m_useavcodec)
        SetupAVCodecVideo();

    if (m_curRecording)
        m_curRecording->ClearPositionMap(MARK_KEYFRAME);
}

void NuppelVideoRecorder::doAudioThread(void)
{
    if (!m_audio_device)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Invalid audio device (%1), exiting").arg(m_audiodevice));
        return;
    }

    if (!m_audio_device->Open(m_audio_bits, m_audio_samplerate, m_audio_channels))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Failed to open audio device %1").arg(m_audiodevice));
        return;
    }

    if (!m_audio_device->Start())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Failed to start audio capture on %1").arg(m_audiodevice));
        return;
    }

    struct timeval anow {};
    unsigned char *buffer = new unsigned char[m_audio_buffer_size];
    int act = 0, lastread = 0;
    m_audio_bytes_per_sample = m_audio_channels * m_audio_bits / 8;

    while (IsHelperRequested() && !IsErrored())
    {
        {
            QMutexLocker locker(&m_pauseLock);
            if (m_request_pause)
            {
                if (!m_audiopaused)
                {
                    m_audiopaused = true;
                    m_pauseWait.wakeAll();
                    if (IsPaused(true) && m_tvrec)
                        m_tvrec->RecorderPaused();
                }
                m_unpauseWait.wait(&m_pauseLock, 100);
                continue;
            }

            if (!m_request_pause && m_audiopaused)
            {
                m_audiopaused = false;
                m_unpauseWait.wakeAll();
            }
        }

        if (!IsHelperRequested() || IsErrored())
            break;

        lastread = m_audio_device->GetSamples(buffer, m_audio_buffer_size);
        if (m_audio_buffer_size != lastread)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Short read, %1 of %2 bytes from ")
                    .arg(lastread).arg(m_audio_buffer_size) + m_audiodevice);
        }

        /* record the current time */
        /* Don't assume that the sound device's record buffer is empty
           (like we used to.) Measure to see how much stuff is in there,
           and correct for it when calculating the timestamp */
        gettimeofday(&anow, &m_tzone);
        int bytes_read = max(m_audio_device->GetNumReadyBytes(), 0);

        act = m_act_audio_buffer;

        if (!audiobuffer[act]->freeToBuffer)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Ran out of free AUDIO buffers :-(");
            m_act_audio_sample++;
            continue;
        }

        audiobuffer[act]->sample = m_act_audio_sample;

        /* calculate timecode. First compute the difference
           between now and stm (start time) */
        audiobuffer[act]->timecode = (anow.tv_sec - m_stm.tv_sec) * 1000 +
                                      anow.tv_usec / 1000 - m_stm.tv_usec / 1000;
        /* We want the timestamp to point to the start of this
           audio chunk. So, subtract off the length of the chunk
           and the length of audio still in the capture buffer. */
        audiobuffer[act]->timecode -= (int)(
                (bytes_read + m_audio_buffer_size)
                 * 1000.0 / (m_audio_samplerate * m_audio_bytes_per_sample));

        memcpy(audiobuffer[act]->buffer, buffer, m_audio_buffer_size);

        audiobuffer[act]->freeToBuffer = 0;
        m_act_audio_buffer++;
        if (m_act_audio_buffer >= m_audio_buffer_count)
            m_act_audio_buffer = 0;
        audiobuffer[act]->freeToEncode = 1;

        m_act_audio_sample++;
    }

    delete [] buffer;

    if (m_audio_device->IsOpen())
        m_audio_device->Close();
}

#ifdef USING_V4L2
void NuppelVideoRecorder::FormatTT(struct VBIData *vbidata)
{
    struct timeval tnow {};
    gettimeofday(&tnow, &m_tzone);

    int act = m_act_text_buffer;
    if (!textbuffer[act]->freeToBuffer)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Teletext #%1: ran out of free TEXT buffers :-(").arg(act));
        return;
    }

    // calculate timecode:
    // compute the difference  between now and stm (start time)
    textbuffer[act]->timecode = (tnow.tv_sec-m_stm.tv_sec) * 1000 +
                                tnow.tv_usec/1000 - m_stm.tv_usec/1000;
    textbuffer[act]->pagenr = (vbidata->teletextpage.pgno << 16) +
                              vbidata->teletextpage.subno;

    unsigned char *inpos = vbidata->teletextpage.data[0];
    unsigned char *outpos = textbuffer[act]->buffer;
    *outpos = 0;
    struct teletextsubtitle st {};
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
                case 0x0f:              /* SI */
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
    m_act_text_buffer++;
    if (m_act_text_buffer >= m_text_buffer_count)
        m_act_text_buffer = 0;
    textbuffer[act]->freeToEncode = 1;
}
#else  // USING_V4L2
void NuppelVideoRecorder::FormatTT(struct VBIData*) {}
#endif // USING_V4L2

void NuppelVideoRecorder::FormatCC(uint code1, uint code2)
{
    struct timeval tnow {};
    gettimeofday (&tnow, &m_tzone);

    // calculate timecode:
    // compute the difference  between now and stm (start time)
    int tc = (tnow.tv_sec - m_stm.tv_sec) * 1000 +
             tnow.tv_usec / 1000 - m_stm.tv_usec / 1000;

    m_ccd->FormatCC(tc, code1, code2);
}

void NuppelVideoRecorder::AddTextData(unsigned char *buf, int len,
                                      int64_t timecode, char /*type*/)
{
    int act = m_act_text_buffer;
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
    m_act_text_buffer++;
    if (m_act_text_buffer >= m_text_buffer_count)
        m_act_text_buffer = 0;
    textbuffer[act]->freeToEncode = 1;
}

void NuppelVideoRecorder::doWriteThread(void)
{
    while (IsHelperRequested() && !IsErrored())
    {
        {
            QMutexLocker locker(&m_pauseLock);
            if (m_request_pause)
            {
                if (!m_writepaused)
                {
                    m_writepaused = true;
                    m_pauseWait.wakeAll();
                    if (IsPaused(true) && m_tvrec)
                        m_tvrec->RecorderPaused();
                }
                m_unpauseWait.wait(&m_pauseLock, 100);
                continue;
            }

            if (!m_request_pause && m_writepaused)
            {
                m_writepaused = false;
                m_unpauseWait.wakeAll();
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

        if (videobuffer[m_act_video_encode]->freeToEncode)
        {
            action = ACTION_VIDEO;
            firsttimecode = videobuffer[m_act_video_encode]->timecode;
        }

        if (m_audio_buffer_count &&
            audiobuffer[m_act_audio_encode]->freeToEncode &&
            (action == ACTION_NONE ||
             (audiobuffer[m_act_audio_encode]->timecode < firsttimecode)))
        {
            action = ACTION_AUDIO;
            firsttimecode = audiobuffer[m_act_audio_encode]->timecode;
        }

        if (m_text_buffer_count &&
            textbuffer[m_act_text_encode]->freeToEncode &&
            (action == ACTION_NONE ||
             (textbuffer[m_act_text_encode]->timecode < firsttimecode)))
        {
            action = ACTION_TEXT;
        }

        switch (action)
        {
            case ACTION_VIDEO:
            {
                VideoFrame frame;
                init(&frame,
                     FMT_YV12, videobuffer[m_act_video_encode]->buffer,
                     m_width, m_height, videobuffer[m_act_video_encode]->bufferlen);

                frame.frameNumber = videobuffer[m_act_video_encode]->sample;
                frame.timecode = videobuffer[m_act_video_encode]->timecode;
                frame.forcekey = videobuffer[m_act_video_encode]->forcekey;

                WriteVideo(&frame);

                videobuffer[m_act_video_encode]->sample = 0;
                videobuffer[m_act_video_encode]->freeToEncode = 0;
                videobuffer[m_act_video_encode]->freeToBuffer = 1;
                videobuffer[m_act_video_encode]->forcekey = 0;
                m_act_video_encode++;
                if (m_act_video_encode >= m_video_buffer_count)
                    m_act_video_encode = 0;
                break;
            }
            case ACTION_AUDIO:
            {
                WriteAudio(audiobuffer[m_act_audio_encode]->buffer,
                           audiobuffer[m_act_audio_encode]->sample,
                           audiobuffer[m_act_audio_encode]->timecode);
                if (IsErrored()) {
                    LOG(VB_GENERAL, LOG_ERR, LOC +
                        "ACTION_AUDIO cannot be completed due to error.");
                    StopRecording();
                    break;
                }
                audiobuffer[m_act_audio_encode]->sample = 0;
                audiobuffer[m_act_audio_encode]->freeToEncode = 0;
                audiobuffer[m_act_audio_encode]->freeToBuffer = 1;
                m_act_audio_encode++;
                if (m_act_audio_encode >= m_audio_buffer_count)
                    m_act_audio_encode = 0;
                break;
            }
            case ACTION_TEXT:
            {
                WriteText(textbuffer[m_act_text_encode]->buffer,
                          textbuffer[m_act_text_encode]->bufferlen,
                          textbuffer[m_act_text_encode]->timecode,
                          textbuffer[m_act_text_encode]->pagenr);
                textbuffer[m_act_text_encode]->freeToEncode = 0;
                textbuffer[m_act_text_encode]->freeToBuffer = 1;
                m_act_text_encode++;
                if (m_act_text_encode >= m_text_buffer_count)
                    m_act_text_encode = 0;
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
    m_framesWritten = 0;
    m_lf = 0;
    m_last_block = 0;

    m_seektable->clear();

    ClearStatistics();

    m_positionMapLock.lock();
    m_positionMap.clear();
    m_positionMapDelta.clear();
    m_positionMapLock.unlock();

    if (m_go7007)
        m_resetcapture = true;
}

void NuppelVideoRecorder::StartNewFile(void)
{
    CreateNuppelFile();
}

void NuppelVideoRecorder::FinishRecording(void)
{
    m_ringBuffer->WriterFlush();

    WriteSeekTable();

    V4LRecorder::FinishRecording();
    
    m_positionMapLock.lock();
    m_positionMap.clear();
    m_positionMapDelta.clear();
    m_positionMapLock.unlock();
}

void NuppelVideoRecorder::WriteVideo(VideoFrame *frame, bool skipsync,
                                     bool forcekey)
{
    int tmp = 0;
    lzo_uint out_len = OUT_LEN;
    struct rtframeheader frameheader {};
    int raw = 0, compressthis = m_compression;
    // cppcheck-suppress variableScope
    uint8_t *planes[3] = {
        frame->buf + frame->offsets[0],
        frame->buf + frame->offsets[1],
        frame->buf + frame->offsets[2] };
    int fnum = frame->frameNumber;
    long long timecode = frame->timecode;

    if (m_lf == 0)
    {   // this will be triggered every new file
        m_lf = fnum;
        m_startnum = fnum;
        m_lasttimecode = 0;
        m_frameofgop = 0;
        forcekey = true;
    }

    // see if it's time for a seeker header, sync information and a keyframe
    frameheader.keyframe  = m_frameofgop;             // no keyframe defaulted

    bool wantkeyframe = forcekey;

    bool writesync = false;

    if ((!m_go7007 && (((fnum-m_startnum)>>1) % m_keyframedist == 0 && !skipsync)) ||
        (m_go7007 && frame->forcekey))
        writesync = true;

    if (writesync)
    {
        m_ringBuffer->Write("RTjjjjjjjjjjjjjjjjjjjjjjjj", FRAMEHEADERSIZE);

        UpdateSeekTable(((fnum - m_startnum) >> 1) / m_keyframedist);

        frameheader.frametype    = 'S';           // sync frame
        frameheader.comptype     = 'V';           // video sync information
        frameheader.filters      = 0;             // no filters applied
        frameheader.packetlength = 0;             // no data packet
        frameheader.timecode     = (fnum-m_startnum)>>1;
        // write video sync info
        WriteFrameheader(&frameheader);
        frameheader.frametype    = 'S';           // sync frame
        frameheader.comptype     = 'A';           // video sync information
        frameheader.filters      = 0;             // no filters applied
        frameheader.packetlength = 0;             // no data packet
        frameheader.timecode     = m_effectivedsp;  // effective dsp frequency
        // write audio sync info
        WriteFrameheader(&frameheader);

        wantkeyframe = true;
        //m_ringBuffer->Sync();
    }

    if (wantkeyframe)
    {
        frameheader.keyframe=0;
        m_frameofgop=0;
    }

    if (m_videoFilters)
        m_videoFilters->ProcessFrame(frame);

    if (m_useavcodec)
    {
        MythAVFrame mpa_picture;
        AVPictureFill(mpa_picture, frame);

        if (wantkeyframe)
            mpa_picture->pict_type = AV_PICTURE_TYPE_I;
        else
            mpa_picture->pict_type = AV_PICTURE_TYPE_NONE;

        if (!m_hardware_encode)
        {
            AVPacket packet;
            av_init_packet(&packet);
            packet.data = (uint8_t *)m_strm;
            packet.size = frame->size;

            int got_packet = 0;

            QMutexLocker locker(avcodeclock);
            tmp = avcodec_encode_video2(m_mpa_vidctx, &packet, mpa_picture,
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
        freecount = m_act_video_buffer > m_act_video_encode ?
                    m_video_buffer_count - (m_act_video_buffer - m_act_video_encode) :
                    m_act_video_encode - m_act_video_buffer;

        if (freecount < (m_video_buffer_count / 3))
            compressthis = 0; // speed up the encode process

        if (freecount < 5)
            raw = 1; // speed up the encode process

        if (m_transcoding)
        {
            raw = 0;
            compressthis = 1;
        }

        if (!raw)
        {
            if (wantkeyframe)
                m_rtjc->SetNextKey();
            tmp = m_rtjc->Compress(m_strm, planes);
        }
        else
            tmp = frame->size;

        // here is lzo compression afterwards
        if (compressthis)
        {
            int r = 0;
            if (raw)
                r = lzo1x_1_compress(frame->buf, frame->size,
                                     m_out, &out_len, wrkmem);
            else
                r = lzo1x_1_compress((unsigned char *)m_strm, tmp, m_out,
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
    m_lasttimecode = frameheader.timecode;
    frameheader.filters   = 0;             // no filters applied

    // compr ends here
    if (m_useavcodec)
    {
        if (m_mpa_vidcodec->id == AV_CODEC_ID_RAWVIDEO)
        {
            frameheader.comptype = '0';
            frameheader.packetlength = frame->size;
            WriteFrameheader(&frameheader);
            m_ringBuffer->Write(frame->buf, frame->size);
        }
        else if (m_hardware_encode)
        {
            frameheader.comptype = '4';
            frameheader.packetlength = frame->size;
            WriteFrameheader(&frameheader);
            m_ringBuffer->Write(frame->buf, frame->size);
        }
        else
        {
            frameheader.comptype = '4';
            frameheader.packetlength = tmp;
            WriteFrameheader(&frameheader);
            m_ringBuffer->Write(m_strm, tmp);
        }
    }
    else if (compressthis == 0 || (tmp < (int)out_len))
    {
        if (!raw)
        {
            frameheader.comptype  = '1'; // video compression: RTjpeg only
            frameheader.packetlength = tmp;
            WriteFrameheader(&frameheader);
            m_ringBuffer->Write(m_strm, tmp);
        }
        else
        {
            frameheader.comptype  = '0'; // raw YUV420
            frameheader.packetlength = frame->size;
            WriteFrameheader(&frameheader);
            m_ringBuffer->Write(frame->buf, frame->size); // we write buf directly
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
        m_ringBuffer->Write(m_out, out_len);
    }

    if (m_framesWritten == 0)
        SendMythSystemRecEvent("REC_STARTED_WRITING", m_curRecording);

    m_frameofgop++;
    m_framesWritten++;

    // now we reset the last frame number so that we can find out
    // how many frames we didn't get next time
    m_lf = fnum;
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
    struct rtframeheader frameheader {};

    if (m_last_block == 0)
    {
        m_firsttc = -1;
    }

    if (m_last_block != 0)
    {
        if (fnum != (m_last_block+1))
        {
            m_audio_behind = fnum - (m_last_block+1);
            LOG(VB_RECORD, LOG_INFO, LOC + QString("audio behind %1 %2").
                    arg(m_last_block).arg(fnum));
        }
    }

    frameheader.frametype = 'A'; // audio frame
    frameheader.timecode = timecode;

    if (m_firsttc == -1)
    {
        m_firsttc = timecode;
#if 0
        LOG(VB_GENERAL, LOG_DEBUG, LOC +
            QString("first timecode=%1").arg(m_firsttc));
#endif
    }
    else
    {
        timecode -= m_firsttc; // this is to avoid the lack between the beginning
                             // of recording and the first timestamp, maybe we
                             // can calculate the audio-video +-lack at the
                            // beginning too
        double abytes = (double)m_audiobytes; // - (double)m_audio_buffer_size;
                                     // wrong guess ;-)
        // need seconds instead of msec's
        double mt = (double)timecode;
        if (mt > 0.0)
        {
            double eff = (abytes / mt) * (100000.0 / m_audio_bytes_per_sample);
            m_effectivedsp = (int)eff;
        }
    }

    if (m_compressaudio)
    {
        char mp3gapless[7200];
        int compressedsize = 0;
        int gaplesssize = 0;
        int lameret = 0;

        int sample_cnt = m_audio_buffer_size / m_audio_bytes_per_sample;

#if HAVE_BIGENDIAN
        bswap_16_buf((short int*) buf, sample_cnt, m_audio_channels);
#endif

        if (m_audio_channels == 2)
        {
            lameret = lame_encode_buffer_interleaved(
                m_gf, (short int*) buf, sample_cnt,
                (unsigned char*) m_mp3buf, m_mp3buf_size);
        }
        else
        {
            lameret = lame_encode_buffer(
                m_gf, (short int*) buf, (short int*) buf, sample_cnt,
                (unsigned char*) m_mp3buf, m_mp3buf_size);
        }

        if (lameret < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("lame error '%1'").arg(lameret));
            m_error = QString("Audio Encoding Error '%1'")
                .arg(lameret);
            return;
        }
        compressedsize = lameret;

        lameret = lame_encode_flush_nogap(m_gf, (unsigned char *)mp3gapless,
                                          7200);
        if (lameret < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("lame error '%1'").arg(lameret));
            m_error = QString("Audio Encoding Error '%1'")
                .arg(lameret);
            return;
        }
        gaplesssize = lameret;

        frameheader.comptype = '3'; // audio is compressed
        frameheader.packetlength = compressedsize + gaplesssize;

        if (frameheader.packetlength > 0)
        {
            WriteFrameheader(&frameheader);
            m_ringBuffer->Write(m_mp3buf, compressedsize);
            m_ringBuffer->Write(mp3gapless, gaplesssize);
        }
        m_audiobytes += m_audio_buffer_size;
    }
    else
    {
        frameheader.comptype = '0'; // uncompressed audio
        frameheader.packetlength = m_audio_buffer_size;

        WriteFrameheader(&frameheader);
        m_ringBuffer->Write(buf, m_audio_buffer_size);
        m_audiobytes += m_audio_buffer_size; // only audio no header!!
    }

    // this will probably never happen and if there would be a
    // 'uncountable' video frame drop -> material==worthless
    if (m_audio_behind > 0)
    {
        LOG(VB_RECORD, LOG_INFO, LOC + "audio behind");
        frameheader.frametype = 'A'; // audio frame
        frameheader.comptype  = 'N'; // output a nullframe with
        frameheader.packetlength = 0;
        WriteFrameheader(&frameheader);
        m_audiobytes += m_audio_buffer_size;
        m_audio_behind--;
    }

    m_last_block = fnum;
}

void NuppelVideoRecorder::WriteText(unsigned char *buf, int len, int timecode,
                                    int pagenr)
{
    struct rtframeheader frameheader {};

    frameheader.frametype = 'T'; // text frame
    frameheader.timecode = timecode;

    if (VBIMode::PAL_TT == m_vbimode)
    {
        frameheader.comptype = 'T'; // european teletext
        frameheader.packetlength = len + 4;
        WriteFrameheader(&frameheader);
        union page_t {
            int32_t val32;
            struct { int8_t a,b,c,d; } val8;
        } v {};
        v.val32 = pagenr;
        m_ringBuffer->Write(&v.val8.d, sizeof(int8_t));
        m_ringBuffer->Write(&v.val8.c, sizeof(int8_t));
        m_ringBuffer->Write(&v.val8.b, sizeof(int8_t));
        m_ringBuffer->Write(&v.val8.a, sizeof(int8_t));
        m_ringBuffer->Write(buf, len);
    }
    else if (VBIMode::NTSC_CC == m_vbimode)
    {
        frameheader.comptype = 'C'; // NTSC CC
        frameheader.packetlength = len;

        WriteFrameheader(&frameheader);
        m_ringBuffer->Write(buf, len);
    }
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
