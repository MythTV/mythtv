/*
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
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

// MythTV
#include "mythlogging.h"
#include "mythcorecontext.h"
#include "audiooutpututil.h"
#include "mythavutil.h"
#include "io/mythavformatwriter.h"

// FFmpeg
extern "C" {
#if HAVE_BIGENDIAN
#include "bswap.h"
#endif
#include "libavutil/opt.h"
#include "libavutil/samplefmt.h"
#include "libavutil/imgutils.h"
}

#define LOC QString("AVFW(%1): ").arg(m_filename)
#define LOC_ERR QString("AVFW(%1) Error: ").arg(m_filename)
#define LOC_WARN QString("AVFW(%1) Warning: ").arg(m_filename)

MythAVFormatWriter::~MythAVFormatWriter()
{
    if (m_ctx)
    {
        av_write_trailer(m_ctx);
        avio_closep(&m_ctx->pb);
        for(uint i = 0; i < m_ctx->nb_streams; i++)
            av_freep(&m_ctx->streams[i]);
        av_freep(&m_ctx);
    }

    av_freep(&m_audioInBuf);
    av_freep(&m_audioInPBuf);
    if (m_audPicture)
        av_frame_free(&m_audPicture);
    Cleanup();
    av_frame_free(&m_picture);
}

bool MythAVFormatWriter::Init(void)
{
    AVOutputFormat *fmt = av_guess_format(m_container.toLatin1().constData(), nullptr, nullptr);
    if (!fmt)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + QString("Init(): Unable to guess AVOutputFormat from container %1")
            .arg(m_container));
        return false;
    }

    m_fmt = *fmt;

    if (m_width && m_height)
    {
        m_avVideoCodec = avcodec_find_encoder_by_name(m_videoCodec.toLatin1().constData());
        if (!m_avVideoCodec)
        {
            LOG(VB_RECORD, LOG_ERR, LOC + QString("Init(): Unable to find video codec %1")
                .arg(m_videoCodec));
            return false;
        }
        m_fmt.video_codec = m_avVideoCodec->id;
    }
    else
    {
        m_fmt.video_codec = AV_CODEC_ID_NONE;
    }

    m_avAudioCodec = avcodec_find_encoder_by_name(m_audioCodec.toLatin1().constData());
    if (!m_avAudioCodec)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + QString("Init(): Unable to find audio codec %1")
            .arg(m_audioCodec));
        return false;
    }

    m_fmt.audio_codec = m_avAudioCodec->id;

    m_ctx = avformat_alloc_context();
    if (!m_ctx)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "Init(): Unable to allocate AVFormatContext");
        return false;
    }

    m_ctx->oformat = &m_fmt;

    if (m_container == "mpegts")
        m_ctx->packet_size = 2324;

    QByteArray filename = m_filename.toLatin1();
    auto size = static_cast<size_t>(filename.size());
    m_ctx->url = static_cast<char*>(av_malloc(size));
    memcpy(m_ctx->url, filename.constData(), size);

    if (m_fmt.video_codec != AV_CODEC_ID_NONE)
        m_videoStream = AddVideoStream();
    if (m_fmt.audio_codec != AV_CODEC_ID_NONE)
        m_audioStream = AddAudioStream();

    if (m_videoStream && !OpenVideo())
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "Init(): OpenVideo() failed");
        return false;
    }

    if (m_audioStream && !OpenAudio())
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "Init(): OpenAudio() failed");
        return false;
    }

    return true;
}

bool MythAVFormatWriter::OpenFile(void)
{
    if (!(m_fmt.flags & AVFMT_NOFILE))
    {
        if (avio_open(&m_ctx->pb, m_filename.toLatin1().constData(), AVIO_FLAG_WRITE) < 0)
        {
            LOG(VB_RECORD, LOG_ERR, LOC + "OpenFile(): avio_open() failed");
            return false;
        }
    }

    m_buffer = MythMediaBuffer::Create(m_filename, true);

    if (!m_buffer || !m_buffer->GetLastError().isEmpty())
    {
        LOG(VB_RECORD, LOG_ERR, LOC + QString("OpenFile(): RingBuffer::Create() failed: '%1'")
            .arg(m_buffer ? m_buffer->GetLastError() : ""));
        Cleanup();
        return false;
    }

    m_avfBuffer    = new MythAVFormatBuffer(m_buffer);
    auto *url      = reinterpret_cast<URLContext*>(m_ctx->pb->opaque);
    url->prot      = MythAVFormatBuffer::GetURLProtocol();
    url->priv_data = static_cast<void*>(m_avfBuffer);

    if (avformat_write_header(m_ctx, nullptr) < 0)
    {
        Cleanup();
        return false;
    }

    return true;
}

void MythAVFormatWriter::Cleanup(void)
{
    if (m_ctx && m_ctx->pb)
        avio_closep(&m_ctx->pb);
    delete m_avfBuffer;
    m_avfBuffer = nullptr;
    delete m_buffer;
    m_buffer = nullptr;
}

bool MythAVFormatWriter::CloseFile(void)
{
    if (m_ctx)
    {
        av_write_trailer(m_ctx);
        avio_close(m_ctx->pb);
        for(uint i = 0; i < m_ctx->nb_streams; i++)
            av_freep(&m_ctx->streams[i]);
        av_freep(&m_ctx);
    }
    return true;
}

bool MythAVFormatWriter::NextFrameIsKeyFrame(void)
{
    return (m_bufferedVideoFrameTypes.isEmpty()) ||
           (m_bufferedVideoFrameTypes.first() == AV_PICTURE_TYPE_I);
}

int MythAVFormatWriter::WriteVideoFrame(MythVideoFrame *Frame)
{
    long long framesEncoded = m_framesWritten + m_bufferedVideoFrameTimes.size();

    av_frame_unref(m_picture);
    MythAVUtil::FillAVFrame(m_picture, Frame);
    m_picture->pts = framesEncoded + 1;
    m_picture->pict_type = ((framesEncoded % m_keyFrameDist) == 0) ? AV_PICTURE_TYPE_I : AV_PICTURE_TYPE_NONE;

    m_bufferedVideoFrameTimes.push_back(Frame->m_timecode);
    m_bufferedVideoFrameTypes.push_back(m_picture->pict_type);

    AVPacket *pkt = av_packet_alloc();
    if (pkt == nullptr)
    {
        LOG(VB_RECORD, LOG_ERR, "packet allocation failed");
        return AVERROR(ENOMEM);
    }

    int got_pkt = 0;
    AVCodecContext *avctx = m_codecMap.GetCodecContext(m_videoStream);
    int ret = avcodec_encode_video2(avctx, pkt, m_picture, &got_pkt);

    if (ret < 0)
    {
        LOG(VB_RECORD, LOG_ERR, "avcodec_encode_video2() failed");
        av_packet_free(&pkt);
        return ret;
    }

    if (!got_pkt)
    {
        av_packet_free(&pkt);
        return ret;
    }

    std::chrono::milliseconds tc = Frame->m_timecode;

    if (!m_bufferedVideoFrameTimes.isEmpty())
        tc = m_bufferedVideoFrameTimes.takeFirst();

    if (!m_bufferedVideoFrameTypes.isEmpty())
    {
        int pict_type = m_bufferedVideoFrameTypes.takeFirst();
        if (pict_type == AV_PICTURE_TYPE_I)
            pkt->flags |= AV_PKT_FLAG_KEY;
    }

    if (m_startingTimecodeOffset == -1ms)
        m_startingTimecodeOffset = tc - 1ms;
    tc -= m_startingTimecodeOffset;

    pkt->pts = tc.count() * m_videoStream->time_base.den / m_videoStream->time_base.num / 1000;
    pkt->dts = AV_NOPTS_VALUE;
    pkt->stream_index= m_videoStream->index;

    ret = av_interleaved_write_frame(m_ctx, pkt);
    if (ret != 0)
        LOG(VB_RECORD, LOG_ERR, LOC + "WriteVideoFrame(): av_interleaved_write_frame couldn't write Video");

    Frame->m_timecode = tc + m_startingTimecodeOffset;
    m_framesWritten++;
    av_packet_unref(pkt);
    av_packet_free(&pkt);
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

int MythAVFormatWriter::WriteAudioFrame(unsigned char *Buffer, int /*FrameNumber*/, std::chrono::milliseconds &Timecode)
{
#if HAVE_BIGENDIAN
    bswap_16_buf((short int*) buf, m_audioFrameSize, m_audioChannels);
#endif

    AVCodecContext *avctx   = m_codecMap.GetCodecContext(m_audioStream);
    int samples_per_avframe = m_audioFrameSize * m_audioChannels;
    int sampleSizeIn        = AudioOutputSettings::SampleSize(FORMAT_S16);
    AudioFormat format      = AudioOutputSettings::AVSampleFormatToFormat(avctx->sample_fmt);
    int sampleSizeOut       = AudioOutputSettings::SampleSize(format);

    AVPacket *pkt = av_packet_alloc();
    if (pkt == nullptr)
    {
        LOG(VB_RECORD, LOG_ERR, "packet allocation failed");
        return AVERROR(ENOMEM);
    }

    if (av_get_packed_sample_fmt(avctx->sample_fmt) == AV_SAMPLE_FMT_FLT)
    {
        AudioOutputUtil::toFloat(FORMAT_S16, static_cast<void*>(m_audioInBuf),
                                 static_cast<void*>(Buffer), samples_per_avframe * sampleSizeIn);
        Buffer = m_audioInBuf;
    }
    if (av_sample_fmt_is_planar(avctx->sample_fmt))
    {
        AudioOutputUtil::DeinterleaveSamples(format, m_audioChannels,
                                             m_audioInPBuf, Buffer,
                                             samples_per_avframe * sampleSizeOut);

        // init AVFrame for planar data (input is interleaved)
        for (int j = 0, jj = 0; j < m_audioChannels; j++, jj += m_audioFrameSize)
            m_audPicture->data[j] = m_audioInPBuf + jj * sampleSizeOut;
    }
    else
    {
        m_audPicture->data[0] = Buffer;
    }

    m_audPicture->linesize[0]   = m_audioFrameSize;
    m_audPicture->nb_samples    = m_audioFrameSize;
    m_audPicture->format        = avctx->sample_fmt;
    m_audPicture->extended_data = m_audPicture->data;
    m_bufferedAudioFrameTimes.push_back(Timecode);

    //  SUGGESTION
    //  Now that avcodec_encode_audio2 is deprecated and replaced
    //  by 2 calls, this could be optimized
    //  into separate routines or separate threads.
    bool got_packet = false;
    int ret = avcodec_receive_packet(avctx, pkt);
    if (ret == 0)
        got_packet = true;
    if (ret == AVERROR(EAGAIN))
        ret = 0;
    if (ret == 0)
        ret = avcodec_send_frame(avctx, m_audPicture);
    // if ret from avcodec_send_frame is AVERROR(EAGAIN) then
    // there are 2 packets to be received while only 1 frame to be
    // sent. The code does not cater for this. Hopefully it will not happen.

    if (ret < 0)
    {
        std::string error;
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("audio encode error: %1 (%2)")
            .arg(av_make_error_stdstring(error, ret)).arg(got_packet));
        av_packet_free(&pkt);
        return ret;
    }

    if (!got_packet)
    {
        av_packet_free(&pkt);
        return ret;
    }

    std::chrono::milliseconds tc = Timecode;

    if (!m_bufferedAudioFrameTimes.empty())
        tc = m_bufferedAudioFrameTimes.takeFirst();

    if (m_startingTimecodeOffset == -1ms)
        m_startingTimecodeOffset = tc - 1ms;
    tc -= m_startingTimecodeOffset;

    if (m_avVideoCodec)
        pkt->pts = tc.count() * m_videoStream->time_base.den / m_videoStream->time_base.num / 1000;
    else
        pkt->pts = tc.count() * m_audioStream->time_base.den / m_audioStream->time_base.num / 1000;

    pkt->dts = AV_NOPTS_VALUE;
    pkt->flags |= AV_PKT_FLAG_KEY;
    pkt->stream_index = m_audioStream->index;

    ret = av_interleaved_write_frame(m_ctx, pkt);
    if (ret != 0)
        LOG(VB_RECORD, LOG_ERR, LOC + "WriteAudioFrame(): av_interleaved_write_frame couldn't write Audio");

    Timecode = tc + m_startingTimecodeOffset;
    av_packet_unref(pkt);
    av_packet_free(&pkt);
    return 1;
}

