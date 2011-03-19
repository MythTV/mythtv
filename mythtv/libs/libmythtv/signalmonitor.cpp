// -*- Mode: c++ -*-
// Copyright (c) 2005, Daniel Thor Kristjansson

// C headers
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>

// MythTV headers
#include "mythcontext.h"
#include "tv_rec.h"
#include "signalmonitor.h"
#include "compat.h"
#include "mythverbose.h"

extern "C" {
#include "libavcodec/avcodec.h"
}
#include "../libmyth/util.h"

#ifdef USING_DVB
#   include "dvbsignalmonitor.h"
#   include "dvbchannel.h"
#endif

#ifdef USING_V4L2
#   include "analogsignalmonitor.h"
#   include "v4lchannel.h"
#endif

#ifdef USING_HDHOMERUN
#   include "hdhrsignalmonitor.h"
#   include "hdhrchannel.h"
#endif

#ifdef USING_IPTV
#   include "iptvsignalmonitor.h"
#   include "iptvchannel.h"
#endif

#ifdef USING_FIREWIRE
#   include "firewiresignalmonitor.h"
#   include "firewirechannel.h"
#endif

#include "channelchangemonitor.h"

#undef DBG_SM
#define DBG_SM(FUNC, MSG) VERBOSE(VB_CHANNEL, \
    "SM("<<channel->GetDevice()<<")::"<<FUNC<<": "<<MSG);

/** \class SignalMonitor
 *  \brief Signal monitoring base class.
 *
 *   Signal monitors should extend this to add signal monitoring to their
 *   recorder. All signal monitors must implement one signals, the
 *   StatusSignalLock(int) signal. The lock signal should only be set to
 *   true when it is absolutely safe to begin or to continue recording.
 *   The optional StatusSignalStrength signal should report the actual
 *   signal value.
 *
 *   Additional signals may be implemented, see DTVSignalMonitor and
 *   DVBSignalMonitor for example.
 *
 *  \sa AnalocSignalMonitor, DTVSignalMonitor, DVBSignalMonitor,
        HDHRSignalMonitor, SignalMonitorValue
 */

static void ALRMhandler(int /*sig*/)
{
     cerr<<"SignalMonitor: Got SIGALRM"<<endl;
     signal(SIGINT, ALRMhandler);
}

SignalMonitor *SignalMonitor::Init(QString cardtype, int db_cardnum,
                                   ChannelBase *channel)
{
    (void) cardtype;
    (void) db_cardnum;
    (void) channel;

    SignalMonitor *signalMonitor = NULL;

    {
        QMutexLocker locker(avcodeclock);
        avcodec_init();
    }

#ifdef USING_DVB
    if (CardUtil::IsDVBCardType(cardtype))
    {
        DVBChannel *dvbc = dynamic_cast<DVBChannel*>(channel);
        if (dvbc)
            signalMonitor = new DVBSignalMonitor(db_cardnum, dvbc);
    }
#endif

#ifdef USING_V4L2
    if ((cardtype.toUpper() == "HDPVR"))
    {
        V4LChannel *chan = dynamic_cast<V4LChannel*>(channel);
        if (chan)
            signalMonitor = new AnalogSignalMonitor(db_cardnum, chan);
    }
#endif

#ifdef USING_HDHOMERUN
    if (cardtype.toUpper() == "HDHOMERUN")
    {
        HDHRChannel *hdhrc = dynamic_cast<HDHRChannel*>(channel);
        if (hdhrc)
            signalMonitor = new HDHRSignalMonitor(db_cardnum, hdhrc);
    }
#endif

#ifdef USING_IPTV
    if (cardtype.toUpper() == "FREEBOX")
    {
        IPTVChannel *fbc = dynamic_cast<IPTVChannel*>(channel);
        if (fbc)
            signalMonitor = new IPTVSignalMonitor(db_cardnum, fbc);
    }
#endif

#ifdef USING_FIREWIRE
    if (cardtype.toUpper() == "FIREWIRE")
    {
        FirewireChannel *fc = dynamic_cast<FirewireChannel*>(channel);
        if (fc)
            signalMonitor = new FirewireSignalMonitor(db_cardnum, fc);
    }
#endif

    if (!signalMonitor)
    {
        // For anything else
        if (channel)
            signalMonitor = new ChannelChangeMonitor(db_cardnum, channel);
    }

    if (!signalMonitor)
    {
        VERBOSE(VB_IMPORTANT,
                QString("Failed to create signal monitor in Init(%1, %2, 0x%3)")
                .arg(cardtype).arg(db_cardnum).arg((long)channel,0,16));
    }

    return signalMonitor;
}

