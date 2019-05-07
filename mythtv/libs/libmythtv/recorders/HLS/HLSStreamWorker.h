#ifndef _HLS_Segment_Worker_h_
#define _HLS_Segment_Worker_h_

#include <QMap>
#include <QWaitCondition>
#include <QMutex>

#include "mthread.h"

class HLSReader;

class HLSStreamWorker : public MThread
{
  public:
    explicit HLSStreamWorker(HLSReader* parent);
    ~HLSStreamWorker(void);

    void Cancel(void);
    void CancelCurrentDownload(void);
    void Wakeup(void) { QMutexLocker lock(&m_lock); m_waitcond.wakeAll(); }

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
    QMutex              m_downloader_lock;
    QWaitCondition      m_waitcond;
};

#endif
