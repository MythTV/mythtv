// C headers
#include <chrono> // for milliseconds
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sched.h> // for sched_yield
#include <thread> // for sleep_for
#include <utility>

// MythTV headers

#include "libmythbase/compat.h"
#include "libmythbase/mythconfig.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythdb.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythrandom.h"
#include "libmythbase/programinfo.h"
#include "libmythbase/remoteutil.h"
#include "libmythbase/storagegroup.h"

#include "cardutil.h"
#include "channelgroup.h"
#include "eitscanner.h"
#include "io/mythmediabuffer.h"
#include "jobqueue.h"
#include "livetvchain.h"
#include "mpeg/atscstreamdata.h"
#include "mpeg/atsctables.h"
#include "mpeg/dvbstreamdata.h"
#include "mythsystemevent.h"
#include "osd.h"
#include "previewgeneratorqueue.h"
#include "recorders/channelbase.h"
#include "recorders/dtvchannel.h"
#include "recorders/dtvrecorder.h"
#include "recorders/dtvsignalmonitor.h"
#include "recorders/v4lchannel.h"
#include "recorders/vboxutils.h"
#include "recordingprofile.h"
#include "recordingrule.h"
#include "sourceutil.h"
#include "tv_rec.h"
#include "tvremoteutil.h"

#define DEBUG_CHANNEL_PREFIX 0 /**< set to 1 to channel prefixing */

#define LOC QString("TVRec[%1]: ").arg(m_inputId)
#define LOC2 QString("TVRec[%1]: ").arg(inputid) // for static functions

QReadWriteLock    TVRec::s_inputsLock;
QMap<uint,TVRec*> TVRec::s_inputs;
QMutex            TVRec::s_eitLock;

static bool is_dishnet_eit(uint inputid);
static int init_jobs(const RecordingInfo *rec, RecordingProfile &profile,
                     bool on_host, bool transcode_bfr_comm, bool on_line_comm);
static void apply_broken_dvb_driver_crc_hack(ChannelBase* /*c*/, MPEGStreamData* /*s*/);
static std::chrono::seconds eit_start_rand(uint inputId, std::chrono::seconds eitTransportTimeout);

/** \class TVRec
 *  \brief This is the coordinating class of the \ref recorder_subsystem.
 *
 *  TVRec is used by EncoderLink, which in turn is used by RemoteEncoder
 *  which allows the TV class on the frontend to communicate with TVRec
 *  and is used by MainServer to implement portions of the
 *  \ref myth_network_protocol on the backend.
 *
 *  TVRec contains an instance of RecorderBase, which actually handles the
 *  recording of a program. It also contains an instance of RingBuffer, which
 *  in this case is used to either stream an existing recording to the
 *  frontend, or to save a stream from the RecorderBase to disk. Finally,
 *  if there is a tuner on the hardware RecorderBase is implementing then
 *  TVRec contains a channel instance for that hardware, and possibly a
 *  SignalMonitor instance which monitors the signal quality on a tuners
 *  current input.
 */

/**
 *  \brief Performs instance initialization not requiring access to database.
 *
 *  \sa Init()
 *  \param _inputid
 */
TVRec::TVRec(int inputid)
      // Various threads
    : m_eventThread(new MThread("TVRecEvent", this)),
      // Configuration variables from setup routines
      m_inputId(inputid)
{
    s_inputs[m_inputId] = this;
}

bool TVRec::CreateChannel(const QString &startchannel,
                          bool enter_power_save_mode)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("CreateChannel(%1)")
        .arg(startchannel));
    // If this recorder is a child and its parent is not in error, we
    // do not need nor want to set the channel.
    bool setchan = true;
    if (m_parentId)
    {
        TVRec *parentTV = GetTVRec(m_parentId);
        if (parentTV && parentTV->GetState() != kState_Error)
            setchan = false;
    }
    m_channel = ChannelBase::CreateChannel(
        this, m_genOpt, m_dvbOpt, m_fwOpt,
        startchannel, enter_power_save_mode, m_rbFileExt, setchan);

#if CONFIG_VBOX
    if (m_genOpt.m_inputType == "VBOX")
    {
        if (!CardUtil::IsVBoxPresent(m_inputId))
        {
            // VBOX presence failed, recorder is marked errored
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("CreateChannel(%1) failed due to VBOX not responding "
                        "to network check on inputid [%2]")
                .arg(startchannel).arg(m_inputId));
            m_channel = nullptr;
        }
    }
#endif

#if CONFIG_SATIP
    if (m_genOpt.m_inputType == "SATIP")
    {
        if (!CardUtil::IsSatIPPresent(m_inputId))
        {
            // SatIP box presence failed, recorder is marked errored
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("CreateChannel(%1) failed due to SatIP box not responding "
                        "to network check on inputid [%2]")
                .arg(startchannel).arg(m_inputId));
            m_channel = nullptr;
        }
    }
#endif

    if (!m_channel)
    {
        SetFlags(kFlagErrored, __FILE__, __LINE__);
        return false;
    }

    return true;
}

/** \fn TVRec::Init(void)
 *  \brief Performs instance initialization, returns true on success.
 *
 *  \return Returns true on success, false on failure.
 */
bool TVRec::Init(void)
{
    QMutexLocker lock(&m_stateChangeLock);

    if (!GetDevices(m_inputId, m_parentId, m_genOpt, m_dvbOpt, m_fwOpt))
    {
        LOG(VB_CHANNEL, LOG_ERR, LOC +
            QString("Failed to GetDevices for input %1")
                .arg(m_inputId));
        return false;
    }

    SetRecordingStatus(RecStatus::Unknown, __LINE__);

    // Configure the Channel instance
    QString startchannel = CardUtil::GetStartChannel(m_inputId);
    if (startchannel.isEmpty())
        return false;
    if (!CreateChannel(startchannel, true))
    {
        LOG(VB_CHANNEL, LOG_ERR, LOC +
            QString("Failed to create channel instance for %1")
                .arg(startchannel));
        return false;
    }

    // All conflicting inputs for this input
    if (m_parentId == 0)
    {
        m_eitInputs = CardUtil::GetConflictingInputs(m_inputId);
    }

    m_transcodeFirst    = gCoreContext->GetBoolSetting("AutoTranscodeBeforeAutoCommflag", false);
    m_earlyCommFlag     = gCoreContext->GetBoolSetting("AutoCommflagWhileRecording", false);
    m_runJobOnHostOnly  = gCoreContext->GetBoolSetting("JobsRunOnRecordHost", false);
    m_eitTransportTimeout = gCoreContext->GetDurSetting<std::chrono::minutes>("EITTransportTimeout", 5min);
    if (m_eitTransportTimeout < 15s)
        m_eitTransportTimeout = 15s;
    m_eitCrawlIdleStart = gCoreContext->GetDurSetting<std::chrono::seconds>("EITCrawIdleStart", 60s);
    m_eitScanPeriod     = gCoreContext->GetDurSetting<std::chrono::minutes>("EITScanPeriod", 15min);
    if (m_eitScanPeriod < 5min)
        m_eitScanPeriod = 5min;
    m_audioSampleRateDB = gCoreContext->GetNumSetting("AudioSampleRate");
    m_overRecordSecNrml = gCoreContext->GetDurSetting<std::chrono::seconds>("RecordOverTime");
    m_overRecordSecCat  = gCoreContext->GetDurSetting<std::chrono::minutes>("CategoryOverTime");
    m_overRecordCategory= gCoreContext->GetSetting("OverTimeCategory");

    m_eventThread->start();

    WaitForEventThreadSleep();

    return true;
}

/** \fn TVRec::~TVRec()
 *  \brief Stops the event and scanning threads and deletes any ChannelBase,
 *         RingBuffer, and RecorderBase instances.
 */
TVRec::~TVRec()
{
    s_inputs.remove(m_inputId);

    if (HasFlags(kFlagRunMainLoop))
    {
        ClearFlags(kFlagRunMainLoop, __FILE__, __LINE__);
        m_eventThread->wait();
        delete m_eventThread;
        m_eventThread = nullptr;
    }

    if (m_channel)
    {
        delete m_channel;
        m_channel = nullptr;
    }
}

void TVRec::TeardownAll(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "TeardownAll");

    TeardownSignalMonitor();

    if (m_scanner)
    {
        delete m_scanner;
        m_scanner = nullptr;
    }

    TeardownRecorder(kFlagKillRec);

    SetRingBuffer(nullptr);
}

void TVRec::WakeEventLoop(void)
{
    QMutexLocker locker(&m_triggerEventLoopLock);
    m_triggerEventLoopSignal = true;
    m_triggerEventLoopWait.wakeAll();
}

/** \fn TVRec::GetState() const
 *  \brief Returns the TVState of the recorder.
 *
 *   If there is a pending state change kState_ChangingState is returned.
 *  \sa EncoderLink::GetState(), \ref recorder_subsystem
 */
TVState TVRec::GetState(void) const
{
    if (m_changeState)
        return kState_ChangingState;
    return m_internalState;
}

/** \fn TVRec::GetRecording(void)
 *  \brief Allocates and returns a ProgramInfo for the current recording.
 *
 *  Note: The user of this function must free the %ProgramInfo this returns.
 *  \return %ProgramInfo for the current recording, if it exists, blank
 *          %ProgramInfo otherwise.
 */
ProgramInfo *TVRec::GetRecording(void)
{
    QMutexLocker lock(&m_stateChangeLock);

    ProgramInfo *tmppginfo = nullptr;

    if (m_curRecording && !m_changeState)
    {
        tmppginfo = new ProgramInfo(*m_curRecording);
        tmppginfo->SetRecordingStatus(RecStatus::Recording);
    }
    else
    {
        tmppginfo = new ProgramInfo();
    }
    tmppginfo->SetInputID(m_inputId);

    return tmppginfo;
}

/**
 *  \brief Tells TVRec "rcinfo" is the next pending recording.
 *
 *   When there is a pending recording and the frontend is in "Live TV"
 *   mode the TVRec event loop will send a "ASK_RECORDING" message to
 *   it. Depending on what that query returns, the recording will be
 *   started or not started.
 *
 *  \sa TV::AskAllowRecording(const QStringList&, int, bool)
 *  \param rcinfo   ProgramInfo on pending program.
 *  \param secsleft Seconds left until pending recording begins.
 *                  Set to -1 to revoke the current pending recording.
 *  \param hasLater If true, a later non-conflicting showing is available.
 */
void TVRec::RecordPending(const ProgramInfo *rcinfo, std::chrono::seconds secsleft,
                          bool hasLater)
{
    QMutexLocker statelock(&m_stateChangeLock);
    QMutexLocker pendlock(&m_pendingRecLock);

    if (secsleft < 0s)
    {
        LOG(VB_RECORD, LOG_INFO, LOC + "Pending recording revoked on " +
            QString("inputid [%1]").arg(rcinfo->GetInputID()));

        PendingMap::iterator it = m_pendingRecordings.find(rcinfo->GetInputID());
        if (it != m_pendingRecordings.end())
        {
            (*it).m_ask = false;
            (*it).m_doNotAsk = true;
            (*it).m_canceled = true;
        }
        return;
    }

    LOG(VB_RECORD, LOG_INFO, LOC +
        QString("RecordPending on inputid [%1]").arg(rcinfo->GetInputID()));

    PendingInfo pending;
    pending.m_info            = new ProgramInfo(*rcinfo);
    pending.m_recordingStart  = MythDate::current().addSecs(secsleft.count());
    pending.m_hasLaterShowing = hasLater;
    pending.m_ask             = true;
    pending.m_doNotAsk        = false;

    m_pendingRecordings[rcinfo->GetInputID()] = pending;

    // If this isn't a recording for this instance to make, we are done
    if (rcinfo->GetInputID() != m_inputId)
        return;

    // We also need to check our input groups
    std::vector<uint> inputids = CardUtil::GetConflictingInputs(rcinfo->GetInputID());

    m_pendingRecordings[rcinfo->GetInputID()].m_possibleConflicts = inputids;

    pendlock.unlock();
    statelock.unlock();
    for (uint inputid : inputids)
        RemoteRecordPending(inputid, rcinfo, secsleft, hasLater);
    statelock.relock();
    pendlock.relock();
}

/** \fn TVRec::SetPseudoLiveTVRecording(RecordingInfo*)
 *  \brief Sets the pseudo LiveTV RecordingInfo
 */
void TVRec::SetPseudoLiveTVRecording(RecordingInfo *pi)
{
    RecordingInfo *old_rec = m_pseudoLiveTVRecording;
    m_pseudoLiveTVRecording = pi;
    delete old_rec;
}

/** \fn TVRec::GetRecordEndTime(const ProgramInfo*) const
 *  \brief Returns recording end time with proper post-roll
 */
QDateTime TVRec::GetRecordEndTime(const ProgramInfo *pi) const
{
    bool spcat = (!m_overRecordCategory.isEmpty() &&
                  pi->GetCategory() == m_overRecordCategory);
    std::chrono::seconds secs = (spcat) ? m_overRecordSecCat : m_overRecordSecNrml;
    return pi->GetRecordingEndTime().addSecs(secs.count());
}

/** \fn TVRec::CancelNextRecording(bool)
 *  \brief Tells TVRec to cancel the upcoming recording.
 *  \sa RecordPending(const ProgramInfo*, int, bool),
 *      TV::AskAllowRecording(const QStringList&, int, bool)
 */
void TVRec::CancelNextRecording(bool cancel)
{
    QMutexLocker pendlock(&m_pendingRecLock);
    LOG(VB_RECORD, LOG_INFO, LOC +
        QString("CancelNextRecording(%1) -- begin").arg(cancel));

    PendingMap::iterator it = m_pendingRecordings.find(m_inputId);
    if (it == m_pendingRecordings.end())
    {
        LOG(VB_RECORD, LOG_INFO, LOC + QString("CancelNextRecording(%1) -- "
                "error, unknown recording").arg(cancel));
        return;
    }

    if (cancel)
    {
        std::vector<unsigned int> &inputids = (*it).m_possibleConflicts;
        for (uint inputid : inputids)
        {
            LOG(VB_RECORD, LOG_INFO, LOC +
                QString("CancelNextRecording -- inputid 0x%1")
                    .arg((uint64_t)inputid,0,16));

            pendlock.unlock();
            RemoteRecordPending(inputid, (*it).m_info, -1s, false);
            pendlock.relock();
        }

        LOG(VB_RECORD, LOG_INFO, LOC +
            QString("CancelNextRecording -- inputid [%1]")
                           .arg(m_inputId));

        RecordPending((*it).m_info, -1s, false);
    }
    else
    {
        (*it).m_canceled = false;
    }

    LOG(VB_RECORD, LOG_INFO, LOC +
        QString("CancelNextRecording(%1) -- end").arg(cancel));
}

/** \fn TVRec::StartRecording(ProgramInfo*)
 *  \brief Tells TVRec to Start recording the program "rcinfo"
 *         as soon as possible.
 *
 *  \sa EncoderLink::StartRecording(ProgramInfo*)
 *      RecordPending(const ProgramInfo*, int, bool), StopRecording()
 */
RecStatus::Type TVRec::StartRecording(ProgramInfo *pginfo)
{
    RecordingInfo ri1(*pginfo);
    ri1.SetDesiredStartTime(ri1.GetRecordingStartTime());
    ri1.SetDesiredEndTime(ri1.GetRecordingEndTime());
    RecordingInfo *rcinfo = &ri1;

    LOG(VB_RECORD, LOG_INFO, LOC + QString("StartRecording(%1)")
            .arg(rcinfo->toString(ProgramInfo::kTitleSubtitle)));

    QMutexLocker lock(&m_stateChangeLock);

    if (m_recStatus != RecStatus::Failing)
        SetRecordingStatus(RecStatus::Aborted, __LINE__);

    // Flush out any pending state changes
    WaitForEventThreadSleep();

    // We need to do this check early so we don't cancel an overrecord
    // that we're trying to extend.
    if (m_internalState != kState_WatchingLiveTV &&
        m_recStatus != RecStatus::Failing &&
        m_curRecording && m_curRecording->IsSameProgramWeakCheck(*rcinfo))
    {
        int post_roll_seconds  = m_curRecording->GetRecordingEndTime()
            .secsTo(m_recordEndTime);

        m_curRecording->SetRecordingRuleType(rcinfo->GetRecordingRuleType());
        m_curRecording->SetRecordingRuleID(rcinfo->GetRecordingRuleID());
        m_curRecording->SetRecordingEndTime(rcinfo->GetRecordingEndTime());
        m_curRecording->UpdateRecordingEnd();

        m_recordEndTime = m_curRecording->GetRecordingEndTime()
            .addSecs(post_roll_seconds);

        QString msg = QString("updating recording: %1 %2 %3 %4")
            .arg(m_curRecording->GetTitle(),
                 QString::number(m_curRecording->GetChanID()),
                 m_curRecording->GetRecordingStartTime(MythDate::ISODate),
                 m_curRecording->GetRecordingEndTime(MythDate::ISODate));
        LOG(VB_RECORD, LOG_INFO, LOC + msg);

        ClearFlags(kFlagCancelNextRecording, __FILE__, __LINE__);

        SetRecordingStatus(RecStatus::Recording, __LINE__);
        return RecStatus::Recording;
    }

    bool cancelNext = false;
    PendingInfo pendinfo;

    m_pendingRecLock.lock();
    PendingMap::iterator it = m_pendingRecordings.find(m_inputId);
    if (it != m_pendingRecordings.end())
    {
        (*it).m_ask = (*it).m_doNotAsk = false;
        cancelNext  = (*it).m_canceled;
    }
    m_pendingRecLock.unlock();

    // Flush out events...
    WaitForEventThreadSleep();

    // Rescan pending recordings since the event loop may have deleted
    // a stale entry.  If this happens the info pointer will not be valid
    // since the HandlePendingRecordings loop will have deleted it.
    m_pendingRecLock.lock();
    it = m_pendingRecordings.find(m_inputId);
    bool has_pending = (it != m_pendingRecordings.end());
    if (has_pending)
        pendinfo = *it;
    m_pendingRecLock.unlock();

    // If the needed input is in a shared input group, and we are
    // not canceling the recording anyway, check other recorders
    if (!cancelNext && has_pending && !pendinfo.m_possibleConflicts.empty())
    {
        LOG(VB_RECORD, LOG_INFO, LOC +
            "Checking input group recorders - begin");
        std::vector<unsigned int> &inputids = pendinfo.m_possibleConflicts;

        uint mplexid = 0;
        uint chanid = 0;
        uint sourceid = 0;
        std::vector<unsigned int> inputids2;
        std::vector<TVState> states;

        // Stop remote recordings if needed
        for (uint inputid : inputids)
        {
            InputInfo busy_input;
            bool is_busy = RemoteIsBusy(inputid, busy_input);

            if (is_busy && !sourceid)
            {
                mplexid  = pendinfo.m_info->QueryMplexID();
                chanid   = pendinfo.m_info->GetChanID();
                sourceid = pendinfo.m_info->GetSourceID();
            }

            if (is_busy &&
                ((sourceid != busy_input.m_sourceId) ||
                 (mplexid  != busy_input.m_mplexId) ||
                 ((mplexid == 0 || mplexid == 32767) &&
                  chanid != busy_input.m_chanId)))
            {
                states.push_back((TVState) RemoteGetState(inputid));
                inputids2.push_back(inputid);
            }
        }

        bool ok = true;
        for (uint i = 0; (i < inputids2.size()) && ok; i++)
        {
            LOG(VB_RECORD, LOG_INFO, LOC +
                QString("Attempting to stop input [%1] in state %2")
                    .arg(inputids2[i]).arg(StateToString(states[i])));

            bool success = RemoteStopRecording(inputids2[i]);
            if (success)
            {
                uint state = RemoteGetState(inputids2[i]);
                LOG(VB_GENERAL, LOG_INFO, LOC + QString("a [%1]: %2")
                        .arg(inputids2[i]).arg(StateToString((TVState)state)));
                success = (kState_None == state);
            }

            // If we managed to stop LiveTV recording, restart playback..
            if (success && states[i] == kState_WatchingLiveTV)
            {
                QString message = QString("QUIT_LIVETV %1").arg(inputids2[i]);
                MythEvent me(message);
                gCoreContext->dispatch(me);
            }

            LOG(VB_RECORD, LOG_INFO, LOC +
                QString("Stopping recording on [%1], %2") .arg(inputids2[i])
                    .arg(success ? "succeeded" : "failed"));

            ok &= success;
        }

        // If we failed to stop the remote recordings, don't record
        if (!ok)
        {
            CancelNextRecording(true);
            cancelNext = true;
        }

        inputids.clear();

        LOG(VB_RECORD, LOG_INFO, LOC + "Checking input group recorders - done");
    }

    bool did_switch = false;
    if (!cancelNext && (GetState() == kState_RecordingOnly))
    {
        RecordingInfo *ri2 = SwitchRecordingRingBuffer(*rcinfo);
        did_switch = (nullptr != ri2);
        if (did_switch)
        {
            // Make sure scheduler is allowed to end this recording
            ClearFlags(kFlagCancelNextRecording, __FILE__, __LINE__);

            SetRecordingStatus(RecStatus::Recording, __LINE__);
        }
        else
        {
            // If in post-roll, end recording
            m_stateChangeLock.unlock();
            StopRecording();
            m_stateChangeLock.lock();
        }
    }

    if (!cancelNext && (GetState() == kState_None))
    {
        if (m_tvChain)
        {
            QString message = QString("LIVETV_EXITED");
            MythEvent me(message, m_tvChain->GetID());
            gCoreContext->dispatch(me);
            m_tvChain->DecrRef();
            m_tvChain = nullptr;
        }

        m_recordEndTime = GetRecordEndTime(rcinfo);

        // Tell event loop to begin recording.
        m_curRecording = new RecordingInfo(*rcinfo);
        m_curRecording->MarkAsInUse(true, kRecorderInUseID);
        StartedRecording(m_curRecording);
        pginfo->SetRecordingID(m_curRecording->GetRecordingID());
        pginfo->SetRecordingStartTime(m_curRecording->GetRecordingStartTime());

        // Make sure scheduler is allowed to end this recording
        ClearFlags(kFlagCancelNextRecording, __FILE__, __LINE__);

        if (m_recStatus != RecStatus::Failing)
            SetRecordingStatus(RecStatus::Tuning, __LINE__);
        else
            LOG(VB_RECORD, LOG_WARNING, LOC + "Still failing.");
        ChangeState(kState_RecordingOnly);
    }
    else if (!cancelNext && (GetState() == kState_WatchingLiveTV))
    {
        SetPseudoLiveTVRecording(new RecordingInfo(*rcinfo));
        m_recordEndTime = GetRecordEndTime(rcinfo);
        SetRecordingStatus(RecStatus::Recording, __LINE__);

        // We want the frontend to change channel for recording
        // and disable the UI for channel change, PiP, etc.

        QString message = QString("LIVETV_WATCH %1 1").arg(m_inputId);
        QStringList prog;
        rcinfo->ToStringList(prog);
        MythEvent me(message, prog);
        gCoreContext->dispatch(me);
    }
    else if (!did_switch)
    {
        QString msg = QString("Wanted to record: %1 %2 %3 %4\n\t\t\t")
            .arg(rcinfo->GetTitle(),
                 QString::number(rcinfo->GetChanID()),
                 rcinfo->GetRecordingStartTime(MythDate::ISODate),
                 rcinfo->GetRecordingEndTime(MythDate::ISODate));

        if (cancelNext)
        {
            msg += "But a user has canceled this recording";
            SetRecordingStatus(RecStatus::Cancelled, __LINE__);
        }
        else
        {
            msg += QString("But the current state is: %1")
                .arg(StateToString(m_internalState));
            SetRecordingStatus(RecStatus::TunerBusy, __LINE__);
        }

        if (m_curRecording && m_internalState == kState_RecordingOnly)
        {
            msg += QString("\n\t\t\tCurrently recording: %1 %2 %3 %4")
                .arg(m_curRecording->GetTitle(),
                     QString::number(m_curRecording->GetChanID()),
                     m_curRecording->GetRecordingStartTime(MythDate::ISODate),
                     m_curRecording->GetRecordingEndTime(MythDate::ISODate));
        }

        LOG(VB_GENERAL, LOG_INFO, LOC + msg);
    }

    for (const auto & pend : std::as_const(m_pendingRecordings))
        delete pend.m_info;
    m_pendingRecordings.clear();

    if (!did_switch)
    {
        WaitForEventThreadSleep();

        QMutexLocker locker(&m_pendingRecLock);
        if ((m_curRecording) &&
            (m_curRecording->GetRecordingStatus() == RecStatus::Failed) &&
            (m_recStatus == RecStatus::Recording ||
             m_recStatus == RecStatus::Tuning ||
             m_recStatus == RecStatus::Failing))
        {
            SetRecordingStatus(RecStatus::Failed, __LINE__, true);
        }
        return m_recStatus;
    }

    return GetRecordingStatus();
}

