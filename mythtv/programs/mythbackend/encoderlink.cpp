// C++ headers
#include <iostream>
using namespace std;

// C headers
#include <unistd.h>

// MythTV headers
#include "mythcontext.h"
#include "encoderlink.h"
#include "playbacksock.h"
#include "tv.h"
#include "programinfo.h"
#include "util.h"

/**
 * \class EncoderLink
 * \brief Provides an interface to both local and remote TVRec's for the mythbackend.
 *
 *  This class be instanciated for either a local or remote TVRec's.
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
EncoderLink::EncoderLink(int capturecardnum, PlaybackSock *lsock, 
                         QString lhostname)
    : m_capturecardnum(capturecardnum), sock(lsock), hostname(lhostname),
      freeDiskSpaceKB(-1), tv(NULL), local(false), locked(false), 
      chanid(""), recordfileprefix(QString::null)
{
    endRecordingTime = QDateTime::currentDateTime().addDays(-2);
    startRecordingTime = endRecordingTime;

    if (sock)
        sock->UpRef();
}

/** \fn EncoderLink::EncoderLink(int, TVRec *)
 *  \brief This is the EncoderLink constructor for local recorders.
 */
EncoderLink::EncoderLink(int capturecardnum, TVRec *ltv)
    : m_capturecardnum(capturecardnum), sock(NULL), hostname(QString::null),
      freeDiskSpaceKB(-1), tv(ltv), local(true), locked(false), 
      chanid(""), recordfileprefix(QString::null)
{
    endRecordingTime = QDateTime::currentDateTime().addDays(-2);
    startRecordingTime = endRecordingTime;
    recordfileprefix = gContext->GetSetting("RecordFilePrefix");
}

/** \fn EncoderLink::~EncoderLink()
 *  \brief Destructor does nothing for non-local EncoderLink instances,
 *         but deletes the TVRec for local EncoderLink instances.
 */
EncoderLink::~EncoderLink(void)
{
    if (tv)
        delete tv;
}

/** \fn EncoderLink::SetSocket(PlaybackSock *lsock)
 *  \brief Used to set the socket for a non-local EncoderLink
 *
 *  Increases refcount on lsock, decreases refcount on old sock, if exists.
 */
void EncoderLink::SetSocket(PlaybackSock *lsock)
{
    if (lsock)
        lsock->UpRef();

    if (sock)
        sock->DownRef();
    sock = lsock;
}

/** \fn EncoderLink::IsBusy()
 *  \brief  Returns true if the recorder is busy, or will be within the 
 *          next 5 seconds.
 *  \sa IsBusyRecording(), TVRec::IsBusy()
 */
bool EncoderLink::IsBusy(void)
{
    if (local)
        return tv->IsBusy();

    if (sock)
        return sock->IsBusy(m_capturecardnum);

    return false;
}

/** \fn EncoderLink::IsBusyRecording()
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

    if (local)
        retval = tv->GetState();
    else if (sock)
        retval = (TVState)sock->GetEncoderState(m_capturecardnum);
    else
        cerr << "Broken for card: " << m_capturecardnum << endl;

    return retval;
}

/** \fn EncoderLink::IsRecording(const ProgramInfo *rec)
 *  \brief Returns true if rec is scheduled for recording.
 *  \param rec Recording to check.
 *  \sa MatchesRecording(const ProgramInfo*)
 */
bool EncoderLink::IsRecording(const ProgramInfo *rec)
{
    bool retval = false;

    if (rec->chanid == chanid && rec->recstartts == startRecordingTime)
        retval = true;

    return retval;
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
    ProgramInfo *tvrec = NULL;

    if (local)
    {
        while (kState_ChangingState == GetState())
            usleep(100);

        if (IsBusyRecording())
            tvrec = tv->GetRecording();

        if (tvrec)
        {
            if (tvrec->chanid == rec->chanid && 
                tvrec->recstartts == rec->recstartts &&
                tvrec->recendts == rec->recendts)
            {
                retval = true;
            }

            delete tvrec;
        }
    }
    else
    {
        if (sock)
            retval = sock->EncoderIsRecording(m_capturecardnum, rec);
    }

    return retval;
}

