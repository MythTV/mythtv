#include <iostream>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>
#include <linux/videodev.h>

using namespace std;

#include "mpegrecorder.h"
#include "RingBuffer.h"

#ifdef HAVE_V4L2
#ifdef V4L2_CAP_VIDEO_CAPTURE
#define USING_V4L2 1
#else
#warning old broken version of v4l2 detected.
#endif
#else
#warning v4l2 support required for hardware mpeg capture
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

    width = 720;
    height = 480;
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
    if (opt == "width")
        width = value;
    else if (opt == "height")
        height = value;
    else
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
#ifndef USING_V4L2
    return;
#else
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

    struct v4l2_format vfmt;
    memset(&vfmt, 0, sizeof(vfmt));

    if (ioctl(chanfd, VIDIOC_G_FMT, &vfmt) < 0)
    {
        cerr << "Error getting format\n";
        perror("VIDIOC_G_FMT:");
        KillChildren();
        return;
    }

    vfmt.fmt.pix.width = width;
    vfmt.fmt.pix.height = height;

    if (ioctl(chanfd, VIDIOC_S_FMT, &vfmt) < 0)
    {
        cerr << "Error setting format\n";
        perror("VIDIOC_S_FMT:");
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

    char buffer[256001];
    int ret;

    encoding = true;
    recording = true;

    struct timeval stm, now;
    gettimeofday(&stm, NULL);

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
            if (cleartimeonpause)
                gettimeofday(&stm, NULL);
            continue;
        }

        if (readfd < 0)
            readfd = open(videodevice.ascii(), O_RDWR);

        ret = read(readfd, buffer, 256000);

        if (ret < 0)
        {
            cerr << "error reading from: " << videodevice << endl;
            perror("read");
            continue;
        }
        else if (ret > 0)
            ringBuffer->Write(buffer, ret);

        gettimeofday(&now, NULL);
    }

    KillChildren();

    recording = false;
#endif
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
    cleartimeonpause = clear;
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
    return framesWritten;
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
