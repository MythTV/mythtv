// Std C headers
#include <cstdio>
#include <cstring>
#include <unistd.h>

// libav headers
extern "C" {
#include "libavutil/mem.h" // for av_free
#include "libavcodec/avcodec.h"
}

// MythTV headers
#include "libmythbase/compat.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"

#include "audiooutputdigitalencoder.h"
#include "audiooutpututil.h"
#include "mythaverror.h"

#define LOC QString("DEnc: ")

AudioOutputDigitalEncoder::AudioOutputDigitalEncoder(void)
  : m_outbuf(static_cast<uint8_t*>(av_mallocz(OUTBUFSIZE)))
{
    if (m_outbuf)
    {
        m_outSize = OUTBUFSIZE;
    }
    m_inbuf = static_cast<uint8_t*>(av_mallocz(INBUFSIZE));
    if (m_inbuf)
    {
        m_inSize = INBUFSIZE;
    }
    m_framebuf = static_cast<uint8_t*>(av_mallocz(INBUFSIZE));
}

AudioOutputDigitalEncoder::~AudioOutputDigitalEncoder()
{
    Reset();
    if (m_outbuf)
    {
        av_freep(reinterpret_cast<void*>(&m_outbuf));
        m_outSize = 0;
    }
    if (m_inbuf)
    {
        av_freep(reinterpret_cast<void*>(&m_inbuf));
        m_inSize = 0;
    }
    if (m_framebuf)
    {
        av_freep(reinterpret_cast<void*>(&m_framebuf));
    }
}

void AudioOutputDigitalEncoder::Reset(void)
{
    if (m_avContext)
        avcodec_free_context(&m_avContext);

    av_frame_free(&m_frame);

    delete m_spdifEnc;
    m_spdifEnc = nullptr;

    clear();
}

void *AudioOutputDigitalEncoder::realloc(void *ptr,
                                         size_t old_size, size_t new_size)
{
    if (!ptr)
        return ptr;

    // av_realloc doesn't maintain 16 bytes alignment
    void *new_ptr = av_malloc(new_size);
    if (!new_ptr)
    {
        av_free(ptr);
        return new_ptr;
    }
    memcpy(new_ptr, ptr, old_size);
    av_free(ptr);
    return new_ptr;
}

// Encode can use either ac3 (floating point) or ac3_fixed (fixed point)
// To use ac3_fixed define AC3_FIXED 1

#define AC3_FIXED 0 // NOLINT(cppcoreguidelines-macro-usage)
#if AC3_FIXED
static constexpr const char* CODECNAME { "ac3_fixed" };
static constexpr AVSampleFormat FFMPEG_SAMPLE_FORMAT { AV_SAMPLE_FMT_S32P };
static constexpr AudioFormat MYTH_SAMPLE_FORMAT { FORMAT_S32 };
#else
static constexpr const char* CODECNAME { "ac3" };
static constexpr AVSampleFormat FFMPEG_SAMPLE_FORMAT { AV_SAMPLE_FMT_FLTP };
static constexpr AudioFormat MYTH_SAMPLE_FORMAT { FORMAT_FLT };
#define MYTH_USE_FLOAT 1 // NOLINT(cppcoreguidelines-macro-usage)
#endif

bool AudioOutputDigitalEncoder::Init(
    AVCodecID codec_id, int bitrate, int samplerate, int channels)
{
    LOG(VB_AUDIO, LOG_INFO, LOC +
        QString("Init codecid=%1, br=%2, sr=%3, ch=%4")
            .arg(avcodec_get_name(codec_id)) .arg(bitrate)
            .arg(samplerate) .arg(channels));

    if (!(m_inbuf || m_framebuf || m_outbuf))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Memory allocation failed");
        return false;
    }

    // Clear digital encoder from all existing content
    Reset();

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Using codec %1 to encode audio").arg(CODECNAME));
    const AVCodec *codec = avcodec_find_encoder_by_name(CODECNAME);
    if (!codec)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Could not find codec");
        return false;
    }

    m_avContext                 = avcodec_alloc_context3(codec);

    m_avContext->bit_rate       = bitrate;
    m_avContext->sample_rate    = samplerate;
    av_channel_layout_default(&(m_avContext->ch_layout), channels);
    m_avContext->sample_fmt     = FFMPEG_SAMPLE_FORMAT;

    // open it
    int ret = avcodec_open2(m_avContext, codec, nullptr);
    if (ret < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Could not open codec, invalid bitrate or samplerate");

        return false;
    }

    m_spdifEnc = new SPDIFEncoder("spdif", AV_CODEC_ID_AC3);
    if (!m_spdifEnc->Succeeded())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Could not create spdif muxer");
        return false;
    }

    m_samplesPerFrame  = m_avContext->frame_size * m_avContext->ch_layout.nb_channels;

    LOG(VB_AUDIO, LOG_INFO, QString("DigitalEncoder::Init fs=%1, spf=%2")
            .arg(m_avContext->frame_size) .arg(m_samplesPerFrame));

    return true;
}

