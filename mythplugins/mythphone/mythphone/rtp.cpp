/*
	rtp.cpp

	(c) 2004 Paul Volkaerts

  Implementation of an RTP class handling voice, video and DTMF.

*/
#include <qapplication.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <iostream>

#ifndef WIN32
#include <netinet/in.h>
#include <linux/soundcard.h>
#include <unistd.h>
#include <fcntl.h>                                     
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/sockios.h>
#include <mythtv/mythcontext.h>
#include "config.h"
#else
#include <io.h>
#include <winsock2.h>
#include <sstream>
#include "gcontext.h"
#endif

#include "dtmffilter.h"
#include "rtp.h"
#include "g711.h"

using namespace std;


rtp::rtp(QWidget *callingApp, int localPort, QString remoteIP, int remotePort, int mediaPay, int dtmfPay, QString micDev, QString spkDev, rtpTxMode txm, rtpRxMode rxm)
{
    eventWindow = callingApp;
    yourIP.setAddress(remoteIP);
    myPort = localPort;
    yourPort = remotePort;
    txMode = txm;
    rxMode = rxm;
    micDevice = micDev;
    spkDevice = spkDev;
    if ((txMode == RTP_TX_VIDEO) || (rxMode == RTP_RX_VIDEO))
    {
        videoPayload = mediaPay;
        audioPayload = -1;
        dtmfPayload = -1;
        initVideoBuffers(10);
        pTxShaper = new TxShaper(28000, 1000, 50); // 28k bytes/sec is 256kbps less 28kbps for speech
    }
    else
    {
        videoPayload = -1;
        audioPayload = mediaPay;
        dtmfPayload = dtmfPay;
        pTxShaper = 0;
    }
    
    // Setup a DTMF Signal Analysis filter to capture DTMF from the inband signal if RFC2833 is not
    // negotiated.  This is not used during 2-way speech, just using voicemail
    DTMFFilter = 0;
    if ((dtmfPayload == -1) && (audioPayload != -1) && (rxMode != RTP_RX_AUDIO_TO_SPEAKER))
    {
        DTMFFilter = new DtmfFilter();
    }

    // Clear variables within the calling tasks thread that are used by the calling 
    // task to prevent race conditions
    pkIn = 0;
    pkOut = 0;
    pkOutDrop = 0;
    pkMissed = 0;
    pkLate = 0;
    pkInDisc = 0;
    framesIn = 0;
    framesOut = 0;
    framesOutDiscarded = 0;
    framesInDiscarded = 0;
    txBuffer = 0;
    recBuffer = 0;
    dtmfIn = "";
    dtmfOut = "";
    videoToTx = 0;
    eventCond = 0;

#ifdef WIN32
    hwndLast = callingApp->winId();
#endif

    rtpInitialise();

    killRtpThread = false;
    start(TimeCriticalPriority);
}

rtp::~rtp()
{
    killRtpThread = true;
    SpeakerOn = false;
    MicrophoneOn = false;
    if (eventCond) 
        eventCond->wakeAll();
    wait();
    destroyVideoBuffers();
    if (DTMFFilter)
        delete DTMFFilter;
    if (pTxShaper)
        delete pTxShaper;
}

void rtp::run()
{
    rtpThreadWorker();
}


void rtp::rtpThreadWorker()
{
    if ((txMode == RTP_TX_VIDEO) || (rxMode == RTP_RX_VIDEO))
        rtpVideoThreadWorker();
    else
        rtpAudioThreadWorker();
}

void rtp::rtpAudioThreadWorker()
{
    RTPPACKET RTPpacket;
    QTime timeNextTx;
    bool micFirstTime = true;

    OpenSocket();
    StartTxRx();

    timeNextTx = (QTime::currentTime()).addMSecs(rxMsPacketSize);

    int pollLoop=0;
    int sleepMs = 0;
    while(!killRtpThread)
    {
        // Awake every 10ms to see if we need to rx/tx anything
        // May need to revisit this; as I'd much prefer it to be event driven
        // usleep(10000) seems to cause a 20ms sleep whereas usleep(0)
        // seems to sleep for ~10ms
        QTime t1 = QTime::currentTime();    
        usleep(10000);  
        sleepMs += t1.msecsTo(QTime::currentTime());
        pollLoop++;

        if (killRtpThread)
            break;

        // Pull in all received packets
        StreamInAudio();

        // Write audio to the speaker, but keep in dejitter buffer as long as possible
        while (isSpeakerHungry() && pJitter->AnyData() && !killRtpThread)
            PlayOutAudio();
        
        // For mic. data, the microphone determines the transmit rate
        // Mic. needs kicked the first time through
        while ((txMode == RTP_TX_AUDIO_FROM_MICROPHONE) && 
            ((isMicrophoneData()) || micFirstTime) && (!killRtpThread))
        {
            micFirstTime = false;
            if (fillPacketfromMic(RTPpacket))
                StreamOut(RTPpacket);
        }

        // For transmitting silence/buffered data we need to use the clock
        // as timing
        if (((txMode == RTP_TX_AUDIO_SILENCE) || (txMode == RTP_TX_AUDIO_FROM_BUFFER)) &&
            (timeNextTx <= QTime::currentTime()))
        {
            timeNextTx = timeNextTx.addMSecs(rxMsPacketSize);
            switch (txMode)
            {
            default:
            case RTP_TX_AUDIO_SILENCE:           fillPacketwithSilence(RTPpacket); break;
            case RTP_TX_AUDIO_FROM_BUFFER:       fillPacketfromBuffer(RTPpacket);  break;
            }
            StreamOut(RTPpacket);
        }

        SendWaitingDtmf();
        CheckSendStatistics();        
    }

    StopTxRx();
    CloseSocket();
    if (pJitter)
        delete pJitter;
    if (Codec)
        delete Codec;
    if (ToneToSpk != 0)
        delete ToneToSpk;
        
    if ((pollLoop != 0) && ((sleepMs/pollLoop)>30))
        cout << "Mythphone: \"sleep 10000\" is sleeping for more than 30ms; please report\n";
}

void rtp::rtpVideoThreadWorker()
{
    OpenSocket();
    eventCond = new QWaitCondition();
    rtpListener *videoListener = new rtpListener(rtpSocket, eventCond);

    while(!killRtpThread)
    {
        // Suspend for events
        eventCond->wait();

        if (killRtpThread)
            break;

        StreamInVideo();
        transmitQueuedVideo();

        CheckSendStatistics();        
    }

    delete videoListener;
    delete eventCond;
    eventCond = 0;

    if (videoToTx)
    {
        freeVideoBuffer(videoToTx);
        videoToTx = 0;
    }

    VIDEOBUFFER *buf;
    while ((buf = rxedVideoFrames.take(0)) != 0)
    {
        freeVideoBuffer(buf);
    }

    CloseSocket();
    if (pJitter)
        delete pJitter;
    if (Codec)
        delete Codec;
}

