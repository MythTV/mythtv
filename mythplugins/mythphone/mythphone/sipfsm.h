/*
	SipFsm.h

	(c) 2003 Paul Volkaerts

	
*/

#ifndef SIPFSM_H_
#define SIPFSM_H_

#include <qsqldatabase.h>
#include <qregexp.h>
#include <qtimer.h>
#include <qptrlist.h>
#include <qthread.h>
#include <qwidget.h>
#include <qsocketdevice.h>
#include <qdatetime.h>

#include <sys/types.h>
#include <sys/stat.h>
#ifndef WIN32
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/videodev.h>
#include <mythtv/mythwidgets.h>
#include <mythtv/dialogbox.h>
#endif

#include "sipstack.h"

#ifndef WIN32
#include "vxml.h"
#endif


class SipEvent : public QCustomEvent
{
public:
    enum Type { SipStateChange = (QEvent::User + 400), SipNotification, SipStartMedia, SipStopMedia, SipChangeMedia, 
                SipAlertUser, SipCeaseAlertUser, SipRingbackTone, SipCeaseRingbackTone };

    SipEvent(Type t) : QCustomEvent(t) {}
    SipEvent(Type t, QString rIp, int ap, QString ac, int vp, QString vc, int dp, int rap, int rvp, QString vr) : QCustomEvent(t) 
    {
        audPayload=ap; vidPayload=vp; dtmfPayload=dp; remoteAudioPort=rap; remoteVideoPort=rvp;
        audCodec=ac; vidCodec=vc; vidResolution=vr;
        remoteIp=rIp;
    }
    SipEvent(Type t, QString cUser, QString cUrl, QString cName, bool cAudio) : QCustomEvent(t) 
    {
        callerUser = cUser; callerName = cName; callerUrl = cUrl; callIsAudioOnly = cAudio;
    }
    ~SipEvent() {}
    int getAudioPayload() { return audPayload; }
    int getVideoPayload() { return vidPayload; }
    int getDTMFPayload()  { return dtmfPayload; }
    int getAudioPort()    { return remoteAudioPort; }
    int getVideoPort()    { return remoteVideoPort; }
    QString getRemoteIp() { return remoteIp; }
    QString getAudioCodec() { return audCodec; }
    QString getVideoCodec() { return vidCodec; }
    QString getVideoRes()   { return vidResolution; }
    QString getCallerUser() { return callerUser; }
    QString getCallerUrl()  { return callerUrl; }
    QString getCallerName() { return callerName; }
    bool    getCallIsAudioOnly() { return callIsAudioOnly; }

private:
    int audPayload, vidPayload, dtmfPayload, remoteAudioPort, remoteVideoPort;
    QString audCodec, vidCodec, vidResolution, remoteIp;
    QString callerUser, callerName, callerUrl;
    bool callIsAudioOnly;
};

class SipDebugEvent : public QCustomEvent
{
public:
    enum Type { SipDebugEv = (QEvent::User + 430), SipErrorEv, SipTraceRxEv, SipTraceTxEv  };

    SipDebugEvent(Type t, QString s) : QCustomEvent(t) { text=s;}
    ~SipDebugEvent() {}
    QString msg() { return text;}

private:
    QString text;
};



// Call States
#define SIP_IDLE                0x1
#define SIP_OCONNECTING1        0x2    // Invite sent, no response yet
#define SIP_OCONNECTING2        0x3    // Invite sent, 1xx response
#define SIP_ICONNECTING         0x4
#define SIP_ICONNECTING_WAITACK 0x5
#define SIP_CONNECTED           0x6
#define SIP_DISCONNECTING       0x7
#define SIP_CONNECTED_VXML      0x8    // This is a false state, only used as indication back to the frontend
#define SIP_CONNECT_MODIFYING1  0x9    // Connected; sent a re-Invite to modify the SDP
#define SIP_CONNECT_MODIFYING2  0xA    // Connected; rxed a re-Invite and sent a 200ok, waiting for ACK

// Registration States
#define SIP_REG_DISABLED        0x01   // Proxy registration turned off
#define SIP_REG_TRYING          0x02   // Sent a REGISTER, waiting for an answer
#define SIP_REG_CHALLENGED      0x03   // Initial REGISTER was met with a challenge, sent an authorized REGISTER
#define SIP_REG_FAILED          0x04   // REGISTER failed; will retry after a period of time
#define SIP_REG_REGISTERED      0x05   // Registration successful

