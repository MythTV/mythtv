/*
	sipfsm.cpp

	(c) 2003 Paul Volkaerts
	
*/
#include <qapplication.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <net/if.h>
#include <linux/sockios.h>

#include <linux/videodev.h>
#include <mythtv/mythcontext.h>

using namespace std;

#include "config.h"
#include "sipfsm.h"


// Static variables for the debug file used
QFile *debugFile;
QTextStream *debugStream;



/**********************************************************************
SipContainer

This is a container class that runs the SIP protocol stack within a 
separate thread and controls communication with it. This is done
such that the SIP protocol stack can run in the background regardless
of which Myth frontend has focus.

**********************************************************************/

SipContainer::SipContainer()
{
    killSipThread = false;
    FrontEndActive = false;
    CallState = -1;
    feNotify = false;
    rnaTimer = -1;
    vxmlCallActive = false;
    vxml = 0;
    Rtp = 0;

    pthread_create(&sipthread, NULL, SipThread, this);
}

SipContainer::~SipContainer()
{
    killSipThread = true;
    pthread_join(sipthread, NULL);

}

void *SipContainer::SipThread(void *p)
{
    SipContainer *me = (SipContainer *)p;
    me->SipThreadWorker();
    return NULL;
}

void SipContainer::SipThreadWorker()
{
    // Open a file for writing debug info into
    char *homeDir = getenv("HOME");
    QString debugFileName = QString(homeDir) + "/.mythtv/MythPhone/siplog.txt";
    debugFile = new QFile(debugFileName);
    if (debugFile->open(IO_WriteOnly))
        debugStream = new QTextStream (debugFile);

    SipFsm *sipFsm = new SipFsm();

    while(!killSipThread)
    {
        // Sleep, checking SIP stack every 1/2 second
        usleep(1000000/SIP_POLL_PERIOD);  // Not very optimal; can look into event-driving it later

        CheckUIEvents(sipFsm);
        CheckNetworkEvents(sipFsm);
        CheckRegistrationStatus(sipFsm); // Probably don't need to do this every 1/2 sec but this is a fallout of a non event-driven arch.
        sipFsm->HandleTimerExpiries();

        // A Ring No Answer timer runs to send calls to voicemail after x seconds
        if ((CallState == SIP_ICONNECTING) && (rnaTimer != -1))
        {
            if (--rnaTimer < 0)
            {
                rnaTimer = -1;
                vxmlCallActive = true;
                sipFsm->Answer(true, "", false);
            }
        }
    }

    delete sipFsm;
    if (debugStream)
        delete debugStream;
    if (debugFile)
    {
        debugFile->close();
        delete debugFile;
    }
}

void SipContainer::CheckUIEvents(SipFsm *sipFsm)
{
    QString event;
    QStringList::Iterator it;

    // Check why we awoke
    event = "";
    EventQLock.lock();
    if (!EventQ.empty())
    {
        it = EventQ.begin();
        event = *it;
        EventQ.remove(it);
    }
    EventQLock.unlock();

    if (event == "PLACECALL")
    {
        EventQLock.lock();
        it = EventQ.begin();
        QString Mode = *it;
        it = EventQ.remove(it);
        QString Uri = *it;
        it = EventQ.remove(it);
        QString Name = *it;
        it = EventQ.remove(it);
        QString UseNat = *it;
        EventQ.remove(it);
        EventQLock.unlock();
        sipFsm->NewCall(Mode == "AUDIOONLY" ? true : false, Uri, Name, Mode, UseNat == "DisableNAT" ? true : false);
    }
    else if (event == "ANSWERCALL")
    {
        EventQLock.lock();
        it = EventQ.begin();
        QString Mode = *it;
        it = EventQ.remove(it);
        QString UseNat = *it;
        EventQ.remove(it);
        EventQLock.unlock();
        sipFsm->Answer(Mode == "AUDIOONLY" ? true : false, Mode, UseNat == "DisableNAT" ? true : false);
    }
    else if (event == "HANGUPCALL")
        sipFsm->HangUp();
}

void SipContainer::CheckRegistrationStatus(SipFsm *sipFsm)
{
    regStatus = sipFsm->isRegistered();
    regTo     = sipFsm->registeredTo();
    regAs     = sipFsm->registeredAs();
}

void SipContainer::CheckNetworkEvents(SipFsm *sipFsm)
{
    // Periodically check for incoming messages
    int OldState = CallState;
    bool Notification = false;
    sipFsm->CheckRxEvent(Notification);

    // We only handle state changes in the "primary" call; we ignore additional calls which are
    // currently just rejected with busy
    CallState = sipFsm->getPrimaryCallState();

    if (Notification)
    {
        EventQLock.lock();
        sipFsm->GetNotification(feNotifyValue, feNotifyString);
        feNotify = true;
        EventQLock.unlock();
    }

    if (OldState != CallState)
    {
        if (CallState == SIP_IDLE)
        {
            callerUser = "";
            callerName = "";
            callerUrl = "";
            remoteIp = "0.0.0.0";
            remoteAudioPort = -1;
            remoteVideoPort = -1;
            audioPayload = -1;
            dtmfPayload = -1;
            videoPayload = -1;
            audioCodec = "";
            videoCodec = "";
            videoRes = "";
            inAudioOnly = true;
        }

        if (CallState == SIP_ICONNECTING)
        {
             // new incoming call; get the caller info
            EventQLock.lock();
            SipCall *call = sipFsm->MatchCall(sipFsm->getPrimaryCall());
            if (call != 0)
                call->GetIncomingCaller(callerUser, callerName, callerUrl, inAudioOnly);
            EventQLock.unlock();

            rnaTimer = atoi((const char *)gContext->GetSetting("TimeToAnswer")) * SIP_POLL_PERIOD;
        }
        else
            rnaTimer = -1;

          
        if (CallState == SIP_CONNECTED)
        {
            // connected call; get the SDP info
            EventQLock.lock();
            SipCall *call = sipFsm->MatchCall(sipFsm->getPrimaryCall());
            if (call != 0)
                call->GetSdpDetails(remoteIp, remoteAudioPort, audioPayload, audioCodec, dtmfPayload, remoteVideoPort, videoPayload, videoCodec, videoRes);
            EventQLock.unlock();

            if (vxmlCallActive)
            {
                int lPort = atoi((const char *)gContext->GetSetting("AudioLocalPort"));
                QString spk = gContext->GetSetting("AudioOutputDevice");
                Rtp = new rtp(lPort, remoteIp, remoteAudioPort, audioPayload, dtmfPayload, "None", spk, RTP_TX_AUDIO_SILENCE, RTP_RX_AUDIO_DISCARD);
                vxml = new vxmlParser(Rtp, callerName);
            }
        }
          
        if ((CallState == SIP_ICONNECTING) && (FrontEndActive == false))
        {
            // No application running to tell of the incoming call
            // Either alert via on-screen popup or send to voicemail
            SipNotify *notify = new SipNotify();
            notify->Display(callerName, callerUrl);
            delete notify;
        }

        // A call answered by VXML has been disconnected
        if ((OldState == SIP_CONNECTED) && vxmlCallActive)
        {
            vxmlCallActive = false;
            if (vxml != 0)
                delete vxml;
            vxml = 0;
            if (Rtp != 0)
                delete Rtp;
            Rtp = 0;
        }
    }
}


void SipContainer::PlaceNewCall(QString Mode, QString uri, QString name, bool disableNat)
{
    EventQLock.lock();
    EventQ.append("PLACECALL");
    EventQ.append(Mode);
    EventQ.append(uri);
    EventQ.append(name);
    EventQ.append(disableNat ? "DisableNAT" : "EnableNAT");
    EventQLock.unlock();
}

void SipContainer::AnswerRingingCall(QString Mode, bool disableNat)
{
    EventQLock.lock();
    EventQ.append("ANSWERCALL");
    EventQ.append(Mode);
    EventQ.append(disableNat ? "DisableNAT" : "EnableNAT");
    EventQLock.unlock();
}

