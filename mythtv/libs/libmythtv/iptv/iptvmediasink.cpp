/** -*- Mode: c++ -*-
 *  IPTVMediaSink
 *  Copyright (c) 2006 by Laurent Arnal, Benjamin Lerman & Mickaël Remars
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#include "mythcontext.h"
#include "iptvmediasink.h"
#include "streamlisteners.h"

#define LOC QString("IPTVSink:")
#define LOC_ERR QString("IPTVSink, Error:")

IPTVMediaSink::IPTVMediaSink(UsageEnvironment &env,
                             unsigned int      bufferSize) :
    MediaSink(env),
    _buf(NULL),         _buf_size(bufferSize),
    _env(env),          _lock(true)
{
    _buf = new unsigned char[_buf_size];
}

IPTVMediaSink::~IPTVMediaSink()
{
    if (_buf)
    {
        delete[] _buf;
        _buf = NULL;
    }
}

IPTVMediaSink *IPTVMediaSink::CreateNew(UsageEnvironment &env,
                                        unsigned int      bufferSize)
{
    return new IPTVMediaSink(env, bufferSize);
}

Boolean IPTVMediaSink::continuePlaying(void)
{
    if (fSource)
    {
        fSource->getNextFrame(_buf, _buf_size, afterGettingFrame, this,
                              onSourceClosure, this);
        return True;
    }

    return False;
}

void IPTVMediaSink::afterGettingFrame(
        void*          clientData,
        unsigned int   frameSize,
        unsigned int   /*numTruncatedBytes*/,
        struct timeval presentationTime,
        unsigned int   /*durationInMicroseconds*/)
{
    IPTVMediaSink *sink = (IPTVMediaSink*) clientData;
    sink->afterGettingFrame1(frameSize, presentationTime);
}

void IPTVMediaSink::afterGettingFrame1(unsigned int   frameSize,
                                          struct timeval)
{
    _lock.lock();
    vector<TSDataListener*>::iterator it = _listeners.begin();
    for (; it != _listeners.end(); ++it)
        (*it)->AddData(_buf, frameSize);
    _lock.unlock();

    continuePlaying();
}

void IPTVMediaSink::AddListener(TSDataListener *item)
{
    VERBOSE(VB_RECORD, LOC + "AddListener("<<item<<") -- begin");
    if (item)
    {
        RemoveListener(item);
        QMutexLocker locker(&_lock);
        _listeners.push_back(item);
    }
    VERBOSE(VB_RECORD, LOC + "AddListener("<<item<<") -- end");
}

void IPTVMediaSink::RemoveListener(TSDataListener *item)
{
    VERBOSE(VB_RECORD, LOC + "RemoveListener("<<item<<") -- begin 1");
    QMutexLocker locker(&_lock);
    vector<TSDataListener*>::iterator it =
        find(_listeners.begin(), _listeners.end(), item);
    if (it != _listeners.end())
    {
        *it = *_listeners.rbegin();
        _listeners.resize(_listeners.size() - 1);
    }
    VERBOSE(VB_RECORD, LOC + "RemoveListener("<<item<<") -- end 6");
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
