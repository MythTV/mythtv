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
#include "mythvideoout.h"
#include "teletextreader.h"
#include "subtitlereader.h"
#include "cc608reader.h"
#include "cc708reader.h"
#include "decoders/decoderbase.h"
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

class MythVideoOutput;
class RemoteEncoder;
class MythSqlDatabase;
class ProgramInfo;
class DecoderBase;
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

using StatusCallback = void (*)(int, void*);

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

#define FlagIsSet(arg) (m_playerFlags & arg)

class DecoderThread : public MThread
{
  public:
    DecoderThread(MythPlayer *mp, bool start_paused)
      : MThread("Decoder"), m_mp(mp), m_startPaused(start_paused) { }
    ~DecoderThread() override { wait(); }

  protected:
    void run(void) override; // MThread

  private:
    MythPlayer *m_mp;
    bool        m_startPaused;
};

class MythMultiLocker
{
  public:
    MythMultiLocker(std::initializer_list<QMutex*> Locks);
    MythMultiLocker() = delete;
   ~MythMultiLocker();

    void Unlock(void);
    void Relock(void);

  private:
    Q_DISABLE_COPY(MythMultiLocker)
    QVector<QMutex*> m_locks;
};

class DecoderCallback
{
  public:
    using Callback = void (*)(void*, void*, void*);
    DecoderCallback() { }
    DecoderCallback(QString &Debug, Callback Function, void *Opaque1, void *Opaque2, void *Opaque3)
      : m_debug(std::move(Debug)),
        m_function(Function),
        m_opaque1(Opaque1),
        m_opaque2(Opaque2),
        m_opaque3(Opaque3)
    {
    }

    QString m_debug;
    Callback m_function { nullptr };
    void* m_opaque1     { nullptr };
    void* m_opaque2     { nullptr };
    void* m_opaque3     { nullptr };
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
    virtual int OpenFile(int Retries = 4);
    bool InitVideo(void);

    // Public Sets
    void SetPlayerInfo(TV *tv, QWidget *widget, PlayerContext *ctx);
    void SetLength(int len)                   { m_totalLength = len; }
    void SetFramesPlayed(uint64_t played);
    void SetEof(EofState eof);
    void SetPIPActive(bool is_active)         { m_pipActive = is_active; }
    void SetPIPVisible(bool is_visible)       { m_pipVisible = is_visible; }

    void SetTranscoding(bool value);
    void SetWatchingRecording(bool mode);
    void SetWatched(bool forceWatched = false);
    void SetKeyframeDistance(int keyframedistance);
    void SetVideoParams(int w, int h, double fps, float aspect,
                        bool ForceUpdate, int ReferenceFrames,
                        FrameScanType scan = kScan_Ignore,
                        const QString& codecName = QString());
    void SetFileLength(int total, int frames);
    void SetDuration(int duration);
    void SetVideoResize(const QRect &videoRect);
    void EnableFrameRateMonitor(bool enable = false);
    void ForceDeinterlacer(bool DoubleRate, MythDeintType Deinterlacer);
    void SetFrameRate(double fps);

    // Gets
    QSize   GetVideoBufferSize(void) const    { return m_videoDim; }
    QSize   GetVideoSize(void) const          { return m_videoDispDim; }
    float   GetVideoAspect(void) const        { return m_videoAspect; }
    float   GetFrameRate(void) const          { return m_videoFrameRate; }
    void    GetPlaybackData(InfoMap &infoMap);
    bool    IsAudioNeeded(void)
        { return !(FlagIsSet(kVideoIsNull)) && m_playerCtx->IsAudioNeeded(); }
    uint    GetVolume(void) { return m_audio.GetVolume(); }
    int     GetFreeVideoFrames(void) const;
    AspectOverrideMode GetAspectOverride(void) const;
    AdjustFillMode     GetAdjustFill(void) const;
    MuteState          GetMuteState(void) { return m_audio.GetMuteState(); }