void SipContainer::HangupCall()
{
    EventQLock.lock();
    EventQ.append("HANGUPCALL");
    EventQLock.unlock();
}

int SipContainer::CheckforRxEvents(bool &Notify)
{
    int tempState;
    EventQLock.lock();
    tempState = CallState;
    if ((tempState == SIP_CONNECTED) && (vxmlCallActive))
        tempState = SIP_CONNECTED_VXML;
    Notify = feNotify;
    EventQLock.unlock();
    return tempState;
}

void SipContainer::GetNotification(int &n, QString &ns)
{
    EventQLock.lock();
    if (feNotify)
    {
        n = feNotifyValue;
        ns = feNotifyString;
        feNotify = false;
    }
    EventQLock.unlock();
}

void SipContainer::GetRegistrationStatus(bool &Registered, QString &RegisteredTo, QString &RegisteredAs)
{
    EventQLock.lock();
    Registered = regStatus;
    RegisteredTo = regTo;
    RegisteredAs = regAs;
    EventQLock.unlock();
}

void SipContainer::GetIncomingCaller(QString &u, QString &d, QString &l, bool &a)
{
    EventQLock.lock();
    u = callerUser;
    d = callerName;
    l = callerUrl;
    a = inAudioOnly;
    EventQLock.unlock();
}

void SipContainer::GetSipSDPDetails(QString &ip, int &aport, int &audPay, QString &audCodec, int &dtmfPay, int &vport, int &vidPay, QString &vidCodec, QString &vidRes)
{
    EventQLock.lock();
    ip = remoteIp;
    aport = remoteAudioPort;
    vport = remoteVideoPort;
    audPay = audioPayload;
    audCodec = audioCodec;
    dtmfPay = dtmfPayload; 
    vidPay = videoPayload; 
    vidCodec = videoCodec;
    vidRes = videoRes;
    EventQLock.unlock();
}


/**********************************************************************
SipFsm

This class forms the container class for the SIP FSM, and creates call
instances which handle actual events.
**********************************************************************/

SipFsm::SipFsm(QWidget *parent, const char *name)
    : QWidget( parent, name )
{
    callCount = 0;
    primaryCall = -1; 
    NotificationId = 0;
    NotificationString = "";

    sipSocket = 0;
    localIp = OpenSocket();
    natIp = DetermineNatAddress();
    if (natIp.length() == 0)
        natIp = localIp;
    cout << "SIP listening on IP Address " << localIp << ":5060 NAT address " << natIp << endl;

    // Create the timer list
    timerList = new SipTimer;

    // Create the Registrar
    sipRegistrar = new SipRegistrar(this, "volkaerts", localIp, 5060);

    // if Proxy Registration is configured ...
    bool RegisterWithProxy = gContext->GetNumSetting("SipRegisterWithProxy",1);
    sipRegistration = 0;
    if (RegisterWithProxy)
    {
        QString ProxyDNS = gContext->GetSetting("SipProxyName");
        QString ProxyUsername = gContext->GetSetting("SipProxyAuthName");
        QString ProxyPassword = gContext->GetSetting("SipProxyAuthPassword");
        if ((ProxyDNS.length() > 0) && (ProxyUsername.length() > 0) && (ProxyPassword.length() > 0))
            sipRegistration = new SipRegistration(this, natIp, 5060, ProxyUsername, ProxyPassword, ProxyDNS, 5060);
        else
            cout << "SIP: Cannot register; proxy, username or password not set\n";
    }

}


SipFsm::~SipFsm()
{
    cout << "Destroying SipFsm object " << endl;
    delete sipRegistrar;
    if (sipRegistration)
        delete sipRegistration;
    delete timerList;
    CloseSocket();
}


QString SipFsm::OpenSocket()
{
    sipSocket = new QSocketDevice (QSocketDevice::Datagram);
    sipSocket->setBlocking(false);

    QString ifName = gContext->GetSetting("SipBindInterface");
    struct ifreq ifreq;
    strcpy(ifreq.ifr_name, ifName);
    if (ioctl(sipSocket->socket(), SIOCGIFADDR, &ifreq) != 0)
    {
        cerr << "Failed to find network interface " << ifName << endl;
        delete sipSocket;
        sipSocket = 0;
        return "";
    }
    struct sockaddr_in * sptr = (struct sockaddr_in *)&ifreq.ifr_addr;
    QHostAddress myIP;
    myIP.setAddress(htonl(sptr->sin_addr.s_addr));

    if (!sipSocket->bind(myIP, 5060))
    {
        cerr << "Failed to bind for SIP connection " << myIP.toString() << endl;
        delete sipSocket;
        sipSocket = 0;
        return "";
    }
    return myIP.toString();
}      

void SipFsm::CloseSocket()
{
    if (sipSocket)
    {
        sipSocket->close();
        delete sipSocket;
        sipSocket = 0;
    }
}


QString SipFsm::DetermineNatAddress()
{
    QString natIP = "";
    QString NatTraversalMethodStr = gContext->GetSetting("NatTraversalMethod");

    if (NatTraversalMethodStr == "Manual")
    {
        natIP = gContext->GetSetting("NatIpAddress");
    }

    // For NAT method "webserver" we send a HTTP GET  to a specified URL and expect the response to
    // contain the NATed IP address. This is based on support for checkip.dyndns.org
    else if (NatTraversalMethodStr == "Web Server")
    {
        // Send a HTTP packet to the configured URL asking for our NAT IP addres
        QString natWebServer = gContext->GetSetting("NatIpAddress");
        QUrl Url(natWebServer);
        QString httpGet = QString("GET %1 HTTP/1.0\r\n"
                                  "User-Agent: MythPhone/1.0\r\n"
                                  "\r\n").arg(Url.path());
        QSocketDevice *httpSock = new QSocketDevice(QSocketDevice::Stream);
        QHostAddress hostIp;
        int port = Url.port();
        if (port == -1)
            port = 80;

        // If the configured web server is not an IP address, do a DNS lookup
        hostIp.setAddress(Url.host());
        if (hostIp.toString() != Url.host())
        {
            // Need a DNS lookup on the URL
            struct hostent *h;
            h = gethostbyname((const char *)Url.host());
            hostIp.setAddress(ntohl(*(long *)h->h_addr));
        }

        // Now send the HTTP GET to the web server and parse the response
        if (httpSock->connect(hostIp, port))
        {
            int bytesAvail;
            if (httpSock->writeBlock(httpGet, httpGet.length()) != -1)
            {
                while ((bytesAvail = httpSock->waitForMore(3000)) != -1)
                {
                    char *httpResponse = new char[bytesAvail+1];
                    int len = httpSock->readBlock(httpResponse, bytesAvail);
                    if (len >= 0)
                    {
                        // Assume body of the response is formatted as "Current IP Address: a.b.c.d"
                        // This is specific to checkip.dyndns.org and may beed to be made more flexible
                        httpResponse[len] = 0;
                        QString resp(httpResponse);

                        if (resp.contains("200 OK") && !resp.contains("</body"))
                        {
                            delete httpResponse;
                            continue;
                        }
                        QString temp1 = resp.section("<body>", 1, 1);
                        QString temp2 = temp1.section("</body>", 0, 0);
                        QString temp3 = temp2.section("Current IP Address: ", 1, 1);

                        natIP = temp3.stripWhiteSpace();
                    }
                    else
                        cout << "Got invalid HTML response: " << endl;
                    delete httpResponse;
                    break;
                }
            }
            else
                cerr << "Error sending NAT discovery packet to socket\n";
        }
        else
            cout << "Could not connect to NAT discovery host " << Url.host() << ":" << Url.port() << endl;
        httpSock->close();
        delete httpSock;
    }

    return natIP;
}

