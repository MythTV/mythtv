// C headers
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <pthread.h>
#include <sched.h> // for sched_yield

// C++ headers
#include <iostream>
using namespace std;

// Qt headers
#include <qapplication.h>
#include <qsqldatabase.h>
#include <qsocket.h>

// MythTV headers
#include "mythconfig.h"
#include "tv_rec.h"
#include "osd.h"
#include "mythcontext.h"
#include "dialogbox.h"
#include "recordingprofile.h"
#include "util.h"
#include "programinfo.h"
#include "recorderbase.h"
#include "NuppelVideoRecorder.h"
#include "NuppelVideoPlayer.h"
#include "channelbase.h"
#include "dtvsignalmonitor.h"
#include "dummydtvrecorder.h"
#include "mythdbcon.h"
#include "jobqueue.h"
#include "scheduledrecording.h"
#include "eitscanner.h"
#include "RingBuffer.h"
#include "previewgenerator.h"

#include "atscstreamdata.h"
#include "hdtvrecorder.h"
#include "atsctables.h"

#include "livetvchain.h"
#include "dummychannel.h"

#ifdef USING_V4L
#include "channel.h"
#endif

#ifdef USING_IVTV
#include "mpegrecorder.h"
#endif

#ifdef USING_DVB
#include "dvbchannel.h"
#include "dvbrecorder.h"
#include "dvbsiparser.h"
#endif

#ifdef USING_FIREWIRE
#ifdef CONFIG_DARWIN
#include "darwinfirewirerecorder.h"
#include "darwinfirewirechannel.h"
#else 
#include "firewirerecorder.h"
#include "firewirechannel.h"
#endif 
#endif

#ifdef USING_DBOX2
#include "dbox2recorder.h"
#include "dbox2channel.h"
#endif

#define DEBUG_CHANNEL_PREFIX 0 /**< set to 1 to channel prefixing */

#define LOC QString("TVRec(%1): ").arg(cardid)
#define LOC_ERR QString("TVRec(%1) Error: ").arg(cardid)

/// How many seconds after entering kState_None should we start EIT Scanner
const uint TVRec::kEITScanStartTimeout = 60; /* 1 minute */

/// How many milliseconds the signal monitor should wait between checks
const uint TVRec::kSignalMonitoringRate = 50; /* msec */

static bool is_dishnet_eit(int cardid);

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

/** \fn TVRec::TVRec(int)
 *  \brief Performs instance initialiation not requiring access to database.
 *
 *  \sa Init()
 *  \param capturecardnum Capture card number
 */
TVRec::TVRec(int capturecardnum)
       // Various components TVRec coordinates
    : recorder(NULL), channel(NULL), signalMonitor(NULL),
      scanner(NULL), dvbsiparser(NULL), dummyRecorder(NULL),
      // Configuration variables from database
      transcodeFirst(false), earlyCommFlag(false), runJobOnHostOnly(false),
      audioSampleRateDB(0), overRecordSecNrml(0), overRecordSecCat(0),
      overRecordCategory(""),
      // Configuration variables from setup rutines
      cardid(capturecardnum), ispip(false),
      // State variables
      stateChangeLock(true),
      internalState(kState_None), desiredNextState(kState_None),
      changeState(false), pauseNotify(true),
      stateFlags(0), lastTuningRequest(0),
      // Current recording info
      curRecording(NULL), autoRunJobs(JOB_NONE),
      // Pending recording info
      pendingRecording(NULL),
      // Pseudo LiveTV recording
      pseudoLiveTVRecording(NULL),
      // tvchain
      tvchain(NULL),
      // RingBuffer info
      ringBuffer(NULL), rbFilePrefix(""), rbFileExt("mpg")
{
}

bool TVRec::CreateChannel(const QString &startchannel)
{
    bool init_run = false;
    if (genOpt.cardtype == "DVB")
    {
#ifdef USING_DVB_EIT
        if (!scanner)
            scanner = new EITScanner();
#endif // USING_DVB_EIT

#ifdef USING_DVB
        channel = new DVBChannel(genOpt.videodev.toInt(), this);
        if (!channel->Open())
            return false;
        GetDVBChannel()->SetSlowTuning(dvbOpt.dvb_tuning_delay);
        InitChannel(genOpt.defaultinput, startchannel);
        CloseChannel(); // Close the channel if in dvb_on_demand mode
        init_run = true;
#endif
    }
    else if (genOpt.cardtype == "FIREWIRE")
    {
#ifdef USING_FIREWIRE
# ifdef CONFIG_DARWIN
        channel = new DarwinFirewireChannel(fwOpt, this);
# else 
        channel = new FirewireChannel(fwOpt, this);
# endif
        if (!channel->Open())
            return false;
        InitChannel(genOpt.defaultinput, startchannel);
        init_run = true;
#endif
    }
    else if (genOpt.cardtype == "DBOX2")
    {
#ifdef USING_DBOX2
        channel = new DBox2Channel(this, &dboxOpt, cardid);
        if (!channel->Open())
            return false;
        InitChannel(genOpt.defaultinput, startchannel);
        init_run = true;
#endif
    }
    else if (genOpt.cardtype == "MPEG" &&
             genOpt.videodev.lower().left(5) == "file:")
    {
        channel = new DummyChannel(this);
        InitChannel(genOpt.defaultinput, startchannel);
        init_run = true;
    }
    else // "V4L" or "MPEG", ie, analog TV, or "HDTV"
    {
#ifdef USING_V4L
        channel = new Channel(this, genOpt.videodev);
        if (!channel->Open())
            return false;
        InitChannel(genOpt.defaultinput, startchannel);
        CloseChannel();
        init_run = true;
#endif
        if (genOpt.cardtype != "HDTV" && genOpt.cardtype != "MPEG")
            rbFileExt = "nuv";
    }

    if (!init_run)
    {
        QString msg = QString(
            "%1 card configured on video device %2, \n"
            "but MythTV was not compiled with %2 support. \n"
            "\n"
            "Recompile MythTV with %3 support or remove the card \n"
            "from the configuration and restart MythTV.")
            .arg(genOpt.cardtype).arg(genOpt.videodev)
            .arg(genOpt.cardtype).arg(genOpt.cardtype);
        VERBOSE(VB_IMPORTANT, LOC_ERR + msg);
        SetFlags(kFlagErrored);
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
    QMutexLocker lock(&stateChangeLock);

    if (!GetDevices(cardid, genOpt, dvbOpt, fwOpt, dboxOpt))
        return false;

    QString startchannel = GetStartChannel(cardid, genOpt.defaultinput);
    if (!CreateChannel(startchannel))
        return false;

    transcodeFirst    =
        gContext->GetNumSetting("AutoTranscodeBeforeAutoCommflag", 0);
    earlyCommFlag     = gContext->GetNumSetting("AutoCommflagWhileRecording", 0);
    runJobOnHostOnly  = gContext->GetNumSetting("JobsRunOnRecordHost", 0);
    audioSampleRateDB = gContext->GetNumSetting("AudioSampleRate");
    overRecordSecNrml = gContext->GetNumSetting("RecordOverTime");
    overRecordSecCat  = gContext->GetNumSetting("CategoryOverTime") * 60;
    overRecordCategory= gContext->GetSetting("OverTimeCategory");
    rbFilePrefix      = gContext->GetSetting("RecordFilePrefix");

    pthread_create(&event_thread, NULL, EventThread, this);

    WaitForEventThreadSleep();

    return true;
}

/** \fn TVRec::~TVRec()
 *  \brief Stops the event and scanning threads and deletes any ChannelBase,
 *         RingBuffer, and RecorderBase instances.
 */
TVRec::~TVRec(void)
{
    TeardownAll();
}

void TVRec::deleteLater(void)
{
    TeardownAll();
    QObject::deleteLater();
}

void TVRec::TeardownAll(void)
{
    if (HasFlags(kFlagRunMainLoop))
    {
        ClearFlags(kFlagRunMainLoop);
        pthread_join(event_thread, NULL);
    }

    TeardownSignalMonitor();
    TeardownSIParser();
#ifdef USING_DVB_EIT
    if (scanner)
    {
        delete scanner;
        scanner = NULL;
    }
#endif // USING_DVB_EIT

#ifdef USING_DVB
    if (GetDVBChannel())
        GetDVBChannel()->deleteLater();
    else
#endif // USING_DVB
#ifdef USING_DBOX2
    if (GetDBox2Channel())
        GetDBox2Channel()->deleteLater();
    else
#endif // USING_DBOX2
    if (channel)
        delete channel;
    channel = NULL;

    TeardownRecorder(true);

    if (dummyRecorder)
    {
        dummyRecorder->deleteLater();
        dummyRecorder = NULL;
    }

    SetRingBuffer(NULL);
}

/** \fn TVRec::GetState()
 *  \brief Returns the TVState of the recorder.
 *
 *   If there is a pending state change kState_ChangingState is returned.
 *  \sa EncoderLink::GetState(), \ref recorder_subsystem
 */
TVState TVRec::GetState(void)
{
    if (changeState)
        return kState_ChangingState;
    return internalState;
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
    QMutexLocker lock(&stateChangeLock);

    ProgramInfo *tmppginfo = NULL;

    if (curRecording && !changeState)
    {
        tmppginfo = new ProgramInfo(*curRecording);
        tmppginfo->recstatus = rsRecording;
    }
    else
        tmppginfo = new ProgramInfo();
    tmppginfo->cardid = cardid;

    return tmppginfo;
}

/** \fn TVRec::RecordPending(const ProgramInfo*, int)
 *  \brief Tells TVRec "rcinfo" is the next pending recording.
 *
 *   When there is a pending recording and the frontend is in "Live TV"
 *   mode the TVRec event loop will send a "ASK_RECORDING" message to
 *   it. Depending on what that query returns, the recording will be
 *   started or not started.
 *
 *  \sa TV::AskAllowRecording(const QStringList&, int)
 *  \param rcinfo   ProgramInfo on pending program.
 *  \param secsleft Seconds left until pending recording begins.
 */
void TVRec::RecordPending(const ProgramInfo *rcinfo, int secsleft)
{
    QMutexLocker lock(&stateChangeLock);
    if (pendingRecording)
        delete pendingRecording;

    pendingRecording = new ProgramInfo(*rcinfo);
    recordPendingStart = QDateTime::currentDateTime().addSecs(secsleft);
    SetFlags(kFlagAskAllowRecording);
}

/** \fn TVRec::SetPseudoLiveTVRecording(ProgramInfo*)
 *  \brief Sets the pseudo LiveTV ProgramInfo
 */
void TVRec::SetPseudoLiveTVRecording(ProgramInfo *pi)
{
    ProgramInfo *old_rec = pseudoLiveTVRecording;
    pseudoLiveTVRecording = pi;
    if (old_rec)
        delete old_rec;
}

/** \fn TVRec::GetRecordEndTime(const ProgramInfo*) const
 *  \brief Returns recording end time with proper post-roll
 */
QDateTime TVRec::GetRecordEndTime(const ProgramInfo *pi) const
{
    bool spcat = (pi->category == overRecordCategory);
    int secs = (spcat) ? overRecordSecCat : overRecordSecNrml;
    return pi->recendts.addSecs(secs);
}

/** \fn TVRec::CancelNextRecording(bool)
 *  \brief Tells TVRec to cancel the upcoming recording.
 *  \sa RecordPending(const ProgramInfo*,int),
 *      TV::AskAllowRecording(const QStringList&,int)
 */
void TVRec::CancelNextRecording(bool cancel)
{
    if (cancel)
        SetFlags(kFlagCancelNextRecording);
    else
        ClearFlags(kFlagCancelNextRecording);
}

/** \fn TVRec::StartRecording(const ProgramInfo*)
 *  \brief Tells TVRec to Start recording the program "rcinfo"
 *         as soon as possible.
 *
 *  \return +1 if the recording started successfully,
 *          -1 if TVRec is busy doing something else, 0 otherwise.
 *  \sa EncoderLink::StartRecording(const ProgramInfo*)
 *      RecordPending(const ProgramInfo*, int), StopRecording()
 */
RecStatusType TVRec::StartRecording(const ProgramInfo *rcinfo)
{
    VERBOSE(VB_RECORD, LOC + QString("StartRecording(%1)").arg(rcinfo->title));

    QMutexLocker lock(&stateChangeLock);
    QString msg("");

    RecStatusType retval = rsAborted;

    // Flush out any pending state changes
    WaitForEventThreadSleep();

    // We need to do this check early so we don't cancel an overrecord
    // that we're trying to extend.
    if (internalState != kState_WatchingLiveTV && 
        curRecording &&
        curRecording->title == rcinfo->title &&
        curRecording->chanid == rcinfo->chanid &&
        curRecording->startts == rcinfo->startts)
    {
        int post_roll_seconds  = curRecording->recendts.secsTo(recordEndTime);
        curRecording->rectype  = rcinfo->rectype;
        curRecording->recordid = rcinfo->recordid;
        curRecording->recendts = rcinfo->recendts;
        curRecording->UpdateRecordingEnd();
        MythEvent me("RECORDING_LIST_CHANGE");
        gContext->dispatch(me);

        recordEndTime = curRecording->recendts.addSecs(post_roll_seconds);

        msg = QString("updating recording: %1 %2 %3 %4")
            .arg(curRecording->title).arg(curRecording->chanid)
            .arg(curRecording->recstartts.toString())
            .arg(curRecording->recendts.toString());
        VERBOSE(VB_RECORD, LOC + msg);

        ClearFlags(kFlagCancelNextRecording);

        retval = rsRecording;
        return retval;
    }

    if (pendingRecording)
    {
        delete pendingRecording;
        pendingRecording = NULL;
    }

    ClearFlags(kFlagAskAllowRecording);

    // Flush out events...
    WaitForEventThreadSleep();

    // If in post-roll, end recording
    if (GetState() == kState_RecordingOnly &&
        !HasFlags(kFlagCancelNextRecording))
    {
        stateChangeLock.unlock();
        StopRecording();
        stateChangeLock.lock();
    }

    if (internalState == kState_None)
    {
        if (tvchain)
        {
            QString message = QString("LIVETV_EXITED");
            MythEvent me(message, tvchain->GetID());
            gContext->dispatch(me);
            tvchain = NULL;
        }

        recordEndTime = GetRecordEndTime(rcinfo);

        // Tell event loop to begin recording.
        curRecording = new ProgramInfo(*rcinfo);
        curRecording->MarkAsInUse(true, "recorder");
        StartedRecording(curRecording);

        // Make sure scheduler is allowed to end this recording
        ClearFlags(kFlagCancelNextRecording);

        ChangeState(kState_RecordingOnly);

        retval = rsRecording;
    }
    else if (!HasFlags(kFlagCancelNextRecording) &&
             (GetState() == kState_WatchingLiveTV))
    {
        SetPseudoLiveTVRecording(new ProgramInfo(*rcinfo));
        recordEndTime = GetRecordEndTime(rcinfo);

        // We want the frontend to to change channel for recording
        // and disable the UI for channel change, PiP, etc.

        QString message = QString("LIVETV_WATCH %1 1").arg(cardid);
        QStringList prog;
        rcinfo->ToStringList(prog);
        MythEvent me(message, prog);
        gContext->dispatch(me);

        retval = rsRecording;
    }
    else if (!HasFlags(kFlagCancelNextRecording))
    {
        msg = QString("Wanted to record: %1 %2 %3 %4\n"
            "\t\t\tBut the current state is: %5")
              .arg(rcinfo->title).arg(rcinfo->chanid)
              .arg(rcinfo->recstartts.toString())
              .arg(rcinfo->recendts.toString())
              .arg(StateToString(internalState));
        if (curRecording && internalState == kState_RecordingOnly)
            msg += QString("\n\t\t\tCurrently recording: %1 %2 %3 %4")
                .arg(curRecording->title).arg(curRecording->chanid)
                .arg(curRecording->recstartts.toString())
                .arg(curRecording->recendts.toString());
        VERBOSE(VB_IMPORTANT, LOC + msg);

        retval = rsTunerBusy;
    }

    WaitForEventThreadSleep();

    return retval;
}

/** \fn TVRec::StopRecording(void)
 *  \brief Changes from kState_RecordingOnly to kState_None.
 *  \sa StartRecording(const ProgramInfo *rec), FinishRecording()
 */
void TVRec::StopRecording(void)
{
    if (StateIsRecording(GetState()))
    {
        QMutexLocker lock(&stateChangeLock);
        ChangeState(RemoveRecording(GetState()));
        // wait for state change to take effect
        WaitForEventThreadSleep();
        ClearFlags(kFlagCancelNextRecording);
    }
}

/** \fn TVRec::StateIsRecording(TVState)
 *  \brief Returns "state == kState_RecordingOnly"
 *  \param state TVState to check.
 */
bool TVRec::StateIsRecording(TVState state)
{
    return (state == kState_RecordingOnly);
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
 *  \brief If "state" is kState_RecordingOnly, returns a kState_None,
 *         otherwise returns kState_Error.
 *  \param state TVState to check.
 */
TVState TVRec::RemoveRecording(TVState state)
{
    if (StateIsRecording(state))
        return kState_None;

    VERBOSE(VB_IMPORTANT, LOC_ERR +
            QString("Unknown state in RemoveRecording: %1")
            .arg(StateToString(state)));
    return kState_Error;
}

/** \fn TVRec::RemovePlaying(TVState)
 *  \brief Returns TVState that would remove the playing, but potentially
 *         keep recording if we are watching an in progress recording.
 *  \param state TVState to check.
 */
TVState TVRec::RemovePlaying(TVState state)
{
    if (StateIsPlaying(state))
    {
        if (state == kState_WatchingPreRecorded)
            return kState_None;
        return kState_RecordingOnly;
    }

    QString msg = "Unknown state in RemovePlaying: %1";
    VERBOSE(VB_IMPORTANT, LOC_ERR + msg.arg(StateToString(state)));

    return kState_Error;
}

/** \fn TVRec::StartedRecording(ProgramInfo *curRec)
 *  \brief Inserts a "curRec" into the database, and issues a
 *         "RECORDING_LIST_CHANGE" event.
 *  \param curRec Recording to add to database.
 *  \sa ProgramInfo::StartedRecording(const QString&)
 */
void TVRec::StartedRecording(ProgramInfo *curRec)
{
    if (!curRec)
        return;

    curRec->StartedRecording(rbFilePrefix, rbFileExt);
    VERBOSE(VB_RECORD, LOC + "StartedRecording("<<curRec<<") fn("
            <<curRec->GetFileName()<<")");

    if (curRec->chancommfree != 0)
        curRec->SetCommFlagged(COMM_FLAG_COMMFREE);

    MythEvent me("RECORDING_LIST_CHANGE");
    gContext->dispatch(me);
}

/** \fn TVRec::FinishedRecording(ProgramInfo *curRec)
 *  \brief If not a premature stop, adds program to history of recorded 
 *         programs. If the recording type is kFindOneRecord this find
 *         is removed.
 *  \sa ProgramInfo::FinishedRecording(bool prematurestop)
 *  \param curRec ProgramInfo or recording to mark as done
 */
void TVRec::FinishedRecording(ProgramInfo *curRec)
{
    if (!curRec)
        return;

    ProgramInfo *pi = NULL;

    QString pigrp = curRec->recgroup;

    pi = ProgramInfo::GetProgramFromRecorded(curRec->chanid,
                                             curRec->recstartts);
    if (pi)
    {
        pigrp = pi->recgroup;
        delete pi;
    }
    VERBOSE(VB_RECORD, LOC + QString("FinishedRecording(%1) in recgroup: %2")
                                     .arg(curRec->title).arg(pigrp));

    curRec->recstatus = rsRecorded;
    curRec->recendts = mythCurrentDateTime();

    if (tvchain)
        tvchain->FinishedRecording(curRec);

    curRec->recendts.setTime(QTime(
        curRec->recendts.addSecs(30).time().hour(),
        curRec->recendts.addSecs(30).time().minute()));

    // Make sure really short recordings have positive run time.
    if (curRec->recendts <= curRec->recstartts)
        curRec->recendts = mythCurrentDateTime().addSecs(1);

    if (pigrp != "LiveTV")
    {
        MythEvent me(QString("UPDATE_RECORDING_STATUS %1 %2 %3 %4 %5")
                     .arg(curRec->cardid)
                     .arg(curRec->chanid)
                     .arg(curRec->startts.toString(Qt::ISODate))
                     .arg(curRec->recstatus)
                     .arg(curRec->recendts.toString(Qt::ISODate)));
        gContext->dispatch(me);
    }
    curRec->FinishedRecording(false);
}

#define TRANSITION(ASTATE,BSTATE) \
   ((internalState == ASTATE) && (desiredNextState == BSTATE))
#define SET_NEXT() do { nextState = desiredNextState; changed = true; } while(0)
#define SET_LAST() do { nextState = internalState; changed = true; } while(0)

/** \fn TVRec::HandleStateChange(void)
 *  \brief Changes the internalState to the desiredNextState if possible.
 *
 *   Note: There must exist a state transition from any state we can enter
 *   to the kState_None state, as this is used to shutdown TV in RunTV.
 *
 */
void TVRec::HandleStateChange(void)
{
    TVState nextState = internalState;

    bool changed = false;

    QString transMsg = QString(" %1 to %2")
        .arg(StateToString(nextState))
        .arg(StateToString(desiredNextState));

    if (desiredNextState == internalState)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "HandleStateChange(): "
                "Null transition" + transMsg);
        changeState = false;
        return;
    }

    // Make sure EIT scan is stopped before any tuning,
    // to avoid race condition with it's tuning requests.
    if (HasFlags(kFlagEITScannerRunning))
    {
#ifdef USING_DVB_EIT
        scanner->StopActiveScan();
#endif // USING_DVB_EIT
        ClearFlags(kFlagEITScannerRunning);
    }

    // Handle different state transitions
    if (TRANSITION(kState_None, kState_WatchingLiveTV))
    {
        tuningRequests.enqueue(TuningRequest(kFlagLiveTV));
        SET_NEXT();
    }
    else if (TRANSITION(kState_WatchingLiveTV, kState_None))
    {
        tuningRequests.enqueue(TuningRequest(kFlagKillRec|kFlagKillRingBuffer));
        SET_NEXT();
    }
    else if (TRANSITION(kState_WatchingLiveTV, kState_RecordingOnly))
    {
        SetPseudoLiveTVRecording(NULL);

        SET_NEXT();
    }
    else if (TRANSITION(kState_None, kState_RecordingOnly))
    {
        SetPseudoLiveTVRecording(NULL);
        tuningRequests.enqueue(TuningRequest(kFlagRecording, curRecording));
        SET_NEXT();
    }
    else if (TRANSITION(kState_RecordingOnly, kState_None))
    {
        tuningRequests.enqueue(
            TuningRequest(kFlagCloseRec|kFlagKillRingBuffer));
        SET_NEXT();
    }

    QString msg = (changed) ? "Changing from" : "Unknown state transition:";
    VERBOSE(VB_IMPORTANT, LOC + msg + transMsg);
 
    // update internal state variable
    internalState = nextState;
    changeState = false;

    eitScanStartTime = QDateTime::currentDateTime();    
    if ((internalState == kState_None) && (genOpt.cardtype == "DVB"))
    {
        // Add some randomness to avoid all cards starting
        // EIT scanning at nearly the same time.
        uint timeout = kEITScanStartTimeout + random() % 59;
        eitScanStartTime = eitScanStartTime.addSecs(timeout);
    }
    else
        eitScanStartTime = eitScanStartTime.addYears(1);
}
#undef TRANSITION
#undef SET_NEXT
#undef SET_LAST