    int     GetFFRewSkip(void) const          { return m_ffrewSkip; }
    float   GetPlaySpeed(void) const          { return m_playSpeed; }
    AudioPlayer* GetAudio(void)               { return &m_audio; }
    const AudioOutputGraph& GetAudioGraph() const { return m_audiograph; }
    float   GetAudioStretchFactor(void)       { return m_audio.GetStretchFactor(); }
    float   GetNextPlaySpeed(void) const      { return m_nextPlaySpeed; }
    int     GetLength(void) const             { return m_totalLength; }
    uint64_t GetTotalFrameCount(void) const   { return m_totalFrames; }
    uint64_t GetCurrentFrameCount(void) const;
    uint64_t GetFramesPlayed(void) const      { return m_framesPlayed; }
    // GetSecondsPlayed() and GetTotalSeconds() internally calculate
    // in terms of milliseconds and divide the result by 1000.  This
    // divisor can be passed in as an argument, e.g. pass divisor=1 to
    // return the time in milliseconds.
    virtual  int64_t GetSecondsPlayed(bool honorCutList,
                                      int divisor = 1000);
    virtual  int64_t GetTotalSeconds(bool honorCutList,
                                     int divisor = 1000) const;
    int64_t  GetLatestVideoTimecode() const   { return m_latestVideoTimecode; }
    virtual  uint64_t GetBookmark(void);
    QString   GetError(void) const;
    QString   GetEncodingType(void) const;
    void      GetCodecDescription(InfoMap &infoMap);
    QString   GetXDS(const QString &key) const;
    PIPLocation GetNextPIPLocation(void) const;

    // Bool Gets
    bool    IsPaused(void) const              { return m_allPaused;      }
    bool    GetRawAudioState(void) const;
    bool    GetLimitKeyRepeat(void) const     { return m_limitKeyRepeat; }
    EofState GetEof(void) const;
    bool    IsErrored(void) const;
    bool    IsPlaying(uint wait_in_msec = 0, bool wait_for = true) const;
    bool    AtNormalSpeed(void) const         { return m_nextNormalSpeed; }
    bool    IsReallyNearEnd(void) const;
    bool    IsNearEnd(void);
    bool    HasAudioOut(void) const           { return m_audio.HasAudioOut(); }
    bool    IsPIPActive(void) const           { return m_pipActive; }
    bool    IsPIPVisible(void) const          { return m_pipVisible; }
    bool    IsMuted(void)                     { return m_audio.IsMuted(); }
    bool    PlayerControlsVolume(void) const  { return m_audio.ControlsVolume(); }
    bool    UsingNullVideo(void) const { return FlagIsSet(kVideoIsNull); }
    bool    HasTVChainNext(void) const;
    bool    CanSupportDoubleRate(void);
    bool    IsWatchingInprogress(void) const;

    // Non-const gets
    virtual char *GetScreenGrabAtFrame(uint64_t FrameNum, bool Absolute, int &BufferSize,
                                       int &FrameWidth, int &FrameHeight, float &AspectRatio);
    virtual char *GetScreenGrab(int SecondsIn, int &BufferSize, int &FrameWidth,
                                int &FrameHeight, float &AspectRatio);
    InteractiveTV *GetInteractiveTV(void);
    MythVideoOutput *GetVideoOutput(void)       { return m_videoOutput; }
    MythCodecContext *GetMythCodecContext(void) { return m_decoder->GetMythCodecContext(); }

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
    VideoFrameType* DirectRenderFormats(void);
    VideoFrame *GetNextVideoFrame(void);
    VideoFrame *GetRawVideoFrame(long long frameNumber = -1);
    VideoFrame *GetCurrentFrame(int &w, int &h);
    void DeLimboFrame(VideoFrame *frame);
    virtual void ReleaseNextVideoFrame(VideoFrame *buffer, int64_t timecode,
                                       bool wrap = true);
    void ReleaseCurrentFrame(VideoFrame *frame);
    void DiscardVideoFrame(VideoFrame *buffer);
    void DiscardVideoFrames(bool KeyFrame, bool Flushed);
    /// Returns the stream decoder currently in use.
    DecoderBase *GetDecoder(void) { return m_decoder; }
    virtual bool HasReachedEof(void) const;
    void SetDisablePassThrough(bool disabled);
    void ForceSetupAudioStream(void);
    static void HandleDecoderCallback(MythPlayer *Player, const QString &Debug,
                                      DecoderCallback::Callback Function,
                                      void *Opaque1, void *Opaque2);
    void ProcessCallbacks(void);
    void QueueCallback(QString Debug, DecoderCallback::Callback Function,
                       void *Opaque1, void *Opaque2, void *Opaque3);

