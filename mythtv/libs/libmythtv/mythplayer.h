#ifndef MYTHPLAYER_H
#define MYTHPLAYER_H

#include <stdint.h>

#include <sys/time.h>

#include <QObject>
#include <QEvent>

#include "playercontext.h"
#include "volumebase.h"
#include "audiooutputsettings.h"
#include "RingBuffer.h"
#include "osd.h"
#include "jitterometer.h"
#include "videooutbase.h"
#include "teletextreader.h"
#include "subtitlereader.h"
#include "tv_play.h"
#include "yuv2rgb.h"
#include "cc608reader.h"
#include "cc608decoder.h"
#include "cc708reader.h"
#include "cc708decoder.h"
#include "cc708window.h"
#include "decoderbase.h"
#include "deletemap.h"
#include "commbreakmap.h"
#include "audioplayer.h"

#include "mythexp.h"

extern "C" {
#include "filter.h"
}
using namespace std;

class VideoOutput;
class RemoteEncoder;
class MythSqlDatabase;
class ProgramInfo;
class DecoderBase;
class FilterManager;
class FilterChain;
class VideoSync;
class LiveTVChain;
class TV;
struct SwsContext;
class InteractiveTV;
class NSAutoreleasePool;
class DetectLetterbox;
class MythPlayer;

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

// Caption Display modes
enum
{
    kDisplayNone                = 0x000,
    kDisplayNUVTeletextCaptions = 0x001,
    kDisplayTeletextCaptions    = 0x002,
    kDisplayAVSubtitle          = 0x004,
    kDisplayCC608               = 0x008,
    kDisplayCC708               = 0x010,
    kDisplayTextSubtitle        = 0x020,
    kDisplayDVDButton           = 0x040,
    kDisplayRawTextSubtitle     = 0x080,
    kDisplayAllCaptions         = 0x0ff,
    kDisplayTeletextMenu        = 0x100,
};

class DecoderThread : public QThread
{
    Q_OBJECT

  public:
    DecoderThread(MythPlayer *mp, bool start_paused)
      : QThread(NULL), m_mp(mp), m_start_paused(start_paused) { }

  protected:
    virtual void run(void);

  private:
    MythPlayer *m_mp;
    bool        m_start_paused;
};

class MPUBLIC MythPlayer
{
    // Do NOT add a decoder class to this list
    friend class PlayerContext;
    friend class PlayerTimer;
    friend class CC708Reader;
    friend class CC608Reader;
    friend class DecoderThread;
    friend class TV;
    friend class DetectLetterbox;
    friend class PlayerThread;

  public:
    MythPlayer(bool muted = false);
   ~MythPlayer();

    // Initialisation
    virtual int OpenFile(uint retries = 4, bool allow_libmpeg2 = true);
    bool InitVideo(void);

    // Public Sets
    void SetPlayerInfo(TV *tv, QWidget *widget, bool frame_exact_seek,
                       PlayerContext *ctx);
    void SetNullVideo(void)                   { using_null_videoout = true; }
    void SetExactSeeks(bool exact)            { exactseeks = exact; }
    void SetLength(int len)                   { totalLength = len; }
    void SetFramesPlayed(uint64_t played)     { framesPlayed = played; }
    void SetVideoFilters(const QString &override);
    void SetEof(bool eof);
    void SetPIPActive(bool is_active)         { pip_active = is_active; }
    void SetPIPVisible(bool is_visible)       { pip_visible = is_visible; }

    void SetTranscoding(bool value);
    void SetWatchingRecording(bool mode);
    void SetWatched(bool forceWatched = false);
    void SetKeyframeDistance(int keyframedistance);
    void SetVideoParams(int w, int h, double fps, int keydist,
                        float a = 1.33333, FrameScanType scan = kScan_Ignore,
                        bool video_codec_changed = false);
    void SetFileLength(int total, int frames);
    void SetDuration(int duration);
    void SetForcedAspectRatio(int mpeg2_aspect_value, int letterbox_permission);
    void SetVideoResize(const QRect &videoRect);

