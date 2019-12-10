
#ifndef AVFORMATDECODER_H_
#define AVFORMATDECODER_H_

#include <cstdint>

#include <QString>
#include <QMap>
#include <QList>

#include "programinfo.h"
#include "format.h"
#include "decoderbase.h"
#include "privatedecoder.h"
#include "audiooutputsettings.h"
#include "audiooutpututil.h"
#include "spdifencoder.h"
#include "vbilut.h"
#include "H264Parser.h"
#include "videodisplayprofile.h"
#include "mythcodeccontext.h"
#include "mythplayer.h"

extern "C" {
#include "mythframe.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
}

#include "avfringbuffer.h"

class TeletextDecoder;
class CC608Decoder;
class CC708Decoder;
class SubtitleReader;
class InteractiveTV;
class ProgramInfo;
class MythSqlDatabase;

struct SwsContext;

extern "C" void HandleStreamChange(void*);
extern "C" void HandleDVDStreamChange(void*);
extern "C" void HandleBDStreamChange(void*);

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
            .arg(ff_codec_id_string(m_codecId),4).arg(m_sampleRate,6)
            .arg(m_channels,2).arg(AudioOutputSettings::FormatToBits(format),2)
            .arg((m_doPassthru) ? "pt":"",3).arg(m_codecProfile);
    }
};

/// A decoder for video files.

/// The AvFormatDecoder is used to decode non-NuppleVideo files.
/// It's used as a decoder of last resort after trying the NuppelDecoder
class AvFormatDecoder : public DecoderBase
{
    friend void HandleStreamChange(void*);
    friend void HandleDVDStreamChange(void*);
    friend void HandleBDStreamChange(void*);
  public:
    static void GetDecoders(RenderOptions &opts);
    AvFormatDecoder(MythPlayer *parent, const ProgramInfo &pginfo,
                    PlayerFlags flags);
    virtual ~AvFormatDecoder();

    void SetEof(bool eof) override; // DecoderBase

    void CloseCodecs();
    void CloseContext();
    void Reset(bool reset_video_data, bool seek_reset,
               bool reset_file) override; // DecoderBase

    /// Perform an av_probe_input_format on the passed data to see if we
    /// can decode it with this class.
    static bool CanHandle(char testbuf[kDecoderProbeBufferSize], 
                          const QString &filename,
                          int testbufsize = kDecoderProbeBufferSize);

    /// Open our file and set up or audio and video parameters.
    int OpenFile(RingBuffer *rbuffer, bool novideo, 
                 char testbuf[kDecoderProbeBufferSize],
                 int testbufsize = kDecoderProbeBufferSize) override; // DecoderBase

    bool GetFrame(DecodeType Type, bool &Retry) override; // DecoderBase

    bool IsLastFrameKey(void) const override { return false; } // DecoderBase

    /// This is a No-op for this class.
    void WriteStoredData(RingBuffer *rb, bool storevid,
                         long timecodeOffset) override // DecoderBase
        { (void)rb; (void)storevid; (void)timecodeOffset;}

    /// This is a No-op for this class.
    void SetRawAudioState(bool state) override { (void)state; } // DecoderBase

    /// This is a No-op for this class.
    bool GetRawAudioState(void) const override { return false; } // DecoderBase

    /// This is a No-op for this class.
    void SetRawVideoState(bool state) override { (void)state; } // DecoderBase

    /// This is a No-op for this class.
    bool GetRawVideoState(void) const override { return false; } // DecoderBase

    /// This is a No-op for this class.
    long UpdateStoredFrameNum(long frame) override { (void)frame; return 0;} // DecoderBase

    QString      GetCodecDecoderName(void) const override; // DecoderBase
    QString      GetRawEncodingType(void) override; // DecoderBase
    MythCodecID  GetVideoCodecID(void) const override { return m_videoCodecId; } // DecoderBase

    void SetDisablePassThrough(bool disable) override; // DecoderBase
    void ForceSetupAudioStream(void) override; // DecoderBase
    void AddTextData(unsigned char *buf, int len, int64_t timecode, char type);

    QString GetTrackDesc(uint type, uint trackNo) const override; // DecoderBase
    int SetTrack(uint type, int trackNo) override; // DecoderBase

    int ScanStreams(bool novideo);
    int FindStreamInfo(void);