RecStatus::Type TVRec::GetRecordingStatus(void) const
{
    QMutexLocker pendlock(&m_pendingRecLock);
    return m_recStatus;
}

void TVRec::SetRecordingStatus(
    RecStatus::Type new_status, int line, bool have_lock)
{
    RecStatus::Type old_status { RecStatus::Unknown };
    if (have_lock)
    {
        old_status = m_recStatus;
        m_recStatus = new_status;
    }
    else
    {
        m_pendingRecLock.lock();
        old_status = m_recStatus;
        m_recStatus = new_status;
        m_pendingRecLock.unlock();
    }

    LOG(VB_RECORD, LOG_INFO, LOC +
        QString("SetRecordingStatus(%1->%2) on line %3")
        .arg(RecStatus::toString(old_status, kSingleRecord),
             RecStatus::toString(new_status, kSingleRecord),
             QString::number(line)));
}

/** \fn TVRec::StopRecording(bool killFile)
 *  \brief Changes from a recording state to kState_None.
 *  \note For the sake of recording quality measurements this is
 *        treated as the desired end point of the recording.
 *  \sa StartRecording(const ProgramInfo *rec), FinishRecording()
 */
void TVRec::StopRecording(bool killFile)
{
    if (StateIsRecording(GetState()))
    {
        QMutexLocker lock(&m_stateChangeLock);
        if (killFile)
            SetFlags(kFlagKillRec, __FILE__, __LINE__);
        else if (m_curRecording)
        {
            QDateTime now = MythDate::current(true);
            if (now < m_curRecording->GetDesiredEndTime())
                m_curRecording->SetDesiredEndTime(now);
        }
        ChangeState(RemoveRecording(GetState()));
        // wait for state change to take effect
        WaitForEventThreadSleep();
        ClearFlags(kFlagCancelNextRecording|kFlagKillRec, __FILE__, __LINE__);

        SetRecordingStatus(RecStatus::Unknown, __LINE__);
    }
}

/** \fn TVRec::StateIsRecording(TVState)
 *  \brief Returns true if "state" is kState_RecordingOnly,
 *         or kState_WatchingLiveTV.
 *  \param state TVState to check.
 */
bool TVRec::StateIsRecording(TVState state)
{
    return (state == kState_RecordingOnly ||
            state == kState_WatchingLiveTV);
}

/** \fn TVRec::StateIsPlaying(TVState)
 *  \brief Returns true if we are in any state associated with a player.
 *  \param state TVState to check.
 */
bool TVRec::StateIsPlaying(TVState state)
{
    return (state == kState_WatchingPreRecorded);
}

/** \fn TVRec::RemoveRecording(TVState)
 *  \brief If "state" is kState_RecordingOnly or kState_WatchingLiveTV,
 *         returns a kState_None, otherwise returns kState_Error.
 *  \param state TVState to check.
 */
TVState TVRec::RemoveRecording(TVState state) const
{
    if (StateIsRecording(state))
        return kState_None;

    LOG(VB_GENERAL, LOG_ERR, LOC +
        QString("Unknown state in RemoveRecording: %1")
            .arg(StateToString(state)));
    return kState_Error;
}

/** \fn TVRec::RemovePlaying(TVState)
 *  \brief Returns TVState that would remove the playing, but potentially
 *         keep recording if we are watching an in progress recording.
 *  \param state TVState to check.
 */
TVState TVRec::RemovePlaying(TVState state) const
{
    if (StateIsPlaying(state))
    {
        if (state == kState_WatchingPreRecorded)
            return kState_None;
        return kState_RecordingOnly;
    }

    QString msg = "Unknown state in RemovePlaying: %1";
    LOG(VB_GENERAL, LOG_ERR, LOC + msg.arg(StateToString(state)));

    return kState_Error;
}

/** \fn TVRec::StartedRecording(RecordingInfo *curRec)
 *  \brief Inserts a "curRec" into the database
 *  \param curRec Recording to add to database.
 *  \sa ProgramInfo::StartedRecording(const QString&)
 */
void TVRec::StartedRecording(RecordingInfo *curRec)
{
    if (!curRec)
        return;

    curRec->StartedRecording(m_rbFileExt);
    LOG(VB_RECORD, LOG_INFO, LOC + QString("StartedRecording(%1) fn(%2)")
        .arg(curRec->MakeUniqueKey(), curRec->GetPathname()));

    if (curRec->IsCommercialFree())
        curRec->SaveCommFlagged(COMM_FLAG_COMMFREE);

    AutoRunInitType t = (curRec->GetRecordingGroup() == "LiveTV") ?
        kAutoRunNone : kAutoRunProfile;
    InitAutoRunJobs(curRec, t, nullptr, __LINE__);

    SendMythSystemRecEvent("REC_STARTED", curRec);
}

/** \brief If not a premature stop, adds program to history of recorded
 *         programs. If the recording type is kOneRecord this find
 *         is removed.
 *  \sa ProgramInfo::FinishedRecording(bool prematurestop)
 *  \param curRec RecordingInfo or recording to mark as done
 *  \param recq   Information on the quality if the recording.
 */
void TVRec::FinishedRecording(RecordingInfo *curRec, RecordingQuality *recq)
{
    if (!curRec)
        return;

    // Make sure the recording group is up to date
    const QString recgrp = curRec->QueryRecordingGroup();
    curRec->SetRecordingGroup(recgrp);

    bool is_good = true;
    if (recq)
    {
        LOG((recq->IsDamaged()) ? VB_GENERAL : VB_RECORD, LOG_INFO,
            LOC + QString("FinishedRecording(%1) %2 recq:\n%3")
            .arg(curRec->MakeUniqueKey(),
                 (recq->IsDamaged()) ? "damaged" : "good",
                 recq->toStringXML()));
        is_good = !recq->IsDamaged();
        delete recq;
        recq = nullptr;
    }

    RecStatus::Type ors = curRec->GetRecordingStatus();
    // Set the final recording status
    if (curRec->GetRecordingStatus() == RecStatus::Recording)
        curRec->SetRecordingStatus(RecStatus::Recorded);
    else if (curRec->GetRecordingStatus() != RecStatus::Recorded)
        curRec->SetRecordingStatus(RecStatus::Failed);
    curRec->SetRecordingEndTime(MythDate::current(true));
    is_good &= (curRec->GetRecordingStatus() == RecStatus::Recorded);

    // Figure out if this was already done for this recording
    bool was_finished = false;
    static QMutex s_finRecLock;
    static QHash<QString,QDateTime> s_finRecMap;
    {
        QMutexLocker locker(&s_finRecLock);
        QDateTime now = MythDate::current();
        QDateTime expired = now.addSecs(-5LL * 60);
        QHash<QString,QDateTime>::iterator it = s_finRecMap.begin();
        while (it != s_finRecMap.end())
        {
            if ((*it) < expired)
                it = s_finRecMap.erase(it);
            else
                ++it;
        }
        QString key = curRec->MakeUniqueKey();
        it = s_finRecMap.find(key);
        if (it != s_finRecMap.end())
            was_finished = true;
        else
            s_finRecMap[key] = now;
    }

    // Print something informative to the log
    LOG(VB_RECORD, LOG_INFO, LOC +
        QString("FinishedRecording(%1) %2 quality"
                "\n\t\t\ttitle: %3\n\t\t\t"
                "in recgroup: %4 status: %5:%6 %7 %8")
            .arg(curRec->MakeUniqueKey(),
                 is_good ? "Good" : "Bad",
                 curRec->GetTitle(),
                 recgrp,
                 RecStatus::toString(ors, kSingleRecord),
                 RecStatus::toString(curRec->GetRecordingStatus(), kSingleRecord),
                 HasFlags(kFlagDummyRecorderRunning)?"is_dummy":"not_dummy",
                 was_finished?"already_finished":"finished_now"));

    // This has already been called on this recording..
    if (was_finished)
        return;

    // Notify the frontend watching live tv that this file is final
    if (m_tvChain)
        m_tvChain->FinishedRecording(curRec);

    // if this is a dummy recorder, do no more..
    if (HasFlags(kFlagDummyRecorderRunning))
    {
        curRec->FinishedRecording(true); // so end time is updated
        SendMythSystemRecEvent("REC_FINISHED", curRec);
        return;
    }

    // Get the width and set the videoprops
    MarkTypes aspectRatio = curRec->QueryAverageAspectRatio();
    uint avg_height = curRec->QueryAverageHeight();
    bool progressive = curRec->QueryAverageScanProgressive();

    uint16_t flags {VID_UNKNOWN};
    if (avg_height > 2000)
        flags |= VID_4K;
    else if (avg_height > 1000)
        flags |= VID_1080;
    else if (avg_height > 700)
        flags |= VID_720;
    if (progressive)
        flags |= VID_PROGRESSIVE;
    if (!is_good)
        flags |= VID_DAMAGED;
    if ((aspectRatio == MARK_ASPECT_16_9) ||
        (aspectRatio == MARK_ASPECT_2_21_1))
        flags |= VID_WIDESCREEN;

    curRec->SaveVideoProperties
        (VID_4K | VID_1080 | VID_720 | VID_DAMAGED |
         VID_WIDESCREEN | VID_PROGRESSIVE, flags);

    // Make sure really short recordings have positive run time.
    if (curRec->GetRecordingEndTime() <= curRec->GetRecordingStartTime())
    {
        curRec->SetRecordingEndTime(
            curRec->GetRecordingStartTime().addSecs(60));
    }

    // HACK Temporary hack, ensure we've loaded the recording file info, do it now
    //      so that it contains the final filesize information
    if (!curRec->GetRecordingFile())
        curRec->LoadRecordingFile();

    // Generate a preview
    uint64_t fsize = curRec->GetFilesize();
    if (curRec->IsLocal() && (fsize >= 1000) &&
        (curRec->GetRecordingStatus() == RecStatus::Recorded))
    {
        PreviewGeneratorQueue::GetPreviewImage(*curRec, "");
    }

    // store recording in recorded table
    curRec->FinishedRecording(!is_good || (recgrp == "LiveTV"));

    // send out UPDATE_RECORDING_STATUS message
    LOG(VB_RECORD, LOG_INFO, LOC +
        QString("FinishedRecording -- UPDATE_RECORDING_STATUS: %1")
        .arg(RecStatus::toString(is_good ? curRec->GetRecordingStatus()
                      : RecStatus::Failed, kSingleRecord)));
    MythEvent me(QString("UPDATE_RECORDING_STATUS %1 %2 %3 %4 %5")
                 .arg(curRec->GetInputID())
                 .arg(curRec->GetChanID())
                 .arg(curRec->GetScheduledStartTime(MythDate::ISODate))
                 .arg(is_good ? curRec->GetRecordingStatus() : RecStatus::Failed)
                 .arg(curRec->GetRecordingEndTime(MythDate::ISODate)));
    gCoreContext->dispatch(me);

    // send out REC_FINISHED message
    SendMythSystemRecEvent("REC_FINISHED", curRec);

    // send out DONE_RECORDING message
    auto secsSince = MythDate::secsInPast(curRec->GetRecordingStartTime());
    QString message = QString("DONE_RECORDING %1 %2 %3")
        .arg(m_inputId).arg(secsSince.count()).arg(GetFramesWritten());
    MythEvent me2(message);
    gCoreContext->dispatch(me2);

    // Handle JobQueue
    QHash<QString,int>::iterator autoJob =
        m_autoRunJobs.find(curRec->MakeUniqueKey());
    if (autoJob == m_autoRunJobs.end())
    {
        LOG(VB_GENERAL, LOG_INFO,
            "autoRunJobs not initialized until FinishedRecording()");
        AutoRunInitType t =
            (recgrp == "LiveTV") ? kAutoRunNone : kAutoRunProfile;
        InitAutoRunJobs(curRec, t, nullptr, __LINE__);
        autoJob = m_autoRunJobs.find(curRec->MakeUniqueKey());
    }
    LOG(VB_JOBQUEUE, LOG_INFO, QString("AutoRunJobs 0x%1").arg(*autoJob,0,16));
    if ((recgrp == "LiveTV") || (fsize < 1000) ||
        (curRec->GetRecordingStatus() != RecStatus::Recorded) ||
        (curRec->GetRecordingStartTime().secsTo(
            MythDate::current()) < 120))
    {
        JobQueue::RemoveJobsFromMask(JOB_COMMFLAG,  *autoJob);
        JobQueue::RemoveJobsFromMask(JOB_TRANSCODE, *autoJob);
    }
    if (*autoJob != JOB_NONE)
        JobQueue::QueueRecordingJobs(*curRec, *autoJob);
    m_autoRunJobs.erase(autoJob);
}

// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define TRANSITION(ASTATE,BSTATE) \
   ((m_internalState == (ASTATE)) && (m_desiredNextState == (BSTATE)))
#define SET_NEXT() do { nextState = m_desiredNextState; changed = true; } while(false)
#define SET_LAST() do { nextState = m_internalState; changed = true; } while(false)
// NOLINTEND(cppcoreguidelines-macro-usage)

/** \fn TVRec::HandleStateChange(void)
 *  \brief Changes the internalState to the desiredNextState if possible.
 *
 *   Note: There must exist a state transition from any state we can enter
 *   to the kState_None state, as this is used to shutdown TV in RunTV.
 *
 */
void TVRec::HandleStateChange(void)
{
    TVState nextState = m_internalState;

    bool changed = false;

    QString transMsg = QString(" %1 to %2")
        .arg(StateToString(nextState), StateToString(m_desiredNextState));

    if (m_desiredNextState == m_internalState)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "HandleStateChange(): Null transition" + transMsg);
        m_changeState = false;
        return;
    }

    // Stop EIT scanning on this input before any tuning,
    // to avoid race condition with it's tuning requests.
    if (m_scanner && HasFlags(kFlagEITScannerRunning))
    {
        LOG(VB_EIT, LOG_INFO, LOC + QString("Stop EIT scan on input %1").arg(GetInputId()));

        m_scanner->StopActiveScan();
        ClearFlags(kFlagEITScannerRunning, __FILE__, __LINE__);
        auto secs = m_eitCrawlIdleStart + eit_start_rand(m_inputId, m_eitTransportTimeout);
        m_eitScanStartTime = MythDate::current().addSecs(secs.count());
    }

    // Stop EIT scanning on all conflicting inputs so that
    // the tuner card is available for a new tuning request.
    // Conflicting inputs are inputs that have independent video sources
    // but that share a tuner card, such as a DVB-S/S2 tuner card that
    // connects to multiple satellites with a DiSEqC switch.
    if (m_scanner && !m_eitInputs.empty())
    {
        s_inputsLock.lockForRead();
        s_eitLock.lock();
        for (auto input : m_eitInputs)
        {
            auto *tv_rec = s_inputs.value(input);
            if (tv_rec && tv_rec->m_scanner && tv_rec->HasFlags(kFlagEITScannerRunning))
            {
                LOG(VB_EIT, LOG_INFO, LOC +
                    QString("Stop EIT scan active on conflicting input %1")
                        .arg(input));
                tv_rec->m_scanner->StopActiveScan();
                tv_rec->ClearFlags(kFlagEITScannerRunning, __FILE__, __LINE__);
                tv_rec->TuningShutdowns(TuningRequest(kFlagNoRec));
                auto secs = m_eitCrawlIdleStart + eit_start_rand(m_inputId, m_eitTransportTimeout);
                tv_rec->m_eitScanStartTime = MythDate::current().addSecs(secs.count());
            }
        }
        s_eitLock.unlock();
        s_inputsLock.unlock();
    }

    // Handle different state transitions
    if (TRANSITION(kState_None, kState_WatchingLiveTV))
    {
        m_tuningRequests.enqueue(TuningRequest(kFlagLiveTV));
        SET_NEXT();
    }
    else if (TRANSITION(kState_WatchingLiveTV, kState_None))
    {
        m_tuningRequests.enqueue(TuningRequest(kFlagKillRec|kFlagKillRingBuffer));
        SET_NEXT();
    }
    else if (TRANSITION(kState_WatchingLiveTV, kState_RecordingOnly))
    {
        SetPseudoLiveTVRecording(nullptr);

        SET_NEXT();
    }
    else if (TRANSITION(kState_None, kState_RecordingOnly))
    {
        SetPseudoLiveTVRecording(nullptr);
        m_tuningRequests.enqueue(TuningRequest(kFlagRecording, m_curRecording));
        SET_NEXT();
    }
    else if (TRANSITION(kState_RecordingOnly, kState_None))
    {
        m_tuningRequests.enqueue(
            TuningRequest(kFlagCloseRec|kFlagKillRingBuffer|
                          (GetFlags()&kFlagKillRec)));
        SET_NEXT();
    }

    QString msg = (changed) ? "Changing from" : "Unknown state transition:";
    LOG(VB_GENERAL, LOG_INFO, LOC + msg + transMsg);

    // update internal state variable
    m_internalState = nextState;
    m_changeState = false;

    if (m_scanner && (m_internalState == kState_None))
    {
        auto secs = m_eitCrawlIdleStart + eit_start_rand(m_inputId, m_eitTransportTimeout);
        m_eitScanStartTime = MythDate::current().addSecs(secs.count());
    }
    else
    {
        m_eitScanStartTime = MythDate::current().addYears(1);
    }
}
#undef TRANSITION
#undef SET_NEXT
#undef SET_LAST

