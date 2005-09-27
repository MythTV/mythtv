// C headers
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <pthread.h>

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
#include "RingBufferInfo.h"

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
    : rbi(NULL), recorder(NULL), channel(NULL), signalMonitor(NULL),
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
      changeState(false),
      startingRecording(false), abortRecordingStart(false),
      waitingForSignal(false), waitingForDVBTables(false),
      prematurelystopped(false), stateFlags(0),
      // Current recording info
      curRecording(NULL), autoRunJobs(JOB_NONE),
      inoverrecord(false), overrecordseconds(0),
      endTimeLock(false),
      // Pending recording info
      pendingRecording(NULL)
{
    event_thread    = PTHREAD_CREATE_JOINABLE;
    recorder_thread = static_cast<pthread_t>(0);
}

/** \fn TVRec::Init()
 *  \brief Performs instance initialization, returns true on success.
 *
 *  \return Returns true on success, false on failure.
 */
bool TVRec::Init(void)
{
    bool ok = GetDevices(cardid, genOpt, dvbOpt, fwOpt, dboxOpt);

    if (!ok)
        return false;

    QString startchannel = GetStartChannel(cardid, genOpt.defaultinput);

    rbi = new RingBufferInfo();
    rbi->SetFileExtension("mpg");

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
        StartChannel(false);
        usleep(500);    // Give DVBCam some breathing room
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
            rbi->SetFileExtension("nuv");
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

    pthread_create(&event_thread, NULL, EventThread, this);

    while (!HasFlags(kFlagRunMainLoop))
        usleep(50);

    return true;
}

/** \fn TVRec::~TVRec()
 *  \brief Stops the event and scanning threads and deletes any ChannelBase,
 *         RingBuffer, and RecorderBase instances.
 */
