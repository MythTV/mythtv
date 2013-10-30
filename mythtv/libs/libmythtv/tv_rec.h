#ifndef TVREC_H
#define TVREC_H

// Qt headers
#include <QWaitCondition>
#include <QStringList>
#include <QDateTime>
#include <QRunnable>
#include <QString>
#include <QMap>

// MythTV headers
#include "mythtimer.h"
#include "mthread.h"
#include "inputinfo.h"
#include "inputgroupmap.h"
#include "mythdeque.h"
#include "recordinginfo.h"
#include "tv.h"
#include "signalmonitorlistener.h"

#include "mythconfig.h"

// locking order
// setChannelLock -> stateChangeLock -> triggerEventLoopLock
//                                   -> pendingRecLock

class RingBuffer;
class EITScanner;
class RecordingProfile;
class LiveTVChain;

class RecorderBase;
class DTVRecorder;
class DVBRecorder;
class HDHRRecorder;
class ASIRecorder;
class CetonRecorder;

class SignalMonitor;
class DTVSignalMonitor;

class ChannelBase;
class DTVChannel;
class DVBChannel;
class FirewireChannel;
class V4LChannel;
class HDHRChannel;
class CetonChannel;

class MPEGStreamData;
class ProgramMapTable;
class RecordingQuality;

class GeneralDBOptions
{
  public:
    GeneralDBOptions() :
        videodev(""),         vbidev(""),
        audiodev(""),
        cardtype("V4L"),
        audiosamplerate(-1),  skip_btaudio(false),
        signal_timeout(1000), channel_timeout(3000),
        wait_for_seqstart(false) {}
  
    QString videodev;
    QString vbidev;
    QString audiodev;
    QString cardtype;
    int     audiosamplerate;
    bool    skip_btaudio;
    uint    signal_timeout;
    uint    channel_timeout;
    bool    wait_for_seqstart;
};

class DVBDBOptions
{
  public:
    DVBDBOptions() : dvb_on_demand(false), dvb_tuning_delay(0), dvb_eitscan(true) {;}
    bool dvb_on_demand;
    uint dvb_tuning_delay;
    bool dvb_eitscan;
};

class FireWireDBOptions
{
  public:
    FireWireDBOptions() : speed(-1), connection(-1), model("") {;}

    int speed;
    int connection;
    QString model;
};

class TuningRequest
{
  public:
    TuningRequest(uint f) :
        flags(f), program(NULL), channel(QString::null),
        input(QString::null), majorChan(0), minorChan(0), progNum(-1) {;}
    TuningRequest(uint f, RecordingInfo *p) :
        flags(f), program(p), channel(QString::null),
        input(QString::null), majorChan(0), minorChan(0), progNum(-1) {;}
    TuningRequest(uint f, QString ch, QString in = QString::null) :
        flags(f), program(NULL), channel(ch),
        input(in), majorChan(0), minorChan(0), progNum(-1) {;}

    QString toString(void) const;

    bool IsOnSameMultiplex(void) const { return minorChan || (progNum >= 0); }

  public:
    uint         flags;
    RecordingInfo *program;
    QString      channel;
    QString      input;
    uint         majorChan;
    uint         minorChan;
    int          progNum;
};
typedef MythDeque<TuningRequest> TuningQueue;

class PendingInfo
{
  public:
    PendingInfo() :
        info(NULL), hasLaterShowing(false), canceled(false),
        ask(false), doNotAsk(false) { }
    ProgramInfo *info;
    QDateTime    recordingStart;
    bool         hasLaterShowing;
    bool         canceled;
    bool         ask;
    bool         doNotAsk;
    vector<uint> possibleConflicts;
};
typedef QMap<uint,PendingInfo> PendingMap;

class MTV_PUBLIC TVRec : public SignalMonitorListener, public QRunnable
{
    friend class TuningRequest;
    friend class TVRecRecordThread;

  public:
    TVRec(int capturecardnum);
   ~TVRec(void);

    bool Init(void);

    void RecordPending(const ProgramInfo *rcinfo, int secsleft, bool hasLater);
    RecStatusType StartRecording(const ProgramInfo *rcinfo);
    RecStatusType GetRecordingStatus(void) const;

