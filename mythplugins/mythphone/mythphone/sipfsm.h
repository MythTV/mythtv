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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include <linux/videodev.h>
#include <mythtv/mythwidgets.h>
#include <mythtv/dialogbox.h>

#include "sipstack.h"
#include "vxml.h"




// Call States
#define SIP_IDLE		            0x1
#define SIP_OCONNECTING1        0x2    // Invite sent, no response yet
#define SIP_OCONNECTING2        0x3    // Invite sent, 1xx response
#define SIP_ICONNECTING         0x4
#define SIP_CONNECTED           0x5
#define SIP_DISCONNECTING       0x6
#define SIP_CONNECTED_VXML      0x7    // This is a false state, only used as indication back to the frontend

// Registration States
#define SIP_REG_DISABLED        0x01   // Proxy registration turned off
#define SIP_REG_TRYING          0x02   // Sent a REGISTER, waiting for an answer
#define SIP_REG_CHALLENGED      0x03   // Initial REGISTER was met with a challenge, sent an authorized REGISTER
#define SIP_REG_FAILED          0x04   // REGISTER failed; will retry after a period of time
#define SIP_REG_REGISTERED      0x05   // Registration successful

// Events
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

#define SIP_CMD(s)              (((s)==SIP_INVITE) || ((s)==SIP_ACK) || ((s)==SIP_BYE) || ((s)==SIP_CANCEL) || ((s)==SIP_REGISTER))
#define SIP_STATUS(s)           (((s)==SIP_INVITESTATUS_2xx) || ((s)==SIP_INVITESTATUS_1xx) || ((s)==SIP_INVITESTATUS_3456xx) || ((s)==SIP_BYTESTATUS) || ((s)==SIP_CANCELSTATUS))
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
#define SIP_ICONNECTING_ANSWER            (SIP_ICONNECTING   | SIP_ANSWER)
#define SIP_ICONNECTING_CANCEL            (SIP_ICONNECTING   | SIP_CANCEL)
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

// Registration FSM Actions - combination of event and state to give a "switch"able value
#define SIP_REG_TRYING_STATUS             (SIP_REG_TRYING    | SIP_REGSTATUS)
#define SIP_REG_CHALL_STATUS              (SIP_REG_CHALLENGED| SIP_REGSTATUS)
#define SIP_REG_REGISTERED_TREGEXP        (SIP_REG_REGISTERED| SIP_REG_TREGEXP)
#define SIP_REG_TRYING_RETX               (SIP_REG_TRYING    | SIP_RETX)
#define SIP_REG_CHALL_RETX                (SIP_REG_CHALLENGED| SIP_RETX)
#define SIP_REG_FAILED_RETX               (SIP_REG_FAILED    | SIP_RETX)

// Build Options logically OR'ed and sent to build procs
#define SIP_OPT_SDP		        1
#define SIP_OPT_CONTACT		    2
#define SIP_OPT_VIA           4
#define SIP_OPT_ALLOW         8


// Timers
#define REG_RETRY_TIMER       3000 // seconds
#define REG_FAIL_RETRY_TIMER  180000 // 3 minutes
#define REG_RETRY_MAXCOUNT    5

#define SIP_POLL_PERIOD       2   // Twice per second

// Forward reference.
class SipFsm;
class SipTimer;
class SipRegisteredUA;
class SipRegistrar;
class SipRegistration;

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
    void GetNotification(int &n, QString &ns);
    void GetRegistrationStatus(bool &Registered, QString &RegisteredTo, QString &RegisteredAs);
    int  CheckforRxEvents(bool &Notify);
    int  getCallState();
    void SetFrontEndActive(bool Active) { FrontEndActive = Active; };
    void GetIncomingCaller(QString &u, QString &d, QString &l, bool &audOnly);
    void GetSipSDPDetails(QString &ip, int &aport, int &audPay, QString &audCodec, int &dtmfPay, int &vport, int &vidPay, QString &vidCodec, QString &vidRes);

  protected:
    static void *SipThread(void *p);
    void SipThreadWorker();

  private:
    void CheckUIEvents(SipFsm *sipFsm);
    void CheckNetworkEvents(SipFsm *sipFsm);
    void CheckRegistrationStatus(SipFsm *sipFsm);

    pthread_t sipthread;
    QMutex EventQLock;
    QStringList EventQ;
    bool killSipThread;
    bool FrontEndActive;
    bool vxmlCallActive;
    vxmlParser *vxml;
    rtp *Rtp;
    int CallState;
    int feNotifyValue;
    QString feNotifyString;
    bool feNotify;
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
    int rnaTimer;                         // Ring No Answer Timer
};



