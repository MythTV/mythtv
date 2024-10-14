#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <limits> // workaround QTBUG-90395
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <QStringList>
#include <QtEndian>

#include "libmyth/mythaverror.h"
#include "libmyth/mythavframe.h"
#include "libmyth/mythcontext.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythmiscutil.h"
#include "libmythbase/programinfo.h"

#include "NuppelVideoRecorder.h"
#include "audioinput.h"
#include "channelbase.h"
#include "fourcc.h"
#include "mythavutil.h"
#include "mythsystemevent.h"
#include "recorders/vbitext/cc.h"
#include "recorders/vbitext/vbi.h"
#include "recordingprofile.h"
#include "tv_play.h"
#include "tv_rec.h"

#if HAVE_BIGENDIAN
extern "C" {
#ifndef MYTHTV_BSWAP_H
#define MYTHTV_BSWAP_H

#include <cstdint> /* uint32_t */

#define bswap_dbl(x) bswap_64(x)

#if HAVE_BYTESWAP_H
#  include <byteswap.h> /* bswap_16|32|64 */
#elif HAVE_SYS_ENDIAN_H
#  include <sys/endian.h>
#  if !defined(bswap_16) && defined(bswap16)
#    define bswap_16(x) bswap16(x)
#  endif
#  if !defined(bswap_32) && defined(bswap32)
#    define bswap_32(x) bswap32(x)
#  endif
#  if !defined(bswap_64) && defined(bswap64)
#    define bswap_64(x) bswap64(x)
#  endif
#elif CONFIG_DARWIN
#  include <libkern/OSByteOrder.h>
#  define bswap_16(x) OSSwapInt16(x)
#  define bswap_32(x) OSSwapInt32(x)
#  define bswap_64(x) OSSwapInt64(x)
#elif HAVE_BIGENDIAN
#  error Byte swapping functions not defined for this platform
#endif

#endif /* ndef MYTHTV_BSWAP_H */
}
#endif

extern "C" {
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
}

#ifdef USING_V4L2
#include <linux/videodev2.h>
#include "go7007_myth.h"
#endif // USING_V4L2

#include "io/mythmediabuffer.h"
#include "RTjpegN.h"

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
    V4LRecorder(rec),
    m_seekTable(new std::vector<struct seektable_entry>),
    m_channelObj(channel),
    m_ccd(new CC608Decoder(this))
{
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
    delete [] m_mp3Buf;
    if (m_gf)
        lame_close(m_gf);
    delete [] m_strm;
    if (m_audioDevice)
    {
        delete m_audioDevice;
        m_audioDevice = nullptr;
    }
    if (m_fd >= 0)
        close(m_fd);
    if (m_seekTable)
    {
        m_seekTable->clear();
        delete m_seekTable;
    }

    while (!m_videoBuffer.empty())
    {
        struct vidbuffertype *vb = m_videoBuffer.back();
        delete [] vb->buffer;
        delete vb;
        m_videoBuffer.pop_back();
    }
    while (!m_audioBuffer.empty())
    {
        struct audbuffertype *ab = m_audioBuffer.back();
        delete [] ab->buffer;
        delete ab;
        m_audioBuffer.pop_back();
    }
    while (!m_textBuffer.empty())
    {
        struct txtbuffertype *tb = m_textBuffer.back();
        delete [] tb->buffer;
        delete tb;
        m_textBuffer.pop_back();
    }

    if (m_mpaVidCodec)
        avcodec_free_context(&m_mpaVidCtx);

    delete m_ccd;
}

void NuppelVideoRecorder::SetOption(const QString &opt, int value)
{
    if (opt == "width")
        m_wOut = m_width = value;
    else if (opt == "height")
        m_hOut = m_height = value;
    else if (opt == "rtjpegchromafilter")
        m_m1 = value;
    else if (opt == "rtjpeglumafilter")
        m_m2 = value;
    else if (opt == "rtjpegquality")
        m_q = value;
    else if ((opt == "mpeg4bitrate") || (opt == "mpeg2bitrate"))
        m_targetBitRate = value;
    else if (opt == "scalebitrate")
        m_scaleBitRate = value;
    else if (opt == "mpeg4maxquality")
    {
        if (value > 0)
            m_maxQuality = value;
        else
            m_maxQuality = 1;
    }
    else if (opt == "mpeg4minquality")
    {
        m_minQuality = value;
    }
    else if (opt == "mpeg4qualdiff")
    {
        m_qualDiff = value;
    }
    else if (opt == "encodingthreadcount")
    {
        m_encodingThreadCount = value;
    }
    else if (opt == "mpeg4optionvhq")
    {
        if (value)
            m_mbDecision = FF_MB_DECISION_RD;
        else
            m_mbDecision = FF_MB_DECISION_SIMPLE;
    }
    else if (opt == "mpeg4option4mv")
    {
        if (value)
            m_mp4Opts |= AV_CODEC_FLAG_4MV;
        else
            m_mp4Opts &= ~AV_CODEC_FLAG_4MV;
    }
    else if (opt == "mpeg4optionidct")
    {
        if (value)
            m_mp4Opts |= AV_CODEC_FLAG_INTERLACED_DCT;
        else
            m_mp4Opts &= ~AV_CODEC_FLAG_INTERLACED_DCT;
    }
    else if (opt == "mpeg4optionime")
    {
        if (value)
            m_mp4Opts |= AV_CODEC_FLAG_INTERLACED_ME;
        else
            m_mp4Opts &= ~AV_CODEC_FLAG_INTERLACED_ME;
    }
    else if (opt == "hardwaremjpegquality")
    {
        m_hmjpgQuality = value;
    }
    else if (opt == "hardwaremjpeghdecimation")
    {
        m_hmjpgHDecimation = value;
    }
    else if (opt == "hardwaremjpegvdecimation")
    {
        m_hmjpgVDecimation = value;
    }
    else if (opt == "audiocompression")
    {
        m_compressAudio = (value != 0);
    }
    else if (opt == "mp3quality")
    {
        m_mp3Quality = value;
    }
    else if (opt == "samplerate")
    {
        m_audioSampleRate = value;
    }
    else if (opt == "audioframesize")
    {
        m_audioBufferSize = value;
    }
    else if (opt == "pip_mode")
    {
        m_pipMode = value;
    }
    else if (opt == "inpixfmt")
    {
        m_inPixFmt = (VideoFrameType)value;
    }
    else if (opt == "skipbtaudio")
    {
        m_skipBtAudio = (value != 0);
    }
    else if (opt == "volume")
    {
        m_volume = value;
    }
    else
    {
        V4LRecorder::SetOption(opt, value);
    }
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
    tmp = profile->byName("audiocodec");
    if (tmp != nullptr)
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
    m_clearTimeOnPause = clear;
    m_writePaused = m_audioPaused = m_mainPaused = false;
    m_requestPause = true;

    // The wakeAll is to make sure [write|audio|main]paused are
    // set immediately, even if we were already paused previously.
    m_unpauseWait.wakeAll();
}

bool NuppelVideoRecorder::IsPaused(bool holding_lock) const
{
    if (!holding_lock)
        m_pauseLock.lock();
    bool ret = m_audioPaused && m_mainPaused && m_writePaused;
    if (!holding_lock)
        m_pauseLock.unlock();
    return ret;
}