/** \fn TVRec::ChangeState(TVState)
 *  \brief Puts a state change on the nextState queue.
 */
void TVRec::ChangeState(TVState nextState)
{
    QMutexLocker lock(&stateChangeLock);

    desiredNextState = nextState;
    changeState = true;
    triggerEventLoop.wakeAll();
}

/** \fn TVRec::SetOption(RecordingProfile&, const QString&)
 *  \brief Calls RecorderBase::SetOption(const QString&,int) with the "name"d
 *         option from the recording profile.
 *  \sa RecordingProfile, RecorderBase
 *  \param profile RecordingProfile to use
 *  \param name RecordingProfile option to get and RecorderBase option to set
 */
void TVRec::SetOption(RecordingProfile &profile, const QString &name)
{
    int value = profile.byName(name)->getValue().toInt();
    recorder->SetOption(name, value);
}

/** \fn TVRec::SetupRecorder(RecordingProfile&)
 *  \brief Allocates and initializes the RecorderBase instance.
 *
 *  Based on the card type, one of the possible recorders are started.
 *  If the card type is "MPEG" a MpegRecorder is started,
 *  if the card type is "HDTV" a HDTVRecorder is started,
 *  if the card type is "FIREWIRE" a FirewireRecorder is started,
 *  if the card type is "DVB" a DVBRecorder is started,
 *  otherwise a NuppelVideoRecorder is started.
 *
 *  If there is any this will return false.
 * \sa IsErrored()
 */
bool TVRec::SetupRecorder(RecordingProfile &profile)
{
    recorder = NULL;
    if (genOpt.cardtype == "MPEG")
    {
#ifdef USING_IVTV
        recorder = new MpegRecorder(this);
#endif // USING_IVTV
    }
    else if (genOpt.cardtype == "HDTV")
    {
#ifdef USING_V4L
        recorder = new HDTVRecorder(this);
        ringBuffer->SetWriteBufferSize(4*1024*1024);
        recorder->SetOption("wait_for_seqstart", genOpt.wait_for_seqstart);
#endif // USING_V4L
    }
    else if (genOpt.cardtype == "FIREWIRE")
    {
#ifdef USING_FIREWIRE
# ifdef CONFIG_DARWIN
        recorder = new DarwinFirewireRecorder(this, this->channel);
# else 
        recorder = new FirewireRecorder(this);
        recorder->SetOption("port",       fwOpt.port);
        recorder->SetOption("node",       fwOpt.node);
        recorder->SetOption("speed",      fwOpt.speed);
        recorder->SetOption("model",      fwOpt.model);
        recorder->SetOption("connection", fwOpt.connection);
# endif // !CONFIG_DARWIN 
#endif // USING_FIREWIRE
    }
    else if (genOpt.cardtype == "DBOX2")
    {
#ifdef USING_DBOX2
        recorder = new DBox2Recorder(this, GetDBox2Channel());
        recorder->SetOption("port",     dboxOpt.port);
        recorder->SetOption("host",     dboxOpt.host);
        recorder->SetOption("httpport", dboxOpt.httpport);
#endif // USING_DBOX2
    }
    else if (genOpt.cardtype == "DVB")
    {
#ifdef USING_DVB
        recorder = new DVBRecorder(this, GetDVBChannel());
        ringBuffer->SetWriteBufferSize(4*1024*1024);
        recorder->SetOption("wait_for_seqstart", genOpt.wait_for_seqstart);
        recorder->SetOption("dvb_on_demand",     dvbOpt.dvb_on_demand);
#endif // USING_DVB
    }
    else
    {
#ifdef USING_V4L
        // V4L/MJPEG/GO7007 from here on
        recorder = new NuppelVideoRecorder(this, channel);
        recorder->SetOption("skipbtaudio", genOpt.skip_btaudio);
#endif // USING_V4L
    }

    if (recorder)
    {
        recorder->SetOptionsFromProfile(
            &profile, genOpt.videodev, genOpt.audiodev, genOpt.vbidev);
        recorder->SetRingBuffer(ringBuffer);
        recorder->Initialize();

        if (recorder->IsErrored())
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to initialize recorder!");
            recorder->deleteLater();
            recorder = NULL;
            return false;
        }

        return true;
    }

    QString msg = "Need %1 recorder, but compiled without %2 support!";
    msg = msg.arg(genOpt.cardtype).arg(genOpt.cardtype);
    VERBOSE(VB_IMPORTANT, LOC_ERR + msg);

    return false;
}

/** \fn TVRec::TeardownRecorder(bool)
 *  \brief Tears down the recorder.
 *  
 *   If a "recorder" exists, RecorderBase::StopRecording() is called.
 *   We then wait for "recorder_thread" to exit, and finally we delete 
 *   "recorder".
 *
 *   If a RingBuffer instance exists, RingBuffer::StopReads() is called,
 *   and then we delete the RingBuffer instance.
 *
 *   If killfile is true, the recording is deleted.
 *
 *   A "RECORDING_LIST_CHANGE" message is dispatched.
 *
 *   Finally, if there was a recording and it was not deleted,
 *   schedule any post-processing jobs.
 *
 *  \param killFile if true the recorded file is deleted.
 */
void TVRec::TeardownRecorder(bool killFile)
{
    int filelen = -1;
    pauseNotify = false;
    ispip = false;

    if (recorder && HasFlags(kFlagRecorderRunning))
    {
        // This is a bad way to calculate this, the framerate
        // may not be constant if using a DTV based recorder.
        filelen = (int)((float)GetFramesWritten() / GetFramerate());

        QString message = QString("DONE_RECORDING %1 %2")
            .arg(cardid).arg(filelen);
        MythEvent me(message);
        gContext->dispatch(me);

        recorder->StopRecording();
        pthread_join(recorder_thread, NULL);
    }
    ClearFlags(kFlagRecorderRunning);

    if (recorder)
    {
        if (GetV4LChannel())
            channel->SetFd(-1);
        recorder->deleteLater();
        recorder = NULL;
    }

    if (ringBuffer)
        ringBuffer->StopReads();

    if (curRecording)
    {
        if (!killFile)
        {
            (new PreviewGenerator(curRecording, true))->Start();

            if (!tvchain)
            {
                int secsSince = curRecording->recstartts.secsTo(
                                                  QDateTime::currentDateTime());
                if (secsSince < 120)
                {
                    JobQueue::RemoveJobsFromMask(JOB_COMMFLAG, autoRunJobs);
                    JobQueue::RemoveJobsFromMask(JOB_TRANSCODE, autoRunJobs);
                }

                if (autoRunJobs)
                    JobQueue::QueueRecordingJobs(curRecording, autoRunJobs);
            }
        }

        FinishedRecording(curRecording);

        curRecording->MarkAsInUse(false);
        delete curRecording;
        curRecording = NULL;
    }

    MythEvent me("RECORDING_LIST_CHANGE");
    gContext->dispatch(me);
    pauseNotify = true;
}    

DVBRecorder *TVRec::GetDVBRecorder(void)
{
#ifdef USING_DVB
    return dynamic_cast<DVBRecorder*>(recorder);
#else // if !USING_DVB
    return NULL;
#endif // !USING_DVB
}