int MythAVFormatWriter::WriteTextFrame(int /*VBIMode*/, unsigned char* /*Buffer*/, int /*Length*/,
                                       std::chrono::milliseconds /*Timecode*/, int /*PageNumber*/)
{
    return 1;
}

int MythAVFormatWriter::WriteSeekTable(void)
{
    return 1;
}

bool MythAVFormatWriter::SwitchToNextFile(void)
{
    return false;
}

bool MythAVFormatWriter::ReOpen(const QString& Filename)
{
    bool result = m_buffer->ReOpen(Filename);
    if (result)
        m_filename = Filename;
    return result;
}

AVStream* MythAVFormatWriter::AddVideoStream(void)
{
    AVStream *stream = avformat_new_stream(m_ctx, nullptr);
    if (!stream)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "AddVideoStream(): avformat_new_stream() failed");
        return nullptr;
    }

    stream->id               = 0;
    stream->time_base.den    = 90000;
    stream->time_base.num    = 1;
    stream->r_frame_rate.num = 0;
    stream->r_frame_rate.den = 0;

    AVCodec *codec = avcodec_find_encoder(m_ctx->oformat->video_codec);
    if (!codec)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "AddVideoStream(): avcodec_find_encoder() failed");
        return nullptr;
    }

    m_codecMap.FreeCodecContext(stream);
    AVCodecContext *context  = m_codecMap.GetCodecContext(stream, codec);
    context->codec           = codec;
    context->codec_id        = m_ctx->oformat->video_codec;
    context->codec_type      = AVMEDIA_TYPE_VIDEO;
    context->bit_rate        = m_videoBitrate;
    context->width           = m_width;
    context->height          = m_height;

    // c->sample_aspect_ratio.num = (int)floor(m_aspect * 10000);
    // c->sample_aspect_ratio.den = 10000;

    context->time_base       = GetCodecTimeBase();
    context->gop_size        = m_keyFrameDist;
    context->pix_fmt         = AV_PIX_FMT_YUV420P;
    context->thread_count    = m_encodingThreadCount;
    context->thread_type     = FF_THREAD_SLICE;

    if (context->codec_id == AV_CODEC_ID_MPEG2VIDEO)
    {
        context->max_b_frames = 2;
    }
    else if (context->codec_id == AV_CODEC_ID_MPEG1VIDEO)
    {
        context->mb_decision = 2;
    }
    else if (context->codec_id == AV_CODEC_ID_H264)
    {

        // Try to provide the widest software/device support by automatically using
        // the Baseline profile where the given bitrate and resolution permits

        if ((context->height > 720) || // Approximate highest resolution supported by Baseline 3.1
            (context->bit_rate > 1000000)) // 14,000 Kbps aka 14Mbps maximum permissable rate for Baseline 3.1
        {
            context->level = 40;
            // AVCodecContext AVOptions:
            av_opt_set(context->priv_data, "profile", "main", 0);
        }
        else if ((context->height > 576) || // Approximate highest resolution supported by Baseline 3.0
            (context->bit_rate > 1000000))  // 10,000 Kbps aka 10Mbps maximum permissable rate for Baseline 3.0
        {
            context->level = 31;
            // AVCodecContext AVOptions:
            av_opt_set(context->priv_data, "profile", "baseline", 0);
        }
        else
        {
            context->level = 30; // Baseline 3.0 is the most widely supported, but it's limited to SD
            // AVCodecContext AVOptions:
            av_opt_set(context->priv_data, "profile", "baseline", 0);
        }

        // AVCodecContext AVOptions:
        // c->coder_type            = 0;
        av_opt_set_int(context, "coder", FF_CODER_TYPE_VLC, 0);
        context->max_b_frames          = 0;
        context->slices                = 8;
        context->flags                |= AV_CODEC_FLAG_LOOP_FILTER;
        context->me_cmp               |= 1;
        // c->me_method             = ME_HEX;
        // av_opt_set_int(c, "me_method", ME_HEX, 0);
        av_opt_set(context, "me_method", "hex", 0);
        context->me_subpel_quality     = 6;
        context->me_range              = 16;
        context->keyint_min            = 25;
        // c->scenechange_threshold = 40;
        av_opt_set_int(context, "sc_threshold", 40, 0);
        context->i_quant_factor        = 0.71F;
        // c->b_frame_strategy      = 1;
        av_opt_set_int(context, "b_strategy", 1, 0);
        context->qcompress             = 0.6F;
        context->qmin                  = 10;
        context->qmax                  = 51;
        context->max_qdiff             = 4;
        context->refs                  = 3;
        context->trellis               = 0;

        // libx264 AVOptions:
        av_opt_set(context, "partitions", "i8x8,i4x4,p8x8,b8x8", 0);
        av_opt_set_int(context, "direct-pred", 1, 0);
        av_opt_set_int(context, "rc-lookahead", 0, 0);
        av_opt_set_int(context, "fast-pskip", 1, 0);
        av_opt_set_int(context, "mixed-refs", 1, 0);
        av_opt_set_int(context, "8x8dct", 0, 0);
        av_opt_set_int(context, "weightb", 0, 0);

        // libx264 AVOptions:
        av_opt_set(context->priv_data, "preset", m_encodingPreset.toLatin1().constData(), 0);
        av_opt_set(context->priv_data, "tune", m_encodingTune.toLatin1().constData(), 0);
    }

    if(m_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    return stream;
}