void SipFsm::Transmit(QString Msg, QString destIP, int destPort)
{
    if ((sipSocket) && (destIP.length()>0))
    {
        QHostAddress dest;
        dest.setAddress(destIP);
        if (debugStream)
            *debugStream << QDateTime::currentDateTime().toString() << " Sent to " << destIP << ":" << QString::number(destPort) << "...\n" << Msg << endl;
        sipSocket->writeBlock((const char *)Msg, Msg.length(), dest, destPort);
    }
    else
        cerr << "SIP: Cannot transmit SIP message to " << destIP << endl;
}

bool SipFsm::Receive(SipMsg &sipMsg)
{
    if (sipSocket)
    {
        char rxMsg[1501];
        int len = sipSocket->readBlock(rxMsg, sizeof(rxMsg)-1);

        if (len > 0)
        {
            rxMsg[len] = 0;
            if (debugStream)
                *debugStream << QDateTime::currentDateTime().toString() << " Received: Len " << len << endl << rxMsg << endl;
            sipMsg.decode(rxMsg);
            return true;
        }
    }
    return false;
}

void SipFsm::NewCall(bool audioOnly, QString uri, QString DispName, QString videoMode, bool DisableNat)
{
    int cr = -1;
    if ((numCalls() == 0) || (primaryCall != -1))
    {
        SipCall *Call;
        primaryCall = cr = callCount++;
        Call = new SipCall(localIp, natIp, cr, this);
        CallList.append(Call);

        // If the dialled number if just a username and we are registered to a proxy, dial
        // via the proxy
        if ((!uri.contains('@')) && (sipRegistration != 0) && (sipRegistration->isRegistered()))
            uri.append(QString("@") + gContext->GetSetting("SipProxyName"));

        Call->to(uri, DispName);
        Call->setDisableNat(DisableNat);
        Call->setAllowVideo(audioOnly ? false : true);
        Call->setVideoResolution(videoMode);
        if (Call->FSM(SIP_OUTCALL) == SIP_IDLE)
            CallCleared(Call);
    }
    else
        cerr << "SIP Call attempt with call in progress\n";
}


void SipFsm::HangUp()
{
    SipCall *Call = MatchCall(primaryCall);
    if (Call)
        if (Call->FSM(SIP_HANGUP) == SIP_IDLE)
            CallCleared(Call);
}


void SipFsm::Answer(bool audioOnly, QString videoMode, bool DisableNat)
{
    SipCall *Call = MatchCall(primaryCall);
    if (Call)
    {
        if (audioOnly)
            Call->setVideoPayload(-1);
        else
            Call->setVideoResolution(videoMode);
        Call->setDisableNat(DisableNat);
        if (Call->FSM(SIP_ANSWER) == SIP_IDLE)
            CallCleared(Call);
    }
}


void SipFsm::HandleTimerExpiries()
{
    SipFsmBase *Instance;
    int Event;
    void *Value;
    while ((Instance = timerList->Expired(&Event, &Value)) != 0)
    {
        if (Instance->FSM(Event, 0, Value) == SIP_IDLE)
            CallCleared(dynamic_cast<SipCall *>(Instance));
    }
}


void SipFsm::CallCleared(SipCall *Call)
{
    if (Call != 0)
    {
        timerList->StopAll(Call);
        if (Call->getCallRef() == primaryCall)
            primaryCall = -1;
        if (MatchCall(Call->getCallRef()))
            CallList.remove();
        delete Call;
    }
}

int SipFsm::getPrimaryCallState()
{
    if (primaryCall == -1)
        return SIP_IDLE;

    SipCall *call = MatchCall(primaryCall);
    if (call == 0)
    {
        primaryCall = -1;
        cerr << "Seemed to lose a call here\n";
        return SIP_IDLE;
    }

    return call->getState();
}


void SipFsm::CheckRxEvent(bool &Notify)
{
    int newState = -1;
    SipCall *call = 0;

    Notify = false;  // Set to true if we have something to tell the user

    SipMsg sipRcv;
    if (Receive(sipRcv))
    {
        if (sipRcv.getMethod() == "REGISTER")
            sipRegistrar->FSM(SIP_REGISTER, &sipRcv);
        else if ((sipRcv.getMethod() == "STATUS") && (sipRcv.getCSeqMethod() == "REGISTER"))
            sipRegistration->FSM(SIP_REGSTATUS, &sipRcv);
        else 
        {
            call = MatchCall(sipRcv.getCallId());
            if (sipRcv.getMethod() == "INVITE")
                newState = call->FSM(SIP_INVITE, &sipRcv);
            else if (sipRcv.getMethod() == "ACK")
                newState = call->FSM(SIP_ACK, &sipRcv);
            else if (sipRcv.getMethod() == "BYE")
                newState = call->FSM(SIP_BYE, &sipRcv);
            else if (sipRcv.getMethod() == "CANCEL")
                newState = call->FSM(SIP_CANCEL, &sipRcv);
            else if (sipRcv.getMethod() == "STATUS")
            {
                lastStatus = sipRcv.getStatusCode();
                QString statusMethod = sipRcv.getCSeqMethod();
                if (statusMethod == "BYE")
                    newState = call->FSM(SIP_BYESTATUS, &sipRcv);
                else if (statusMethod == "CANCEL")
                    newState = call->FSM(SIP_CANCELSTATUS, &sipRcv);
                else if ((lastStatus >= 200) && (lastStatus < 300))
                {
                    if (statusMethod == "INVITE")
                        newState = call->FSM(SIP_INVITESTATUS_2xx, &sipRcv);
                    else
                        cerr << "Unknown method in Status " << statusMethod << endl;
                }
                else if ((lastStatus >= 100) && (lastStatus < 200))
                {
                    if (statusMethod == "INVITE")
                        newState = call->FSM(SIP_INVITESTATUS_1xx, &sipRcv);
                    else
                        cerr << "Unknown method in Status " << statusMethod << endl;
                    Notify = true;
                    NotificationId = lastStatus;
                    NotificationString = sipRcv.getReasonPhrase();
                }
                else
                {
                    if (statusMethod == "INVITE")
                        newState = call->FSM(SIP_INVITESTATUS_3456xx, &sipRcv);
                    else
                        cerr << "Unknown method in Status " << statusMethod << endl;
                    Notify = true;
                    NotificationId = lastStatus;
                    NotificationString = sipRcv.getReasonPhrase();
                }
            }

            if ((newState == SIP_IDLE) && (call))
                CallCleared(call);
        }
    }
}



SipCall *SipFsm::MatchCall(int cr)
{
    SipCall *it;
    for (it=CallList.first(); it; it=CallList.next())
        if (it->getCallRef() == cr)
            return it;
    return 0;
}

SipCall *SipFsm::MatchCall(SipCallId &CallId)
{
    SipCall *it;
    for (it=CallList.first(); it; it=CallList.next())
        if (*(it->getCallId()) == CallId)
            return it;

    int cr = callCount++;
    it = new SipCall(localIp, natIp, cr, this);
    if (primaryCall == -1)
        primaryCall = cr;
    CallList.append(it);
    return it;
}

int SipFsm::numCalls()
{
    return CallList.count();
}



/**********************************************************************
SipCall

This class handles a per call instance of the FSM
**********************************************************************/

SipCall::SipCall(QString localIp, QString natIp, int n, SipFsm *par) : SipFsmBase()
{
    callRef = n;
    parent = par;
    sipLocalIP = localIp;
    sipNatIP = natIp;
    initialise();
}

SipCall::~SipCall()
{
    if (remoteUrl != 0)
        delete remoteUrl;
    if (toUrl != 0)
        delete toUrl;
    if (myUrl != 0)
        delete myUrl;
    if (contactUrl != 0)
        delete contactUrl;
    if (recRouteUrl != 0)
        delete recRouteUrl;
    remoteUrl = 0;
    toUrl = 0;
    myUrl = 0;
    contactUrl = 0;
    recRouteUrl = 0;
}


