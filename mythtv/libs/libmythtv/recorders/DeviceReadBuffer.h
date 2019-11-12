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
    virtual ~DeviceReaderCB() = default;
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
    DeviceReadBuffer(DeviceReaderCB *cb,
                     bool use_poll = true,
                     bool error_exit_on_poll_timeout = true);
   ~DeviceReadBuffer();

    bool Setup(const QString &streamName,
               int streamfd,
               uint readQuanta       = sizeof(TSPacket),
               uint deviceBufferSize = 0,
               uint deviceBufferCount = 1);

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
    uint GetUsed(void) const;

  private:
    void run(void) override; // MThread

    void SetPaused(bool);
    void IncrWritePointer(uint len);
    void IncrReadPointer(uint len);

    bool HandlePausing(void);
    bool Poll(void) const;
    void WakePoll(void) const;
    uint WaitForUnused(uint needed) const;
    uint WaitForUsed  (uint needed, uint max_wait /*ms*/) const;

    bool IsPauseRequested(void) const;
    bool IsOpen(void) const { return m_stream_fd >= 0; }
    void ClosePipes(void) const;
    uint GetUnused(void) const;
    uint GetContiguousUnused(void) const;

    bool CheckForErrors(ssize_t read_len, size_t requested_len, uint &errcnt);
    void ReportStats(void);

    QString                 m_videodevice;
    int                     m_stream_fd             {-1};
    mutable int             m_wake_pipe[2]          {-1,-1};
    mutable long            m_wake_pipe_flags[2]    {0,0};

    DeviceReaderCB         *m_readerCB              {nullptr};

    // Data for managing the device ringbuffer
    mutable QMutex          m_lock;
    volatile bool           m_dorun                 {false};
    bool                    m_eof                   {false};
    mutable bool            m_error                 {false};
    bool                    m_request_pause         {false};
    bool                    m_paused                {false};
    bool                    m_using_poll            {true};
    bool                    m_poll_timeout_is_error {true};
    uint                    m_max_poll_wait         {2500 /*ms*/};

    size_t                  m_size                  {0};
    size_t                  m_used                  {0};
    size_t                  m_read_quanta           {0};
    size_t                  m_dev_buffer_count      {1};
    size_t                  m_dev_read_size         {0};
    size_t                  m_readThreshold         {0};
    unsigned char          *m_buffer                {nullptr};
    unsigned char          *m_readPtr               {nullptr};
    unsigned char          *m_writePtr              {nullptr};
    unsigned char          *m_endPtr                {nullptr};

    mutable QWaitCondition  m_dataWait;
    QWaitCondition          m_runWait;
    QWaitCondition          m_pauseWait;
    QWaitCondition          m_unpauseWait;

    // statistics
    size_t                  m_max_used              {0};
    size_t                  m_avg_used              {0};
    size_t                  m_avg_buf_write_cnt     {0};
    size_t                  m_avg_buf_read_cnt      {0};
    size_t                  m_avg_buf_sleep_cnt     {0};
    MythTimer               m_lastReport;
};

#endif // _DEVICEREADBUFFER_H_

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
