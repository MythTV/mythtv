#include <QMutex>
#include <QMutexLocker>

#include "mythlogging.h"
#include "packetqueue.h"

PacketQueue::PacketQueue(int maxSize) : m_maxSize(maxSize), m_full(false),
    m_empty(true), m_abort(false)
{
}

PacketQueue::~PacketQueue(void)
{
}

void PacketQueue::enqueue(AVStream *stream, AVPacket *pkt, QMutex *mutex)
{
    QMutexLocker locker(&m_mutex);

    if (m_abort)
        return;

    while (m_full)
        m_notFullCond.wait(locker.mutex(), 100);

    Packet *packet = new Packet(stream, pkt, mutex);
    m_queue.enqueue(packet);
    resetConds();
}

Packet *PacketQueue::dequeue(int timeout)
{
    QMutexLocker locker(&m_mutex);

    int timeoutMs = (timeout < 0 ? 100 : timeout);

    while (m_empty && !m_abort)
    {
        m_notEmptyCond.wait(locker.mutex(), timeoutMs);
        if ((timeout >= 0 && m_empty) | m_abort)
            return NULL;
    }

    Packet *packet = m_queue.dequeue();
    resetConds();
    return packet;
}

void PacketQueue::flush(void)
{
    QMutexLocker locker(&m_mutex);

    m_queue.clear();
    resetConds();
}

void PacketQueue::stop(void)
{
    QMutexLocker locker(&m_mutex);

    m_abort = true;
    // Let the dequeue give up as the queue will be staying empty
    if (m_empty)
        m_notEmptyCond.wakeAll();
}

bool PacketQueue::isFull(void)
{
    QMutexLocker locker(&m_mutex);

    return m_full;
}

bool PacketQueue::isEmpty(void)
{
    QMutexLocker locker(&m_mutex);

    return m_empty;
}

void PacketQueue::resetConds(void)
{
    // This MUST be run while m_mutex is locked!
    bool oldEmpty = m_empty;
    bool oldFull  = m_full;

    if (m_queue.isEmpty())
    {
        m_full = false;
        m_empty = true;
    }
    else
        m_empty = false;

    if (m_queue.count() >= m_maxSize)
    {
        m_full = true;
        m_empty = false;
    }
    else
        m_full = false;

    if (oldFull && !m_full)
        m_notFullCond.wakeAll();

    if (oldEmpty && !m_empty)
        m_notEmptyCond.wakeAll();
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