// Presence Subscriber States
#define SIP_SUB_IDLE            SIP_IDLE
#define SIP_SUB_SUBSCRIBED      0x10

// Presence Watcher States
#define SIP_WATCH_IDLE          SIP_IDLE
#define SIP_WATCH_TRYING        0x20
#define SIP_WATCH_ACTIVE        0x21
#define SIP_WATCH_STOPPING      0x22
#define SIP_WATCH_HOLDOFF       0x23

// IM States
#define SIP_IM_IDLE             SIP_IDLE
#define SIP_IM_ACTIVE           0x30


// Events
#define SIP_UNKNOWN             0x0
#define SIP_OUTCALL             0x100
#define SIP_INVITE              0x200
#define SIP_INVITESTATUS_2xx    0x300
#define SIP_INVITESTATUS_1xx    0x400
#define SIP_INVITESTATUS_3456xx 0x500
#define SIP_ANSWER              0x600
#define SIP_ACK                 0x700
#define SIP_BYE                 0x800
#define SIP_HANGUP              0x900
#define SIP_BYESTATUS           0xA00
#define SIP_CANCEL              0xB00
#define SIP_CANCELSTATUS        0xC00
#define SIP_REGISTER            0xD00
#define SIP_RETX                0xE00
#define SIP_REGISTRAR_TEXP      0xF00
#define SIP_REGSTATUS           0x1000
#define SIP_REG_TREGEXP         0x1100
#define SIP_SUBSCRIBE           0x1200
#define SIP_SUBSTATUS           0x1300
#define SIP_NOTIFY              0x1400
#define SIP_NOTSTATUS           0x1500
#define SIP_PRESENCE_CHANGE     0x1600
#define SIP_SUBSCRIBE_EXPIRE    0x1700
#define SIP_WATCH               0x1800
#define SIP_STOPWATCH           0x1900
#define SIP_MESSAGE             0x1A00
#define SIP_MESSAGESTATUS       0x1B00
#define SIP_INFO                0x1C00
#define SIP_INFOSTATUS          0x1D00
#define SIP_IM_TIMEOUT          0x1E00
#define SIP_USER_MESSAGE        0x1F00
#define SIP_KICKWATCH           0x2000
#define SIP_MODIFYSESSION       0x2100
#define SIP_OPTIONS             0x2200

#define SIP_CMD(s)              (((s)==SIP_INVITE) || ((s)==SIP_ACK) || ((s)==SIP_BYE) || ((s)==SIP_CANCEL) || ((s)==SIP_REGISTER) || ((s)==SIP_SUBSCRIBE) || ((s)==SIP_NOTIFY) || ((s)==SIP_MESSAGE) || ((s)==SIP_INFO) || ((s)==SIP_OPTIONS))
#define SIP_STATUS(s)           (((s)==SIP_INVITESTATUS_2xx) || ((s)==SIP_INVITESTATUS_1xx) || ((s)==SIP_INVITESTATUS_3456xx) || ((s)==SIP_BYTESTATUS) || ((s)==SIP_CANCELSTATUS) || ((s)==SIP_SUBSTATUS) || ((s)==SIP_NOTSTATUS) || ((s)==SIP_MESSAGESTATUS) || ((s)==SIP_INFOSTATUS) )
#define SIP_MSG(s)              (SIP_CMD(s) || SIP_STATUS(s))