    // Reinit
    void ReinitVideo(bool ForceUpdate);

    // Add data
    virtual bool PrepareAudioSample(int64_t &timecode);

    // Public Closed caption and teletext stuff
    uint GetCaptionMode(void) const    { return m_textDisplayMode; }
    virtual CC708Reader    *GetCC708Reader(uint /*id*/=0) { return &m_cc708; }
    virtual CC608Reader    *GetCC608Reader(uint /*id*/=0) { return &m_cc608; }
    virtual SubtitleReader *GetSubReader(uint /*id*/=0) { return &m_subReader; }
    virtual TeletextReader *GetTeletextReader(uint /*id*/=0) { return &m_ttxReader; }

    // Public Audio/Subtitle/EIA-608/EIA-708 stream selection - thread safe
    void TracksChanged(uint trackType);
    void EnableSubtitles(bool enable);
    void EnableForcedSubtitles(bool enable);
    bool ForcedSubtitlesFavored(void) const {
        return m_allowForcedSubtitles && !m_captionsEnabledbyDefault;
    }
    // How to handle forced Subtitles (i.e. when in a movie someone speaks
    // in a different language than the rest of the movie, subtitles are
    // forced on even if the user doesn't have them turned on.)
    // These two functions are not thread-safe (UI thread use only).
    void SetAllowForcedSubtitles(bool allow);
    bool GetAllowForcedSubtitles(void) const { return m_allowForcedSubtitles; }

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
    bool TryLockOSD(void) { return m_osdLock.tryLock(50); }
    void LockOSD(void)    { m_osdLock.lock();   }
    void UnlockOSD(void)  { m_osdLock.unlock(); }

    // Public picture controls
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
    bool IsScanTypeLocked(void) const { return m_scanLocked; }
    void Zoom(ZoomDirection direction);
    void ToggleMoveBottomLine(void);
    void SaveBottomLine(void);
    void FileChanged(void);


    // Windowing stuff
    void EmbedInWidget(QRect rect);
    void StopEmbedding(void);
    bool IsEmbedding(void);
    void WindowResized(const QSize &new_size);

    // Audio Sets
    uint AdjustVolume(int change)           { return m_audio.AdjustVolume(change); }
    uint SetVolume(int newvolume)           { return m_audio.SetVolume(newvolume); }
    bool SetMuted(bool mute)                { return m_audio.SetMuted(mute);       }
    MuteState SetMuteState(MuteState state) { return m_audio.SetMuteState(state);  }
    MuteState IncrMuteState(void)           { return m_audio.IncrMuteState();      }

