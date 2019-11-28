#ifndef NUPPELVIDEORECORDER
#define NUPPELVIDEORECORDER

// C headers
#include <sys/time.h>
#ifdef MMX
#undef MMX
#define MMXBLAH
#endif
#include <lame/lame.h>
#ifdef MMXBLAH
#define MMX
#endif

#include "mythconfig.h"

#undef HAVE_AV_CONFIG_H
extern "C" {
#include "libavcodec/avcodec.h"
}

// C++ std headers
#include <cstdint>
#include <ctime>
#include <vector>
using namespace std;

// Qt headers
#include <QString>

// MythTV headers
#include "v4lrecorder.h"
#include "format.h"
#include "cc608decoder.h"
#include "lzo/lzo1x.h"
#include "mthread.h"
#include "mythframe.h"

#include "mythtvexp.h"

#define KEYFRAMEDIST   30

struct video_audio;
class RTjpeg;
class RingBuffer;
class ChannelBase;
class AudioInput;
class NuppelVideoRecorder;

class NVRWriteThread : public MThread
{
  public:
    explicit NVRWriteThread(NuppelVideoRecorder *parent) :
        MThread("NVRWrite"), m_parent(parent) {}
    virtual ~NVRWriteThread() { wait(); m_parent = nullptr; }
    void run(void) override; // MThread
  private:
    NuppelVideoRecorder *m_parent;
};

class NVRAudioThread : public MThread
{
  public:
    explicit NVRAudioThread(NuppelVideoRecorder *parent) :
        MThread("NVRAudio"), m_parent(parent) {}
    virtual ~NVRAudioThread() { wait(); m_parent = nullptr; }
    void run(void) override; // MThread
  private:
    NuppelVideoRecorder *m_parent;
};

class MTV_PUBLIC NuppelVideoRecorder : public V4LRecorder, public CC608Input
{
    friend class NVRWriteThread;
    friend class NVRAudioThread;
  public:
    NuppelVideoRecorder(TVRec *rec, ChannelBase *channel);
   ~NuppelVideoRecorder();

    void SetOption(const QString &opt, int value) override; // DTVRecorder
    void SetOption(const QString &name, const QString &value) override; // DTVRecorder

    void SetOptionsFromProfile(RecordingProfile *profile,
                               const QString &videodev,
                               const QString &audiodev,
                               const QString &vbidev) override; // DTVRecorder
 
    void Initialize(void) override; // DTVRecorder
    void run(void) override; // RecorderBase
    
    void Pause(bool clear = true) override; // RecorderBase
    bool IsPaused(bool holding_lock = false) const override; // RecorderBase
 
    bool IsRecording(void) override; // RecorderBase

    long long GetFramesWritten(void) override; // DTVRecorder

    bool Open(void);
    int GetVideoFd(void) override; // DTVRecorder
    void Reset(void) override; // DTVRecorder

    void SetVideoFilters(QString &filters) override; // DTVRecorder
    void SetTranscoding(bool value) { m_transcoding = value; };

    void ResetForNewFile(void) override; // DTVRecorder
    void FinishRecording(void) override; // DTVRecorder
    void StartNewFile(void) override; // RecorderBase

    // reencode stuff
    void StreamAllocate(void);
    void WriteHeader(void);
    void WriteSeekTable(void);
    void WriteKeyFrameAdjustTable(
        const vector<struct kfatable_entry> &kfa_table);
    void UpdateSeekTable(int frame_num, long offset = 0);

    bool SetupAVCodecVideo(void);
    void SetupRTjpeg(void);
    int AudioInit(bool skipdevice = false);
    void SetVideoAspect(float newAspect) {m_video_aspect = newAspect; };
    void WriteVideo(VideoFrame *frame, bool skipsync = false, 
                    bool forcekey = false);
    void WriteAudio(unsigned char *buf, int fnum, int timecode);
    void WriteText(unsigned char *buf, int len, int timecode, int pagenr);

    void SetNewVideoParams(double newaspect);

 protected:
    void doWriteThread(void);
    void doAudioThread(void);

 private:
    inline void WriteFrameheader(rtframeheader *fh);

    void WriteFileHeader(void);

    void InitBuffers(void);
    void ResizeVideoBuffers(void);

    bool MJPEGInit(void);
 
    void KillChildren(void);
    
    void BufferIt(unsigned char *buf, int len = -1, bool forcekey = false);
    
    int CreateNuppelFile(void);

    void ProbeV4L2(void);
    bool SetFormatV4L2(void);
    void DoV4L1(void);
    void DoV4L2(void);
    void DoMJPEG(void);

    void FormatTT(struct VBIData*) override; // V4LRecorder
    void FormatCC(uint code1, uint code2) override; // V4LRecorder
    void AddTextData(unsigned char*,int,int64_t,char) override; // CC608Input

    void UpdateResolutions(void);
    
