#include <QMap>
#include <QMutex>
#include <QMutexLocker>

#include "mythlogging.h"
#include "videodecoder.h"
#include "videopacket.h"

extern "C" {
#include "libavcodec/avcodec.h"
}

VideoPacketMap videoPacketMap;

VideoPacket::VideoPacket(VideoDecoder *decoder, AVFrame *frame) :
    m_decoder(decoder), m_frameIn(frame)
{
    m_frame = m_decoder->DecodeFrame(frame);
    videoPacketMap.Add(m_frameIn, this);
}

VideoPacket::~VideoPacket()
{
    videoPacketMap.Remove(m_frameIn);

    m_decoder->DiscardFrame(m_frame);
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