    // Gets
    QSize   GetVideoBufferSize(void) const    { return video_dim; }
    QSize   GetVideoSize(void) const          { return video_disp_dim; }
    float   GetVideoAspect(void) const        { return video_aspect; }
    float   GetFrameRate(void) const          { return video_frame_rate; }

    bool    IsAudioNeeded(void) { return !using_null_videoout && player_ctx->IsAudioNeeded(); }
    uint    GetVolume(void) { return audio.GetVolume(); }
    int     GetSecondsBehind(void) const;
    AspectOverrideMode GetAspectOverride(void) const;
    AdjustFillMode     GetAdjustFill(void) const;
    MuteState          GetMuteState(void) { return audio.GetMuteState(); }

    int     GetFFRewSkip(void) const          { return ffrew_skip; }
    float   GetPlaySpeed(void) const          { return play_speed; }
    AudioPlayer* GetAudio(void)               { return &audio; }
    float   GetAudioStretchFactor(void)       { return audio.GetStretchFactor(); }
    float   GetNextPlaySpeed(void) const      { return next_play_speed; }
    int     GetLength(void) const             { return totalLength; }
    uint64_t GetTotalFrameCount(void) const   { return totalFrames; }
    uint64_t GetFramesPlayed(void) const      { return framesPlayed; }
    virtual  uint64_t GetBookmark(void);
    QString   GetError(void) const;
    bool      IsErrorRecoverable(void) const
        { return (errorType & kError_Switch_Renderer); }
    bool      IsDecoderErrored(void)   const
        { return (errorType & kError_Decode); }
    QString   GetEncodingType(void) const;
    void      GetCodecDescription(InfoMap &infoMap);
    QString   GetXDS(const QString &key) const;
    PIPLocation GetNextPIPLocation(void) const;

    // Bool Gets
    bool    IsPaused(void)                    { return allpaused;      }
    bool    GetRawAudioState(void) const;
    bool    GetLimitKeyRepeat(void) const     { return limitKeyRepeat; }
    bool    GetEof(void);
    bool    IsErrored(void) const;
    bool    IsPlaying(uint wait_ms = 0, bool wait_for = true) const;
    bool    AtNormalSpeed(void) const         { return next_normal_speed; }
    bool    IsReallyNearEnd(void) const;
    bool    IsNearEnd(int64_t framesRemaining = -1);
    bool    HasAudioOut(void) const           { return audio.HasAudioOut(); }
    bool    IsPIPActive(void) const           { return pip_active; }
    bool    IsPIPVisible(void) const          { return pip_visible; }
    bool    IsMuted(void)                     { return audio.IsMuted(); }
    bool    UsingNullVideo(void) const { return using_null_videoout; }
    bool    HasTVChainNext(void) const;

    // Non-const gets
    VideoOutput *getVideoOutput(void)         { return videoOutput; }
    virtual char *GetScreenGrabAtFrame(uint64_t frameNum, bool absolute,
                                       int &buflen, int &vw, int &vh, float &ar);
    char        *GetScreenGrab(int secondsin, int &buflen,
                               int &vw, int &vh, float &ar);
    InteractiveTV *GetInteractiveTV(void);

    // Title stuff
    virtual bool SwitchTitle(int title) { return false; }
    virtual bool NextTitle(void) { return false; }
    virtual bool PrevTitle(void) { return false; }

    // Angle stuff
    virtual bool SwitchAngle(int title) { return false; }
    virtual bool NextAngle(void) { return false; }
    virtual bool PrevAngle(void) { return false; }