/** \fn RecordPending(const ProgramInfo*, int)
 *  \brief Tells TVRec there is a pending recording "rec" in "secsleft" seconds.
 *  \param rec      Recording to make.
 *  \param secsleft Seconds to wait before starting recording.
 *  \sa StartRecording(const ProgramInfo*), CancelNextRecording()
 */
void EncoderLink::RecordPending(const ProgramInfo *rec, int secsleft)
{
    if (local)
        tv->RecordPending(rec, secsleft);
    else if (sock)
        sock->RecordPending(m_capturecardnum, rec, secsleft);
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

    if (rec->recstartts < endRecordingTime)
        return true;

    return false;
}

/** \fn EncoderLink::GetFreeDiskSpace(long long&,long long&,bool)
 *  \brief Returns total used and free disk space in Kilobytes
 *         and if use_cache is false updates the cached value.
 *
 *   May be a local or remote query.
 *
 *  \param use_cache if true, then a cached value is used.
 */
long long EncoderLink::GetFreeDiskSpace(long long &totalKB, long long &usedKB,
                                        bool use_cache)
{
    if (!use_cache)
    {
        if (local)
            freeDiskSpaceKB = getDiskSpace(recordfileprefix, totalKB, usedKB);
        else if (sock)
        {
            sock->GetFreeDiskSpace(totalKB, usedKB);
            freeDiskSpaceKB = totalKB - usedKB;
            if ((-1 == totalKB) || (-1 == usedKB))
                freeDiskSpaceKB = -1;
        }
    }
    return freeDiskSpaceKB;
}

/** \fn EncoderLink::GetFreeDiskSpace(bool use_cache=false)
 *  \brief Returns total used and free disk space in Kilobytes
 *         and if use_cache is false updates the cached value.
 *
 *   May be a local or remote query.
 *
 *  \param use_cache if true, then a cached value is used.
 */
long long EncoderLink::GetFreeDiskSpace(bool use_cache)
{
    long long totalKB, usedKB;
    return GetFreeDiskSpace(totalKB, usedKB, use_cache);
}

/** \fn EncoderLink::GetMaxBitrate()
 *  \brief Returns maximum bits per second this recorder might output.
 * 
 *  \sa TVRec::GetFreeSpace(long long), RemoteEncoder::GetFreeSpace(long long)
 *   May be a local or remote query.
 */
long long EncoderLink::GetMaxBitrate()
{
    if (local)
        return tv->GetMaxBitrate();
    else if (sock)
        return sock->GetMaxBitrate(m_capturecardnum);

    return -1;
}

/** \fn EncoderLink::SetSignalMonitoringRate(int,int)
 *  \brief Sets the signal monitoring rate.
 *
 *   May be a local or remote query.
 *
 *  \sa TVRec::SetSignalMonitoringRate(int,int),
 *      RemoteEncoder::SetSignalMonitoringRate(int,int)
 *  \param rate           Milliseconds between each signal check,
 *                        0 to disable, -1 to preserve old value.
 *  \param notifyFrontend If 1 SIGNAL messages are sent to the frontend,
 *                        if 0 SIGNAL messages will not be sent, and if
 *                        -1 the old value is preserved.
 *  \return Old rate if it succeeds, -1 if it fails.
 */
int EncoderLink::SetSignalMonitoringRate(int rate, int notifyFrontend)
{
    if (local)
        return tv->SetSignalMonitoringRate(rate, notifyFrontend);
    else if (sock)
        return sock->SetSignalMonitoringRate(m_capturecardnum, rate,
                                             notifyFrontend);
    return -1;
}

/** \brief Lock the tuner for exclusive use.
 *  \return -2 if tuner is already locked, GetCardID() if you get the lock.
 * \sa FreeTuner(), IsTunerLocked()
 */
int EncoderLink::LockTuner()
{
    if (locked)
        return -2;

    locked = true;  
    return m_capturecardnum;
}

