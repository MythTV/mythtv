// -*- Mode: c++ -*-
/* Device Buffer written by John Poet */

#ifndef _DEVICEREADBUFFER_H_
#define _DEVICEREADBUFFER_H_

#include <unistd.h>
#include <pthread.h>
#include <sys/poll.h>

#include <qmutex.h>
#include <qwaitcondition.h>
#include <qstring.h>

#include "util.h"

class ReaderPausedCB
{
  protected:
    virtual ~ReaderPausedCB() {}
  public:
    virtual void ReaderPaused(int fd) = 0;
};

/** \class DeviceReadBuffer
 *  \brief Buffers reads from device files.
 *
 *  This allows us to read the device regularly even in the presence
 *  of long blocking conditions on writing to disk or accessing the
 *  database.
 */
class DeviceReadBuffer
{
  public:
    DeviceReadBuffer(ReaderPausedCB *callback, bool use_poll = true);
   ~DeviceReadBuffer();

    bool Setup(const QString &streamName, int streamfd);

    void Start(void);
    void Reset(const QString &streamName, int streamfd);
    void Stop(void);

    void SetRequestPause(bool request);
    bool IsPaused(void) const;
    bool WaitForUnpause(int timeout);
    
    bool IsErrored(void) const { return error; }
    bool IsEOF(void)     const { return eof;   }
    bool IsRunning(void) const;

    uint Read(unsigned char *buf, uint count);

  private:
    static void *boot_ringbuffer(void *);
    void fill_ringbuffer(void);

    void SetPaused(bool);
    void IncrWritePointer(uint len);
    void IncrReadPointer(uint len);

    bool HandlePausing(void);
    bool Poll(void) const;
    uint WaitForUnused(uint bytes_needed) const;
    uint WaitForUsed  (uint bytes_needed) const;

    bool IsPauseRequested(void) const;
    bool IsOpen(void) const { return _stream_fd >= 0; }
    uint GetUnused(void) const;
    uint GetUsed(void) const;
    uint GetContiguousUnused(void) const;

    bool CheckForErrors(ssize_t read_len, uint &err_cnt);
    void ReportStats(void);

    QString          videodevice;
    int              _stream_fd;

    ReaderPausedCB  *readerPausedCB;
    pthread_t        thread;

    // Data for managing the device ringbuffer
    mutable QMutex   lock;
    bool             run;
    bool             running;
    bool             eof;
    bool             error;
    bool             request_pause;
    bool             paused;
    bool             using_poll;

    size_t           size;
    size_t           used;
    size_t           dev_read_size;
    size_t           min_read;
    unsigned char   *buffer;
    unsigned char   *readPtr;
    unsigned char   *writePtr;
    unsigned char   *endPtr;

    QWaitCondition   pauseWait;
    QWaitCondition   unpauseWait;

    // statistics
    size_t           max_used;
    size_t           avg_used;
    size_t           avg_cnt;
    MythTimer        lastReport;
};

#endif // _DEVICEREADBUFFER_H_