/** \fn TVRec::ChangeState(TVState)
 *  \brief Puts a state change on the nextState queue.
 */
void TVRec::ChangeState(TVState nextState)
{
    QMutexLocker lock(&m_stateChangeLock);
    m_desiredNextState = nextState;
    m_changeState = true;
    WakeEventLoop();
}

/** \brief Tears down the recorder.
 *
 *   If a "recorder" exists, RecorderBase::StopRecording() is called.
 *   We then wait for "recorder_thread" to exit, and finally we delete
 *   "recorder".
 *
 *   If a RingBuffer instance exists, RingBuffer::StopReads() is called.
 *
 *   If request_flags include kFlagKillRec we mark the recording as
 *   being damaged.
 *
 *   Finally, if there was a recording and it was not damaged,
 *   schedule any post-processing jobs.
 */
void TVRec::TeardownRecorder(uint request_flags)
{
    LOG(VB_RECORD, LOG_INFO, LOC + QString("TeardownRecorder(%1)")
        .arg((request_flags & kFlagKillRec) ? "kFlagKillRec" : ""));

    m_pauseNotify = false;
    m_isPip = false;

    if (m_recorder && HasFlags(kFlagRecorderRunning))
    {
        m_recorder->StopRecording();
        m_recorderThread->wait();
        delete m_recorderThread;
        m_recorderThread = nullptr;
    }
    ClearFlags(kFlagRecorderRunning | kFlagNeedToStartRecorder,
               __FILE__, __LINE__);

    RecordingQuality *recq = nullptr;
    if (m_recorder)
    {
        if (GetV4LChannel())
            m_channel->SetFd(-1);

        recq = m_recorder->GetRecordingQuality(m_curRecording);

        QMutexLocker locker(&m_stateChangeLock);
        delete m_recorder;
        m_recorder = nullptr;
    }

    if (m_buffer)
    {
        LOG(VB_FILE, LOG_INFO, LOC + "calling StopReads()");
        m_buffer->StopReads();
    }

    if (m_curRecording)
    {
        if (!!(request_flags & kFlagKillRec))
            m_curRecording->SetRecordingStatus(RecStatus::Failed);

        FinishedRecording(m_curRecording, recq);

        m_curRecording->MarkAsInUse(false, kRecorderInUseID);
        delete m_curRecording;
        m_curRecording = nullptr;
    }

    m_pauseNotify = true;

    if (GetDTVChannel())
        GetDTVChannel()->EnterPowerSavingMode();
}

DTVRecorder *TVRec::GetDTVRecorder(void)
{
    return dynamic_cast<DTVRecorder*>(m_recorder);
}

void TVRec::CloseChannel(void)
{
    if (m_channel &&
        ((m_genOpt.m_inputType == "DVB" && m_dvbOpt.m_dvbOnDemand) ||
         m_genOpt.m_inputType == "FREEBOX" ||
         m_genOpt.m_inputType == "VBOX" ||
         m_genOpt.m_inputType == "HDHOMERUN" ||
         m_genOpt.m_inputType == "EXTERNAL" ||
         CardUtil::IsV4L(m_genOpt.m_inputType)))
    {
        m_channel->Close();
    }
}

DTVChannel *TVRec::GetDTVChannel(void)
{
    return dynamic_cast<DTVChannel*>(m_channel);
}

V4LChannel *TVRec::GetV4LChannel(void)
{
#if CONFIG_V4L2
    return dynamic_cast<V4LChannel*>(m_channel);
#else
    return nullptr;
#endif // CONFIG_V4L2
}

// Check if EIT is enabled for the video source connected to this input
static bool get_use_eit(uint inputid)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT SUM(useeit) "
        "FROM videosource, capturecard "
        "WHERE videosource.sourceid = capturecard.sourceid AND"
        "      capturecard.cardid   = :INPUTID");
    query.bindValue(":INPUTID", inputid);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("get_use_eit", query);
        return false;
    }
    if (query.next())
        return query.value(0).toBool();
    return false;
}

static bool is_dishnet_eit(uint inputid)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT SUM(dishnet_eit) "
        "FROM videosource, capturecard "
        "WHERE videosource.sourceid = capturecard.sourceid AND"
        "      capturecard.cardid     = :INPUTID");
    query.bindValue(":INPUTID", inputid);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("is_dishnet_eit", query);
        return false;
    }
    if (query.next())
        return query.value(0).toBool();
    return false;
}

// Highest capturecard instance number including multirec instances
static int get_highest_input(void)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT MAX(cardid) "
        "FROM capturecard ");

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("highest_input", query);
        return -1;
    }
    if (query.next())
        return query.value(0).toInt();
    return -1;
}

static std::chrono::seconds eit_start_rand(uint inputId, std::chrono::seconds eitTransportTimeout)
{
    // Randomize start time a bit
    auto timeout = std::chrono::seconds(MythRandom(0, eitTransportTimeout.count() / 3));

    // Use the highest input number and the current input number
    // to distribute the scan start evenly over eitTransportTimeout
    int highest_input = get_highest_input();
    if (highest_input > 0)
        timeout += eitTransportTimeout * inputId / highest_input;

    return timeout;
}

/// \brief Event handling method, contains event loop.
void TVRec::run(void)
{
    QMutexLocker lock(&m_stateChangeLock);
    SetFlags(kFlagRunMainLoop, __FILE__, __LINE__);
    ClearFlags(kFlagExitPlayer | kFlagFinishRecording, __FILE__, __LINE__);

    // Check whether we should use the EITScanner in this TVRec instance
    if (CardUtil::IsEITCapable(m_genOpt.m_inputType) &&         // Card type capable of receiving EIT?
        (!GetDTVChannel() || GetDTVChannel()->IsMaster()) &&    // Card is master and not a multirec instance
        (m_dvbOpt.m_dvbEitScan || get_use_eit(m_inputId)))      // EIT is selected for card OR EIT is selected for video source
    {
        m_scanner = new EITScanner(m_inputId);
        auto secs = m_eitCrawlIdleStart + eit_start_rand(m_inputId, m_eitTransportTimeout);
        m_eitScanStartTime = MythDate::current().addSecs(secs.count());
    }
    else
    {
        m_eitScanStartTime = MythDate::current().addYears(10);
    }

    while (HasFlags(kFlagRunMainLoop))
    {
        // If there is a state change queued up, do it...
        if (m_changeState)
        {
            HandleStateChange();
            ClearFlags(kFlagFrontendReady | kFlagCancelNextRecording,
                       __FILE__, __LINE__);
        }

        // Quick exit on fatal errors.
        if (IsErrored())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "RunTV encountered fatal error, exiting event thread.");
            ClearFlags(kFlagRunMainLoop, __FILE__, __LINE__);
            TeardownAll();
            return;
        }

        // Handle any tuning events..  Blindly grabbing the lock here
        // can sometimes cause a deadlock with Init() while it waits
        // to make sure this thread starts.  Until a better solution
        // is found, don't run HandleTuning unless we can safely get
        // the lock.
        if (s_inputsLock.tryLockForRead())
        {
            HandleTuning();
            s_inputsLock.unlock();
        }

        // Tell frontends about pending recordings
        HandlePendingRecordings();

        // If we are recording a program, check if the recording is
        // over or someone has asked us to finish the recording.
        // Add an extra 60 seconds to the recording end time if we
        // might want a back to back recording.
        QDateTime recEnd = (!m_pendingRecordings.empty()) ?
            m_recordEndTime.addSecs(60) : m_recordEndTime;
        if ((GetState() == kState_RecordingOnly) &&
            (MythDate::current() > recEnd ||
             HasFlags(kFlagFinishRecording)))
        {
            ChangeState(kState_None);
            ClearFlags(kFlagFinishRecording, __FILE__, __LINE__);
        }

        if (m_curRecording)
        {
            m_curRecording->UpdateInUseMark();

            if (m_recorder)
            {
                m_recorder->SavePositionMap();

                // Check for recorder errors
                if (m_recorder->IsErrored())
                {
                    m_curRecording->SetRecordingStatus(RecStatus::Failed);

                    if (GetState() == kState_WatchingLiveTV)
                    {
                        QString message = QString("QUIT_LIVETV %1").arg(m_inputId);
                        MythEvent me(message);
                        gCoreContext->dispatch(me);
                    }
                    else
                    {
                        ChangeState(kState_None);
                    }
                }
            }
        }

        // Check for the end of the current program..
        if (GetState() == kState_WatchingLiveTV)
        {
            QDateTime now   = MythDate::current();
            bool has_finish = HasFlags(kFlagFinishRecording);
            bool has_rec    = m_pseudoLiveTVRecording;
            bool enable_ui  = true;

            m_pendingRecLock.lock();
            bool rec_soon   = m_pendingRecordings.contains(m_inputId);
            m_pendingRecLock.unlock();

            if (has_rec && (has_finish || (now > m_recordEndTime)))
            {
                SetPseudoLiveTVRecording(nullptr);
            }
            else if (!has_rec && !rec_soon && m_curRecording &&
                     (now >= m_curRecording->GetScheduledEndTime()))
            {
                if (!m_switchingBuffer)
                {
                    LOG(VB_RECORD, LOG_INFO, LOC +
                        "Switching Buffer (" +
                        QString("!has_rec(%1) && ").arg(has_rec) +
                        QString("!rec_soon(%1) && (").arg(rec_soon) +
                        MythDate::toString(now, MythDate::ISODate) + " >= " +
                        m_curRecording->GetScheduledEndTime(MythDate::ISODate) +
                        QString("(%1) ))")
                        .arg(now >= m_curRecording->GetScheduledEndTime()));

                    m_switchingBuffer = true;

                    SwitchLiveTVRingBuffer(m_channel->GetChannelName(),
                                           false, true);
                }
                else
                {
                    LOG(VB_RECORD, LOG_INFO, "Waiting for ringbuffer switch");
                }
            }
            else
            {
                enable_ui = false;
            }

            if (enable_ui)
            {
                LOG(VB_RECORD, LOG_INFO, LOC + "Enabling Full LiveTV UI.");
                QString message = QString("LIVETV_WATCH %1 0").arg(m_inputId);
                MythEvent me(message);
                gCoreContext->dispatch(me);
            }
        }

        // Check for ExitPlayer flag, and if set change to a non-watching
        // state (either kState_RecordingOnly or kState_None).
        if (HasFlags(kFlagExitPlayer))
        {
            if (m_internalState == kState_WatchingLiveTV)
                ChangeState(kState_None);
            else if (StateIsPlaying(m_internalState))
                ChangeState(RemovePlaying(m_internalState));
            ClearFlags(kFlagExitPlayer, __FILE__, __LINE__);
        }

        // Start active EIT scan
        bool conflicting_input = false;
        if (m_scanner && m_channel &&
            MythDate::current() > m_eitScanStartTime)
        {
            if (!m_dvbOpt.m_dvbEitScan)
            {
                LOG(VB_EIT, LOG_INFO, LOC +
                    QString("EIT scanning disabled for input %1")
                        .arg(GetInputId()));
                m_eitScanStartTime = MythDate::current().addYears(10);
            }
            else if (!get_use_eit(GetInputId()))
            {
                LOG(VB_EIT, LOG_INFO, LOC +
                    QString("EIT scanning disabled for video source %1")
                        .arg(GetSourceID()));
                m_eitScanStartTime = MythDate::current().addYears(10);
            }
            else
            {
                LOG(VB_EIT, LOG_INFO, LOC +
                    QString("EIT scanning enabled for input %1 connected to video source %2 '%3'")
                        .arg(GetInputId()).arg(GetSourceID()).arg(SourceUtil::GetSourceName(GetSourceID())));

                // Check if another card in the same input group is busy recording.
                // This could be either a virtual DVB-device or a second tuner on a single card.
                s_inputsLock.lockForRead();
                s_eitLock.lock();
                bool allow_eit = true;
                std::vector<uint> inputids = CardUtil::GetConflictingInputs(m_inputId);
                InputInfo busy_input;
                for (uint i = 0; i < inputids.size() && allow_eit; ++i)
                    allow_eit = !RemoteIsBusy(inputids[i], busy_input);

                // Check if another card in the same input group is busy with an EIT scan.
                // We cannot start an EIT scan on this input if there is already an EIT scan
                // running on a conflicting real input.
                // Note that EIT scans never run on virtual inputs.
                if (allow_eit)
                {
                    for (auto input : inputids)
                    {
                        auto *tv_rec = s_inputs.value(input);
                        if (tv_rec && tv_rec->m_scanner)
                        {
                            conflicting_input = true;
                            if (tv_rec->HasFlags(kFlagEITScannerRunning))
                            {
                                LOG(VB_EIT, LOG_INFO, LOC +
                                    QString("EIT scan on conflicting input %1").arg(input));
                                allow_eit = false;
                                busy_input.m_inputId = tv_rec->m_inputId;
                                break;
                            }
                        }
                    }
                }

                if (allow_eit)
                {
                    LOG(VB_EIT, LOG_INFO, LOC +
                        QString("Start EIT active scan on input %1")
                            .arg(m_inputId));
                    m_scanner->StartActiveScan(this, m_eitTransportTimeout);
                    SetFlags(kFlagEITScannerRunning, __FILE__, __LINE__);
                    m_eitScanStartTime = MythDate::current().addYears(1);
                    if (conflicting_input)
                        m_eitScanStopTime = MythDate::current().addSecs(m_eitScanPeriod.count());
                    else
                        m_eitScanStopTime = MythDate::current().addYears(1);
                }
                else
                {
                    const int seconds_postpone = 300;
                    LOG(VB_EIT, LOG_INFO, LOC +
                        QString("Postponing EIT scan on input %1 for %2 seconds because input %3 is busy")
                            .arg(m_inputId).arg(seconds_postpone).arg(busy_input.m_inputId));
                    m_eitScanStartTime = m_eitScanStartTime.addSecs(seconds_postpone);
                }
                s_eitLock.unlock();
                s_inputsLock.unlock();
            }
        }


        // Stop active EIT scan and allow start of the EIT scan on one of the conflicting real inputs.
        if (m_scanner && HasFlags(kFlagEITScannerRunning) && MythDate::current() > m_eitScanStopTime)
        {
            LOG(VB_EIT, LOG_INFO, LOC +
                QString("Stop EIT scan on input %1 to allow scan on a conflicting input")
                    .arg(GetInputId()));

            m_scanner->StopActiveScan();
            ClearFlags(kFlagEITScannerRunning, __FILE__, __LINE__);
            TuningShutdowns(TuningRequest(kFlagNoRec));

            auto secs = m_eitCrawlIdleStart + eit_start_rand(m_inputId, m_eitTransportTimeout);
            secs += m_eitScanPeriod;
            m_eitScanStartTime = MythDate::current().addSecs(secs.count());
        }

        // We should be no more than a few thousand milliseconds,
        // as the end recording code does not have a trigger...
        // NOTE: If you change anything here, make sure that
        // WaitforEventThreadSleep() will still work...
        if (m_tuningRequests.empty() && !m_changeState)
        {
            lock.unlock(); // stateChangeLock

            {
                QMutexLocker locker(&m_triggerEventSleepLock);
                m_triggerEventSleepSignal = true;
                m_triggerEventSleepWait.wakeAll();
            }

            sched_yield();

            {
                QMutexLocker locker(&m_triggerEventLoopLock);
                // We check triggerEventLoopSignal because it is possible
                // that WakeEventLoop() was called since we
                // unlocked the stateChangeLock
                if (!m_triggerEventLoopSignal)
                {
                    m_triggerEventLoopWait.wait(
                        &m_triggerEventLoopLock, 1000 /* ms */);
                }
                m_triggerEventLoopSignal = false;
            }

            lock.relock(); // stateChangeLock
        }
    }

    if (GetState() != kState_None)
    {
        ChangeState(kState_None);
        HandleStateChange();
    }

    TeardownAll();
}

/** \fn TVRec::WaitForEventThreadSleep(bool wake, ulong time)
 *
 *  You MUST HAVE the stateChange-lock locked when you call this method!
 */

bool TVRec::WaitForEventThreadSleep(bool wake, std::chrono::milliseconds time)
{
    bool ok = false;
    MythTimer t;
    t.start();

    while (!ok && (t.elapsed() < time))
    {
        MythTimer t2;
        t2.start();

        if (wake)
            WakeEventLoop();

        m_stateChangeLock.unlock();

        sched_yield();

        {
            QMutexLocker locker(&m_triggerEventSleepLock);
            if (!m_triggerEventSleepSignal)
                m_triggerEventSleepWait.wait(&m_triggerEventSleepLock);
            m_triggerEventSleepSignal = false;
        }

        m_stateChangeLock.lock();

        // verify that we were triggered.
        ok = (m_tuningRequests.empty() && !m_changeState);

        std::chrono::milliseconds te = t2.elapsed();
        if (!ok && te < 10ms)
            std::this_thread::sleep_for(10ms - te);
    }
    return ok;
}

void TVRec::HandlePendingRecordings(void)
{
    QMutexLocker pendlock(&m_pendingRecLock);

    for (auto it = m_pendingRecordings.begin(); it != m_pendingRecordings.end();)
    {
        if (MythDate::current() > (*it).m_recordingStart.addSecs(30))
        {
            LOG(VB_RECORD, LOG_INFO, LOC + "Deleting stale pending recording " +
                QString("[%1] '%2'")
                    .arg((*it).m_info->GetInputID())
                    .arg((*it).m_info->GetTitle()));

            delete (*it).m_info;
            it = m_pendingRecordings.erase(it);
        }
        else
        {
            it++;
        }
    }

    if (m_pendingRecordings.empty())
        return;

    // Make sure EIT scan is stopped so it does't interfere
    if (m_scanner && HasFlags(kFlagEITScannerRunning))
    {
        LOG(VB_CHANNEL, LOG_INFO,
            LOC + "Stopping active EIT scan for pending recording.");
        m_tuningRequests.enqueue(TuningRequest(kFlagNoRec));
    }

    // If we have a pending recording and AskAllowRecording
    // or DoNotAskAllowRecording is set and the frontend is
    // ready send an ASK_RECORDING query to frontend.

    bool has_rec = false;
    auto it = m_pendingRecordings.begin();
    if ((1 == m_pendingRecordings.size()) &&
        (*it).m_ask &&
        ((*it).m_info->GetInputID() == m_inputId) &&
        (GetState() == kState_WatchingLiveTV))
    {
        CheckForRecGroupChange();
        has_rec = m_pseudoLiveTVRecording &&
            (m_pseudoLiveTVRecording->GetRecordingEndTime() >
             (*it).m_recordingStart);
    }

    for (it = m_pendingRecordings.begin(); it != m_pendingRecordings.end(); ++it)
    {
        if (!(*it).m_ask && !(*it).m_doNotAsk)
            continue;

        auto timeuntil = ((*it).m_doNotAsk) ?
            -1s: MythDate::secsInFuture((*it).m_recordingStart);

        if (has_rec)
            (*it).m_canceled = true;

        QString query = QString("ASK_RECORDING %1 %2 %3 %4")
            .arg(m_inputId)
            .arg(timeuntil.count())
            .arg(has_rec ? 1 : 0)
            .arg((*it).m_hasLaterShowing ? 1 : 0);

        LOG(VB_GENERAL, LOG_INFO, LOC + query);

        QStringList msg;
        (*it).m_info->ToStringList(msg);
        MythEvent me(query, msg);
        gCoreContext->dispatch(me);

        (*it).m_ask = (*it).m_doNotAsk = false;
    }
}

bool TVRec::GetDevices(uint inputid,
                       uint &parentid,
                       GeneralDBOptions   &gen_opts,
                       DVBDBOptions       &dvb_opts,
                       FireWireDBOptions  &firewire_opts)
{
    int testnum = 0;
    QString test;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT videodevice,      vbidevice,           audiodevice,     "
        "       audioratelimit,   cardtype,        "
        "       skipbtaudio,      signal_timeout,      channel_timeout, "
        "       dvb_wait_for_seqstart, "
        ""
        "       dvb_on_demand,    dvb_tuning_delay,    dvb_eitscan,"
        ""
        "       firewire_speed,   firewire_model,      firewire_connection, "
        "       parentid "
        ""
        "FROM capturecard "
        "WHERE cardid = :INPUTID");
    query.bindValue(":INPUTID", inputid);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("getdevices", query);
        return false;
    }

    if (!query.next())
        return false;

    // General options
    test = query.value(0).toString();
    if (!test.isEmpty())
        gen_opts.m_videoDev = test;

    test = query.value(1).toString();
    if (!test.isEmpty())
        gen_opts.m_vbiDev = test;

    test = query.value(2).toString();
    if (!test.isEmpty())
        gen_opts.m_audioDev = test;

    gen_opts.m_audioSampleRate = std::max(testnum, query.value(3).toInt());

    test = query.value(4).toString();
    if (!test.isEmpty())
        gen_opts.m_inputType = test;

    gen_opts.m_skipBtAudio = query.value(5).toBool();

    gen_opts.m_signalTimeout  = (uint) std::max(query.value(6).toInt(), 0);
    gen_opts.m_channelTimeout = (uint) std::max(query.value(7).toInt(), 0);

    // We should have at least 1000 ms to acquire tables...
    int table_timeout = ((int)gen_opts.m_channelTimeout -
                         (int)gen_opts.m_signalTimeout);
    if (table_timeout < 1000)
        gen_opts.m_channelTimeout = gen_opts.m_signalTimeout + 1000;

    gen_opts.m_waitForSeqstart = query.value(8).toBool();

    // DVB options
    uint dvboff = 9;
    dvb_opts.m_dvbOnDemand    = query.value(dvboff + 0).toBool();
    dvb_opts.m_dvbTuningDelay = std::chrono::milliseconds(query.value(dvboff + 1).toUInt());
    dvb_opts.m_dvbEitScan     = query.value(dvboff + 2).toBool();

    // Firewire options
    uint fireoff = dvboff + 3;
    firewire_opts.m_speed     = query.value(fireoff + 0).toUInt();

    test = query.value(fireoff + 1).toString();
    if (!test.isEmpty())
        firewire_opts.m_model = test;

    firewire_opts.m_connection = query.value(fireoff + 2).toUInt();

    parentid = query.value(15).toUInt();

    return true;
}