HDTVRecorder *TVRec::GetHDTVRecorder(void)
{
#ifdef USING_V4L
    return dynamic_cast<HDTVRecorder*>(recorder);
#else // if !USING_V4L
    return NULL;
#endif // USING_V4L
}

/** \fn TVRec::InitChannel(const QString&, const QString&)
 *  \brief Performs ChannelBase instance init from database and 
 *         tuner hardware (requires that channel be open).
 */
void TVRec::InitChannel(const QString &inputname, const QString &startchannel)
{
    if (!channel)
        return;

#ifdef USING_V4L
    Channel *chan = GetV4LChannel();
    if (chan)
    {
        chan->SetFormat(gContext->GetSetting("TVFormat"));
        chan->SetDefaultFreqTable(gContext->GetSetting("FreqTable"));
    }
#endif // USING_V4L

    if (inputname.isEmpty())
        channel->SetChannelByString(startchannel);
    else
        channel->SwitchToInput(inputname, startchannel);

    // Set channel ordering, and check validity...
    QString chanorder = gContext->GetSetting("ChannelOrdering", "channum + 0");
    channel->SetChannelOrdering(chanorder);
    QString channum_out(startchannel), chanid("");
    DoGetNextChannel(channum_out, channel->GetCurrentInput(), cardid,
                     channel->GetOrdering(), BROWSE_SAME, chanid);
}

void TVRec::CloseChannel(void)
{
    if (!channel)
        return;

#ifdef USING_DVB
    if (GetDVBChannel())
    {
        if (dvbOpt.dvb_on_demand)
        {
            TeardownSIParser();
            ClearFlags(kFlagSIParserRunning);
            GetDVBChannel()->Close();
        }
        return;
    }
#endif // USING_DVB

    channel->Close();
}

DBox2Channel *TVRec::GetDBox2Channel(void)
{
#ifdef USING_DBOX2
    return dynamic_cast<DBox2Channel*>(channel);
#else
    return NULL;
#endif // USING_DBOX2
}

DVBChannel *TVRec::GetDVBChannel(void)
{
#ifdef USING_DVB
    return dynamic_cast<DVBChannel*>(channel);
#else
    return NULL;
#endif // USING_DVB
}

Channel *TVRec::GetV4LChannel(void)
{
#ifdef USING_V4L
    return dynamic_cast<Channel*>(channel);
#else
    return NULL;
#endif // USING_V4L
}

void TVRec::CreateSIParser(MPEGStreamData *stream_data, int program_num)
{
    (void) program_num;
#ifdef USING_DVB

    DVBChannel *dvbc = GetDVBChannel();
    if (!dvbc)
        return;

    if (!dvbsiparser)
    {
        dvbsiparser = new DVBSIParser(dvbc->GetCardNum(), true);
        QObject::connect(dvbsiparser,
                         SIGNAL(UpdatePMT(uint, const ProgramMapTable*)),
                         dvbc,
                         SLOT(   SetPMT(  uint, const ProgramMapTable*)));
    }

    dvbsiparser->ReinitSIParser(dvbc->GetSIStandard(),
                                stream_data, program_num);

#ifdef USING_DVB_EIT
    if (is_dishnet_eit(GetCaptureCardNum()))
    {
        VERBOSE(VB_EIT, "Enabling DishNet Long Term EIT Support");
        dvbsiparser->SetDishNetEIT(true);
    }

    if (scanner)
        scanner->StartPassiveScan(dvbc, dvbsiparser);
#endif // USING_DVB_EIT

#endif // USING_DVB
}

void TVRec::TeardownSIParser(void)
{
#ifdef USING_DVB_EIT
    if (scanner)
        scanner->StopPassiveScan();
#endif // USING_DVB_EIT
#ifdef USING_DVB
    if (dvbsiparser)
    {
        dvbsiparser->deleteLater();
        dvbsiparser = NULL;
    }
#endif // USING_DVB
}

/** \fn TVRec::EventThread(void*)
 *  \brief Thunk that allows event pthread to call RunTV().
 */
void *TVRec::EventThread(void *param)
{
    TVRec *thetv = (TVRec *)param;
    thetv->RunTV();
    return NULL;
}

/** \fn TVRec::RecorderThread(void*)
 *  \brief Thunk that allows recorder pthread to
 *         call RecorderBase::StartRecording().
 */
void *TVRec::RecorderThread(void *param)
{
    RecorderBase *recorder = (RecorderBase *)param;
    recorder->StartRecording();
    return NULL;
}

bool get_use_eit(uint cardid)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT SUM(useeit) "
        "FROM videosource, cardinput "
        "WHERE videosource.sourceid = cardinput.sourceid AND"
        "      cardinput.cardid     = :CARDID");
    query.bindValue(":CARDID", cardid);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("get_use_eit", query);
        return false;
    }
    else if (query.next())
        return query.value(0).toBool();
    return false;
}

static bool is_dishnet_eit(int cardid)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT SUM(dishnet_eit) "
        "FROM videosource, cardinput "
        "WHERE videosource.sourceid = cardinput.sourceid AND"
        "      cardinput.cardid     = :CARDID");
    query.bindValue(":CARDID", cardid);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("is_dishnet_eit", query);
        return false;
    }
    else if (query.next())
        return query.value(0).toBool();
    return false;
}

/** \fn TVRec::RunTV(void)
 *  \brief Event handling method, contains event loop.
 */
void TVRec::RunTV(void)
{ 
    QMutexLocker lock(&stateChangeLock);
    SetFlags(kFlagRunMainLoop);
    ClearFlags(kFlagExitPlayer | kFlagFinishRecording);

    // Add some randomness to avoid all cards starting
    // EIT scanning at nearly the same time.
    uint timeout = kEITScanStartTimeout + random() % 59;
    eitScanStartTime = QDateTime::currentDateTime().addSecs(timeout);

    while (HasFlags(kFlagRunMainLoop))
    {
        // If there is a state change queued up, do it...
        if (changeState)
        {
            HandleStateChange();
            ClearFlags(kFlagFrontendReady | kFlagCancelNextRecording);
            SetFlags(kFlagAskAllowRecording);
        }

        // Quick exit on fatal errors.
        if (IsErrored())
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "RunTV encountered fatal error, exiting event thread.");
            ClearFlags(kFlagRunMainLoop);
            return;
        }

        // Handle any tuning events..
        HandleTuning();

        // If we have a pending recording and AskAllowRecording is set
        // and the frontend is ready send an ASK_RECORDING query to frontend.
        if (pendingRecording && HasFlags(kFlagAskAllowRecording))
        {
            ClearFlags(kFlagAskAllowRecording);

            if (GetState() == kState_WatchingLiveTV)
            {
                CheckForRecGroupChange();
                if (pseudoLiveTVRecording)
                    SetFlags(kFlagCancelNextRecording);
                else
                    ClearFlags(kFlagCancelNextRecording);

                int timeuntil = QDateTime::currentDateTime()
                    .secsTo(recordPendingStart);

                QString query = QString("ASK_RECORDING %1 %2 %3")
                    .arg(cardid).arg(timeuntil)
                    .arg(pseudoLiveTVRecording ? 1 : 0);
                VERBOSE(VB_IMPORTANT, LOC + query);
                QStringList messages;
                messages << pendingRecording->title
                         << pendingRecording->chanstr
                         << pendingRecording->chansign
                         << pendingRecording->channame;
                MythEvent me(query, messages);
                gContext->dispatch(me);
            }
        }

        // If we are recording a program, check if the recording is
        // over or someone has asked us to finish the recording.
        if (GetState() == kState_RecordingOnly &&
            (QDateTime::currentDateTime() > recordEndTime ||
             HasFlags(kFlagFinishRecording)))
        {
            ChangeState(kState_None);
            ClearFlags(kFlagFinishRecording);
        }

        if (curRecording)
            curRecording->UpdateInUseMark();

        // Check for the end of the current program..
        if (GetState() == kState_WatchingLiveTV)
        {
#define LIVETV_END (now >= curRecording->endts)
// use the following instead to test ringbuffer switching
//static QDateTime last = QDateTime::currentDateTime(); 
//#define LIVETV_END ((now >= curRecording->recstartts.addSecs(20)) && (now > last))

            QDateTime now   = QDateTime::currentDateTime();
            bool has_finish = HasFlags(kFlagFinishRecording);
            bool has_rec    = pseudoLiveTVRecording;
            bool rec_soon   = pendingRecording;
            bool enable_ui  = true;

            if (has_rec && (has_finish || (now > recordEndTime)))
                SetPseudoLiveTVRecording(NULL);
            else if (!has_rec && !rec_soon && curRecording && LIVETV_END)
            {
                SwitchLiveTVRingBuffer();

                QDateTime starttime; starttime.setTime_t(0);
                if (curRecording)
                    starttime = curRecording->recstartts;
                VERBOSE(VB_RECORD, LOC 
                        <<"!has_rec("<<!has_rec<<") "
                        <<"!rec_soon("<<!rec_soon<<") "
                        <<"curRec("<<curRecording<<") "
                        <<"starttm("<<starttime.toString(Qt::ISODate)<<")");
//                last = QDateTime::currentDateTime().addSecs(20); 
            }
            else
                enable_ui = false;

            if (enable_ui)
            {
                VERBOSE(VB_IMPORTANT, LOC + "Enabling Full LiveTV UI.");
                QString message = QString("LIVETV_WATCH %1 0").arg(cardid);
                MythEvent me(message);
                gContext->dispatch(me);
            }   
        }

        // Check for ExitPlayer flag, and if set change to a non-watching
        // state (either kState_RecordingOnly or kState_None).
        if (HasFlags(kFlagExitPlayer))
        {
            if (internalState == kState_WatchingLiveTV)
                ChangeState(kState_None);
            else if (StateIsPlaying(internalState))
                ChangeState(RemovePlaying(internalState));
            ClearFlags(kFlagExitPlayer);
        }

#ifdef USING_DVB_EIT
        if (channel && scanner &&
            QDateTime::currentDateTime() > eitScanStartTime)
        {
            if (!get_use_eit(GetCaptureCardNum()))
            {
                VERBOSE(VB_EIT, LOC + "EIT scanning disabled "
                        "for all sources on this card.");
                eitScanStartTime = eitScanStartTime.addYears(1);
            }
            else
            {
                uint ttMin = gContext->GetNumSetting("EITTransportTimeout", 5);
                scanner->StartActiveScan(this, ttMin * 60);
                SetFlags(kFlagEITScannerRunning);
                eitScanStartTime = QDateTime::currentDateTime().addYears(1);
            }
        }
#endif // USING_DVB_EIT

        // We should be no more than a few thousand milliseconds,
        // as the end recording code does not have a trigger...
        // NOTE: If you change anything here, make sure that
        // WaitforEventThreadSleep() will still work...
        if (tuningRequests.empty() && !changeState)
        {
            triggerEventSleep.wakeAll();
            lock.mutex()->unlock();
            sched_yield();
            triggerEventSleep.wakeAll();
            triggerEventLoop.wait(1000 /* ms */);
            lock.mutex()->lock();
        }
    }
  
    if (GetState() != kState_None)
    {
        ChangeState(kState_None);
        HandleStateChange();
    }
}

bool TVRec::WaitForEventThreadSleep(bool wake, ulong time)
{
    bool ok = false;
    MythTimer t;
    t.start();
    while (!ok && ((unsigned long) t.elapsed()) < time)
    {
        if (wake)
            triggerEventLoop.wakeAll();

        stateChangeLock.unlock();
        // It is possible for triggerEventSleep.wakeAll() to be sent
        // before we enter wait so we only wait 100 ms so we can try
        // again a few times before 15 second timeout on frontend...
        triggerEventSleep.wait(100);
        stateChangeLock.lock();

        // verify that we were triggered.
        ok = (tuningRequests.empty() && !changeState);
    }
    return ok;
}

bool TVRec::GetDevices(int cardid,
                       GeneralDBOptions  &gen_opts,
                       DVBDBOptions      &dvb_opts,
                       FireWireDBOptions &firewire_opts,
                       DBox2DBOptions    &dbox2_opts)
{
    int testnum = 0;
    QString test;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT videodevice,      vbidevice,           audiodevice,     "
        "       audioratelimit,   defaultinput,        cardtype,        "
        "       skipbtaudio,      signal_timeout,      channel_timeout, "
        "       dvb_wait_for_seqstart, "
        ""
        "       dvb_on_demand,    dvb_tuning_delay, "
        ""
        "       firewire_port,    firewire_node,       firewire_speed,  "
        "       firewire_model,   firewire_connection,                  "
        ""
        "       dbox2_port,       dbox2_host,          dbox2_httpport   "
        ""
        "FROM capturecard "
        "WHERE cardid = :CARDID");
    query.bindValue(":CARDID", cardid);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("getdevices", query);
        return false;
    }

    if (!query.next())
        return false;

    // General options
    test = query.value(0).toString();
    if (test != QString::null)
        gen_opts.videodev = QString::fromUtf8(test);

    test = query.value(1).toString();
    if (test != QString::null)
        gen_opts.vbidev = QString::fromUtf8(test);

    test = query.value(2).toString();
    if (test != QString::null)
        gen_opts.audiodev = QString::fromUtf8(test);

    gen_opts.audiosamplerate = max(testnum, query.value(3).toInt());

    test = query.value(4).toString();
    if (test != QString::null)
        gen_opts.defaultinput = QString::fromUtf8(test);

    test = query.value(5).toString();
    if (test != QString::null)
        gen_opts.cardtype = QString::fromUtf8(test);

    gen_opts.skip_btaudio = query.value(6).toUInt();

    gen_opts.signal_timeout  = (uint) max(query.value(7).toInt(), 0);
    gen_opts.channel_timeout = (uint) max(query.value(8).toInt(), 0);

    // We should have at least 100 ms to acquire tables...
    int table_timeout = ((int)gen_opts.channel_timeout - 
                         (int)gen_opts.signal_timeout);
    if (table_timeout < 100)
        gen_opts.channel_timeout = gen_opts.signal_timeout + 2500;

    gen_opts.wait_for_seqstart = query.value(9).toUInt();

    // DVB options
    uint dvboff = 10;
    dvb_opts.dvb_on_demand    = query.value(dvboff + 0).toUInt();
    dvb_opts.dvb_tuning_delay = query.value(dvboff + 1).toUInt();

    // Firewire options
    uint fireoff = dvboff + 2;
    firewire_opts.port        = query.value(fireoff + 0).toUInt();
    firewire_opts.node        = query.value(fireoff + 1).toUInt(); 
    firewire_opts.speed       = query.value(fireoff + 2).toUInt();

    test = query.value(fireoff + 3).toString();
    if (test != QString::null)
        firewire_opts.model = QString::fromUtf8(test);

    firewire_opts.connection  = query.value(fireoff + 4).toUInt();

    // DBOX2 options
    uint dbox2off = fireoff + 5;
    dbox2_opts.port = query.value(dbox2off + 0).toUInt();

    test = query.value(dbox2off + 1).toString();
    if (test != QString::null)
        dbox2_opts.host = QString::fromUtf8(test);

    dbox2_opts.httpport = query.value(dbox2off + 2).toUInt();

    return true;
}

