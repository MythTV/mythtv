
#ifndef AVFORMATDECODER_H_
#define AVFORMATDECODER_H_

#include <array>
#include <cstdint>

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
}

#include <QList>
#include <QMap>
#include <QString>

#include "libmyth/audio/audiooutputsettings.h"
#include "libmyth/audio/audiooutpututil.h"
#include "libmyth/audio/spdifencoder.h"
#include "libmythbase/programinfo.h"

#include "captions/vbilut.h"
#include "decoderbase.h"
#include "format.h"
#include "io/mythavformatbuffer.h"
#include "mpeg/AVCParser.h"
#include "mythcodeccontext.h"
#include "mythplayer.h"

class TeletextDecoder;
class CC608Decoder;
class CC708Decoder;
class SubtitleReader;
class InteractiveTV;
class MythSqlDatabase;

struct SwsContext;

class AudioInfo
{
  public:
    AudioInfo() = default;

    AudioInfo(AVCodecID id, AudioFormat fmt, int sr, int ch, bool passthru,
              int original_ch, int profile = 0) :
        m_codecId(id), format(fmt),
        m_sampleSize(ch * AudioOutputSettings::SampleSize(fmt)),
        m_sampleRate(sr), m_channels(ch), m_codecProfile(profile),
        m_doPassthru(passthru), m_originalChannels(original_ch)
    {
    }

    AVCodecID   m_codecId          {AV_CODEC_ID_NONE};
    AudioFormat format             {FORMAT_NONE};
    int         m_sampleSize       {-2};
    int         m_sampleRate       {-1};
    int         m_channels         {-1};
    int         m_codecProfile     {0};
    bool        m_doPassthru       {false};
    int         m_originalChannels {-1};

    bool operator==(const AudioInfo &o) const
    {
        return (m_codecId==o.m_codecId        && m_channels==o.m_channels     &&
                m_sampleSize==o.m_sampleSize  && m_sampleRate==o.m_sampleRate &&
                format==o.format              && m_doPassthru==o.m_doPassthru &&
                m_originalChannels==o.m_originalChannels &&
                m_codecProfile == o.m_codecProfile);
    }
    QString toString() const
    {
        return QString("id(%1) %2Hz %3ch %4bps %5 (profile %6)")
            .arg(avcodec_get_name(m_codecId),4).arg(m_sampleRate,6)
            .arg(m_channels,2).arg(AudioOutputSettings::FormatToBits(format),2)
            .arg((m_doPassthru) ? "pt":"",3).arg(m_codecProfile);
    }
};

/// A decoder for media files.
class AvFormatDecoder : public DecoderBase
{
  public:
    AvFormatDecoder(MythPlayer *parent, const ProgramInfo &pginfo,
                    PlayerFlags flags);
    ~AvFormatDecoder() override;

    // Deleted functions should be public.
    AvFormatDecoder(const AvFormatDecoder &) = delete;            // not copyable
    AvFormatDecoder &operator=(const AvFormatDecoder &) = delete; // not copyable

    void SetEof(bool eof) override; // DecoderBase

    MythCodecMap* CodecMap(void);
    void CloseCodecs();
    void CloseContext();
    void Reset(bool reset_video_data, bool seek_reset,
               bool reset_file) override; // DecoderBase

    /// Perform an av_probe_input_format on the passed data to see if we
    /// can decode it with this class.
    static bool CanHandle(TestBufferVec & testbuf,
                          const QString &filename);

    /// Open our file and set up or audio and video parameters.
    int OpenFile(MythMediaBuffer *Buffer, bool novideo,
                 TestBufferVec & testbuf) override; // DecoderBase

    bool GetFrame(DecodeType Type, bool &Retry) override; // DecoderBase

    bool IsLastFrameKey(void) const override { return false; } // DecoderBase

    QString      GetCodecDecoderName(void) const override; // DecoderBase
    QString      GetRawEncodingType(void) override; // DecoderBase
    MythCodecID  GetVideoCodecID(void) const override { return m_videoCodecId; } // DecoderBase

    void SetDisablePassThrough(bool disable) override; // DecoderBase
    void ForceSetupAudioStream(void) override; // DecoderBase
    void AddTextData(unsigned char *buf, int len, int64_t timecode, char type);

    QString GetTrackDesc(uint Type, uint TrackNo) override;
    int SetTrack(uint Type, int TrackNo) override;

    int ScanStreams(bool novideo);
    int FindStreamInfo(void);

    int  GetNumChapters() override; // DecoderBase
    void GetChapterTimes(QList<std::chrono::seconds> &times) override; // DecoderBase
    int  GetCurrentChapter(long long framesPlayed) override; // DecoderBase
    long long GetChapter(int chapter) override; // DecoderBase
    bool DoRewind(long long desiredFrame, bool discardFrames = true) override; // DecoderBase
    bool DoFastForward(long long desiredFrame, bool discardFrames = true) override; // DecoderBase
    void SetIdrOnlyKeyframes(bool value) override // DecoderBase
        { m_avcParser->use_I_forKeyframes(!value); }