    int  GetNumChapters() override; // DecoderBase
    void GetChapterTimes(QList<long long> &times) override; // DecoderBase
    int  GetCurrentChapter(long long framesPlayed) override; // DecoderBase
    long long GetChapter(int chapter) override; // DecoderBase
    bool DoRewind(long long desiredFrame, bool discardFrames = true) override; // DecoderBase
    bool DoFastForward(long long desiredFrame, bool discardFrames = true) override; // DecoderBase
    void SetIdrOnlyKeyframes(bool value) override // DecoderBase
        { m_h264Parser->use_I_forKeyframes(!value); }

    int64_t NormalizeVideoTimecode(int64_t timecode) override; // DecoderBase
    virtual int64_t NormalizeVideoTimecode(AVStream *st, int64_t timecode);

    int  GetTeletextDecoderType(void) const override; // DecoderBase

    QString GetXDS(const QString&) const override; // DecoderBase
    QByteArray GetSubHeader(uint trackNo) const override; // DecoderBase
    void GetAttachmentData(uint trackNo, QByteArray &filename,
                           QByteArray &data) override; // DecoderBase

    // MHEG stuff
    bool SetAudioByComponentTag(int tag) override; // DecoderBase
    bool SetVideoByComponentTag(int tag) override; // DecoderBase

    // Stream language info
    virtual int GetTeletextLanguage(uint lang_idx) const;
    virtual int GetSubtitleLanguage(uint subtitle_index, uint stream_index);
    virtual int GetCaptionLanguage(TrackType trackType, int service_num);
    virtual int GetAudioLanguage(uint audio_index, uint stream_index);
    virtual AudioTrackType GetAudioTrackType(uint stream_index);

    static int GetMaxReferenceFrames(AVCodecContext *Context);

  private:
    AvFormatDecoder(const AvFormatDecoder &) = delete;            // not copyable
    AvFormatDecoder &operator=(const AvFormatDecoder &) = delete; // not copyable

  protected:
    int  AutoSelectTrack(uint type) override; // DecoderBase
    void ScanATSCCaptionStreams(int av_index);
    void UpdateATSCCaptionTracks(void);
    void UpdateCaptionTracksFromStreams(bool check_608, bool check_708);
    void ScanTeletextCaptions(int av_index);
    void ScanRawTextCaptions(int av_stream_index);
    void ScanDSMCCStreams(void);
    int  AutoSelectAudioTrack(void);
    int  filter_max_ch(const AVFormatContext *ic,
                       const sinfo_vec_t     &tracks,
                       const vector<int>     &fs,
                       enum AVCodecID         codecId = AV_CODEC_ID_NONE,
                       int                    profile = -1);

    friend int get_avf_buffer(struct AVCodecContext *c, AVFrame *pic,
                              int flags);
    friend int open_avf(URLContext *h, const char *filename, int flags);
    friend int read_avf(URLContext *h, uint8_t *buf, int buf_size);
    friend int write_avf(URLContext *h, uint8_t *buf, int buf_size);
    friend int64_t seek_avf(URLContext *h, int64_t offset, int whence);
    friend int close_avf(URLContext *h);

    void DecodeDTVCC(const uint8_t *buf, uint buf_size, bool scte);
    void DecodeCCx08(const uint8_t *buf, uint buf_size, bool scte);
    void InitByteContext(bool forceseek = false);
    void InitVideoCodec(AVStream *stream, AVCodecContext *enc,
                        bool selectedStream = false);

    /// Preprocess a packet, setting the video parms if necessary.
    void MpegPreProcessPkt(AVStream *stream, AVPacket *pkt);
    int  H264PreProcessPkt(AVStream *stream, AVPacket *pkt);
    bool PreProcessVideoPacket(AVStream *stream, AVPacket *pkt);
    virtual bool ProcessVideoPacket(AVStream *stream, AVPacket *pkt, bool &Retry);
    virtual bool ProcessVideoFrame(AVStream *Stream, AVFrame *AvFrame);
    bool ProcessAudioPacket(AVStream *stream, AVPacket *pkt,
                            DecodeType decodetype);
    bool ProcessSubtitlePacket(AVStream *stream, AVPacket *pkt);
    bool ProcessRawTextPacket(AVPacket *pkt);
    virtual bool ProcessDataPacket(AVStream *curstream, AVPacket *pkt,
                                   DecodeType decodetype);

    void ProcessVBIDataPacket(const AVStream *stream, const AVPacket *pkt);
    void ProcessDVBDataPacket(const AVStream *stream, const AVPacket *pkt);
    void ProcessDSMCCPacket(const AVStream *stream, const AVPacket *pkt);