QString TVRec::GetStartChannel(int cardid, const QString &defaultinput)
{
    QString msg("");
    QString startchan = QString::null;

    // Get last tuned channel from database, to use as starting channel
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT startchan "
        "FROM capturecard, cardinput "
        "WHERE capturecard.cardid = cardinput.cardid AND "
        "      capturecard.cardid = :CARDID          AND "
        "      inputname          = :INPUTNAME");
    query.bindValue(":CARDID",    cardid);
    query.bindValue(":INPUTNAME", defaultinput);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("getstartchan", query);
    }
    else if (query.next())
    {
        QString test = query.value(0).toString();
        if (test != QString::null)
            startchan = QString::fromUtf8(test);
    }
    if (!startchan.isEmpty())
        return startchan;

    // If we failed to get the last tuned channel,
    // get a valid channel on our current input.
    query.prepare(
        "SELECT channum "
        "FROM capturecard, cardinput, channel "
        "WHERE capturecard.cardid = cardinput.cardid   AND "
        "      channel.sourceid   = cardinput.sourceid AND "
        "      capturecard.cardid = :CARDID AND "
        "      inputname          = :INPUTNAME");
    query.bindValue(":CARDID",    cardid);
    query.bindValue(":INPUTNAME", defaultinput);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("getstartchan2", query);
    }
    else if (query.next())
    {
        QString test = query.value(0).toString();
        if (test != QString::null)
        {
            msg = "Start channel from DB is empty, setting to '%1' instead.";
            VERBOSE(VB_IMPORTANT, LOC_ERR + msg.arg(test));
            startchan = QString::fromUtf8(test);
        }
    }
    if (!startchan.isEmpty())
        return startchan;

    // If we failed to get a channel on our current input,
    // widen search to any input.
    query.prepare(
        "SELECT channum, inputname "
        "FROM capturecard, cardinput, channel "
        "WHERE capturecard.cardid = cardinput.cardid   AND "
        "      channel.sourceid   = cardinput.sourceid AND "
        "      capturecard.cardid = :CARDID");
    query.bindValue(":CARDID", cardid);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("getstartchan3", query);
    }
    else if (query.size() > 0)
    {
        query.next();

        QString test = query.value(0).toString();
        if (test != QString::null)
        {
            msg = QString("Start channel invalid, setting "
                          "to '%1' on input %2 instead.")
                .arg(test).arg(query.value(1).toString());
            VERBOSE(VB_IMPORTANT, LOC_ERR + msg);
            startchan = QString::fromUtf8(test);
        }
    }
    if (startchan.isEmpty())
    {
        // If there are no valid channels, just use a random channel
        startchan = "3";
        msg = "Problem finding starting channel, setting to default of '%1'.";
        VERBOSE(VB_IMPORTANT, LOC_ERR + msg.arg(startchan));
    }
    return startchan;
}

void GetPidsToCache(DTVSignalMonitor *dtvMon, pid_cache_t &pid_cache)
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

bool ApplyCachedPids(DTVSignalMonitor *dtvMon, const ChannelBase* channel)
{
    pid_cache_t pid_cache;
    channel->GetCachedPids(pid_cache);
    pid_cache_t::const_iterator it = pid_cache.begin();
    bool vctpid_cached = false;
    for (; it != pid_cache.end(); ++it)
    {
        if ((it->second == TableID::TVCT) ||
            (it->second == TableID::CVCT))
        {
            vctpid_cached = true;
            dtvMon->GetATSCStreamData()->AddListeningPID(it->first);
        }
    }
    return vctpid_cached;
}

/** \fn bool TVRec::SetupDTVSignalMonitor(void)
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
 */
bool TVRec::SetupDTVSignalMonitor(void)
{
    VERBOSE(VB_RECORD, LOC + "Setting up table monitoring.");

    DTVSignalMonitor *sm = GetDTVSignalMonitor();
    ATSCStreamData *sd = NULL;
#ifdef USING_V4L
    if (GetHDTVRecorder())
    {
        sd = GetHDTVRecorder()->GetStreamData();
        sd->SetCaching(true);
    }
#endif // USING_V4L
#ifdef USING_DVB
    if (GetDVBRecorder())
    {
        sd = GetDVBRecorder()->GetStreamData();
        sd->SetCaching(true);
    }
#endif // USING_DVB

    if (!sd)
        sd = new ATSCStreamData(-1, -1, true);

    // Check if this is an ATSC Channel
    int major = channel->GetMajorChannel();
    int minor = channel->GetMinorChannel();
    if (minor > 0)
    {
        QString msg = QString("ATSC channel: %1_%2").arg(major).arg(minor);
        VERBOSE(VB_RECORD, LOC + msg);

        sm->SetStreamData(sd);
        sd->Reset(major, minor);
        sm->SetChannel(major, minor);
        sd->SetVideoStreamsRequired(1);
        sm->SetFTAOnly(true);

        // Try to get pid of VCT from cache and
        // require MGT if we don't have VCT pid.
        if (!ApplyCachedPids(sm, channel))
            sm->AddFlags(kDTVSigMon_WaitForMGT);

        VERBOSE(VB_RECORD, LOC + "Successfully set up ATSC table monitoring.");
        return true;
    }

    // Check if this is an MPEG Channel
    int progNum = channel->GetProgramNumber();
    if (progNum >= 0)
    {
        QString msg = QString("MPEG program number: %1").arg(progNum);
        VERBOSE(VB_RECORD, LOC + msg);

#ifdef USING_DVB
        // Some DVB devices munge the PMT and/or PAT so the CRC check fails.
        // We need to tell the stream data class to not check the CRC on 
        // these devices.
        if (GetDVBChannel())
            sd->SetIgnoreCRC(GetDVBChannel()->HasCRCBug());
#endif // USING_DVB

        bool fta = CardUtil::IgnoreEncrypted(
            GetCaptureCardNum(), channel->GetCurrentInput());

        sm->SetStreamData(sd);
        ((MPEGStreamData*)sd)->Reset(progNum);
        sm->SetProgramNumber(progNum);
        sd->SetVideoStreamsRequired(0);
        sm->SetFTAOnly(fta);
        sm->AddFlags(kDTVSigMon_WaitForPAT | kDTVSigMon_WaitForPMT);

        VERBOSE(VB_RECORD, LOC + "Successfully set up MPEG table monitoring.");
        return true;
    }

    QString msg = "No valid DTV info, ATSC maj(%1) min(%2), MPEG pn(%3)";
    VERBOSE(VB_RECORD, LOC_ERR + msg.arg(major).arg(minor).arg(progNum));
    return false;
}

/** \fn TVRec::SetupSignalMonitor(bool,bool)
 *  \brief This creates a SignalMonitor instance if one is needed and
 *         begins signal monitoring.
 *
 *   If the channel exists and the cardtype is "DVB" or "HDTV" a 
 *   SignalMonitor instance is created and SignalMonitor::Start()
 *   is called to start the signal monitoring thread.
 *
 *  \param tablemon If set we enable table monitoring
 *  \param notify   If set we notify the frontend of the signal values
 */
void TVRec::SetupSignalMonitor(bool tablemon, bool notify)
{
    VERBOSE(VB_RECORD, LOC + "SetupSignalMonitor("
            <<tablemon<<", "<<notify<<")");

    // if it already exists, there no need to initialize it
    if (signalMonitor)
        return;

    // if there is no channel object we can't monitor it
    if (!channel)
        return;

    // make sure statics are initialized
    SignalMonitorValue::Init();

    if (SignalMonitor::IsSupported(genOpt.cardtype) && channel->Open())
        signalMonitor = SignalMonitor::Init(genOpt.cardtype, cardid, channel);
    
    if (signalMonitor)
    {
        VERBOSE(VB_RECORD, LOC + "Signal monitor successfully created");
        // If this is a monitor for Digital TV, initialize table monitors
        if (GetDTVSignalMonitor() && tablemon)
            SetupDTVSignalMonitor();

        connect(signalMonitor, SIGNAL(AllGood(void)),
                this, SLOT(SignalMonitorAllGood(void)));

        signalMonitor->SetUpdateRate(kSignalMonitoringRate);
        signalMonitor->SetNotifyFrontend(notify);

        // Start the monitoring thread
        signalMonitor->Start();
    }
}

/** \fn TVRec::TeardownSignalMonitor()
 *  \brief If a SignalMonitor instance exists, the monitoring thread is
 *         stopped and the instance is deleted.
 */
void TVRec::TeardownSignalMonitor()
{
    if (!signalMonitor)
        return;

    VERBOSE(VB_RECORD, LOC + "TeardownSignalMonitor() -- begin");

    // If this is a DTV signal monitor, save any pids we know about.
    DTVSignalMonitor *dtvMon = GetDTVSignalMonitor();
    if (dtvMon && channel)
    {
        pid_cache_t pid_cache;
        GetPidsToCache(dtvMon, pid_cache);
        if (pid_cache.size())
            channel->SaveCachedPids(pid_cache);
    }

    if (signalMonitor)
    {
        signalMonitor->deleteLater();
        signalMonitor = NULL;
    }

    VERBOSE(VB_RECORD, LOC + "TeardownSignalMonitor() -- end");
}

/** \fn TVRec::SetSignalMonitoringRate(int,int)
 *  \brief Sets the signal monitoring rate.
 *
 *  \sa EncoderLink::SetSignalMonitoringRate(int,int),
 *      RemoteEncoder::SetSignalMonitoringRate(int,int)
 *  \param rate           The update rate to use in milliseconds,
 *                        use 0 to disable signal monitoring.
 *  \param notifyFrontend If 1, SIGNAL messages will be sent to
 *                        the frontend using this recorder.
 *  \return 1 if it signal monitoring is turned on, 0 otherwise.
 */
int TVRec::SetSignalMonitoringRate(int rate, int notifyFrontend)
{
    QString msg = "SetSignalMonitoringRate(%1, %2)";
    VERBOSE(VB_RECORD, LOC + msg.arg(rate).arg(notifyFrontend) + "-- start");

    QMutexLocker lock(&stateChangeLock);

    if (!SignalMonitor::IsSupported(genOpt.cardtype))
    {
        VERBOSE(VB_IMPORTANT, LOC + "Signal Monitoring is not"
                "supported by your hardware.");
        return 0;
    }

    if (GetState() != kState_WatchingLiveTV)
    {
        VERBOSE(VB_IMPORTANT, LOC + "Signal can only "
                "be monitored in LiveTV Mode.");
        return 0;
    }

    ClearFlags(kFlagRingBufferReady);

    TuningRequest req = (rate > 0) ?
        TuningRequest(kFlagAntennaAdjust, channel->GetCurrentName()) :
        TuningRequest(kFlagLiveTV);

    tuningRequests.enqueue(req);

    // Wait for RingBuffer reset
    while (!HasFlags(kFlagRingBufferReady))
        WaitForEventThreadSleep();
    VERBOSE(VB_RECORD, LOC + msg.arg(rate).arg(notifyFrontend) + " -- end");
    return 1;
}

DTVSignalMonitor *TVRec::GetDTVSignalMonitor(void)
{
    return dynamic_cast<DTVSignalMonitor*>(signalMonitor);
}

/** \fn TVRec::ShouldSwitchToAnotherCard(QString)
 *  \brief Checks if named channel exists on current tuner, or
 *         another tuner.
 *
 *  \param chanid channel to verify against tuners.
 *  \return true if the channel on another tuner and not current tuner,
 *          false otherwise.
 *  \sa EncoderLink::ShouldSwitchToAnotherCard(const QString&),
 *      RemoteEncoder::ShouldSwitchToAnotherCard(QString),
 *      CheckChannel(QString)
 */
bool TVRec::ShouldSwitchToAnotherCard(QString chanid)
{
    QString msg("");
    MSqlQuery query(MSqlQuery::InitCon());

    if (!query.isConnected())
        return false;

    query.prepare("SELECT channel.channum, channel.callsign "
                  "FROM channel "
                  "WHERE channel.chanid = :CHANID");
    query.bindValue(":CHANID", chanid);
    if (!query.exec() || !query.isActive() || query.size() == 0)
    {
        MythContext::DBError("ShouldSwitchToAnotherCard", query);
        return false;
    }

    query.next();
    QString channelname = query.value(0).toString();
    QString callsign = query.value(1).toString();

    query.prepare(
        "SELECT channel.channum "
        "FROM channel,cardinput "
        "WHERE ( channel.chanid = :CHANID OR             "
        "        ( channel.channum  = :CHANNUM AND       "
        "          channel.callsign = :CALLSIGN    )     "
        "      )                                     AND "
        "      channel.sourceid = cardinput.sourceid AND "
        "      cardinput.cardid = :CARDID");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":CHANNUM", channelname);
    query.bindValue(":CALLSIGN", callsign);
    query.bindValue(":CARDID", cardid);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("ShouldSwitchToAnotherCard", query);
    }
    else if (query.size() > 0)
    {
        msg = "Found channel (%1) on current card(%2).";
        VERBOSE(VB_RECORD, LOC + msg.arg(channelname).arg(cardid));
        return false;
    }

    // We didn't find it on the current card, so now we check other cards.
    query.prepare(
        "SELECT channel.channum, cardinput.cardid "
        "FROM channel,cardinput "
        "WHERE ( channel.chanid = :CHANID OR              "
        "        ( channel.channum  = :CHANNUM AND        "
        "          channel.callsign = :CALLSIGN    )      "
        "      )                                      AND "
        "      channel.sourceid  = cardinput.sourceid AND "
        "      cardinput.cardid != :CARDID");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":CHANNUM", channelname);
    query.bindValue(":CALLSIGN", callsign);
    query.bindValue(":CARDID", cardid);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("ShouldSwitchToAnotherCard", query);
    }
    else if (query.next())
    {
        msg = QString("Found channel (%1) on different card(%2).")
            .arg(query.value(0).toString()).arg(query.value(1).toString());
        VERBOSE(VB_RECORD, LOC + msg);
        return true;
    }

    msg = QString("Did not find channel(%1) on any card.").arg(channelname);
    VERBOSE(VB_RECORD, LOC + msg);
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
 *      ShouldSwitchToAnotherCard(QString)
 */
bool TVRec::CheckChannel(QString name) const
{
    if (!channel)
        return false;

    QString dummyID;
    return channel->CheckChannel(name, dummyID);
}

/** \fn QString add_spacer(const QString&, const QString&)
 *  \brief Adds the spacer before the last character in chan.
 */
