#ifndef MYTHPLAYER_H
#define MYTHPLAYER_H

#include <cstdint>

#include <QCoreApplication>
#include <QList>
#include <QMutex>                       // for QMutex
#include <QTime>                        // for QTime
#include <QString>                      // for QString
#include <QRect>                        // for QRect
#include <QSize>                        // for QSize
#include <QStringList>                  // for QStringList
#include <QWaitCondition>               // for QWaitCondition

#include "playercontext.h"
#include "volumebase.h"
#include "osd.h"
#include "videooutbase.h"
#include "teletextreader.h"
#include "subtitlereader.h"
#include "cc608reader.h"
#include "cc708reader.h"
#include "decoderbase.h"
#include "deletemap.h"
#include "commbreakmap.h"
#include "audioplayer.h"
#include "audiooutputgraph.h"
#include "mthread.h"                    // for MThread
#include "mythavutil.h"                 // for VideoFrame
#include "mythtypes.h"                  // for InfoMap
#include "programtypes.h"               // for frm_dir_map_t, etc
#include "tv.h"                         // for CommSkipMode
#include "videoouttypes.h"              // for FrameScanType, PIPLocation, etc

#include "mythtvexp.h"

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
class Jitterometer;
class QThread;
class QWidget;
class RingBuffer;

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
    kDisplayAllCaptions         = 0x0FF,
    kDisplayTeletextMenu        = 0x100,
    kDisplayAllTextCaptions     = ~kDisplayDVDButton &
                                   kDisplayAllCaptions,
};

enum PlayerFlags
{
    kNoFlags              = 0x000000,
    kDecodeLowRes         = 0x000001,
    kDecodeSingleThreaded = 0x000002,
    kDecodeFewBlocks      = 0x000004,
    kDecodeNoLoopFilter   = 0x000008,
    kDecodeNoDecode       = 0x000010,
    kDecodeDisallowCPU    = 0x000020, // NB CPU always available by default
    kDecodeAllowGPU       = 0x000040, // VDPAU, VAAPI, DXVA2
    kDecodeAllowEXT       = 0x000080, // VDA, CrystalHD
    kVideoIsNull          = 0x000100,
    kAudioMuted           = 0x010000,
    kNoITV                = 0x020000,
    kMusicChoice          = 0x040000,
};

#define FlagIsSet(arg) (playerFlags & arg)

class DecoderThread : public MThread
{
  public:
    DecoderThread(MythPlayer *mp, bool start_paused)
      : MThread("Decoder"), m_mp(mp), m_start_paused(start_paused) { }
    ~DecoderThread() { wait(); }

  protected:
    void run(void) override; // MThread

  private:
    MythPlayer *m_mp;
    bool        m_start_paused;
};

class MTV_PUBLIC MythPlayer
{
    Q_DECLARE_TR_FUNCTIONS(MythPlayer)

    // Do NOT add a decoder class to this list
    friend class PlayerContext;
    friend class CC708Reader;
    friend class CC608Reader;
    friend class DecoderThread;
    friend class DetectLetterbox;
    friend class TeletextScreen;
    friend class SubtitleScreen;
    friend class InteractiveScreen;
    friend class BDOverlayScreen;
    friend class VideoPerformanceTest;
    // TODO remove these
    friend class TV;
    friend class Transcode;

  public:
    explicit MythPlayer(PlayerFlags flags = kNoFlags);
    MythPlayer(const MythPlayer& rhs);
    virtual ~MythPlayer();

    // Initialisation
    virtual int OpenFile(uint retries = 4);
    bool InitVideo(void);

    // Public Sets
    void SetPlayerInfo(TV *tv, QWidget *widget, PlayerContext *ctx);
    void SetLength(int len)                   { totalLength = len; }
    void SetFramesPlayed(uint64_t played);
    void SetVideoFilters(const QString &overridefilter);
    void SetEof(EofState eof);
    void SetPIPActive(bool is_active)         { pip_active = is_active; }
    void SetPIPVisible(bool is_visible)       { pip_visible = is_visible; }