    void SeekReset(long long, uint skipFrames, bool doFlush, bool discardFrames) override; // DecoderBase

    inline bool DecoderWillDownmix(const AVCodecContext *ctx);
    bool DoPassThrough(const AVCodecParameters *par, bool withProfile=true);
    bool SetupAudioStream(void);
    void SetupAudioStreamSubIndexes(int streamIndex);
    void RemoveAudioStreams();

    /// Update our position map, keyframe distance, and the like.
    /// Called for key frame packets.
    void HandleGopStart(AVPacket *pkt, bool can_reliably_parse_keyframes);

    bool GenerateDummyVideoFrames(void);
    bool HasVideo(const AVFormatContext *ic);
    float normalized_fps(AVStream *stream, AVCodecContext *enc);
    static void av_update_stream_timings_video(AVFormatContext *ic);
    static bool OpenAVCodec(AVCodecContext *avctx, const AVCodec *codec);

    void UpdateFramesPlayed(void) override; // DecoderBase
    bool DoRewindSeek(long long desiredFrame) override; // DecoderBase
    void DoFastForwardSeek(long long desiredFrame, bool &needflush) override; // DecoderBase
    virtual void StreamChangeCheck(void);
    virtual void PostProcessTracks(void) { }
    virtual bool IsValidStream(int /*streamid*/) {return true;}

    int DecodeAudio(AVCodecContext *ctx, uint8_t *buffer, int &data_size,
                    AVPacket *pkt);

    virtual int ReadPacket(AVFormatContext *ctx, AVPacket *pkt, bool &storePacket);

    PrivateDecoder    *m_privateDec                   {nullptr};

    bool               m_isDbIgnored;

    H264Parser        *m_h264Parser                   {nullptr};

    AVFormatContext   *m_ic                           {nullptr};
    // AVFormatParameters params;

    URLContext         m_readContext                  {};

    int                m_frameDecoded                 {0};
    VideoFrame        *m_decodedVideoFrame            {nullptr};
    AVFRingBuffer     *m_avfRingBuffer                {nullptr};

    struct SwsContext *m_swsCtx                       {nullptr};
    bool               m_directRendering              {false};

    bool               m_noDtsHack                    {false};
    bool               m_doRewind                     {false};

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

    long long          m_lastVPts                     {0};
    long long          m_lastAPts                     {0};
    long long          m_lastCcPtsu                   {0};
    long long          m_firstVPts                    {0};
    bool               m_firstVPtsInuse               {false};

    int64_t            m_faultyPts                    {0};
    int64_t            m_faultyDts                    {0};
    int64_t            m_lastPtsForFaultDetection     {0};
    int64_t            m_lastDtsForFaultDetection     {0};
    bool               m_ptsDetected                  {false};
    bool               m_reorderedPtsDetected         {false};
    bool               m_ptsSelected                  {true};
    // set use_frame_timing true to utilize the pts values in returned
    // frames. Set fale to use deprecated method.
    bool               m_useFrameTiming               {false};

    bool               m_forceDtsTimestamps           {false};

    PlayerFlags        m_playerFlags;
    MythCodecID        m_videoCodecId                 {kCodec_NONE};

    int                m_maxKeyframeDist              {-1};
    int                m_averrorCount                 {0};

    // Caption/Subtitle/Teletext decoders
    uint               m_ignoreScte                   {0};
    uint               m_invertScteField              {0};
    uint               m_lastScteField                {0};
    CC608Decoder      *m_ccd608                       {nullptr};
    CC708Decoder      *m_ccd708                       {nullptr};
    TeletextDecoder   *m_ttd                          {nullptr};
    int                m_cc608ParityTable[256]        {0};
    /// Lookup table for whether a stream was seen in the PMT
    /// entries 0-3 correspond to CEA-608 CC1 through CC4, while
    /// entries 4-67 corresport to CEA-708 streams 0 through 64
    bool               m_ccX08InPmt[64+4]             {};
    /// Lookup table for whether a stream is represented in the UI
    /// entries 0-3 correspond to CEA-608 CC1 through CC4, while
    /// entries 4-67 corresport to CEA-708 streams 0 through 64
    bool               m_ccX08InTracks[64+4]          {};
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

    float              m_fps                          {0.0F};
    bool               m_processFrames                {true};

    bool               m_streamsChanged               { false };
    bool               m_resetHardwareDecoders        { false };

    // Value in milliseconds, from setting AudioReadAhead
    int                m_audioReadAhead               {100};
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