void SipCall::initialise()
{
    char myHostname[64];

    // Initialise Local Parameters.  We get info from the database on every new
    // call in case it has been changed
    myDisplayName = gContext->GetSetting("MySipName");
    sipUsername = gContext->GetSetting("MySipUser");

    // Get other params - done on a per call basis so config changes take effect immediately
    sipAudioRtpPort = atoi((const char *)gContext->GetSetting("AudioLocalPort"));
    sipVideoRtpPort = atoi((const char *)gContext->GetSetting("VideoLocalPort"));

    sipLocalPort = 5060;
    sipRtpPacketisation = 20;
    State = SIP_IDLE;
    remoteAudioPort = 0;
    remoteVideoPort = 0;
    remoteIp = "";
    audioPayloadIdx = -1;
    videoPayload = -1; 
    dtmfPayload = -1;
    remoteIp = "";
    allowVideo = true;
    disableNat = false;
    rxVideoResolution = "CIF";
    txVideoResolution = "CIF";

    remoteUrl = 0;
    toUrl = 0;
    myUrl = 0;
    contactUrl = 0;
    recRouteUrl = 0;
    remoteTag = "";
    myTag = "";
    cseq = 0;
    rxedTo = "";
    rxedFrom = "";


    // Read the codec priority list from the database into an array
    CodecList[0].Payload = 0;
    CodecList[0].Encoding = "PCMU";
    int n=0;       
    QString CodecListString = gContext->GetSetting("CodecPriorityList");
    while ((CodecListString.length() > 0) && (n < MAX_AUDIO_CODECS-1))
    {
        int sep = CodecListString.find(';');
        QString CodecStr = CodecListString;
        if (sep != -1)
            CodecStr = CodecListString.left(sep);
        if (CodecStr == "G.711u")
        {
            CodecList[n].Payload = 0;
            CodecList[n++].Encoding = "PCMU";
        }
        else if (CodecStr == "G.711a")
        {
            CodecList[n].Payload = 8;
            CodecList[n++].Encoding = "PCMA";
        }
#ifdef VA_G729
        else if (CodecStr == "G.729")
        {
            CodecList[n].Payload = 18;
            CodecList[n++].Encoding = "G729";
        }
#endif
        else
            cout << "Unknown codec " << CodecStr << " in Codec Priority List\n";
        if (sep != -1)
        {
            QString tempStr = CodecListString.mid(sep+1);
            CodecListString = tempStr;
        }
        else
            break;
    }
    CodecList[n].Payload = -1;
}


int SipCall::FSM(int Event, SipMsg *sipMsg, void *Value)
{
    int Action = Event | State; // Just prevents messy nested switch constructs
    int oldState = State;
    debugSent = "";


    // Parse SIP messages for general relevant data
    if (sipMsg != 0)
        ParseSipMsg(Event, sipMsg);


    switch(Action)
    {
    case SIP_IDLE_BYE:
        BuildSendStatus(481, "BYE"); //481 Call/Transaction does not exist
        State = SIP_IDLE;
        break;
    case SIP_IDLE_INVITESTATUS_1xx:
    case SIP_IDLE_INVITESTATUS_2xx:
    case SIP_IDLE_INVITESTATUS_3456:
        // Check if we are being a proxy
        if (sipMsg->getViaIp() == sipLocalIP)
        {
            ForwardMessage(sipMsg);
            State = SIP_IDLE;
        }
        break;
    case SIP_IDLE_OUTCALL:
        remoteUrl = new SipUrl(DestinationUri, "");
#ifdef SIPREGISTRAR
        // If the domain matches the local registrar, see if user is registered
        if ((remoteUrl->getHost() == "volkaerts") &&
            (!(parent->getRegistrar())->getRegisteredContact(remoteUrl)))
        {
            cout << DestinationUri << " is not registered here\n";
            break;
        }
#endif
        if (UseNat(remoteUrl->getHostIp()))
            sipLocalIP = sipNatIP;
        myUrl = new SipUrl(myDisplayName, sipUsername, sipLocalIP, sipLocalPort);
        BuildSendInvite();
        State = SIP_OCONNECTING1;
        break;
    case SIP_IDLE_INVITE:
        if (UseNat(remoteUrl->getHostIp()))
            sipLocalIP = sipNatIP;
        myUrl = new SipUrl(myDisplayName, sipUsername, sipLocalIP, sipLocalPort);
#ifdef SIPREGISTRAR
        if ((toUrl->getUser() == sipUsername)) && (toUrl->getHost() ==  "Volkaerts"))
#endif
        {
            if (parent->numCalls() > 1)     // Check there are no active calls, and give busy if there is
            {
                BuildSendStatus(486, "INVITE"); //486 Busy Here
                State = SIP_DISCONNECTING;
            }
            else 
            {
                GetSDPInfo(sipMsg);
                if (audioPayloadIdx != -1) // INVITE had a codec we support; proces
                {
                    AlertUser(sipMsg);
                    BuildSendStatus(100, "INVITE", SIP_OPT_CONTACT); //100 Trying
                    BuildSendStatus(180, "INVITE", SIP_OPT_CONTACT | SIP_OPT_ALLOW); //180 Ringing
                    State = SIP_ICONNECTING;
                }
                else
                {
                    BuildSendStatus(488, "INVITE"); //488 Not Acceptable Here
                    State = SIP_DISCONNECTING;
                }
            }
        }

#ifdef SIPREGISTRAR
        // Not for me, see if it is for a registered UA
        else if ((toUrl->getHost() == "volkaerts") && ((parent->getRegistrar())->getRegisteredContact(toUrl)))
        {
            ForwardMessage(sipMsg);
            State = SIP_IDLE;
        }

        // Not for me and not for anyone registered here
        else
        {
            BuildSendStatus(404, "INVITE"); //404 Not Found
            State = SIP_DISCONNECTING;
        }
#endif
        break;
    case SIP_OCONNECTING1_INVITESTATUS_1xx:
        (parent->Timer())->Stop(this, SIP_RETX);
        State = SIP_OCONNECTING2;
        break;
    case SIP_OCONNECTING1_INVITESTATUS_3456:
        (parent->Timer())->Stop(this, SIP_RETX);
        // Fall through
    case SIP_OCONNECTING2_INVITESTATUS_3456:
        BuildSendAck();
        State = SIP_IDLE;
        break;
    case SIP_OCONNECTING1_INVITESTATUS_2xx:
        (parent->Timer())->Stop(this, SIP_RETX);
        // Fall through
    case SIP_OCONNECTING2_INVITESTATUS_2xx:
        GetSDPInfo(sipMsg);
        if (audioPayloadIdx != -1) // INVITE had a codec we support; proces
        {
            BuildSendAck();
            State = SIP_CONNECTED;
        }
        else
        {
            cerr << "2xx STATUS did not contain a valid Audio codec\n";
            BuildSendAck();  // What is the right thing to do here?
            BuildSendBye();
            State = SIP_DISCONNECTING;
        }
        break;
    case SIP_OCONNECTING1_INVITE:
        // This is usually because we sent the INVITE to ourselves, & when we receive it matches the call-id for this call leg
        (parent->Timer())->Stop(this, SIP_RETX);
        BuildSendCancel();
        State = SIP_DISCONNECTING;
        break;
    case SIP_OCONNECTING1_HANGUP:
        (parent->Timer())->Stop(this, SIP_RETX);
        BuildSendCancel();
        State = SIP_IDLE;
        break;
    case SIP_OCONNECTING1_RETX:
        if (Retransmit(false))
            (parent->Timer())->Start(this, t1, SIP_RETX);
        else
            State = SIP_IDLE;
        break;
    case SIP_OCONNECTING2_HANGUP:
        BuildSendCancel();
        State = SIP_DISCONNECTING;
        break;
    case SIP_ICONNECTING_ANSWER:
        BuildSendStatus(200, "INVITE", SIP_OPT_SDP | SIP_OPT_CONTACT | SIP_OPT_ALLOW);
        State = SIP_CONNECTED;
        break;
    case SIP_ICONNECTING_CANCEL:
        BuildSendStatus(200, "CANCEL"); //200 Ok
        State = SIP_IDLE;
        break;
    case SIP_CONNECTED_ACK:
        (parent->Timer())->Stop(this, SIP_RETX); // Stop resending 200 OKs
        break;
    case SIP_CONNECTED_INVITESTATUS_2xx:
        Retransmit(true); // Resend our ACK
        break;
    case SIP_CONNECTED_RETX:
        if (Retransmit(false))
            (parent->Timer())->Start(this, t1, SIP_RETX);
        else
            State = SIP_IDLE;
        break;
    case SIP_CONNECTED_BYE:
        (parent->Timer())->Stop(this, SIP_RETX); 
        if (sipMsg->getCSeqValue() > cseq)
        {
            cseq = sipMsg->getCSeqValue();
            BuildSendStatus(200, "BYE"); //200 Ok
            State = SIP_IDLE;
        }
        else
            BuildSendStatus(400, "BYE"); //400 Bad Request
        break;
    case SIP_CONNECTED_HANGUP:
        BuildSendBye();
        State = SIP_DISCONNECTING;
        break;
    case SIP_DISCONNECTING_ACK:
        (parent->Timer())->Stop(this, SIP_RETX); 
        State = SIP_IDLE;
        break;
    case SIP_DISCONNECTING_RETX:
        if (Retransmit(false))
            (parent->Timer())->Start(this, t1, SIP_RETX);
        else
            State = SIP_IDLE;
        break;
    case SIP_DISCONNECTING_CANCEL:
        (parent->Timer())->Stop(this, SIP_RETX); 
        BuildSendStatus(200, "CANCEL"); //200 Ok
        State = SIP_IDLE;
        break;
    case SIP_DISCONNECTING_BYESTATUS:
        (parent->Timer())->Stop(this, SIP_RETX); 
        State = SIP_IDLE;
        break;
    case SIP_DISCONNECTING_CANCELSTATUS:
        (parent->Timer())->Stop(this, SIP_RETX); 
        State = SIP_IDLE;
        break;
    case SIP_DISCONNECTING_BYE:
        (parent->Timer())->Stop(this, SIP_RETX); 
        BuildSendStatus(200, "BYE"); //200 Ok
        State = SIP_IDLE;
        break;

    // Events ignored in states
    case SIP_OCONNECTING2_INVITESTATUS_1xx:
        break;

    // Everything else is an error, just flag it for now
    default:
        if (debugStream)
            *debugStream << "SIP FSM Error received " << EventtoString(Event) << " in state " << StatetoString(State) << endl << endl;
        break;
    }

    DebugFsm(Event, oldState, State);
    return State;
}