    int                 m_fd                     {-1}; // v4l input file handle
    signed char        *m_strm                   {nullptr};
    unsigned int        m_lf                     {0};
    int                 m_tf                     {0};
    int                 m_M1                     {0};
    int                 m_M2                     {0};
    int                 m_Q                      {255};
    int                 m_width                  {352};
    int                 m_height                 {240};
    int                 m_pip_mode               {0};
    int                 m_compression            {1};
    bool                m_compressaudio          {true};
    AudioInput         *m_audio_device           {nullptr};
    unsigned long long  m_audiobytes             {0};
    int                 m_audio_channels         {2}; 
    int                 m_audio_bits             {16};
    int                 m_audio_bytes_per_sample {m_audio_channels * m_audio_bits / 8};
    int                 m_audio_samplerate       {44100}; // rate we request from sounddevice
    int                 m_effectivedsp           {0}; // actual measured rate

    int                 m_usebttv                {1};
    float               m_video_aspect           {1.33333F};

    bool                m_transcoding            {false};

    int                 m_mp3quality             {3};
    char               *m_mp3buf                 {nullptr};
    int                 m_mp3buf_size            {0};
    lame_global_flags  *m_gf                     {nullptr};

    RTjpeg             *m_rtjc                   {nullptr};

#define OUT_LEN (1024*1024 + 1024*1024 / 64 + 16 + 3)    
    lzo_byte            m_out[OUT_LEN] {};
#define HEAP_ALLOC(var,size) \
    long __LZO_MMODEL var [ ((size) + (sizeof(long) - 1)) / sizeof(long) ]    
    HEAP_ALLOC(wrkmem, LZO1X_1_MEM_COMPRESS);

    vector<struct vidbuffertype *> videobuffer;
    vector<struct audbuffertype *> audiobuffer;
    vector<struct txtbuffertype *> textbuffer;

    int                 m_act_video_encode       {0};
    int                 m_act_video_buffer       {0};

    int                 m_act_audio_encode       {0};
    int                 m_act_audio_buffer       {0};
    long long           m_act_audio_sample       {0};
   
    int                 m_act_text_encode        {0};
    int                 m_act_text_buffer        {0};
 
    int                 m_video_buffer_count     {0};
    int                 m_audio_buffer_count     {0};
    int                 m_text_buffer_count      {0};

    long                m_video_buffer_size      {0};
    long                m_audio_buffer_size      {0};
    long                m_text_buffer_size       {0};

    struct timeval      m_stm                    {0,0};
    struct timezone     m_tzone                  {0,0};

    NVRWriteThread     *m_write_thread           {nullptr};
    NVRAudioThread     *m_audio_thread           {nullptr};

    bool                m_recording              {false};

    int                 m_keyframedist           {KEYFRAMEDIST};
    vector<struct seektable_entry> *m_seektable  {nullptr};
    long long           m_lastPositionMapPos     {0};

    long long           m_extendeddataOffset     {0};

    long long           m_framesWritten          {0};

    bool                m_livetv                 {false};
    bool                m_writepaused            {false};
    bool                m_audiopaused            {false};
    bool                m_mainpaused             {false};

    double              m_framerate_multiplier   {1.0};
    double              m_height_multiplier      {1.0};

    int                 m_last_block             {0};
    int                 m_firsttc                {0};
    long int            m_oldtc                  {0};
    int                 m_startnum               {0};
    int                 m_frameofgop             {0};
    int                 m_lasttimecode           {0};
    int                 m_audio_behind           {0};
    
    bool                m_useavcodec             {false};

    AVCodec            *m_mpa_vidcodec           {nullptr};
    AVCodecContext     *m_mpa_vidctx             {nullptr};

    int                 m_targetbitrate          {2200};
    int                 m_scalebitrate           {1};
    int                 m_maxquality             {2};
    int                 m_minquality             {31};
    int                 m_qualdiff               {3};
    int                 m_mp4opts                {0};
    int                 m_mb_decision            {FF_MB_DECISION_SIMPLE};
    /// Number of threads to use for MPEG-2 and MPEG-4 encoding
    int                 m_encoding_thread_count  {1};

    VideoFrameType      m_inpixfmt               {FMT_YV12};
    AVPixelFormat       m_picture_format         {AV_PIX_FMT_YUV420P};
#ifdef USING_V4L2
    uint32_t            m_v4l2_pixelformat       {0};
#endif
    int                 m_w_out                  {0};
    int                 m_h_out                  {0};

    bool                m_hardware_encode        {false};
    int                 m_hmjpg_quality          {80};
    int                 m_hmjpg_hdecimation      {2};
    int                 m_hmjpg_vdecimation      {2};
    int                 m_hmjpg_maxw             {640};

    bool                m_cleartimeonpause       {false};

    bool                m_usingv4l2              {false};
    int                 m_channelfd              {-1};

    ChannelBase        *m_channelObj             {nullptr};

    bool                m_skip_btaudio           {false};

    bool                m_correct_bttv           {false};

    int                 m_volume                 {100};

    CC608Decoder       *m_ccd                    {nullptr};

    bool                m_go7007                 {false};
    bool                m_resetcapture           {false};
};

#endif
