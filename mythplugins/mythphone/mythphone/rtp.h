#ifndef RTP_H_
#define RTP_H_

#include <qsocketdevice.h>
#include <qsqldatabase.h>
#include <qregexp.h>
#include <qtimer.h>
#include <qptrlist.h>
#include <qthread.h>
#include <qdatetime.h>



#define IP_MTU                    1460     // Standard MTU is 1500, less UDP header size of 40
#define TX_USER_STREAM_SIZE       4096
#define MINBYTESIZE               80
#define RTP_HEADER_SIZE           12
#define UDP_HEADER_SIZE           28
#define	MESSAGE_SIZE              80       // bytes to send per 10ms
#define	ULAW_BYTES_PER_MS         8        // bytes to send per 1ms
#define MAX_COMP_AUDIO_SIZE       256      // This would equate to 30ms sample size
#define MAX_DECOMP_AUDIO_SAMPLES  (MAX_COMP_AUDIO_SIZE) // CHANGE FOR HIGHER COMPRESSION CODECS; G.711 has same no. samples after decomp.
#define PCM_SAMPLES_PER_MS        8


class RtpEvent : public QCustomEvent
{
public:
    enum Type { RxVideoFrame = (QEvent::User + 300) };

    RtpEvent(Type t) : QCustomEvent(t) { }
    ~RtpEvent() {  }

};




typedef struct RTPPACKET
{
  int     len;                       // Not part of the RTP frame itself
	uchar	  RtpVPXCC;
	uchar	  RtpMPT;
	ushort  RtpSequenceNumber;
	ulong	  RtpTimeStamp;
	ulong   RtpSourceID;
	uchar	  RtpData[IP_MTU-RTP_HEADER_SIZE];
} RTPPACKET;

typedef struct 
{
    uchar  dtmfDigit;
    uchar  dtmfERVolume;
    short dtmfDuration;
} DTMF_RFC2833;

typedef struct 
{
    ulong h263hdr;
} H263_RFC2190_HDR;

#define H263HDR(s)              ((s)<<13)
#define H263HDR_GETSZ(h)        (((h)>>13) & 0x7)

#define H263_SRC_SQCIF          1
#define H263_SRC_QCIF           2
#define H263_SRC_CIF            3
#define H263_SRC_4CIF           4
#define H263_SRC_16CIF          5


#define MAX_VIDEO_LEN 256000
typedef struct VIDEOBUFFER
{
  int     len;
  int     w,h;
	uchar	  video[MAX_VIDEO_LEN];
} VIDEOBUFFER;

// Values for RTP Payload Type
#define RTP_PAYLOAD_G711U		    0x00
#define RTP_PAYLOAD_G711A    		0x08
#define RTP_PAYLOAD_COMF_NOISE  0x0D
#define RTP_PAYLOAD_G729		    0x12
#define RTP_PAYLOAD_MARKER_BIT	0x80
#define PAYLOAD(r)              (((r)->RtpMPT) & (~RTP_PAYLOAD_MARKER_BIT))
#define RTP_DTMF_EBIT           0x80
#define RTP_DTMF_VOLUMEMASK     0x3F
#define JITTERQ_SIZE	          512
#define PKLATE(c,r)             (((r)<(c)) && (((c)-(r))<32000))    // check if rxed seq-number is less than current but handle wrap
#define H263SPACE               (IP_MTU-RTP_HEADER_SIZE-sizeof(H263_RFC2190_HDR))

#define DTMF_STAR 10
#define DTMF_HASH 11
#define DTMF2CHAR(d) ((d)>DTMF_HASH ? '?' : ((d)==DTMF_STAR ? '*' : ((d) == DTMF_HASH ? '#' : ((d)+'0'))))
#define CHAR2DTMF(c) ((c)=='#' ? DTMF_HASH : ((c)=='*' ? DTMF_STAR : ((c)-'0')))



class codec
{
public:
    codec();
    virtual ~codec();
    virtual int Decode(uchar *In, short *out, int Len, short &maxPower);
    virtual int Encode(short *In, uchar *out, int Samples, short &maxPower, int gain);
    virtual int Silence(uchar *out, int ms);
    virtual QString WhoAreYou();
private:
};