void rtp::rtpInitialise()
{
    rtpSocket             = 0;
    rxMsPacketSize        = 20;
    rxPCMSamplesPerPacket = rxMsPacketSize * PCM_SAMPLES_PER_MS;
    txMsPacketSize        = 20;
    txPCMSamplesPerPacket = txMsPacketSize*PCM_SAMPLES_PER_MS;
    SpkJitter             = 5; // Size of the jitter buffer * (rxMsPacketSize/2); so 5=50ms for 20ms packet size
    SpeakerOn             = false;
    MicrophoneOn          = false;
    speakerFd             = -1;
    microphoneFd          = -1;
    txSequenceNumber      = 1; //udp packet sequence number  
    txTimeStamp	          = 0;  
    txBuffer              = 0;
    lastDtmfTimestamp     = 0;
    dtmfIn                = "";
    dtmfOut               = "";
    recBuffer             = 0;
    recBufferLen          = 0;
    recBufferMaxLen       = 0;
    rxFirstFrame          = true;
    spkLowThreshold       = (rxPCMSamplesPerPacket*sizeof(short));
    spkSeenData           = false;
    spkUnderrunCount      = 0;
    oobError              = false;
    micMuted              = false;

    ToneToSpk = 0;
    ToneToSpkSamples = 0;
    ToneToSpkPlayed = 0;

    pkIn = 0;
    pkOut = 0;
    pkOutDrop = 0;
    pkMissed = 0;
    pkLate = 0;
    pkInDisc = 0;
    bytesIn = 0;
    bytesOut = 0;
    bytesToSpeaker = 0;
    framesIn = 0;
    framesOut = 0;
    framesOutDiscarded = 0;
    micPower = 0;
    spkPower = 0;
    spkInBuffer = 0;
    
    timeNextStatistics = QTime::currentTime().addSecs(RTP_STATS_INTERVAL);
    timeLastStatistics = QTime::currentTime();


    pJitter = new Jitter();
    //pJitter->Debug();

    if (videoPayload != -1)
    {
        Codec = 0;
        rtpMPT = videoPayload;
    }
    else
    {
        if (audioPayload == RTP_PAYLOAD_G711U)
            Codec = new g711ulaw();
        else if (audioPayload == RTP_PAYLOAD_G711A)
            Codec = new g711alaw();
#ifdef VA_G729
        else if (audioPayload == RTP_PAYLOAD_G729)
            Codec = new g729();
#endif
        else if (audioPayload == RTP_PAYLOAD_GSM)
            Codec = new gsmCodec();
        else 
        {
            cerr << "Unknown audio payload " << audioPayload << endl;
            audioPayload = RTP_PAYLOAD_G711U;
            Codec = new g711ulaw();
        }

        rtpMPT = audioPayload;
    }
    rtpMarker = 0;
}


#ifndef WIN32

void rtp::StartTxRx()
{
    if (rtpSocket == 0)
    {
        cerr << "Cannot start RTP spk/mic, socket not opened\n";
        return;
    }

    // Open the audio devices
    if ((rxMode == RTP_RX_AUDIO_TO_SPEAKER) && (txMode == RTP_TX_AUDIO_FROM_MICROPHONE) && (spkDevice == micDevice))
        microphoneFd = speakerFd = OpenAudioDevice(spkDevice, O_RDWR);    
    else
    {
        if (rxMode == RTP_RX_AUDIO_TO_SPEAKER)
            speakerFd = OpenAudioDevice(spkDevice, O_WRONLY);    

        if ((txMode == RTP_TX_AUDIO_FROM_MICROPHONE) && (micDevice != "None"))
            microphoneFd = OpenAudioDevice(micDevice, O_RDONLY);    
    }

    if (speakerFd != -1)
    {
        PlayoutDelay = SpkJitter;
        PlayLen = 0;
        memset(SilenceBuffer, 0, sizeof(SilenceBuffer));
        SilenceLen = rxPCMSamplesPerPacket * sizeof(short);
        rxTimestamp = 0;
        rxSeqNum = 0;
        rxFirstFrame = true;
        SpeakerOn = true;
    }

    if (microphoneFd != -1)
    {
        txSequenceNumber = 1;
        txTimeStamp	= 0;  
        MicrophoneOn = true;
    }
    else
        txMode = RTP_TX_AUDIO_SILENCE;
}


int rtp::OpenAudioDevice(QString devName, int mode)
{
    int fd = open(devName, mode, 0);
    if (fd == -1)
    {
        cerr << "Cannot open device " << devName << endl;
        return -1;
    }                  

    // Set Full Duplex operation
    /*if (ioctl(fd, SNDCTL_DSP_SETDUPLEX, 0) == -1)
    {
        cerr << "Error setting audio driver duplex\n";
        close(fd);
        return -1;
    }*/

    int format = AFMT_S16_LE;//AFMT_MU_LAW;
    if (ioctl(fd, SNDCTL_DSP_SETFMT, &format) == -1)
    {
        cerr << "Error setting audio driver format\n";
        close(fd);
        return -1;
    }

    int channels = 1;
    if (ioctl(fd, SNDCTL_DSP_CHANNELS, &channels) == -1)
    {
        cerr << "Error setting audio driver num-channels\n";
        close(fd);
        return -1;
    }

    int speed = 8000; // 8KHz
    if (ioctl(fd, SNDCTL_DSP_SPEED, &speed) == -1)
    {
        cerr << "Error setting audio driver speed\n";
        close(fd);
        return -1;
    }

    if ((format != AFMT_S16_LE/*AFMT_MU_LAW*/) || (channels != 1) || (speed != 8000))
    {
        cerr << "Error setting audio driver; " << format << ", " << channels << ", " << speed << endl;
        close(fd);
        return -1;
    }

    uint frag_size = 0x7FFF0007; // unlimited number of fragments; fragment size=128 bytes (ok for most RTP sample sizes)
    if (ioctl(fd, SNDCTL_DSP_SETFRAGMENT, &frag_size) == -1)
    {
        cerr << "Error setting audio fragment size\n";
        close(fd);
        return -1;
    }

    int flags;
    if ((flags = fcntl(fd, F_GETFL, 0)) > 0) 
    {
        flags &= O_NDELAY;
        fcntl(fd, F_SETFL, flags);
    }

/*    audio_buf_info info;
    if ((ioctl(fd, SNDCTL_DSP_GETBLKSIZE, &frag_size) == -1) ||
        (ioctl(fd, SNDCTL_DSP_GETOSPACE, &info) == -1))
    {
        cerr << "Error getting audio driver fragment info\n";
        close(fd);
        return -1;
    }*/
    //cout << "Frag size " << frag_size << " Fragments " << info.fragments << " Ftotal " << info.fragstotal << endl;
    return fd;
}


void rtp::StopTxRx()
{
    SpeakerOn = false;
    MicrophoneOn = false;

    if (speakerFd > 0)
        close(speakerFd);
    if ((microphoneFd != speakerFd) && (microphoneFd > 0))
        close(microphoneFd);

    speakerFd = -1;
    microphoneFd = -1;
}

#else

void rtp::StartTxRx()
{
    if (rtpSocket == 0)
    {
        cerr << "Cannot start RTP spk/mic, socket not opened\n";
        return;
    }

    MicDevice = SpeakerDevice = WAVE_MAPPER;
    WAVEOUTCAPS AudioCap;
    int numAudioDevs = waveOutGetNumDevs();
    for (int i=0; i<=numAudioDevs; i++)
    {
        MMRESULT err = waveOutGetDevCaps(i, &AudioCap, sizeof(AudioCap));
        if ((err == MMSYSERR_NOERROR) && (spkDevice == AudioCap.szPname))
            SpeakerDevice = i;
    }

    WAVEINCAPS AudioInCap;
    numAudioDevs = waveInGetNumDevs();
    for (i=0; i<=numAudioDevs; i++)
    {
        MMRESULT err = waveInGetDevCaps(i, &AudioInCap, sizeof(AudioInCap));
        if ((err == MMSYSERR_NOERROR) && (micDevice == AudioInCap.szPname))
            MicDevice = i;
    }

    StartTx();
    StartRx();
}

 
bool rtp::StartTx()
{
unsigned long dwResult;
WAVEFORMATEX wfx;
int b;
char TextBuffer[100];


    micCurrBuffer = 0;

    wfx.cbSize = 0;
    wfx.nAvgBytesPerSec = 16000;
    wfx.nBlockAlign = 2;
    wfx.nChannels = 1;
    wfx.nSamplesPerSec = 8000;
    wfx.wBitsPerSample = 16;
    wfx.wFormatTag = WAVE_FORMAT_PCM;

    // First just query to check there is a compatible Mic.
    if (dwResult = waveInOpen(0, MicDevice, &wfx, 0, 0, WAVE_FORMAT_QUERY))
    {
        //UpdateDebug("Error waveInOpen Query %d\r\n", dwResult);
        return FALSE;
    }

    // Now actually open the device
    dwResult = waveInOpen(&hMicrophone, MicDevice, &wfx, 0, 0, CALLBACK_NULL);
    if (dwResult)
    {
        //UpdateDebug("Error waveInOpen Query %d\r\n", dwResult);
        return FALSE;
    }

    // Inform the wave device of where to put the data
    for (b=0; b<NUM_MIC_BUFFERS; b++)
    {
        micBufferDescr[b].lpData = (LPSTR)(MicBuffer[b]);
        micBufferDescr[b].dwBufferLength = txPCMSamplesPerPacket * sizeof(short); 
        //micBufferDescr[b].dwBytesRecorded = 0L;
        micBufferDescr[b].dwFlags = 0L;
        //micBufferDescr[b].dwUser = 0L;
        //micBufferDescr[b].lpNext = ((b==(NUM_MIC_BUFFERS-1)) ? (&micBufferDescr[0]) : (&micBufferDescr[b+1]));
        if (dwResult = waveInPrepareHeader(hMicrophone, &(micBufferDescr[b]), sizeof(WAVEHDR)))
        {
            waveInGetErrorText(dwResult, TextBuffer, sizeof(TextBuffer));
            //UpdateDebug("Error waveInPrepareHeader %d %d\r\n%s\r\n", b, dwResult, TextBuffer);
            return FALSE;
        }
    }

    // Start recording the data into the buffer
    if (dwResult = waveInStart(hMicrophone))
    {
        //UpdateDebug("Error waveInStart %d\r\n", dwResult);
        return FALSE;
    }

    // Send a buffer to fill
    for (b=0; b<NUM_MIC_BUFFERS; b++)
    {
        if (dwResult = waveInAddBuffer(hMicrophone, &(micBufferDescr[b]), sizeof(WAVEHDR)))
        {
            //UpdateDebug("Error waveInAddBuffer %d\r\n", dwResult);
            return FALSE;
        }
    }

    MicrophoneOn = TRUE;

    //UpdateDebug("Microphone started\r\n");

    return TRUE;	
}