// Call FSM Actions - combination of event and state to give a "switch"able value
#define SIP_IDLE_OUTCALL                  (SIP_IDLE          | SIP_OUTCALL)
#define SIP_IDLE_BYE                      (SIP_IDLE          | SIP_BYE)
#define SIP_IDLE_INVITE                   (SIP_IDLE          | SIP_INVITE)
#define SIP_IDLE_INVITESTATUS_1xx         (SIP_IDLE          | SIP_INVITESTATUS_1xx)
#define SIP_IDLE_INVITESTATUS_2xx         (SIP_IDLE          | SIP_INVITESTATUS_2xx)
#define SIP_IDLE_INVITESTATUS_3456        (SIP_IDLE          | SIP_INVITESTATUS_3456xx)
#define SIP_OCONNECTING1_INVITESTATUS_3456 (SIP_OCONNECTING1  | SIP_INVITESTATUS_3456xx)
#define SIP_OCONNECTING1_INVITESTATUS_2xx (SIP_OCONNECTING1  | SIP_INVITESTATUS_2xx)
#define SIP_OCONNECTING1_INVITESTATUS_1xx (SIP_OCONNECTING1  | SIP_INVITESTATUS_1xx)
#define SIP_OCONNECTING1_RETX             (SIP_OCONNECTING1  | SIP_RETX)
#define SIP_OCONNECTING2_INVITESTATUS_3456 (SIP_OCONNECTING2  | SIP_INVITESTATUS_3456xx)
#define SIP_OCONNECTING2_INVITESTATUS_2xx (SIP_OCONNECTING2  | SIP_INVITESTATUS_2xx)
#define SIP_OCONNECTING2_INVITESTATUS_1xx (SIP_OCONNECTING2  | SIP_INVITESTATUS_1xx)
#define SIP_OCONNECTING1_HANGUP           (SIP_OCONNECTING1  | SIP_HANGUP)
#define SIP_OCONNECTING2_HANGUP           (SIP_OCONNECTING2  | SIP_HANGUP)
#define SIP_OCONNECTING1_INVITE           (SIP_OCONNECTING1  | SIP_INVITE)
#define SIP_ICONNECTING_INVITE            (SIP_ICONNECTING   | SIP_INVITE)
#define SIP_ICONNECTING_ANSWER            (SIP_ICONNECTING   | SIP_ANSWER)
#define SIP_ICONNECTING_CANCEL            (SIP_ICONNECTING   | SIP_CANCEL)
#define SIP_ICONNECTING_WAITACK_ACK       (SIP_ICONNECTING_WAITACK | SIP_ACK)
#define SIP_ICONNECTING_WAITACK_RETX      (SIP_ICONNECTING_WAITACK | SIP_RETX)
#define SIP_ICONNECTING_WAITACK_CANCEL    (SIP_ICONNECTING_WAITACK | SIP_CANCEL)
#define SIP_ICONNECTING_WAITACK_HANGUP    (SIP_ICONNECTING_WAITACK | SIP_HANGUP)
#define SIP_CONNECTED_ACK                 (SIP_CONNECTED     | SIP_ACK)
#define SIP_CONNECTED_INVITESTATUS_2xx    (SIP_CONNECTED     | SIP_INVITESTATUS_2xx)
#define SIP_CONNECTED_RETX                (SIP_CONNECTED     | SIP_RETX)
#define SIP_CONNECTED_BYE                 (SIP_CONNECTED     | SIP_BYE)
#define SIP_CONNECTED_HANGUP              (SIP_CONNECTED     | SIP_HANGUP)
#define SIP_DISCONNECTING_BYESTATUS       (SIP_DISCONNECTING | SIP_BYESTATUS)
#define SIP_DISCONNECTING_ACK             (SIP_DISCONNECTING | SIP_ACK)
#define SIP_DISCONNECTING_RETX            (SIP_DISCONNECTING | SIP_RETX)
#define SIP_DISCONNECTING_CANCEL          (SIP_DISCONNECTING | SIP_CANCEL)
#define SIP_DISCONNECTING_CANCELSTATUS    (SIP_DISCONNECTING | SIP_CANCELSTATUS)
#define SIP_DISCONNECTING_BYE             (SIP_DISCONNECTING | SIP_BYE)
#define SIP_CONNECTED_MODIFYSESSION       (SIP_CONNECTED     | SIP_MODIFYSESSION)
#define SIP_CONNECTED_INVITE              (SIP_CONNECTED     | SIP_INVITE)
#define SIP_CONNMOD1_INVITESTATUS_1xx     (SIP_CONNECT_MODIFYING1 | SIP_INVITESTATUS_1xx)
#define SIP_CONNMOD1_INVITESTATUS_2xx     (SIP_CONNECT_MODIFYING1 | SIP_INVITESTATUS_2xx)
#define SIP_CONNMOD1_INVITESTATUS_3456    (SIP_CONNECT_MODIFYING1 | SIP_INVITESTATUS_3456xx)
#define SIP_CONNMOD1_RETX                 (SIP_CONNECT_MODIFYING1 | SIP_RETX)
#define SIP_CONNMOD2_ACK                  (SIP_CONNECT_MODIFYING2 | SIP_ACK)
#define SIP_CONNMOD2_RETX                 (SIP_CONNECT_MODIFYING2 | SIP_RETX)

