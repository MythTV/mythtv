#ifndef _HLS_Playlist_Worker_h
#define _HLS_Playlist_Worker_h

#include <QWaitCondition>
#include <QMutex>

#include "mthread.h"

class HLSReader;

class HLSPlaylistWorker : public MThread
{
  public:
    explicit HLSPlaylistWorker(HLSReader* parent);
    ~HLSPlaylistWorker(void);
    
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

#endif
