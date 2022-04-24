// -*- Mode: c++ -*-
/* Device Buffer written by John Poet */

#ifndef DEVICEREADBUFFER_H
#define DEVICEREADBUFFER_H

#include <unistd.h>

#include <QMutex>
#include <QWaitCondition>
#include <QString>

#include "libmythbase/mthread.h"
#include "libmythbase/mythbaseutil.h"
#include "libmythbase/mythtimer.h"

#include "mpeg/tspacket.h"

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
    explicit DeviceReadBuffer(DeviceReaderCB *cb,
                     bool use_poll = true,
                     bool error_exit_on_poll_timeout = true);
   ~DeviceReadBuffer() override;

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

    void SetPaused(bool val);
    void IncrWritePointer(uint len);
    void IncrReadPointer(uint len);

    bool HandlePausing(void);
    bool Poll(void) const;
    void WakePoll(void) const;
    uint WaitForUnused(uint needed) const;
    uint WaitForUsed  (uint needed, std::chrono::milliseconds max_wait) const;

    bool IsPauseRequested(void) const;
    bool IsOpen(void) const { return m_streamFd >= 0; }
    void ClosePipes(void) const;
    uint GetUnused(void) const;
    uint GetContiguousUnused(void) const;

    bool CheckForErrors(ssize_t read_len, size_t requested_len, uint &errcnt);
    void ReportStats(void);

    QString                 m_videoDevice;
    int                     m_streamFd              {-1};
    mutable pipe_fd_array   m_wakePipe              {-1,-1};
    mutable pipe_flag_array m_wakePipeFlags         {0,0};

    DeviceReaderCB         *m_readerCB              {nullptr};

    // Data for managing the device ringbuffer
    mutable QMutex          m_lock;
    volatile bool           m_doRun                 {false};
    bool                    m_eof                   {false};
    mutable bool            m_error                 {false};
    bool                    m_requestPause          {false};
    bool                    m_paused                {false};
    bool                    m_usingPoll             {true};
    bool                    m_pollTimeoutIsError    {true};
    std::chrono::milliseconds m_maxPollWait         {2500ms};

    size_t                  m_size                  {0};
    size_t                  m_used                  {0};
    size_t                  m_readQuanta            {0};
    size_t                  m_devBufferCount        {1};
    size_t                  m_devReadSize           {0};
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
    size_t                  m_maxUsed               {0};
    size_t                  m_avgUsed               {0};
    size_t                  m_avgBufWriteCnt        {0};
    size_t                  m_avgBufReadCnt         {0};
    size_t                  m_avgBufSleepCnt        {0};
    MythTimer               m_lastReport;
};

#endif // DEVICEREADBUFFER_H

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
