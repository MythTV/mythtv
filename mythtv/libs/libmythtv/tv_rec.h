#ifndef TVREC_H
#define TVREC_H

// Qt headers
#include <QWaitCondition>
#include <QStringList>
#include <QDateTime>
#include <QRunnable>
#include <QString>
#include <QMap>
#include <QMutex>                       // for QMutex
#include <QReadWriteLock>
#include <QHash>                        // for QHash

// C++ headers
#include <vector>                       // for vector

// MythTV headers
#include "mythtimer.h"
#include "mthread.h"
#include "inputinfo.h"
#include "mythdeque.h"

#include "recordinginfo.h"
#include "tv.h"
#include "signalmonitorlistener.h"
#include "mythtvexp.h"                  // for MTV_PUBLIC
#include "programtypes.h"               // for RecStatus, RecStatus::Type, etc
#include "videoouttypes.h"              // for PictureAttribute

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
    GeneralDBOptions()= default;

    QString m_videoDev;
    QString m_vbiDev;
    QString m_audioDev;
    QString m_inputType       {"V4L"};
    int     m_audioSampleRate {-1};
    bool    m_skipBtAudio     {false};
    uint    m_signalTimeout   {1000};
    uint    m_channelTimeout  {3000};
    bool    m_waitForSeqstart {false};
};

class DVBDBOptions
{
  public:
    DVBDBOptions() = default;

    bool m_dvbOnDemand    {false};
    uint m_dvbTuningDelay {0};
    bool m_dvbEitScan     {true};
};

class FireWireDBOptions
{
  public:
    FireWireDBOptions() = default;

    int     m_speed      {-1};
    int     m_connection {-1};
    QString m_model;
};

class TuningRequest
{
  public:
    explicit TuningRequest(uint f) :
        m_flags(f) {;}
    TuningRequest(uint f, RecordingInfo *p) :
        m_flags(f), m_program(p) {;}
    TuningRequest(uint f, const QString& ch, const QString& in = QString()) :
        m_flags(f), m_channel(ch), m_input(in) {;}

    QString toString(void) const;

    bool IsOnSameMultiplex(void) const { return m_minorChan || (m_progNum >= 0); }

  public:
    uint           m_flags;
    RecordingInfo *m_program   {nullptr};
    QString        m_channel;
    QString        m_input;
    uint           m_majorChan {0};
    uint           m_minorChan {0};
    int            m_progNum   {-1};
};
using TuningQueue = MythDeque<TuningRequest>;
inline TuningRequest myth_deque_init(const TuningRequest*) { return (TuningRequest)(0); }

class PendingInfo
{
  public:
    PendingInfo() = default;

    ProgramInfo *m_info              {nullptr};
    QDateTime    m_recordingStart;
    bool         m_hasLaterShowing   {false};
    bool         m_canceled          {false};
    bool         m_ask               {false};
    bool         m_doNotAsk          {false};
    vector<uint> m_possibleConflicts;
};
using PendingMap = QMap<uint,PendingInfo>;

class MTV_PUBLIC TVRec : public SignalMonitorListener, public QRunnable
{
    Q_DECLARE_TR_FUNCTIONS(TVRec)

    friend class TuningRequest;
    friend class TVRecRecordThread;

  public:
    explicit TVRec(int _inputid);
   ~TVRec(void);

    bool Init(void);

    void RecordPending(const ProgramInfo *rcinfo, int secsleft, bool hasLater);
    RecStatus::Type StartRecording(ProgramInfo *pginfo);
    RecStatus::Type GetRecordingStatus(void) const;

    void StopRecording(bool killFile = false);
    /// \brief Tells TVRec to finish the current recording as soon as possible.
    void FinishRecording(void)  { SetFlags(kFlagFinishRecording,
                                           __FILE__, __LINE__); }
    /// \brief Tells TVRec that the frontend's TV class is ready for messages.
    void FrontendReady(void)    { SetFlags(kFlagFrontendReady,
                                           __FILE__, __LINE__); }
    void CancelNextRecording(bool cancel);
    ProgramInfo *GetRecording(void);

    /// \brief Returns true if event loop has not been told to shut down
    bool IsRunning(void)  const { return HasFlags(kFlagRunMainLoop); }
    /// \brief Tells TVRec to stop event loop
    void Stop(void) { ClearFlags(kFlagRunMainLoop, __FILE__, __LINE__); }

    TVState GetState(void) const;
    /// \brief Returns "state == kState_RecordingPreRecorded"
    bool IsPlaying(void) { return StateIsPlaying(m_internalState); }
    /// \brief Returns "state == kState_RecordingRecordedOnly"
    /// \sa IsReallyRecording()
    bool IsRecording(void) { return StateIsRecording(m_internalState); }

    bool SetVideoFiltersForChannel(uint sourceid, const QString &channum);

    bool IsBusy(InputInfo *busy_input = nullptr, int time_buffer = 5) const;
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
    void ToggleChannelFavorite(const QString&);