    // Transcode stuff
    void InitForTranscode(bool copyaudio, bool copyvideo);
    bool TranscodeGetNextFrame(frm_dir_map_t::iterator &dm_iter,
                               int &did_ff, bool &is_key, bool honorCutList);
    bool WriteStoredData(
        RingBuffer *outRingBuffer, bool writevideo, long timecodeOffset);
    long UpdateStoredFrameNum(long curFrameNum);
    void SetCutList(const frm_dir_map_t &newCutList);

    // Decoder stuff..
    VideoFrame *GetNextVideoFrame(bool allow_unsafe = true);
    VideoFrame *GetRawVideoFrame(long long frameNumber = -1);
    VideoFrame *GetCurrentFrame(int &w, int &h);
    virtual void ReleaseNextVideoFrame(VideoFrame *buffer, int64_t timecode,
                                       bool wrap = true);
    void ReleaseNextVideoFrame(void)
        { videoOutput->ReleaseFrame(GetNextVideoFrame(false)); }
    void ReleaseCurrentFrame(VideoFrame *frame);
    void DiscardVideoFrame(VideoFrame *buffer);
    void DiscardVideoFrames(bool next_frame_keyframe);
    void DrawSlice(VideoFrame *frame, int x, int y, int w, int h);
    /// Returns the stream decoder currently in use.
    DecoderBase *GetDecoder(void) { return decoder; }

    // Preview Image stuff
    void SaveScreenshot(void);

    // Reinit
    void ReinitVideo(void);

    // Add data
    virtual bool PrepareAudioSample(int64_t &timecode);

    // Public Closed caption and teletext stuff
    uint GetCaptionMode(void) const    { return textDisplayMode; }
    CC708Reader* GetCC708Reader(void)  { return &cc708; }
    CC608Reader* GetCC608Reader(void)  { return &cc608; }
    SubtitleReader* GetSubReader(void) { return &subReader; }
    TeletextReader* GetTeletextReader(void) { return &ttxReader; }

    // Public Audio/Subtitle/EIA-608/EIA-708 stream selection - thread safe
    void TracksChanged(uint trackType);
    void EnableSubtitles(bool enable);

    // Public MHEG/MHI stream selection
    bool SetAudioByComponentTag(int tag);
    bool SetVideoByComponentTag(int tag);

    // LiveTV public stuff
    void CheckTVChain();
    void FileChangedCallback();

    // Chapter public stuff
    virtual int  GetNumChapters(void);
    virtual int  GetCurrentChapter(void);
    virtual void GetChapterTimes(QList<long long> &times);

    // Title public stuff
    virtual int GetNumTitles(void) const { return 0; }
    virtual int GetCurrentTitle(void) const { return 0; }
    virtual int GetTitleDuration(int title) const { return 0; }
    virtual QString GetTitleName(int title) const { return QString(); }

    // Angle public stuff
    virtual int GetNumAngles(void) const { return 0; }
    virtual int GetCurrentAngle(void) const { return 0; }
    virtual QString GetAngleName(int title) const { return QString(); }

    // DVD public stuff
    virtual void ChangeDVDTrack(bool ffw)       { (void) ffw;       }
    virtual bool GoToDVDMenu(QString str)       { return false;     }
    virtual void GoToDVDProgram(bool direction) { (void) direction; }

    // Position Map Stuff
    bool PosMapFromEnc(unsigned long long          start,
                       QMap<long long, long long> &posMap);

    // OSD locking for TV class
    bool TryLockOSD(void) { return osdLock.tryLock(50); }
    void LockOSD(void)    { osdLock.lock();   }
    void UnlockOSD(void)  { osdLock.unlock(); }

  protected:
    // Initialization
    void OpenDummy(void);

    // Non-public sets
    virtual void SetBookmark(void);
    virtual void ClearBookmark(bool message = true);
    bool AddPIPPlayer(MythPlayer *pip, PIPLocation loc, uint timeout);
    bool RemovePIPPlayer(MythPlayer *pip, uint timeout);
    void DisableHardwareDecoders(void)        { no_hardware_decoders = true; }
    void NextScanType(void)
        { SetScanType((FrameScanType)(((int)m_scan + 1) & 0x3)); }
    void SetScanType(FrameScanType);
    FrameScanType GetScanType(void) const { return m_scan; }
    bool IsScanTypeLocked(void) const { return m_scan_locked; }
    void Zoom(ZoomDirection direction);

