#include <string>
#include <vector>
#include <linux/videodev.h>
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

#include "RTjpegN.h"
#include "minilzo.h"
#include "format.h"

using namespace std;

class NuppelVideoRecorder
{
 public:
    NuppelVideoRecorder();
   ~NuppelVideoRecorder();

    void SetMotionLevels(int lM1, int lM2) { M1 = lM1; M2 = lM2; }
    void SetQuality(int quality) { Q = quality; }
    void SetResolution(int width, int height) { w = width; h = height; }
    
    void SetFilename(char *filename) { sfilename = filename; }
    void SetAudioDevice(char *device) { audiodevice = device; }
    void SetVideoDevice(char *device) { videodevice = device; }
   
    void Initialize(void);
    void StartRecording(void);
 
 protected:
    static void *WriteThread(void *param);
    static void *AudioThread(void *param);

    void doWriteThread(void);
    void doAudioThread(void);
    
 private:
    int AudioInit(void);
    void InitBuffers(void);

    int SpawnChildren(void);
    void KillChildren(void);
    
    void BufferIt(unsigned char *buf);
    
    int CreateNuppelFile(void);

    void WriteVideo(unsigned char *buf, int fnum, int timecode);
    void WriteAudio(unsigned char *buf, int fnum, int timecode);

    string sfilename;
    bool encoding;
    
    int fd; // v4l input file handle
    int ostr;
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
    int effectivedsp;

    int ntsc;
    int quiet;
    int rawmode;
    int usebttv;
    struct video_audio origaudio;

    char *mp3buf;
    int mp3buf_size;
    lame_global_flags *gf;

    RTjpeg *rtjc;

#define OUT_LEN (1024*1024 + 1024*1024 / 64 + 16 + 3)    
    lzo_byte out[OUT_LEN];
#define HEAP_ALLOC(var,size) \
    long __LZO_MMODEL var [ ((size) + (sizeof(long) - 1)) / sizeof(long) ]    
    HEAP_ALLOC(wrkmem, LZO1X_1_MEM_COMPRESS);

    int shmid;
    char *sharedbuffer;

    string audiodevice;
    string videodevice;
    
    vector<struct vidbuffertype *> videobuffer;
    vector<struct audbuffertype *> audiobuffer;

    int act_video_encode;
    int act_video_buffer;

    int act_audio_encode;
    int act_audio_buffer;

    int video_buffer_count;
    int audio_buffer_count;

    long video_buffer_size;
    long audio_buffer_size;

    struct timeval now, anow, vnow, stm;
    struct timezone tzone;

    bool childrenLive;

    pthread_t write_tid;
    pthread_t audio_tid;
};
