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
                         VideoSurface *prevYUV, VideoSurface *prevWavelet,
                         VideoHistogram *prevHistogram) :
    m_num(num), m_decoder(decoder), m_frameIn(frame),
    m_prevFrameYUVSNORM(prevYUV), m_prevWavelet(prevWavelet),
    m_prevHistogram(prevHistogram)
{
    m_frameRaw = m_decoder->DecodeFrame(frame);

    OpenCLDevice *dev = m_frameRaw->m_dev;

    m_frameYUV = new VideoSurface(m_frameRaw->m_dev, kSurfaceYUV,
                                  m_frameRaw->m_width, m_frameRaw->m_height);
    if (!strcmp(m_decoder->Name(), "FFMpeg"))
        OpenCLCombineFFMpegYUV(dev, m_frameRaw, m_frameYUV);
    else
        OpenCLCombineYUV(dev, m_frameRaw, m_frameYUV);

    m_frameYUVSNORM = new VideoSurface(m_frameRaw->m_dev, kSurfaceYUV,
                                       m_frameRaw->m_width,
                                       m_frameRaw->m_height);
    OpenCLYUVToSNORM(dev, m_frameYUV, m_frameYUVSNORM);
    m_wavelet  = new VideoSurface(m_frameRaw->m_dev, kSurfaceWavelet,
                                  m_frameRaw->m_width, m_frameRaw->m_height);
    OpenCLWavelet(dev, m_frameYUVSNORM, m_wavelet);

    m_histogram = new VideoHistogram(64);
    OpenCLHistogram64(dev, m_frameYUVSNORM, m_histogram);

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
    if (m_wavelet)
        m_wavelet->DownRef();
    if (m_histogram)
        m_histogram->DownRef();
    if (m_prevFrameYUVSNORM)
        m_prevFrameYUVSNORM->DownRef();
    if (m_prevWavelet)
        m_prevWavelet->DownRef();
    if (m_prevHistogram)
        m_prevHistogram->DownRef();
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
