/*
	phoneui.h

	(c) 2004 Paul Volkaerts
	Part of the mythTV project
	
    header for the main interface screen
*/

#ifndef PHONEUI_H_
#define PHONEUI_H_

#include <qregexp.h>
#include <qtimer.h>
#include <qptrlist.h>
#include <qthread.h>
#include <qsound.h>

#include <mythtv/mythwidgets.h>
#include <mythtv/dialogbox.h>
#include <mythtv/volumecontrol.h>

#include "directory.h"
#include "webcam.h"
#include "sipfsm.h"
#include "rtp.h"
#include "h263.h"
#include "tone.h"


// The SIP stack exists even when the PhoneUI doesn't
extern SipContainer *sipStack;
class PhoneUIStatusBar;

#define MAX_DISPLAY_IM_MSGS    5        // No lines of IM per mythdialog box

// Dimensions of local webcam window when viewing fullscreen
#define WC_INSET_WIDTH        (176)
#define WC_INSET_HEIGHT       (144)

class PhoneUIBox : public MythThemedDialog
{

  Q_OBJECT

  public:

    typedef QValueVector<int> IntVector;
    
    PhoneUIBox(MythMainWindow *parent, QString window_name,
               QString theme_filename, const char *name = 0);

    ~PhoneUIBox(void);

    void keyPressEvent(QKeyEvent *e);
    void customEvent(QCustomEvent *);
    
  public slots:

    void MenuButtonPushed();
    void InfoButtonPushed();
    void LoopbackButtonPushed();
    void handleTreeListSignals(int, IntVector*);
    void TransmitLocalWebcamImage(uchar *yuvBuffer, int w, int h);
    void OnScreenClockTick();
    void closeUrlPopup();
    void dialUrlVideo();
    void dialUrlVoice();
    void dialUrlSwitchToDigits();
    void dialUrlSwitchToUrl();
    void closeAddEntryPopup();
    void entryAddSelected();
    void closeAddDirectoryPopup();
    void directoryAddSelected();
    void closeCallPopup();
    void incallDialVoiceSelected();
    void incallDialVideoSelected();
    void incallSendIMSelected();
    void menuCallUrl();
    void menuAddContact();
    void menuDirAdd();
    void menuDirDel();
    void menuDirRen();
    void menuSpeedDialRemove();
    void menuHistorySave();
    void menuHistoryClear();
    void menuEntryEdit();
    void menuEntryMakeSpeedDial();
    void menuEntryDelete();
    void vmailEntryDelete();
    void vmailEntryDeleteAll();
    void closeMenuPopup();
    void closeIMPopup();
    void imSendReply();
    void closeStatisticsPopup();
    void changeVolumeControl(bool up_or_down);
    void changeVolume(bool up_or_down);
    void toggleMute();
    QString getVideoFrameSizeText();
    void hideVolume(){showVolume(false);}
    void showVolume(bool on_or_off);
    void DisplayMicSpkPower();


  private:
    void    DrawLocalWebcamImage();
    void    TransmitLocalWebcamImage();
    void    ProcessRxVideoFrame();
    void    ProcessSipStateChange();
    void    ProcessSipNotification();
    void    PlaceorAnswerCall(QString url, QString name, QString Mode, bool onLocalLan=false);
    void    HangUp();
    void    StartVideo(int lPort, QString remoteIp, int remoteVideoPort, int videoPayload, QString rxVidRes);
    void    StopVideo();
    void    ChangeVideoTxResolution();
    void    ChangeVideoRxResolution();
    void    keypadPressed(char k);
    void    getResolution(QString setting, int &width, int &height);
    void    videoCifModeToRes(QString cifMode, int &w, int &h);
    const char *videoResToCifMode(int w);
    void    ProcessAudioRtpStatistics(RtpEvent *stats);
    void    ProcessVideoRtpStatistics(RtpEvent *stats);
    void    doMenuPopup();
    void    doUrlPopup(char key, bool DigitsOrUrl);
    void    doIMPopup(QString otherParty, QString callId, QString Msg);
    void    scrollIMText(QString Msg, bool msgReceived);
    void    doAddEntryPopup(DirEntry *edit=0, QString nn="", QString Url="");
    void    doAddDirectoryPopup();
    void    addNewDirectoryEntry(QString Name, QString Url, QString Dir, QString fn, QString sn, QString ph, bool isSpeed, bool OnHomeLan);
    void    doCallPopup(DirEntry *entry, QString DialorAnswer, bool audioOnly);
    void    drawCallPopupCallHistory(MythPopupBox *popup, CallRecord *call);
    void    startRTP(int audioPayload, int videoPayload, int dtmfPayload, int audioPort, int videoPort, 
                     QString remoteIp, QString audioCodec, QString videoCodec, QString videoRes);
    void    stopRTP(bool stopAudio=true, bool stopVideo=true);
    void    alertUser(QString callerUser, QString callerName, QString callerUrl, bool inAudioOnly);
    void    showStatistics(bool showVideo);
    void    updateAudioStatistics(int pkIn, int pkLost, int pkLate, int pkOut, int pkInDisc, int pkOutDrop, int bIn, int bOut);
    void    updateVideoStatistics(int pkIn, int pkLost, int pkLate, int pkOut, int pkInDisc, int pkOutDrop, int bIn, int bOut, int fIn, int fOut, int fDiscIn, int fDiscOut);

    void    wireUpTheme();
    DirectoryContainer *DirContainer;
    PhoneUIStatusBar *phoneUIStatusBar;

    int    State;
    rtp    *rtpAudio;
    rtp    *rtpVideo;
    QString audioCodecInUse;
    QString videoCodecInUse;
    TelephonyTones Tones;
    Tone   *vmail;

