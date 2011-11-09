#ifndef _AUDIOCONSUMER_H
#define _AUDIOCONSUMER_H

#include <QMap>

#include "queueconsumer.h"
#include "packetqueue.h"
#include "resultslist.h"
#include "audioprocessor.h"

class AudioConsumer : public QueueConsumer
{
  public:
    AudioConsumer(PacketQueue *inQ, ResultsList *outL, OpenCLDevice *dev);
    ~AudioConsumer() { av_free(m_audioSamples); };
    bool Initialize(void) { return true; };
    void ProcessPacket(Packet *packet);
    void ProcessFrame(int16_t *samples, int size, int frames, int64_t pts,
                      int rate, uint64_t pos);

    int16_t *m_audioSamples;
    AudioProcessorList *m_proclist;
};

#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
