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
#include "videosource.h"
#include "RingBuffer.h"

#include "atscstreamdata.h"
#include "hdtvrecorder.h"
#include "atsctables.h"

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
#include "firewirerecorder.h"
#include "firewirechannel.h"
#endif

#ifdef USING_DBOX2
#include "dbox2recorder.h"
#include "dbox2channel.h"
#endif

#define LOC QString("TVRec(%1): ").arg(cardid)
#define LOC_ERR QString("TVRec(%1) Error: ").arg(cardid)

/** Milliseconds to wait for whole amount of data requested to be
 *  sent in RequestBlock if we have not hit the end of the file.
 */
const uint TVRec::kStreamedFileReadTimeout = 100; /* msec */
/** The request buffer should be big enough to make disk reads
 *  efficient, but small enough so that we don't block too long
 *  waiting for the disk read in LiveTV mode.
 */
const uint TVRec::kRequestBufferSize = 256*1024; /* 256 KB */

/// How many seconds after entering kState_None should we start EIT Scanner
const uint TVRec::kEITScanStartTimeout = 60; /* 1 minute */

/// How many seconds should EIT Scanner spend on each transport
const uint TVRec::kEITScanTransportTimeout = 5*60; /* 5 minutes */

/// How many milliseconds the signal monitor should wait between checks
const uint TVRec::kSignalMonitoringRate = 50; /* msec */

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
      liveTVRingBufSize(0), liveTVRingBufFill(0), liveTVRingBufLoc(""),
      recprefix(""),
      // Configuration variables from setup rutines
      cardid(capturecardnum), ispip(false),
      // State variables
      stateChangeLock(true),
      internalState(kState_None), desiredNextState(kState_None),
      changeState(false), stateFlags(0), lastTuningRequest(0),
      // Current recording info
      curRecording(NULL), autoRunJobs(JOB_NONE),
      inoverrecord(false), overrecordseconds(0),
      // Pending recording info
      pendingRecording(NULL),
      // RingBuffer info
      ringBuffer(NULL), rbFileName(""), rbFileExt("mpg"),
      rbStreamingSock(NULL), rbStreamingBuffer(NULL), rbStreamingLive(false)
{
}

/** \fn TVRec::Init()
 *  \brief Performs instance initialization, returns true on success.
 *
 *  \return Returns true on success, false on failure.
 */
