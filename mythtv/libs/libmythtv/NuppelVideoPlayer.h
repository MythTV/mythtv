#ifndef NUPPELVIDEOPLAYER
#define NUPPELVIDEOPLAYER

#include <qstring.h>
#include <qmutex.h>
#include <qwaitcondition.h>
#include <qptrqueue.h>
#include <qdatetime.h>
#include <sys/time.h>

#include "RingBuffer.h"
#include "osd.h"
#include "jitterometer.h"
#include "recordingprofile.h"
#include "videooutbase.h"
#include "tv_play.h"

extern "C" {
#include "filter.h"
}
using namespace std;

#define MAXTBUFFER 21

class VideoOutput;
class OSDSet;
class RemoteEncoder;
class MythSqlDatabase;
class ProgramInfo;
class DecoderBase;
class AudioOutput;
class FilterManager;
class FilterChain;
class VideoSync;
class LiveTVChain;
class TV;
struct AVSubtitle;

struct TextContainer
{
    int timecode;
    int len;
    unsigned char *buffer;
    char type;
};

typedef  void (*StatusCallback)(int, void*);

/// Timecode types
enum TCTypes
{
    TC_VIDEO = 0,
    TC_AUDIO,
    TC_SUB,
    TC_CC
};
#define TCTYPESMAX 4

class NuppelVideoPlayer
{
 public:
    NuppelVideoPlayer(QString inUseID = "Unknown",
                      const ProgramInfo *info = NULL);
   ~NuppelVideoPlayer();

    // Initialization
    void ForceVideoOutputType(VideoOutputType type);
    bool InitVideo(void);
    int  OpenFile(bool skipDsp = false, uint retries = 4,
                  bool allow_libmpeg2 = true);

    // Windowing stuff
    void EmbedInWidget(WId wid, int x, int y, int w, int h);
    void StopEmbedding(void);
    void ExposeEvent(void);

    // Sets
    void SetParentWidget(QWidget *widget)     { parentWidget = widget; }
    void SetAsPIP(void)                       { SetNoAudio(); SetNullVideo(); }
    void SetNoAudio(void)                     { no_audio_out = true; }
    void SetNullVideo(void)                   { using_null_videoout = true; }
    void SetAudioDevice(QString device)       { audiodevice = device; }
    void SetFileName(QString lfilename)       { filename = lfilename; }
    void SetExactSeeks(bool exact)            { exactseeks = exact; }
    void SetAutoCommercialSkip(int autoskip);
    void SetCommercialSkipMethod(int m)       { commercialskipmethod = m; }
    void SetCommBreakMap(QMap<long long, int> &newMap);
    void SetRingBuffer(RingBuffer *rbuf)      { ringBuffer = rbuf; }
    void SetLiveTVChain(LiveTVChain *tvchain) { livetvchain = tvchain; }
    void SetAudioSampleRate(int rate)         { audio_samplerate = rate; }
    void SetAudioStretchFactor(float factor)  { audio_stretchfactor = factor; }
    void SetLength(int len)                   { totalLength = len; }
    void SetAudioOutput (AudioOutput *ao)     { audioOutput = ao; }
    void SetVideoFilters(QString &filters)    { videoFilterList = filters; }
    void SetFramesPlayed(long long played)    { framesPlayed = played; }
    void SetEof(void)                         { eof = true; }
    void SetPipPlayer(NuppelVideoPlayer *pip)
        { setpipplayer = pip; needsetpipplayer = true; }
    void SetRecorder(RemoteEncoder *recorder);
    void SetParentPlayer(TV *tv)             { m_tv = tv; }

    void SetTranscoding(bool value);
    void SetWatchingRecording(bool mode);
    void SetBookmark(void);
    void SetKeyframeDistance(int keyframedistance);
    void SetVideoParams(int w, int h, double fps, int keydist,
                        float a = 1.33333, FrameScanType scan = kScan_Ignore,
                        bool reinit = false);
    void SetAudioParams(int bits, int channels, int samplerate);
    void SetEffDsp(int dsprate);
    void SetFileLength(int total, int frames);
    void Zoom(int direction);
    void ClearBookmark(void);

    // Toggle Sets
    void ToggleLetterbox(int letterboxMode = -1);