void NuppelVideoRecorder::SetVideoFilters(QString& /*filters*/)
{
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
    return m_channelFd;
}

bool NuppelVideoRecorder::SetupAVCodecVideo(void)
{
    if (!m_useAvCodec)
        m_useAvCodec = true;

    if (m_mpaVidCodec)
        avcodec_free_context(&m_mpaVidCtx);

    QByteArray vcodec = m_videocodec.toLatin1();
    m_mpaVidCodec = avcodec_find_encoder_by_name(vcodec.constData());

    if (!m_mpaVidCodec)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Video Codec not found: %1")
                .arg(vcodec.constData()));
        return false;
    }

    m_mpaVidCtx = avcodec_alloc_context3(nullptr);

    switch (m_pictureFormat)
    {
        case AV_PIX_FMT_YUV420P:
        case AV_PIX_FMT_YUV422P:
        case AV_PIX_FMT_YUVJ420P:
            m_mpaVidCtx->pix_fmt = m_pictureFormat;
            break;
        default:
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("Unknown picture format: %1")
                    .arg(m_pictureFormat));
    }

    m_mpaVidCtx->width = m_wOut;
    m_mpaVidCtx->height = (int)(m_height * m_heightMultiplier);

    int usebitrate = m_targetBitRate * 1000;
    if (m_scaleBitRate)
    {
        float diff = (m_wOut * m_hOut) / (640.0 * 480.0);
        usebitrate = (int)(diff * usebitrate);
    }

    if (m_targetBitRate == -1)
        usebitrate = -1;

    m_mpaVidCtx->time_base.den = (int)ceil(m_videoFrameRate * 100 *
                                    m_frameRateMultiplier);
    m_mpaVidCtx->time_base.num = 100;

    // avcodec needs specific settings for mpeg2 compression
    switch (m_mpaVidCtx->time_base.den)
    {
        case 2397:
        case 2398: m_mpaVidCtx->time_base.den = 24000;
                   m_mpaVidCtx->time_base.num = 1001;
                   break;
        case 2997:
        case 2998: m_mpaVidCtx->time_base.den = 30000;
                   m_mpaVidCtx->time_base.num = 1001;
                   break;
        case 5994:
        case 5995: m_mpaVidCtx->time_base.den = 60000;
                   m_mpaVidCtx->time_base.num = 1001;
                   break;
    }

    AVDictionary *opts = nullptr;

    m_mpaVidCtx->bit_rate = usebitrate;
    m_mpaVidCtx->bit_rate_tolerance = usebitrate * 100;
    m_mpaVidCtx->qmin = m_maxQuality;
    m_mpaVidCtx->qmax = m_minQuality;
    m_mpaVidCtx->max_qdiff = m_qualDiff;
    m_mpaVidCtx->flags = m_mp4Opts;
    m_mpaVidCtx->mb_decision = m_mbDecision;

    m_mpaVidCtx->qblur = 0.5;
    m_mpaVidCtx->max_b_frames = 0;
    m_mpaVidCtx->b_quant_factor = 0;
    av_dict_set(&opts, "rc_strategy", "2", 0);
    av_dict_set(&opts, "b_strategy", "0", 0);
    m_mpaVidCtx->gop_size = 30;
    m_mpaVidCtx->rc_max_rate = 0;
    m_mpaVidCtx->rc_min_rate = 0;
    m_mpaVidCtx->rc_buffer_size = 0;
    m_mpaVidCtx->rc_override_count = 0;
    av_dict_set(&opts, "rc_init_cplx", "0", 0);
    m_mpaVidCtx->dct_algo = FF_DCT_AUTO;
    m_mpaVidCtx->idct_algo = FF_IDCT_AUTO;
    if (m_videocodec.toLower() == "huffyuv" || m_videocodec.toLower() == "mjpeg")
        m_mpaVidCtx->strict_std_compliance = FF_COMPLIANCE_UNOFFICIAL;
    m_mpaVidCtx->thread_count = m_encodingThreadCount;

    if (avcodec_open2(m_mpaVidCtx, m_mpaVidCodec, &opts) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Unable to open FFMPEG/%1 codec")
                .arg(m_videocodec));
        return false;
    }


    return true;
}

void NuppelVideoRecorder::SetupRTjpeg(void)
{
    m_pictureFormat = AV_PIX_FMT_YUV420P;

    m_rtjc = new RTjpeg();
    int setval = RTJ_YUV420;
    m_rtjc->SetFormat(&setval);
    setval = (int)(m_hOut * m_heightMultiplier);
    m_rtjc->SetSize(&m_wOut, &setval);
    m_rtjc->SetQuality(&m_q);
    setval = 2;
    m_rtjc->SetIntra(&setval, &m_m1, &m_m2);
}


void NuppelVideoRecorder::UpdateResolutions(void)
{
    int tot_height = (int)(m_height * m_heightMultiplier);
    double aspectnum = m_wOut / (double)tot_height;
    uint aspect = 0;

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

    if (m_wOut && tot_height &&
        ((uint)tot_height != m_videoHeight ||
         (uint)m_wOut     != m_videoWidth))
    {
        m_videoHeight = tot_height;
        m_videoWidth = m_wOut;
        ResolutionChange(m_wOut, tot_height, 0);
    }

    int den = (int)ceil(m_videoFrameRate * 100 * m_frameRateMultiplier);
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

    auto frameRate = MythAVRational(den, num);
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
        m_hardwareEncode = true;

        MJPEGInit();

        m_width = m_hmjpgMaxW / m_hmjpgHDecimation;

        if (m_ntsc)
        {
            switch (m_hmjpgVDecimation)
            {
                case 2: m_height = 240; break;
                case 4: m_height = 120; break;
                default: m_height = 480; break;
            }
        }
        else
        {
            switch (m_hmjpgVDecimation)
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
        m_ringBuffer = MythMediaBuffer::Create("output.nuv", true);
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
    {
        m_livetv = m_ringBuffer->LiveMode();
    }

    m_audioBytes = 0;

    InitBuffers();
}

int NuppelVideoRecorder::AudioInit(bool skipdevice)
{
    if (!skipdevice)
    {
        m_audioDevice = AudioInput::CreateDevice(m_audioDeviceName.toLatin1());
        if (!m_audioDevice)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Failed to create audio device: %1") .arg(m_audioDeviceName));
            return 1;
        }

        if (!m_audioDevice->Open(m_audioBits, m_audioSampleRate, m_audioChannels))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Failed to open audio device %1").arg(m_audioDeviceName));
            return 1;
        }

        int blocksize = m_audioDevice->GetBlockSize();
        if (blocksize <= 0)
        {
            blocksize = 1024;
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Failed to determine audio block size on %1,"
                        "using default 1024 bytes").arg(m_audioDeviceName));
        }

        m_audioDevice->Close();
        m_audioBufferSize = blocksize;
    }

    m_audioBytesPerSample = m_audioChannels * m_audioBits / 8;
    LOG(VB_AUDIO, LOG_INFO, LOC +
        QString("Audio device %1 buffer size: %1 bytes")
            .arg(m_audioBufferSize));

    if (m_compressAudio)
    {
        m_gf = lame_init();
        lame_set_bWriteVbrTag(m_gf, 0);
        lame_set_quality(m_gf, m_mp3Quality);
        lame_set_compression_ratio(m_gf, 11);
        lame_set_mode(m_gf, m_audioChannels == 2 ? STEREO : MONO);
        lame_set_num_channels(m_gf, m_audioChannels);
        lame_set_in_samplerate(m_gf, m_audioSampleRate);
        int tmp = lame_init_params(m_gf);
        if (tmp != 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("AudioInit(): lame_init_params error %1").arg(tmp));
            m_compressAudio = false;
        }

        if (m_audioBits != 16)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "AudioInit(): lame support requires 16bit audio");
            m_compressAudio = false;
        }
    }
    m_mp3BufSize = (int)((1.25 * 16384) + 7200);
    m_mp3Buf = new char[m_mp3BufSize];

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
    LOG(VB_GENERAL, LOG_ERR, LOC + "MJPEG not supported by device");
    return false;
}