static QString add_spacer(const QString &chan, const QString &spacer)
{
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
 *   then the cardid of the recorder it matches is returned in
 *   'is_complete_valid_channel_on_rec'; if it matches multiple recorders,
 *   and one of them is this recorder, this recorder is returned in
 *   'is_complete_valid_channel_on_rec'; if it isn't complete for any channel
 *    on any recorder 'is_complete_valid_channel_on_rec' is set to zero.
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
                               uint          &is_complete_valid_channel_on_rec,
                               bool          &is_extra_char_useful,
                               QString       &needed_spacer)
{
    static const uint kSpacerListSize = 5;
    static const char* spacers[kSpacerListSize] = { "", "_", "-", "#", "." };

    MSqlQuery query(MSqlQuery::InitCon());
    QString basequery = QString(
        "SELECT channel.chanid, channel.channum, cardinput.cardid "
        "FROM channel, capturecard, cardinput "
        "WHERE channel.channum LIKE '%1%%'           AND "
        "      channel.sourceid = cardinput.sourceid AND "
        "      cardinput.cardid = capturecard.cardid");

    QString cardquery[2] =
    {
        QString(" AND capturecard.cardid  = '%1'").arg(cardid),
        QString(" AND capturecard.cardid != '%1'").arg(cardid),
    };

    vector<uint>    fchanid;
    vector<QString> fchannum;
    vector<uint>    fcardid;
    vector<QString> fspacer;

    for (uint i = 0; i < 2; i++)
    {
        for (uint j = 0; j < kSpacerListSize; j++)
        {
            QString qprefix = add_spacer(prefix, spacers[j]);
            query.prepare(basequery.arg(qprefix) + cardquery[i]);

            if (!query.exec() || !query.isActive())
            {
                MythContext::DBError("checkchannel -- locate channum", query);
            }
            else if (query.size())
            {
                while (query.next())
                {
                    fchanid.push_back(query.value(0).toUInt());
                    fchannum.push_back(query.value(1).toString());
                    fcardid.push_back(query.value(2).toUInt());
                    fspacer.push_back(spacers[j]);
#if DEBUG_CHANNEL_PREFIX
                    VERBOSE(VB_IMPORTANT, QString("(%1,%2) Adding %3 rec %4")
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
    is_complete_valid_channel_on_rec = 0;
    needed_spacer = "";

    if (fchanid.size() == 0)
        return false;

    if (fchanid.size() == 1) // Unique channel...
    {
        needed_spacer = QDeepCopy<QString>(fspacer[0]);
        bool nc       = (fchannum[0] != add_spacer(prefix, fspacer[0]));

        is_complete_valid_channel_on_rec = (nc) ? 0 : fcardid[0];
        is_extra_char_useful             = nc;
        return true;
    }

    // If we get this far there is more than one channel
    // sharing the prefix we were given.

    // Is an extra characher useful for disambiguation?
    for (uint i = 0; i < fchannum.size(); i++)
    {
        if (fchannum[i] != add_spacer(prefix, fspacer[0]))
        {
            is_extra_char_useful = true;
            break;
        }
    }

    // Are any of the channels complete w/o spacer?
    // If so set is_complete_valid_channel_on_rec,
    // with a preference for our cardid.
    for (uint i = 0; i < fchannum.size(); i++)
    {
        if (fchannum[i] == add_spacer(prefix, fspacer[0]))
        {
            is_complete_valid_channel_on_rec = fcardid[i];
            if (fcardid[i] == (uint)cardid)
                break;
        }
    }

    // Add a spacer, if one is needed to select a valid channel.
    if (!is_complete_valid_channel_on_rec)
    {
        bool spacer_needed = true;
        for (uint i = 0; i < fspacer.size(); i++)
        {
            if (fspacer[i].isEmpty())
            {
                spacer_needed = false;
                break;
            }
        }
        if (spacer_needed)
            needed_spacer = QDeepCopy<QString>(fspacer[0]);
    }

    return true;
}

bool TVRec::SetVideoFiltersForChannel(ChannelBase *chan, const QString &channum)
{
    if (!chan)
        return false;

    bool ret = false;

    QString channelinput = chan->GetCurrentInput();
    QString videoFilters = "";

    if (channelinput.isEmpty())
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    if (!query.isConnected())
        return true;

    query.prepare(
        "SELECT channel.videofilters "
        "FROM channel, capturecard, cardinput "
        "WHERE channel.channum      = :CHANNUM           AND "
        "      channel.sourceid     = cardinput.sourceid AND "
        "      cardinput.inputname  = :INPUT             AND "
        "      cardinput.cardid     = capturecard.cardid AND "
        "      capturecard.cardid   = :CARDID            AND "
        "      capturecard.hostname = :HOSTNAME");
    query.bindValue(":CHANNUM", channum);
    query.bindValue(":INPUT", channelinput);
    query.bindValue(":CARDID", cardid);
    query.bindValue(":HOSTNAME", gContext->GetHostName());

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("setvideofilterforchannel", query);
    }
    else if (query.size() > 0)
    {
        query.next();

        videoFilters = query.value(0).toString();

        if (recorder != NULL)
        {
            recorder->SetVideoFilters(videoFilters);
        }

        return true;
    }

    query.prepare("SELECT NULL FROM channel");
    query.exec();

    if (query.size() == 0)
        ret = true;

    return ret;
}

int TVRec::GetChannelValue(const QString &channel_field, ChannelBase *chan, 
                           const QString &channum)
{
    int retval = -1;

    if (!chan)
        return retval;

    MSqlQuery query(MSqlQuery::InitCon());
    if (!query.isConnected())
        return retval;

    QString channelinput = chan->GetCurrentInput();
   
    query.prepare(
        QString(
            "SELECT channel.%1 "
            "FROM channel, capturecard, cardinput "
            "WHERE channel.channum      = :CHANNUM           AND "
            "      channel.sourceid     = cardinput.sourceid AND "
            "      cardinput.inputname  = :INPUT             AND "
            "      cardinput.cardid     = capturecard.cardid AND "
            "      capturecard.cardid   = :CARDID            AND "
            "      capturecard.hostname = :HOSTNAME")
        .arg(channel_field));
    query.bindValue(":CHANNUM", channum);
    query.bindValue(":INPUT", channelinput);
    query.bindValue(":CARDID", cardid);
    query.bindValue(":HOSTNAME", gContext->GetHostName());

    if (!query.exec() || !query.isActive())
        MythContext::DBError("getchannelvalue", query);
    else if (query.next())
        retval = query.value(0).toInt();

    return retval;
}

void TVRec::SetChannelValue(QString &field_name, int value, ChannelBase *chan,
                            const QString &channum)
{
    int sourceid = GetChannelValue("sourceid", chan, channum);
    if (sourceid < 0)
        return;

    MSqlQuery query(MSqlQuery::InitCon());
    if (!query.isConnected())
        return;

    query.prepare(
        QString("UPDATE channel SET channel.%1=:VALUE "
                "WHERE channel.channum  = :CHANNUM AND "
                "      channel.sourceid = :SOURCEID").arg(field_name));

    query.bindValue(":VALUE",    value);
    query.bindValue(":CHANNUM",  channum);
    query.bindValue(":SOURCEID", sourceid);

    query.exec();
}

QString TVRec::GetNextChannel(ChannelBase *chan, int channeldirection)
{
    QString ret = "";

    if (!chan)
        return ret;

    // Get info on the current channel we're on
    QString channum = chan->GetCurrentName();
    QString chanid = "";

    DoGetNextChannel(channum, chan->GetCurrentInput(), cardid,
                     chan->GetOrdering(), channeldirection, chanid);

    return channum;
}

QString TVRec::GetNextRelativeChanID(QString channum, int channeldirection)
{
    // Get info on the current channel we're on
    QString channum_out = channum;
    QString chanid = "";

    if (!channel)
        return chanid;

    DoGetNextChannel(channum_out, channel->GetCurrentInput(), 
                     cardid, channel->GetOrdering(),
                     channeldirection, chanid);

    return chanid;
}

void TVRec::DoGetNextChannel(QString &channum, QString channelinput,
                             int cardid, QString channelorder,
                             int channeldirection, QString &chanid)
{
    bool isNum = true;
    channum.toULong(&isNum);

    if (!isNum && channelorder == "channum + 0")
    {
        bool is_atsc = GetChannelValue("atscsrcid", channel, channum) > 0;
        channelorder = (is_atsc) ? "atscsrcid" : "channum";
        channel->SetChannelOrdering(channelorder);
        if (!is_atsc)
        {
            VERBOSE(VB_IMPORTANT, LOC +
                    "Your channel ordering method \"channel number (numeric)\"\n"
                    "\t\t\twill not work with channels like: "<<channum<<"    \n"
                    "\t\t\tConsider switching to order by \"database order\"  \n"
                    "\t\t\tor \"channel number (alpha)\" in the general       \n"
                    "\t\t\tsettings section of the frontend setup             \n"
                    "\t\t\tSwitched to "<<channelorder<<" order.");
        }
    }

    MSqlQuery query(MSqlQuery::InitCon());

    QString querystr = QString(
        "SELECT %1 "
        "FROM channel,capturecard,cardinput "
        "WHERE channel.channum      = :CHANNUM           AND "
        "      channel.sourceid     = cardinput.sourceid AND "
        "      cardinput.cardid     = capturecard.cardid AND "
        "      capturecard.cardid   = :CARDID            AND "
        "      capturecard.hostname = :HOSTNAME").arg(channelorder);
    query.prepare(querystr);
    query.bindValue(":CHANNUM",  channum);
    query.bindValue(":CARDID",   cardid);
    query.bindValue(":HOSTNAME", gContext->GetHostName());

    QString id = QString::null;

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        query.next();

        id = query.value(0).toString();
    }
    else
    {
        QString msg = QString(
            "Channel: '%1' was not found in the database.\n"
            "\t\t\tMost likely, the default channel set for this input\n"
            "\t\t\tcardid %2, input '%3'\n"
            "\t\t\tin setup is wrong\n")
            .arg(channum).arg(cardid).arg(channelinput);
        VERBOSE(VB_IMPORTANT, LOC + msg);

        querystr = QString(
            "SELECT %1 "
            "FROM channel, capturecard, cardinput "
            "WHERE channel.sourceid     = cardinput.sourceid AND "
            "      cardinput.cardid     = capturecard.cardid AND "
            "      capturecard.cardid   = :CARDID            AND "
            "      capturecard.hostname = :HOSTNAME "
            "ORDER BY %2 "
            "LIMIT 1").arg(channelorder).arg(channelorder);
        query.prepare(querystr);
        query.bindValue(":CARDID",   cardid);
        query.bindValue(":HOSTNAME", gContext->GetHostName());

        if (query.exec() && query.isActive() && query.size() > 0)
        {
            query.next();
            id = query.value(0).toString();
        }
    }

    if (id == QString::null) 
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Couldn't find any channels in the database,\n"
                "\t\t\tplease make sure your inputs are associated\n"
                "\t\t\tproperly with your cards.");
        channum = "";
        chanid = "";
        return;
    }

    // Now let's try finding the next channel in the desired direction
    QString comp = ">";
    QString ordering = "";
    QString fromfavorites = "";
    QString wherefavorites = "";

    if (channeldirection == CHANNEL_DIRECTION_DOWN)
    {
        comp = "<";
        ordering = " DESC ";
    }
    else if (channeldirection == CHANNEL_DIRECTION_FAVORITE)
    {
        fromfavorites = ",favorites";
        wherefavorites = "AND favorites.chanid = channel.chanid";
    }
    else if (channeldirection == CHANNEL_DIRECTION_SAME)
    {
        comp = "=";
    }

    QString wherepart = QString(
        "cardinput.cardid     = capturecard.cardid AND "
        "capturecard.cardid   = :CARDID            AND "
        "capturecard.hostname = :HOSTNAME          AND "
        "channel.visible      = 1                  AND "
        "cardinput.sourceid = channel.sourceid ");

    querystr = QString(
        "SELECT channel.channum, channel.chanid "
        "FROM channel, capturecard, cardinput%1 "
        "WHERE channel.%2 %3 :ID %4 AND "
        "      %5 "
        "ORDER BY channel.%6 %7 "
        "LIMIT 1")
        .arg(fromfavorites).arg(channelorder)
        .arg(comp).arg(wherefavorites)
        .arg(wherepart).arg(channelorder).arg(ordering);

    query.prepare(querystr);
    query.bindValue(":CARDID",   cardid);
    query.bindValue(":HOSTNAME", gContext->GetHostName());
    query.bindValue(":ID",       id);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("getnextchannel", query);
    }
    else if (query.next())
    {
        channum = query.value(0).toString();
        chanid = query.value(1).toString();
    }
    else
    {
        // Couldn't find the channel going in the desired direction, 
        // so loop around and find it on the flip side...
        comp = "<";
        if (channeldirection == CHANNEL_DIRECTION_DOWN) 
            comp = ">";

        // again, %9 is the limit for this
        querystr = QString(
            "SELECT channel.channum, channel.chanid "
            "FROM channel, capturecard, cardinput%1 "
            "WHERE channel.%2 %3 :ID %4 AND "
            "      %5 "
            "ORDER BY channel.%6 %7 "
            "LIMIT 1")
            .arg(fromfavorites).arg(channelorder)
            .arg(comp).arg(wherefavorites)
            .arg(wherepart).arg(channelorder).arg(ordering);

        query.prepare(querystr);
        query.bindValue(":CARDID",   cardid);
        query.bindValue(":HOSTNAME", gContext->GetHostName());
        query.bindValue(":ID",       id); 

        if (!query.exec() || !query.isActive())
        {
            MythContext::DBError("getnextchannel", query);
        }
        else if (query.next())
        {
            channum = query.value(0).toString();
            chanid = query.value(1).toString();
        }
        else
        {
            VERBOSE(VB_IMPORTANT, "getnextchannel, query failed: "<<querystr);
            chanid = id; // just stay on same channel
        }
    }
}

/** \fn TVRec::IsReallyRecording()
 *  \brief Returns true if frontend can consider the recorder started.
 *  \sa IsRecording()
 */
bool TVRec::IsReallyRecording(void)
{
    return ((recorder && recorder->IsRecording()) ||
            (dummyRecorder && dummyRecorder->IsRecording()));
}

/** \fn TVRec::IsBusy()
 *  \brief Returns true if the recorder is busy, or will be within
 *         the next 5 seconds.
 *  \sa EncoderLink::IsBusy(), 
 */
bool TVRec::IsBusy(void)
{
    QMutexLocker lock(&stateChangeLock);

    bool retval = (GetState() != kState_None);

    if (pendingRecording)
    {
        int timeLeft = QDateTime::currentDateTime().secsTo(recordPendingStart);
        retval |= (timeLeft <= 5);
    }

    return retval;
}

/** \fn TVRec::GetFramerate()
 *  \brief Returns recordering frame rate from the recorder.
 *  \sa RemoteEncoder::GetFrameRate(), EncoderLink::GetFramerate(void),
 *      RecorderBase::GetFrameRate()
 *  \return Frames per second if query succeeds -1 otherwise.
 */
float TVRec::GetFramerate(void)
{
    QMutexLocker lock(&stateChangeLock);

    if (recorder)
        return recorder->GetFrameRate();
    return -1.0f;
}

/** \fn TVRec::GetFramesWritten()
 *  \brief Returns number of frames written to disk by recorder.
 *
 *  \sa EncoderLink::GetFramesWritten(), RemoteEncoder::GetFramesWritten()
 *  \return Number of frames if query succeeds, -1 otherwise.
 */
long long TVRec::GetFramesWritten(void)
{
    QMutexLocker lock(&stateChangeLock);

    if (recorder)
        return recorder->GetFramesWritten();
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
    QMutexLocker lock(&stateChangeLock);

    if (ringBuffer)
        return ringBuffer->GetWritePosition();
    return -1;
}

/** \fn TVRec::GetKeyframePosition(long long)
 *  \brief Returns byte position in RingBuffer of a keyframe according to recorder.
 *
 *  \sa EncoderLink::GetKeyframePosition(long long),
 *      RemoteEncoder::GetKeyframePosition(long long)
 *  \return Byte position of keyframe if query succeeds, -1 otherwise.
 */
long long TVRec::GetKeyframePosition(long long desired)
{
    QMutexLocker lock(&stateChangeLock);

    if (recorder)
        return recorder->GetKeyframePosition(desired);
    return -1;
}

/** \fn TVRec::GetMaxBitrate(void)
 *  \brief Returns the maximum bits per second this recorder can produce.
 *
 *  \sa EncoderLink::GetMaxBitrate(void), RemoteEncoder::GetMaxBitrate(void)
 */
