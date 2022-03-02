#ifndef HLS_PLAYLIST_WORKER_H
#define HLS_PLAYLIST_WORKER_H

#include <QWaitCondition>
#include <QMutex>

#include "libmythbase/mthread.h"

class HLSReader;

class HLSPlaylistWorker : public MThread
{
  public:
    explicit HLSPlaylistWorker(HLSReader* parent);
    ~HLSPlaylistWorker(void) override;
    
    void Cancel(void);

  protected:
    void run() override; // MThread

  private:
    // Class vars
    HLSReader      *m_parent  {nullptr};
    bool            m_cancel  {false};
    bool            m_wokenup {false};

    QWaitCondition  m_waitcond;
    QMutex          m_lock;
};

#endif // HLS_PLAYLIST_WORKER_H