TVRec::~TVRec(void)
{
    ClearFlags(kFlagRunMainLoop);
    pthread_join(event_thread, NULL);

    TeardownSignalMonitor();
    TeardownSIParser();
#ifdef USING_DVB_EIT
    if (scanner)
    {
        scanner->deleteLater();
        scanner = NULL;
    }
#endif // USING_DVB_EIT

    if (GetDVBChannel())
        GetDVBChannel()->deleteLater();
    else if (GetDBox2Channel())
        GetDBox2Channel()->deleteLater();
    else if (channel)
        delete channel;
    channel = NULL;

    if (rbi)
    {
        delete rbi;
        rbi = NULL;
    }

    TeardownRecorder(true);

    if (dummyRecorder)
    {
        dummyRecorder->deleteLater();
        dummyRecorder = NULL;
    }
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
    QString msg("");

    RecStatusType retval = rsAborted;

    // We need to do this check early so we don't cancel an overrecord
    // that we're trying to extend.
    endTimeLock.lock();
    if (!changeState &&
        StateIsRecording(internalState) &&
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
        endTimeLock.unlock();

        retval = rsRecording;
        return retval;
    }
    endTimeLock.unlock();

    if (pendingRecording)
    {
        delete pendingRecording;
        pendingRecording = NULL;
    }

    ClearFlags(kFlagAskAllowRecording);

    if (inoverrecord)
    {
        ChangeState(kState_None);

        while (changeState)
            usleep(50);
    }

    if (changeState)
    {
        VERBOSE(VB_RECORD, LOC + "backend still changing state, waiting..");
        while (changeState)
            usleep(50);
        VERBOSE(VB_RECORD, LOC + "changing state finished, starting now");
    }

    if (internalState == kState_WatchingLiveTV && 
        !HasFlags(kFlagCancelNextRecording))
    {
        QString message = QString("QUIT_LIVETV %1").arg(cardid);
        MythEvent me(message);
        gContext->dispatch(me);

        MythTimer timer;
        timer.start();

        while (internalState != kState_None && timer.elapsed() < 10000)
            usleep(100);

        // Try again
        if (internalState != kState_None)
        {
            gContext->dispatch(me);

            timer.restart();

            while (internalState != kState_None && timer.elapsed() < 10000)
                usleep(100);
        }

        // Try harder
        if (internalState != kState_None)
        {
            SetFlags(kFlagExitPlayer);
            timer.restart();
            while (internalState != kState_None && timer.elapsed() < 5000)
                usleep(100);
        }
    }

    if (internalState == kState_None)
    {
        QString fn = rcinfo->CreateRecordBasename(rbi->GetFileExtension());
        rbi->SetFilename(recprefix, fn);
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

    return retval;
}

/** \fn TVRec::StopRecording()
 *  \brief Changes from kState_RecordingOnly to kState_None.
 *  \sa StartRecording(const ProgramInfo *rec), FinishRecording()
 */
void TVRec::StopRecording(void)
{
    if (StateIsRecording(internalState))
    {
        ChangeState(RemoveRecording(internalState));
        prematurelystopped = false;

        while (changeState)
            usleep(50);
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

    curRecording->StartedRecording(rbi->GetUIFilename());

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

    curRecording->FinishedRecording(prematurelystopped);
}

bool TVRec::CreateRecorderThread()
{
    pthread_create(&recorder_thread, NULL, TVRec::RecorderThread, recorder);

    VERBOSE(VB_IMPORTANT, LOC + "Waiting for recorder to start");
    while (!recorder->IsRecording() && !recorder->IsErrored())
        usleep(50);
    if (recorder->IsRecording())
        VERBOSE(VB_IMPORTANT, LOC + "Recorder started");
    else
        VERBOSE(VB_IMPORTANT, LOC + "Error encountered starting recorder");
    return recorder->IsRecording();
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

static void init_jobs(ProgramInfo *curRecording,
                      int &autoRunJobs)
{
    JobQueue::ClearJobMask(autoRunJobs);

    if (!curRecording)
        return;

    JobQueue::AddJobsToMask(curRecording->GetAutoRunJobs(), autoRunJobs);

    if (curRecording->chancommfree)
        JobQueue::RemoveJobsFromMask(JOB_COMMFLAG, autoRunJobs);
}

static void enable_auto_transcode(RecordingProfile &profile,
                                  ProgramInfo *curRecording,
                                  int &autoRunJobs)
{
    if (!curRecording)
        return;

    // Make sure transcoding is OFF if the profile does not allow
    // AutoTranscoding.
    Setting *profileAutoTranscode = profile.byName("autotranscode");
    if ((!profileAutoTranscode) ||
        (profileAutoTranscode->getValue().toInt() == 0))
        JobQueue::RemoveJobsFromMask(JOB_TRANSCODE, autoRunJobs);
}

static void enable_realtime_commflag(ProgramInfo *curRecording,
                                     int &autoRunJobs,
                                     bool runJobOnHostOnly,
                                     bool transcodeFirst,
                                     bool earlyCommFlag)
{
    if (!curRecording)
        return;

    bool autocommflag = JobQueue::JobIsInMask(JOB_COMMFLAG, autoRunJobs) &&
        earlyCommFlag;
    bool commflagfirst = JobQueue::JobIsNotInMask(JOB_TRANSCODE, autoRunJobs) ||
        !transcodeFirst;

    if (autocommflag && commflagfirst)
    {
        JobQueue::QueueJob(
            JOB_COMMFLAG, curRecording->chanid,
            curRecording->recstartts, "", "",
            (runJobOnHostOnly) ? gContext->GetHostName() : "",
            JOB_LIVE_REC);

        JobQueue::RemoveJobsFromMask(JOB_COMMFLAG, autoRunJobs);
    }
}

static bool wait_for_dvb(ChannelBase *channel, int timeout,
                         bool &abortRecordingStart)
{
    (void) channel;
    (void) timeout;
    (void) abortRecordingStart;
#ifdef USING_DVB
    DVBChannel *dvbc = dynamic_cast<DVBChannel*>(channel);
    if (!dvbc)
        return true;

    VERBOSE(VB_RECORD, "TVRec: DVB Recorder waiting for PMT.");
    MythTimer t;
    t.start();
    while (t.elapsed() < timeout && !abortRecordingStart)
    {
        usleep(50);
        if (dvbc->IsPMTSet())
        {
            VERBOSE(VB_RECORD, "TVRec: DVB Recorder's PMT set.");
            if (!dvbc->HasTelevisionService())
            {
                VERBOSE(VB_IMPORTANT, "TVRec: DVB Recorder's PMT "
                        "does not have audio and video.");
            }
            return dvbc->HasTelevisionService();
        }
    }
    VERBOSE(VB_IMPORTANT, "TVRec: DVB Recorder's PMT NOT SET.");
    return false;
#else // if !USING_DVB
    return true;
#endif // !USING_DVB
}

bool TVRec::StartRecorder(bool livetv)
{
    bool ok = true;
    prematurelystopped = false;

    RecordingProfile profile;
    load_recording_profile(profile, curRecording, cardid);

    init_jobs(curRecording, autoRunJobs);

    if (!livetv)
        enable_auto_transcode(profile, curRecording, autoRunJobs);

    ok = SetupRecorder(profile);
    if (!ok || IsErrored() || recorder->IsErrored())
    {
        VERBOSE(VB_IMPORTANT, LOC + "Error setting up recorder -- aborting");
        if (livetv)
        {
            QString message = QString("QUIT_LIVETV %1").arg(cardid);
            MythEvent me(message);
            gContext->dispatch(me);
        }
        else
        {
            rbi->SetRingBuffer(NULL);
        }
        prematurelystopped = true;
        TeardownRecorder(true);
        ok = false;
    }
 
    if (ok)
    {
        if (livetv && SignalMonitor::IsSupported(genOpt.cardtype))
            ok = StartRecorderPostThread(livetv);
        else
            ok = StartRecorderPost(livetv);
    }
    return ok;
}

class srp
{
  public:
    srp(TVRec *_rec, bool _livetv) : rec(_rec), livetv(_livetv) { ; }
    TVRec *rec;
    bool livetv;
};

void *TVRec::StartRecorderPostThunk(void *param)
{
    srp *p = (srp*) param;
    p->rec->StartRecorderPost(p->livetv);
    delete p;
    return NULL;
}

bool TVRec::StartRecorderPostThread(bool livetv)
{
    startingRecording = true;

    if (dummyRecorder && livetv)
        dummyRecorder->StartRecordingThread();

    // Handle rest in a seperate thread
    pthread_create(&start_recorder_thread, NULL,
                   StartRecorderPostThunk, new srp(this, livetv));
    return true;
}

void TVRec::AbortStartRecorderThread()
{
    VERBOSE(VB_IMPORTANT, LOC + "HandleStateChange() "
            "Abort starting recording -- begin");

    abortRecordingStart = true;

    while (startingRecording)
        usleep(200);

    abortRecordingStart = false;

    VERBOSE(VB_IMPORTANT, LOC + "HandleStateChange() "
            "Abort starting recording -- end");
}

bool TVRec::StartRecorderPost(bool livetv)
{
    recorder->SetRecording(curRecording);
    bool ok = true;
    if (channel)
        ok = StartChannel(livetv);

    if (ok)
        ok = wait_for_dvb(channel,
                          genOpt.channel_timeout - genOpt.signal_timeout,
                          abortRecordingStart);

    if (dummyRecorder)
        dummyRecorder->StopRecordingThread();

    if (ok && !CreateRecorderThread())
    {
        VERBOSE(VB_IMPORTANT, LOC + "Failed to start recorder thread -- "
                "aborting recording");
        ok = false;
    }

    if (ok && !abortRecordingStart)
    {
#ifdef USING_V4L
        // Evil
        if (dynamic_cast<Channel*>(channel))
            channel->SetFd(recorder->GetVideoFd());
#endif // USING_V4L

        // Start up real-time comm flag if we can
        if (!livetv)
            enable_realtime_commflag(
                curRecording, autoRunJobs, runJobOnHostOnly,
                transcodeFirst, earlyCommFlag);
    }
    else
    {
        QString common(LOC + "StartRecorderPost(): ");
        if (abortRecordingStart)
            VERBOSE(VB_IMPORTANT, common + "canceled");
        else
        {
            VERBOSE(VB_IMPORTANT, common + "aborting recording");
            if (livetv)
            {
                QString message = QString("QUIT_LIVETV %1").arg(cardid);
                MythEvent me(message);
                gContext->dispatch(me);
            }
        }
        if (!livetv)
        {
            rbi->SetRingBuffer(NULL);
        }

        prematurelystopped = true;

        VERBOSE(VB_RECORD, common + "closeRecorder -- begin");
        TeardownSignalMonitor();
        TeardownRecorder(true);
        CloseChannel();
        VERBOSE(VB_RECORD, common + "closeRecorder -- end");

        stateChangeLock.lock();
        internalState = kState_None;
        stateChangeLock.unlock();
    }

    startingRecording = false;
    return ok;
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
    if (startingRecording)
        AbortStartRecorderThread();

    QMutexLocker lock(&stateChangeLock);

    TVState nextState = internalState;

    ClearFlags(kFlagFrontendReady | kFlagCancelNextRecording);
    SetFlags(kFlagAskAllowRecording);

    bool changed = false;
    bool startRecorder = false;
    bool closeRecorder = false;
    bool killRecordingFile = false;

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

    if (desiredNextState == kState_Error)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "HandleStateChange(): "
                "Attempting to set to an error state.");
        SetFlags(kFlagErrored);
        return;
    }

    if (desiredNextState == kState_None)
    {
        if (waitingForSignal)
        {
            SetSignalMonitoringRate(0, 0);
            StopDummyRecorder();
            recorder->Unpause();
            rbi->Unpause();
            if (!dvbOpt.dvb_on_demand)
            {
                CreateSIParser(-1);
                usleep(500);    // Give DVBCam some breathing room
            }
        }
        if (waitingForDVBTables)
        {
            StopDummyRecorder();
            recorder->Unpause();
            rbi->Unpause();
            if (dvbOpt.dvb_on_demand)
                TeardownSIParser();
        }
    }

    // Handle different state transitions
    if (TRANSITION(kState_None, kState_WatchingLiveTV))
    {
        startRecorder = true;
        SET_NEXT();
    }
    else if (TRANSITION(kState_WatchingLiveTV, kState_None))
    {
        if (!waitingForSignal && channel)
            channel->StoreInputChannels();

        closeRecorder = true;
        killRecordingFile = true;
        SET_NEXT();
    }
    else if (TRANSITION(kState_None, kState_RecordingOnly))
    {
        bool ok = true;
        QString channum, input;
        if (channel)
        {
            ok = curRecording->GetChannel(channum, input);
            channel->Open();
            if (ok)
                ok = channel->SwitchToInput(input, channum);
            CloseChannel();
        }
        if (ok)
        {
            rbi->SetRingBuffer(new RingBuffer(rbi->GetFilename(), true));
            if (rbi->GetRingBuffer()->IsOpen())
            {
                StartedRecording(curRecording);
                startRecorder = true;
                SET_NEXT();
            }
            else
            {
                VERBOSE(VB_IMPORTANT, LOC + "Failed to open ringbuffer. "
                        "Aborting new recording.");
                rbi->SetRingBuffer(NULL);
                SET_LAST();
            }
        }
        else
        {
            VERBOSE(VB_IMPORTANT, LOC +
                    "Failed to set channel for recording. Aborting.");
            SET_LAST();
        }
    }
    else if (TRANSITION(kState_RecordingOnly, kState_None))
    {
        FinishedRecording(curRecording);
        closeRecorder = true;
        inoverrecord = false;

        SET_NEXT();
    }
    waitingForSignal    = false;
    waitingForDVBTables = false;

    QString msg = (changed) ? "Changing from" : "Unknown state transition:";
    VERBOSE(VB_IMPORTANT, LOC + msg + transMsg);
 
    // Handle starting the recorder
    bool livetv = (nextState == kState_WatchingLiveTV);
    if (startRecorder && !StartRecorder(livetv))
        SET_LAST();

    // Handle closing the recorder
    if (closeRecorder)
    {
        VERBOSE(VB_RECORD, LOC + "HandleStateChange()::closeRecorder -- beg");
        TeardownSignalMonitor();
        TeardownRecorder(killRecordingFile);
        CloseChannel();
        VERBOSE(VB_RECORD, LOC + "HandleStateChange()::closeRecorder -- end");
    }

    // update internal state variable
    internalState = nextState;
    changeState = false;
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
    bool use_dummy = SignalMonitor::IsSupported(genOpt.cardtype);
    bool is_atsc   = false;
    recorder = NULL;
    if (genOpt.cardtype == "MPEG")
    {
#ifdef USING_IVTV
        recorder = new MpegRecorder();
#endif // USING_IVTV
    }
    else if (genOpt.cardtype == "HDTV")
    {
#ifdef USING_V4L
        recorder = new HDTVRecorder();
        rbi->GetRingBuffer()->SetWriteBufferSize(4*1024*1024);
        recorder->SetOption("wait_for_seqstart", genOpt.wait_for_seqstart);
        is_atsc = true;
#endif // USING_V4L
    }
    else if (genOpt.cardtype == "FIREWIRE")
    {
#ifdef USING_FIREWIRE
        recorder = new FirewireRecorder();
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
        recorder = new DBox2Recorder(GetDBox2Channel(), cardid);
        recorder->SetOption("port",     dboxOpt.port);
        recorder->SetOption("host",     dboxOpt.host);
        recorder->SetOption("httpport", dboxOpt.httpport);
#endif // USING_DBOX2
    }
    else if (genOpt.cardtype == "DVB")
    {
#ifdef USING_DVB
        recorder = new DVBRecorder(GetDVBChannel());
        recorder->SetOption("wait_for_seqstart", genOpt.wait_for_seqstart);
        recorder->SetOption("hw_decoder",        dvbOpt.hw_decoder);
        recorder->SetOption("recordts",          dvbOpt.recordts);
        recorder->SetOption("dmx_buf_size",      dvbOpt.dmx_buf_size);
        recorder->SetOption("pkt_buf_size",      dvbOpt.pkt_buf_size);
        recorder->SetOption("dvb_on_demand",     dvbOpt.dvb_on_demand);

        use_dummy &= GetDVBRecorder()->RecordsTransportStream();
#if (DVB_API_VERSION_MINOR == 1)
        struct dvb_frontend_info info;
        int fd_frontend = channel->GetFd();
        if (fd_frontend >= 0 && (ioctl(fd_frontend, FE_GET_INFO, &info) >= 0))
            is_atsc = (info.type == FE_ATSC);
#endif // if (DVB_API_VERSION_MINOR == 1)

#endif // USING_DVB
    }
    else
    {
#ifdef USING_V4L
        // V4L/MJPEG/GO7007 from here on
        recorder = new NuppelVideoRecorder(channel);
        recorder->SetOption("skipbtaudio", genOpt.skip_btaudio);
#endif // USING_V4L
    }

    if (use_dummy)
    {
        if (!is_atsc)
            dummyRecorder = new DummyDTVRecorder(
                true, rbi->GetRingBuffer(), 768, 576, 50,
                90, 30000000, false);
        else
            dummyRecorder = new DummyDTVRecorder(
                true, rbi->GetRingBuffer(), 1920, 1088, 29.97,
                90, 30000000, false);
    }

    if (recorder)
    {
        recorder->SetRingBuffer(rbi->GetRingBuffer());
        recorder->SetOptionsFromProfile(
            &profile, genOpt.videodev, genOpt.audiodev, genOpt.vbidev, ispip);
        recorder->Initialize();
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

    if (recorder)
    {
        // This is a bad way to calculate this, the framerate
        // may not be constant if using a DTV based recorder.
        filelen = (int)((float)GetFramesWritten() / GetFramerate());

        QString message = QString("DONE_RECORDING %1 %2")
            .arg(cardid).arg(filelen);
        MythEvent me(message);
        gContext->dispatch(me);

        recorder->StopRecording();

        if (recorder_thread)
            pthread_join(recorder_thread, NULL);
        recorder_thread = static_cast<pthread_t>(0);
        recorder->deleteLater();
        recorder = NULL;
    }

    if (dummyRecorder)
    {
        dummyRecorder->deleteLater();
        dummyRecorder = NULL;
    }

    if (rbi->GetRingBuffer())
    {
        rbi->GetRingBuffer()->StopReads();
        rbi->SetAlive(false);
        rbi->SetRingBuffer(NULL);
    }

    if (killFile)
        rbi->UnlinkRingBufferFile();

    if (curRecording)
    {
        if (autoRunJobs && !killFile && !prematurelystopped)
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

    QString chanorder = gContext->GetSetting("ChannelOrdering", "channum + 0");
    if (inputname.isEmpty())
        channel->SetChannelByString(startchannel);
    else
        channel->SwitchToInput(inputname, startchannel);
    channel->SetChannelOrdering(chanorder);
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


static int get_max_timeout(SignalMonitor *signalMonitor, bool livetv)
{
    int timeout = 2000000000;
    if (!livetv)
    {
        QStringList slist = signalMonitor->GetStatusList(false);
        SignalMonitorList list = SignalMonitorValue::Parse(slist);
        timeout = SignalMonitorValue::MaxWait(list);
    }
    return timeout;
}

static bool wait_for_good_signal(SignalMonitor *signalMonitor, int timeout,
                                 bool &abortRecordingStart)
{
    MythTimer t;
    t.start();
    while (true)
    {
        if (signalMonitor->IsAllGood())
            return true;

        QStringList slist = signalMonitor->GetStatusList(false);
        SignalMonitorList list = SignalMonitorValue::Parse(slist);
        if ((t.elapsed() > timeout) || abortRecordingStart)
        {
            // BEGIN DEBUG CODE
            if (print_verbose_messages & VB_CHANNEL)
            {
                QString flagStr = sm_flags_to_string(signalMonitor->GetFlags());
                VERBOSE(VB_CHANNEL, "SigMon Flags are: "<<flagStr);
                for (uint i = 0; i < list.size(); ++i)
                {
                    VERBOSE(VB_CHANNEL, "SML["<<i<<"]: "
                            <<list.at(i).toString());
                }
            }
            // END DEBUG CODE
            return false;
        }

        usleep(20 * 1000);
    }
}

static int get_program_number(SignalMonitor *sm)
{
    int progNum = -1;
    DTVSignalMonitor* dtvSigMon = dynamic_cast<DTVSignalMonitor*>(sm);
    if (dtvSigMon)
        progNum = dtvSigMon->GetProgramNumber();
    return progNum;
}

bool TVRec::StartChannel(bool livetv)
{
#ifdef USING_V4L
    if (recorder)
    {
        HDTVRecorder *hdtvRec = dynamic_cast<HDTVRecorder*>(recorder);
        if (hdtvRec)
        {
            hdtvRec->StreamData()->Reset(channel->GetMajorChannel(),
                                         channel->GetMinorChannel());
        }
    }
#endif // USING_V4L
    int progNum = -1;

    if (SignalMonitor::IsSupported(genOpt.cardtype))
    {
        SetSignalMonitoringRate(50, livetv);
        if (signalMonitor)
        {
            int timeout = get_max_timeout(signalMonitor, livetv);
            bool ok = wait_for_good_signal(signalMonitor, timeout, 
                                           abortRecordingStart);
            if (!ok)
            {
                VERBOSE(VB_IMPORTANT, LOC +
                        ((abortRecordingStart) ?
                         "StartChannel() -- canceled" :
                         "Timed out waiting for lock -- aborting recording"));
                SetSignalMonitoringRate(0, 0);
                return false;
            }
            progNum = get_program_number(signalMonitor);
        }
        SetSignalMonitoringRate(0, 0);
    }

    SetVideoFiltersForChannel(channel, channel->GetCurrentName());
#ifdef USING_DVB
    DVBChannel *dvbc = dynamic_cast<DVBChannel*>(channel);
    if (dvbc && recorder)
    {
        dvbc->SetPMT(NULL);
        CreateSIParser(progNum);
    }
#endif // USING_DVB
#ifdef USING_V4L
    if (dynamic_cast<Channel*>(channel) && channel->Open())
    {
        channel->SetBrightness();
        channel->SetContrast();
        channel->SetColour();
        channel->SetHue();
        CloseChannel();
    }
#endif // USING_V4L

    return true;
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
    QString msg = "Error: backend compiled without USING_FRONTEND !!!!";
    VERBOSE(VB_IMPORTANT, LOC + msg);
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
        if (!dvbOpt.dvb_on_demand)
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

/** \fn TVRec::StartDummyRecorder(void)
 *  \brief Unpauses the RingBuffer and tells the dummy recorder to 
 *         begin stuffing bits into the Ringbuffer.
 */
void TVRec::StartDummyRecorder(void)
{
    VERBOSE(VB_RECORD, LOC + "StartDummyRecorder() -- begin");
    if (dummyRecorder)
    {
        dummyRecorder->StartRecordingThread();
        rbi->Unpause();
    }
    VERBOSE(VB_RECORD, LOC + "StartDummyRecorder() -- end");
}

/** \fn TVRec::StopDummyRecorder(void)
 *  \brief Tells the dummy recorder to stop stuffing bits into the
 *         RingBuffer and then pauses the RingBuffer.
 */
void TVRec::StopDummyRecorder(void)
{
    VERBOSE(VB_RECORD, LOC + "StopDummyRecorder() -- begin");
    if (dummyRecorder)
    {
        dummyRecorder->StopRecordingThread();
        rbi->Pause();
    }
    VERBOSE(VB_RECORD, LOC + "StopDummyRecorder() -- end");
}

/** \fn TVRec::RunTV()
 *  \brief Event handling method, contains event loop.
 */
void TVRec::RunTV(void)
{ 
    SetFlags(kFlagRunMainLoop);
    ClearFlags(kFlagExitPlayer | kFlagFinishRecording);

    while (HasFlags(kFlagRunMainLoop))
    {
        bool doSleep = true;

        // If there is a state change queued up, do it...
        if (changeState)
            HandleStateChange();

        // Quick exit on fatal errors.
        if (IsErrored())
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "RunTV encountered fatal error, exiting event thread.");
            ClearFlags(kFlagRunMainLoop);
            return;
        }
        if (waitingForSignal)
        {
            if (signalMonitor->IsAllGood())
            {
                VERBOSE(VB_IMPORTANT, LOC + "Got good signal");
                int progNum = get_program_number(signalMonitor);
                SetSignalMonitoringRate(0, 0);
#ifdef USING_DVB
                if (genOpt.cardtype == "DVB")
                {
                    DVBChannel *dvbc = dynamic_cast<DVBChannel*>(channel);
                    dvbc->SetPMT(NULL);
                    CreateSIParser(progNum);
                    waitingForDVBTables = true;
                }
                else
#endif // USING_DVB
                {
                    StopDummyRecorder();
                    recorder->Unpause();
                    rbi->Unpause();
                }
                waitingForSignal = false;
                continue;
            }
        }
#ifdef USING_DVB
        if (waitingForDVBTables)
        {
            if (GetDVBChannel()->IsPMTSet())
            {
                StopDummyRecorder();
                recorder->Unpause();
                recorder->Open();
                rbi->Unpause();
                waitingForDVBTables = false;
            }
        }
#endif // USING_DVB

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
            endTimeLock.lock();
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
            endTimeLock.unlock();
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

        // We should be no more than a few thousand milliseconds,
        // as the end recording code does not have a trigger...
        if (doSleep)
        {
            triggerEventSleep.wakeAll();
            triggerEventLoop.wait(25 /* ms */);
        }
    }
  
    if (GetState() != kState_None)
    {
        ChangeState(kState_None);
        HandleStateChange();
    }
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
#ifdef USING_DVB
        VERBOSE(VB_RECORD, LOC + "SetupSignalMonitor() -- DVB hack begin");
        TeardownSIParser();
        if (GetDVBRecorder())
            GetDVBRecorder()->Close();
        VERBOSE(VB_RECORD, LOC + "SetupSignalMonitor() -- DVB hack end");
#endif

        // TODO reset the tuner hardware
        //channel->SwitchToInput(channel->GetCurrentInputNum(), true);

        signalMonitor =
            SignalMonitor::Init(genOpt.cardtype, GetCaptureCardNum(), channel);
    }
    
    if (signalMonitor)
    {
        VERBOSE(VB_RECORD, LOC + "Signal monitor successfully created");
        // If this is a monitor for Digital TV, initialize table monitors
        if (GetDTVSignalMonitor())
            SetupDTVSignalMonitor();

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
            delete sd;
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
 *  This will actually call SetupSignalMonitor() and 
 *  TeardownSignalMonitor(bool) as needed, so it can
 *  be used directly, without worrying about the 
 *  SignalMonitor instance.
 *
 *  \sa EncoderLink::SetSignalMonitoringRate(int,bool),
 *      RemoteEncoder::SetSignalMonitoringRate(int,int)
 *  \param rate           The update rate to use in milliseconds,
 *                        use 0 to disable, -1 to leave the same.
 *  \param notifyFrontend If 1, SIGNAL messages will be sent to
 *                        the frontend using this recorder, if 0
 *                        they will not be sent, if -1 then messages
 *                        will be sent if they were already being
 *                        sent.
 *  \return Previous update rate
 */
int TVRec::SetSignalMonitoringRate(int rate, int notifyFrontend)
{
    QString msg = "SetSignalMonitoringRate(%1, %2)";
    VERBOSE(VB_RECORD, LOC + msg.arg(rate).arg(notifyFrontend));

    int oldrate = 0; 
    if (signalMonitor)
        oldrate = signalMonitor->GetUpdateRate();

    if (rate == 0)
    {
        TeardownSignalMonitor();
    }
    else if (rate < 0)
    {
        if (signalMonitor && notifyFrontend >= 0)
            signalMonitor->SetNotifyFrontend(notifyFrontend);
    }
    else
    {
        SetupSignalMonitor();
        
        if (signalMonitor)
        {
            signalMonitor->SetUpdateRate(rate);
            if (notifyFrontend >= 0)
                signalMonitor->SetNotifyFrontend(notifyFrontend);
            else if (0 == oldrate)
                signalMonitor->SetNotifyFrontend(false);
        }
        else if (0 != notifyFrontend)
        {
            // send status to frontend, since this may be used in tuning.
            // if this is a card capable of signal monitoring, send error
            // otherwise send an signal lock message.
            bool useMonitor = SignalMonitor::IsSupported(genOpt.cardtype);
            QStringList slist = useMonitor ?
                SignalMonitorValue::ERROR_NO_CHANNEL :
                SignalMonitorValue::SIGNAL_LOCK;
            
            MythEvent me(QString("SIGNAL %1").arg(cardid), slist);
            gContext->dispatch(me);
        }
    }
    return oldrate;
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
    if (!chan)
        return;

    MSqlQuery query(MSqlQuery::InitCon());
    if (!query.isConnected())
        return;

    QString channelinput = chan->GetCurrentInput();

    // Only mysql 4.x can do multi table updates so we need two steps to get 
    // the sourceid from the table join.
    QString querystr = QString(
        "SELECT channel.sourceid "
        "FROM channel, cardinput, capturecard "
        "WHERE channel.channum      = '%1'               AND "
        "      channel.sourceid     = cardinput.sourceid AND "
        "      cardinput.inputname  = '%2'               AND "
        "      cardinput.cardid     = capturecard.cardid AND "
        "      capturecard.cardid   = '%3'               AND "
        "      capturecard.hostname = '%4'")
        .arg(channum).arg(channelinput)
        .arg(cardid)
        .arg(gContext->GetHostName());

    query.prepare(querystr);
    int sourceid = -1;

    if (!query.exec() || !query.isActive())
        MythContext::DBError("setchannelvalue", query);
    else if (query.next())
        sourceid = query.value(0).toInt();

    if (sourceid != -1)
    {
        querystr = QString("UPDATE channel SET channel.%1='%2' "
                           "WHERE channel.channum  = '%3' AND "
                           "      channel.sourceid = '%4'")
            .arg(field_name).arg(value).arg(channum).arg(sourceid);
        query.prepare(querystr);
        query.exec();
    }
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
        VERBOSE(VB_IMPORTANT, LOC +
                "Your channel ordering method \"channel number (numeric)\"\n"
                "\t\t\twill not work with channels like: "<<channum<<"    \n"
                "\t\t\tConsider switching to order by \"database order\"  \n"
                "\t\t\tor \"channel number (alpha)\" in the general       \n"
                "\t\t\tsettings section of the frontend setup             \n");
        channel->SetChannelOrdering(channelorder = "channum");
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
    else if (query.size() > 0)
    {
        query.next();

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
 
        if (!query.exec() || !query.isActive())
        {
            MythContext::DBError("getnextchannel", query);
        }
        else if (query.size() > 0)
        { 
            query.next();

            channum = query.value(0).toString();
            chanid = query.value(1).toString();
        }
    }

    return;
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

    if (rbi->GetRingBuffer())
        return rbi->GetRingBuffer()->GetTotalWritePosition();
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

    if (rbi->GetRingBuffer())
    {
        return totalreadpos + 
            rbi->GetRingBuffer()->GetFileSize() - 
            rbi->GetRingBuffer()->GetTotalWritePosition() -
            rbi->GetRingBuffer()->GetSmudgeSize();
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

    if (rbi->GetRingBuffer())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Can not create RingBuffer.\n\t\t\t"
                "A RingBuffer already exists for LiveTV on this recorder");
        return false;
    }

    ispip = pip;
    filesize = liveTVRingBufSize;
    fillamount = liveTVRingBufFill;

    rbi->SetFilename(liveTVRingBufLoc, cardid);
    path = rbi->GetFilename();

    filesize = filesize * 1024 * 1024 * 1024;
    fillamount = fillamount * 1024 * 1024;

    rbi->SetRingBuffer(new RingBuffer(path, filesize, fillamount));
    if (rbi->GetRingBuffer()->IsOpen())
    {
        rbi->GetRingBuffer()->SetWriteBufferMinWriteSize(1);
        return true;
    }
    else
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to open RingBuffer file.");
        rbi->SetRingBuffer(NULL);
        return false;
    }
}

/** \fn TVRec::SpawnLiveTV()
 *  \brief Tells TVRec to spawn a "Live TV" recorder.
 *  \sa EncoderLink::SpawnLiveTV(), RemoteEncoder::SpawnLiveTV()
 */
void TVRec::SpawnLiveTV(void)
{
    ChangeState(kState_WatchingLiveTV);

    while (changeState)
        usleep(50);
}

/** \fn TVRec::StopLiveTV()
 *  \brief Tells TVRec to stop a "Live TV" recorder.
 *  \sa EncoderLink::StopLiveTV(), RemoteEncoder::StopLiveTV()
 */
void TVRec::StopLiveTV(void)
{
    if (GetState() != kState_None)
    {
        ChangeState(kState_None);
        
        while (changeState)
            usleep(50);
    }
    if (GetState() == kState_None)
    {
        VERBOSE(VB_RECORD, LOC + "StopLiveTV()::closeRecorder -- begin");
        TeardownSignalMonitor();
        TeardownRecorder(true);
        VERBOSE(VB_RECORD, LOC + "StopLiveTV()::closeRecorder -- end rbuffer("
                <<rbi->GetRingBuffer()<<")");
    }
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

/** \fn TVRec::ToggleInputs(void)
 *  \brief Toggles between inputs on current capture card.
 *
 *   You must call PauseRecorder() before calling this.
 */
void TVRec::ToggleInputs(void)
{
    QMutexLocker lock(&stateChangeLock);

    Pause();
    if (channel)
        channel->ToggleInputs();
    Unpause();
}

/** \fn TVRec::ChangeChannel(ChannelChangeDirection)
 *  \brief Changes to a channel in the 'dir' channel change direction.
 *
 *   You must call PauseRecorder() before calling this.
 *
 *  \param dir channel change direction \sa ChannelChangeDirection.
 */
void TVRec::ChangeChannel(ChannelChangeDirection dir)
{
    QMutexLocker lock(&stateChangeLock);

    Pause();
    if (channel)
        channel->SetChannelByDirection(dir);
    Unpause();
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
    QString channelinput = channel->GetCurrentInput();


    MSqlQuery query(MSqlQuery::InitCon());

    QString querystr = QString(
        "SELECT channel.chanid "
        "FROM channel, capturecard, cardinput "
        "WHERE channel.channum      = '%1'               AND "
        "      channel.sourceid     = cardinput.sourceid AND "
        "      cardinput.inputname  = '%2'               AND "
        "      cardinput.cardid     = capturecard.cardid AND "
        "      capturecard.cardid   = '%3'               AND "
        "      capturecard.hostname = '%4'")
        .arg(channum).arg(channelinput).arg(cardid)
        .arg(gContext->GetHostName());

    query.prepare(querystr);

    QString chanid = QString::null;

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        query.next();

        chanid = query.value(0).toString();
    }
    else
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + QString(
                "Channel: \'%1\' was not found in the database.\n"
                "\t\t\tMost likely, your DefaultTVChannel setting is wrong.\n"
                "\t\t\tCould not toggle favorite.").arg(channum));
        return;
    }

    // Check if favorite exists for that chanid...
    querystr = QString(
        "SELECT favorites.favid "
        "FROM favorites "
        "WHERE favorites.chanid = '%1' "
        "LIMIT 1").arg(chanid);

    query.prepare(querystr);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("togglechannelfavorite", query);
    }
    else if (query.size() > 0)
    {
        // We have a favorites record...Remove it to toggle...
        query.next();
        QString favid = query.value(0).toString();

        querystr = QString("DELETE FROM favorites "
                           "WHERE favid = '%1'").arg(favid);

        query.prepare(querystr);
        query.exec();
        VERBOSE(VB_RECORD, LOC + "Removing Favorite.");
    }
    else
    {
        // We have no favorites record...Add one to toggle...
        querystr = QString(
            "INSERT INTO favorites (chanid) VALUES ('%1')").arg(chanid);

        query.prepare(querystr);
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

/** \fn TVRec::SetChannel(QString name)
 *  \brief Changes to a named channel on the current tuner.
 *
 *   You must call PauseRecorder() before calling this.
 *
 *  \param name channel to change to.
 */
void TVRec::SetChannel(QString name)
{
    QMutexLocker lock(&stateChangeLock);

    VERBOSE(VB_RECORD, LOC + QString("SetChannel(%1)").arg(name));
    Pause();
    if (channel)
    {
        QString chan = name.stripWhiteSpace();
        QString prevchan = channel->GetCurrentName();

        if (!channel->SetChannelByString(chan))
        {
            VERBOSE(VB_IMPORTANT, LOC + "SetChannelByString() failed");
            channel->SetChannelByString(prevchan);
        }
    }
    Unpause();
}

/** \fn TVRec::Pause()
 *  \brief Waits for recorder pause and then resets the pauses and
 *         resets ring buffer.
 *  \sa PauseRecorder(), Unpause()
 */
void TVRec::Pause(void)
{
    VERBOSE(VB_RECORD, LOC + "Pause() -- begin");
    QMutexLocker lock(&stateChangeLock);

    bool rbi_pause = true;
    if (waitingForSignal)
    {
        waitingForSignal = false;
        SetSignalMonitoringRate(0, 0);
        StopDummyRecorder();
        rbi_pause = false;
    }
    if (waitingForDVBTables)
    {
        waitingForDVBTables = false;
        StopDummyRecorder();
        rbi_pause = false;
    }

    if (recorder)
        recorder->WaitForPause();

    if (rbi_pause)
        rbi->Pause();

    if (rbi->GetRingBuffer())
        rbi->GetRingBuffer()->Reset();

    VERBOSE(VB_RECORD, LOC + "Pause() -- end");
}

/** \fn TVRec::Unpause()
 *  \brief unpauses recorder and ring buffer.
 *  \sa Pause()
 */
void TVRec::Unpause(void)
{
    VERBOSE(VB_RECORD, LOC + "Unpause() -- begin");
    QMutexLocker lock(&stateChangeLock);

    if (recorder)
        recorder->Reset();

    if (SignalMonitor::IsSupported(genOpt.cardtype))
    {
        StartDummyRecorder();
        SetSignalMonitoringRate(50, 1);
        waitingForSignal = true;
        waitingForDVBTables = false;
    }
    else if (recorder)
    {
        recorder->Unpause();
        rbi->Unpause();
    }

    VERBOSE(VB_RECORD, LOC + "Unpause() -- end");
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
    return rbi->Seek(curpos, pos, whence);
}

/** \fn TVRec::GetReadThreadSocket(void)
 *  \brief Returns RingBuffer data socket, for A/V streaming.
 *  \sa SetReadThreadSock(QSocket*), RequestRingBufferBlock(uint)
 */
QSocket *TVRec::GetReadThreadSocket(void)
{
    return rbi->GetDataSocket();
}

/** \fn TVRec::SetReadThreadSocket(QSocket*)
 *  \brief Sets RingBuffer data socket, for A/V streaming.
 *  \sa GetReadThreadSocket(void), RequestRingBufferBlock(uint)
 */
void TVRec::SetReadThreadSocket(QSocket *sock)
{
    QMutexLocker lock(&stateChangeLock);
    rbi->SetDataSocket(sock);
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
    return rbi->RequestBlock(size);
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

    if (kFlagFrontendReady & f)
        msg += "FrontendReady";
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

    if (msg.isEmpty())
        msg = QString("0x%1").arg(f,0,16);

    return msg;
}