// Registration FSM Actions - combination of event and state to give a "switch"able value
#define SIP_REG_TRYING_STATUS             (SIP_REG_TRYING    | SIP_REGSTATUS)
#define SIP_REG_CHALL_STATUS              (SIP_REG_CHALLENGED| SIP_REGSTATUS)
#define SIP_REG_REGISTERED_TREGEXP        (SIP_REG_REGISTERED| SIP_REG_TREGEXP)
#define SIP_REG_TRYING_RETX               (SIP_REG_TRYING    | SIP_RETX)
#define SIP_REG_CHALL_RETX                (SIP_REG_CHALLENGED| SIP_RETX)
#define SIP_REG_FAILED_RETX               (SIP_REG_FAILED    | SIP_RETX)

// Presence Subscriber FSM Actions - combination of event and state to give a "switch"able value
#define SIP_SUB_IDLE_SUBSCRIBE            (SIP_SUB_IDLE       | SIP_SUBSCRIBE)
#define SIP_SUB_SUBS_SUBSCRIBE            (SIP_SUB_SUBSCRIBED | SIP_SUBSCRIBE)
#define SIP_SUB_SUBS_SUBSCRIBE_EXPIRE     (SIP_SUB_SUBSCRIBED | SIP_SUBSCRIBE_EXPIRE)
#define SIP_SUB_SUBS_RETX                 (SIP_SUB_SUBSCRIBED | SIP_RETX)
#define SIP_SUB_SUBS_NOTSTATUS            (SIP_SUB_SUBSCRIBED | SIP_NOTSTATUS)
#define SIP_SUB_SUBS_PRESENCE_CHANGE      (SIP_SUB_SUBSCRIBED | SIP_PRESENCE_CHANGE)

// Presence Watcher FSM Actions - combination of event and state to give a "switch"able value
#define SIP_WATCH_IDLE_WATCH              (SIP_WATCH_IDLE     | SIP_WATCH)
#define SIP_WATCH_TRYING_WATCH            (SIP_WATCH_TRYING   | SIP_WATCH)
#define SIP_WATCH_ACTIVE_SUBSCRIBE_EXPIRE (SIP_WATCH_ACTIVE   | SIP_SUBSCRIBE_EXPIRE)
#define SIP_WATCH_TRYING_RETX             (SIP_WATCH_TRYING   | SIP_RETX)
#define SIP_WATCH_ACTIVE_RETX             (SIP_WATCH_ACTIVE   | SIP_RETX)
#define SIP_WATCH_TRYING_SUBSTATUS        (SIP_WATCH_TRYING   | SIP_SUBSTATUS)
#define SIP_WATCH_ACTIVE_SUBSTATUS        (SIP_WATCH_ACTIVE   | SIP_SUBSTATUS)
#define SIP_WATCH_ACTIVE_NOTIFY           (SIP_WATCH_ACTIVE   | SIP_NOTIFY)
#define SIP_WATCH_TRYING_STOPWATCH        (SIP_WATCH_TRYING   | SIP_STOPWATCH)
#define SIP_WATCH_ACTIVE_STOPWATCH        (SIP_WATCH_ACTIVE   | SIP_STOPWATCH)
#define SIP_WATCH_STOPPING_RETX           (SIP_WATCH_STOPPING | SIP_RETX)
#define SIP_WATCH_STOPPING_SUBSTATUS      (SIP_WATCH_STOPPING | SIP_SUBSTATUS)
#define SIP_WATCH_TRYING_SUBSCRIBE        (SIP_WATCH_TRYING   | SIP_SUBSCRIBE)
#define SIP_WATCH_HOLDOFF_WATCH           (SIP_WATCH_HOLDOFF  | SIP_WATCH)
#define SIP_WATCH_HOLDOFF_STOPWATCH       (SIP_WATCH_HOLDOFF  | SIP_STOPWATCH)
#define SIP_WATCH_HOLDOFF_SUBSCRIBE       (SIP_WATCH_HOLDOFF  | SIP_SUBSCRIBE)
#define SIP_WATCH_HOLDOFF_KICK            (SIP_WATCH_HOLDOFF  | SIP_KICKWATCH)


// Build Options logically OR'ed and sent to build procs
#define SIP_OPT_SDP		          1
#define SIP_OPT_CONTACT		          2
#define SIP_OPT_VIA                       4
#define SIP_OPT_ALLOW                     8
#define SIP_OPT_EXPIRES                   16
#define SIP_OPT_TIMESTAMP                 32

// Timers
#define REG_RETRY_TIMER                   3000 // seconds
#define REG_FAIL_RETRY_TIMER              180000 // 3 minutes
#define REG_RETRY_MAXCOUNT                5
#define SIP_POLL_OFFLINE_UA               180000 // 3 minutes - period between polls to see if a logged off user has logged on

