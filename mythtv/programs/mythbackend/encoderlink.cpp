// ANSI C++ headers
#include <chrono> // for milliseconds
#include <thread> // for sleep_for
#include <utility>

// QT headers
#include <QMap>                         // for QMap

// MythTV headers
#include "libmythbase/compat.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythlogging.h"      // for LOG
#include "libmythbase/programinfo.h"      // for ProgramInfo
#include "libmythbase/referencecounter.h"
#include "libmythprotoserver/requesthandler/fileserverutil.h"
#include "libmythtv/inputinfo.h"          // for InputInfo
#include "libmythtv/tv_rec.h"

// MythBackend
#include "encoderlink.h"
#include "playbacksock.h"

#define LOC QString("EncoderLink(%1): ").arg(m_inputid)
#define LOC_ERR QString("EncoderLink(%1) Error: ").arg(m_inputid)

/**
 * \class EncoderLink
 * \brief Provides an interface to both local and remote TVRec's for the mythbackend.
 *
 *  This class be instantiated for either a local or remote TVRec's.
 *  Many of the methods will work with either, but many only work for
 *  a local TVRec's and these are labeled appropriately in this document.
 *
 *  When used with a remote TVRec all calls go through a PlaybackSock
 *  instance.
 *
 *  This class is used primarily by the MainServer, Scheduler and
 *  AutoExpire classes.
 */

/** \fn EncoderLink::EncoderLink(int, PlaybackSock*, QString)
 *  \brief This is the EncoderLink constructor for non-local recorders.
 */
EncoderLink::EncoderLink(int inputid, PlaybackSock *lsock,
                         QString lhostname)
    : m_inputid(inputid), m_sock(lsock), m_hostname(std::move(lhostname)),
      m_sleepStatusTime(MythDate::current()),
      m_lastSleepTime(MythDate::current()),
      m_lastWakeTime(MythDate::current()),
      m_endRecordingTime(MythDate::current().addDays(-2)),
      m_startRecordingTime(m_endRecordingTime)
{
    HasSockAndIncrRef();
}

/** \fn EncoderLink::EncoderLink(int, TVRec *)
 *  \brief This is the EncoderLink constructor for local recorders.
 */
EncoderLink::EncoderLink(int inputid, TVRec *ltv)
    : m_inputid(inputid), m_tv(ltv), m_local(true),
      m_sleepStatusTime(MythDate::current()),
      m_lastSleepTime(MythDate::current()),
      m_lastWakeTime(MythDate::current()),
      m_endRecordingTime(MythDate::current().addDays(-2)),
      m_startRecordingTime(m_endRecordingTime)
{
}

/** \fn EncoderLink::~EncoderLink()
 *  \brief Destructor does nothing for non-local EncoderLink instances,
 *         but deletes the TVRec for local EncoderLink instances.
 */
EncoderLink::~EncoderLink(void)
{
    if (m_tv)
    {
        delete m_tv;
        m_tv = nullptr;
    }
    SetSocket(nullptr);
}

/**
 *  \brief Atomicly checks if sock is not null and increases its refcount.
 */
bool EncoderLink::HasSockAndIncrRef()
{
    QMutexLocker locker(&m_sockLock);
    if (m_sock)
    {
        m_sock->IncrRef();
        return true;
    }
    return false;
}

/**
 *  \brief Atomicly checks if sock is not null and decreases its refcount.
 */
bool EncoderLink::HasSockAndDecrRef()
{
    QMutexLocker locker(&m_sockLock);
    if (m_sock)
    {
        m_sock->DecrRef();
        return true;
    }
    return false;
}

/** \fn EncoderLink::SetSocket(PlaybackSock *lsock)
 *  \brief Used to set the socket for a non-local EncoderLink
 *
 *  Increases refcount on lsock, decreases refcount on old sock, if exists.
 */
