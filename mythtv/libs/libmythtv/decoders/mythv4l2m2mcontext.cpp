// Qt
#include <QDir>

// MythTV
#include "mythlogging.h"
#include "v4l2util.h"
#include "fourcc.h"
#include "avformatdecoder.h"
#include "mythdrmprimeinterop.h"
#include "mythv4l2m2mcontext.h"

// Sys
#include <sys/ioctl.h>

// FFmpeg
extern "C" {
#include "libavutil/opt.h"
}

#define LOC QString("V4L2_M2M: ")

MythV4L2M2MContext::MythV4L2M2MContext(DecoderBase *Parent, MythCodecID CodecID)
  : MythCodecContext(Parent, CodecID)
{
}

MythV4L2M2MContext::~MythV4L2M2MContext()
{
    if (m_interop)
        m_interop->DecrRef();
}

inline uint32_t V4L2CodecType(AVCodecID Id)
{
    switch (Id)
    {
        case AV_CODEC_ID_MPEG1VIDEO: return V4L2_PIX_FMT_MPEG1;
        case AV_CODEC_ID_MPEG2VIDEO: return V4L2_PIX_FMT_MPEG2;
        case AV_CODEC_ID_MPEG4:      return V4L2_PIX_FMT_MPEG4;
        case AV_CODEC_ID_H263:       return V4L2_PIX_FMT_H263;
        case AV_CODEC_ID_H264:       return V4L2_PIX_FMT_H264;
        case AV_CODEC_ID_VC1:        return V4L2_PIX_FMT_VC1_ANNEX_G;
        case AV_CODEC_ID_VP8:        return V4L2_PIX_FMT_VP8;
        case AV_CODEC_ID_VP9:        return V4L2_PIX_FMT_VP9;
        case AV_CODEC_ID_HEVC:       return V4L2_PIX_FMT_HEVC;
        default: break;
    }
    return 0;
}
MythCodecID MythV4L2M2MContext::GetSupportedCodec(AVCodecContext **Context,
                                                  AVCodec **Codec,
                                                  const QString &Decoder,
                                                  AVStream *Stream,
                                                  uint StreamType)
{
    bool decodeonly = Decoder == "v4l2-dec";
    MythCodecID success = static_cast<MythCodecID>((decodeonly ? kCodec_MPEG1_V4L2_DEC : kCodec_MPEG1_V4L2) + (StreamType - 1));
    MythCodecID failure = static_cast<MythCodecID>(kCodec_MPEG1 + (StreamType - 1));

    // not us
    if (!Decoder.startsWith("v4l2"))
        return failure;

    // unknown codec
    uint32_t v4l2_fmt = V4L2CodecType((*Codec)->id);
    if (!v4l2_fmt)
        return failure;

    // supported by device driver?
    if (!HaveV4L2Codecs((*Codec)->id))
        return failure;

    // look for a decoder
    QString name = QString((*Codec)->name) + "_v4l2m2m";
    if (name == "mpeg2video_v4l2m2m")
        name = "mpeg2_v4l2m2m";
    AVCodec *codec = avcodec_find_decoder_by_name(name.toLocal8Bit());
    if (!codec)
    {
        // this shouldn't happen!
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to find %1").arg(name));
        return failure;
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Found V4L2/FFmpeg decoder '%1'").arg(name));
    *Codec = codec;
    gCodecMap->freeCodecContext(Stream);
    *Context = gCodecMap->getCodecContext(Stream, *Codec);
    (*Context)->pix_fmt = decodeonly ? (*Context)->pix_fmt : AV_PIX_FMT_DRM_PRIME;
    return success;
}

int MythV4L2M2MContext::HwDecoderInit(AVCodecContext *Context)
{
    if (!Context)
        return -1;

    if (codec_is_v4l2_dec(m_codecID))
        return 0;

    if (!codec_is_v4l2(m_codecID) || Context->pix_fmt != AV_PIX_FMT_DRM_PRIME)
        return -1;

    MythRenderOpenGL *context = MythRenderOpenGL::GetOpenGLRender();
    m_interop = MythDRMPRIMEInterop::Create(context, MythOpenGLInterop::DRMPRIME);
    return m_interop ? 0 : -1;
}

void MythV4L2M2MContext::InitVideoCodec(AVCodecContext *Context, bool SelectedStream, bool &DirectRendering)
{
    if (codec_is_v4l2(m_codecID))
    {
        DirectRendering = false;
        Context->get_format = MythV4L2M2MContext::GetFormat;
        return;
    }
    if (codec_is_v4l2_dec(m_codecID))
    {
        DirectRendering = false;
        return;
    }

    MythCodecContext::InitVideoCodec(Context, SelectedStream, DirectRendering);
}

bool MythV4L2M2MContext::RetrieveFrame(AVCodecContext *Context, VideoFrame *Frame, AVFrame *AvFrame)
{
    if (codec_is_v4l2(m_codecID))
        return GetDRMBuffer(Context, Frame, AvFrame, 0);
    else if (codec_is_v4l2_dec(m_codecID))
        return GetBuffer(Context, Frame, AvFrame, 0);
    return false;
}

/*! \brief Reduce the number of capture buffers
 *
 * Testing on Pi 3, the default of 20 is too high and leads to memory allocation
 * failures in the the kernel driver.
*/
void MythV4L2M2MContext::SetDecoderOptions(AVCodecContext* Context, AVCodec* Codec)
{
    if (!(Context && Codec))
        return;
    if (!(Codec->priv_class && Context->priv_data))
        return;

    // best guess currently - this matches the number of capture buffers to the
    // number of output buffers - and hence to the number of video buffers for
    // direct rendering
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Setting number of capture buffers to 6");
    av_opt_set_int(Context->priv_data, "num_capture_buffers", 6, 0);
}

/*! \brief Retrieve a frame from CPU memory
 *
 * This is similar to the default, direct render supporting, get_av_buffer in
 * AvFormatDecoder but we copy the data from the AVFrame rather than providing
 * our own buffer (the codec does not support direct rendering).
*/
bool MythV4L2M2MContext::GetBuffer(AVCodecContext *Context, VideoFrame *Frame, AVFrame *AvFrame, int)
{
    // Sanity checks
    if (!Context || !AvFrame || !Frame)
        return false;

    // Ensure we can render this format
    AvFormatDecoder *decoder = static_cast<AvFormatDecoder*>(Context->opaque);
    VideoFrameType type = PixelFormatToFrameType(static_cast<AVPixelFormat>(AvFrame->format));
    VideoFrameType* supported = decoder->GetPlayer()->DirectRenderFormats();
    bool found = false;
    while (*supported != FMT_NONE)
    {
        if (*supported == type)
        {
            found = true;
            break;
        }
        supported++;
    }

    // No fallback currently (unlikely)
    if (!found)
        return false;

    // Re-allocate if necessary
    if ((Frame->codec != type) || (Frame->width != AvFrame->width) || (Frame->height != AvFrame->height))
        if (!VideoBuffers::ReinitBuffer(Frame, type, decoder->GetVideoCodecID(), AvFrame->width, AvFrame->height))
            return false;

    // Copy data
    uint count = planes(Frame->codec);
    for (uint plane = 0; plane < count; ++plane)
        copyplane(Frame->buf + Frame->offsets[plane], Frame->pitches[plane], AvFrame->data[plane], AvFrame->linesize[plane],
                  pitch_for_plane(Frame->codec, AvFrame->width, plane), height_for_plane(Frame->codec, AvFrame->height, plane));

    return true;
}

bool MythV4L2M2MContext::GetDRMBuffer(AVCodecContext *Context, VideoFrame *Frame, AVFrame *AvFrame, int)
{
    if (!Context || !AvFrame || !Frame)
        return false;

    if (Frame->codec != FMT_DRMPRIME || static_cast<AVPixelFormat>(AvFrame->format) != AV_PIX_FMT_DRM_PRIME)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Not a DRM PRIME buffer");
        return false;
    }

    Frame->width = AvFrame->width;
    Frame->height = AvFrame->height;
    Frame->pix_fmt = Context->pix_fmt;
    Frame->sw_pix_fmt = Context->sw_pix_fmt;
    Frame->directrendering = 1;
    AvFrame->opaque = Frame;
    AvFrame->reordered_opaque = Context->reordered_opaque;

    // Frame->data[0] holds AVDRMFrameDescriptor
    Frame->buf = AvFrame->data[0];
    // Retain the buffer so it is not released before we display it
    Frame->priv[0] = reinterpret_cast<unsigned char*>(av_buffer_ref(AvFrame->buf[0]));
    // Add interop
    Frame->priv[1] = reinterpret_cast<unsigned char*>(m_interop);
    // Set the release method
    AvFrame->buf[1] = av_buffer_create(reinterpret_cast<uint8_t*>(Frame), 0, MythCodecContext::ReleaseBuffer,
                                       static_cast<AvFormatDecoder*>(Context->opaque), 0);
    return true;
}

