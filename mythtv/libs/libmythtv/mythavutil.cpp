//
//  mythavutil.cpp
//  MythTV
//
//  Created by Jean-Yves Avenard on 28/06/2014.
//  Copyright (c) 2014 Bubblestuff Pty Ltd. All rights reserved.
//

// Qt
#include <QtGlobal>
#include <QMutexLocker>
#include <QFile>

// MythTV
#include "libmyth/mythaverror.h"
#include "libmythbase/mythconfig.h"
#include "libmythbase/mythlogging.h"
#include "mythdeinterlacer.h"
#include "mythavutil.h"

// FFmpeg
extern "C" {
#include "libavcodec/avcodec.h"
#include "libavutil/imgutils.h"
#include "libavformat/avformat.h"
}

AVPixelFormat MythAVUtil::FrameTypeToPixelFormat(VideoFrameType Type)
{
    switch (Type)
    {
        case FMT_YV12:       return AV_PIX_FMT_YUV420P;
        case FMT_YUV420P9:   return AV_PIX_FMT_YUV420P9;
        case FMT_YUV420P10:  return AV_PIX_FMT_YUV420P10;
        case FMT_YUV420P12:  return AV_PIX_FMT_YUV420P12;
        case FMT_YUV420P14:  return AV_PIX_FMT_YUV420P14;
        case FMT_YUV420P16:  return AV_PIX_FMT_YUV420P16;
        case FMT_NV12:       return AV_PIX_FMT_NV12;
        case FMT_P010:       return AV_PIX_FMT_P010;
        case FMT_P016:       return AV_PIX_FMT_P016;
        case FMT_YUV422P:    return AV_PIX_FMT_YUV422P;
        case FMT_YUV422P9:   return AV_PIX_FMT_YUV422P9;
        case FMT_YUV422P10:  return AV_PIX_FMT_YUV422P10;
        case FMT_YUV422P12:  return AV_PIX_FMT_YUV422P12;
        case FMT_YUV422P14:  return AV_PIX_FMT_YUV422P14;
        case FMT_YUV422P16:  return AV_PIX_FMT_YUV422P16;
        case FMT_YUV444P:    return AV_PIX_FMT_YUV444P;
        case FMT_YUV444P9:   return AV_PIX_FMT_YUV444P9;
        case FMT_YUV444P10:  return AV_PIX_FMT_YUV444P10;
        case FMT_YUV444P12:  return AV_PIX_FMT_YUV444P12;
        case FMT_YUV444P14:  return AV_PIX_FMT_YUV444P14;
        case FMT_YUV444P16:  return AV_PIX_FMT_YUV444P16;
        case FMT_RGB24:      return AV_PIX_FMT_RGB24;
        case FMT_BGRA:       return AV_PIX_FMT_BGRA; // NOLINT(bugprone-branch-clone)
        case FMT_RGB32:      return AV_PIX_FMT_RGB32;
        case FMT_ARGB32:     return AV_PIX_FMT_ARGB;
        case FMT_RGBA32:     return AV_PIX_FMT_RGBA;
        case FMT_YUY2:       return AV_PIX_FMT_UYVY422;
        case FMT_VDPAU:      return AV_PIX_FMT_VDPAU;
        case FMT_VTB:        return AV_PIX_FMT_VIDEOTOOLBOX;
        case FMT_VAAPI:      return AV_PIX_FMT_VAAPI;
        case FMT_MEDIACODEC: return AV_PIX_FMT_MEDIACODEC;
        case FMT_NVDEC:      return AV_PIX_FMT_CUDA;
        case FMT_DXVA2:      return AV_PIX_FMT_DXVA2_VLD;
        case FMT_MMAL:       return AV_PIX_FMT_MMAL;
        case FMT_DRMPRIME:   return AV_PIX_FMT_DRM_PRIME;
        case FMT_NONE: break;
    }
    return AV_PIX_FMT_NONE;
}

