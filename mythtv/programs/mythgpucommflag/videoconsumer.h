#ifndef _VIDEOCONSUMER_H
#define _VIDEOCONSUMER_H

#include "queueconsumer.h"
#include "packetqueue.h"
#include "flagresults.h"
#include "mythxdisplay.h"
#include "videodecoder.h"
#include "videoouttypes.h"
#include "videoprocessor.h"

class VideoConsumer : public QueueConsumer
{
  public:
    VideoConsumer(PacketQueue *inQ, ResultsMap *outMap, OpenCLDevice *dev);
    ~VideoConsumer() { if (m_decoder) delete m_decoder; };
    bool Initialize(void);
    void ProcessPacket(Packet *packet);
    void EnableX(void)  { m_useX = true; };
    void InitVideoCodec(void);

    bool m_useX;
    VideoDecoder *m_decoder;
    AVSpecialDecode m_specialDecode;
    VideoProcessorList *m_proclist;
};

#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