    void SetTranscoding(bool value);
    void SetWatchingRecording(bool mode);
    void SetWatched(bool forceWatched = false);
    void SetKeyframeDistance(int keyframedistance);
    void SetVideoParams(int w, int h, double fps,
           FrameScanType scan = kScan_Ignore, const QString& codecName = QString());
    void SetFileLength(int total, int frames);
    void SetDuration(int duration);
    void SetVideoResize(const QRect &videoRect);
    void EnableFrameRateMonitor(bool enable = false);
    void ForceDeinterlacer(const QString &override = QString());
    void SetFrameRate(double fps);

    // Gets
    QSize   GetVideoBufferSize(void) const    { return video_dim; }
    QSize   GetVideoSize(void) const          { return video_disp_dim; }
    float   GetVideoAspect(void) const        { return video_aspect; }
    float   GetFrameRate(void) const          { return video_frame_rate; }
    void    GetPlaybackData(InfoMap &infoMap);
    bool    IsAudioNeeded(void)
        { return !(FlagIsSet(kVideoIsNull)) && player_ctx->IsAudioNeeded(); }
    uint    GetVolume(void) { return audio.GetVolume(); }
    int     GetSecondsBehind(void) const;
    int     GetFreeVideoFrames(void) const;
    AspectOverrideMode GetAspectOverride(void) const;
    AdjustFillMode     GetAdjustFill(void) const;
    MuteState          GetMuteState(void) { return audio.GetMuteState(); }

    int     GetFFRewSkip(void) const          { return ffrew_skip; }
    float   GetPlaySpeed(void) const          { return play_speed; }
    AudioPlayer* GetAudio(void)               { return &audio; }
    const AudioOutputGraph& GetAudioGraph() const { return m_audiograph; }
    float   GetAudioStretchFactor(void)       { return audio.GetStretchFactor(); }
    float   GetNextPlaySpeed(void) const      { return next_play_speed; }
    int     GetLength(void) const             { return totalLength; }
    uint64_t GetTotalFrameCount(void) const   { return totalFrames; }
    uint64_t GetCurrentFrameCount(void) const;
    uint64_t GetFramesPlayed(void) const      { return framesPlayed; }
    // GetSecondsPlayed() and GetTotalSeconds() internally calculate
    // in terms of milliseconds and divide the result by 1000.  This
    // divisor can be passed in as an argument, e.g. pass divisor=1 to
    // return the time in milliseconds.
    virtual  int64_t GetSecondsPlayed(bool honorCutList,
                                      int divisor = 1000) const;
    virtual  int64_t GetTotalSeconds(bool honorCutList,
                                     int divisor = 1000) const;
    int64_t  GetLatestVideoTimecode() const   { return m_latestVideoTimecode; }
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
    bool    IsPaused(void) const              { return allpaused;      }
    bool    GetRawAudioState(void) const;
    bool    GetLimitKeyRepeat(void) const     { return limitKeyRepeat; }
    EofState GetEof(void) const;
    bool    IsErrored(void) const;
    bool    IsPlaying(uint wait_in_msec = 0, bool wait_for = true) const;
    bool    AtNormalSpeed(void) const         { return next_normal_speed; }
    bool    IsReallyNearEnd(void) const;
    bool    IsNearEnd(void);
    bool    HasAudioOut(void) const           { return audio.HasAudioOut(); }
    bool    IsPIPActive(void) const           { return pip_active; }
    bool    IsPIPVisible(void) const          { return pip_visible; }
    bool    IsMuted(void)                     { return audio.IsMuted(); }
    bool    PlayerControlsVolume(void) const  { return audio.ControlsVolume(); }
    bool    UsingNullVideo(void) const { return FlagIsSet(kVideoIsNull); }
    bool    HasTVChainNext(void) const;
    bool    CanSupportDoubleRate(void);
    bool    GetScreenShot(int width = 0, int height = 0, QString filename = "");
    bool    IsWatchingInprogress(void) const;