void EncoderLink::SetSocket(PlaybackSock *lsock)
{
    if (lsock)
    {
        lsock->IncrRef();

        if (gCoreContext->GetSettingOnHost("SleepCommand", m_hostname).isEmpty())
            SetSleepStatus(sStatus_Undefined);
        else
            SetSleepStatus(sStatus_Awake);
    }
    else
    {
        if (IsFallingAsleep())
            SetSleepStatus(sStatus_Asleep);
        else
            SetSleepStatus(sStatus_Undefined);
    }

    HasSockAndDecrRef();
    m_sock = lsock;
}

/** \fn EncoderLink::SetSleepStatus(SleepStatus)
 *  \brief Sets the sleep status of a recorder
 */
void EncoderLink::SetSleepStatus(SleepStatus newStatus)
{
    m_sleepStatus     = newStatus;
    m_sleepStatusTime = MythDate::current();
}

/**
 *  \brief  Returns true if the recorder is busy, or will be within the
 *          next time_buffer seconds.
 *  \sa IsBusyRecording(void), TVRec::IsBusy(InputInfo*)
 */
bool EncoderLink::IsBusy(InputInfo *busy_input, std::chrono::seconds time_buffer)
{
    if (m_local)
        return m_tv->IsBusy(busy_input, time_buffer);

    if (HasSockAndIncrRef())
    {
        ReferenceLocker rlocker(m_sock);
        return m_sock->IsBusy(m_inputid, busy_input, time_buffer);
    }

    return false;
}

/** \fn EncoderLink::IsBusyRecording(void)
 *  \brief Returns true if the TVRec state is in a recording state.
 *
 *   Contrast with IsBusy() which returns true if a recording is pending
 *   and is generally the safer call to make.
 *
 *  \sa IsBusy()
 */
bool EncoderLink::IsBusyRecording(void)
{
    bool retval = false;

    TVState state = GetState();

    if (state == kState_RecordingOnly || state == kState_WatchingRecording ||
        state == kState_WatchingLiveTV)
    {
        retval = true;
    }

    return retval;
}

/** \fn EncoderLink::GetState()
 *  \brief Returns the TVState of the recorder.
 *  \sa TVRec::GetState(), \ref recorder_subsystem
 */
TVState EncoderLink::GetState(void)
{
    TVState retval = kState_Error;

    if (!IsConnected())
        return retval;

    if (m_local)
        retval = m_tv->GetState();
    else if (HasSockAndIncrRef())
    {
        ReferenceLocker rlocker(m_sock);
        retval = (TVState)m_sock->GetEncoderState(m_inputid);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Broken for input: %1")
            .arg(m_inputid));
    }

    return retval;
}

/** \fn EncoderLink::GetFlags(void) const
 *  \brief Returns the flag state of the recorder.
 *  \sa TVRec::GetFlags(void) const, \ref recorder_subsystem
 */
uint EncoderLink::GetFlags(void)
{
    uint retval = 0;

    if (!IsConnected())
        return retval;

    if (m_local)
        retval = m_tv->GetFlags();
    else if (HasSockAndIncrRef())
    {
        ReferenceLocker rlocker(m_sock);
        retval = m_sock->GetEncoderState(m_inputid);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "GetFlags failed");
    }

    return retval;
}

/** \fn EncoderLink::IsRecording(const ProgramInfo *rec)
 *  \brief Returns true if rec is scheduled for recording.
 *  \param rec Recording to check.
 *  \sa MatchesRecording(const ProgramInfo*)
 */
bool EncoderLink::IsRecording(const ProgramInfo *rec)
{
    return (rec->GetChanID() == m_chanid &&
            rec->GetRecordingStartTime() == m_startRecordingTime);
}

/** \fn EncoderLink::MatchesRecording(const ProgramInfo *rec)
 *  \brief Returns true if rec is actually being recorded by TVRec.
 *
 *   This waits for TVRec to enter a state other than kState_ChangingState
 *   Then it checks TVRec::GetRecording() against rec.
 *  \param rec Recording to check against TVRec::GetRecording().
 *  \sa IsRecording(const ProgramInfo*)
 */