void NuppelVideoRecorder::InitBuffers(void)
{
    int videomegs = 0;
    int audiomegs = 2;

    if (!m_videoBufferSize)
    {
        m_videoBufferSize = static_cast<long>(
            MythVideoFrame::GetBufferSize(m_pictureFormat == AV_PIX_FMT_YUV422P ? FMT_YUV422P : FMT_YV12,
                                          m_wOut, m_hOut));
    }

    if (m_width >= 480 || m_height > 288)
        videomegs = 20;
    else
        videomegs = 12;

    m_videoBufferCount = (videomegs * 1000 * 1000) / static_cast<int>(m_videoBufferSize);

    if (m_audioBufferSize != 0)
        m_audioBufferCount = (audiomegs * 1000 * 1000) / static_cast<int>(m_audioBufferSize);
    else
        m_audioBufferCount = 0;

    m_textBufferSize = 8 * (sizeof(teletextsubtitle) + VT_WIDTH);
    m_textBufferCount = m_videoBufferCount;

    for (int i = 0; i < m_videoBufferCount; i++)
    {
        auto *vidbuf = new vidbuffertype;
        vidbuf->buffer = new unsigned char[m_videoBufferSize];
        vidbuf->sample = 0;
        vidbuf->freeToEncode = 0;
        vidbuf->freeToBuffer = 1;
        vidbuf->bufferlen = 0;
        vidbuf->forcekey = false;

        m_videoBuffer.push_back(vidbuf);
    }

    for (int i = 0; i < m_audioBufferCount; i++)
    {
        auto *audbuf = new audbuffertype;
        audbuf->buffer = new unsigned char[m_audioBufferSize];
        audbuf->sample = 0;
        audbuf->freeToEncode = 0;
        audbuf->freeToBuffer = 1;

        m_audioBuffer.push_back(audbuf);
    }

    for (int i = 0; i < m_textBufferCount; i++)
    {
        auto *txtbuf = new txtbuffertype;
        txtbuf->buffer = new unsigned char[m_textBufferSize];
        txtbuf->freeToEncode = 0;
        txtbuf->freeToBuffer = 1;

        m_textBuffer.push_back(txtbuf);
    }
}

void NuppelVideoRecorder::ResizeVideoBuffers(void)
{
    for (auto & vidbuf : m_videoBuffer)
    {
        delete [] (vidbuf->buffer);
        vidbuf->buffer = new unsigned char[m_videoBufferSize];
    }
}

void NuppelVideoRecorder::StreamAllocate(void)
{
    delete [] m_strm;
    m_strm = new signed char[(m_width * m_height * 2) + 10];
}

bool NuppelVideoRecorder::Open(void)
{
    if (m_channelFd>0)
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

    m_channelFd = m_fd;
    return true;
}

void NuppelVideoRecorder::ProbeV4L2(void)
{
#ifdef USING_V4L2
    m_usingV4l2 = true;

    struct v4l2_capability vcap {};

    if (ioctl(m_channelFd, VIDIOC_QUERYCAP, &vcap) < 0)
    {
        m_usingV4l2 = false;
    }

    if (m_usingV4l2 && !(vcap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Not a v4l2 capture device, falling back to v4l");
        m_usingV4l2 = false;
    }

    if (m_usingV4l2 && !(vcap.capabilities & V4L2_CAP_STREAMING))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Won't work with the streaming interface, falling back");
        m_usingV4l2 = false;
    }

    if (vcap.card[0] == 'B' && vcap.card[1] == 'T' &&
        vcap.card[2] == '8' && vcap.card[4] == '8')
        m_correctBttv = true;

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

    if (m_usingV4l2 && !SetFormatV4L2())
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

    m_useAvCodec = (m_videocodec.toLower() != "rtjpeg");
    if (m_useAvCodec)
        m_useAvCodec = SetupAVCodecVideo();

    if (!m_useAvCodec)
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
        m_requestRecording = true;
        m_requestHelper = true;
        m_recording = true;
        m_recordingWait.wakeAll();
    }

    m_writeThread = new NVRWriteThread(this);
    m_writeThread->start();

    m_audioThread = new NVRAudioThread(this);
    m_audioThread->start();

    if ((m_vbiMode != VBIMode::None) && (OpenVBIDevice() >= 0))
        m_vbiThread = new VBIThread(this);

    // save the start time
    gettimeofday(&m_stm, nullptr);

    // try to get run at higher scheduling priority, ignore failure
    myth_nice(-10);

    if (m_usingV4l2)
    {
        m_inPixFmt = FMT_NONE;;
        DoV4L2();
    }

    {
        QMutexLocker locker(&m_pauseLock);
        m_requestRecording = false;
        m_requestHelper = false;
        m_recording = false;
        m_recordingWait.wakeAll();
    }
}

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
    else if (m_inPixFmt == FMT_YUV422P)
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
            if (m_inPixFmt == FMT_YUV422P)
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
            if (m_inPixFmt == FMT_YUV422P)
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
    else
    {
        // cool, we can do our preferred format, most likely running on bttv.
        LOG(VB_RECORD, LOG_INFO, LOC +
            "v4l2: format set, getting yuv420 from v4l");
    }

    // VIDIOC_S_FMT might change the format, check it
    if (m_width  != (int)vfmt.fmt.pix.width ||
        m_height != (int)vfmt.fmt.pix.height)
    {
        LOG(VB_RECORD, LOG_INFO, LOC +
            QString("v4l2: resolution changed. requested %1x%2, using "
                    "%3x%4 now")
                .arg(m_width).arg(m_height)
                .arg(vfmt.fmt.pix.width) .arg(vfmt.fmt.pix.height));
        m_wOut = m_width  = vfmt.fmt.pix.width;
        m_hOut = m_height = vfmt.fmt.pix.height;
    }

    m_v4l2PixelFormat = vfmt.fmt.pix.pixelformat;

    return true;
}
#else // if !USING_V4L2
bool NuppelVideoRecorder::SetFormatV4L2(void) { return false; }
#endif // !USING_V4L2