    // Gets
    int     GetVideoWidth(void) const         { return video_width; }
    int     GetVideoHeight(void) const        { return video_height; }
    float   GetVideoAspect(void) const        { return video_aspect; }
    float   GetFrameRate(void) const          { return video_frame_rate; }

    int     GetSecondsBehind(void) const;
    int     GetLetterbox(void) const;
    int     GetFFRewSkip(void) const          { return ffrew_skip; }
    float   GetAudioStretchFactor(void) const { return audio_stretchfactor; }
    float   GetNextPlaySpeed(void) const      { return next_play_speed; }
    int     GetLength(void) const             { return totalLength; }
    long long GetTotalFrameCount(void) const  { return totalFrames; }
    long long GetFramesPlayed(void) const     { return framesPlayed; }
    long long GetBookmark(void) const;
    QString   GetEncodingType(void) const;

    // Bool Gets
    bool    GetRawAudioState(void) const;
    bool    GetLimitKeyRepeat(void) const     { return limitKeyRepeat; }
    bool    GetEof(void) const                { return eof; }
    bool    PipPlayerSet(void) const          { return !needsetpipplayer; }
    bool    IsErrored(void) const             { return errored; }
    bool    IsPlaying(void) const             { return playing; }
    bool    AtNormalSpeed(void) const         { return next_normal_speed; }
    bool    IsDecoderThreadAlive(void) const  { return decoder_thread_alive; }
    bool    IsNearEnd(long long framesRemaining = -1) const;
    bool    PlayingSlowForPrebuffer(void) const { return m_playing_slower; }
    bool    HasAudioIn(void) const            { return !no_audio_in; }
    bool    HasAudioOut(void) const           { return !no_audio_out; }

    // Complicated gets
    long long CalcMaxFFTime(long long ff, bool setjump = true) const;
    long long CalcRWTime(long long rw) const;
    void      calcSliderPos(struct StatusPosInfo &posInfo) const;

    /// Non-const gets
    OSD         *GetOSD(void)                 { return osd; }
    VideoOutput *getVideoOutput(void)         { return videoOutput; }
    AudioOutput *getAudioOutput(void)         { return audioOutput; }
    char        *GetScreenGrab(int secondsin, int &buflen,
                               int &vw, int &vh, float &ar);
    LiveTVChain *GetTVChain(void)             { return livetvchain; }

    // Start/Reset/Stop playing
    void StartPlaying(void);
    void ResetPlaying(void);
    void StopPlaying(void) { killplayer = true; decoder_thread_alive = false; }

    // Pause stuff
    void PauseDecoder(void);
    void Pause(bool waitvideo = true);
    bool Play(float speed = 1.0, bool normal = true,
              bool unpauseaudio = true);
    bool GetPause(void) const;

    // Seek stuff
    bool FastForward(float seconds);
    bool Rewind(float seconds);
    bool RebuildSeekTable(bool showPercentage = true, StatusCallback cb = NULL,
                          void* cbData = NULL);

    // Commercial stuff
    void SkipCommercials(int direction);
    int FlagCommercials(bool showPercentage, bool fullSpeed,
                        bool inJobQueue);

    // Transcode stuff
    void InitForTranscode(bool copyaudio, bool copyvideo);
    bool TranscodeGetNextFrame(QMap<long long, int>::Iterator &dm_iter,
                               int *did_ff, bool *is_key, bool honorCutList);
    void TranscodeWriteText(
        void (*func)(void *, unsigned char *, int, int, int), void *ptr);
    bool WriteStoredData(
        RingBuffer *outRingBuffer, bool writevideo, long timecodeOffset);
    long UpdateStoredFrameNum(long curFrameNum);

    // Closed caption and teletext stuff
    void ToggleCC(uint mode, uint arg);
    void FlushTxtBuffers(void) { rtxt = wtxt; }

    // Edit mode stuff
    bool EnableEdit(void);
    bool DoKeypress(QKeyEvent *e);
    bool GetEditMode(void) const { return editmode; }