#define JB_REASON_OK          0   // Buffer which matches seq-number returned
#define JB_REASON_EMPTY       1   // No buffers queued
#define JB_REASON_MISSING     2   // Buffers exist, but none which match your seq-num
#define JB_REASON_SEQERR      3   // Buffers exist, but their seq-numbers are nowhere near yours
#define JB_REASON_DTMF        4   // Buffer which matches seq-number contained DTMF
#define JB_REASON_DUPLICATE   5   // Got the same sequence number twice

class Jitter : public QPtrList<RTPPACKET>
{
public:
    Jitter();
    ~Jitter();
    RTPPACKET *	GetJBuffer();
    void		FreeJBuffer(RTPPACKET *Buf);
    void    InsertDTMF(RTPPACKET *Buffer);
    void		InsertJBuffer(RTPPACKET *Buffer);
    RTPPACKET *DequeueJBuffer(ushort seqNum, int &reason);  
    int     DumpAllJBuffers(bool StopAtMarkerBit);
    virtual int compareItems(QPtrCollection::Item s1, QPtrCollection::Item s2);
    bool AnyData() { return (count()>0); };
    bool isPacketQueued(ushort Seq);
    int GotAllBufsInFrame(ushort seq, int offset);
    void Debug();


private:
    QPtrList<RTPPACKET> FreeJitterQ;
};


enum rtpTxMode { RTP_TX_AUDIO_FROM_BUFFER=1, RTP_TX_AUDIO_FROM_MICROPHONE=2, RTP_TX_AUDIO_SILENCE=3, RTP_TX_VIDEO=4 };
enum rtpRxMode { RTP_RX_AUDIO_TO_BUFFER=1,   RTP_RX_AUDIO_TO_SPEAKER=2,      RTP_RX_AUDIO_DISCARD=3, RTP_RX_VIDEO=4 };

class rtp 
{

public:
    rtp(QObject *callingApp, int localPort, QString remoteIP, int remotePort, int mediaPay, int dtmfPay, QString micDev, QString spkDev, rtpTxMode txm=RTP_TX_AUDIO_FROM_MICROPHONE, rtpRxMode rxm=RTP_RX_AUDIO_TO_SPEAKER);
    ~rtp();
    void Transmit(short *pcmBuffer, int Samples);
    void Transmit(int ms);
    void Record(short *pcmBuffer, int Samples);
    void StopTransmitRecord() { rtpMutex.lock(); txMode=RTP_TX_AUDIO_SILENCE; rxMode=RTP_RX_AUDIO_DISCARD; if (txBuffer) delete txBuffer; txBuffer=0; recBuffer=0; recBufferMaxLen=0; rtpMutex.unlock(); };
    bool Finished()           { rtpMutex.lock(); bool b = ((txBuffer == 0) && (recBuffer == 0)); rtpMutex.unlock(); return b;};
    int  GetRecordSamples()   { rtpMutex.lock(); int s = recBufferLen; rtpMutex.unlock(); return s;};
    bool checkDtmf()          { rtpMutex.lock(); bool b=(dtmfIn[0] != 0); rtpMutex.unlock(); return b; }
    QString getDtmf()         { rtpMutex.lock(); QString s = dtmfIn; dtmfIn = ""; rtpMutex.unlock(); return s; }
    void sendDtmf(char d)     { rtpMutex.lock(); dtmfOut.append(d); rtpMutex.unlock(); }
    bool toggleMute()         { micMuted = !micMuted; return micMuted; }
    void getPower(short &m, short &s) { m = micPower; s = spkPower; micPower = 0; spkPower = 0; }
    void getRxStats(int &pIn, int &pMiss, int &pLt, int &bIn, int &bPlayed, int &bOut) { pIn = pkIn; pMiss = pkMissed; pLt = pkLate; bIn = bytesIn; bPlayed = bytesToSpeaker; bOut = bytesOut; }
    void getTxStats(int &pOut) { pOut = pkOut; }
    bool queueVideo(VIDEOBUFFER *v) { bool res=false; rtpMutex.lock(); if (videoToTx==0) {videoToTx=v; res=true; } rtpMutex.unlock(); return res; }
    VIDEOBUFFER *getRxedVideo()     { rtpMutex.lock(); VIDEOBUFFER *b=rxedVideoFrames.take(0); rtpMutex.unlock(); return b; }
    VIDEOBUFFER *getVideoBuffer(int len=0);
    void freeVideoBuffer(VIDEOBUFFER *Buf);
    void PlayToneToSpeaker(short *tone, int Samples);


private:
    static void *rtpThread(void *p);
    void rtpThreadWorker();
    void rtpAudioThreadWorker();
    void rtpVideoThreadWorker();
    void rtpInitialise();
    void StartTxRx();
    int  OpenAudioDevice(QString devName, int mode);
    void StopTxRx();
    void OpenSocket();
    void CloseSocket();
    void StreamInVideo();
    void StreamInAudio();
    void PlayOutAudio();
    bool isSpeakerHungry();
    bool isMicrophoneData();
    void recordInPacket(short *data, int dataBytes);
    void HandleRxDTMF(RTPPACKET *RTPpacket);
    void SendWaitingDtmf();
    void StreamOut(void* pData, int nLen);
    void StreamOut(RTPPACKET &RTPpacket);
    void fillPacketwithSilence(RTPPACKET &RTPpacket);
    void fillPacketfromMic(RTPPACKET &RTPpacket);
    void fillPacketfromBuffer(RTPPACKET &RTPpacket);
    void initVideoBuffers(int Num);
    void destroyVideoBuffers();
    void transmitQueuedVideo();
    void AddToneToAudio(short *buffer, int Samples);

