#ifndef TVREC_H
#define TVREC_H

#include <qstring.h>
#include <pthread.h>
#include <qdatetime.h>
#include <qvaluelist.h>
#include <qptrlist.h>

#include "tv.h"

#include <map>
using namespace std;

class QSocket;
class ChannelBase;
class ProgramInfo;
class RingBuffer;
class NuppelVideoRecorder;
class RecorderBase;
class SIScan;

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
    int swfilter;
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
    QString model;
} firewire_options_t;

class TVRec
{
 public:
    TVRec(int capturecardnum);
   ~TVRec(void);

    void Init(void);

    void RecordPending(ProgramInfo *rcinfo, int secsleft);
    int StartRecording(ProgramInfo *rcinfo);

    void StopRecording(void);
    void FinishRecording(void) { finishRecording = true; }

    void FrontendReady(void) { frontendReady = true; }
    void CancelNextRecording(void) { cancelNextRecording = true; }
    ProgramInfo *GetRecording(void);

    char *GetScreenGrab(ProgramInfo *pginfo, const QString &filename, 
                        int secondsin, int &bufferlen,
                        int &video_width, int &video_height);

    bool IsRunning(void) { return runMainLoop; }
    void Stop(void) { runMainLoop = false; }

    TVState GetState(void);
    bool IsPlaying(void) { return StateIsPlaying(internalState); }
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

    void RetrieveInputChannels(map<int, QString> &inputChannel,
                               map<int, QString> &inputTuneTo,
                               map<int, QString> &externalChanger,
                               map<int, QString> &sourceid);
    void StoreInputChannels(map<int, QString> &inputChannel);

    bool IsBusy(void);
    bool IsReallyRecording(void);

    float GetFramerate(void);
    long long GetFramesWritten(void);
    long long GetFilePosition(void);
    long long GetFreeSpace(long long totalreadpos);
    long long GetKeyframePosition(long long desired);
    void StopPlaying(void);
    void SetupRingBuffer(QString &path, long long &filesize, 
                         long long &fillamount, bool pip = false);
    void SpawnLiveTV(void);
    void StopLiveTV(void);
    void PauseRecorder(void);
    void ToggleInputs(void);
    void ToggleChannelFavorite(void);
    void ChangeChannel(int channeldirection);
    void SetChannel(QString name);
    int ChangeColour(bool direction);
    int ChangeContrast(bool direction);
    int ChangeBrightness(bool direction);
    int ChangeHue(bool direction);
    bool CheckChannel(QString name);
    bool CheckChannelPrefix(QString name, bool &unique);
    void GetNextProgram(int direction,
                        QString &title, QString &subtitle, QString &desc,
                        QString &category, QString &starttime, 
                        QString &endtime, QString &callsign, QString &iconpath,
                        QString &channelname, QString &chanid,
                        QString &seriesid, QString &programid);
    void GetChannelInfo(QString &title, QString &subtitle, QString &desc,
                        QString &category, QString &starttime, 
                        QString &endtime, QString &callsign, QString &iconpath,
                        QString &channelname, QString &chanid,
                        QString &seriesid, QString &programid,
                        QString &chanFilters, QString& repeat, QString& airdate,
                        QString &stars);
    void GetInputName(QString &inputname);

    QSocket *GetReadThreadSocket(void);
    void SetReadThreadSock(QSocket *sock);

    void UnpauseRingBuffer(void);
    void PauseClearRingBuffer(void);
    int RequestRingBufferBlock(int size);
    long long SeekRingBuffer(long long curpos, long long pos, int whence);

    bool isParsingCommercials(ProgramInfo *pginfo);

    int GetCaptureCardNum(void) { return m_capturecardnum; }

 protected:
    void RunTV(void);
    static void *EventThread(void *param);

 private:
    void SetChannel(bool needopen = false);

    void GetChannelInfo(ChannelBase *chan, QString &title, QString &subtitle, 
                        QString &desc, QString &category, QString &starttime, 
                        QString &endtime, QString &callsign, QString &iconpath,
                        QString &channelname, QString &chanid, QString &seriesid, 
                        QString &programid, QString &outputFilters, QString& repeat, 
                        QString& airdate, QString &stars);

    void GetDevices(int cardnum, QString &video, QString &vbi, QString &audio,
                    int &rate, QString &defaultinput, QString &startchannel,
                    QString &type, dvb_options_t &dvb_opts, firewire_options_t &firewire_opts,
                    bool &skip_bt);

    void SetupRecorder(class RecordingProfile& profile);
    void TeardownRecorder(bool killFile = false);
    
    void StateToString(TVState state, QString &statestr);
    void HandleStateChange(void);
    bool StateIsRecording(TVState state);
    bool StateIsPlaying(TVState state);
    TVState RemovePlaying(TVState state);
    TVState RemoveRecording(TVState state);

    void StartedRecording(void);
    void FinishedRecording(void);

    void SetOption(RecordingProfile &profile, const QString &name);

    RecorderBase *nvr;
    RingBuffer *rbuffer;
    ChannelBase *channel;

    TVState internalState;

    bool frontendReady;
    bool runMainLoop;
    bool exitPlayer;
    bool finishRecording;
    bool paused;
    bool prematurelystopped;
    QDateTime recordEndTime;

    float frameRate;

    pthread_t event, encode, readthread, commercials;

    bool changeState;
    TVState nextState;
    QString outputFilename;

    bool watchingLiveTV;

    ProgramInfo *curRecording;
    ProgramInfo *prevRecording;
 
    QString videodev, vbidev, audiodev, cardtype;
    int audiosamplerate;
    bool skip_btaudio;

    bool inoverrecord;
    int overrecordseconds;

    QSocket *readthreadSock;
    bool readthreadlive;
    QMutex readthreadLock;

    int m_capturecardnum;

    bool ispip;

    bool askAllowRecording;
    bool recordPending;
    bool cancelNextRecording;

    ProgramInfo *pendingRecording;
    QDateTime recordPendingStart;

    QString profileName;
    int autoTranscode;

    dvb_options_t dvb_options;
    firewire_options_t firewire_options;

    char requestBuffer[256001];

#ifdef USING_DVB
    SIScan* scanner;
    pthread_t scanner_thread;
#endif
};

#endif
