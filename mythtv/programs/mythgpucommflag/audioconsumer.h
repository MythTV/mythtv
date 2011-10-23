#ifndef _AUDIOCONSUMER_H
#define _AUDIOCONSUMER_H

#include "queueconsumer.h"
#include "packetqueue.h"
#include "resultslist.h"

class AudioConsumer : public QueueConsumer
{
  public:
    AudioConsumer(PacketQueue *inQ, ResultsList *outL) : 
        QueueConsumer(inQ, outL, "AudioConsumer") 
    {
        m_audioSamples = (int16_t *)av_mallocz(AVCODEC_MAX_AUDIO_FRAME_SIZE *
                                               sizeof(int32_t));
    }
    ~AudioConsumer() {};
    void ProcessPacket(Packet *packet);
    void ProcessFrame(int16_t *samples, int size, int frames, int64_t pts);

    int16_t *m_audioSamples;
};

#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