#ifdef USING_V4L2
static constexpr size_t MAX_VIDEO_BUFFERS { 5 };
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

        comp.gop_size = m_keyframeDist;
        comp.max_b_frames = 0;

        if (fabs(m_videoAspect - 1.33333F) < 0.01F)
        {
            if (m_ntsc)
                comp.aspect_ratio = GO7007_ASPECT_RATIO_4_3_NTSC;
            else
                comp.aspect_ratio = GO7007_ASPECT_RATIO_4_3_PAL;
        }
        else if (fabs(m_videoAspect - 1.77777F) < 0.01F)
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

        int usebitrate = m_targetBitRate * 1000;
        if (m_scaleBitRate)
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

        m_hardwareEncode = true;
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

    std::array<uint8_t*,MAX_VIDEO_BUFFERS> buffers   {};
    std::array<int,MAX_VIDEO_BUFFERS>      bufferlen {};

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

    struct timeval tv {};
    fd_set rdset {};
    int frame = 0;
    bool forcekey = false;

    m_resetCapture = false;

    // setup pixel format conversions for YUYV and UYVY
    uint8_t *output_buffer = nullptr;
    struct SwsContext *convert_ctx = nullptr;
    AVFrame img_out;
    if (m_v4l2PixelFormat == V4L2_PIX_FMT_YUYV ||
        m_v4l2PixelFormat == V4L2_PIX_FMT_UYVY)
    {
        AVPixelFormat in_pixfmt = m_v4l2PixelFormat == V4L2_PIX_FMT_YUYV ?
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
            if (m_requestPause)
            {
                if (!m_mainPaused)
                {
                    m_mainPaused = true;
                    m_pauseWait.wakeAll();
                    if (IsPaused(true) && m_tvrec)
                        m_tvrec->RecorderPaused();
                }
                m_unpauseWait.wait(&m_pauseLock, 100);
                if (m_clearTimeOnPause)
                    gettimeofday(&m_stm, nullptr);
                continue;
            }

            if (!m_requestPause && m_mainPaused)
            {
                m_mainPaused = false;
                m_unpauseWait.wakeAll();
            }
        }

        if (m_resetCapture)
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
             m_resetCapture = false;
        }

        tv.tv_sec = 5;
        tv.tv_usec = 0;
        FD_ZERO(&rdset); // NOLINT(readability-isolate-declaration)
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
                m_resetCapture = true;
                continue;
            }

            if (errno == EIO || errno == EINVAL)
            {
                m_resetCapture = true;
                continue;
            }

            if (errno == EAGAIN)
                continue;
        }

        frame = vbuf.index;
        if (m_go7007)
            forcekey = ((vbuf.flags & V4L2_BUF_FLAG_KEYFRAME) != 0U);

        if (!m_requestPause)
        {
            if ((m_v4l2PixelFormat == V4L2_PIX_FMT_YUYV) &&
                     (output_buffer != nullptr))
            {
                AVFrame img_in;
                av_image_fill_arrays(img_in.data, img_in.linesize,
                    buffers[frame], AV_PIX_FMT_YUYV422, m_width, m_height,
                    IMAGE_ALIGN);
                sws_scale(convert_ctx, img_in.data, img_in.linesize,
                          0, m_height, img_out.data, img_out.linesize);
                BufferIt(output_buffer, m_videoBufferSize);
            }
            else if ((m_v4l2PixelFormat == V4L2_PIX_FMT_UYVY) &&
                     (output_buffer != nullptr))
            {
                AVFrame img_in;
                av_image_fill_arrays(img_in.data, img_in.linesize,
                    buffers[frame], AV_PIX_FMT_UYVY422, m_width, m_height,
                    IMAGE_ALIGN);
                sws_scale(convert_ctx, img_in.data, img_in.linesize,
                          0, m_height, img_out.data, img_out.linesize);
                BufferIt(output_buffer, m_videoBufferSize);
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
    close(m_channelFd);
}
#else // if !USING_V4L2
void NuppelVideoRecorder::DoV4L2(void) {}
#endif // !USING_V4L2

void NuppelVideoRecorder::KillChildren(void)
{
    {
        QMutexLocker locker(&m_pauseLock);
        m_requestHelper = false;
        m_unpauseWait.wakeAll();
    }

    if (m_writeThread)
    {
        m_writeThread->wait();
        delete m_writeThread;
        m_writeThread = nullptr;
    }

    if (m_audioThread)
    {
        m_audioThread->wait();
        delete m_audioThread;
        m_audioThread = nullptr;
    }

    if (m_vbiThread)
    {
        m_vbiThread->wait();
        delete m_vbiThread;
        m_vbiThread = nullptr;
        CloseVBIDevice();
    }
}

void NuppelVideoRecorder::BufferIt(unsigned char *buf, int len, bool forcekey)
{
    struct timeval now {};

    int act = m_actVideoBuffer;

    if (!m_videoBuffer[act]->freeToBuffer) {
        return;
    }

    gettimeofday(&now, nullptr);

    auto tcres = durationFromTimevalDelta<std::chrono::milliseconds>(now, m_stm);

    m_useBttv = 0;
    // here is the non preferable timecode - drop algorithm - fallback
    if (!m_useBttv)
    {
        if (m_tf==0)
            m_tf = 2;
        else
        {
            int fn = (tcres - m_oldTc).count();

     // the difference should be less than 1,5*timeperframe or we have
     // missed at least one frame, this code might be inaccurate!

            if (m_ntscFrameRate)
                fn = (fn+16)/33;
            else
                fn = (fn+20)/40;
            fn = std::max(fn, 1);
            m_tf += 2*fn; // two fields
        }
    }

    m_oldTc = tcres;

    if (!m_videoBuffer[act]->freeToBuffer)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            "DROPPED frame due to full buffer in the recorder.");
        return; // we can't buffer the current frame
    }

    m_videoBuffer[act]->sample = m_tf;

    // record the time at the start of this frame.
    // 'tcres' is at the end of the frame, so subtract the right # of ms
    m_videoBuffer[act]->timecode =
        (m_ntscFrameRate) ? (tcres - 33ms) : (tcres - 40ms);

    memcpy(m_videoBuffer[act]->buffer, buf, len);
    m_videoBuffer[act]->bufferlen = len;
    m_videoBuffer[act]->forcekey = forcekey;

    m_videoBuffer[act]->freeToBuffer = 0;
    m_actVideoBuffer++;
    if (m_actVideoBuffer >= m_videoBufferCount)
        m_actVideoBuffer = 0; // cycle to begin of buffer
    m_videoBuffer[act]->freeToEncode = 1; // set last to prevent race
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
    if (newaspect == static_cast<double>(m_videoAspect))
        return;

    m_videoAspect = newaspect;

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
    static const std::string kFinfo { "MythTVVideo" };
    static const std::string kVers  { "0.07" };

    std::copy(kFinfo.cbegin(), kFinfo.cend(), fileheader.finfo);
    std::copy(kVers.cbegin(), kVers.cend(), fileheader.version);
    fileheader.width  = m_wOut;
    fileheader.height = (int)(m_hOut * m_heightMultiplier);
    fileheader.desiredwidth  = 0;
    fileheader.desiredheight = 0;
    fileheader.pimode = 'P';
    fileheader.aspect = m_videoAspect;
    fileheader.fps = m_videoFrameRate;
    fileheader.fps *= m_frameRateMultiplier;
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

    WriteFileHeader();

    frameheader.frametype = 'D'; // compressor data

    if (m_useAvCodec)
    {
        frameheader.comptype = 'F';
        frameheader.packetlength = m_mpaVidCtx->extradata_size;

        WriteFrameheader(&frameheader);
        m_ringBuffer->Write(m_mpaVidCtx->extradata, frameheader.packetlength);
    }
    else
    {
        static std::array<uint32_t,128> s_tbls {};

        frameheader.comptype = 'R'; // compressor data for RTjpeg
        frameheader.packetlength = s_tbls.size() * sizeof(uint32_t);

        // compression configuration header
        WriteFrameheader(&frameheader);

        m_ringBuffer->Write(s_tbls.data(), s_tbls.size() * sizeof(uint32_t));
    }

    memset(&frameheader, 0, sizeof(frameheader));
    frameheader.frametype = 'X'; // extended data
    frameheader.packetlength = sizeof(extendeddata);

    // extended data header
    WriteFrameheader(&frameheader);

    struct extendeddata moredata {};

    moredata.version = 1;
    if (m_useAvCodec)
    {
        int vidfcc = 0;
        switch(m_mpaVidCodec->id)
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
        moredata.lavc_bitrate = m_mpaVidCtx->bit_rate;
        moredata.lavc_qmin = m_mpaVidCtx->qmin;
        moredata.lavc_qmax = m_mpaVidCtx->qmax;
        moredata.lavc_maxqdiff = m_mpaVidCtx->max_qdiff;
    }
    else
    {
        moredata.video_fourcc = FOURCC_RJPG;
        moredata.rtjpeg_quality = m_q;
        moredata.rtjpeg_luma_filter = m_m1;
        moredata.rtjpeg_chroma_filter = m_m2;
    }

    if (m_compressAudio)
    {
        moredata.audio_fourcc = FOURCC_LAME;
        moredata.audio_compression_ratio = 11;
        moredata.audio_quality = m_mp3Quality;
    }
    else
    {
        moredata.audio_fourcc = FOURCC_RAWA;
    }

    moredata.audio_sample_rate = m_audioSampleRate;
    moredata.audio_channels = m_audioChannels;
    moredata.audio_bits_per_sample = m_audioBits;

    m_extendedDataOffset = m_ringBuffer->GetWritePosition();

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

    m_lastBlock = 0;
    m_lf = 0; // that resets framenumber so that seeking in the
              // continues parts works too
}