void SipCall::ParseSipMsg(int Event, SipMsg *sipMsg)
{
    // Pull out Remote TAG
    remoteTag = (SIP_CMD(Event)) ? sipMsg->getFromTag() : sipMsg->getToTag();

    // Pull out VIA, To and From information from CMDs to send back in Status
    if (SIP_CMD(Event))
    {
        Via      = sipMsg->getCompleteVia();
        RecRoute = sipMsg->getCompleteRR();
        viaIp    = sipMsg->getViaIp();
        viaPort  = sipMsg->getViaPort();
        rxedTo   = sipMsg->getCompleteTo();
        rxedFrom = sipMsg->getCompleteFrom();
    }

    // Pull out Contact info
    SipUrl *s;
    if ((s = sipMsg->getContactUrl()) != 0)
    {
        if (contactUrl)
            delete contactUrl;
        contactUrl = new SipUrl(s);
    }

    // Pull out Record Route info
    if ((s = sipMsg->getRecRouteUrl()) != 0)
    {
        if (recRouteUrl)
            delete recRouteUrl;
        recRouteUrl = new SipUrl(s);
    }

    // Pull out Call Specific info when in IDLE
    if (State == SIP_IDLE)
    {
        CallId = sipMsg->getCallId();
        cseq = sipMsg->getCSeqValue();
        remoteUrl = new SipUrl(sipMsg->getFromUrl());
        toUrl     = new SipUrl(sipMsg->getToUrl());
    }
}

bool SipCall::UseNat(QString destIPAddress)
{
    // User to check subnets but this was a flawed concept; now checks a configuration item per-remote user
    return !disableNat;
}


void SipCall::BuildSendInvite()
{
    CallId.Generate(sipLocalIP);

    SipMsg Invite("INVITE");
    Invite.addRequestLine(*remoteUrl);
    Invite.addVia(sipLocalIP, sipLocalPort);
    Invite.addFrom(*myUrl);
    Invite.addTo(*remoteUrl);
    Invite.addCallId(CallId);
    Invite.addCSeq(++cseq);
    Invite.addUserAgent();
    //Invite.addAllow();
    Invite.addContact(*myUrl);
    addSdpToInvite(Invite, allowVideo);
    
    parent->Transmit(Invite.string(), retxIp = remoteUrl->getHostIp(), retxPort = remoteUrl->getPort());
    retx = Invite.string();
    t1 = 500;
    (parent->Timer())->Start(this, t1, SIP_RETX);
}



void SipCall::ForwardMessage(SipMsg *msg)
{
    QString toIp;
    int toPort;

    if (msg->getMethod() != "STATUS")
    {
        msg->insertVia(sipLocalIP, sipLocalPort);
        toIp = toUrl->getHostIp();
        toPort = toUrl->getPort();
    }
    else
    {
        msg->removeVia();
        toIp = msg->getViaIp();
        toPort = msg->getViaPort();
    }
    parent->Transmit(msg->string(), toIp, toPort);
}



void SipCall::BuildSendAck()
{
    debugSent.append("Ack ");

    if ((myUrl == 0) || (remoteUrl == 0))
    {
        cerr << "URL variables not setup\n";
        return;
    }

    SipMsg Ack("ACK");
    Ack.addRequestLine(*remoteUrl);
    Ack.addVia(sipLocalIP, sipLocalPort);
    Ack.addFrom(*myUrl);
    Ack.addTo(*remoteUrl, remoteTag);
    Ack.addCallId(CallId);
    Ack.addCSeq(cseq);
    Ack.addUserAgent();
    Ack.addNullContent();

    // Even if we have a contact URL in one of the response messages; we still send the ACK to
    // the same place we sent the INVITE to
    parent->Transmit(Ack.string(), retxIp = remoteUrl->getHostIp(), retxPort = remoteUrl->getPort());
    retx = Ack.string();
}


void SipCall::BuildSendCancel()
{
    debugSent.append("Cancel ");

    if ((myUrl == 0) || (remoteUrl == 0))
    {
        cerr << "URL variables not setup\n";
        return;
    }

    SipMsg Cancel("CANCEL");
    Cancel.addRequestLine(*remoteUrl);
    Cancel.addVia(sipLocalIP, sipLocalPort);
    Cancel.addTo(*remoteUrl, remoteTag);
    Cancel.addFrom(*myUrl);
    Cancel.addCallId(CallId);
    Cancel.addCSeq(cseq);
    Cancel.addUserAgent();
    Cancel.addNullContent();

    // Send new transactions to (a) record route, (b) contact URL or (c) configured URL
    if (recRouteUrl)
        parent->Transmit(Cancel.string(), retxIp = recRouteUrl->getHostIp(), retxPort = recRouteUrl->getPort());
    else if (contactUrl)
        parent->Transmit(Cancel.string(), retxIp = contactUrl->getHostIp(), retxPort = contactUrl->getPort());
    else
        parent->Transmit(Cancel.string(), retxIp = remoteUrl->getHostIp(), retxPort = remoteUrl->getPort());
    retx = Cancel.string();
    t1 = 500;
    (parent->Timer())->Start(this, t1, SIP_RETX);
}