bool TVRec::Init(void)
{
    QMutexLocker lock(&stateChangeLock);

    bool ok = GetDevices(cardid, genOpt, dvbOpt, fwOpt, dboxOpt);

    if (!ok)
        return false;

    QString startchannel = GetStartChannel(cardid, genOpt.defaultinput);

    bool init_run = false;
    if (genOpt.cardtype == "DVB")
    {
#ifdef USING_DVB_EIT
        scanner = new EITScanner();
#endif // USING_DVB_EIT

#ifdef USING_DVB
        channel = new DVBChannel(genOpt.videodev.toInt(), this);
        channel->Open();
        InitChannel(genOpt.defaultinput, startchannel);
        connect(GetDVBChannel(), SIGNAL(UpdatePMTObject(const PMTObject*)),
                this, SLOT(SetPMTObject(const PMTObject*)));
        CloseChannel(); // Close the channel if in dvb_on_demand mode
        init_run = true;
#endif
    }
    else if (genOpt.cardtype == "FIREWIRE")
    {
#ifdef USING_FIREWIRE
        channel = new FirewireChannel(fwOpt, this);
        channel->Open();
        InitChannel(genOpt.defaultinput, startchannel);
        init_run = true;
#endif
    }
    else if (genOpt.cardtype == "DBOX2")
    {
#ifdef USING_DBOX2
        channel = new DBox2Channel(this, &dboxOpt, cardid);
        channel->Open();
        InitChannel(genOpt.defaultinput, startchannel);
        init_run = true;
#endif
    }
    else if (genOpt.cardtype == "MPEG" &&
             genOpt.videodev.lower().left(5) == "file:")
    {
        // No need to initialize channel..
        init_run = true;
    }
    else // "V4L" or "MPEG", ie, analog TV, or "HDTV"
    {
#ifdef USING_V4L
        channel = new Channel(this, genOpt.videodev);
        channel->Open();
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

    transcodeFirst    =
        gContext->GetNumSetting("AutoTranscodeBeforeAutoCommflag", 0);
    earlyCommFlag     = gContext->GetNumSetting("AutoCommflagWhileRecording", 0);
    runJobOnHostOnly  = gContext->GetNumSetting("JobsRunOnRecordHost", 0);
    audioSampleRateDB = gContext->GetNumSetting("AudioSampleRate");
    overRecordSecNrml = gContext->GetNumSetting("RecordOverTime");
    overRecordSecCat  = gContext->GetNumSetting("CategoryOverTime") * 60;
    overRecordCategory= gContext->GetSetting("OverTimeCategory");
    liveTVRingBufSize = gContext->GetNumSetting("BufferSize", 5);
    liveTVRingBufFill = gContext->GetNumSetting("MaxBufferFill", 50);
    liveTVRingBufLoc  = gContext->GetSetting("LiveBufferDir");
    recprefix         = gContext->GetSetting("RecordFilePrefix");

    rbStreamingBuffer = new char[kRequestBufferSize+64];
    if (!rbStreamingBuffer)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to allocate streaming buffer");
        return false;
    }

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
        scanner->deleteLater();
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
    if (rbStreamingBuffer)
        delete[] rbStreamingBuffer;
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
    QMutexLocker lock(&stateChangeLock);
    QString msg("");

    RecStatusType retval = rsAborted;

    // Flush out any pending state changes
    WaitForEventThreadSleep();

    // We need to do this check early so we don't cancel an overrecord
    // that we're trying to extend.
    if (curRecording &&
        curRecording->title == rcinfo->title &&
        curRecording->chanid == rcinfo->chanid &&
        curRecording->startts == rcinfo->startts)
    {
        curRecording->rectype = rcinfo->rectype;
        curRecording->recordid = rcinfo->recordid;
        curRecording->recendts = rcinfo->recendts;
        curRecording->UpdateRecordingEnd();
        MythEvent me("RECORDING_LIST_CHANGE");
        gContext->dispatch(me);

        recordEndTime = curRecording->recendts;
        inoverrecord = (recordEndTime < QDateTime::currentDateTime());

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

    if (inoverrecord)
        ChangeState(kState_None);

    // Flush out events...
    WaitForEventThreadSleep();
    inoverrecord = false;

    if (internalState == kState_WatchingLiveTV && 
        !HasFlags(kFlagCancelNextRecording))
    {
        QString message = QString("QUIT_LIVETV %1").arg(cardid);
        MythEvent me(message);
        gContext->dispatch(me);

        MythTimer timer;
        timer.start();

        // Wait at least 10 seconds for response
        while ((internalState != kState_None) && timer.elapsed() < 10000)
            WaitForEventThreadSleep(false, max(10000 - timer.elapsed(), 100));

        // Try again
        if (internalState != kState_None)
        {
            gContext->dispatch(me);

            timer.restart();
            // Wait at least 10 seconds for response
            while ((internalState != kState_None) && timer.elapsed() < 10000)
                WaitForEventThreadSleep(
                    false, max(10000 - timer.elapsed(), 100));
        }

        // Try harder
        if (internalState != kState_None)
        {
            SetFlags(kFlagExitPlayer);
            WaitForEventThreadSleep();
        }
    }

    if (internalState == kState_None)
    {
        QString rbBaseName = rcinfo->CreateRecordBasename(rbFileExt);
        rbFileName = QString("%1/%2").arg(recprefix).arg(rbBaseName);
        recordEndTime = rcinfo->recendts;
        curRecording = new ProgramInfo(*rcinfo);

        overrecordseconds = overRecordSecNrml;
        if (curRecording->category == overRecordCategory)
        {
            overrecordseconds = overRecordSecCat;
            msg = QString("Show category '%1', desired postroll %2")
                .arg(curRecording->category).arg(overrecordseconds);
            VERBOSE(VB_RECORD, LOC + msg);
        }
        
        ChangeState(kState_RecordingOnly);
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
        if (curRecording)
            msg += QString("\t\t\tCurrently recording: %1 %2 %3 %4")
                .arg(curRecording->title).arg(curRecording->chanid)
                .arg(curRecording->recstartts.toString())
                .arg(curRecording->recendts.toString());
        VERBOSE(VB_IMPORTANT, LOC + msg);

        retval = rsTunerBusy;
    }

    ClearFlags(kFlagCancelNextRecording);

    WaitForEventThreadSleep();

    return retval;
}

/** \fn TVRec::StopRecording()
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

/** \fn TVRec::StartedRecording(ProgramInfo *curRecording)
 *  \brief Inserts a "curRecording" into the database, and issues a
 *         "RECORDING_LIST_CHANGE" event.
 *  \param curRecording Recording to add to database.
 *  \sa ProgramInfo::StartedRecording(const QString&)
 */
void TVRec::StartedRecording(ProgramInfo *curRecording)
{
    if (!curRecording)
        return;

    QString rbBaseName = rbFileName.section("/", -1);
    curRecording->StartedRecording(rbBaseName);

    if (curRecording->chancommfree != 0)
        curRecording->SetCommFlagged(COMM_FLAG_COMMFREE);

    MythEvent me("RECORDING_LIST_CHANGE");
    gContext->dispatch(me);
}

/** \fn TVRec::FinishedRecording(ProgramInfo *curRecording)
 *  \brief If not a premature stop, adds program to history of recorded 
 *         programs. If the recording type is kFindOneRecord this find
 *         is removed.
 *  \sa ProgramInfo::FinishedRecording(bool prematurestop)
 *  \param prematurestop If true, we only fetch the recording status.
 */
void TVRec::FinishedRecording(ProgramInfo *curRecording)
{
    if (!curRecording)
        return;

    curRecording->recstatus = rsRecorded;
    curRecording->recendts = QDateTime::currentDateTime().addSecs(30);
    curRecording->recendts.setTime(QTime(
        curRecording->recendts.time().hour(),
        curRecording->recendts.time().minute()));

    MythEvent me(QString("UPDATE_RECORDING_STATUS %1 %2 %3 %4 %5")
                 .arg(curRecording->cardid)
                 .arg(curRecording->chanid)
                 .arg(curRecording->startts.toString(Qt::ISODate))
                 .arg(curRecording->recstatus)
                 .arg(curRecording->recendts.toString(Qt::ISODate)));
    gContext->dispatch(me);

    curRecording->FinishedRecording(false);
}

#define TRANSITION(ASTATE,BSTATE) \
   ((internalState == ASTATE) && (desiredNextState == BSTATE))
#define SET_NEXT() do { nextState = desiredNextState; changed = true; } while(0)
#define SET_LAST() do { nextState = internalState; changed = true; } while(0)

/** \fn TVRec::HandleStateChange()
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
        tuningRequests.enqueue(TuningRequest(kFlagKillRec));
        SET_NEXT();
    }
    else if (TRANSITION(kState_None, kState_RecordingOnly))
    {
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
        eitScanStartTime = eitScanStartTime.addSecs(kEITScanStartTimeout);
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
 *  \param "name" Profile/RecorderBase option to get/set.
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
        recorder = new FirewireRecorder(this);
        recorder->SetOption("port",       fwOpt.port);
        recorder->SetOption("node",       fwOpt.node);
        recorder->SetOption("speed",      fwOpt.speed);
        recorder->SetOption("model",      fwOpt.model);
        recorder->SetOption("connection", fwOpt.connection);
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
        recorder->SetOption("wait_for_seqstart", genOpt.wait_for_seqstart);
        recorder->SetOption("hw_decoder",        dvbOpt.hw_decoder);
        recorder->SetOption("recordts",          dvbOpt.recordts);
        recorder->SetOption("dmx_buf_size",      dvbOpt.dmx_buf_size);
        recorder->SetOption("pkt_buf_size",      dvbOpt.pkt_buf_size);
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
        recorder->SetRingBuffer(ringBuffer);
        recorder->SetOptionsFromProfile(
            &profile, genOpt.videodev, genOpt.audiodev, genOpt.vbidev, ispip);
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

    if (killFile)
    {
        if (!rbFileName.isEmpty())
            unlink(rbFileName.ascii());
        rbFileName = "";
    }

    if (curRecording)
    {
        if (autoRunJobs && !killFile)
            JobQueue::QueueRecordingJobs(curRecording, autoRunJobs);

        delete curRecording;
        curRecording = NULL;
    }

    MythEvent me("RECORDING_LIST_CHANGE");
    gContext->dispatch(me);
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

/** \fn TVRec::GetScreenGrab(const ProgramInfo*,const QString&,int,int&,int&,int&)
 *  \brief Returns a PIX_FMT_RGBA32 buffer containg a frame from the video.
 *
 *  \param pginfo       Recording to grab from.
 *  \param filename     File containing recording.
 *  \param secondsin    Seconds into the video to seek before capturing a frame.
 *  \param bufferlen    Returns size of buffer returned (in bytes).
 *  \param video_width  Returns width of frame grabbed.
 *  \param video_height Returns height of frame grabbed.
 *  \return Buffer allocated with new containing frame in RGBA32 format if
 *          successful, NULL otherwise.
 */
char *TVRec::GetScreenGrab(const ProgramInfo *pginfo, const QString &filename,
                           int secondsin, int &bufferlen,
                           int &video_width, int &video_height,
                           float &video_aspect)
{
    (void) pginfo;
    (void) filename;
    (void) secondsin;
    (void) bufferlen;
    (void) video_width;
    (void) video_height;
#ifdef USING_FRONTEND
    RingBuffer *tmprbuf = new RingBuffer(filename, false);

    if (!MSqlQuery::testDBConnection())
        return NULL;

    NuppelVideoPlayer *nupvidplay =
        new NuppelVideoPlayer(pginfo);
    nupvidplay->SetRingBuffer(tmprbuf);
    nupvidplay->SetAudioSampleRate(audioSampleRateDB);

    char *retbuf = nupvidplay->GetScreenGrab(secondsin, bufferlen, video_width,
                                             video_height, video_aspect);

    delete nupvidplay;
    delete tmprbuf;

    return retbuf;
#else // USING_FRONTEND
    QString msg = "Backend compiled without USING_FRONTEND !!!!";
    VERBOSE(VB_IMPORTANT, LOC_ERR + msg);
    return NULL;
#endif // USING_FRONTEND
}

void TVRec::CreateSIParser(int program_num)
{
    (void) program_num;
#ifdef USING_DVB
    DVBChannel *dvbc = GetDVBChannel();
    if (dvbc)
    {
        int service_id = dvbc->GetServiceID();

        VERBOSE(VB_RECORD, LOC + QString("prog_num(%1) vs. dvbc->srv_id(%2)")
                .arg(program_num).arg(service_id));

        if (!dvbsiparser)
        {
            dvbsiparser = new DVBSIParser(dvbc->GetCardNum(), true);
            QObject::connect(dvbsiparser, SIGNAL(UpdatePMT(const PMTObject*)),
                             dvbc, SLOT(SetPMT(const PMTObject*)));
        }

        dvbsiparser->ReinitSIParser(
            dvbc->GetSIStandard(),
            (program_num >= 0) ? program_num : service_id);

#ifdef USING_DVB_EIT
        if (scanner)
            scanner->StartPassiveScan(dvbc, dvbsiparser);
#endif // USING_DVB_EIT
    }
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

/** \fn TVRec::RunTV()
 *  \brief Event handling method, contains event loop.
 */
void TVRec::RunTV(void)
{ 
    QMutexLocker lock(&stateChangeLock);
    SetFlags(kFlagRunMainLoop);
    ClearFlags(kFlagExitPlayer | kFlagFinishRecording);
    eitScanStartTime = QDateTime::currentDateTime()
        .addSecs(kEITScanStartTimeout);

    while (HasFlags(kFlagRunMainLoop))
    {
        bool doSleep = true;

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
        if (pendingRecording &&
            HasFlags(kFlagAskAllowRecording | kFlagFrontendReady))
        {
            ClearFlags(kFlagAskAllowRecording);

            int timeuntil = QDateTime::currentDateTime()
                .secsTo(recordPendingStart);

            QString query = QString("ASK_RECORDING %1 %2")
                .arg(cardid).arg(timeuntil);
            QStringList messages;
            messages << pendingRecording->title
                     << pendingRecording->chanstr
                     << pendingRecording->chansign
                     << pendingRecording->channame;
            
            MythEvent me(query, messages);

            gContext->dispatch(me);
        }

        // If we are recording a program, check if the recording is
        // over or someone has asked us to finish the recording.
        // If so, then we enter overrecord if overrecordseconds > 0
        // otherwise we queue up the state change to kState_None.
        if (StateIsRecording(internalState))
        {
#if 0
            // This is debugging code for ProgramInfo/RingBuffer
            // switching code. It will only work once per startup
            // of mythbackend.
            QDateTime switchTime = curRecording->recstartts.addSecs(15);
            static bool rbSwitched = false;
            if ((QDateTime::currentDateTime() > switchTime) && !rbSwitched)
            {
                RingBuffer *rb = new RingBuffer("/video/mythtv/x.mpg", true);
                VERBOSE(VB_IMPORTANT, LOC +"Switching RingBuffer soon");
                recorder->SetNextRecording(curRecording, rb);
                rbSwitched = true;
            }
#endif
            if (QDateTime::currentDateTime() > recordEndTime ||
                HasFlags(kFlagFinishRecording))
            {
                if (!inoverrecord && overrecordseconds > 0)
                {
                    recordEndTime = recordEndTime.addSecs(overrecordseconds);
                    inoverrecord = true;
                    QString msg = QString(
                        "Switching to overrecord for %1 more seconds")
                        .arg(overrecordseconds);
                    VERBOSE(VB_RECORD, LOC + msg);
                }
                else
                {
                    ChangeState(RemoveRecording(internalState));
                    doSleep = false;
                }
                ClearFlags(kFlagFinishRecording);
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
            doSleep = false;
        }

#ifdef USING_DVB_EIT
        if (scanner && QDateTime::currentDateTime() > eitScanStartTime)
        {
            scanner->StartActiveScan(this, kEITScanTransportTimeout);
            SetFlags(kFlagEITScannerRunning);
            eitScanStartTime = QDateTime::currentDateTime().addYears(1);
        }
#endif // USING_DVB_EIT

        // We should be no more than a few thousand milliseconds,
        // as the end recording code does not have a trigger...
        // NOTE: If you change anything here, make sure that
        // WaitforEventThreadSleep() will still work...
        if (doSleep && tuningRequests.empty())
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

bool TVRec::WaitForEventThreadSleep(bool wake, unsigned long time)
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
        ok = triggerEventSleep.wait(100);
        stateChangeLock.lock();

        // In case we missed trigger, check sleep state ourselves.
        // But only do this if we are sending wake events.
        ok |= (wake && tuningRequests.empty() && !changeState);
    }
    return ok;
}

void TVRec::GetChannelInfo(ChannelBase *chan, QString &title, QString &subtitle,
                           QString &desc, QString &category, 
                           QString &starttime, QString &endtime, 
                           QString &callsign, QString &iconpath, 
                           QString &channelname, QString &chanid,
                           QString &seriesid, QString &programid,
                           QString &outputFilters, QString &repeat, QString &airdate,
                           QString &stars)
{
    title = "";
    subtitle = "";
    desc = "";
    category = "";
    starttime = "";
    endtime = "";
    callsign = "";
    iconpath = "";
    channelname = "";
    chanid = "";
    seriesid = "";
    programid = "";
    outputFilters = "";
    repeat = "";
    airdate = "";
    stars = "";
    
    if (!chan)
        return;

    QString curtimestr = QDateTime::currentDateTime()
        .toString("yyyyMMddhhmmss");

    channelname = chan->GetCurrentName();
    QString channelinput = chan->GetCurrentInput();
 
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT starttime,       endtime,         title,    "
        "       subtitle,        description,     category, "
        "       callsign,        icon,            channel.chanid, "
        "       seriesid,        programid,       channel.outputfilters, "
        "       previouslyshown, originalairdate, stars  "
        "FROM program,channel, capturecard, cardinput    "
        "WHERE channel.channum = :CHANNAME           AND "
        "      starttime < :CURTIME                  AND "
        "      endtime   > :CURTIME                  AND "
        "      program.chanid   = channel.chanid     AND "
        "      channel.sourceid = cardinput.sourceid AND "
        "      cardinput.cardid = capturecard.cardid AND "
        "      capturecard.cardid   = :CARDID        AND "
        "      capturecard.hostname = :HOSTNAME");
    query.bindValue(":CHANNAME", channelname);
    query.bindValue(":CURTIME", curtimestr);
    query.bindValue(":CARDID", cardid);
    query.bindValue(":HOSTNAME", gContext->GetHostName());

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        query.next();

        starttime = query.value(0).toString();
        endtime = query.value(1).toString();
        title = QString::fromUtf8(query.value(2).toString());
        subtitle = QString::fromUtf8(query.value(3).toString());
        desc = QString::fromUtf8(query.value(4).toString());
        category = QString::fromUtf8(query.value(5).toString());
        callsign = QString::fromUtf8(query.value(6).toString());
        iconpath = query.value(7).toString();
        chanid = query.value(8).toString();
        seriesid = query.value(9).toString();
        programid = query.value(10).toString();
        outputFilters = query.value(11).toString();
        
        repeat = query.value(12).toString();
        airdate = query.value(13).toString();
        stars = query.value(14).toString();
    }
    else
    {
        // couldn't find a matching program for the current channel.
        // get the information about the channel anyway
        
        query.prepare(
            "SELECT callsign, icon, channel.chanid, channel.outputfilters "
            "FROM channel, capturecard, cardinput "
            "WHERE channel.channum  = :CHANNUM           AND "
            "      channel.sourceid = cardinput.sourceid AND "
            "      cardinput.cardid = capturecard.cardid AND "
            "      capturecard.cardid   = :CARDID        AND "
            "      capturecard.hostname = :HOSTNAME");
        query.bindValue(":CHANNUM", channelname);
        query.bindValue(":CARDID", cardid);
        query.bindValue(":HOSTNAME", gContext->GetHostName());

        if (query.exec() && query.isActive() && query.size() > 0)
        {
            query.next();

            callsign = QString::fromUtf8(query.value(0).toString());
            iconpath = query.value(1).toString();
            chanid = query.value(2).toString();
            outputFilters = query.value(3).toString();
        }
    }
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
        "SELECT videodevice,      vbidevice,           audiodevice,    "
        "       audioratelimit,   defaultinput,        cardtype,       "
        "       skipbtaudio,"
        "       dvb_hw_decoder,   dvb_recordts,        dvb_wait_for_seqstart,"
        "       dvb_dmx_buf_size, dvb_pkt_buf_size,    dvb_on_demand,  "
        "       firewire_port,    firewire_node,       firewire_speed, "
        "       firewire_model,   firewire_connection,                 "
        "       dbox2_port,       dbox2_host,          dbox2_httpport, "
        "       signal_timeout,   channel_timeout                      "
        "FROM capturecard "
        "WHERE cardid = :CARDID");
    query.bindValue(":CARDID", cardid);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("getdevices", query);
        return false;
    }
    else if (query.size() > 0)
    {
        query.next();

        test = query.value(0).toString();
        if (test != QString::null)
            gen_opts.videodev = QString::fromUtf8(test);
        test = query.value(1).toString();
        if (test != QString::null)
            gen_opts.vbidev = QString::fromUtf8(test);
        test = query.value(2).toString();
        if (test != QString::null)
            gen_opts.audiodev = QString::fromUtf8(test);
        testnum = query.value(3).toInt();
        if (testnum > 0)
            gen_opts.audiosamplerate = testnum;
        else
            gen_opts.audiosamplerate = -1;

        test = query.value(4).toString();
        if (test != QString::null)
            gen_opts.defaultinput = QString::fromUtf8(test);
        test = query.value(5).toString();
        if (test != QString::null)
            gen_opts.cardtype = QString::fromUtf8(test);

        gen_opts.skip_btaudio = query.value(6).toInt();

        dvb_opts.hw_decoder    = query.value(7).toInt();
        dvb_opts.recordts      = query.value(8).toInt();
        gen_opts.wait_for_seqstart = query.value(9).toInt();
        dvb_opts.dmx_buf_size  = query.value(10).toInt();
        dvb_opts.pkt_buf_size  = query.value(11).toInt();
        dvb_opts.dvb_on_demand = query.value(12).toInt();

        firewire_opts.port     = query.value(13).toInt();
        firewire_opts.node     = query.value(14).toInt(); 
        firewire_opts.speed    = query.value(15).toInt();
        test = query.value(16).toString();
        if (test != QString::null)
            firewire_opts.model = QString::fromUtf8(test);
        firewire_opts.connection = query.value(17).toInt();

        dbox2_opts.port = query.value(18).toInt();
        dbox2_opts.httpport = query.value(20).toInt();
        test = query.value(19).toString();
        if (test != QString::null)
           dbox2_opts.host = QString::fromUtf8(test);

        gen_opts.signal_timeout  = (uint) max(query.value(21).toInt(), 0);
        gen_opts.channel_timeout = (uint) max(query.value(22).toInt(), 0);

        // We should have at least 100 ms to acquire tables...
        int table_timeout = ((int)gen_opts.channel_timeout - 
                             (int)gen_opts.signal_timeout);
        if (table_timeout < 100)
            gen_opts.channel_timeout = gen_opts.signal_timeout + 2500;
    }
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
            msg = "Start channel '%1' invalid, setting to '%2' instead.";
            VERBOSE(VB_IMPORTANT, LOC_ERR + msg.arg(startchan).arg(test));
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
            msg = QString("Start channel '%1' invalid, setting "
                          "to '%2' on input %3 instead.")
                .arg(startchan).arg(test).arg(query.value(1).toString());
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
        sd = GetHDTVRecorder()->StreamData();
        sd->SetCaching(true);
    }