    // Decoder stuff..
    VideoFrame *GetNextVideoFrame(bool allow_unsafe = true);
    VideoFrame *GetRawVideoFrame(long long frameNumber = -1);
    VideoFrame *GetCurrentFrame(int &w, int &h);
    void ReleaseNextVideoFrame(VideoFrame *buffer, long long timecode);
    void ReleaseNextVideoFrame(void)
        { videoOutput->ReleaseFrame(GetNextVideoFrame(false)); }
    void ReleaseCurrentFrame(VideoFrame *frame);
    void DiscardVideoFrame(VideoFrame *buffer);
    void DiscardVideoFrames(void);
    void DrawSlice(VideoFrame *frame, int x, int y, int w, int h);

    // Reinit
    void    ReinitOSD(void);
    void    ReinitVideo(void);
    QString ReinitAudio(void);

    // Add data
    void AddAudioData(char *buffer, int len, long long timecode);
    void AddAudioData(short int *lbuffer, short int *rbuffer, int samples,
                      long long timecode);
    void AddTextData(char *buffer, int len, long long timecode, char type);
    void AddSubtitle(const AVSubtitle& subtitle);

    // Audio Track Selection
    void incCurrentAudioTrack(void);
    void decCurrentAudioTrack(void);
    bool setCurrentAudioTrack(int trackNo);
    int  getCurrentAudioTrack(void) const;
    QStringList listAudioTracks(void) const;

    // Subtitle Track Selection
    void incCurrentSubtitleTrack(void);
    void decCurrentSubtitleTrack(void);
    bool setCurrentSubtitleTrack(int trackNo);
    int  getCurrentSubtitleTrack(void) const;
    QStringList listSubtitleTracks(void) const;

    // Time Code adjustment stuff
    long long AdjustAudioTimecodeOffset(long long v)
        { tc_wrap[TC_AUDIO] += v;  return tc_wrap[TC_AUDIO]; }
    long long ResetAudioTimecodeOffset(void)
        { tc_wrap[TC_AUDIO] = 0LL; return tc_wrap[TC_AUDIO]; }
    long long ResyncAudioTimecodeOffset(void)
        { tc_wrap[TC_AUDIO] = LONG_LONG_MIN; return 0L; }
    long long GetAudioTimecodeOffset(void) const 
        { return tc_wrap[TC_AUDIO]; }

    // LiveTV public stuff
    void CheckTVChain();
    void FileChangedCallback();

    // DVD public stuff
    void ChangeDVDTrack(bool ffw);

  protected:
    void DisplayPauseFrame(void);
    void DisplayNormalFrame(void);
    void OutputVideoLoop(void);
    void IvtvVideoLoop(void);

    static void *kickoffOutputVideoLoop(void *player);

  private:
    // Private initialization stuff
    void InitFilters(void);
    FrameScanType detectInterlace(FrameScanType newScan, FrameScanType scan,
                                  float fps, int video_height);

    // Private Sets
    void SetPrebuffering(bool prebuffer);

    // Private Gets
    int  GetStatusbarPos(void) const;
    bool IsInDelete(long long testframe) const;

    // Private pausing stuff
    void PauseVideo(bool wait = true);
    void UnpauseVideo(void);
    bool GetVideoPause(void) const { return video_actually_paused; }

    // Private decoder stuff
    void  SetDecoder(DecoderBase *dec);
    /// Returns the stream decoder currently in use.
    DecoderBase *GetDecoder(void) { return decoder; }
    /// Returns the stream decoder currently in use.
    const DecoderBase *GetDecoder(void) const { return decoder; }
    bool DecodeFrame(struct rtframeheader *frameheader,
                     unsigned char *strm, unsigned char *outbuf);

    void CheckPrebuffering(void);
    bool GetFrameNormal(int onlyvideo);
    bool GetFrameFFREW(void);
    bool GetFrame(int onlyvideo, bool unsafe = false);

    // These actually execute commands requested by public members
    void DoPause(void);
    void DoPlay(void);
    bool DoFastForward(void);
    bool DoRewind(void);

    // Private seeking stuff
    void ClearAfterSeek(void);
    bool FrameIsInMap(long long frameNumber, QMap<long long, int> &breakMap);
    void JumpToFrame(long long frame);
    void JumpToNetFrame(long long net) { JumpToFrame(framesPlayed + net); }

    // Private commercial skipping
    void SkipCommercialsByBlanks(void);
    bool DoSkipCommercials(int direction);
    void AutoCommercialSkip(void);

