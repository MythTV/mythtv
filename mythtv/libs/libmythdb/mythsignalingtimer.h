// -*- Mode: c++ -*-

#ifndef _MYTH_SIGNALING_TIMER_H_
#define _MYTH_SIGNALING_TIMER_H_

#include <stdint.h>

#include <QThread>
#include <QMutex>

#include "mythexp.h"

/** \class MythSignalingTimer
 *  This class is essentially a workaround for a Qt 4.5.2 bug where it
 *  will get stuck in the Qt event loop for up to 999 nanoseconds per
 *  timer firing. This lost millisecond is not a huge issue for infrequent
 *  timers, but causes 7% lost CPU in the MythUI animate() handling.
 */
class MPUBLIC MythSignalingTimer : private QThread
{
    Q_OBJECT

  public:
    MythSignalingTimer(QObject *parent, const char *slot);
    ~MythSignalingTimer();

    virtual void stop(void);
    virtual void start(int msec);

    virtual bool blockSignals(bool block)
        { return QObject::blockSignals(block); }

  signals:
    void timeout(void);

  private:
    virtual void run(void);

    QMutex            startStopLock;
    volatile bool     dorun;
    volatile bool     running;
    volatile uint64_t microsec;
};

#endif // _MYTH_SIGNALING_TIMER_H_