bool rtp::StartRx()
{
DWORD   dwResult;
int b;

    spkInBuffer = 0;
    rxTimestamp = SpkJitter;
    rxSeqNum = 0;
    rxFirstFrame = TRUE;

    // Playback the buffer through the speakers
    WAVEFORMATEX wfx;
    wfx.cbSize = 0;
    wfx.nAvgBytesPerSec = 16000;
    wfx.nBlockAlign = 2;
    wfx.nChannels = 1;
    wfx.nSamplesPerSec = 8000;
    wfx.wBitsPerSample = 16;
    wfx.wFormatTag = WAVE_FORMAT_PCM;

    if (dwResult = waveOutOpen(&hSpeaker, SpeakerDevice, &wfx, 0, 0L, WAVE_FORMAT_QUERY))
    {
        //UpdateDebug("waveOutOpen Query error %d", dwResult);
        return FALSE;
    }

    if (dwResult = waveOutOpen(&hSpeaker, SpeakerDevice, &wfx, 0, 0L, CALLBACK_NULL))
    {
        //UpdateDebug("waveOutOpen error %d", dwResult);
        return FALSE;
    }

    for (b=0; b<NUM_SPK_BUFFERS; b++)
    {
        spkBufferDescr[b].lpData = (LPSTR)(SpkBuffer[b]);
        spkBufferDescr[b].dwBufferLength = rxPCMSamplesPerPacket * sizeof(short);
        spkBufferDescr[b].dwFlags = 0L;
        spkBufferDescr[b].dwLoops = 0L;
        //spkBufferDescr[b].lpNext = ((b==(NUM_SPK_BUFFERS-1)) ? (&spkBufferDescr[0]) : (&spkBufferDescr[b+1]));

        if (dwResult = waveOutPrepareHeader(hSpeaker, &(spkBufferDescr[b]), sizeof(WAVEHDR)))
        {
            //UpdateDebug("waveOutPrepareHeader error %d", dwResult);
            return FALSE;
        }
    }

    //UpdateDebug("Speaker started\r\n");
    SpeakerOn = TRUE;

    return true;
}

void rtp::StopTxRx()
{
    StopTx();
    StopRx();
}

bool rtp::StopTx()
{
DWORD   dwResult;
char TextBuffer[70];
int b;

	MicrophoneOn = FALSE;

	if (dwResult = waveInReset(hMicrophone))
	{
		waveInGetErrorText(dwResult, TextBuffer, sizeof(TextBuffer));
		//UpdateDebug("Error waveInStop %d\r\n %s\r\n", dwResult, TextBuffer);
		return FALSE;
	}

	for (b=0; b<NUM_MIC_BUFFERS; b++)
	{
		if (dwResult = waveInUnprepareHeader(hMicrophone, &(micBufferDescr[0]), sizeof(WAVEHDR)))
		{
			waveInGetErrorText(dwResult, TextBuffer, sizeof(TextBuffer));
			//UpdateDebug("Error waveInUnprepareHeader %d %s\r\n", dwResult, TextBuffer);
			return FALSE;
		}
	}

	if (dwResult = waveInClose(hMicrophone))
	{
		waveInGetErrorText(dwResult, TextBuffer, sizeof(TextBuffer));
		//UpdateDebug("Error waveInStop %d\r\n %s\r\n", dwResult, TextBuffer);
		return FALSE;
	}

	return TRUE;
}


bool rtp::StopRx()
{
DWORD   dwResult;
int b;
char TextBuffer[100];

    SpeakerOn = FALSE;

    if (dwResult = waveOutReset(hSpeaker))
    {
        waveOutGetErrorText(dwResult, TextBuffer, sizeof(TextBuffer));
        //UpdateDebug("Error waveInStop %d\r\n %s\r\n", dwResult, TextBuffer);
        return FALSE;
    }

    for (b=0; b<NUM_SPK_BUFFERS; b++)
    {
        if (dwResult = waveOutUnprepareHeader(hSpeaker, &(spkBufferDescr[0]), sizeof(WAVEHDR)))
        {
            waveOutGetErrorText(dwResult, TextBuffer, sizeof(TextBuffer));
            //UpdateDebug("Error waveOutUnprepareHeader %d %s\r\n", dwResult, TextBuffer);
            return FALSE;
        }
    }

    if (dwResult = waveOutClose(hSpeaker))
    {
        waveOutGetErrorText(dwResult, TextBuffer, sizeof(TextBuffer));
        //UpdateDebug("Error waveOutClose %d\r\n %s\r\n", dwResult, TextBuffer);
        return FALSE;
    }

    return TRUE;	
}

#endif


void rtp::Debug(QString dbg)
{
#ifdef WIN32
    QString now = (QTime::currentTime()).toString("hh:mm:ss.zzz");
    if (eventWindow)
        QApplication::postEvent(eventWindow, new RtpEvent(RtpEvent::RtpDebugEv, now + " " + dbg));
#else
    cout << dbg;
#endif
}

void rtp::CheckSendStatistics()
{
    QTime now = QTime::currentTime();
    if (timeNextStatistics <= now)
    {
        int statsMsPeriod = timeLastStatistics.msecsTo(now);
        timeLastStatistics = now;
        timeNextStatistics = now.addSecs(RTP_STATS_INTERVAL);
        if (eventWindow)
            QApplication::postEvent(eventWindow, 
                        new RtpEvent(RtpEvent::RtpStatisticsEv, this, now, statsMsPeriod,
                                     pkIn, pkOut, pkMissed, pkLate, pkInDisc, pkOutDrop, 
                                     bytesIn, bytesOut, bytesToSpeaker, framesIn, framesOut, 
                                     framesInDiscarded, framesOutDiscarded));
    }
}

