// -*- Mode: c++ -*-
// Copyright (c) 2005, Daniel Thor Kristjansson

// C headers
#include <csignal>
#include <sys/types.h>
#include <unistd.h>

// MythTV headers
#include "libmyth/mythcontext.h"
#include "libmythbase/compat.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythlogging.h"

#include "scriptsignalmonitor.h"
#include "signalmonitor.h"
#include "tv_rec.h"

extern "C" {
#include "libavcodec/avcodec.h"
}

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

#ifdef USING_SATIP
#   include "satipsignalmonitor.h"
#   include "satipchannel.h"
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

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define DBG_SM(FUNC, MSG) LOG(VB_CHANNEL, LOG_DEBUG, \
    QString("SigMon[%1](%2)::%3: %4").arg(m_inputid) \
                              .arg(m_channel->GetDevice(), FUNC, MSG))

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

SignalMonitor *SignalMonitor::Init([[maybe_unused]] const QString& cardtype,
                                   [[maybe_unused]] int db_cardnum,
                                   [[maybe_unused]] ChannelBase *channel,
                                   bool release_stream)
{
    SignalMonitor *signalMonitor = nullptr;

    if (cardtype == "GuaranteedToFail")
    {
        // This lets all the conditionally compiled tests be set up as
        // 'else if' statements
    }
#ifdef USING_DVB
    else if (CardUtil::IsDVBInputType(cardtype))
    {
        auto *dvbc = dynamic_cast<DVBChannel*>(channel);
        if (dvbc)
            signalMonitor = new DVBSignalMonitor(db_cardnum, dvbc,
                                                 release_stream);
    }
#endif

#ifdef USING_V4L2
    else if ((cardtype.toUpper() == "HDPVR"))
    {
        auto *chan = dynamic_cast<V4LChannel*>(channel);
        if (chan)
            signalMonitor = new AnalogSignalMonitor(db_cardnum, chan,
                                                    release_stream);
    }
    else if (cardtype.toUpper() == "V4L2ENC")
    {
        auto *chan = dynamic_cast<V4LChannel*>(channel);
        if (chan)
            signalMonitor = new V4L2encSignalMonitor(db_cardnum, chan,
                                                     release_stream);
    }
#endif

#ifdef USING_HDHOMERUN
    else if (cardtype.toUpper() == "HDHOMERUN")
    {
        auto *hdhrc = dynamic_cast<HDHRChannel*>(channel);
        if (hdhrc)
            signalMonitor = new HDHRSignalMonitor(db_cardnum, hdhrc,
                                                  release_stream);
    }
#endif

#ifdef USING_SATIP
    else if (cardtype.toUpper() == "SATIP")
    {
        auto *satipchan = dynamic_cast<SatIPChannel*>(channel);
        if (satipchan)
            signalMonitor = new SatIPSignalMonitor(db_cardnum, satipchan, release_stream);
    }
#endif

#ifdef USING_CETON
    else if (cardtype.toUpper() == "CETON")
    {
        auto *cetonchan = dynamic_cast<CetonChannel*>(channel);
        if (cetonchan)
            signalMonitor = new CetonSignalMonitor(db_cardnum, cetonchan,
                                                   release_stream);
    }
#endif

#ifdef USING_IPTV
    else if (cardtype.toUpper() == "FREEBOX")
    {
        auto *fbc = dynamic_cast<IPTVChannel*>(channel);
        if (fbc)
            signalMonitor = new IPTVSignalMonitor(db_cardnum, fbc,
                                                  release_stream);
    }
#endif

#ifdef USING_VBOX
    else if (cardtype.toUpper() == "VBOX")
    {
        auto *fbc = dynamic_cast<IPTVChannel*>(channel);
        if (fbc)
            signalMonitor = new IPTVSignalMonitor(db_cardnum, fbc,
                                                  release_stream);
    }
#endif

#ifdef USING_FIREWIRE
    else if (cardtype.toUpper() == "FIREWIRE")
    {
        auto *fc = dynamic_cast<FirewireChannel*>(channel);
        if (fc)
            signalMonitor = new FirewireSignalMonitor(db_cardnum, fc,
                                                      release_stream);
    }
#endif

#ifdef USING_ASI
    else if (cardtype.toUpper() == "ASI")
    {
        auto *fc = dynamic_cast<ASIChannel*>(channel);
        if (fc)
            signalMonitor = new ASISignalMonitor(db_cardnum, fc,
                                                 release_stream);
    }
#endif

    else if (cardtype.toUpper() == "EXTERNAL")
    {
        auto *fc = dynamic_cast<ExternalChannel*>(channel);
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
                .arg(cardtype).arg(db_cardnum).arg((uintptr_t)channel,0,16));
    }

    return signalMonitor;
}

/**
 *  \brief Initializes signal lock and signal values.
 *
 *   Start() must be called to actually begin continuous
 *   signal monitoring.
 *
 *  \param _inputid        Recorder number to monitor, if this is less than 0,
 *                         SIGNAL events will not be sent to the frontend even
 *                         if SetNotifyFrontend(true) is called.
 *  \param _channel        ChannelBase class for our monitoring
 *  \param wait_for_mask   SignalMonitorFlags to start with.
 */