    // Private edit stuff
    void SaveCutList(void);
    void LoadCutList(void);
    void LoadCommBreakList(void);
    void DisableEdit(void);

    void AddMark(long long frames, int type);
    void DeleteMark(long long frames);
    void ReverseMark(long long frames);

    void SetDeleteIter(void);
    void SetBlankIter(void);
    void SetCommBreakIter(void);

    void HandleArbSeek(bool right);
    void HandleSelect(void);
    void HandleResponse(void);

    void UpdateTimeDisplay(void);
    void UpdateSeekAmount(bool up);
    void UpdateEditSlider(void);

    // Private A/V Sync Stuff
    float WarpFactor(void);
    void  WrapTimecode(long long &timecode, TCTypes tc_type);
    void  InitAVSync(void);
    void  AVSync(void);
    void  ShutdownAVSync(void);

    // Private closed caption and teletext stuff
    int   tbuffer_numvalid(void); // number of valid slots in the text buffer
    int   tbuffer_numfree(void); // number of free slots in the text buffer
    void  ShowText(void);
    void  ResetCC(void);
    void  UpdateCC(unsigned char *inpos);

    // Private subtitle stuff
    void  DisplaySubtitles(void);
    void  ClearSubtitles(void);

    // Private LiveTV stuff
    void  SwitchToProgram(void);
    void  JumpToProgram(void);

  private:
    VideoOutputType forceVideoOutput;

    DecoderBase   *decoder;
    VideoOutput   *videoOutput;
    RemoteEncoder *nvr_enc;
    ProgramInfo   *m_playbackinfo;

    // Window stuff
    QWidget *parentWidget;
    WId embedid;
    int embx, emby, embw, embh;


    // State
    QWaitCondition decoderThreadPaused;
    QWaitCondition videoThreadPaused;
    QMutex   vidExitLock;
    bool     eof;             ///< At end of file/ringbuffer
    bool     m_double_framerate;///< Output fps is double Video (input) rate
    bool     m_can_double;    ///< VideoOutput capable of doubling frame rate
    bool     paused;
    bool     pausevideo;
    bool     actuallypaused;
    bool     video_actually_paused;
    bool     playing;
    bool     decoder_thread_alive;
    bool     killplayer;
    bool     killvideo;
    bool     livetv;
    bool     watchingrecording;
    bool     editmode;
    bool     resetvideo;
    bool     using_null_videoout;
    bool     no_audio_in;
    bool     no_audio_out;
    bool     transcoding;
    bool     hasFullPositionMap;
    mutable bool     limitKeyRepeat;
    bool     errored;
    int      m_DeintSetting;

    // Bookmark stuff
    long long bookmarkseek;
    bool      previewFromBookmark;

    // Seek
    /// If fftime>0, number of frames to seek forward.
    /// If fftime<0, number of frames to seek backward.
    long long fftime;
    /// 1..9 == keyframe..10 minutes. 0 == cut point
    int       seekamountpos;
    /// Seekable frame increment when not using exact seeks.
    /// Usually equal to keyframedist.
    int      seekamount;
    /// Iff true we ignore seek amount and try to seek to an
    /// exact frame ignoring key frame restrictions.
    bool     exactseeks;

    // Playback misc.
    /// How often we have tried to wait for a video output buffer and failed
    int       videobuf_retries;
    long long framesPlayed;
    long long totalFrames;
    long long totalLength;
    long long rewindtime;
    QString m_recusage;

    // -- end state stuff -- 


    // Input Video Attributes
    int      video_width;     ///< Video (input) width
    int      video_height;    ///< Video (input) height
    int      video_size;      ///< Video (input) buffer size in bytes
    double   video_frame_rate;///< Video (input) Frame Rate (often inaccurate)
    float    video_aspect;    ///< Video (input) Apect Ratio
    /// Video (input) Scan Type (interlaced, progressive, detect, ignore...)
    FrameScanType m_scan;
    /// Video (input) Number of frames between key frames (often inaccurate)
    int keyframedist;

    // RingBuffer stuff
    QString    filename;        ///< Filename if we create our own ringbuf
    bool       weMadeBuffer;    ///< Iff true, we can delete ringBuffer
    RingBuffer *ringBuffer;     ///< Pointer to the RingBuffer we read from
    
