// -*- Mode: c++ -*-

#include <cerrno>
#include <cstring>
#include <cmath>

#include <unistd.h>

#include "mythcontext.h"
#include "mythdbcon.h"
#include "dvbsignalmonitor.h"
#include "dvbchannel.h"
#include "dvbstreamdata.h"
#include "atscstreamdata.h"
#include "mpegtables.h"
#include "atsctables.h"
#include "cardutil.h"

#include "dvbtypes.h"
#include "dvbchannel.h"
#include "dvbrecorder.h"
#include "dvbstreamhandler.h"

#define LOC QString("DVBSM[%1](%2): ") \
            .arg(capturecardnum).arg(channel->GetDevice())

/**
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
 *  \param _channel DVBChannel for card
 *  \param _flags   Flags to start with
 */
DVBSignalMonitor::DVBSignalMonitor(int db_cardnum, DVBChannel* _channel,
                                   uint64_t _flags)
    : DTVSignalMonitor(db_cardnum, _channel, _flags),
      // This snr setup is incorrect for API 3.x but works better
      // than int16_t range in practice, however this is correct
      // for the 4.0 DVB API which uses a uint16_t for the snr
      signalToNoise    (QObject::tr("Signal To Noise"),    "snr",
                        0,      true,      0, 65535, 0),
      bitErrorRate     (QObject::tr("Bit Error Rate"),     "ber",
                        65535,  false,     0, 65535, 0),
      uncorrectedBlocks(QObject::tr("Uncorrected Blocks"), "ucb",
                        65535,  false,     0, 65535, 0),
      rotorPosition    (QObject::tr("Rotor Progress"),     "pos",
                        100,    true,      0,   100, 0),
      streamHandlerStarted(false),
      streamHandler(NULL),
      lock_timeout(1000 * 60 * 2 /* 2 minutes */)
{
    // These two values should probably come from the database...
    int wait = 3000; // timeout when waiting on signal
    int threshold = 0; // signal strength threshold

    signalLock.SetTimeout(wait);
    signalStrength.SetTimeout(wait);
    signalStrength.SetThreshold(threshold);

    // This is incorrect for API 3.x but works better than int16_t range
    // in practice, however this is correct for the 4.0 DVB API
    signalStrength.SetRange(0, 65535);

    bool ok;
    _channel->HasLock(&ok);
    if (!ok)
        LOG(VB_GENERAL, LOG_ERR, LOC + "Cannot read DVB status" + ENO);

    uint64_t rmflags = 0;

#define DVB_IO(FLAG, METHOD, MSG) \
  do { if (HasFlags(FLAG)) { bool ok; _channel->METHOD(&ok); \
          if (!ok) { \
              LOG(VB_GENERAL, LOG_WARNING, LOC+"Cannot "+MSG+ENO); \
              rmflags |= FLAG; } \
          else { \
              LOG(VB_CHANNEL, LOG_INFO, LOC + "Can " + MSG); } } } while (false)

    DVB_IO(kSigMon_WaitForSig, GetSignalStrength,
           "measure Signal Strength");
    DVB_IO(kDVBSigMon_WaitForSNR, GetSNR,
           "measure S/N");
    DVB_IO(kDVBSigMon_WaitForBER, GetBitErrorRate,
           "measure Bit Error Rate");
    DVB_IO(kDVBSigMon_WaitForUB, GetUncorrectedBlockCount,
           "count Uncorrected Blocks");

#undef DVB_IO

    RemoveFlags(rmflags);

    LOG(VB_CHANNEL, LOG_INFO, LOC + "DVBSignalMonitor::ctor " +
            QString("initial flags %1").arg(sm_flags_to_string(flags)));

    minimum_update_rate = _channel->GetMinSignalMonitorDelay();
    if (minimum_update_rate > 30)
        usleep(minimum_update_rate * 1000);

    streamHandler = DVBStreamHandler::Get(_channel->GetCardNum());
}

/** \fn DVBSignalMonitor::~DVBSignalMonitor()
 *  \brief Stops signal monitoring and table monitoring threads.
 */