void NuppelVideoRecorder::WriteSeekTable(void)
{
    int numentries = m_seekTable->size();

    struct rtframeheader frameheader {};
    frameheader.frametype = 'Q'; // SeekTable
    frameheader.packetlength = sizeof(struct seektable_entry) * numentries;

    long long currentpos = m_ringBuffer->GetWritePosition();

    m_ringBuffer->Write(&frameheader, sizeof(frameheader));

    char *seekbuf = new char[frameheader.packetlength];
    int offset = 0;

    for (auto & entry : *m_seekTable)
    {
        memcpy(seekbuf + offset, (const void *)&entry,
               sizeof(struct seektable_entry));
        offset += sizeof(struct seektable_entry);
    }

    m_ringBuffer->Write(seekbuf, frameheader.packetlength);

    m_ringBuffer->WriterSeek(m_extendedDataOffset +
                           offsetof(struct extendeddata, seektable_offset),
                           SEEK_SET);

    m_ringBuffer->Write(&currentpos, sizeof(long long));

    m_ringBuffer->WriterSeek(0, SEEK_END);

    delete [] seekbuf;
}

void NuppelVideoRecorder::WriteKeyFrameAdjustTable(
    const std::vector<struct kfatable_entry> &kfa_table)
{
    int numentries = kfa_table.size();

    struct rtframeheader frameheader {};
    frameheader.frametype = 'K'; // KFA Table
    frameheader.packetlength = sizeof(struct kfatable_entry) * numentries;

    long long currentpos = m_ringBuffer->GetWritePosition();

    m_ringBuffer->Write(&frameheader, sizeof(frameheader));

    char *kfa_buf = new char[frameheader.packetlength];
    uint offset = 0;

    for (const auto& kfa : kfa_table)
    {
        memcpy(kfa_buf + offset, &kfa,
               sizeof(struct kfatable_entry));
        offset += sizeof(struct kfatable_entry);
    }

    m_ringBuffer->Write(kfa_buf, frameheader.packetlength);


    m_ringBuffer->WriterSeek(m_extendedDataOffset +
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
    m_seekTable->push_back(ste);

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

    for (int i = 0; i < m_videoBufferCount; i++)
    {
        vidbuffertype *vidbuf = m_videoBuffer[i];
        vidbuf->sample = 0;
        vidbuf->timecode = 0ms;
        vidbuf->freeToEncode = 0;
        vidbuf->freeToBuffer = 1;
        vidbuf->forcekey = false;
    }

    for (int i = 0; i < m_audioBufferCount; i++)
    {
        audbuffertype *audbuf = m_audioBuffer[i];
        audbuf->sample = 0;
        audbuf->timecode = 0ms;
        audbuf->freeToEncode = 0;
        audbuf->freeToBuffer = 1;
    }

    for (int i = 0; i < m_textBufferCount; i++)
    {
        txtbuffertype *txtbuf = m_textBuffer[i];
        txtbuf->freeToEncode = 0;
        txtbuf->freeToBuffer = 1;
    }

    m_actVideoEncode = 0;
    m_actVideoBuffer = 0;
    m_actAudioEncode = 0;
    m_actAudioBuffer = 0;
    m_actAudioSample = 0;
    m_actTextEncode = 0;
    m_actTextBuffer = 0;

    m_audioBytes = 0;
    m_effectiveDsp = 0;

    if (m_useAvCodec)
        SetupAVCodecVideo();

    if (m_curRecording)
        m_curRecording->ClearPositionMap(MARK_KEYFRAME);
}

void NuppelVideoRecorder::doAudioThread(void)
{
    if (!m_audioDevice)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Invalid audio device (%1), exiting").arg(m_audioDeviceName));
        return;
    }

    if (!m_audioDevice->Open(m_audioBits, m_audioSampleRate, m_audioChannels))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Failed to open audio device %1").arg(m_audioDeviceName));
        return;
    }

    if (!m_audioDevice->Start())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Failed to start audio capture on %1").arg(m_audioDeviceName));
        return;
    }

    struct timeval anow {};
    auto *buffer = new unsigned char[m_audioBufferSize];
    m_audioBytesPerSample = m_audioChannels * m_audioBits / 8;

    while (IsHelperRequested() && !IsErrored())
    {
        {
            QMutexLocker locker(&m_pauseLock);
            if (m_requestPause)
            {
                if (!m_audioPaused)
                {
                    m_audioPaused = true;
                    m_pauseWait.wakeAll();
                    if (IsPaused(true) && m_tvrec)
                        m_tvrec->RecorderPaused();
                }
                m_unpauseWait.wait(&m_pauseLock, 100);
                continue;
            }

            if (!m_requestPause && m_audioPaused)
            {
                m_audioPaused = false;
                m_unpauseWait.wakeAll();
            }
        }

        if (!IsHelperRequested() || IsErrored())
            break;

        int lastread = m_audioDevice->GetSamples(buffer, m_audioBufferSize);
        if (m_audioBufferSize != lastread)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Short read, %1 of %2 bytes from ")
                    .arg(lastread).arg(m_audioBufferSize) + m_audioDeviceName);
        }

        /* record the current time */
        /* Don't assume that the sound device's record buffer is empty
           (like we used to.) Measure to see how much stuff is in there,
           and correct for it when calculating the timestamp */
        gettimeofday(&anow, nullptr);
        int bytes_read = std::max(m_audioDevice->GetNumReadyBytes(), 0);

        int act = m_actAudioBuffer;

        if (!m_audioBuffer[act]->freeToBuffer)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Ran out of free AUDIO buffers :-(");
            m_actAudioSample++;
            continue;
        }

        m_audioBuffer[act]->sample = m_actAudioSample;

        /* calculate timecode. First compute the difference
           between now and stm (start time) */
        m_audioBuffer[act]->timecode =
            durationFromTimevalDelta<std::chrono::milliseconds>(anow, m_stm);
        /* We want the timestamp to point to the start of this
           audio chunk. So, subtract off the length of the chunk
           and the length of audio still in the capture buffer. */
        m_audioBuffer[act]->timecode -= millisecondsFromFloat(
                (bytes_read + m_audioBufferSize)
                 * 1000.0 / (m_audioSampleRate * m_audioBytesPerSample));

        memcpy(m_audioBuffer[act]->buffer, buffer, m_audioBufferSize);

        m_audioBuffer[act]->freeToBuffer = 0;
        m_actAudioBuffer++;
        if (m_actAudioBuffer >= m_audioBufferCount)
            m_actAudioBuffer = 0;
        m_audioBuffer[act]->freeToEncode = 1;

        m_actAudioSample++;
    }

    delete [] buffer;

    if (m_audioDevice->IsOpen())
        m_audioDevice->Close();
}

