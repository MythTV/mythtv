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
    HLSStreamWorker(HLSReader* parent);
    ~HLSStreamWorker(void);

    void Cancel(void);
    void CancelCurrentDownload(void);
    void Wakeup(void) { QMutexLocker lock(&m_lock); m_waitcond.wakeAll(); }

  protected:
    void run(void);

  private:
    void Segment(void);

    // Class vars
    HLSReader      *m_parent;
    MythSingleDownload *m_downloader;
    bool            m_cancel;
    bool            m_wokenup;
    mutable QMutex  m_lock;
    QMutex          m_downloader_lock;
    QWaitCondition  m_waitcond;
};

#endif
