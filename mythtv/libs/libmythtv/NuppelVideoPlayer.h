#ifndef NUPPELVIDEOPLAYER
#define NUPPELVIDEOPLAYER

#include <qstring.h>
#include <qmutex.h>
#include <qwaitcondition.h>
#include <qptrqueue.h>
#include <sys/time.h>

#include "RingBuffer.h"
#include "osd.h"
#include "jitterometer.h"
#include "recordingprofile.h"
#include "videooutbase.h"

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
struct AVSubtitle;

struct TextContainer
{
    int timecode;
    int len;
    unsigned char *buffer;
    char type;
};

typedef  void (*StatusCallback)(int, void*);

class NuppelVideoPlayer
{
 public:
    NuppelVideoPlayer(const ProgramInfo *info = NULL);
   ~NuppelVideoPlayer();

    void SetParentWidget(QWidget *widget) { parentWidget = widget; }

    void SetAsPIP(void) { disableaudio = using_null_videoout = true; }
    void SetNoAudio(void) { disableaudio = true; }
    void SetNullVideo(void) { using_null_videoout = true; }

    void SetAudioDevice(QString device) { audiodevice = device; }
    void SetFileName(QString lfilename) { filename = lfilename; }

    void SetExactSeeks(bool exact) { exactseeks = exact; }
    void SetAutoCommercialSkip(int autoskip);
    void SetCommercialSkipMethod(int method) { commercialskipmethod = method; }
    void SetCommBreakMap(QMap<long long, int> &newMap);

    int OpenFile(bool skipDsp = false);
    void StartPlaying(void);
    void StopPlaying(void) { killplayer = true; decoder_thread_alive = false; }

    bool IsPlaying(void) { return playing; }
    bool IsDecoderThreadAlive(void) { return decoder_thread_alive; }

    void SetRingBuffer(RingBuffer *rbuf) { ringBuffer = rbuf; }

    void SetAudioSampleRate(int rate) { audio_samplerate = rate; }

    void SetAudioStretchFactor(float factor) { audio_stretchfactor = factor; }

    void Pause(bool waitvideo = true);
    bool Play(float speed = 1.0, bool normal = true,
              bool unpauseaudio = true);
    bool GetPause(void);
    int GetFFRewSkip(void) { return ffrew_skip; }
    bool AtNormalSpeed(void) { return next_normal_speed; }

    bool FastForward(float seconds);
    bool Rewind(float seconds);

    void SkipCommercials(int direction);

    void ResetPlaying(void);

    int GetVideoWidth(void) { return video_width; }
    int GetVideoHeight(void) { return video_height; }
    float GetVideoAspect(void) { return video_aspect; }
    float GetFrameRate(void) { return video_frame_rate; }
    long long GetTotalFrameCount(void) { return totalFrames; }
    long long GetFramesPlayed(void) { return framesPlayed; }
    int GetSecondsBehind(void);
    float GetAudioStretchFactor(void) const { return audio_stretchfactor; }
    float GetNextPlaySpeed(void) const { return next_play_speed; }

    void SetRecorder(RemoteEncoder *recorder);

    OSD *GetOSD(void) { return osd; }

    // don't use this on something you're playing
    char *GetScreenGrab(int secondsin, int &buflen, int &vw, int &vh, float &ar);

    void SetLength(int len) { totalLength = len; }
    int GetLength(void) { return totalLength; }

    QString GetEncodingType(void);
    void SetAudioOutput (AudioOutput *ao) { audioOutput = ao; }
    void FlushTxtBuffers(void) { rtxt = wtxt; }
    bool WriteStoredData(RingBuffer *outRingBuffer, bool writevideo,
                         long timecodeOffset);
    long UpdateStoredFrameNum(long curFrameNum);
    void InitForTranscode(bool copyaudio, bool copyvideo);
    bool TranscodeGetNextFrame(QMap<long long, int>::Iterator &dm_iter,
                               int *did_ff, bool *is_key, bool honorCutList);
    void TranscodeWriteText(void (*func)(void *, unsigned char *, int, int, int), void *ptr);

