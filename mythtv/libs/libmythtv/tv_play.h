#ifndef TVPLAY_H
#define TVPLAY_H

#include <qstring.h>
#include <qmap.h>
#include <qdatetime.h>
#include <pthread.h>
#include <qvaluevector.h>
#include <qptrlist.h>
#include <qmutex.h>

#include "tv.h"

#include <qobject.h>

class MythSqlDatabase;
class QDateTime;
class OSD;
class RemoteEncoder;
class NuppelVideoPlayer;
class RingBuffer;
class ProgramInfo;
class MythDialog;
class UDPNotify;
class OSDListTreeType;
class OSDGenericTree;

class TV : public QObject
{
    Q_OBJECT
  public:
    TV(void);
   ~TV();

    static void InitKeys(void);

    void Init(bool createWindow = true);

    int LiveTV(bool showDialogs = true);
    void StopLiveTV(void) { exitPlayer = true; }
    void FinishRecording(void);
    bool WantsToQuit(void) { return wantsToQuit; }
    int GetLastRecorderNum(void) { return lastRecorderNum; }
    int PlayFromRecorder(int recordernum);

    void AskAllowRecording(const QStringList &messages, int timeuntil);

    // next two functions only work on recorded programs.
    int Playback(ProgramInfo *rcinfo);

    bool IsRunning(void) { return runMainLoop; }
    void Stop(void) { runMainLoop = false; }

    TVState GetState(void);
    bool IsPlaying(void) { return StateIsPlaying(GetState()); }
    bool IsRecording(void) { return StateIsRecording(GetState()); }
    bool IsMenuRunning(void) { return menurunning; }
    bool IsSwitchingCards(void) { return switchingCards; }

    void GetNextProgram(RemoteEncoder *enc, int direction,
                        QMap<QString, QString> &infoMap);
    void GetNextProgram(RemoteEncoder *enc, int direction,
                        QString &title, QString &subtitle,
                        QString &desc, QString &category, QString &starttime,
                        QString &endtime, QString &callsign, QString &iconpath,
                        QString &channelname, QString &chanid,
                        QString &seriesid, QString &programid);

    void GetChannelInfo(RemoteEncoder *enc, QMap<QString, QString> &infoMap);
    void GetChannelInfo(RemoteEncoder *enc, QString &title, QString &subtitle,
                        QString &desc, QString &category, QString &starttime,
                        QString &endtime, QString &callsign, QString &iconpath,
                        QString &channelname, QString &chanid,
                        QString &seriesid, QString &programid, QString &outFilters, 
                        QString &repeat, QString &airdate, QString &stars);

    // for the guidegrid to use
    void EmbedOutput(WId wid, int x, int y, int w, int h);
    void StopEmbeddingOutput(void);
    void EPGChannelUpdate(QString chanstr);
   
    bool getRequestDelete(void) { return requestDelete; }
    bool getEndOfRecording(void) { return endOfRecording; }

    void ProcessKeypress(QKeyEvent *e);
    void customEvent(QCustomEvent *e);

    void AddPreviousChannel(void);
    void PreviousChannel(void);

    OSD *GetOSD(void);

  public slots:
    void HandleOSDClosed(int osdType);

  protected slots:
    void SetPreviousChannel(void);
    void UnMute(void);
    void KeyRepeatOK(void);
    void BrowseEndTimer(void) { BrowseEnd(false); }
    void SleepEndTimer(void);
    void TreeMenuEntered(OSDListTreeType *tree, OSDGenericTree *item);
    void TreeMenuSelected(OSDListTreeType *tree, OSDGenericTree *item);

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
    QString getFiltersForChannel();

    void ToggleChannelFavorite(void);
    void ChangeChannel(int direction, bool force = false);
    void ChangeChannelByString(QString &name, bool force = false);

    void ChangeVolume(bool up);
    void ToggleMute(void);
    void ToggleLetterbox(int letterboxMode = -1);
    void ChangeContrast(bool up, bool recorder);
    void ChangeBrightness(bool up, bool recorder);
    void ChangeColour(bool up, bool recorder);
    void ChangeHue(bool up, bool recorder);

    QString QueuedChannel(void);
    void ChannelClear(bool hideosd = false); 
    void ChannelKey(char key);
    void ChannelCommit(void);

    void ToggleInputs(void); 

    void SwitchCards(void);

    void ToggleSleepTimer(void);
    void ToggleSleepTimer(const QString);

    void DoInfo(void);
    void DoPlay(void);
    void DoPause(void);
    bool UpdatePosOSD(float time, const QString &mesg, int disptime = 2);
    void DoSeek(float time, const QString &mesg);
    void DoArbSeek(int dir);
    void NormalSpeed(void);
    void ChangeSpeed(int direction);
    void ChangeTimeStretch(int dir, bool allowEdit = true);
    float StopFFRew(void);
    void ChangeFFRew(int direction);
    void SetFFRew(int index);
    void DoToggleCC(int mode);
    void DoSkipCommercials(int direction);
    void DoEditMode(void);

    void DoQueueTranscode(void);  

    void SetAutoCommercialSkip(int skipMode = 0);
    void SetManualZoom(bool zoomON = false);
 
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

    void ToggleAutoExpire(void);

    void BrowseStart(void);
    void BrowseEnd(bool change);
    void BrowseDispInfo(int direction);
    void ToggleRecord(void);
    void BrowseChannel(QString &chan);

    void DoTogglePictureAttribute(void);
    void DoToggleRecPictureAttribute(void);
    void DoChangePictureAttribute(int control, bool up, bool rec);

    void BuildOSDTreeMenu(void);
    void ShowOSDTreeMenu(void);

    void UpdateLCD(void);
    void ShowLCDChannelInfo(void);

    QString PlayMesg(void);

    int osd_display_time;

    bool arrowAccel;

    bool channelqueued;
    char channelKeys[5];
    int channelkeysstored;
    bool smartChannelChange;

    bool menurunning;

    TVState internalState;

    bool runMainLoop;
    bool exitPlayer;
    bool paused;

    int autoCommercialSkip;
    bool tryUnflaggedSkip;

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
    bool ff_rew_reverse;
    int doing_ff_rew;
    int ff_rew_index;
    int speed_index;
    float normal_speed;
    bool stretchAdjustment;

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

    bool zoomMode;

    ProgramInfo *playbackinfo;

    MythSqlDatabase *m_db;

    WId embedid;
    int embx, emby, embw, embh;

    typedef QValueVector<QString> PrevChannelVector;
    PrevChannelVector channame_vector;
    unsigned int times_pressed;
    QTimer *prevChannelTimer;
    QTimer *muteTimer;
    QString last_channel;

    MythDialog *myWindow;

    QPtrList<QKeyEvent> keyList;
    QMutex keyListLock;

    bool wantsToQuit;
    int lastRecorderNum;
    bool getRecorderPlaybackInfo;
    ProgramInfo *recorderPlaybackInfo;

    bool keyRepeat;
    QTimer *keyrepeatTimer;
    
    int picAdjustment;
    int recAdjustment;
    bool usePicControls;

    bool smartForward;
    bool doSmartForward;

    UDPNotify *udpnotify;

    QString lastCC;
    int lastCCDir;
    bool showBufferedWarnings;
    int bufferedChannelThreshold;

    bool MuteIndividualChannels;

    bool switchingCards;

    OSDGenericTree *treeMenu;

    int sleep_index;
    QTimer *sleepTimer;

    char vbimode;

    QDateTime lastLcdUpdate;
    QString lcdTitle, lcdSubtitle, lcdCallsign;
    
    QString baseFilters;
    int repoLevel;
};

#endif