long long TVRec::GetMaxBitrate(void)
{
    long long bitrate;
    if (genOpt.cardtype == "MPEG")
        bitrate = 10080000LL; // use DVD max bit rate
    else if (genOpt.cardtype == "HDTV")
        bitrate = 19400000LL; // 1080i
    else if (genOpt.cardtype == "FIREWIRE")
        bitrate = 19400000LL; // 1080i
    else if (genOpt.cardtype == "DVB")
        bitrate = 19400000LL; // 1080i
    else // frame grabber
        bitrate = 10080000LL; // use DVD max bit rate, probably too big

    return bitrate;
}

/** \fn TVRec::SpawnLiveTV(LiveTVChain*,bool)
 *  \brief Tells TVRec to spawn a "Live TV" recorder.
 *  \sa EncoderLink::SpawnLiveTV(LiveTVChain*,bool),
 *      RemoteEncoder::SpawnLiveTV(LiveTVChain*,bool)
 */
void TVRec::SpawnLiveTV(LiveTVChain *newchain, bool pip)
{
    QMutexLocker lock(&stateChangeLock);

    tvchain = newchain;
    tvchain->ReloadAll();

    QString hostprefix = QString("myth://%1:%2/")
                                .arg(gContext->GetSetting("BackendServerIP"))
                                .arg(gContext->GetSetting("BackendServerPort"));

    tvchain->SetHostPrefix(hostprefix);
    tvchain->SetCardType(genOpt.cardtype);

    ispip = pip;

    // Change to WatchingLiveTV
    ChangeState(kState_WatchingLiveTV);
    // Wait for state change to take effect
    WaitForEventThreadSleep();

    // Make sure StartRecording can't steal our tuner
    SetFlags(kFlagCancelNextRecording);
}

/** \fn TVRec::GetChainID()
 *  \brief Get the chainid of the livetv instance
 */
QString TVRec::GetChainID(void)
{
    if (tvchain)
        return tvchain->GetID();
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
    QMutexLocker lock(&stateChangeLock);

    if (internalState == kState_None)
        return; // already stopped

    ProgramInfo *pi = NULL;
    if (curRecording)
    {
        pi = ProgramInfo::GetProgramFromRecorded(
            curRecording->chanid, curRecording->recstartts);
    }

    if (pi && pi->recgroup != "LiveTV" && !pseudoLiveTVRecording)
    {
        // User wants this recording to continue
        SetPseudoLiveTVRecording(pi);
        pi = NULL;
    }
    else if (pi->recgroup == "LiveTV" && pseudoLiveTVRecording)
    {
        // User wants to abandon scheduled recording
        SetPseudoLiveTVRecording(NULL);
    }

    if (pi)
        delete pi;
}

static uint get_input_id(uint cardid, const QString &inputname)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(
        "SELECT cardinputid "
        "FROM cardinput "
        "WHERE cardid    = :CARDID AND "
        "      inputname = :INNAME");

    query.bindValue(":CARDID", cardid);
    query.bindValue(":INNAME", inputname);

    if (!query.exec() || !query.isActive())
        MythContext::DBError("get_input_id", query);
    else if (query.next())
        return query.value(0).toUInt();

    return 0;
}

/** \fn TVRec::NotifySchedulerOfRecording(ProgramInfo*)
 *  \brief Tell scheduler about the recording.
 *
 *   This is needed if the frontend has marked the LiveTV
 *   buffer for recording after we exit LiveTV. In this case
 *   the scheduler needs to know about the recording so it
 *   can properly take overrecord into account, and to properly
 *   reschedule other recordings around to avoid this recording.
 */
void TVRec::NotifySchedulerOfRecording(ProgramInfo *rec)
{
    if (!channel)
        return;

    // Notify scheduler of the recording.
    // + set up recording so it can be resumed
    rec->cardid    = cardid;
    rec->inputid   = get_input_id(cardid, channel->GetCurrentInput());

    rec->rectype = rec->GetScheduledRecording()->getRecordingType();

    if(rec->rectype == kNotRecording)
    {
        rec->rectype = kSingleRecord;
        rec->GetScheduledRecording()->setRecordingType(kSingleRecord);
    }

    // + save rsInactive recstatus to so that a reschedule call
    //   doesn't start recording this on another card before we
    //   send the SCHEDULER_ADD_RECORDING message to the scheduler.
    rec->recstatus = rsInactive;
    rec->AddHistory(false);

    // + save ScheduledRecording so that we get a recordid
    //   (don't allow signalChange(), avoiding unneeded reschedule)
    rec->GetScheduledRecording()->save(false);

    // + save recordid to recorded entry
    rec->ApplyRecordRecID();

    // + set proper recstatus (saved later)
    rec->recstatus = rsRecording;

    // + pass proginfo to scheduler and reschedule
    QStringList prog;
    rec->ToStringList(prog);
    MythEvent me("SCHEDULER_ADD_RECORDING", prog);
    gContext->dispatch(me);

    // Allow scheduler to end this recording before post-roll,
    // if it has another recording for this recorder.
    ClearFlags(kFlagCancelNextRecording);
}

/** \fn TVRec::SetLiveRecording(int)
 *  \brief Tells the Scheduler about changes to the recording status
 *         of the LiveTV recording.
 *
 *   NOTE: Currently the 'recording' parameter is ignored and decisions
 *         are based on the recording group alone.
 *
 *  \param recording Set to 1 to mark as rsRecording, set to 0 to mark as
 *         rsCancelled, and set to -1 to base the decision of the recording
 *         group.
 */
void TVRec::SetLiveRecording(int recording)
{
    VERBOSE(VB_IMPORTANT, LOC + "SetLiveRecording("<<recording<<")");
    QMutexLocker locker(&stateChangeLock);

    (void) recording;

    RecStatusType recstat = rsCancelled;
    bool was_rec = pseudoLiveTVRecording;
    CheckForRecGroupChange();
    if (was_rec && !pseudoLiveTVRecording)
    {
        VERBOSE(VB_IMPORTANT, LOC + "SetLiveRecording() -- cancel");
        // cancel -- 'recording' should be 0 or -1
        SetFlags(kFlagCancelNextRecording);
    }
    else if (!was_rec && pseudoLiveTVRecording)
    {
        VERBOSE(VB_IMPORTANT, LOC + "SetLiveRecording() -- record");
        // record -- 'recording' should be 1 or -1

        // If the last recording was flagged for keeping
        // in the frontend, then add the recording rule
        // so that transcode, commfrag, etc can be run.
        recordEndTime = GetRecordEndTime(pseudoLiveTVRecording);
        NotifySchedulerOfRecording(curRecording);
        recstat = curRecording->recstatus;
    }

    MythEvent me(QString("UPDATE_RECORDING_STATUS %1 %2 %3 %4 %5")
                 .arg(curRecording->cardid)
                 .arg(curRecording->chanid)
                 .arg(curRecording->startts.toString(Qt::ISODate))
                 .arg(recstat)
                 .arg(curRecording->recendts.toString(Qt::ISODate)));

    gContext->dispatch(me);
}

/** \fn TVRec::StopLiveTV(void)
 *  \brief Tells TVRec to stop a "Live TV" recorder.
 *  \sa EncoderLink::StopLiveTV(), RemoteEncoder::StopLiveTV()
 */
void TVRec::StopLiveTV(void)
{
    QMutexLocker lock(&stateChangeLock);
    VERBOSE(VB_RECORD, "StopLiveTV(void) curRec: "<<curRecording
            <<" pseudoRec: "<<pseudoLiveTVRecording);

    if (internalState == kState_None)
        return; // already stopped

    bool hadPseudoLiveTVRec = pseudoLiveTVRecording;
    CheckForRecGroupChange();

    if (!hadPseudoLiveTVRec && pseudoLiveTVRecording)
        NotifySchedulerOfRecording(curRecording);

    // Figure out next state and if needed recording end time.
    TVState next_state = kState_None;
    if (pseudoLiveTVRecording)
    {
        recordEndTime = GetRecordEndTime(pseudoLiveTVRecording);
        next_state = kState_RecordingOnly;
    }

    // Change to the appropriate state
    ChangeState(next_state);

    // Wait for state change to take effect...
    WaitForEventThreadSleep();

    // We are done with the tvchain...
    tvchain = NULL;
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
    QMutexLocker lock(&stateChangeLock);

    if (!recorder)
    {
        VERBOSE(VB_IMPORTANT, LOC + "PauseRecorder() "
                "called with no recorder");
        return;
    }

    recorder->Pause();
} 

/** \fn TVRec::RecorderPaused(void)
 *  \brief This is a callback, called by the "recorder" instance when
 *         it has actually paused.
 *  \sa PauseRecorder(void)
 */
void TVRec::RecorderPaused(void)
{
    if (pauseNotify)
    {
        QMutexLocker lock(&stateChangeLock);
        triggerEventLoop.wakeAll();
    }
}

/** \fn TVRec::ToggleChannelFavorite()
 *  \brief Toggles whether the current channel should be on our favorites list.
 */
void TVRec::ToggleChannelFavorite(void)
{
    QMutexLocker lock(&stateChangeLock);

    if (!channel)
        return;

    // Get current channel id...
    QString channum = channel->GetCurrentName();

    int chanid = GetChannelValue("chanid", channel, channum);
    if (chanid < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + QString(
                "Channel: \'%1\' was not found in the database.\n"
                "\t\t\tMost likely, your DefaultTVChannel setting is wrong.\n"
                "\t\t\tCould not toggle favorite.").arg(channum));
        return;
    }

    // Check if favorite exists for that chanid...
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT favorites.favid "
        "FROM favorites "
        "WHERE favorites.chanid = :CHANID "
        "LIMIT 1");
    query.bindValue(":CHANID", chanid);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("togglechannelfavorite", query);
    }
    else if (query.size() > 0)
    {
        // We have a favorites record...Remove it to toggle...
        query.next();
        QString favid = query.value(0).toString();
        query.prepare(
            QString("DELETE FROM favorites "
                    "WHERE favid = '%1'").arg(favid));
        query.exec();
        VERBOSE(VB_RECORD, LOC + "Removing Favorite.");
    }
    else
    {
        // We have no favorites record...Add one to toggle...
        query.prepare(
            QString("INSERT INTO favorites (chanid) "
                    "VALUES ('%1')").arg(chanid));
        query.exec();
        VERBOSE(VB_RECORD, LOC + "Adding Favorite.");
    }
}

/** \fn TVRec::ChangeContrast(bool)
 *  \brief Changes contrast of a recording.
 *
 *  Note: In practice this only works with frame grabbing recorders.
 *
 *  \return contrast if it succeeds, -1 otherwise.
 */
int TVRec::ChangeContrast(bool direction)
{
    QMutexLocker lock(&stateChangeLock);

    if (!channel)
        return -1;

    int ret = channel->ChangeContrast(direction);
    return ret;
}

/** \fn TVRec::ChangeBrightness(bool)
 *  \brief Changes the brightness of a recording.
 *
 *  Note: In practice this only works with frame grabbing recorders.
 *
 *  \return brightness if it succeeds, -1 otherwise.
 */
int TVRec::ChangeBrightness(bool direction)
{
    QMutexLocker lock(&stateChangeLock);

    if (!channel)
        return -1;

    int ret = channel->ChangeBrightness(direction);
    return ret;
}

/** \fn TVRec::ChangeColour(bool)
 *  \brief Changes the colour phase of a recording.
 *
 *  Note: In practice this only works with frame grabbing recorders.
 *
 *  \return colour if it succeeds, -1 otherwise.
 */
int TVRec::ChangeColour(bool direction)
{
    QMutexLocker lock(&stateChangeLock);

    if (!channel)
        return -1;

    int ret = channel->ChangeColour(direction);
    return ret;
}

/** \fn TVRec::ChangeHue(bool)
 *  \brief Changes the hue of a recording.
 *
 *  Note: In practice this only works with frame grabbing recorders.
 *
 *  \return hue if it succeeds, -1 otherwise.
 */
int TVRec::ChangeHue(bool direction)
{
    QMutexLocker lock(&stateChangeLock);

    if (!channel)
        return -1;

    int ret = channel->ChangeHue(direction);
    return ret;
}

/** \fn TVRec::GetConnectedInputs(void) const
 *  \brief Returns TVRec's recorders connected inputs.
 */
QStringList TVRec::GetConnectedInputs(void) const
{
    QStringList list;
    if (channel)
        list = channel->GetConnectedInputs();
    return list;
}

/** \fn TVRec::GetInput(void) const
 *  \brief Returns current input.
 */
QString TVRec::GetInput(void) const
{
    if (channel)
        return channel->GetCurrentInput();
    return QString::null;
}

/** \fn TVRec::SetInput(QString, uint)
 *  \brief Changes to the specified input.
 *
 *   You must call PauseRecorder(void) before calling this.
 *
 *  \param input Input to switch to, or "SwitchToNectInput".
 *  \return input we have switched to
 */