    int FlagCommercials(bool showPercentage, bool fullSpeed,
                        bool inJobQueue);
    bool RebuildSeekTable(bool showPercentage = true, StatusCallback cb = NULL,
                          void* cbData = NULL);

    VideoFrame *GetCurrentFrame(int &w, int &h);
    void ReleaseCurrentFrame(VideoFrame *frame);

    void SetPipPlayer(NuppelVideoPlayer *pip)
        { setpipplayer = pip; needsetpipplayer = true; }
    bool PipPlayerSet(void) const { return !needsetpipplayer; }

    void SetVideoFilters(QString &filters) { videoFilterList = filters; }
    void SetTranscoding(bool value);

    void SetWatchingRecording(bool mode);
    void SetBookmark(void);
    void ClearBookmark(void);
    long long GetBookmark(void);

    void ToggleCC(char mode, int arg);

    // edit mode stuff
    bool EnableEdit(void);
    bool DoKeypress(QKeyEvent *e);
    bool GetEditMode(void) { return editmode; }

    void EmbedInWidget(WId wid, int x, int y, int w, int h);
    void StopEmbedding(void);

    // decoder stuff..
    void ForceVideoOutputType(VideoOutputType type);

    void SetKeyframeDistance(int keyframedistance);
    void SetVideoParams(int width, int height, double fps,
                        int keyframedistance, float aspect = 1.33333,
                        FrameScanType scan = kScan_Ignore, bool reinit = false);
    void SetAudioParams(int bps, int channels, int samplerate);
    void SetEffDsp(int dsprate);
    void SetFileLength(int total, int frames);

    VideoFrame *GetNextVideoFrame(bool allow_unsafe = true);
    void ReleaseNextVideoFrame(VideoFrame *buffer, long long timecode);
    void ReleaseNextVideoFrame(void)
        { videoOutput->ReleaseFrame(GetNextVideoFrame(false)); }
    void DiscardVideoFrame(VideoFrame *buffer);
    void DiscardVideoFrames(void);

    void DrawSlice(VideoFrame *frame, int x, int y, int w, int h);

    bool GetRawAudioState(void);
    void AddAudioData(char *buffer, int len, long long timecode);
    void AddAudioData(short int *lbuffer, short int *rbuffer, int samples,
                      long long timecode);

    void AddTextData(char *buffer, int len, long long timecode, char type);

    void SetEof(void) { eof = 1; }
    int GetEof(void) { return eof; }
    void SetFramesPlayed(long long played) { framesPlayed = played; }

    VideoOutput *getVideoOutput(void) { return videoOutput; }
    AudioOutput *getAudioOutput(void) { return audioOutput; }

    VideoSync *getVideoSync() const { return videosync; }

    void StopVideoSync(void);

    int calcSliderPos(QString &desc);

    bool GetLimitKeyRepeat(void) { return limitKeyRepeat; }

    void ReinitOSD(void);
    void ReinitVideo(void);
    QString ReinitAudio(void);

    void ToggleLetterbox(int letterboxMode = -1);
    void Zoom(int direction);
    int GetLetterbox(void);

    void ExposeEvent(void);

    void incCurrentAudioTrack();
    void decCurrentAudioTrack();
    bool setCurrentAudioTrack(int trackNo);
    int getCurrentAudioTrack();
    QStringList listAudioTracks();

    void incCurrentSubtitleTrack();
    void decCurrentSubtitleTrack();
    bool setCurrentSubtitleTrack(int trackNo);
    int getCurrentSubtitleTrack();
    QStringList listSubtitleTracks();

    long long CalcMaxFFTime(long long ff);

    bool IsNearEnd(long long framesRemaining = -1);

    bool IsErrored() { return errored; }
    bool InitVideo(void);
    VideoFrame* GetRawVideoFrame(long long frameNumber = -1);
    
