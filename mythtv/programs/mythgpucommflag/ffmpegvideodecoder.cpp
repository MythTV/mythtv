#include "ffmpegvideodecoder.h"
#include "videoconsumer.h"
#include "frame.h"
#include "videosurface.h"
#include "mythlogging.h"

extern "C" {
#include "libavcodec/avcodec.h"
}


bool FFMpegVideoDecoder::Initialize(void)
{
    return true;
}

VideoSurface *FFMpegVideoDecoder::DecodeFrame(AVFrame *frame)
{
    VideoSurface *surface = (VideoSurface *)frame->opaque;
    cl_int ciErrNum;

    if (m_dev)
    {
        // This is heading to OpenCL
        size_t origin[3] = { 0, 0, 0 };
        size_t region[3] = { surface->m_realWidth, surface->m_realHeight, 1 };
        ciErrNum =
            clEnqueueWriteImage(m_dev->m_commandQ, surface->m_clBuffer[0],
                                CL_TRUE, origin, region, 0, 0, surface->m_buf,
                                0, NULL, NULL);

        if (ciErrNum != CL_SUCCESS)
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("OpenCL write image failed: %2 (%3)")
                .arg(ciErrNum) .arg(openCLErrorString(ciErrNum)));
            return NULL;
        }
    }

    return surface;
}

void FFMpegVideoDecoder::DiscardFrame(VideoSurface *frame)
{
}

void FFMpegVideoDecoder::Shutdown(void)
{
}

static bool IS_DR1_PIX_FMT(const enum PixelFormat fmt)
{
    switch (fmt)
    {
        case PIX_FMT_YUV420P:
        case PIX_FMT_YUVJ420P:
            return true;
        default:
            return false;
    }
}

#define MIN_FRAMES 64
int get_avf_buffer(struct AVCodecContext *c, AVFrame *pic)
{
    if (!IS_DR1_PIX_FMT(c->pix_fmt))
    {
        return avcodec_default_get_buffer(c, pic);
    }

    FFMpegVideoDecoder *nd = (FFMpegVideoDecoder *)(c->opaque);
    if (nd->m_unusedSurfaces.isEmpty())
    {
        int created = 0;
        for (int i = 0; i < MIN_FRAMES; i++)
        {
            uint tmp = nd->CreateVideoSurface(nd->m_width, nd->m_height);
            if (tmp)
            {
                nd->m_unusedSurfaces.append(tmp);
                created++;
            }
        }
        nd->m_decoderBufferSize += created;
    }

    uint id = nd->m_unusedSurfaces.takeFirst();
    nd->m_usedSurfaces.append(id);
    VideoSurface *surface = nd->m_videoSurfaces[id];

    if (!surface)
        return -1;

    for (int i = 0; i < 3; i++)
    {
        pic->data[i]     = surface->m_buf + surface->m_offsets[i];
        pic->linesize[i] = surface->m_pitches[i];
    }

    pic->opaque = (void *)surface;
    pic->type = FF_BUFFER_TYPE_USER;

    pic->age = 256 * 256 * 256 * 64;

    pic->reordered_opaque = c->reordered_opaque;

    return 0;
}

void release_avf_buffer(struct AVCodecContext *c, AVFrame *pic)
{
    if (pic->type == FF_BUFFER_TYPE_INTERNAL)
    {
        avcodec_default_release_buffer(c, pic);
        return;
    }

    FFMpegVideoDecoder *nd = (FFMpegVideoDecoder *)(c->opaque);
    if (nd)
    {
        VideoSurface *surface = (VideoSurface *)pic->opaque;
        uint id = surface->m_id;
        nd->m_usedSurfaces.removeOne(id);
        nd->m_unusedSurfaces.append(id);
    }

    if (pic->type != FF_BUFFER_TYPE_USER)
    {
        LOG(VB_GENERAL, LOG_DEBUG, QString("Pic type %1").arg(pic->type));
        return;
    }

    for (uint i = 0; i < 4; i++)
        pic->data[i] = NULL;
}

uint FFMpegVideoDecoder::CreateVideoSurface(uint32_t width, uint32_t height,
                                            VdpChromaType type)
{
    QMutexLocker locker(&m_renderLock);
    QMutexLocker idlock(&m_idLock);

    while (m_videoSurfaces.contains(m_nextId))
        if ((++m_nextId) == 0)
            m_nextId = 1;

    uint id = m_nextId;
    VideoSurface *surface = new VideoSurface(m_dev, kSurfaceFFMpegRender, 
                                             width, height);
    surface->m_id = id;
    m_videoSurfaces.insert(id, surface);

    return id;
}


/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
