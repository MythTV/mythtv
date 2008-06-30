#ifndef MYTHTIMER_H_
#define MYTHTIMER_H_

#include <QTime>
#include "mythexp.h"

class MPUBLIC MythTimer
{
  public:
    MythTimer() : m_running(false) {}

    void start() { m_running = true; m_timer.start(); }
    int restart() { int ret = elapsed();
                    m_timer.restart();
                    return ret;
                  }
    int elapsed() { int ret = m_timer.elapsed();
                    if (ret > 86300000) { ret = 0;  m_timer.restart(); }
                    return ret;
                  }
    void stop() { m_running = false; }
    bool isRunning() const { return m_running; }

    void addMSecs(int ms) { m_timer.addMSecs(ms); }

  private:
    QTime m_timer;
    bool  m_running;
};

#endif