QString TVRec::SetInput(QString input, uint requestType)
{
    QMutexLocker lock(&stateChangeLock);
    QString origIn = input;
    VERBOSE(VB_RECORD, LOC + "SetInput("<<input<<") -- begin");

    if (!channel)
    {
        VERBOSE(VB_RECORD, LOC + "SetInput() -- end  no channel class");
        return QString::null;
    }

    input = (input == "SwitchToNextInput") ? channel->GetNextInput() : input;

    if (input == channel->GetCurrentInput())
    {
        VERBOSE(VB_RECORD, LOC + "SetInput("<<origIn<<":"<<input
                <<") -- end  nothing to do");
        return input;
    }

    QString name = channel->GetNextInputStartChan();
    
    // Detect tuning request type if needed
    if (requestType & kFlagDetect)
    {
        WaitForEventThreadSleep();
        requestType = lastTuningRequest.flags & (kFlagRec | kFlagNoRec);
    }

    // Clear the RingBuffer reset flag, in case we wait for a reset below
    ClearFlags(kFlagRingBufferReady);

    // Actually add the tuning request to the queue, and
    // then wait for it to start tuning
    tuningRequests.enqueue(TuningRequest(requestType, name, input));
    WaitForEventThreadSleep();

    // If we are using a recorder, wait for a RingBuffer reset
    if (requestType & kFlagRec)
    {
        while (!HasFlags(kFlagRingBufferReady))
            WaitForEventThreadSleep();
    }
    VERBOSE(VB_RECORD, LOC + "SetInput("<<origIn<<":"<<input<<") -- end");

    return GetInput();
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
void TVRec::SetChannel(QString name, uint requestType)
{
    QMutexLocker lock(&stateChangeLock);
    VERBOSE(VB_RECORD, LOC + "SetChannel()" + " -- begin");

    // Detect tuning request type if needed
    if (requestType & kFlagDetect)
    {
        WaitForEventThreadSleep();
        requestType = lastTuningRequest.flags & (kFlagRec | kFlagNoRec);
    }

    // Clear the RingBuffer reset flag, in case we wait for a reset below
    ClearFlags(kFlagRingBufferReady);

    // Actually add the tuning request to the queue, and
    // then wait for it to start tuning
    tuningRequests.enqueue(TuningRequest(requestType, name));
    WaitForEventThreadSleep();

    // If we are using a recorder, wait for a RingBuffer reset
    if (requestType & kFlagRec)
    {
        while (!HasFlags(kFlagRingBufferReady))
            WaitForEventThreadSleep();
    }
    VERBOSE(VB_RECORD, LOC + "SetChannel()" + " -- end");
}

/** \fn TVRec::GetNextProgram(int,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&)
 *  \brief Returns information about the program that would be seen if we changed
 *         the channel using ChangeChannel(int) with "direction".
 *  \sa EncoderLink::GetNextProgram(int,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&),
 *      RemoteEncoder::GetNextProgram(int,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&)
 */
void TVRec::GetNextProgram(int direction,
                           QString &title,       QString &subtitle,
                           QString &desc,        QString &category,
                           QString &starttime,   QString &endtime,
                           QString &callsign,    QString &iconpath,
                           QString &channelname, QString &chanid,
                           QString &seriesid,    QString &programid)
{
    QString querystr;
    QString nextchannum = channelname;
    QString compare = "<";
    QString sortorder = "";


    querystr = QString(
        "SELECT title,     subtitle,       description,   category, "
        "       starttime, endtime,        callsign,      icon,     "
        "       channum,   program.chanid, seriesid,      programid "
        "FROM program, channel "
        "WHERE program.chanid = channel.chanid ");

    switch (direction)
    {
        case BROWSE_SAME:
                chanid = GetNextRelativeChanID(channelname,
                                               CHANNEL_DIRECTION_SAME);
                compare = "<=";
                sortorder = "desc";
                break;
        case BROWSE_UP:
                chanid = GetNextRelativeChanID(channelname,
                                               CHANNEL_DIRECTION_UP);
                compare = "<=";
                sortorder = "desc";
                break;
        case BROWSE_DOWN:
                chanid = GetNextRelativeChanID(channelname,
                                               CHANNEL_DIRECTION_DOWN);
                compare = "<=";
                sortorder = "desc";
                break;
        case BROWSE_LEFT:
                compare = "<";
                sortorder = "desc";
                break;
        case BROWSE_RIGHT:
                compare = ">";
                sortorder = "asc";
                break;
        case BROWSE_FAVORITE:
                chanid = GetNextRelativeChanID(channelname,
                                               CHANNEL_DIRECTION_FAVORITE);
                compare = "<=";
                sortorder = "desc";
                break;
    }

    querystr += QString(
        "AND channel.chanid = '%1' "
        "AND starttime %3 '%2' "
        "ORDER BY starttime %4 "
        "LIMIT 1")
        .arg(chanid).arg(starttime).arg(compare).arg(sortorder);

    MSqlQuery sqlquery(MSqlQuery::InitCon());

    sqlquery.prepare(querystr);

    if (sqlquery.exec() && sqlquery.isActive() && sqlquery.size() > 0)
    {
        if (sqlquery.next())
        {
            title = QString::fromUtf8(sqlquery.value(0).toString());
            subtitle = QString::fromUtf8(sqlquery.value(1).toString());
            desc = QString::fromUtf8(sqlquery.value(2).toString());
            category = QString::fromUtf8(sqlquery.value(3).toString());
            starttime =  sqlquery.value(4).toString();
            endtime = sqlquery.value(5).toString();
            callsign = sqlquery.value(6).toString();
            iconpath = sqlquery.value(7).toString();
            channelname = sqlquery.value(8).toString();
            chanid = sqlquery.value(9).toString();
            seriesid = sqlquery.value(10).toString();
            programid = sqlquery.value(11).toString();
        }
    }
    else
    {
        // Couldn't get program info, so get the channel info and clear 
        // everything else.
        starttime = "";
        endtime = "";
        title = "";
        subtitle = "";
        desc = "";
        category = "";
        seriesid = "";
        programid = "";

        querystr = QString(
            "SELECT channum, callsign, icon, chanid "
            "FROM channel "
            "WHERE chanid = %1").arg(chanid);
        sqlquery.prepare(querystr);

        if (sqlquery.exec() && sqlquery.isActive() && sqlquery.size() > 0 && 
            sqlquery.next())
        {
            channelname = sqlquery.value(0).toString();
            callsign = sqlquery.value(1).toString();
            iconpath = sqlquery.value(2).toString();
            chanid = sqlquery.value(3).toString();
        }
    }

}

/** \fn TVRec::SetRingBuffer(RingBuffer*)
 *  \brief Sets "ringBuffer", deleting any existing RingBuffer.
 */
void TVRec::SetRingBuffer(RingBuffer *rb)
{
    QMutexLocker lock(&stateChangeLock);

    RingBuffer *rb_old = ringBuffer;
    ringBuffer = rb;

    if (rb_old && (rb_old != rb))
    {
        if (dummyRecorder)
        {
            dummyRecorder->deleteLater();
            dummyRecorder = NULL;
            ClearFlags(kFlagDummyRecorderRunning);
        }
        delete rb_old;
    }
}

void TVRec::RingBufferChanged(RingBuffer *rb, ProgramInfo *pginfo)
{
    VERBOSE(VB_IMPORTANT, LOC + "RingBufferChanged()");

    SetRingBuffer(rb);

    if (pginfo)
    {
        if (curRecording)
        {
            FinishedRecording(curRecording);
            curRecording->MarkAsInUse(false);
            delete curRecording;
        }
        curRecording = new ProgramInfo(*pginfo);
        curRecording->MarkAsInUse(true, "recorder");
    }
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
    if (tuningRequests.size())
    {
        const TuningRequest *request = &tuningRequests.front();
        VERBOSE(VB_RECORD, LOC + "Request: "<<request->toString());
        TuningShutdowns(*request);

        // Now we start new stuff
        if (request->flags & (kFlagRecording|kFlagLiveTV|
                              kFlagEITScan|kFlagAntennaAdjust))
        {
            if (!recorder)
                TuningFrequency(*request);
            else
                SetFlags(kFlagWaitingForRecPause);
        }

        lastTuningRequest = tuningRequests.dequeue();
    }

    if (HasFlags(kFlagWaitingForRecPause))
    {
        if (!recorder->IsPaused())
            return;

        ClearFlags(kFlagWaitingForRecPause);
#ifdef USING_DVB
        if (GetDVBRecorder())
        {
            // We currently need to close the file descriptor for
            // DVB signal monitoring to work with some drivers
            GetDVBRecorder()->Close();
            GetDVBRecorder()->SetRingBuffer(NULL);
        }
#endif // USING_DVB
        TuningFrequency(lastTuningRequest);
    }

    if (HasFlags(kFlagWaitingForSignal))
    {
        if (!TuningSignalCheck())
            return;
    }

    if (HasFlags(kFlagWaitingForSIParser))
    {
        if (!TuningPMTCheck())
            return;
    }

    if (HasFlags(kFlagNeedToStartRecorder))
    { 
        if (recorder)
            TuningRestartRecorder();
        else
            TuningNewRecorder();

        // If we got this far it is safe to set a new starting channel...
        if (channel)
            channel->StoreInputChannels();
    }
}

/** \fn TVRec::TuningCheckForHWChange(const TuningRequest&,QString&,QString&)
 *  \brief Returns cardid for device info row in capturecard if it changes.
 */
uint TVRec::TuningCheckForHWChange(const TuningRequest &request,
                                   QString &channum,
                                   QString &inputname)
{
    if (!channel)
        return 0;

    uint curCardID = 0, newCardID = 0;
    channum   = request.channel;
    inputname = request.input;

    if (request.program)
        request.program->GetChannel(channum, inputname);

    if (!channum.isEmpty() && inputname.isEmpty())
        channel->CheckChannel(channum, inputname);

    if (!inputname.isEmpty())
    {
        int current_input = channel->GetCurrentInputNum();
        int new_input     = channel->GetInputByName(inputname);
        curCardID = channel->GetInputCardID(current_input);
        newCardID = channel->GetInputCardID(new_input);
        VERBOSE(VB_IMPORTANT, LOC<<"HW Tuner: "<<curCardID<<"->"<<newCardID);
    }

    if (curCardID != newCardID)
    {    
        if (channum.isEmpty())
            channum = GetStartChannel(newCardID, inputname);
        return newCardID;
    }

    return 0;
}

/** \fn TVRec::TuningShutdowns(const TuningRequest&)
 *  \brief This shuts down anything that needs to be shut down 
 *         before handling the passed in tuning request.
 */
void TVRec::TuningShutdowns(const TuningRequest &request)
{
    QString channum, inputname;
    uint newCardID = TuningCheckForHWChange(request, channum, inputname);

#ifdef USING_DVB_EIT
    if (!(request.flags & kFlagEITScan) && HasFlags(kFlagEITScannerRunning))
    {
        scanner->StopActiveScan();
        ClearFlags(kFlagEITScannerRunning);
    }
#endif // USING_DVB_EIT

#ifdef USING_DVB
    if (HasFlags(kFlagSIParserRunning))
    {
        TeardownSIParser();
        ClearFlags(kFlagSIParserRunning);
    }
    if (HasFlags(kFlagWaitingForSIParser))
        ClearFlags(kFlagWaitingForSIParser);
#endif // USING_DVB

    if (HasFlags(kFlagSignalMonitorRunning))
    {
        TeardownSignalMonitor();
        ClearFlags(kFlagSignalMonitorRunning);
    }
    if (HasFlags(kFlagWaitingForSignal))
        ClearFlags(kFlagWaitingForSignal);

    // At this point any waits are canceled.

    if (newCardID || (request.flags & kFlagNoRec))
    {
        if (HasFlags(kFlagDummyRecorderRunning))
        {
            dummyRecorder->StopRecordingThread();
            ClearFlags(kFlagDummyRecorderRunning);
            FinishedRecording(curRecording);
            curRecording->MarkAsInUse(false);
        }

        if (request.flags & kFlagCloseRec)
            FinishedRecording(lastTuningRequest.program);

        if (HasFlags(kFlagRecorderRunning))
        {
            stateChangeLock.unlock();
            TeardownRecorder(request.flags & kFlagKillRec);
            stateChangeLock.lock();
            ClearFlags(kFlagRecorderRunning);
        }
        // At this point the recorders are shut down

        CloseChannel();
        // At this point the channel is shut down
    }
    
    // handle HW change for digital/analog cards
    if (newCardID)
    {
        VERBOSE(VB_IMPORTANT, "Recreating channel...");
        channel->Close();
        delete channel;
        channel = NULL;

        GetDevices(newCardID, genOpt, dvbOpt, fwOpt, dboxOpt);
        genOpt.defaultinput = inputname;
        CreateChannel(channum);
        if (!(request.flags & kFlagNoRec))
            channel->Open();
    }

    if (ringBuffer && (request.flags & kFlagKillRingBuffer))
    {
        VERBOSE(VB_RECORD, LOC + "Tearing down RingBuffer");
        SetRingBuffer(NULL);
        // At this point the ringbuffer is shut down
    }

    // Clear pending actions from last request
    ClearFlags(kFlagPendingActions);
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
 *   an EIT scan so that the recorder is started or restarted a
 *   appropriate.
 */
void TVRec::TuningFrequency(const TuningRequest &request)
{
    QString channum, input;
    if (request.program)
        request.program->GetChannel(channum, input);
    else
    {
        channum = request.channel;
        input   = request.input;

        // If this is Live TV startup, we need a channel...
        if (channum.isEmpty() && (request.flags & kFlagLiveTV))
        {
            input   = genOpt.defaultinput;
            channum = GetStartChannel(cardid, input);
        }
    }
    bool ok = false;
    if (channel)
        channel->Open();
    else
        ok = true;

    if (channel && !channum.isEmpty())
    {
        if (channum.find("NextChannel") >= 0)
        {
            int dir = channum.right(channum.length() - 12).toInt();
            channum = GetNextChannel(channel, dir);
        }

        if (!input.isEmpty())
            ok = channel->SwitchToInput(input, channum);
        else
            ok = channel->SetChannelByString(channum);
    }

    if (!ok)
    {
        if (!(request.flags & kFlagLiveTV))
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("Failed to set channel to %1. "
                            "Reverting to kState_None")
                    .arg(channum));
            if (kState_None != internalState)
                ChangeState(kState_None);
            else
                tuningRequests.enqueue(TuningRequest(kFlagKillRec));
            return;
        }
        else
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("Failed to set channel to %1.").arg(channum));
        }
    }

    bool livetv = request.flags & kFlagLiveTV;
    bool antadj = request.flags & kFlagAntennaAdjust;
    bool use_sm = SignalMonitor::IsSupported(genOpt.cardtype);
    bool use_dr = use_sm && (livetv || antadj);
    if (use_dr)
    {
        // We need there to be a ringbuffer for these modes
        bool ok;
        ProgramInfo *tmp = pseudoLiveTVRecording;
        pseudoLiveTVRecording = NULL;
        if (!ringBuffer)
            ok = CreateLiveTVRingBuffer();
        else
            ok = SwitchLiveTVRingBuffer(true, false);
        pseudoLiveTVRecording = tmp;
        if (!ok)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to create RingBuffer 1");
            return;
        }

        // Start dummy recorder for devices capable of signal monitoring.
        bool is_atsc = genOpt.cardtype == "HDTV";
#if (DVB_API_VERSION_MINOR == 1)
        if (genOpt.cardtype == "DVB")
        {
            struct dvb_frontend_info info;
            int fd = channel->GetFd();
            if (fd >= 0 && (ioctl(fd, FE_GET_INFO, &info) >= 0))
                is_atsc = (info.type == FE_ATSC);
        }
#endif // if (DVB_API_VERSION_MINOR == 1)

        if (!is_atsc)
            dummyRecorder = new DummyDTVRecorder(
                this, true, ringBuffer, 768, 576, 50,
                90, 30000000, false);
        else
            dummyRecorder = new DummyDTVRecorder(
                this, true, ringBuffer, 1920, 1088, 29.97,
                90, 30000000, false);
        dummyRecorder->SetRecording(curRecording);
    }

    // Start signal monitoring for devices capable of monitoring
    if (use_sm)
    {
        VERBOSE(VB_RECORD, LOC + "Starting Signal Monitor");
        SetupSignalMonitor(!antadj, livetv | antadj);

        if (signalMonitor)
        {
            if (request.flags & kFlagEITScan)
            {
                GetDTVSignalMonitor()->GetStreamData()->
                    SetVideoStreamsRequired(0);
            }

            SetFlags(kFlagSignalMonitorRunning);
            ClearFlags(kFlagWaitingForSignal);
            if (!antadj)
                SetFlags(kFlagWaitingForSignal);
        }

        if (dummyRecorder && ringBuffer)
        {
            dummyRecorder->StartRecordingThread(5);
            SetFlags(kFlagDummyRecorderRunning);
            VERBOSE(VB_RECORD, "DummyDTVRecorder -- started");
            SetFlags(kFlagRingBufferReady);
        }
    }

    // Request a recorder, if the command is a recording command
    ClearFlags(kFlagNeedToStartRecorder);
    if (request.flags & kFlagRec && !antadj)
        SetFlags(kFlagNeedToStartRecorder);
}

/** \fn TVRec::TuningSignalCheck(void)
 *  \brief This checks if we have a channel lock.
 *
 *   If we have a channel lock this shuts down the signal monitoring.
 *
 *   If this is a DVB recorder the we start the SIParser and add a
 *   kFlagWaitingForSIParser flag to the tuning state.
 *
 *  \return true iff we have a complete lock
 */