#endif // USING_V4L
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
        // Some DVB devices munge the PMT so the CRC check fails  we need to
        // tell the stream data class to not check the CRC on these devices.
        if (GetDVBChannel())
            sd->SetIgnoreCRCforPMT(GetDVBChannel()->HasCRCBug());
#endif // USING_DVB

        bool fta = CardUtil::IgnoreEncrypted(
            GetCaptureCardNum(), channel->GetCurrentInput());

        sm->SetStreamData(sd);
        sd->Reset(progNum);
        sm->SetProgramNumber(progNum);
        sd->SetVideoStreamsRequired(1);
        sm->SetFTAOnly(fta);
        sm->AddFlags(kDTVSigMon_WaitForPAT | kDTVSigMon_WaitForPMT);

        VERBOSE(VB_RECORD, LOC + "Successfully set up MPEG table monitoring.");
        return true;
    }

    QString msg = "No valid DTV info, ATSC maj(%1) min(%2), MPEG pn(%3)";
    VERBOSE(VB_RECORD, LOC_ERR + msg.arg(major).arg(minor).arg(progNum));
    return false;
}

/** \fn TVRec::SetupSignalMonitor()
 *  \brief This creates a SignalMonitor instance if one is needed and
 *         begins signal monitoring.
 *
 *   If the channel exists and the cardtype is "DVB" or "HDTV" a 
 *   SignalMonitor instance is created and SignalMonitor::Start()
 *   is called to start the signal monitoring thread.
 */
