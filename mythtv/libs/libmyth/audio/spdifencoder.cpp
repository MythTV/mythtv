#include "config.h"

#include "mythcorecontext.h"

#include "compat.h"
#include "spdifencoder.h"
#include "mythlogging.h"
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
SPDIFEncoder::SPDIFEncoder(QString muxer, int codec_id)
    : m_complete(false), m_oc(NULL), m_stream(NULL), m_size(0)
{
    QByteArray dev_ba     = muxer.toLatin1();
    AVOutputFormat *fmt;

    avcodeclock->lock();
    av_register_all();
    avcodeclock->unlock();

    fmt = av_guess_format(dev_ba.constData(), NULL, NULL);
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

    m_oc->pb = avio_alloc_context(m_buffer, sizeof(m_buffer), 0,
                                  this, NULL, funcIO, NULL);
    if (!m_oc->pb)
    {
        LOG(VB_AUDIO, LOG_ERR, LOC + "avio_alloc_context");
        Destroy();
        return;
    }

    m_oc->pb->seekable    = 0;
    m_oc->flags          |= AVFMT_NOFILE | AVFMT_FLAG_IGNIDX;

    m_stream = avformat_new_stream(m_oc, NULL);
    if (!m_stream)
    {
        LOG(VB_AUDIO, LOG_ERR, LOC + "avformat_new_stream");
        Destroy();
        return;
    }

    m_stream->id          = 1;

    AVCodecContext *codec = m_stream->codec;

    codec->codec_id       = (CodecID)codec_id;
    avformat_write_header(m_oc, NULL);

    LOG(VB_AUDIO, LOG_INFO, LOC + QString("Creating %1 encoder (for %2)")
            .arg(muxer).arg(ff_codec_id_string((CodecID)codec_id)));

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
    AVPacket packet;
    av_init_packet(&packet);
    static int pts = 1; // to avoid warning "Encoder did not produce proper pts"
    packet.pts  = pts++; 
    packet.data    = data;
    packet.size    = size;

    if (av_write_frame(m_oc, &packet) < 0)
    {
        LOG(VB_AUDIO, LOG_ERR, LOC + "av_write_frame");
    }
}

/**
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
int SPDIFEncoder::funcIO(void *opaque, unsigned char *buf, int size)
{
    SPDIFEncoder *enc = (SPDIFEncoder *)opaque;

    memcpy(enc->m_buffer + enc->m_size, buf, size);
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
        av_freep(&m_oc);
    }
}
