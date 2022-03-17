#ifndef MYTHDECODERTHREAD_H
#define MYTHDECODERTHREAD_H

// MythTV
#include "libmythbase/mthread.h"

class MythPlayer;

class MythDecoderThread : public MThread
{
  public:
    MythDecoderThread(MythPlayer *Player, bool StartPaused);
    ~MythDecoderThread() override;

  protected:
    void run() override;

  private:
    Q_DISABLE_COPY(MythDecoderThread)
    MythPlayer *m_player      { nullptr };
    bool        m_startPaused { false   };
};

#endif