void rtp::OpenSocket()
{
    rtpSocket = new QSocketDevice (QSocketDevice::Datagram);
    rtpSocket->setBlocking(false);
    rtpSocket->setSendBufferSize(49152);
    rtpSocket->setReceiveBufferSize(49152);

#ifndef WIN32
    QString ifName = gContext->GetSetting("SipBindInterface");
    struct ifreq ifreq;
    strcpy(ifreq.ifr_name, ifName);
    if (ioctl(rtpSocket->socket(), SIOCGIFADDR, &ifreq) != 0)
    {
        cerr << "Failed to find network interface " << ifName << endl;
        delete rtpSocket;
        rtpSocket = 0;
        return;
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
        delete rtpSocket;
        rtpSocket = 0;
        return;
    }
    QHostAddress myIP; 
    for (int ifTotal=0; hostAddr->h_addr_list[ifTotal] != 0; ifTotal++)
        ;
    if (ifNum >= ifTotal)
        ifNum = 0;
    myIP.setAddress(htonl(((struct in_addr *)hostAddr->h_addr_list[ifNum])->S_un.S_addr));
#endif

    if (!rtpSocket->bind(myIP, myPort))
    {
        cerr << "Failed to bind for RTP connection " << myIP.toString() << endl;
        delete rtpSocket;
        rtpSocket = 0;
    }
}


void rtp::CloseSocket()
{
    if (rtpSocket)
    {
        rtpSocket->close();
        delete rtpSocket;
        rtpSocket = 0;
    }
}

bool rtp::isSpeakerHungry()
{
    if (rxMode == RTP_RX_AUDIO_TO_SPEAKER)
    {
        if (!SpeakerOn)
            return false;    

#ifndef WIN32
        int bytesQueued;
        audio_buf_info info;
        ioctl(speakerFd, SNDCTL_DSP_GETODELAY, &bytesQueued);
        ioctl(speakerFd, SNDCTL_DSP_GETOSPACE, &info);
    
        if (bytesQueued > 0)
            spkSeenData = true;

        // Never return true if it will result in the speaker blocking
        if (info.bytes <= (int)(rxPCMSamplesPerPacket*sizeof(short)))
            return false;

        // Always push packets from the jitter buffer into the Speaker buffer 
        // if the correct packet is available
        if (pJitter->isPacketQueued(rxSeqNum))
            return true;

        // Right packet not waiting for us - keep waiting unless the Speaker is going to 
        // underrun, in which case we will have to abandon the late/lost packet
        if (bytesQueued > spkLowThreshold)
            return false;

        // Ok; so right packet is not sat waiting, and Speaker is hungry.  If the speaker has ran down to
        // zero, i.e. underrun, flag this condition. Check for false alerts.
        // Only look for underruns if ... speaker has no data left to play, but has been receiving data,
        // and there IS data queued in the jitter buffer
        if ((bytesQueued == 0) && spkSeenData && pJitter->AnyData() && (++spkUnderrunCount > 3))
        {
            spkUnderrunCount = 0;
            // Increase speaker driver buffer since we are not servicing it
            // fast enough, up to an arbitary limit
            if (spkLowThreshold < (int)(6*(rxPCMSamplesPerPacket*sizeof(short))))
                spkLowThreshold += (rxPCMSamplesPerPacket*sizeof(short));
//            cout << "Excessive speaker underrun, adjusting spk buffer to " << spkLowThreshold << endl;
            //pJitter->Debug();
        }

#else
        // Windows -- just check if there is a free buffer    
        if ((spkBufferDescr[spkInBuffer].dwFlags & WHDR_INQUEUE) != 0)
            return false;

#endif
    
    }

    // Note - when receiving audio to a buffer; this will effectively
    // remove all jitter buffers by always looking hungry for rxed
    // packets. Ideally we should run off a clock instead
    return true;
}

bool rtp::isMicrophoneData()
{
#ifndef WIN32
    audio_buf_info info;
    ioctl(microphoneFd, SNDCTL_DSP_GETISPACE, &info);
    if (info.bytes > (int)(txPCMSamplesPerPacket*sizeof(short)))
        return true;
#else
    if ((micBufferDescr[micCurrBuffer].dwFlags & WHDR_DONE) != 0)
        return true;
#endif
    return false;
}

void rtp::PlayToneToSpeaker(short *tone, int Samples)
{
    if (SpeakerOn && (rxMode == RTP_RX_AUDIO_TO_SPEAKER) && (ToneToSpk == 0))
    {
        ToneToSpk = new short[Samples];
        memcpy(ToneToSpk, tone, Samples*sizeof(short));
        ToneToSpkSamples = Samples;
        ToneToSpkPlayed = 0;
    }
}

void rtp::AddToneToAudio(short *buffer, int Samples)
{
    if (ToneToSpk != 0)
    {
        int s = QMIN(Samples, ToneToSpkSamples);
        for (int c=0; c<s; c++)
            buffer[c] += ToneToSpk[ToneToSpkPlayed+c];
        ToneToSpkPlayed += s;
        ToneToSpkSamples -= s;
        if (ToneToSpkSamples == 0)
        {
            delete ToneToSpk;
            ToneToSpk = 0;
        }
    }
}


void rtp::Transmit(int ms)
{
    rtpMutex.lock();
    if (txBuffer)
       cerr << "Don't tell me to transmit something whilst I'm already busy\n";
    else
    {
        int Samples = ms * PCM_SAMPLES_PER_MS;
        txBuffer = new short[Samples+txPCMSamplesPerPacket]; // Increase space to multiples of full packets
        memset(txBuffer, 0, (Samples+txPCMSamplesPerPacket)*2);
        txBufferLen=Samples;
        txBufferPtr=0;
        txMode = RTP_TX_AUDIO_FROM_BUFFER;
    }
    rtpMutex.unlock();
}

void rtp::Transmit(short *pcmBuffer, int Samples)
{
    if (pcmBuffer && (Samples > 0))
    {
        rtpMutex.lock();
        if (txBuffer)
           cerr << "Don't tell me to transmit something whilst I'm already busy\n";
        else
        {
            txBuffer = new short[Samples+txPCMSamplesPerPacket]; // Increase space to multiples of full packets
            memcpy(txBuffer, pcmBuffer, Samples*sizeof(short));
            memset(txBuffer+Samples, 0, txPCMSamplesPerPacket*2);
            txBufferLen=Samples;
            txBufferPtr=0;
            txMode = RTP_TX_AUDIO_FROM_BUFFER;
        }
        rtpMutex.unlock();
    }
}

void rtp::Record(short *pcmBuffer, int Samples)
{
    rtpMutex.lock();
    if (recBuffer)
       cerr << "Don't tell me to record something whilst I'm already busy\n";
    else
    {
        recBuffer = pcmBuffer;
        recBufferMaxLen=Samples;
        recBufferLen=0;
        rxMode = RTP_RX_AUDIO_TO_BUFFER;
    }
    rtpMutex.unlock();
}

void rtp::StreamInAudio()
{
    RTPPACKET rtpDump;
    RTPPACKET *JBuf;
    bool tryAgain;

    if (rtpSocket)
    {
        do
        {
            tryAgain = false; // We keep going until we empty the socket

            // Get a buffer from the Jitter buffer to put the packet in
            if ((JBuf = pJitter->GetJBuffer()) != 0)
            {
                JBuf->len = rtpSocket->readBlock((char *)&JBuf->RtpVPXCC, sizeof(RTPPACKET));
                if (JBuf->len > 0)
                {
                    bytesIn += (JBuf->len + UDP_HEADER_SIZE);
                    tryAgain = true;
                    if (PAYLOAD(JBuf) == rtpMPT)
                    {
                        pkIn++;
                        JBuf->RtpSequenceNumber = ntohs(JBuf->RtpSequenceNumber);
                        JBuf->RtpTimeStamp = ntohl(JBuf->RtpTimeStamp);
                        if (rxFirstFrame)
                        {
                            rxFirstFrame = FALSE;
                            rxSeqNum = JBuf->RtpSequenceNumber;
                        }
                        if (PKLATE(rxSeqNum, JBuf->RtpSequenceNumber))
                        {
                            cout << "Packet arrived too late to play, try increasing jitter buffer\n";
                            pJitter->FreeJBuffer(JBuf);
                            pkLate++;
                        }
                        else
                            pJitter->InsertJBuffer(JBuf);
                    }
                    else if (PAYLOAD(JBuf) == dtmfPayload) 
                    {
                        tryAgain = true; // Force us to get another frame since this one is additional
                        HandleRxDTMF(JBuf);
                        if (PKLATE(rxSeqNum, JBuf->RtpSequenceNumber))
                            pJitter->FreeJBuffer(JBuf);
                        else
                            pJitter->InsertDTMF(JBuf); // Do this just so seq-numbers stay intact, it gets discarded later
                    }
                    else
                    {
                        if (PAYLOAD(JBuf) != RTP_PAYLOAD_COMF_NOISE) 
                            cerr << "Received Invalid Payload " << (int)JBuf->RtpMPT << "\n";
                        else
                            cout << "Received Comfort Noise Payload\n";
                        pJitter->FreeJBuffer(JBuf);
                    }
                }
                else // No received frames, free the buffer
                    pJitter->FreeJBuffer(JBuf);
            } 

            // No free buffers, still get the data from the socket but dump it. Unlikely to recover from this by
            // ourselves so we really need to discard all queued frames and reset the receiver
            else
            {
                rtpSocket->readBlock((char *)&rtpDump.RtpVPXCC, sizeof(RTPPACKET));
                if (!oobError)
                {
                    cerr << "Dumping received RTP frame, no free buffers; rx-mode " << rxMode << "; tx-mode " << txMode << endl;
                    pJitter->Debug();
                    oobError = true;
                }
            }
        } while (tryAgain);

        // Check size of Jitter buffer, make sure it doesn't grow too big
        //pJitter->Debug();
    }
}

