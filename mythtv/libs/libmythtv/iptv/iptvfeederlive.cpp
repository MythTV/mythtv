/** -*- Mode: c++ -*-
 *  IPTVFeederLive -- base class for livemedia based IPTVFeeders
 *  Copyright (c) 2006 by Laurent Arnal, Benjamin Lerman & Mickaël Remars
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#include "iptvfeederlive.h"

// MythTV headers
#include "mythcontext.h"
#include "timeoutedtaskscheduler.h"

#define LOC QString("FbFeedLive:")
#define LOC_ERR QString("FbFeedLive, Error:")


IPTVFeederLive::IPTVFeederLive() :
    _live_env(NULL),    _lock(false),
    _abort(0),          _running(false)
{
}

IPTVFeederLive::~IPTVFeederLive()
{
}

bool IPTVFeederLive::InitEnv(void)
{
    if (_live_env)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "InitEnv, live env. already exits.");
        return false;
    }

    TaskScheduler *scheduler = new TimeoutedTaskScheduler(500);
    if (!scheduler)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to create Live Scheduler.");
        return false;
    }

    _live_env = BasicUsageEnvironment::createNew(*scheduler);
    if (!_live_env)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to create Live Environment.");
        delete scheduler;
        return false;
    }
    
    return true;
}

void IPTVFeederLive::FreeEnv(void)
{
    if (_live_env)
    {
        TaskScheduler *scheduler = &_live_env->taskScheduler();
        _live_env->reclaim();
        _live_env = NULL;
        if (scheduler)
            delete scheduler;
    }
}

void IPTVFeederLive::Run(void)
{
    VERBOSE(VB_RECORD, LOC + "Run() -- begin");
    _lock.lock();
    _running = true;
    _abort   = 0;
    _lock.unlock();

    VERBOSE(VB_RECORD, LOC + "Run() -- loop begin");
    if (_live_env)
        _live_env->taskScheduler().doEventLoop(&_abort);
    VERBOSE(VB_RECORD, LOC + "Run() -- loop end");

    _lock.lock();
    _running = false;
    _cond.wakeAll();
    _lock.unlock();
    VERBOSE(VB_RECORD, LOC + "Run() -- end");
}

void IPTVFeederLive::Stop(void)
{
    VERBOSE(VB_RECORD, LOC + "Stop() -- begin");
    QMutexLocker locker(&_lock);
    _abort = 0xFF;

    while (_running)
        _cond.wait(&_lock, 500);
    VERBOSE(VB_RECORD, LOC + "Stop() -- end");
}
