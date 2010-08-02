// -*- Mode: c++ -*-
// Copyright (c) 2005, Daniel Thor Kristjansson

#include <cerrno>
#include <unistd.h>
#include <sys/ioctl.h>

#include <linux/videodev.h>

#include "mythverbose.h"
#include "analogsignalmonitor.h"
#include "v4lchannel.h"

#define LOC QString("AnalogSM(%1): ").arg(channel->GetDevice())
#define LOC_ERR QString("AnalogSM(%1), Error: ").arg(channel->GetDevice())

AnalogSignalMonitor::AnalogSignalMonitor(
    int db_cardnum, V4LChannel *_channel, uint64_t _flags) :
    SignalMonitor(db_cardnum, _channel, _flags),
    usingv4l2(false)
{
    int videofd = channel->GetFd();
    if (videofd >= 0)
        usingv4l2 = CardUtil::hasV4L2(videofd);
}

void AnalogSignalMonitor::UpdateValues(void)
{
    if (!running || exit)
        return;

    int videofd = channel->GetFd();
    if (videofd < 0)
        return;

    if (!IsChannelTuned())
        return;

    bool isLocked = false;
    if (usingv4l2)
    {
        struct v4l2_tuner tuner;
        bzero(&tuner, sizeof(tuner));

        if (ioctl(videofd, VIDIOC_G_TUNER, &tuner, 0) < 0)
        {
            VERBOSE(VB_IMPORTANT,
                    LOC_ERR + "Failed to probe signal (v4l2)" + ENO);
        }
        else
        {
            isLocked = tuner.signal;
        }
    }
    else
    {
        struct video_tuner tuner;
        bzero(&tuner, sizeof(tuner));

        if (ioctl(videofd, VIDIOCGTUNER, &tuner, 0) < 0)
        {
            VERBOSE(VB_IMPORTANT,
                    LOC_ERR + "Failed to probe signal (v4l1)" + ENO);
        }
        else
        {
            isLocked = tuner.signal;
        }
    }

    {
        QMutexLocker locker(&statusLock);
        signalLock.SetValue(isLocked);
        signalStrength.SetValue(isLocked ? 100 : 0);
    }

    EmitStatus();
    if (IsAllGood())
        SendMessageAllGood();
}