void TVRec::SetupSignalMonitor(void)
{
    VERBOSE(VB_RECORD, LOC + "SetupSignalMonitor()");
    // if it already exists, there no need to initialize it
    if (signalMonitor)
        return;

    // if there is no channel object we can't monitor it
    if (!channel)
        return;

    // make sure statics are initialized
    SignalMonitorValue::Init();

    if (SignalMonitor::IsSupported(genOpt.cardtype) && channel->Open())
    {
        // TODO reset the tuner hardware
        //channel->SwitchToInput(channel->GetCurrentInputNum(), true);

        signalMonitor = SignalMonitor::Init(genOpt.cardtype, cardid, channel);
    }
    
    if (signalMonitor)
    {
        VERBOSE(VB_RECORD, LOC + "Signal monitor successfully created");
        // If this is a monitor for Digital TV, initialize table monitors
        if (GetDTVSignalMonitor())
            SetupDTVSignalMonitor();

        connect(signalMonitor, SIGNAL(AllGood(void)),
                this, SLOT(SignalMonitorAllGood(void)));

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
    if (dtvMon)
    {
        if (channel)
        {
            pid_cache_t pid_cache;
            GetPidsToCache(dtvMon, pid_cache);
            if (pid_cache.size())
                channel->SaveCachedPids(pid_cache);
        }

#ifdef USING_DVB
        if (genOpt.cardtype == "DVB" && dtvMon->GetATSCStreamData())
        {
            dtvMon->Stop();
            ATSCStreamData *sd = dtvMon->GetATSCStreamData();
            dtvMon->SetStreamData(NULL);
            sd->deleteLater();
            sd = NULL;
        }
#endif // USING_DVB
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
    VERBOSE(VB_RECORD, LOC + msg.arg(rate).arg(notifyFrontend));

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

    ClearFlags(kFlagRingBufferReset);
    if (rate > 0)
    {
        tuningRequests.enqueue(TuningRequest(kFlagKillRec));
        tuningRequests.enqueue(TuningRequest(kFlagAntennaAdjust,
                                             channel->GetCurrentName()));
    }
    else
    {
        tuningRequests.enqueue(TuningRequest(kFlagLiveTV));
    }
    // Wait for RingBuffer reset
    while (!HasFlags(kFlagRingBufferReset))
        WaitForEventThreadSleep();

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
 *  \param channelid channel to verify against tuners.
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

/** \fn TVRec::CheckChannel(QString)
 *  \brief Checks if named channel exists on current tuner.
 *
 *  \param name channel to verify against current tuner.
 *  \return true if it succeeds, false otherwise.
 *  \sa EncoderLink::CheckChannel(const QString&),
 *      RemoteEncoder::CheckChannel(QString), 
 *      CheckChannel(ChannelBase*,const QString&,QString&),
 *      ShouldSwitchToAnotherCard(QString)
 */
bool TVRec::CheckChannel(QString name)
{
    if (!channel)
        return false;

    QString dummyID;
    return CheckChannel(channel, name, dummyID);
}

bool TVRec::CheckChannel(ChannelBase *chan, const QString &channum, 
                         QString& inputName)
{
    if (!chan)
        return false;

    inputName = "";
    
    bool ret = false;

    QString channelinput = chan->GetCurrentInput();

    MSqlQuery query(MSqlQuery::InitCon());

    if (!query.isConnected())
        return true;

    query.prepare(
        "SELECT channel.chanid "
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
        MythContext::DBError("checkchannel", query);
    }
    else if (query.size() > 0)
    {
        return true;
    }

    QString msg = QString(
        "Failed to find channel(%1) on current input (%2) of card (%3).")
        .arg(channum).arg(channelinput).arg(cardid);
    VERBOSE(VB_RECORD, LOC + msg);

    // We didn't find it on the current input let's widen the search
    query.prepare(
        "SELECT channel.chanid, cardinput.inputname "
        "FROM channel, capturecard, cardinput "
        "WHERE channel.channum      = :CHANNUM           AND "
        "      channel.sourceid     = cardinput.sourceid AND "
        "      cardinput.cardid     = capturecard.cardid AND "
        "      capturecard.cardid   = :CARDID            AND "
        "      capturecard.hostname = :HOSTNAME");
    query.bindValue(":CHANNUM", channum);
    query.bindValue(":CARDID", cardid);
    query.bindValue(":HOSTNAME", gContext->GetHostName());

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("checkchannel", query);
    } 
    else if (query.size() > 0)
    {
        query.next();
        QString test = query.value(1).toString();
        if (test != QString::null)
            inputName = QString::fromUtf8(test);

        msg = QString("Found channel(%1) on another input (%2) of card (%3).")
            .arg(channum).arg(inputName).arg(cardid);
        VERBOSE(VB_RECORD, LOC + msg);

        return true;
    }

    msg = QString("Failed to find channel(%1) on any input of card (%2).")
        .arg(channum).arg(cardid);
    VERBOSE(VB_RECORD, LOC + msg);

    query.prepare("SELECT NULL FROM channel");

    if (query.exec() && query.size() == 0)
        ret = true;

    return ret;
}

/** \fn TVRec::CheckChannelPrefix(QString name, bool &unique)
 *  \brief Returns true if the numbers in prefix_num match the first digits
 *         of any channel, if it unquely identifies a channel the unique
 *         parameter is set.
 *
 *   If name is a valid channel name and not a valid channel prefix
 *   unique is set to true.
 *
 *   For example, if name == "36" and "36", "360", "361", "362", and "363" are
 *   valid channel names, this function would return true but set *unique to
 *   false.  However if name == "361" it would both return true and set *unique
 *   to true.
 *
 *  \param prefix_num Channel number prefix to check
 *  \param unique     This is set to true if prefix uniquely identifies
 *                    channel, false otherwise.
 *  \return true if the prefix matches any channels.
 *
 */
bool TVRec::CheckChannelPrefix(QString name, bool &unique)
{
    if (!channel)
        return false;

    bool ret = false;
    unique = false;

    QString channelinput = channel->GetCurrentInput();

    MSqlQuery query(MSqlQuery::InitCon());
  
    if (!query.isConnected())
        return true;

    QString querystr = QString(
        "SELECT channel.chanid "
        "FROM channel, capturecard, cardinput "
        "WHERE channel.channum LIKE '%1%%'               AND "
        "      channel.sourceid     = cardinput.sourceid AND "
        "      cardinput.inputname  = '%2'               AND "
        "      cardinput.cardid     = capturecard.cardid AND "
        "      capturecard.cardid   = '%3'               AND "
        "      capturecard.hostname = '%4'")
        .arg(name)
        .arg(channelinput)
        .arg(cardid)
        .arg(gContext->GetHostName());

    query.prepare(querystr);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("checkchannel", query);
    }
    else if (query.size() > 0)
    {
        if (query.size() == 1)
        {
            unique = CheckChannel(name);
        }
        return true;
    }

    query.prepare("SELECT NULL FROM channel");
    query.exec();

    if (query.size() == 0) 
    {
        unique = true;
        ret = true;
    }

    return ret;
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
        "WHERE channel.%2 %3 '%4' %5 AND "
        "      %6 "
        "ORDER BY channel.%7 %8 "
        "LIMIT 1")
        .arg(fromfavorites).arg(channelorder)
        .arg(comp).arg(id).arg(wherefavorites)
        .arg(wherepart).arg(channelorder).arg(ordering);

    query.prepare(querystr);
    query.bindValue(":CARDID",   cardid);
    query.bindValue(":HOSTNAME", gContext->GetHostName());

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
            "WHERE channel.%2 %3 '%4' %5 AND "
            "      %6 "
            "ORDER BY channel.%7 %8 "
            "LIMIT 1")
            .arg(fromfavorites).arg(channelorder)
            .arg(comp).arg(id).arg(wherefavorites)
            .arg(wherepart).arg(channelorder).arg(ordering);

        query.prepare(querystr);
        query.bindValue(":CARDID",   cardid);
        query.bindValue(":HOSTNAME", gContext->GetHostName());
 
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
        return ringBuffer->GetTotalWritePosition();
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

