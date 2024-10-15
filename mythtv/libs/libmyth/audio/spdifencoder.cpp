#include "libmythbase/compat.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"

#include "spdifencoder.h"

extern "C" {
#include "libavformat/avformat.h"
#include "libavutil/opt.h"
}

#define LOC QString("SPDIFEncoder: ")

/**
 * SPDIFEncoder constructor
 * Args:
 *   QString muxer       : name of the muxer.
 *                         Use "spdif" for IEC 958 or IEC 61937 encapsulation
 *                         (AC3, DTS, E-AC3, TrueHD, DTS-HD-MA)
 *                         Use "adts" for ADTS encpsulation (AAC)
 *   AVCodecContext *ctx : CodecContext to be encaspulated
 */
SPDIFEncoder::SPDIFEncoder(const QString& muxer, AVCodecID codec_id)
{
    QByteArray dev_ba     = muxer.toLatin1();

    const AVOutputFormat *fmt = av_guess_format(dev_ba.constData(), nullptr, nullptr);
    if (!fmt)
    {
        LOG(VB_AUDIO, LOG_ERR, LOC + "av_guess_format");
        return;
    }

    m_oc = avformat_alloc_context();
    if (!m_oc)
    {
        LOG(VB_AUDIO, LOG_ERR, LOC + "avformat_alloc_context");
        return;
    }
    m_oc->oformat = fmt;

    auto *buffer = (unsigned char *)av_malloc(AudioOutput::kMaxSizeBuffer);
    m_oc->pb = avio_alloc_context(buffer, AudioOutput::kMaxSizeBuffer, 1,
                                  this, nullptr, funcIO, nullptr);
    if (!m_oc->pb)
    {
        LOG(VB_AUDIO, LOG_ERR, LOC + "avio_alloc_context");
        Destroy();
        return;
    }

    m_oc->pb->seekable    = 0;
    m_oc->flags          |= AVFMT_NOFILE | AVFMT_FLAG_IGNIDX;

    const AVCodec *codec = avcodec_find_decoder(codec_id);
    if (!codec)
    {
        LOG(VB_AUDIO, LOG_ERR, LOC + "avcodec_find_decoder");
        Destroy();
        return;
    }

    AVStream *stream = avformat_new_stream(m_oc, nullptr);
    if (!stream)
    {
        LOG(VB_AUDIO, LOG_ERR, LOC + "avformat_new_stream");
        Destroy();
        return;
    }

    stream->id                    = 1;
    stream->codecpar->codec_id    = codec->id;
    stream->codecpar->codec_type  = codec->type;
    stream->codecpar->sample_rate = 48000; // dummy rate, so codecpar initialization doesn't fail.

    if (avformat_write_header(m_oc, nullptr) < 0)
    {
        LOG(VB_AUDIO, LOG_ERR, LOC + "avformat_write_header");
        Destroy();
        return;
    }

    LOG(VB_AUDIO, LOG_INFO, LOC + QString("Creating %1 encoder (for %2)")
            .arg(muxer, avcodec_get_name(codec_id)));

    m_complete = true;
}

SPDIFEncoder::~SPDIFEncoder(void)
{
    Destroy();
}

/**
 * Encode data through created muxer
 * unsigned char data: pointer to data to encode
 * int           size: size of data to encode
 */
void SPDIFEncoder::WriteFrame(unsigned char *data, int size)
{
    AVPacket *packet = av_packet_alloc();
    if (packet == nullptr)
    {
        LOG(VB_GENERAL, LOG_ERR, "packet allocation failed");
        return;
    }

    static int s_pts = 1; // to avoid warning "Encoder did not produce proper pts"
    packet->pts     = s_pts++;
    packet->data    = data;
    packet->size    = size;

    if (av_write_frame(m_oc, packet) < 0)
    {
        LOG(VB_AUDIO, LOG_ERR, LOC + "av_write_frame");
    }

    av_packet_free(&packet);
}

/**
 * Retrieve encoded data and copy it in the provided buffer.
 * Return -1 if there is no data to retrieve.
 * On return, dest_size will contain the length of the data copied
 * Upon completion, the internal encoder buffer is emptied.
 */
int SPDIFEncoder::GetData(unsigned char *buffer, size_t &dest_size)
{
    if ((m_oc == nullptr) || (m_oc->pb == nullptr))
    {
        LOG(VB_AUDIO, LOG_ERR, LOC + "GetData");
        return -1;
    }

    if(m_size > 0)
    {
        memcpy(buffer, m_oc->pb->buffer, m_size);
        dest_size = m_size;
        m_size = 0;
        return dest_size;
    }
    return -1;
}

int SPDIFEncoder::GetProcessedSize()
{
    if ((m_oc == nullptr) || (m_oc->pb == nullptr))
        return -1;
    return m_size;
}

unsigned char *SPDIFEncoder::GetProcessedBuffer()
{
    if ((m_oc == nullptr) || (m_oc->pb == nullptr))
        return nullptr;
    return m_oc->pb->buffer;
}

/**
 * Reset the internal encoder buffer
 */
void SPDIFEncoder::Reset()
{
    m_size = 0;
}

/**
 * Set the maximum HD rate.
 * If playing DTS-HD content, setting a HD rate of 0 will only use the DTS-Core
 * and the HD stream be stripped out before encoding
 * Input: rate = maximum HD rate in Hz
 */
bool SPDIFEncoder::SetMaxHDRate(int rate)
{
    if (!m_oc)
    {
        return false;
    }
    av_opt_set_int(m_oc->priv_data, "dtshd_rate", rate, 0);
    return true;
}

/**
 * funcIO: Internal callback function that will receive encoded frames
 */
int SPDIFEncoder::funcIO(void *opaque, const uint8_t *buf, int size)
{
    auto *enc = static_cast<SPDIFEncoder *>(opaque);

    if ((enc->m_oc == nullptr) || (enc->m_oc->pb == nullptr))
    {
        LOG(VB_AUDIO, LOG_ERR, LOC + "funcIO");
        return 0;
    }

    memcpy(enc->m_oc->pb->buffer + enc->m_size, buf, size);
    enc->m_size += size;
    return size;
}

/**
 * Destroy and free all allocated memory
 */
void SPDIFEncoder::Destroy()
{
    Reset();

    if (m_complete)
    {
        av_write_trailer(m_oc);
    }

    if (m_oc )
    {
        if (m_oc->pb)
        {
            av_free(m_oc->pb->buffer);
            av_freep(reinterpret_cast<void*>(&m_oc->pb));
        }
        avformat_free_context(m_oc);
        m_oc = nullptr;
    }
}
