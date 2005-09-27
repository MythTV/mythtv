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

#include "programinfo.h"
#include "tv.h"

class QSocket;
class RingBufferInfo;
class NuppelVideoRecorder;
class EITScanner;
class DVBSIParser;
class DummyDTVRecorder;
class RecordingProfile;

class RecorderBase;
class DVBRecorder;
class HDTVRecorder;

class SignalMonitor;
class DTVSignalMonitor;

class ChannelBase;
class DBox2Channel;
class DVBChannel;
class Channel;

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
    DVBDBOptions() :
        hw_decoder(0),        recordts(1),
        dmx_buf_size(4*1024), pkt_buf_size(16*1024),
        dvb_on_demand(false) {;}

    int hw_decoder;
    int recordts;
    int dmx_buf_size;
    int pkt_buf_size;
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

class TVRec
{
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
    /** \brief Tells TVRec to cancel the upcoming recording.
     *  \sa RecordPending(const ProgramInfo*,int),
     *      TV::AskAllowRecording(const QStringList&,int) */
    void CancelNextRecording(void) { SetFlags(kFlagCancelNextRecording); }
    ProgramInfo *GetRecording(void);

    char *GetScreenGrab(const ProgramInfo *pginfo, const QString &filename, 
                        int secondsin, int &bufferlen,
                        int &video_width, int &video_height,
                        float &video_aspect);

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

    bool CheckChannel(ChannelBase *chan, const QString &channum, 
                      QString& inputID); 
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

    void RetrieveInputChannels(QMap<int, QString> &inputChannel,
                               QMap<int, QString> &inputTuneTo,
                               QMap<int, QString> &externalChanger,
                               QMap<int, QString> &sourceid);
    void StoreInputChannels(const QMap<int, QString> &inputChannel);

    bool IsBusy(void);
    bool IsReallyRecording(void);

    float GetFramerate(void);
    long long GetFramesWritten(void);
    long long GetFilePosition(void);
    long long GetFreeSpace(long long totalreadpos);
    long long GetMaxBitrate();
    long long GetKeyframePosition(long long desired);
    void StopPlaying(void);
    bool SetupRingBuffer(QString &path, long long &filesize, 
                         long long &fillamount, bool pip = false);
    void SpawnLiveTV(void);
    void StopLiveTV(void);
    void PauseRecorder(void);
    void ToggleChannelFavorite(void);

    void ToggleInputs(void);
    void ChangeChannel(ChannelChangeDirection dir);
    void SetChannel(QString name);

    void Pause(void);
    void Unpause(void);

    int SetSignalMonitoringRate(int msec, int notifyFrontend = 1);
    int ChangeColour(bool direction);
    int ChangeContrast(bool direction);
    int ChangeBrightness(bool direction);
    int ChangeHue(bool direction);
    bool CheckChannel(QString name);
    bool ShouldSwitchToAnotherCard(QString chanid);
    bool CheckChannelPrefix(QString name, bool &unique);
    void GetNextProgram(int direction,
                        QString &title,       QString &subtitle,
                        QString &desc,        QString &category,
                        QString &starttime,   QString &endtime,
                        QString &callsign,    QString &iconpath,
                        QString &channelname, QString &chanid,
                        QString &seriesid,    QString &programid);
    void GetChannelInfo(QString &title,       QString &subtitle,
                        QString &desc,        QString &category,
                        QString &starttime,   QString &endtime,
                        QString &callsign,    QString &iconpath,
                        QString &channelname, QString &chanid,
                        QString &seriesid,    QString &programid,
                        QString &chanFilters, QString &repeat,
                        QString &airdate,     QString &stars);
    void GetInputName(QString &inputname);

    QSocket *GetReadThreadSocket(void);
    void SetReadThreadSocket(QSocket *sock);

    int RequestRingBufferBlock(uint size);
    long long SeekRingBuffer(long long curpos, long long pos, int whence);

    /// \brief Returns the caputure card number
    int GetCaptureCardNum(void) { return cardid; }
    /// \brief Returns true is "errored" is true, false otherwise.
    bool IsErrored(void)  const { return HasFlags(kFlagErrored); }