AVPixelFormat MythV4L2M2MContext::GetFormat(AVCodecContext*, const AVPixelFormat *PixFmt)
{
    while (*PixFmt != AV_PIX_FMT_NONE)
    {
        if (*PixFmt == AV_PIX_FMT_DRM_PRIME)
            return AV_PIX_FMT_DRM_PRIME;
        PixFmt++;
    }
    return AV_PIX_FMT_NONE;
}

bool MythV4L2M2MContext::HaveV4L2Codecs(AVCodecID Codec /* = AV_CODEC_ID_NONE */)
{
    static QVector<AVCodecID> s_avcodecs({AV_CODEC_ID_MPEG1VIDEO, AV_CODEC_ID_MPEG2VIDEO,
                                          AV_CODEC_ID_MPEG4,      AV_CODEC_ID_H263,
                                          AV_CODEC_ID_H264,       AV_CODEC_ID_VC1,
                                          AV_CODEC_ID_VP8,        AV_CODEC_ID_VP9,
                                          AV_CODEC_ID_HEVC});

    static QMutex s_lock(QMutex::Recursive);
    static bool s_needscheck = true;
    static QVector<AVCodecID> s_supportedV4L2Codecs;

    QMutexLocker locker(&s_lock);

    if (s_needscheck)
    {
        s_needscheck = false;
        s_supportedV4L2Codecs.clear();

        // Iterate over /dev/videoXX and check for generic codecs support
        // We don't care what device is used or actually try a given format (which
        // would require width/height etc) - but simply check for capture and output
        // support for the given codecs, whether multiplanar or not.
        const QString root("/dev/");
        QDir dir(root);
        QStringList namefilters;
        namefilters.append("video*");
        QStringList devices = dir.entryList(namefilters, QDir::Files |QDir::System);
        foreach (QString device, devices)
        {
            V4L2util v4l2dev(root + device);
            uint32_t caps = v4l2dev.GetCapabilities();
            LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Device: %1 Driver: '%2' Capabilities: 0x%3")
                .arg(v4l2dev.GetDeviceName()).arg(v4l2dev.GetDriverName()).arg(caps, 0, 16));

            // check capture and output support
            // these mimic the device checks in v4l2_m2m.c
            bool mplanar = (caps & (V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_VIDEO_OUTPUT_MPLANE) &&
                           caps & V4L2_CAP_STREAMING);
            bool mplanarm2m = caps & V4L2_CAP_VIDEO_M2M_MPLANE;
            bool splanar = (caps & (V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_VIDEO_OUTPUT) &&
                            caps & V4L2_CAP_STREAMING);
            bool splanarm2m = caps & V4L2_CAP_VIDEO_M2M;

            if (!(mplanar || mplanarm2m || splanar || splanarm2m))
                continue;

            v4l2_buf_type capturetype = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            v4l2_buf_type outputtype = V4L2_BUF_TYPE_VIDEO_OUTPUT;

            if (mplanar || mplanarm2m)
            {
                capturetype = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
                outputtype = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
            }

            // check codec support
            QStringList debug;
            foreach (AVCodecID codec, s_avcodecs)
            {
                bool found = false;
                uint32_t v4l2pixfmt = V4L2CodecType(codec);
                struct v4l2_fmtdesc fdesc;
                memset(&fdesc, 0, sizeof(fdesc));

                // check output first
                fdesc.type = outputtype;
                while (!found)
                {
                    int res = ioctl(v4l2dev.FD(), VIDIOC_ENUM_FMT, &fdesc);
                    if (res)
                        break;
                    if (fdesc.pixelformat == v4l2pixfmt)
                        found = true;
                    fdesc.index++;
                }

                if (found)
                {
                    QStringList pixformats;
                    bool foundfmt = false;
                    // check capture
                    memset(&fdesc, 0, sizeof(fdesc));
                    fdesc.type = capturetype;
                    while (true)
                    {
                        int res = ioctl(v4l2dev.FD(), VIDIOC_ENUM_FMT, &fdesc);
                        if (res)
                            break;
                        pixformats.append(fourcc_str(static_cast<int>(fdesc.pixelformat)));

                        // this is a bit of a shortcut
                        if (fdesc.pixelformat == V4L2_PIX_FMT_YUV420 ||
                            fdesc.pixelformat == V4L2_PIX_FMT_YVU420 ||
                            fdesc.pixelformat == V4L2_PIX_FMT_YUV420M ||
                            fdesc.pixelformat == V4L2_PIX_FMT_YVU420M ||
                            fdesc.pixelformat == V4L2_PIX_FMT_NV12   ||
                            fdesc.pixelformat == V4L2_PIX_FMT_NV12M  ||
                            fdesc.pixelformat == V4L2_PIX_FMT_NV21   ||
                            fdesc.pixelformat == V4L2_PIX_FMT_NV21M)
                        {
                            if (!s_supportedV4L2Codecs.contains(codec))
                                s_supportedV4L2Codecs.append(codec);
                            debug.append(avcodec_get_name(codec));
                            foundfmt = true;
                            break;
                        }
                        fdesc.index++;
                    }

                    if (!foundfmt)
                    {
                        if (pixformats.isEmpty())
                            pixformats.append("None");
                        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Codec '%1' has no supported formats (Supported: %2)")
                            .arg(codec).arg(pixformats.join((","))));
                    }
                }
            }
            if (debug.isEmpty())
                debug.append("None");
            LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Device: %1 Supported codecs: '%2'")
                .arg(v4l2dev.GetDeviceName()).arg(debug.join(",")));
        }
        QStringList gdebug;
        foreach (AVCodecID codec, s_supportedV4L2Codecs)
            gdebug.append(avcodec_get_name(codec));
        if (gdebug.isEmpty())
            gdebug.append("None");
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("V4L2 codecs supported: %1").arg(gdebug.join(",")));
    }

    if (!Codec)
        return !s_supportedV4L2Codecs.isEmpty();
    return s_supportedV4L2Codecs.contains(Codec);
}