    void StopRecording(bool killFile = false);
    /// \brief Tells TVRec to finish the current recording as soon as possible.
    void FinishRecording(void)  { SetFlags(kFlagFinishRecording); }
    /// \brief Tells TVRec that the frontend's TV class is ready for messages.
    void FrontendReady(void)    { SetFlags(kFlagFrontendReady); }
    void CancelNextRecording(bool cancel);
    ProgramInfo *GetRecording(void);

    /// \brief Returns true if event loop has not been told to shut down
    bool IsRunning(void)  const { return HasFlags(kFlagRunMainLoop); }
    /// \brief Tells TVRec to stop event loop
    void Stop(void)             { ClearFlags(kFlagRunMainLoop); }

    TVState GetState(void) const;
    /// \brief Returns "state == kState_RecordingPreRecorded"
    bool IsPlaying(void) { return StateIsPlaying(internalState); }
    /// \brief Returns "state == kState_RecordingRecordedOnly"
    /// \sa IsReallyRecording()
    bool IsRecording(void) { return StateIsRecording(internalState); }

    bool SetVideoFiltersForChannel(uint sourceid, const QString &channum);

    bool IsBusy(TunedInputInfo *busy_input = NULL, int time_buffer = 5) const;
    bool IsReallyRecording(void);

    float GetFramerate(void);
    long long GetFramesWritten(void);
    long long GetFilePosition(void);
    long long GetMaxBitrate(void) const;
    int64_t GetKeyframePosition(uint64_t desired) const;
    bool GetKeyframePositions(int64_t start, int64_t end, frm_pos_map_t&) const;
    bool GetKeyframeDurations(int64_t start, int64_t end, frm_pos_map_t&) const;
    void SpawnLiveTV(LiveTVChain *newchain, bool pip, QString startchan);
    QString GetChainID(void);
    void StopLiveTV(void);
    void PauseRecorder(void);
    void ToggleChannelFavorite(QString);

    void SetLiveRecording(int recording);

    vector<InputInfo> GetFreeInputs(const vector<uint> &excluded_cards) const;
    QString     GetInput(void) const;
    uint        GetSourceID(void) const;
    QString     SetInput(QString input, uint requestType = kFlagDetect);

    /// Changes to a channel in the 'dir' channel change direction.
    void ChangeChannel(ChannelChangeDirection dir)
        { SetChannel(QString("NextChannel %1").arg((int)dir)); }
    void SetChannel(QString name, uint requestType = kFlagDetect);
    bool QueueEITChannelChange(const QString &name);

    int SetSignalMonitoringRate(int msec, int notifyFrontend = 1);
    int  GetPictureAttribute(PictureAttribute attr);
    int  ChangePictureAttribute(PictureAdjustType type, PictureAttribute attr,
                                bool direction);
    bool CheckChannel(QString name) const;
    bool ShouldSwitchToAnotherCard(QString chanid);
    bool CheckChannelPrefix(const QString&,uint&,bool&,QString&);
    void GetNextProgram(BrowseDirection direction,
                        QString &title,       QString &subtitle,
                        QString &desc,        QString &category,
                        QString &starttime,   QString &endtime,
                        QString &callsign,    QString &iconpath,
                        QString &channelname, uint    &chanid,
                        QString &seriesid,    QString &programid);
    bool GetChannelInfo(uint &chanid, uint &sourceid,
                        QString &callsign, QString &channum,
                        QString &channame, QString &xmltvid) const;
    bool SetChannelInfo(uint chanid, uint sourceid, QString oldchannum,
                        QString callsign, QString channum,
                        QString channame, QString xmltvid);

    /// \brief Returns the caputure card number
    uint GetCaptureCardNum(void) { return cardid; }
    /// \brief Returns true is "errored" is true, false otherwise.
    bool IsErrored(void)  const { return HasFlags(kFlagErrored); }

    void RingBufferChanged(RingBuffer*, RecordingInfo*, RecordingQuality*);
    void RecorderPaused(void);

    void SetNextLiveTVDir(QString dir);

    uint GetFlags(void) const { return stateFlags; }

    static TVRec *GetTVRec(uint cardid);

