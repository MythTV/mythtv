#ifndef MYTHPLAYER_H
#define MYTHPLAYER_H

// Std
#include <cstdint>
#include <utility>
#include <thread>

// Qt
#include <QCoreApplication>
#include <QList>
#include <QMutex>
#include <QRecursiveMutex>
#include <QTime>
#include <QString>
#include <QRect>
#include <QSize>
#include <QStringList>
#include <QTimer>
#include <QWaitCondition>

// MythTV
#include "libmyth/audio/volumebase.h"
#include "libmythbase/mthread.h"
#include "libmythbase/mythmiscutil.h"
#include "libmythbase/mythtypes.h"
#include "libmythbase/programtypes.h"
#include "libmythtv/audioplayer.h"
#include "libmythtv/captions/cc608reader.h"
#include "libmythtv/captions/cc708reader.h"
#include "libmythtv/captions/subtitlereader.h"
#include "libmythtv/captions/teletextreader.h"
#include "libmythtv/commbreakmap.h"
#include "libmythtv/decoders/decoderbase.h"
#include "libmythtv/deletemap.h"
#include "libmythtv/mythavutil.h"
#include "libmythtv/mythplayeravsync.h"
#include "libmythtv/mythtvexp.h"
#include "libmythtv/mythvideoout.h"
#include "libmythtv/osd.h"
#include "libmythtv/playercontext.h"
#include "libmythtv/tv.h"
#include "libmythtv/videoouttypes.h"

class ProgramInfo;
class InteractiveTV;
class QThread;
class MythMediaBuffer;
class MythDecoderThread;

using StatusCallback = void (*)(int, void*);

/// Timecode types
enum TCTypes : std::uint8_t
{
    TC_VIDEO = 0,
    TC_AUDIO,
    TC_SUB,
    TC_CC
};
static constexpr size_t TCTYPESMAX { 4 };
using tctype_arr = std::array<std::chrono::milliseconds,TCTYPESMAX>;

enum PlayerFlags
{
    kNoFlags              = 0x000000,
    kDecodeLowRes         = 0x000001,
    kDecodeSingleThreaded = 0x000002,
    kDecodeFewBlocks      = 0x000004,
    kDecodeNoLoopFilter   = 0x000008,
    kDecodeNoDecode       = 0x000010,
    kDecodeAllowGPU       = 0x000020,
    kVideoIsNull          = 0x000040,
    kAudioMuted           = 0x010000,
    kNoITV                = 0x020000,
    kMusicChoice          = 0x040000,
};

// Padding between class members reduced from 113 to 73 bytes, but its
// still higher than the default warning threshhold of 24 bytes.
//
// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding)
class MTV_PUBLIC MythPlayer : public QObject
{
    Q_OBJECT

    // Do NOT add a decoder class to this list
    friend class PlayerContext;
    friend class CC708Reader;
    friend class CC608Reader;
    friend class MythDecoderThread;
    friend class VideoPerformanceTest;
    // TODO remove these
    friend class TV;
    friend class Transcode;

  signals:
    void CheckCallbacks();
    void SeekingSlow(int Count);
    void SeekingComplete();
    void SeekingDone();
    void PauseChanged(bool Paused);
    void RequestResetCaptions();

  public:
    explicit MythPlayer(PlayerContext* Context, PlayerFlags Flags = kNoFlags);
   ~MythPlayer() override;

    // Initialisation
    virtual int  OpenFile(int Retries = 4);
    virtual bool InitVideo(void);
    virtual void ReinitVideo(bool ForceUpdate);
    virtual void InitFrameInterval();

    // Public Sets
    void SetLength(std::chrono::seconds len)  { m_totalLength = len; }
    void SetFramesPlayed(uint64_t played);
    void SetEof(EofState eof);
    void SetWatchingRecording(bool mode);
    void SetKeyframeDistance(int keyframedistance);
    virtual void SetVideoParams(int w, int h, double fps, float aspect,
                        bool ForceUpdate, int ReferenceFrames,
                        FrameScanType /*scan*/ = kScan_Ignore,
                        const QString& codecName = QString());
    void SetFileLength(std::chrono::seconds total, int frames);
    void SetDuration(std::chrono::seconds duration);
    void SetFrameRate(double fps);

    // Gets
    QSize   GetVideoBufferSize(void) const    { return m_videoDim; }
    QSize   GetVideoSize(void) const          { return m_videoDispDim; }
    float   GetVideoAspect(void) const        { return m_videoAspect; }
    float   GetFrameRate(void) const          { return m_videoFrameRate; }
    bool    IsAudioNeeded(void)               { return !FlagIsSet(kVideoIsNull); }
    int     GetFreeVideoFrames(void) const;