bool EncoderLink::MatchesRecording(const ProgramInfo *rec)
{
    bool retval = false;
    ProgramInfo *tvrec = nullptr;

    if (m_local)
    {
        while (kState_ChangingState == GetState())
            std::this_thread::sleep_for(100us);

        if (IsBusyRecording())
            tvrec = m_tv->GetRecording();

        if (tvrec)
        {
            retval = tvrec->IsSameRecording(*rec);
            delete tvrec;
        }
    }
    else
    {
        if (HasSockAndIncrRef())
        {
            ReferenceLocker rlocker(m_sock);
            retval = m_sock->EncoderIsRecording(m_inputid, rec);
        }
    }

    return retval;
}

/**
 *  \brief Tells TVRec there is a pending recording "rec" in "secsleft" seconds.
 *
 *  \param rec      Recording to make.
 *  \param secsleft Seconds to wait before starting recording.
 *  \param hasLater If true, a later non-conflicting showing is available.
 *  \sa StartRecording(const ProgramInfo*), CancelNextRecording(bool)
 */
void EncoderLink::RecordPending(const ProgramInfo *rec,
                                std::chrono::seconds secsleft,
                                bool hasLater)
{
    if (m_local)
        m_tv->RecordPending(rec, secsleft, hasLater);
    else if (HasSockAndIncrRef())
    {
        ReferenceLocker rlocker(m_sock);
        m_sock->RecordPending(m_inputid, rec, secsleft, hasLater);
    }
}

/** \fn EncoderLink::WouldConflict(const ProgramInfo*)
 *  \brief Checks a recording against any recording current or pending
 *         recordings on the recorder represented by this EncoderLink.
 *  \param rec Recording to check against current/pending recording.
 */
bool EncoderLink::WouldConflict(const ProgramInfo *rec)
{
    if (!IsConnected())
        return true;

    if (rec->GetRecordingStartTime() < m_endRecordingTime)
        return true;

    return false;
}

/// Checks if program is stored locally
bool EncoderLink::CheckFile(ProgramInfo *pginfo)
{
    if (HasSockAndIncrRef())
    {
        ReferenceLocker rlocker(m_sock);
        return m_sock->CheckFile(pginfo);
    }
    pginfo->SetPathname(GetPlaybackURL(pginfo));
    return pginfo->IsLocal();
}

/** \fn EncoderLink::GetMaxBitrate()
 *  \brief Returns maximum bits per second this recorder might output.
 *
 *  \sa TVRec::GetFreeSpace(long long), RemoteEncoder::GetFreeSpace(long long)
 *   May be a local or remote query.
 */
long long EncoderLink::GetMaxBitrate()
{
    if (m_local)
        return m_tv->GetMaxBitrate();
    if (HasSockAndIncrRef())
    {
        ReferenceLocker rlocker(m_sock);
        return m_sock->GetMaxBitrate(m_inputid);
    }

    return -1;
}

/**
 *  \brief Sets the signal monitoring rate.
 *
 *   May be a local or remote query.
 *
 *  \sa TVRec::SetSignalMonitoringRate(milliseconds,int),
 *      RemoteEncoder::SetSignalMonitoringRate(int,int)
 *  \param rate           Milliseconds between each signal check,
 *                        0 to disable, -1 to preserve old value.
 *  \param notifyFrontend If 1 SIGNAL messages are sent to the frontend,
 *                        if 0 SIGNAL messages will not be sent, and if
 *                        -1 the old value is preserved.
 *  \return Old rate if it succeeds, -1 if it fails.
 */
std::chrono::milliseconds EncoderLink::SetSignalMonitoringRate(std::chrono::milliseconds rate, int notifyFrontend)
{
    if (m_local)
        return m_tv->SetSignalMonitoringRate(rate, notifyFrontend);
    if (HasSockAndIncrRef())
    {
        ReferenceLocker rlocker(m_sock);
        return m_sock->SetSignalMonitoringRate(m_inputid, rate,
                                             notifyFrontend);
    }
    return -1ms;
}

/** \brief Tell a slave to go to sleep
 */