    // Non-const gets
    virtual char *GetScreenGrabAtFrame(uint64_t frameNum, bool absolute,
                                       int &bufflen, int &vw, int &vh, float &ar);
    virtual char *GetScreenGrab(int secondsin, int &bufflen,
                                int &vw, int &vh, float &ar);
    InteractiveTV *GetInteractiveTV(void);
    VideoOutput *GetVideoOutput(void)       { return videoOutput; }
    MythCodecContext *GetMythCodecContext(void) { return decoder->GetMythCodecContext(); }

    // Title stuff
    virtual bool SwitchTitle(int /*title*/) { return false; }
    virtual bool NextTitle(void) { return false; }
    virtual bool PrevTitle(void) { return false; }

    // Angle stuff
    virtual bool SwitchAngle(int /*title*/) { return false; }
    virtual bool NextAngle(void) { return false; }
    virtual bool PrevAngle(void) { return false; }

    // Transcode stuff
    void InitForTranscode(bool copyaudio, bool copyvideo);
    bool TranscodeGetNextFrame(int &did_ff, bool &is_key, bool honorCutList);
    bool WriteStoredData(
        RingBuffer *outRingBuffer, bool writevideo, long timecodeOffset);
    long UpdateStoredFrameNum(long curFrameNum);
    void SetCutList(const frm_dir_map_t &newCutList);

    // Decoder stuff..
    VideoFrame *GetNextVideoFrame(void);
    VideoFrame *GetRawVideoFrame(long long frameNumber = -1);
    VideoFrame *GetCurrentFrame(int &w, int &h);
    void DeLimboFrame(VideoFrame *frame);
    virtual void ReleaseNextVideoFrame(VideoFrame *buffer, int64_t timecode,
                                       bool wrap = true);
    void ReleaseCurrentFrame(VideoFrame *frame);
    void ClearDummyVideoFrame(VideoFrame *frame);
    void DiscardVideoFrame(VideoFrame *buffer);
    void DiscardVideoFrames(bool next_frame_keyframe);
    void DrawSlice(VideoFrame *frame, int x, int y, int w, int h);
    /// Returns the stream decoder currently in use.
    DecoderBase *GetDecoder(void) { return decoder; }
    void *GetDecoderContext(unsigned char* buf, uint8_t*& id);
    virtual bool HasReachedEof(void) const;
    void SetDisablePassThrough(bool disabled);
    void ForceSetupAudioStream(void);

    // Preview Image stuff
    void SaveScreenshot(void);

    // Reinit
    void ReinitVideo(void);

    // Add data
    virtual bool PrepareAudioSample(int64_t &timecode);

    // Public Closed caption and teletext stuff
    uint GetCaptionMode(void) const    { return textDisplayMode; }
    virtual CC708Reader    *GetCC708Reader(uint /*id*/=0) { return &cc708; }
    virtual CC608Reader    *GetCC608Reader(uint /*id*/=0) { return &cc608; }
    virtual SubtitleReader *GetSubReader(uint /*id*/=0) { return &subReader; }
    virtual TeletextReader *GetTeletextReader(uint /*id*/=0) { return &ttxReader; }

    // Public Audio/Subtitle/EIA-608/EIA-708 stream selection - thread safe
    void TracksChanged(uint trackType);
    void EnableSubtitles(bool enable);
    void EnableForcedSubtitles(bool enable);
    bool ForcedSubtitlesFavored(void) const {
        return allowForcedSubtitles && !captionsEnabledbyDefault;
    }
    // How to handle forced Subtitles (i.e. when in a movie someone speaks
    // in a different language than the rest of the movie, subtitles are
    // forced on even if the user doesn't have them turned on.)
    // These two functions are not thread-safe (UI thread use only).
    void SetAllowForcedSubtitles(bool allow);
    bool GetAllowForcedSubtitles(void) const { return allowForcedSubtitles; }