    std::chrono::milliseconds NormalizeVideoTimecode(std::chrono::milliseconds timecode) override; // DecoderBase
    virtual std::chrono::milliseconds NormalizeVideoTimecode(AVStream *st, std::chrono::milliseconds timecode);

    int  GetTeletextDecoderType(void) const override; // DecoderBase

    QString GetXDS(const QString &Key) const override; // DecoderBase
    QByteArray GetSubHeader(uint TrackNo) override;
    void GetAttachmentData(uint TrackNo, QByteArray &Filename, QByteArray &Data) override; // DecoderBase

    // MHEG stuff
    bool SetAudioByComponentTag(int Tag) override; // DecoderBase
    bool SetVideoByComponentTag(int Tag) override; // DecoderBase

    // Stream language info
    virtual int GetTeletextLanguage(uint Index);
    virtual int GetSubtitleLanguage(uint /*unused*/, uint StreamIndex);
    virtual int GetCaptionLanguage(TrackType TrackType, int ServiceNum);
    virtual int GetAudioLanguage(uint AudioIndex, uint StreamIndex);
    virtual AudioTrackType GetAudioTrackType(uint StreamIndex);

    static int GetMaxReferenceFrames(AVCodecContext *Context);

    static void streams_changed(void *data, int avprogram_id);

  protected:
    int  AutoSelectTrack(uint type) override; // DecoderBase
    void ScanATSCCaptionStreams(int av_index);
    void UpdateATSCCaptionTracks(void);
    void UpdateCaptionTracksFromStreams(bool check_608, bool check_708);
    void ScanTeletextCaptions(int av_index);
    void ScanRawTextCaptions(int av_stream_index);
    void ScanDSMCCStreams(AVBufferRef* pmt_section);
    int  AutoSelectAudioTrack(void);
    int  filter_max_ch(const AVFormatContext *ic,
                       const sinfo_vec_t     &tracks,
                       const std::vector<int>&fs,
                       enum AVCodecID         codecId = AV_CODEC_ID_NONE,
                       int                    profile = -1);
    int selectBestAudioTrack(int lang_key, const std::vector<int> &ftype);

    friend int get_avf_buffer(struct AVCodecContext *c, AVFrame *pic,
                              int flags);

    void DecodeCCx08(const uint8_t *buf, uint buf_size);
    void InitVideoCodec(AVStream *stream, AVCodecContext *codecContext,
                        bool selectedStream = false);

    /// Preprocess a packet, setting the video parms if necessary.
    void MpegPreProcessPkt(AVCodecContext* codecContext, AVStream *stream, AVPacket *pkt);
    int  H264PreProcessPkt(AVCodecContext* codecContext, AVStream *stream, AVPacket *pkt);
    bool PreProcessVideoPacket(AVCodecContext* codecContext, AVStream *stream, AVPacket *pkt);
    virtual bool ProcessVideoPacket(AVCodecContext* codecContext, AVStream *stream, AVPacket *pkt, bool &Retry);
    virtual bool ProcessVideoFrame(AVCodecContext* codecContext, AVStream *Stream, AVFrame *AvFrame);
    bool ProcessAudioPacket(AVCodecContext* codecContext, AVStream *stream, AVPacket *pkt,
                            DecodeType decodetype);
    bool ProcessSubtitlePacket(AVCodecContext* codecContext, AVStream *stream, AVPacket *pkt);
    bool ProcessRawTextPacket(AVPacket* Packet);
    virtual bool ProcessDataPacket(AVStream *curstream, AVPacket *pkt,
                                   DecodeType decodetype);

    void ProcessVBIDataPacket(const AVStream *stream, const AVPacket *pkt);
    void ProcessDVBDataPacket(const AVStream *stream, const AVPacket *pkt);
    void ProcessDSMCCPacket(const AVStream *stream, const AVPacket *pkt);

    void SeekReset(long long newkey, uint skipFrames, bool doFlush, bool discardFrames) override; // DecoderBase

    inline bool DecoderWillDownmix(const AVCodecContext *ctx);
    bool DoPassThrough(const AVCodecParameters *par, bool withProfile=true);
    bool SetupAudioStream(void);
    void SetupAudioStreamSubIndexes(int streamIndex);
    void RemoveAudioStreams();

    /// Update our position map, keyframe distance, and the like.
    /// Called for key frame packets.
    void HandleGopStart(AVPacket *pkt, bool can_reliably_parse_keyframes);

    bool GenerateDummyVideoFrames(void);
    bool HasVideo();
    float GetVideoFrameRate(AVStream *Stream, AVCodecContext *Context, bool Sanitise = false);
    static void av_update_stream_timings_video(AVFormatContext *ic);
    bool OpenAVCodec(AVCodecContext *avctx, const AVCodec *codec);

