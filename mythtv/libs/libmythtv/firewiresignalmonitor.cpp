// -*- Mode: c++ -*-
// Copyright (c) 2006, Daniel Thor Kristjansson

#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/select.h>

#include "mythverbose.h"
#include "mythdbcon.h"
#include "atscstreamdata.h"
#include "mpegtables.h"
#include "atsctables.h"
#include "firewirechannel.h"
#include "firewiresignalmonitor.h"

#define LOC QString("FireSM(%1): ").arg(channel->GetDevice())
#define LOC_WARN QString("FireSM(%1), Warning: ").arg(channel->GetDevice())
#define LOC_ERR QString("FireSM(%1), Error: ").arg(channel->GetDevice())

const uint FirewireSignalMonitor::kPowerTimeout  = 3000; /* ms */
const uint FirewireSignalMonitor::kBufferTimeout = 5000; /* ms */

QMap<void*,uint> FirewireSignalMonitor::pat_keys;
QMutex           FirewireSignalMonitor::pat_keys_lock;

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
FirewireSignalMonitor::FirewireSignalMonitor(
    int db_cardnum,
    FirewireChannel *_channel,
    uint64_t _flags) :
    DTVSignalMonitor(db_cardnum, _channel, _flags),
    dtvMonitorRunning(false),
    stb_needs_retune(true),
    stb_needs_to_wait_for_pat(false),
    stb_needs_to_wait_for_power(false)
{
    VERBOSE(VB_CHANNEL, LOC + "ctor");

    signalStrength.SetThreshold(65);

    AddFlags(kSigMon_WaitForSig);

    stb_needs_retune =
        (FirewireDevice::kAVCPowerOff == _channel->GetPowerState());
}

/** \fn FirewireSignalMonitor::~FirewireSignalMonitor()
 *  \brief Stops signal monitoring and table monitoring threads.
 */
FirewireSignalMonitor::~FirewireSignalMonitor()
{
    VERBOSE(VB_CHANNEL, LOC + "dtor");
    Stop();
}

/** \fn FirewireSignalMonitor::Stop(void)
 *  \brief Stop signal monitoring and table monitoring threads.
 */
void FirewireSignalMonitor::Stop(void)
{
    VERBOSE(VB_CHANNEL, LOC + "Stop() -- begin");
    SignalMonitor::Stop();
    if (dtvMonitorRunning)
    {
        dtvMonitorRunning = false;
        pthread_join(table_monitor_thread, NULL);
    }
    VERBOSE(VB_CHANNEL, LOC + "Stop() -- end");
}

void FirewireSignalMonitor::HandlePAT(const ProgramAssociationTable *pat)
{
    AddFlags(kDTVSigMon_PATSeen);

    FirewireChannel *fwchan = dynamic_cast<FirewireChannel*>(channel);
    if (!fwchan)
        return;

    bool crc_bogus = !fwchan->GetFirewireDevice()->IsSTBBufferCleared();
    if (crc_bogus && stb_needs_to_wait_for_pat &&
        (stb_wait_for_pat_timer.elapsed() < (int)kBufferTimeout))
    {
        VERBOSE(VB_CHANNEL, LOC + "HandlePAT() ignoring PAT");
        uint tsid = pat->TransportStreamID();
        GetStreamData()->SetVersionPAT(tsid, -1,0);
        return;
    }

    if (crc_bogus && stb_needs_to_wait_for_pat)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "Wait for valid PAT timed out");
        stb_needs_to_wait_for_pat = false;
    }

    DTVSignalMonitor::HandlePAT(pat);
}

void FirewireSignalMonitor::HandlePMT(uint pnum, const ProgramMapTable *pmt)
{
    VERBOSE(VB_CHANNEL, LOC + "HandlePMT()");

    AddFlags(kDTVSigMon_PMTSeen);

    if (!HasFlags(kDTVSigMon_PATMatch))
    {
        GetStreamData()->SetVersionPMT(pnum, -1,0);
        VERBOSE(VB_CHANNEL, LOC + "HandlePMT() ignoring PMT");
        return;
    }

    DTVSignalMonitor::HandlePMT(pnum, pmt);
}