#define SIP_POLL_PERIOD                   2   // Twice per second


// Forward reference.
class SipFsm;
class SipTimer;
class SipThread;
class SipRegisteredUA;
class SipRegistrar;
class SipRegistration;
class SipContainer;

// Global variable ref
extern SipContainer  *sipContainer;


struct CodecNeg
{
    int Payload;
    QString Encoding;
};
#define MAX_AUDIO_CODECS   5        // Make 1 more than max so we can use last place in array as a terminator

class SipContainer 
{
  public:
    SipContainer();
    ~SipContainer();
    void PlaceNewCall(QString Mode, QString uri, QString name, bool disableNat);
    void AnswerRingingCall(QString Mode, bool disableNat);
    void HangupCall();
    void ModifyCall(QString audCodec, QString vidCodec="UNCHANGED");
    void UiOpened(QObject *);
    void UiClosed();
    void UiWatch(QStrList uriList);
    void UiWatch(QString uri);
    void UiStopWatchAll();
    QString UiSendIMMessage(QString DestUrl, QString CallId, QString Msg);
    bool GetNotification(QString &type, QString &url, QString &param1, QString &param2);
    void GetRegistrationStatus(bool &Registered, QString &RegisteredTo, QString &RegisteredAs);
    int  GetSipState();
    void GetIncomingCaller(QString &u, QString &d, QString &l, bool &audOnly);
    void GetSipSDPDetails(QString &ip, int &aport, int &audPay, QString &audCodec, int &dtmfPay, int &vport, int &vidPay, QString &vidCodec, QString &vidRes);
    void notifyRegistrationStatus(bool reg, QString To, QString As) { regStatus=reg; regTo=To; regAs=As;}
    void notifyCallState(int s) { CallState=s;}
    void notifySDPDetails(QString ip, int aport, int audPay, QString audCodec, int dtmfPay, int vport, int vidPay, QString vidCodec, QString vidRes)
            { remoteIp=ip; remoteAudioPort=aport; audioPayload=audPay; audioCodec=audCodec;
              dtmfPayload=dtmfPay; remoteVideoPort=vport; videoPayload=vidPay; videoCodec=vidCodec; videoRes=vidRes; }
    void notifyCallerDetails(QString cU, QString cN, QString cUrl, bool inAudOnly)
            { callerUser=cU; callerName=cN; callerUrl=cUrl; inAudioOnly=inAudOnly; }
    bool killThread() { return killSipThread; }
    QString getLocalIpAddress();
    QString getNatIpAddress();


  private:
	SipThread *sipThread;
    bool killSipThread;
    int CallState;
    bool regStatus;
    QString regTo;
    QString regAs;
    QString callerUser, callerName, callerUrl;
    bool inAudioOnly;
    QString remoteIp;
    int remoteAudioPort;
    int remoteVideoPort;
    int audioPayload;
    int dtmfPayload;
    int videoPayload;
    QString audioCodec;
    QString videoCodec;
    QString videoRes;
};



class SipThread : public QThread
{
  public:
	SipThread(SipContainer *c) { sipContainer = c;};
	~SipThread() {};
	virtual void run();

  private:
    void SipThreadWorker();
    void CheckUIEvents(SipFsm *sipFsm);
    void CheckNetworkEvents(SipFsm *sipFsm);
    void CheckRegistrationStatus(SipFsm *sipFsm);
    void ChangePrimaryCallState(SipFsm *sipFsm, int NewState);

	SipContainer *sipContainer;
    bool FrontEndActive;
    bool vxmlCallActive;
#ifndef WIN32
    vxmlParser *vxml;
    rtp *Rtp;
#endif
    int CallState;
    QString callerUser, callerName, callerUrl;
    bool inAudioOnly;
    QString remoteIp;
    int remoteAudioPort;
    int remoteVideoPort;
    int audioPayload;
    int dtmfPayload;
    int videoPayload;
    QString audioCodec;
    QString videoCodec;
    QString videoRes;
    int rnaTimer;                         // Ring No Answer Timer
};



// This is a base class to allow the SipFsm class to pass events to multiple FSMs
class SipFsmBase
{
  public:
    SipFsmBase(SipFsm *p);
    virtual ~SipFsmBase();
    virtual int     FSM(int Event, SipMsg *sipMsg=0, void *Value=0) { (void)Event; (void)sipMsg; (void)Value; return 0; }
    virtual QString type()       { return "BASE"; }
    virtual SipUrl *getUrl()     { return remoteUrl; }
    virtual int     getCallRef() { return -1; }
    QString callId()   { return CallId.string(); }