    int     GetFFRewSkip(void) const          { return m_ffrewSkip; }
    float   GetPlaySpeed(void) const          { return m_playSpeed; }
    AudioPlayer* GetAudio(void)               { return &m_audio; }
    float   GetNextPlaySpeed(void) const      { return m_nextPlaySpeed; }
    std::chrono::seconds GetLength(void) const  { return m_totalLength; }
    uint64_t GetTotalFrameCount(void) const   { return m_totalFrames; }
    uint64_t GetCurrentFrameCount(void) const;
    uint64_t GetFramesPlayed(void) const      { return m_framesPlayed; }
    virtual  uint64_t GetBookmark(void);
    QString   GetError(void) const;
    QString   GetEncodingType(void) const;
    QString   GetXDS(const QString &key) const;

    // Bool Gets
    bool    IsPaused(void) const              { return m_allPaused;      }
    bool    GetLimitKeyRepeat(void) const     { return m_limitKeyRepeat; }
    EofState GetEof(void) const;
    bool    IsErrored(void) const;
    bool    IsPlaying(std::chrono::milliseconds wait_in_msec = 0ms,
                      bool wait_for = true) const;
    bool    AtNormalSpeed(void) const         { return m_nextNormalSpeed; }
    bool    IsReallyNearEnd(void) const;
    bool    IsNearEnd(void);
    bool    HasTVChainNext(void) const;
    bool    IsWatchingInprogress(void) const;

    virtual InteractiveTV *GetInteractiveTV() { return nullptr; }
    MythVideoOutput *GetVideoOutput(void)     { return m_videoOutput; }

    // Title stuff
    virtual bool SwitchTitle(int /*title*/) { return false; }
    virtual bool NextTitle(void) { return false; }
    virtual bool PrevTitle(void) { return false; }

    // Angle stuff
    virtual bool SwitchAngle(int /*title*/) { return false; }
    virtual bool NextAngle(void) { return false; }
    virtual bool PrevAngle(void) { return false; }

    // Decoder stuff..
    MythVideoFrame *GetNextVideoFrame(void);
    void DeLimboFrame(MythVideoFrame *frame);
    virtual void ReleaseNextVideoFrame(MythVideoFrame *buffer, std::chrono::milliseconds timecode,
                                       bool wrap = true);
    void DiscardVideoFrame(MythVideoFrame *buffer);
    void DiscardVideoFrames(bool KeyFrame, bool Flushed);
    /// Returns the stream decoder currently in use.
    DecoderBase *GetDecoder(void) { return m_decoder; }
    virtual bool HasReachedEof(void) const;
    void SetDisablePassThrough(bool disabled);
    void ForceSetupAudioStream(void);

    // Add data
    virtual bool PrepareAudioSample(std::chrono::milliseconds &timecode);

    // Public Closed caption and teletext stuff
    virtual CC708Reader    *GetCC708Reader(uint /*id*/=0) { return &m_cc708; }
    virtual CC608Reader    *GetCC608Reader(uint /*id*/=0) { return &m_cc608; }
    virtual SubtitleReader *GetSubReader(uint /*id*/=0) { return &m_subReader; }
    virtual TeletextReader *GetTeletextReader(uint /*id*/=0) { return &m_ttxReader; }

    // Public Audio/Subtitle/EIA-608/EIA-708 stream selection - thread safe
    void EnableForcedSubtitles(bool enable);
    // How to handle forced Subtitles (i.e. when in a movie someone speaks
    // in a different language than the rest of the movie, subtitles are
    // forced on even if the user doesn't have them turned on.)
    // These two functions are not thread-safe (UI thread use only).
    bool GetAllowForcedSubtitles(void) const { return m_allowForcedSubtitles; }

    virtual void tracksChanged([[maybe_unused]] uint TrackType) {}

    // LiveTV public stuff
    void CheckTVChain();
    void FileChangedCallback();

    // Chapter public stuff
    virtual int  GetNumChapters(void);
    virtual int  GetCurrentChapter(void);
    virtual void GetChapterTimes(QList<std::chrono::seconds> &times);

    // Title public stuff
    virtual int GetNumTitles(void) const { return 0; }
    virtual int GetCurrentTitle(void) const { return 0; }
    virtual std::chrono::seconds GetTitleDuration(int /*title*/) const { return 0s; }
    virtual QString GetTitleName(int /*title*/) const { return {}; }

    // Angle public stuff
    virtual int GetNumAngles(void) const { return 0; }
    virtual int GetCurrentAngle(void) const { return 0; }
    virtual QString GetAngleName(int /*title*/) const { return {}; }

    // DVD public stuff
    virtual bool IsInStillFrame() const         { return false;     }

    // Position Map Stuff
    bool PosMapFromEnc(uint64_t start, frm_pos_map_t &posMap, frm_pos_map_t &durMap);