    void UpdateFramesPlayed(void) override; // DecoderBase
    bool DoRewindSeek(long long desiredFrame) override; // DecoderBase
    void DoFastForwardSeek(long long desiredFrame, bool &needflush) override; // DecoderBase
    virtual void StreamChangeCheck(void);
    virtual void PostProcessTracks(void) { }
    virtual bool IsValidStream(int /*streamid*/) {return true;}

    int DecodeAudio(AVCodecContext *ctx, uint8_t *buffer, int &data_size,
                    AVPacket *pkt);

    virtual int ReadPacket(AVFormatContext *ctx, AVPacket *pkt, bool &storePacket);

    bool FlagIsSet(PlayerFlags arg) { return m_playerFlags & arg; }

    int autoSelectVideoTrack(int& scanerror);
    void remove_tracks_not_in_same_AVProgram(int stream_index);

    int get_current_AVStream_index(TrackType type);
    AVProgram* get_current_AVProgram();

    bool do_av_seek(long long desiredFrame, bool discardFrames, int flags);

    bool               m_isDbIgnored;

    AVCParser         *m_avcParser                    {nullptr};

    AVFormatContext   *m_ic                           {nullptr};
    MythCodecMap       m_codecMap;

    // AVFormatParameters params;

    int                m_frameDecoded                 {0};
    MythVideoFrame    *m_decodedVideoFrame            {nullptr};
    MythAVFormatBuffer *m_avfRingBuffer               {nullptr};

    struct SwsContext *m_swsCtx                       {nullptr};
    bool               m_directRendering              {false};

    bool               m_gopSet                       {false};
    /// A flag to indicate that we've seen a GOP frame.  Used in junction with seq_count.
    bool               m_seenGop                      {false};
    /// A counter used to determine if we need to force a call to HandleGopStart
    int                m_seqCount                     {0};

    QList<AVPacket*>   m_storedPackets;

    int                m_prevGopPos                   {0};

    // GetFrame
    bool               m_gotVideoFrame                {false};
    bool               m_hasVideo                     {false};
    bool               m_needDummyVideoFrames         {false};
    bool               m_skipAudio                    {false};
    bool               m_allowedQuit                  {false};

    uint32_t           m_startCodeState               {0xffffffff};

    std::chrono::milliseconds  m_lastVPts             {0ms};
    std::chrono::milliseconds  m_lastAPts             {0ms};
    std::chrono::microseconds  m_lastCcPtsu           {0ms};
    std::chrono::milliseconds  m_firstVPts            {0ms};
    bool               m_firstVPtsInuse               {false};

    PlayerFlags        m_playerFlags;
    MythCodecID        m_videoCodecId                 {kCodec_NONE};

    int                m_maxKeyframeDist              {-1};
    int                m_averrorCount                 {0};

    // Caption/Subtitle/Teletext decoders
    CC608Decoder      *m_ccd608                       {nullptr};
    CC708Decoder      *m_ccd708                       {nullptr};
    TeletextDecoder   *m_ttd                          {nullptr};
    /// Lookup table for whether a stream was seen in the PMT
    /// entries 0-3 correspond to CEA-608 CC1 through CC4, while
    /// entries 4-67 corresport to CEA-708 streams 0 through 64
    std::array<bool,68> m_ccX08InPmt                  {};
    /// Lookup table for whether a stream is represented in the UI
    /// entries 0-3 correspond to CEA-608 CC1 through CC4, while
    /// entries 4-67 corresport to CEA-708 streams 0 through 64
    std::array<bool,68> m_ccX08InTracks               {};
    /// StreamInfo for 608 and 708 Captions seen in the PMT descriptor
    QList<StreamInfo>  m_pmtTracks;
    /// TrackType (608 or 708) for Captions seen in the PMT descriptor
    QList<TrackType>   m_pmtTrackTypes;
    /// StreamInfo for 608 and 708 Captions seen in the caption stream itself
    /// but not seen in the PMT
    QList<StreamInfo>  m_streamTracks;
    /// TrackType (608 or 708) for Captions seen in the caption stream itself
    /// but not seen in the PMT
    QList<TrackType>   m_streamTrackTypes;

    /// MHEG/MHP decoder
    InteractiveTV     *m_itv                          {nullptr};

    // Audio
    uint8_t           *m_audioSamples                 {nullptr};
    bool               m_disablePassthru              {false};

    AudioInfo          m_audioIn;
    AudioInfo          m_audioOut;

    bool               m_processFrames                {true};

    bool               m_streamsChanged               { false };
    bool               m_resetHardwareDecoders        { false };

    // Value in milliseconds, from setting AudioReadAhead
    std::chrono::milliseconds  m_audioReadAhead       {100ms};

    QRecursiveMutex    m_avCodecLock;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