void SipCall::BuildSendStatus(int Code, QString Method, int Option)
{
    QString statusStr = QString("%1 ").arg(Code);
    debugSent.append(statusStr);

    if ((myUrl == 0) || (remoteUrl == 0))
    {
        cerr << "URL variables not setup\n";
        return;
    }

    SipMsg Status(Method);
    Status.addStatusLine(Code);
    if (RecRoute.length() > 0)
        Status.addRRCopy(RecRoute);
    if (Via.length() > 0)
        Status.addViaCopy(Via);
    Status.addFromCopy(rxedFrom);
    Status.addToCopy(rxedTo);
    //Status.addFrom(*remoteUrl, remoteTag);
    //Status.addTo(*myUrl);
    Status.addCallId(CallId);
    Status.addCSeq(cseq);
//    Status.addUserAgent();
    //if (Option & SIP_OPT_ALLOW) // Add my Contact URL to the message
    //    Status.addAllow();
    if (Option & SIP_OPT_CONTACT) // Add my Contact URL to the message
        Status.addContact(*myUrl);
    if (Option & SIP_OPT_SDP) // Add an SDP to the message
        BuildSdpResponse(Status);
    else
        Status.addNullContent();

    // Send STATUS messages to the VIA address
    parent->Transmit(Status.string(), retxIp = viaIp, retxPort = viaPort);

    retx = Status.string();
    if (((Code >= 200) && (Code <= 299)) && (Method == "INVITE"))
    {
        t1 = 500;
        (parent->Timer())->Start(this, t1, SIP_RETX);
    }
}


void SipCall::BuildSendBye()
{
    debugSent.append("Bye ");

    if ((myUrl == 0) || (remoteUrl == 0))
    {
        cerr << "URL variables not setup\n";
        return;
    }

    SipMsg Bye("BYE");
    Bye.addRequestLine(*remoteUrl);
    Bye.addVia(sipLocalIP, sipLocalPort);
    Bye.addFrom(*myUrl);
    Bye.addTo(*remoteUrl, remoteTag);
    Bye.addCallId(CallId);
    Bye.addCSeq(++cseq);
    Bye.addUserAgent();
    Bye.addNullContent();

    // Send new transactions to (a) record route, (b) contact URL or (c) configured URL
    if (recRouteUrl)
        parent->Transmit(Bye.string(), retxIp = recRouteUrl->getHostIp(), retxPort = recRouteUrl->getPort());
    else if (contactUrl)
        parent->Transmit(Bye.string(), retxIp = contactUrl->getHostIp(), retxPort = contactUrl->getPort());
    else
        parent->Transmit(Bye.string(), retxIp = remoteUrl->getHostIp(), retxPort = remoteUrl->getPort());
    retx = Bye.string();
    t1 = 500;
    (parent->Timer())->Start(this, t1, SIP_RETX);
}

bool SipCall::Retransmit(bool force)
{
    if (force || (t1 < 8000))
    {
        t1 *= 2;
        if ((retx.length() > 0) && (retxIp.length() > 0))
        {
            parent->Transmit(retx, retxIp, retxPort);
            return true;
        }
    }
    return false;
}


void SipCall::AlertUser(SipMsg *rxMsg)
{
    // A new incoming call has been received, tell someone!
    // Actually we just pull out the important bits here & on the
    // next call to poll the stack the State will have changed to
    // alert the user
    if (rxMsg != 0)
    {
        SipUrl *from = rxMsg->getFromUrl();

        if (from)
        {
            CallersUserid = from->getUser();
            CallerUrl = from->getUser() + "@" + from->getHost();
            if (from->getPort() != 5060)
                CallerUrl += ":" + QString::number(from->getPort());
            CallersDisplayName = from->getDisplay();
        }
        else
            cerr << "What no from in INVITE?  It is invalid then.\n";
    }
    else
        cerr << "What no INVITE?  How did we get here then?\n";
}

void SipCall::GetSDPInfo(SipMsg *sipMsg)
{
    audioPayloadIdx = -1;
    videoPayload = -1;
    dtmfPayload = -1;
    remoteAudioPort = 0;
    remoteVideoPort = 0;
    rxVideoResolution = "AUDIOONLY";

    SipSdp *Sdp = sipMsg->getSdp();

    if (Sdp != 0)
    {
        remoteIp = Sdp->getMediaIP();
        remoteAudioPort = Sdp->getAudioPort();
        remoteVideoPort = Sdp->getVideoPort();

        // See if there is an audio codec we support
        QPtrList<sdpCodec> *audioCodecs = Sdp->getAudioCodecList();
        sdpCodec *c;
        if (audioCodecs)
        {
            for (c=audioCodecs->first(); c; c=audioCodecs->next())
            {
                for (int n=0; (n<MAX_AUDIO_CODECS) && (audioPayloadIdx == -1); n++)
                    if ((CodecList[n].Payload != -1) && (CodecList[n].Payload == c->intValue()))
                        audioPayloadIdx = n;
    
                // Note - no checking for dynamic payloads implemented yet --- need to match
                // by text if .Payload == -1
    
                // Also check for DTMF
                if (c->strValue() == "telephone-event/8000")
                    dtmfPayload = c->intValue();
            }
        }

        // See if there is a video codec we support
        QPtrList<sdpCodec> *videoCodecs = Sdp->getVideoCodecList();
        if (videoCodecs)
        {
            for (c=videoCodecs->first(); c; c=videoCodecs->next())
            {
                if ((c->intValue() == 34) && (c->strValue() == "H263/90000"))
                {
                    videoPayload = c->intValue();
                    rxVideoResolution = (c->fmtValue()).section('=', 0, 0);
                    break;
                }
            }
        }

        if (debugStream)
            *debugStream << "SDP contains IP " << remoteIp << " A-Port " << remoteAudioPort << " V-Port " << remoteVideoPort << " Audio Codec:" << audioPayloadIdx << " Video Codec:" << videoPayload << " Format:" << rxVideoResolution << " DTMF: " << dtmfPayload << endl;
    }
}



void SipCall::addSdpToInvite(SipMsg& msg, bool advertiseVideo)
{
    SipSdp sdp(sipLocalIP, sipAudioRtpPort, advertiseVideo ? sipVideoRtpPort : 0);

    for (int n=0; (n<MAX_AUDIO_CODECS) && (CodecList[n].Payload != -1); n++)
        sdp.addAudioCodec(CodecList[n].Payload, CodecList[n].Encoding + "/8000");

    // Signal support for DTMF
    sdp.addAudioCodec(101, "telephone-event/8000", "0-11");

    if (advertiseVideo)
        sdp.addVideoCodec(34, "H263/90000", txVideoResolution +"=2");
    sdp.encode();
    msg.addSDP(sdp);
}


void SipCall::BuildSdpResponse(SipMsg& msg)
{
    SipSdp sdp(sipLocalIP, sipAudioRtpPort, (videoPayload != -1) ? sipVideoRtpPort : 0);

    sdp.addAudioCodec(CodecList[audioPayloadIdx].Payload, CodecList[audioPayloadIdx].Encoding + "/8000");

    // Signal support for DTMF
    if (dtmfPayload != -1)
        sdp.addAudioCodec(dtmfPayload, "telephone-event/8000", "0-11");

    if (videoPayload != -1)
        sdp.addVideoCodec(34, "H263/90000", txVideoResolution +"=2");

    sdp.encode();
    msg.addSDP(sdp);
}



void SipCall::DebugFsm(int event, int old_state, int new_state)
{
    if (debugStream)
        *debugStream << "SIP FSM " << callRef << " Event " << EventtoString(event) << " : "
         << StatetoString(old_state) << " -> " << StatetoString(new_state) 
         << "; Sent: " << debugSent << endl;
}