static void GetPidsToCache(DTVSignalMonitor *dtvMon, pid_cache_t &pid_cache)
{
    if (!dtvMon->GetATSCStreamData())
        return;

    const MasterGuideTable *mgt = dtvMon->GetATSCStreamData()->GetCachedMGT();
    if (!mgt)
        return;

    for (uint i = 0; i < mgt->TableCount(); ++i)
    {
        pid_cache_item_t item(mgt->TablePID(i), mgt->TableType(i));
        pid_cache.push_back(item);
    }
    dtvMon->GetATSCStreamData()->ReturnCachedTable(mgt);
}

static bool ApplyCachedPids(DTVSignalMonitor *dtvMon, const DTVChannel* channel)
{
    pid_cache_t pid_cache;
    channel->GetCachedPids(pid_cache);
    bool vctpid_cached = false;
    for (const auto& pid : pid_cache)
    {
        if ((pid.GetTableID() == TableID::TVCT) ||
            (pid.GetTableID() == TableID::CVCT))
        {
            vctpid_cached = true;
            if (dtvMon->GetATSCStreamData())
                dtvMon->GetATSCStreamData()->AddListeningPID(pid.GetPID());
        }
    }
    return vctpid_cached;
}

/**
 *  \brief Tells DTVSignalMonitor what channel to look for.
 *
 *   If the major and minor channels are set we tell the signal
 *   monitor to look for those in the VCT.
 *
 *   Otherwise, we tell the signal monitor to look for the MPEG
 *   program number in the PAT.
 *
 *   This method also grabs the ATSCStreamData() from the recorder
 *   if possible, or creates one if needed.
 *
 *  \param EITscan if set we never look for video streams and we
 *         lock on encrypted streams even if we can't decode them.
 */
bool TVRec::SetupDTVSignalMonitor(bool EITscan)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "Setting up table monitoring.");

    DTVSignalMonitor *sm = GetDTVSignalMonitor();
    DTVChannel *dtvchan = GetDTVChannel();
    if (!sm || !dtvchan)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Setting up table monitoring.");
        return false;
    }

    MPEGStreamData *sd = nullptr;
    if (GetDTVRecorder())
    {
        sd = GetDTVRecorder()->GetStreamData();
        sd->SetCaching(true);
    }

    QString recording_type = "all";
    RecordingInfo *rec = m_lastTuningRequest.m_program;
    RecordingProfile profile;
    m_recProfileName = LoadProfile(m_tvChain, rec, profile);
    const StandardSetting *setting = profile.byName("recordingtype");
    if (setting)
        recording_type = setting->getValue();

    const QString tuningmode = dtvchan->GetTuningMode();

    // Check if this is an ATSC Channel
    int major = dtvchan->GetMajorChannel();
    int minor = dtvchan->GetMinorChannel();
    if ((minor > 0) && (tuningmode == "atsc"))
    {
        QString msg = QString("ATSC channel: %1_%2").arg(major).arg(minor);
        LOG(VB_RECORD, LOG_INFO, LOC + msg);

        auto *asd = dynamic_cast<ATSCStreamData*>(sd);
        if (!asd)
        {
            sd = asd = new ATSCStreamData(major, minor, m_inputId);
            sd->SetCaching(true);
            if (GetDTVRecorder())
                GetDTVRecorder()->SetStreamData(asd);
        }

        asd->Reset();
        sm->SetStreamData(sd);
        sm->SetChannel(major, minor);
        sd->SetRecordingType(recording_type);

        // Try to get pid of VCT from cache and
        // require MGT if we don't have VCT pid.
        if (!ApplyCachedPids(sm, dtvchan))
            sm->AddFlags(SignalMonitor::kDTVSigMon_WaitForMGT);

        LOG(VB_RECORD, LOG_INFO, LOC +
            "Successfully set up ATSC table monitoring.");
        return true;
    }

    // Check if this is an DVB channel
    int progNum = dtvchan->GetProgramNumber();
    if ((progNum >= 0) && (tuningmode == "dvb") && CardUtil::IsChannelReusable(m_genOpt.m_inputType))
    {
        int netid   = dtvchan->GetOriginalNetworkID();
        int tsid    = dtvchan->GetTransportID();

        auto *dsd = dynamic_cast<DVBStreamData*>(sd);
        if (!dsd)
        {
            sd = dsd = new DVBStreamData(netid, tsid, progNum, m_inputId);
            sd->SetCaching(true);
            if (GetDTVRecorder())
                GetDTVRecorder()->SetStreamData(dsd);
        }

        LOG(VB_RECORD, LOG_INFO, LOC +
            QString("DVB service_id %1 on net_id %2 tsid %3")
                .arg(progNum).arg(netid).arg(tsid));

        apply_broken_dvb_driver_crc_hack(m_channel, sd);

        dsd->Reset();
        sm->SetStreamData(sd);
        sm->SetDVBService(netid, tsid, progNum);
        sd->SetRecordingType(recording_type);

        sm->AddFlags(SignalMonitor::kDTVSigMon_WaitForPMT |
                     SignalMonitor::kDTVSigMon_WaitForSDT |
                     SignalMonitor::kDVBSigMon_WaitForPos);
        sm->SetRotorTarget(1.0F);

        if (EITscan)
        {
            sm->GetStreamData()->SetVideoStreamsRequired(0);
            sm->IgnoreEncrypted(true);
        }

        LOG(VB_RECORD, LOG_INFO, LOC +
            "Successfully set up DVB table monitoring.");
        return true;
    }

    // Check if this is an MPEG channel
    if (progNum >= 0)
    {
        if (!sd)
        {
            sd = new MPEGStreamData(progNum, m_inputId, true);
            sd->SetCaching(true);
            if (GetDTVRecorder())
                GetDTVRecorder()->SetStreamData(sd);
        }

        QString msg = QString("MPEG program number: %1").arg(progNum);
        LOG(VB_RECORD, LOG_INFO, LOC + msg);

        apply_broken_dvb_driver_crc_hack(m_channel, sd);

        sd->Reset();
        sm->SetStreamData(sd);
        sm->SetProgramNumber(progNum);
        sd->SetRecordingType(recording_type);

        sm->AddFlags(SignalMonitor::kDTVSigMon_WaitForPAT |
                     SignalMonitor::kDTVSigMon_WaitForPMT |
                     SignalMonitor::kDVBSigMon_WaitForPos);
        sm->SetRotorTarget(1.0F);

        if (EITscan)
        {
            sm->GetStreamData()->SetVideoStreamsRequired(0);
            sm->IgnoreEncrypted(true);
        }

        LOG(VB_RECORD, LOG_INFO, LOC +
            "Successfully set up MPEG table monitoring.");
        return true;
    }

    // If this is not an ATSC, DVB or MPEG channel then check to make sure
    // that we have permanent pidcache entries.
    bool ok = false;
    if (GetDTVChannel())
    {
        pid_cache_t pid_cache;
        GetDTVChannel()->GetCachedPids(pid_cache);
        for (auto item = pid_cache.cbegin(); !ok && item != pid_cache.cend(); ++item)
            ok |= item->IsPermanent();
    }

    if (!ok)
    {
        QString msg = "No valid DTV info, ATSC maj(%1) min(%2), MPEG pn(%3)";
        LOG(VB_GENERAL, LOG_ERR, LOC + msg.arg(major).arg(minor).arg(progNum));
    }
    else
    {
        LOG(VB_RECORD, LOG_INFO, LOC +
            "Successfully set up raw pid monitoring.");
    }

    return ok;
}

/**
 *  \brief This creates a SignalMonitor instance and
 *         begins signal monitoring.
 *
 *   If the channel exists and there is something to monitor a
 *   SignalMonitor instance is created and SignalMonitor::Start()
 *   is called to start the signal monitoring thread.
 *
 *  \param tablemon If set we enable table monitoring
 *  \param EITscan if set we never look for video streams and we
 *         lock on encrypted streams even if we can't decode them.
 *  \param notify   If set we notify the frontend of the signal values
 *  \return true on success, false on failure
 */
bool TVRec::SetupSignalMonitor(bool tablemon, bool EITscan, bool notify)
{
    LOG(VB_RECORD, LOG_INFO, LOC + QString("SetupSignalMonitor(%1, %2)")
            .arg(tablemon).arg(notify));

    // if it already exists, there no need to initialize it
    if (m_signalMonitor)
        return true;

    // if there is no channel object we can't monitor it
    if (!m_channel)
        return false;

    // nothing to monitor here either (DummyChannel)
    if (m_genOpt.m_inputType == "IMPORT" || m_genOpt.m_inputType == "DEMO")
        return true;

    // make sure statics are initialized
    SignalMonitorValue::Init();

    if (SignalMonitor::IsSupported(m_genOpt.m_inputType) && m_channel->Open())
        m_signalMonitor = SignalMonitor::Init(m_genOpt.m_inputType, m_inputId,
                                              m_channel, false);

    if (m_signalMonitor)
    {
        LOG(VB_RECORD, LOG_INFO, LOC + "Signal monitor successfully created");
        // If this is a monitor for Digital TV, initialize table monitors
        if (GetDTVSignalMonitor() && tablemon &&
            !SetupDTVSignalMonitor(EITscan))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "Failed to setup digital signal monitoring");

            return false;
        }

        m_signalMonitor->AddListener(this);
        m_signalMonitor->SetUpdateRate(m_signalMonitor->HasExtraSlowTuning() ?
                                     kSignalMonitoringRate * 5 :
                                     kSignalMonitoringRate);
        m_signalMonitor->SetNotifyFrontend(notify);

        // Start the monitoring thread
        m_signalMonitor->Start();
    }

    return true;
}

/** \fn TVRec::TeardownSignalMonitor()
 *  \brief If a SignalMonitor instance exists, the monitoring thread is
 *         stopped and the instance is deleted.
 */
void TVRec::TeardownSignalMonitor()
{
    if (!m_signalMonitor)
        return;

    LOG(VB_RECORD, LOG_INFO, LOC + "TeardownSignalMonitor() -- begin");

    // If this is a DTV signal monitor, save any pids we know about.
    DTVSignalMonitor *dtvMon  = GetDTVSignalMonitor();
    DTVChannel       *dtvChan = GetDTVChannel();
    if (dtvMon && dtvChan)
    {
        pid_cache_t pid_cache;
        GetPidsToCache(dtvMon, pid_cache);
        if (!pid_cache.empty())
            dtvChan->SaveCachedPids(pid_cache);
    }

    if (m_signalMonitor)
    {
        delete m_signalMonitor;
        m_signalMonitor = nullptr;
    }

    LOG(VB_RECORD, LOG_INFO, LOC + "TeardownSignalMonitor() -- end");
}

/**
 *  \brief Sets the signal monitoring rate.
 *
 *  \sa EncoderLink::SetSignalMonitoringRate(milliseconds,int),
 *      RemoteEncoder::SetSignalMonitoringRate(milliseconds,int)
 *  \param rate           The update rate to use in milliseconds,
 *                        use 0 to disable signal monitoring.
 *  \param notifyFrontend If 1, SIGNAL messages will be sent to
 *                        the frontend using this recorder.
 *  \return 1 if it signal monitoring is turned on, 0 otherwise.
 */
std::chrono::milliseconds TVRec::SetSignalMonitoringRate(std::chrono::milliseconds rate, int notifyFrontend)
{
    QString msg = "SetSignalMonitoringRate(%1, %2)";
    LOG(VB_RECORD, LOG_INFO, LOC +
        msg.arg(rate.count()).arg(notifyFrontend) + "-- start");

    QMutexLocker lock(&m_stateChangeLock);

    if (!SignalMonitor::IsSupported(m_genOpt.m_inputType))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Signal Monitoring is notsupported by your hardware.");
        return 0ms;
    }

    if (GetState() != kState_WatchingLiveTV)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Signal can only be monitored in LiveTV Mode.");
        return 0ms;
    }

    ClearFlags(kFlagRingBufferReady, __FILE__, __LINE__);

    TuningRequest req = (rate > 0ms) ?
        TuningRequest(kFlagAntennaAdjust, m_channel->GetChannelName()) :
        TuningRequest(kFlagLiveTV);

    m_tuningRequests.enqueue(req);

    // Wait for RingBuffer reset
    while (!HasFlags(kFlagRingBufferReady))
        WaitForEventThreadSleep();
    LOG(VB_RECORD, LOG_INFO, LOC +
        msg.arg(rate.count()).arg(notifyFrontend) + " -- end");
    return 1ms;
}

DTVSignalMonitor *TVRec::GetDTVSignalMonitor(void)
{
    return dynamic_cast<DTVSignalMonitor*>(m_signalMonitor);
}

/** \fn TVRec::ShouldSwitchToAnotherInput(QString)
 *  \brief Checks if named channel exists on current tuner, or
 *         another tuner.
 *
 *  \param chanid channel to verify against tuners.
 *  \return true if the channel on another tuner and not current tuner,
 *          false otherwise.
 *  \sa EncoderLink::ShouldSwitchToAnotherInput(const QString&),
 *      RemoteEncoder::ShouldSwitchToAnotherInput(QString),
 *      CheckChannel(QString)
 */
bool TVRec::ShouldSwitchToAnotherInput(const QString& chanid) const
{
    QString msg("");
    MSqlQuery query(MSqlQuery::InitCon());

    if (!query.isConnected())
        return false;

    query.prepare("SELECT channel.channum, channel.callsign "
                  "FROM channel "
                  "WHERE channel.chanid = :CHANID");
    query.bindValue(":CHANID", chanid);
    if (!query.exec() || !query.next())
    {
        MythDB::DBError("ShouldSwitchToAnotherInput", query);
        return false;
    }

    QString channelname = query.value(0).toString();
    QString callsign = query.value(1).toString();

    query.prepare(
        "SELECT channel.channum "
        "FROM channel, capturecard "
        "WHERE deleted IS NULL AND "
        "      ( channel.chanid = :CHANID OR             "
        "        ( channel.channum  = :CHANNUM AND       "
        "          channel.callsign = :CALLSIGN    )     "
        "      )                                     AND "
        "      channel.sourceid = capturecard.sourceid AND "
        "      capturecard.cardid = :INPUTID");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":CHANNUM", channelname);
    query.bindValue(":CALLSIGN", callsign);
    query.bindValue(":INPUTID", m_inputId);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("ShouldSwitchToAnotherInput", query);
    }
    else if (query.size() > 0)
    {
        msg = "Found channel (%1) on current input[%2].";
        LOG(VB_RECORD, LOG_INFO, LOC + msg.arg(channelname).arg(m_inputId));
        return false;
    }

    // We didn't find it on the current input, so now we check other inputs.
    query.prepare(
        "SELECT channel.channum, capturecard.cardid "
        "FROM channel, capturecard "
        "WHERE deleted IS NULL AND "
        "      ( channel.chanid = :CHANID OR              "
        "        ( channel.channum  = :CHANNUM AND        "
        "          channel.callsign = :CALLSIGN    )      "
        "      )                                      AND "
        "      channel.sourceid  = capturecard.sourceid AND "
        "      capturecard.cardid != :INPUTID");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":CHANNUM", channelname);
    query.bindValue(":CALLSIGN", callsign);
    query.bindValue(":INPUTID", m_inputId);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("ShouldSwitchToAnotherInput", query);
    }
    else if (query.next())
    {
        msg = QString("Found channel (%1) on different input(%2).")
            .arg(query.value(0).toString(), query.value(1).toString());
        LOG(VB_RECORD, LOG_INFO, LOC + msg);
        return true;
    }

    msg = QString("Did not find channel(%1) on any input.").arg(channelname);
    LOG(VB_RECORD, LOG_ERR, LOC + msg);
    return false;
}

/** \fn TVRec::CheckChannel(QString) const
 *  \brief Checks if named channel exists on current tuner.
 *
 *  \param name channel to verify against current tuner.
 *  \return true if it succeeds, false otherwise.
 *  \sa EncoderLink::CheckChannel(const QString&),
 *      RemoteEncoder::CheckChannel(QString),
 *      CheckChannel(ChannelBase*,const QString&,QString&),
 *      ShouldSwitchToAnotherInput(QString)
 */
bool TVRec::CheckChannel(const QString& name) const
{
    if (!m_channel)
        return false;

    return m_channel->CheckChannel(name);
}

/** \fn QString add_spacer(const QString&, const QString&)
 *  \brief Adds the spacer before the last character in chan.
 */
static QString add_spacer(const QString &channel, const QString &spacer)
{
    QString chan = channel;
    if ((chan.length() >= 2) && !spacer.isEmpty())
        return chan.left(chan.length()-1) + spacer + chan.right(1);
    return chan;
}

/** \fn TVRec::CheckChannelPrefix(const QString&,uint&,bool&,QString&)
 *  \brief Checks a prefix against the channels in the DB.
 *
 *   If the prefix matches a channel on any recorder this function returns
 *   true, otherwise it returns false.
 *
 *   If the prefix matches any channel entirely (i.e. prefix == channum),
 *   then the inputid of the recorder it matches is returned in
 *   'complete_valid_channel_on_rec'; if it matches multiple recorders,
 *   and one of them is this recorder, this recorder is returned in
 *   'complete_valid_channel_on_rec'; if it isn't complete for any channel
 *    on any recorder 'complete_valid_channel_on_rec' is set to zero.
 *
 *   If adding another character could reduce the number of channels the
 *   prefix matches 'is_extra_char_useful' is set to true, otherwise it
 *   is set to false.
 *
 *   Finally, if in order for the prefix to match a channel, a spacer needs
 *   to be added, the first matching spacer is returned in needed_spacer.
 *   If there is more than one spacer that might be employed and one of them
 *   is used for the current recorder, and others are used for other
 *   recorders, then the one for the current recorder is returned. The
 *   spacer must be inserted before the last character of the prefix for
 *   anything else returned from the function to be valid.
 *
 *  \return true if this is a valid prefix for a channel, false otherwise
 */
