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
#include "audiooutpututil.h"

extern "C" {
#if HAVE_BIGENDIAN
#include "byteswap.h"
#endif
#include "libavutil/opt.h"
#include "libavutil/samplefmt.h"
#include "libavutil/mem.h" // for av_free
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
      m_audPicture(NULL),
      m_audioInBuf(NULL),    m_audioInPBuf(NULL)
{
    av_register_all();
    avcodec_register_all();
    memset(&m_fmt, 0, sizeof(m_fmt));
}

AVFormatWriter::~AVFormatWriter()
{
    QMutexLocker locker(avcodeclock);

    if (m_ctx)
    {
        av_write_trailer(m_ctx);
        avio_close(m_ctx->pb);
        for(unsigned int i = 0; i < m_ctx->nb_streams; i++) {
            av_freep(&m_ctx->streams[i]);
        }
        av_freep(&m_ctx);
    }

    if (m_audioInBuf)
        av_freep(&m_audioInBuf);

    if (m_audioInPBuf)
        av_freep(&m_audioInPBuf);

    if (m_audPicture)
        avcodec_free_frame(&m_audPicture);
}

bool AVFormatWriter::Init(void)
{
    AVOutputFormat *fmt = av_guess_format(m_container.toLatin1().constData(),
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
            m_videoCodec.toLatin1().constData());
        if (!m_avVideoCodec)
        {
            LOG(VB_RECORD, LOG_ERR, LOC +
                QString("Init(): Unable to find video codec %1").arg(m_videoCodec));
            return false;
        }

        m_fmt.video_codec = m_avVideoCodec->id;
    }
    else
        m_fmt.video_codec = AV_CODEC_ID_NONE;

    m_avAudioCodec = avcodec_find_encoder_by_name(
        m_audioCodec.toLatin1().constData());
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
             m_filename.toLatin1().constData());

    if (m_fmt.video_codec != AV_CODEC_ID_NONE)
        m_videoStream = AddVideoStream();
    if (m_fmt.audio_codec != AV_CODEC_ID_NONE)
        m_audioStream = AddAudioStream();

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
        if (avio_open(&m_ctx->pb, m_filename.toLatin1().constData(),
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
    if ((m_bufferedVideoFrameTypes.isEmpty()) ||
        (m_bufferedVideoFrameTypes.first() == AV_PICTURE_TYPE_I))
        return true;

    return false;
}

int AVFormatWriter::WriteVideoFrame(VideoFrame *frame)
{
    //AVCodecContext *c = m_videoStream->codec;

    uint8_t *planes[3];
    unsigned char *buf = frame->buf;
    int framesEncoded = m_framesWritten + m_bufferedVideoFrameTimes.size();

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
    m_picture->pts = framesEncoded + 1;
    m_picture->type = FF_BUFFER_TYPE_SHARED;

    if ((framesEncoded % m_keyFrameDist) == 0)
        m_picture->pict_type = AV_PICTURE_TYPE_I;
    else
        m_picture->pict_type = AV_PICTURE_TYPE_NONE;

    int got_pkt = 0;
    int ret = 0;

    m_bufferedVideoFrameTimes.push_back(frame->timecode);
    m_bufferedVideoFrameTypes.push_back(m_picture->pict_type);

    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;
    {
        QMutexLocker locker(avcodeclock);
        ret = avcodec_encode_video2(m_videoStream->codec, &pkt,
                                    m_picture, &got_pkt);
    }

    if (ret < 0)
    {
        LOG(VB_RECORD, LOG_ERR, "avcodec_encode_video2() failed");
        return ret;
    }

    if (!got_pkt)
    {
        //LOG(VB_RECORD, LOG_DEBUG, QString("WriteVideoFrame(): Frame Buffered: cs: %1, mfw: %2, f->tc: %3, fn: %4, pt: %5").arg(pkt.size).arg(m_framesWritten).arg(frame->timecode).arg(frame->frameNumber).arg(m_picture->pict_type));
        return ret;
    }

    long long tc = frame->timecode;

    if (!m_bufferedVideoFrameTimes.isEmpty())
        tc = m_bufferedVideoFrameTimes.takeFirst();
    if (!m_bufferedVideoFrameTypes.isEmpty())
    {
        int pict_type = m_bufferedVideoFrameTypes.takeFirst();
        if (pict_type == AV_PICTURE_TYPE_I)
            pkt.flags |= AV_PKT_FLAG_KEY;
    }

    if (m_startingTimecodeOffset == -1)
        m_startingTimecodeOffset = tc - 1;
    tc -= m_startingTimecodeOffset;

    pkt.pts = tc * m_videoStream->time_base.den / m_videoStream->time_base.num / 1000;
    pkt.dts = AV_NOPTS_VALUE;
    pkt.stream_index= m_videoStream->index;

    //LOG(VB_RECORD, LOG_DEBUG, QString("WriteVideoFrame(): cs: %1, mfw: %2, pkt->pts: %3, tc: %4, fn: %5, pic->pts: %6, f->tc: %7, pt: %8").arg(pkt.size).arg(m_framesWritten).arg(pkt.pts).arg(tc).arg(frame->frameNumber).arg(m_picture->pts).arg(frame->timecode).arg(m_picture->pict_type));
    ret = av_interleaved_write_frame(m_ctx, &pkt);
    if (ret != 0)
        LOG(VB_RECORD, LOG_ERR, LOC + "WriteVideoFrame(): "
                "av_interleaved_write_frame couldn't write Video");

    frame->timecode = tc + m_startingTimecodeOffset;
    m_framesWritten++;

    av_free_packet(&pkt);

    return 1;
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

int AVFormatWriter::WriteAudioFrame(unsigned char *buf, int fnum, long long &timecode)
{
#if HAVE_BIGENDIAN
    bswap_16_buf((short int*) buf, m_audioFrameSize, m_audioChannels);
#endif

    int got_packet = 0;
    int ret = 0;
    int samples_per_avframe  = m_audioFrameSize * m_audioChannels;
    int sampleSizeIn   = AudioOutputSettings::SampleSize(FORMAT_S16);
    AudioFormat format =
        AudioOutputSettings::AVSampleFormatToFormat(m_audioStream->codec->sample_fmt);
    int sampleSizeOut  = AudioOutputSettings::SampleSize(format);

    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data          = NULL;
    pkt.size          = 0;

    if (av_get_packed_sample_fmt(m_audioStream->codec->sample_fmt) == AV_SAMPLE_FMT_FLT)
    {
        AudioOutputUtil::toFloat(FORMAT_S16, (void *)m_audioInBuf, (void *)buf,
                                 samples_per_avframe * sampleSizeIn);
        buf = m_audioInBuf;
    }
    if (av_sample_fmt_is_planar(m_audioStream->codec->sample_fmt))
    {
        AudioOutputUtil::DeinterleaveSamples(format,
                                             m_audioChannels,
                                             m_audioInPBuf,
                                             buf,
                                             samples_per_avframe * sampleSizeOut);

        // init AVFrame for planar data (input is interleaved)
        for (int j = 0, jj = 0; j < m_audioChannels; j++, jj += m_audioFrameSize)
        {
            m_audPicture->data[j] = (uint8_t*)(m_audioInPBuf + jj * sampleSizeOut);
        }
    }
    else
    {
        m_audPicture->data[0] = buf;
    }

    m_audPicture->linesize[0] = m_audioFrameSize;
    m_audPicture->nb_samples = m_audioFrameSize;
    m_audPicture->format = m_audioStream->codec->sample_fmt;
    m_audPicture->extended_data = m_audPicture->data;

    m_bufferedAudioFrameTimes.push_back(timecode);

    {
        QMutexLocker locker(avcodeclock);
        ret = avcodec_encode_audio2(m_audioStream->codec, &pkt,
                                    m_audPicture, &got_packet);
    }

    if (ret < 0)
    {
        LOG(VB_RECORD, LOG_ERR, "avcodec_encode_audio2() failed");
        return ret;
    }

    if (!got_packet)
    {
        //LOG(VB_RECORD, LOG_ERR, QString("WriteAudioFrame(): Frame Buffered: cs: %1, mfw: %2, f->tc: %3, fn: %4").arg(m_audPkt->size).arg(m_framesWritten).arg(timecode).arg(fnum));
        return ret;
    }

    long long tc = timecode;

    if (m_bufferedAudioFrameTimes.size())
        tc = m_bufferedAudioFrameTimes.takeFirst();

    if (m_startingTimecodeOffset == -1)
        m_startingTimecodeOffset = tc - 1;
    tc -= m_startingTimecodeOffset;

    if (m_avVideoCodec)
        pkt.pts = tc * m_videoStream->time_base.den / m_videoStream->time_base.num / 1000;
    else
        pkt.pts = tc * m_audioStream->time_base.den / m_audioStream->time_base.num / 1000;

    pkt.dts = AV_NOPTS_VALUE;
    pkt.flags |= AV_PKT_FLAG_KEY;
    pkt.stream_index = m_audioStream->index;

    //LOG(VB_RECORD, LOG_ERR, QString("WriteAudioFrame(): cs: %1, mfw: %2, pkt->pts: %3, tc: %4, fn: %5, f->tc: %6").arg(m_audPkt->size).arg(m_framesWritten).arg(m_audPkt->pts).arg(tc).arg(fnum).arg(timecode));

    ret = av_interleaved_write_frame(m_ctx, &pkt);
    if (ret != 0)
        LOG(VB_RECORD, LOG_ERR, LOC + "WriteAudioFrame(): "
                "av_interleaved_write_frame couldn't write Audio");
    timecode = tc + m_startingTimecodeOffset;

    av_free_packet(&pkt);

    return 1;
}

int AVFormatWriter::WriteTextFrame(int vbimode, unsigned char *buf, int len,
                                   long long timecode, int pagenr)
{
    return 1;
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
    AVCodec *codec;

    st = avformat_new_stream(m_ctx, NULL);
    if (!st)
    {
        LOG(VB_RECORD, LOG_ERR,
            LOC + "AddVideoStream(): avformat_new_stream() failed");
        return NULL;
    }
    st->id = 0;

    c = st->codec;

    codec = avcodec_find_encoder(m_ctx->oformat->video_codec);
    if (!codec)
    {
        LOG(VB_RECORD, LOG_ERR,
            LOC + "AddVideoStream(): avcodec_find_encoder() failed");
        return NULL;
    }

    avcodec_get_context_defaults3(c, codec);

    c->codec                      = codec;
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
    c->thread_type                = FF_THREAD_SLICE;

    if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
        c->max_b_frames          = 2;
    }
    else if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO)
    {
        c->mb_decision           = 2;
    }
    else if (c->codec_id == AV_CODEC_ID_H264)
    {

        if ((c->width > 480) ||
            (c->bit_rate > 600000))
        {
            c->level = 31;
            av_opt_set(c->priv_data, "profile", "main", 0);
        }
        else
        {
            c->level = 30;
            av_opt_set(c->priv_data, "profile", "baseline", 0);
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

    return st;
}

bool AVFormatWriter::OpenVideo(void)
{
    AVCodecContext *c;

    c = m_videoStream->codec;

    if (!m_width || !m_height)
        return false;

    if (avcodec_open2(c, NULL, NULL) < 0)
    {
        LOG(VB_RECORD, LOG_ERR,
            LOC + "OpenVideo(): avcodec_open() failed");
        return false;
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
    c->codec_id     = m_ctx->oformat->audio_codec;
    c->codec_type   = AVMEDIA_TYPE_AUDIO;
    c->bit_rate     = m_audioBitrate;
    c->sample_rate  = m_audioFrameRate;
    c->channels     = m_audioChannels;

    // c->flags |= CODEC_FLAG_QSCALE; // VBR
    // c->global_quality = blah;

    if (!m_avVideoCodec)
    {
        c->time_base      = GetCodecTimeBase();
        st->time_base.den = 90000;
        st->time_base.num = 1;
    }

    if(m_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= CODEC_FLAG_GLOBAL_HEADER;

    return st;
}

bool AVFormatWriter::FindAudioFormat(AVCodecContext *ctx, AVCodec *c, AVSampleFormat format)
{
    if (c->sample_fmts)
    {
        for (int i = 0; c->sample_fmts[i] != AV_SAMPLE_FMT_NONE; i++)
        {
            if (av_get_packed_sample_fmt(c->sample_fmts[i]) == format)
            {
                ctx->sample_fmt = c->sample_fmts[i];
                return true;
            }
        }
    }
    return false;
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

    // try to find suitable format we can use. avcodec_open2 will fail if we don't
    // find one, so no need to worry otherwise. Can only handle S16 or FLOAT
    // we give priority to S16 as libmp3lame requires aligned floats which we can't guarantee
    if (!FindAudioFormat(c, codec, AV_SAMPLE_FMT_S16))
    {
        FindAudioFormat(c, codec, AV_SAMPLE_FMT_FLT);
    }

    if (avcodec_open2(c, codec, NULL) < 0)
    {
        LOG(VB_RECORD, LOG_ERR,
            LOC + "OpenAudio(): avcodec_open() failed");
        return false;
    }

    m_audioFrameSize = c->frame_size; // number of *samples* per channel in an AVFrame

    m_audPicture = avcodec_alloc_frame();
    if (!m_audPicture)
    {
        LOG(VB_RECORD, LOG_ERR,
            LOC + "OpenAudio(): alloc_frame() failed");
        return false;
    }

    int samples_per_frame = m_audioFrameSize * m_audioChannels;
    int bps = av_get_bytes_per_sample(c->sample_fmt);
    if (av_get_packed_sample_fmt(c->sample_fmt) == AV_SAMPLE_FMT_FLT)
    {
        // allocate buffer to convert from S16 to float
        if (!(m_audioInBuf = (unsigned char*)av_malloc(bps * samples_per_frame)))
            return false;
    }
    if (av_sample_fmt_is_planar(c->sample_fmt))
    {
        // allocate buffer to convert interleaved to planar audio
        if (!(m_audioInPBuf = (unsigned char*)av_malloc(bps * samples_per_frame)))
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