void rtp::PlayOutAudio()
{
    bool tryAgain;
    int mLen, m, reason;

    if (rtpSocket)
    {
        // Implement a playout de-jitter delay
        if (PlayoutDelay > 0)
        {
            PlayoutDelay--;
            return;
        }

        // Now process buffers from the Jitter Buffer
        do
        {
            tryAgain = false;
            RTPPACKET *JBuf = pJitter->DequeueJBuffer(rxSeqNum, reason);
            switch (reason)
            {
            case JB_REASON_OK:
                ++rxSeqNum;
                mLen = JBuf->len - RTP_HEADER_SIZE;
                if ((rxMode == RTP_RX_AUDIO_TO_SPEAKER) && SpeakerOn)
                {
                    PlayLen = Codec->Decode(JBuf->RtpData, SpkBuffer[spkInBuffer], mLen, spkPower);
                    AddToneToAudio(SpkBuffer[spkInBuffer], PlayLen/sizeof(short));
#ifndef WIN32
                    m = write(speakerFd, (uchar *)SpkBuffer[spkInBuffer], PlayLen);
#else
                    if (waveOutWrite(hSpeaker, &(spkBufferDescr[spkInBuffer]), sizeof(WAVEHDR)))
                    {
                        //UpdateDebug("waveOutWrite error %d", dwResult);
                        //return;
                    }
                    int NextInBuffer = (spkInBuffer+1)%NUM_SPK_BUFFERS;
                    spkInBuffer = NextInBuffer;
#endif
                    bytesToSpeaker += m;
                }
                else if (rxMode == RTP_RX_AUDIO_TO_BUFFER)
                {
                    PlayLen = Codec->Decode(JBuf->RtpData, SpkBuffer[spkInBuffer], mLen, spkPower);
                    recordInPacket(SpkBuffer[spkInBuffer], PlayLen);
                    if (DTMFFilter)
                    {
                        QChar dtmf = DTMFFilter->process(SpkBuffer[spkInBuffer], PlayLen/sizeof(short));
                        if (dtmf)
                        {
                            rtpMutex.lock();
                            dtmfIn.append(dtmf);
                            rtpMutex.unlock();
                        }
                    }
                }
                else // rxMode is RTP_RX_AUDIO_DISCARD
                {
                    if (DTMFFilter)
                    {
                        PlayLen = Codec->Decode(JBuf->RtpData, SpkBuffer[spkInBuffer], mLen, spkPower);
                        QChar dtmf = DTMFFilter->process(SpkBuffer[spkInBuffer], PlayLen/sizeof(short));
                        if (dtmf)
                        {
                            rtpMutex.lock();
                            dtmfIn.append(dtmf);
                            rtpMutex.unlock();
                        }
                    }
                }
                rxTimestamp += mLen;
                pJitter->FreeJBuffer(JBuf);
                break;

            case JB_REASON_DUPLICATE: // This should not happen; but it does, especially with DTMF frames!
                if (JBuf != 0)
                    pJitter->FreeJBuffer(JBuf);
                tryAgain = true;
                break;

            case JB_REASON_DTMF:
                ++rxSeqNum;
                pJitter->FreeJBuffer(JBuf);
                tryAgain = true;
                break;

            case JB_REASON_MISSING: // This may just be because we are putting frames into the driver too early, but no way to tell
                rxSeqNum++;
                memset(SilenceBuffer, 0, sizeof(SilenceBuffer));
                SilenceLen = rxPCMSamplesPerPacket * sizeof(short);
                if ((rxMode == RTP_RX_AUDIO_TO_SPEAKER) && SpeakerOn)
                {
                    AddToneToAudio(SilenceBuffer, SilenceLen/sizeof(short));
                    m = write(speakerFd, (uchar *)SilenceBuffer, SilenceLen); 
                    bytesToSpeaker += m;
                }
                else if (rxMode == RTP_RX_AUDIO_TO_BUFFER)                
                {
                    recordInPacket(SilenceBuffer, SilenceLen);
                }
                pkMissed++;
                break;

            case JB_REASON_EMPTY: // nothing to do, just hope the driver playout buffer is full (since we can't tell!)
                break;
            case JB_REASON_SEQERR:
            default:
//                cerr << "Something funny happened with the seq numbers, should reset them & start again\n";
                break;
            }
        } while (tryAgain);
    }
}

