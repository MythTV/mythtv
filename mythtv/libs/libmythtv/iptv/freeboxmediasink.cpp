/** -*- Mode: c++ -*-
 *  FreeboxMediaSink
 *  Copyright (c) 2006 by Laurent Arnal, Benjamin Lerman & Mickaël Remars
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#include "freeboxmediasink.h"
#include "mythcontext.h"
#include "iptvlistener.h"

#define LOC QString("IPTVSink:")
#define LOC_ERR QString("IPTVSink, Error:")

FreeboxMediaSink::FreeboxMediaSink(UsageEnvironment &pEnv,
                                   unsigned int      bufferSize) :
    MediaSink(pEnv),    fBufferSize(bufferSize),
    env(pEnv),          lock(true)
{
    fBuffer = new unsigned char[fBufferSize];
}

FreeboxMediaSink::~FreeboxMediaSink()
{
    if (fBuffer)
    {
        delete[] fBuffer;
        fBuffer = NULL;
    }
}

FreeboxMediaSink *FreeboxMediaSink::CreateNew(UsageEnvironment &env,
                                              unsigned int      bufferSize)
{
    return new FreeboxMediaSink(env, bufferSize);
}

Boolean FreeboxMediaSink::continuePlaying(void)
{
    if (fSource)
    {
        fSource->getNextFrame(fBuffer, fBufferSize, afterGettingFrame, this,
                              onSourceClosure, this);
        return True;
    }

    return False;
}

void FreeboxMediaSink::afterGettingFrame(
        void*          clientData,
        unsigned int   frameSize,
        unsigned int   /*numTruncatedBytes*/,
        struct timeval presentationTime,
        unsigned int   /*durationInMicroseconds*/)
{
    FreeboxMediaSink *sink = (FreeboxMediaSink*) clientData;
    sink->afterGettingFrame1(frameSize, presentationTime);
}

void FreeboxMediaSink::afterGettingFrame1(unsigned int   frameSize,
                                          struct timeval)
{
    lock.lock();
    vector<IPTVListener*>::iterator it = listeners.begin();
    for (; it != listeners.end(); ++it)
        (*it)->AddData(fBuffer, frameSize);
    lock.unlock();

    continuePlaying();
}

void FreeboxMediaSink::AddListener(IPTVListener *item)
{
    VERBOSE(VB_RECORD, LOC + "AddListener("<<item<<") -- begin");
    if (item)
    {
        RemoveListener(item);
        QMutexLocker locker(&lock);
        listeners.push_back(item);
    }
    VERBOSE(VB_RECORD, LOC + "AddListener("<<item<<") -- end");
}

void FreeboxMediaSink::RemoveListener(IPTVListener *item)
{
    VERBOSE(VB_RECORD, LOC + "RemoveListener("<<item<<") -- begin 1");
    QMutexLocker locker(&lock);
    vector<IPTVListener*>::iterator it =
        find(listeners.begin(), listeners.end(), item);
    if (it != listeners.end())
    {
        *it = *listeners.rbegin();
        listeners.resize(listeners.size() - 1);
    }
    VERBOSE(VB_RECORD, LOC + "RemoveListener("<<item<<") -- end 6");
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