    // Windowing stuff
    void EmbedInWidget(int x, int y, int w, int h, WId id);
    void StopEmbedding(void);
    void ExposeEvent(void);
    bool IsEmbedding(void);
    void WindowResized(const QSize &new_size);

    // Audio Sets
    uint AdjustVolume(int change)           { return audio.AdjustVolume(change); }
    bool SetMuted(bool mute)                { return audio.SetMuted(mute);       }
    MuteState SetMuteState(MuteState state) { return audio.SetMuteState(state);  }
    MuteState IncrMuteState(void)           { return audio.IncrMuteState();      }

    // Non-const gets
    OSD         *GetOSD(void) { return osd; }
    virtual void SeekForScreenGrab(uint64_t &number, uint64_t frameNum,
                                   bool absolute);

    // Complicated gets
    virtual long long CalcMaxFFTime(long long ff, bool setjump = true) const;
    long long CalcRWTime(long long rw) const;
    virtual void calcSliderPos(osdInfo &info, bool paddedFields = false);

    // Commercial stuff
    void SetAutoCommercialSkip(CommSkipMode autoskip)
        { commBreakMap.SetAutoCommercialSkip(autoskip, framesPlayed); }
    void SkipCommercials(int direction)
        { commBreakMap.SkipCommercials(direction); }
    void SetCommBreakMap(frm_dir_map_t &newMap);
    CommSkipMode GetAutoCommercialSkip(void)
        { return commBreakMap.GetAutoCommercialSkip(); }

    // Toggle Sets
    void ToggleAspectOverride(AspectOverrideMode aspectMode = kAspect_Toggle);
    void ToggleAdjustFill(AdjustFillMode adjustfillMode = kAdjustFill_Toggle);

    // Start/Reset/Stop playing
    virtual bool StartPlaying(void);
    virtual void ResetPlaying(bool resetframes = true);
    virtual void EndPlaying(void) { }
    virtual void StopPlaying(void);

    // Pause stuff
    bool PauseDecoder(void);
    void UnpauseDecoder(void);
    bool Pause(void);
    bool Play(float speed = 1.0, bool normal = true, bool unpauseaudio = true);

    // Seek stuff
    virtual bool FastForward(float seconds);
    virtual bool Rewind(float seconds);
    virtual bool JumpToFrame(uint64_t frame);

    // Chapter stuff
    void JumpChapter(int chapter);

    // Playback
    virtual bool PrebufferEnoughFrames(bool pause_audio = true,
                                       int  min_buffers = 0);
    void         SetBuffering(bool new_buffering, bool pause_audio = false);
    void         RefreshPauseFrame(void);
    virtual void DisplayPauseFrame(void);
    virtual void DisplayNormalFrame(bool check_prebuffer = true);
    virtual void PreProcessNormalFrame(void);
    virtual void VideoStart(void);
    virtual bool VideoLoop(void);
    virtual void VideoEnd(void);
    virtual void DecoderStart(bool start_paused);
    virtual void DecoderLoop(bool pause);
    virtual void DecoderEnd(void);
    virtual void DecoderPauseCheck(void);
    virtual void AudioEnd(void);
    virtual void EventStart(void);
    virtual void EventLoop(void);
    virtual void InitialSeek(void);

    // Protected MHEG/MHI stuff
    bool ITVHandleAction(const QString &action);
    void ITVRestart(uint chanid, uint cardid, bool isLiveTV);

