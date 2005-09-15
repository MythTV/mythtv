// -*- Mode: c++ -*-

// C headers
#include <unistd.h>

// Qt headers
#include <qsocket.h>

// MythTV headers
#include "RingBufferInfo.h"
#include "RingBuffer.h"
#include "mythcontext.h"
#include "util.h" // for WriteBlock

const uint RingBufferInfo::kStreamedFileReadTimeout = 100; /* msec */
const uint RingBufferInfo::kRequestBufferSize = 256*1024; /* 32 KB */

/** \fn RingBufferInfo::SetDataSocket(QSocket*)
 *  \brief Sets RingBuffer data socket, for A/V streaming.
 *  \sa GetDataSocket(void)
 */
void RingBufferInfo::SetDataSocket(QSocket *sock)
{
    GetLock(false);
    if ((IsAlive() && sock) || (!IsAlive() && !sock))
        return;

    if (sock)
    {
        dataSocket = sock;
        SetAlive(true);
    }
    else
    {
        SetAlive(false);
        if (ringBuffer)
            ringBuffer->StopReads();
    }
    ReturnLock();
}

/** \fn RingBufferInfo::GetDataSocket(void)
 *  \brief Returns RingBuffer data socket, for A/V streaming.
 *  \sa SetDataSocket(QSocket*)
 */
QSocket *RingBufferInfo::GetDataSocket(void)
{
    GetLock(false);
    QSocket *sock = dataSocket;
    ReturnLock();
    return sock;
}

void RingBufferInfo::SetRingBuffer(RingBuffer *rb)
{
    RingBuffer *rb_old = ringBuffer;

    ringBuffer = rb;

    if (rb_old)
        delete rb_old;
}

/** \fn RingBufferInfo::SetAlive(bool)
 *  \brief Sets alive state for data socket.
 *
 *   Note: You must be holding the lock to call this method.
 */
void RingBufferInfo::SetAlive(bool is_live)
{
    live = is_live;
}

/** \fn RingBufferInfo::RequestBlock(uint)
 *  \brief Tells ring buffer to send data on the read thread sock, if the
 *         ring buffer thread is alive and the ringbuffer isn't paused.
 *
 *  \param size Requested block size, may not be respected, but this many
 *              bytes of data will be returned if it is available.
 *  \return -1 if request does not succeed, amount of data sent otherwise.
 */
int RingBufferInfo::RequestBlock(uint size)
{
    int tot = 0;
    int ret = 0;

    GetLock(true);

    if (!IsAlive() || !ringBuffer)
    {
        ReturnLock();
        VERBOSE(VB_IMPORTANT, "RingBufferInfo::RequestBlock("
                <<size<<") err");
        return -1;
    }

    QTime t;
    t.start();
    while (tot < (int)size &&
           !ringBuffer->GetStopReads() &&
           IsAlive() &&
           t.elapsed() < (int)kStreamedFileReadTimeout)
    {
        uint request = size - tot;

        request = min(request, kRequestBufferSize);
 
        ret = ringBuffer->Read(requestBuffer, request);
        
        if (ringBuffer->GetStopReads() || ret <= 0)
            break;
        
        if (!WriteBlock(dataSocket->socketDevice(), requestBuffer, ret))
        {
            tot = -1;
            break;
        }

        tot += ret;
        if (ret < (int)request)
            break; // we hit eof
    }
    ReturnLock();

    if (ret < 0)
        tot = -1;

    return tot;
}

/** \fn RingBufferInfo::Seek(long long, long long, int)
 *  \brief Tells RingBufferInfo to seek to a specific byte in RingBuffer.
 *
 *  \param curpos Current byte position in RingBuffer
 *  \param pos    Desired position, or position delta.
 *  \param whence One of SEEK_SET, or SEEK_CUR, or SEEK_END.
 *                These work like the equivalent fseek parameters.
 *  \return new position if seek is successful, -1 otherwise.
 */
long long RingBufferInfo::Seek(long long curpos, long long pos, int whence)
{ 
    GetLock(false);
    bool err = !ringBuffer || !IsAlive();
    ReturnLock();

    if (err)
    {
        VERBOSE(VB_IMPORTANT, "RingBufferInfo::Seek("
                <<curpos<<", "<<pos<<", "<<whence<<") err");
        return -1;
    }

    Pause();
    if (whence == SEEK_CUR)
    {
        long long realpos = ringBuffer->GetTotalReadPosition();
        pos = pos + curpos - realpos;
    }
    long long ret = ringBuffer->Seek(pos, whence);
    Unpause();

    return ret;
}

/** \fn RingBufferInfo::Pause(void)
 *  \brief Calls RingBuffer::StopReads(void)
 */
void RingBufferInfo::Pause(void)
{
    GetLock(false);
    paused++;
    if (ringBuffer)
        ringBuffer->StopReads();
    ReturnLock();
}

/** \fn RingBufferInfo::Unpause(void)
 *  \brief Calls RingBuffer::StartReads(void)
 */
void RingBufferInfo::Unpause(void)
{
    GetLock(false);
    if (IsPaused() && ringBuffer)
        ringBuffer->StartReads();
    paused--;
    if (paused < 0)
        paused = 0;
    ReturnLock();
}

void RingBufferInfo::GetLock(bool requireUnpaused)
{
    lock.lock();
    while (requireUnpaused && IsPaused())
        unpausedWait.wait(&lock);
}

void RingBufferInfo::ReturnLock(void)
{
    lock.unlock();
    unpausedWait.wakeAll();
}