bool MythAVFormatWriter::OpenVideo(void)
{
    if (!m_width || !m_height)
        return false;

    AVCodecContext *context = m_codecMap.GetCodecContext(m_videoStream);
    if (avcodec_open2(context, nullptr, nullptr) < 0)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "OpenVideo(): avcodec_open() failed");
        return false;
    }

    if (!m_picture)
    {
        m_picture = AllocPicture(context->pix_fmt);
        if (!m_picture)
        {
            LOG(VB_RECORD, LOG_ERR, LOC + "OpenVideo(): AllocPicture() failed");
            return false;
        }
    }
    else
    {
        av_frame_unref(m_picture);
    }

    return true;
}

AVStream* MythAVFormatWriter::AddAudioStream(void)
{
    AVStream *stream = avformat_new_stream(m_ctx, nullptr);
    if (!stream)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "AddAudioStream(): avformat_new_stream() failed");
        return nullptr;
    }
    stream->id = 1;

    AVCodecContext *context = m_codecMap.GetCodecContext(stream, nullptr, true);

    context->codec_id     = m_ctx->oformat->audio_codec;
    context->codec_type   = AVMEDIA_TYPE_AUDIO;
    context->bit_rate     = m_audioBitrate;
    context->sample_rate  = m_audioFrameRate;
    context->channels     = m_audioChannels;
    context->channel_layout = static_cast<uint64_t>(av_get_default_channel_layout(m_audioChannels));

    // c->flags |= CODEC_FLAG_QSCALE; // VBR
    // c->global_quality = blah;

    if (!m_avVideoCodec)
    {
        context->time_base    = GetCodecTimeBase();
        stream->time_base.den = 90000;
        stream->time_base.num = 1;
    }

    if (m_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    return stream;
}