    // Public MHEG/MHI stream selection
    bool SetAudioByComponentTag(int tag);
    bool SetVideoByComponentTag(int tag);
    bool SetStream(const QString &);
    long GetStreamPos(); // mS
    long GetStreamMaxPos(); // mS
    long SetStreamPos(long); // mS
    void StreamPlay(bool play = true);

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
    virtual int GetTitleDuration(int /*title*/) const { return 0; }
    virtual QString GetTitleName(int /*title*/) const { return QString(); }

    // Angle public stuff
    virtual int GetNumAngles(void) const { return 0; }
    virtual int GetCurrentAngle(void) const { return 0; }
    virtual QString GetAngleName(int /*title*/) const { return QString(); }

    // DVD public stuff
    virtual bool GoToMenu(QString /*str*/)      { return false;     }
    virtual void GoToDVDProgram(bool direction) { (void) direction; }
    virtual bool IsInStillFrame() const         { return false;     }

    // Position Map Stuff
    bool PosMapFromEnc(uint64_t start,
                       frm_pos_map_t &posMap,
                       frm_pos_map_t &durMap);

    // OSD locking for TV class
    bool TryLockOSD(void) { return osdLock.tryLock(50); }
    void LockOSD(void)    { osdLock.lock();   }
    void UnlockOSD(void)  { osdLock.unlock(); }

    // Public picture controls
    void ToggleStudioLevels(void);
    void ToggleNightMode(void);

    // Visualisations
    bool CanVisualise(void);
    bool IsVisualising(void);
    QString GetVisualiserName(void);
    QStringList GetVisualiserList(void);
    bool EnableVisualisation(bool enable, const QString &name = QString(""));

    void SaveTotalDuration(void);
    void ResetTotalDuration(void);

    static const int kNightModeBrightenssAdjustment;
    static const int kNightModeContrastAdjustment;
    static const double kInaccuracyNone;
    static const double kInaccuracyDefault;
    static const double kInaccuracyEditor;
    static const double kInaccuracyFull;

    void SaveTotalFrames(void);
    void SetErrored(const QString &reason);

  protected:
    // Initialization
    void OpenDummy(void);

    // Non-public sets
    virtual void SetBookmark(bool clear = false);
    bool AddPIPPlayer(MythPlayer *pip, PIPLocation loc);
    bool RemovePIPPlayer(MythPlayer *pip);
    void NextScanType(void)
        { SetScanType((FrameScanType)(((int)m_scan + 1) & 0x3)); }
    void SetScanType(FrameScanType);
    FrameScanType GetScanType(void) const { return m_scan; }
    bool IsScanTypeLocked(void) const { return m_scan_locked; }
    void Zoom(ZoomDirection direction);
    void ToggleMoveBottomLine(void);
    void SaveBottomLine(void);
    void FileChanged(void);


    // Windowing stuff
    void EmbedInWidget(QRect rect);
    void StopEmbedding(void);
    void ExposeEvent(void);
    bool IsEmbedding(void);
    void WindowResized(const QSize &new_size);

    // Audio Sets
    uint AdjustVolume(int change)           { return audio.AdjustVolume(change); }
    uint SetVolume(int newvolume)           { return audio.SetVolume(newvolume); }
    bool SetMuted(bool mute)                { return audio.SetMuted(mute);       }
    MuteState SetMuteState(MuteState state) { return audio.SetMuteState(state);  }
    MuteState IncrMuteState(void)           { return audio.IncrMuteState();      }

    // Non-const gets
    OSD         *GetOSD(void)               { return osd;         }
    virtual void SeekForScreenGrab(uint64_t &number, uint64_t frameNum,
                                   bool absolute);