    // Edit mode stuff
    bool EnableEdit(void);
    bool HandleProgramEditorActions(QStringList &actions, long long frame = -1);
    bool GetEditMode(void) { return deleteMap.IsEditing(); }
    void DisableEdit(bool save = true);
    bool IsInDelete(uint64_t frame);
    uint64_t GetNearestMark(uint64_t frame, bool right);
    bool IsTemporaryMark(uint64_t frame);
    bool HasTemporaryMark(void);

    // Reinit
    void ReinitOSD(void);

    // OSD conveniences
    void SetOSDMessage(const QString &msg, OSDTimeout timeout);
    void SetOSDStatus(const QString &title, OSDTimeout timeout);

    // Closed caption and teletext stuff
    void ResetCaptions(void);
    bool ToggleCaptions(void);
    bool ToggleCaptions(uint mode);
    bool HasTextSubtitles(void)        { return subReader.HasTextSubtitles(); }
    void SetCaptionsEnabled(bool, bool osd_msg=true);
    virtual void DisableCaptions(uint mode, bool osd_msg=true);
    virtual void EnableCaptions(uint mode, bool osd_msg=true);

    // Audio/Subtitle/EIA-608/EIA-708 stream selection
    QStringList GetTracks(uint type);
    virtual int SetTrack(uint type, int trackNo);
    int  GetTrack(uint type);
    int  ChangeTrack(uint type, int dir);
    void ChangeCaptionTrack(int dir);
    int  NextCaptionTrack(int mode);

    // Teletext Menu and non-NUV teletext decoder
    void EnableTeletext(int page = 0x100);
    void DisableTeletext(void);
    void ResetTeletext(void);
    bool HandleTeletextAction(const QString &action);

    // Teletext NUV Captions
    void SetTeletextPage(uint page);

    // Time Code adjustment stuff
    int64_t AdjustAudioTimecodeOffset(int64_t v)
        { tc_wrap[TC_AUDIO] += v;  return tc_wrap[TC_AUDIO]; }
    int64_t ResetAudioTimecodeOffset(void)
        { tc_wrap[TC_AUDIO] = 0LL; return tc_wrap[TC_AUDIO]; }
    int64_t ResyncAudioTimecodeOffset(void)
        { tc_wrap[TC_AUDIO] = INT64_MIN; return 0L; }
    int64_t GetAudioTimecodeOffset(void) const
        { return tc_wrap[TC_AUDIO]; }
    void SaveAudioTimecodeOffset(int64_t v)
        { savedAudioTimecodeOffset = v; }

    // Playback (output) zoom automation
    DetectLetterbox *detect_letter_box;

  protected:
    // Private initialization stuff
    void InitFilters(void);
    FrameScanType detectInterlace(FrameScanType newScan, FrameScanType scan,
                                  float fps, int video_height);
    virtual void AutoDeint(VideoFrame* frame, bool allow_lock = true);

    // Private Sets
    void SetPlayingInfo(const ProgramInfo &pginfo);
    void SetPlaying(bool is_playing);
    void SetErrored(const QString &reason) const;

    // Private Gets
    int  GetStatusbarPos(void) const;

    // Private pausing stuff
    void PauseVideo(void);
    void UnpauseVideo(void);
    void PauseBuffer(void);
    void UnpauseBuffer(void);

    // Private decoder stuff
    virtual void CreateDecoder(char *testbuf, int testreadsize,
                               bool allow_libmpeg2, bool no_accel);
    void  SetDecoder(DecoderBase *dec);
    /// Returns the stream decoder currently in use.
    const DecoderBase *GetDecoder(void) const { return decoder; }
    bool DecodeFrame(struct rtframeheader *frameheader,
                     unsigned char *strm, unsigned char *outbuf);

    virtual bool DecoderGetFrameFFREW(void);
    virtual bool DecoderGetFrameREW(void);
    bool         DecoderGetFrame(DecodeType, bool unsafe = false);