bool EncoderLink::GoToSleep(void)
{
    if (IsLocal() || !HasSockAndIncrRef())
        return false;
    ReferenceLocker rlocker(m_sock);

    m_lastSleepTime = MythDate::current();

    return m_sock->GoToSleep();
}

/** \brief Lock the tuner for exclusive use.
 *  \return -2 if tuner is already locked, GetInputID() if you get the lock.
 * \sa FreeTuner(), IsTunerLocked()
 */
int EncoderLink::LockTuner()
{
    if (m_locked)
        return -2;

    m_locked = true;
    return m_inputid;
}

/** \fn EncoderLink::StartRecording(ProgramInfo*)
 *  \brief Tells TVRec to Start recording the program "rec" as soon as possible.
 *
 *  \return a RecStatus::Type indicating the state of the recording.
 *  \sa RecordPending(const ProgramInfo*, int, bool), StopRecording()
 */
RecStatus::Type EncoderLink::StartRecording(ProgramInfo *rec)
{
    RecStatus::Type retval = RecStatus::Aborted;

    m_endRecordingTime = rec->GetRecordingEndTime();
    m_startRecordingTime = rec->GetRecordingStartTime();
    m_chanid = rec->GetChanID();

    if (m_local)
        retval = m_tv->StartRecording(rec);
    else if (HasSockAndIncrRef())
    {
        ReferenceLocker rlocker(m_sock);
        retval = m_sock->StartRecording(m_inputid, rec);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Wanted to start recording on recorder %1,\n\t\t\t"
                    "but the backend is not there anymore\n")
                .arg(m_inputid));
    }

    if (retval != RecStatus::Recording &&
        retval != RecStatus::Tuning &&
        retval != RecStatus::Failing)
    {
        m_endRecordingTime = MythDate::current().addDays(-2);
        m_startRecordingTime = m_endRecordingTime;
        m_chanid = 0;
    }

    return retval;
}

RecStatus::Type EncoderLink::GetRecordingStatus(void)
{
    RecStatus::Type retval = RecStatus::Aborted;

    if (m_local)
        retval = m_tv->GetRecordingStatus();
    else if (HasSockAndIncrRef())
    {
        ReferenceLocker rlocker(m_sock);
        retval = m_sock->GetRecordingStatus(m_inputid);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Wanted to get status on recorder %1,\n\t\t\t"
                    "but the backend is not there anymore\n")
                .arg(m_inputid));
    }

    if (retval != RecStatus::Recording &&
        retval != RecStatus::Tuning &&
        retval != RecStatus::Failing)
    {
        m_endRecordingTime = MythDate::current().addDays(-2);
        m_startRecordingTime = m_endRecordingTime;
        m_chanid = 0;
    }

    return retval;
}

/** \fn EncoderLink::GetRecording()
 *  \brief Returns TVRec's current recording.
 *
 *   Caller is responsible for deleting the ProgramInfo when done with it.
 *  \return Returns TVRec's current recording if it succeeds, nullptr otherwise.
 */
ProgramInfo *EncoderLink::GetRecording(void)
{
    ProgramInfo *info = nullptr;

    if (m_local)
        info = m_tv->GetRecording();
    else if (HasSockAndIncrRef())
    {
        ReferenceLocker rlocker(m_sock);
        info = m_sock->GetRecording(m_inputid);
    }

    return info;
}

/** \fn EncoderLink::StopRecording(bool killFile)
 *  \brief Tells TVRec to stop recording immediately.
 *         <b>This only works on local recorders.</b>
 *  \sa StartRecording(const ProgramInfo *rec), FinishRecording()
 */
void EncoderLink::StopRecording(bool killFile)
{
    m_endRecordingTime = MythDate::current().addDays(-2);
    m_startRecordingTime = m_endRecordingTime;
    m_chanid = 0;

    if (m_local)
    {
        m_tv->StopRecording(killFile);
        return;
    }
}

/** \fn EncoderLink::FinishRecording()
 *  \brief Tells TVRec to stop recording, but only after "overrecord" seconds.
 *         <b>This only works on local recorders.</b>
 *  \sa StopRecording()
 */