/** \fn TVRec::GetFreeSpace(long long)
 *  \brief Returns number of bytes beyond "totalreadpos" it is safe to read.
 *
 *  Note: This may return a negative number, including -1 if the call
 *  succeeds. This means totalreadpos is past the "safe read" portion of
 *  the file.
 *
 *  \sa EncoderLink::GetFreeSpace(long long), RemoteEncoder::GetFreeSpace(long long)
 *  \return Returns number of bytes ahead of totalreadpos it is safe to read
 *          if call succeeds, -1 otherwise.
 */
long long TVRec::GetFreeSpace(long long totalreadpos)
{
    QMutexLocker lock(&stateChangeLock);

    if (ringBuffer)
    {
        return totalreadpos + 
            ringBuffer->GetFileSize() - 
            ringBuffer->GetTotalWritePosition() -
            ringBuffer->GetSmudgeSize();
    }

    return -1;
}

/** \fn TVRec::GetMaxBitrate()
 *  \brief Returns the maximum bits per second this recorder can produce.
 *
 *  \sa EncoderLink::GetMaxBitrate(), RemoteEncoder::GetMaxBitrate()
 */
long long TVRec::GetMaxBitrate()
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

/** \fn TVRec::StopPlaying()
 *  \brief Tells TVRec to stop streaming a recording to the frontend.
 *
 *  \sa EncoderLink::StopPlaying(), RemoteEncoder::StopPlaying()
 */