    void SaveTotalDuration(void);
    void ResetTotalDuration(void);

    static const int kNightModeBrightenssAdjustment;
    static const int kNightModeContrastAdjustment;
    static const double kInaccuracyNone;
    static const double kInaccuracyDefault;
    static const double kInaccuracyEditor;
    static const double kInaccuracyFull;
    static const double kSeekToEndOffset;

    void SaveTotalFrames(void);
    void SetErrored(const QString &reason);

  protected:
    // Initialization
    void OpenDummy(void);

    // Complicated gets
    virtual long long CalcMaxFFTime(long long ff, bool setjump = true) const;
    long long CalcRWTime(long long rw) const;
    std::chrono::milliseconds TranslatePositionFrameToMs(uint64_t position,
                                        bool use_cutlist) const;
    uint64_t TranslatePositionMsToFrame(std::chrono::milliseconds position,
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
        return TranslatePositionFrameToMs(position, use_cutlist).count() / 1000.0;
    }
    uint64_t FindFrame(float offset, bool use_cutlist) const;

    // Commercial stuff
    void SetAutoCommercialSkip(CommSkipMode autoskip)
        { m_commBreakMap.SetAutoCommercialSkip(autoskip, m_framesPlayed); }
    void SkipCommercials(int direction)
        { m_commBreakMap.SkipCommercials(direction); }
    void SetCommBreakMap(const frm_dir_map_t& NewMap);
    CommSkipMode GetAutoCommercialSkip(void)
        { return m_commBreakMap.GetAutoCommercialSkip(); }

    // Start/Reset/Stop playing
    virtual void ResetPlaying(bool resetframes = true);
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
    virtual void VideoEnd(void);
    virtual void DecoderStart(bool start_paused);
    virtual void DecoderLoop(bool pause);
    virtual void DecoderEnd(void);
    virtual void DecoderPauseCheck(void);
    virtual void AudioEnd(void);

    // Edit mode stuff
    bool GetEditMode(void) const { return m_deleteMap.IsEditing(); }
    bool IsInDelete(uint64_t frame);

    bool FlagIsSet(PlayerFlags arg) { return (m_playerFlags & arg) != 0; }

  protected:
    // Private Sets
    void SetPlayingInfo(const ProgramInfo &pginfo);
    void SetPlaying(bool is_playing);
    void ResetErrored(void);

    // Private pausing stuff
    void PauseVideo(void);
    void UnpauseVideo(void);
    void PauseBuffer(void);
    void UnpauseBuffer(void);

    // Private decoder stuff
    virtual void CreateDecoder(TestBufferVec & TestBuffer);
    void  SetDecoder(DecoderBase *dec);
    /// Returns the stream decoder currently in use.
    const DecoderBase *GetDecoder(void) const { return m_decoder; }
    virtual void DoFFRewSkip(void);
    bool         DecoderGetFrame(DecodeType decodetype, bool unsafe = false);
    bool         DoGetFrame(DecodeType DecodeType);

    // These actually execute commands requested by public members
    bool UpdateFFRewSkip(float ffrewScale = 1.0F);
    virtual void ChangeSpeed(void);
    // The "inaccuracy" argument is generally one of the kInaccuracy* values.
    bool DoFastForward(uint64_t frames, double inaccuracy);
    bool DoRewind(uint64_t frames, double inaccuracy);
    void DoJumpToFrame(uint64_t frame, double inaccuracy);

    // Private seeking stuff
    void WaitForSeek(uint64_t frame, uint64_t seeksnap_wanted);
    void ClearAfterSeek(bool clearvideobuffers = true);

    // Private chapter stuff
    virtual bool DoJumpChapter(int chapter);
    virtual int64_t GetChapter(int chapter);

    // Private A/V Sync Stuff
    void  WrapTimecode(std::chrono::milliseconds &timecode, TCTypes tc_type);
    void  SetFrameInterval(FrameScanType scan, double frame_period);

  protected:
    DecoderBase     *m_decoder            {nullptr};
    mutable QRecursiveMutex  m_decoderChangeLock;
    MythVideoOutput *m_videoOutput        {nullptr};
    const VideoFrameTypes* m_renderFormats { &MythVideoFrame::kDefaultRenderFormats };
    PlayerContext   *m_playerCtx          {nullptr};
    MythDecoderThread* m_decoderThread    {nullptr};
    QThread         *m_playerThread       {nullptr};
#ifdef Q_OS_ANDROID
    int            m_playerThreadId       {0};
#endif
    PlayerFlags      m_playerFlags;

