// -*- Mode: c++ -*-
// Copyright (c) 2005, Daniel Thor Kristjansson

#include <cerrno>
#include <unistd.h>
#include <sys/ioctl.h>
#include <poll.h>

#ifdef USING_V4L1
#include <linux/videodev.h>
#endif // USING_V4L1

#include "mythlogging.h"
#include "analogsignalmonitor.h"
#include "v4lchannel.h"

#define LOC QString("AnalogSM[%1](%2): ") \
            .arg(capturecardnum).arg(channel->GetDevice())

AnalogSignalMonitor::AnalogSignalMonitor(
    int db_cardnum, V4LChannel *_channel, uint64_t _flags) :
    SignalMonitor(db_cardnum, _channel, _flags),
    m_usingv4l2(false), m_version(0), m_width(0), m_stable_time(2000),
    m_lock_cnt(0), m_lock_timeout(1000 * 30 /* 30 seconds */)
{
    int videofd = channel->GetFd();
    if (videofd >= 0)
    {
        uint32_t caps;
        if (!CardUtil::GetV4LInfo(videofd, m_card, m_driver, m_version, caps))
        {
            videofd = -1;
            return;
        }
        m_usingv4l2 = !!(caps & V4L2_CAP_VIDEO_CAPTURE);
        LOG(VB_RECORD, LOG_INFO, QString("card '%1' driver '%2' version '%3'")
                .arg(m_card).arg(m_driver).arg(m_version));
    }
}

bool AnalogSignalMonitor::VerifyHDPVRaudio(int videofd)
{
    struct v4l2_queryctrl qctrl;
    qctrl.id = V4L2_CID_MPEG_AUDIO_ENCODING;

    int audtype = V4L2_MPEG_AUDIO_ENCODING_AC3;

    if (ioctl(videofd, VIDIOC_QUERYCTRL, &qctrl) != 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Unable to get supported audio codecs for verification." + ENO);
        return false;
    }

    int  current_audio;

    struct v4l2_ext_control  ext_ctrl;
    struct v4l2_ext_controls ext_ctrls;

    memset(&ext_ctrl, 0, sizeof(struct v4l2_ext_control));
    ext_ctrl.id = V4L2_CID_MPEG_AUDIO_ENCODING;

    ext_ctrls.reserved[0] = ext_ctrls.reserved[1] = 0;
    ext_ctrls.count = 1;
    ext_ctrls.ctrl_class = V4L2_CTRL_CLASS_MPEG;
    ext_ctrls.controls = &ext_ctrl;

    if (ioctl(videofd, VIDIOC_G_EXT_CTRLS, &ext_ctrls) != 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Unable to get current audio codecs for verification." + ENO);
        return false;
    }

    current_audio = ext_ctrls.controls->value;

    if (audtype != current_audio)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Audio desired %1, current %2 "
                                               "min %3 max %4")
            .arg(audtype)
            .arg(current_audio)
            .arg(qctrl.minimum)
            .arg(qctrl.maximum)
            );

        ext_ctrl.id = V4L2_CID_MPEG_AUDIO_ENCODING;
        ext_ctrl.value = audtype;
        if (ioctl(videofd, VIDIOC_S_EXT_CTRLS, &ext_ctrls) == 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("Changed audio encoding "
                                                   "from %1 to %2.")
                .arg(current_audio)
                .arg(audtype)
                );
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to changed audio "
                                                   "encoding from %1 to %2."
                                                   + ENO)
                .arg(current_audio)
                .arg(audtype)
                );
        }

        return false;
    }

    return true;
}

/* m_stable_time is used to designate how long we need to see a stable
 * resolution reported from the HD-PVR driver, before we consider it a
 * good lock.  In my testing 2 seconds is safe, while 1 second worked
 * most of the time.  --jp
 */
bool AnalogSignalMonitor::handleHDPVR(int videofd)
{
    struct v4l2_format vfmt;
    memset(&vfmt, 0, sizeof(vfmt));
    vfmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if ((ioctl(videofd, VIDIOC_G_FMT, &vfmt) == 0) &&
        vfmt.fmt.pix.width && m_width == vfmt.fmt.pix.width &&
        VerifyHDPVRaudio(videofd))
    {
        if (!m_timer.isRunning())
        {
            LOG(VB_RECORD, LOG_ERR, QString("hd-pvr resolution %1 x %2")
                .arg(vfmt.fmt.pix.width).arg(vfmt.fmt.pix.height));
            ++m_lock_cnt;
            m_timer.start();
        }
        else if (m_timer.elapsed() > m_stable_time)
        {
            LOG(VB_RECORD, LOG_ERR, QString("hd-pvr stable at %1 x %2")
                .arg(vfmt.fmt.pix.width).arg(vfmt.fmt.pix.height));
            m_timer.stop();
            return true;
        }
        else
        {
            QMutexLocker locker(&statusLock);
            signalStrength.SetValue(60 + m_lock_cnt);
        }
    }
    else
    {
        LOG(VB_RECORD, LOG_ERR, "hd-pvr waiting for valid resolution");
        m_width = vfmt.fmt.pix.width;
        m_timer.stop();
        QMutexLocker locker(&statusLock);
        signalStrength.SetValue(20 + m_lock_cnt);
    }

    return false;
}

void AnalogSignalMonitor::UpdateValues(void)
{
    SignalMonitor::UpdateValues();

    {
        QMutexLocker locker(&statusLock);
        if (!scriptStatus.IsGood())
            return;
    }

    if (lock_timer.elapsed() > m_lock_timeout)
        error = "Timed out.";

    if (!running || exit)
        return;

    int videofd = channel->GetFd();
    if (videofd < 0)
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
                LOG(VB_GENERAL, LOG_ERR, "Failed to probe signal (v4l2)" + ENO);
            }
            else
            {
                isLocked = tuner.signal;
            }
        }
    }
#ifdef USING_V4L1
    else
    {
        struct video_tuner tuner;
        memset(&tuner, 0, sizeof(tuner));

        if (ioctl(videofd, VIDIOCGTUNER, &tuner, 0) < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, "Failed to probe signal (v4l1)" + ENO);
        }
        else
        {
            isLocked = tuner.signal;
        }
    }
#endif // USING_V4L1

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