void rtp::StreamInVideo()
{
    RTPPACKET *JBuf;
    int mLen, reason;
    bool MarketBitSet = false;

    if (rtpSocket)
    {
        // Get a buffer from the Jitter buffer to put the packet in
        while (((JBuf = pJitter->GetJBuffer()) != 0) && 
               ((JBuf->len = rtpSocket->readBlock((char *)&JBuf->RtpVPXCC, sizeof(RTPPACKET))) > 0))
        {
            bytesIn += (JBuf->len + UDP_HEADER_SIZE);
            if (PAYLOAD(JBuf) == rtpMPT)
            {
                if (JBuf->RtpMPT & RTP_PAYLOAD_MARKER_BIT)
                {
                    MarketBitSet = true;
                    framesIn++;
                }
                pkIn++;
                JBuf->RtpSequenceNumber = ntohs(JBuf->RtpSequenceNumber);
                JBuf->RtpTimeStamp = ntohl(JBuf->RtpTimeStamp);
                if (rxFirstFrame)
                {
                    rxFirstFrame = FALSE;
                    videoFrameFirstSeqNum = rxSeqNum = JBuf->RtpSequenceNumber;
                }
                if (JBuf->RtpSequenceNumber < videoFrameFirstSeqNum)
                {
                    cout << "Packet arrived too late to play, try increasing jitter buffer\n";
                    pJitter->FreeJBuffer(JBuf);
                    pkLate++;
                }
                else
                    pJitter->InsertJBuffer(JBuf);
            }
            else
            {
                cerr << "Received Invalid Payload " << (int)JBuf->RtpMPT << "\n";
                pJitter->FreeJBuffer(JBuf);
            }
        }

        if (JBuf == 0)
        {
            // No free buffers, abort
            cerr << "No free buffers, aborting network read\n";
        }
        else if (JBuf->len <= 0)
        {
            // Got a buffer but no received frames, free the buffer
            pJitter->FreeJBuffer(JBuf);
        } 
		


        // Currently, whilst we buffer frames until the final one, we use receipt of the final frame
        // to cause processing of all the received buffers. So any mis-orderering will cause problems!
        // This should hopefully be flagged by the "VIDEOPKLATE" check above so we will know to fix it!
        if (MarketBitSet)
        {
            // Check if we have all packets in the sequence up until the marker
            int vidLen = pJitter->GotAllBufsInFrame(rxSeqNum, sizeof(H263_RFC2190_HDR));
            if (vidLen == 0) 
            {
                ushort valid, missing;
                pJitter->CountMissingPackets(rxSeqNum, valid, missing);
                cout << "RTP Dropping video frame: Lost Packet\n";
                rxSeqNum = pJitter->DumpAllJBuffers(true) + 1;
                framesInDiscarded++;
                pkMissed += missing;
                pkInDisc += valid;
            }
            else
            {
                VIDEOBUFFER *picture = getVideoBuffer(vidLen);
                if (picture)
                {
                    int pictureIndex = 0;
                    bool markerSetOnLastPacket = false;
                    picture->w = picture->h = 0;
    
                    // Concatenate received IP packets into a picture buffer, checking we have all we parts
                    while ((JBuf = pJitter->DequeueJBuffer(rxSeqNum, reason)) != 0)
                    {
                        ++rxSeqNum;
                        mLen = JBuf->len - RTP_HEADER_SIZE - sizeof(H263_RFC2190_HDR);
                        rxTimestamp += mLen;
                        pictureIndex = appendVideoPacket(picture, pictureIndex, JBuf, mLen);
                        if (JBuf->RtpMPT & RTP_PAYLOAD_MARKER_BIT)
                        {
                            markerSetOnLastPacket = true;
                        }
                        if (picture->w == 0)
                        {
                            H263_RFC2190_HDR *h263Hdr = (H263_RFC2190_HDR *)JBuf->RtpData;
                            switch (H263HDR_GETSZ(h263Hdr->h263hdr))
                            {
                            case H263_SRC_4CIF:  picture->w = 704; picture->h = 576; break;
                            default:
                            case H263_SRC_CIF:   picture->w = 352; picture->h = 288; break;
                            case H263_SRC_QCIF:  picture->w = 176; picture->h = 144; break;
                            case H263_SRC_SQCIF: picture->w = 128; picture->h = 96;  break;
                            }
                        }
                        pJitter->FreeJBuffer(JBuf);
                    }

                    // Check rxed frame was not too big
                    if (pictureIndex > (int)sizeof(picture->video))
                    {
                        cout << "SIP: Received video frame size " << pictureIndex << "; too big for buffer\n";
                        freeVideoBuffer(picture);
                        framesInDiscarded++;
                        picture = 0;
                    }
    
                    // Now pass the received picture up to the higher layer. If the last packet has the marker bit set
                    // then we have received a full pictures worth of packets.
                    else if (markerSetOnLastPacket)
                    {
                        //cout << "Received frame length " << pictureIndex << " bytes\n";
                        picture->len = pictureIndex;
    
                        // Pass received picture to app
                        rtpMutex.lock();
                        if (rxedVideoFrames.count() < 3)    // Limit no of buffes tied up queueing to app
                        {
                            rxedVideoFrames.append(picture);
                            rtpMutex.unlock();
                        }
                        else
                        {
                            rtpMutex.unlock();
                            freeVideoBuffer(picture);
                            framesInDiscarded++;
                            cout << "Discarding frame, app consuming too slowly\n";
                        }
                        if (eventWindow)
                            QApplication::postEvent(eventWindow, new RtpEvent(RtpEvent::RxVideoFrame));
                        picture = 0;
                    }
                    else
                    {
                        // We didn't get the whole frame, so dump all buffered packets
                        cout << "RTP Dropping video frame: ";
                        switch (reason)
                        {
                        case JB_REASON_DUPLICATE: 
                            cout << "Duplicate\n";
                            break;
                        case JB_REASON_DTMF:
                            break;
                            cout << "DTMF\n";
                        case JB_REASON_MISSING: 
                            cout << "Missed Packets\n";
                            pkMissed++;
                            break;
                        case JB_REASON_EMPTY: 
                            cout << "Empty\n";
                            break;
                        case JB_REASON_SEQERR:
                            cout << "Sequence Error\n";
                            break;
                        default:
                            cout << "Unknown\n";
                            break;
                        }
                        rxSeqNum = pJitter->DumpAllJBuffers(true) + 1;
                        freeVideoBuffer(picture);
                        picture = 0;
                    }
                }
                else
                {
                    cout << "No buffers for video frame, dropping\n";
                    rxSeqNum = pJitter->DumpAllJBuffers(true) + 1;
                    framesInDiscarded++;
                }
            }   
            videoFrameFirstSeqNum = rxSeqNum;
        }
    }
}

int rtp::appendVideoPacket(VIDEOBUFFER *picture, int curLen, RTPPACKET *JBuf, int mLen)
{
    if ((curLen + mLen) <= (int)sizeof(picture->video))
    {
        H263_RFC2190_HDR *h263Hdr = (H263_RFC2190_HDR *)JBuf->RtpData;
        int bitOffset = H263HDR_GETSBIT(h263Hdr->h263hdr);
        if ((bitOffset == 0) || (curLen == 0))
        {
            memcpy(&picture->video[curLen], JBuf->RtpData+sizeof(H263_RFC2190_HDR), mLen);
            curLen += mLen;
        }
        else
        {
            uchar mask = (0xFF >> bitOffset) << bitOffset;
            picture->video[curLen-1] &= mask; // Keep most sig bits from last frame
            picture->video[curLen-1] |= (*(JBuf->RtpData+sizeof(H263_RFC2190_HDR)) & (~mask));
            memcpy(&picture->video[curLen], JBuf->RtpData+sizeof(H263_RFC2190_HDR)+1, mLen-1);
            curLen += mLen-1;
        }
    }
    return curLen;
}

void rtp::initVideoBuffers(int Num)
{
    while (Num-- > 0)
        FreeVideoBufferQ.append(new VIDEOBUFFER);
}
 
VIDEOBUFFER *rtp::getVideoBuffer(int len)
{
    if ((len==0) || (len <= MAX_VIDEO_LEN) && (!killRtpThread)) // len parameter, is passed, should be checked against buffer sizes
    {
        VIDEOBUFFER *buf;
        rtpMutex.lock();
        buf = FreeVideoBufferQ.take(0);
        rtpMutex.unlock();
        return buf;
    }
    cerr << "Received video picture size " << len << " too big for preallocated buffer size " << MAX_VIDEO_LEN << endl;
    return 0;
}

void rtp::freeVideoBuffer(VIDEOBUFFER *Buf)
{
    rtpMutex.lock();
    FreeVideoBufferQ.append(Buf);
    rtpMutex.unlock();
}

void rtp::destroyVideoBuffers()
{
    VIDEOBUFFER *buf = FreeVideoBufferQ.first();
    while (buf)
    {
        FreeVideoBufferQ.remove();
        delete buf;
        buf = FreeVideoBufferQ.current();
    }
}


void rtp::recordInPacket(short *data, int dataBytes)
{
    rtpMutex.lock();
    if (recBuffer)
    {
        int bufferLeft = (recBufferMaxLen-recBufferLen)*sizeof(short);
        int recBytes = QMIN(dataBytes, bufferLeft);
        memcpy(&recBuffer[recBufferLen], data, recBytes);
        recBufferLen+=(recBytes/sizeof(short));
        if (recBufferLen == recBufferMaxLen)
        {
            // Don't overwrite the actual length recorded, it is used later
            recBuffer = 0;
            rxMode = RTP_RX_AUDIO_DISCARD;
        }
    }
    else
        rxMode = RTP_RX_AUDIO_DISCARD;
    rtpMutex.unlock();
}

void rtp::HandleRxDTMF(RTPPACKET *RTPpacket)
{
    DTMF_RFC2833 *dtmf = (DTMF_RFC2833 *)RTPpacket->RtpData;
    RTPpacket->RtpSequenceNumber = ntohs(RTPpacket->RtpSequenceNumber);
    RTPpacket->RtpTimeStamp = ntohl(RTPpacket->RtpTimeStamp);

    // Check if it is a NEW or REPEATED DTMF character
    if (RTPpacket->RtpTimeStamp != lastDtmfTimestamp)
    {
        lastDtmfTimestamp = RTPpacket->RtpTimeStamp;
        rtpMutex.lock();
        dtmfIn.append(DTMF2CHAR(dtmf->dtmfDigit));
        cout << "Received DTMF digit " << dtmfIn << endl;
        rtpMutex.unlock();
    }
}