VideoFrameType MythAVUtil::PixelFormatToFrameType(AVPixelFormat Fmt)
{
    switch (Fmt)
    {
        case AV_PIX_FMT_YUVJ420P:
        case AV_PIX_FMT_YUV420P:   return FMT_YV12;
        case AV_PIX_FMT_YUV420P9:  return FMT_YUV420P9;
        case AV_PIX_FMT_YUV420P10: return FMT_YUV420P10;
        case AV_PIX_FMT_YUV420P12: return FMT_YUV420P12;
        case AV_PIX_FMT_YUV420P14: return FMT_YUV420P14;
        case AV_PIX_FMT_YUV420P16: return FMT_YUV420P16;
        case AV_PIX_FMT_NV12:      return FMT_NV12;
        case AV_PIX_FMT_P010:      return FMT_P010;
        case AV_PIX_FMT_P016:      return FMT_P016;
        case AV_PIX_FMT_YUVJ422P:
        case AV_PIX_FMT_YUV422P:   return FMT_YUV422P;
        case AV_PIX_FMT_YUV422P9:  return FMT_YUV422P9;
        case AV_PIX_FMT_YUV422P10: return FMT_YUV422P10;
        case AV_PIX_FMT_YUV422P12: return FMT_YUV422P12;
        case AV_PIX_FMT_YUV422P14: return FMT_YUV422P14;
        case AV_PIX_FMT_YUV422P16: return FMT_YUV422P16;
        case AV_PIX_FMT_YUVJ444P:
        case AV_PIX_FMT_YUV444P:   return FMT_YUV444P;
        case AV_PIX_FMT_YUV444P9:  return FMT_YUV444P9;
        case AV_PIX_FMT_YUV444P10: return FMT_YUV444P10;
        case AV_PIX_FMT_YUV444P12: return FMT_YUV444P12;
        case AV_PIX_FMT_YUV444P14: return FMT_YUV444P14;
        case AV_PIX_FMT_YUV444P16: return FMT_YUV444P16;
        case AV_PIX_FMT_UYVY422:   return FMT_YUY2;
        case AV_PIX_FMT_RGB24:     return FMT_RGB24;
        case AV_PIX_FMT_ARGB:      return FMT_ARGB32;
        case AV_PIX_FMT_RGBA:      return FMT_RGBA32;
        case AV_PIX_FMT_BGRA:      return FMT_BGRA;
        case AV_PIX_FMT_CUDA:      return FMT_NVDEC;
        case AV_PIX_FMT_MMAL:      return FMT_MMAL;
        case AV_PIX_FMT_VDPAU:     return FMT_VDPAU;
        case AV_PIX_FMT_VIDEOTOOLBOX: return FMT_VTB;
        case AV_PIX_FMT_VAAPI:     return FMT_VAAPI;
        case AV_PIX_FMT_DXVA2_VLD: return FMT_DXVA2;
        case AV_PIX_FMT_MEDIACODEC: return FMT_MEDIACODEC;
        case AV_PIX_FMT_DRM_PRIME: return FMT_DRMPRIME;
        default: break;
    }
    return FMT_NONE;
}

MythHDR::HDRType MythAVUtil::FFmpegTransferToHDRType(int Transfer)
{
    switch (Transfer)
    {
        case AVCOL_TRC_SMPTE2084: return MythHDR::HDR10;
        case AVCOL_TRC_BT2020_10:
        case AVCOL_TRC_ARIB_STD_B67: return MythHDR::HLG;
        default: break;
    }
    return MythHDR::SDR;
}

