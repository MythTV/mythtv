// -*- Mode: c++ -*-
// Copyright (c) 2005, Daniel Thor Kristjansson

#include <cerrno>
#include <unistd.h>
#include <sys/ioctl.h>
#include <poll.h>

#include <linux/videodev.h>

#include "mythverbose.h"
#include "analogsignalmonitor.h"
#include "v4lchannel.h"

#define LOC QString("AnalogSM(%1): ").arg(channel->GetDevice())
#define LOC_ERR QString("AnalogSM(%1), Error: ").arg(channel->GetDevice())

AnalogSignalMonitor::AnalogSignalMonitor(
    int db_cardnum, V4LChannel *_channel, uint64_t _flags) :
    SignalMonitor(db_cardnum, _channel, _flags),
    m_usingv4l2(false), m_stage(0)
{
    int videofd = channel->GetFd();
    if (videofd >= 0)
    {
        m_usingv4l2 = CardUtil::hasV4L2(videofd);
        CardUtil::GetV4LInfo(videofd, m_card, m_driver, m_version);
        VERBOSE(VB_RECORD, LOC + QString("card '%1' driver '%2' version '%3'")
                .arg(m_card).arg(m_driver).arg(m_version));
    }
}

bool AnalogSignalMonitor::handleHDPVR(int videofd)
{
    struct v4l2_encoder_cmd command;
    struct pollfd polls;

    if (m_stage == 0)
    {
        VERBOSE(VB_RECORD, LOC + "hd-pvr start encoding");
        // Tell it to start encoding, then wait for it to actually feed us
        // some data.
        memset(&command, 0, sizeof(struct v4l2_encoder_cmd));
        command.cmd = V4L2_ENC_CMD_START;
        if (ioctl(videofd, VIDIOC_ENCODER_CMD, &command) == 0)
            m_stage = 1;
        else
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Start encoding failed" + ENO);
            command.cmd = V4L2_ENC_CMD_STOP;
            ioctl(videofd, VIDIOC_ENCODER_CMD, &command);
        }
    }

    if (m_stage == 1)
    {
        VERBOSE(VB_RECORD, LOC + "hd-pvr wait for data");

        polls.fd      = videofd;
        polls.events  = POLLIN;
        polls.revents = 0;

        if (poll(&polls, 1, 1500) > 0)
        {
            m_stage = 2;
            QMutexLocker locker(&statusLock);
            signalStrength.SetValue(25);
        }
        else
        {
            VERBOSE(VB_RECORD, LOC + "Poll timed-out.  Resetting");
            memset(&command, 0, sizeof(struct v4l2_encoder_cmd));
            command.cmd = V4L2_ENC_CMD_STOP;
            ioctl(videofd, VIDIOC_ENCODER_CMD, &command);
            m_stage = 0;

            QMutexLocker locker(&statusLock);
            signalStrength.SetValue(0);
        }
    }

    if (m_stage == 2)
    {
        VERBOSE(VB_RECORD, LOC + "hd-pvr data ready.  Stop encoding");

        command.cmd = V4L2_ENC_CMD_STOP;
        if (ioctl(videofd, VIDIOC_ENCODER_CMD, &command) == 0)
            m_stage = 3;
        else
        {
            QMutexLocker locker(&statusLock);
            signalStrength.SetValue(50);
        }
    }

    if (m_stage == 3)
    {
        struct v4l2_format vfmt;
        memset(&vfmt, 0, sizeof(vfmt));
        vfmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        VERBOSE(VB_RECORD, LOC + "hd-pvr waiting for valid resolution");
        if ((ioctl(videofd, VIDIOC_G_FMT, &vfmt) == 0) && vfmt.fmt.pix.width)
        {
            VERBOSE(VB_RECORD, LOC + QString("hd-pvr resolution %1 x %2")
                    .arg(vfmt.fmt.pix.width).arg(vfmt.fmt.pix.height));
            m_stage = 4;
        }
        else
        {
            QMutexLocker locker(&statusLock);
            signalStrength.SetValue(75);
        }
    }

    return (m_stage == 4);
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
    if (m_usingv4l2)
    {
        if (m_driver == "hdpvr")
            isLocked = handleHDPVR(videofd);
        else
        {
            struct v4l2_tuner tuner;
            memset(&tuner, 0, sizeof(tuner));

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
    }
    else
    {
        struct video_tuner tuner;
        memset(&tuner, 0, sizeof(tuner));

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
        if (isLocked)
            signalStrength.SetValue(100);
    }

    EmitStatus();
    if (IsAllGood())
        SendMessageAllGood();
}