/**
 *  \brief Initializes signal lock and signal values.
 *
 *   Start() must be called to actually begin continuous
 *   signal monitoring.
 *
 *  \param db_cardnum Recorder number to monitor,
 *         if this is less than 0, SIGNAL events will not be
 *         sent to the frontend even if SetNotifyFrontend(true)
 *         is called.
 *  \param _channel      ChannelBase class for our monitoring
 *  \param wait_for_mask SignalMonitorFlags to start with.
 */
SignalMonitor::SignalMonitor(int _capturecardnum, ChannelBase *_channel,
                             uint64_t wait_for_mask)
    : channel(_channel),
      capturecardnum(_capturecardnum), flags(wait_for_mask),
      update_rate(25),                 minimum_update_rate(5),
      running(false),                  exit(false),
      update_done(false),              notify_frontend(true),
      is_tuned(false),                 tablemon(false),
      eit_scan(false),                 error(""),
      signalLock    (QObject::tr("Signal Lock"),  "slock",
                     1, true, 0,   1, 0),
      signalStrength(QObject::tr("Signal Power"), "signal",
                     0, true, 0, 100, 0),
      channelTuned("Channel Tuned", "tuned", 3, true, 0, 3, 0),
      statusLock(QMutex::Recursive)
{
}

/** \fn SignalMonitor::~SignalMonitor()
 *  \brief Stops monitoring thread.
 */
SignalMonitor::~SignalMonitor()
{
    Stop();
}

void SignalMonitor::AddFlags(uint64_t _flags)
{
    DBG_SM("AddFlags", sm_flags_to_string(_flags));
    flags |= _flags;
}

void SignalMonitor::RemoveFlags(uint64_t _flags)
{
    DBG_SM("RemoveFlags", sm_flags_to_string(_flags));
    flags &= ~_flags;
}

bool SignalMonitor::HasFlags(uint64_t _flags) const
{
    return (flags & _flags) == _flags;
}

bool SignalMonitor::HasAnyFlag(uint64_t _flags) const
{
    return (flags & _flags);
}

/** \fn SignalMonitor::Start()
 *  \brief Start signal monitoring thread.
 */
void SignalMonitor::Start(bool waitfor_tune)
{
    DBG_SM("Start", "begin");
    {
        QMutexLocker locker(&startStopLock);

        // When used for scanning, don't wait for the tuning thread
        is_tuned = !waitfor_tune;
        if (waitfor_tune)
        {
            m_channelTimeout = 40000; // 40 seconds (in milliseconds)
            m_channelTimer.start();
        }

        if (!running)
        {
            int rval = pthread_create(
                &monitor_thread, NULL, SpawnMonitorLoop, this);

            if (0 != rval)
            {
                VERBOSE(VB_IMPORTANT, "Failed to create signal monitor thread");
                return;
            }
        }

        while (!running)
            usleep(50);
    }
    DBG_SM("Start", "end");
}

/** \fn SignalMonitor::Stop()
 *  \brief Stop signal monitoring thread.
 */
void SignalMonitor::Stop()
{
    DBG_SM("Stop", "begin");
    {
        QMutexLocker locker(&startStopLock);
        if (running)
        {
            exit = true;
            pthread_join(monitor_thread, NULL);
        }
    }
    DBG_SM("Stop", "end");
}