  protected:
    void BuildSendStatus(int Code, QString Method, int statusCseq, int Option=0, int statusExpires=-1, QString sdp="");
    void ParseSipMsg(int Event, SipMsg *sipMsg);
    bool Retransmit(bool force);
    void DebugFsm(int event, int old_state, int new_state);
    QString EventtoString(int Event);
    QString StatetoString(int S);

    QString retx;
    QString retxIp;
    int retxPort;
    int t1;
    bool sentAuthenticated;
    SipFsm   *parent;

    SipCallId CallId;
    QString viaIp;
    int viaPort;
    int rxedTimestamp;
    QString myTag;
    QString remoteTag;
    QString remoteEpid;
    QString rxedTo;
    QString rxedFrom;
    QString RecRoute;
    QString Via;
    SipUrl *remoteUrl;
    SipUrl *toUrl;
    SipUrl *contactUrl;
    SipUrl *recRouteUrl;
    SipUrl *MyUrl;
    SipUrl *MyContactUrl;

};


class SipRegistration : public SipFsmBase
{
  public:
    SipRegistration(SipFsm *par, QString localIp, int localPort, QString Username, QString Password, QString ProxyName, int ProxyPort);
    ~SipRegistration();
    virtual int  FSM(int Event, SipMsg *sipMsg=0, void *Value=0);
    virtual QString type()     { return "REGISTRATION"; };
    bool isRegistered()        { return (State == SIP_REG_REGISTERED); }
    QString registeredTo()     { return ProxyUrl->getHost(); }
    QString registeredAs()     { return MyContactUrl->getUser(); }
    int     registeredPort()   { return ProxyUrl->getPort(); }
    QString registeredPasswd() { return MyPassword; }

  private:
    void SendRegister(SipMsg *authMsg=0);

    int State;
    int Expires;
    QString sipLocalIp;
    int sipLocalPort;
    int regRetryCount;

    SipUrl *ProxyUrl;
    QString MyPassword;
    int cseq;
};


class SipCall : public SipFsmBase
{
  public:
    SipCall(QString localIp, QString natIp, int localPort, int n, SipFsm *par);
    ~SipCall();
    int  getState() { return State; };
    void setVideoPayload(int p) { videoPayload = p; };
    void setVideoResolution(QString v) { txVideoResolution = v; };
    void setAllowVideo(bool a)  { allowVideo = a; };
    void setDisableNat(bool n)  { disableNat = n; };
    void to(QString uri, QString DispName) { DestinationUri = uri; DisplayName = DispName; };
    void dialViaProxy(SipRegistration *s) { viaRegProxy = s; };
    virtual int  FSM(int Event, SipMsg *sipMsg=0, void *Value=0);
    virtual QString type() { return "CALL"; };
    virtual int     getCallRef() { return callRef; };
    void GetIncomingCaller(QString &u, QString &d, QString &l, bool &aud)
            { u = CallersUserid; d = CallersDisplayName; l = CallerUrl; aud = (videoPayload == -1); }
    void GetSdpDetails(QString &ip, int &aport, int &audPay, QString &audCodec, int &dtmfPay, int &vport, int &vidPay, QString &vidCodec, QString &vidRes)
            { ip=remoteIp; aport=remoteAudioPort; vport=remoteVideoPort; audPay = CodecList[audioPayloadIdx].Payload; 
              audCodec = CodecList[audioPayloadIdx].Encoding; dtmfPay = dtmfPayload; vidPay = videoPayload; 
              vidCodec = (vidPay == 34 ? "H263" : ""); vidRes = rxVideoResolution; }
    bool ModifyCodecs(QString audioCodec, QString videoCodec);

  private:
    int       State;
    int       callRef;

    void initialise();
    bool UseNat();
    void ForwardMessage(SipMsg *msg);
    void BuildSendInvite(SipMsg *authMsg);
    void BuildSendReInvite(SipMsg *authMsg);
    void BuildSendAck();
    void BuildSendBye(SipMsg *authMsg);
    void BuildSendCancel(SipMsg *authMsg);
    void AlertUser(SipMsg *rxMsg);
    void GetSDPInfo(SipMsg *sipMsg);
    void addSdpToInvite(SipMsg& msg, bool advertiseVideo, int audioCodec=-1);
    QString BuildSdpResponse();