/** \fn  EncoderLink::HasEnoughFreeSpace(const ProgramInfo*, bool)
 *  \brief Returns true if there is enough free space for entire recording.
 *
 *   May be a local or remote query.
 *
 *   If AutoExpire is enabled, and the encoder is connected, this always
 *   returns true. If it is not enabled this function estimates the size
 *   of the recording, and only records if there is enough space left on
 *   the file system for both the recording and the "Extra Space" the 
 *   user desires on the file system.
 *
 *
 *   Note: If AutoExpire is not enabled, the calculation is only valid
 *         when one recording per file system is in progress at any one
 *         time.
 *
 *  \param rec Recording we wish to make, used to determine length of recording.
 *  \param try_to_use_cache If true we try the the free space calculation with
 *                          the cached disk free space, and only query the
 *                          backend for the true free space value, if the
 *                          cached value tells us we don't have enough free
 *                          space to fit the recording.
 */
bool EncoderLink::HasEnoughFreeSpace(const ProgramInfo *rec, bool try_to_use_cache)
{
    if (!IsConnected())
        return false;

    // if auto expire is enabled, the space will be freed just in time...
    if (gContext->GetNumSetting("AutoExpireMethod", 1))
        return true;

    // if we can't determine the free space, assume it's just an fstat problem.
    long long freeSpaceKB = GetFreeDiskSpace(try_to_use_cache);
    if (freeSpaceKB<0)
        return true;
    // shrink by "Extra Space" desired for other non-MythTV activities.
    freeSpaceKB -= 1024 * 1024 *
        gContext->GetNumSetting("AutoExpireExtraSpace", 0);
    // if already below threshold we can't record anything.
    if (freeSpaceKB<0)
        return false;

    // if we can't determine the bitrate assume it's ATSC 1080i.
    long long maxBitrate = GetMaxBitrate();
    if (maxBitrate<=0)
        maxBitrate = 19500000LL;

    // determine maximum program size
    size_t bitRateKBperMin = (((size_t)maxBitrate)*((size_t)15))>>11;
    size_t progLengthMin = (rec->startts.secsTo(rec->endts)+59)/60;
    size_t programSizeKB = bitRateKBperMin * progLengthMin;
    programSizeKB += programSizeKB>>2; // + 25%
    VERBOSE(VB_IMPORTANT,
            QString("Estimated program length: %1 minutes, "
                    "size: %2 MB, free space: %3 MB")
            .arg(progLengthMin).arg((long)(programSizeKB/1024))
            .arg((long)(freeSpaceKB/1024)));
    
    // if program is smaller than available space, then ok
    // if not ok, and we were using cache, try again with measured values
    bool ok = ((long long)programSizeKB < freeSpaceKB);
    if (!ok && try_to_use_cache)
        ok = HasEnoughFreeSpace(rec, false);
    return ok;
}

/** \fn EncoderLink::StartRecording(const ProgramInfo*)
 *  \brief Tells TVRec to Start recording the program "rec" as soon as possible.
 *
 *  \return +1 if the recording started successfully,
 *          -1 if TVRec is busy doing something else, 0 otherwise.
 *  \sa RecordPending(const ProgramInfo*, int), StopRecording()
 */
RecStatusType EncoderLink::StartRecording(const ProgramInfo *rec)
{
    RecStatusType retval = rsAborted;

    endRecordingTime = rec->recendts;
    startRecordingTime = rec->recstartts;
    chanid = rec->chanid;

    if (local)
        retval = tv->StartRecording(rec);
    else if (sock)
        retval = sock->StartRecording(m_capturecardnum, rec);
    else
        VERBOSE(VB_IMPORTANT,
                QString("Wanted to start recording on recorder %1,\n\t\t\t"
                        "but the backend is not there anymore\n")
                .arg(m_capturecardnum));

    if (retval != rsRecording)
    {
        endRecordingTime = QDateTime::currentDateTime().addDays(-2);
        startRecordingTime = endRecordingTime;
        chanid = "";
    }

    return retval;
}