/** \fn SignalMonitor::Kick()
 *  \brief Wake up monitor thread, and wait for
 *         UpdateValue() to execute once.
 */
void SignalMonitor::Kick()
{
    update_done = false;

    //pthread_kill(monitor_thread, SIGALRM);

    while (!update_done)
        usleep(50);
}

/** \fn SignalMonitor::GetStatusList(bool)
 *  \brief Returns QStringList containing all signals and their current
 *         values.
 *
 *   This serializes the signal monitoring values so that they can
 *   be passed from a backend to a frontend.
 *
 *   SignalMonitorValue::Parse(const QStringList&) will convert this
 *   to a vector of SignalMonitorValue instances.
 *
 *  \param kick if true Kick() will be employed so that this
 *         call will not have to wait for the next signal
 *         monitoring event.
 */
QStringList SignalMonitor::GetStatusList(bool kick)
{
    if (kick && running)
        Kick();
    else if (!running)
        UpdateValues();

    QStringList list;
    statusLock.lock();
    list<<channelTuned.GetName()<<channelTuned.GetStatus();
    list<<signalLock.GetName()<<signalLock.GetStatus();
    if (HasFlags(kSigMon_WaitForSig))
        list<<signalStrength.GetName()<<signalStrength.GetStatus();
    statusLock.unlock();

    return list;
}

/** \fn SignalMonitor::MonitorLoop()
 *  \brief Basic signal monitoring loop
 */
void SignalMonitor::MonitorLoop()
{
    //signal(SIGALRM, ALRMhandler);

    running = true;
    exit = false;

    while (!exit)
    {
        UpdateValues();

        if (notify_frontend && capturecardnum>=0)
        {
            QStringList slist = GetStatusList(false);
            MythEvent me(QString("SIGNAL %1").arg(capturecardnum), slist);
            gCoreContext->dispatch(me);
            //cerr<<"sent SIGNAL"<<endl;
        }

        usleep(update_rate * 1000);
    }

    // We need to send a last informational message because a
    // signal update may have come in while we were sleeping
    // if we are using the multithreaded dtvsignalmonitor.
    if (notify_frontend && capturecardnum>=0)
    {
        QStringList slist = GetStatusList(false);
        MythEvent me(QString("SIGNAL %1").arg(capturecardnum), slist);
        gCoreContext->dispatch(me);
    }

    //signal(SIGALRM, SIG_DFL);

    running = false;
}

/** \fn SignalMonitor::SpawnMonitorLoop(void*)
 *  \brief Runs MonitorLoop() within the monitor_thread pthread.
 */
void* SignalMonitor::SpawnMonitorLoop(void* self)
{
    ((SignalMonitor*)self)->MonitorLoop();
    return NULL;
}

/** \fn  SignalMonitor::WaitForLock(int)
 *  \brief Wait for a StatusSignaLock(int) of true.
 *
 *   This can be called whether or not the signal
 *   monitoring thread has been started.
 *
 *  \param timeout maximum time to wait in milliseconds.
 *  \return true if signal was acquired.
 */
bool SignalMonitor::WaitForLock(int timeout)
{
    statusLock.lock();
    if (-1 == timeout)
        timeout = signalLock.GetTimeout();
    statusLock.unlock();
    if (timeout<0)
        return false;

    MythTimer t;
    t.start();
    if (running)
    {
        while (t.elapsed()<timeout && running)
        {
            Kick();
            statusLock.lock();
            bool ok = signalLock.IsGood();
            statusLock.unlock();
            if (ok)
                return true;

            usleep(50);
        }
        if (!running)
            return WaitForLock(timeout-t.elapsed());
    }
    else
    {
        while (t.elapsed()<timeout && !running)
        {
            UpdateValues();
            statusLock.lock();
            bool ok = signalLock.IsGood();
            statusLock.unlock();
            if (ok)
                return true;

            usleep(50);
        }
        if (running)
            return WaitForLock(timeout-t.elapsed());
    }
    return false;
}