// input = 6 channel data from upconvert or speedup
// m_inbuf = 6 channel data converted to S32 or FLT samples interleaved
// m_framebuf = 1 frame, deinterleaved into planar format
// m_inlen = number of bytes available in m_inbuf
// format = incoming sample format, normally FORMAT_FLT
// upconvert and speedup both use floating point for parameter format

int AudioOutputDigitalEncoder::Encode(void *input, int len, AudioFormat format)
{
    int sampleSize = AudioOutputSettings::SampleSize(format);
    if (sampleSize <= 0)
    {
        LOG(VB_AUDIO, LOG_ERR, LOC + "AC-3 encode error, sample size is zero");
        return 0;
    }

    // Check if there is enough space in incoming buffer
    ssize_t required_len = m_inlen +
        (len * AudioOutputSettings::SampleSize(MYTH_SAMPLE_FORMAT) / sampleSize);

    if (required_len > m_inSize)
    {
        required_len = ((required_len / INBUFSIZE) + 1) * INBUFSIZE;
        LOG(VB_AUDIO, LOG_INFO, LOC +
            QString("low mem, reallocating in buffer from %1 to %2")
                .arg(m_inSize) .arg(required_len));
        auto *tmp = static_cast<uint8_t*> (realloc(m_inbuf, m_inSize, required_len));
        if (!tmp)
        {
            m_inbuf = nullptr;
            m_inSize = 0;
            LOG(VB_AUDIO, LOG_ERR, LOC +
                "AC-3 encode error, insufficient memory");
            return m_outlen;
        }
        m_inbuf = tmp;
        m_inSize = required_len;
    }

    if (format == MYTH_SAMPLE_FORMAT)
    {
        // The input format is the same as the ffmpeg desired format so just copy the data
        memcpy(m_inbuf + m_inlen, input, len);
        m_inlen += len;
    }
#if ! MYTH_USE_FLOAT
    else if (format == FORMAT_FLT)
    {
        // The input format is float but ffmpeg wants something else so convert it
        m_inlen += AudioOutputUtil::fromFloat(MYTH_SAMPLE_FORMAT, m_inbuf + m_inlen,
                                            input, len);
    }
#endif
    else
    {
        LOG(VB_AUDIO, LOG_ERR, LOC +
            QString("AC-3 encode error, cannot handle input format %1")
            .arg(format));
        return 0;
    }

    int frames           = m_inlen / AudioOutputSettings::SampleSize(MYTH_SAMPLE_FORMAT) / m_samplesPerFrame;
    int i                = 0;
    int channels         = m_avContext->ch_layout.nb_channels;
    int size_channel     = m_avContext->frame_size *
        AudioOutputSettings::SampleSize(MYTH_SAMPLE_FORMAT);
    if (!m_frame)
    {
        m_frame = av_frame_alloc();
        if (m_frame == nullptr)
        {
            m_inbuf = nullptr;
            m_inSize = 0;
            LOG(VB_AUDIO, LOG_ERR, LOC +
                "AC-3 encode error, insufficient memory");
            return m_outlen;
        }
    }
    else
    {
        av_frame_unref(m_frame);
    }
    m_frame->nb_samples = m_avContext->frame_size;
    m_frame->pts        = AV_NOPTS_VALUE;
    m_frame->format         = m_avContext->sample_fmt;
    av_channel_layout_copy(&(m_frame->ch_layout), &(m_avContext->ch_layout));
    m_frame->sample_rate = m_avContext->sample_rate;

    if (frames > 0)
    {
        // init AVFrame for planar data (input is interleaved)
        for (int j = 0, jj = 0; j < channels; j++, jj += size_channel)
        {
            m_frame->data[j] = m_framebuf + jj;
        }
    }

    while (i < frames)
    {
        AVPacket *pkt = av_packet_alloc();
        if (pkt == nullptr)
        {
            LOG(VB_RECORD, LOG_ERR, "packet allocation failed");
            return AVERROR(ENOMEM);
        }
        bool got_packet   = false;

        AudioOutputUtil::DeinterleaveSamples(
            MYTH_SAMPLE_FORMAT, channels,
            m_framebuf,
            m_inbuf + (static_cast<ptrdiff_t>(i) * size_channel * channels),
            size_channel * channels);

        //  SUGGESTION
        //  Now that avcodec_encode_audio2 is deprecated and replaced
        //  by 2 calls, this could be optimized
        //  into separate routines or separate threads.
        int ret = avcodec_receive_packet(m_avContext, pkt);
        if (ret == 0)
            got_packet = true;
        if (ret == AVERROR(EAGAIN))
            ret = 0;
        if (ret == 0)
            ret = avcodec_send_frame(m_avContext, m_frame);
        // if ret from avcodec_send_frame is AVERROR(EAGAIN) then
        // there are 2 packets to be received while only 1 frame to be
        // sent. The code does not cater for this. Hopefully it will not happen.

        if (ret < 0)
        {
            std::string error;
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("audio encode error: %1 (%2)")
                .arg(av_make_error_stdstring(error, ret))
                .arg(got_packet));
            av_packet_free(&pkt);
            return ret;
        }
        i++;
        if (!got_packet)
        {
            m_inlen  -= m_samplesPerFrame * AudioOutputSettings::SampleSize(MYTH_SAMPLE_FORMAT);
            continue;
        }
        if (!m_spdifEnc)
        {
            m_spdifEnc = new SPDIFEncoder("spdif", AV_CODEC_ID_AC3);
        }

        m_spdifEnc->WriteFrame(pkt->data, pkt->size);
        av_packet_unref(pkt);
        av_packet_free(&pkt);

        // Check if output buffer is big enough
        required_len = m_outlen + m_spdifEnc->GetProcessedSize();
        if (required_len > m_outSize)
        {
            required_len = ((required_len / OUTBUFSIZE) + 1) * OUTBUFSIZE;
            LOG(VB_AUDIO, LOG_WARNING, LOC +
                QString("low mem, reallocating out buffer from %1 to %2")
                    .arg(m_outSize) .arg(required_len));
            auto *tmp = static_cast<uint8_t*>(realloc(m_outbuf, m_outSize, required_len));
            if (!tmp)
            {
                m_outbuf = nullptr;
                m_outSize = 0;
                LOG(VB_AUDIO, LOG_ERR, LOC +
                    "AC-3 encode error, insufficient memory");
                return m_outlen;
            }
            m_outbuf = tmp;
            m_outSize = required_len;
        }
        size_t data_size = 0;
        m_spdifEnc->GetData(m_outbuf + m_outlen, data_size);
        m_outlen += data_size;
        m_inlen  -= m_samplesPerFrame * AudioOutputSettings::SampleSize(MYTH_SAMPLE_FORMAT);
    }

    memmove(m_inbuf, m_inbuf + (static_cast<ptrdiff_t>(i) * m_samplesPerFrame * AudioOutputSettings::SampleSize(MYTH_SAMPLE_FORMAT)), m_inlen);
    return m_outlen;
}

int AudioOutputDigitalEncoder::GetFrames(void *ptr, int maxlen)
{
    int len = std::min(maxlen, m_outlen);
    if (len != maxlen)
    {
        LOG(VB_AUDIO, LOG_INFO, LOC + "GetFrames: getting less than requested");
    }
    memcpy(ptr, m_outbuf, len);
    m_outlen -= len;
    memmove(m_outbuf, m_outbuf + len, m_outlen);
    return len;
}

void AudioOutputDigitalEncoder::clear()
{
    m_inlen = m_outlen = 0;
}
