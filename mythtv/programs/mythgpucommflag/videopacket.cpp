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

VideoPacket::VideoPacket(VideoDecoder *decoder, AVFrame *frame, int num) :
    m_num(num), m_decoder(decoder), m_frameIn(frame)
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

    videoPacketMap.Add(m_frameIn, this);
}

VideoPacket::~VideoPacket()
{
    videoPacketMap.Remove(m_frameIn);

    if (m_decoder && m_frameRaw)
        m_decoder->DiscardFrame(m_frameRaw);

    if (m_frameYUVSNORM)
        delete m_frameYUVSNORM;
    if (m_frameYUV)
        delete m_frameYUV;
    if (m_wavelet)
        delete m_wavelet;
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