#ifdef USING_V4L2
void NuppelVideoRecorder::FormatTT(struct VBIData *vbidata)
{
    struct timeval tnow {};
    gettimeofday(&tnow, nullptr);

    int act = m_actTextBuffer;
    if (!m_textBuffer[act]->freeToBuffer)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Teletext #%1: ran out of free TEXT buffers :-(").arg(act));
        return;
    }

    // calculate timecode:
    // compute the difference  between now and stm (start time)
    m_textBuffer[act]->timecode =
            durationFromTimevalDelta<std::chrono::milliseconds>(tnow, m_stm);
    m_textBuffer[act]->pagenr = (vbidata->teletextpage.pgno << 16) +
                                vbidata->teletextpage.subno;

    unsigned char *inpos = vbidata->teletextpage.data[0];
    unsigned char *outpos = m_textBuffer[act]->buffer;
    *outpos = 0;
    struct teletextsubtitle st {};
    std::vector<uint8_t> linebuf {};
    linebuf.reserve(VT_WIDTH + 1);

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
            {
                if ((c & 0xa0) == 0x20)
                {
                    last_c = c;
                    c += (c & 0x40) ? 32 : -32;
                }
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
                    linebuf.clear();
                }
                linebuf.push_back(c);
                visible = 1;
            }

            (void) box;
            (void) sep;
        }
        if (visible)
        {
            st.len = linebuf.size() + 1;
            int maxlen = 200;
            int bufsize = ((outpos - m_textBuffer[act]->buffer + 1) + st.len);
            if (bufsize > maxlen)
                break;
            memcpy(outpos, &st, sizeof(st));
            outpos += sizeof(st);
            int count = std::max(st.len, static_cast<uint8_t>(41));
            std::copy(linebuf.cbegin(), linebuf.cbegin() + count, outpos);
            outpos += count;
            *outpos = 0;
        }
    }

    m_textBuffer[act]->bufferlen = outpos - m_textBuffer[act]->buffer + 1;
    m_textBuffer[act]->freeToBuffer = 0;
    m_actTextBuffer++;
    if (m_actTextBuffer >= m_textBufferCount)
        m_actTextBuffer = 0;
    m_textBuffer[act]->freeToEncode = 1;
}
#else  // USING_V4L2
void NuppelVideoRecorder::FormatTT(struct VBIData*) {}
#endif // USING_V4L2

void NuppelVideoRecorder::FormatCC(uint code1, uint code2)
{
    struct timeval tnow {};
    gettimeofday (&tnow, nullptr);

    // calculate timecode:
    // compute the difference  between now and stm (start time)
    auto tc = durationFromTimevalDelta<std::chrono::milliseconds>(tnow, m_stm);
    m_ccd->FormatCC(tc, code1, code2);
}

void NuppelVideoRecorder::AddTextData(unsigned char *buf, int len,
                                      std::chrono::milliseconds timecode, char /*type*/)
{
    int act = m_actTextBuffer;
    if (!m_textBuffer[act]->freeToBuffer)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Teletext#%1").arg(act) +
                " ran out of free TEXT buffers :-(");
        return;
    }

    m_textBuffer[act]->timecode = timecode;
    memcpy(m_textBuffer[act]->buffer, buf, len);
    m_textBuffer[act]->bufferlen = len + sizeof(ccsubtitle);

    m_textBuffer[act]->freeToBuffer = 0;
    m_actTextBuffer++;
    if (m_actTextBuffer >= m_textBufferCount)
        m_actTextBuffer = 0;
    m_textBuffer[act]->freeToEncode = 1;
}

