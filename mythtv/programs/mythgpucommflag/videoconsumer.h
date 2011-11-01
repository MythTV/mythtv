#ifndef _VIDEOCONSUMER_H
#define _VIDEOCONSUMER_H

#include "queueconsumer.h"
#include "packetqueue.h"
#include "resultslist.h"

class VideoConsumer : public QueueConsumer
{
  public:
    VideoConsumer(PacketQueue *inQ, ResultsList *outL, OpenCLDevice *dev) : 
        QueueConsumer(inQ, outL, dev, "VideoConsumer") {};
    ~VideoConsumer() {};
    void ProcessPacket(Packet *packet);
};

#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
