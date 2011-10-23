#ifndef _QUEUECONSUMER_H
#define _QUEUECONSUMER_H

#include "packetqueue.h"
#include "resultslist.h"
#include "mthread.h"
#include "openclinterface.h"

class QueueConsumer : public MThread
{
  public:
    QueueConsumer(PacketQueue *inQ, ResultsList *outL, QString name) : 
        MThread(name), m_name(name), m_inQ(inQ), m_outL(outL), m_done(false),
        m_dev(NULL) {};
    ~QueueConsumer() {};
    virtual void ProcessPacket(Packet *packet) = 0;

    void run(void);
    void done(void)  { m_done = true; m_inQ->stop(); }
    void SetOpenCLDevice(OpenCLDevice *dev) { m_dev = dev; };
    OpenCLDevice *GetOpenCLDevice(void)  { return m_dev; };
  protected:
    QString m_name;
    PacketQueue *m_inQ;
    ResultsList *m_outL;
    bool m_done;
    OpenCLDevice *m_dev;
};

#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