void TVRec::StopPlaying(void)
{
    SetFlags(kFlagExitPlayer);
}

/** \fn TVRec::SetupRingBuffer(QString&,long long&,long long&,bool)
 *  \brief Sets up RingBuffer for "Live TV" playback.
 *
 *  \sa EncoderLink::SetupRingBuffer(QString&,long long&,long long&,bool),
 *      RemoteEncoder::SetupRingBuffer(QString&,long long&,long long&,bool),
 *      StopLiveTV()
 *
 *  \param path Returns path to recording.
 *  \param filesize Returns size of recording file in bytes.
 *  \param fillamount Returns the maximum buffer fill in bytes.
 *  \param pip Tells TVRec that this is for a Picture in Picture display.
 *  \return true if successful, false otherwise.
 */
bool TVRec::SetupRingBuffer(QString &path, long long &filesize, 
                            long long &fillamount, bool pip)
{
    QMutexLocker lock(&stateChangeLock);

    if (ringBuffer)
    {
        // If we are in state none, assume frontend crashed in LiveTV...
        if (GetState() == kState_None)
        {
            stateChangeLock.unlock();
            StopLiveTV();
            WaitForEventThreadSleep();
            stateChangeLock.lock();
        }
        if (ringBuffer)
        {
            VERBOSE(VB_IMPORTANT, "A RingBuffer already on this recorder...\n"
                    "\t\t\tIs someone watich 'Live TV' on this recorder?");
            return false;
        }
    }

    ispip = pip;
    filesize = liveTVRingBufSize;
    fillamount = liveTVRingBufFill;

    path = rbFileName = QString("%1/ringbuf%2.%3")
        .arg(liveTVRingBufLoc).arg(cardid).arg(rbFileExt);

    filesize = filesize * 1024 * 1024 * 1024;
    fillamount = fillamount * 1024 * 1024;

    SetRingBuffer(new RingBuffer(path, filesize, fillamount));
    if (ringBuffer->IsOpen())
    {
        ringBuffer->SetWriteBufferMinWriteSize(1);
        return true;
    }
    else
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to open RingBuffer file.");
        SetRingBuffer(NULL);
        return false;
    }
}

/** \fn TVRec::SpawnLiveTV()
 *  \brief Tells TVRec to spawn a "Live TV" recorder.
 *  \sa EncoderLink::SpawnLiveTV(), RemoteEncoder::SpawnLiveTV()
 */
void TVRec::SpawnLiveTV(void)
{
    QMutexLocker lock(&stateChangeLock);
    // Change to WatchingLiveTV
    ChangeState(kState_WatchingLiveTV);
    // Wait for state change to take effect
    WaitForEventThreadSleep();
}

/** \fn TVRec::StopLiveTV()
 *  \brief Tells TVRec to stop a "Live TV" recorder.
 *  \sa EncoderLink::StopLiveTV(), RemoteEncoder::StopLiveTV()
 */
void TVRec::StopLiveTV(void)
{
    if (GetState() != kState_None)
    {
        QMutexLocker lock(&stateChangeLock);
        ChangeState(kState_None);
        // Wait for state change to take effect...
        WaitForEventThreadSleep();
    }

    // Tell tuner it is safe to shut down ring buffer.
    tuningRequests.enqueue(TuningRequest(kFlagKillRingBuffer));
    triggerEventLoop.wakeAll();
}

/** \fn TVRec::PauseRecorder()
 *  \brief Tells recorder to pause, used for channel and input changes.
 *  \sa EncoderLink::PauseRecorder(), RemoteEncoder::PauseRecorder(),
 *      RecorderBase::Pause()
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

/** \fn TVRec::SetChannel(QString,uint)
 *  \brief Changes to a named channel on the current tuner.
 *
 *   You must call PauseRecorder() before calling this.
 *
 *  \param name channel to change to.
 */