    // Prebuffering (RingBuffer) control
    QWaitCondition prebuffering_wait;///< QWaitContition used by prebuffering
    QMutex     prebuffering_lock;///< Mutex used to control access to prebuf
    bool       prebuffering;    ///< Iff true, don't play until done prebuf
    int        prebuffer_tries; ///< Number of times prebuf wait attempted

    // Support for analog captions and teletext
    // (i.e. Vertical Blanking Interval (VBI) encoded data.)
    uint     vbimode;         ///< VBI decoder to use
    bool     subtitlesOn;     ///< true iff cc/tt display is enabled
    int      ttPageNum;       ///< VBI page to display when in PAL vbimode
    int      ccmode;          ///< VBI text to display when in NTSC vbimode

    int      wtxt;            ///< Write position for VBI text
    int      rtxt;            ///< Read position for VBI text
    QMutex   text_buflock;    ///< Lock for rtxt and wtxt VBI text positions
    int      text_size;       ///< Maximum size of a text buffer
    struct TextContainer txtbuffers[MAXTBUFFER+1]; ///< VBI text buffers

    QString  ccline;
    int      cccol;
    int      ccrow;

    // Support for captions, teletext, etc. decoded by libav
    QMutex    subtitleLock;
    bool      osdHasSubtitles;
    long long osdSubtitlesExpireAt;
    MythDeque<AVSubtitle> nonDisplayedSubtitles;

    // OSD stuff
    OSD      *osd;
    OSDSet   *timedisplay;
    QString   dialogname;
    int       dialogtype;

    // Audio stuff
    AudioOutput *audioOutput;
    QString  audiodevice;
    int      audio_channels;
    int      audio_bits;
    int      audio_samplerate;
    float    audio_stretchfactor;

    // Picture-in-Picture
    NuppelVideoPlayer *pipplayer;
    NuppelVideoPlayer *setpipplayer;
    bool needsetpipplayer;

    // Filters
    QMutex   videofiltersLock;
    QString  videoFilterList;
    int      postfilt_width;  ///< Post-Filter (output) width
    int      postfilt_height; ///< Post-Filter (output) height
    FilterChain   *videoFilters;
    FilterManager *FiltMan;

    // Commercial filtering
    QMutex     commBreakMapLock;
    int        skipcommercials;
    int        autocommercialskip;
    int        commercialskipmethod;
    int        commrewindamount;
    int        commnotifyamount;
    int        lastCommSkipDirection;
    time_t     lastCommSkipTime;
    long long  lastCommSkipStart;
    time_t     lastSkipTime;

    long long  deleteframe;
    bool       hasdeletetable;
    bool       hasblanktable;
    bool       hascommbreaktable;
    QMap<long long, int> deleteMap;
    QMap<long long, int> blankMap;
    QMap<long long, int> commBreakMap;
    QMap<long long, int>::Iterator deleteIter;
    QMap<long long, int>::Iterator blankIter;
    QMap<long long, int>::Iterator commBreakIter;

    // Playback (output) speed control
    /// Lock for next_play_speed and next_normal_speed
    QMutex     decoder_lock;
    float      next_play_speed;
    bool       next_normal_speed;

    float      play_speed;    
    bool       normal_speed;  
    int        frame_interval;///< always adjusted for play_speed

    int        ffrew_skip;    

    // Audio and video synchronization stuff
    VideoSync *videosync;
    int        delay;
    int        vsynctol;
    int        avsync_delay;
    int        avsync_adjustment;
    int        avsync_avg;
    int        avsync_oldavg;
    int        refreshrate;
    bool       lastsync;
    bool       m_playing_slower;
    float      m_stored_audio_stretchfactor;

    // Audio warping stuff
    bool       usevideotimebase;
    float      warpfactor;
    float      warpfactor_avg;
    short int *warplbuff;
    short int *warprbuff;
    int        warpbuffsize;
 
    // Time Code stuff
    int        prevtc;        ///< 32 bit timecode if last VideoFrame shown
    int        tc_avcheck_framecounter;
    long long  tc_wrap[TCTYPESMAX];
    long long  tc_lastval[TCTYPESMAX];
    long long  tc_diff_estimate;

    // LiveTV
    LiveTVChain *livetvchain;
    TV *m_tv;

    // Debugging variables
    Jitterometer *output_jmeter;
};

#endif