void EncoderLink::FinishRecording(void)
{
    if (m_local)
    {
        m_tv->FinishRecording();
        return;
    }
    m_endRecordingTime = MythDate::current().addDays(-2);
}

/** \fn EncoderLink::IsReallyRecording()
 *  \brief Checks if the RecorderBase held by TVRec is actually recording.
 *         <b>This only works on local recorders.</b>
 *  \return true if actually recording, false otherwise.
 */
bool EncoderLink::IsReallyRecording(void)
{
    if (m_local)
        return m_tv->IsReallyRecording();

    LOG(VB_GENERAL, LOG_ERR, "Should be local only query: IsReallyRecording");
    return false;
}

/** \fn EncoderLink::GetFramerate()
 *  \brief Returns the recording frame rate from TVRec.
 *         <b>This only works on local recorders.</b>
 *  \sa RemoteEncoder::GetFrameRate(), TVRec::GetFramerate(void),
 *      RecorderBase::GetFrameRate()
 *  \return Frames per second if query succeeds -1 otherwise.
 */
float EncoderLink::GetFramerate(void)
{
    if (m_local)
        return m_tv->GetFramerate();

    LOG(VB_GENERAL, LOG_ERR, "Should be local only query: GetFramerate");
    return -1;
}

/** \fn EncoderLink::GetFramesWritten()
 *  \brief Returns number of frames written to disk by TVRec's RecorderBase
 *         instance. <b>This only works on local recorders.</b>
 *
 *  \sa TVRec::GetFramesWritten(), RemoteEncoder::GetFramesWritten()
 *  \return Number of frames if query succeeds, -1 otherwise.
 */
long long EncoderLink::GetFramesWritten(void)
{
    if (m_local)
        return m_tv->GetFramesWritten();

    LOG(VB_GENERAL, LOG_ERR, "Should be local only query: GetFramesWritten");
    return -1;
}

/** \fn EncoderLink::GetFilePosition()
 *  \brief Returns total number of bytes written by TVRec's RingBuffer.
 *         <b>This only works on local recorders.</b>
 *  \sa TVRec::GetFilePosition(), RemoteEncoder::GetFilePosition()
 *  \return Bytes written if query succeeds, -1 otherwise.
 */
long long EncoderLink::GetFilePosition(void)
{
    if (m_local)
        return m_tv->GetFilePosition();

    LOG(VB_GENERAL, LOG_ERR, "Should be local only query: GetFilePosition");
    return -1;
}

/** \brief Returns byte position in RingBuffer of a keyframe.
 *         <b>This only works on local recorders.</b>
 *  \sa TVRec::GetKeyframePosition(uint64_t),
 *      RemoteEncoder::GetKeyframePosition(uint64_t)
 *  \return Byte position of keyframe if query succeeds, -1 otherwise.
 */
int64_t EncoderLink::GetKeyframePosition(uint64_t desired)
{
    if (m_local)
        return m_tv->GetKeyframePosition(desired);

    LOG(VB_GENERAL, LOG_ERR, "Should be local only query: GetKeyframePosition");
    return -1;
}

bool EncoderLink::GetKeyframePositions(
    int64_t start, int64_t end, frm_pos_map_t &map)
{
    if (!m_local)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "Should be local only query: GetKeyframePositions");
        return false;
    }

    return m_tv->GetKeyframePositions(start, end, map);
}

bool EncoderLink::GetKeyframeDurations(
    int64_t start, int64_t end, frm_pos_map_t &map)
{
    if (!m_local)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "Should be local only query: GetKeyframeDurations");
        return false;
    }

    return m_tv->GetKeyframeDurations(start, end, map);
}

/** \fn EncoderLink::FrontendReady()
 *  \brief Tells TVRec that the frontend is ready for data.
 *         <b>This only works on local recorders.</b>
 *  \sa TVRec::, RemoteEncoder::
 */
void EncoderLink::FrontendReady(void)
{
    if (m_local)
        m_tv->FrontendReady();
    else
        LOG(VB_GENERAL, LOG_ERR, "Should be local only query: FrontendReady");
}