    long long GetAudioTimecodeOffset() { return audio_timecode_offset; }
    long long AdjustAudioTimecodeOffset(long long v) { 
        audio_timecode_offset += v;
        return audio_timecode_offset; 
    }

    long long ResetAudioTimecodeOffset() { 
        audio_timecode_offset = 0;
        return audio_timecode_offset; 
    }

    long long ResyncAudioTimecodeOffset() { 
        audio_timecode_offset = (long long)0x8000000000000000LL;
        return 0; 
    }

    void AddSubtitle(const AVSubtitle& subtitle);

 protected:
    void DisplayPauseFrame(void);
    void DisplayNormalFrame(void);
    void OutputVideoLoop(void);
    void IvtvVideoLoop(void);

    static void *kickoffOutputVideoLoop(void *player);

 private:
    void InitFilters(void);

    bool GetVideoPause(void);
    void PauseVideo(bool wait = true);
    void UnpauseVideo(void);

    void setPrebuffering(bool prebuffer);

    bool DecodeFrame(struct rtframeheader *frameheader,
                     unsigned char *strm, unsigned char *outbuf);
    bool GetFrame(int onlyvideo, bool unsafe = false);

    void DoPause();
    void DoPlay();
    bool DoFastForward();
    bool DoRewind();

    void ClearAfterSeek(); // caller should not hold any locks

    int GetStatusbarPos(void);

    int SkipTooCloseToEnd(int frames);
    void SkipCommercialsByBlanks(void);
    bool DoSkipCommercials(int direction);
    void AutoCommercialSkip(void);
    bool FrameIsInMap(long long frameNumber, QMap<long long, int> &breakMap);

    void JumpToFrame(long long frame);
    void JumpToNetFrame(long long net) { JumpToFrame(framesPlayed + net); }

    int tbuffer_numvalid(void); // number of valid slots in the text buffer
    int tbuffer_numfree(void); // number of free slots in the text buffer

    void ShowText(void);
    void ResetCC(void);
    void UpdateCC(unsigned char *inpos);

    void UpdateTimeDisplay(void);
    void UpdateSeekAmount(bool up);
    void UpdateEditSlider(void);
    void AddMark(long long frames, int type);
    void DeleteMark(long long frames);
    void ReverseMark(long long frames);
    void HandleSelect(void);
    void HandleResponse(void);
    void HandleArbSeek(bool right);
    bool IsInDelete(long long testframe);
    void SaveCutList(void);
    void LoadCutList(void);
    void LoadCommBreakList(void);
    void DisableEdit(void);
    void SetDeleteIter(void);
    void SetBlankIter(void);
    void SetCommBreakIter(void);

    float WarpFactor(void);

    FrameScanType detectInterlace(FrameScanType newScan, FrameScanType scan,
                                  float fps, int video_height);

    void DisplaySubtitles();
    void ClearSubtitles();

    void SetDecoder(DecoderBase *dec);
    // Returns the stream decoder currently in use.
    DecoderBase *GetDecoder() { return decoder; }
    // Returns the stream decoder currently in use.
    const DecoderBase *GetDecoder() const { return decoder; }

    // timecode types
    enum TCTypes
    {
        TC_VIDEO = 0,
        TC_AUDIO,
        TC_SUB,
        TC_CC
    };
#define TCTYPESMAX 4

  private:
    void WrapTimecode(long long &timecode, TCTypes tc_type);
    void InitAVSync(void);
    void AVSync(void);
    void ShutdownAVSync(void);

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
    bool     disableaudio;
    bool     transcoding;
    bool     hasFullPositionMap;
    bool     limitKeyRepeat;
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
    int      vbimode;         ///< VBI decoder to use
    bool     cc;              ///< true iff vbimode == 2 (NTSC Line 21 CC)
    int      vbipagenr;       ///< VBI page to display when in PAL vbimode
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
    long long  lastaudiotime;
    long long  audio_timecode_offset;
    bool       lastsync;

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

    // Debugging variables
    Jitterometer *output_jmeter;
};

#endif