void SignalMonitor::AddListener(SignalMonitorListener *listener)
{
    QMutexLocker locker(&listenerLock);
    for (uint i = 0; i < listeners.size(); i++)
    {
        if (listeners[i] == listener)
            return;
    }
    listeners.push_back(listener);
}

void SignalMonitor::RemoveListener(SignalMonitorListener *listener)
{
    QMutexLocker locker(&listenerLock);

    vector<SignalMonitorListener*> new_listeners;
    for (uint i = 0; i < listeners.size(); i++)
    {
        if (listeners[i] != listener)
            new_listeners.push_back(listeners[i]);
    }

    listeners = new_listeners;
}

void SignalMonitor::SendMessage(
    SignalMonitorMessageType type, const SignalMonitorValue &value)
{
    statusLock.lock();
    SignalMonitorValue val = value;
    statusLock.unlock();

    QMutexLocker locker(&listenerLock);
    for (uint i = 0; i < listeners.size(); i++)
    {
        SignalMonitorListener *listener = listeners[i];
        DVBSignalMonitorListener *dvblistener =
            dynamic_cast<DVBSignalMonitorListener*>(listener);

        switch (type)
        {
        case kStatusSignalLock:
            listener->StatusSignalLock(val);
            break;
        case kAllGood:
            listener->AllGood();
            break;
        case kStatusSignalStrength:
            listener->StatusSignalStrength(val);
            break;
        case kStatusChannelTuned:
            listener->StatusChannelTuned(val);
            break;
        case kStatusSignalToNoise:
            if (dvblistener)
                dvblistener->StatusSignalToNoise(val);
            break;
        case kStatusBitErrorRate:
            if (dvblistener)
                dvblistener->StatusBitErrorRate(val);
            break;
        case kStatusUncorrectedBlocks:
            if (dvblistener)
                dvblistener->StatusUncorrectedBlocks(val);
            break;
        case kStatusRotorPosition:
            if (dvblistener)
                dvblistener->StatusRotorPosition(val);
            break;
        }
    }
}

bool SignalMonitor::IsChannelTuned(void)
{
    if (is_tuned)
        return true;

    ChannelBase::Status status = channel->GetStatus();
    QMutexLocker locker(&statusLock);

    switch (status) {
      case ChannelBase::changePending:
        if (HasFlags(SignalMonitor::kDVBSigMon_WaitForPos))
        {
            // Still waiting on rotor
            m_channelTimer.start();
            channelTuned.SetValue(1);
        }
        else if (m_channelTimer.elapsed() > m_channelTimeout)
        {
            // channel change is taking too long
            VERBOSE(VB_IMPORTANT, "SignalMonitor: channel change timed-out");
            error = QObject::tr("Error: channel change failed");
            channelTuned.SetValue(2);
        }
        else
            channelTuned.SetValue(1);
        break;
      case ChannelBase::changeFailed:
        VERBOSE(VB_IMPORTANT, "SignalMonitor: channel change failed");
        channelTuned.SetValue(2);
        error = QObject::tr("Error: channel change failed");
        break;
      case ChannelBase::changeSuccess:
        channelTuned.SetValue(3);
        break;
    }

    EmitStatus();

    if (status == ChannelBase::changeSuccess)
    {
        if (tablemon)
            pParent->SetupDTVSignalMonitor(eit_scan);

        is_tuned = true;
        return true;
    }

    return false;
}

void SignalMonitor::SendMessageAllGood(void)
{
    QMutexLocker locker(&listenerLock);
    for (uint i = 0; i < listeners.size(); i++)
        listeners[i]->AllGood();
}

void SignalMonitor::EmitStatus(void)
{
    SendMessage(kStatusChannelTuned, channelTuned);
    SendMessage(kStatusSignalLock, signalLock);
    if (HasFlags(kSigMon_WaitForSig))
        SendMessage(kStatusSignalStrength,    signalStrength);
}
