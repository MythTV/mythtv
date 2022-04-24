// -*- Mode: c++ -*-
// Copyright (c) 2006, Daniel Thor Kristjansson

#include <chrono> // for milliseconds
#include <fcntl.h>
#include <sys/select.h>
#include <thread> // for sleep_for

#include "libmythbase/mythdbcon.h"
#include "libmythbase/mythlogging.h"

#include "firewirechannel.h"
#include "firewiresignalmonitor.h"
#include "mpeg/atscstreamdata.h"
#include "mpeg/atsctables.h"
#include "mpeg/mpegtables.h"

#define LOC QString("FireSigMon[%1](%2): ") \
            .arg(m_inputid).arg(m_channel->GetDevice())

void FirewireTableMonitorThread::run(void)
{
    RunProlog();
    m_parent->RunTableMonitor();
    RunEpilog();
}

QHash<void*,uint> FirewireSignalMonitor::s_patKeys;
QMutex            FirewireSignalMonitor::s_patKeysLock;

/** \fn FirewireSignalMonitor::FirewireSignalMonitor(int,FirewireChannel*,uint64_t)
 *  \brief Initializes signal lock and signal values.
 *
 *   Start() must be called to actually begin continuous
 *   signal monitoring. The timeout is set to 3 seconds,
 *   and the signal threshold is initialized to 0%.
 *
 *  \param db_cardnum Recorder number to monitor,
 *                    if this is less than 0, SIGNAL events will not be
 *                    sent to the frontend even if SetNotifyFrontend(true)
 *                    is called.
 *  \param _channel FirewireChannel for card
 *  \param _flags   Flags to start with
 */
FirewireSignalMonitor::FirewireSignalMonitor(int db_cardnum,
                                             FirewireChannel *_channel,
                                             bool _release_stream,
                                             uint64_t _flags)
    : DTVSignalMonitor(db_cardnum, _channel, _release_stream, _flags)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "ctor");

    m_signalStrength.SetThreshold(65);

    AddFlags(kSigMon_WaitForSig);

    m_stbNeedsRetune =
        (FirewireDevice::kAVCPowerOff == _channel->GetPowerState());
}

/** \fn FirewireSignalMonitor::~FirewireSignalMonitor()
 *  \brief Stops signal monitoring and table monitoring threads.
 */
FirewireSignalMonitor::~FirewireSignalMonitor()
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "dtor");
    FirewireSignalMonitor::Stop();
}

/** \fn FirewireSignalMonitor::Stop(void)
 *  \brief Stop signal monitoring and table monitoring threads.
 */
void FirewireSignalMonitor::Stop(void)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Stop() -- begin");
    SignalMonitor::Stop();
    if (m_tableMonitorThread)
    {
        m_dtvMonitorRunning = false;
        m_tableMonitorThread->wait();
        delete m_tableMonitorThread;
        m_tableMonitorThread = nullptr;
    }
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Stop() -- end");
}

void FirewireSignalMonitor::HandlePAT(const ProgramAssociationTable *pat)
{
    AddFlags(kDTVSigMon_PATSeen);

    auto *fwchan = dynamic_cast<FirewireChannel*>(m_channel);
    if (!fwchan)
        return;

    bool crc_bogus = !fwchan->GetFirewireDevice()->IsSTBBufferCleared();
    if (crc_bogus && m_stbNeedsToWaitForPat &&
        (m_stbWaitForPatTimer.elapsed() < kBufferTimeout))
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC + "HandlePAT() ignoring PAT");
        uint tsid = pat->TransportStreamID();
        GetStreamData()->SetVersionPAT(tsid, -1,0);
        return;
    }

    if (crc_bogus && m_stbNeedsToWaitForPat)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Wait for valid PAT timed out");
        m_stbNeedsToWaitForPat = false;
    }

    DTVSignalMonitor::HandlePAT(pat);
}

void FirewireSignalMonitor::HandlePMT(uint pnum, const ProgramMapTable *pmt)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "HandlePMT()");

    AddFlags(kDTVSigMon_PMTSeen);

    if (!HasFlags(kDTVSigMon_PATMatch))
    {
        GetStreamData()->SetVersionPMT(pnum, -1,0);
        LOG(VB_CHANNEL, LOG_INFO, LOC + "HandlePMT() ignoring PMT");
        return;
    }

    DTVSignalMonitor::HandlePMT(pnum, pmt);
}