// This is a base class to allow the SipFsm class to pass events to multiple FSMs
class SipFsmBase
{
  public:
    SipFsmBase() {};
    virtual ~SipFsmBase() {};
    virtual int  FSM(int Event, SipMsg *sipMsg=0, void *Value=0) { return 0; };

  private:
};


class SipRegistration : public SipFsmBase
{
  public:
    SipRegistration(SipFsm *par, QString localIp, int localPort, QString Username, QString Password, QString ProxyName, int ProxyPort);
    ~SipRegistration();
    virtual int  FSM(int Event, SipMsg *sipMsg=0, void *Value=0);
    bool isRegistered() { return (State == SIP_REG_REGISTERED); }
    QString registeredTo() { return ProxyUrl->getHost(); }
    QString registeredAs() { return MyContactUrl->getUser(); }

  private:
    void SendRegister(SipMsg *authMsg=0);

    int State;
    int Expires;
    QString sipLocalIp;
    int sipLocalPort;
    SipFsm   *parent;
    int regRetryCount;

    SipUrl *ProxyUrl;
    SipUrl *MyUrl;
    SipUrl *MyContactUrl;
    QString MyPassword;
    int cseq;
    SipCallId CallId;
};


class SipCall : public SipFsmBase
{
  public:
    SipCall(QString localIp, QString natIp, int n, SipFsm *par);
    ~SipCall();
    SipCallId *getCallId() { return &CallId; };
    int  getCallRef() { return callRef; };
    int  getState() { return State; };
    void setVideoPayload(int p) { videoPayload = p; };
    void setVideoResolution(QString v) { txVideoResolution = v; };
    void setAllowVideo(bool a)  { allowVideo = a; };
    void setDisableNat(bool n)  { disableNat = n; };
    void to(QString uri, QString DispName) { DestinationUri = uri; DisplayName = DispName; };
    virtual int  FSM(int Event, SipMsg *sipMsg=0, void *Value=0);
    void GetIncomingCaller(QString &u, QString &d, QString &l, bool &aud)
            { u = CallersUserid; d = CallersDisplayName; l = CallerUrl; aud = (videoPayload == -1); }
    void GetSdpDetails(QString &ip, int &aport, int &audPay, QString &audCodec, int &dtmfPay, int &vport, int &vidPay, QString &vidCodec, QString &vidRes)
            { ip=remoteIp; aport=remoteAudioPort; vport=remoteVideoPort; audPay = CodecList[audioPayloadIdx].Payload; 
              audCodec = CodecList[audioPayloadIdx].Encoding; dtmfPay = dtmfPayload; vidPay = videoPayload; 
              vidCodec = (vidPay == 34 ? "H263" : ""); vidRes = rxVideoResolution; }

  private:
    SipCallId CallId;
    SipFsm   *parent;
    int       State;
    int       callRef;

    void initialise();
    void ParseSipMsg(int Event, SipMsg *sipMsg);
    bool UseNat(QString destIPAddress);
    void ForwardMessage(SipMsg *msg);
    void BuildSendInvite();
    void BuildSendAck();
    void BuildSendBye();
    void BuildSendCancel();
    void BuildSendStatus(int Status, QString Method, int Option=0);
    bool Retransmit(bool force);
    void AlertUser(SipMsg *rxMsg);
    void GetSDPInfo(SipMsg *sipMsg);
    void addSdpToInvite(SipMsg& msg, bool advertiseVideo);
    void BuildSdpResponse(SipMsg& msg);
    void DebugFsm(int event, int old_state, int new_state);
    QString EventtoString(int Event);
    QString StatetoString(int S);

