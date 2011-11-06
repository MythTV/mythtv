#ifndef _AUDIOPACKET_H
#define _AUDIOPACKET_H

#include <QMap>
#include <QMutex>

#include "openclinterface.h"

class AudioPacket
{
  public:
    AudioPacket(OpenCLDevice *dev, int16_t *samples, int size, int count);
    ~AudioPacket();

    OpenCLDevice *m_dev;
    int16_t *m_audioSamples;
    int m_size;
    int m_frameCount;
    int m_channels;
    cl_mem m_buffer;
};

class AudioPacketMap : public QMap<int16_t *, AudioPacket *>
{
  public:
    AudioPacketMap() {};
    ~AudioPacketMap() {};
    AudioPacket *Lookup(int16_t *key);
    void Add(int16_t *key, AudioPacket *value);
    void Remove(int16_t *key);

    QMutex m_mutex;
};
extern AudioPacketMap audioPacketMap;
 
#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