/*! \brief Deinterlace an AVFrame
 *
 * This is only used by the mytharchive plugin and can be removed if MythArchive
 * becomes obsolete.
*/
void MythAVUtil::DeinterlaceAVFrame(AVFrame *Frame)
{
    if (!Frame)
        return;

    // Create a wrapper frame and set it up
    // (yes - this will end up being a wrapper around a wrapper!)
    if (VideoFrameType type = PixelFormatToFrameType(static_cast<AVPixelFormat>(Frame->format));
        MythVideoFrame::YUVFormat(type))
    {
        // AVFrame video data consists of 3 independent buffers.
        // MythVideoFrame requires video data to be in one memory block.
        // Create a copy of the frame with all video data in one memory block.
        AVFrame tempFrame = *Frame;
        AVFrame *tf = &tempFrame;

        size_t b0 = tf->buf[0]->size;
        size_t b1 = tf->buf[1]->size;
        size_t b2 = tf->buf[2]->size;

        // Extend size to a multiple of 16 bytes for MythVideoFrame alignment constraints
        size_t b0a = ((b0 + 15) / 16) * 16;
        size_t b1a = ((b1 + 15) / 16) * 16;
        size_t b2a = ((b2 + 15) / 16) * 16;

        // Allocate contiguous buffer with enough space for the three segments
        uint8_t *tbuf{ new uint8_t[b0a + b1a + b2a]{} };

        tf->data[0] = tbuf;
        tf->data[1] = tbuf + b0a;
        tf->data[2] = tbuf + b0a + b1a;
        memcpy(tf->data[0], tf->buf[0]->data, b0);
        memcpy(tf->data[1], tf->buf[1]->data, b1);
        memcpy(tf->data[2], tf->buf[2]->data, b2);

        // Create a MythVideoFrame from the temporary AVFrame
        MythVideoFrame mythframe(type, tf->data[0],
                                 MythVideoFrame::GetBufferSize(type, tf->width, tf->height),
                                 tf->width, tf->height);
        mythframe.m_offsets[0] = 0;
        mythframe.m_offsets[1] = tf->data[1] - tf->data[0];
        mythframe.m_offsets[2] = tf->data[2] - tf->data[0];
        mythframe.m_pitches[0] = tf->linesize[0];
        mythframe.m_pitches[1] = tf->linesize[1];
        mythframe.m_pitches[2] = tf->linesize[2];

        // Deinterlacing
        mythframe.m_deinterlaceSingle = DEINT_CPU | DEINT_MEDIUM;
        mythframe.m_deinterlaceAllowed = DEINT_ALL;
        MythDeinterlacer deinterlacer;
        deinterlacer.Filter(&mythframe, kScan_Interlaced, nullptr, true);

        // Copy back our deinterlaced frame and free the video buffer of the temporary AVFrame
        memcpy(Frame->data[0], tf->data[0], b0);
        memcpy(Frame->data[1], tf->data[1], b1);
        memcpy(Frame->data[2], tf->data[2], b2);
        delete[] tbuf;

        // Must remove buffer before mythframe is deleted
        mythframe.m_buffer = nullptr;
    }
}

/// \brief Initialise AVFrame with content from MythVideoFrame
int MythAVUtil::FillAVFrame(AVFrame *Frame, const MythVideoFrame *From, AVPixelFormat Fmt)
{
    if (Fmt == AV_PIX_FMT_NONE)
        Fmt = FrameTypeToPixelFormat(From->m_type);

    av_image_fill_arrays(Frame->data, Frame->linesize, From->m_buffer,
        Fmt, From->m_width, From->m_height, IMAGE_ALIGN);
    Frame->data[1] = From->m_buffer + From->m_offsets[1];
    Frame->data[2] = From->m_buffer + From->m_offsets[2];
    Frame->linesize[0] = From->m_pitches[0];
    Frame->linesize[1] = From->m_pitches[1];
    Frame->linesize[2] = From->m_pitches[2];
    return static_cast<int>(MythVideoFrame::GetBufferSize(From->m_type, From->m_width, From->m_height));
}

/*! \class MythAVCopy
 * Copy AVFrame<->frame, performing the required conversion if any
 */
MythAVCopy::~MythAVCopy()
{
    sws_freeContext(m_swsctx);
}

int MythAVCopy::SizeData(int Width, int Height, AVPixelFormat Fmt)
{
    if (Width == m_width && Height == m_height && Fmt == m_format)
        return m_size;

    m_size    = av_image_get_buffer_size(Fmt, Width, Height, IMAGE_ALIGN);
    m_width   = Width;
    m_height  = Height;
    m_format  = Fmt;
    return m_size;
}

int MythAVCopy::Copy(AVFrame* To, AVPixelFormat ToFmt, const AVFrame* From, AVPixelFormat FromFmt,
                     int Width, int Height)
{
    int newwidth = Width;
#ifdef Q_PROCESSOR_ARM
    // references https://code.mythtv.org/trac/ticket/12888 and
    // https://trac.ffmpeg.org/ticket/6192
    // TODO: This is from 2017-02-17 and probably needs to be tested again.

    // The ARM build of FFMPEG has a bug that if sws_scale is
    // called with source and dest sizes the same, and
    // formats as shown below, it causes a bus error and the
    // application core dumps. To avoid this I make a -1
    // difference in the new width, causing it to bypass
    // the code optimization which is failing.
    if (FromFmt == AV_PIX_FMT_YUV420P && ToFmt == AV_PIX_FMT_BGRA)
        newwidth = Width - 1;
#endif
    m_swsctx = sws_getCachedContext(m_swsctx, Width, Height, FromFmt,
                                    newwidth, Height, ToFmt, SWS_FAST_BILINEAR,
                                    nullptr, nullptr, nullptr);
    if (m_swsctx == nullptr)
        return -1;
    sws_scale(m_swsctx, From->data, From->linesize, 0, Height, To->data, To->linesize);
    return SizeData(Width, Height, ToFmt);
}

