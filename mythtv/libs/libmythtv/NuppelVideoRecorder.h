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

#include "commercial_skip.h"
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
class ChannelBase;
class FilterManager;
class FilterChain;

class NuppelVideoRecorder : public RecorderBase
{
 public:
    NuppelVideoRecorder(ChannelBase *channel);
   ~NuppelVideoRecorder();

    void SetOption(const QString &name, int value);
    void SetOption(const QString &name, const QString &value)
                  { RecorderBase::SetOption(name, value); }

    void SetOptionsFromProfile(RecordingProfile *profile,
                               const QString &videodev,
                               const QString &audiodev,
                               const QString &vbidev, int ispip);
 
    void Initialize(void);
    void StartRecording(void);
    void StopRecording(void); 
    
    void Pause(bool clear = true);
    void Unpause(void);
    bool GetPause(void); 
    void WaitForPause(void);   
 
    bool IsRecording(void);
    bool IsErrored(void);

    long long GetFramesWritten(void); 

    bool Open(void);
    int GetVideoFd(void);
    void Reset(void);

    void SetVideoFilters(QString &filters);
    void SetTranscoding(bool value) { transcoding = value; };

    long long GetKeyframePosition(long long desired);
    void GetBlankFrameMap(QMap<long long, int> &blank_frame_map);

    // reencode stuff
    void StreamAllocate(void);
    void WriteHeader(void);
    void WriteSeekTable(void);
    void WriteKeyFrameAdjustTable(QPtrList<struct kfatable_entry> *kfa_table);
    void UpdateSeekTable(int frame_num, bool update_db, long offset = 0);

    bool SetupAVCodec(void);
    void SetupRTjpeg(void);
    int AudioInit(bool skipdevice = false);
    void SetVideoAspect(float newAspect) {video_aspect = newAspect; };
    void WriteVideo(VideoFrame *frame, bool skipsync = false, 
                    bool forcekey = false);
    void WriteAudio(unsigned char *buf, int fnum, int timecode);
    void WriteText(unsigned char *buf, int len, int timecode, int pagenr);

 protected:
    static void *WriteThread(void *param);
    static void *AudioThread(void *param);
    static void *VbiThread(void *param);

    void doWriteThread(void);
    void doAudioThread(void);
    void doVbiThread(void);
    
 private:
    inline void NuppelVideoRecorder::WriteFrameheader(rtframeheader *fh);

    void InitBuffers(void);
    void InitFilters(void);   
    void ResizeVideoBuffers(void);

    int MJPEGInit(void);
 
    int SpawnChildren(void);
    void KillChildren(void);
    
    void BufferIt(unsigned char *buf, int len = -1);
    
    int CreateNuppelFile(void);

    void DoV4L2(void);
    void DoMJPEG(void);

    void FormatTeletextSubtitles(struct VBIData *vbidata);
    void FormatCC(struct cc *cc);
    void FormatCCField(struct cc *cc, int tc, int field);
    QChar CharCC(int code);
    void ResetCC(struct cc *cc, int mode);
    void BufferCC(struct cc *cc, int mode, int len, int clr);
    int NewRowCC(struct cc *cc, int mode, int len);
    
    bool encoding;
    
    int fd; // v4l input file handle
    signed char *strm;
    long dropped;
    unsigned int lf, tf;
    int M1, M2, Q;
    int w, h;
    int pip_mode;
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
    float video_aspect;

    CommDetect *commDetect;
    bool transcoding;

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
    bool errored;

    int keyframedist;
    vector<struct seektable_entry> *seektable;
    QMap<long long, long long> positionMap;
    QMap<long long, long long> positionMapDelta;

    long long extendeddataOffset;

    long long framesWritten;

    bool livetv;
    bool paused;
    bool pausewritethread;
    bool actuallypaused;
    bool audiopaused;
    bool mainpaused;

    double framerate_multiplier;
    double height_multiplier;

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
    int mb_decision;

    QString videoFilterList;
    FilterChain *videoFilters;
    FilterManager *FiltMan;

    VideoFrameType inpixfmt;
    PixelFormat picture_format;
    int w_out;
    int h_out;

    bool hardware_encode;
    int hmjpg_quality;
    int hmjpg_hdecimation;
    int hmjpg_vdecimation;
    int hmjpg_maxw;

    bool cleartimeonpause;

    bool usingv4l2;
    int channelfd;

    long long prev_bframe_save_pos;

    ChannelBase *channelObj;

    bool skip_btaudio;

    bool correct_bttv;

    int volume;
};

#endif