void TVRec::SetChannel(QString name, uint requestType)
{
    QMutexLocker lock(&stateChangeLock);

    // Detect tuning request type if needed
    if (requestType & kFlagDetect)
    {
        WaitForEventThreadSleep();
        requestType = lastTuningRequest.flags & (kFlagRec | kFlagNoRec);
    }

    // Clear the RingBuffer reset flag, in case we wait for a reset below
    ClearFlags(kFlagRingBufferReset);

    // Actually add the tuning request to the queue, and
    // then wait for it to start tuning
    tuningRequests.enqueue(TuningRequest(requestType, name));
    WaitForEventThreadSleep();

    // If we are using a recorder, wait for a RingBuffer reset
    if (requestType & kFlagRec)
    {
        while (!HasFlags(kFlagRingBufferReset))
            WaitForEventThreadSleep();
    }
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

/** \fn TVRec::GetChannelInfo(QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&)
 *  \brief Returns information on the current program and current channel.
 *  \sa EncoderLink::GetChannelInfo(QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&),
 *      RemoteEncoder::GetChannelInfo(QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&),
 */
void TVRec::GetChannelInfo(QString &title,       QString &subtitle, 
                           QString &desc,        QString &category, 
                           QString &starttime,   QString &endtime, 
                           QString &callsign,    QString &iconpath,
                           QString &channelname, QString &chanid,
                           QString &seriesid,    QString &programid, 
                           QString &outputFilters, QString &repeat,
                           QString &airdate,     QString &stars)
{
    if (!channel)
        return;

    GetChannelInfo(channel, title, subtitle, desc, category, starttime,
                   endtime, callsign, iconpath, channelname, chanid,
                   seriesid, programid, outputFilters, repeat, airdate, stars);
}

/** \fn TVRec::GetInputName(QString&)
 *  \brief Sets inputname to the textual name of the current input,
 *         if a tuner is being used.
 *
 *  \sa EncoderLink::GetInputName(), RemoteEncoder::GetInputName(QString&)
 *  \return Sets input name if successful, does nothing if it isn't.
 */
void TVRec::GetInputName(QString &inputname)
{
    if (!channel)
        return;

    inputname = channel->GetCurrentInput();
}

/** \fn TVRec::SeekRingBuffer(long long, long long, int)
 *  \brief Tells TVRec to seek to a specific byte in RingBuffer.
 *
 *  \param curpos Current byte position in RingBuffer
 *  \param pos    Desired position, or position delta.
 *  \param whence One of SEEK_SET, or SEEK_CUR, or SEEK_END.
 *                These work like the equivalent fseek parameters.
 *  \return new position if seek is successful, -1 otherwise.
 */
long long TVRec::SeekRingBuffer(long long curpos, long long pos, int whence)
{
    QMutexLocker lock(&stateChangeLock);
    long long ret = -1;

    if (!ringBuffer || !rbStreamingLive)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Seek("<<curpos<<", "<<pos<<", "<<whence<<") err");
    }
    else
    {
        ringBuffer->StopReads();
        if (whence == SEEK_CUR)
        {
            long long realpos = ringBuffer->GetTotalReadPosition();
            pos = pos + curpos - realpos;
        }
        ret = ringBuffer->Seek(pos, whence);
        ringBuffer->StartReads();
    }

    return ret;
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

/** \fn TVRec::GetReadThreadSocket(void)
 *  \brief Returns RingBuffer data socket, for A/V streaming.
 *  \sa SetReadThreadSock(QSocket*), RequestRingBufferBlock(uint)
 */
QSocket *TVRec::GetReadThreadSocket(void)
{
    return rbStreamingSock;
}

/** \fn TVRec::SetReadThreadSocket(QSocket*)
 *  \brief Sets RingBuffer data socket, for A/V streaming.
 *  \sa GetReadThreadSocket(void), RequestRingBufferBlock(uint)
 */
void TVRec::SetReadThreadSocket(QSocket *sock)
{
    QMutexLocker lock(&stateChangeLock);

    if ((rbStreamingLive && sock) || (!rbStreamingLive && !sock))
    {
        VERBOSE(VB_IMPORTANT, LOC +
                "Not setting streaming socket live("<<rbStreamingLive<<")\n"
                "\t\t\tnew sock("<<sock<<") old sock("<<rbStreamingSock<<")");
        
        return;
    }

    if (sock)
    {
        rbStreamingSock = sock;
        rbStreamingLive = true;
    }
    else
    {
        rbStreamingLive = false;
        if (ringBuffer)
            ringBuffer->StopReads();
    }
}

/** \fn TVRec::RequestRingBufferBlock(uint)
 *  \brief Tells ring buffer to send data on the read thread sock, if the
 *         ring buffer thread is alive and the ringbuffer isn't paused.
 *
 *  \param size Requested block size, may not be respected, but this many
 *              bytes of data will be returned if it is available.
 *  \return -1 if request does not succeed, amount of data sent otherwise.
 */
int TVRec::RequestRingBufferBlock(uint size)
{
    QMutexLocker lock(&stateChangeLock);
    int tot = 0;
    int ret = 0;

    if (!rbStreamingLive || !ringBuffer)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "RequestBlock("<<size<<") err");
        return -1;
    }

    MythTimer t;
    t.start();
    while (tot < (int)size &&
           ringBuffer && !ringBuffer->GetStopReads() &&
           rbStreamingLive &&
           t.elapsed() < (int)kStreamedFileReadTimeout)
    {
        uint request = size - tot;

        request = min(request, kRequestBufferSize);
 
        ret = ringBuffer->Read(rbStreamingBuffer, request);
        
        if (ringBuffer->GetStopReads() || ret <= 0)
            break;
        
        if (!WriteBlock(rbStreamingSock->socketDevice(),
                        rbStreamingBuffer, ret))
        {
            tot = -1;
            break;
        }

        tot += ret;
        if (ret < (int)request)
            break; // we hit eof
    }

    if (ret < 0)
        tot = -1;

    return tot;
}

/** \fn  TVRec::RetrieveInputChannels(QMap<int,QString>&,QMap<int,QString>&,QMap<int,QString>&,QMap<int,QString>&)
 *  \brief Returns all channels, used for channel browsing.
 */
void TVRec::RetrieveInputChannels(QMap<int, QString> &inputChannel,
                                  QMap<int, QString> &inputTuneTo,
                                  QMap<int, QString> &externalChanger,
                                  QMap<int, QString> &sourceid)
{
    if (!channel)
        return;

    MSqlQuery query(MSqlQuery::InitCon());
    QString querystr = QString(
        "SELECT inputname, "
        "       trim(externalcommand), "
        "       if(tunechan='', 'Undefined', tunechan), "
        "       if(startchan, startchan, ''), "
        "       sourceid "
        "FROM capturecard, cardinput "
        "WHERE capturecard.cardid = %1 AND "
        "      capturecard.cardid = cardinput.cardid")
        .arg(cardid);

    query.prepare(querystr);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("RetrieveInputChannels", query);
    }
    else if (!query.size())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + 
                "Error getting inputs for the capturecard.      \n"
                "\t\t\tPerhaps you have forgotten to bind video \n"
                "\t\t\tsources to your card's inputs?");
    }
    else
    {
        int cap;

        while (query.next())
        {
            cap = channel->GetInputByName(query.value(0).toString());
            externalChanger[cap] = query.value(1).toString();
            inputTuneTo[cap] = query.value(2).toString();
            inputChannel[cap] = query.value(3).toString();
            sourceid[cap] = query.value(4).toString();
        }
    }

}

/** \fn TVRec::StoreInputChannels(const QMap<int, QString>&)
 *  \brief Sets starting channel for the each input in the "inputChannel" map.
 *
 *  \param inputChannel Map from input number to channel, with an input and 
 *                      default channel for each input to update.
 */
void TVRec::StoreInputChannels(const QMap<int, QString> &inputChannel)
{
    if (!channel)
        return;

    QString querystr, input;

    MSqlQuery query(MSqlQuery::InitCon());

    for (int i = 0;; i++)
    {
        input = channel->GetInputByNum(i);
        if (input.isEmpty())
            break;

        if (inputChannel[i].isEmpty())
            continue;

        querystr = QString(
            "UPDATE cardinput "
            "SET startchan = '%1' "
            "WHERE cardid = %2 AND inputname = '%3'")
            .arg(inputChannel[i]).arg(cardid).arg(input);

        query.prepare(querystr);

        if (!query.exec() || !query.isActive())
            MythContext::DBError("StoreInputChannels", query);
    }

}

void TVRec::RingBufferChanged(void)
{
    VERBOSE(VB_IMPORTANT, LOC + "RingBufferChanged()");
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
            if (!recorder || genOpt.cardtype == "DVB")
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
        if (ringBuffer)
            ringBuffer->Reset();
        SetFlags(kFlagRingBufferReset);
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
        channel->StoreInputChannels();
    }
}

/** \fn TuningShutdowns(const TuningRequest&)
 *  \brief This shuts down anything that needs to be shut down 
 *         before handling the passed in tuning request.
 */