void *FirewireSignalMonitor::TableMonitorThread(void *param)
{
    FirewireSignalMonitor *mon = (FirewireSignalMonitor*) param;
    mon->RunTableMonitor();
    return NULL;
}

void FirewireSignalMonitor::RunTableMonitor(void)
{
    stb_needs_to_wait_for_pat = true;
    stb_wait_for_pat_timer.start();
    dtvMonitorRunning = true;

    VERBOSE(VB_CHANNEL, LOC + "RunTableMonitor(): -- begin");

    FirewireChannel *lchan = dynamic_cast<FirewireChannel*>(channel);
    if (!lchan)
    {
        VERBOSE(VB_CHANNEL, LOC + "RunTableMonitor(): -- err end");
        dtvMonitorRunning = false;
        return;
    }

    FirewireDevice *dev = lchan->GetFirewireDevice();

    dev->OpenPort();
    dev->AddListener(this);

    while (dtvMonitorRunning && GetStreamData())
        usleep(100000);

    VERBOSE(VB_CHANNEL, LOC + "RunTableMonitor(): -- shutdown ");

    dev->RemoveListener(this);
    dev->ClosePort();

    dtvMonitorRunning = false;

    VERBOSE(VB_CHANNEL, LOC + "RunTableMonitor(): -- end");
}

void FirewireSignalMonitor::AddData(const unsigned char *data, uint len)
{
    if (!dtvMonitorRunning)
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
 *   This is automatically called by MonitorLoop(), after Start()
 *   has been used to start the signal monitoring thread.
 */
void FirewireSignalMonitor::UpdateValues(void)
{
    if (!running || exit)
        return;

    if (!IsChannelTuned())
        return;

    if (dtvMonitorRunning)
    {
        EmitStatus();
        if (IsAllGood())
            SendMessageAllGood();
        // TODO dtv signals...

        update_done = true;
        return;
    }

    if (stb_needs_to_wait_for_power &&
        (stb_wait_for_power_timer.elapsed() < (int)kPowerTimeout))
    {
        return;
    }
    stb_needs_to_wait_for_power = false;

    FirewireChannel *fwchan = dynamic_cast<FirewireChannel*>(channel);
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
                stb_wait_for_power_timer.start();
                stb_needs_to_wait_for_power = true;
            }
            else
            {
                bool qfailed = (FirewireDevice::kAVCPowerQueryFailed == power);
                if (qfailed && !retried)
                {
                    retried = true;
                    continue;
                }

                VERBOSE(VB_RECORD, "Can't determine if STB is power on, "
                        "assuming it is...");
                AddFlags(kFWSigMon_PowerSeen | kFWSigMon_PowerMatch);
            }
            break;
        }
    }

    bool isLocked = !HasFlags(kFWSigMon_WaitForPower) ||
        HasFlags(kFWSigMon_WaitForPower | kFWSigMon_PowerMatch);

    if (isLocked && stb_needs_retune)
    {
        fwchan->Retune();
        isLocked = stb_needs_retune = false;
    }

    // Set SignalMonitorValues from info from card.
    {
        QMutexLocker locker(&statusLock);
        signalStrength.SetValue(isLocked ? 100 : 0);
        signalLock.SetValue(isLocked ? 1 : 0);
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
        pthread_create(&table_monitor_thread, NULL,
                       TableMonitorThread, this);

        VERBOSE(VB_CHANNEL, LOC + "UpdateValues() -- "
                "Waiting for table monitor to start");

        while (!dtvMonitorRunning)
            usleep(50);

        VERBOSE(VB_CHANNEL, LOC + "UpdateValues() -- "
                "Table monitor started");
    }

    update_done = true;
}
