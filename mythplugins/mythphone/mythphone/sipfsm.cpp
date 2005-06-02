/*
	sipfsm.cpp

	(c) 2003 Paul Volkaerts
	
*/
#include <qapplication.h>
#include <qfile.h>
#include <qsocket.h>
#include <qsocketdevice.h>
#include <qdatetime.h>
#include <qurl.h>

#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifndef WIN32
#include <sys/ioctl.h>
#include <unistd.h>
#include <pthread.h>
#include <netdb.h>
#include <netinet/in.h>
#include <net/if.h>
#include <linux/sockios.h>
#include <linux/videodev.h>
#include <mythtv/mythcontext.h>
#include "config.h"
#endif

#ifdef WIN32
#include <winsock2.h>
#include "gcontext.h"
#endif

using namespace std;

#include "sipfsm.h"


// Static variables for the debug file used
QFile *debugFile;
QTextStream *debugStream;
QObject *eventWindow;
QStringList EventQ;
QStringList NotifyQ;
QMutex EventQLock;
QString localIp;
QString natIp;



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
    CallState = -1;
    eventWindow = 0;

	sipThread = new SipThread(this);
	sipThread->start();
}

SipContainer::~SipContainer()
{
    killSipThread = true;
	sipThread->wait();
	delete sipThread;
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

void SipContainer::ModifyCall(QString audCodec, QString vidCodec)
{
    EventQLock.lock();
    EventQ.append("MODIFYCALL");
    EventQ.append(audCodec);
    EventQ.append(vidCodec);
    EventQLock.unlock();
}

void SipContainer::UiOpened(QObject *callingApp)
{
    EventQLock.lock();
    eventWindow = callingApp;
    EventQ.append("UIOPENED");
    EventQLock.unlock();
}

void SipContainer::UiClosed()
{
    EventQLock.lock();
    eventWindow = 0;
    EventQ.append("UICLOSED");
    EventQLock.unlock();
}

void SipContainer::UiWatch(QStrList uriList)
{
    QStrListIterator it(uriList);

    EventQLock.lock();
    EventQ.append("UIWATCH");
    for (; it.current(); ++it)
        EventQ.append(it.current());
    EventQ.append("");
    EventQLock.unlock();
}

void SipContainer::UiWatch(QString uri)
{
    EventQLock.lock();
    EventQ.append("UIWATCH");
    EventQ.append(uri);
    EventQ.append("");
    EventQLock.unlock();
}

void SipContainer::UiStopWatchAll()
{
    EventQLock.lock();
    EventQ.append("UISTOPWATCHALL");
    EventQLock.unlock();
}

QString SipContainer::UiSendIMMessage(QString DestUrl, QString CallId, QString Msg)
{
    SipCallId sipCallId;
    
    if (CallId.length() == 0)
    {
        sipCallId.Generate(localIp);
        CallId = sipCallId.string();
    }

    EventQLock.lock();
    EventQ.append("SENDIM");
    EventQ.append(DestUrl);
    EventQ.append(CallId);
    EventQ.append(Msg);
    EventQLock.unlock();
    return CallId;
}


int SipContainer::GetSipState()
{
    int tempState;
    EventQLock.lock();
    tempState = CallState;
    EventQLock.unlock();
    return tempState;
}

bool SipContainer::GetNotification(QString &type, QString &url, QString &param1, QString &param2)
{
    bool notifyFlag = false;
    EventQLock.lock();

    if (!NotifyQ.empty())
    {
        QStringList::Iterator it;
        notifyFlag = true;
        it = NotifyQ.begin();
        type = *it;
        it = NotifyQ.remove(it);
        url = *it;
        it = NotifyQ.remove(it);
        param1 = *it;
        it = NotifyQ.remove(it);
        param2 = *it;
        NotifyQ.remove(it);
    }

    EventQLock.unlock();
    return notifyFlag;
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

QString SipContainer::getLocalIpAddress()
{
    return localIp;
}

QString SipContainer::getNatIpAddress()
{
    return natIp;
}


/**********************************************************************
SipThread

The main SIP thread that polls for events and handles communication
with the user via the SipContainer class
**********************************************************************/

void SipThread::run()
{
    SipThreadWorker();
}

void SipThread::SipThreadWorker()
{
    FrontEndActive = false;
    rnaTimer = -1;
    vxmlCallActive = false;
#ifndef WIN32
    vxml = new vxmlParser();
    Rtp = 0;
#endif

    // Open a file for writing debug info into
#ifndef WIN32
    QString debugFileName = MythContext::GetConfDir() + "/MythPhone/siplog.txt";
    debugFile = new QFile(debugFileName);
    if (debugFile->open(IO_WriteOnly))
        debugStream = new QTextStream (debugFile);
#else
    debugStream = 0;
#endif

    SipFsm *sipFsm = new SipFsm();

    if (sipFsm->SocketOpenedOk())
    {
        while(!sipContainer->killThread())
        {
            int OldCallState = CallState;

            // This blocks for timeout or data in Linux
            CheckNetworkEvents(sipFsm);
            CheckUIEvents(sipFsm);
            CheckRegistrationStatus(sipFsm); // Probably don't need to do this every 1/2 sec but this is a fallout of a non event-driven arch.
            sipFsm->HandleTimerExpiries();
            ChangePrimaryCallState(sipFsm, sipFsm->getPrimaryCallState());

        // A Ring No Answer timer runs to send calls to voicemail after x seconds
#ifndef WIN32
            if ((CallState == SIP_ICONNECTING) && (rnaTimer != -1))
            {
                if (--rnaTimer < 0)
                {
                    rnaTimer = -1;
                    vxmlCallActive = true;
                    sipFsm->Answer(true, "", false);
                }
            }
#endif

            ChangePrimaryCallState(sipFsm, sipFsm->getPrimaryCallState());

            EventQLock.lock();
            if ((OldCallState != CallState) && (eventWindow))
                QApplication::postEvent(eventWindow, new SipEvent(SipEvent::SipStateChange));
            EventQLock.unlock();
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
#ifndef WIN32
    delete vxml;
#endif
}

void SipThread::CheckUIEvents(SipFsm *sipFsm)
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
    else if (event == "MODIFYCALL")
    {
        EventQLock.lock();
        it = EventQ.begin();
        QString audioCodec = *it;
        it = EventQ.remove(it);
        QString videoCodec = *it;
        EventQ.remove(it);
        EventQLock.unlock();
        sipFsm->ModifyCall(audioCodec, videoCodec);
    }
    else if (event == "UIOPENED")
    {
        sipFsm->StatusChanged("OPEN");
        FrontEndActive = true;
    }
    else if (event == "UICLOSED")
    {
        sipFsm->StatusChanged("CLOSED");
        FrontEndActive = false;
    }
    else if (event == "UIWATCH")
    {
        QString uri;
        do
        {
            EventQLock.lock();
            it = EventQ.begin();
            uri = *it;
            EventQ.remove(it);
            EventQLock.unlock();
            if (uri.length() > 0)
                sipFsm->CreateWatcherFsm(uri);
        }
        while (uri.length() > 0);
    }
    else if (event == "UISTOPWATCHALL")
        sipFsm->StopWatchers();
    else if (event == "SENDIM")
    {
        EventQLock.lock();
        it = EventQ.begin();
        QString DestUrl = *it;
        it = EventQ.remove(it);
        QString CallId = *it;
        it = EventQ.remove(it);
        QString imMsg = *it;
        EventQ.remove(it);
        EventQLock.unlock();
        sipFsm->SendIM(DestUrl, CallId, imMsg);
    }

    ChangePrimaryCallState(sipFsm, sipFsm->getPrimaryCallState());
}

void SipThread::CheckRegistrationStatus(SipFsm *sipFsm)
{
	sipContainer->notifyRegistrationStatus(sipFsm->isRegistered(), sipFsm->registeredTo(), 
										   sipFsm->registeredAs());
}

void SipThread::CheckNetworkEvents(SipFsm *sipFsm)
{
    // Check for incoming SIP messages
    sipFsm->CheckRxEvent();

    // We only handle state changes in the "primary" call; we ignore additional calls which are
    // currently just rejected with busy
    ChangePrimaryCallState(sipFsm, sipFsm->getPrimaryCallState());
}


void SipThread::ChangePrimaryCallState(SipFsm *sipFsm, int NewState)
{
    int OldState = CallState;
    CallState = NewState;
    if ((CallState == SIP_CONNECTED) && (vxmlCallActive))
        sipContainer->notifyCallState(SIP_CONNECTED_VXML);
    else
        sipContainer->notifyCallState(CallState);

    if (OldState != CallState)
    {
        if (CallState == SIP_IDLE)
        {
            callerUser = "";
            callerName = "";
            callerUrl = "";
            inAudioOnly = true;
            sipContainer->notifyCallerDetails(callerUser, callerName, callerUrl, inAudioOnly);
            remoteIp = "0.0.0.0";
            remoteAudioPort = -1;
            remoteVideoPort = -1;
            audioPayload = -1;
            dtmfPayload = -1;
            videoPayload = -1;
            audioCodec = "";
            videoCodec = "";
            videoRes = "";
            sipContainer->notifySDPDetails(remoteIp, remoteAudioPort, audioPayload, audioCodec, dtmfPayload, remoteVideoPort, videoPayload, videoCodec, videoRes);
        }

        if (CallState == SIP_ICONNECTING)
        {
             // new incoming call; get the caller info
            EventQLock.lock();
            SipCall *call = sipFsm->MatchCall(sipFsm->getPrimaryCall());
            if (call != 0)
            {
                call->GetIncomingCaller(callerUser, callerName, callerUrl, inAudioOnly);
                sipContainer->notifyCallerDetails(callerUser, callerName, callerUrl, inAudioOnly);
            }
            EventQLock.unlock();

            rnaTimer = atoi((const char *)gContext->GetSetting("TimeToAnswer")) * SIP_POLL_PERIOD;
            if (rnaTimer == 0) // Never auto-answer
                rnaTimer = -1;
        }
        else
            rnaTimer = -1;

          
        if (CallState == SIP_CONNECTED)
        {
            // connected call; get the SDP info
            EventQLock.lock();
            SipCall *call = sipFsm->MatchCall(sipFsm->getPrimaryCall());
            if (call != 0)
            {
                call->GetSdpDetails(remoteIp, remoteAudioPort, audioPayload, audioCodec, dtmfPayload, remoteVideoPort, videoPayload, videoCodec, videoRes);
                sipContainer->notifySDPDetails(remoteIp, remoteAudioPort, audioPayload, audioCodec, dtmfPayload, remoteVideoPort, videoPayload, videoCodec, videoRes);

            }
            EventQLock.unlock();

#ifndef WIN32
            if (vxmlCallActive)
            {
                int lPort = atoi((const char *)gContext->GetSetting("AudioLocalPort"));
                int playout = atoi((const char *)gContext->GetSetting("PlayoutAudioCall"));
                QString spk = gContext->GetSetting("AudioOutputDevice");
                Rtp = new rtp(0, lPort, remoteIp, remoteAudioPort, audioPayload, playout, dtmfPayload, "None", spk, RTP_TX_AUDIO_SILENCE, RTP_RX_AUDIO_DISCARD);
                vxml->beginVxmlSession(Rtp, callerName.length() != 0 ? callerName : callerUser);
            }
#endif
        }
          
#ifndef WIN32
        if ((CallState == SIP_ICONNECTING) && (FrontEndActive == false))
        {
            // No application running to tell of the incoming call
            // Either alert via on-screen popup or send to voicemail
            SipNotify *notify = new SipNotify();
            notify->Display(callerName, callerUrl);
            delete notify;
        }
#endif

#ifndef WIN32
        // A call answered by VXML has been disconnected
        if ((OldState == SIP_CONNECTED) && vxmlCallActive)
        {
            vxmlCallActive = false;
            vxml->endVxmlSession();
            if (Rtp != 0)
                delete Rtp;
            Rtp = 0;
        }
#endif
    }
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
    PresenceStatus = "CLOSED";

    sipSocket = 0;
    localPort = atoi((const char *)gContext->GetSetting("SipLocalPort"));
    if (localPort == 0)
        localPort = 5060;
    localIp = OpenSocket(localPort);
    natIp = DetermineNatAddress();
    if (natIp.length() == 0)
        natIp = localIp;
    SipFsm::Debug(SipDebugEvent::SipDebugEv, QString("SIP listening on IP Address ") + localIp + ":" + QString::number(localPort) + " NAT address " + natIp + "\n\n");
    cout << "SIP listening on IP Address " << localIp << ":" << localPort << " NAT address " << natIp << endl;

    // Create the timer list
    timerList = new SipTimer;

    // Create the Registrar
    sipRegistrar = new SipRegistrar(this, "volkaerts", localIp, localPort);

    // if Proxy Registration is configured ...
    bool RegisterWithProxy = gContext->GetNumSetting("SipRegisterWithProxy",1);
    sipRegistration = 0;
    if (RegisterWithProxy)
    {
        QString ProxyDNS = gContext->GetSetting("SipProxyName");
        QString ProxyUsername = gContext->GetSetting("SipProxyAuthName");
        QString ProxyPassword = gContext->GetSetting("SipProxyAuthPassword");
        if ((ProxyDNS.length() > 0) && (ProxyUsername.length() > 0) && (ProxyPassword.length() > 0))
        {
            sipRegistration = new SipRegistration(this, natIp, localPort, ProxyUsername, ProxyPassword, ProxyDNS, 5060);
            FsmList.append(sipRegistration);
        }
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

void SipFsm::Debug(SipDebugEvent::Type t, QString dbg)
{
#ifdef WIN32
    if (eventWindow)
        QApplication::postEvent(eventWindow, new SipDebugEvent(t, dbg));
#else
    if ((debugStream) && ((t == SipDebugEvent::SipTraceRxEv) || (t == SipDebugEvent::SipTraceTxEv) || (t == SipDebugEvent::SipDebugEv)))
        *debugStream << dbg;
#endif
}

QString SipFsm::OpenSocket(int Port)
{
    sipSocket = new QSocketDevice (QSocketDevice::Datagram);
    sipSocket->setBlocking(false);

#ifndef WIN32
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
#endif

#ifdef WIN32  // SIOCGIFADDR not supported on Windows
    char         hostname[100];
    HOSTENT FAR *hostAddr;
    int ifNum = atoi(gContext->GetSetting("SipBindInterface"));
    if ((gethostname(hostname, 100) != 0) || ((hostAddr = gethostbyname(hostname)) == NULL)) 
    {
        cerr << "Failed to find network interface " << endl;
        delete sipSocket;
        sipSocket = 0;
        return "";
    }
    QHostAddress myIP; 
    for (int ifTotal=0; hostAddr->h_addr_list[ifTotal] != 0; ifTotal++)
        ;
    if (ifNum >= ifTotal)
        ifNum = 0;
    myIP.setAddress(htonl(((struct in_addr *)hostAddr->h_addr_list[ifNum])->S_un.S_addr));
#endif

    if (!sipSocket->bind(myIP, Port))
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
            if (h)
                hostIp.setAddress(ntohl(*(long *)h->h_addr));
            else
            {
                cout << "SIP: Failed to detect your NAT settings\n";
                return "";
            }
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
                        cout << "SIP: Got invalid HTML response whilst detecting your NAT settings " << endl;
                    delete httpResponse;
                    break;
                }
            }
            else
                cerr << "Error sending NAT discovery packet to socket\n";
        }
        else
            cout << "SIP: Could not connect to NAT discovery host " << Url.host() << ":" << Url.port() << endl;
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
        SipFsm::Debug(SipDebugEvent::SipTraceTxEv, QDateTime::currentDateTime().toString() + " Sent to " + destIP + ":" + QString::number(destPort) + "...\n" + Msg + "\n");
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
            SipFsm::Debug(SipDebugEvent::SipTraceRxEv, QDateTime::currentDateTime().toString() + " Received: Len " + QString::number(len) + "\n" + rxMsg + "\n");
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
        Call = new SipCall(localIp, natIp, localPort, cr, this);
        FsmList.append(Call);

        // If the dialled number if just a username and we are registered to a proxy, dial
        // via the proxy
        if ((!uri.contains('@')) && (sipRegistration != 0) && (sipRegistration->isRegistered()))
            uri.append(QString("@") + gContext->GetSetting("SipProxyName"));

        Call->dialViaProxy(sipRegistration);
        Call->to(uri, DispName);
        Call->setDisableNat(DisableNat);
        Call->setAllowVideo(audioOnly ? false : true);
        Call->setVideoResolution(videoMode);
        if (Call->FSM(SIP_OUTCALL) == SIP_IDLE)
            DestroyFsm(Call);
    }
    else
        cerr << "SIP Call attempt with call in progress\n";
}


void SipFsm::HangUp()
{
    SipCall *Call = MatchCall(primaryCall);
    if (Call)
        if (Call->FSM(SIP_HANGUP) == SIP_IDLE)
            DestroyFsm(Call);
}


void SipFsm::ModifyCall(QString audioCodec, QString videoCodec)
{
    SipCall *Call = MatchCall(primaryCall);
    if (Call)
    {
        if ((Call->ModifyCodecs(audioCodec, videoCodec)) && 
            (Call->FSM(SIP_MODIFYSESSION) == SIP_IDLE))
            DestroyFsm(Call);
    }
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
            DestroyFsm(Call);
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
            DestroyFsm(Instance);
    }
}