    // Complicated gets
    virtual long long CalcMaxFFTime(long long ff, bool setjump = true) const;
    long long CalcRWTime(long long rw) const;
    virtual void calcSliderPos(osdInfo &info, bool paddedFields = false);
    uint64_t TranslatePositionFrameToMs(uint64_t position,
                                        bool use_cutlist) const;
    uint64_t TranslatePositionMsToFrame(uint64_t position,
                                        bool use_cutlist) const {
        return deleteMap.TranslatePositionMsToFrame(position,
                                                    GetFrameRate(),
                                                    use_cutlist);
    }
    // TranslatePositionAbsToRel and TranslatePositionRelToAbs are
    // used for frame calculations when seeking relative to a number
    // of frames rather than by time.
    uint64_t TranslatePositionAbsToRel(uint64_t position) const {
        return deleteMap.TranslatePositionAbsToRel(position);
    }
    uint64_t TranslatePositionRelToAbs(uint64_t position) const {
        return deleteMap.TranslatePositionRelToAbs(position);
    }
    float ComputeSecs(uint64_t position, bool use_cutlist) const {
        return TranslatePositionFrameToMs(position, use_cutlist) / 1000.0;
    }
    uint64_t FindFrame(float offset, bool use_cutlist) const;

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
    virtual bool PrebufferEnoughFrames(int min_buffers = 0);
    void         SetBuffering(bool new_buffering);
    void         RefreshPauseFrame(void);
    void         CheckAspectRatio(VideoFrame* frame);
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
    bool HandleProgramEditorActions(QStringList &actions);
    bool GetEditMode(void) const { return deleteMap.IsEditing(); }
    void DisableEdit(int howToSave);
    bool IsInDelete(uint64_t frame);
    uint64_t GetNearestMark(uint64_t frame, bool right);
    bool IsTemporaryMark(uint64_t frame);
    bool HasTemporaryMark(void);
    bool IsCutListSaved(void) { return deleteMap.IsSaved(); }
    bool DeleteMapHasUndo(void) { return deleteMap.HasUndo(); }
    bool DeleteMapHasRedo(void) { return deleteMap.HasRedo(); }
    QString DeleteMapGetUndoMessage(void) { return deleteMap.GetUndoMessage(); }
    QString DeleteMapGetRedoMessage(void) { return deleteMap.GetRedoMessage(); }

    // Reinit
    void ReinitOSD(void);

    // OSD conveniences
    void SetOSDMessage(const QString &msg, OSDTimeout timeout);
    void SetOSDStatus(const QString &title, OSDTimeout timeout);

    // Closed caption and teletext stuff
    void ResetCaptions(void);
    bool ToggleCaptions(void);
    bool ToggleCaptions(uint type);
    bool HasTextSubtitles(void)        { return subReader.HasTextSubtitles(); }
    void SetCaptionsEnabled(bool, bool osd_msg=true);
    bool GetCaptionsEnabled(void);
    virtual void DisableCaptions(uint mode, bool osd_msg=true);
    virtual void EnableCaptions(uint mode, bool osd_msg=true);

    // Audio/Subtitle/EIA-608/EIA-708 stream selection
    QStringList GetTracks(uint type);
    uint GetTrackCount(uint type);
    virtual int SetTrack(uint type, int trackNo);
    int  GetTrack(uint type);
    int  ChangeTrack(uint type, int dir);
    void ChangeCaptionTrack(int dir);
    bool HasCaptionTrack(int mode);
    int  NextCaptionTrack(int mode);
    void DoDisableForcedSubtitles(void);
    void DoEnableForcedSubtitles(void);

    // Teletext Menu and non-NUV teletext decoder
    void EnableTeletext(int page = 0x100);
    void DisableTeletext(void);
    void ResetTeletext(void);
    bool HandleTeletextAction(const QString &action);

    // Teletext NUV Captions
    void SetTeletextPage(uint page);

    // Time Code adjustment stuff
    int64_t AdjustAudioTimecodeOffset(int64_t v, int newsync = -9999);
    int64_t GetAudioTimecodeOffset(void) const
        { return tc_wrap[TC_AUDIO]; }

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
    void ResetErrored(void);

    // Private Gets
    int  GetStatusbarPos(void) const;

    // Private pausing stuff
    void PauseVideo(void);
    void UnpauseVideo(void);
    void PauseBuffer(void);
    void UnpauseBuffer(void);

