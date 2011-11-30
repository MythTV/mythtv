#ifndef _VIDEOPACKET_H
#define _VIDEOPACKET_H

#include <QMap>
#include <QMutex>

extern "C" {
#include "libavcodec/avcodec.h"
}

#include "videodecoder.h"
#include "videosurface.h"

class VideoPacket
{
  public:
    VideoPacket(VideoDecoder *decoder, AVFrame *frame, int num,
                VideoSurface *prevYUV, VideoSurface *prevWavelet,
                VideoHistogram *prevHistogram);
    ~VideoPacket();

    uint64_t m_num;
    VideoDecoder *m_decoder;
    AVFrame *m_frameIn;
    VideoSurface *m_frameRaw;
    VideoSurface *m_frameYUV;
    VideoSurface *m_frameYUVSNORM;
    VideoSurface *m_wavelet;
    VideoHistogram *m_histogram;

    VideoSurface *m_prevFrameYUVSNORM;
    VideoSurface *m_prevWavelet;
    VideoHistogram *m_prevHistogram;
};

class VideoPacketMap : public QMap<void *, VideoPacket *>
{
  public:
    VideoPacketMap() {};
    ~VideoPacketMap() {};
    VideoPacket *Lookup(void *key);
    void Add(void *key, VideoPacket *value);
    void Remove(void *key);

    QMutex m_mutex;
};
extern VideoPacketMap videoPacketMap;
 
#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
