#ifndef _HLS_Playlist_Worker_h
#define _HLS_Playlist_Worker_h

#include <QWaitCondition>
#include <QMutex>

#include "mthread.h"

class HLSReader;

class HLSPlaylistWorker : public MThread
{
  public:
    HLSPlaylistWorker(HLSReader* parent);
    ~HLSPlaylistWorker(void);
    
    void Cancel(void);

  protected:
    void run(void);

  private:
    // Class vars
    HLSReader * m_parent;
    bool        m_cancel;
    bool        m_wokenup;

    QWaitCondition  m_waitcond;
    QMutex          m_lock;
};

#endif
