/** -*- Mode: c++ -*-
 *  TimeoutedTaskScheduler
 *  Copyright (c) 2006 by Mike Mironov
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#include "timeoutedtaskscheduler.h"


TimeoutedTaskScheduler::TimeoutedTaskScheduler(unsigned int maxDelayTimeMS) :
    _maxDelayTimeUS(maxDelayTimeMS * 1000)
{
}

void TimeoutedTaskScheduler::doEventLoop(char *watchVariable)
{
    // Repeatedly loop, handling readble sockets and timed events:
    while (!watchVariable || !(*watchVariable))
    {
        SingleStep(_maxDelayTimeUS);
    }
}
