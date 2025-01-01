#ifndef DECODERBASE_H_
#define DECODERBASE_H_

#include <array>
#include <cstdint>
#include <vector>

#include "libmyth/mythcontext.h"
#include "libmythbase/mythdbcon.h"
#include "libmythbase/programinfo.h"
#include "libmythtv/io/mythmediabuffer.h"
#include "libmythtv/mythavrational.h"
#include "libmythtv/mythavutil.h"
#include "libmythtv/mythcodecid.h"
#include "libmythtv/mythvideoprofile.h"
#include "libmythtv/remoteencoder.h"

class TeletextViewer;
class MythPlayer;
class AudioPlayer;
class MythCodecContext;

const int kDecoderProbeBufferSize = 256 * 1024;
using TestBufferVec = std::vector<char>;

/// Track types
enum TrackType : std::uint8_t
{
    kTrackTypeUnknown = 0,
    kTrackTypeAudio,            // 1
    kTrackTypeVideo,            // 2
    kTrackTypeSubtitle,         // 3
    kTrackTypeCC608,            // 4
    kTrackTypeCC708,            // 5
    kTrackTypeTeletextCaptions, // 6
    kTrackTypeTeletextMenu,     // 7
    kTrackTypeRawText,          // 8
    kTrackTypeAttachment,       // 9
    kTrackTypeCount,            // 10
    // The following are intentionally excluded from kTrackTypeCount which
    // is used when auto-selecting the correct tracks to decode according to
    // language, bitrate etc
    kTrackTypeTextSubtitle,     // 11
};
QString toString(TrackType type);
int to_track_type(const QString &str);

enum DecodeType : std::uint8_t
{
    kDecodeNothing = 0x00, // Demux and preprocess only.
    kDecodeVideo   = 0x01,
    kDecodeAudio   = 0x02,
    kDecodeAV      = 0x03,
};

enum AudioTrackType : std::uint8_t
{
    kAudioTypeNormal = 0,
    kAudioTypeAudioDescription, // Audio Description for the visually impaired
    kAudioTypeCleanEffects, // No dialog, soundtrack or effects only e.g. Karaoke
    kAudioTypeHearingImpaired, // Greater contrast between dialog and background audio
    kAudioTypeSpokenSubs, // Spoken subtitles for the visually impaired
    kAudioTypeCommentary // Director/actor/etc Commentary
};
QString toString(AudioTrackType type);

// Eof States
enum EofState : std::uint8_t
{
    kEofStateNone,     // no eof
    kEofStateDelayed,  // decoder eof, but let player drain buffered frames
    kEofStateImmediate // true eof
};

class StreamInfo
{
  public:
    StreamInfo() = default;
    /*
    Video and Attachment use only the first two parameters; the rest are used
    for Subtitle, CC, Teletext, and RawText.
    */
    StreamInfo(int av_stream_index, int stream_id, int language = 0, uint language_index = 0,
               bool forced = false) :
        m_av_stream_index(av_stream_index),
        m_stream_id(stream_id),
        m_language(language),
        m_language_index(language_index),
        m_forced(forced)
    {}
    /*
    For Audio
    */
    StreamInfo(int av_stream_index, int stream_id, int language, uint language_index,
               AudioTrackType audio_type) :
        m_av_stream_index(av_stream_index),
        m_stream_id(stream_id),
        m_language(language),
        m_language_index(language_index),
        m_audio_type(audio_type)
    {}

  public:
    int            m_av_stream_index    {-1};
    int            m_stream_id          {-1};
    /// ISO639 canonical language key; Audio, Subtitle, CC, Teletext, RawText
    int            m_language           {-2};
    uint           m_language_index     {0}; ///< Audio, Subtitle, Teletext
    bool           m_forced             {false}; ///< Subtitle and RawText
    AudioTrackType m_audio_type {kAudioTypeNormal}; // Audio only
    /// Audio only; -1 for no substream, 0 for first dual audio stream, 1 for second dual
    int            m_av_substream_index {-1};

    bool operator<(const StreamInfo& b) const
    {
        return (this->m_stream_id < b.m_stream_id);
    }
};
using sinfo_vec_t = std::vector<StreamInfo>;

class DecoderBase
{
  public:
    DecoderBase(MythPlayer *parent, const ProgramInfo &pginfo);
    virtual ~DecoderBase();

    void SetRenderFormats(const VideoFrameTypes* RenderFormats);
    virtual void Reset(bool reset_video_data, bool seek_reset, bool reset_file);
    virtual int OpenFile(MythMediaBuffer *Buffer, bool novideo,
                         TestBufferVec & testbuf) = 0;

    virtual void SetEofState(EofState eof)  { m_atEof = eof;  }
    virtual void SetEof(bool eof)  {
        m_atEof = eof ? kEofStateDelayed : kEofStateNone;
    }
    EofState     GetEof(void)      { return m_atEof; }

    void SetSeekSnap(uint64_t snap)  { m_seekSnap = snap; }
    uint64_t GetSeekSnap(void) const { return m_seekSnap;  }
    void SetLiveTVMode(bool live)  { m_livetv = live;      }