    // State
    QWaitCondition m_decoderThreadPause;
    QWaitCondition m_decoderThreadUnpause;
    mutable QMutex m_decoderPauseLock;
    mutable QMutex m_decoderSeekLock;
    mutable QMutex m_bufferPauseLock;
    mutable QMutex m_videoPauseLock;
    mutable QMutex m_pauseLock;
    int64_t        m_decoderSeek            {-1};
    bool           m_totalDecoderPause      {false};
    bool           m_decoderPaused          {false};
    bool           m_inJumpToProgramPause   {false};
    bool           m_pauseDecoder           {false};
    bool           m_unpauseDecoder         {false};
    bool volatile  m_killDecoder            {false};
    bool           m_decodeOneFrame         {false};
    bool           m_renderOneFrame         {false};
    bool           m_needNewPauseFrame      {false};
    bool           m_bufferPaused           {false};
    bool           m_videoPaused            {false};
    bool           m_allPaused              {false};
    bool           m_playing                {false};

    mutable QWaitCondition m_playingWaitCond;
    mutable QMutex m_vidExitLock;
    mutable QMutex m_playingLock;
    mutable QMutex   m_errorLock;
    QString  m_errorMsg;   ///< Reason why NVP exited with a error
    int m_errorType                       {kError_None};
    bool     m_liveTV                     {false};
    bool     m_watchingRecording          {false};
    bool     m_transcoding                {false};
    bool     m_hasFullPositionMap         {false};
    mutable bool     m_limitKeyRepeat     {false};

    // Chapter stuff
    int m_jumpChapter                     {0};

    // Bookmark stuff
    uint64_t m_bookmarkSeek               {0};
    int      m_endExitPrompt;

    // Seek
    /// If m_ffTime>0, number of frames to seek forward.
    /// If m_ffTime<0, number of frames to seek backward.
    long long m_ffTime                    {0};

    // Playback misc.
    /// How often we have tried to wait for a video output buffer and failed
    int       m_videobufRetries           {0};
    uint64_t  m_framesPlayed              {0};
    uint64_t  m_totalFrames               {0};
    std::chrono::seconds  m_totalLength   {0s};
    std::chrono::seconds  m_totalDuration {0s};
    long long m_rewindTime                {0};
    std::chrono::milliseconds  m_latestVideoTimecode {-1ms};
    MythPlayerAVSync m_avSync;

    // -- end state stuff --

    // Input Video Attributes
    double   m_videoFrameRate            {29.97};///< Video (input) Frame Rate (often inaccurate)
    /// Codec Name - used by playback profile
    QString  m_codecName;
    QSize    m_videoDispDim              {0,0}; ///< Video (input) width & height
    QSize    m_videoDim                  {0,0}; ///< Video (input) buffer width & height
    int      m_maxReferenceFrames        {0}; ///< Number of reference frames used in the video stream
    float    m_videoAspect               {4.0F / 3.0F};    ///< Video (input) Apect Ratio
    float    m_forcedVideoAspect         {-1};

    /// Video (input) Number of frames between key frames (often inaccurate)
    uint     m_keyframeDist              {30};

    // Buffering
    bool     m_buffering                  {false};
    QTime    m_bufferingStart;
    QTime    m_bufferingLastMsg;

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
    bool      m_enableForcedSubtitles     {false};
    bool      m_disableForcedSubtitles    {false};
    bool      m_allowForcedSubtitles      {true};

    // CC608/708
    CC608Reader m_cc608;
    CC708Reader m_cc708;

    // Audio stuff
    AudioPlayer      m_audio;

    // Commercial filtering
    CommBreakMap   m_commBreakMap;
    bool       m_forcePositionMapSync     {false};
    // Manual editing
    DeleteMap  m_deleteMap;

    // Playback (output) speed control
    /// Lock for next_play_speed and next_normal_speed
    QRecursiveMutex  m_decoderLock;
    float      m_nextPlaySpeed            {1.0F};
    float      m_playSpeed                {1.0F};
    std::chrono::microseconds m_frameInterval
        {microsecondsFromFloat(1000000.0F / 30)};///< always adjusted for play_speed
    int        m_fpsMultiplier            {1}; ///< used to detect changes
    int        m_ffrewSkip                {1};
    int        m_ffrewAdjust              {0}; ///< offset after last skip
    float      m_ffrewScale               {1.0F}; ///< scale skip for large gops
    bool       m_fileChanged              {false};
    bool       m_nextNormalSpeed          {true};
    bool       m_normalSpeed              {true};

    // Time Code stuff
    tctype_arr m_tcWrap                   {};
    std::chrono::milliseconds m_savedAudioTimecodeOffset {0ms};

    // LiveTV
    bool m_isDummy                        {false};

    // Counter for buffering messages
    int  m_bufferingCounter               {0};

  private:
    Q_DISABLE_COPY(MythPlayer)
    void syncWithAudioStretch();
    bool m_disablePassthrough             {false};
};

#endif