QString SipCall::EventtoString(int Event)
{
    switch (Event)
    {
    case SIP_OUTCALL:             return "OUTCALL";
    case SIP_REGISTER:            return "REGISTER";
    case SIP_INVITE:              return "INVITE";
    case SIP_INVITESTATUS_3456xx: return "INVST-3456xx";
    case SIP_INVITESTATUS_2xx:    return "INVSTAT-2xx";
    case SIP_INVITESTATUS_1xx:    return "INVSTAT-1xx";
    case SIP_ANSWER:              return "ANSWER";
    case SIP_ACK:                 return "ACK";
    case SIP_BYE:                 return "BYE";
    case SIP_CANCEL:              return "CANCEL";
    case SIP_HANGUP:              return "HANGUP";
    case SIP_BYESTATUS:           return "BYESTATUS";
    case SIP_CANCELSTATUS:        return "CANCSTATUS";
    case SIP_RETX:                return "RETX";
    default:
        break;
    }
    return "Unknown-Event";
}


QString SipCall::StatetoString(int S)
{
    switch (S)
    {
    case SIP_IDLE:              return "IDLE";
    case SIP_OCONNECTING1:      return "OCONNECT1";
    case SIP_OCONNECTING2:      return "OCONNECT2";
    case SIP_ICONNECTING:       return "ICONNECT";
    case SIP_CONNECTED:         return "CONNECTED";
    case SIP_DISCONNECTING:     return "DISCONNECT ";
    case SIP_CONNECTED_VXML:    return "CONNECT-VXML";  // A false state! Only used to indicate to frontend 

    default:
        break;
    }
    return "Unknown-State";
}


/**********************************************************************
SipRegistrar

A simple registrar class used mainly for testing purposes. Allows
SIP UAs which need to register, like Microsoft Messenger, to be able
to handle calls.
**********************************************************************/

SipRegisteredUA::SipRegisteredUA(SipUrl *Url, QString cIp, int cPort)
{
    userUrl = new SipUrl(Url);
    contactIp = cIp;
    contactPort = cPort;
}

SipRegisteredUA::~SipRegisteredUA()
{
    if (userUrl != 0)
        delete userUrl;
}

bool SipRegisteredUA::matches(SipUrl *u)
{
    if ((u != 0) && (userUrl != 0))
    {
        if (u->getUser() == userUrl->getUser())
            return true;
    }
    return false;
}


SipRegistrar::SipRegistrar(SipFsm *par, QString domain, QString localIp, int localPort) : SipFsmBase()
{
    parent = par;
    sipLocalIp = localIp;
    sipLocalPort = localPort;
    regDomain = domain;
}

SipRegistrar::~SipRegistrar()
{
    SipRegisteredUA *it;
    while ((it=RegisteredList.first()) != 0)
    {
        RegisteredList.remove();
        delete it;
    }
    (parent->Timer())->StopAll(this);
}

int SipRegistrar::FSM(int Event, SipMsg *sipMsg, void *Value)
{
    switch(Event)
    {
    case SIP_REGISTER:
        {
            SipUrl *s = sipMsg->getContactUrl();
            SipUrl *to = sipMsg->getToUrl();
            if ((to->getHost() == regDomain) || (to->getHostIp() == sipLocalIp))
            {
                if (sipMsg->getExpires() != 0)
                    add(to, s->getHostIp(), s->getPort(), sipMsg->getExpires());
                else
                    remove(to);
                SendResponse(200, sipMsg, s->getHostIp(), s->getPort());
            }
            else
            {
                cout << "SIP Registration rejected for domain " << (sipMsg->getToUrl())->getHost() << endl;
                SendResponse(404, sipMsg, s->getHostIp(), s->getPort());
            }
        }
        break;
    case SIP_REGISTRAR_TEXP:
        if (Value != 0)
        {
            SipRegisteredUA *it = (SipRegisteredUA *)Value;
            RegisteredList.remove(it);
            cout << "SIP Registration Expired client " << it->getContactIp() << ":" << it->getContactPort() << endl;
            delete it;
        }
        break;
    }
    return 0;
}

void SipRegistrar::add(SipUrl *Url, QString hostIp, int Port, int Expires)
{
    // Check entry exists and refresh rather than add if it does
    SipRegisteredUA *it = find(Url);

    // Entry does not exist, new client, add an entry
    if (it == 0)
    {
        SipRegisteredUA *entry = new SipRegisteredUA(Url, hostIp, Port);
        RegisteredList.append(entry);
        //TODO - Start expiry timer
        (parent->Timer())->Start(this, Expires*1000, SIP_REGISTRAR_TEXP, RegisteredList.current());
        cout << "SIP Registered client " << Url->getUser() << " at " << hostIp << endl;
    }

    // Entry does exist, refresh the entry expiry timer
    else
    {
        // TODO - Restart expiry timer
        (parent->Timer())->Start(this, Expires*1000, SIP_REGISTRAR_TEXP, it);
        //cout << "SIP Re-Registered client " << Url->getUser() << " at " << hostIp << endl;
    }
}

void SipRegistrar::remove(SipUrl *Url)
{
    // Check entry exists and refresh rather than add if it does
    SipRegisteredUA *it = find(Url);

    if (it != 0)
    {
        RegisteredList.remove(it);
        (parent->Timer())->Stop(this, SIP_REGISTRAR_TEXP, it);
        cout << "SIP Unregistered client " << Url->getUser() << " at " << Url->getHostIp() << endl;
        delete it;
    }
    else
        cerr << "SIP Registrar could not find registered client " << Url->getUser() << endl;
}

bool SipRegistrar::getRegisteredContact(SipUrl *Url)
{
    // See if we can find the registered contact
    SipRegisteredUA *it = find(Url);

    if (it)
    {
        Url->setHostIp(it->getContactIp());
        Url->setPort(it->getContactPort());
        return true;
    }
    return false;
}

SipRegisteredUA *SipRegistrar::find(SipUrl *Url)
{
    // First check if the URL matches our domain, otherwise it can't be registered
    if ((Url->getHost() == regDomain) || (Url->getHostIp() == sipLocalIp))
    {
        // Now see if we can find the user himself
        SipRegisteredUA *it;
        for (it=RegisteredList.first(); it; it=RegisteredList.next())
        {
            if (it->matches(Url))
                return it;
        }
    }
    return 0;
}

void SipRegistrar::SendResponse(int Code, SipMsg *sipMsg, QString rIp, int rPort)
{
    SipMsg Status("REGISTER");
    Status.addStatusLine(Code);
    Status.addVia(sipLocalIp, sipLocalPort);
    Status.addFrom(*(sipMsg->getFromUrl()), sipMsg->getFromTag());
    Status.addTo(*(sipMsg->getFromUrl()));
    Status.addCallId(sipMsg->getCallId());
    Status.addCSeq(sipMsg->getCSeqValue());
    Status.addExpires(sipMsg->getExpires());
    Status.addContact(sipMsg->getContactUrl());
    Status.addNullContent();

    parent->Transmit(Status.string(), rIp, rPort);
}


/**********************************************************************
SipRegistration

This class is used to register with a SIP Proxy.
**********************************************************************/

SipRegistration::SipRegistration(SipFsm *par, QString localIp, int localPort, QString Username, QString Password, QString ProxyName, int ProxyPort) : SipFsmBase()
{
    parent = par;
    sipLocalIp = localIp;
    sipLocalPort = localPort;
    ProxyUrl = new SipUrl("", "", ProxyName, ProxyPort);
    MyUrl = new SipUrl("", Username, ProxyName, ProxyPort);
    MyContactUrl = new SipUrl("", Username, sipLocalIp, sipLocalPort);
    MyPassword = Password;
    cseq = 1;
    CallId.Generate(sipLocalIp);

    SendRegister();
    State = SIP_REG_TRYING;
    regRetryCount = REG_RETRY_MAXCOUNT;
    Expires = 3600;
    (parent->Timer())->Start(this, REG_RETRY_TIMER, SIP_RETX); 
}