/** \fn EncoderLink::StopRecording()
 *  \brief Tells TVRec to stop recording immediately.
 *         <b>This only works on local recorders.</b>
 *  \sa StartRecording(const ProgramInfo *rec), FinishRecording()
 */
void EncoderLink::StopRecording(void)
{
    endRecordingTime = QDateTime::currentDateTime().addDays(-2);
    startRecordingTime = endRecordingTime;
    chanid = "";

    if (local)
    {
        tv->StopRecording();
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
    if (local)
    {
        tv->FinishRecording();
        return;
    }
    else
    {
        endRecordingTime = QDateTime::currentDateTime().addDays(-2);
    }
}

/** \fn EncoderLink::IsReallyRecording()
 *  \brief Checks if the RecorderBase held by TVRec is actually recording.
 *         <b>This only works on local recorders.</b>
 *  \return true if actually recording, false otherwise.
 */
bool EncoderLink::IsReallyRecording(void)
{
    if (local)
        return tv->IsReallyRecording();

    VERBOSE(VB_IMPORTANT, "Should be local only query: IsReallyRecording");
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
    if (local)
        return tv->GetFramerate();

    VERBOSE(VB_IMPORTANT, "Should be local only query: GetFramerate");
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
    if (local)
        return tv->GetFramesWritten();

    VERBOSE(VB_IMPORTANT, "Should be local only query: GetFramesWritten");
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
    if (local)
        return tv->GetFilePosition();

    VERBOSE(VB_IMPORTANT, "Should be local only query: GetFilePosition");
    return -1;
}

/** \fn EncoderLink::GetFreeSpace(long long)
 *  \brief Returns number of bytes beyond "totalreadpos" it is safe to read.
 *         <b>This only works on local recorders.</b>
 *
 *  Note: This may return a negative number, including -1 if the call
 *  succeeds. This means totalreadpos is past the "safe read" portion of
 *  the file.
 *
 *  \sa TVRec::GetFreeSpace(long long), RemoteEncoder::GetFreeSpace(long long)
 *  \return Returns number of bytes ahead of totalreadpos it is safe to read
 *          if call succeeds, -1 otherwise.
 */
long long EncoderLink::GetFreeSpace(long long totalreadpos)
{
    if (local)
        return tv->GetFreeSpace(totalreadpos);

    VERBOSE(VB_IMPORTANT, "Should be local only query: GetFreeSpace");
    return -1;
}

/** \fn EncoderLink::GetKeyframePosition(long long)
 *  \brief Returns byte position in RingBuffer of a keyframe.
 *         <b>This only works on local recorders.</b>
 *  \sa TVRec::GetKeyframePosition(long long),
 *      RemoteEncoder::GetKeyframePosition(long long)
 *  \return Byte position of keyframe if query succeeds, -1 otherwise.
 */
long long EncoderLink::GetKeyframePosition(long long desired)
{
    if (local)
        return tv->GetKeyframePosition(desired);

    VERBOSE(VB_IMPORTANT, "Should be local only query: GetKeyframePosition");
    return -1;
}

/** \fn EncoderLink::FrontendReady()
 *  \brief Tells TVRec that the frontend is ready for data.
 *         <b>This only works on local recorders.</b> 
 *  \sa TVRec::, RemoteEncoder::
 */
void EncoderLink::FrontendReady(void)
{
    if (local)
        tv->FrontendReady();
    else
        VERBOSE(VB_IMPORTANT, "Should be local only query: FrontendReady");
}

/** \fn EncoderLink::CancelNextRecording()
 *  \brief Tells TVRec to cancel the next recording.
 *         <b>This only works on local recorders.</b>
 *
 *   This is used when the user is watching "Live TV" and does not
 *   want to allow the recorder to be taken for a pending recording.
 *
 *  \sa RecordPending(const ProgramInfo*,int)
 */
void EncoderLink::CancelNextRecording(void)
{
    if (local)
        tv->CancelNextRecording();
    else
        VERBOSE(VB_IMPORTANT, "Should be local only query: CancelNextRecording");
}

/** \fn EncoderLink::GetRecording()
 *  \brief Returns TVRec's current recording.
 *         <b>This only works on local recorders.</b>
 *  \return Returns TVRec's current recording if it succeeds, NULL otherwise.
 */
ProgramInfo *EncoderLink::GetRecording(void)
{
    if (local)
        return tv->GetRecording();

    VERBOSE(VB_IMPORTANT, "Should be local only query: GetRecording");
    return NULL;
}

/** \fn EncoderLink::StopPlaying()
 *  \brief Tells TVRec to stop streaming a recording to the frontend.
 *         <b>This only works on local recorders.</b>
 */
void EncoderLink::StopPlaying(void)
{
    if (local)
        tv->StopPlaying();
    else
        VERBOSE(VB_IMPORTANT, "Should be local only query: StopPlaying");
}

/** \fn EncoderLink::SetupRingBuffer(QString&,long long&,long long&,bool)
 *  \brief Sets up TVRec's RingBuffer for "Live TV" playback.
 *         <b>This only works on local recorders.</b>
 *
 *  \sa TVRec::SetupRingBuffer(QString&,long long&,long long&,bool),
 *      RemoteEncoder::SetupRingBuffer(QString&,long long&,long long&,bool),
 *      StopLiveTV()
 *
 *  \param path Returns path to recording.
 *  \param filesize Returns size of recording file in bytes.
 *  \param fillamount Returns the maximum buffer fill in bytes.
 *  \param pip Tells TVRec's RingBuffer that this is for a Picture in Picture display.
 *  \return true if successful, false otherwise.
 */
bool EncoderLink::SetupRingBuffer(QString &path, long long &filesize,
                                  long long &fillamount, bool pip)
{
    if (local)
        return tv->SetupRingBuffer(path, filesize, fillamount, pip);

    VERBOSE(VB_IMPORTANT, "Should be local only query: SetupRingBuffer");
    return false;
}

/** \fn EncoderLink::SpawnLiveTV()
 *  \brief Tells TVRec to Spawn a "Live TV" recorder.
 *         <b>This only works on local recorders.</b>
 *  \sa TVRec::SpawnLiveTV(), RemoteEncoder::SpawnLiveTV()
 */
void EncoderLink::SpawnLiveTV(void)
{
    if (local)
        tv->SpawnLiveTV();
    else
        VERBOSE(VB_IMPORTANT, "Should be local only query: SpawnLiveTV");
}

/** \fn EncoderLink::StopLiveTV()
 *  \brief Tells TVRec to stop a "Live TV" recorder.
 *         <b>This only works on local recorders.</b>
 *  \sa TVRec::StopLiveTV(), RemoteEncoder::StopLiveTV()
 */
void EncoderLink::StopLiveTV(void)
{
    if (local)
        tv->StopLiveTV();
    else
        VERBOSE(VB_IMPORTANT, "Should be local only query: StopLiveTV");
}

/** \fn EncoderLink::PauseRecorder()
 *  \brief Tells TVRec to pause a recorder, used for channel and input changes.
 *         <b>This only works on local recorders.</b>
 *  \sa TVRec::PauseRecorder(), RemoteEncoder::PauseRecorder(),
 *      RecorderBase::Pause()
 */
void EncoderLink::PauseRecorder(void)
{
    if (local)
        tv->PauseRecorder();
    else
        VERBOSE(VB_IMPORTANT, "Should be local only query: PauseRecorder");
}

/** \fn EncoderLink::ToggleInputs()
 *  \brief Tells TVRec's recorder to change to the next input.
 *         <b>This only works on local recorders.</b>
 *
 *   You must call PauseRecorder() before calling this.
 */
void EncoderLink::ToggleInputs(void)
{
    if (local)
        tv->ToggleInputs();
    else
        VERBOSE(VB_IMPORTANT, "Should be local only query: ToggleInputs");
}

/** \fn EncoderLink::ToggleChannelFavorite()
 *  \brief Toggles whether the current channel should be on our favorites list.
 *         <b>This only works on local recorders.</b>
 *  \return -1 if query does not succeed, otherwise.
 */
void EncoderLink::ToggleChannelFavorite(void)
{
    if (local)
        tv->ToggleChannelFavorite();
    else
        VERBOSE(VB_IMPORTANT, "Should be local only query: ToggleChannelFavorite");
}

/** \fn EncoderLink::ChangeChannel(int)
 *  \brief Changes to the next or previous channel.
 *         <b>This only works on local recorders.</b>
 *
 *   You must call PauseRecorder() before calling this.
 *  \param channeldirection channel change direction \sa BrowseDirections.
 */
void EncoderLink::ChangeChannel(int channeldirection)
{
    if (local)
        tv->ChangeChannel((ChannelChangeDirection)channeldirection);
    else
        VERBOSE(VB_IMPORTANT, "Should be local only query: ChangeChannel");
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
    if (local)
        tv->SetChannel(name);
    else
        VERBOSE(VB_IMPORTANT, "Should be local only query: SetChannel");
}

/** \fn EncoderLink::ChangeContrast(bool)
 *  \brief Changes contrast of a recording.
 *         <b>This only works on local recorders.</b>
 *
 *  Note: In practice this only works with frame grabbing recorders.
 *
 *  \return contrast if it succeeds, -1 otherwise.
 */
int EncoderLink::ChangeContrast(bool direction)
{
    int ret = -1;

    if (local)
        ret = tv->ChangeContrast(direction);
    else
        VERBOSE(VB_IMPORTANT, "Should be local only query: ChangeContrast");

    return ret;
}

/** \fn EncoderLink::ChangeBrightness(bool)
 *  \brief Changes the brightness of a recording.
 *         <b>This only works on local recorders.</b>
 *
 *  Note: In practice this only works with frame grabbing recorders.
 *
 *  \return brightness if it succeeds, -1 otherwise.
 */
int EncoderLink::ChangeBrightness(bool direction)
{
    int ret = -1;

    if (local)
        ret = tv->ChangeBrightness(direction);
    else
        VERBOSE(VB_IMPORTANT, "Should be local only query: ChangeBrightness");

    return ret;
}

/** \fn EncoderLink::ChangeColour(bool)
 *  \brief Changes the colour phase of a recording.
 *         <b>This only works on local recorders.</b>
 *
 *  Note: In practice this only works with frame grabbing recorders.
 *
 *  \return colour if it succeeds, -1 otherwise.
 */
int EncoderLink::ChangeColour(bool direction)
{
    int ret = -1;

    if (local)
        ret = tv->ChangeColour(direction);
    else
        VERBOSE(VB_IMPORTANT, "Should be local only query: ChangeColor");

    return ret;
}

/** \fn EncoderLink::ChangeHue(bool)
 *  \brief Changes the hue of a recording.
 *         <b>This only works on local recorders.</b>
 *
 *  Note: In practice this only works with frame grabbing recorders.
 *
 *  \return hue if it succeeds, -1 otherwise.
 */
int EncoderLink::ChangeHue(bool direction)
{
    int ret = -1;

    if (local)
        ret = tv->ChangeHue(direction);
    else
        VERBOSE(VB_IMPORTANT, "Should be local only query: ChangeHue");

    return ret;
}

/** \fn EncoderLink::CheckChannel(const QString&)
 *  \brief Checks if named channel exists on current tuner.
 *         <b>This only works on local recorders.</b>
 *  \param name Channel to verify against current tuner.
 *  \return true if it succeeds, false otherwise.
 *  \sa TVRec::CheckChannel(QString),
 *      RemoteEncoder::CheckChannel(QString), 
 *      ShouldSwitchToAnotherCard(const QString&)
 */
bool EncoderLink::CheckChannel(const QString &name)
{
    if (local)
        return tv->CheckChannel(name);

    VERBOSE(VB_IMPORTANT, "Should be local only query: CheckChannel");
    return false;
}

/** \fn EncoderLink::ShouldSwitchToAnotherCard(const QString&)
 *  \brief Checks if named channel exists on current tuner, or
 *         another tuner.
 *         <b>This only works on local recorders.</b>
 *  \param channelid channel to verify against tuners.
 *  \return true if the channel on another tuner and not current tuner,
 *          false otherwise.
 *  \sa CheckChannel(const QString&)
 */
bool EncoderLink::ShouldSwitchToAnotherCard(const QString &channelid)
{
    if (local)
        return tv->ShouldSwitchToAnotherCard(channelid);

    VERBOSE(VB_IMPORTANT, "Should be local only query: ShouldSwitchToAnotherCard");
    return false;
}

/** \fn EncoderLink::CheckChannelPrefix(const QString&,bool&)
 *  \brief Returns true if the numbers in prefix_num match the first digits
 *         of any channel, if it unquely identifies a channel the unique
 *         parameter is set.
 *         <b>This only works on local recorders.</b>
 *
 *  \param prefix_num Channel number prefix to check
 *  \param unique     This is set to true if prefix uniquely identifies
 *                    channel, false otherwise.
 *  \return true if the prefix matches any channels.
 */
bool EncoderLink::CheckChannelPrefix(const QString &prefix_num, bool &unique)
{
    if (local)
        return tv->CheckChannelPrefix(prefix_num, unique);

    VERBOSE(VB_IMPORTANT, "Should be local only query: CheckChannelPrefix");
    unique = false;
    return false;
}

/** \fn EncoderLink::GetNextProgram(int,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&)
 *  \brief Returns information about the program that would be seen if we changed
 *         the channel using ChangeChannel(int) with "direction".
 *         <b>This only works on local recorders.</b>
 */
void EncoderLink::GetNextProgram(int direction,
                                 QString &title,       QString &subtitle, 
                                 QString &desc,        QString &category, 
                                 QString &starttime,   QString &endtime, 
                                 QString &callsign,    QString &iconpath,
                                 QString &channelname, QString &chanid,
                                 QString &seriesid,    QString &programid)
{
    if (local)
        tv->GetNextProgram(direction,
                           title, subtitle, desc, category, starttime,
                           endtime, callsign, iconpath, channelname, chanid,
                           seriesid, programid);
    else
        VERBOSE(VB_IMPORTANT, "Should be local only query: GetNextProgram");
}

/** \fn EncoderLink::GetChannelInfo(QString&,QString&,QString&,QString&,QString&, QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&)
 *  \brief Returns information on the current program and current channel.
 *         <b>This only works on local recorders.</b>
 *  \sa TVRec::GetChannelInfo(QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&),
 *      RemoteEncoder::GetChannelInfo(QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&)
 *  \return -1 if query does not succeed, otherwise.
 */
void EncoderLink::GetChannelInfo(QString &title,       QString &subtitle, 
                                 QString &desc,        QString &category, 
                                 QString &starttime,   QString &endtime, 
                                 QString &callsign,    QString &iconpath,
                                 QString &channelname, QString &chanid,
                                 QString &seriesid,    QString &programid, 
                                 QString &chanFilters, QString &repeat,
                                 QString &airdate,     QString &stars)
{
    if (local)
        tv->GetChannelInfo(title, subtitle, desc, category, starttime,
                           endtime, callsign, iconpath, channelname, chanid,
                           seriesid, programid, chanFilters, repeat, airdate,
                           stars);
    else
        VERBOSE(VB_IMPORTANT, "Should be local only query: GetChannelInfo");
}

/** \fn EncoderLink::GetInputName()
 *  \brief Returns the textual name of the current input,
 *         if a tuner is being used.
 *         <b>This only works on local recorders.</b>
 *  \sa TVRec::GetInputName(QString&), RemoteEncoder::GetInputName(QString&)
 *  \return Returns input name if successful, "" if not.
 */
QString EncoderLink::GetInputName()
{
    QString inputname("");
    if (local)
        tv->GetInputName(inputname);
    else
        VERBOSE(VB_IMPORTANT, "Should be local only query: GetInputName");
    return inputname;
}

/** \fn EncoderLink::SetReadThreadSock(QSocket*)
 *  \brief Sets RingBuffer streaming socket.
 *         <b>This only works on local recorders.</b>
 *  \param rsock socket to use.
 *  \sa GetReadThreadSocket(), RequestRingBufferBlock(int)
 */
void EncoderLink::SetReadThreadSock(QSocket *rsock)
{
    if (local)
    {
        tv->SetReadThreadSock(rsock);
        return;
    }

    VERBOSE(VB_IMPORTANT, "Should be local only query: SpawnReadThread");
}

/** \fn EncoderLink::GetReadThreadSocket()
 *  \brief Returns RingBuffer streaming socket.
 *         <b>This only works on local recorders.</b>
 *  \return socket if it exists, NULL otherwise.
 *  \sa SetReadThreadSock(QSocket*), RequestRingBufferBlock(int)
 */
QSocket *EncoderLink::GetReadThreadSocket(void)
{
    if (local)
        return tv->GetReadThreadSocket();

    VERBOSE(VB_IMPORTANT, "Should be local only query: GetReadThreadSocket");
    return NULL;
}

/** \fn EncoderLink::RequestRingBufferBlock(int)
 *  \brief Tells TVRec to stream A/V data if it is available.
 *         <b>This only works on local recorders.</b>
 *
 *  \param size Requested block size, may not be respected, but this many
 *              bytes of data will be returned if it is available.
 *  \return -1 if request does not succeed, amount of data sent otherwise.
 */
int EncoderLink::RequestRingBufferBlock(int size)
{
    if (local)
        return tv->RequestRingBufferBlock(size);

    VERBOSE(VB_IMPORTANT, "Should be local only query: RequestRingBufferBlock");
    return -1;
}

/** \fn EncoderLink::SeekRingBuffer(long long, long long, int)
 *  \brief Tells TVRec to seek to a specific byte in RingBuffer.
 *         <b>This only works on local recorders.</b>
 *  \param curpos Current byte position in RingBuffer
 *  \param pos    Desired position, or position delta.
 *  \param whence One of SEEK_SET, or SEEK_CUR, or SEEK_END.
 *                These work like the equivalent fseek parameters.
 *  \return new position if seek is successful, -1 otherwise.
 */
long long EncoderLink::SeekRingBuffer(long long curpos, long long pos, 
                                      int whence)
{
    if (local)
        return tv->SeekRingBuffer(curpos, pos, whence);

    VERBOSE(VB_IMPORTANT, "Should be local only query: SeekRingBuffer");
    return -1;
}

/** \fn EncoderLink::GetScreenGrab(const ProgramInfo*,const QString&,int,int&,int&,int&)
 *  \brief Returns a PIX_FMT_RGBA32 buffer containg a frame from the video.
 *         <b>This only works on local recorders.</b>
 *  \param pginfo       Recording to grab from.
 *  \param filename     File containing recording.
 *  \param secondsin    Seconds into the video to seek before capturing a frame.
 *  \param bufferlen    Returns size of buffer returned (in bytes).
 *  \param video_width  Returns width of frame grabbed.
 *  \param video_height Returns height of frame grabbed.
 *  \return Buffer allocated with new containing frame in RGBA32 format if
 *          successful, NULL otherwise.
 */
char *EncoderLink::GetScreenGrab(const ProgramInfo *pginfo,
                                 const QString &filename, 
                                 int secondsin, int &bufferlen, 
                                 int &video_width, int &video_height,
                                 float &video_aspect)
{
    if (local && tv)
        return tv->GetScreenGrab(pginfo, filename, secondsin, bufferlen, 
                                 video_width, video_height, video_aspect);
    else if (local)
        VERBOSE(VB_IMPORTANT, "EncoderLink::GetScreenGrab() -- Error, tv is null");
    else
        VERBOSE(VB_IMPORTANT, "Should be local only query: GetScreenGrab");
    return NULL;
}

