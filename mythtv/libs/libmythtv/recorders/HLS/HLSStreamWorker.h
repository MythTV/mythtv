#ifndef HLS_SEGMENT_WORKER_H
#define HLS_SEGMENT_WORKER_H

#include <QMap>
#include <QWaitCondition>
#include <QMutex>

#include "libmythbase/mthread.h"

class HLSReader;

class HLSStreamWorker : public MThread
{
  public:
    explicit HLSStreamWorker(HLSReader* parent);
    ~HLSStreamWorker(void) override;

    void Cancel(void);
    void CancelCurrentDownload(void);
    void Wakeup(void) { QMutexLocker lock(&m_lock); m_waitCond.wakeAll(); }

  protected:
    void run() override; // MThread

  private:
    void Segment(void);

    // Class vars
    HLSReader          *m_parent     {nullptr};
    MythSingleDownload *m_downloader {nullptr};
    bool                m_cancel     {false};
    bool                m_wokenup    {false};
    mutable QMutex      m_lock;
    QMutex              m_downloaderLock;
    QWaitCondition      m_waitCond;
};

#endif // HLS_SEGMENT_WORKER_H
