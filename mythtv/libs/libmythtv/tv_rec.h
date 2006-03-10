#ifndef TVREC_H
#define TVREC_H

#include <qstring.h>
#include <pthread.h>
#include <qdatetime.h>
#include <qvaluelist.h>
#include <qptrlist.h>
#include <qmap.h>
#include <qstringlist.h>
#include <qwaitcondition.h>

#include "mythdeque.h"
#include "programinfo.h"
#include "tv.h"

class QSocket;
class NuppelVideoRecorder;
class RingBuffer;
class EITScanner;
class DVBSIParser;
class DummyDTVRecorder;
class RecordingProfile;
class LiveTVChain;

class RecorderBase;
class DVBRecorder;
class HDTVRecorder;

class SignalMonitor;
class DTVSignalMonitor;

class ChannelBase;
class DBox2Channel;
class DVBChannel;
class Channel;

class MPEGStreamData;
class ProgramMapTable;

/// Used to request ProgramInfo for channel browsing.
typedef enum
{
    BROWSE_SAME,    ///< Fetch browse information on current channel and time
    BROWSE_UP,      ///< Fetch information on previous channel
    BROWSE_DOWN,    ///< Fetch information on next channel
    BROWSE_LEFT,    ///< Fetch information on current channel in the past
    BROWSE_RIGHT,   ///< Fetch information on current channel in the future
    BROWSE_FAVORITE ///< Fetch information on the next favorite channel
} BrowseDirections;

class GeneralDBOptions
{
  public:
    GeneralDBOptions() :
        videodev(""),         vbidev(""),
        audiodev(""),         defaultinput("Television"),
        cardtype("V4L"),
        audiosamplerate(-1),  skip_btaudio(false),
        signal_timeout(1000), channel_timeout(3000) {;}
  
    QString videodev;
    QString vbidev;
    QString audiodev;
    QString defaultinput;
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
    DVBDBOptions() : dvb_on_demand(false) {;}
    bool dvb_on_demand;
};

class FireWireDBOptions
{
  public:
    FireWireDBOptions() :
        port(-1), node(-1), speed(-1), connection(-1), model("") {;}
        
    int port;
    int node;
    int speed;
    int connection;
    QString model;
};

class DBox2DBOptions
{
  public:
    DBox2DBOptions() : port(-1), httpport(-1), host("") {;}

    int port;
    int httpport;
    QString host;
};

class TuningRequest
{
  public:
    TuningRequest(uint f) :
        flags(f), program(NULL), channel(QString::null),
        input(QString::null) {;}
    TuningRequest(uint f, ProgramInfo *p) :
        flags(f), program(p), channel(QString::null), input(QString::null) {;}
    TuningRequest(uint f, QString ch, QString in = QString::null) :
        flags(f), program(NULL), channel(ch), input(in) {;}

    QString toString(void) const;

  public:
    uint         flags;
    ProgramInfo *program;
    QString      channel;
    QString      input;
};
typedef MythDeque<TuningRequest> TuningQueue;

class TVRec : public QObject
{
    friend class TuningRequest;
    Q_OBJECT
  public:
    TVRec(int capturecardnum);
   ~TVRec(void);

    bool Init(void);

    void RecordPending(const ProgramInfo *rcinfo, int secsleft);
    RecStatusType StartRecording(const ProgramInfo *rcinfo);

    void StopRecording(void);
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

    TVState GetState(void);
    /// \brief Returns "state == kState_RecordingPreRecorded"
    bool IsPlaying(void) { return StateIsPlaying(internalState); }
    /// \brief Returns "state == kState_RecordingRecordedOnly"
    /// \sa IsReallyRecording()
    bool IsRecording(void) { return StateIsRecording(internalState); }

    void SetChannelValue(QString &field_name,int value, ChannelBase *chan,
                         const QString &channum);
    int GetChannelValue(const QString &channel_field, ChannelBase *chan, 
                        const QString &channum);
    bool SetVideoFiltersForChannel(ChannelBase *chan, const QString &channum);
    QString GetNextChannel(ChannelBase *chan, int channeldirection);
    QString GetNextRelativeChanID(QString channum, int channeldirection);
    void DoGetNextChannel(QString &channum, QString channelinput,
                          int cardid, QString channelorder,
                          int channeldirection, QString &chanid);

