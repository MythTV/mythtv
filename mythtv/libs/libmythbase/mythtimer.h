#ifndef MYTHTIMER_H_
#define MYTHTIMER_H_

#include <QElapsedTimer>
#include "mythbaseexp.h"
#include "mythchrono.h"

/** A QElapsedTimer based timer to replace use of QTime as a timer.
 *
 *  This class is not thread-safe.
 */

class MBASE_PUBLIC MythTimer
{
  public:
    enum StartState : std::uint8_t {
        kStartRunning,
        kStartInactive,
    };

    explicit MythTimer(StartState state = kStartInactive);

    void start(void);
    std::chrono::milliseconds restart(void);
    void stop(void);

    void addMSecs(std::chrono::milliseconds ms);

    std::chrono::milliseconds elapsed(void);
    std::chrono::nanoseconds nsecsElapsed(void) const;
    bool isRunning(void) const;

  private:
    QElapsedTimer             m_timer;
    std::chrono::milliseconds m_offset {0ms};
};

#endif
