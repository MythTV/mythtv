#include <QMap>
#include <QMutex>
#include <QMutexLocker>

#include "mythlogging.h"
#include "openclinterface.h"
#include "audiopacket.h"

AudioPacketMap audioPacketMap;

AudioPacket::AudioPacket(OpenCLDevice *dev, int16_t *samples, int size,
                         int count) :
    m_dev(dev), m_audioSamples(samples), m_size(size), m_frameCount(count)
{
    cl_int ciErrNum;

    m_channels = size / count / sizeof(int16_t);

    m_buffer = clCreateBuffer(m_dev->m_context,
                              CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                              m_size, m_audioSamples, &ciErrNum);
    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Error %1 creating audioPacket")
            .arg(ciErrNum));
        return;
    }
    audioPacketMap.Add(m_audioSamples, this);
}

AudioPacket::~AudioPacket()
{
    audioPacketMap.Remove(m_audioSamples);

    clReleaseMemObject(m_buffer); 
}


AudioPacket *AudioPacketMap::Lookup(int16_t *key)
{
    QMutexLocker locker(&m_mutex);
    AudioPacket *value = audioPacketMap.value(key, NULL);

    return value;
}

void AudioPacketMap::Add(int16_t *key, AudioPacket *value)
{
    QMutexLocker locker(&m_mutex);
    audioPacketMap.insert(key, value);
}

void AudioPacketMap::Remove(int16_t *key)
{
    QMutexLocker locker(&m_mutex);
    audioPacketMap.remove(key);
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