    // These actually execute commands requested by public members
    virtual void ChangeSpeed(void);
    bool DoFastForward(uint64_t frames, bool override_seeks = false,
                       bool seeks_wanted = false);
    bool DoRewind(uint64_t frames, bool override_seeks = false,
                  bool seeks_wanted = false);
    void DoJumpToFrame(uint64_t frame, bool override_seeks = false,
                       bool seeks_wanted = false);

    // Private seeking stuff
    void WaitForSeek(uint64_t frame, bool override_seeks = false,
                     bool seeks_wanted = false);
    void ClearAfterSeek(bool clearvideobuffers = true);

    // Private chapter stuff
    bool DoJumpChapter(int chapter);
    virtual int64_t GetChapter(int chapter);

    // Private edit stuff
    void HandleArbSeek(bool right);

    // Private A/V Sync Stuff
    void  WrapTimecode(int64_t &timecode, TCTypes tc_type);
    void  InitAVSync(void);
    virtual void AVSync(VideoFrame *buffer, bool limit_delay = false);
    void  FallbackDeint(void);
    void  CheckExtraAudioDecode(void);

    // Private LiveTV stuff
    void  SwitchToProgram(void);
    void  JumpToProgram(void);

    void calcSliderPosPriv(osdInfo &info, bool paddedFields,
                           int playbackLen, float secsplayed, bool islive);

  protected:
    DecoderBase   *decoder;
    QMutex         decoder_change_lock;
    VideoOutput   *videoOutput;
    PlayerContext *player_ctx;
    DecoderThread *decoderThread;
    QThread       *playerThread;
    bool           no_hardware_decoders;

    // Window stuff
    QWidget *parentWidget;
    WId embedid;
    int embx, emby, embw, embh;

    // State
    QWaitCondition decoderThreadPause;
    QWaitCondition decoderThreadUnpause;
    mutable QMutex decoderPauseLock;
    mutable QMutex decoderSeekLock;
    bool           totalDecoderPause;
    bool           decoderPaused;
    bool           pauseDecoder;
    bool           unpauseDecoder;
    bool           killdecoder;
    int64_t        decoderSeek;
    bool           decodeOneFrame;
    bool           needNewPauseFrame;
    mutable QMutex bufferPauseLock;
    mutable QMutex videoPauseLock;
    mutable QMutex pauseLock;
    bool           bufferPaused;
    bool           videoPaused;
    bool           allpaused;
    bool           playing;

    mutable QWaitCondition playingWaitCond;
    mutable QMutex vidExitLock;
    mutable QMutex playingLock;
    bool     m_double_framerate;///< Output fps is double Video (input) rate
    bool     m_double_process;///< Output filter must processed at double rate
    bool     m_can_double;    ///< VideoOutput capable of doubling frame rate
    bool     m_deint_possible;
    bool     livetv;
    bool     watchingrecording;
    bool     using_null_videoout;
    bool     transcoding;
    bool     hasFullPositionMap;
    mutable bool     limitKeyRepeat;
    mutable QMutex   errorLock;
    mutable QString  errorMsg;   ///< Reason why NVP exited with a error
    mutable int errorType;

    // Chapter stuff
    int jumpchapter;

    // Bookmark stuff
    uint64_t bookmarkseek;

    // Seek
    /// If fftime>0, number of frames to seek forward.
    /// If fftime<0, number of frames to seek backward.
    long long fftime;
    /// Iff true we ignore seek amount and try to seek to an
    /// exact frame ignoring key frame restrictions.
    bool     exactseeks;

    // Playback misc.
    /// How often we have tried to wait for a video output buffer and failed
    int       videobuf_retries;
    uint64_t  framesPlayed;
    uint64_t  totalFrames;
    long long totalLength;
    int64_t   totalDuration;
    long long rewindtime;

    // -- end state stuff --