void TVRec::TuningShutdowns(const TuningRequest &request)
{
#ifdef USING_DVB
    if (!(request.flags & kFlagEITScan) && HasFlags(kFlagEITScannerRunning))
    {
#ifdef USING_DVB_EIT
        scanner->StopActiveScan();
#endif // USING_DVB_EIT
        ClearFlags(kFlagEITScannerRunning);
    }

    if (HasFlags(kFlagSIParserRunning))
    {
        TeardownSIParser();

        if (recorder)
            GetDVBRecorder()->Close();
        if (ringBuffer)
            ringBuffer->Reset();
        SetFlags(kFlagRingBufferReset);
    }
    ClearFlags(kFlagWaitingForSIParser | kFlagSIParserRunning);
    // At this point the SIParser is shut down, the 
    // recorder is closed, and the ringbuffer reset.

#endif // USING_DVB

    if (HasFlags(kFlagSignalMonitorRunning))
    {
        TeardownSignalMonitor();
        ClearFlags(kFlagSignalMonitorRunning);
    }
    ClearFlags(kFlagWaitingForSignal);
    // At this point any waits are canceled.

    if ((request.flags & kFlagNoRec))
    {
        if (HasFlags(kFlagDummyRecorderRunning))
        {
            dummyRecorder->StopRecordingThread();
            ClearFlags(kFlagDummyRecorderRunning);
        }

        if (request.flags & kFlagCloseRec)
            FinishedRecording(lastTuningRequest.program);

        if (HasFlags(kFlagRecorderRunning))
        {
            TeardownRecorder(request.flags & kFlagKillRec);
            ClearFlags(kFlagRecorderRunning);
        }
        // At this point the recorders are shut down

        CloseChannel();
        // At this point the channel is shut down
    }

    if (ringBuffer && (request.flags & kFlagKillRingBuffer))
    {
        if (lastTuningRequest.flags & kFlagLiveTV)
        {
            VERBOSE(VB_RECORD, LOC + "Stopping LiveTV RingBuffer reads");
            if (ringBuffer)
                ringBuffer->StopReads();
        }
        VERBOSE(VB_RECORD, LOC + "Tearing down RingBuffer");
        rbStreamingLive = false;
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
    channel->Open();
    if (!channum.isEmpty())
    {
        if (channum.find("NextChannel") >= 0)
        {
            int dir = channum.right(channum.length() - 12).toInt();
            channum = GetNextChannel(channel, dir);
        }

        if (!input.isEmpty())
            ok = channel->SwitchToInput(input, channum);
        else if (channum.find("ToggleInputs") >= 0)
            ok = channel->ToggleInputs();
        else
            ok = channel->SetChannelByString(channum);
    }

    if (!ok)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Failed to set channel to %1. Reverting to kState_None")
                .arg(channum));
        if (kState_None != internalState)
            ChangeState(kState_None);
        else
            tuningRequests.enqueue(TuningRequest(kFlagKillRec));
        return;
    }

    // Setup for framebuffer capture devices..
    SetVideoFiltersForChannel(channel, channel->GetCurrentName());
#ifdef USING_V4L
    if (GetV4LChannel())
    {
        channel->SetBrightness();
        channel->SetContrast();
        channel->SetColour();
        channel->SetHue();
    }
#endif // USING_V4L

    // Start dummy recorder for devices capable of signal monitoring.
    bool use_sm = SignalMonitor::IsSupported(genOpt.cardtype);
    bool livetv = request.flags & kFlagLiveTV;
    bool antadj = request.flags & kFlagAntennaAdjust;
    if (use_sm && !dummyRecorder && (livetv || antadj))
    {
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
    }

    // Start signal monitoring for devices capable of monitoring
    if (use_sm)
    {
        VERBOSE(VB_RECORD, LOC + "Starting Signal Monitor");
        SetupSignalMonitor();
        if (signalMonitor)
        {
            signalMonitor->SetUpdateRate(kSignalMonitoringRate);
            signalMonitor->SetNotifyFrontend(true);

            if (request.flags & kFlagEITScan)
            {
                GetDTVSignalMonitor()->GetStreamData()->
                    SetVideoStreamsRequired(0);
            }

            SetFlags(kFlagSignalMonitorRunning);
            if (request.flags & kFlagRec|kFlagEITScan)
                SetFlags(kFlagWaitingForSignal);
        }
        if (dummyRecorder && ringBuffer)
        {
            ringBuffer->Reset();
            ringBuffer->StartReads();
            SetFlags(kFlagRingBufferReset);
            dummyRecorder->StartRecordingThread();
            SetFlags(kFlagDummyRecorderRunning);
        }
    }

    // Request a recorder, if the command is a recording command
    if (request.flags & kFlagRec)
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
    if (GetDVBChannel())
    {
        GetDVBChannel()->SetPMT(NULL);
        CreateSIParser(programNum);
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

static void load_recording_profile(
    RecordingProfile &profile, ProgramInfo *rec, int cardid)
{
    QString profileName = "Live TV";
    bool done = false;

    if (rec)
    {
        profileName = rec->GetScheduledRecording()->getProfileName();
        if (!profileName.isEmpty())
            done = profile.loadByCard(profileName, cardid);
        profileName = (done) ? profileName : QString("Default");
    }

    if (!done)
        profile.loadByCard(profileName, cardid);

    QString msg = QString("Using profile '%1' to record").arg(profileName);
    VERBOSE(VB_RECORD, LOC + msg);
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
    RecordingProfile profile;
    ProgramInfo *rec = lastTuningRequest.program;

    load_recording_profile(profile, rec, cardid);

    if (lastTuningRequest.flags & kFlagRecording)
    {
        SetRingBuffer(new RingBuffer(rbFileName, true));
        if (!ringBuffer->IsOpen())
        {
            QString msg = "RingBuffer '%1' not open...";
            VERBOSE(VB_IMPORTANT, LOC_ERR + msg.arg(rbFileName));
            SetRingBuffer(NULL);
            ClearFlags(kFlagPendingActions);
            ChangeState(kState_None);
            return;
        }
    }

    if (HasFlags(kFlagDummyRecorderRunning))
    {
        dummyRecorder->StopRecordingThread();
        ClearFlags(kFlagDummyRecorderRunning); 
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
        ChangeState(kState_None);
        return;
    }

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
        ChangeState(kState_None);
        return;
    }

    recorder->SetRecording(lastTuningRequest.program);
    if (GetV4LChannel())
        CloseChannel();
    pthread_create(&recorder_thread, NULL, TVRec::RecorderThread, recorder);

    // Wait for recorder to start.
    stateChangeLock.unlock();
    while (!recorder->IsRecording() && !recorder->IsErrored())
        usleep(5 * 1000);
    stateChangeLock.lock();

    if (GetV4LChannel())
        channel->SetFd(recorder->GetVideoFd());

    SetFlags(kFlagRecorderRunning);
    if (lastTuningRequest.flags & kFlagRecording)
        StartedRecording(lastTuningRequest.program);

    autoRunJobs = init_jobs(rec, profile, runJobOnHostOnly,
                            transcodeFirst, earlyCommFlag);

    ClearFlags(kFlagNeedToStartRecorder);
}

/** \fn TVRec::TuningRestartRecorder(void)
 *  \brief Restarts a stopped recorder or unpauses a paused recorder.
 */
void TVRec::TuningRestartRecorder(void)
{
    VERBOSE(VB_RECORD, LOC + "Restarting Recorder");
    if (HasFlags(kFlagDummyRecorderRunning))
    {
        dummyRecorder->StopRecordingThread();
        ClearFlags(kFlagDummyRecorderRunning);
    }
    recorder->Reset();

    // Set file descriptor of channel from recorder for V4L
    channel->SetFd(recorder->GetVideoFd());

    recorder->Unpause();

#ifdef USING_DVB
    if (GetDVBRecorder())
        GetDVBRecorder()->Open();
#endif // USING_DVB

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
    if (kFlagRingBufferReset & f)
        msg += "RingBufferReset,";

    if (msg.isEmpty())
        msg = QString("0x%1").arg(f,0,16);

    return msg;
}

QString TuningRequest::toString(void) const
{
    return QString("Program(%1) channel(%2) input(%3) flags(%4)")
        .arg((program != 0) ? "yes" : "no").arg(channel).arg(input)
        .arg(TVRec::FlagToString(flags));
}