    QString DestinationUri;
    QString DisplayName;
    CodecNeg CodecList[MAX_AUDIO_CODECS];
    QString txVideoResolution;
    QString rxVideoResolution;

    int cseq;
    SipRegistration *viaRegProxy;

    // Incoming call information
    QString CallersUserid;
    QString CallersDisplayName;
    QString CallerUrl;
    QString remoteIp;
    int     remoteAudioPort;
    int     remoteVideoPort;
    int     audioPayloadIdx;
    int     videoPayload;
    int     dtmfPayload;
    bool    allowVideo;
    bool    disableNat;
    int     ModifyAudioCodec;

    QString myDisplayName;	// The name to display when I call others
    QString sipLocalIP;
    QString sipNatIP;
    int     sipLocalPort;
    QString sipUsername;

    int sipRtpPacketisation;	// RTP Packetisation period in ms
    int sipAudioRtpPort;		
    int sipVideoRtpPort;		
};


class SipSubscriber : public SipFsmBase
{
  public:
    SipSubscriber(SipFsm *par, QString localIp, int localPort, SipRegistration *reg, QString status);
    ~SipSubscriber();
    virtual int  FSM(int Event, SipMsg *sipMsg=0, void *Value=0);
    virtual QString type() { return "SUBSCRIBER"; };
    virtual SipUrl *getUrl()     { return watcherUrl; }

  private:
    void SendNotify(SipMsg *authMsg);

    QString sipLocalIp;
    int sipLocalPort;
    SipRegistration *regProxy;
    QString myStatus;

    int State;
    SipUrl *watcherUrl;
    int expires;
    int cseq;
};


class SipWatcher : public SipFsmBase
{
  public:
    SipWatcher(SipFsm *par, QString localIp, int localPort, SipRegistration *reg, QString destUrl);
    ~SipWatcher();
    virtual int  FSM(int Event, SipMsg *sipMsg=0, void *Value=0);
    virtual QString type() { return "WATCHER"; };
    virtual SipUrl *getUrl()     { return watchedUrl; }

  private:
    void SendSubscribe(SipMsg *authMsg);

    QString sipLocalIp;
    int sipLocalPort;
    SipRegistration *regProxy;
    SipUrl *watchedUrl;
    QString watchedUrlString;
    int State;
    int expires;
    int cseq;
};


class SipIM : public SipFsmBase
{
  public:
    SipIM(SipFsm *par, QString localIp, int localPort, SipRegistration *reg, QString destUrl="", QString callIdStr="");
    ~SipIM();
    virtual int  FSM(int Event, SipMsg *sipMsg=0, void *Value=0);
    virtual QString type() { return "IM"; };

  private:
    void SendMessage(SipMsg *authMsg, QString Text);

    QString msgToSend;
    QString sipLocalIp;
    int sipLocalPort;
    SipUrl *imUrl;
    SipRegistration *regProxy;
    int State;
    int rxCseq;
    int txCseq;
};


class SipOptions : public SipFsmBase
{
  public:
    SipOptions(SipFsm *par, QString localIp, int localPort, SipRegistration *reg, QString callIdStr="");
    ~SipOptions();
    virtual int FSM(int Event, SipMsg *sipMsg=0, void *Value=0);
    virtual QString type() { return "Options"; };

  private:
    QString sipLocalIp;
    int sipLocalPort;
    SipRegistration *regProxy;
    int rxCseq;
    int txCseq;
};


class SipFsm : public QWidget
{

  Q_OBJECT

  public:

    SipFsm(QWidget *parent = 0, const char * = 0);
    ~SipFsm(void);
    bool SocketOpenedOk() { return sipSocket != 0 ? true : false; }
    static void Debug(SipDebugEvent::Type t, QString dbg);
    void NewCall(bool audioOnly, QString uri, QString DispName, QString videoMode, bool DisableNat);
    void HangUp(void);
    void Answer(bool audioOnly, QString videoMode, bool DisableNat);
    void ModifyCall(QString audioCodec, QString videoCodec);
    void StatusChanged(char *newStatus);
    void DestroyFsm(SipFsmBase *Fsm);
    void CheckRxEvent();
    SipCall *MatchCall(int cr);
    SipFsmBase *MatchCallId(SipCallId *CallId);
    SipCall *CreateCallFsm();
    SipSubscriber *CreateSubscriberFsm();
    SipWatcher *CreateWatcherFsm(QString Url);
    SipIM *CreateIMFsm(QString Url="", QString callIdStr="");
    SipOptions *CreateOptionsFsm(QString Url="", QString callIdStr="");
    void StopWatchers();
    void KickWatcher(SipUrl *Url);
    void SendIM(QString destUrl, QString CallId, QString imMsg);
    int numCalls();
    int getPrimaryCall() { return primaryCall; };
    int getPrimaryCallState();
    QString OpenSocket(int Port);
    void CloseSocket();
    void Transmit(QString Msg, QString destIP, int destPort);
    bool Receive(SipMsg &sipMsg);
    void SetNotification(QString type, QString url, QString param1, QString param2);
    SipTimer *Timer() { return timerList; };
    void HandleTimerExpiries();
    SipRegistrar *getRegistrar() { return sipRegistrar; }
    bool isRegistered() { return (sipRegistration != 0 && sipRegistration->isRegistered()); }
    QString registeredTo() { if (sipRegistration) return sipRegistration->registeredTo(); else return ""; }
    QString registeredAs() { if (sipRegistration) return sipRegistration->registeredAs(); else return ""; }

//  public slots:
  

  private:
    int MsgToEvent(SipMsg *sipMsg);
    QString DetermineNatAddress();

    int localPort;
    QPtrList<SipFsmBase> FsmList;
    QSocketDevice *sipSocket;
    int callCount;
    int primaryCall;                   // Currently the frontend is only interested in one call at a time, and others are rejected
    int lastStatus;
    SipTimer *timerList;
    SipRegistrar    *sipRegistrar;
    SipRegistration *sipRegistration;
    QString PresenceStatus;
};

class SipRegisteredUA
{
  public:
    SipRegisteredUA(SipUrl *Url, QString cIp, int cPort);
    ~SipRegisteredUA();
    QString getContactIp()   { return contactIp;   }
    int     getContactPort() { return contactPort; }
    bool    matches(SipUrl *u);

  private:
    SipUrl *userUrl;
    QString contactIp;
    int contactPort;
};

class SipRegistrar : public SipFsmBase
{
  public:
    SipRegistrar(SipFsm *par, QString domain, QString localIp, int localPort);
    ~SipRegistrar();
    virtual int  FSM(int Event, SipMsg *sipMsg=0, void *Value=0);
    virtual QString type() { return "REGISTRAR"; };
    bool getRegisteredContact(SipUrl *remoteUrl);

  private:
    void SendResponse(int Code, SipMsg *sipMsg, QString rIp, int rPort);
    void add(SipUrl *Url, QString hostIp, int Port, int Expires);
    void remove(SipUrl *Url);
    SipRegisteredUA *find(SipUrl *Url);

    QPtrList<SipRegisteredUA> RegisteredList;
    QString sipLocalIp;
    int sipLocalPort;
    QString  regDomain;

};


class SipNotify
{
  public:
    SipNotify();
    ~SipNotify();
    void Display(QString name, QString number);

  private:
    QSocketDevice *notifySocket;
};



class aSipTimer
{
  public:
    aSipTimer(SipFsmBase *I, QDateTime exp, int ev, void *v=0) { Instance = I; Expires = exp; Event = ev; Value = v;};
    ~aSipTimer() {};
    bool match(SipFsmBase *I, int ev, void *v=0) { return ((Instance == I) && ((Event == ev) || (ev == -1)) && ((Value == v) || (v == 0))); };
    SipFsmBase *getInstance() { return Instance;  };
    int getEvent()     { return Event; };
    void *getValue()   { return Value; };
    QDateTime getExpire()  { return Expires; };
    bool Expired()     { return (QDateTime::currentDateTime() > Expires); };
  private:
    SipFsmBase *Instance;
    QDateTime Expires;
    int Event;
    void *Value;
};

class SipTimer : public QPtrList<aSipTimer>
{
  public:
    SipTimer();
    ~SipTimer();
    void Start(SipFsmBase *Instance, int ms, int expireEvent, void *Value=0);
    void Stop(SipFsmBase *Instance, int expireEvent, void *Value=0);
    int msLeft(SipFsmBase *Instance, int expireEvent, void *Value=0);
    virtual int compareItems(QPtrCollection::Item s1, QPtrCollection::Item s2);
    void StopAll(SipFsmBase *Instance);
    SipFsmBase *Expired(int *Event, void **Value);

  private:
};




#endif
