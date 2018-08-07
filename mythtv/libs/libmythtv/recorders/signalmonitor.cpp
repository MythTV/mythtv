// -*- Mode: c++ -*-
// Copyright (c) 2005, Daniel Thor Kristjansson

// C headers
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>

// MythTV headers
#include "scriptsignalmonitor.h"
#include "signalmonitor.h"
#include "mythcontext.h"
#include "compat.h"
#include "mythlogging.h"
#include "tv_rec.h"

extern "C" {
#include "libavcodec/avcodec.h"
}
#include "mythdate.h"

#ifdef USING_DVB
#   include "dvbsignalmonitor.h"
#   include "dvbchannel.h"
#endif

#ifdef USING_V4L2
// Old
#   include "analogsignalmonitor.h"
// New
#   include "v4l2encsignalmonitor.h"
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

#ifdef USING_ASI
#   include "asisignalmonitor.h"
#   include "asichannel.h"
#endif

#ifdef USING_CETON
#   include "cetonsignalmonitor.h"
#   include "cetonchannel.h"
#endif

#include "ExternalSignalMonitor.h"
#include "ExternalChannel.h"

#undef DBG_SM
#define DBG_SM(FUNC, MSG) LOG(VB_CHANNEL, LOG_DEBUG, \
    QString("SigMon[%1](%2)::%3: %4").arg(capturecardnum) \
                              .arg(channel->GetDevice()).arg(FUNC).arg(MSG))

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

