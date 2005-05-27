#include <cerrno>
#include <sys/ioctl.h>
#include "videodev_myth.h"
#include "mythcontext.h"
#include "pchdtvsignalmonitor.h"
#include "channel.h"

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
 *                        if this is less than 0, SIGNAL events will not be
 *                        sent to the frontend even if SetNotifyFrontend(true)
 *                        is called.
 *  \param _input          Input of device to monitor.
 *  \param _channel    File descriptor for device being monitored.
 */
pcHDTVSignalMonitor::pcHDTVSignalMonitor(int _capturecardnum, uint _input,
                                         Channel *_channel):
    DTVSignalMonitor(_capturecardnum, _channel->GetFd(), 0),
    input(_input), usingv4l2(false), channel(_channel)
{
    int wait = gContext->GetNumSetting("ATSCCheckSignalWait", 5000);
    int threshold = gContext->GetNumSetting("ATSCCheckSignalThreshold", 65);

    signalLock.SetTimeout(wait);
    signalStrength.SetTimeout(wait);
    signalStrength.SetThreshold(threshold);

    /*
    cerr<<"pcHDTVSignalMonitor(input "<<input<<", fd "<<_fd_frontend<<")"<<endl;
    cerr<<"("<<signalLock.GetName()<<", ("<<signalLock.GetStatus()<<"))"<<endl;
    cerr<<"("<<signalStrength.GetName()<<", ("<<signalStrength.GetStatus()<<"))"<<endl;
    */

    struct v4l2_capability vcap;
    bzero(&vcap, sizeof(vcap));
    usingv4l2 = (ioctl(fd, VIDIOC_QUERYCAP, &vcap) >= 0) &&
        (vcap.capabilities & V4L2_CAP_VIDEO_CAPTURE);
}

/** \fn pcHDTVSignalMonitor::UpdateValues()
 *  \brief Queries signal strength and emits status Qt signals.
 *
 *  This uses GetSignal(int,uint,bool) to actually collect the signal values.
 *  It is automatically called by MonitorLoop(), after Start() has been uset
 *  to start the signal monitoring thread.
 */
void pcHDTVSignalMonitor::UpdateValues()
{
    signalStrength.SetValue(GetSignal(fd, input, usingv4l2));
    signalLock.SetValue(signalStrength.IsGood());
    
    emit StatusSignalLock(signalLock.GetValue());
    emit StatusSignalStrength(signalStrength.GetValue());

    update_done = true;
}

template<typename V>
V clamp(V val, V minv, V maxv) { return std::min(maxv, std::max(minv, val)); }

/** \fn pcHDTVSignalMonitor::GetSignal(fd,uint,bool)
 *  \brief Returns ATSC signal strength as a percentage from 0 to 100%.
 *
 *  \param fd        File descriptor of V4L device.
 *  \param input     Input of device to monitor.
 *  \param usingv4l2 If true use V4L version 2 ioctls, else use V4L version 1 ioctls.
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
            VERBOSE(VB_RECORD,
                    QString("pcHDTV::GetSignal(2): raw signal(%1)")
                    .arg(vsig.signal));

            return clamp(vsig.signal, 0, 100);
        }
        else
        {
            VERBOSE(VB_IMPORTANT,
                    QString("pcHDTV::GetSignal(2), error: %1")
                    .arg(strerror(errno)));
            // falling through to v4l v1
        }
    }

    struct video_signal vsig;
    memset(&vsig, 0, sizeof(vsig));

    int ioctlval = ioctl(fd, VIDIOCGSIGNAL, &vsig);
    if (ioctlval == -1) 
    {
        VERBOSE(VB_IMPORTANT, QString("pcHDTV::GetSignal(1), error: %1")
                .arg(strerror(errno)));
        return 0;
    }

    int signal = (input == 0) ? vsig.strength : vsig.aux;
    int retsig = 0;

    if ((signal & 0xff) == 0x43)
        retsig = clamp(101 - (signal >> 9), 0, 100);

    VERBOSE(VB_RECORD,
            QString("pcHDTV::GetSignal(1): raw signal(0x%1), processed "
                    "signal(%2)").arg(signal,0,16).arg(retsig));

    return retsig;
}