bool MythAVFormatWriter::FindAudioFormat(AVCodecContext *Ctx, AVCodec *Codec, AVSampleFormat Format)
{
    if (Codec->sample_fmts)
    {
        for (int i = 0; Codec->sample_fmts[i] != AV_SAMPLE_FMT_NONE; i++)
        {
            if (av_get_packed_sample_fmt(Codec->sample_fmts[i]) == Format)
            {
                Ctx->sample_fmt = Codec->sample_fmts[i];
                return true;
            }
        }
    }
    return false;
}

bool MythAVFormatWriter::OpenAudio(void)
{
    AVCodecContext *context = m_codecMap.GetCodecContext(m_audioStream);
    context->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

    AVCodec *codec = avcodec_find_encoder(context->codec_id);
    if (!codec)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "OpenAudio(): avcodec_find_encoder() failed");
        return false;
    }

    // try to find suitable format we can use. avcodec_open2 will fail if we don't
    // find one, so no need to worry otherwise. Can only handle S16 or FLOAT
    // we give priority to S16 as libmp3lame requires aligned floats which we can't guarantee
    if (!FindAudioFormat(context, codec, AV_SAMPLE_FMT_S16))
        FindAudioFormat(context, codec, AV_SAMPLE_FMT_FLT);

    if (avcodec_open2(context, codec, nullptr) < 0)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "OpenAudio(): avcodec_open() failed");
        return false;
    }

    m_audioFrameSize = context->frame_size; // number of *samples* per channel in an AVFrame

    m_audPicture = av_frame_alloc();
    if (!m_audPicture)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "OpenAudio(): alloc_frame() failed");
        return false;
    }

    auto size = static_cast<size_t>(m_audioFrameSize * m_audioChannels *
                                    av_get_bytes_per_sample(context->sample_fmt));
    if (av_get_packed_sample_fmt(context->sample_fmt) == AV_SAMPLE_FMT_FLT)
    {
        // allocate buffer to convert from S16 to float
        if (!(m_audioInBuf = static_cast<unsigned char*>(av_malloc(size))))
            return false;
    }

    if (av_sample_fmt_is_planar(context->sample_fmt))
    {
        // allocate buffer to convert interleaved to planar audio
        if (!(m_audioInPBuf = static_cast<unsigned char*>(av_malloc(size))))
            return false;
    }
    return true;
}