    void SetLiveRecording(int recording);

    QString     GetInput(void) const;
    uint        GetSourceID(void) const;
    QString     SetInput(QString input);

    /// Changes to a channel in the 'dir' channel change direction.
    void ChangeChannel(ChannelChangeDirection dir)
        { SetChannel(QString("NextChannel %1").arg((int)dir)); }
    void SetChannel(const QString& name, uint requestType = kFlagDetect);
    bool QueueEITChannelChange(const QString &name);

    int SetSignalMonitoringRate(int rate, int notifyFrontend = 1);
    int  GetPictureAttribute(PictureAttribute attr);
    int  ChangePictureAttribute(PictureAdjustType type, PictureAttribute attr,
                                bool direction);
    bool CheckChannel(const QString& name) const;
    bool ShouldSwitchToAnotherInput(const QString& chanid);
    bool CheckChannelPrefix(const QString&,uint&,bool&,QString&);
    void GetNextProgram(BrowseDirection direction,
                        QString &title,       QString &subtitle,
                        QString &desc,        QString &category,
                        QString &starttime,   QString &endtime,
                        QString &callsign,    QString &iconpath,
                        QString &channum,     uint    &chanid,
                        QString &seriesid,    QString &programid);
    bool GetChannelInfo(uint &chanid, uint &sourceid,
                        QString &callsign, QString &channum,
                        QString &channame, QString &xmltvid) const;
    bool SetChannelInfo(uint chanid, uint sourceid, const QString& oldchannum,
                        const QString& callsign, const QString& channum,
                        const QString& channame, const QString& xmltvid);

    /// \brief Returns the inputid
    uint GetInputId(void) { return m_inputid; }
    uint GetParentId(void) { return m_parentid; }
    uint GetMajorId(void) { return m_parentid ? m_parentid : m_inputid; }
    /// \brief Returns true is "errored" is true, false otherwise.
    bool IsErrored(void)  const { return HasFlags(kFlagErrored); }

    void RingBufferChanged(RingBuffer*, RecordingInfo*, RecordingQuality*);
    void RecorderPaused(void);

    void SetNextLiveTVDir(QString dir);

    uint GetFlags(void) const { return m_stateFlags; }

    static TVRec *GetTVRec(uint inputid);

    void AllGood(void) override { WakeEventLoop(); } // SignalMonitorListener
    void StatusChannelTuned(const SignalMonitorValue&) override { } // SignalMonitorListener
    void StatusSignalLock(const SignalMonitorValue&) override { } // SignalMonitorListener
    void StatusSignalStrength(const SignalMonitorValue&) override { } // SignalMonitorListener

  protected:
    void run(void) override; // QRunnable
    bool WaitForEventThreadSleep(bool wake = true, ulong time = ULONG_MAX);

  private:
    void SetRingBuffer(RingBuffer *);
    void SetPseudoLiveTVRecording(RecordingInfo*);
    void TeardownAll(void);
    void WakeEventLoop(void);

    static bool GetDevices(uint inputid,
                           uint &parentid,
                           GeneralDBOptions   &gen_opts,
                           DVBDBOptions       &dvb_opts,
                           FireWireDBOptions  &firewire_opts);

    static QString GetStartChannel(uint inputid);

    void TeardownRecorder(uint request_flags);
    DTVRecorder  *GetDTVRecorder(void);

    bool CreateChannel(const QString &startchannel,
                       bool enter_power_save_mode);
    void CloseChannel(void);
    DTVChannel *GetDTVChannel(void);
    V4LChannel *GetV4LChannel(void);

    bool SetupSignalMonitor(
        bool tablemon, bool EITscan, bool notify);
    bool SetupDTVSignalMonitor(bool EITscan);
    void TeardownSignalMonitor(void);
    DTVSignalMonitor *GetDTVSignalMonitor(void);

    bool HasFlags(uint f) const { return (m_stateFlags & f) == f; }
    void SetFlags(uint f, const QString & file, int line);
    void ClearFlags(uint f, const QString & file, int line);
    static QString FlagToString(uint);

    void HandleTuning(void);
    void TuningShutdowns(const TuningRequest&);
    void TuningFrequency(const TuningRequest&);
    MPEGStreamData *TuningSignalCheck(void);

    void TuningNewRecorder(MPEGStreamData*);
    void TuningRestartRecorder(void);
    QString TuningGetChanNum(const TuningRequest&, QString &input) const;
    bool TuningOnSameMultiplex(TuningRequest &request);

    void HandleStateChange(void);
    void ChangeState(TVState nextState);
    static bool StateIsRecording(TVState state);
    static bool StateIsPlaying(TVState state);
    TVState RemovePlaying(TVState state);
    TVState RemoveRecording(TVState state);

    void HandlePendingRecordings(void);

