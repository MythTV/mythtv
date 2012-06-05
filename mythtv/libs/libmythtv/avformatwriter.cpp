/*  -*- Mode: c++ -*-
 *
 *   Class AVFormatWriter
 *
 *   Copyright (C) Chris Pinkham 2011
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "mythlogging.h"
#include "mythcorecontext.h"
#include "NuppelVideoRecorder.h"
#include "avformatwriter.h"

extern "C" {
#if HAVE_BIGENDIAN
#include "byteswap.h"
#endif
#include "libavutil/opt.h"
}

#define LOC QString("AVFW(%1): ").arg(m_filename)
#define LOC_ERR QString("AVFW(%1) Error: ").arg(m_filename)
#define LOC_WARN QString("AVFW(%1) Warning: ").arg(m_filename)

AVFormatWriter::AVFormatWriter()
    : FileWriterBase(),

      m_avfRingBuffer(NULL), m_ringBuffer(NULL),

      m_ctx(NULL),
      m_videoStream(NULL),   m_avVideoCodec(NULL),
      m_audioStream(NULL),   m_avAudioCodec(NULL),
      m_picture(NULL),       m_tmpPicture(NULL),
      m_videoOutBuf(NULL),   m_videoOutBufSize(0),
      m_audioSamples(NULL),  m_audioOutBuf(NULL),
      m_audioOutBufSize(0),  m_audioInputFrameSize(0)
{
    av_register_all();
    avcodec_register_all();

    // bool debug = VERBOSE_LEVEL_CHECK(VB_LIBAV, LOG_ANY);
    // av_log_set_level((debug) ? AV_LOG_DEBUG : AV_LOG_ERROR);
    // av_log_set_callback(myth_av_log);
}

AVFormatWriter::~AVFormatWriter()
{
    QMutexLocker locker(avcodeclock);

    if (m_pkt)
    {
        av_free_packet(m_pkt);
        delete m_pkt;
        m_pkt = NULL;
    }

    if (m_audPkt)
    {
        av_free_packet(m_audPkt);
        delete m_audPkt;
        m_audPkt = NULL;
    }

    if (m_ctx)
    {
        av_write_trailer(m_ctx);
        avio_close(m_ctx->pb);
        for(unsigned int i = 0; i < m_ctx->nb_streams; i++) {
            av_freep(&m_ctx->streams[i]);
        }

        av_free(m_ctx);
        m_ctx = NULL;
    }

    if (m_videoOutBuf)
        delete [] m_videoOutBuf;
}

bool AVFormatWriter::Init(void)
{
    if (m_videoOutBuf)
        delete [] m_videoOutBuf;

    if (m_width && m_height)
        m_videoOutBuf = new unsigned char[m_width * m_height * 2 + 10];

    AVOutputFormat *fmt = av_guess_format(m_container.toAscii().constData(),
                                          NULL, NULL);
    if (!fmt)
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            QString("Init(): Unable to guess AVOutputFormat from container %1")
                    .arg(m_container));
        return false;
    }

    m_fmt = *fmt;

    if (m_width && m_height)
    {
        m_avVideoCodec = avcodec_find_encoder_by_name(
            m_videoCodec.toAscii().constData());
        if (!m_avVideoCodec)
        {
            LOG(VB_RECORD, LOG_ERR, LOC +
                QString("Init(): Unable to find video codec %1").arg(m_videoCodec));
            return false;
        }

        m_fmt.video_codec = m_avVideoCodec->id;
    }
    else
        m_fmt.video_codec = CODEC_ID_NONE;

    m_avAudioCodec = avcodec_find_encoder_by_name(
        m_audioCodec.toAscii().constData());
    if (!m_avAudioCodec)
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            QString("Init(): Unable to find audio codec %1").arg(m_audioCodec));
        return false;
    }

    m_fmt.audio_codec = m_avAudioCodec->id;

    m_ctx = avformat_alloc_context();
    if (!m_ctx)
    {
        LOG(VB_RECORD, LOG_ERR,
            LOC + "Init(): Unable to allocate AVFormatContext");
        return false;
    }

    m_ctx->oformat = &m_fmt;

    if (m_container == "mpegts")
        m_ctx->packet_size = 2324;

    snprintf(m_ctx->filename, sizeof(m_ctx->filename), "%s",
             m_filename.toAscii().constData());

    if (m_fmt.video_codec != CODEC_ID_NONE)
        m_videoStream = AddVideoStream();
    if (m_fmt.audio_codec != CODEC_ID_NONE)
        m_audioStream = AddAudioStream();

    m_pkt = new AVPacket;
    if (!m_pkt)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "Init(): error allocating AVPacket");
        return false;
    }
    av_new_packet(m_pkt, m_ctx->packet_size);

    m_audPkt = new AVPacket;
    if (!m_audPkt)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "Init(): error allocating AVPacket");
        return false;
    }
    av_new_packet(m_audPkt, m_ctx->packet_size);

    if ((m_videoStream) && (!OpenVideo()))
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "Init(): OpenVideo() failed");
        return false;
    }

    if ((m_audioStream) && (!OpenAudio()))
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "Init(): OpenAudio() failed");
        return false;
    }

    return true;
}

bool AVFormatWriter::OpenFile(void)
{
    if (!(m_fmt.flags & AVFMT_NOFILE))
    {
        if (avio_open(&m_ctx->pb, m_filename.toAscii().constData(),
                      AVIO_FLAG_WRITE) < 0)
        {
            LOG(VB_RECORD, LOG_ERR, LOC + "OpenFile(): avio_open() failed");
            return false;
        }
    }

    m_ringBuffer = RingBuffer::Create(m_filename, true);

    if (!m_ringBuffer)
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            "OpenFile(): RingBuffer::Create() failed");
        return false;
    }

    m_avfRingBuffer     = new AVFRingBuffer(m_ringBuffer);
    URLContext *uc      = (URLContext *)m_ctx->pb->opaque;
    uc->prot            = AVFRingBuffer::GetRingBufferURLProtocol();
    uc->priv_data       = (void *)m_avfRingBuffer;

    avformat_write_header(m_ctx, NULL);

    return true;
}

bool AVFormatWriter::CloseFile(void)
{
    if (m_ctx)
    {
        av_write_trailer(m_ctx);
        avio_close(m_ctx->pb);
        for(unsigned int i = 0; i < m_ctx->nb_streams; i++) {
            av_freep(&m_ctx->streams[i]);
        }

        av_free(m_ctx);
        m_ctx = NULL;
    }

    return true;
}

bool AVFormatWriter::NextFrameIsKeyFrame(void)
{
    if ((m_framesWritten % m_keyFrameDist) == 0)
        return true;

    return false;
}

bool AVFormatWriter::WriteVideoFrame(VideoFrame *frame)
{
    AVCodecContext *c;

    c = m_videoStream->codec;

    uint8_t *planes[3];
    int len = frame->size;
    unsigned char *buf = frame->buf;

    planes[0] = buf;
    planes[1] = planes[0] + frame->width * frame->height;
    planes[2] = planes[1] + (frame->width * frame->height) /
        4; // (pictureFormat == PIX_FMT_YUV422P ? 2 : 4);

    m_picture->data[0] = planes[0];
    m_picture->data[1] = planes[1];
    m_picture->data[2] = planes[2];
    m_picture->linesize[0] = frame->width;
    m_picture->linesize[1] = frame->width / 2;
    m_picture->linesize[2] = frame->width / 2;
    m_picture->pts = m_framesWritten + 1;
    m_picture->type = FF_BUFFER_TYPE_SHARED;

    if ((m_framesWritten % m_keyFrameDist) == 0)
        m_picture->pict_type = AV_PICTURE_TYPE_I;
    else
        m_picture->pict_type = AV_PICTURE_TYPE_NONE;

    int got_pkt = 0;
    int ret = 0;

    av_init_packet(m_pkt);
    m_pkt->data = (unsigned char *)m_videoOutBuf;
    m_pkt->size = len;

    {
        QMutexLocker locker(avcodeclock);
        ret = avcodec_encode_video2(m_videoStream->codec, m_pkt,
                                      m_picture, &got_pkt); 
    }

    if (ret < 0 || !got_pkt)
    {
#if 0
        LOG(VB_RECORD, LOG_ERR, QString("WriteVideoFrame(): cs: %1, mfw: %2, tc: %3, fn: %4").arg(m_pkt->size).arg(m_framesWritten).arg(frame->timecode).arg(frame->frameNumber));
#endif
        return false;
    }

    if ((m_framesWritten % m_keyFrameDist) == 0)
        m_pkt->flags |= AV_PKT_FLAG_KEY;

    long long tc = frame->timecode;
    if (m_startingTimecodeOffset == -1)
        m_startingTimecodeOffset = tc;
    tc -= m_startingTimecodeOffset;

    m_pkt->pts = tc * m_videoStream->time_base.den / m_videoStream->time_base.num / 1000;
    m_pkt->dts = AV_NOPTS_VALUE;
    m_pkt->stream_index= m_videoStream->index;

    // LOG(VB_RECORD, LOG_ERR, QString("WriteVideoFrame(): cs: %1, mfw: %2, pkt->pts: %3, tc: %4, fn: %5, pic->pts: %6").arg(m_pkt->size).arg(m_framesWritten).arg(m_pkt->pts).arg(frame->timecode).arg(frame->frameNumber).arg(m_picture->pts));
    ret = av_interleaved_write_frame(m_ctx, m_pkt);
    if (ret != 0)
        LOG(VB_RECORD, LOG_ERR, LOC + "WriteVideoFrame(): "
                "av_interleaved_write_frame couldn't write Video");

    m_framesWritten++;

    return true;
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

bool AVFormatWriter::WriteAudioFrame(unsigned char *buf, int fnum, int timecode)
{
#if HAVE_BIGENDIAN
    int sample_cnt = m_audioFrameSize / m_audioBytesPerSample;
    bswap_16_buf((short int*) buf, sample_cnt, m_audioChannels);
#endif

    int got_packet = 0;
    int ret = 0;

    av_init_packet(m_audPkt);
    m_audPkt->data = m_audioOutBuf;
    m_audPkt->size = m_audioOutBufSize;

    m_audPicture->data[0] = buf;
    m_audPicture->linesize[0] = m_audioFrameSize;
    m_audPicture->nb_samples = m_audioFrameSize;
    m_audPicture->format = AV_SAMPLE_FMT_S16;

    {
        QMutexLocker locker(avcodeclock);
        ret = avcodec_encode_audio2(m_audioStream->codec, m_audPkt,
                                      m_audPicture, &got_packet);
    }

    if (ret < 0 || !got_packet)
    {
#if 0
        LOG(VB_RECORD, LOG_ERR, QString("WriteAudioFrame(): No Encoded Data: cs: %1, mfw: %2, tc: %3, fn: %4").arg(m_audPkt->size).arg(m_framesWritten).arg(timecode).arg(fnum));
#endif
        return false;
    }

    long long tc = timecode;
    if (m_startingTimecodeOffset == -1)
        m_startingTimecodeOffset = tc;
    tc -= m_startingTimecodeOffset;

    if (m_avVideoCodec)
        m_audPkt->pts = tc * m_videoStream->time_base.den / m_videoStream->time_base.num / 1000;
    else
        m_audPkt->pts = tc * m_audioStream->time_base.den / m_audioStream->time_base.num / 1000;

    m_audPkt->dts = AV_NOPTS_VALUE;
    m_audPkt->flags |= AV_PKT_FLAG_KEY;
    m_audPkt->data = (uint8_t*)m_audioOutBuf;
    m_audPkt->stream_index = m_audioStream->index;

    // LOG(VB_RECORD, LOG_ERR, QString("WriteAudioFrame(): cs: %1, mfw: %2, pkt->pts: %3, tc: %4, fn: %5").arg(m_audPkt->size).arg(m_framesWritten).arg(m_audPkt->pts).arg(timecode).arg(fnum));

    ret = av_interleaved_write_frame(m_ctx, m_audPkt);
    if (ret != 0)
        LOG(VB_RECORD, LOG_ERR, LOC + "WriteAudioFrame(): "
                "av_interleaved_write_frame couldn't write Audio");

    return true;
}

bool AVFormatWriter::WriteTextFrame(int vbimode, unsigned char *buf, int len,
                                    int timecode, int pagenr)
{
    return true;
}

bool AVFormatWriter::ReOpen(QString filename)
{
    bool result = m_ringBuffer->ReOpen(filename);

    if (result)
        m_filename = filename;

    return result;
}

AVStream* AVFormatWriter::AddVideoStream(void)
{
    AVCodecContext *c;
    AVStream *st;

    st = avformat_new_stream(m_ctx, NULL);
    if (!st)
    {
        LOG(VB_RECORD, LOG_ERR,
            LOC + "AddVideoStream(): avformat_new_stream() failed");
        return NULL;
    }
    st->id = 0;

    c = st->codec;

    c->codec_id                   = m_ctx->oformat->video_codec;
    c->codec_type                 = AVMEDIA_TYPE_VIDEO;
    c->bit_rate                   = m_videoBitrate;
    c->width                      = m_width;
    c->height                     = m_height;

    // c->sample_aspect_ratio.num    = (int)floor(m_aspect * 10000);
    // c->sample_aspect_ratio.den    = 10000;

    c->time_base                  = GetCodecTimeBase();

    st->time_base.den             = 90000;
    st->time_base.num             = 1;
    st->r_frame_rate.num          = 0;
    st->r_frame_rate.den          = 0;

    c->gop_size                   = m_keyFrameDist;
    c->pix_fmt                    = PIX_FMT_YUV420P;
    c->thread_count               = m_encodingThreadCount;

    if (c->codec_id == CODEC_ID_MPEG2VIDEO) {
        c->max_b_frames          = 2;
    }
    else if (c->codec_id == CODEC_ID_MPEG1VIDEO)
    {
        c->mb_decision           = 2;
    }
    else if (c->codec_id == CODEC_ID_H264)
    {
        av_opt_set(c->priv_data, "profile", "baseline", 0);

        if ((c->height <= 240) &&
            (c->width  <= 320) &&
            (c->bit_rate <= 768000))
        {
            c->level             = 13;
        }
        else if (c->width >= 960)
        {
            if (c->width >= 1024)
                c->level             = 41;
            else
                c->level             = 31;

            av_opt_set(c->priv_data, "profile", "main", 0);
        }
        else
        {
            c->level             = 30;
        }

        c->coder_type            = 0;
        c->max_b_frames          = 0;
        c->slices                = 8;

        c->flags                |= CODEC_FLAG_LOOP_FILTER;
        c->me_cmp               |= 1;
        c->me_method             = ME_HEX;
        c->me_subpel_quality     = 6;
        c->me_range              = 16;
        c->keyint_min            = 25;
        c->scenechange_threshold = 40;
        c->i_quant_factor        = 0.71;
        c->b_frame_strategy      = 1;
        c->qcompress             = 0.6;
        c->qmin                  = 10;
        c->qmax                  = 51;
        c->max_qdiff             = 4;
        c->refs                  = 3;
        c->trellis               = 0;

        av_opt_set(c, "partitions", "i8x8,i4x4,p8x8,b8x8", 0);
        av_opt_set_int(c, "direct-pred", 1, 0);
        av_opt_set_int(c, "rc-lookahead", 0, 0);
        av_opt_set_int(c, "fast-pskip", 1, 0);
        av_opt_set_int(c, "mixed-refs", 1, 0);
        av_opt_set_int(c, "8x8dct", 0, 0);
        av_opt_set_int(c, "weightb", 0, 0);
    }

    if(m_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= CODEC_FLAG_GLOBAL_HEADER;

    // LOG(VB_RECORD, LOG_ERR, LOC + QString("AddVideoStream(): br: %1, gs: %2, tb: %3/%4, w: %5, h: %6").arg(c->bit_rate).arg(c->gop_size).arg(c->time_base.den).arg(c->time_base.num).arg(c->width).arg(c->height));

    return st;
}

bool AVFormatWriter::OpenVideo(void)
{
    AVCodec *codec;
    AVCodecContext *c;

    c = m_videoStream->codec;

    codec = avcodec_find_encoder(c->codec_id);
    if (!codec)
    {
        LOG(VB_RECORD, LOG_ERR,
            LOC + "OpenVideo(): avcodec_find_encoder() failed");
        return false;
    }

    if (avcodec_open2(c, codec, NULL) < 0)
    {
        LOG(VB_RECORD, LOG_ERR,
            LOC + "OpenVideo(): avcodec_open() failed");
        return false;
    }

    m_videoOutBuf = NULL;
    if (!(m_ctx->oformat->flags & AVFMT_RAWPICTURE)) {
        m_videoOutBufSize = 200000;
        m_videoOutBuf = (unsigned char *)av_malloc(m_videoOutBufSize);
    }

    m_picture = AllocPicture(c->pix_fmt);
    if (!m_picture)
    {
        LOG(VB_RECORD, LOG_ERR,
            LOC + "OpenVideo(): AllocPicture() failed");
        return false;
    }

    m_tmpPicture = NULL;
    if (c->pix_fmt != PIX_FMT_YUV420P)
    {
        m_tmpPicture = AllocPicture(PIX_FMT_YUV420P);
        if (!m_tmpPicture)
        {
            LOG(VB_RECORD, LOG_ERR,
                LOC + "OpenVideo(): m_tmpPicture AllocPicture() failed");
            return false;
        }
    }

    return true;
}

AVStream* AVFormatWriter::AddAudioStream(void)
{
    AVCodecContext *c;
    AVStream *st;

    st = avformat_new_stream(m_ctx, NULL);
    if (!st)
    {
        LOG(VB_RECORD, LOG_ERR,
            LOC + "AddAudioStream(): avformat_new_stream() failed");
        return NULL;
    }
    st->id = 1;

    c = st->codec;
    c->codec_id = m_ctx->oformat->audio_codec;
    c->codec_type = AVMEDIA_TYPE_AUDIO;

    c->sample_fmt = AV_SAMPLE_FMT_S16;
    m_audioBytesPerSample = m_audioChannels * 2;

    c->bit_rate = m_audioBitrate;
    c->sample_rate = m_audioSampleRate;
    c->channels = m_audioChannels;

    // c->flags |= CODEC_FLAG_QSCALE; // VBR
    // c->global_quality = blah;

    if (!m_avVideoCodec)
    {
        c->time_base = GetCodecTimeBase();
        st->time_base.den = 90000;
        st->time_base.num = 1;
    }

    // LOG(VB_RECORD, LOG_ERR, LOC + QString("AddAudioStream(): br: %1, sr, %2, c: %3, tb: %4/%5").arg(c->bit_rate).arg(c->sample_rate).arg(c->channels).arg(c->time_base.den).arg(c->time_base.num));

    // Disable this for support on some devices
    //if (c->codec_id == CODEC_ID_AAC)
    //    c->profile = FF_PROFILE_AAC_MAIN;

    if(m_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= CODEC_FLAG_GLOBAL_HEADER;

    return st;
}

bool AVFormatWriter::OpenAudio(void)
{
    AVCodecContext *c;
    AVCodec *codec;

    c = m_audioStream->codec;

    c->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

    codec = avcodec_find_encoder(c->codec_id);
    if (!codec)
    {
        LOG(VB_RECORD, LOG_ERR,
            LOC + "OpenAudio(): avcodec_find_encoder() failed");
        return false;
    }

    if (avcodec_open2(c, codec, NULL) < 0)
    {
        LOG(VB_RECORD, LOG_ERR,
            LOC + "OpenAudio(): avcodec_open() failed");
        return false;
    }

    m_audioFrameSize = c->frame_size;

    m_audioOutBufSize = (int)(1.25 * 16384 * 7200);
    //m_audioOutBufSize = c->frame_size * 2 * c->channels;

    m_audioOutBuf = (unsigned char *)av_malloc(m_audioOutBufSize);

    if (c->frame_size <= 1) {
        m_audioInputFrameSize = m_audioOutBufSize / c->channels;
        switch(m_audioStream->codec->codec_id) {
        case CODEC_ID_PCM_S16LE:
        case CODEC_ID_PCM_S16BE:
        case CODEC_ID_PCM_U16LE:
        case CODEC_ID_PCM_U16BE:
            m_audioInputFrameSize >>= 1;
            break;
        default:
            break;
        }
    } else {
        m_audioInputFrameSize = c->frame_size;
    }
    m_audioSamples =
        (unsigned int *)av_malloc(m_audioInputFrameSize * 2 * c->channels);

    m_audPicture = avcodec_alloc_frame();
    if (!m_audPicture)
    {
        LOG(VB_RECORD, LOG_ERR,
            LOC + "OpenAudio(): alloc_frame() failed");
        return false;
    }

    return true;
}

AVFrame* AVFormatWriter::AllocPicture(enum PixelFormat pix_fmt)
{
    AVFrame *picture;
    unsigned char *picture_buf;
    int size;

    picture = avcodec_alloc_frame();
    if (!picture)
    {
        LOG(VB_RECORD, LOG_ERR,
            LOC + "AllocPicture(): avcodec_alloc_frame() failed");
        return NULL;
    }
    size = avpicture_get_size(pix_fmt, m_width, m_height);
    picture_buf = (unsigned char *)av_malloc(size);
    if (!picture_buf)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "AllocPicture(): av_malloc() failed");
        av_free(picture);
        return NULL;
    }
    avpicture_fill((AVPicture *)picture, picture_buf,
                   pix_fmt, m_width, m_height);
    return picture;
}

AVRational AVFormatWriter::GetCodecTimeBase(void)
{
    AVRational result;

    result.den = (int)floor(m_frameRate * 100);
    result.num = 100;

    if (m_avVideoCodec && m_avVideoCodec->supported_framerates) {
        const AVRational *p= m_avVideoCodec->supported_framerates;
        AVRational req =
            (AVRational){result.den, result.num};
        const AVRational *best = NULL;
        AVRational best_error= (AVRational){INT_MAX, 1};
        for(; p->den!=0; p++) {
            AVRational error = av_sub_q(req, *p);
            if (error.num <0)
                error.num *= -1;
            if (av_cmp_q(error, best_error) < 0) {
                best_error = error;
                best = p;
            }
        }

        if (best && best->num && best->den)
        {
            result.den = best->num;
            result.num = best->den;
        }
    }

    if (result.den == 2997)
    {
         result.den = 30000;
         result.num = 1001;
    }
    else if (result.den == 5994)
    {
         result.den = 60000;
         result.num = 1001;
    }

    return result;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */

