#ifndef NUPPELVIDEORECORDER
#define NUPPELVIDEORECORDER

#include <qstring.h>
#include <qmap.h>
#include <vector>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>

#ifdef MMX
#undef MMX
#define MMXBLAH
#endif
#include <lame/lame.h>
#ifdef MMXBLAH
#define MMX
#endif

#include "recorderbase.h"

#include "minilzo.h"
#include "format.h"

extern "C" {
#include "../libavcodec/avcodec.h"
#include "filter.h"
}

using namespace std;

struct video_audio;
struct VBIData;
struct cc;
class RTjpeg;
class RingBuffer;

struct MyFilterData
{
    int bShowDeinterlacedAreaOnly;
    int bBlend;
    // int iThresholdBlend; // here we start blending
    int iThreshold;         // here we start interpolating TODO FIXME
    int iEdgeDetect;
    int picsize;
    unsigned char *src;
};

class NuppelVideoRecorder : public RecorderBase
{
 public:
    NuppelVideoRecorder();
   ~NuppelVideoRecorder();

    void SetEncodingOption(const QString &opt, int value);
    void ChangeDeinterlacer(int deint_mode);   
 
    void Initialize(void);
    void StartRecording(void);
    void StopRecording(void); 
    
    void Pause(bool clear = true);
    void Unpause(void);
    bool GetPause(void); 
    
    bool IsRecording(void);
   
    long long GetFramesWritten(void); 

    int GetVideoFd(void);
    void Reset(void);

    void SetVideoFilters(QString &filters);

    void TransitionToFile(const QString &lfilename);
    void TransitionToRing(void);

    long long GetKeyframePosition(long long desired);
    void GetBlankFrameMap(QMap<long long, int> &blank_frame_map);

 protected:
    static void *WriteThread(void *param);
    static void *AudioThread(void *param);
    static void *VbiThread(void *param);

    void doWriteThread(void);
    void doAudioThread(void);
    void doVbiThread(void);
    
 private:
    int AudioInit(void);
    void InitBuffers(void);
    void InitFilters(void);   
 
    int SpawnChildren(void);
    void KillChildren(void);
    
    void BufferIt(unsigned char *buf, int len = -1);
    
    int CreateNuppelFile(void);
    void WriteHeader(bool todumpfile = false);

    void WriteVideo(Frame *frame);
    void WriteAudio(unsigned char *buf, int fnum, int timecode);
    void WriteText(unsigned char *buf, int len, int timecode, int pagenr);

    Frame * areaDeinterlace(unsigned char *yuvptr, int width, int height);
    Frame * GetField(struct vidbuffertype *vidbuf, bool top_field, 
                     bool interpolate);
    bool SetupAVCodec(void);

    void WriteSeekTable(bool todumpfile);

    void DoMJPEG();

    void FormatTeletextSubtitles(struct VBIData *vbidata);
    void FormatCC(struct ccsubtitle *subtitle, struct cc *cc, int data);
    
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

    int quiet;
    int rawmode;
    int usebttv;
    struct video_audio *origaudio;

    int mp3quality;
    char *mp3buf;
    int mp3buf_size;
    lame_global_flags *gf;

    QMap<long long, int> blank_frames;

    RTjpeg *rtjc;

#define OUT_LEN (1024*1024 + 1024*1024 / 64 + 16 + 3)    
    lzo_byte out[OUT_LEN];
#define HEAP_ALLOC(var,size) \
    long __LZO_MMODEL var [ ((size) + (sizeof(long) - 1)) / sizeof(long) ]    
    HEAP_ALLOC(wrkmem, LZO1X_1_MEM_COMPRESS);

    vector<struct vidbuffertype *> videobuffer;
    vector<struct audbuffertype *> audiobuffer;
    vector<struct txtbuffertype *> textbuffer;

    int act_video_encode;
    int act_video_buffer;

    int act_audio_encode;
    int act_audio_buffer;
    long long act_audio_sample;
   
    int act_text_encode;
    int act_text_buffer;
 
    int video_buffer_count;
    int audio_buffer_count;
    int text_buffer_count;

    long video_buffer_size;
    long audio_buffer_size;
    long text_buffer_size;

    struct timeval stm;
    struct timezone tzone;

    bool childrenLive;

    pthread_t write_tid;
    pthread_t audio_tid;
    pthread_t vbi_tid;

    bool recording;

    int keyframedist;
    vector<struct seektable_entry> *seektable;

    long long extendeddataOffset;

    long long framesWritten;

    bool livetv;
    bool paused;
    bool pausewritethread;
    bool actuallypaused;
    bool audiopaused;
    bool mainpaused;

    enum DeinterlaceMode {
        DEINTERLACE_NONE =0,
        DEINTERLACE_BOB,
        DEINTERLACE_BOB_FULLHEIGHT_COPY,
        DEINTERLACE_BOB_FULLHEIGHT_LINEAR_INTERPOLATION,
        DEINTERLACE_DISCARD_TOP,
        DEINTERLACE_DISCARD_BOTTOM,
        DEINTERLACE_AREA,
        DEINTERLACE_LAST
    };

    DeinterlaceMode deinterlace_mode;
    double framerate_multiplier;
    double height_multiplier;

    struct MyFilterData myfd;   
 
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
    AVFrame mpa_picture;

    int targetbitrate;
    int scalebitrate;
    int maxquality;
    int minquality;
    int qualdiff;
    int mp4opts;

    QString videoFilterList;
    vector<VideoFilter *> videoFilters;

    PixelFormat picture_format;

    bool hardware_encode;
    int hmjpg_quality;
    int hmjpg_hdecimation;
    int hmjpg_vdecimation;
    int hmjpg_maxw;

    bool cleartimeonpause;

    struct ccsubtitle subtitle;
};

#endif