void NuppelVideoRecorder::doWriteThread(void)
{
    while (IsHelperRequested() && !IsErrored())
    {
        {
            QMutexLocker locker(&m_pauseLock);
            if (m_requestPause)
            {
                if (!m_writePaused)
                {
                    m_writePaused = true;
                    m_pauseWait.wakeAll();
                    if (IsPaused(true) && m_tvrec)
                        m_tvrec->RecorderPaused();
                }
                m_unpauseWait.wait(&m_pauseLock, 100);
                continue;
            }

            if (!m_requestPause && m_writePaused)
            {
                m_writePaused = false;
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
        std::chrono::milliseconds firsttimecode = -1ms;

        if (m_videoBuffer[m_actVideoEncode]->freeToEncode)
        {
            action = ACTION_VIDEO;
            firsttimecode = m_videoBuffer[m_actVideoEncode]->timecode;
        }

        if (m_audioBufferCount &&
            m_audioBuffer[m_actAudioEncode]->freeToEncode &&
            (action == ACTION_NONE ||
             (m_audioBuffer[m_actAudioEncode]->timecode < firsttimecode)))
        {
            action = ACTION_AUDIO;
            firsttimecode = m_audioBuffer[m_actAudioEncode]->timecode;
        }

        if (m_textBufferCount &&
            m_textBuffer[m_actTextEncode]->freeToEncode &&
            (action == ACTION_NONE ||
             (m_textBuffer[m_actTextEncode]->timecode < firsttimecode)))
        {
            action = ACTION_TEXT;
        }

        switch (action)
        {
            case ACTION_VIDEO:
            {
                MythVideoFrame frame(FMT_YV12, m_videoBuffer[m_actVideoEncode]->buffer,
                                     m_videoBuffer[m_actVideoEncode]->bufferlen,
                                     m_width, m_height);
                frame.m_frameNumber = m_videoBuffer[m_actVideoEncode]->sample;
                frame.m_timecode = m_videoBuffer[m_actVideoEncode]->timecode;
                frame.m_forceKey = m_videoBuffer[m_actVideoEncode]->forcekey;
                WriteVideo(&frame);
                // Ensure buffer isn't deleted
                frame.m_buffer = nullptr;

                m_videoBuffer[m_actVideoEncode]->sample = 0;
                m_videoBuffer[m_actVideoEncode]->freeToEncode = 0;
                m_videoBuffer[m_actVideoEncode]->freeToBuffer = 1;
                m_videoBuffer[m_actVideoEncode]->forcekey = false;
                m_actVideoEncode++;
                if (m_actVideoEncode >= m_videoBufferCount)
                    m_actVideoEncode = 0;
                break;
            }
            case ACTION_AUDIO:
            {
                WriteAudio(m_audioBuffer[m_actAudioEncode]->buffer,
                           m_audioBuffer[m_actAudioEncode]->sample,
                           m_audioBuffer[m_actAudioEncode]->timecode);
                if (IsErrored()) {
                    LOG(VB_GENERAL, LOG_ERR, LOC +
                        "ACTION_AUDIO cannot be completed due to error.");
                    StopRecording();
                    break;
                }
                m_audioBuffer[m_actAudioEncode]->sample = 0;
                m_audioBuffer[m_actAudioEncode]->freeToEncode = 0;
                m_audioBuffer[m_actAudioEncode]->freeToBuffer = 1;
                m_actAudioEncode++;
                if (m_actAudioEncode >= m_audioBufferCount)
                    m_actAudioEncode = 0;
                break;
            }
            case ACTION_TEXT:
            {
                WriteText(m_textBuffer[m_actTextEncode]->buffer,
                          m_textBuffer[m_actTextEncode]->bufferlen,
                          m_textBuffer[m_actTextEncode]->timecode,
                          m_textBuffer[m_actTextEncode]->pagenr);
                m_textBuffer[m_actTextEncode]->freeToEncode = 0;
                m_textBuffer[m_actTextEncode]->freeToBuffer = 1;
                m_actTextEncode++;
                if (m_actTextEncode >= m_textBufferCount)
                    m_actTextEncode = 0;
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
    m_lastBlock = 0;

    m_seekTable->clear();

    ClearStatistics();

    m_positionMapLock.lock();
    m_positionMap.clear();
    m_positionMapDelta.clear();
    m_positionMapLock.unlock();

    if (m_go7007)
        m_resetCapture = true;
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

void NuppelVideoRecorder::WriteVideo(MythVideoFrame *frame, bool skipsync,
                                     bool forcekey)
{
    int tmp = 0;
    lzo_uint out_len = kOutLen;
    struct rtframeheader frameheader {};
    int raw = 0;
    int compressthis = m_compression;
    std::array<uint8_t*,3> planes {
        frame->m_buffer + frame->m_offsets[0],
        frame->m_buffer + frame->m_offsets[1],
        frame->m_buffer + frame->m_offsets[2] };
    int fnum = frame->m_frameNumber;
    std::chrono::milliseconds timecode = frame->m_timecode;

    if (m_lf == 0)
    {   // this will be triggered every new file
        m_lf = fnum;
        m_startNum = fnum;
        m_lastTimecode = 0;
        m_frameOfGop = 0;
        forcekey = true;
    }

    // see if it's time for a seeker header, sync information and a keyframe
    frameheader.keyframe  = m_frameOfGop;             // no keyframe defaulted

    bool wantkeyframe = forcekey;

    bool writesync = false;

    if ((!m_go7007 && (((fnum-m_startNum)>>1) % m_keyframeDist == 0 && !skipsync)) ||
        (m_go7007 && frame->m_forceKey))
        writesync = true;

    if (writesync)
    {
        m_ringBuffer->Write("RTjjjjjjjjjjjjjjjjjjjjjjjj", FRAMEHEADERSIZE);

        UpdateSeekTable(((fnum - m_startNum) >> 1) / m_keyframeDist);

        frameheader.frametype    = 'S';           // sync frame
        frameheader.comptype     = 'V';           // video sync information
        frameheader.filters      = 0;             // no filters applied
        frameheader.packetlength = 0;             // no data packet
        frameheader.timecode     = (fnum-m_startNum)>>1;
        // write video sync info
        WriteFrameheader(&frameheader);
        frameheader.frametype    = 'S';           // sync frame
        frameheader.comptype     = 'A';           // video sync information
        frameheader.filters      = 0;             // no filters applied
        frameheader.packetlength = 0;             // no data packet
        frameheader.timecode     = m_effectiveDsp;  // effective dsp frequency
        // write audio sync info
        WriteFrameheader(&frameheader);

        wantkeyframe = true;
        //m_ringBuffer->Sync();
    }

    if (wantkeyframe)
    {
        frameheader.keyframe=0;
        m_frameOfGop=0;
    }

    if (m_useAvCodec)
    {
        MythAVFrame mpa_picture;
        MythAVUtil::FillAVFrame(mpa_picture, frame);

        if (wantkeyframe)
            mpa_picture->pict_type = AV_PICTURE_TYPE_I;
        else
            mpa_picture->pict_type = AV_PICTURE_TYPE_NONE;

        if (!m_hardwareEncode)
        {
            AVPacket *packet = av_packet_alloc();
            if (packet == nullptr)
            {
                LOG(VB_RECORD, LOG_ERR, "packet allocation failed");
                return;
            }
            packet->data = (uint8_t *)m_strm;
            packet->size = frame->m_bufferSize;

            bool got_packet = false;
            int ret = avcodec_receive_packet(m_mpaVidCtx, packet);
            if (ret == 0)
                got_packet = true;
            if (ret == AVERROR(EAGAIN))
                ret = 0;
            if (ret == 0)
                ret = avcodec_send_frame(m_mpaVidCtx, mpa_picture);
            // if ret from avcodec_send_frame is AVERROR(EAGAIN) then
            // there are 2 packets to be received while only 1 frame to be
            // sent. The code does not cater for this. Hopefully it will not happen.

            if (ret < 0)
            {
                std::string error;
                LOG(VB_GENERAL, LOG_ERR, LOC + QString("video encode error: %1 (%2)")
                    .arg(av_make_error_stdstring(error, ret)).arg(got_packet));
                av_packet_free(&packet);
                return;
            }

            if (!got_packet)
            {
                av_packet_free(&packet);
                return;
            }


            tmp = packet->size;
            av_packet_free(&packet);
        }
    }
    else
    {
        int freecount = 0;
        freecount = m_actVideoBuffer > m_actVideoEncode ?
                    m_videoBufferCount - (m_actVideoBuffer - m_actVideoEncode) :
                    m_actVideoEncode - m_actVideoBuffer;

        if (freecount < (m_videoBufferCount / 3))
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
            tmp = m_rtjc->Compress(m_strm, planes.data());
        }
        else
        {
            tmp = frame->m_bufferSize;
        }

        // here is lzo compression afterwards
        if (compressthis)
        {
            int r = 0;
            if (raw)
            {
                r = lzo1x_1_compress(frame->m_buffer, frame->m_bufferSize,
                                     m_out.data(), &out_len, m_wrkmem.data());
            }
            else
            {
                r = lzo1x_1_compress((unsigned char *)m_strm, tmp, m_out.data(),
                                     &out_len, m_wrkmem.data());
            }
            if (r != LZO_E_OK)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + "lzo compression failed");
                return;
            }
        }
    }

    frameheader.frametype = 'V'; // video frame
    frameheader.timecode  = timecode.count();
    m_lastTimecode = frameheader.timecode;
    frameheader.filters   = 0;             // no filters applied

    // compr ends here
    if (m_useAvCodec)
    {
        if (m_mpaVidCodec->id == AV_CODEC_ID_RAWVIDEO)
        {
            frameheader.comptype = '0';
            frameheader.packetlength = frame->m_bufferSize;
            WriteFrameheader(&frameheader);
            m_ringBuffer->Write(frame->m_buffer, frame->m_bufferSize);
        }
        else if (m_hardwareEncode)
        {
            frameheader.comptype = '4';
            frameheader.packetlength = frame->m_bufferSize;
            WriteFrameheader(&frameheader);
            m_ringBuffer->Write(frame->m_buffer, frame->m_bufferSize);
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
            frameheader.packetlength = frame->m_bufferSize;
            WriteFrameheader(&frameheader);
            m_ringBuffer->Write(frame->m_buffer, frame->m_bufferSize); // we write buf directly
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
        m_ringBuffer->Write(m_out.data(), out_len);
    }

    if (m_framesWritten == 0)
        SendMythSystemRecEvent("REC_STARTED_WRITING", m_curRecording);

    m_frameOfGop++;
    m_framesWritten++;

    // now we reset the last frame number so that we can find out
    // how many frames we didn't get next time
    m_lf = fnum;
}

void NuppelVideoRecorder::WriteAudio(unsigned char *buf, int fnum, std::chrono::milliseconds timecode)
{
    struct rtframeheader frameheader {};

    if (m_lastBlock == 0)
    {
        m_firstTc = -1ms;
    }

    if (m_lastBlock != 0)
    {
        if (fnum != (m_lastBlock+1))
        {
            m_audioBehind = fnum - (m_lastBlock+1);
            LOG(VB_RECORD, LOG_INFO, LOC + QString("audio behind %1 %2").
                    arg(m_lastBlock).arg(fnum));
        }
    }

    frameheader.frametype = 'A'; // audio frame
    frameheader.timecode = timecode.count();

    if (m_firstTc == -1ms)
    {
        m_firstTc = timecode;
#if 0
        LOG(VB_GENERAL, LOG_DEBUG, LOC +
            QString("first timecode=%1").arg(m_firstTc.count()));
#endif
    }
    else
    {
        timecode -= m_firstTc; // this is to avoid the lack between the beginning
                             // of recording and the first timestamp, maybe we
                             // can calculate the audio-video +-lack at the
                            // beginning too
        auto abytes = (double)m_audioBytes; // - (double)m_audioBufferSize;
                                     // wrong guess ;-)
        // need seconds instead of msec's
        auto mt = (double)timecode.count();
        if (mt > 0.0)
        {
            double eff = (abytes / mt) * (100000.0 / m_audioBytesPerSample);
            m_effectiveDsp = (int)eff;
        }
    }

    if (m_compressAudio)
    {
        std::array<uint8_t,7200> mp3gapless {};
        int compressedsize = 0;
        int gaplesssize = 0;
        int lameret = 0;

        int sample_cnt = m_audioBufferSize / m_audioBytesPerSample;

#if (Q_BYTE_ORDER == Q_BIG_ENDIAN)
        auto buf16 = reinterpret_cast<uint16_t *>(buf);
        for (int i = 0; i < m_audioChannels * sample_cnt; i++)
            buf16[i] = qToLittleEndian<quint16>(buf16[i]);
#endif
        if (m_audioChannels == 2)
        {
            lameret = lame_encode_buffer_interleaved(
                m_gf, (short int*) buf, sample_cnt,
                (unsigned char*) m_mp3Buf, m_mp3BufSize);
        }
        else
        {
            lameret = lame_encode_buffer(
                m_gf, (short int*) buf, (short int*) buf, sample_cnt,
                (unsigned char*) m_mp3Buf, m_mp3BufSize);
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

        lameret = lame_encode_flush_nogap(m_gf, mp3gapless.data(), mp3gapless.size());
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
            m_ringBuffer->Write(m_mp3Buf, compressedsize);
            m_ringBuffer->Write(mp3gapless.data(), gaplesssize);
        }
        m_audioBytes += m_audioBufferSize;
    }
    else
    {
        frameheader.comptype = '0'; // uncompressed audio
        frameheader.packetlength = m_audioBufferSize;

        WriteFrameheader(&frameheader);
        m_ringBuffer->Write(buf, m_audioBufferSize);
        m_audioBytes += m_audioBufferSize; // only audio no header!!
    }

    // this will probably never happen and if there would be a
    // 'uncountable' video frame drop -> material==worthless
    if (m_audioBehind > 0)
    {
        LOG(VB_RECORD, LOG_INFO, LOC + "audio behind");
        frameheader.frametype = 'A'; // audio frame
        frameheader.comptype  = 'N'; // output a nullframe with
        frameheader.packetlength = 0;
        WriteFrameheader(&frameheader);
        m_audioBytes += m_audioBufferSize;
        m_audioBehind--;
    }

    m_lastBlock = fnum;
}

void NuppelVideoRecorder::WriteText(unsigned char *buf, int len, std::chrono::milliseconds timecode,
                                    int pagenr)
{
    struct rtframeheader frameheader {};

    frameheader.frametype = 'T'; // text frame
    frameheader.timecode = timecode.count();

    if (VBIMode::PAL_TT == m_vbiMode)
    {
        frameheader.comptype = 'T'; // european teletext
        frameheader.packetlength = len + 4;
        WriteFrameheader(&frameheader);
        union page_t {
            int32_t m_val32;
            struct { int8_t m_a,m_b,m_c,m_d; } m_val8;
        } v {};
        v.m_val32 = pagenr;
        m_ringBuffer->Write(&v.m_val8.m_d, sizeof(int8_t));
        m_ringBuffer->Write(&v.m_val8.m_c, sizeof(int8_t));
        m_ringBuffer->Write(&v.m_val8.m_b, sizeof(int8_t));
        m_ringBuffer->Write(&v.m_val8.m_a, sizeof(int8_t));
        m_ringBuffer->Write(buf, len);
    }
    else if (VBIMode::NTSC_CC == m_vbiMode)
    {
        frameheader.comptype = 'C'; // NTSC CC
        frameheader.packetlength = len;

        WriteFrameheader(&frameheader);
        m_ringBuffer->Write(buf, len);
    }
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