bool TVRec::CheckChannelPrefix(const QString &prefix,
                               uint          &complete_valid_channel_on_rec,
                               bool          &is_extra_char_useful,
                               QString       &needed_spacer) const
{
#if DEBUG_CHANNEL_PREFIX
    LOG(VB_GENERAL, LOG_DEBUG, QString("CheckChannelPrefix(%1)").arg(prefix));
#endif

    static const std::array<const QString,5> s_spacers = { "", "_", "-", "#", "." };

    MSqlQuery query(MSqlQuery::InitCon());
    QString basequery = QString(
        "SELECT channel.chanid, channel.channum, capturecard.cardid "
        "FROM channel, capturecard "
        "WHERE deleted IS NULL AND "
        "      channel.channum LIKE '%1%'            AND "
        "      channel.sourceid = capturecard.sourceid");

    const std::array<const QString,2> inputquery
    {
        QString(" AND capturecard.cardid  = '%1'").arg(m_inputId),
        QString(" AND capturecard.cardid != '%1'").arg(m_inputId),
    };

    std::vector<unsigned int>   fchanid;
    std::vector<QString>        fchannum;
    std::vector<unsigned int>   finputid;
    std::vector<QString>        fspacer;

    for (const auto & str : inputquery)
    {
        for (const auto & spacer : s_spacers)
        {
            QString qprefix = add_spacer(
                prefix, (spacer == "_") ? "\\_" : spacer);
            query.prepare(basequery.arg(qprefix) + str);

            if (!query.exec() || !query.isActive())
            {
                MythDB::DBError("checkchannel -- locate channum", query);
            }
            else if (query.size())
            {
                while (query.next())
                {
                    fchanid.push_back(query.value(0).toUInt());
                    fchannum.push_back(query.value(1).toString());
                    finputid.push_back(query.value(2).toUInt());
                    fspacer.emplace_back(spacer);
#if DEBUG_CHANNEL_PREFIX
                    LOG(VB_GENERAL, LOG_DEBUG,
                        QString("(%1,%2) Adding %3 rec %4")
                            .arg(i).arg(j).arg(query.value(1).toString(),6)
                            .arg(query.value(2).toUInt()));
#endif
                }
            }

            if (prefix.length() < 2)
                break;
        }
    }

    // Now process the lists for the info we need...
    is_extra_char_useful = false;
    complete_valid_channel_on_rec = 0;
    needed_spacer.clear();

    if (fchanid.empty())
        return false;

    if (fchanid.size() == 1) // Unique channel...
    {
        needed_spacer = fspacer[0];
        bool nc       = (fchannum[0] != add_spacer(prefix, fspacer[0]));

        complete_valid_channel_on_rec = (nc) ? 0 : finputid[0];
        is_extra_char_useful             = nc;
        return true;
    }

    // If we get this far there is more than one channel
    // sharing the prefix we were given.

    // Is an extra characher useful for disambiguation?
    is_extra_char_useful = false;
    for (uint i = 0; (i < fchannum.size()) && !is_extra_char_useful; i++)
    {
        is_extra_char_useful = (fchannum[i] != add_spacer(prefix, fspacer[i]));
#if DEBUG_CHANNEL_PREFIX
        LOG(VB_GENERAL, LOG_DEBUG, QString("is_extra_char_useful(%1!=%2): %3")
                .arg(fchannum[i]).arg(add_spacer(prefix, fspacer[i]))
                .arg(is_extra_char_useful));
#endif
    }

    // Are any of the channels complete w/o spacer?
    // If so set complete_valid_channel_on_rec,
    // with a preference for our inputid.
    for (size_t i = 0; i < fchannum.size(); i++)
    {
        if (fchannum[i] == prefix)
        {
            complete_valid_channel_on_rec = finputid[i];
            if (finputid[i] == m_inputId)
                break;
        }
    }

    if (complete_valid_channel_on_rec != 0)
        return true;

    // Add a spacer, if one is needed to select a valid channel.
    bool spacer_needed = true;
    for (uint i = 0; (i < fspacer.size() && spacer_needed); i++)
        spacer_needed = !fspacer[i].isEmpty();
    if (spacer_needed)
        needed_spacer = fspacer[0];

    // If it isn't useful to wait for more characters,
    // then try to commit to any true match immediately.
    for (size_t i = 0; i < ((is_extra_char_useful) ? 0 : fchanid.size()); i++)
    {
        if (fchannum[i] == add_spacer(prefix, fspacer[i]))
        {
            needed_spacer = fspacer[i];
            complete_valid_channel_on_rec = finputid[i];
            return true;
        }
    }

    return true;
}

bool TVRec::SetVideoFiltersForChannel(uint  sourceid,
                                      const QString &channum)
{
    if (!m_recorder)
        return false;

    QString videoFilters = ChannelUtil::GetVideoFilters(sourceid, channum);
    if (!videoFilters.isEmpty())
    {
        m_recorder->SetVideoFilters(videoFilters);
        return true;
    }

    return false;
}

/** \fn TVRec::IsReallyRecording()
 *  \brief Returns true if frontend can consider the recorder started.
 *  \sa IsRecording()
 */
bool TVRec::IsReallyRecording(void)
{
    return ((m_recorder && m_recorder->IsRecording()) ||
            HasFlags(kFlagDummyRecorderRunning));
}

/**
 *  \brief Returns true if the recorder is busy, or will be within
 *         the next time_buffer seconds.
 *  \sa EncoderLink::IsBusy(TunedInputInfo*, int time_buffer)
 */
bool TVRec::IsBusy(InputInfo *busy_input, std::chrono::seconds time_buffer) const
{
    InputInfo dummy;
    if (!busy_input)
        busy_input = &dummy;

    busy_input->Clear();

    if (!m_channel)
        return false;

    if (!m_channel->GetInputID())
        return false;

    uint chanid = 0;

    if (GetState() != kState_None)
    {
        busy_input->m_inputId = m_channel->GetInputID();
        chanid                = m_channel->GetChanID();
    }

    PendingInfo pendinfo;
    bool has_pending = false;
    {
        m_pendingRecLock.lock();
        PendingMap::const_iterator it = m_pendingRecordings.find(m_inputId);
        has_pending = (it != m_pendingRecordings.end());
        if (has_pending)
            pendinfo = *it;
        m_pendingRecLock.unlock();
    }

    if (!busy_input->m_inputId && has_pending)
    {
        auto timeLeft = MythDate::secsInFuture(pendinfo.m_recordingStart);

        if (timeLeft <= time_buffer)
        {
            QString channum;
            QString input;
            if (pendinfo.m_info->QueryTuningInfo(channum, input))
            {
                busy_input->m_inputId = m_channel->GetInputID();
                chanid = pendinfo.m_info->GetChanID();
            }
        }
    }

    if (busy_input->m_inputId)
    {
        CardUtil::GetInputInfo(*busy_input);
        busy_input->m_chanId  = chanid;
        busy_input->m_mplexId = ChannelUtil::GetMplexID(busy_input->m_chanId);
        busy_input->m_mplexId =
            (32767 == busy_input->m_mplexId) ? 0 : busy_input->m_mplexId;
    }

    return busy_input->m_inputId != 0U;
}


/** \fn TVRec::GetFramerate()
 *  \brief Returns recordering frame rate from the recorder.
 *  \sa RemoteEncoder::GetFrameRate(), EncoderLink::GetFramerate(void),
 *      RecorderBase::GetFrameRate()
 *  \return Frames per second if query succeeds -1 otherwise.
 */
float TVRec::GetFramerate(void)
{
    QMutexLocker lock(&m_stateChangeLock);

    if (m_recorder)
        return m_recorder->GetFrameRate();
    return -1.0F;
}

/** \fn TVRec::GetFramesWritten()
 *  \brief Returns number of frames written to disk by recorder.
 *
 *  \sa EncoderLink::GetFramesWritten(), RemoteEncoder::GetFramesWritten()
 *  \return Number of frames if query succeeds, -1 otherwise.
 */
long long TVRec::GetFramesWritten(void)
{
    QMutexLocker lock(&m_stateChangeLock);

    if (m_recorder)
        return m_recorder->GetFramesWritten();
    return -1;
}

/** \fn TVRec::GetFilePosition()
 *  \brief Returns total number of bytes written by RingBuffer.
 *
 *  \sa EncoderLink::GetFilePosition(), RemoteEncoder::GetFilePosition()
 *  \return Bytes written if query succeeds, -1 otherwise.
 */
long long TVRec::GetFilePosition(void)
{
    QMutexLocker lock(&m_stateChangeLock);

    if (m_buffer)
        return m_buffer->GetWritePosition();
    return -1;
}

/** \brief Returns byte position in RingBuffer
 *         of a keyframe according to recorder.
 *
 *  \sa EncoderLink::GetKeyframePosition(uint64_t),
 *      RemoteEncoder::GetKeyframePosition(uint64_t)
 *  \return Byte position of keyframe if query succeeds, -1 otherwise.
 */
int64_t TVRec::GetKeyframePosition(uint64_t desired) const
{
    QMutexLocker lock(&m_stateChangeLock);

    if (m_recorder)
        return m_recorder->GetKeyframePosition(desired);
    return -1;
}

/**
 *  \brief Returns byte position in RingBuffer of a keyframes
 *         according to recorder.
 *
 *  \sa EncoderLink::GetKeyframePositions(int64_t, int64_t, frm_pos_map_t&),
 *      RemoteEncoder::GetKeyframePositions(int64_t, int64_t, frm_pos_map_t&)
 *  \return Byte position of keyframe if query succeeds, -1 otherwise.
 */
bool TVRec::GetKeyframePositions(
    int64_t start, int64_t end, frm_pos_map_t &map) const
{
    QMutexLocker lock(&m_stateChangeLock);

    if (m_recorder)
        return m_recorder->GetKeyframePositions(start, end, map);

    return false;
}

bool TVRec::GetKeyframeDurations(
    int64_t start, int64_t end, frm_pos_map_t &map) const
{
    QMutexLocker lock(&m_stateChangeLock);

    if (m_recorder)
        return m_recorder->GetKeyframeDurations(start, end, map);

    return false;
}

/** \fn TVRec::GetMaxBitrate(void) const
 *  \brief Returns the maximum bits per second this recorder can produce.
 *
 *  \sa EncoderLink::GetMaxBitrate(void), RemoteEncoder::GetMaxBitrate(void)
 */
long long TVRec::GetMaxBitrate(void) const
{
    long long bitrate = 0;
    if (m_genOpt.m_inputType == "MPEG")
    {   // NOLINT(bugprone-branch-clone)
        bitrate = 10080000LL; // use DVD max bit rate
    }
    else if (m_genOpt.m_inputType == "HDPVR")
    {
        bitrate = 20200000LL; // Peak bit rate for HD-PVR
    }
    else if (!CardUtil::IsEncoder(m_genOpt.m_inputType))
    {
        bitrate = 22200000LL; // 1080i
    }
    else // frame grabber
    {
        bitrate = 10080000LL; // use DVD max bit rate, probably too big
    }

    return bitrate;
}

/** \fn TVRec::SpawnLiveTV(LiveTVChain*,bool,QString)
 *  \brief Tells TVRec to spawn a "Live TV" recorder.
 *  \sa EncoderLink::SpawnLiveTV(LiveTVChain*,bool,QString),
 *      RemoteEncoder::SpawnLiveTV(QString,bool,QSting)
 */
void TVRec::SpawnLiveTV(LiveTVChain *newchain, bool pip, QString startchan)
{
    QMutexLocker lock(&m_stateChangeLock);

    m_tvChain = newchain;
    m_tvChain->IncrRef(); // mark it for TVRec use
    m_tvChain->ReloadAll();

    QString hostprefix = MythCoreContext::GenMythURL(
                    gCoreContext->GetHostName(),
                    gCoreContext->GetBackendServerPort());

    m_tvChain->SetHostPrefix(hostprefix);
    m_tvChain->SetInputType(m_genOpt.m_inputType);

    m_isPip = pip;
    m_liveTVStartChannel = std::move(startchan);

    // Change to WatchingLiveTV
    ChangeState(kState_WatchingLiveTV);
    // Wait for state change to take effect
    WaitForEventThreadSleep();

    // Make sure StartRecording can't steal our tuner
    SetFlags(kFlagCancelNextRecording, __FILE__, __LINE__);
}

/** \fn TVRec::GetChainID()
 *  \brief Get the chainid of the livetv instance
 */
QString TVRec::GetChainID(void)
{
    if (m_tvChain)
        return m_tvChain->GetID();
    return "";
}

/** \fn TVRec::CheckForRecGroupChange(void)
 *  \brief Check if frontend changed the recording group.
 *
 *   This is needed because the frontend may toggle whether something
 *   should be kept as a recording in the frontend, but this class may
 *   not find out about it in time unless we check the DB when this
 *   information is important.
 */
void TVRec::CheckForRecGroupChange(void)
{
    QMutexLocker lock(&m_stateChangeLock);

    if (m_internalState == kState_None)
        return; // already stopped

    if (!m_curRecording)
        return;

    const QString recgrp = m_curRecording->QueryRecordingGroup();
    m_curRecording->SetRecordingGroup(recgrp);

    if (recgrp != "LiveTV" && !m_pseudoLiveTVRecording)
    {
        // User wants this recording to continue
        SetPseudoLiveTVRecording(new RecordingInfo(*m_curRecording));
    }
    else if (recgrp == "LiveTV" && m_pseudoLiveTVRecording)
    {
        // User wants to abandon scheduled recording
        SetPseudoLiveTVRecording(nullptr);
    }
}

/** \fn TVRec::NotifySchedulerOfRecording(RecordingInfo*)
 *  \brief Tell scheduler about the recording.
 *
 *   This is needed if the frontend has marked the LiveTV
 *   buffer for recording after we exit LiveTV. In this case
 *   the scheduler needs to know about the recording so it
 *   can properly take overrecord into account, and to properly
 *   reschedule other recordings around to avoid this recording.
 */
void TVRec::NotifySchedulerOfRecording(RecordingInfo *rec)
{
    if (!m_channel)
        return;

    // Notify scheduler of the recording.
    // + set up recording so it can be resumed
    rec->SetInputID(m_inputId);
    rec->SetRecordingRuleType(rec->GetRecordingRule()->m_type);

    if (rec->GetRecordingRuleType() == kNotRecording)
    {
        rec->SetRecordingRuleType(kSingleRecord);
        rec->GetRecordingRule()->m_type = kSingleRecord;
    }

    // + remove any end offset which would mismatch the live session
    rec->GetRecordingRule()->m_endOffset = 0;

    // + save RecStatus::Inactive recstatus to so that a reschedule call
    //   doesn't start recording this on another input before we
    //   send the SCHEDULER_ADD_RECORDING message to the scheduler.
    rec->SetRecordingStatus(RecStatus::Inactive);
    rec->AddHistory(false);

    // + save RecordingRule so that we get a recordid
    //   (don't allow RescheduleMatch(), avoiding unneeded reschedule)
    rec->GetRecordingRule()->Save(false);

    // + save recordid to recorded entry
    rec->ApplyRecordRecID();

    // + set proper recstatus (saved later)
    rec->SetRecordingStatus(RecStatus::Recording);

    // + pass proginfo to scheduler and reschedule
    QStringList prog;
    rec->ToStringList(prog);
    MythEvent me("SCHEDULER_ADD_RECORDING", prog);
    gCoreContext->dispatch(me);

    // Allow scheduler to end this recording before post-roll,
    // if it has another recording for this recorder.
    ClearFlags(kFlagCancelNextRecording, __FILE__, __LINE__);
}

void TVRec::InitAutoRunJobs(RecordingInfo *rec, AutoRunInitType t,
                            RecordingProfile *recpro, int line)
{
    if (kAutoRunProfile == t)
    {
        RecordingProfile profile;
        if (!recpro)
        {
            LoadProfile(nullptr, rec, profile);
            recpro = &profile;
        }
        m_autoRunJobs[rec->MakeUniqueKey()] =
            init_jobs(rec, *recpro, m_runJobOnHostOnly,
                      m_transcodeFirst, m_earlyCommFlag);
    }
    else
    {
        m_autoRunJobs[rec->MakeUniqueKey()] = JOB_NONE;
    }
    LOG(VB_JOBQUEUE, LOG_INFO,
        QString("InitAutoRunJobs for %1, line %2 -> 0x%3")
        .arg(rec->MakeUniqueKey()).arg(line)
        .arg(m_autoRunJobs[rec->MakeUniqueKey()],0,16));
}

/** \fn TVRec::SetLiveRecording(int)
 *  \brief Tells the Scheduler about changes to the recording status
 *         of the LiveTV recording.
 *
 *   NOTE: Currently the 'recording' parameter is ignored and decisions
 *         are based on the recording group alone.
 *
 *  \param recording Set to 1 to mark as RecStatus::Recording, set to 0 to mark as
 *         RecStatus::Cancelled, and set to -1 to base the decision of the recording
 *         group.
 */
void TVRec::SetLiveRecording([[maybe_unused]] int recording)
{
    LOG(VB_GENERAL, LOG_INFO, LOC +
        QString("SetLiveRecording(%1)").arg(recording));
    QMutexLocker locker(&m_stateChangeLock);

    RecStatus::Type recstat = RecStatus::Cancelled;
    bool was_rec = m_pseudoLiveTVRecording;
    CheckForRecGroupChange();
    if (was_rec && !m_pseudoLiveTVRecording)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "SetLiveRecording() -- cancel");
        // cancel -- 'recording' should be 0 or -1
        SetFlags(kFlagCancelNextRecording, __FILE__, __LINE__);
        m_curRecording->SetRecordingGroup("LiveTV");
        InitAutoRunJobs(m_curRecording, kAutoRunNone, nullptr, __LINE__);
    }
    else if (!was_rec && m_pseudoLiveTVRecording)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "SetLiveRecording() -- record");
        // record -- 'recording' should be 1 or -1

        // If the last recording was flagged for keeping
        // in the frontend, then add the recording rule
        // so that transcode, commfrag, etc can be run.
        m_recordEndTime = GetRecordEndTime(m_pseudoLiveTVRecording);
        NotifySchedulerOfRecording(m_curRecording);
        recstat = m_curRecording->GetRecordingStatus();
        m_curRecording->SetRecordingGroup("Default");
        InitAutoRunJobs(m_curRecording, kAutoRunProfile, nullptr, __LINE__);
    }

    MythEvent me(QString("UPDATE_RECORDING_STATUS %1 %2 %3 %4 %5")
                 .arg(m_curRecording->GetInputID())
                 .arg(m_curRecording->GetChanID())
                 .arg(m_curRecording->GetScheduledStartTime(MythDate::ISODate))
                 .arg(recstat)
                 .arg(m_curRecording->GetRecordingEndTime(MythDate::ISODate)));

    gCoreContext->dispatch(me);
}

/** \fn TVRec::StopLiveTV(void)
 *  \brief Tells TVRec to stop a "Live TV" recorder.
 *  \sa EncoderLink::StopLiveTV(), RemoteEncoder::StopLiveTV()
 */
void TVRec::StopLiveTV(void)
{
    QMutexLocker lock(&m_stateChangeLock);
    LOG(VB_RECORD, LOG_INFO, LOC +
        QString("StopLiveTV(void) curRec: 0x%1 pseudoRec: 0x%2")
            .arg((uint64_t)m_curRecording,0,16)
            .arg((uint64_t)m_pseudoLiveTVRecording,0,16));

    if (m_internalState != kState_WatchingLiveTV)
        return;

    bool hadPseudoLiveTVRec = m_pseudoLiveTVRecording;
    CheckForRecGroupChange();

    if (!hadPseudoLiveTVRec && m_pseudoLiveTVRecording)
        NotifySchedulerOfRecording(m_curRecording);

    // Figure out next state and if needed recording end time.
    TVState next_state = kState_None;
    if (m_pseudoLiveTVRecording)
    {
        m_recordEndTime = GetRecordEndTime(m_pseudoLiveTVRecording);
        next_state = kState_RecordingOnly;
    }

    // Change to the appropriate state
    ChangeState(next_state);

    // Wait for state change to take effect...
    WaitForEventThreadSleep();

    // We are done with the tvchain...
    if (m_tvChain)
    {
        m_tvChain->DecrRef();
    }
    m_tvChain = nullptr;
}

/** \fn TVRec::PauseRecorder(void)
 *  \brief Tells "recorder" to pause, used for channel and input changes.
 *
 *   When the RecorderBase instance has paused it calls RecorderPaused(void)
 *
 *  \sa EncoderLink::PauseRecorder(void), RemoteEncoder::PauseRecorder(void),
 *      RecorderBase::Pause(void)
 */
void TVRec::PauseRecorder(void)
{
    QMutexLocker lock(&m_stateChangeLock);

    if (!m_recorder)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "PauseRecorder() called with no recorder");
        return;
    }

    m_recorder->Pause();
}

/** \fn TVRec::RecorderPaused(void)
 *  \brief This is a callback, called by the "recorder" instance when
 *         it has actually paused.
 *  \sa PauseRecorder(void)
 */
void TVRec::RecorderPaused(void)
{
    if (m_pauseNotify)
        WakeEventLoop();
}

/**
 *  \brief Toggles whether the current channel should be on our favorites list.
 */
void TVRec::ToggleChannelFavorite(const QString& changroupname)
{
    QMutexLocker lock(&m_stateChangeLock);

    if (!m_channel)
        return;

    // Get current channel id...
    uint    sourceid = m_channel->GetSourceID();
    QString channum  = m_channel->GetChannelName();
    uint chanid = ChannelUtil::GetChanID(sourceid, channum);

    if (!chanid)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Channel: \'%1\' was not found in the database.\n"
                    "\t\tMost likely, the 'starting channel' for this "
                    "Input Connection is invalid.\n"
                    "\t\tCould not toggle favorite.").arg(channum));
        return;
    }

    int changrpid = ChannelGroup::GetChannelGroupId(changroupname);
    if (changrpid <1)
    {
          LOG(VB_RECORD, LOG_ERR, LOC +
              QString("ToggleChannelFavorite: Invalid channel group name %1,")
                  .arg(changroupname));
    }
    else
    {
        bool result = ChannelGroup::ToggleChannel(chanid, changrpid, true);

        if (!result)
           LOG(VB_RECORD, LOG_ERR, LOC + "Unable to toggle channel favorite.");
        else
        {
           LOG(VB_RECORD, LOG_INFO, LOC +
               QString("Toggled channel favorite.channum %1, chan group %2")
                   .arg(channum, changroupname));
        }
    }
}

/** \fn TVRec::ChangePictureAttribute(PictureAdjustType,PictureAttribute,bool)
 *  \brief Returns current value [0,100] if it succeeds, -1 otherwise.
 *
 *  Note: In practice this only works with frame grabbing recorders.
 */
int TVRec::GetPictureAttribute(PictureAttribute attr)
{
    QMutexLocker lock(&m_stateChangeLock);
    if (!m_channel)
        return -1;

    int ret = m_channel->GetPictureAttribute(attr);

    return (ret < 0) ? -1 : ret / 655;
}

