#ifndef NUPPELVIDEORECORDER
#define NUPPELVIDEORECORDER

#include <qstring.h>
#include <vector>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>

#include <linux/videodev.h>

#ifdef MMX
#undef MMX
#define MMXBLAH
#endif
#include <lame/lame.h>
#ifdef MMXBLAH
#define MMX
#endif

#include "RTjpegN.h"
#include "minilzo.h"
#include "format.h"
#include "RingBuffer.h"

extern "C" {
#include "../libavcodec/avcodec.h"
#include "filter.h"
}

using namespace std;

class NuppelVideoRecorder
{
 public:
    NuppelVideoRecorder();
   ~NuppelVideoRecorder();

    void SetRingBuffer(RingBuffer *rbuf) { ringBuffer = rbuf; }
    void SetAsPIP(void) { pip = true; }
    
    void SetCodec(QString desiredcodec) { codec = desiredcodec; }
    void SetRTJpegMotionLevels(int lM1, int lM2) { M1 = lM1; M2 = lM2; }
    void SetRTJpegQuality(int quality) { Q = quality; }
    void SetMP4TargetBitrate(int rate) { targetbitrate = rate; }
    void SetMP4ScaleBitrate(int scale) { scalebitrate = scale;}
    void SetMP4Quality(int max, int min, int diff) 
                       { maxquality = max; minquality = min; qualdiff = diff; }
    void SetResolution(int width, int height) { w = width; h = height; }
    void SetAudioSampleRate(int rate) { audio_samplerate = rate; }   
    void SetAudioCompression(bool compress) { compressaudio = compress; }
 
    void SetFilename(QString filename) { sfilename = filename; }
    void SetAudioDevice(QString device) { audiodevice = device; }
    void SetVideoDevice(QString device) { videodevice = device; }
    void SetTVFormat(QString tvformat);
    void SetHMJPGQuality(int quality) { hmjpg_quality = quality; }
    void SetHMJPGDecimation(int deci) { hmjpg_decimation = deci; }
    
    void Initialize(void);
    void StartRecording(void);
    void StopRecording(void) { encoding = false; } 
    
    void Pause(void) { paused = true; pausewritethread = true;
                       actuallypaused = audiopaused = mainpaused = false; }
    void Unpause(void) { paused = false; pausewritethread = false; }
    bool GetPause(void) { return (audiopaused && mainpaused && 
                                  actuallypaused); }
    
    bool IsRecording(void) { return recording; }
   
    long long GetFramesWritten(void) { return framesWritten; } 
    void ResetFramesWritten(void) { framesWritten = 0; }

    void SetMP3Quality(int quality) { mp3quality = quality; }

    int GetVideoFd(void) { return fd; }
    void Reset(void);

    float GetFrameRate(void) { return video_frame_rate; }
  
    void WriteHeader(bool todumpfile = false);

    void SetVideoFilters(QString &filters) { videoFilterList = filters; }

    void TransitionToFile(const QString &lfilename);
    void TransitionToRing(void);

    long long GetKeyframePosition(long long desired);


 protected:
    static void *WriteThread(void *param);
    static void *AudioThread(void *param);

    void doWriteThread(void);
    void doAudioThread(void);
    
 private:
    int AudioInit(void);
    void InitFilters(void);
    void InitBuffers(void);
    
    int SpawnChildren(void);
    void KillChildren(void);
    
    void BufferIt(unsigned char *buf, int len = -1);
    
    int CreateNuppelFile(void);

    void WriteVideo(unsigned char *buf, int len, int fnum, int timecode);
    void WriteAudio(unsigned char *buf, int fnum, int timecode);

    bool SetupAVCodec(void);

    void WriteSeekTable(bool todumpfile);

    void DoMJPEG();
    
    QString sfilename;
    bool encoding;
    
    int fd; // v4l input file handle
    signed char *strm;
    long dropped;
    unsigned int lf, tf;
    int M1, M2, Q;
    int w, h;
    int pid, pid2;
    int inputchannel;
    int compression;
    int compressaudio;
    unsigned long long audiobytes;
    int audio_channels; 
    int audio_bits;
    int audio_bytes_per_sample;
    int audio_samplerate; // rate we request from sounddevice
    int effectivedsp; // actual measured rate

    int ntsc;
    int quiet;
    int rawmode;
    int usebttv;
    struct video_audio origaudio;

    int mp3quality;
    char *mp3buf;
    int mp3buf_size;
    lame_global_flags *gf;

    RTjpeg *rtjc;

#define OUT_LEN (1024*1024 + 1024*1024 / 64 + 16 + 3)    
    lzo_byte out[OUT_LEN];
#define HEAP_ALLOC(var,size) \
    long __LZO_MMODEL var [ ((size) + (sizeof(long) - 1)) / sizeof(long) ]    
    HEAP_ALLOC(wrkmem, LZO1X_1_MEM_COMPRESS);

    QString audiodevice;
    QString videodevice;
 
    vector<struct vidbuffertype *> videobuffer;
    vector<struct audbuffertype *> audiobuffer;

    int act_video_encode;
    int act_video_buffer;

    int act_audio_encode;
    int act_audio_buffer;
    long long act_audio_sample;
    
    int video_buffer_count;
    int audio_buffer_count;

    long video_buffer_size;
    long audio_buffer_size;

    struct timeval stm;
    struct timezone tzone;

    bool childrenLive;

    pthread_t write_tid;
    pthread_t audio_tid;

    bool recording;

    RingBuffer *ringBuffer;
    bool weMadeBuffer;

    int keyframedist;
    vector<struct seektable_entry> *seektable;

    long long extendeddataOffset;

    long long framesWritten;
    double video_frame_rate;

    bool livetv;
    bool paused;
    bool pausewritethread;
    bool actuallypaused;
    bool audiopaused;
    bool mainpaused;
    
    int last_block;
    int firsttc;
    long int oldtc;
    int startnum;
    int frameofgop;
    int lasttimecode;
    int audio_behind;
    
    bool useavcodec;

    AVCodec *mpa_codec;
    AVCodecContext *mpa_ctx;
    AVPicture mpa_picture;

    int targetbitrate;
    int scalebitrate;
    int maxquality;
    int minquality;
    int qualdiff;
    QString codec;

    bool pip;

    QString videoFilterList;
    vector<VideoFilter *> videoFilters;

    PixelFormat picture_format;

    bool hardware_encode;
    int hmjpg_quality;
    int hmjpg_decimation;
};

#endif
