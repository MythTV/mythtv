#include <iostream>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/videodev.h>

using namespace std;

#include "mpegrecorder.h"
#include "RingBuffer.h"

#ifndef HAVE_V4L2
#warning Hardware MPEG-2 supports requires v4l2 in the kernel.
#endif

MpegRecorder::MpegRecorder()
{
    childrenLive = false;
    paused = false;
    pausewritethread = false;
    actuallypaused = false;
    mainpaused = false;

    recording = false;

    framesWritten = false;

    chanfd = -1; 
    readfd = -1;
}

MpegRecorder::~MpegRecorder()
{
    if (chanfd > 0)
        close(chanfd);
    if (readfd > 0)
        close(readfd);
}

void MpegRecorder::SetEncodingOption(const QString &opt, int value)
{
    cerr << "Unknown option: " << opt << ": " << value << endl;
}

void MpegRecorder::ChangeDeinterlacer(int deint_mode)
{
    (void)deint_mode;
}

void MpegRecorder::SetVideoFilters(QString &filters)
{
    (void)filters;
}

void MpegRecorder::Initialize(void)
{
}

void MpegRecorder::StartRecording(void)
{
    if (childrenLive)
    {
        cerr << "Error: children are already alive\n";
        return;
    }

    if (SpawnChildren() < 0)
    {
        cerr << "Couldn't spawn children\n";
        return;
    }

    chanfd = open(videodevice.ascii(), O_RDWR);
    if (chanfd <= 0)
    {
        cerr << "Can't open video device: " << videodevice << endl;
        perror("open video:");
        KillChildren();
        return;
    }

    readfd = open(videodevice.ascii(), O_RDWR);
    if (readfd <= 0)
    {
        cerr << "Can't open video device: " << videodevice << endl;
        perror("open video:");
        KillChildren();
        return;
    }

    char buffer[256000];
    int ret;

    encoding = true;
    recording = true;

    while (encoding)
    {
        if (paused)
        {
            if (readfd > 0)
            {
                close(readfd);
                readfd = -1;
            }
            mainpaused = true;
            usleep(50);
            continue;
        }

        if (readfd < 0)
            readfd = open(videodevice.ascii(), O_RDWR);

        ret = read(readfd, buffer, 128000);
        ringBuffer->Write(buffer, ret);
    }

    KillChildren();

    recording = false;
}

void MpegRecorder::StopRecording(void)
{
    encoding = false;
}

void MpegRecorder::Reset(void)
{
    framesWritten = 0;
}

void MpegRecorder::Pause(bool clear)
{
    actuallypaused = mainpaused = false;
    paused = true;
    pausewritethread = true;
}

void MpegRecorder::Unpause(void)
{
    paused = false;
    pausewritethread = false;
}

bool MpegRecorder::GetPause(void)
{
    return (mainpaused && actuallypaused);
}

bool MpegRecorder::IsRecording(void)
{
    return recording;
}

long long MpegRecorder::GetFramesWritten(void)
{
    return framesWritten;
}

int MpegRecorder::GetVideoFd(void)
{
    return chanfd;
}

void MpegRecorder::TransitionToFile(const QString &lfilename)
{
    ringBuffer->TransitionToFile(lfilename);
}

void MpegRecorder::TransitionToRing(void)
{
    ringBuffer->TransitionToRing();
}

long long MpegRecorder::GetKeyframePosition(long long desired)
{
    return 0;
}

void MpegRecorder::GetBlankFrameMap(QMap<long long, int> &blank_frame_map)
{
}

int MpegRecorder::SpawnChildren(void)
{
    int result;

    childrenLive = true;

    result = pthread_create(&write_tid, NULL, MpegRecorder::WriteThread, this);

    if (result)
    {
        cerr << "Couldn't spawn writer thread, exiting\n";
        return -1;
    }

    return 0;
}

void MpegRecorder::KillChildren(void)
{
    childrenLive = false;

    pthread_join(write_tid, NULL);
}

void *MpegRecorder::WriteThread(void *param)
{
    MpegRecorder *mr = (MpegRecorder *)param;
    mr->doWriteThread();
    return NULL;
}

void MpegRecorder::doWriteThread(void)
{
    actuallypaused = false;
    while (childrenLive)
    {
        if (pausewritethread)
        {
            actuallypaused = true;
            usleep(50);
            continue;
        }

        usleep(100);
    }
}
