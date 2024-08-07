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

// C++ std headers
#include <cstdint>
#include <ctime>
#include <vector>

// Qt headers
#include <QString>

// MythTV headers
#include "libmythbase/mthread.h"
#include "libmythbase/mythconfig.h"
#include "libmythtv/captions/cc608decoder.h"
#include "libmythtv/format.h"
#include "libmythtv/mythframe.h"
#include "libmythtv/mythtvexp.h"

#include "lzo/lzo1x.h"
#include "v4lrecorder.h"

#undef HAVE_AV_CONFIG_H
extern "C" {
#include "libavcodec/avcodec.h"
}

static constexpr int8_t KEYFRAMEDIST { 30 };

struct video_audio;
class RTjpeg;
class MythMediaBuffer;
class ChannelBase;
class AudioInput;
class NuppelVideoRecorder;

class NVRWriteThread : public MThread
{
  public:
    explicit NVRWriteThread(NuppelVideoRecorder *parent) :
        MThread("NVRWrite"), m_parent(parent) {}
    ~NVRWriteThread() override { wait(); m_parent = nullptr; }
    void run(void) override; // MThread
  private:
    NuppelVideoRecorder *m_parent;
};

class NVRAudioThread : public MThread
{
  public:
    explicit NVRAudioThread(NuppelVideoRecorder *parent) :
        MThread("NVRAudio"), m_parent(parent) {}
    ~NVRAudioThread() override { wait(); m_parent = nullptr; }
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
   ~NuppelVideoRecorder() override;

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
        const std::vector<struct kfatable_entry> &kfa_table);
    void UpdateSeekTable(int frame_num, long offset = 0);

    bool SetupAVCodecVideo(void);
    void SetupRTjpeg(void);
    int AudioInit(bool skipdevice = false);
    void SetVideoAspect(float newAspect) {m_videoAspect = newAspect; };
    void WriteVideo(MythVideoFrame *frame, bool skipsync = false, 
                    bool forcekey = false);
    void WriteAudio(unsigned char *buf, int fnum, std::chrono::milliseconds timecode);
    void WriteText(unsigned char *buf, int len, std::chrono::milliseconds timecode, int pagenr);

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
    void DoV4L2(void);

    void FormatTT(struct VBIData *vbidata) override; // V4LRecorder
    void FormatCC(uint code1, uint code2) override; // V4LRecorder
    void AddTextData(unsigned char *buf, int len, std::chrono::milliseconds timecode, char type) override; // CC608Input

    void UpdateResolutions(void);
    
    int                 m_fd                     {-1}; // v4l input file handle
    signed char        *m_strm                   {nullptr};
    unsigned int        m_lf                     {0};
    int                 m_tf                     {0};
    int                 m_m1                     {0};
    int                 m_m2                     {0};
    int                 m_q                      {255};
    int                 m_width                  {352};
    int                 m_height                 {240};
    int                 m_pipMode                {0};
    int                 m_compression            {1};
    bool                m_compressAudio          {true};
    AudioInput         *m_audioDevice            {nullptr};
    unsigned long long  m_audioBytes             {0};
    int                 m_audioChannels          {2};
    int                 m_audioBits              {16};
    int                 m_audioBytesPerSample    {m_audioChannels * m_audioBits / 8};
    int                 m_audioSampleRate        {44100}; // rate we request from sounddevice
    int                 m_effectiveDsp           {0}; // actual measured rate

    int                 m_useBttv                {1};
    float               m_videoAspect            {1.33333F};

    bool                m_transcoding            {false};

    int                 m_mp3Quality             {3};
    char               *m_mp3Buf                 {nullptr};
    int                 m_mp3BufSize             {0};
    lame_global_flags  *m_gf                     {nullptr};

    RTjpeg             *m_rtjc                   {nullptr};

    static constexpr size_t kOutLen {(1024*1024) + (1024*1024 / 64) + 16 + 3};
    std::array<lzo_byte,kOutLen> m_out                {};

    alignas(8) std::array<uint8_t,LZO1X_1_MEM_COMPRESS> __LZO_MMODEL m_wrkmem {0} ;

    std::vector<struct vidbuffertype *> m_videoBuffer;
    std::vector<struct audbuffertype *> m_audioBuffer;
    std::vector<struct txtbuffertype *> m_textBuffer;

    int                 m_actVideoEncode         {0};
    int                 m_actVideoBuffer         {0};

    int                 m_actAudioEncode         {0};
    int                 m_actAudioBuffer         {0};
    long long           m_actAudioSample         {0};
   
    int                 m_actTextEncode          {0};
    int                 m_actTextBuffer          {0};
 
    int                 m_videoBufferCount       {0};
    int                 m_audioBufferCount       {0};
    int                 m_textBufferCount        {0};

    long                m_videoBufferSize        {0};
    long                m_audioBufferSize        {0};
    long                m_textBufferSize         {0};

    struct timeval      m_stm                    {0,0};

    NVRWriteThread     *m_writeThread            {nullptr};
    NVRAudioThread     *m_audioThread            {nullptr};

    bool                m_recording              {false};

    int                 m_keyframeDist           {KEYFRAMEDIST};
    std::vector<struct seektable_entry> *m_seekTable  {nullptr};
    long long           m_lastPositionMapPos     {0};

    long long           m_extendedDataOffset     {0};

    long long           m_framesWritten          {0};

    bool                m_livetv                 {false};
    bool                m_writePaused            {false};
    bool                m_audioPaused            {false};
    bool                m_mainPaused             {false};

    double              m_frameRateMultiplier    {1.0};
    double              m_heightMultiplier       {1.0};

    int                 m_lastBlock              {0};
    std::chrono::milliseconds  m_firstTc         {0ms};
    std::chrono::milliseconds  m_oldTc           {0ms};
    int                 m_startNum               {0};
    int                 m_frameOfGop             {0};
    int                 m_lastTimecode           {0};
    int                 m_audioBehind            {0};
    
    bool                m_useAvCodec             {false};

    const AVCodec      *m_mpaVidCodec            {nullptr};
    AVCodecContext     *m_mpaVidCtx              {nullptr};

    int                 m_targetBitRate          {2200};
    int                 m_scaleBitRate           {1};
    int                 m_maxQuality             {2};
    int                 m_minQuality             {31};
    int                 m_qualDiff               {3};
    int                 m_mp4Opts                {0};
    int                 m_mbDecision             {FF_MB_DECISION_SIMPLE};
    /// Number of threads to use for MPEG-2 and MPEG-4 encoding
    int                 m_encodingThreadCount    {1};

    VideoFrameType      m_inPixFmt               {FMT_YV12};
    AVPixelFormat       m_pictureFormat          {AV_PIX_FMT_YUV420P};
#ifdef USING_V4L2
    uint32_t            m_v4l2PixelFormat        {0};
#endif
    int                 m_wOut                   {0};
    int                 m_hOut                   {0};

    bool                m_hardwareEncode         {false};
    int                 m_hmjpgQuality           {80};
    int                 m_hmjpgHDecimation       {2};
    int                 m_hmjpgVDecimation       {2};
    int                 m_hmjpgMaxW              {640};

    bool                m_clearTimeOnPause       {false};

    bool                m_usingV4l2              {false};
    int                 m_channelFd              {-1};

    ChannelBase        *m_channelObj             {nullptr};

    bool                m_skipBtAudio            {false};

#ifdef USING_V4L2
    bool                m_correctBttv            {false};
#endif

    int                 m_volume                 {100};

    CC608Decoder       *m_ccd                    {nullptr};

    bool                m_go7007                 {false};
    bool                m_resetCapture           {false};
};

#endif