/** \fn EncoderLink::CancelNextRecording(bool)
 *  \brief Tells TVRec to cancel the next recording.
 *
 *   This is used when the user is watching "Live TV" and does not
 *   want to allow the recorder to be taken for a pending recording.
 *
 *  \sa RecordPending(const ProgramInfo*, int, bool)
 */
void EncoderLink::CancelNextRecording(bool cancel)
{
    if (m_local)
        m_tv->CancelNextRecording(cancel);
    else if (HasSockAndIncrRef())
    {
        ReferenceLocker rlocker(m_sock);
        m_sock->CancelNextRecording(m_inputid, cancel);
    }
}

/** \fn EncoderLink::SpawnLiveTV(LiveTVChain*, bool, QString)
 *  \brief Tells TVRec to Spawn a "Live TV" recorder.
 *         <b>This only works on local recorders.</b>
 *
 *  \param chain The LiveTV chain to use
 *  \param startchan The channel the LiveTV should start with
 *  \param pip Tells TVRec's RingBuffer that this is for a Picture in Picture
 *  display.
 *  \sa TVRec::SpawnLiveTV(LiveTVChain*,bool,QString),
 *      RemoteEncoder::SpawnLiveTV(QString,bool,QString)
 */
void EncoderLink::SpawnLiveTV(LiveTVChain *chain, bool pip, QString startchan)
{
    if (m_local)
        m_tv->SpawnLiveTV(chain, pip, std::move(startchan));
    else
        LOG(VB_GENERAL, LOG_ERR, "Should be local only query: SpawnLiveTV");
}

/** \fn EncoderLink::GetChainID()
 *  \brief Get the LiveTV chain id that's in use.
 */
QString EncoderLink::GetChainID(void)
{
    if (m_local)
        return m_tv->GetChainID();

    LOG(VB_GENERAL, LOG_ERR, "Should be local only query: SpawnLiveTV");
    return "";
}

/** \fn EncoderLink::StopLiveTV()
 *  \brief Tells TVRec to stop a "Live TV" recorder.
 *         <b>This only works on local recorders.</b>
 *  \sa TVRec::StopLiveTV(), RemoteEncoder::StopLiveTV()
 */
void EncoderLink::StopLiveTV(void)
{
    if (m_local)
        m_tv->StopLiveTV();
    else
        LOG(VB_GENERAL, LOG_ERR, "Should be local only query: StopLiveTV");
}

/** \fn EncoderLink::PauseRecorder()
 *  \brief Tells TVRec to pause a recorder, used for channel and input changes.
 *         <b>This only works on local recorders.</b>
 *  \sa TVRec::PauseRecorder(), RemoteEncoder::PauseRecorder(),
 *      RecorderBase::Pause()
 */
void EncoderLink::PauseRecorder(void)
{
    if (m_local)
        m_tv->PauseRecorder();
    else
        LOG(VB_GENERAL, LOG_ERR, "Should be local only query: PauseRecorder");
}

/** \fn EncoderLink::SetLiveRecording(int recording)
 *  \brief Tells TVRec to keep a LiveTV recording if 'recording' is 1.
 *         and to not keep a LiveTV recording if 'recording; is 0.
 *         <b>This only works on local recorders.</b>
 */
void EncoderLink::SetLiveRecording(int recording)
{
    if (m_local)
        m_tv->SetLiveRecording(recording);
    else
        LOG(VB_GENERAL, LOG_ERR,
            "Should be local only query: SetLiveRecording");
}

/**
 *  \brief Tells TVRec where to put the next LiveTV recording.
 */
void EncoderLink::SetNextLiveTVDir(const QString& dir)
{
    if (m_local)
        m_tv->SetNextLiveTVDir(dir);
    else if (HasSockAndIncrRef())
    {
        ReferenceLocker rlocker(m_sock);
        m_sock->SetNextLiveTVDir(m_inputid, dir);
    }
}