    // Non-const gets
    OSD         *GetOSD(void)               { return m_osd;       }
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
        return m_deleteMap.TranslatePositionMsToFrame(position,
                                                    GetFrameRate(),
                                                    use_cutlist);
    }
    // TranslatePositionAbsToRel and TranslatePositionRelToAbs are
    // used for frame calculations when seeking relative to a number
    // of frames rather than by time.
    uint64_t TranslatePositionAbsToRel(uint64_t position) const {
        return m_deleteMap.TranslatePositionAbsToRel(position);
    }
    uint64_t TranslatePositionRelToAbs(uint64_t position) const {
        return m_deleteMap.TranslatePositionRelToAbs(position);
    }
    float ComputeSecs(uint64_t position, bool use_cutlist) const {
        return TranslatePositionFrameToMs(position, use_cutlist) / 1000.0;
    }
    uint64_t FindFrame(float offset, bool use_cutlist) const;

    // Commercial stuff
    void SetAutoCommercialSkip(CommSkipMode autoskip)
        { m_commBreakMap.SetAutoCommercialSkip(autoskip, m_framesPlayed); }
    void SkipCommercials(int direction)
        { m_commBreakMap.SkipCommercials(direction); }
    void SetCommBreakMap(frm_dir_map_t &newMap);
    CommSkipMode GetAutoCommercialSkip(void)
        { return m_commBreakMap.GetAutoCommercialSkip(); }

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
    bool GetEditMode(void) const { return m_deleteMap.IsEditing(); }
    void DisableEdit(int howToSave);
    bool IsInDelete(uint64_t frame);
    uint64_t GetNearestMark(uint64_t frame, bool right);
    bool IsTemporaryMark(uint64_t frame);
    bool HasTemporaryMark(void);
    bool IsCutListSaved(void) { return m_deleteMap.IsSaved(); }
    bool DeleteMapHasUndo(void) { return m_deleteMap.HasUndo(); }
    bool DeleteMapHasRedo(void) { return m_deleteMap.HasRedo(); }
    QString DeleteMapGetUndoMessage(void) { return m_deleteMap.GetUndoMessage(); }
    QString DeleteMapGetRedoMessage(void) { return m_deleteMap.GetRedoMessage(); }

    // Reinit
    void ReinitOSD(void);

    // OSD conveniences
    void SetOSDMessage(const QString &msg, OSDTimeout timeout);
    void SetOSDStatus(const QString &title, OSDTimeout timeout);

    // Closed caption and teletext stuff
    void ResetCaptions(void);
    bool ToggleCaptions(void);
    bool ToggleCaptions(uint type);
    bool HasTextSubtitles(void)        { return m_subReader.HasTextSubtitles(); }
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
        { return m_tcWrap[TC_AUDIO]; }

    // Playback (output) zoom automation
    DetectLetterbox *m_detectLetterBox;

  protected:
    // Private initialization stuff
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
    virtual void CreateDecoder(char *TestBuffer, int TestSize);
    void  SetDecoder(DecoderBase *dec);
    /// Returns the stream decoder currently in use.
    const DecoderBase *GetDecoder(void) const { return m_decoder; }
    bool DecodeFrame(struct rtframeheader *frameheader,
                     unsigned char *strm, unsigned char *outbuf);

    virtual bool DecoderGetFrameFFREW(void);
    virtual bool DecoderGetFrameREW(void);
    bool         DecoderGetFrame(DecodeType, bool unsafe = false);
    bool         DoGetFrame(DecodeType DecodeType);

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
    void ClearBeforeSeek(uint64_t Frames);

    // Private chapter stuff
    virtual bool DoJumpChapter(int chapter);
    virtual int64_t GetChapter(int chapter);

    // Private edit stuff
    void HandleArbSeek(bool right);

    // Private A/V Sync Stuff
    void  WrapTimecode(int64_t &timecode, TCTypes tc_type);
    void  InitAVSync(void);
    virtual void AVSync(VideoFrame *buffer);
    void  ResetAVSync(void);
    void  SetFrameInterval(FrameScanType scan, double frame_period);
    void  WaitForTime(int64_t framedue);

    // Private LiveTV stuff
    void  SwitchToProgram(void);
    void  JumpToProgram(void);
    void  JumpToStream(const QString&);

  protected:
    PlayerFlags      m_playerFlags;
    DecoderBase     *m_decoder            {nullptr};
    QMutex           m_decoderCallbackLock;
    QVector<DecoderCallback> m_decoderCallbacks;
    mutable QMutex   m_decoderChangeLock  {QMutex::Recursive};
    MythVideoOutput *m_videoOutput        {nullptr};
    PlayerContext   *m_playerCtx          {nullptr};
    DecoderThread   *m_decoderThread      {nullptr};
    QThread         *m_playerThread       {nullptr};

#ifdef Q_OS_ANDROID
    int            m_playerThreadId       {0};