    QObject *eventWindow;
    pthread_t rtpthread;
    QMutex rtpMutex;
    QSocketDevice *rtpSocket;
    codec   *Codec;
    Jitter *pJitter;
    int rxMsPacketSize;
    int txMsPacketSize;
    int rxPCMSamplesPerPacket;
    int txPCMSamplesPerPacket;
    int SpkJitter;
    bool SpeakerOn;
    bool MicrophoneOn;
    ulong rxTimestamp;
    ushort rxSeqNum;
    bool rxFirstFrame;
    ushort txSequenceNumber;
    ulong txTimeStamp;
    int PlayoutDelay;
    int speakerFd;
    int microphoneFd;
    short PlayBuffer[MAX_DECOMP_AUDIO_SAMPLES];
    short SilenceBuffer[MAX_DECOMP_AUDIO_SAMPLES];
    int PlayLen;
    int SilenceLen;
    uchar rtpMPT;
    uchar rtpMarker;
    QHostAddress yourIP;
    int myPort, yourPort;
    rtpTxMode txMode;
    rtpRxMode rxMode;
    QString micDevice;
    QString spkDevice;
    bool oobError;
    bool killRtpThread;
    short *txBuffer;
    int txBufferLen, txBufferPtr;
    ulong lastDtmfTimestamp;
    QString dtmfIn;
    QString dtmfOut;
    short *recBuffer;
    int recBufferLen, recBufferMaxLen;
    int audioPayload;
    int dtmfPayload;
    int spkLowThreshold;
    bool spkSeenData;
    int spkUnderrunCount;
    bool micMuted;

    short *ToneToSpk;
    int ToneToSpkSamples;
    int ToneToSpkPlayed;

    // Video
    int videoPayload;
    QPtrList<VIDEOBUFFER> FreeVideoBufferQ;
    QPtrList<VIDEOBUFFER> rxedVideoFrames;
    VIDEOBUFFER *videoToTx;
    int videoFrameFirstSeqNum;

    // Stats
    int pkIn;
    int pkOut;
    int pkMissed;
    int pkLate;
    int bytesIn;
    int bytesOut;
    int bytesToSpeaker;
    short micPower;
    short spkPower;
};




#endif
