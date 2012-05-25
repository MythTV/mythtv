/** -*- Mode: c++ -*-
 *  IPTVFeederLive -- base class for livemedia based IPTVFeeders
 *  Copyright (c) 2006 by Laurent Arnal, Benjamin Lerman & Mickaël Remars
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#include <algorithm>

#include "iptvfeederlive.h"

// MythTV headers
#include "mythlogging.h"
#include "timeoutedtaskscheduler.h"

#define LOC QString("FbFeedLive:")


IPTVFeederLive::IPTVFeederLive() :
    _live_env(NULL),    _lock(),
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
        LOG(VB_GENERAL, LOG_ERR, LOC + "InitEnv, live env. already exits.");
        return false;
    }

    TaskScheduler *scheduler = new TimeoutedTaskScheduler(500);
    if (!scheduler)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create Live Scheduler.");
        return false;
    }

    _live_env = BasicUsageEnvironment::createNew(*scheduler);
    if (!_live_env)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create Live Environment.");
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
    LOG(VB_RECORD, LOG_INFO, LOC + "Run() -- begin");
    _lock.lock();
    _running = true;
    _abort   = 0;
    _lock.unlock();

    LOG(VB_RECORD, LOG_INFO, LOC + "Run() -- loop begin");
    if (_live_env)
        _live_env->taskScheduler().doEventLoop(&_abort);
    LOG(VB_RECORD, LOG_INFO, LOC + "Run() -- loop end");

    _lock.lock();
    _running = false;
    _cond.wakeAll();
    _lock.unlock();
    LOG(VB_RECORD, LOG_INFO, LOC + "Run() -- end");
}

void IPTVFeederLive::Stop(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "Stop() -- begin");
    QMutexLocker locker(&_lock);
    _abort = 0xFF;

    while (_running)
        _cond.wait(&_lock, 500);
    LOG(VB_RECORD, LOG_INFO, LOC + "Stop() -- end");
}

void IPTVFeederLive::AddListener(TSDataListener *item)
{
    LOG(VB_RECORD, LOG_INFO, LOC + QString("AddListener(0x%1) -- begin")
        .arg((uint64_t)item,0,16));
    if (!item)
    {
        LOG(VB_RECORD, LOG_INFO, LOC + QString("AddListener(0x%1) -- end")
            .arg((uint64_t)item,0,16));
        return;
    }

    // avoid duplicates
    RemoveListener(item);

    // add to local list
    QMutexLocker locker(&_lock);
    _listeners.push_back(item);

    LOG(VB_RECORD, LOG_INFO, LOC + QString("AddListener(0x%1) -- end")
        .arg((uint64_t)item,0,16));
}

void IPTVFeederLive::RemoveListener(TSDataListener *item)
{
    LOG(VB_RECORD, LOG_INFO, LOC + QString("RemoveListener(0x%1) -- begin")
        .arg((uint64_t)item,0,16));
    QMutexLocker locker(&_lock);
    vector<TSDataListener*>::iterator it =
    find(_listeners.begin(), _listeners.end(), item);

    if (it == _listeners.end())
    {
        LOG(VB_RECORD, LOG_INFO, LOC + QString("RemoveListener(0x%1) -- end 1")
            .arg((uint64_t)item,0,16));
        return;
    }

    // remove from local list..
    *it = *_listeners.rbegin();
    _listeners.resize(_listeners.size() - 1);

    LOG(VB_RECORD, LOG_INFO, LOC + QString("RemoveListener(0x%1) -- end 2")
        .arg((uint64_t)item,0,16));
}

