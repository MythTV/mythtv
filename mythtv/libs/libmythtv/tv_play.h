#ifndef TVPLAY_H
#define TVPLAY_H

#include <qstring.h>
#include <qmap.h>
#include <qdatetime.h>
#include <pthread.h>
#include <qvaluevector.h>
#include <qvaluelist.h>
#include <qmutex.h>

#include "tv.h"

#include <qobject.h>

class QSqlDatabase;
class QDateTime;
class OSD;
class RemoteEncoder;
class VolumeControl;
class NuppelVideoPlayer;
class RingBuffer;
class ProgramInfo;
class MythDialog;

class TV : public QObject
{
    Q_OBJECT
  public:
    TV(QSqlDatabase *db);
   ~TV(void);

    void Init(bool createWindow = true);

    int LiveTV(bool showDialogs = true);
    void StopLiveTV(void) { exitPlayer = true; }
    void FinishRecording(void);
    bool WantsToQuit(void) { return wantsToQuit; }
    int GetLastRecorderNum(void) { return lastRecorderNum; }
    int PlayFromRecorder(int recordernum);

    void AskAllowRecording(const QString &message, int timeuntil);

    // next two functions only work on recorded programs.
    int Playback(ProgramInfo *rcinfo);

    bool IsRunning(void) { return runMainLoop; }
    void Stop(void) { runMainLoop = false; }

    TVState GetState(void);
    bool IsPlaying(void) { return StateIsPlaying(GetState()); }
    bool IsRecording(void) { return StateIsRecording(GetState()); }
    bool IsMenuRunning(void) { return menurunning; }

    void GetNextProgram(RemoteEncoder *enc, int direction,
                        QMap<QString, QString> &regexpMap);
    void GetNextProgram(RemoteEncoder *enc, int direction,
                        QString &title, QString &subtitle,
                        QString &desc, QString &category, QString &starttime,
                        QString &endtime, QString &callsign, QString &iconpath,
                        QString &channelname, QString &chanid);

    void GetChannelInfo(RemoteEncoder *enc, QMap<QString, QString> &regexpMap);
    void GetChannelInfo(RemoteEncoder *enc, QString &title, QString &subtitle,
                        QString &desc, QString &category, QString &starttime,
                        QString &endtime, QString &callsign, QString &iconpath,
                        QString &channelname, QString &chanid);

    // for the guidegrid to use
    void EmbedOutput(unsigned long wid, int x, int y, int w, int h);
    void StopEmbeddingOutput(void);
    void EPGChannelUpdate(QString chanstr);
   
    bool getRequestDelete(void) { return requestDelete; }
    bool getEndOfRecording(void) { return endOfRecording; }

    void ProcessKeypress(int keypressed);
    void customEvent(QCustomEvent *e);

    void AddPreviousChannel(void);
    void PreviousChannel(void);

  public slots:
    void HandleOSDClosed(int osdType);

  protected slots:
    void SetPreviousChannel(void);
    void UnMute(void);
    void KeyRepeatOK(void);
    void BrowseEndTimer(void) { BrowseEnd(false); }

  protected:
    void doLoadMenu(void);
    static void *MenuHandler(void *param);

    void RunTV(void);
    static void *EventThread(void *param);

    bool eventFilter(QObject *o, QEvent *e);

  private:
    void StartPlayerAndRecorder(bool StartPlayer, bool StartRecorder);
    void StopPlayerAndRecorder(bool StopPlayer, bool StopRecorder);

    void SetChannel(bool needopen = false);

    void ToggleChannelFavorite(void);
    void ChangeChannel(int direction);
    void ChangeChannelByString(QString &name);

    void ChangeVolume(bool up);
    void ToggleMute(void);
    void ToggleLetterbox(void);
    void ChangeContrast(bool up, bool recorder);
    void ChangeBrightness(bool up, bool recorder);
    void ChangeColour(bool up, bool recorder);
    void ChangeHue(bool up, bool recorder);
 
    void ChannelKey(int key);
    void ChannelCommit(void);

    void ToggleInputs(void); 

    void DoInfo(void);
    void DoPause(void);
    bool UpdatePosOSD(float time, const QString &mesg);
    void DoSeek(float time, const QString &mesg);
    void NormalSpeed(void);
    void ChangeSpeed(int direction);
    float StopFFRew(void);
    void ChangeFFRew(int direction);
    void RepeatFFRew(void);
    void DoSkipCommercials(int direction);

    void DoQueueTranscode(void);  
 
    void ToggleOSD(void); 
    void UpdateOSD(void);
    void UpdateOSDInput(void);

    void LoadMenu(void);

    void SetupPlayer();
    void TeardownPlayer();
    void SetupPipPlayer();
    void TeardownPipPlayer();
    
    void StateToString(TVState state, QString &statestr);
    void HandleStateChange();
    bool StateIsRecording(TVState state);
    bool StateIsPlaying(TVState state);
    TVState RemovePlaying(TVState state);
    TVState RemoveRecording(TVState state);

    void TogglePIPView(void);
    void ToggleActiveWindow(void);
    void SwapPIP(void);

    void BrowseStart(void);
    void BrowseEnd(bool change);
    void BrowseDispInfo(int direction);
    void BrowseToggleRecord(void);

    void DoTogglePictureAttribute(void);
    void DoChangePictureAttribute(bool up);

    int osd_display_time;

    bool channelqueued;
    char channelKeys[5];
    int channelkeysstored;

    bool menurunning;

    TVState internalState;

    bool runMainLoop;
    bool exitPlayer;
    bool paused;

    float frameRate;

    pthread_t event, decode, pipdecode;
    bool changeState;
    TVState nextState;
    QString inputFilename;

    int playbackLen;

    int jumptime;
    int fftime;
    int rewtime;
    int stickykeys;
    int ff_rew_repos;
    int doing_ff_rew;
    int ff_rew_index;
    int speed_index;

    OSD *osd;
    bool update_osd_pos;

    NuppelVideoPlayer *nvp;
    NuppelVideoPlayer *pipnvp;
    NuppelVideoPlayer *activenvp;

    RemoteEncoder *recorder;
    RemoteEncoder *piprecorder;
    RemoteEncoder *activerecorder;

    RingBuffer *prbuffer;
    RingBuffer *piprbuffer;
    RingBuffer *activerbuffer;

    QString dialogname;
    bool editmode;
    bool requestDelete;
    bool endOfRecording;
    bool queuedTranscode;

    QTimer *browseTimer;
    bool browsemode;
    bool persistentbrowsemode;
    QString browsechannum;
    QString browsechanid;
    QString browsestarttime;

    ProgramInfo *playbackinfo;

    QSqlDatabase *m_db;

    VolumeControl *volumeControl;

    unsigned int embedid;
    int embx, emby, embw, embh;

    typedef QValueVector<QString> PrevChannelVector;
    PrevChannelVector channame_vector;
    unsigned int times_pressed;
    QTimer *prevChannelTimer;
    QTimer *muteTimer;
    QString last_channel;

    MythDialog *myWindow;

    QValueList<int> keyList;
    QMutex keyListLock;

    bool wantsToQuit;
    int lastRecorderNum;
    bool getRecorderPlaybackInfo;
    ProgramInfo *recorderPlaybackInfo;

    bool keyRepeat;
    QTimer *keyrepeatTimer;
    
    int picAdjustment;
};

#endif