void SipFsm::DestroyFsm(SipFsmBase *Fsm)
{
    if (Fsm != 0)
    {
        timerList->StopAll(Fsm);
        if (Fsm->type() == "CALL")
        {
            if (Fsm->getCallRef() == primaryCall)
                primaryCall = -1;
        }
        FsmList.remove(Fsm);
        delete Fsm;
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


void SipFsm::CheckRxEvent()
{
    SipMsg sipRcv;
    if ((sipSocket->waitForMore(1000/SIP_POLL_PERIOD) > 0) && (Receive(sipRcv)))
    {
        int Event = MsgToEvent(&sipRcv);

        // Try and match an FSM based on an existing CallID; if no match then 
        // we ahve to create a new FSM based on the event
        SipFsmBase *fsm = MatchCallId(sipRcv.getCallId());
        if (fsm == 0)
        {
            switch (Event)
            {
            case SIP_UNKNOWN:     fsm = 0;                       break;//ignore event
            case SIP_REGISTER:    fsm = sipRegistrar;            break;
            case SIP_SUBSCRIBE:   fsm = CreateSubscriberFsm();   break;
            case SIP_MESSAGE:     fsm = CreateIMFsm();           break;
            default:              fsm = CreateCallFsm();         break;
            }
        }

        // Now push the event through the FSM and see if the event causes the FSM to self-destruct
        if (fsm)
        {
            if ((fsm->FSM(Event, &sipRcv)) == SIP_IDLE)
                DestroyFsm(fsm);
        }
        else if (Event != SIP_UNKNOWN)
            cerr << "SIP: fsm should not be zero here\n";
    }
}


void SipFsm::SetNotification(QString type, QString uri, QString param1, QString param2)
{
    EventQLock.lock();
    if (eventWindow) // Is there someone listening?
    {
        NotifyQ.append(type);
        NotifyQ.append(uri);
        NotifyQ.append(param1);
        NotifyQ.append(param2);

        QApplication::postEvent(eventWindow, new SipEvent(SipEvent::SipNotification));
    }
    EventQLock.unlock();
}


int SipFsm::MsgToEvent(SipMsg *sipMsg)
{
    QString Method = sipMsg->getMethod();
    if (Method == "INVITE")     return SIP_INVITE;
    if (Method == "ACK")        return SIP_ACK;
    if (Method == "BYE")        return SIP_BYE;
    if (Method == "CANCEL")     return SIP_CANCEL;
    if (Method == "INVITE")     return SIP_INVITE;
    if (Method == "REGISTER")   return SIP_REGISTER;
    if (Method == "SUBSCRIBE")  return SIP_SUBSCRIBE;
    if (Method == "NOTIFY")     return SIP_NOTIFY;
    if (Method == "MESSAGE")    return SIP_MESSAGE;
    if (Method == "INFO")       return SIP_INFO;

    if (Method == "STATUS")
    {
        QString statusMethod = sipMsg->getCSeqMethod();
        if (statusMethod == "REGISTER")    return SIP_REGSTATUS;
        if (statusMethod == "SUBSCRIBE")   return SIP_SUBSTATUS;
        if (statusMethod == "NOTIFY")      return SIP_NOTSTATUS;
        if (statusMethod == "BYE")         return SIP_BYESTATUS;
        if (statusMethod == "CANCEL")      return SIP_CANCELSTATUS;
        if (statusMethod == "MESSAGE")     return SIP_MESSAGESTATUS;
        if (statusMethod == "INFO")        return SIP_INFOSTATUS;

        if (statusMethod == "INVITE")
        {
            int statusCode = sipMsg->getStatusCode();
            if ((statusCode >= 200) && (statusCode < 300))    return SIP_INVITESTATUS_2xx;
            if ((statusCode >= 100) && (statusCode < 200))    return SIP_INVITESTATUS_1xx;
            if ((statusCode >= 300) && (statusCode < 700))    return SIP_INVITESTATUS_3456xx;
        }
        cerr << "SIP: Unknown STATUS method " << statusMethod << endl;
    }
    else
        cerr << "SIP: Unknown method " << Method << endl << sipMsg->string() << endl;
    return SIP_UNKNOWN;
}

SipCall *SipFsm::MatchCall(int cr)
{
    SipFsmBase *it;
    for (it=FsmList.first(); it; it=FsmList.next())
        if ((it->type() == "CALL") && (it->getCallRef() == cr))
            return (dynamic_cast<SipCall *>(it));
    return 0;
}

SipFsmBase *SipFsm::MatchCallId(SipCallId *CallId)
{
    SipFsmBase *it;
    SipFsmBase *match=0;
    
    if (CallId != 0)
    {
        for (it=FsmList.first(); it; it=FsmList.next())
        {
            if (it->callId() == CallId->string())
            {
                if (match != 0)
                    cerr << "SIP: Oops; we have two FSMs with the same Call Id\n";
                match = it;
            }
        }
    }
    return match;
}

SipCall *SipFsm::CreateCallFsm()
{
    int cr = callCount++;
    SipCall *it = new SipCall(localIp, natIp, localPort, cr, this);
    if (primaryCall == -1)
        primaryCall = cr;
    FsmList.append(it);
    
    it->dialViaProxy(sipRegistration);
   
    return it;
}

SipSubscriber *SipFsm::CreateSubscriberFsm()
{
    SipSubscriber *sub = new SipSubscriber(this, natIp, localPort, sipRegistration, PresenceStatus);
    FsmList.append(sub);
    return sub;
}

SipWatcher *SipFsm::CreateWatcherFsm(QString Url)
{
    SipWatcher *watcher = new SipWatcher(this, natIp, localPort, sipRegistration, Url);
    FsmList.append(watcher);
    return watcher;
}

SipIM *SipFsm::CreateIMFsm(QString Url, QString callIdStr)
{
    SipIM *im = new SipIM(this, natIp, localPort, sipRegistration, Url, callIdStr);
    FsmList.append(im);
    return im;
}

void SipFsm::StopWatchers()
{
    SipFsmBase *it=FsmList.first();
    while (it)
    {
        // Because we may delete the instance we need to step onwards before we destroy it
        SipFsmBase *thisone=it;
        it=FsmList.next();
        if ((thisone->type() == "WATCHER") && (thisone->FSM(SIP_STOPWATCH) == SIP_IDLE))
            DestroyFsm(thisone);
    }
}

void SipFsm::KickWatcher(SipUrl *Url)
{
    SipFsmBase *it=FsmList.first();
    while (it)
    {
        // Because we may delete the instance we need to step onwards before we destroy it
        SipFsmBase *thisone=it;
        it=FsmList.next();
        if ((thisone->type() == "WATCHER") && 
            (*thisone->getUrl() == *Url) && 
            (thisone->FSM(SIP_KICKWATCH) == SIP_IDLE))
            DestroyFsm(thisone);
    }
}

void SipFsm::SendIM(QString destUrl, QString CallId, QString imMsg)
{
    SipCallId sipCallId;
    sipCallId.setValue(CallId);
    SipFsmBase *Fsm = MatchCallId(&sipCallId);
    if ((Fsm) && (Fsm->type() == "IM"))
    {
        if ((Fsm->FSM(SIP_USER_MESSAGE, 0, &imMsg)) == SIP_IDLE)
            DestroyFsm(Fsm);
    }
    
    // Did not match a call-id, create new FSM entry
    else if (Fsm == 0)
    {
        SipIM *imFsm = CreateIMFsm(destUrl, CallId);
        if (imFsm)
        {
            if ((imFsm->FSM(SIP_USER_MESSAGE, 0, &imMsg)) == SIP_IDLE)
                DestroyFsm(imFsm);
        }
    }
    
    // Matched a call-id, but it was not an IM FSM, should not happen with random call-ids
    else 
    {
        cerr << "SIP: call-id used by non-IM FSM\n";
    }
}

int SipFsm::numCalls()
{
    SipFsmBase *it;
    int cnt=0;
    for (it=FsmList.first(); it; it=FsmList.next())
        if (it->type() == "CALL")
            cnt++;
    return cnt;
}

void SipFsm::StatusChanged(char *newStatus)
{
    PresenceStatus = newStatus;
    SipFsmBase *it;
    for (it=FsmList.first(); it; it=FsmList.next())
        if (it->type() == "SUBSCRIBER")
            it->FSM(SIP_PRESENCE_CHANGE, 0, newStatus);
}




/**********************************************************************
SipFsmBase

A base class for FSM which defines a set of default procedures that are
used by the derived classes.
**********************************************************************/

SipFsmBase::SipFsmBase(SipFsm *p) 
{ 
    parent = p;
    remoteUrl = 0;
    toUrl = 0;
    contactUrl = 0;
    recRouteUrl = 0;
    myTag = "abcdef";
    remoteTag = "";
    remoteEpid = "";
    rxedTo = "";
    rxedFrom = "";
    MyUrl = 0;
    MyContactUrl = 0;
    rxedTimestamp = -1;
    sentAuthenticated = false;
}

SipFsmBase::~SipFsmBase()
{
    if (remoteUrl != 0)
        delete remoteUrl;
    if (toUrl != 0)
        delete toUrl;
    if (contactUrl != 0)
        delete contactUrl;
    if (recRouteUrl != 0)
        delete recRouteUrl;
    if (MyUrl != 0)
        delete MyUrl;
    if (MyContactUrl != 0)
        delete MyContactUrl;

    remoteUrl = 0;
    toUrl = 0;
    contactUrl = 0;
    recRouteUrl = 0;
    MyUrl = 0;
    MyContactUrl = 0;
}

bool SipFsmBase::Retransmit(bool force)
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

void SipFsmBase::ParseSipMsg(int Event, SipMsg *sipMsg)
{
    // Pull out Remote TAG
    remoteTag = (SIP_CMD(Event)) ? sipMsg->getFromTag() : sipMsg->getToTag();
    remoteEpid = (SIP_CMD(Event)) ? sipMsg->getFromEpid() : QString("");

    // Pull out VIA, To and From information from CMDs to send back in Status
    if (SIP_CMD(Event))
    {
        rxedTo   = sipMsg->getCompleteTo();
        rxedFrom = sipMsg->getCompleteFrom();
        RecRoute = sipMsg->getCompleteRR();
        Via      = sipMsg->getCompleteVia();
        CallId   = *(sipMsg->getCallId());
        viaIp    = sipMsg->getViaIp();
        viaPort  = sipMsg->getViaPort();
        rxedTimestamp = sipMsg->getTimestamp();
        if (remoteUrl == 0)
            remoteUrl = new SipUrl(sipMsg->getFromUrl());
        if (toUrl == 0)
            toUrl = new SipUrl(sipMsg->getToUrl());
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
}

void SipFsmBase::BuildSendStatus(int Code, QString Method, int statusCseq, int Option, int statusExpires, QString sdp)
{
    if (remoteUrl == 0)
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
    Status.addToCopy(rxedTo, myTag);
    Status.addCallId(CallId);
    Status.addCSeq(statusCseq);
    Status.addUserAgent();
    
    if ((Option & SIP_OPT_EXPIRES) && (statusExpires >= 0))
        Status.addExpires(statusExpires);
    if (Option & SIP_OPT_TIMESTAMP) // Add Timestamp if there was one in the INVITE
        Status.addTimestamp(rxedTimestamp);
    if (Option & SIP_OPT_ALLOW) // Add my Contact URL to the message
        Status.addAllow();
    if (Option & SIP_OPT_CONTACT) // Add my Contact URL to the message
        Status.addContact(*MyContactUrl);
    if (Option & SIP_OPT_SDP) // Add an SDP to the message
        Status.addContent("application/sdp", sdp);
    else
        Status.addNullContent();

    // Send STATUS messages to the VIA address
    parent->Transmit(Status.string(), retxIp = viaIp, retxPort = viaPort);

    if (((Code >= 200) && (Code <= 299)) && (Method == "INVITE"))
    {
        retx = Status.string();
        t1 = 500;
        (parent->Timer())->Start(this, t1, SIP_RETX);
    }
}


void SipFsmBase::DebugFsm(int event, int old_state, int new_state)
{
    SipFsm::Debug(SipDebugEvent::SipDebugEv, "SIP FSM: Event " + EventtoString(event) + " : "
         + StatetoString(old_state) + " -> " + StatetoString(new_state) + "\n");
}


QString SipFsmBase::EventtoString(int Event)
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
    case SIP_REGISTRAR_TEXP:      return "REGITRAR_T";
    case SIP_REGSTATUS:           return "REG_STATUS";
    case SIP_REG_TREGEXP:         return "REG_TEXP";
    case SIP_SUBSCRIBE:           return "SUBSCRIBE";
    case SIP_SUBSTATUS:           return "SUB_STATUS";
    case SIP_NOTIFY:              return "NOTIFY";
    case SIP_NOTSTATUS:           return "NOT_STATUS";
    case SIP_PRESENCE_CHANGE:     return "PRESENCE_CHNG";
    case SIP_SUBSCRIBE_EXPIRE:    return "SUB_EXPIRE";
    case SIP_WATCH:               return "WATCH";
    case SIP_STOPWATCH:           return "STOPWATCH";
    case SIP_MESSAGE:             return "MESSAGE";
    case SIP_MESSAGESTATUS:       return "MESSAGESTATUS";
    case SIP_INFO:                return "INFO";
    case SIP_INFOSTATUS:          return "INFOSTATUS";
    case SIP_IM_TIMEOUT:          return "IM_TIMEOUT";
    case SIP_USER_MESSAGE:        return "USER_IM";
    case SIP_KICKWATCH:           return "KICKWATCH";
    case SIP_MODIFYSESSION:       return "MODIFYSESS";
    default:
        break;
    }
    return "Unknown-Event";
}


QString SipFsmBase::StatetoString(int S)
{
    switch (S)
    {
    case SIP_IDLE:                  return "IDLE";
    case SIP_OCONNECTING1:          return "OCONNECT1";
    case SIP_OCONNECTING2:          return "OCONNECT2";
    case SIP_ICONNECTING:           return "ICONNECT";
    case SIP_ICONNECTING_WAITACK:   return "ICONNECT_WA";
    case SIP_CONNECTED:             return "CONNECTED";
    case SIP_DISCONNECTING:         return "DISCONNECT ";
    case SIP_CONNECTED_VXML:        return "CONNECT-VXML";  // A false state! Only used to indicate to frontend 
    case SIP_SUB_SUBSCRIBED:        return "SUB_SUBSCRIBED";
    case SIP_WATCH_TRYING:          return "WTCH_TRYING"; 
    case SIP_WATCH_ACTIVE:          return "WTCH_ACTIVE"; 
    case SIP_WATCH_STOPPING:        return "WTCH_STOPPING";
    case SIP_WATCH_HOLDOFF:         return "WTCH_HOLDDOFF";
    case SIP_IM_ACTIVE:             return "IM_ACTIVE";
    case SIP_CONNECT_MODIFYING1:    return "CONN_MOD1";
    case SIP_CONNECT_MODIFYING2:    return "CONN_MOD2";

    default:
        break;
    }
    return "Unknown-State";
}






/**********************************************************************
SipCall

This class handles a per call instance of the FSM
**********************************************************************/

SipCall::SipCall(QString localIp, QString natIp, int localPort, int n, SipFsm *par) : SipFsmBase(par)
{
    callRef = n;
    sipLocalIP = localIp;
    sipNatIP = natIp;
    sipLocalPort = localPort;
    initialise();
}

SipCall::~SipCall()
{
}


void SipCall::initialise()
{
    // Initialise Local Parameters.  We get info from the database on every new
    // call in case it has been changed
    myDisplayName = gContext->GetSetting("MySipName");
    sipUsername = "MythPhone";//gContext->GetSetting("MySipUser");  -- Note; this is really not needed & is too much config

    // Get other params - done on a per call basis so config changes take effect immediately
    sipAudioRtpPort = atoi((const char *)gContext->GetSetting("AudioLocalPort"));
    sipVideoRtpPort = atoi((const char *)gContext->GetSetting("VideoLocalPort"));

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
    viaRegProxy = 0;
    ModifyAudioCodec = -1;

    MyUrl = 0;
    MyContactUrl = 0;

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
        else if (CodecStr == "GSM")
        {
            CodecList[n].Payload = 3;
            CodecList[n++].Encoding = "GSM";
        }
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


bool SipCall::ModifyCodecs(QString audioCodec, QString videoCodec)
{
    ModifyAudioCodec = -1;
    for (int n=0; n<MAX_AUDIO_CODECS; n++)
    {
        if (CodecList[n].Encoding == audioCodec)
            ModifyAudioCodec = n;
    }

    if (ModifyAudioCodec == -1)
        return false;

    if (videoCodec == "H.263")
        allowVideo = true;
    else if (videoCodec != "UNCHANGED")
        allowVideo = false;

    return true;
}


int SipCall::FSM(int Event, SipMsg *sipMsg, void *Value)
{
    (void)Value;
    int oldState = State;

    // Parse SIP messages for general relevant data
    if (sipMsg != 0)
        ParseSipMsg(Event, sipMsg);

    if (State == SIP_IDLE) // Setup local variables on first event
    {
        if (UseNat())
            sipLocalIP = sipNatIP;
        MyContactUrl = new SipUrl(myDisplayName, sipUsername, sipLocalIP, sipLocalPort);
        if (viaRegProxy == 0)
            MyUrl = new SipUrl(myDisplayName, sipUsername, sipLocalIP, sipLocalPort);
        else
            MyUrl = new SipUrl(myDisplayName, viaRegProxy->registeredAs(), viaRegProxy->registeredTo(), viaRegProxy->registeredPort());
    }

    switch(Event | State)
    {
    case SIP_IDLE_BYE:
        BuildSendStatus(481, "BYE", sipMsg->getCSeqValue()); //481 Call/Transaction does not exist
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
        cseq = 1;
        remoteUrl = new SipUrl(DestinationUri, "");
        if ((remoteUrl->getHostIp()).length() == 0)
        {
            cout << "SIP: Tried to call " << DestinationUri << " but can't get destination IP address\n";
            State = SIP_IDLE;
            break;
        }

#ifdef SIPREGISTRAR
        // If the domain matches the local registrar, see if user is registered
        if ((remoteUrl->getHost() == "volkaerts") &&
            (!(parent->getRegistrar())->getRegisteredContact(remoteUrl)))
        {
            cout << DestinationUri << " is not registered here\n";
            break;
        }
#endif
        BuildSendInvite(0);
        State = SIP_OCONNECTING1;
        break;
    case SIP_IDLE_INVITE:
        cseq = sipMsg->getCSeqValue();
#ifdef SIPREGISTRAR
        if ((toUrl->getUser() == sipUsername)) && (toUrl->getHost() ==  "Volkaerts"))
#endif
        {
            if (parent->numCalls() > 1)     // Check there are no active calls, and give busy if there is
            {
                BuildSendStatus(486, "INVITE", sipMsg->getCSeqValue()); //486 Busy Here
                State = SIP_DISCONNECTING;
            }
            else 
            {
                GetSDPInfo(sipMsg);
                if (audioPayloadIdx != -1) // INVITE had a codec we support; proces
                {
                    AlertUser(sipMsg);
                    BuildSendStatus(100, "INVITE", sipMsg->getCSeqValue(), SIP_OPT_CONTACT | SIP_OPT_TIMESTAMP); //100 Trying
                    BuildSendStatus(180, "INVITE", sipMsg->getCSeqValue(), SIP_OPT_CONTACT); //180 Ringing
                    State = SIP_ICONNECTING;
                }
                else
                {
                    BuildSendStatus(488, "INVITE", sipMsg->getCSeqValue()); //488 Not Acceptable Here
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
            BuildSendStatus(404, "INVITE", sipMsg->getCSeqValue()); //404 Not Found
            State = SIP_DISCONNECTING;
        }
#endif
        break;
    case SIP_OCONNECTING1_INVITESTATUS_1xx:
        (parent->Timer())->Stop(this, SIP_RETX);
        if ((sipMsg->getStatusCode() == 180) && (eventWindow))
            QApplication::postEvent(eventWindow, new SipEvent(SipEvent::SipRingbackTone)); 
        parent->SetNotification("CALLSTATUS", "", QString::number(sipMsg->getStatusCode()), sipMsg->getReasonPhrase());
        State = SIP_OCONNECTING2;
        break;
    case SIP_OCONNECTING1_INVITESTATUS_3456:
        (parent->Timer())->Stop(this, SIP_RETX);
        parent->SetNotification("CALLSTATUS", "", QString::number(sipMsg->getStatusCode()), sipMsg->getReasonPhrase());
        // Fall through
    case SIP_OCONNECTING2_INVITESTATUS_3456:
        if (((sipMsg->getStatusCode() == 407) || (sipMsg->getStatusCode() == 401)) && 
            (viaRegProxy != 0) && (viaRegProxy->isRegistered())) // Authentication Required
        {
            if (!sentAuthenticated) // This avoids loops where we are not authenticating properly
            {
                BuildSendAck();
                BuildSendInvite(sipMsg);
                State = SIP_OCONNECTING1;
            }
        }
        else
        {
            BuildSendAck();
            State = SIP_IDLE;
        }
        break;
    
    case SIP_OCONNECTING2_INVITESTATUS_1xx:
        if ((sipMsg->getStatusCode() == 180) && (eventWindow))
            QApplication::postEvent(eventWindow, new SipEvent(SipEvent::SipRingbackTone)); 
        parent->SetNotification("CALLSTATUS", "", QString::number(sipMsg->getStatusCode()), sipMsg->getReasonPhrase());
        break;

    case SIP_OCONNECTING1_INVITESTATUS_2xx:
        (parent->Timer())->Stop(this, SIP_RETX);
        // Fall through
    case SIP_OCONNECTING2_INVITESTATUS_2xx:
        GetSDPInfo(sipMsg);
        if (audioPayloadIdx != -1) // INVITE had a codec we support; proces
        {
            BuildSendAck();
            if (eventWindow)
            {
                QApplication::postEvent(eventWindow, new SipEvent(SipEvent::SipCeaseRingbackTone)); 
                QApplication::postEvent(eventWindow, new SipEvent(SipEvent::SipStartMedia, 
                                                                  remoteIp,
                                                                  CodecList[audioPayloadIdx].Payload,
                                                                  CodecList[audioPayloadIdx].Encoding,
                                                                  videoPayload,
                                                                  (videoPayload == 34 ? "H263" : ""),
                                                                  dtmfPayload,
                                                                  remoteAudioPort,
                                                                  remoteVideoPort,
                                                                  rxVideoResolution));
            }                                                      
            State = SIP_CONNECTED;
        }
        else
        {
            cerr << "2xx STATUS did not contain a valid Audio codec\n";
            BuildSendAck();  // What is the right thing to do here?
            BuildSendBye(0);
            if (eventWindow)
                QApplication::postEvent(eventWindow, new SipEvent(SipEvent::SipCeaseRingbackTone)); 
            State = SIP_DISCONNECTING;
        }
        break;
    case SIP_OCONNECTING1_INVITE:
        // This is usually because we sent the INVITE to ourselves, & when we receive it matches the call-id for this call leg
        (parent->Timer())->Stop(this, SIP_RETX);
        BuildSendCancel(0);
        if (eventWindow)
            QApplication::postEvent(eventWindow, new SipEvent(SipEvent::SipCeaseRingbackTone)); 
        State = SIP_DISCONNECTING;
        break;
    case SIP_OCONNECTING1_HANGUP:
        (parent->Timer())->Stop(this, SIP_RETX);
        BuildSendCancel(0);
        if (eventWindow)
            QApplication::postEvent(eventWindow, new SipEvent(SipEvent::SipCeaseRingbackTone)); 
        State = SIP_IDLE;
        break;
    case SIP_OCONNECTING1_RETX:
        if (Retransmit(false))
            (parent->Timer())->Start(this, t1, SIP_RETX);
        else
        {
            if (eventWindow)
                QApplication::postEvent(eventWindow, new SipEvent(SipEvent::SipCeaseRingbackTone)); 
            State = SIP_IDLE;
        }
        break;
    case SIP_OCONNECTING2_HANGUP:
        BuildSendCancel(0);
        if (eventWindow)
            QApplication::postEvent(eventWindow, new SipEvent(SipEvent::SipCeaseRingbackTone)); 
        State = SIP_DISCONNECTING;
        break;
    case SIP_ICONNECTING_INVITE:
        BuildSendStatus(180, "INVITE", sipMsg->getCSeqValue(), SIP_OPT_CONTACT); // Retxed INVITE, resend 180 Ringing
        break;
    case SIP_ICONNECTING_ANSWER:
        BuildSendStatus(200, "INVITE", cseq, SIP_OPT_SDP | SIP_OPT_CONTACT, -1, BuildSdpResponse());
        if (eventWindow)
            QApplication::postEvent(eventWindow, new SipEvent(SipEvent::SipCeaseAlertUser)); 
        State = SIP_ICONNECTING_WAITACK;
        break;
    case SIP_ICONNECTING_CANCEL:
        if (eventWindow)
            QApplication::postEvent(eventWindow, new SipEvent(SipEvent::SipCeaseAlertUser)); 
    case SIP_ICONNECTING_WAITACK_CANCEL:
        BuildSendStatus(200, "CANCEL", sipMsg->getCSeqValue()); //200 Ok
        State = SIP_IDLE;
        break;
    case SIP_ICONNECTING_WAITACK_ACK:
        (parent->Timer())->Stop(this, SIP_RETX); // Stop resending 200 OKs
        if (eventWindow)
            QApplication::postEvent(eventWindow, new SipEvent(SipEvent::SipStartMedia,
                                                              remoteIp,
                                                              CodecList[audioPayloadIdx].Payload,
                                                              CodecList[audioPayloadIdx].Encoding,
                                                              videoPayload,
                                                              (videoPayload == 34 ? "H263" : ""),
                                                              dtmfPayload,
                                                              remoteAudioPort,
                                                              remoteVideoPort,
                                                              rxVideoResolution));
        State = SIP_CONNECTED;
        break;
    case SIP_ICONNECTING_WAITACK_RETX:
        if (Retransmit(false))
            (parent->Timer())->Start(this, t1, SIP_RETX);
        else
            State = SIP_IDLE;
        break;
    case SIP_ICONNECTING_WAITACK_HANGUP:
        BuildSendBye(0);
        State = SIP_DISCONNECTING;
        break;
    case SIP_CONNECTED_ACK:
        break;
    case SIP_CONNECTED_INVITESTATUS_2xx:
        Retransmit(true); // Resend our ACK
        break;
    case SIP_CONNECTED_RETX:
        if (Retransmit(false))
            (parent->Timer())->Start(this, t1, SIP_RETX);
        else
        {
            State = SIP_IDLE;
            if (eventWindow)
                QApplication::postEvent(eventWindow, new SipEvent(SipEvent::SipStopMedia));
        }
        break;
    case SIP_CONNECTED_BYE:
        (parent->Timer())->Stop(this, SIP_RETX); 
        if (sipMsg->getCSeqValue() > cseq)
        {
            cseq = sipMsg->getCSeqValue();
            BuildSendStatus(200, "BYE", cseq); //200 Ok
            if (eventWindow)
                QApplication::postEvent(eventWindow, new SipEvent(SipEvent::SipStopMedia));
            State = SIP_IDLE;
        }
        else
            BuildSendStatus(400, "BYE", sipMsg->getCSeqValue()); //400 Bad Request
        break;
    case SIP_CONNECTED_HANGUP:
        BuildSendBye(0);
        if (eventWindow)
            QApplication::postEvent(eventWindow, new SipEvent(SipEvent::SipStopMedia));
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
        BuildSendStatus(200, "CANCEL", sipMsg->getCSeqValue()); //200 Ok
        State = SIP_IDLE;
        break;
    case SIP_DISCONNECTING_BYESTATUS:
        (parent->Timer())->Stop(this, SIP_RETX); 
        if (((sipMsg->getStatusCode() == 407) || (sipMsg->getStatusCode() == 401)) && 
             (viaRegProxy != 0) && (viaRegProxy->isRegistered())) // Authentication Required
        {
            if (!sentAuthenticated)
                BuildSendBye(sipMsg);
        }
        else
            State = SIP_IDLE;
        break;
    case SIP_DISCONNECTING_CANCELSTATUS:
        (parent->Timer())->Stop(this, SIP_RETX); 
        if (((sipMsg->getStatusCode() == 407) || (sipMsg->getStatusCode() == 401)) && 
            (viaRegProxy != 0) && (viaRegProxy->isRegistered())) // Authentication Required
        {
            if (!sentAuthenticated)
                BuildSendCancel(sipMsg);
        }
        else
            State = SIP_IDLE;
        break;
    case SIP_DISCONNECTING_BYE:
        (parent->Timer())->Stop(this, SIP_RETX); 
        BuildSendStatus(200, "BYE", sipMsg->getCSeqValue()); //200 Ok
        State = SIP_IDLE;
        break;
        
    // REINVITE FSM        
    case SIP_CONNECTED_MODIFYSESSION:
        BuildSendReInvite(0);
        State = SIP_CONNECT_MODIFYING1;
        break;
    case SIP_CONNECTED_INVITE:
        GetSDPInfo(sipMsg);
        if (audioPayloadIdx != -1) // INVITE had a codec we support; proces
        {
            BuildSendStatus(200, "INVITE", cseq, SIP_OPT_SDP | SIP_OPT_CONTACT, -1, BuildSdpResponse());
            State = SIP_CONNECT_MODIFYING2;
        }
        else
        {
            BuildSendStatus(488, "INVITE", sipMsg->getCSeqValue()); //488 Not Acceptable Here
            State = SIP_CONNECTED; // Ignore media change request
        }
        break;
    case SIP_CONNMOD1_INVITESTATUS_1xx:
        break;
    case SIP_CONNMOD1_INVITESTATUS_2xx:
        (parent->Timer())->Stop(this, SIP_RETX);
        GetSDPInfo(sipMsg);
        if (audioPayloadIdx != -1) // INVITE had a codec we support; proces
        {
            BuildSendAck();
            if (eventWindow)
            {
                QApplication::postEvent(eventWindow, new SipEvent(SipEvent::SipChangeMedia, 
                                                                  remoteIp,
                                                                  CodecList[audioPayloadIdx].Payload,
                                                                  CodecList[audioPayloadIdx].Encoding,
                                                                  videoPayload,
                                                                  (videoPayload == 34 ? "H263" : ""),
                                                                  dtmfPayload,
                                                                  remoteAudioPort,
                                                                  remoteVideoPort,
                                                                  rxVideoResolution));
            }                                                      
            State = SIP_CONNECTED;
        }
        else
        {
            cerr << "2xx STATUS on reINVITE did not contain a valid Audio codec\n";
            BuildSendAck();  
            State = SIP_CONNECTED;
        }
        break;
    case SIP_CONNMOD1_INVITESTATUS_3456:
        (parent->Timer())->Stop(this, SIP_RETX);
        if (((sipMsg->getStatusCode() == 407) || (sipMsg->getStatusCode() == 401)) && 
            (viaRegProxy != 0) && (viaRegProxy->isRegistered())) // Authentication Required
        {
            if (!sentAuthenticated) // This avoids loops where we are not authenticating properly
            {
                BuildSendAck();
                BuildSendReInvite(sipMsg);
            }
        }
        else // Unknown status message, return to connected state without changing media
        {
            BuildSendAck();
            State = SIP_CONNECTED;
        }
        break;
    case SIP_CONNMOD1_RETX:
        if (Retransmit(false))
            (parent->Timer())->Start(this, t1, SIP_RETX);
        else
        {
            if (eventWindow)
                QApplication::postEvent(eventWindow, new SipEvent(SipEvent::SipStopMedia));
            State = SIP_IDLE;
        }
        break;
    case SIP_CONNMOD2_RETX:
        if (Retransmit(false))
            (parent->Timer())->Start(this, t1, SIP_RETX);
        else
        {
            if (eventWindow)
                QApplication::postEvent(eventWindow, new SipEvent(SipEvent::SipStopMedia));
            State = SIP_IDLE;
        }
        break;
    case SIP_CONNMOD2_ACK:
        (parent->Timer())->Stop(this, SIP_RETX); // Stop resending 200 OKs
        if (eventWindow)
            QApplication::postEvent(eventWindow, new SipEvent(SipEvent::SipChangeMedia,
                                                              remoteIp,
                                                              CodecList[audioPayloadIdx].Payload,
                                                              CodecList[audioPayloadIdx].Encoding,
                                                              videoPayload,
                                                              (videoPayload == 34 ? "H263" : ""),
                                                              dtmfPayload,
                                                              remoteAudioPort,
                                                              remoteVideoPort,
                                                              rxVideoResolution));
        State = SIP_CONNECTED;
        break;
        
    // Everything else is an error, just flag it for now
    default:
        SipFsm::Debug(SipDebugEvent::SipErrorEv, "SIP CALL FSM Error; received " + EventtoString(Event) + " in state " + StatetoString(State) + "\n\n");
        break;
    }

    DebugFsm(Event, oldState, State);
    return State;
}

bool SipCall::UseNat()
{
    return !disableNat;
}


void SipCall::BuildSendInvite(SipMsg *authMsg)
{
    if (authMsg == 0)
        CallId.Generate(sipLocalIP);

    SipMsg Invite("INVITE");
    Invite.addRequestLine(*remoteUrl);
    Invite.addVia(sipLocalIP, sipLocalPort);
    Invite.addFrom(*MyUrl, myTag);
    Invite.addTo(*remoteUrl);
    Invite.addCallId(CallId);
    Invite.addCSeq(++cseq);
    Invite.addUserAgent();

    if (authMsg)
    {
        if (authMsg->getAuthMethod() == "Digest")
            Invite.addAuthorization(authMsg->getAuthMethod(), viaRegProxy->registeredAs(), viaRegProxy->registeredPasswd(), authMsg->getAuthRealm(), authMsg->getAuthNonce(), remoteUrl->formatReqLineUrl(), authMsg->getStatusCode() == 407);
        else
            cout << "SIP: Unknown Auth Type: " << authMsg->getAuthMethod() << endl;
        sentAuthenticated = true;
    }
    else    
        sentAuthenticated = false;

    //Invite.addAllow();
    Invite.addContact(*MyContactUrl);
    addSdpToInvite(Invite, allowVideo);
    
    parent->Transmit(Invite.string(), retxIp = remoteUrl->getHostIp(), retxPort = remoteUrl->getPort());
    retx = Invite.string();
    t1 = 500;
    (parent->Timer())->Start(this, t1, SIP_RETX);
}



void SipCall::BuildSendReInvite(SipMsg *authMsg)
{
    SipMsg Invite("INVITE");
    Invite.addRequestLine(*remoteUrl);
    Invite.addVia(sipLocalIP, sipLocalPort);
    Invite.addFrom(*MyUrl, myTag);
    Invite.addTo(*remoteUrl);
    Invite.addCallId(CallId);
    Invite.addCSeq(++cseq);
    Invite.addUserAgent();

    if (authMsg)
    {
        if (authMsg->getAuthMethod() == "Digest")
            Invite.addAuthorization(authMsg->getAuthMethod(), viaRegProxy->registeredAs(), viaRegProxy->registeredPasswd(), authMsg->getAuthRealm(), authMsg->getAuthNonce(), remoteUrl->formatReqLineUrl(), authMsg->getStatusCode() == 407);
        else
            cout << "SIP: Unknown Auth Type: " << authMsg->getAuthMethod() << endl;
        sentAuthenticated = true;
    }
    else    
        sentAuthenticated = false;

    //Invite.addAllow();
    Invite.addContact(*MyContactUrl);
    addSdpToInvite(Invite, allowVideo, ModifyAudioCodec);
    
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
    if ((MyUrl == 0) || (remoteUrl == 0))
    {
        cerr << "URL variables not setup\n";
        return;
    }

    SipMsg Ack("ACK");
    Ack.addRequestLine(*remoteUrl);
    Ack.addVia(sipLocalIP, sipLocalPort);
    Ack.addFrom(*MyUrl, myTag);
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


void SipCall::BuildSendCancel(SipMsg *authMsg)
{
    if ((MyUrl == 0) || (remoteUrl == 0))
    {
        cerr << "URL variables not setup\n";
        return;
    }

    SipMsg Cancel("CANCEL");
    Cancel.addRequestLine(*remoteUrl);
    Cancel.addVia(sipLocalIP, sipLocalPort);
    Cancel.addTo(*remoteUrl, remoteTag);
    Cancel.addFrom(*MyUrl, myTag);
    Cancel.addCallId(CallId);
    Cancel.addCSeq(cseq);
    Cancel.addUserAgent();

    if (authMsg)
    {
        if (authMsg->getAuthMethod() == "Digest")
            Cancel.addAuthorization(authMsg->getAuthMethod(), viaRegProxy->registeredAs(), viaRegProxy->registeredPasswd(), authMsg->getAuthRealm(), authMsg->getAuthNonce(), remoteUrl->formatReqLineUrl(), authMsg->getStatusCode() == 407);
        else
            cout << "SIP: Unknown Auth Type: " << authMsg->getAuthMethod() << endl;
        sentAuthenticated = true;
    }
    else    
        sentAuthenticated = false;

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


void SipCall::BuildSendBye(SipMsg *authMsg)
{
    if (remoteUrl == 0)
    {
        cerr << "URL variables not setup\n";
        return;
    }

    SipMsg Bye("BYE");
    Bye.addRequestLine(*remoteUrl);
    Bye.addVia(sipLocalIP, sipLocalPort);
    if (rxedFrom.length() > 0)
    {
        Bye.addFromCopy(rxedFrom);
        Bye.addToCopy(rxedTo, myTag);
    } 
    else
    {
        Bye.addFrom(*MyUrl, myTag);
        Bye.addTo(*remoteUrl, remoteTag);
    }
    Bye.addCallId(CallId);
    Bye.addCSeq(++cseq);
    Bye.addUserAgent();

    if (authMsg)
    {
        if (authMsg->getAuthMethod() == "Digest")
            Bye.addAuthorization(authMsg->getAuthMethod(), viaRegProxy->registeredAs(), viaRegProxy->registeredPasswd(), authMsg->getAuthRealm(), authMsg->getAuthNonce(), remoteUrl->formatReqLineUrl(), authMsg->getStatusCode() == 407);
        else
            cout << "SIP: Unknown Auth Type: " << authMsg->getAuthMethod() << endl;
        sentAuthenticated = true;
    }
    else    
        sentAuthenticated = false;

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
            if ((viaRegProxy) && (viaRegProxy->registeredTo() == from->getHost()))
                CallerUrl = from->getUser();
            else
            {
                CallerUrl = from->getUser() + "@" + from->getHost();
                if (from->getPort() != 5060)
                    CallerUrl += ":" + QString::number(from->getPort());
            }
            CallersDisplayName = from->getDisplay();
            
            if (eventWindow)
                QApplication::postEvent(eventWindow, new SipEvent(SipEvent::SipAlertUser, 
                                                                  CallersUserid,
                                                                  CallerUrl,
                                                                  CallersDisplayName,
                                                                  videoPayload == -1));
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
            for (int n=0; (n<MAX_AUDIO_CODECS) && (CodecList[n].Payload != -1) &&
                          (audioPayloadIdx == -1); n++)
            {
                for (c=audioCodecs->first(); c; c=audioCodecs->next())
                {
                    if (CodecList[n].Payload == c->intValue())
                        audioPayloadIdx = n;
                        
                    // Note - no checking for dynamic payloads implemented yet --- need to match
                    // by text if .Payload == -1
                }
            }
    
            // Also check for DTMF
            for (c=audioCodecs->first(); c; c=audioCodecs->next())
            {
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

        SipFsm::Debug(SipDebugEvent::SipDebugEv, "SDP contains IP " + remoteIp + " A-Port " + QString::number(remoteAudioPort) + " V-Port " + QString::number(remoteVideoPort) + " Audio Codec:" + QString::number(audioPayloadIdx) + " Video Codec:" + QString::number(videoPayload) + " Format:" + rxVideoResolution + " DTMF: " + QString::number(dtmfPayload) + "\n\n");
    }
    else
        SipFsm::Debug(SipDebugEvent::SipDebugEv, "SIP: No SDP in message\n");
}



void SipCall::addSdpToInvite(SipMsg& msg, bool advertiseVideo, int audioCodec)
{
    SipSdp sdp(sipLocalIP, sipAudioRtpPort, advertiseVideo ? sipVideoRtpPort : 0);

    if ((audioCodec < 0) || (audioCodec >= MAX_AUDIO_CODECS))
    {
        for (int n=0; (n<MAX_AUDIO_CODECS) && (CodecList[n].Payload != -1); n++)
            sdp.addAudioCodec(CodecList[n].Payload, CodecList[n].Encoding + "/8000");
    }
    else
        sdp.addAudioCodec(CodecList[audioCodec].Payload, CodecList[audioCodec].Encoding + "/8000");

    // Signal support for DTMF
    sdp.addAudioCodec(101, "telephone-event/8000", "0-11");

    if (advertiseVideo)
        sdp.addVideoCodec(34, "H263/90000", txVideoResolution +"=2");
    sdp.encode();
    msg.addContent("application/sdp", sdp.string());
}


QString SipCall::BuildSdpResponse()
{
    SipSdp sdp(sipLocalIP, sipAudioRtpPort, (videoPayload != -1) ? sipVideoRtpPort : 0);

    sdp.addAudioCodec(CodecList[audioPayloadIdx].Payload, CodecList[audioPayloadIdx].Encoding + "/8000");

    // Signal support for DTMF
    if (dtmfPayload != -1)
        sdp.addAudioCodec(dtmfPayload, "telephone-event/8000", "0-11");

    if (videoPayload != -1)
        sdp.addVideoCodec(34, "H263/90000", txVideoResolution +"=2");

    sdp.encode();
    return sdp.string();
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


SipRegistrar::SipRegistrar(SipFsm *par, QString domain, QString localIp, int localPort) : SipFsmBase(par)
{
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
    Status.addTo(*(sipMsg->getFromUrl()), myTag);
    Status.addCallId(*sipMsg->getCallId());
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

SipRegistration::SipRegistration(SipFsm *par, QString localIp, int localPort, QString Username, QString Password, QString ProxyName, int ProxyPort) : SipFsmBase(par)
{
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
    if (ProxyUrl)
        delete ProxyUrl;
    if (MyUrl)
        delete MyUrl;
    if (MyContactUrl)
        delete MyContactUrl;
    ProxyUrl = MyUrl = MyContactUrl = 0;
    (parent->Timer())->StopAll(this);
}

int SipRegistration::FSM(int Event, SipMsg *sipMsg, void *Value)
{
    (void)Value;
    switch (Event | State)
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
        case 407:
            SendRegister(sipMsg);
            regRetryCount = REG_RETRY_MAXCOUNT;
            State = SIP_REG_CHALLENGED;
            (parent->Timer())->Start(this, REG_RETRY_TIMER, SIP_RETX); 
            break;
        default:
            if (sipMsg->getStatusCode() != 100)
            {
                cout << "SIP Registration failed; Reason " << sipMsg->getStatusCode() << " " << sipMsg->getReasonPhrase() << endl;
                State = SIP_REG_FAILED;
                (parent->Timer())->Start(this, REG_FAIL_RETRY_TIMER, SIP_RETX); // Try again in 3 minutes
            }
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
            if (sipMsg->getStatusCode() != 100)
            {
                cout << "SIP Registration failed; Reason " << sipMsg->getStatusCode() << " " << sipMsg->getReasonPhrase() << endl;
                State = SIP_REG_FAILED;
                (parent->Timer())->Start(this, REG_FAIL_RETRY_TIMER, SIP_RETX); // Try again in 3 minutes
            }
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
            State = SIP_REG_TRYING;
            SendRegister();
            (parent->Timer())->Start(this, REG_RETRY_TIMER, SIP_RETX); // Retry every 10 seconds
        }
        else
        {
            State = SIP_REG_FAILED;
            cout << "SIP Registration failed; no Response from Server. Are you behind a firewall?\n";
        }
        break;

    default:
        cerr << "SIP Registration: Unknown Event " << EventtoString(Event) << ", State " << State << endl;
        break;
    }
    return 0;
}

void SipRegistration::SendRegister(SipMsg *authMsg)
{
    SipMsg Register("REGISTER");
    Register.addRequestLine(*ProxyUrl);
    Register.addVia(sipLocalIp, sipLocalPort);
    Register.addFrom(*MyUrl, myTag);
    Register.addTo(*MyUrl);
    Register.addCallId(CallId);
    Register.addCSeq(++cseq);

    if (authMsg && (authMsg->getAuthMethod() == "Digest"))
    {
        Register.addAuthorization(authMsg->getAuthMethod(), MyUrl->getUser(), MyPassword, 
                                  authMsg->getAuthRealm(), authMsg->getAuthNonce(), 
                                  ProxyUrl->formatReqLineUrl(), authMsg->getStatusCode() == 407);
        sentAuthenticated = true;
    }
    else    
        sentAuthenticated = false;

    Register.addUserAgent();
    Register.addExpires(Expires=3600);
    Register.addContact(*MyContactUrl);
    Register.addNullContent();

    parent->Transmit(Register.string(), ProxyUrl->getHostIp(), ProxyUrl->getPort());
}



/**********************************************************************
SipSubscriber

FSM to handle clients subscribed to our presence status.
**********************************************************************/

SipSubscriber::SipSubscriber(SipFsm *par, QString localIp, int localPort, SipRegistration *reg, QString status) : SipFsmBase(par)
{
    sipLocalIp = localIp;
    sipLocalPort = localPort;
    regProxy = reg;
    myStatus = status;
    watcherUrl = 0;
    State = SIP_SUB_IDLE;

    if (regProxy)
        MyUrl = new SipUrl("", regProxy->registeredAs(), regProxy->registeredTo(), 5060);
    else
        MyUrl = new SipUrl("", "MythPhone", sipLocalIp, sipLocalPort);
    MyContactUrl = new SipUrl("", "", sipLocalIp, sipLocalPort);
    cseq = 2;
}

SipSubscriber::~SipSubscriber()
{
    (parent->Timer())->StopAll(this); 
    if (watcherUrl)
        delete watcherUrl;
    if (MyUrl)
        delete MyUrl;
    if (MyContactUrl)
        delete MyContactUrl;
    watcherUrl = MyUrl = MyContactUrl = 0;
}

int SipSubscriber::FSM(int Event, SipMsg *sipMsg, void *Value)
{
    int OldState = State;

    switch (Event | State)
    {
    case SIP_SUB_IDLE_SUBSCRIBE:
        ParseSipMsg(Event, sipMsg);
        if (watcherUrl == 0)
            watcherUrl = new SipUrl(sipMsg->getFromUrl());
        expires = sipMsg->getExpires();
        if (expires == -1) // No expires in SUBSCRIBE, choose default value
            expires = 600;
        BuildSendStatus(200, "SUBSCRIBE", sipMsg->getCSeqValue(), SIP_OPT_CONTACT | SIP_OPT_EXPIRES, expires);
        if (expires > 0)
        {
            (parent->Timer())->Start(this, expires*1000, SIP_SUBSCRIBE_EXPIRE); // Expire subscription
            SendNotify(0);
            State = SIP_SUB_SUBSCRIBED;
            
            // If this is a client just being turned on, see if we are watching its status
            // and if so shortcut the retry timers
            parent->KickWatcher(watcherUrl);
        }
        break;

    case SIP_SUB_SUBS_SUBSCRIBE:
        ParseSipMsg(Event, sipMsg);
        expires = sipMsg->getExpires();
        if (expires == -1) // No expires in SUBSCRIBE, choose default value
            expires = 600;
        BuildSendStatus(200, "SUBSCRIBE", sipMsg->getCSeqValue(), SIP_OPT_CONTACT | SIP_OPT_EXPIRES, expires);
        if (expires > 0)
        {
            (parent->Timer())->Start(this, expires*1000, SIP_SUBSCRIBE_EXPIRE); // Expire subscription
            SendNotify(0);
        }
        else
            State = SIP_SUB_IDLE;
        break;

    case SIP_SUB_SUBS_SUBSCRIBE_EXPIRE:
        break;

    case SIP_SUB_SUBS_RETX:
        if (Retransmit(false))
            (parent->Timer())->Start(this, t1, SIP_RETX);
        break;

    case SIP_SUB_SUBS_NOTSTATUS:
        (parent->Timer())->Stop(this, SIP_RETX); 
        if (((sipMsg->getStatusCode() == 407) || (sipMsg->getStatusCode() == 401)) && 
            (!sentAuthenticated))
            SendNotify(sipMsg);
        break;

    case SIP_SUB_SUBS_PRESENCE_CHANGE:
        myStatus = (char *)Value;
        SendNotify(0);
        break;

    default:
        SipFsm::Debug(SipDebugEvent::SipErrorEv, "SIP Subscriber FSM Error; received " + EventtoString(Event) + " in state " + StatetoString(State) + "\n\n");
        break;
    }

    DebugFsm(Event, OldState, State);
    return State;
}

void SipSubscriber::SendNotify(SipMsg *authMsg)
{
    SipMsg Notify("NOTIFY");
    Notify.addRequestLine(*watcherUrl);
    Notify.addVia(sipLocalIp, sipLocalPort);
    Notify.addFrom(*MyUrl, myTag);
    Notify.addTo(*watcherUrl, remoteTag, remoteEpid);
    Notify.addCallId(CallId);
    Notify.addCSeq(++cseq);
    int expLeft = (parent->Timer())->msLeft(this, SIP_SUBSCRIBE_EXPIRE)/1000;
    Notify.addExpires(expLeft);
    Notify.addUserAgent();
    Notify.addContact(MyContactUrl);
    Notify.addSubState("active", expLeft);
    Notify.addEvent("presence");

    if (authMsg)
    {
        if (authMsg->getAuthMethod() == "Digest")
            Notify.addAuthorization(authMsg->getAuthMethod(), regProxy->registeredAs(), regProxy->registeredPasswd(), authMsg->getAuthRealm(), authMsg->getAuthNonce(), watcherUrl->formatReqLineUrl(), authMsg->getStatusCode() == 407);
        else
            cout << "SIP: Unknown Auth Type: " << authMsg->getAuthMethod() << endl;
        sentAuthenticated = true;
    }
    else    
        sentAuthenticated = false;

    SipXpidf xpidf(*MyUrl);
    if (myStatus == "CLOSED")
        xpidf.setStatus("inactive", "away");
    else if (myStatus == "ONTHEPHONE")
        xpidf.setStatus("inuse", "onthephone");
    else if (myStatus == "OPEN")
        xpidf.setStatus("open", "online");

    Notify.addContent("application/xpidf+xml", xpidf.encode());

    // Send new transactions to (a) record route, (b) contact URL or (c) configured URL
    if (recRouteUrl)
        parent->Transmit(Notify.string(), retxIp = recRouteUrl->getHostIp(), retxPort = recRouteUrl->getPort());
    else if (contactUrl)
        parent->Transmit(Notify.string(), retxIp = contactUrl->getHostIp(), retxPort = contactUrl->getPort());
    else
        parent->Transmit(Notify.string(), retxIp = watcherUrl->getHostIp(), retxPort = watcherUrl->getPort());
    retx = Notify.string();
    t1 = 500;
    (parent->Timer())->Start(this, t1, SIP_RETX);
}



/**********************************************************************
SipWatcher

FSM to handle subscribing to other clients presence status.
**********************************************************************/

SipWatcher::SipWatcher(SipFsm *par, QString localIp, int localPort, SipRegistration *reg, QString destUrl) : SipFsmBase(par)
{
    sipLocalIp = localIp;
    sipLocalPort = localPort;
    regProxy = reg;
    watchedUrlString = destUrl;

    // If the dialled number if just a username and we are registered to a proxy, append 
    // the proxy hostname
    if ((!destUrl.contains('@')) && (regProxy != 0))
        destUrl.append(QString("@") + gContext->GetSetting("SipProxyName"));

    watchedUrl = new SipUrl(destUrl, "");
    State = SIP_WATCH_IDLE;
    cseq = 1;
    expires = -1;
    CallId.Generate(sipLocalIp);
    if (regProxy)
        MyUrl = new SipUrl("", regProxy->registeredAs(), regProxy->registeredTo(), 5060);
    else
        MyUrl = new SipUrl("", "MythPhone", sipLocalIp, sipLocalPort);
    MyContactUrl = new SipUrl("", "", sipLocalIp, sipLocalPort);

    FSM(SIP_WATCH, 0);
}

SipWatcher::~SipWatcher()
{
    (parent->Timer())->StopAll(this); 
    if (watchedUrl != 0)
        delete watchedUrl;
    if (MyUrl)
        delete MyUrl;
    if (MyContactUrl)
        delete MyContactUrl;
    watchedUrl = MyUrl = MyContactUrl = 0;
}

int SipWatcher::FSM(int Event, SipMsg *sipMsg, void *Value)
{
    (void)Value;
    int OldState = State;
    SipXpidf *xpidf;

    switch (Event | State)
    {
    case SIP_WATCH_IDLE_WATCH:
    case SIP_WATCH_TRYING_WATCH:
    case SIP_WATCH_HOLDOFF_WATCH:
    case SIP_WATCH_HOLDOFF_KICK:
        if ((regProxy == 0) || (regProxy->isRegistered()))
            SendSubscribe(0);
        else
            (parent->Timer())->Start(this, 5000, SIP_WATCH); // Not registered, wait
        State = SIP_WATCH_TRYING;
        break;

    case SIP_WATCH_ACTIVE_SUBSCRIBE_EXPIRE:
        SendSubscribe(0);
        break;

    case SIP_WATCH_TRYING_RETX:
    case SIP_WATCH_ACTIVE_RETX:
        if (Retransmit(false))
            (parent->Timer())->Start(this, t1, SIP_RETX);
        else 
        {
            // We failed to get a response; so retry after a delay
            State = SIP_WATCH_HOLDOFF;
            parent->SetNotification("PRESENCE", watchedUrlString, "offline", "offline");
            (parent->Timer())->Start(this, SIP_POLL_OFFLINE_UA, SIP_WATCH); 
        }
        break;

    case SIP_WATCH_TRYING_SUBSTATUS:
        (parent->Timer())->Stop(this, SIP_RETX); 
        if ((sipMsg->getStatusCode() == 407) || (sipMsg->getStatusCode() == 401))
        {
            if (!sentAuthenticated) // This avoids loops where we are not authenticating properly
                SendSubscribe(sipMsg);
        }
        else if (sipMsg->getStatusCode() == 200)
        {
            State = SIP_WATCH_ACTIVE;
            expires = sipMsg->getExpires();
            if (expires == -1) // No expires in SUBSCRIBE, choose default value
                expires = 600;
            (parent->Timer())->Start(this, expires*1000, SIP_SUBSCRIBE_EXPIRE);
            parent->SetNotification("PRESENCE", watchedUrlString, "open", "undetermined");
        }
        else 
        {
            // We got an invalid response so wait before we retry. Ideally here this
            // should depend on status code; e.g. 404 means try again but 403 means never retry again
            State = SIP_WATCH_HOLDOFF;
            parent->SetNotification("PRESENCE", watchedUrlString, "offline", "offline");
            (parent->Timer())->Start(this, SIP_POLL_OFFLINE_UA, SIP_WATCH); 
        }
        break;

    case SIP_WATCH_ACTIVE_SUBSTATUS:
        (parent->Timer())->Stop(this, SIP_RETX); 
        if ((sipMsg->getStatusCode() == 407) || (sipMsg->getStatusCode() == 401))
        {
            if (!sentAuthenticated)
                SendSubscribe(sipMsg);
        }
        else if (sipMsg->getStatusCode() == 200)
        {
            expires = sipMsg->getExpires();
            if (expires == -1) // No expires in SUBSCRIBE, choose default value
                expires = 600;
            (parent->Timer())->Start(this, expires*1000, SIP_SUBSCRIBE_EXPIRE);
        }
        else 
        {
            // We failed to get a response; so retry after a delay
            State = SIP_WATCH_TRYING;
            (parent->Timer())->Start(this, 120*1000, SIP_WATCH); 
        }
        break;

    case SIP_WATCH_ACTIVE_NOTIFY:
        ParseSipMsg(Event, sipMsg);
        xpidf = sipMsg->getXpidf();
        if (xpidf)
        {
            parent->SetNotification("PRESENCE", watchedUrlString, xpidf->getStatus(), xpidf->getSubstatus());
            BuildSendStatus(200, "NOTIFY", sipMsg->getCSeqValue(), SIP_OPT_CONTACT);
        }
        else
            BuildSendStatus(406, "NOTIFY", sipMsg->getCSeqValue(), SIP_OPT_CONTACT);
        break;

    case SIP_WATCH_TRYING_STOPWATCH:
    case SIP_WATCH_ACTIVE_STOPWATCH:
        State = SIP_WATCH_STOPPING;
        SendSubscribe(0);
        break;

    case SIP_WATCH_HOLDOFF_STOPWATCH:
        State = SIP_WATCH_IDLE;
        break;

    case SIP_WATCH_STOPPING_RETX:
        if (Retransmit(false))
            (parent->Timer())->Start(this, t1, SIP_RETX);
        else
            State = SIP_WATCH_IDLE;
        break;

    case SIP_WATCH_STOPPING_SUBSTATUS:
        (parent->Timer())->Stop(this, SIP_RETX); 
        if ((sipMsg->getStatusCode() == 407) || (sipMsg->getStatusCode() == 401))
        {
            if (!sentAuthenticated)
                SendSubscribe(sipMsg);
        }
        else 
            State = SIP_WATCH_IDLE;
        break;

    case SIP_WATCH_HOLDOFF_SUBSCRIBE:
    case SIP_WATCH_TRYING_SUBSCRIBE:
        // Probably sent a subscribe to myself by accident; leave the FSM in place to soak
        // up messages on this call-id but stop all activity on it
        (parent->Timer())->Stop(this, SIP_RETX); 
        State = SIP_WATCH_HOLDOFF;
        break;

    default:
        SipFsm::Debug(SipDebugEvent::SipErrorEv, "SIP Watcher FSM Error; received " + EventtoString(Event) + " in state " + StatetoString(State) + "\n\n");
        break;
    }

    DebugFsm(Event, OldState, State);
    return State;
}

void SipWatcher::SendSubscribe(SipMsg *authMsg)
{
    SipMsg Subscribe("SUBSCRIBE");
    Subscribe.addRequestLine(*watchedUrl);
    Subscribe.addVia(sipLocalIp, sipLocalPort);
    Subscribe.addFrom(*MyUrl, myTag);
    Subscribe.addTo(*watchedUrl);
    Subscribe.addCallId(CallId);
    Subscribe.addCSeq(++cseq);
    if (State == SIP_WATCH_STOPPING)
        Subscribe.addExpires(0);

    if (authMsg)
    {
        if (authMsg->getAuthMethod() == "Digest")
            Subscribe.addAuthorization(authMsg->getAuthMethod(), regProxy->registeredAs(), regProxy->registeredPasswd(), authMsg->getAuthRealm(), authMsg->getAuthNonce(), watchedUrl->formatReqLineUrl(), authMsg->getStatusCode() == 407);
        else
            cout << "SIP: Unknown Auth Type: " << authMsg->getAuthMethod() << endl;
        sentAuthenticated = true;
    }
    else    
        sentAuthenticated = false;

    Subscribe.addUserAgent();
    Subscribe.addContact(MyContactUrl);

    Subscribe.addEvent("presence");
    //Subscribe.addGenericLine("Accept: application/xpidf+xml\r\n");
    Subscribe.addGenericLine("Accept: application/xpidf+xml, text/xml+msrtc.pidf\r\n");
    Subscribe.addGenericLine("Supported: com.microsoft.autoextend\r\n");
    Subscribe.addNullContent();

    parent->Transmit(Subscribe.string(), retxIp = watchedUrl->getHostIp(), retxPort = watchedUrl->getPort());
    retx = Subscribe.string();
    t1 = 500;
    (parent->Timer())->Start(this, t1, SIP_RETX);
}



/**********************************************************************
SipIM

FSM to handle Instant Messaging
**********************************************************************/

SipIM::SipIM(SipFsm *par, QString localIp, int localPort, SipRegistration *reg, QString destUrl, QString callIdStr) : SipFsmBase(par)
{
    sipLocalIp = localIp;
    sipLocalPort = localPort;
    regProxy = reg;

    State = SIP_IDLE;
    rxCseq = -1;
    txCseq = 1;
    if (callIdStr.length() > 0)
        CallId.setValue(callIdStr);
    else
        CallId.Generate(sipLocalIp);
    imUrl = 0;
    if (destUrl.length() > 0)
    {
        // If the dialled number if just a username and we are registered to a proxy, append 
        // the proxy hostname
        if ((!destUrl.contains('@')) && (regProxy != 0))
            destUrl.append(QString("@") + gContext->GetSetting("SipProxyName"));
    
        imUrl = new SipUrl(destUrl, "");
    }
    
    if (regProxy)
        MyUrl = new SipUrl("", regProxy->registeredAs(), regProxy->registeredTo(), 5060);
    else
        MyUrl = new SipUrl("", "MythPhone", sipLocalIp, sipLocalPort);
    MyContactUrl = new SipUrl("", "", sipLocalIp, sipLocalPort);
}

SipIM::~SipIM()
{
    (parent->Timer())->StopAll(this); 
    if (imUrl)
        delete imUrl;
    if (MyUrl)
        delete MyUrl;
    if (MyContactUrl)
        delete MyContactUrl;
    MyUrl = MyContactUrl = 0;
}

int SipIM::FSM(int Event, SipMsg *sipMsg, void *Value)
{
    int OldState = State;
    QString textContent;

    switch (Event)
    {
    case SIP_USER_MESSAGE:
        msgToSend = *((QString *)Value);
        SendMessage(0, msgToSend);
        State = SIP_IM_ACTIVE;
        break;
        
    case SIP_MESSAGE:
        ParseSipMsg(Event, sipMsg);
        if (rxCseq != sipMsg->getCSeqValue()) // Check for retransmissions
        {
            rxCseq = sipMsg->getCSeqValue();
            textContent = sipMsg->getPlainText();
            parent->SetNotification("IM", remoteUrl->getUser(), CallId.string(), textContent);
        }
        if (imUrl == 0)
            imUrl = new SipUrl(sipMsg->getFromUrl());
        BuildSendStatus(200, "MESSAGE", sipMsg->getCSeqValue(), SIP_OPT_CONTACT);
        State = SIP_IM_ACTIVE;
        (parent->Timer())->Start(this, 30*60*1000, SIP_IM_TIMEOUT); // 30 min of inactivity and IM session clears
        break;

    case SIP_INFO:
        ParseSipMsg(Event, sipMsg);
        BuildSendStatus(200, "INFO", sipMsg->getCSeqValue(), SIP_OPT_CONTACT);
        State = SIP_IM_ACTIVE;
        (parent->Timer())->Start(this, 30*60*1000, SIP_IM_TIMEOUT); 
        break;

    case SIP_MESSAGESTATUS:
        (parent->Timer())->Stop(this, SIP_RETX); 
        if ((sipMsg->getStatusCode() == 407) || (sipMsg->getStatusCode() == 401))
        {
            if (!sentAuthenticated)
                SendMessage(sipMsg, msgToSend); // Note - this "could" have changed if the user is typing quickly
        }
        else if (sipMsg->getStatusCode() != 200)
            cout << "SIP: Send IM got status code " << sipMsg->getStatusCode() << endl;
        (parent->Timer())->Start(this, 30*60*1000, SIP_IM_TIMEOUT); // 30 min of inactivity and IM session clears
        break;

    case SIP_RETX:
        if (Retransmit(false))
            (parent->Timer())->Start(this, t1, SIP_RETX);
        else
            cout << "SIP: Send IM failed to get a response\n";
        break;

    case SIP_IM_TIMEOUT:
        State = SIP_IM_IDLE;
        break;

    default:
        SipFsm::Debug(SipDebugEvent::SipErrorEv, "SIP IM FSM Error; received " + EventtoString(Event) + " in state " + StatetoString(State) + "\n\n");
        break;
    }

    DebugFsm(Event, OldState, State);
    return State;
}

void SipIM::SendMessage(SipMsg *authMsg, QString Text)
{
    SipMsg Message("MESSAGE");
    Message.addRequestLine(*imUrl);
    Message.addVia(sipLocalIp, sipLocalPort);
    Message.addFrom(*MyUrl, myTag);
    Message.addTo(*imUrl, remoteTag, remoteEpid);
    Message.addCallId(CallId);
    Message.addCSeq(++txCseq);

    if (authMsg)
    {
        if (authMsg->getAuthMethod() == "Digest")
            Message.addAuthorization(authMsg->getAuthMethod(), regProxy->registeredAs(), regProxy->registeredPasswd(), authMsg->getAuthRealm(), authMsg->getAuthNonce(), imUrl->formatReqLineUrl(), authMsg->getStatusCode() == 407);
        else
            cout << "SIP: Unknown Auth Type: " << authMsg->getAuthMethod() << endl;
        sentAuthenticated = true;
    }
    else    
        sentAuthenticated = false;

    Message.addUserAgent();
    Message.addContact(MyContactUrl);

    Message.addContent("text/plain", Text);

    if (recRouteUrl != 0)
        parent->Transmit(Message.string(), retxIp = recRouteUrl->getHostIp(), 
                         retxPort = recRouteUrl->getPort());
    else
        parent->Transmit(Message.string(), retxIp = imUrl->getHostIp(), 
                         retxPort = imUrl->getPort());
    retx = Message.string();
    t1 = 500;
    (parent->Timer())->Start(this, t1, SIP_RETX);
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

int SipTimer::msLeft(SipFsmBase *Instance, int expireEvent, void *Value)
{
    aSipTimer *it;
    for (it=first(); it; it=next())
    {
        if (it->match(Instance, expireEvent, Value))
        {
            int secsLeft = (QDateTime::currentDateTime()).secsTo(it->getExpire());
            return ((secsLeft > 0 ? secsLeft : 0)*1000);
        }
    }
    return 0;
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