/** \fn EncoderLink::GetInput(void) const
 *  \brief Returns TVRec's recorders current input.
 *         <b>This only works on local recorders.</b>
 *
 *  \sa TVRec::GetInput(void) const
 */
QString EncoderLink::GetInput(void) const
{
    if (m_local)
        return m_tv->GetInput();

    LOG(VB_GENERAL, LOG_ERR, "Should be local only query: GetInput");
    return {};
}

/** \fn EncoderLink::SetInput(QString)
 *  \brief Tells TVRec's recorder to change to the specified input.
 *         <b>This only works on local recorders.</b>
 *
 *   You must call PauseRecorder(void) before calling this.
 *
 *  \param input Input to switch to, or "SwitchToNextInput".
 *  \return input we have switched to
 *  \sa TVRec::SetInput(QString)
 */
QString EncoderLink::SetInput(QString input)
{
    if (m_local)
        return m_tv->SetInput(std::move(input));

    LOG(VB_GENERAL, LOG_ERR, "Should be local only query: SetInput");
    return {};
}

/**
 *  \brief Toggles whether the current channel should be on our favorites list.
 *         <b>This only works on local recorders.</b>
 *  \return -1 if query does not succeed, otherwise.
 */
void EncoderLink::ToggleChannelFavorite(const QString& changroup)
{
    if (m_local)
        m_tv->ToggleChannelFavorite(changroup);
    else
        LOG(VB_GENERAL, LOG_ERR,
            "Should be local only query: ToggleChannelFavorite");
}

/** \brief Changes to the next or previous channel.
 *         <b>This only works on local recorders.</b>
 *
 *   You must call PauseRecorder() before calling this.
 *  \param channeldirection channel change direction \sa BrowseDirections.
 */
void EncoderLink::ChangeChannel(ChannelChangeDirection channeldirection)
{
    if (m_local)
        m_tv->ChangeChannel(channeldirection);
    else
        LOG(VB_GENERAL, LOG_ERR, "Should be local only query: ChangeChannel");
}

/** \fn EncoderLink::SetChannel(const QString&)
 *  \brief Changes to a named channel on the current tuner.
 *         <b>This only works on local recorders.</b>
 *
 *   You must call PauseRecorder() before calling this.
 *  \param name Name of channel to change to.
 */
void EncoderLink::SetChannel(const QString &name)
{
    if (m_local)
        m_tv->SetChannel(name);
    else
        LOG(VB_GENERAL, LOG_ERR, "Should be local only query: SetChannel");
}

/** \fn EncoderLink::GetPictureAttribute(PictureAttribute)
 *  \brief Changes brightness/contrast/colour/hue of a recording.
 *         <b>This only works on local recorders.</b>
 *
 *  Note: In practice this only works with frame grabbing recorders.
 *
 *  \return current value if it succeeds, -1 otherwise.
 */
int EncoderLink::GetPictureAttribute(PictureAttribute attr)
{
    if (!m_local)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "Should be local only query: GetPictureAttribute");
        return -1;
    }

    return m_tv->GetPictureAttribute(attr);
}

/** \fn EncoderLink::ChangePictureAttribute(PictureAdjustType,PictureAttribute,bool)
 *  \brief Changes brightness/contrast/colour/hue of a recording.
 *         <b>This only works on local recorders.</b>
 *
 *  Note: In practice this only works with frame grabbing recorders.
 *
 *  \return current value if it succeeds, -1 otherwise.
 */
int EncoderLink::ChangePictureAttribute(PictureAdjustType type,
                                        PictureAttribute  attr,
                                        bool              direction)
{
    if (!m_local)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "Should be local only query: ChangePictureAttribute");
        return -1;
    }

    return m_tv->ChangePictureAttribute(type, attr, direction);
}

/** \fn EncoderLink::CheckChannel(const QString&)
 *  \brief Checks if named channel exists on current tuner.
 *         <b>This only works on local recorders.</b>
 *  \param name Channel to verify against current tuner.
 *  \return true if it succeeds, false otherwise.
 *  \sa TVRec::CheckChannel(QString),
 *      RemoteEncoder::CheckChannel(QString),
 *      ShouldSwitchToAnotherInput(const QString&)
 */