SipRegistration::~SipRegistration()
{
    delete ProxyUrl;
    delete MyUrl;
    delete MyContactUrl;
    (parent->Timer())->StopAll(this);
}

int SipRegistration::FSM(int Event, SipMsg *sipMsg, void *Value)
{
    int Action = Event | State; // Just prevents messy nested switch constructs


    switch (Action)
    {
    case SIP_REG_TRYING_STATUS:
        (parent->Timer())->Stop(this, SIP_RETX);
        switch (sipMsg->getStatusCode())
        {
        case 200:
            if (sipMsg->getExpires() > 0)
                Expires = sipMsg->getExpires();
            cout << "SIP Registered to " << ProxyUrl->getHost() << " for " << Expires << "s" << endl;
            State = SIP_REG_REGISTERED;
            (parent->Timer())->Start(this, (Expires-30)*1000, SIP_REG_TREGEXP); // Assume 30secs max to reregister
            break;
        case 401:
            cseq++;
            SendRegister(sipMsg);
            regRetryCount = REG_RETRY_MAXCOUNT;
            State = SIP_REG_CHALLENGED;
            (parent->Timer())->Start(this, REG_RETRY_TIMER, SIP_RETX); 
            break;
        default:
            cout << "SIP Registration failed; Reason " << sipMsg->getStatusCode() << " " << sipMsg->getReasonPhrase() << endl;
            State = SIP_REG_FAILED;
            (parent->Timer())->Start(this, REG_FAIL_RETRY_TIMER, SIP_RETX); // Try again in 3 minutes
            break;
        }
        break;

    case SIP_REG_CHALL_STATUS:
        (parent->Timer())->Stop(this, SIP_RETX);
        switch (sipMsg->getStatusCode())
        {
        case 200:
            if (sipMsg->getExpires() > 0)
                Expires = sipMsg->getExpires();
            cout << "SIP Registered to " << ProxyUrl->getHost() << " for " << Expires << "s" << endl;
            State = SIP_REG_REGISTERED;
            (parent->Timer())->Start(this, (Expires-30)*1000, SIP_REG_TREGEXP); // Assume 30secs max to reregister
            break;
        default:
            cout << "SIP Registration failed; Reason " << sipMsg->getStatusCode() << " " << sipMsg->getReasonPhrase() << endl;
            State = SIP_REG_FAILED;
            (parent->Timer())->Start(this, REG_FAIL_RETRY_TIMER, SIP_RETX); // Try again in 3 minutes
            break;
        }
        break;

    case SIP_REG_REGISTERED_TREGEXP:
        regRetryCount = REG_RETRY_MAXCOUNT+1;
    case SIP_REG_TRYING_RETX:
    case SIP_REG_CHALL_RETX:
    case SIP_REG_FAILED_RETX:
        if (--regRetryCount > 0)
        {
            cseq++; // TODO --- Check if this is correct?
            State = SIP_REG_TRYING;
            SendRegister();
            (parent->Timer())->Start(this, REG_RETRY_TIMER, SIP_RETX); // Retry every 10 seconds
        }
        else
        {
            State = SIP_REG_FAILED;
            cout << "SIP Registration failed; No Respone from Server\n";
        }
        break;

    default:
        cerr << "SIP Registration: Unknown Event/State\n";
        break;
    }
    return 0;
}

void SipRegistration::SendRegister(SipMsg *authMsg)
{
    SipMsg Register("REGISTER");
    Register.addRequestLine(*ProxyUrl);
    Register.addVia(sipLocalIp, sipLocalPort);
    Register.addFrom(*MyUrl);
    Register.addTo(*MyUrl);
    Register.addCallId(CallId);
    Register.addCSeq(cseq);

    if (authMsg && (authMsg->getAuthMethod() == "Digest"))
    {
        Register.addAuthorization(authMsg->getAuthMethod(), MyUrl->getUser(), MyPassword, authMsg->getAuthRealm(), authMsg->getAuthNonce(), "sip:" + ProxyUrl->getHost());
    }

    Register.addUserAgent();
    Register.addExpires(Expires=3600);
    Register.addContact(*MyContactUrl);
    Register.addNullContent();

    parent->Transmit(Register.string(), ProxyUrl->getHostIp(), ProxyUrl->getPort());
}



/**********************************************************************
SipNotify

This class notifies the Myth Frontend that there is an incoming call
by building and sending an XML formatted UDP packet to port 6948; where
a listener will create an OSD message.
**********************************************************************/

SipNotify::SipNotify()
{
    notifySocket = new QSocketDevice (QSocketDevice::Datagram);
    notifySocket->setBlocking(false);
    QHostAddress thisIP;
    thisIP.setAddress("127.0.0.1");
    if (!notifySocket->bind(thisIP, 6951))
    {
        cerr << "Failed to bind for CLI NOTIFY connection\n";
        delete notifySocket;
        notifySocket = 0;
    }
//    notifySocket->close();
}

SipNotify::~SipNotify()
{
    if (notifySocket)
    {
        delete notifySocket;
        notifySocket = 0;
    }

}

void SipNotify::Display(QString name, QString number)
{
    if (notifySocket)
    {
        QString text;
        text =  "<mythnotify version=\"1\">"
                "  <container name=\"notify_cid_info\">"
                "    <textarea name=\"notify_cid_name\">"
                "      <value>NAME : ";
        text += name;
        text += "      </value>"
                "    </textarea>"
                "    <textarea name=\"notify_cid_num\">"
                "      <value>NUM : ";
        text += number;
        text += "      </value>"
                "    </textarea>"
                "  </container>"
                "</mythnotify>";

        QHostAddress RemoteIP;
        RemoteIP.setAddress("127.0.0.1");
        notifySocket->writeBlock(text.ascii(), text.length(), RemoteIP, 6948);
    }
}


/**********************************************************************
SipTimer

This class handles timers for retransmission and other call-specific
events.  Would be better implemented as a QT timer but is not because
of  thread problems.
**********************************************************************/

SipTimer::SipTimer():QPtrList<aSipTimer>()
{
}

SipTimer::~SipTimer()
{
    aSipTimer *p;
    while ((p = first()) != 0)
    {
        remove();
        delete p;   // auto-delete is disabled
    }
}

void SipTimer::Start(SipFsmBase *Instance, int ms, int expireEvent, void *Value)
{
    Stop(Instance, expireEvent, Value);
    QDateTime expire = (QDateTime::currentDateTime()).addSecs(ms/1000); // Note; we lose accuracy here; but no "addMSecs" fn exists
    aSipTimer *t = new aSipTimer(Instance, expire, expireEvent, Value);
    inSort(t);
}

int SipTimer::compareItems(QPtrCollection::Item s1, QPtrCollection::Item s2)
{
    QDateTime t1 = ((aSipTimer *)s1)->getExpire();
    QDateTime t2 = ((aSipTimer *)s2)->getExpire();

    return (t1==t2 ? 0 : (t1>t2 ? 1 : -1));
}

void SipTimer::Stop(SipFsmBase *Instance, int expireEvent, void *Value)
{
    aSipTimer *it;
    for (it=first(); it; it=next())
    {
        if (it->match(Instance, expireEvent, Value))
        {
            remove();
            delete it;
        }
    }
}

void SipTimer::StopAll(SipFsmBase *Instance)
{
    Stop(Instance, -1);
}

SipFsmBase *SipTimer::Expired(int *Event, void **Value)
{
    aSipTimer *it = first();
    if ((it) && (it->Expired()))
    {
        SipFsmBase *c = it->getInstance();
        *Event = it->getEvent();
        *Value = it->getValue();
        remove();
        delete it;
        return c;
    }
    *Event = 0;
    return 0;
}