SignalMonitor::SignalMonitor(int _inputid, ChannelBase *_channel,
                             bool _release_stream, uint64_t wait_for_mask)
    : MThread("SignalMonitor"),
      m_channel(_channel),
      m_inputid(_inputid), m_flags(wait_for_mask),
      m_releaseStream(_release_stream),
      m_signalLock    (QCoreApplication::translate("(Common)", "Signal Lock"),
                     "slock", 1, true, 0,   1, 0ms),
      m_signalStrength(QCoreApplication::translate("(Common)", "Signal Power"),
                     "signal", 0, true, 0, 100, 0ms),
      m_scriptStatus  (QCoreApplication::translate("(Common)", "Script Status"),
                     "script", 3, true, 0, 3, 0ms)
{
    if (!m_channel->IsExternalChannelChangeInUse())
    {
        m_scriptStatus.SetValue(3);
    }
}

/** \fn SignalMonitor::~SignalMonitor()
 *  \brief Stops monitoring thread.
 */
SignalMonitor::~SignalMonitor()
{
    SignalMonitor::Stop();
    wait();
}

void SignalMonitor::AddFlags(uint64_t _flags)
{
    DBG_SM("AddFlags", sm_flags_to_string(_flags));
    m_flags |= _flags;
}

void SignalMonitor::RemoveFlags(uint64_t _flags)
{
    DBG_SM("RemoveFlags", sm_flags_to_string(_flags));
    m_flags &= ~_flags;
}

bool SignalMonitor::HasFlags(uint64_t _flags) const
{
    return (m_flags & _flags) == _flags;
}

bool SignalMonitor::HasAnyFlag(uint64_t _flags) const
{
    return (m_flags & _flags) != 0U;
}

/** \fn SignalMonitor::Start()
 *  \brief Start signal monitoring thread.
 */
void SignalMonitor::Start()
{
    DBG_SM("Start", "begin");
    {
        QMutexLocker locker(&m_startStopLock);
        m_exit = false;
        start();
        while (!m_running)
            m_startStopWait.wait(locker.mutex());
    }
    DBG_SM("Start", "end");
}

/** \fn SignalMonitor::Stop()
 *  \brief Stop signal monitoring thread.
 */
void SignalMonitor::Stop()
{
    DBG_SM("Stop", "begin");

    QMutexLocker locker(&m_startStopLock);
    m_exit = true;
    if (m_running)
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
    m_statusLock.lock();
    list<<m_scriptStatus.GetName()<<m_scriptStatus.GetStatus();
    list<<m_signalLock.GetName()<<m_signalLock.GetStatus();
    if (HasFlags(kSigMon_WaitForSig))
        list<<m_signalStrength.GetName()<<m_signalStrength.GetStatus();
    m_statusLock.unlock();

    return list;
}

/// \brief Basic signal monitoring loop
void SignalMonitor::run(void)
{
    RunProlog();

    QMutexLocker locker(&m_startStopLock);
    m_running = true;
    m_startStopWait.wakeAll();

    while (!m_exit)
    {
        locker.unlock();

        UpdateValues();

        if (m_notifyFrontend && m_inputid>=0)
        {
            QStringList slist = GetStatusList();
            MythEvent me(QString("SIGNAL %1").arg(m_inputid), slist);
            gCoreContext->dispatch(me);
        }

        locker.relock();
        m_startStopWait.wait(locker.mutex(), m_updateRate.count());
    }

    // We need to send a last informational message because a
    // signal update may have come in while we were sleeping
    // if we are using the multithreaded dtvsignalmonitor.
    locker.unlock();
    if (m_notifyFrontend && m_inputid>=0)
    {
        QStringList slist = GetStatusList();
        MythEvent me(QString("SIGNAL %1").arg(m_inputid), slist);
        gCoreContext->dispatch(me);
    }
    locker.relock();

    m_running = false;
    m_startStopWait.wakeAll();

    RunEpilog();
}

void SignalMonitor::AddListener(SignalMonitorListener *listener)
{
    QMutexLocker locker(&m_listenerLock);
    for (auto & entry : m_listeners)
    {
        if (entry == listener)
            return;
    }
    m_listeners.push_back(listener);
}

void SignalMonitor::RemoveListener(SignalMonitorListener *listener)
{
    QMutexLocker locker(&m_listenerLock);

    std::vector<SignalMonitorListener*> new_listeners;
    for (auto & entry : m_listeners)
    {
        if (entry != listener)
            new_listeners.push_back(entry);
    }

    m_listeners = new_listeners;
}

void SignalMonitor::SendMessage(
    SignalMonitorMessageType type, const SignalMonitorValue &value)
{
    m_statusLock.lock();
    const SignalMonitorValue& val = value;
    m_statusLock.unlock();

    QMutexLocker locker(&m_listenerLock);
    for (auto *listener : m_listeners)
    {
        auto *dvblistener = dynamic_cast<DVBSignalMonitorListener*>(listener);

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
    QMutexLocker locker(&m_statusLock);
    if (m_scriptStatus.GetValue() < 2)
    {
        m_scriptStatus.SetValue(m_channel->GetScriptStatus());
    }
}

void SignalMonitor::SendMessageAllGood(void)
{
    QMutexLocker locker(&m_listenerLock);
    for (auto & entry : m_listeners)
        entry->AllGood();
}

void SignalMonitor::EmitStatus(void)
{
    SendMessage(kStatusChannelTuned, m_scriptStatus);
    SendMessage(kStatusSignalLock, m_signalLock);
    if (HasFlags(kSigMon_WaitForSig))
        SendMessage(kStatusSignalStrength,    m_signalStrength);
}
