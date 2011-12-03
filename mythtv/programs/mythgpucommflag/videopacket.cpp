#include <QMap>
#include <QMutex>
#include <QMutexLocker>

#include <strings.h>

#include "mythlogging.h"
#include "videodecoder.h"
#include "videopacket.h"
#include "videoprocessor.h"

extern "C" {
#include "libavcodec/avcodec.h"
}

VideoPacketMap videoPacketMap;

VideoPacket::VideoPacket(VideoDecoder *decoder, AVFrame *frame, int num,
                         VideoSurface *prevYUV, VideoSurface *prevRGB,
                         VideoSurface *prevWavelet,
                         VideoHistogram *prevHistogram,
                         VideoHistogram *prevCorrelation) :
    m_num(num), m_decoder(decoder), m_frameIn(frame),
    m_prevFrameYUVSNORM(prevYUV), m_prevFrameRGB(prevRGB),
    m_prevWavelet(prevWavelet), m_prevHistogram(prevHistogram),
    m_prevCorrelation(prevCorrelation), m_blank(false)
{
    m_frameRaw = m_decoder->DecodeFrame(frame);

    OpenCLDevice *dev = m_frameRaw->m_dev;
    uint32_t height   = m_frameRaw->m_height;
    uint32_t width    = m_frameRaw->m_width;

    m_frameYUV = new VideoSurface(dev, kSurfaceYUV, width, height);
    if (!strcmp(m_decoder->Name(), "FFMpeg"))
        OpenCLCombineFFMpegYUV(dev, m_frameRaw, m_frameYUV);
    else
        OpenCLCombineYUV(dev, m_frameRaw, m_frameYUV);

    m_frameRGB = new VideoSurface(dev, kSurfaceRGB, width, height);
    OpenCLYUVToRGB(dev, m_frameYUV, m_frameRGB);

    m_frameYUVSNORM = new VideoSurface(dev, kSurfaceYUV, width, height);
    OpenCLYUVToSNORM(dev, m_frameYUV, m_frameYUVSNORM);

    m_wavelet  = new VideoSurface(dev, kSurfaceWavelet, width, height);
    OpenCLWavelet(dev, m_frameYUVSNORM, m_wavelet);

    m_histogram = new VideoHistogram(dev, 64);
    OpenCLHistogram64(dev, m_frameRGB, m_histogram);

    if (m_prevHistogram)
    {
        m_correlation = new VideoHistogram(dev, 127, -63);
        OpenCLCrossCorrelate(dev, m_prevHistogram, m_histogram, m_correlation);
    }
    else
    {
        m_correlation = NULL;
    }

    videoPacketMap.Add(m_frameIn, this);
}

VideoPacket::~VideoPacket()
{
    videoPacketMap.Remove(m_frameIn);

    // These don't get deleted
    if (m_decoder && m_frameRaw)
        m_decoder->DiscardFrame(m_frameRaw);

    // These are deleted once all users are done with them
    if (m_frameYUVSNORM)
        m_frameYUVSNORM->DownRef();
    if (m_frameYUV)
        m_frameYUV->DownRef();
    if (m_frameRGB)
        m_frameRGB->DownRef();
    if (m_wavelet)
        m_wavelet->DownRef();
    if (m_histogram)
        m_histogram->DownRef();
    if (m_correlation)
        m_correlation->DownRef();

    if (m_prevFrameYUVSNORM)
        m_prevFrameYUVSNORM->DownRef();
    if (m_prevFrameRGB)
        m_prevFrameRGB->DownRef();
    if (m_prevWavelet)
        m_prevWavelet->DownRef();
    if (m_prevHistogram)
        m_prevHistogram->DownRef();
    if (m_prevCorrelation)
        m_prevCorrelation->DownRef();
}


VideoPacket *VideoPacketMap::Lookup(void *key)
{
    QMutexLocker locker(&m_mutex);
    VideoPacket *value = videoPacketMap.value(key, NULL);

    return value;
}

void VideoPacketMap::Add(void *key, VideoPacket *value)
{
    QMutexLocker locker(&m_mutex);
    videoPacketMap.insert(key, value);
}

void VideoPacketMap::Remove(void *key)
{
    QMutexLocker locker(&m_mutex);
    videoPacketMap.remove(key);
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