/** \fn TVRec::ChangePictureAttribute(PictureAdjustType,PictureAttribute,bool)
 *  \brief Changes brightness/contrast/colour/hue of a recording.
 *
 *  Note: In practice this only works with frame grabbing recorders.
 *
 *  \return current value [0,100] if it succeeds, -1 otherwise.
 */
int TVRec::ChangePictureAttribute(PictureAdjustType type,
                                  PictureAttribute  attr,
                                  bool              direction)
{
    QMutexLocker lock(&m_stateChangeLock);
    if (!m_channel)
        return -1;

    int ret = m_channel->ChangePictureAttribute(type, attr, direction);

    return (ret < 0) ? -1 : ret / 655;
}

/** \fn TVRec::GetInput(void) const
 *  \brief Returns current input.
 */
QString TVRec::GetInput(void) const
{
    if (m_channel)
        return m_channel->GetInputName();
    return {};
}

/** \fn TVRec::GetSourceID(void) const
 *  \brief Returns current source id.
 */
uint TVRec::GetSourceID(void) const
{
    if (m_channel)
        return m_channel->GetSourceID();
    return 0;
}

/**
 *  \brief Changes to the specified input.
 *
 *   You must call PauseRecorder(void) before calling this.
 *
 *  \param input Input to switch to, or "SwitchToNextInput".
 *  \return input we have switched to
 */
QString TVRec::SetInput(QString input)
{
    QMutexLocker lock(&m_stateChangeLock);
    QString origIn = input;
    LOG(VB_RECORD, LOG_INFO, LOC + "SetInput(" + input + ") -- begin");

    if (!m_channel)
    {
        LOG(VB_RECORD, LOG_INFO, LOC + "SetInput() -- end  no channel class");
        return {};
    }

    LOG(VB_RECORD, LOG_INFO, LOC + "SetInput(" + origIn + ":" + input +
        ") -- end  nothing to do");
    return input;
}

/** \fn TVRec::SetChannel(QString,uint)
 *  \brief Changes to a named channel on the current tuner.
 *
 *   You must call PauseRecorder() before calling this.
 *
 *  \param name channum of channel to change to
 *  \param requestType tells us what kind of request to actually send to
 *                     the tuning thread, kFlagDetect is usually sufficient
 */
void TVRec::SetChannel(const QString& name, uint requestType)
{
    QMutexLocker locker1(&m_setChannelLock);
    QMutexLocker locker2(&m_stateChangeLock);

    LOG(VB_CHANNEL, LOG_INFO, LOC +
        QString("SetChannel(%1) -- begin").arg(name));

    // Detect tuning request type if needed
    if (requestType & kFlagDetect)
    {
        WaitForEventThreadSleep();
        requestType = m_lastTuningRequest.m_flags & (kFlagRec | kFlagNoRec);
    }

    // Clear the RingBuffer reset flag, in case we wait for a reset below
    ClearFlags(kFlagRingBufferReady, __FILE__, __LINE__);

    // Clear out any EITScan channel change requests
    auto it = m_tuningRequests.begin();
    while (it != m_tuningRequests.end())
    {
        if ((*it).m_flags & kFlagEITScan)
            it = m_tuningRequests.erase(it);
        else
            ++it;
    }

    // Actually add the tuning request to the queue, and
    // then wait for it to start tuning
    m_tuningRequests.enqueue(TuningRequest(requestType, name));
    WaitForEventThreadSleep();

    // If we are using a recorder, wait for a RingBuffer reset
    if (requestType & kFlagRec)
    {
        while (!HasFlags(kFlagRingBufferReady))
            WaitForEventThreadSleep();
    }
    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("SetChannel(%1) -- end").arg(name));
}

/** \brief Queues up a channel change for the EITScanner.
 *
 *   Unlike the normal SetChannel() this does not block until
 *   the channel change occurs to avoid a deadlock if
 *   EITScanner::StopActiveScan() is called with the stateChangeLock
 *   held while the EITScanner is calling TVRec::SetChannel().
 */
bool TVRec::QueueEITChannelChange(const QString &name)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC +
        QString("QueueEITChannelChange(%1)").arg(name));

    bool ok = false;
    if (m_setChannelLock.tryLock())
    {
        if (m_stateChangeLock.tryLock())
        {
            if (m_tuningRequests.empty())
            {
                m_tuningRequests.enqueue(TuningRequest(kFlagEITScan, name));
                ok = true;
            }
            m_stateChangeLock.unlock();
        }
        m_setChannelLock.unlock();
    }

    LOG(VB_CHANNEL, LOG_DEBUG, LOC +
         QString("QueueEITChannelChange(%1) %2")
            .arg(name, ok ? "done" : "failed"));

    return ok;
}

void TVRec::GetNextProgram(BrowseDirection direction,
                           QString &title,       QString &subtitle,
                           QString &desc,        QString &category,
                           QString &starttime,   QString &endtime,
                           QString &callsign,    QString &iconpath,
                           QString &channum,     uint    &sourceChanid,
                           QString &seriesid,    QString &programid)
{
    QString compare     = "<=";
    QString sortorder   = "desc";
    uint    chanid      = 0;

    if (sourceChanid)
    {
        chanid = sourceChanid;

        if (BROWSE_UP == direction)
            chanid = m_channel->GetNextChannel(chanid, CHANNEL_DIRECTION_UP);
        else if (BROWSE_DOWN == direction)
            chanid = m_channel->GetNextChannel(chanid, CHANNEL_DIRECTION_DOWN);
        else if (BROWSE_FAVORITE == direction)
        {
            chanid = m_channel->GetNextChannel(
                chanid, CHANNEL_DIRECTION_FAVORITE);
        }
        else if (BROWSE_LEFT == direction)
        {
            compare = "<";
        }
        else if (BROWSE_RIGHT == direction)
        {
            compare = ">";
            sortorder = "asc";
        }
    }

    if (!chanid)
    {
        if (BROWSE_SAME == direction)
            chanid = m_channel->GetNextChannel(channum, CHANNEL_DIRECTION_SAME);
        else if (BROWSE_UP == direction)
            chanid = m_channel->GetNextChannel(channum, CHANNEL_DIRECTION_UP);
        else if (BROWSE_DOWN == direction)
            chanid = m_channel->GetNextChannel(channum, CHANNEL_DIRECTION_DOWN);
        else if (BROWSE_FAVORITE == direction)
        {
            chanid = m_channel->GetNextChannel(channum,
                                             CHANNEL_DIRECTION_FAVORITE);
        }
        else if (BROWSE_LEFT == direction)
        {
            chanid = m_channel->GetNextChannel(channum, CHANNEL_DIRECTION_SAME);
            compare = "<";
        }
        else if (BROWSE_RIGHT == direction)
        {
            chanid = m_channel->GetNextChannel(channum, CHANNEL_DIRECTION_SAME);
            compare = ">";
            sortorder = "asc";
        }
    }

    QString querystr = QString(
        "SELECT title,     subtitle, description, category, "
        "       starttime, endtime,  callsign,    icon,     "
        "       channum,   seriesid, programid "
        "FROM program, channel "
        "WHERE program.chanid = channel.chanid AND "
        "      channel.chanid = :CHANID        AND "
        "      starttime %1 :STARTTIME "
        "ORDER BY starttime %2 "
        "LIMIT 1").arg(compare, sortorder);

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(querystr);
    query.bindValue(":CHANID",    chanid);
    query.bindValue(":STARTTIME", starttime);

    // Clear everything now in case either query fails.
    title     = subtitle  = desc      = category  = "";
    starttime = endtime   = callsign  = iconpath  = "";
    channum   = seriesid  = programid = "";
    sourceChanid = 0;

    // Try to get the program info
    if (!query.exec() && !query.isActive())
    {
        MythDB::DBError("GetNextProgram -- get program info", query);
    }
    else if (query.next())
    {
        title     = query.value(0).toString();
        subtitle  = query.value(1).toString();
        desc      = query.value(2).toString();
        category  = query.value(3).toString();
        starttime = query.value(4).toString();
        endtime   = query.value(5).toString();
        callsign  = query.value(6).toString();
        iconpath  = query.value(7).toString();
        channum   = query.value(8).toString();
        seriesid  = query.value(9).toString();
        programid = query.value(10).toString();
        sourceChanid = chanid;
        return;
    }

    // Couldn't get program info, so get the channel info instead
    query.prepare(
        "SELECT channum, callsign, icon "
        "FROM channel "
        "WHERE chanid = :CHANID");
    query.bindValue(":CHANID", chanid);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("GetNextProgram -- get channel info", query);
    }
    else if (query.next())
    {
        sourceChanid = chanid;
        channum  = query.value(0).toString();
        callsign = query.value(1).toString();
        iconpath = query.value(2).toString();
    }
}

bool TVRec::GetChannelInfo(uint &chanid, uint &sourceid,
                           QString &callsign, QString &channum,
                           QString &channame, QString &xmltvid) const
{
    callsign.clear();
    channum.clear();
    channame.clear();
    xmltvid.clear();

    if ((!chanid || !sourceid) && !m_channel)
        return false;

    if (!chanid)
        chanid = (uint) std::max(m_channel->GetChanID(), 0);

    if (!sourceid)
        sourceid = m_channel->GetSourceID();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT callsign, channum, name, xmltvid "
        "FROM channel "
        "WHERE chanid = :CHANID");
    query.bindValue(":CHANID", chanid);
    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("GetChannelInfo", query);
        return false;
    }

    if (!query.next())
        return false;

    callsign = query.value(0).toString();
    channum  = query.value(1).toString();
    channame = query.value(2).toString();
    xmltvid  = query.value(3).toString();

    return true;
}

bool TVRec::SetChannelInfo(uint chanid, uint sourceid,
                           const QString& oldchannum,
                           const QString& callsign, const QString& channum,
                           const QString& channame, const QString& xmltvid)
{
    if (!chanid || !sourceid || channum.isEmpty())
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "UPDATE channel "
        "SET callsign = :CALLSIGN, "
        "    channum  = :CHANNUM,  "
        "    name     = :CHANNAME, "
        "    xmltvid  = :XMLTVID   "
        "WHERE chanid   = :CHANID AND "
        "      sourceid = :SOURCEID");
    query.bindValue(":CALLSIGN", callsign);
    query.bindValue(":CHANNUM",  channum);
    query.bindValue(":CHANNAME", channame);
    query.bindValue(":XMLTVID",  xmltvid);
    query.bindValue(":CHANID",   chanid);
    query.bindValue(":SOURCEID", sourceid);

    if (!query.exec())
    {
        MythDB::DBError("SetChannelInfo", query);
        return false;
    }

    if (m_channel)
        m_channel->Renumber(sourceid, oldchannum, channum);

    return true;
}

/** \fn TVRec::SetRingBuffer(RingBuffer*)
 *  \brief Sets "ringBuffer", deleting any existing RingBuffer.
 */
void TVRec::SetRingBuffer(MythMediaBuffer* Buffer)
{
    QMutexLocker lock(&m_stateChangeLock);

    MythMediaBuffer *oldbuffer = m_buffer;
    m_buffer = Buffer;

    if (oldbuffer && (oldbuffer != Buffer))
    {
        if (HasFlags(kFlagDummyRecorderRunning))
            ClearFlags(kFlagDummyRecorderRunning, __FILE__, __LINE__);
        delete oldbuffer;
    }

    m_switchingBuffer = false;
}

void TVRec::RingBufferChanged(MythMediaBuffer *Buffer, RecordingInfo *pginfo, RecordingQuality *recq)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "RingBufferChanged()");

    QMutexLocker lock(&m_stateChangeLock);

    if (pginfo)
    {
        if (m_curRecording)
        {
            FinishedRecording(m_curRecording, recq);
            m_curRecording->MarkAsInUse(false, kRecorderInUseID);
            delete m_curRecording;
        }
        m_recordEndTime = GetRecordEndTime(pginfo);
        m_curRecording = new RecordingInfo(*pginfo);
        m_curRecording->MarkAsInUse(true, kRecorderInUseID);
        m_curRecording->SetRecordingStatus(RecStatus::Recording);
    }

    SetRingBuffer(Buffer);
}

QString TVRec::TuningGetChanNum(const TuningRequest &request,
                                QString &input) const
{
    QString channum;

    if (request.m_program)
    {
        request.m_program->QueryTuningInfo(channum, input);
        return channum;
    }

    channum = request.m_channel;
    input   = request.m_input;

    // If this is Live TV startup, we need a channel...
    if (channum.isEmpty() && (request.m_flags & kFlagLiveTV))
    {
        if (!m_liveTVStartChannel.isEmpty())
            channum = m_liveTVStartChannel;
        else
        {
            input   = CardUtil::GetInputName(m_inputId);
            channum = CardUtil::GetStartChannel(m_inputId);
        }
    }
    if (request.m_flags & kFlagLiveTV)
        m_channel->Init(channum, false);

    if (m_channel && !channum.isEmpty() && (channum.indexOf("NextChannel") >= 0))
    {
        // FIXME This is just horrible
        int dir     = channum.right(channum.length() - 12).toInt();
        uint chanid = m_channel->GetNextChannel(0, static_cast<ChannelChangeDirection>(dir));
        channum     = ChannelUtil::GetChanNum(chanid);
    }

    return channum;
}

bool TVRec::TuningOnSameMultiplex(TuningRequest &request)
{
    if ((request.m_flags & kFlagAntennaAdjust) || request.m_input.isEmpty() ||
        !GetDTVRecorder() || m_signalMonitor || !m_channel || !m_channel->IsOpen())
    {
        return false;
    }

    uint    sourceid   = m_channel->GetSourceID();
    QString oldchannum = m_channel->GetChannelName();
    QString newchannum = request.m_channel;

    if (ChannelUtil::IsOnSameMultiplex(sourceid, newchannum, oldchannum))
    {
        MPEGStreamData *mpeg = GetDTVRecorder()->GetStreamData();
        auto *atsc = dynamic_cast<ATSCStreamData*>(mpeg);

        if (atsc)
        {
            uint major = 0;
            uint minor = 0;
            ChannelUtil::GetATSCChannel(sourceid, newchannum, major, minor);

            if (minor && atsc->HasChannel(major, minor))
            {
                request.m_majorChan = major;
                request.m_minorChan = minor;
                return true;
            }
        }

        if (mpeg)
        {
            uint progNum = ChannelUtil::GetProgramNumber(sourceid, newchannum);
            if (mpeg->HasProgram(progNum))
            {
                request.m_progNum = progNum;
                return true;
            }
        }
    }

    return false;
}

/** \fn TVRec::HandleTuning(void)
 *  \brief Handles all tuning events.
 *
 *   This method pops tuning events off the tuningState queue
 *   and does what needs to be done, mostly by calling one of
 *   the Tuning... methods.
 */
void TVRec::HandleTuning(void)
{
    if (!m_tuningRequests.empty())
    {
        TuningRequest request = m_tuningRequests.front();
        LOG(VB_RECORD, LOG_INFO, LOC +
            "HandleTuning Request: " + request.toString());

        QString input;
        request.m_channel = TuningGetChanNum(request, input);
        request.m_input   = input;

        if (TuningOnSameMultiplex(request))
            LOG(VB_CHANNEL, LOG_INFO, LOC + "On same multiplex");

        TuningShutdowns(request);

        // The dequeue isn't safe to do until now because we
        // release the stateChangeLock to teardown a recorder
        m_tuningRequests.dequeue();

        // Now we start new stuff
        if (request.m_flags & (kFlagRecording|kFlagLiveTV|
                               kFlagEITScan|kFlagAntennaAdjust))
        {
            if (!m_recorder)
            {
                LOG(VB_RECORD, LOG_INFO, LOC +
                    "No recorder yet, calling TuningFrequency");
                TuningFrequency(request);
            }
            else
            {
                LOG(VB_RECORD, LOG_INFO, LOC + "Waiting for recorder pause..");
                SetFlags(kFlagWaitingForRecPause, __FILE__, __LINE__);
            }
        }
        m_lastTuningRequest = request;
    }

    if (HasFlags(kFlagWaitingForRecPause))
    {
        if (!m_recorder || !m_recorder->IsPaused())
            return;

        ClearFlags(kFlagWaitingForRecPause, __FILE__, __LINE__);
        LOG(VB_RECORD, LOG_INFO, LOC +
            "Recorder paused, calling TuningFrequency");
        TuningFrequency(m_lastTuningRequest);
    }

    MPEGStreamData *streamData = nullptr;
    if (HasFlags(kFlagWaitingForSignal))
    {
        streamData = TuningSignalCheck();
        if (streamData == nullptr)
            return;
    }

    if (HasFlags(kFlagNeedToStartRecorder))
    {
        if (m_recorder)
            TuningRestartRecorder();
        else
            TuningNewRecorder(streamData);

        // If we got this far it is safe to set a new starting channel...
        if (m_channel)
            m_channel->StoreInputChannels();
    }
}

/** \fn TVRec::TuningShutdowns(const TuningRequest&)
 *  \brief This shuts down anything that needs to be shut down
 *         before handling the passed in tuning request.
 */
void TVRec::TuningShutdowns(const TuningRequest &request)
{
    LOG(VB_RECORD, LOG_INFO, LOC + QString("TuningShutdowns(%1)")
        .arg(request.toString()));

    if (m_scanner && !(request.m_flags & kFlagEITScan) &&
        HasFlags(kFlagEITScannerRunning))
    {
        m_scanner->StopActiveScan();
        ClearFlags(kFlagEITScannerRunning, __FILE__, __LINE__);
        auto secs = m_eitCrawlIdleStart + eit_start_rand(m_inputId, m_eitTransportTimeout);
        m_eitScanStartTime = MythDate::current().addSecs(secs.count());
    }

    if (m_scanner && !request.IsOnSameMultiplex())
        m_scanner->StopEITEventProcessing();

    if (HasFlags(kFlagSignalMonitorRunning))
    {
        MPEGStreamData *sd = nullptr;
        if (GetDTVSignalMonitor())
            sd = GetDTVSignalMonitor()->GetStreamData();
        TeardownSignalMonitor();
        ClearFlags(kFlagSignalMonitorRunning, __FILE__, __LINE__);

        // Delete StreamData if it is not in use by the recorder.
        MPEGStreamData *rec_sd = nullptr;
        if (GetDTVRecorder())
            rec_sd = GetDTVRecorder()->GetStreamData();
        if (sd && (sd != rec_sd))
            delete sd;
    }
    if (HasFlags(kFlagWaitingForSignal))
        ClearFlags(kFlagWaitingForSignal, __FILE__, __LINE__);

    // At this point any waits are canceled.

    if (request.m_flags & kFlagNoRec)
    {
        if (HasFlags(kFlagDummyRecorderRunning))
        {
            FinishedRecording(m_curRecording, nullptr);
            ClearFlags(kFlagDummyRecorderRunning, __FILE__, __LINE__);
            m_curRecording->MarkAsInUse(false, kRecorderInUseID);
        }

        if (HasFlags(kFlagRecorderRunning) ||
            (m_curRecording &&
             (m_curRecording->GetRecordingStatus() == RecStatus::Failed ||
              m_curRecording->GetRecordingStatus() == RecStatus::Failing)))
        {
            m_stateChangeLock.unlock();
            TeardownRecorder(request.m_flags);
            m_stateChangeLock.lock();
        }
        // At this point the recorders are shut down

        CloseChannel();
        // At this point the channel is shut down
    }

    if (m_buffer && (request.m_flags & kFlagKillRingBuffer))
    {
        LOG(VB_RECORD, LOG_INFO, LOC + "Tearing down RingBuffer");
        SetRingBuffer(nullptr);
        // At this point the ringbuffer is shut down
    }

    // Clear pending actions from last request
    ClearFlags(kFlagPendingActions, __FILE__, __LINE__);
}

/** \fn TVRec::TuningFrequency(const TuningRequest&)
 *  \brief Performs initial tuning required for any tuning event.
 *
 *   This figures out the channel name, and possibly the
 *   input name we need to pass to "channel" and then calls
 *   channel appropriately.
 *
 *   Then it adds any filters and sets any video capture attributes
 *   that need to be set.
 *
 *   The signal monitoring is started if possible. If it is started
 *   the kFlagWaitForSignal flag is set.
 *
 *   The kFlagNeedToStartRecorder flag is ald set if this isn't
 *   an EIT scan so that the recorder is started or restarted as
 *   appropriate.
 */