    bool WaitForNextLiveTVDir(void);
    bool GetProgramRingBufferForLiveTV(RecordingInfo **pginfo, RingBuffer **rb,
                                       const QString &channum);
    bool CreateLiveTVRingBuffer(const QString & channum);
    bool SwitchLiveTVRingBuffer(const QString & channum,
                                bool discont, bool set_rec);

    RecordingInfo *SwitchRecordingRingBuffer(const RecordingInfo &rcinfo);

    void StartedRecording(RecordingInfo*);
    void FinishedRecording(RecordingInfo*, RecordingQuality*);
    QDateTime GetRecordEndTime(const ProgramInfo*) const;
    void CheckForRecGroupChange(void);
    void NotifySchedulerOfRecording(RecordingInfo*);
    enum AutoRunInitType { kAutoRunProfile, kAutoRunNone, };
    void InitAutoRunJobs(RecordingInfo*, AutoRunInitType,
                         RecordingProfile *, int line);

    void SetRecordingStatus(
        RecStatus::Type new_status, int line, bool have_lock = false);

    QString LoadProfile(void*, RecordingInfo*,
                        RecordingProfile&);

    // Various components TVRec coordinates
    RecorderBase      *m_recorder                 {nullptr};
    ChannelBase       *m_channel                  {nullptr};
    SignalMonitor     *m_signalMonitor            {nullptr};
    EITScanner        *m_scanner                  {nullptr};

    QDateTime          m_signalEventCmdTimeout;
    bool               m_signalEventCmdSent       {false};

    QDateTime          m_startRecordingDeadline;
    QDateTime          m_signalMonitorDeadline;
    uint               m_signalMonitorCheckCnt    {0};
    bool               m_reachedRecordingDeadline {false};
    QDateTime          m_preFailDeadline;
    bool               m_reachedPreFail           {false};

    // Various threads
    /// Event processing thread, runs TVRec::run().
    MThread           *m_eventThread              {nullptr};
    /// Recorder thread, runs RecorderBase::run().
    MThread           *m_recorderThread           {nullptr};

    // Configuration variables from database
    bool               m_transcodeFirst           {false};
    bool               m_earlyCommFlag            {false};
    bool               m_runJobOnHostOnly         {false};
    int                m_eitCrawlIdleStart        {60};
    int                m_eitTransportTimeout      {5*60};
    int                m_audioSampleRateDB        {0};
    int                m_overRecordSecNrml        {0};
    int                m_overRecordSecCat         {0};
    QString            m_overRecordCategory;

    // Configuration variables from setup routines
    uint               m_inputid;
    uint               m_parentid                 {0};
    bool               m_ispip                    {false};

    // Configuration variables from database, based on inputid
    GeneralDBOptions   m_genOpt;
    DVBDBOptions       m_dvbOpt;
    FireWireDBOptions  m_fwOpt;

    QString            m_recProfileName;

    // State variables
    mutable QMutex     m_setChannelLock;
    mutable QMutex     m_stateChangeLock          {QMutex::Recursive};
    mutable QMutex     m_pendingRecLock           {QMutex::Recursive};
    TVState            m_internalState            {kState_None};
    TVState            m_desiredNextState         {kState_None};
    bool               m_changeState              {false};
    bool               m_pauseNotify              {true};
    uint               m_stateFlags               {0};
    TuningQueue        m_tuningRequests;
    TuningRequest      m_lastTuningRequest        {0};
    QDateTime          m_eitScanStartTime;
    mutable QMutex     m_triggerEventLoopLock     {QMutex::NonRecursive};
    QWaitCondition     m_triggerEventLoopWait;
    bool               m_triggerEventLoopSignal   {false};
    mutable QMutex     m_triggerEventSleepLock    {QMutex::NonRecursive};
    QWaitCondition     m_triggerEventSleepWait;
    bool               m_triggerEventSleepSignal  {false};
    volatile bool      m_switchingBuffer          {false};
    RecStatus::Type    m_recStatus                {RecStatus::Unknown};

    // Current recording info
    RecordingInfo     *m_curRecording             {nullptr};
    QDateTime          m_recordEndTime;
     // RecordingInfo::MakeUniqueKey()->autoRun
    QHash<QString,int> m_autoRunJobs;
    int                m_overrecordseconds        {0};

    // Pending recording info
    PendingMap         m_pendingRecordings;

    // Pseudo LiveTV recording
    RecordingInfo     *m_pseudoLiveTVRecording    {nullptr};
    QString            m_nextLiveTVDir;
    QMutex             m_nextLiveTVDirLock;
    QWaitCondition     m_triggerLiveTVDir;
    QString            m_liveTVStartChannel;

    // LiveTV file chain
    LiveTVChain       *m_tvChain                  {nullptr};

    // RingBuffer info
    RingBuffer        *m_ringBuffer               {nullptr};
    QString            m_rbFileExt                {"ts"};

  public:
    static QReadWriteLock    s_inputsLock;
    static QMap<uint,TVRec*> s_inputs;

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
