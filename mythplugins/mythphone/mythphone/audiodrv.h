#ifndef AUDIODRV_H_
#define AUDIODRV_H_

#include <qtimer.h>
#include <qptrlist.h>
#include <qthread.h>
#include <qdatetime.h>

#ifdef WIN32
#include <windows.h>
#include <io.h>
#include <sstream>
#include <mmsystem.h>
#include <Dsound.h>
#else
#include <mythtv/audiooutput.h>
#endif


#define MAX_COMP_AUDIO_SIZE       320      // This would equate to 40ms sample size
#define MAX_DECOMP_AUDIO_SAMPLES  (MAX_COMP_AUDIO_SIZE) // CHANGE FOR HIGHER COMPRESSION CODECS; G.711 has same no. samples after decomp.
#define PCM_SAMPLES_PER_MS        8

// WIN32 WAVE driver constants
#define NUM_MIC_BUFFERS           32
#define MIC_BUFFER_SIZE           MAX_DECOMP_AUDIO_SAMPLES
#define NUM_SPK_BUFFERS           1
#define SPK_BUFFER_SIZE           (PCM_SAMPLES_PER_MS * 1000 * 10) 

// WIN32 DirectSound driver constants
#define DS_SPK_BUFFER_SIZE       (PCM_SAMPLES_PER_MS * 1000 * 10) 
#define DS_SPK_BUFFER_BYTES      (DS_SPK_BUFFER_SIZE*2)
#define DS_NUM_MIC_BUFFERS       32


class AudioDriver 
{
public:
    AudioDriver(int mCap) { micCaptureSamples = mCap;};
    virtual ~AudioDriver() {};
    virtual void StartSpeaker() {};
    virtual void Open() {};
    virtual void Close() {};
    virtual int  Write(short *, int) {return 0;};
    virtual int  WriteSilence(int samples);
    virtual int  msOutQueued() {return 0;};
    virtual int  samplesOutSpaceRemaining() {return 0;};
    virtual bool anyMicrophoneData() {return false;};
    virtual int  Read(short *, int) { return 0;};
protected:
    int micCaptureSamples;
};

#ifdef WIN32
class waveAudioDriver : public AudioDriver
{
public:
    waveAudioDriver(QString s, QString m, int mCap);
    ~waveAudioDriver();
    virtual void StartSpeaker();
    virtual void Open();
    virtual void Close();
    virtual int  Write(short *data, int samples);
    virtual int  msOutQueued();
    virtual int  samplesOutSpaceRemaining();
    virtual bool anyMicrophoneData();
    virtual int  Read(short *buffer, int maxSamples);
private:
    bool openSpeaker();
    bool openMicrophone();
    bool closeMicrophone();
    bool closeSpeaker();

//    HWND        hwndLast;
    int         MicDevice;
    int         SpeakerDevice;
    HWAVEIN     hMicrophone;
    WAVEHDR     micBufferDescr[NUM_MIC_BUFFERS];
    short       MicBuffer[NUM_MIC_BUFFERS][MIC_BUFFER_SIZE];
    int         micCurrBuffer;
    HWAVEOUT    hSpeaker;
    WAVEHDR     spkBufferDescr[NUM_SPK_BUFFERS];
    short       SpkBuffer[NUM_SPK_BUFFERS][SPK_BUFFER_SIZE];
    int         spkIndex;
    int         spkInBuffer;
};

class dsAudioDriver : public AudioDriver
{
public:
    dsAudioDriver(QString s, QString m, int mCap, HWND hMainWindow);
    ~dsAudioDriver();
    virtual void StartSpeaker();
    virtual void Open();
    virtual void Close();
    virtual int  Write(short *data, int samples);
    virtual int  msOutQueued();
    virtual int  samplesOutSpaceRemaining();
    virtual bool anyMicrophoneData();
    virtual int  Read(short *buffer, int maxSamples);
    void dsEnumerateCallback(LPGUID lpGUID, LPCTSTR lpszDesc, LPCTSTR lpszDrvName);
private:
    bool openSpeaker();
    bool openMicrophone();
    bool closeMicrophone();
    bool closeSpeaker();
    QString spkName;
    QString micName;
    GUID *spkGuid;
    GUID *micGuid;
    bool enumerateSpeaker;
    IDirectSound *spkDS;
    IDirectSoundCapture *micDS;
    IDirectSoundBuffer *dsSpkBuffer;
    IDirectSoundCaptureBuffer *dsMicBuffer;
    unsigned char *dsSpkMemory; // actual buffer
    int spkByteIndex;
    int lastPlayPos;
    int playBufferWraps;
    int lastReadPos;
    int micBufferBytes;
};
#endif

#ifndef WIN32
class ossAudioDriver : public AudioDriver
{
public:
    ossAudioDriver(QString s, QString m, int mCap);
    ~ossAudioDriver();
    virtual void StartSpeaker();
    virtual void Open();
    virtual void Close();
    virtual int  Write(short *data, int samples);
    virtual int  msOutQueued();
    virtual int  samplesOutSpaceRemaining();
    virtual bool anyMicrophoneData();
    virtual int  Read(short *buffer, int maxSamples);
private:
    int OpenAudioDevice(QString devName, int mode);
    int speakerFd;
    int microphoneFd;
    QString spkDevice;
    QString micDevice;
    bool readAnyData;
};
#endif

#ifndef WIN32
class mythAudioDriver : public AudioDriver
{
public:
    mythAudioDriver(QString s, QString m, int mCap);
    ~mythAudioDriver();
    virtual void StartSpeaker();
    virtual void Open();
    virtual void Close();
    virtual int  Write(short *data, int samples);
    virtual int  msOutQueued();
    virtual int  samplesOutSpaceRemaining();
    virtual bool anyMicrophoneData();
    virtual int  Read(short *buffer, int maxSamples);
private:
    int OpenAudioDevice(QString devName, int mode);
    AudioOutput *mythOutput;
    int microphoneFd;
    QString spkDevice;
    QString micDevice;
    bool readAnyData;
};
#endif


#endif