    // Private decoder stuff
    virtual void CreateDecoder(char *testbuf, int testreadsize);
    void  SetDecoder(DecoderBase *dec);
    /// Returns the stream decoder currently in use.
    const DecoderBase *GetDecoder(void) const { return decoder; }
    bool DecodeFrame(struct rtframeheader *frameheader,
                     unsigned char *strm, unsigned char *outbuf);

    virtual bool DecoderGetFrameFFREW(void);
    virtual bool DecoderGetFrameREW(void);
    bool         DecoderGetFrame(DecodeType, bool unsafe = false);

    // These actually execute commands requested by public members
    bool UpdateFFRewSkip(void);
    virtual void ChangeSpeed(void);
    // The "inaccuracy" argument is generally one of the kInaccuracy* values.
    bool DoFastForward(uint64_t frames, double inaccuracy);
    bool DoRewind(uint64_t frames, double inaccuracy);
    bool DoFastForwardSecs(float secs, double inaccuracy, bool use_cutlist);
    bool DoRewindSecs(float secs, double inaccuracy, bool use_cutlist);
    void DoJumpToFrame(uint64_t frame, double inaccuracy);

    // Private seeking stuff
    void WaitForSeek(uint64_t frame, uint64_t seeksnap_wanted);
    void ClearAfterSeek(bool clearvideobuffers = true);

    // Private chapter stuff
    virtual bool DoJumpChapter(int chapter);
    virtual int64_t GetChapter(int chapter);

    // Private edit stuff
    void HandleArbSeek(bool right);

    // Private A/V Sync Stuff
    void  WrapTimecode(int64_t &timecode, TCTypes tc_type);
    void  InitAVSync(void);
    virtual void AVSync(VideoFrame *buffer, bool limit_delay = false);
    // New video sync method
    void AVSync2(VideoFrame *buffer);
    void  ResetAVSync(void);
    int64_t AVSyncGetAudiotime(void);
    void  SetFrameInterval(FrameScanType scan, double frame_period);
    void  FallbackDeint(void);
    void WaitForTime(int64_t framedue);

    // Private LiveTV stuff
    void  SwitchToProgram(void);
    void  JumpToProgram(void);
    void  JumpToStream(const QString&);

  protected:
    PlayerFlags    playerFlags;
    DecoderBase   *decoder              {nullptr};
    mutable QMutex decoder_change_lock  {QMutex::Recursive};
    VideoOutput   *videoOutput          {nullptr};
    PlayerContext *player_ctx           {nullptr};
    DecoderThread *decoderThread        {nullptr};
    QThread       *playerThread         {nullptr};
#ifdef Q_OS_ANDROID
    int            playerThreadId       {0};
#endif

    // Window stuff
    QWidget *parentWidget               {nullptr};
    bool     embedding                  {false};
    QRect    embedRect                  {0,0,0,0};
    float    defaultDisplayAspect       {1.7777F};

    // State
    QWaitCondition decoderThreadPause;
    QWaitCondition decoderThreadUnpause;
    mutable QMutex decoderPauseLock;
    mutable QMutex decoderSeekLock;
    bool           totalDecoderPause    {false};
    bool           decoderPaused        {false};
    bool           inJumpToProgramPause {false};
    bool           pauseDecoder         {false};
    bool           unpauseDecoder       {false};
    bool volatile  killdecoder          {false};
    int64_t        decoderSeek          {-1};
    bool           decodeOneFrame       {false};
    bool           needNewPauseFrame    {false};
    mutable QMutex bufferPauseLock;
    mutable QMutex videoPauseLock;
    mutable QMutex pauseLock;
    bool           bufferPaused         {false};
    bool           videoPaused          {false};
    bool           allpaused            {false};
    bool           playing              {false};

