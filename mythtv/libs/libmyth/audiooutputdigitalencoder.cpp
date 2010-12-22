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
    bytes_per_sample(0),
    av_context(NULL),
    outlen(0),
    inlen(0),
    one_frame_bytes(0),
    m_spdifenc(NULL)
{
    in = (unsigned char *)(((long)&inbuf + 15) & ~15);
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
        av_free(av_context);
        av_context = NULL;
    }
    if (m_spdifenc)
    {
        delete m_spdifenc;
        m_spdifenc = NULL;
    }
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
    codec = avcodec_find_encoder(CODEC_ID_AC3);
    if (!codec)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Could not find codec");
        return false;
    }

    av_context              = avcodec_alloc_context();
    av_context->bit_rate    = bitrate;
    av_context->sample_rate = samplerate;
    av_context->channels    = channels;

    // open it */
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

    m_spdifenc = new SPDIFEncoder("spdif", av_context);
    if (!m_spdifenc->Succeeded())
    {
        Dispose();
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Could not create spdif muxer");
        return false;
    }

    bytes_per_sample = av_context->channels * sizeof(short);
    one_frame_bytes  = bytes_per_sample * av_context->frame_size;

    VERBOSE(VB_AUDIO, QString("DigitalEncoder::Init fs=%1, bpf=%2 ofb=%3")
            .arg(av_context->frame_size)
            .arg(bytes_per_sample)
            .arg(one_frame_bytes));

    return true;
}

size_t AudioOutputDigitalEncoder::Encode(void *buf, int len, bool isFloat)
{
    size_t outsize = 0;
    int data_size;

    if (isFloat)
        inlen += AudioOutputUtil::fromFloat(FORMAT_S16, in + inlen, buf, len);
    else
    {
        memcpy(in + inlen, buf, len);
        inlen += len;
    }

    int frames = inlen / one_frame_bytes;
    int i = 0;

    while (i < frames)
    {
        outsize = avcodec_encode_audio(av_context,
                                       m_encodebuffer,
                                       FF_MIN_BUFFER_SIZE,
                                       (short *)(in + i * one_frame_bytes));
        if (outsize < 0)
        {
            VERBOSE(VB_AUDIO, LOC_ERR + "AC-3 encode error");
            return outlen;
        }

        if (!m_spdifenc)
        {
            m_spdifenc = new SPDIFEncoder("spdif", av_context);
        }
        m_spdifenc->WriteFrame(m_encodebuffer, outsize);
        m_spdifenc->GetData(out + outlen, data_size);
        outlen += data_size;
        inlen -= one_frame_bytes;
        i++;
    }

    memmove(in, in + i * one_frame_bytes, inlen);
    return outlen;
}

void AudioOutputDigitalEncoder::GetFrames(void *ptr, int maxlen)
{
    int len = std::min(maxlen, outlen);
    memcpy(ptr, out, len);
    outlen -= len;
    memmove(out, out + len, outlen);
}
