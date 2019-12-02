#ifndef MYTHTIMER_H_
#define MYTHTIMER_H_

#include <cinttypes>
#include <QElapsedTimer>
#include "mythbaseexp.h"

/** A QElapsedTimer based timer to replace use of QTime as a timer.
 *
 *  This class is not thread-safe.
 */

class MBASE_PUBLIC MythTimer
{
  public:
    enum StartState {
        kStartRunning,
        kStartInactive,
    };

    explicit MythTimer(StartState state = kStartInactive);

    void start(void);
    int restart(void);
    void stop(void);

    void addMSecs(int ms);

    int elapsed(void);
    int64_t nsecsElapsed(void) const;
    bool isRunning(void) const;

  private:
    QElapsedTimer m_timer;
    int           m_offset {0};
};

#endif