    QString DestinationUri;
    QString DisplayName;
    CodecNeg CodecList[MAX_AUDIO_CODECS];
    QString txVideoResolution;
    QString rxVideoResolution;
    QString debugSent;

    QString Via;
    QString RecRoute;
    QString viaIp;
    int viaPort;
    SipUrl *remoteUrl;
    SipUrl *toUrl;
    SipUrl *myUrl;
    SipUrl *contactUrl;
    SipUrl *recRouteUrl;
    QString remoteTag;
    QString myTag;
    QString rxedTo;
    QString rxedFrom;
    int cseq;
    QString retx;
    QString retxIp;
    int retxPort;
    int t1;

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

    QString myDisplayName;	// The name to display when I call others
    QString sipLocalIP;
    QString sipNatIP;
    int     sipLocalPort;
    QString sipUsername;

    int sipRtpPacketisation;	// RTP Packetisation period in ms
    int sipAudioRtpPort;		
    int sipVideoRtpPort;		
};


class SipFsm : public QWidget
{

  Q_OBJECT

  public:

    SipFsm(QWidget *parent = 0, const char * = 0);
    ~SipFsm(void);
    void NewCall(bool audioOnly, QString uri, QString DispName, QString videoMode, bool DisableNat);
    void HangUp(void);
    void Answer(bool audioOnly, QString videoMode, bool DisableNat);
    void CallCleared(SipCall *Call);
    void CheckRxEvent(bool &Notify);
    SipCall *MatchCall(int cr);
    SipCall *MatchCall(SipCallId &CallId);
    int numCalls();
    int getPrimaryCall() { return primaryCall; };
    int getPrimaryCallState();
    QString OpenSocket();
    void CloseSocket();
    void Transmit(QString Msg, QString destIP, int destPort);
    bool Receive(SipMsg &sipMsg);
    void GetNotification(int &n, QString &ns)
            { n = NotificationId; ns = NotificationString; NotificationId = 0; }
    SipTimer *Timer() { return timerList; };
    void HandleTimerExpiries();
    SipRegistrar *getRegistrar() { return sipRegistrar; }
    bool isRegistered() { return (sipRegistration != 0 && sipRegistration->isRegistered()); }
    QString registeredTo() { if (sipRegistration) return sipRegistration->registeredTo(); else return ""; }
    QString registeredAs() { if (sipRegistration) return sipRegistration->registeredAs(); else return ""; }

//  public slots:
  

  private:
    QString DetermineNatAddress();

    QString localIp;
    QString natIp;
    QPtrList<SipCall> CallList;
    QSocketDevice *sipSocket;
    int callCount;
    int primaryCall;                   // Currently the frontend is only interested in one call at a time, and others are rejected
    int lastStatus;
    int NotificationId;
    QString NotificationString;
    SipTimer *timerList;
    SipRegistrar    *sipRegistrar;
    SipRegistration *sipRegistration;
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
    bool getRegisteredContact(SipUrl *remoteUrl);

  private:
    void SendResponse(int Code, SipMsg *sipMsg, QString rIp, int rPort);
    void add(SipUrl *Url, QString hostIp, int Port, int Expires);
    void remove(SipUrl *Url);
    SipRegisteredUA *find(SipUrl *Url);

    QPtrList<SipRegisteredUA> RegisteredList;
    QString sipLocalIp;
    int sipLocalPort;
    SipFsm   *parent;
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
    virtual int compareItems(QPtrCollection::Item s1, QPtrCollection::Item s2);
    void StopAll(SipFsmBase *Instance);
    SipFsmBase *Expired(int *Event, void **Value);

  private:
};




#endif