void rtp::SendWaitingDtmf()
{
    if ((dtmfPayload != -1) && (rtpSocket))
    {
        QChar digit = ' ';
        rtpMutex.lock();
        if (dtmfOut.length() > 0)
        {
            digit = dtmfOut[0];
            dtmfOut.remove(0,1);
        }
        rtpMutex.unlock();

        if (digit != ' ')
        {
            //cout << "Sending DTMF digit " << digit << endl;
            RTPPACKET dtmfPacket;
            DTMF_RFC2833 *dtmf = (DTMF_RFC2833 *)(dtmfPacket.RtpData);
            
            dtmf->dtmfDigit = CHAR2DTMF(digit);
            dtmf->dtmfERVolume = 0x0A | RTP_DTMF_EBIT; // Volume=10; E-bit set indicating end of event
            dtmf->dtmfDuration = htons(0x0500); // Duration = 16ms

            txSequenceNumber += 1; // Increment seq-num; don't increment timestamp
            dtmfPacket.RtpVPXCC = 128;
            dtmfPacket.RtpMPT = dtmfPayload | RTP_PAYLOAD_MARKER_BIT; // Set for 1st tx of a digit, clear for retransmissions
            dtmfPacket.RtpSequenceNumber = htons(txSequenceNumber);
            dtmfPacket.RtpTimeStamp = htonl(txTimeStamp);
            dtmfPacket.RtpSourceID = 0x666;    
            
            rtpSocket->writeBlock((char *)&dtmfPacket.RtpVPXCC, RTP_HEADER_SIZE+sizeof(DTMF_RFC2833), yourIP, yourPort);
        }
    }
}


// Packetisation as per RFC 2190 Mode A
void rtp::transmitQueuedVideo()
{
    // Currently we only allow one video frame outstanding between the app and the RTP stack. This should be
    // ok as its only meant to produce 10-30 frames / second, and we should consume much quicker
    rtpMutex.lock();
    VIDEOBUFFER *queuedVideo = videoToTx;
    videoToTx = 0;
    rtpMutex.unlock();

    if (queuedVideo)
    {
        if ((pTxShaper) && (!pTxShaper->OkToSend()))
        {
            cout << "Dropped video frame bceause shaper says so\n";
            freeVideoBuffer(queuedVideo);
            return;
        }
    
        framesOut++;
        
        RTPPACKET videoPacket;
        uchar *v = queuedVideo->video;
        int queuedLen = queuedVideo->len;

        txTimeStamp += 25000; // TODO --- fix this, this is a guessed-at value
    
        videoPacket.RtpVPXCC = 128;
        videoPacket.RtpMPT = videoPayload;
        videoPacket.RtpTimeStamp = htonl(txTimeStamp);
        videoPacket.RtpSourceID = 0x666;    
        H263_RFC2190_HDR *h263Hdr = (H263_RFC2190_HDR *)videoPacket.RtpData;
        switch (queuedVideo->w)
        {
        case 704: h263Hdr->h263hdr = H263HDR(H263_SRC_4CIF); break;
        default:
        case 352: h263Hdr->h263hdr = H263HDR(H263_SRC_CIF); break;
        case 176: h263Hdr->h263hdr = H263HDR(H263_SRC_QCIF); break;
        case 128: h263Hdr->h263hdr = H263HDR(H263_SRC_SQCIF); break;
        }

        while (queuedLen > 0)
        {
            txSequenceNumber += 1; // Increment seq-num; don't increment timestamp
            videoPacket.RtpSequenceNumber = htons(txSequenceNumber);
    
            uint pkLen = queuedLen;
            if (pkLen > H263SPACE)
                pkLen = H263SPACE;
    
            memcpy(videoPacket.RtpData+sizeof(H263_RFC2190_HDR), v, pkLen);
            v += pkLen;
            queuedLen -= pkLen;
            
            if (queuedLen == 0)
                videoPacket.RtpMPT |= RTP_PAYLOAD_MARKER_BIT;  // Last packet has Marker bit set as per RFC 2190
    
            if (rtpSocket)
            {
                if (rtpSocket->writeBlock((char *)&videoPacket.RtpVPXCC, RTP_HEADER_SIZE+sizeof(H263_RFC2190_HDR)+pkLen, yourIP, yourPort) == -1)
                    pkOutDrop++;
                else
                {
                    pkOut++;
                    int sentBytes = (UDP_HEADER_SIZE+RTP_HEADER_SIZE+sizeof(H263_RFC2190_HDR)+pkLen);
                    bytesOut += sentBytes;
                    if (pTxShaper)
                        pTxShaper->Send(sentBytes);
                }
            }
        }

        freeVideoBuffer(queuedVideo);
    }
}


void rtp::StreamOut(void* pData, int nLen)
{
    RTPPACKET RTPpacket;
    memcpy(RTPpacket.RtpData, pData, nLen);
    RTPpacket.len = nLen;
    StreamOut(RTPpacket);
}

void rtp::StreamOut(RTPPACKET &RTPpacket)
{
    if (rtpSocket)
    {
        txSequenceNumber += 1;
        txTimeStamp += txPCMSamplesPerPacket;

        RTPpacket.RtpVPXCC = 128;
        RTPpacket.RtpMPT = rtpMPT | rtpMarker;
        rtpMarker = 0;
        RTPpacket.RtpSequenceNumber = htons(txSequenceNumber);
        RTPpacket.RtpTimeStamp = htonl(txTimeStamp);
        RTPpacket.RtpSourceID = 0x666;    
            
        // as long as we are only doing one stream any hard
        // coded value will do, they must be unique for each stream

        if (rtpSocket->writeBlock((char *)&RTPpacket.RtpVPXCC, RTPpacket.len+RTP_HEADER_SIZE, yourIP, yourPort) == -1)
            pkOutDrop++;
        else
        {
            bytesOut += (UDP_HEADER_SIZE+RTPpacket.len+RTP_HEADER_SIZE);
            pkOut++;
        }
    }
}


void rtp::fillPacketwithSilence(RTPPACKET &RTPpacket)
{
    RTPpacket.len = Codec->Silence(RTPpacket.RtpData, txMsPacketSize);
}

bool rtp::fillPacketfromMic(RTPPACKET &RTPpacket)
{
    int gain=0;
    if (MicrophoneOn)
    {
        short buffer[MAX_DECOMP_AUDIO_SAMPLES];
#ifndef WIN32
        int len = read(microphoneFd, (char *)buffer, txPCMSamplesPerPacket*sizeof(short));
#else
        int len = txPCMSamplesPerPacket*sizeof(short);
        if ((micBufferDescr[micCurrBuffer].dwFlags & WHDR_DONE) != 0)
        {
            memcpy(buffer, MicBuffer[micCurrBuffer], txPCMSamplesPerPacket*sizeof(short));
            int NextBuffer = ((micCurrBuffer+1)%NUM_MIC_BUFFERS);
            waveInAddBuffer(hMicrophone, &(micBufferDescr[micCurrBuffer]), sizeof(WAVEHDR));
            micCurrBuffer = NextBuffer;
        }
        else
            return false;
#endif

        if (len != (int)(txPCMSamplesPerPacket*sizeof(short)))
        {
            fillPacketwithSilence(RTPpacket);
        }
        else if (micMuted)
            fillPacketwithSilence(RTPpacket);
        else
            RTPpacket.len = Codec->Encode(buffer, RTPpacket.RtpData, txPCMSamplesPerPacket, micPower, gain);
    }
    else
        fillPacketwithSilence(RTPpacket);
    return true;
}

