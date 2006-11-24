// -*- Mode: c++ -*-
// Copyright (c) 2005, Daniel Thor Kristjansson

#include <cerrno>
#include <unistd.h>
#include <sys/ioctl.h>

#include "videodev_myth.h"
#include "mythcontext.h"
#include "analogsignalmonitor.h"
#include "channel.h"

#define LOC QString("AnalogSM: ").arg(channel->GetDevice())
#define LOC_ERR QString("AnalogSM, Error: ").arg(channel->GetDevice())

AnalogSignalMonitor::AnalogSignalMonitor(int db_cardnum, Channel *_channel,
                                         uint _flags, const char *_name) :
    SignalMonitor(db_cardnum, _channel, _flags, _name),
    usingv4l2(false)
{
    int videofd = channel->GetFd();
    if (videofd >= 0)
        usingv4l2 = CardUtil::hasV4L2(videofd);
}

#define EMIT(SIGNAL_FUNC, SIGNAL_VAL) \
    do { statusLock.lock(); \
         SignalMonitorValue val = SIGNAL_VAL; \
         statusLock.unlock(); \
         emit SIGNAL_FUNC(val); } while (false)

void AnalogSignalMonitor::UpdateValues(void)
{
    if (!running || exit)
        return;

    int videofd = channel->GetFd();
    if (videofd < 0)
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

    EMIT(StatusSignalLock, signalLock);
    EMIT(StatusSignalStrength, signalStrength);

    if (IsAllGood())
        emit AllGood();
}

#undef EMIT