    // Input Video Attributes
    QSize    video_disp_dim;  ///< Video (input) width & height
    QSize    video_dim;       ///< Video (input) buffer width & height
    double   video_frame_rate;///< Video (input) Frame Rate (often inaccurate)
    float    video_aspect;    ///< Video (input) Apect Ratio
    float    forced_video_aspect;
    /// Video (input) Scan Type (interlaced, progressive, detect, ignore...)
    FrameScanType m_scan;
    /// Set when the user selects a scan type, overriding the detected one
    bool     m_scan_locked;
    /// Used for tracking of scan type for auto-detection of interlacing
    int      m_scan_tracker;
    /// Set when SetScanType runs the first time
    bool     m_scan_initialized;
    /// Video (input) Number of frames between key frames (often inaccurate)
    uint     keyframedist;
    /// Stream has no video tracks
    bool     noVideoTracks;
    // Buffering
    bool     buffering;
    QTime    buffering_start;
    // General Caption/Teletext/Subtitle support
    uint     textDisplayMode;
    uint     prevTextDisplayMode;

    // Support for analog captions and teletext
    // (i.e. Vertical Blanking Interval (VBI) encoded data.)
    uint     vbimode;         ///< VBI decoder to use
    int      ttPageNum;       ///< VBI page to display when in PAL vbimode

    // Support for captions, teletext, etc. decoded by libav
    SubtitleReader subReader;
    TeletextReader ttxReader;
    /// This allows us to enable captions/subtitles later if the streams
    /// are not immediately available when the video starts playing.
    bool      textDesired;
    bool      enableCaptions;
    bool      disableCaptions;

    // CC608/708
    bool db_prefer708;
    CC608Reader cc608;
    CC708Reader cc708;

    // Support for MHEG/MHI
    bool       itvVisible;
    InteractiveTV *interactiveTV;
    bool       itvEnabled;
    QMutex     itvLock;

    // OSD stuff
    OSD  *osd;
    bool  reinit_osd;
    QMutex osdLock;

    // Audio stuff
    AudioPlayer audio;

    // Picture-in-Picture
    PIPMap         pip_players;
    volatile bool  pip_active;
    volatile bool  pip_visible;

    // Filters
    QMutex   videofiltersLock;
    QString  videoFiltersForProgram;
    QString  videoFiltersOverride;
    int      postfilt_width;  ///< Post-Filter (output) width
    int      postfilt_height; ///< Post-Filter (output) height
    FilterChain   *videoFilters;
    FilterManager *FiltMan;

    // Commercial filtering
    CommBreakMap   commBreakMap;
    bool       forcePositionMapSync;
    // Manual editing
    DeleteMap  deleteMap;
    bool       pausedBeforeEdit;
    QTime      editUpdateTimer;
    float      speedBeforeEdit;

    // Playback (output) speed control
    /// Lock for next_play_speed and next_normal_speed
    QMutex     decoder_lock;
    float      next_play_speed;
    bool       next_normal_speed;

    float      play_speed;
    bool       normal_speed;
    int        frame_interval;///< always adjusted for play_speed
    int        m_frame_interval;///< used to detect changes to frame_interval

    int        ffrew_skip;
    int        ffrew_adjust;

    // Audio and video synchronization stuff
    VideoSync *videosync;
    int        avsync_delay;
    int        avsync_adjustment;
    int        avsync_avg;
    int        refreshrate;
    bool       lastsync;
    bool       decode_extra_audio;
    int        repeat_delay;
    int64_t    disp_timecode;
    bool       avsync_audiopaused;

    // Time Code stuff
    int        prevtc;        ///< 32 bit timecode if last VideoFrame shown
    int        prevrp;        ///< repeat_pict of last frame
    int64_t    tc_wrap[TCTYPESMAX];
    int64_t    tc_lastval[TCTYPESMAX];
    int64_t    savedAudioTimecodeOffset;

    // LiveTV
    TV *m_tv;
    bool isDummy;

    // Debugging variables
    Jitterometer *output_jmeter;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