void rtp::fillPacketfromBuffer(RTPPACKET &RTPpacket)
{
    rtpMutex.lock();
    if (txBuffer == 0)
    {
        fillPacketwithSilence(RTPpacket); 
        txMode = RTP_TX_AUDIO_SILENCE;
        cerr << "No buffer to playout, changing to playing silence\n";
    }
    else 
    {
        RTPpacket.len = Codec->Encode(txBuffer+txBufferPtr, RTPpacket.RtpData, txPCMSamplesPerPacket, micPower, 0);
        txBufferPtr += txPCMSamplesPerPacket;
        if (txBufferPtr >= txBufferLen) 
        {
            delete txBuffer;
            txBuffer = 0;
            txMode = RTP_TX_AUDIO_SILENCE;
        }
    }
    rtpMutex.unlock();
}



////////////////////////////////////////////////////////////////////////////////////////////////////
//                                     TRANSMIT SHAPER
////////////////////////////////////////////////////////////////////////////////////////////////////

TxShaper::TxShaper(int bw, int period, int granularity)
{
    txGranularity = granularity;
    historySize = period/granularity;
    txHistory = new int[historySize];
    for (int cnt=0; cnt<historySize; cnt++)
        txHistory[cnt]=0;
    txWindowTotal = 0;
    maxBandwidth = bw;
    itTail = 0;
    itHead = 0;

    timestamp.start();
    timeLastFlush = 0;
    timeLastSend = 0;
}

TxShaper::~TxShaper()
{
    delete txHistory;
}

bool TxShaper::OkToSend()
{
    flushHistory();
    return (txWindowTotal < maxBandwidth);
}

void TxShaper::Send(int Bytes)
{
    flushHistory();
    int timeThisSend = timestamp.elapsed();
    itHead += ((timeThisSend - timeLastSend) / txGranularity);
    itHead %= historySize;
    txHistory[itHead] += Bytes;    
    txWindowTotal += Bytes;
    timeLastSend = timeThisSend;
}

void TxShaper::flushHistory()
{
    int timeNow = timestamp.elapsed();
    for (int i=timeLastFlush; i<timeNow; i+=txGranularity)
    {
        txWindowTotal -= txHistory[itTail];
        txHistory[itTail++] = 0;
        if (itTail>=historySize)
            itTail = 0;
    }
    timeLastFlush = timeNow;
}



////////////////////////////////////////////////////////////////////////////////////////////////////
//                                     JITTER BUFFER
////////////////////////////////////////////////////////////////////////////////////////////////////

Jitter::Jitter():QPtrList<RTPPACKET>()
{
    // Make a linked list of all the free buffers
    for (int i=0; i<JITTERQ_SIZE; i++)
        FreeJitterQ.append(new RTPPACKET);
}


Jitter::~Jitter()
{
    // Free up all the linked FREE buffers
    RTPPACKET *buf = FreeJitterQ.first();
    while (buf)
    {
        FreeJitterQ.remove();
        delete buf;
        buf = FreeJitterQ.current();
    }
        
    // Free up all the linked in-use buffers
    buf = first();
    while (buf)
    {
        remove();
        delete buf;
        buf = current();
    }
}


RTPPACKET *Jitter::GetJBuffer()
{
    return (FreeJitterQ.take(0));
}


void Jitter::FreeJBuffer(RTPPACKET *Buf)
{
    FreeJitterQ.append(Buf);
}


void Jitter::InsertDTMF(RTPPACKET *Buffer)
{
    Buffer->len = 0; // Mark so we know its a DTMF frame later
    InsertJBuffer(Buffer);
}


void Jitter::InsertJBuffer(RTPPACKET *Buffer)
{
    if (count() == 0)
        append(Buffer);
    else
    {
        // Common case, packets in right order, place packet at back
        RTPPACKET *latest = getLast();
        if (latest->RtpSequenceNumber < Buffer->RtpSequenceNumber) 
            append(Buffer);

        // Packet misordering occured.             
        else
        {
            RTPPACKET *head = first();
            cout << "Packet misordering; got " << Buffer->RtpSequenceNumber << ", head " << head->RtpSequenceNumber << ", tail " << latest->RtpSequenceNumber << endl;
            inSort(Buffer);
        }
    }
}


int Jitter::compareItems(QPtrCollection::Item s1, QPtrCollection::Item s2)
{
    RTPPACKET *r1 = (RTPPACKET *)s1;
    RTPPACKET *r2 = (RTPPACKET *)s2;
		return (r1->RtpSequenceNumber - r2->RtpSequenceNumber);
}


RTPPACKET *Jitter::DequeueJBuffer(ushort seqNum, int &reason)
{
    RTPPACKET *head = first();

    // Normal case - first queued buffer is the one we want
    if ((head != 0) && (head->RtpSequenceNumber == seqNum))
    {
        remove(); 
        reason = JB_REASON_OK;
        if (head->len == 0) // Marked DTMF frames with a len of zero earlier
            reason = JB_REASON_DTMF;
        return head;
    }

    // Otherwise nothing to return; but see if anything odd had happened & report / recover
    if (head == 0)
        reason = JB_REASON_EMPTY;

    else if (head->RtpSequenceNumber == seqNum-1)
    {
        reason = JB_REASON_DUPLICATE;
        remove(); 
    }
    else if ((head->RtpSequenceNumber < seqNum) || (head->RtpSequenceNumber > seqNum+50))
        reason = JB_REASON_SEQERR;
    else
        reason = JB_REASON_MISSING;

    return 0;
}

void Jitter::CountMissingPackets(ushort seq, ushort &cntValid, ushort &cntMissing)
{
    RTPPACKET *head = first();
    cntValid=0;
    cntMissing=0;

    while (head != 0) 
    {
        if (head->RtpSequenceNumber == seq) 
            cntValid++;
        else if ((head->RtpSequenceNumber > seq) && 
                 (head->RtpSequenceNumber < seq+100))
        {
            cntMissing += (head->RtpSequenceNumber - seq);
        }
        else
        {
            cout << "Big gap in RTP sequence numbers, possibly restarted\n";
            cntMissing++; // No way to know how many were actually missed
        }
        seq = head->RtpSequenceNumber + 1;
        head = next();
    }
}

int Jitter::GotAllBufsInFrame(ushort seq, int offset)
{
    RTPPACKET *head = first();
    int len=0;

    while ((head != 0) && (head->RtpSequenceNumber == seq++))
    {
        len += (head->len - RTP_HEADER_SIZE - offset);
        if (head->RtpMPT & RTP_PAYLOAD_MARKER_BIT)
            return len;
        head = next();
    }
    return 0;
}

bool Jitter::isPacketQueued(ushort Seq)
{
    RTPPACKET *head = first();

    // Does the head packet match the sequence number
    if ((head != 0) && (head->RtpSequenceNumber == Seq))
        return true;
    return false;
}

int Jitter::DumpAllJBuffers(bool StopAtMarkerBit)
{
    bool MarkerFound = false;
    int lastRxSeqNum = 0;

    RTPPACKET *head = first();
    while ((head) && (!MarkerFound))
    {
        remove();
        lastRxSeqNum = head->RtpSequenceNumber;
        if (StopAtMarkerBit && (head->RtpMPT & RTP_PAYLOAD_MARKER_BIT))
            MarkerFound = true;
        FreeJBuffer(head);
        head = current();
    }

    return lastRxSeqNum;
}

void Jitter::Debug()
{
    cout << "Jitter buffers " << count() << " queued " << FreeJitterQ.count() << " free\n";
}



////////////////////////////////////////////////////////////////////////////////////////////////////
//                                 CODEC -- Base Class
////////////////////////////////////////////////////////////////////////////////////////////////////

codec::codec()
{
}

codec::~codec()
{
}

int codec::Encode(short *In, uchar *Out, int Samples, short &maxPower, int gain)
{
    (void)maxPower;
    (void)gain;
    memcpy(Out, (char *)In, Samples);
    return Samples;
}

int codec::Decode(uchar *In, short *Out, int Len, short &maxPower)
{
    (void)maxPower;
    memcpy((char *)Out, In, Len);
    return Len;
}

int codec::Silence(uchar *out, int ms)
{
    int len = ms * PCM_SAMPLES_PER_MS;
    memset(out, 0, len);
    return len;
}

QString codec::WhoAreYou()
{
    return "NO-CODEC";
}