bool EncoderLink::CheckChannel(const QString &name)
{
    if (m_local)
        return m_tv->CheckChannel(name);

    LOG(VB_GENERAL, LOG_ERR, "Should be local only query: CheckChannel");
    return false;
}

/** \fn EncoderLink::ShouldSwitchToAnotherInput(const QString&)
 *  \brief Checks if named channel exists on current tuner, or
 *         another tuner.
 *         <b>This only works on local recorders.</b>
 *  \param channelid channel to verify against tuners.
 *  \return true if the channel on another tuner and not current tuner,
 *          false otherwise.
 *  \sa CheckChannel(const QString&)
 */
bool EncoderLink::ShouldSwitchToAnotherInput(const QString &channelid)
{
    if (m_local)
        return m_tv->ShouldSwitchToAnotherInput(channelid);

    LOG(VB_GENERAL, LOG_ERR,
        "Should be local only query: ShouldSwitchToAnotherInput");
    return false;
}

/** \fn EncoderLink::CheckChannelPrefix(const QString&,uint&,bool&,QString&)
 *  \brief Checks a prefix against the channels in the DB.
 *         <b>This only works on local recorders.</b>
 *
 *  \sa TVRec::CheckChannelPrefix(const QString&,uint&,bool&,QString&)
 *      for details.
 */
bool EncoderLink::CheckChannelPrefix(
    const QString &prefix,
    uint          &complete_valid_channel_on_rec,
    bool          &is_extra_char_useful,
    QString       &needed_spacer)
{
    if (m_local)
    {
        return m_tv->CheckChannelPrefix(
            prefix, complete_valid_channel_on_rec,
            is_extra_char_useful, needed_spacer);
    }

    LOG(VB_GENERAL, LOG_ERR, "Should be local only query: CheckChannelPrefix");
    complete_valid_channel_on_rec    = 0;
    is_extra_char_useful             = false;
    needed_spacer                    = "";
    return false;
}

/** \brief Returns information about the program that would be
 *         seen if we changed the channel using ChangeChannel(int)
 *         with "direction".
 *         <b>This only works on local recorders.</b>
 */
void EncoderLink::GetNextProgram(BrowseDirection direction,
                                 QString &title,       QString &subtitle,
                                 QString &desc,        QString &category,
                                 QString &starttime,   QString &endtime,
                                 QString &callsign,    QString &iconpath,
                                 QString &channelname, uint    &_chanid,
                                 QString &seriesid,    QString &programid)
{
    if (m_local)
    {
        m_tv->GetNextProgram(direction,
                           title, subtitle, desc, category, starttime,
                           endtime, callsign, iconpath, channelname,
                           _chanid, seriesid, programid);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, "Should be local only query: GetNextProgram");
    }
}

bool EncoderLink::GetChannelInfo(uint &chanid, uint &sourceid,
                                 QString &callsign, QString &channum,
                                 QString &channame, QString &xmltv) const
{
    if (!m_local)
    {
        LOG(VB_GENERAL, LOG_ERR, "Should be local only query: GetChannelInfo");
        return false;
    }

    return m_tv->GetChannelInfo(chanid, sourceid,
                              callsign, channum, channame, xmltv);
}

bool EncoderLink::SetChannelInfo(uint chanid, uint sourceid,
                                 const QString& oldchannum,
                                 const QString& callsign, const QString& channum,
                                 const QString& channame, const QString& xmltv)
{
    if (!m_local)
    {
        LOG(VB_GENERAL, LOG_ERR, "Should be local only query: SetChannelInfo");
        return false;
    }

    return m_tv->SetChannelInfo(chanid, sourceid, oldchannum,
                                callsign, channum,
                                channame, xmltv);
}

bool EncoderLink::AddChildInput(uint childid)
{
    if (m_local)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Called on local recorder");
        return false;
    }

    bool retval = m_sock->AddChildInput(childid);
    return retval;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
