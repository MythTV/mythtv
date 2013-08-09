// Std C headers
#include <cstdio>
#include <unistd.h>
#include <string.h>

#include "mythcorecontext.h"
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
#include "mythlogging.h"

#define LOC QString("DEnc: ")

AudioOutputDigitalEncoder::AudioOutputDigitalEncoder(void) :
    av_context(NULL),
    out(NULL), out_size(0),
    in(NULL), inp(NULL), in_size(0),
    outlen(0), inlen(0),
    samples_per_frame(0),
    m_spdifenc(NULL)
{
    out = (outbuf_t *)av_mallocz(OUTBUFSIZE);
    if (out)
    {
        out_size = OUTBUFSIZE;
    }
    in = (inbuf_t *)av_mallocz(INBUFSIZE);
    if (in)
    {
        in_size = INBUFSIZE;
    }
    inp = (inbuf_t *)av_mallocz(INBUFSIZE);
}

AudioOutputDigitalEncoder::~AudioOutputDigitalEncoder()
{
    Reset();
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
    if (inp)
    {
        av_freep(&inp);
    }
}

void AudioOutputDigitalEncoder::Reset(void)
{
    if (av_context)
    {
        avcodec_close(av_context);
        av_freep(&av_context);
    }
    delete m_spdifenc;
    m_spdifenc = NULL;

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

bool AudioOutputDigitalEncoder::Init(
    CodecID codec_id, int bitrate, int samplerate, int channels)
{
    AVCodec *codec;
    int ret;

    LOG(VB_AUDIO, LOG_INFO, LOC +
        QString("Init codecid=%1, br=%2, sr=%3, ch=%4")
            .arg(ff_codec_id_string(codec_id)) .arg(bitrate)
            .arg(samplerate) .arg(channels));

    if (!(in || inp || out))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Memory allocation failed");
        return false;
    }

    // Clear digital encoder from all existing content
    Reset();

    // We need to do this when called from mythmusic
    avcodeclock->lock();
    avcodec_register_all();
    avcodeclock->unlock();
    codec = avcodec_find_encoder_by_name("ac3_fixed");
    if (!codec)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Could not find codec");
        return false;
    }

    av_context                 = avcodec_alloc_context3(codec);
    avcodec_get_context_defaults3(av_context, codec);

    av_context->bit_rate       = bitrate;
    av_context->sample_rate    = samplerate;
    av_context->channels       = channels;
    av_context->channel_layout = av_get_default_channel_layout(channels);
    av_context->sample_fmt     = AV_SAMPLE_FMT_S16P;

    // open it
    ret = avcodec_open2(av_context, codec, NULL);
    if (ret < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Could not open codec, invalid bitrate or samplerate");

        return false;
    }

    m_spdifenc = new SPDIFEncoder("spdif", AV_CODEC_ID_AC3);
    if (!m_spdifenc->Succeeded())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Could not create spdif muxer");
        return false;
    }

    samples_per_frame  = av_context->frame_size * av_context->channels;

    LOG(VB_AUDIO, LOG_INFO, QString("DigitalEncoder::Init fs=%1, spf=%2")
            .arg(av_context->frame_size) .arg(samples_per_frame));

    return true;
}