/*! \brief Initialise AVFrame and copy contents of VideoFrame frame into it, performing
 * any required conversion.
 *
 * \returns Size of buffer allocated
 * \note AVFrame buffers must be deleted manually by the caller (av_freep(pic->data[0]))
 */
int MythAVCopy::Copy(AVFrame* To, const MythVideoFrame* From,
                     unsigned char* Buffer, AVPixelFormat Fmt)
{
    if (!Buffer)
        return 0;
    AVFrame frame;
    AVPixelFormat fromfmt = MythAVUtil::FrameTypeToPixelFormat(From->m_type);
    MythAVUtil::FillAVFrame(&frame, From, fromfmt);
    av_image_fill_arrays(To->data, To->linesize, Buffer, Fmt, From->m_width, From->m_height, IMAGE_ALIGN);
    return Copy(To, Fmt, &frame, fromfmt, From->m_width, From->m_height);
}

/*! \class MythCodecMap
 * Utility class that keeps pointers to an AVStream and its AVCodecContext. The
 * codec member of AVStream was previously used for this but is now deprecated.
*/
MythCodecMap::~MythCodecMap()
{
    FreeAllContexts();
}

AVCodecContext *MythCodecMap::GetCodecContext(const AVStream* Stream,
                                              const AVCodec* Codec,
                                              bool NullCodec)
{
    if (Stream == nullptr || Stream->codecpar == nullptr)
        return nullptr;
    QMutexLocker lock(&m_mapLock);
    AVCodecContext* avctx = m_streamMap.value(Stream, nullptr);
    if (avctx == nullptr)
    {
        if (NullCodec)
        {
            Codec = nullptr;
        }
        else if (Codec == nullptr)
        {
            Codec = avcodec_find_decoder(Stream->codecpar->codec_id);
            if (Codec == nullptr)
            {
                LOG(VB_GENERAL, LOG_WARNING, QString("avcodec_find_decoder fail for %1")
                    .arg(Stream->codecpar->codec_id));
                return nullptr;
            }
        }
        avctx = avcodec_alloc_context3(Codec);
        if (avctx != nullptr && avcodec_parameters_to_context(avctx, Stream->codecpar) < 0)
            avcodec_free_context(&avctx);

        if (avctx != nullptr)
        {
            avctx->pkt_timebase = Stream->time_base;
            m_streamMap.insert(Stream, avctx);
        }
    }
    return avctx;
}

AVCodecContext *MythCodecMap::FindCodecContext(const AVStream* Stream)
{
    return m_streamMap.value(Stream, nullptr);
}

/// \note This will not free a hardware or frames context that is in anyway referenced outside
/// of the decoder. Probably need to force the VideoOutput class to discard buffers
/// as well. Leaking hardware contexts is a very bad idea.
void MythCodecMap::FreeCodecContext(const AVStream* Stream)
{
    QMutexLocker lock(&m_mapLock);
    AVCodecContext* avctx = m_streamMap.take(Stream);
    if (avctx)
    {
        if (avctx->internal)
            avcodec_flush_buffers(avctx);
        avcodec_free_context(&avctx);
    }
}

void MythCodecMap::FreeAllContexts()
{
    QMutexLocker lock(&m_mapLock);
    QMap<const AVStream*, AVCodecContext*>::iterator i = m_streamMap.begin();
    while (i != m_streamMap.end())
    {
        const AVStream *stream = i.key();
        ++i;
        FreeCodecContext(stream);
    }
}

