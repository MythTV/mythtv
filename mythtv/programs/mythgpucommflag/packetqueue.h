#ifndef _PACKETQUEUE_H
#define _PACKETQUEUE_H

#include <QQueue>
#include <QMutex>
#include <QWaitCondition>

extern "C" {
#include "libavutil/avutil.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
}

class Packet
{
  public:
    Packet(AVStream *stream, AVPacket *pkt, QMutex *mutex) : m_stream(stream),
        m_pkt(pkt), m_mutex(mutex) {};
    ~Packet()
    { 
        av_free_packet(m_pkt);
        delete m_pkt; 
    };

    AVStream *m_stream;
    AVPacket *m_pkt;
    QMutex   *m_mutex;
};

typedef QQueue<Packet *> PktQueue;

class PacketQueue
{
  public:
    PacketQueue(int maxSize);
    ~PacketQueue(void);

    void enqueue(AVStream *stream, AVPacket *pkt, QMutex *mutex);
    Packet *dequeue(int timeout);
    void flush(void);
    bool isFull(void);
    bool isEmpty(void);
    void stop(void);

  private:
    void resetConds(void);

    int             m_maxSize;
    PktQueue	    m_queue;
    QMutex	        m_mutex;
    QWaitCondition  m_notFullCond;
    QWaitCondition  m_notEmptyCond;
    bool            m_full;
    bool            m_empty;
    bool            m_abort;
};

#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