void TVRec::TuningFrequency(const TuningRequest &request)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("TuningFrequency(%1)")
        .arg(request.toString()));

    DTVChannel *dtvchan = GetDTVChannel();
    if (dtvchan)
    {
        MPEGStreamData *mpeg = nullptr;

        if (GetDTVRecorder())
            mpeg = GetDTVRecorder()->GetStreamData();

        // Tune with SI table standard (dvb, atsc, mpeg) from database, see issue #452
        m_channel->SetChannelByString(request.m_channel);

        const QString tuningmode = (HasFlags(kFlagEITScannerRunning)) ?
            dtvchan->GetSIStandard() :
            dtvchan->GetSuggestedTuningMode(
                kState_WatchingLiveTV == m_internalState);

        dtvchan->SetTuningMode(tuningmode);

        if (request.m_minorChan && (tuningmode == "atsc"))
        {
            auto *atsc = dynamic_cast<ATSCStreamData*>(mpeg);
            if (atsc)
                atsc->SetDesiredChannel(request.m_majorChan, request.m_minorChan);
        }
        else if (request.m_progNum >= 0)
        {
            if (mpeg)
                mpeg->SetDesiredProgram(request.m_progNum);
        }
    }

    if (request.IsOnSameMultiplex())
    {
        // Update the channel number for SwitchLiveTVRingBuffer (called from
        // TuningRestartRecorder).  This ensures that the livetvchain will be
        // updated with the new channel number
        if (m_channel)
        {
            m_channel->Renumber( CardUtil::GetSourceID(m_channel->GetInputID()),
                                 m_channel->GetChannelName(), request.m_channel );
        }

        QStringList slist;
        slist<<"message"<<QObject::tr("On known multiplex...");
        MythEvent me(QString("SIGNAL %1").arg(m_inputId), slist);
        gCoreContext->dispatch(me);

        SetFlags(kFlagNeedToStartRecorder, __FILE__, __LINE__);
        return;
    }

    QString channum = request.m_channel;

    bool ok1 = true;
    if (m_channel)
    {
        m_channel->Open();
        if (!channum.isEmpty())
            ok1 = m_channel->SetChannelByString(channum);
        else
            ok1 = false;
    }

    if (!ok1)
    {
        if (!(request.m_flags & kFlagLiveTV) || !(request.m_flags & kFlagEITScan))
        {
            if (m_curRecording)
                m_curRecording->SetRecordingStatus(RecStatus::Failed);

            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Failed to set channel to %1. Reverting to kState_None")
                    .arg(channum));
            if (kState_None != m_internalState)
                ChangeState(kState_None);
            else
                m_tuningRequests.enqueue(TuningRequest(kFlagKillRec));
            return;
        }

        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Failed to set channel to %1.").arg(channum));
    }

    bool mpts_only = GetDTVChannel() &&
                     GetDTVChannel()->GetFormat().compare("MPTS") == 0;
    if (mpts_only)
    {
        // Not using a signal monitor, so just set the status to recording
        SetRecordingStatus(RecStatus::Recording, __LINE__);
        if (m_curRecording)
        {
            m_curRecording->SetRecordingStatus(RecStatus::Recording);
        }
    }


    bool livetv = (request.m_flags & kFlagLiveTV) != 0U;
    bool antadj = (request.m_flags & kFlagAntennaAdjust) != 0U;
    bool use_sm = !mpts_only && SignalMonitor::IsRequired(m_genOpt.m_inputType);
    bool use_dr = use_sm && (livetv || antadj);
    bool has_dummy = false;

    if (use_dr)
    {
        // We need there to be a ringbuffer for these modes
        bool ok2 = false;
        RecordingInfo *tmp = m_pseudoLiveTVRecording;
        m_pseudoLiveTVRecording = nullptr;

        m_tvChain->SetInputType("DUMMY");

        if (!m_buffer)
            ok2 = CreateLiveTVRingBuffer(channum);
        else
            ok2 = SwitchLiveTVRingBuffer(channum, true, false);
        m_pseudoLiveTVRecording = tmp;

        m_tvChain->SetInputType(m_genOpt.m_inputType);

        if (!ok2)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create RingBuffer 1");
            return;
        }

        has_dummy = true;
    }

    // Start signal monitoring for devices capable of monitoring
    if (use_sm)
    {
        LOG(VB_RECORD, LOG_INFO, LOC + "Starting Signal Monitor");
        bool error = false;
        if (!SetupSignalMonitor(
                !antadj, (request.m_flags & kFlagEITScan) != 0U, livetv || antadj))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to setup signal monitor");
            if (m_signalMonitor)
            {
                delete m_signalMonitor;
                m_signalMonitor = nullptr;
            }

            // pretend the signal monitor is running to prevent segfault
            SetFlags(kFlagSignalMonitorRunning, __FILE__, __LINE__);
            ClearFlags(kFlagWaitingForSignal, __FILE__, __LINE__);
            error = true;
        }

        if (m_signalMonitor)
        {
            if (request.m_flags & kFlagEITScan)
            {
                GetDTVSignalMonitor()->GetStreamData()->
                    SetVideoStreamsRequired(0);
                GetDTVSignalMonitor()->IgnoreEncrypted(true);
            }

            SetFlags(kFlagSignalMonitorRunning, __FILE__, __LINE__);
            ClearFlags(kFlagWaitingForSignal, __FILE__, __LINE__);
            if (!antadj)
            {
                QDateTime expire = MythDate::current();

                SetFlags(kFlagWaitingForSignal, __FILE__, __LINE__);
                if (m_curRecording)
                {
                    m_reachedRecordingDeadline = m_reachedPreFail = false;
                    // If startRecordingDeadline is passed, this
                    // recording is marked as failed, so the scheduler
                    // can try another showing.
                    m_startRecordingDeadline =
                        expire.addMSecs(m_genOpt.m_channelTimeout);
                    m_preFailDeadline =
                        expire.addMSecs(m_genOpt.m_channelTimeout * 2 / 3);
                    // Keep trying to record this showing (even if it
                    // has been marked as failed) until the scheduled
                    // end time.
                    m_signalMonitorDeadline =
                        m_curRecording->GetRecordingEndTime().addSecs(-10);

                    LOG(VB_CHANNEL, LOG_DEBUG, LOC +
                        QString("Pre-fail start deadline: %1 "
                                "Start recording deadline: %2 "
                                "Good signal deadline: %3")
                        .arg(m_preFailDeadline.toLocalTime()
                             .toString("hh:mm:ss.zzz"),
                             m_startRecordingDeadline.toLocalTime()
                             .toString("hh:mm:ss.zzz"),
                             m_signalMonitorDeadline.toLocalTime()
                             .toString("hh:mm:ss.zzz")));
                }
                else
                {
                    m_signalMonitorDeadline =
                        expire.addMSecs(m_genOpt.m_channelTimeout);
                }
                m_signalMonitorCheckCnt = 0;

                //System Event TUNING_TIMEOUT deadline
                m_signalEventCmdTimeout = expire.addMSecs(m_genOpt.m_channelTimeout);
                m_signalEventCmdSent = false;
            }
        }

        if (has_dummy && m_buffer)
        {
            // Make sure recorder doesn't point to bogus ringbuffer before
            // it is potentially restarted without a new ringbuffer, if
            // the next channel won't tune and the user exits LiveTV.
            if (m_recorder)
                m_recorder->SetRingBuffer(nullptr);

            SetFlags(kFlagDummyRecorderRunning, __FILE__, __LINE__);
            LOG(VB_RECORD, LOG_INFO, "DummyDTVRecorder -- started");
            SetFlags(kFlagRingBufferReady, __FILE__, __LINE__);
        }

        // if we had problems starting the signal monitor,
        // we don't want to start the recorder...
        if (error)
            return;
    }

    // Request a recorder, if the command is a recording command
    ClearFlags(kFlagNeedToStartRecorder, __FILE__, __LINE__);
    if (request.m_flags & kFlagRec && !antadj)
        SetFlags(kFlagNeedToStartRecorder, __FILE__, __LINE__);
}

/** \fn TVRec::TuningSignalCheck(void)
 *  \brief This checks if we have a channel lock.
 *
 *   If we have a channel lock this shuts down the signal monitoring.
 *
 *  \return MPEGStreamData pointer if we have a complete lock, nullptr otherwise
 */
MPEGStreamData *TVRec::TuningSignalCheck(void)
{
    RecStatus::Type newRecStatus = RecStatus::Unknown;
    bool keep_trying  = false;
    QDateTime current_time = MythDate::current();

    if ((m_signalMonitor->IsErrored() || current_time > m_signalEventCmdTimeout) &&
         !m_signalEventCmdSent)
    {
        gCoreContext->SendSystemEvent(QString("TUNING_SIGNAL_TIMEOUT CARDID %1")
                                      .arg(m_inputId));
        m_signalEventCmdSent = true;
    }

    if (m_signalMonitor->IsAllGood())
    {
        LOG(VB_RECORD, LOG_INFO, LOC + "TuningSignalCheck: Good signal");
        if (m_curRecording && (current_time > m_startRecordingDeadline))
        {
            newRecStatus = RecStatus::Failing;
            m_curRecording->SaveVideoProperties(VID_DAMAGED, VID_DAMAGED);

            QString desc = tr("Good signal seen after %1 ms")
                           .arg(m_genOpt.m_channelTimeout +
                        m_startRecordingDeadline.msecsTo(current_time));
            QString title = m_curRecording->GetTitle();
            if (!m_curRecording->GetSubtitle().isEmpty())
                title += " - " + m_curRecording->GetSubtitle();

            MythNotification mn(MythNotification::kCheck, desc,
                                "Recording", title,
                                tr("See 'Tuning timeout' in mythtv-setup "
                                   "for this input."));
            gCoreContext->SendEvent(mn);

            LOG(VB_GENERAL, LOG_WARNING, LOC +
                QString("It took longer than %1 ms to get a signal lock. "
                        "Keeping status of '%2'")
                .arg(m_genOpt.m_channelTimeout)
                .arg(RecStatus::toString(newRecStatus, kSingleRecord)));
            LOG(VB_GENERAL, LOG_WARNING, LOC +
                "See 'Tuning timeout' in mythtv-setup for this input");
        }
        else
        {
            newRecStatus = RecStatus::Recording;
        }
    }
    else if (m_signalMonitor->IsErrored() || current_time > m_signalMonitorDeadline)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "TuningSignalCheck: SignalMonitor " +
            (m_signalMonitor->IsErrored() ? "failed" : "timed out"));

        ClearFlags(kFlagNeedToStartRecorder, __FILE__, __LINE__);
        newRecStatus = RecStatus::Failed;

        if (m_scanner && HasFlags(kFlagEITScannerRunning))
        {
            m_tuningRequests.enqueue(TuningRequest(kFlagNoRec));
        }
    }
    else if (m_curRecording && !m_reachedPreFail && current_time > m_preFailDeadline)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "TuningSignalCheck: Hit pre-fail timeout");
        SendMythSystemRecEvent("REC_PREFAIL", m_curRecording);
        m_reachedPreFail = true;
        return nullptr;
    }
    else if (m_curRecording && !m_reachedRecordingDeadline &&
             current_time > m_startRecordingDeadline)
    {
        newRecStatus = RecStatus::Failing;
        m_reachedRecordingDeadline = true;
        keep_trying = true;

        SendMythSystemRecEvent("REC_FAILING", m_curRecording);

        QString desc = tr("Taking more than %1 ms to get a lock.")
                       .arg(m_genOpt.m_channelTimeout);
        QString title = m_curRecording->GetTitle();
        if (!m_curRecording->GetSubtitle().isEmpty())
            title += " - " + m_curRecording->GetSubtitle();

        MythNotification mn(MythNotification::kError, desc,
                            "Recording", title,
                            tr("See 'Tuning timeout' in mythtv-setup "
                               "for this input."));
        mn.SetDuration(30s);
        gCoreContext->SendEvent(mn);

        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("TuningSignalCheck: taking more than %1 ms to get a lock. "
                    "marking this recording as '%2'.")
            .arg(m_genOpt.m_channelTimeout)
            .arg(RecStatus::toString(newRecStatus, kSingleRecord)));
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "See 'Tuning timeout' in mythtv-setup for this input");
    }
    else
    {
        if (m_signalMonitorCheckCnt) // Don't flood log file
            --m_signalMonitorCheckCnt;
        else
        {
            LOG(VB_RECORD, LOG_INFO, LOC +
                QString("TuningSignalCheck: Still waiting.  Will timeout @ %1")
                .arg(m_signalMonitorDeadline.toLocalTime()
                     .toString("hh:mm:ss.zzz")));
            m_signalMonitorCheckCnt = 5;
        }
        return nullptr;
    }

    SetRecordingStatus(newRecStatus, __LINE__);

    if (m_curRecording)
    {
        m_curRecording->SetRecordingStatus(newRecStatus);
        MythEvent me(QString("UPDATE_RECORDING_STATUS %1 %2 %3 %4 %5")
                    .arg(m_curRecording->GetInputID())
                    .arg(m_curRecording->GetChanID())
                    .arg(m_curRecording->GetScheduledStartTime(MythDate::ISODate))
                    .arg(newRecStatus)
                    .arg(m_curRecording->GetRecordingEndTime(MythDate::ISODate)));
        gCoreContext->dispatch(me);
    }

    if (keep_trying)
        return nullptr;

    // grab useful data from DTV signal monitor before we kill it...
    MPEGStreamData *streamData = nullptr;
    if (GetDTVSignalMonitor())
        streamData = GetDTVSignalMonitor()->GetStreamData();

    if (!HasFlags(kFlagEITScannerRunning))
    {
        // shut down signal monitoring
        TeardownSignalMonitor();
        ClearFlags(kFlagSignalMonitorRunning, __FILE__, __LINE__);
    }
    ClearFlags(kFlagWaitingForSignal, __FILE__, __LINE__);

    if (streamData)
    {
        auto *dsd = dynamic_cast<DVBStreamData*>(streamData);
        if (dsd)
            dsd->SetDishNetEIT(is_dishnet_eit(m_inputId));
        if (m_scanner)
        {
            if (get_use_eit(GetInputId()))
            {
                m_scanner->StartEITEventProcessing(m_channel, streamData);
            }
            else
            {
                LOG(VB_EIT, LOG_INFO, LOC +
                    QString("EIT scanning disabled for video source %1")
                        .arg(GetSourceID()));            }
        }
    }

    return streamData;
}

static int init_jobs(const RecordingInfo *rec, RecordingProfile &profile,
                      bool on_host, bool transcode_bfr_comm, bool on_line_comm)
{
    if (!rec)
        return 0; // no jobs for Live TV recordings..

    int jobs = 0; // start with no jobs

    // grab standard jobs flags from program info
    JobQueue::AddJobsToMask(rec->GetAutoRunJobs(), jobs);

    // disable commercial flagging on PBS, BBC, etc.
    if (rec->IsCommercialFree())
        JobQueue::RemoveJobsFromMask(JOB_COMMFLAG, jobs);

    // disable transcoding if the profile does not allow auto transcoding
    const StandardSetting *autoTrans = profile.byName("autotranscode");
    if ((!autoTrans) || (autoTrans->getValue().toInt() == 0))
        JobQueue::RemoveJobsFromMask(JOB_TRANSCODE, jobs);

    bool ml = JobQueue::JobIsInMask(JOB_METADATA, jobs);
    if (ml)
    {
        // When allowed, metadata lookup should occur at the
        // start of a recording to make the additional info
        // available immediately (and for use in future jobs).
        QString host = (on_host) ? gCoreContext->GetHostName() : "";
        JobQueue::QueueJob(JOB_METADATA,
                           rec->GetChanID(),
                           rec->GetRecordingStartTime(), "", "",
                           host, JOB_LIVE_REC);

        // don't do regular metadata lookup, we won't need it.
        JobQueue::RemoveJobsFromMask(JOB_METADATA, jobs);
    }

    // is commercial flagging enabled, and is on-line comm flagging enabled?
    bool rt = JobQueue::JobIsInMask(JOB_COMMFLAG, jobs) && on_line_comm;
    // also, we either need transcoding to be disabled or
    // we need to be allowed to commercial flag before transcoding?
    rt &= JobQueue::JobIsNotInMask(JOB_TRANSCODE, jobs) ||
        !transcode_bfr_comm;
    if (rt)
    {
        // queue up real-time (i.e. on-line) commercial flagging.
        QString host = (on_host) ? gCoreContext->GetHostName() : "";
        JobQueue::QueueJob(JOB_COMMFLAG,
                           rec->GetChanID(),
                           rec->GetRecordingStartTime(), "", "",
                           host, JOB_LIVE_REC);

        // don't do regular comm flagging, we won't need it.
        JobQueue::RemoveJobsFromMask(JOB_COMMFLAG, jobs);
    }

    return jobs;
}

QString TVRec::LoadProfile(void *tvchain, RecordingInfo *rec,
                           RecordingProfile &profile) const
{
    // Determine the correct recording profile.
    // In LiveTV mode use "Live TV" profile, otherwise use the
    // recording's specified profile. If the desired profile can't
    // be found, fall back to the "Default" profile for input type.
    QString profileName = "Live TV";
    if (!tvchain && rec)
        profileName = rec->GetRecordingRule()->m_recProfile;

    QString profileRequested = profileName;

    if (profile.loadByType(profileName, m_genOpt.m_inputType,
                           m_genOpt.m_videoDev))
    {
        LOG(VB_RECORD, LOG_INFO, LOC +
            QString("Using profile '%1' to record")
                .arg(profileName));
    }
    else
    {
        profileName = "Default";
        if (profile.loadByType(profileName, m_genOpt.m_inputType, m_genOpt.m_videoDev))
        {
            LOG(VB_RECORD, LOG_INFO, LOC +
                QString("Profile '%1' not found, using "
                        "fallback profile '%2' to record")
                    .arg(profileRequested, profileName));
        }
        else
        {
            LOG(VB_RECORD, LOG_ERR, LOC +
                QString("Profile '%1' not found, and unable "
                        "to load fallback profile '%2'.  Results "
                        "may be unpredicable")
                    .arg(profileRequested, profileName));
        }
    }

    return profileName;
}

/** \fn TVRec::TuningNewRecorderReal(MPEGStreamData*)
 *  \brief Helper function for TVRec::TuningNewRecorder.
 */
bool TVRec::TuningNewRecorderReal(MPEGStreamData *streamData,
                                  RecordingInfo **rec,
                                  RecordingProfile& profile,
                                  bool had_dummyrec)
{
    if (m_tvChain)
    {
        bool ok = false;
        if (!m_buffer)
        {
            ok = CreateLiveTVRingBuffer(m_channel->GetChannelName());
            SetFlags(kFlagRingBufferReady, __FILE__, __LINE__);
        }
        else
        {
            ok = SwitchLiveTVRingBuffer(m_channel->GetChannelName(),
                                        true, !had_dummyrec && m_recorder);
        }
        if (!ok)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create RingBuffer 2");
            return false;
        }
        *rec = m_curRecording;  // new'd in Create/SwitchLiveTVRingBuffer()
    }

    if (m_lastTuningRequest.m_flags & kFlagRecording)
    {
        bool write = m_genOpt.m_inputType != "IMPORT";
        QString pathname = (*rec)->GetPathname();
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("rec->GetPathname(): '%1'")
                .arg(pathname));
        SetRingBuffer(MythMediaBuffer::Create(pathname, write));
        if (!m_buffer->IsOpen() && write)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("RingBuffer '%1' not open...")
                    .arg(pathname));
            SetRingBuffer(nullptr);
            ClearFlags(kFlagPendingActions, __FILE__, __LINE__);
            return false;
        }
    }

    if (!m_buffer)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Failed to start recorder!  ringBuffer is NULL\n"
                    "\t\t\t\t  Tuning request was %1\n")
                .arg(m_lastTuningRequest.toString()));

        if (HasFlags(kFlagLiveTV))
        {
            QString message = QString("QUIT_LIVETV %1").arg(m_inputId);
            MythEvent me(message);
            gCoreContext->dispatch(me);
        }
        return false;
    }

    if (m_channel && m_genOpt.m_inputType == "MJPEG")
        m_channel->Close(); // Needed because of NVR::MJPEGInit()

    LOG(VB_GENERAL, LOG_INFO, LOC + "TuningNewRecorder - CreateRecorder()");
    m_recorder = RecorderBase::CreateRecorder(this, m_channel, profile, m_genOpt);

    if (m_recorder)
    {
        m_recorder->SetRingBuffer(m_buffer);
        m_recorder->Initialize();
        if (m_recorder->IsErrored())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to initialize recorder!");
            delete m_recorder;
            m_recorder = nullptr;
        }
    }

    if (!m_recorder)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Failed to start recorder!\n"
                    "\t\t\t\t  Tuning request was %1\n")
                .arg(m_lastTuningRequest.toString()));

        if (HasFlags(kFlagLiveTV))
        {
            QString message = QString("QUIT_LIVETV %1").arg(m_inputId);
            MythEvent me(message);
            gCoreContext->dispatch(me);
        }
        TeardownRecorder(kFlagKillRec);
        if (m_tvChain)
            (*rec) = nullptr;
        return false;
    }

    if (*rec)
        m_recorder->SetRecording(*rec);

    if (GetDTVRecorder() && streamData)
    {
        const StandardSetting *setting = profile.byName("recordingtype");
        if (setting)
            streamData->SetRecordingType(setting->getValue());
        GetDTVRecorder()->SetStreamData(streamData);
    }

    if (m_channel && m_genOpt.m_inputType == "MJPEG")
        m_channel->Open(); // Needed because of NVR::MJPEGInit()

    // Setup for framebuffer capture devices..
    if (m_channel)
    {
        SetVideoFiltersForChannel(m_channel->GetSourceID(),
                                  m_channel->GetChannelName());
    }

    if (GetV4LChannel())
    {
        m_channel->InitPictureAttributes();
        CloseChannel();
    }

    m_recorderThread = new MThread("RecThread", m_recorder);
    m_recorderThread->start();

    // Wait for recorder to start.
    m_stateChangeLock.unlock();
    while (!m_recorder->IsRecording() && !m_recorder->IsErrored())
        std::this_thread::sleep_for(5us);
    m_stateChangeLock.lock();

    if (GetV4LChannel())
        m_channel->SetFd(m_recorder->GetVideoFd());

    SetFlags(kFlagRecorderRunning | kFlagRingBufferReady, __FILE__, __LINE__);

    ClearFlags(kFlagNeedToStartRecorder, __FILE__, __LINE__);

    //workaround for failed import recordings, no signal monitor means we never
    //go to recording state and the status here seems to override the status
    //set in the importrecorder and backend via setrecordingstatus
    if (m_genOpt.m_inputType == "IMPORT")
    {
        SetRecordingStatus(RecStatus::Recording, __LINE__);
        if (m_curRecording)
            m_curRecording->SetRecordingStatus(RecStatus::Recording);
    }

    return true;
}