  protected:
    void RunTV(void);
    bool WaitForEventThreadSleep(bool wake = true, ulong time = ULONG_MAX);
    static void *EventThread(void *param);
    static void *RecorderThread(void *param);

  private:
    void GetChannelInfo(ChannelBase *chan,  QString &title,
                        QString &subtitle,  QString &desc,
                        QString &category,  QString &starttime, 
                        QString &endtime,   QString &callsign,
                        QString &iconpath,  QString &channelname,
                        QString &chanid,    QString &seriesid, 
                        QString &programid, QString &outputFilters,
                        QString &repeat,    QString &airdate,
                        QString &stars);

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
    
    void InitChannel(const QString &inputname, const QString &startchannel);
    bool StartChannel(bool livetv);
    void CloseChannel(void);
    DBox2Channel *GetDBox2Channel(void);
    DVBChannel   *GetDVBChannel(void);
    Channel      *GetV4LChannel(void);

    void SetupSignalMonitor(void);
    bool SetupDTVSignalMonitor(void);
    void TeardownSignalMonitor(void);
    DTVSignalMonitor *GetDTVSignalMonitor(void);

    void CreateSIParser(int num);
    void TeardownSIParser(void);

    bool HasFlags(uint f) const { return (stateFlags & f) == f; }
    void SetFlags(uint f);
    void ClearFlags(uint f);
    static QString FlagToString(uint);

    bool StartRecorder(bool livetv);
    bool StartRecorderPost(bool livetv);
    bool StartRecorderPostThread(bool livetv);
    void AbortStartRecorderThread();
    static void *StartRecorderPostThunk(void*);
    bool CreateRecorderThread(void);
    void StartDummyRecorder(void);
    void StopDummyRecorder(void);

    void HandleStateChange(void);
    void ChangeState(TVState nextState);
    bool StateIsRecording(TVState state);
    bool StateIsPlaying(TVState state);
    TVState RemovePlaying(TVState state);
    TVState RemoveRecording(TVState state);

    void StartedRecording(ProgramInfo*);
    void FinishedRecording(ProgramInfo*);

    void SetOption(RecordingProfile &profile, const QString &name);

    // Various components TVRec coordinates
    RingBufferInfo   *rbi;
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
    /// Thread used to start Recorder after HandleStateChange
    pthread_t start_recorder_thread;

    // Configuration variables from database
    bool    transcodeFirst;
    bool    earlyCommFlag;
    bool    runJobOnHostOnly;
    int     audioSampleRateDB;
    int     overRecordSecNrml;
    int     overRecordSecCat;
    QString overRecordCategory;
    int     liveTVRingBufSize;
    int     liveTVRingBufFill;
    QString liveTVRingBufLoc;
    QString recprefix;

    // Configuration variables from setup routines
    int               cardid;
    bool              ispip;

    // Configuration variables from database, based on cardid
    GeneralDBOptions  genOpt;
    DVBDBOptions      dvbOpt;
    FireWireDBOptions fwOpt;
    DBox2DBOptions    dboxOpt;

    // State variables
    QMutex  stateChangeLock;
    TVState internalState;
    TVState desiredNextState;
    bool    changeState;
    bool    startingRecording;
    bool    abortRecordingStart;
    bool    waitingForSignal;
    bool    waitingForDVBTables;
    bool    prematurelystopped;
    uint           stateFlags;
    QWaitCondition triggerEventLoop;
    QWaitCondition triggerEventSleep;

    // Current recording info
    ProgramInfo *curRecording;
    QDateTime    recordEndTime;
    int          autoRunJobs;
    bool         inoverrecord;
    int          overrecordseconds;

    // Pending recording info
    ProgramInfo *pendingRecording;
    QDateTime    recordPendingStart;

    // General State flags
    static const uint kFlagFrontendReady        = 0x00000001;
    static const uint kFlagRunMainLoop          = 0x00000002;
    static const uint kFlagExitPlayer           = 0x00000004;
    static const uint kFlagFinishRecording      = 0x00000008;
    static const uint kFlagErrored              = 0x00000010;
    static const uint kFlagCancelNextRecording  = 0x00000020;
    static const uint kFlagAskAllowRecording    = 0x00000040;
};

#endif