#endif

    // Window stuff
    MythDisplay* m_display                {nullptr};
    QWidget *m_parentWidget               {nullptr};
    bool     m_embedding                  {false};
    QRect    m_embedRect                  {0,0,0,0};

    // State
    QWaitCondition m_decoderThreadPause;
    QWaitCondition m_decoderThreadUnpause;
    mutable QMutex m_decoderPauseLock;
    mutable QMutex m_decoderSeekLock;
    bool           m_totalDecoderPause      {false};
    bool           m_decoderPaused          {false};
    bool           m_inJumpToProgramPause   {false};
    bool           m_pauseDecoder           {false};
    bool           m_unpauseDecoder         {false};
    bool volatile  m_killDecoder            {false};
    int64_t        m_decoderSeek            {-1};
    bool           m_decodeOneFrame         {false};
    bool           m_needNewPauseFrame      {false};
    mutable QMutex m_bufferPauseLock;
    mutable QMutex m_videoPauseLock;
    mutable QMutex m_pauseLock;
    bool           m_bufferPaused           {false};
    bool           m_videoPaused            {false};
    bool           m_allPaused              {false};
    bool           m_playing                {false};

    mutable QWaitCondition m_playingWaitCond;
    mutable QMutex m_vidExitLock;
    mutable QMutex m_playingLock;
    bool     m_doubleFramerate            {false};///< Output fps is double Video (input) rate
    bool     m_liveTV                     {false};
    bool     m_watchingRecording          {false};
    bool     m_transcoding                {false};
    bool     m_hasFullPositionMap         {false};
    mutable bool     m_limitKeyRepeat     {false};
    mutable QMutex   m_errorLock;
    QString  m_errorMsg;   ///< Reason why NVP exited with a error
    int m_errorType                       {kError_None};

    // Chapter stuff
    int m_jumpChapter                     {0};

    // Bookmark stuff
    uint64_t m_bookmarkSeek               {0};
    int      m_clearSavedPosition         {1};
    int      m_endExitPrompt;

    // Seek
    /// If m_ffTime>0, number of frames to seek forward.
    /// If m_ffTime<0, number of frames to seek backward.
    long long m_ffTime                    {0};

    // Playback misc.
    /// How often we have tried to wait for a video output buffer and failed
    int       m_videobufRetries           {0};
    uint64_t  m_framesPlayed              {0};
    // "Fake" frame counter for when the container frame rate doesn't
    // match the stream frame rate.
    uint64_t  m_framesPlayedExtra         {0};
    uint64_t  m_totalFrames               {0};
    long long m_totalLength               {0};
    int64_t   m_totalDuration             {0};
    long long m_rewindTime                {0};
    int64_t   m_latestVideoTimecode       {-1};
    QElapsedTimer m_avTimer;

    // Tracks deinterlacer for Debug OSD
    MythDeintType m_lastDeinterlacer    { DEINT_NONE };
    bool      m_lastDeinterlacer2x      { false };
    VideoFrameType m_lastFrameCodec     { FMT_NONE };

    // -- end state stuff --

    // Input Video Attributes
    QSize    m_videoDispDim              {0,0}; ///< Video (input) width & height
    QSize    m_videoDim                  {0,0}; ///< Video (input) buffer width & height
    int      m_maxReferenceFrames        {0}; ///< Number of reference frames used in the video stream
    double   m_videoFrameRate            {29.97};///< Video (input) Frame Rate (often inaccurate)
    float    m_videoAspect               {4.0F / 3.0F};    ///< Video (input) Apect Ratio
    float    m_forcedVideoAspect         {-1};
    /// Tell the player thread to set the scan type (and hence deinterlacers)
    FrameScanType m_resetScan            {kScan_Ignore};
    /// Video (input) Scan Type (interlaced, progressive, detect, ignore...)
    FrameScanType m_scan                 {kScan_Interlaced};
    /// Set when the user selects a scan type, overriding the detected one
    bool     m_scanLocked                {false};
    /// Used for tracking of scan type for auto-detection of interlacing
    int      m_scanTracker               {0};
    /// Set when SetScanType runs the first time
    bool     m_scanInitialized           {false};
    /// Video (input) Number of frames between key frames (often inaccurate)
    uint     m_keyframeDist              {30};
    /// Codec Name - used by playback profile
    QString  m_codecName;

    // Buffering
    bool     m_buffering                  {false};
    QTime    m_bufferingStart;
    QTime    m_bufferingLastMsg;

    // General Caption/Teletext/Subtitle support
    uint     m_textDisplayMode            {kDisplayNone};
    uint     m_prevTextDisplayMode        {kDisplayNone};
    uint     m_prevNonzeroTextDisplayMode {kDisplayNone};

    // Support for analog captions and teletext
    // (i.e. Vertical Blanking Interval (VBI) encoded data.)
    uint     m_vbiMode                    {VBIMode::None}; ///< VBI decoder to use
    int      m_ttPageNum                  {0x888}; ///< VBI page to display when in PAL vbimode

    // Support for captions, teletext, etc. decoded by libav
    SubtitleReader m_subReader;
    TeletextReader m_ttxReader;
    /// This allows us to enable captions/subtitles later if the streams
    /// are not immediately available when the video starts playing.
    bool      m_captionsEnabledbyDefault  {false};
    bool      m_textDesired               {false};
    bool      m_enableCaptions            {false};
    bool      m_disableCaptions           {false};
    bool      m_enableForcedSubtitles     {false};
    bool      m_disableForcedSubtitles    {false};
    bool      m_allowForcedSubtitles      {true};

    // CC608/708
    CC608Reader m_cc608;
    CC708Reader m_cc708;

    // Support for MHEG/MHI
    bool       m_itvVisible               {false};
    InteractiveTV *m_interactiveTV        {nullptr};
    bool       m_itvEnabled               {false};
    QMutex     m_itvLock;
    QMutex     m_streamLock;
    QString    m_newStream; // Guarded by streamLock

    // OSD stuff
    OSD    *m_osd                         {nullptr};
    bool    m_reinitOsd                   {false};
    QMutex  m_osdLock                     {QMutex::Recursive};

    // Audio stuff
    AudioPlayer      m_audio;
    AudioOutputGraph m_audiograph;

    // Picture-in-Picture
    PIPMap         m_pipPlayers;
    volatile bool  m_pipActive            {false};
    volatile bool  m_pipVisible           {true};
    PIPLocation    m_pipDefaultLoc;

    // Commercial filtering
    CommBreakMap   m_commBreakMap;
    bool       m_forcePositionMapSync     {false};
    // Manual editing
    DeleteMap  m_deleteMap;
    bool       m_pausedBeforeEdit         {false};
    QTime      m_editUpdateTimer;
    float      m_speedBeforeEdit          {1.0F};

    // Playback (output) speed control
    /// Lock for next_play_speed and next_normal_speed
    QMutex     m_decoderLock              {QMutex::Recursive};
    float      m_nextPlaySpeed            {1.0F};
    bool       m_nextNormalSpeed        {true};

    float      m_playSpeed                {1.0F};
    bool       m_normalSpeed              {true};
    int        m_frameInterval            {static_cast<int>((1000000.0F / 30))};///< always adjusted for play_speed
    int        m_frameIntervalPrev        {0}; ///< used to detect changes to frame_interval
    int        m_fpsMultiplier            {1}; ///< used to detect changes

    int        m_ffrewSkip                {1};
    int        m_ffrewAdjust              {0};
    bool       m_fileChanged              {false};

    // Audio and video synchronization stuff
    VideoSync *m_videoSync                {nullptr};
    int        m_avsyncAvg                {0};
    int        m_avsyncPredictor          {0};
    int        m_refreshRate              {0};
    int64_t    m_dispTimecode             {0};
    bool       m_avsyncAudioPaused        {false};
    int64_t    m_rtcBase                  {0}; // real time clock base for presentation time (microsecs)
    int64_t    m_maxTcVal                 {0}; // maximum to date video tc
    int        m_maxTcFrames              {0}; // number of frames seen since max to date tc
    int        m_numDroppedFrames         {0}; // number of consecutive dropped frames.
    int64_t    m_priorAudioTimecode       {0}; // time code from prior frame
    int64_t    m_priorVideoTimecode       {0}; // time code from prior frame
    float      m_lastFix                  {0.0F}; //last sync adjustment to prior frame
    int64_t    m_timeOffsetBase           {0};

    // Time Code stuff
    int        m_prevTc                   {0}; ///< 32 bit timecode if last VideoFrame shown
    int        m_prevRp                   {0}; ///< repeat_pict of last frame
    int64_t    m_tcWrap[TCTYPESMAX]       {};
    int64_t    m_tcLastVal[TCTYPESMAX]    {};
    int64_t    m_savedAudioTimecodeOffset {0};

    // LiveTV
    TV *m_tv                              {nullptr};
    bool m_isDummy                        {false};

    // Counter for buffering messages
    int  m_bufferingCounter               {0};

    // Debugging variables
    Jitterometer *m_outputJmeter          {nullptr};

  private:
    void syncWithAudioStretch();
    bool m_disablePassthrough             {false};
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