MythStreamInfoList::MythStreamInfoList(const QString& filename)
{
    const int probeBufferSize = 8 * 1024;
    const AVInputFormat *fmt      = nullptr;
    AVProbeData probe;
    memset(&probe, 0, sizeof(AVProbeData));
    probe.filename = "";
    probe.buf = new unsigned char[probeBufferSize + AVPROBE_PADDING_SIZE];
    probe.buf_size = probeBufferSize;
    memset(probe.buf, 0, probeBufferSize + AVPROBE_PADDING_SIZE);
    av_log_set_level(AV_LOG_FATAL);
    if (filename == "")
        m_errorCode = 97;
    QFile infile(filename);
    if (m_errorCode == 0 && !infile.open(QIODevice::ReadOnly))
        m_errorCode = 99;
    if (m_errorCode==0) {
        int64_t leng = infile.read(reinterpret_cast<char*>(probe.buf), probeBufferSize);
        probe.buf_size = static_cast<int>(leng);
        infile.close();
        fmt = av_probe_input_format(&probe, static_cast<int>(true));
        if (fmt == nullptr)
            m_errorCode = 98;
    }
    AVFormatContext *ctx = nullptr;
    if (m_errorCode==0)
    {
        ctx = avformat_alloc_context();
        m_errorCode = avformat_open_input(&ctx, filename.toUtf8(), fmt, nullptr);
    }
    if (m_errorCode==0)
        m_errorCode = avformat_find_stream_info(ctx, nullptr);

    if (m_errorCode==0)
    {
        for (uint ix = 0; ix < ctx->nb_streams; ix++)
        {
            AVStream *stream = ctx->streams[ix];
            if (stream == nullptr)
                continue;
            AVCodecParameters *codecpar = stream->codecpar;
            const AVCodecDescriptor* desc = nullptr;
            MythStreamInfo info;
            info.m_codecType = ' ';
            if (codecpar != nullptr)
            {
                desc = avcodec_descriptor_get(codecpar->codec_id);
                switch (codecpar->codec_type)
                {
                    case AVMEDIA_TYPE_VIDEO:
                        info.m_codecType = 'V';
                        break;
                    case AVMEDIA_TYPE_AUDIO:
                        info.m_codecType = 'A';
                        break;
                    case AVMEDIA_TYPE_SUBTITLE:
                        info.m_codecType = 'S';
                        break;
                   default:
                       continue;
                }
            }
            if (desc != nullptr)
                info.m_codecName = desc->name;
            info.m_duration  = stream->duration * stream->time_base.num / stream->time_base.den;
            if (info.m_codecType == 'V')
            {
                if (codecpar != nullptr)
                {
                    info.m_width  = codecpar->width;
                    info.m_height = codecpar->height;
                    info.m_SampleAspectRatio = static_cast<float>(codecpar->sample_aspect_ratio.num)
                        / static_cast<float>(codecpar->sample_aspect_ratio.den);
                    switch (codecpar->field_order)
                    {
                        case AV_FIELD_PROGRESSIVE:
                            info.m_fieldOrder = "PR";
                            break;
                        case AV_FIELD_TT:
                            info.m_fieldOrder = "TT";
                            break;
                        case AV_FIELD_BB:
                            info.m_fieldOrder = "BB";
                            break;
                        case AV_FIELD_TB:
                            info.m_fieldOrder = "TB";
                            break;
                        case AV_FIELD_BT:
                            info.m_fieldOrder = "BT";
                            break;
                        default:
                            break;
                    }
                }
                info.m_frameRate = static_cast<float>(stream->r_frame_rate.num)
                    / static_cast<float>(stream->r_frame_rate.den);
                info.m_avgFrameRate = static_cast<float>(stream->avg_frame_rate.num)
                    / static_cast<float>(stream->avg_frame_rate.den);
            }
            if (info.m_codecType == 'A')
                info.m_channels = codecpar->ch_layout.nb_channels;
            m_streamInfoList.append(info);
        }
    }
    if (m_errorCode != 0)
    {
        switch(m_errorCode) {
            case 97:
                m_errorMsg = "File Not Found";
                break;
            case 98:
                m_errorMsg = "av_probe_input_format returned no result";
                break;
            case 99:
                m_errorMsg = "File could not be opened";
                break;
            default:
                m_errorMsg = QString::fromStdString(av_make_error_stdstring_unknown(m_errorCode));
        }
        LOG(VB_GENERAL, LOG_ERR,
            QString("MythStreamInfoList failed for %1. Error code:%2 Message:%3")
            .arg(filename).arg(m_errorCode).arg(m_errorMsg));

    }

    if (ctx != nullptr)
    {
        avformat_close_input(&ctx);
        avformat_free_context(ctx);
    }
    delete [] probe.buf;
}
