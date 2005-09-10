// -*- Mode: c++ -*-
// Copyright (c) 2005, Daniel Thor Kristjansson

#include <cerrno>
#include <unistd.h>
#include <sys/ioctl.h>

#include "videodev_myth.h"
#include "mythcontext.h"
#include "pchdtvsignalmonitor.h"
#include "channel.h"
#include "atscstreamdata.h"
#include "mpegtables.h"
#include "atsctables.h"

#undef DBG_SM
#define DBG_SM(FUNC, MSG) VERBOSE(VB_CHANNEL, \
    "pcHDTVSM("<<channel->GetDevice()<<")::"<<FUNC<<": "<<MSG);

/** \fn pcHDTVSignalMonitor::pcHDTVSignalMonitor(int,uint,int)
 *  \brief Initializes signal lock and signal values.
 *
 *   Start() must be called to actually begin continuous signal
 *   monitoring. The timeout is initialized to the value of
 *   the "ATSCCheckSignalWait" setting in milliseconds, and
 *   the threshold is initialized to the value of the
 *   "ATSCCheckSignalThreshold" setting as a percentage.
 *
 *  \param _capturecardnum Recorder number to monitor,
 *                         if this is less than 0, SIGNAL events will not be
 *                         sent to the frontend even if SetNotifyFrontend(true)
 *                         is called.
 *  \param _input          Input of device to monitor.
 *  \param _channel        File descriptor for device being monitored.
 */
pcHDTVSignalMonitor::pcHDTVSignalMonitor(int db_cardnum, Channel *_channel,
                                         uint _flags, const char *_name)
    : DTVSignalMonitor(db_cardnum, _channel, _flags, _name),
      usingv4l2(false), dtvMonitorRunning(false)
{
    int wait      = gContext->GetNumSetting("ATSCCheckSignalWait", 5000);
    int threshold = gContext->GetNumSetting("ATSCCheckSignalThreshold", 65);

    signalLock.SetTimeout(wait);
    signalStrength.SetTimeout(wait);
    signalStrength.SetThreshold(threshold);

    struct v4l2_capability vcap;
    bzero(&vcap, sizeof(vcap));
    usingv4l2 = (ioctl(channel->GetFd(), VIDIOC_QUERYCAP, &vcap) >= 0) &&
        (vcap.capabilities & V4L2_CAP_VIDEO_CAPTURE);
}

pcHDTVSignalMonitor::~pcHDTVSignalMonitor()
{
    Stop();
}

/** \fn pcHDTVSignalMonitor::Stop()
 *  \brief Stops signal monitoring and table monitoring threads.
 */
void pcHDTVSignalMonitor::Stop()
{
    DBG_SM("Stop()", "begin");
    SignalMonitor::Stop();
    if (dtvMonitorRunning)
    {
        dtvMonitorRunning = false;
        pthread_join(table_monitor_thread, NULL);
    }
    DBG_SM("Stop()", "end");
}

void *pcHDTVSignalMonitor::TableMonitorThread(void *param)
{
    pcHDTVSignalMonitor *mon = (pcHDTVSignalMonitor*) param;
    mon->RunTableMonitor();
    return NULL;
}

void pcHDTVSignalMonitor::RunTableMonitor()
{
    dtvMonitorRunning = true;
    int remainder = 0;
    int buffer_size = TSPacket::SIZE * 15000;
    unsigned char *buffer = new unsigned char[buffer_size];
    if (!buffer)
        return;
    bzero(buffer, buffer_size);

    DBG_SM("RunTableMonitor()", "begin (# of pids "
            <<GetStreamData()->ListeningPIDs().size()<<")");
    while (dtvMonitorRunning && GetStreamData())
    {
        long long len = read(
            channel->GetFd(), &(buffer[remainder]), buffer_size - remainder);

        if ((0 == len) || (-1 == len))
        {
            usleep(100);
            continue;
        }

        len += remainder;
        remainder = GetStreamData()->ProcessData(buffer, len);
        if (remainder > 0) // leftover bytes
            memmove(buffer, &(buffer[buffer_size - remainder]), remainder);
    }
    DBG_SM("RunTableMonitor()", "end");
}

#define EMIT(SIGNAL_FUNC, SIGNAL_VAL) \
    do { statusLock.lock(); \
         SignalMonitorValue val = SIGNAL_VAL; \
         statusLock.unlock(); \
         emit SIGNAL_FUNC(val); } while (false)

