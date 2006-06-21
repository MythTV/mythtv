/**
 *  FreeboxMediaSink
 *  Copyright (c) 2006 by Laurent Arnal, Benjamin Lerman & Mickaël Remars
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#include "freeboxmediasink.h"
#include "freeboxrecorder.h"

FreeboxMediaSink::FreeboxMediaSink(UsageEnvironment &pEnv,
                                   FreeboxRecorder  &pRecorder,
                                   unsigned int      bufferSize) :
    MediaSink(pEnv),    fBufferSize(bufferSize),
    env(pEnv),          recorder(pRecorder)
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

FreeboxMediaSink *FreeboxMediaSink::createNew(UsageEnvironment &env,
                                              FreeboxRecorder  &pRecorder,
                                              unsigned int      bufferSize)
{
    return new FreeboxMediaSink(env, pRecorder, bufferSize);
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
                                          struct timeval presentationTime)
{
    addData(fBuffer, frameSize, presentationTime);
    continuePlaying();
}

void FreeboxMediaSink::addData(unsigned char *data,
                               unsigned int   dataSize,
                               struct timeval presentationTime)
{
    recorder.AddData(data, dataSize, presentationTime);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