    virtual void AllGood(void) { WakeEventLoop(); }
    virtual void StatusChannelTuned(const SignalMonitorValue&) { }
    virtual void StatusSignalLock(const SignalMonitorValue&) { }
    virtual void StatusSignalStrength(const SignalMonitorValue&) { }

  protected:
    virtual void run(void); // QRunnable
    bool WaitForEventThreadSleep(bool wake = true, ulong time = ULONG_MAX);

  private:
    void SetRingBuffer(RingBuffer *);
    void SetPseudoLiveTVRecording(RecordingInfo*);
    void TeardownAll(void);
    void WakeEventLoop(void);

    static bool GetDevices(uint cardid,
                           GeneralDBOptions   &general_opts,
                           DVBDBOptions       &dvb_opts,
                           FireWireDBOptions  &firewire_opts);

    static QString GetStartChannel(uint cardid, const QString &startinput);

    void TeardownRecorder(uint request_flags);
    DTVRecorder  *GetDTVRecorder(void);

    bool CreateChannel(const QString &startChanNum,
                       bool enter_power_save_mode);
    void CloseChannel(void);
    DTVChannel *GetDTVChannel(void);
    V4LChannel *GetV4LChannel(void);

    bool SetupSignalMonitor(
        bool enable_table_monitoring, bool EITscan, bool notify);
    bool SetupDTVSignalMonitor(bool EITscan);
    void TeardownSignalMonitor(void);
    DTVSignalMonitor *GetDTVSignalMonitor(void);

    bool HasFlags(uint f) const { return (stateFlags & f) == f; }
    void SetFlags(uint f);
    void ClearFlags(uint f);
    static QString FlagToString(uint);

    void HandleTuning(void);
    void TuningShutdowns(const TuningRequest&);
    void TuningFrequency(const TuningRequest&);
    MPEGStreamData *TuningSignalCheck(void);

    void TuningNewRecorder(MPEGStreamData*);
    void TuningRestartRecorder(void);
    QString TuningGetChanNum(const TuningRequest&, QString &input) const;
    uint TuningCheckForHWChange(const TuningRequest&,
                                QString &channum,
                                QString &inputname);
    bool TuningOnSameMultiplex(TuningRequest &request);

    void HandleStateChange(void);
    void ChangeState(TVState nextState);
    bool StateIsRecording(TVState state);
    bool StateIsPlaying(TVState state);
    TVState RemovePlaying(TVState state);
    TVState RemoveRecording(TVState state);

    void HandlePendingRecordings(void);

    bool WaitForNextLiveTVDir(void);
    bool GetProgramRingBufferForLiveTV(RecordingInfo **pginfo, RingBuffer **rb,
                                       const QString &channum, int inputID);
    bool CreateLiveTVRingBuffer(const QString & channum);
    bool SwitchLiveTVRingBuffer(const QString & channum,
                                bool discont, bool set_rec);

    RecordingInfo *SwitchRecordingRingBuffer(const RecordingInfo &rcinfo);

    void StartedRecording(RecordingInfo*);
    void FinishedRecording(RecordingInfo*, RecordingQuality*);
    QDateTime GetRecordEndTime(const ProgramInfo*) const;
    void CheckForRecGroupChange(void);
    void NotifySchedulerOfRecording(RecordingInfo*);
    typedef enum { kAutoRunProfile, kAutoRunNone, } AutoRunInitType;
    void InitAutoRunJobs(RecordingInfo*, AutoRunInitType,
                         RecordingProfile *, int line);

    void SetRecordingStatus(
        RecStatusType new_status, int line, bool have_lock = false);

    // Various components TVRec coordinates
    RecorderBase     *recorder;
    ChannelBase      *channel;
    SignalMonitor    *signalMonitor;
    EITScanner       *scanner;

    QDateTime         signalMonitorDeadline;
    uint              signalMonitorCheckCnt;

    // Various threads
    /// Event processing thread, runs TVRec::run().
    MThread          *eventThread;
    /// Recorder thread, runs RecorderBase::run().
    MThread          *recorderThread;

    // Configuration variables from database
    bool    transcodeFirst;
    bool    earlyCommFlag;
    bool    runJobOnHostOnly;
    int     eitCrawlIdleStart;
    int     eitTransportTimeout;
    int     audioSampleRateDB;
    int     overRecordSecNrml;
    int     overRecordSecCat;
    QString overRecordCategory;
    InputGroupMap igrp;