    // Must be done while player is paused.
    void SetProgramInfo(const ProgramInfo &pginfo);

    /// Disables AC3/DTS pass through
    virtual void SetDisablePassThrough(bool disable) { (void)disable; }
    // Reconfigure audio as necessary, following configuration change
    virtual void ForceSetupAudioStream(void) { }

    virtual void SetWatchingRecording(bool mode);
    /// Demux, preprocess and possibly decode a frame of video/audio.
    virtual bool GetFrame(DecodeType Type, bool &Retry) = 0;
    MythPlayer *GetPlayer() { return m_parent; }

    virtual int GetNumChapters(void)                      { return 0; }
    virtual int GetCurrentChapter(long long /*framesPlayed*/) { return 0; }
    virtual void GetChapterTimes(QList<std::chrono::seconds> &/*times*/) { }
    virtual long long GetChapter(int /*chapter*/)             { return m_framesPlayed; }
    virtual bool DoRewind(long long desiredFrame, bool discardFrames = true);
    virtual bool DoFastForward(long long desiredFrame, bool discardFrames = true);
    virtual void SetIdrOnlyKeyframes(bool /*value*/) { }

    static uint64_t
        TranslatePositionAbsToRel(const frm_dir_map_t &deleteMap,
                                  uint64_t absPosition,
                                  const frm_pos_map_t &map = frm_pos_map_t(),
                                  float fallback_ratio = 1.0);
    static uint64_t
        TranslatePositionRelToAbs(const frm_dir_map_t &deleteMap,
                                  uint64_t relPosition,
                                  const frm_pos_map_t &map = frm_pos_map_t(),
                                  float fallback_ratio = 1.0);
    static uint64_t TranslatePosition(const frm_pos_map_t &map,
                                      long long key,
                                      float fallback_ratio);
    std::chrono::milliseconds TranslatePositionFrameToMs(long long position,
                                        float fallback_framerate,
                                        const frm_dir_map_t &cutlist);
    uint64_t TranslatePositionMsToFrame(std::chrono::milliseconds dur_ms,
                                        float fallback_framerate,
                                        const frm_dir_map_t &cutlist);

    float GetVideoAspect(void) const { return m_currentAspect; }

    virtual std::chrono::milliseconds NormalizeVideoTimecode(std::chrono::milliseconds timecode)
        { return timecode; }

    virtual bool IsLastFrameKey(void) const = 0;

    virtual double  GetFPS(void) const { return m_fps; }
    /// Returns the estimated bitrate if the video were played at normal speed.
    uint GetRawBitrate(void) const { return m_bitrate; }

    virtual void UpdateFramesPlayed(void);
    long long GetFramesRead(void) const { return m_framesRead; }
    long long GetFramesPlayed(void) const { return m_framesPlayed; }
    void SetFramesPlayed(long long newValue) {m_framesPlayed = newValue;}

    virtual QString GetCodecDecoderName(void) const = 0;
    virtual QString GetRawEncodingType(void) { return {}; }
    virtual MythCodecID GetVideoCodecID(void) const = 0;

    virtual void ResetPosMap(void);
    virtual bool SyncPositionMap(void);
    virtual bool PosMapFromDb(void);
    virtual bool PosMapFromEnc(void);

    virtual bool FindPosition(long long desired_value, bool search_adjusted,
                              int &lower_bound, int &upper_bound);

    uint64_t SavePositionMapDelta(long long first_frame, long long last_frame);
    virtual void SeekReset(long long newkey, uint skipFrames,
                           bool doFlush, bool discardFrames);

    void SetTranscoding(bool value) { m_transcoding = value; }

    bool IsErrored() const { return m_errored; }

    bool HasPositionMap(void) const { return GetPositionMapSize() != 0U; }

    void SetWaitForChange(void);
    bool GetWaitForChange(void) const;
    void SetReadAdjust(long long adjust);

    // Audio/Subtitle/EIA-608/EIA-708 stream selection
    void         SetDecodeAllSubtitles(bool DecodeAll);
    virtual QStringList GetTracks(uint Type);
    virtual uint GetTrackCount(uint Type);

    virtual int  GetTrackLanguageIndex(uint Type, uint TrackNo);
    virtual QString GetTrackDesc(uint Type, uint TrackNo);
    virtual int  SetTrack(uint Type, int TrackNo);
    int          GetTrack(uint Type);
    StreamInfo   GetTrackInfo(uint Type, uint TrackNo);
    int          ChangeTrack(uint Type, int Dir);
    int          NextTrack(uint Type);

    virtual int  GetTeletextDecoderType(void) const { return -1; }

    virtual QString GetXDS(const QString &/*key*/) const { return {}; }
    virtual QByteArray GetSubHeader(uint /*trackNo*/) { return {}; }
    virtual void GetAttachmentData(uint /*trackNo*/, QByteArray &/*filename*/,
                                   QByteArray &/*data*/) {}