DVBSignalMonitor::~DVBSignalMonitor()
{
    Stop();
    DVBStreamHandler::Return(streamHandler);
}

// documented in dtvsignalmonitor.h
void DVBSignalMonitor::SetRotorTarget(float target)
{
    QMutexLocker locker(&statusLock);
    rotorPosition.SetThreshold((int)roundf(100 * target));
}

void DVBSignalMonitor::GetRotorStatus(bool &was_moving, bool &is_moving)
{
    DVBChannel *dvbchannel = GetDVBChannel();
    if (!dvbchannel)
        return;

    const DiSEqCDevRotor *rotor = dvbchannel->GetRotor();
    if (!rotor)
        return;

    QMutexLocker locker(&statusLock);
    was_moving = rotorPosition.GetValue() < 100;
    int pos    = (int)truncf(rotor->GetProgress() * 100);
    rotorPosition.SetValue(pos);
    is_moving  = rotorPosition.GetValue() < 100;
}

/** \fn DVBSignalMonitor::Stop(void)
 *  \brief Stop signal monitoring and table monitoring threads.
 */
void DVBSignalMonitor::Stop(void)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Stop() -- begin");
    SignalMonitor::Stop();
    if (GetStreamData())
        streamHandler->RemoveListener(GetStreamData());
    streamHandlerStarted = false;
    streamHandler->SetRetuneAllowed(false, NULL, NULL);
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Stop() -- end");
}

QStringList DVBSignalMonitor::GetStatusList(void) const
{
    QStringList list = DTVSignalMonitor::GetStatusList();
    statusLock.lock();
    if (HasFlags(kDVBSigMon_WaitForSNR))
        list<<signalToNoise.GetName()<<signalToNoise.GetStatus();
    if (HasFlags(kDVBSigMon_WaitForBER))
        list<<bitErrorRate.GetName()<<bitErrorRate.GetStatus();
    if (HasFlags(kDVBSigMon_WaitForUB))
        list<<uncorrectedBlocks.GetName()<<uncorrectedBlocks.GetStatus();
    if (HasFlags(kDVBSigMon_WaitForPos))
        list<<rotorPosition.GetName()<<rotorPosition.GetStatus();
    statusLock.unlock();
    return list;
}

void DVBSignalMonitor::HandlePMT(uint program_num, const ProgramMapTable *pmt)
{
    DTVSignalMonitor::HandlePMT(program_num, pmt);

    if (pmt->ProgramNumber() == (uint)programNumber)
    {
        DVBChannel *dvbchannel = GetDVBChannel();
        if (dvbchannel)
            dvbchannel->SetPMT(pmt);
    }
}

void DVBSignalMonitor::HandleSTT(const SystemTimeTable *stt)
{
    DTVSignalMonitor::HandleSTT(stt);
    DVBChannel *dvbchannel = GetDVBChannel();
    if (dvbchannel)
        dvbchannel->SetTimeOffset(GetStreamData()->TimeOffset());
}

void DVBSignalMonitor::HandleTDT(const TimeDateTable *tdt)
{
    DTVSignalMonitor::HandleTDT(tdt);
    DVBChannel *dvbchannel = GetDVBChannel();
    if (dvbchannel)
        dvbchannel->SetTimeOffset(GetStreamData()->TimeOffset());
}

DVBChannel *DVBSignalMonitor::GetDVBChannel(void)
{
    return dynamic_cast<DVBChannel*>(channel);
}

/** \fn DVBSignalMonitor::UpdateValues()
 *  \brief Fills in frontend stats and emits status Qt signals.
 *
 *   This is automatically called by run(), after Start()
 *   has been used to start the signal monitoring thread.
 */