    mutable QWaitCondition playingWaitCond;
    mutable QMutex vidExitLock;
    mutable QMutex playingLock;
    bool     m_double_framerate         {false};///< Output fps is double Video (input) rate
    bool     m_double_process           {false};///< Output filter must processed at double rate
    bool     m_deint_possible           {true};
    bool     livetv                     {false};
    bool     watchingrecording          {false};
    bool     transcoding                {false};
    bool     hasFullPositionMap         {false};
    mutable bool     limitKeyRepeat     {false};
    mutable QMutex   errorLock;
    QString  errorMsg;   ///< Reason why NVP exited with a error
    int errorType                       {kError_None};

    // Chapter stuff
    int jumpchapter                     {0};

    // Bookmark stuff
    uint64_t bookmarkseek               {0};
    int      clearSavedPosition         {1};
    int      endExitPrompt;

    // Seek
    /// If fftime>0, number of frames to seek forward.
    /// If fftime<0, number of frames to seek backward.
    long long fftime                    {0};

    // Playback misc.
    /// How often we have tried to wait for a video output buffer and failed
    int       videobuf_retries          {0};
    uint64_t  framesPlayed              {0};
    // "Fake" frame counter for when the container frame rate doesn't
    // match the stream frame rate.
    uint64_t  framesPlayedExtra         {0};
    uint64_t  totalFrames               {0};
    long long totalLength               {0};
    int64_t   totalDuration             {0};
    long long rewindtime                {0};
    int64_t   m_latestVideoTimecode     {-1};
    QElapsedTimer m_avTimer;

    // -- end state stuff --

    // Input Video Attributes
    QSize    video_disp_dim             {0,0}; ///< Video (input) width & height
    QSize    video_dim                  {0,0}; ///< Video (input) buffer width & height
    double   video_frame_rate           {29.97F};///< Video (input) Frame Rate (often inaccurate)
    float    video_aspect               {4.0F / 3.0F};    ///< Video (input) Apect Ratio
    float    forced_video_aspect        {-1};
    /// Tell the player thread to set the scan type (and hence deinterlacers)
    FrameScanType resetScan             {kScan_Ignore};
    /// Video (input) Scan Type (interlaced, progressive, detect, ignore...)
    FrameScanType m_scan                {kScan_Interlaced};
    /// Set when the user selects a scan type, overriding the detected one
    bool     m_scan_locked              {false};
    /// Used for tracking of scan type for auto-detection of interlacing
    int      m_scan_tracker             {0};
    /// Set when SetScanType runs the first time
    bool     m_scan_initialized         {false};
    /// Video (input) Number of frames between key frames (often inaccurate)
    uint     keyframedist               {30};
    /// Codec Name - used by playback profile
    QString  m_codecName;

    // Buffering
    bool     buffering                  {false};
    QTime    buffering_start;
    QTime    buffering_last_msg;

    // General Caption/Teletext/Subtitle support
    uint     textDisplayMode            {kDisplayNone};
    uint     prevTextDisplayMode        {kDisplayNone};
    uint     prevNonzeroTextDisplayMode {kDisplayNone};

    // Support for analog captions and teletext
    // (i.e. Vertical Blanking Interval (VBI) encoded data.)
    uint     vbimode                    {VBIMode::None}; ///< VBI decoder to use
    int      ttPageNum                  {0x888}; ///< VBI page to display when in PAL vbimode

    // Support for captions, teletext, etc. decoded by libav
    SubtitleReader subReader;
    TeletextReader ttxReader;
    /// This allows us to enable captions/subtitles later if the streams
    /// are not immediately available when the video starts playing.
    bool      captionsEnabledbyDefault  {false};
    bool      textDesired               {false};
    bool      enableCaptions            {false};
    bool      disableCaptions           {false};
    bool      enableForcedSubtitles     {false};
    bool      disableForcedSubtitles    {false};
    bool      allowForcedSubtitles      {true};

    // CC608/708
    CC608Reader cc608;
    CC708Reader cc708;

    // Support for MHEG/MHI
    bool       itvVisible               {false};
    InteractiveTV *interactiveTV        {nullptr};
    bool       itvEnabled               {false};
    QMutex     itvLock;
    QMutex     streamLock;
    QString    m_newStream; // Guarded by streamLock

