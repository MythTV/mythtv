#ifndef TVREC_H
#define TVREC_H

#include <qstring.h>
#include <pthread.h>
#include <qdatetime.h>
#include <qvaluelist.h>
#include <qptrlist.h>
#include <qmap.h>
#include <qstringlist.h>

#include "tv.h"

class QSocket;
class ProgramInfo;
class RingBuffer;
class NuppelVideoRecorder;
class RecorderBase;
class SIScan;
class DVBSIParser;
class SignalMonitor;
class ChannelBase;
class DVBChannel;

typedef enum
{
    BROWSE_SAME,
    BROWSE_UP,
    BROWSE_DOWN,
    BROWSE_LEFT,
    BROWSE_RIGHT,
    BROWSE_FAVORITE
} BrowseDirections;

typedef struct _dvb_options_t
{
    int hw_decoder;
    int recordts;
    int wait_for_seqstart;
    int dmx_buf_size;
    int pkt_buf_size;
    bool dvb_on_demand;
} dvb_options_t;

typedef struct _firewire_options_t
{
    int port;
    int node;
    int speed;
    int connection;
    QString model;
} firewire_options_t;

class TVRec
{
  public:
    TVRec(int capturecardnum);
   ~TVRec(void);

    bool Init(void);

    void RecordPending(const ProgramInfo *rcinfo, int secsleft);
    int StartRecording(const ProgramInfo *rcinfo);

    void StopRecording(void);
    /// \brief Tells TVRec to finush the current recording as soon as possible.
    void FinishRecording(void) { finishRecording = true; }
    /// \brief Tells TVRec that the frontend's TV class is ready for messages.
    void FrontendReady(void) { frontendReady = true; }
    /** \brief Tells TVRec to cancel the upcoming recording.
     *  \sa RecordPending(const ProgramInfo*,int),
     *      TV::AskAllowRecording(const QStringList&,int) */
    void CancelNextRecording(void) { cancelNextRecording = true; }
    ProgramInfo *GetRecording(void);

    char *GetScreenGrab(const ProgramInfo *pginfo, const QString &filename, 
                        int secondsin, int &bufferlen,
                        int &video_width, int &video_height,
                        float &video_aspect);

    /// \brief Returns true if event loop has not been told to shut down
    bool IsRunning(void) { return runMainLoop; }
    /// \brief Tells TVRec to stop event loop
    void Stop(void) { runMainLoop = false; }

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
    void ToggleInputs(void);
    void ToggleChannelFavorite(void);
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
    void SetReadThreadSock(QSocket *sock);

    void UnpauseRingBuffer(void);
    void PauseClearRingBuffer(void);
    int RequestRingBufferBlock(int size);
    long long SeekRingBuffer(long long curpos, long long pos, int whence);

    /// \brief Returns the caputure card number
    int GetCaptureCardNum(void) { return m_capturecardnum; }
    /// \brief Returns true is "errored" is true, false otherwise.
    bool IsErrored(void) { return errored; }
  protected:
    void RunTV(void);
    static void *EventThread(void *param);
    static void *RecorderThread(void *param);
    static void *ScannerThread(void *param);

  private:
    void SetChannel(bool needopen = false);

    void GetChannelInfo(ChannelBase *chan,  QString &title,
                        QString &subtitle,  QString &desc,
                        QString &category,  QString &starttime, 
                        QString &endtime,   QString &callsign,
                        QString &iconpath,  QString &channelname,
                        QString &chanid,    QString &seriesid, 
                        QString &programid, QString &outputFilters,
                        QString &repeat,    QString &airdate,
                        QString &stars);

    void GetDevices(int cardnum, QString &video, QString &vbi, QString &audio,
                    int &rate, QString &defaultinput, QString &startchannel,
                    QString &type, dvb_options_t &dvb_opts,
                    firewire_options_t &firewire_opts, bool &skip_bt);

    void SetupRecorder(class RecordingProfile& profile);
    void TeardownRecorder(bool killFile = false);
    
    void SetupSignalMonitor(void);
    void TeardownSignalMonitor(void);

    void HandleStateChange(void);
    bool StateIsRecording(TVState state);
    bool StateIsPlaying(TVState state);
    TVState RemovePlaying(TVState state);
    TVState RemoveRecording(TVState state);

    void StartedRecording(void);
    void FinishedRecording(void);

    void SetOption(RecordingProfile &profile, const QString &name);

    // Various components TVRec coordinates
    RingBuffer    *rbuffer;
    RecorderBase  *recorder;
    ChannelBase   *channel;
    SignalMonitor *signalMonitor;
#ifdef USING_DVB
    SIScan        *scanner;
#endif

    // Various threads
    /// Event processing thread, runs RunTV().
    pthread_t event_thread;
    /// Recorder thread, runs RecorderBase::StartRecording()
    pthread_t recorder_thread;
#ifdef USING_DVB
    pthread_t scanner_thread;
#endif

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

    // Configuration variables from setup rutines
    int     m_capturecardnum;
    bool    ispip;
    dvb_options_t dvb_options;
    firewire_options_t firewire_options;

    // State variables
    QMutex  stateChangeLock;
    TVState internalState;
    TVState nextState;
    bool    changeState;
    bool    frontendReady;
    bool    runMainLoop;
    bool    exitPlayer;
    bool    finishRecording;
    bool    paused;
    bool    prematurelystopped;
    bool    inoverrecord;
    bool    errored;
    float   frameRate;
    int     overrecordseconds;

    // Current recording info
    ProgramInfo *curRecording;
    QDateTime    recordEndTime;
    QString      profileName;
    bool         askAllowRecording;
    int          autoRunJobs;

    // Pending recording info
    ProgramInfo *pendingRecording;
    QDateTime    recordPendingStart;
    bool         recordPending;
    bool         cancelNextRecording;

    // RingBuffer info
    QString  outputFilename;
    char    *requestBuffer;
    QSocket *readthreadSock;
    QMutex   readthreadLock;
    bool     readthreadlive;

    // Current recorder info
    QString  videodev;
    QString  vbidev;
    QString  audiodev;
    QString  cardtype;
    int      audiosamplerate;
    bool     skip_btaudio;

    // class constants
    static const int kRequestBufferSize;
};

#endif
