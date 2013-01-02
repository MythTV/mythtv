// -*- Mode: c++ -*-
/* Device Buffer written by John Poet */

#ifndef _DEVICEREADBUFFER_H_
#define _DEVICEREADBUFFER_H_

#include <unistd.h>

#include <QMutex>
#include <QWaitCondition>
#include <QString>

#include "mythtimer.h"
#include "tspacket.h"
#include "mthread.h"

class DeviceReaderCB
{
  protected:
    virtual ~DeviceReaderCB() {}
  public:
    virtual void ReaderPaused(int fd) = 0;
    virtual void PriorityEvent(int fd) = 0;
};

/** \class DeviceReadBuffer
 *  \brief Buffers reads from device files.
 *
 *  This allows us to read the device regularly even in the presence
 *  of long blocking conditions on writing to disk or accessing the
 *  database.
 */
class DeviceReadBuffer : protected MThread
{
  public:
    DeviceReadBuffer(DeviceReaderCB *callback,
                     bool use_poll = true,
                     bool error_exit_on_poll_timeout = true);
   ~DeviceReadBuffer();

    bool Setup(const QString &streamName,
               int streamfd,
               uint readQuanta       = sizeof(TSPacket),
               uint deviceBufferSize = 0);

    void Start(void);
    void Reset(const QString &streamName, int streamfd);
    void Stop(void);

    void SetRequestPause(bool request);
    bool IsPaused(void) const;
    bool WaitForUnpause(unsigned long timeout);
    bool WaitForPaused(unsigned long timeout);

    bool IsErrored(void) const;
    bool IsEOF(void)     const;
    bool IsRunning(void) const;

    uint Read(unsigned char *buf, uint count);

  private:
    virtual void run(void); // MThread

    void SetPaused(bool);
    void IncrWritePointer(uint len);
    void IncrReadPointer(uint len);

    bool HandlePausing(void);
    bool Poll(void) const;
    void WakePoll(void) const;
    uint WaitForUnused(uint bytes_needed) const;
    uint WaitForUsed  (uint bytes_needed, uint max_wait /*ms*/) const;

    bool IsPauseRequested(void) const;
    bool IsOpen(void) const { return _stream_fd >= 0; }
    void ClosePipes(void) const;
    uint GetUnused(void) const;
    uint GetUsed(void) const;
    uint GetContiguousUnused(void) const;

    bool CheckForErrors(ssize_t read_len, size_t requested_len, uint &err_cnt);
    void ReportStats(void);

    QString          videodevice;
    int              _stream_fd;
    mutable int      wake_pipe[2];
    mutable long     wake_pipe_flags[2];

    DeviceReaderCB  *readerCB;

    // Data for managing the device ringbuffer
    mutable QMutex   lock;
    volatile bool    dorun;
    bool             eof;
    mutable bool     error;
    bool             request_pause;
    bool             paused;
    bool             using_poll;
    bool             poll_timeout_is_error;
    uint             max_poll_wait;

    size_t           size;
    size_t           used;
    size_t           read_quanta;
    size_t           dev_read_size;
    size_t           min_read;
    unsigned char   *buffer;
    unsigned char   *readPtr;
    unsigned char   *writePtr;
    unsigned char   *endPtr;

    mutable QWaitCondition dataWait;
    QWaitCondition   runWait;
    QWaitCondition   pauseWait;
    QWaitCondition   unpauseWait;

    // statistics
    size_t           max_used;
    size_t           avg_used;
    size_t           avg_buf_write_cnt;
    size_t           avg_buf_read_cnt;
    size_t           avg_buf_sleep_cnt;
    MythTimer        lastReport;
};

#endif // _DEVICEREADBUFFER_H_

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