    // Configuration variables from setup routines
    uint              cardid;
    bool              ispip;

    // Configuration variables from database, based on cardid
    GeneralDBOptions   genOpt;
    DVBDBOptions       dvbOpt;
    FireWireDBOptions  fwOpt;

    // State variables
    mutable QMutex setChannelLock;
    mutable QMutex stateChangeLock;
    mutable QMutex pendingRecLock;
    TVState        internalState;
    TVState        desiredNextState;
    bool           changeState;
    bool           pauseNotify;
    uint           stateFlags;
    TuningQueue    tuningRequests;
    TuningRequest  lastTuningRequest;
    QDateTime      eitScanStartTime;
    mutable QMutex triggerEventLoopLock;
    QWaitCondition triggerEventLoopWait;
    bool           triggerEventLoopSignal;
    mutable QMutex triggerEventSleepLock;
    QWaitCondition triggerEventSleepWait;
    bool           triggerEventSleepSignal;
    volatile bool  switchingBuffer;
    RecStatusType  m_recStatus;

    // Current recording info
    RecordingInfo *curRecording;
    QDateTime    recordEndTime;
    QHash<QString,int> autoRunJobs; // RecordingInfo::MakeUniqueKey()->autoRun
    int          overrecordseconds;

    // Pending recording info
    PendingMap   pendingRecordings;

    // Pseudo LiveTV recording
    RecordingInfo *pseudoLiveTVRecording;
    QString      nextLiveTVDir;
    QMutex       nextLiveTVDirLock;
    QWaitCondition triggerLiveTVDir;
    QString      LiveTVStartChannel;

    // LiveTV file chain
    LiveTVChain *tvchain;

    // RingBuffer info
    RingBuffer  *ringBuffer;
    QString      rbFileExt;

  public:
    static QMutex            cardsLock;
    static QMap<uint,TVRec*> cards;

  public:
    static const uint kSignalMonitoringRate;

    // General State flags
    static const uint kFlagFrontendReady        = 0x00000001;
    static const uint kFlagRunMainLoop          = 0x00000002;
    static const uint kFlagExitPlayer           = 0x00000004;
    static const uint kFlagFinishRecording      = 0x00000008;
    static const uint kFlagErrored              = 0x00000010;
    static const uint kFlagCancelNextRecording  = 0x00000020;

    // Tuning flags
    /// final result desired is LiveTV recording
    static const uint kFlagLiveTV               = 0x00000100;
    /// final result desired is a timed recording
    static const uint kFlagRecording            = 0x00000200;
    /// antenna adjusting mode (LiveTV without recording).
    static const uint kFlagAntennaAdjust        = 0x00000400;
    static const uint kFlagRec                  = 0x00000F00;

    // Non-recording Commands
    /// final result desired is an EIT Scan
    static const uint kFlagEITScan              = 0x00001000;
    /// close recorder, keep recording
    static const uint kFlagCloseRec             = 0x00002000;
    /// close recorder, discard recording
    static const uint kFlagKillRec              = 0x00004000;

    static const uint kFlagNoRec                = 0x0000F000;
    static const uint kFlagKillRingBuffer       = 0x00010000;

    // Waiting stuff
    static const uint kFlagWaitingForRecPause   = 0x00100000;
    static const uint kFlagWaitingForSignal     = 0x00200000;
    static const uint kFlagNeedToStartRecorder  = 0x00800000;
    static const uint kFlagPendingActions       = 0x00F00000;

    // Running stuff
    static const uint kFlagSignalMonitorRunning = 0x01000000;
    static const uint kFlagEITScannerRunning    = 0x04000000;

    static const uint kFlagDummyRecorderRunning = 0x10000000;
    static const uint kFlagRecorderRunning      = 0x20000000;
    static const uint kFlagAnyRecRunning        = 0x30000000;
    static const uint kFlagAnyRunning           = 0x3F000000;

    // Tuning state
    static const uint kFlagRingBufferReady      = 0x40000000;
    static const uint kFlagDetect               = 0x80000000;
};

#endif
