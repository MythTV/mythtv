// Std C headers
#include <cstdio>
#include <unistd.h>
#include <string.h>

#include "config.h"

// libav headers
extern "C" {
#include "libavutil/mem.h" // for av_free
#include "libavcodec/avcodec.h"
}

// MythTV headers
#include "audiooutputdigitalencoder.h"
#include "audiooutpututil.h"
#include "compat.h"
#include "mythverbose.h"

#define LOC QString("DEnc: ")
#define LOC_ERR QString("DEnc, Error: ")

AudioOutputDigitalEncoder::AudioOutputDigitalEncoder(void) :
    av_context(NULL),
    out(NULL), out_size(0),
    in(NULL), in_size(0),
    outlen(0), inlen(0),
    samples_per_frame(0),
    m_spdifenc(NULL)
{
    out = (outbuf_t *)av_malloc(OUTBUFSIZE);
    if (out)
    {
        out_size = OUTBUFSIZE;
    }
    in = (inbuf_t *)av_malloc(INBUFSIZE);
    if (in)
    {
        in_size = INBUFSIZE;
    }
}

AudioOutputDigitalEncoder::~AudioOutputDigitalEncoder()
{
    Dispose();
}

void AudioOutputDigitalEncoder::Dispose()
{
    if (av_context)
    {
        avcodec_close(av_context);
        av_freep(&av_context);
    }
    if (out)
    {
        av_freep(&out);
        out_size = 0;
    }
    if (in)
    {
        av_freep(&in);
        in_size = 0;
    }
    if (m_spdifenc)
    {
        delete m_spdifenc;
        m_spdifenc = NULL;
    }
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

bool AudioOutputDigitalEncoder::Init(
    CodecID codec_id, int bitrate, int samplerate, int channels)
{
    AVCodec *codec;
    int ret;

    VERBOSE(VB_AUDIO, LOC + QString("Init codecid=%1, br=%2, sr=%3, ch=%4")
            .arg(ff_codec_id_string(codec_id))
            .arg(bitrate)
            .arg(samplerate)
            .arg(channels));

    // We need to do this when called from mythmusic
    avcodec_init();
    avcodec_register_all();
#if LIBAVCODEC_VERSION_INT > AV_VERSION_INT( 52, 113, 0 )
    codec = avcodec_find_encoder_by_name("ac3_fixed");
#else
    codec = avcodec_find_encoder(CODEC_ID_AC3);
#endif
    if (!codec)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Could not find codec");
        return false;
    }

    av_context                 = avcodec_alloc_context();
    avcodec_get_context_defaults3(av_context, codec);

    av_context->bit_rate       = bitrate;
    av_context->sample_rate    = samplerate;
    av_context->channels       = channels;
#if LIBAVCODEC_VERSION_INT > AV_VERSION_INT( 52, 113, 0 )
    av_context->channel_layout = AV_CH_LAYOUT_5POINT1;
    av_context->sample_fmt     = AV_SAMPLE_FMT_S16;
#else
    av_context->channel_layout = CH_LAYOUT_5POINT1;
    av_context->sample_fmt     = SAMPLE_FMT_S16;
#endif

// open it
    ret = avcodec_open(av_context, codec);
    if (ret < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Could not open codec, invalid bitrate or samplerate");

        Dispose();
        return false;
    }

    if (m_spdifenc)
    {
        delete m_spdifenc;
    }

    m_spdifenc = new SPDIFEncoder("spdif", CODEC_ID_AC3);
    if (!m_spdifenc->Succeeded())
    {
        Dispose();
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Could not create spdif muxer");
        return false;
    }

    samples_per_frame  = av_context->frame_size * av_context->channels;

    VERBOSE(VB_AUDIO, QString("DigitalEncoder::Init fs=%1, spf=%2")
            .arg(av_context->frame_size)
            .arg(samples_per_frame));

    return true;
}

size_t AudioOutputDigitalEncoder::Encode(void *buf, int len, AudioFormat format)
{
    size_t outsize = 0;
    int data_size;

    // Check if there is enough space in incoming buffer
    int required_len = inlen + len / AudioOutputSettings::SampleSize(format) *
        AudioOutputSettings::SampleSize(FORMAT_S16);
    if (required_len > (int)in_size)
    {
        required_len = ((required_len / INBUFSIZE) + 1) * INBUFSIZE;
        VERBOSE(VB_AUDIO, LOC +
                QString("low mem, reallocating in buffer from %1 to %2")
                .arg(in_size)
                .arg(required_len));
        if (!(in = (inbuf_t *)realloc(in, in_size, required_len)))
        {
            in_size = 0;
            VERBOSE(VB_AUDIO, LOC_ERR +
                    "AC-3 encode error, insufficient memory");
            return outlen;
        }
        in_size = required_len;
    }
    if (format != FORMAT_S16)
    {
        inlen += AudioOutputUtil::fromFloat(FORMAT_S16, (char *)in + inlen,
                                            buf, len);
    }
    else
    {
        memcpy((char *)in + inlen, buf, len);
        inlen += len;
    }

    int frames = inlen / sizeof(inbuf_t) / samples_per_frame;
    int i = 0;

    while (i < frames)
    {
        outsize = avcodec_encode_audio(av_context,
                                       (uint8_t *)m_encodebuffer,
                                       sizeof(m_encodebuffer),
                                       (short *)(in + i * samples_per_frame));
        if (outsize < 0)
        {
            VERBOSE(VB_AUDIO, LOC_ERR + "AC-3 encode error");
            return outlen;
        }

        if (!m_spdifenc)
        {
            m_spdifenc = new SPDIFEncoder("spdif", CODEC_ID_AC3);
        }
        m_spdifenc->WriteFrame((uint8_t *)m_encodebuffer, outsize);
        // Check if output buffer is big enough
        required_len = outlen + m_spdifenc->GetProcessedSize();
        if (required_len > (int)out_size)
        {
            required_len = ((required_len / OUTBUFSIZE) + 1) * OUTBUFSIZE;
            VERBOSE(VB_AUDIO, LOC +
                    QString("low mem, reallocating out buffer from %1 to %2")
                    .arg(out_size)
                    .arg(required_len));
            if (!(out = (outbuf_t *)realloc(out, out_size, required_len)))
            {
                out_size = 0;
                VERBOSE(VB_AUDIO, LOC_ERR +
                        "AC-3 encode error, insufficient memory");
                return outlen;
            }
            out_size = required_len;
        }
        m_spdifenc->GetData((uint8_t *)out + outlen, data_size);
        outlen += data_size;
        inlen  -= samples_per_frame * sizeof(inbuf_t);
        i++;
    }

    memmove(in, in + i * samples_per_frame, inlen);
    return outlen;
}

size_t AudioOutputDigitalEncoder::GetFrames(void *ptr, int maxlen)
{
    int len = std::min(maxlen, outlen);
    if (len != maxlen)
    {
        VERBOSE(VB_AUDIO, LOC + QString("GetFrames: getting less than requested"));
    }
    memcpy(ptr, out, len);
    outlen -= len;
    memmove(out, (char *)out + len, outlen);
    return len;
}
