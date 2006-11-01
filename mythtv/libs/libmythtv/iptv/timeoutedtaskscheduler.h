/** -*- Mode: c++ -*-
 *  TimeoutedTaskScheduler
 *  Copyright (c) 2006 by Mike Mironov
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef _TIMEOUTEDTASKSCHEDULER_H_
#define _TIMEOUTEDTASKSCHEDULER_H_

// Live555 headers
#include <BasicUsageEnvironment.hh>


class TimeoutedTaskScheduler : public BasicTaskScheduler
{
  public:
    TimeoutedTaskScheduler(unsigned int maxDelayTimeMS);

  public:
    virtual void doEventLoop(char *watchVariable);

  private:
    unsigned int _maxDelayTimeUS;
};

#endif //_TIMEOUTEDTASKSCHEDULER_H_