    bool IsBusy(void);
    bool IsReallyRecording(void);

    float GetFramerate(void);
    long long GetFramesWritten(void);
    long long GetFilePosition(void);
    long long GetMaxBitrate();
    long long GetKeyframePosition(long long desired);
    void SpawnLiveTV(LiveTVChain *newchain, bool pip = false);
    QString GetChainID(void);
    void StopLiveTV(void);
    void PauseRecorder(void);
    void ToggleChannelFavorite(void);

    void SetLiveRecording(int recording);

    QStringList GetConnectedInputs(void) const;
    QString     GetInput(void) const;
    QString     SetInput(QString input, uint requestType = kFlagDetect);

    /// Changes to a channel in the 'dir' channel change direction.
    void ChangeChannel(ChannelChangeDirection dir)
        { SetChannel(QString("NextChannel %1").arg((int)dir)); }
    void SetChannel(QString name, uint requestType = kFlagDetect);

    int SetSignalMonitoringRate(int msec, int notifyFrontend = 1);
    int ChangeColour(bool direction);
    int ChangeContrast(bool direction);
    int ChangeBrightness(bool direction);
    int ChangeHue(bool direction);
    bool CheckChannel(QString name) const;
    bool ShouldSwitchToAnotherCard(QString chanid);
    bool CheckChannelPrefix(const QString&,uint&,bool&,QString&);
    void GetNextProgram(int direction,
                        QString &title,       QString &subtitle,
                        QString &desc,        QString &category,
                        QString &starttime,   QString &endtime,
                        QString &callsign,    QString &iconpath,
                        QString &channelname, QString &chanid,
                        QString &seriesid,    QString &programid);

    /// \brief Returns the caputure card number
    int GetCaptureCardNum(void) { return cardid; }
    /// \brief Returns true is "errored" is true, false otherwise.
    bool IsErrored(void)  const { return HasFlags(kFlagErrored); }

    void RingBufferChanged(RingBuffer *rb, ProgramInfo *pginfo);
    void RecorderPaused(void);

    void DVBGotPMT(void)
        { QMutexLocker lock(&stateChangeLock); triggerEventLoop.wakeAll(); }

  public slots:
    void SignalMonitorAllGood() { triggerEventLoop.wakeAll(); }
    void deleteLater(void);

  protected:
    void RunTV(void);
    bool WaitForEventThreadSleep(bool wake = true, ulong time = ULONG_MAX);
    static void *EventThread(void *param);
    static void *RecorderThread(void *param);

  private:
    void SetRingBuffer(RingBuffer *);
    void SetPseudoLiveTVRecording(ProgramInfo*);
    void TeardownAll(void);

    static bool GetDevices(int cardid,
                           GeneralDBOptions   &general_opts,
                           DVBDBOptions       &dvb_opts,
                           FireWireDBOptions  &firewire_opts,
                           DBox2DBOptions     &dbox2_opts);


    static QString GetStartChannel(int cardid, const QString &defaultinput);

    bool SetupRecorder(RecordingProfile& profile);
    void TeardownRecorder(bool killFile = false);
    HDTVRecorder *GetHDTVRecorder(void);
    DVBRecorder  *GetDVBRecorder(void);
    
    bool CreateChannel(const QString &startChanNum);
    void InitChannel(const QString &inputname, const QString &startchannel);
    void CloseChannel(void);
    DBox2Channel *GetDBox2Channel(void);
    DVBChannel   *GetDVBChannel(void);
    Channel      *GetV4LChannel(void);

    void SetupSignalMonitor(bool enable_table_monitoring, bool notify);
    bool SetupDTVSignalMonitor(void);
    void TeardownSignalMonitor(void);
    DTVSignalMonitor *GetDTVSignalMonitor(void);

#ifdef USING_DVB_EIT
    bool WantDishNetEIT(int cardnum);
#endif

    void CreateSIParser(MPEGStreamData*, int num);
    void TeardownSIParser(void);

    bool HasFlags(uint f) const { return (stateFlags & f) == f; }
    void SetFlags(uint f);
    void ClearFlags(uint f);
    static QString FlagToString(uint);

