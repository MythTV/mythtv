#ifndef _QUEUECONSUMER_H
#define _QUEUECONSUMER_H

#include "packetqueue.h"
#include "resultslist.h"
#include "mthread.h"
#include "openclinterface.h"

class QueueConsumer : public MThread
{
  public:
    QueueConsumer(PacketQueue *inQ, ResultsList *outL, OpenCLDevice *dev,
                  QString name) : 
        MThread(name), m_name(name), m_inQ(inQ), m_outL(outL), m_done(false),
        m_dev(dev), m_context(NULL), m_codec(NULL), m_stream(NULL),
        m_opened(false) {};
    ~QueueConsumer() {};
    virtual bool Initialize(void) = 0;
    virtual void ProcessPacket(Packet *packet) = 0;
    int64_t NormalizeTimecode(int64_t timecode);
    int64_t NormalizeDuration(int64_t duration);

    void run(void);
    void done(void)  { m_done = true; m_inQ->stop(); }
  protected:
    QString m_name;
    PacketQueue *m_inQ;
    ResultsList *m_outL;
    bool m_done;
    OpenCLDevice *m_dev;
    AVCodecContext *m_context;
    AVCodec *m_codec;
    AVStream *m_stream;
    AVRational m_timebase;
    bool m_opened;
};

#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