AVFrame* MythAVFormatWriter::AllocPicture(enum AVPixelFormat PixFmt)
{
    AVFrame *picture = av_frame_alloc();
    if (!picture)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "AllocPicture failed");
        return nullptr;
    }

    int size = av_image_get_buffer_size(PixFmt, m_width, m_height, IMAGE_ALIGN);
    auto *buffer = static_cast<unsigned char*>(av_malloc(static_cast<size_t>(size)));
    if (!buffer)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "AllocPicture(): av_malloc() failed");
        av_frame_free(&picture);
        return nullptr;
    }

    av_image_fill_arrays(picture->data, picture->linesize, buffer, PixFmt, m_width, m_height, IMAGE_ALIGN);
    return picture;
}

AVRational MythAVFormatWriter::GetCodecTimeBase(void)
{
    AVRational result;
    result.den = static_cast<int>(floor(m_frameRate * 100));
    result.num = 100;

    if (m_avVideoCodec && m_avVideoCodec->supported_framerates)
    {
        const AVRational *rates = m_avVideoCodec->supported_framerates;
        AVRational req = { result.den, result.num };
        const AVRational *best = nullptr;
        AVRational best_error= { INT_MAX, 1 };
        for (; rates->den != 0; rates++)
        {
            AVRational error = av_sub_q(req, *rates);
            if (error.num < 0)
                error.num *= -1;
            if (av_cmp_q(error, best_error) < 0)
            {
                best_error = error;
                best = rates;
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