void DVBSignalMonitor::UpdateValues(void)
{
    if (lock_timer.elapsed() > lock_timeout)
        error = "Timed out.";

    if (!running || exit)
        return;

    if (streamHandlerStarted)
    {
        if (!streamHandler->IsRunning())
        {
            error = QObject::tr("Error: stream handler died");
            update_done = true;
            return;
        }

        EmitStatus();
        if (IsAllGood())
            SendMessageAllGood();

        // TODO dtv signals...

        update_done = true;
        return;
    }

    AddFlags(kSigMon_WaitForSig);

    DVBChannel *dvbchannel = GetDVBChannel();
    if (!dvbchannel)
        return;

    // Handle retuning after rotor has turned
    if (HasFlags(SignalMonitor::kDVBSigMon_WaitForPos))
    {
        if (dvbchannel->GetRotor())
        {
            if (!streamHandler->IsRetuneAllowed())
                streamHandler->SetRetuneAllowed(true, this, dvbchannel);
            streamHandler->RetuneMonitor();
        }
        else
            RemoveFlags(SignalMonitor::kDVBSigMon_WaitForPos);
    }

    bool wasLocked = false, isLocked = false;
    uint sig = 0, snr = 0, ber = 0, ublocks = 0;

    // Get info from card
    bool has_lock = dvbchannel->HasLock();
    if (HasFlags(kSigMon_WaitForSig))
        sig = (uint) (dvbchannel->GetSignalStrength() * 65535);
    if (HasFlags(kDVBSigMon_WaitForSNR))
        snr = (uint) (dvbchannel->GetSNR() * 65535);
    if (HasFlags(kDVBSigMon_WaitForBER))
        ber = (uint) dvbchannel->GetBitErrorRate();
    if (HasFlags(kDVBSigMon_WaitForUB))
        ublocks = (uint) dvbchannel->GetUncorrectedBlockCount();

    has_lock |= streamHandler->IsRunning();

    // Set SignalMonitorValues from info from card.
    {
        QMutexLocker locker(&statusLock);

        // BER and UB are actually uint32 values, but we
        // clamp them at 64K. This is because these values
        // are acutally cumulative, but we don't try to
        // normalize these to a time period.

        wasLocked = signalLock.IsGood();
        signalLock.SetValue((has_lock) ? 1 : 0);
        isLocked = signalLock.IsGood();

        if (HasFlags(kSigMon_WaitForSig))
            signalStrength.SetValue(sig);
        if (HasFlags(kDVBSigMon_WaitForSNR))
            signalToNoise.SetValue(snr);
        if (HasFlags(kDVBSigMon_WaitForBER))
            bitErrorRate.SetValue(ber);
        if (HasFlags(kDVBSigMon_WaitForUB))
            uncorrectedBlocks.SetValue(ublocks);
    }

    // Debug output
    if (wasLocked != isLocked)
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC + "UpdateValues -- Signal " +
                (isLocked ? "Locked" : "Lost"));
    }

    EmitStatus();
    if (IsAllGood())
        SendMessageAllGood();

    // Start table monitoring if we are waiting on any table
    // and we have a lock.
    if (isLocked && GetStreamData() &&
        (!HasFlags(kDVBSigMon_WaitForPos) || rotorPosition.IsGood()) &&
        HasAnyFlag(kDTVSigMon_WaitForPAT | kDTVSigMon_WaitForPMT |
                   kDTVSigMon_WaitForMGT | kDTVSigMon_WaitForVCT |
                   kDTVSigMon_WaitForNIT | kDTVSigMon_WaitForSDT))
    {
        GetStreamData()->AddListeningPID(MPEG_PAT_PID);
        streamHandler->AddListener(GetStreamData(), true, false);
        streamHandlerStarted = true;
    }

    update_done = true;
}

/** \fn DVBSignalMonitor::EmitStatus(void)
 *  \brief Emits signals for lock, signal strength, etc.
 */
void DVBSignalMonitor::EmitStatus(void)
{
    // Emit signals..
    DTVSignalMonitor::EmitStatus();
    if (HasFlags(kDVBSigMon_WaitForSNR))
        SendMessage(kStatusSignalToNoise,     signalToNoise);
    if (HasFlags(kDVBSigMon_WaitForBER))
        SendMessage(kStatusBitErrorRate,      bitErrorRate);
    if (HasFlags(kDVBSigMon_WaitForUB))
        SendMessage(kStatusUncorrectedBlocks, uncorrectedBlocks);
    if (HasFlags(kDVBSigMon_WaitForPos))
        SendMessage(kStatusRotorPosition,     rotorPosition);
}