    // OSD stuff
    OSD  *osd                           {nullptr};
    bool  reinit_osd                    {false};
    QMutex osdLock                      {QMutex::Recursive};

    // Audio stuff
    AudioPlayer audio;
    AudioOutputGraph m_audiograph;

    // Picture-in-Picture
    PIPMap         pip_players;
    volatile bool  pip_active           {false};
    volatile bool  pip_visible          {true};
    PIPLocation    pip_default_loc;

    // Filters
    QMutex   videofiltersLock;
    QString  videoFiltersForProgram     {};
    QString  videoFiltersOverride       {};
    int      postfilt_width             {0}; ///< Post-Filter (output) width
    int      postfilt_height            {0}; ///< Post-Filter (output) height
    FilterChain   *videoFilters         {nullptr};
    FilterManager *FiltMan              {nullptr};

    // Commercial filtering
    CommBreakMap   commBreakMap;
    bool       forcePositionMapSync     {false};
    // Manual editing
    DeleteMap  deleteMap;
    bool       pausedBeforeEdit         {false};
    QTime      editUpdateTimer;
    float      speedBeforeEdit          {1.0F};

    // Playback (output) speed control
    /// Lock for next_play_speed and next_normal_speed
    QMutex     decoder_lock             {QMutex::Recursive};
    float      next_play_speed          {1.0F};
    bool       next_normal_speed        {true};

    float      play_speed               {1.0F};
    bool       normal_speed             {true};
    int        frame_interval           {(int)(1000000.0F / 30)};///< always adjusted for play_speed
    int        m_frame_interval         {0}; ///< used to detect changes to frame_interval
    int        m_fpsMultiplier          {1}; ///< used to detect changes

    int        ffrew_skip               {1};
    int        ffrew_adjust             {0};
    bool       fileChanged              {false};

    // Audio and video synchronization stuff
    VideoSync *videosync                {nullptr};
    int        avsync_delay             {0};
    int        avsync_adjustment        {0};
    int        avsync_avg               {0};
    int        avsync_predictor         {0};
    bool       avsync_predictor_enabled {false};
    int        refreshrate              {0};
    bool       lastsync                 {false};
    bool       decode_extra_audio       {false};
    int        repeat_delay             {0};
    int64_t    disp_timecode            {0};
    bool       avsync_audiopaused       {false};
    float      max_diverge              {3.0F};  // from setting PlayerMaxDiverge default 2
    // AVSync for Raspberry Pi digital streams
    int        avsync_averaging         {4}; // Number of frames to average
    int        avsync_interval          {0}; // Number of frames skip between sync checks
    int        avsync_next              {0}; // Frames till next sync check

    // Time Code stuff
    int        prevtc                   {0}; ///< 32 bit timecode if last VideoFrame shown
    int        prevrp                   {0}; ///< repeat_pict of last frame
    int64_t    tc_wrap[TCTYPESMAX]      {};
    int64_t    tc_lastval[TCTYPESMAX]   {};
    int64_t    savedAudioTimecodeOffset {0};

    // AVSync2
    int64_t   rtcbase                   {0}; // real time clock base for presentation time (microsecs)
    int64_t   maxtcval                  {0}; // maximum to date video tc
    int       maxtcframes               {0}; // number of frames seen since max to date tc
    int64_t   avsync2adjustms           {10};// number of milliseconds to adjust for av sync errors
    int       numdroppedframes          {0}; // number of consecutive dropped frames.
    int64_t   prior_audiotimecode       {0}; // time code from prior frame
    int64_t   prior_videotimecode       {0}; // time code from prior frame
    int64_t   m_timeOffsetBase          {0};

    // LiveTV
    TV *m_tv                            {nullptr};
    bool isDummy                        {false};

    // Counter for buffering messages
    int  bufferingCounter               {0};

    // Debugging variables
    Jitterometer *output_jmeter         {nullptr};

  private:
    void syncWithAudioStretch();
    bool disable_passthrough            {false};
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