    Webcam  *webcam;
    wcClient *localClient;
    wcClient *txClient;
    int wcWidth, wcHeight, txWidth, txHeight, rxWidth, rxHeight;
    int wcDeliveredFrames;
    int wcDroppedFrames;
    QString txVideoMode;
    int zoomWidth, zoomHeight, zoomFactor;
    int hPan, wPan;
    int screenwidth, screenheight;
    bool fullScreen;
    QRect rxVideoArea;
    bool loopbackMode;

    H263Container *h263;
    QTimer *powerDispTimer;
    QTimer *OnScreenClockTimer;
    QTime ConnectTime;
 
    VolumeControl     *volume_control;
    QTimer            *volume_display_timer;
    UIStatusBarType   *volume_status;
    enum {VOL_VOLUME, VOL_MICVOLUME, VOL_BRIGHTNESS, VOL_CONTRAST, VOL_COLOUR, VOL_TXSIZE, VOL_TXRATE, VOL_AUDCODEC } VolumeMode;

    int camBrightness, camContrast, camColour, txFps;

    uchar localRgbBuffer[MAX_RGB_704_576];
    uchar rxRgbBuffer[MAX_RGB_704_576];
    uchar yuvBuffer1[MAX_YUV_704_576];
    uchar yuvBuffer2[MAX_YUV_704_576];
    QImage savedLocalWebcam;

    UIManagedTreeListType *DirectoryList;

    UIRepeatedImageType   *micAmplitude;
    UIRepeatedImageType   *spkAmplitude;

    UIImageType           *volume_bkgnd;
    UIImageType           *volume_icon;
    UITextType            *volume_setting;
    UITextType            *volume_value;
    UITextType            *volume_info;
    UIBlackHoleType       *localWebcamArea;
    UIBlackHoleType       *receivedWebcamArea;


    bool                   VideoOn;
    bool                   show_whole_tree;  // Copied from mythmusic but not really useful here, leave as true
    CallRecord            *currentCallEntry;

    MythPopupBox          *menuPopup;

    MythPopupBox          *urlPopup;
    MythRemoteLineEdit    *urlRemoteField;
    MythLineEdit          *urlField;
    QLabel                *callLabelUrl;
    QLabel                *callLabelName;
    MythPopupBox          *imPopup;
    MythRemoteLineEdit    *imReplyField;
    QMap<int,QLabel *>     imLine;
    int                    displayedIMMsgs;
    QString                imCallid;
    QString                imUrl;

    MythPopupBox *statsPopup;
    QLabel *audioPkInOutLabel;
    QLabel *audioBytesInOutLabel;
    QLabel *audioAvgBwLabel;
    QLabel *videoPkInLabel;
    QLabel *videoPkOutLabel;
    QLabel *videoBytesInOutLabel;
    QLabel *videoAvgBwLabel;
    QLabel *videoFramesInOutDiscLabel;
    QLabel *videoAvgFpsLabel;
    QLabel *videoWebcamFpsLabel;
    QLabel *videoResLabel;


    MythPopupBox          *addEntryPopup;
    MythRemoteLineEdit    *entryNickname;
    MythRemoteLineEdit    *entryFirstname;
    MythRemoteLineEdit    *entrySurname;
    MythRemoteLineEdit    *entryUrl;
    MythComboBox          *entryDir;
    MythComboBox          *entryPhoto;
    MythCheckBox          *entrySpeed;
    MythCheckBox          *entryOnHomeLan;
    DirEntry              *EntrytoEdit;
    bool                   entryIsOnLocalLan;

    MythPopupBox          *addDirectoryPopup;
    MythRemoteLineEdit    *newDirName;

    MythPopupBox          *incallPopup;

    bool SelectHit; // Indicates the user hit select to stop "false alarms"
};



class PhoneUIStatusBar : public QObject
{

  Q_OBJECT

  public:

    PhoneUIStatusBar(UITextType *a, UITextType *b, UITextType *c, UITextType *d, UITextType *e, UITextType *f, QObject *parent = 0, const char * = 0);
    ~PhoneUIStatusBar();
    void DisplayInCallStats(bool initialise);
    void DisplayCallState(QString s);
    void DisplayNotification(QString s, int Seconds);
    void updateMidCallCaller(QString t);
    void updateMidCallTime(int Seconds);
    void updateMidCallAudioStats(int pIn, int pMiss, int pLate, int pOut, int bIn, int bOut, int msPeriod);
    void updateMidCallVideoStats(int pIn, int pMiss, int pLate, int pOut, int bIn, int bOut, int msPeriod);
    void updateMidCallVideoCodec(QString c);
    void updateMidCallAudioCodec(QString c);

  public slots:
    void notificationTimeout();

  private:
    QTimer *notificationTimer;

    bool modeInCallStats;
    bool modeNotification;
    QString callerString;
    QString TimeString;
    QString audioStatsString;
    QString videoStatsString;
    QString bwStatsString;
    QString statusMsgString;
    QString callStateString;

    // Stats
    QString statsVideoCodec;
    QString statsAudioCodec;
    int audLast_pIn;
    int audLast_pLoss;
    int audLast_pTotal;
    int audLast_bIn;
    int audLast_bOut;
    int vidLast_bIn;
    int vidLast_bOut;
    int vidLast_pIn;
    int vidLast_pLoss;
    int vidLast_pTotal;
    int last_bOut;
    QTime lastPoll;

    UITextType       *callerText;
    UITextType       *audioStatsText;
    UITextType       *videoStatsText;
    UITextType       *bwStatsText;
    UITextType       *callTimeText;
    UITextType       *statusMsgText;


};



#endif