SignalMonitor *SignalMonitor::Init(QString cardtype, int db_cardnum,
                                   ChannelBase *channel,
                                   bool release_stream)
{
    (void) cardtype;
    (void) db_cardnum;
    (void) channel;

    SignalMonitor *signalMonitor = NULL;

#ifdef USING_DVB
    if (CardUtil::IsDVBInputType(cardtype))
    {
        DVBChannel *dvbc = dynamic_cast<DVBChannel*>(channel);
        if (dvbc)
            signalMonitor = new DVBSignalMonitor(db_cardnum, dvbc,
                                                 release_stream);
    }
#endif

#ifdef USING_V4L2
    if ((cardtype.toUpper() == "HDPVR"))
    {
        V4LChannel *chan = dynamic_cast<V4LChannel*>(channel);
        if (chan)
            signalMonitor = new AnalogSignalMonitor(db_cardnum, chan,
                                                    release_stream);
    }
    else if (cardtype.toUpper() == "V4L2ENC")
    {
        V4LChannel *chan = dynamic_cast<V4LChannel*>(channel);
        if (chan)
            signalMonitor = new V4L2encSignalMonitor(db_cardnum, chan,
                                                     release_stream);
    }
#endif

#ifdef USING_HDHOMERUN
    if (cardtype.toUpper() == "HDHOMERUN")
    {
        HDHRChannel *hdhrc = dynamic_cast<HDHRChannel*>(channel);
        if (hdhrc)
            signalMonitor = new HDHRSignalMonitor(db_cardnum, hdhrc,
                                                  release_stream);
    }
#endif

#ifdef USING_CETON
    if (cardtype.toUpper() == "CETON")
    {
        CetonChannel *cetonchan = dynamic_cast<CetonChannel*>(channel);
        if (cetonchan)
            signalMonitor = new CetonSignalMonitor(db_cardnum, cetonchan,
                                                   release_stream);
    }
#endif

#ifdef USING_IPTV
    if (cardtype.toUpper() == "FREEBOX")
    {
        IPTVChannel *fbc = dynamic_cast<IPTVChannel*>(channel);
        if (fbc)
            signalMonitor = new IPTVSignalMonitor(db_cardnum, fbc,
                                                  release_stream);
    }
#endif

#ifdef USING_VBOX
    if (cardtype.toUpper() == "VBOX")
    {
        IPTVChannel *fbc = dynamic_cast<IPTVChannel*>(channel);
        if (fbc)
            signalMonitor = new IPTVSignalMonitor(db_cardnum, fbc,
                                                  release_stream);
    }
#endif

#ifdef USING_FIREWIRE
    if (cardtype.toUpper() == "FIREWIRE")
    {
        FirewireChannel *fc = dynamic_cast<FirewireChannel*>(channel);
        if (fc)
            signalMonitor = new FirewireSignalMonitor(db_cardnum, fc,
                                                      release_stream);
    }
#endif

#ifdef USING_ASI
    if (cardtype.toUpper() == "ASI")
    {
        ASIChannel *fc = dynamic_cast<ASIChannel*>(channel);
        if (fc)
            signalMonitor = new ASISignalMonitor(db_cardnum, fc,
                                                 release_stream);
    }
#endif

    if (cardtype.toUpper() == "EXTERNAL")
    {
        ExternalChannel *fc = dynamic_cast<ExternalChannel*>(channel);
        if (fc)
            signalMonitor = new ExternalSignalMonitor(db_cardnum, fc,
                                                      release_stream);
    }

    if (!signalMonitor && channel)
    {
        signalMonitor = new ScriptSignalMonitor(db_cardnum, channel,
                                                release_stream);
    }

    if (!signalMonitor)
    {
        LOG(VB_GENERAL, LOG_ERR,
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
 *  \param _capturecardnum Recorder number to monitor, if this is less than 0,
 *                         SIGNAL events will not be sent to the frontend even
 *                         if SetNotifyFrontend(true) is called.
 *  \param _channel        ChannelBase class for our monitoring
 *  \param wait_for_mask   SignalMonitorFlags to start with.
 */
SignalMonitor::SignalMonitor(int _capturecardnum, ChannelBase *_channel,
                             uint64_t wait_for_mask, bool _release_stream)
    : MThread("SignalMonitor"),
      channel(_channel),               pParent(NULL),
      capturecardnum(_capturecardnum), flags(wait_for_mask),
      release_stream(_release_stream),
      update_rate(25),                 minimum_update_rate(5),
      update_done(false),              notify_frontend(true),
      tablemon(false),                 eit_scan(false),
      signalLock    (QCoreApplication::translate("(Common)", "Signal Lock"),
                     "slock", 1, true, 0,   1, 0),
      signalStrength(QCoreApplication::translate("(Common)", "Signal Power"),
                     "signal", 0, true, 0, 100, 0),
      scriptStatus  (QCoreApplication::translate("(Common)", "Script Status"),
                     "script", 3, true, 0, 3, 0),
      running(false),                  exit(false),
      statusLock(QMutex::Recursive)
{
    if (!channel->IsExternalChannelChangeInUse())
    {
        scriptStatus.SetValue(3);
    }
}

/** \fn SignalMonitor::~SignalMonitor()
 *  \brief Stops monitoring thread.
 */
SignalMonitor::~SignalMonitor()
{
    Stop();
    wait();
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
void SignalMonitor::Start()
{
    DBG_SM("Start", "begin");
    {
        QMutexLocker locker(&startStopLock);
        exit = false;
        start();
        while (!running)
            startStopWait.wait(locker.mutex());
    }
    DBG_SM("Start", "end");
}

/** \fn SignalMonitor::Stop()
 *  \brief Stop signal monitoring thread.
 */
void SignalMonitor::Stop()
{
    DBG_SM("Stop", "begin");

    QMutexLocker locker(&startStopLock);
    exit = true;
    if (running)
    {
        locker.unlock();
        wait();
    }

    DBG_SM("Stop", "end");
}

/** \brief Returns QStringList containing all signals and their current
 *         values.
 *
 *   This serializes the signal monitoring values so that they can
 *   be passed from a backend to a frontend.
 *
 *   SignalMonitorValue::Parse(const QStringList&) will convert this
 *   to a vector of SignalMonitorValue instances.
 */
QStringList SignalMonitor::GetStatusList(void) const
{
    QStringList list;
    statusLock.lock();
    list<<scriptStatus.GetName()<<scriptStatus.GetStatus();
    list<<signalLock.GetName()<<signalLock.GetStatus();
    if (HasFlags(kSigMon_WaitForSig))
        list<<signalStrength.GetName()<<signalStrength.GetStatus();
    statusLock.unlock();

    return list;
}

/// \brief Basic signal monitoring loop
void SignalMonitor::run(void)
{
    RunProlog();

    QMutexLocker locker(&startStopLock);
    running = true;
    startStopWait.wakeAll();

    while (!exit)
    {
        locker.unlock();

        UpdateValues();

        if (notify_frontend && capturecardnum>=0)
        {
            QStringList slist = GetStatusList();
            MythEvent me(QString("SIGNAL %1").arg(capturecardnum), slist);
            gCoreContext->dispatch(me);
        }

        locker.relock();
        startStopWait.wait(locker.mutex(), update_rate);
    }

    // We need to send a last informational message because a
    // signal update may have come in while we were sleeping
    // if we are using the multithreaded dtvsignalmonitor.
    locker.unlock();
    if (notify_frontend && capturecardnum>=0)
    {
        QStringList slist = GetStatusList();
        MythEvent me(QString("SIGNAL %1").arg(capturecardnum), slist);
        gCoreContext->dispatch(me);
    }
    locker.relock();

    running = false;
    startStopWait.wakeAll();

    RunEpilog();
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

void SignalMonitor::UpdateValues(void)
{
    QMutexLocker locker(&statusLock);
    if (scriptStatus.GetValue() < 2)
    {
        scriptStatus.SetValue(channel->GetScriptStatus());
    }
}

void SignalMonitor::SendMessageAllGood(void)
{
    QMutexLocker locker(&listenerLock);
    for (uint i = 0; i < listeners.size(); i++)
        listeners[i]->AllGood();
}

void SignalMonitor::EmitStatus(void)
{
    SendMessage(kStatusChannelTuned, scriptStatus);
    SendMessage(kStatusSignalLock, signalLock);
    if (HasFlags(kSigMon_WaitForSig))
        SendMessage(kStatusSignalStrength,    signalStrength);
}