/** \fn pcHDTVSignalMonitor::UpdateValues()
 *  \brief Queries signal strength and emits status Qt signals.
 *
 *   This uses GetSignal(int,uint,bool) to actually collect the signal values.
 *   It is automatically called by MonitorLoop(), after Start() has been uset
 *   to start the signal monitoring thread.
 */
void pcHDTVSignalMonitor::UpdateValues()
{
    if (dtvMonitorRunning)
    {
        EMIT(StatusSignalLock, signalLock);
        EMIT(StatusSignalStrength, signalStrength);
        // TODO dtv signals...
        update_done = true;
        return;
    }

    bool isLocked = false;
    int sig = GetSignal(
        channel->GetFd(), channel->GetCurrentInputNum(), usingv4l2);

    {
        QMutexLocker locker(&statusLock);
        signalStrength.SetValue(sig);
        signalLock.SetValue(signalStrength.IsGood());
        isLocked = signalLock.IsGood();
    }

    if (isLocked && GetStreamData() &&
        HasAnyFlag(kDTVSigMon_WaitForPAT | kDTVSigMon_WaitForPMT |
                   kDTVSigMon_WaitForMGT | kDTVSigMon_WaitForVCT |
                   kDTVSigMon_WaitForNIT | kDTVSigMon_WaitForSDT))
    {
        pthread_create(&table_monitor_thread, NULL,
                       TableMonitorThread, this);
        DBG_SM("UpdateValues", "Waiting for table monitor to start");
        while (!dtvMonitorRunning)
            usleep(50);
        DBG_SM("UpdateValues", "Table monitor started");
    }

    EMIT(StatusSignalLock, signalLock);
    EMIT(StatusSignalStrength, signalStrength);
    update_done = true;
}

#undef EMIT

template<typename V>
V clamp(V val, V minv, V maxv) { return std::min(maxv, std::max(minv, val)); }

/** \fn pcHDTVSignalMonitor::GetSignal(fd,uint,bool)
 *  \brief Returns ATSC signal strength as a percentage from 0 to 100%.
 *
 *  \param fd        File descriptor of V4L device.
 *  \param input     Input of device to monitor.
 *  \param usingv4l2 If true use V4L version 2 ioctls,
 *                   else use V4L version 1 ioctls.
 *  \return ATSC signal strength 0-100. >75 is good.
 */
int pcHDTVSignalMonitor::GetSignal(int fd, uint input, bool usingv4l2)
{
    // the 0 == input test works around a bug in the pcHDTV v4l2 support
    if (usingv4l2 && (0 == input))
    {
        struct v4l2_tuner vsig;
        memset(&vsig, 0, sizeof(vsig));

        //setting index doesn't currently work with pcHDTV drivers....
        //vsig.index = input;

        int ioctlval = ioctl(fd, VIDIOC_G_TUNER, &vsig);
        if (ioctlval != -1)
        {
            VERBOSE(VB_CHANNEL,
                    QString("pcHDTV::GetSignal_v4l2(fd %1, input %2, v4l%3): "
                            "raw signal(%4)")
                    .arg(fd).arg(input).arg(usingv4l2 ? 2 : 1).arg(vsig.signal));

            return clamp(vsig.signal, 0, 100);
        }
        else
        {
            VERBOSE(VB_IMPORTANT,
                    QString("pcHDTV::GetSignal_v4l2(fd %1, input %2, v4l%3): "
                            "error(%4)")
                    .arg(fd).arg(input).arg(usingv4l2 ? 2 : 1).arg(strerror(errno)));
            // falling through to v4l v1
        }
    }

    struct video_signal vsig;
    memset(&vsig, 0, sizeof(vsig));

    int ioctlval = ioctl(fd, VIDIOCGSIGNAL, &vsig);
    if (ioctlval == -1) 
    {
        VERBOSE(VB_IMPORTANT,
                QString("pcHDTV::GetSignal_v4l1(fd %1, input %2, v4l%3): "
                        "error(%4)")
                .arg(fd).arg(input).arg(usingv4l2 ? 2 : 1).arg(strerror(errno)));
        return 0;
    }

    int signal = (input == 0) ? vsig.strength : vsig.aux;
    int retsig = 0;

    if ((signal & 0xff) == 0x43)
        retsig = clamp(101 - (signal >> 9), 0, 100);

    VERBOSE(VB_CHANNEL,
            QString("pcHDTV::GetSignal_v4l1(fd %1, input %2, v4l%3): "
                    "processed signal(%4)")
            .arg(fd).arg(input).arg(usingv4l2 ? 2 : 1).arg(retsig));

    return retsig;
}
