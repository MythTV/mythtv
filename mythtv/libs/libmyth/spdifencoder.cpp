#include "config.h"

#include "compat.h"
#include "spdifencoder.h"
#include "mythverbose.h"

#define LOC QString("SPDIFEncoder: ")
#define LOC_ERR QString("SPDIFEncoder, Error: ")

/*
 * SPDIFEncoder constructor
 * Args:
 *   QString muxer       : name of the muxer.
 *                         Use "spdif" for IEC 958 or IEC 61937 encapsulation
 *                         (AC3, DTS, E-AC3, TrueHD, DTS-HD-MA)
 *                         Use "adts" for ADTS encpsulation (AAC)
 *   AVCodecContext *ctx : CodecContext to be encaspulated
 */
    
SPDIFEncoder::SPDIFEncoder(QString muxer, AVCodecContext *ctx)
    : m_complete(false), m_oc(NULL), m_stream(NULL), m_size(0)
{
    QByteArray dev_ba     = muxer.toAscii();
    AVOutputFormat *fmt =
        av_guess_format(dev_ba.constData(), NULL, NULL);

    if (!fmt)
    {
        VERBOSE(VB_AUDIO, LOC_ERR + "av_guess_format");
        return;
    }

    m_oc = avformat_alloc_context();
    if (!m_oc)
    {
        VERBOSE(VB_AUDIO, LOC_ERR + "avformat_alloc_context");
        return;
    }
    m_oc->oformat = fmt;

    if (av_set_parameters(m_oc, NULL) < 0)
    {
        VERBOSE(VB_AUDIO, LOC_ERR + "av_set_parameters");
        Destroy();
        return;
    }

    m_oc->pb = av_alloc_put_byte(m_buffer, sizeof(m_buffer), URL_RDONLY,
                                 this, NULL, funcIO, NULL);
    if (!m_oc->pb)
    {
        VERBOSE(VB_AUDIO, LOC_ERR + "av_alloc_put_byte");
        Destroy();
        return;
    }

    m_oc->pb->is_streamed    = true;
    m_oc->flags             |= AVFMT_NOFILE | AVFMT_FLAG_IGNIDX;

    if (av_set_parameters(m_oc, NULL) != 0)
    {
        VERBOSE(VB_AUDIO, LOC_ERR + "av_set_parameters");
        Destroy();
        return;
    }

    m_stream = av_new_stream(m_oc, 1);
    if (!m_stream)
    {
        VERBOSE(VB_AUDIO, LOC_ERR + "av_new_stream");
        Destroy();
        return;
    }

        // copy without decoding or reencoding
    m_stream->stream_copy           = true;

    AVCodecContext *codec = m_stream->codec;

    codec->codec_type     = ctx->codec_type;
    codec->codec_id       = ctx->codec_id;
    codec->sample_rate    = ctx->sample_rate;
    codec->sample_fmt     = ctx->sample_fmt;
    codec->channels       = ctx->channels;
    codec->bit_rate       = ctx->bit_rate;
    codec->extradata      = new uint8_t[ctx->extradata_size];
    codec->extradata_size = ctx->extradata_size;
    memcpy(codec->extradata, ctx->extradata, ctx->extradata_size);

    av_write_header(m_oc);

    VERBOSE(VB_AUDIO, LOC + QString("Creating %1 encoder (%2, %3Hz)")
            .arg(muxer).arg(ff_codec_id_string((CodecID)codec->codec_type))
            .arg(codec->sample_rate));

    m_complete = true;
}

SPDIFEncoder::~SPDIFEncoder(void)
{
    Destroy();
}

/*
 * Encode data through created muxer
 * unsigned char data: pointer to data to encode
 * int           size: size of data to encode
 */

void SPDIFEncoder::WriteFrame(unsigned char *data, int size)
{
    AVPacket packet;

    av_init_packet(&packet);
    packet.data = data;
    packet.size = size;

    if (av_write_frame(m_oc, &packet) < 0)
    {
        VERBOSE(VB_AUDIO, LOC_ERR + "av_write_frame");
    }
}

/*
 * Retrieve encoded data and copy it in the provided buffer.
 * Return -1 if there is no data to retrieve.
 * On return, dest_size will contain the length of the data copied
 * Upon completion, the internal encoder buffer is emptied.
 */
int SPDIFEncoder::GetData(unsigned char *buffer, int &dest_size)
{
    if(m_size > 0)
    {
        memcpy(buffer, m_buffer, m_size);
        dest_size = m_size;
        m_size = 0;
        return dest_size;
    }
    return -1;
}

/*
 * Reset the internal encoder buffer
 */
void SPDIFEncoder::Reset()
{
    m_size = 0;
}

/*
 * funcIO: Internal callback function that will receive encoded frames
 */

int SPDIFEncoder::funcIO(void *opaque, unsigned char *buf, int size)
{
    SPDIFEncoder *enc = (SPDIFEncoder *)opaque;

    memcpy(enc->m_buffer + enc->m_size, buf, size);
    enc->m_size += size;
    return size;
}

/*
 * Destroy and free all allocated memory
 */

void SPDIFEncoder::Destroy()
{
    Reset();

    if (m_complete)
    {
        av_write_trailer(m_oc);
    }

    if (m_stream)
    {
        delete[] m_stream->codec->extradata;
        avcodec_close(m_stream->codec);
        av_freep(&m_stream);
    }

    if (m_oc )
    {
        if (m_oc->pb)
        {
            av_freep(&m_oc->pb);
        }
        av_free(m_oc);
    }
}