    // MHEG/MHI stuff
    virtual bool SetAudioByComponentTag(int /*tag*/) { return false; }
    virtual bool SetVideoByComponentTag(int /*tag*/) { return false; }

    void SaveTotalDuration(void);
    void ResetTotalDuration(void) { m_totalDuration = MythAVRational(0); }
    void SaveTotalFrames(void);
    void TrackTotalDuration(bool track) { m_trackTotalDuration = track; }
    int GetfpsMultiplier(void) const { return m_fpsMultiplier; }
    MythCodecContext *GetMythCodecContext(void) { return m_mythCodecCtx; }
    static AVPixelFormat GetBestVideoFormat(AVPixelFormat* Formats, const VideoFrameTypes* RenderFormats);

  protected:
    int          BestTrack(uint Type, bool forcedPreferred, int preferredLanguage = 0);
    virtual int  AutoSelectTrack(uint Type);
    void         AutoSelectTracks(void);
    void         ResetTracks(void);
    void         FileChanged(void);
    virtual bool DoRewindSeek(long long desiredFrame);
    virtual void DoFastForwardSeek(long long desiredFrame, bool &needflush);

    long long ConditionallyUpdatePosMap(long long desiredFrame);
    long long GetLastFrameInPosMap(void) const;
    unsigned long GetPositionMapSize(void) const;

    struct PosMapEntry
    {
        long long index;    // frame or keyframe number
        long long adjFrame; // keyFrameAdjustTable adjusted frame number
        long long pos;      // position in stream
    };
    long long GetKey(const PosMapEntry &entry) const;

    MythPlayer          *m_parent                  {nullptr};
    ProgramInfo         *m_playbackInfo            {nullptr};
    AudioPlayer         *m_audio                   {nullptr};
    MythMediaBuffer     *m_ringBuffer              {nullptr};

    double               m_fps                     {29.97};
    int                  m_fpsMultiplier           {1};
    int                  m_fpsSkip                 {0};
    uint                 m_bitrate                 {4000};
    int                  m_currentWidth            {640};
    int                  m_currentHeight           {480};
    float                m_currentAspect           {1.33333F};

    long long            m_framesPlayed            {0};
    long long            m_framesRead              {0};
    uint64_t             m_frameCounter            {0};
    MythAVRational       m_totalDuration           {0};
    int                  m_keyframeDist            {-1};
    long long            m_lastKey                 {0};
    long long            m_indexOffset             {0};
    MythAVCopy           m_copyFrame;
    bool                 m_nextDecodedFrameIsKeyFrame { false };

    EofState             m_atEof                   {kEofStateNone};

    // The totalDuration field is only valid when the video is played
    // from start to finish without any jumping.  trackTotalDuration
    // indicates whether this is the case.
    bool                 m_trackTotalDuration      {false};

    bool                 m_exitAfterDecoded        {false};
    bool                 m_transcoding             {false};

    bool                 m_hasFullPositionMap      {false};
    bool                 m_recordingHasPositionMap {false};
    bool                 m_posmapStarted           {false};
    MarkTypes            m_positionMapType         {MARK_UNSET};

    mutable QRecursiveMutex m_positionMapLock;
    std::vector<PosMapEntry>  m_positionMap;
    frm_pos_map_t        m_frameToDurMap; // guarded by m_positionMapLock
    frm_pos_map_t        m_durToFrameMap; // guarded by m_positionMapLock
    mutable QDateTime    m_lastPositionMapUpdate; // guarded by m_positionMapLock

    uint64_t             m_seekSnap                {UINT64_MAX};
    bool                 m_dontSyncPositionMap     {false};
    bool                 m_livetv                  {false};
    bool                 m_watchingRecording       {false};

    bool                 m_hasKeyFrameAdjustTable  {false};

    bool                 m_errored                 {false};

    bool                 m_waitingForChange        {false};
    bool                 m_justAfterChange         {false};
    long long            m_readAdjust              {0};
    int                  m_videoRotation           {0};
    uint                 m_stereo3D                {0};

    // Audio/Subtitle/EIA-608/EIA-708 stream selection
    QRecursiveMutex      m_trackLock;
    bool                 m_decodeAllSubtitles            { false };
    std::array<int,        kTrackTypeCount> m_currentTrack {};
    std::array<sinfo_vec_t,kTrackTypeCount> m_tracks;
    std::array<StreamInfo, kTrackTypeCount> m_wantedTrack;
    std::array<StreamInfo, kTrackTypeCount> m_selectedTrack;
    std::array<StreamInfo, kTrackTypeCount> m_selectedForcedTrack;

    /// language preferences for auto-selection of streams
    std::vector<int>     m_languagePreference;
    MythCodecContext    *m_mythCodecCtx         { nullptr };
    MythVideoProfile     m_videoDisplayProfile;
    const VideoFrameTypes* m_renderFormats { &MythVideoFrame::kDefaultRenderFormats };

  private:
    Q_DISABLE_COPY(DecoderBase)
};
#endif