/** \fn TVRec::TuningNewRecorder(MPEGStreamData*)
 *  \brief Creates a recorder instance.
 */
void TVRec::TuningNewRecorder(MPEGStreamData *streamData)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "Starting Recorder");

    bool had_dummyrec = false;
    if (HasFlags(kFlagDummyRecorderRunning))
    {
        FinishedRecording(m_curRecording, nullptr);
        ClearFlags(kFlagDummyRecorderRunning, __FILE__, __LINE__);
        m_curRecording->MarkAsInUse(false, kRecorderInUseID);
        had_dummyrec = true;
    }

    RecordingInfo *rec = m_lastTuningRequest.m_program;

    RecordingProfile profile;
    m_recProfileName = LoadProfile(m_tvChain, rec, profile);

    if (TuningNewRecorderReal(streamData, &rec, profile, had_dummyrec))
        return;

    SetRecordingStatus(RecStatus::Failed, __LINE__, true);
    ChangeState(kState_None);

    if (rec)
    {
        // Make sure the scheduler knows...
        rec->SetRecordingStatus(RecStatus::Failed);
        LOG(VB_RECORD, LOG_INFO, LOC +
            QString("TuningNewRecorder -- UPDATE_RECORDING_STATUS: %1")
            .arg(RecStatus::toString(RecStatus::Failed, kSingleRecord)));
        MythEvent me(QString("UPDATE_RECORDING_STATUS %1 %2 %3 %4 %5")
                     .arg(rec->GetInputID())
                     .arg(rec->GetChanID())
                     .arg(rec->GetScheduledStartTime(MythDate::ISODate))
                     .arg(RecStatus::Failed)
                     .arg(rec->GetRecordingEndTime(MythDate::ISODate)));
        gCoreContext->dispatch(me);
    }

    if (m_tvChain)
        delete rec;
}

/** \fn TVRec::TuningRestartRecorder(void)
 *  \brief Restarts a stopped recorder or unpauses a paused recorder.
 */
void TVRec::TuningRestartRecorder(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "Restarting Recorder");

    bool had_dummyrec = false;

    if (m_curRecording)
    {
        FinishedRecording(m_curRecording, nullptr);
        m_curRecording->MarkAsInUse(false, kRecorderInUseID);
    }

    if (HasFlags(kFlagDummyRecorderRunning))
    {
        ClearFlags(kFlagDummyRecorderRunning, __FILE__, __LINE__);
        had_dummyrec = true;
    }

    SwitchLiveTVRingBuffer(m_channel->GetChannelName(), true, !had_dummyrec);

    if (had_dummyrec)
    {
        m_recorder->SetRingBuffer(m_buffer);
        ProgramInfo *progInfo = m_tvChain->GetProgramAt(-1);
        RecordingInfo recinfo(*progInfo);
        delete progInfo;
        recinfo.SetInputID(m_inputId);
        m_recorder->SetRecording(&recinfo);
    }
    m_recorder->Reset();

    // Set file descriptor of channel from recorder for V4L
    if (GetV4LChannel())
        m_channel->SetFd(m_recorder->GetVideoFd());

    // Some recorders unpause on Reset, others do not...
    m_recorder->Unpause();

    if (m_pseudoLiveTVRecording && m_curRecording)
    {
        ProgramInfo *rcinfo1 = m_pseudoLiveTVRecording;
        QString msg1 = QString("Recording: %1 %2 %3 %4")
            .arg(rcinfo1->GetTitle(), QString::number(rcinfo1->GetChanID()),
                 rcinfo1->GetRecordingStartTime(MythDate::ISODate),
                 rcinfo1->GetRecordingEndTime(MythDate::ISODate));
        ProgramInfo *rcinfo2 = m_tvChain->GetProgramAt(-1);
        QString msg2 = QString("Recording: %1 %2 %3 %4")
            .arg(rcinfo2->GetTitle(), QString::number(rcinfo2->GetChanID()),
                 rcinfo2->GetRecordingStartTime(MythDate::ISODate),
                 rcinfo2->GetRecordingEndTime(MythDate::ISODate));
        delete rcinfo2;
        LOG(VB_RECORD, LOG_INFO, LOC + "Pseudo LiveTV recording starting." +
                "\n\t\t\t" + msg1 + "\n\t\t\t" + msg2);

        m_curRecording->SaveAutoExpire(
            m_curRecording->GetRecordingRule()->GetAutoExpire());

        m_curRecording->ApplyRecordRecGroupChange(m_curRecording->GetRecordingRule()->m_recGroupID);

        InitAutoRunJobs(m_curRecording, kAutoRunProfile, nullptr, __LINE__);
    }

    ClearFlags(kFlagNeedToStartRecorder, __FILE__, __LINE__);
}

void TVRec::SetFlags(uint f, const QString & file, int line)
{
    QMutexLocker lock(&m_stateChangeLock);
    m_stateFlags |= f;
    LOG(VB_RECORD, LOG_INFO, LOC + QString("SetFlags(%1) -> %2 @ %3:%4")
        .arg(FlagToString(f), FlagToString(m_stateFlags), file, QString::number(line)));
    WakeEventLoop();
}

void TVRec::ClearFlags(uint f, const QString & file, int line)
{
    QMutexLocker lock(&m_stateChangeLock);
    m_stateFlags &= ~f;
    LOG(VB_RECORD, LOG_INFO, LOC + QString("ClearFlags(%1) -> %2 @ %3:%4")
        .arg(FlagToString(f), FlagToString(m_stateFlags), file, QString::number(line)));
    WakeEventLoop();
}

QString TVRec::FlagToString(uint f)
{
    QString msg("");

    // General flags
    if (kFlagFrontendReady & f)
        msg += "FrontendReady,";
    if (kFlagRunMainLoop & f)
        msg += "RunMainLoop,";
    if (kFlagExitPlayer & f)
        msg += "ExitPlayer,";
    if (kFlagFinishRecording & f)
        msg += "FinishRecording,";
    if (kFlagErrored & f)
        msg += "Errored,";
    if (kFlagCancelNextRecording & f)
        msg += "CancelNextRecording,";

    // Tuning flags
    if ((kFlagRec & f) == kFlagRec)
        msg += "REC,";
    else
    {
        if (kFlagLiveTV & f)
            msg += "LiveTV,";
        if (kFlagRecording & f)
            msg += "Recording,";
    }
    if ((kFlagNoRec & f) == kFlagNoRec)
        msg += "NOREC,";
    else
    {
        if (kFlagEITScan & f)
            msg += "EITScan,";
        if (kFlagCloseRec & f)
            msg += "CloseRec,";
        if (kFlagKillRec & f)
            msg += "KillRec,";
        if (kFlagAntennaAdjust & f)
            msg += "AntennaAdjust,";
    }
    if ((kFlagPendingActions & f) == kFlagPendingActions)
        msg += "PENDINGACTIONS,";
    else
    {
        if (kFlagWaitingForRecPause & f)
            msg += "WaitingForRecPause,";
        if (kFlagWaitingForSignal & f)
            msg += "WaitingForSignal,";
        if (kFlagNeedToStartRecorder & f)
            msg += "NeedToStartRecorder,";
        if (kFlagKillRingBuffer & f)
            msg += "KillRingBuffer,";
    }
    if ((kFlagAnyRunning & f) == kFlagAnyRunning)
        msg += "ANYRUNNING,";
    else
    {
        if (kFlagSignalMonitorRunning & f)
            msg += "SignalMonitorRunning,";
        if (kFlagEITScannerRunning & f)
            msg += "EITScannerRunning,";
        if ((kFlagAnyRecRunning & f) == kFlagAnyRecRunning)
            msg += "ANYRECRUNNING,";
        else
        {
            if (kFlagDummyRecorderRunning & f)
                msg += "DummyRecorderRunning,";
            if (kFlagRecorderRunning & f)
                msg += "RecorderRunning,";
        }
    }
    if (kFlagRingBufferReady & f)
        msg += "RingBufferReady,";

    if (msg.isEmpty())
        msg = QString("0x%1").arg(f,0,16);

    return msg;
}

bool TVRec::WaitForNextLiveTVDir(void)
{
    QMutexLocker lock(&m_nextLiveTVDirLock);

    bool found = !m_nextLiveTVDir.isEmpty();
    if (!found && m_triggerLiveTVDir.wait(&m_nextLiveTVDirLock, 500))
    {
        found = !m_nextLiveTVDir.isEmpty();
    }

    return found;
}

void TVRec::SetNextLiveTVDir(QString dir)
{
    QMutexLocker lock(&m_nextLiveTVDirLock);

    m_nextLiveTVDir = std::move(dir);
    m_triggerLiveTVDir.wakeAll();
}

bool TVRec::GetProgramRingBufferForLiveTV(RecordingInfo **pginfo,
                                          MythMediaBuffer **Buffer,
                                          const QString & channum)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "GetProgramRingBufferForLiveTV()");
    if (!m_channel || !m_tvChain || !pginfo || !Buffer)
        return false;

    m_nextLiveTVDirLock.lock();
    m_nextLiveTVDir.clear();
    m_nextLiveTVDirLock.unlock();

    // Dispatch this early, the response can take a while.
    MythEvent me(QString("QUERY_NEXT_LIVETV_DIR %1").arg(m_inputId));
    gCoreContext->dispatch(me);

    uint    sourceid = m_channel->GetSourceID();
    int     chanid   = ChannelUtil::GetChanID(sourceid, channum);

    if (chanid < 0)
    {
        // Test setups might have zero channels
        if (m_genOpt.m_inputType == "IMPORT" || m_genOpt.m_inputType == "DEMO")
            chanid = 9999;
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Channel: \'%1\' was not found in the database.\n"
                        "\t\tMost likely, the 'starting channel' for this "
                        "Input Connection is invalid.\n"
                        "\t\tCould not start livetv.").arg(channum));
            return false;
        }
    }

    auto hoursMax =
        gCoreContext->GetDurSetting<std::chrono::hours>("MaxHoursPerLiveTVRecording", 8h);
    if (hoursMax <= 0h)
        hoursMax = 8h;

    RecordingInfo *prog = nullptr;
    if (m_pseudoLiveTVRecording)
        prog = new RecordingInfo(*m_pseudoLiveTVRecording);
    else
    {
        prog = new RecordingInfo(
            chanid, MythDate::current(true), true, hoursMax);
    }

    prog->SetInputID(m_inputId);

    if (prog->GetRecordingStartTime() == prog->GetRecordingEndTime())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "GetProgramRingBufferForLiveTV()"
                "\n\t\t\tProgramInfo is invalid."
                "\n" + prog->toString());
        prog->SetScheduledEndTime(prog->GetRecordingStartTime().addSecs(3600));
        prog->SetRecordingEndTime(prog->GetScheduledEndTime());

        prog->SetChanID(chanid);
    }

    if (!m_pseudoLiveTVRecording)
        prog->SetRecordingStartTime(MythDate::current(true));

    prog->SetStorageGroup("LiveTV");

    if (WaitForNextLiveTVDir())
    {
        QMutexLocker lock(&m_nextLiveTVDirLock);
        prog->SetPathname(m_nextLiveTVDir);
    }
    else
    {
        StorageGroup sgroup("LiveTV", gCoreContext->GetHostName());
        prog->SetPathname(sgroup.FindNextDirMostFree());
    }

    if (!m_pseudoLiveTVRecording)
        prog->SetRecordingGroup("LiveTV");

    StartedRecording(prog);

    *Buffer = MythMediaBuffer::Create(prog->GetPathname(), true);
    if (!(*Buffer) || !(*Buffer)->IsOpen())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("RingBuffer '%1' not open...")
                .arg(prog->GetPathname()));

        delete *Buffer;
        delete prog;

        return false;
    }

    *pginfo = prog;
    return true;
}

bool TVRec::CreateLiveTVRingBuffer(const QString & channum)
{
    LOG(VB_RECORD, LOG_INFO, LOC + QString("CreateLiveTVRingBuffer(%1)")
            .arg(channum));

    RecordingInfo *pginfo = nullptr;
    MythMediaBuffer *buffer = nullptr;

    if (!m_channel ||
        !m_channel->CheckChannel(channum))
    {
        ChangeState(kState_None);
        return false;
    }

    if (!GetProgramRingBufferForLiveTV(&pginfo, &buffer, channum))
    {
        ClearFlags(kFlagPendingActions, __FILE__, __LINE__);
        ChangeState(kState_None);
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("CreateLiveTVRingBuffer(%1) failed").arg(channum));
        return false;
    }

    SetRingBuffer(buffer);

    pginfo->SaveAutoExpire(kLiveTVAutoExpire);
    if (!m_pseudoLiveTVRecording)
        pginfo->ApplyRecordRecGroupChange(RecordingInfo::kLiveTVRecGroup);

    bool discont = (m_tvChain->TotalSize() > 0);
    m_tvChain->AppendNewProgram(pginfo, m_channel->GetChannelName(),
                                m_channel->GetInputName(), discont);

    if (m_curRecording)
    {
        m_curRecording->MarkAsInUse(false, kRecorderInUseID);
        delete m_curRecording;
    }

    m_curRecording = pginfo;
    m_curRecording->MarkAsInUse(true, kRecorderInUseID);

    return true;
}

bool TVRec::SwitchLiveTVRingBuffer(const QString & channum,
                                   bool discont, bool set_rec)
{
    QString msg;
    if (m_curRecording)
    {
        msg = QString(" curRec(%1) curRec.size(%2)")
            .arg(m_curRecording->MakeUniqueKey())
            .arg(m_curRecording->GetFilesize());
    }
    LOG(VB_RECORD, LOG_INFO, LOC +
        QString("SwitchLiveTVRingBuffer(discont %1, set_next_rec %2)")
        .arg(discont).arg(set_rec) + msg);

    RecordingInfo *pginfo = nullptr;
    MythMediaBuffer *buffer = nullptr;

    if (!m_channel ||
        !m_channel->CheckChannel(channum))
    {
        ChangeState(kState_None);
        return false;
    }

    if (!GetProgramRingBufferForLiveTV(&pginfo, &buffer, channum))
    {
        ChangeState(kState_None);
        return false;
    }

    QString oldinputtype = m_tvChain->GetInputType(-1);

    pginfo->MarkAsInUse(true, kRecorderInUseID);
    pginfo->SaveAutoExpire(kLiveTVAutoExpire);
    if (!m_pseudoLiveTVRecording)
        pginfo->ApplyRecordRecGroupChange(RecordingInfo::kLiveTVRecGroup);
    m_tvChain->AppendNewProgram(pginfo, m_channel->GetChannelName(),
                                m_channel->GetInputName(), discont);

    if (set_rec && m_recorder)
    {
        m_recorder->SetNextRecording(pginfo, buffer);
        if (discont)
            m_recorder->CheckForRingBufferSwitch();
        delete pginfo;
        SetFlags(kFlagRingBufferReady, __FILE__, __LINE__);
    }
    else if (!set_rec)
    {
        // dummy recordings are finished before this
        // is called and other recordings must be finished..
        if (m_curRecording && oldinputtype != "DUMMY")
        {
            FinishedRecording(m_curRecording, nullptr);
            m_curRecording->MarkAsInUse(false, kRecorderInUseID);
            delete m_curRecording;
        }
        m_curRecording = pginfo;
        SetRingBuffer(buffer);
    }
    else
    {
        delete buffer;
    }

    return true;
}

RecordingInfo *TVRec::SwitchRecordingRingBuffer(const RecordingInfo &rcinfo)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "SwitchRecordingRingBuffer()");

    if (m_switchingBuffer)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "SwitchRecordingRingBuffer -> "
            "already switching.");
        return nullptr;
    }

    if (!m_recorder)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "SwitchRecordingRingBuffer -> "
            "invalid recorder.");
        return nullptr;
    }

    if (!m_curRecording)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "SwitchRecordingRingBuffer -> "
            "invalid recording.");
        return nullptr;
    }

    if (rcinfo.GetChanID() != m_curRecording->GetChanID())
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "SwitchRecordingRingBuffer -> "
            "Not the same channel.");
        return nullptr;
    }

    auto *ri = new RecordingInfo(rcinfo);
    RecordingProfile profile;

    QString pn = LoadProfile(nullptr, ri, profile);

    if (pn != m_recProfileName)
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            QString("SwitchRecordingRingBuffer() -> "
                    "cannot switch profile '%1' to '%2'")
            .arg(m_recProfileName, pn));
        return nullptr;
    }

    PreviewGeneratorQueue::GetPreviewImage(*m_curRecording, "");

    ri->MarkAsInUse(true, kRecorderInUseID);
    StartedRecording(ri);

    bool write = m_genOpt.m_inputType != "IMPORT";
    MythMediaBuffer *buffer = MythMediaBuffer::Create(ri->GetPathname(), write);
    if (!buffer || !buffer->IsOpen())
    {
        delete buffer;
        ri->SetRecordingStatus(RecStatus::Failed);
        FinishedRecording(ri, nullptr);
        ri->MarkAsInUse(false, kRecorderInUseID);
        delete ri;
        LOG(VB_RECORD, LOG_ERR, LOC + "SwitchRecordingRingBuffer() -> "
            "Failed to create new RB.");
        return nullptr;
    }

    m_recorder->SetNextRecording(ri, buffer);
    SetFlags(kFlagRingBufferReady, __FILE__, __LINE__);
    m_recordEndTime = GetRecordEndTime(ri);
    m_switchingBuffer = true;
    ri->SetRecordingStatus(RecStatus::Recording);
    LOG(VB_RECORD, LOG_INFO, LOC + "SwitchRecordingRingBuffer -> done");
    return ri;
}

TVRec* TVRec::GetTVRec(uint inputid)
{
    QMap<uint,TVRec*>::const_iterator it = s_inputs.constFind(inputid);
    if (it == s_inputs.constEnd())
        return nullptr;
    return *it;
}

void TVRec::EnableActiveScan(bool enable)
{
    LOG(VB_RECORD, LOG_INFO, LOC + QString("enable:%1").arg(enable));

    if (m_scanner != nullptr)
    {
        if (enable)
        {
            if (!HasFlags(kFlagEITScannerRunning)
                && m_eitScanStartTime > MythDate::current().addYears(9))
            {
                auto secs = m_eitCrawlIdleStart + eit_start_rand(m_inputId, m_eitTransportTimeout);
                m_eitScanStartTime = MythDate::current().addSecs(secs.count());
            }
        }
        else
        {
            m_eitScanStartTime = MythDate::current().addYears(10);
            if (HasFlags(kFlagEITScannerRunning))
            {
                m_scanner->StopActiveScan();
                ClearFlags(kFlagEITScannerRunning, __FILE__, __LINE__);
            }
        }
    }
}

QString TuningRequest::toString(void) const
{
    return QString("Program(%1) channel(%2) input(%3) flags(%4)")
        .arg(m_program == nullptr ? "NULL" : m_program->toString(),
             m_channel.isEmpty() ? "<empty>" : m_channel,
             m_input.isEmpty() ? "<empty>" : m_input,
             TVRec::FlagToString(m_flags));
}

#if CONFIG_DVB
#include "recorders/dvbchannel.h"
static void apply_broken_dvb_driver_crc_hack(ChannelBase *c, MPEGStreamData *s)
{
    // Some DVB devices munge the PMT and/or PAT so the CRC check fails.
    // We need to tell the stream data class to not check the CRC on
    // these devices. This can cause segfaults.
    auto * dvb = dynamic_cast<DVBChannel*>(c);
    if (dvb != nullptr)
        s->SetIgnoreCRC(dvb->HasCRCBug());
}
#else
static void apply_broken_dvb_driver_crc_hack(ChannelBase*, MPEGStreamData*) {}
#endif // CONFIG_DVB

/* vim: set expandtab tabstop=4 shiftwidth=4: */