    void HandleTuning(void);
    void TuningShutdowns(const TuningRequest&);
    void TuningFrequency(const TuningRequest&);
    bool TuningSignalCheck(void);
    bool TuningPMTCheck(void);
    void TuningNewRecorder(void);
    void TuningRestartRecorder(void);
    uint TuningCheckForHWChange(const TuningRequest&,
                                QString &channum,
                                QString &inputname);

    void HandleStateChange(void);
    void ChangeState(TVState nextState);
    bool StateIsRecording(TVState state);
    bool StateIsPlaying(TVState state);
    TVState RemovePlaying(TVState state);
    TVState RemoveRecording(TVState state);

    bool GetProgramRingBufferForLiveTV(ProgramInfo **pginfo, RingBuffer **rb);
    bool CreateLiveTVRingBuffer(void);
    bool SwitchLiveTVRingBuffer(bool discont = false, bool set_rec = true);

    void StartedRecording(ProgramInfo*);
    void FinishedRecording(ProgramInfo*);
    QDateTime GetRecordEndTime(const ProgramInfo*) const;
    void CheckForRecGroupChange(void);
    void NotifySchedulerOfRecording(ProgramInfo*);

    void SetOption(RecordingProfile &profile, const QString &name);

    // Various components TVRec coordinates
    RecorderBase     *recorder;
    ChannelBase      *channel;
    SignalMonitor    *signalMonitor;
    EITScanner       *scanner;
    DVBSIParser      *dvbsiparser;
    DummyDTVRecorder *dummyRecorder;

    // Various threads
    /// Event processing thread, runs RunTV().
    pthread_t event_thread;
    /// Recorder thread, runs RecorderBase::StartRecording()
    pthread_t recorder_thread;

    // Configuration variables from database
    bool    transcodeFirst;
    bool    earlyCommFlag;
    bool    runJobOnHostOnly;
    int     audioSampleRateDB;
    int     overRecordSecNrml;
    int     overRecordSecCat;
    QString overRecordCategory;

    // Configuration variables from setup routines
    int               cardid;
    bool              ispip;

    // Configuration variables from database, based on cardid
    GeneralDBOptions  genOpt;
    DVBDBOptions      dvbOpt;
    FireWireDBOptions fwOpt;
    DBox2DBOptions    dboxOpt;

    // State variables
    QMutex         stateChangeLock;
    TVState        internalState;
    TVState        desiredNextState;
    bool           changeState;
    bool           pauseNotify;
    uint           stateFlags;
    TuningQueue    tuningRequests;
    TuningRequest  lastTuningRequest;
    QDateTime      eitScanStartTime;
    QWaitCondition triggerEventLoop;
    QWaitCondition triggerEventSleep;

    // Current recording info
    ProgramInfo *curRecording;
    QDateTime    recordEndTime;
    int          autoRunJobs;
    int          overrecordseconds;

    // Pending recording info
    ProgramInfo *pendingRecording;
    QDateTime    recordPendingStart;

    // Pseudo LiveTV recording
    ProgramInfo *pseudoLiveTVRecording;

    // LiveTV file chain
    LiveTVChain *tvchain;

    // RingBuffer info
    RingBuffer  *ringBuffer;
    QString      rbFilePrefix;
    QString      rbFileExt;

  public:
    static const uint kEITScanStartTimeout;
    static const uint kSignalMonitoringRate;

    // General State flags
    static const uint kFlagFrontendReady        = 0x00000001;
    static const uint kFlagRunMainLoop          = 0x00000002;
    static const uint kFlagExitPlayer           = 0x00000004;
    static const uint kFlagFinishRecording      = 0x00000008;
    static const uint kFlagErrored              = 0x00000010;
    static const uint kFlagCancelNextRecording  = 0x00000020;
    static const uint kFlagAskAllowRecording    = 0x00000040;

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
    static const uint kFlagWaitingForSIParser   = 0x00400000;
    static const uint kFlagNeedToStartRecorder  = 0x00800000;
    static const uint kFlagPendingActions       = 0x00F00000;

    // Running stuff
    static const uint kFlagSignalMonitorRunning = 0x01000000;
    static const uint kFlagSIParserRunning      = 0x02000000;
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