void FirewireSignalMonitor::RunTableMonitor(void)
{
    m_stbNeedsToWaitForPat = true;
    m_stbWaitForPatTimer.start();
    m_dtvMonitorRunning = true;

    LOG(VB_CHANNEL, LOG_INFO, LOC + "RunTableMonitor(): -- begin");

    auto *lchan = dynamic_cast<FirewireChannel*>(m_channel);
    if (!lchan)
    {
        // clang-tidy assumes that if the result of dynamic_cast is
        // nullptr that the thing being casted must also be a nullptr.
        // NOLINTNEXTLINE(clang-analyzer-core.CallAndMessage)
        LOG(VB_CHANNEL, LOG_INFO, LOC + "RunTableMonitor(): -- err");
        while (m_dtvMonitorRunning)
            std::this_thread::sleep_for(10ms);

        LOG(VB_CHANNEL, LOG_INFO, LOC + "RunTableMonitor(): -- err end");
        return;
    }

    FirewireDevice *dev = lchan->GetFirewireDevice();

    dev->OpenPort();
    dev->AddListener(this);

    while (m_dtvMonitorRunning && GetStreamData())
        std::this_thread::sleep_for(10ms);

    LOG(VB_CHANNEL, LOG_INFO, LOC + "RunTableMonitor(): -- shutdown ");

    dev->RemoveListener(this);
    dev->ClosePort();

    while (m_dtvMonitorRunning)
        std::this_thread::sleep_for(10ms);

    LOG(VB_CHANNEL, LOG_INFO, LOC + "RunTableMonitor(): -- end");
}

void FirewireSignalMonitor::AddData(const unsigned char *data, uint len)
{
    if (!m_dtvMonitorRunning)
        return;

    if (GetStreamData())
        GetStreamData()->ProcessData((unsigned char *)data, len);
}

/** \fn FirewireSignalMonitor::UpdateValues(void)
 *  \brief Fills in frontend stats and emits status Qt signals.
 *
 *   This function uses five ioctl's FE_READ_SNR, FE_READ_SIGNAL_STRENGTH
 *   FE_READ_BER, FE_READ_UNCORRECTED_BLOCKS, and FE_READ_STATUS to obtain
 *   statistics from the frontend.
 *
 *   This is automatically called by run(), after Start()
 *   has been used to start the signal monitoring thread.
 */
void FirewireSignalMonitor::UpdateValues(void)
{
    if (!m_running || m_exit)
        return;

    if (m_dtvMonitorRunning)
    {
        EmitStatus();
        if (IsAllGood())
            SendMessageAllGood();
        // TODO dtv signals...

        m_updateDone = true;
        return;
    }

    if (m_stbNeedsToWaitForPower &&
        (m_stbWaitForPowerTimer.elapsed() < kPowerTimeout))
    {
        return;
    }
    m_stbNeedsToWaitForPower = false;

    auto *fwchan = dynamic_cast<FirewireChannel*>(m_channel);
    if (!fwchan)
        return;

    if (HasFlags(kFWSigMon_WaitForPower) && !HasFlags(kFWSigMon_PowerMatch))
    {
        bool retried = false;
        while (true)
        {
            FirewireDevice::PowerState power = fwchan->GetPowerState();
            if (FirewireDevice::kAVCPowerOn == power)
            {
                AddFlags(kFWSigMon_PowerSeen | kFWSigMon_PowerMatch);
            }
            else if (FirewireDevice::kAVCPowerOff == power)
            {
                AddFlags(kFWSigMon_PowerSeen);
                fwchan->SetPowerState(true);
                m_stbWaitForPowerTimer.start();
                m_stbNeedsToWaitForPower = true;
            }
            else
            {
                bool qfailed = (FirewireDevice::kAVCPowerQueryFailed == power);
                if (qfailed && !retried)
                {
                    retried = true;
                    continue;
                }

                LOG(VB_RECORD, LOG_WARNING,
                    "Can't determine if STB is power on, assuming it is...");
                AddFlags(kFWSigMon_PowerSeen | kFWSigMon_PowerMatch);
            }
            break;
        }
    }

    bool isLocked = !HasFlags(kFWSigMon_WaitForPower) ||
        HasFlags(kFWSigMon_WaitForPower | kFWSigMon_PowerMatch);

    if (isLocked && m_stbNeedsRetune)
    {
        fwchan->Retune();
        isLocked = m_stbNeedsRetune = false;
    }

    SignalMonitor::UpdateValues();

    {
        QMutexLocker locker(&m_statusLock);
        if (!m_scriptStatus.IsGood())
            return;
    }

    // Set SignalMonitorValues from info from card.
    {
        QMutexLocker locker(&m_statusLock);
        m_signalStrength.SetValue(isLocked ? 100 : 0);
        m_signalLock.SetValue(isLocked ? 1 : 0);
    }

    EmitStatus();
    if (IsAllGood())
        SendMessageAllGood();

    // Start table monitoring if we are waiting on any table
    // and we have a lock.
    if (isLocked && GetStreamData() &&
        HasAnyFlag(kDTVSigMon_WaitForPAT | kDTVSigMon_WaitForPMT |
                   kDTVSigMon_WaitForMGT | kDTVSigMon_WaitForVCT |
                   kDTVSigMon_WaitForNIT | kDTVSigMon_WaitForSDT))
    {
        m_tableMonitorThread = new FirewireTableMonitorThread(this);

        LOG(VB_CHANNEL, LOG_INFO, LOC + "UpdateValues() -- "
                "Waiting for table monitor to start");

        while (!m_dtvMonitorRunning)
            std::this_thread::sleep_for(5ms);

        LOG(VB_CHANNEL, LOG_INFO, LOC + "UpdateValues() -- "
                "Table monitor started");
    }

    m_updateDone = true;
}
