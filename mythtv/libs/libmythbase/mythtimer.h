#ifndef MYTHTIMER_H_
#define MYTHTIMER_H_

#include <inttypes.h>
#include <QElapsedTimer>
#include "mythbaseexp.h"

/** A QElapsedTimer based timer to replace use of QTime as a timer.
 *  
 *  This class is not thread-safe.
 */

class MBASE_PUBLIC MythTimer
{
  public:
    typedef enum {
        kStartRunning,
        kStartInactive,
    } StartState;

    MythTimer(StartState state = kStartInactive);

    void start(void);
    int restart(void);
    void stop(void);

    void addMSecs(int ms);

    int elapsed(void) const;
    int64_t nsecsElapsed(void) const;
    bool isRunning(void) const;

  private:
    QElapsedTimer m_timer;
    int m_offset;
};

#endif