bool TVRec::TuningSignalCheck(void)
{
    if (!signalMonitor->IsAllGood())
        return false;

    VERBOSE(VB_RECORD, LOC + "Got good signal");

    // grab useful data from DTV signal monitor before we kill it...
    int programNum = -1;
    ATSCStreamData *streamData = NULL;
    if (GetDTVSignalMonitor())
    {
        programNum = GetDTVSignalMonitor()->GetProgramNumber();
        streamData = GetDTVSignalMonitor()->GetATSCStreamData();
        VERBOSE(VB_RECORD, LOC + "MPEG program num("<<programNum<<")");
    }

    // shut down signal monitoring
    TeardownSignalMonitor();
    ClearFlags(kFlagWaitingForSignal | kFlagSignalMonitorRunning);

    // tell recorder/siparser about useful DTV monitor info..
#ifdef USING_DVB
    if (GetDVBRecorder())
        GetDVBRecorder()->SetStreamData(streamData);

    if (GetDVBChannel())
    {
        GetDVBChannel()->SetPMT(0, NULL);
        CreateSIParser(streamData, programNum);
        SetFlags(kFlagWaitingForSIParser | kFlagSIParserRunning);
    }
#endif // USING_DVB

#ifdef USING_V4L
    if (GetHDTVRecorder())
        GetHDTVRecorder()->SetStreamData(streamData);
#endif // USING_V4L

    return true;
}

/** \fn TVRec::TuningPMTCheck(void)
 *  \brief Clears the kFlagWaitingForSIParser flag
 *         when the SIParser has a PMT.
 *
 *  \return true if we have a PMT, or we don't need it.
 */
bool TVRec::TuningPMTCheck(void)
{
#ifdef USING_DVB
    if (!GetDVBChannel()->IsPMTSet())
        return false;
#endif // USING_DVB
    VERBOSE(VB_RECORD, LOC + "Got SIParser PMT");
    ClearFlags(kFlagWaitingForSIParser);
    return true;
}

static int init_jobs(const ProgramInfo *rec, RecordingProfile &profile,
                      bool on_host, bool transcode_bfr_comm, bool on_line_comm)
{
    if (!rec)
        return 0; // no jobs for Live TV recordings..

    int jobs = 0; // start with no jobs

    // grab standard jobs flags from program info
    JobQueue::AddJobsToMask(rec->GetAutoRunJobs(), jobs);

    // disable commercial flagging on PBS, BBC, etc.
    if (rec->chancommfree)
        JobQueue::RemoveJobsFromMask(JOB_COMMFLAG, jobs);

    // disable transcoding if the profile does not allow auto transcoding
    const Setting *autoTrans = profile.byName("autotranscode");
    if ((!autoTrans) || (autoTrans->getValue().toInt() == 0))
        JobQueue::RemoveJobsFromMask(JOB_TRANSCODE, jobs);

    // is commercial flagging enabled, and is on-line comm flagging enabled?
    bool rt = JobQueue::JobIsInMask(JOB_COMMFLAG, jobs) && on_line_comm;
    // also, we either need transcoding to be disabled or 
    // we need to be allowed to commercial flag before transcoding?
    rt &= JobQueue::JobIsNotInMask(JOB_TRANSCODE, jobs) ||
        !transcode_bfr_comm;
    if (rt)
    {
        // queue up real-time (i.e. on-line) commercial flagging.
        QString host = (on_host) ? gContext->GetHostName() : "";
        JobQueue::QueueJob(JOB_COMMFLAG,
                           rec->chanid, rec->recstartts, "", "",
                           host, JOB_LIVE_REC);

        // don't do regular comm flagging, we won't need it.
        JobQueue::RemoveJobsFromMask(JOB_COMMFLAG, jobs);
    }

    return jobs;
}

/** \fn TVRec::TuningNewRecorder(void)
 *  \brief Creates a recorder instance.
 */
void TVRec::TuningNewRecorder(void)
{
    VERBOSE(VB_RECORD, LOC + "Starting Recorder");

    bool had_dummyrec = false;
    if (HasFlags(kFlagDummyRecorderRunning))
    {
        dummyRecorder->StopRecordingThread();
        ClearFlags(kFlagDummyRecorderRunning);
        FinishedRecording(curRecording);
        curRecording->MarkAsInUse(false);
        had_dummyrec = true;
    }

    ProgramInfo *rec = lastTuningRequest.program;

    // Load the correct recording profile.
    // In LiveTV mode use "Live TV" profile, otherwise use the
    // recording's specified profile. If the desired profile can't
    // be found, fall back to the "Default" profile for card type.
    QString profileName = (tvchain) ? QString("Live TV") :
        rec->GetScheduledRecording()->getProfileName();
    RecordingProfile profile;
    if (!profile.loadByType(profileName, genOpt.cardtype))
    {
        profileName = "Default";
        profile.loadByType(profileName, genOpt.cardtype);
    }
    VERBOSE(VB_RECORD, LOC + QString("Using profile '%1' to record")
            .arg(profileName));

    if (tvchain)
    {
        bool ok;
        if (!ringBuffer)
        {
            ok = CreateLiveTVRingBuffer();
            SetFlags(kFlagRingBufferReady);
        }
        else
            ok = SwitchLiveTVRingBuffer(true, !had_dummyrec && recorder);
        if (!ok)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to create RingBuffer 2");
            goto err_ret;
        }
        rec = tvchain->GetProgramAt(-1);
    }

    if (lastTuningRequest.flags & kFlagRecording)
    {
        SetRingBuffer(new RingBuffer(rec->GetFileName(), true));
        if (!ringBuffer->IsOpen())
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("RingBuffer '%1' not open...")
                    .arg(rec->GetFileName()));
            SetRingBuffer(NULL);
            ClearFlags(kFlagPendingActions);
            goto err_ret;
        }
    }

    if (!ringBuffer)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + QString(
                    "Failed to start recorder!  ringBuffer is NULL\n"
                    "\t\t\t\t  Tuning request was %1\n")
                .arg(lastTuningRequest.toString()));

        if (HasFlags(kFlagLiveTV))
        {
            QString message = QString("QUIT_LIVETV %1").arg(cardid);
            MythEvent me(message);
            gContext->dispatch(me);
        }
        goto err_ret;
    }

    if (channel && genOpt.cardtype == "MJPEG")
        channel->Close(); // Needed because of NVR::MJPEGInit()

    if (!SetupRecorder(profile))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + QString(
                    "Failed to start recorder!\n"
                    "\t\t\t\t  Tuning request was %1\n")
                .arg(lastTuningRequest.toString()));

        if (HasFlags(kFlagLiveTV))
        {
            QString message = QString("QUIT_LIVETV %1").arg(cardid);
            MythEvent me(message);
            gContext->dispatch(me);
        }
        TeardownRecorder(true);
        goto err_ret;
    }

    if (channel && genOpt.cardtype == "MJPEG")
        channel->Open(); // Needed because of NVR::MJPEGInit()

    if (rec)
        recorder->SetRecording(rec);

    // Setup for framebuffer capture devices..
    if (channel)
        SetVideoFiltersForChannel(channel, channel->GetCurrentName());

#ifdef USING_V4L 
    if (GetV4LChannel())
    {
        channel->SetBrightness();
        channel->SetContrast();
        channel->SetColour();
        channel->SetHue();
        CloseChannel();
    }
#endif

    pthread_create(&recorder_thread, NULL, TVRec::RecorderThread, recorder);

    // Wait for recorder to start.
    stateChangeLock.unlock();
    while (!recorder->IsRecording() && !recorder->IsErrored())
        usleep(5 * 1000);
    stateChangeLock.lock();

    if (GetV4LChannel())
        channel->SetFd(recorder->GetVideoFd());

    SetFlags(kFlagRecorderRunning | kFlagRingBufferReady);

    if (!tvchain)
        autoRunJobs = init_jobs(rec, profile, runJobOnHostOnly,
                                transcodeFirst, earlyCommFlag);

    ClearFlags(kFlagNeedToStartRecorder);
    if (tvchain)
        delete rec;
    return;

  err_ret:
    ChangeState(kState_None);
    if (tvchain)
        delete rec;
}

/** \fn TVRec::TuningRestartRecorder(void)
 *  \brief Restarts a stopped recorder or unpauses a paused recorder.
 */
void TVRec::TuningRestartRecorder(void)
{
    VERBOSE(VB_RECORD, LOC + "Restarting Recorder");

    bool had_dummyrec = false;
    if (HasFlags(kFlagDummyRecorderRunning))
    {
        dummyRecorder->StopRecordingThread();
        ClearFlags(kFlagDummyRecorderRunning);
        had_dummyrec = true;
    }

    if (curRecording)
    {
        FinishedRecording(curRecording);
        curRecording->MarkAsInUse(false);
    }

    SwitchLiveTVRingBuffer(true, !had_dummyrec);

    recorder->Reset();
    if (had_dummyrec)
    {
        recorder->SetRingBuffer(ringBuffer);
        ProgramInfo *progInfo = tvchain->GetProgramAt(-1);
        recorder->SetRecording(progInfo);
        delete progInfo;
    }

#ifdef USING_DVB
    if (GetDVBRecorder())
    {
        pauseNotify = false;
        GetDVBRecorder()->Close();
        pauseNotify = true;
        GetDVBRecorder()->Open();
    }
#endif // USING_DVB

    // Set file descriptor of channel from recorder for V4L
    channel->SetFd(recorder->GetVideoFd());

    // Some recorders unpause on Reset, others do not...
    recorder->Unpause();

    if (pseudoLiveTVRecording)
    {
        ProgramInfo *rcinfo1 = pseudoLiveTVRecording; 
        QString msg1 = QString("Recording: %1 %2 %3 %4")
            .arg(rcinfo1->title).arg(rcinfo1->chanid)
            .arg(rcinfo1->recstartts.toString())
            .arg(rcinfo1->recendts.toString());
        ProgramInfo *rcinfo2 = tvchain->GetProgramAt(-1);
        QString msg2 = QString("Recording: %1 %2 %3 %4")
            .arg(rcinfo2->title).arg(rcinfo2->chanid)
            .arg(rcinfo2->recstartts.toString())
            .arg(rcinfo2->recendts.toString());
        delete rcinfo2;
        VERBOSE(VB_RECORD, LOC + "Pseudo LiveTV recording starting." +
                "\n\t\t\t" + msg1 + "\n\t\t\t" + msg2);

        int autoexpiredef = gContext->GetNumSetting("AutoExpireDefault", 0);
        curRecording->SetAutoExpire(autoexpiredef);
        curRecording->ApplyRecordRecGroupChange("Default");
    }

    ClearFlags(kFlagNeedToStartRecorder);
}

void TVRec::SetFlags(uint f)
{
    QMutexLocker lock(&stateChangeLock);
    stateFlags |= f;
    VERBOSE(VB_RECORD, LOC + QString("SetFlags(%1) -> %2")
            .arg(FlagToString(f)).arg(FlagToString(stateFlags)));
    triggerEventLoop.wakeAll();
}

void TVRec::ClearFlags(uint f)
{
    QMutexLocker lock(&stateChangeLock);
    stateFlags &= ~f;
    VERBOSE(VB_RECORD, LOC + QString("ClearFlags(%1) -> %2")
            .arg(FlagToString(f)).arg(FlagToString(stateFlags)));
    triggerEventLoop.wakeAll();
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
    if (kFlagAskAllowRecording & f)
        msg += "AskAllowRecording,";

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
        if (kFlagWaitingForSIParser & f)
            msg += "WaitingForSIParser,";
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
        if (kFlagSIParserRunning & f)
            msg += "SIParserRunning,";
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

bool TVRec::GetProgramRingBufferForLiveTV(ProgramInfo **pginfo,
                                          RingBuffer **rb)
{
    VERBOSE(VB_RECORD, LOC + "GetProgramRingBufferForLiveTV()");
    if (!channel || !tvchain || !pginfo || !rb)
        return false;

    QString channum = channel->GetCurrentName();
    int chanid = GetChannelValue("chanid", channel, channum);

    if (chanid < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + QString(
                "Channel: \'%1\' was not found in the database.\n"
                "\t\t\tMost likely, your DefaultTVChannel setting is wrong.\n"
                "\t\t\tCould not start livetv.").arg(channum));
        return false;
    }

    QString chanids = QString::number(chanid);

    ProgramInfo *prog = NULL;
    if (pseudoLiveTVRecording)
        prog = new ProgramInfo(*pseudoLiveTVRecording);
    else
        prog = ProgramInfo::GetProgramAtDateTime(
            chanids, mythCurrentDateTime(), true);

    if (prog->recstartts == prog->recendts)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "GetProgramRingBufferForLiveTV()"
                "\n\t\t\tProgramInfo is invalid."
                "\n" + prog->toString());
        prog->endts = prog->recendts = prog->recstartts.addSecs(3600);
    }

    prog->recstartts = mythCurrentDateTime();
    StartedRecording(prog);

    *rb = new RingBuffer(prog->GetFileName(), true);
    if (!(*rb)->IsOpen())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("RingBuffer '%1' not open...")
                .arg(prog->GetFileName()));

        delete *rb;
        delete prog;

        return false;
    }

    *pginfo = prog;
    return true;
}

bool TVRec::CreateLiveTVRingBuffer(void)
{
    VERBOSE(VB_RECORD, LOC + "CreateLiveTVRingBuffer()");
    ProgramInfo *pginfo = NULL;
    RingBuffer *rb = NULL;

    if (!GetProgramRingBufferForLiveTV(&pginfo, &rb))
    {
        ClearFlags(kFlagPendingActions);
        ChangeState(kState_None);
        VERBOSE(VB_IMPORTANT, LOC_ERR + "CreateLiveTVRingBuffer() failed");
        return false;
    }

    SetRingBuffer(rb);

    pginfo->SetAutoExpire(kLiveTVAutoExpire);
    pginfo->ApplyRecordRecGroupChange("LiveTV");

    bool discont = (tvchain->TotalSize() > 0);
    tvchain->AppendNewProgram(pginfo, channel->GetCurrentName(),
                              channel->GetCurrentInput(), discont);

    if (curRecording)
    {
        curRecording->MarkAsInUse(false);
        delete curRecording;
    }

    curRecording = pginfo;
    curRecording->MarkAsInUse(true, "recorder");

    return true;
}

bool TVRec::SwitchLiveTVRingBuffer(bool discont, bool set_rec)
{
    VERBOSE(VB_RECORD, LOC + "SwitchLiveTVRingBuffer(discont "
            <<discont<<", set_rec "<<set_rec<<")");

    ProgramInfo *pginfo = NULL;
    RingBuffer *rb = NULL;

    if (!GetProgramRingBufferForLiveTV(&pginfo, &rb))
    {
        ChangeState(kState_None);
        return false;
    }

    ProgramInfo *oldinfo = tvchain->GetProgramAt(-1);
    if (oldinfo)
    {
        FinishedRecording(oldinfo);
        (new PreviewGenerator(oldinfo, true))->Start();
        delete oldinfo;
    }

    pginfo->SetAutoExpire(kLiveTVAutoExpire);
    pginfo->ApplyRecordRecGroupChange("LiveTV");
    tvchain->AppendNewProgram(pginfo, channel->GetCurrentName(),
                              channel->GetCurrentInput(), discont);

    if (set_rec && recorder)
    {
        recorder->SetNextRecording(pginfo, rb);
        if (discont)
            recorder->CheckForRingBufferSwitch();
        delete pginfo;
        SetFlags(kFlagRingBufferReady);
    }
    else if (!set_rec)
    {
        if (curRecording)
            delete curRecording;
        curRecording = pginfo;
        SetRingBuffer(rb);
    }

    return true;
}

QString TuningRequest::toString(void) const
{
    return QString("Program(%1) channel(%2) input(%3) flags(%4)")
        .arg((program != 0) ? "yes" : "no").arg(channel).arg(input)
        .arg(TVRec::FlagToString(flags));
}