size_t AudioOutputDigitalEncoder::Encode(void *buf, int len, AudioFormat format)
{
    int sampleSize = AudioOutputSettings::SampleSize(format);
    if (sampleSize <= 0)
    {
        LOG(VB_AUDIO, LOG_ERR, LOC + "AC-3 encode error, sample size is zero");
        return 0;
    }

    // Check if there is enough space in incoming buffer
    int required_len = inlen +
        len * AudioOutputSettings::SampleSize(FORMAT_S16) / sampleSize;

    if (required_len > (int)in_size)
    {
        required_len = ((required_len / INBUFSIZE) + 1) * INBUFSIZE;
        LOG(VB_AUDIO, LOG_INFO, LOC +
            QString("low mem, reallocating in buffer from %1 to %2")
                .arg(in_size) .arg(required_len));
        inbuf_t *tmp = reinterpret_cast<inbuf_t*>
            (realloc(in, in_size, required_len));
        if (!tmp)
        {
            in = NULL;
            in_size = 0;
            LOG(VB_AUDIO, LOG_ERR, LOC +
                "AC-3 encode error, insufficient memory");
            return outlen;
        }
        in = tmp;
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

    int frames           = inlen / sizeof(inbuf_t) / samples_per_frame;
    int i                = 0;
    int channels         = av_context->channels;
    AVFrame *frame       = avcodec_alloc_frame();
    int size_channel     = av_context->frame_size *
        AudioOutputSettings::SampleSize(FORMAT_S16);
    frame->extended_data = frame->data;
    frame->nb_samples    = av_context->frame_size;
    frame->pts           = AV_NOPTS_VALUE;

    if (frames > 0)
    {
            // init AVFrame for planar data (input is interleaved)
        for (int j = 0, jj = 0; j < channels; j++, jj += av_context->frame_size)
        {
            frame->data[j] = (uint8_t*)(inp + jj);
        }
    }

    while (i < frames)
    {
        AVPacket pkt;
        av_init_packet(&pkt);
        pkt.data          = NULL;
        pkt.size          = 0;
        int got_packet    = 0;

        AudioOutputUtil::DeinterleaveSamples(
            FORMAT_S16, channels,
            (uint8_t*)inp,
            (uint8_t*)(in + i * samples_per_frame),
            size_channel * channels);

        int ret           = avcodec_encode_audio2(av_context, &pkt, frame,
                                                  &got_packet);

        if (ret < 0)
        {
            LOG(VB_AUDIO, LOG_ERR, LOC + "AC-3 encode error");
            avcodec_free_frame(&frame);
            return ret;
        }
        i++;
        if (!got_packet)
            continue;

        if (!m_spdifenc)
        {
            m_spdifenc = new SPDIFEncoder("spdif", AV_CODEC_ID_AC3);
        }

        m_spdifenc->WriteFrame((uint8_t *)pkt.data, pkt.size);
        av_free_packet(&pkt);

        // Check if output buffer is big enough
        required_len = outlen + m_spdifenc->GetProcessedSize();
        if (required_len > (int)out_size)
        {
            required_len = ((required_len / OUTBUFSIZE) + 1) * OUTBUFSIZE;
            LOG(VB_AUDIO, LOG_WARNING, LOC +
                QString("low mem, reallocating out buffer from %1 to %2")
                    .arg(out_size) .arg(required_len));
            outbuf_t *tmp = reinterpret_cast<outbuf_t*>
                (realloc(out, out_size, required_len));
            if (!tmp)
            {
                avcodec_free_frame(&frame);
                out = NULL;
                out_size = 0;
                LOG(VB_AUDIO, LOG_ERR, LOC +
                    "AC-3 encode error, insufficient memory");
                return outlen;
            }
            out = tmp;
            out_size = required_len;
        }
        int data_size = 0;
        m_spdifenc->GetData((uint8_t *)out + outlen, data_size);
        outlen += data_size;
        inlen  -= samples_per_frame * sizeof(inbuf_t);
    }
    avcodec_free_frame(&frame);

    memmove(in, in + i * samples_per_frame, inlen);
    return outlen;
}

size_t AudioOutputDigitalEncoder::GetFrames(void *ptr, int maxlen)
{
    int len = std::min(maxlen, outlen);
    if (len != maxlen)
    {
        LOG(VB_AUDIO, LOG_INFO, LOC + "GetFrames: getting less than requested");
    }
    memcpy(ptr, out, len);
    outlen -= len;
    memmove(out, (char *)out + len, outlen);
    return len;
}

void AudioOutputDigitalEncoder::clear()
{
    inlen = outlen = 0;
}
