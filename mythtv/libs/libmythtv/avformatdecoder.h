#ifndef AVFORMATDECODER_H_
#define AVFORMATDECODER_H_

#include <stdint.h>

#include <QString>
#include <QMap>
#include <QList>

#include "programinfo.h"
#include "format.h"
#include "decoderbase.h"
#include "privatedecoder.h"
#include "audiooutputsettings.h"
#include "vbilut.h"
#include "H264Parser.h"
#include "videodisplayprofile.h"

extern "C" {
#include "frame.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavcodec/audioconvert.h"
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

class AudioInfo
{
  public:
    AudioInfo() :
        codec_id(CODEC_ID_NONE), format(FORMAT_NONE), sample_size(-2),
        sample_rate(-1), channels(-1), do_passthru(false),
        original_channels(-1)
    {;}

    AudioInfo(CodecID id, AudioFormat fmt, int sr, int ch, bool passthru,
              int original_ch) :
        codec_id(id), format(fmt),
        sample_size(ch * AudioOutputSettings::SampleSize(fmt)),
        sample_rate(sr), channels(ch), do_passthru(passthru),
        original_channels(original_ch)
    {
    }

    CodecID codec_id;
    AudioFormat format;
    int sample_size, sample_rate, channels;
    bool do_passthru;
    int original_channels;

    bool operator==(const AudioInfo &o) const
    {
        return (codec_id==o.codec_id        && channels==o.channels       &&
                sample_size==o.sample_size  && sample_rate==o.sample_rate &&
                format==o.format            && do_passthru==o.do_passthru &&
                original_channels==o.original_channels);
    }
    QString toString() const
    {
        return QString("id(%1) %2Hz %3ch %4bps %5")
            .arg(ff_codec_id_string(codec_id),4).arg(sample_rate,6)
            .arg(channels,2).arg(AudioOutputSettings::FormatToBits(format),2)
            .arg((do_passthru) ? "pt":"",3);
    }
};

/// A decoder for video files.

/// The AvFormatDecoder is used to decode non-NuppleVideo files.
/// It's used as a decoder of last resort after trying the NuppelDecoder
class AvFormatDecoder : public DecoderBase
{
    friend void HandleStreamChange(void*);
    friend void HandleDVDStreamChange(void*);
  public:
    static void GetDecoders(render_opts &opts);
    AvFormatDecoder(MythPlayer *parent, const ProgramInfo &pginfo,
                    bool use_null_video_out,
                    bool allow_private_decode = true,
                    bool no_hardware_decode = false,
                    AVSpecialDecode av_special_decode = kAVSpecialDecode_None);
   ~AvFormatDecoder();

    virtual void SetEof(bool eof);

    void CloseCodecs();
    void CloseContext();
    virtual void Reset(void);
    void Reset(bool reset_video_data = true, bool seek_reset = true);

    /// Perform an av_probe_input_format on the passed data to see if we
    /// can decode it with this class.
    static bool CanHandle(char testbuf[kDecoderProbeBufferSize], 
                          const QString &filename,
                          int testbufsize = kDecoderProbeBufferSize);

    /// Open our file and set up or audio and video parameters.
    int OpenFile(RingBuffer *rbuffer, bool novideo, 
                 char testbuf[kDecoderProbeBufferSize],
                 int testbufsize = kDecoderProbeBufferSize);

    virtual bool GetFrame(DecodeType); // DecoderBase

    bool isLastFrameKey(void) { return false; }

    /// This is a No-op for this class.
    void WriteStoredData(RingBuffer *rb, bool storevid, long timecodeOffset)
                           { (void)rb; (void)storevid; (void)timecodeOffset;}

    /// This is a No-op for this class.
    void SetRawAudioState(bool state) { (void)state; }

    /// This is a No-op for this class.
    bool GetRawAudioState(void) const { return false; }

    /// This is a No-op for this class.
    void SetRawVideoState(bool state) { (void)state; }

    /// This is a No-op for this class.
    bool GetRawVideoState(void) const { return false; }

    /// This is a No-op for this class.
    long UpdateStoredFrameNum(long frame) { (void)frame; return 0;}

    QString      GetCodecDecoderName(void) const;
    QString      GetRawEncodingType(void);
    MythCodecID  GetVideoCodecID(void) const { return video_codec_id; }
    void        *GetVideoCodecPrivate(void);

    virtual void SetDisablePassThrough(bool disable);
    void AddTextData(unsigned char *buf, int len, int64_t timecode, char type);

    virtual QString GetTrackDesc(uint type, uint trackNo) const;
    virtual int SetTrack(uint type, int trackNo);

    int ScanStreams(bool novideo);

    virtual int  GetNumChapters();
    virtual void GetChapterTimes(QList<long long> &times);
    virtual int  GetCurrentChapter(long long framesPlayed);
    virtual long long GetChapter(int chapter);
    virtual bool DoRewind(long long desiredFrame, bool doflush = true);
    virtual bool DoFastForward(long long desiredFrame, bool doflush = true);

    virtual int64_t NormalizeVideoTimecode(int64_t timecode);
    virtual int64_t NormalizeVideoTimecode(AVStream *st, int64_t timecode);

    virtual int  GetTeletextDecoderType(void) const;

    virtual QString GetXDS(const QString&) const;

    // MHEG stuff
    virtual bool SetAudioByComponentTag(int tag);
    virtual bool SetVideoByComponentTag(int tag);

  protected:
    RingBuffer *getRingBuf(void) { return ringBuffer; }

    virtual int AutoSelectTrack(uint type);

    void ScanATSCCaptionStreams(int av_stream_index);
    void UpdateATSCCaptionTracks(void);
    void UpdateCaptionTracksFromStreams(bool check_608, bool check_708);
    void ScanTeletextCaptions(int av_stream_index);
    void ScanRawTextCaptions(int av_stream_index);
    void ScanDSMCCStreams(void);
    int AutoSelectAudioTrack(void);

  private:
    friend int get_avf_buffer(struct AVCodecContext *c, AVFrame *pic);
    friend void release_avf_buffer(struct AVCodecContext *c, AVFrame *pic);

    friend int get_avf_buffer_xvmc(struct AVCodecContext *c, AVFrame *pic);
    friend void release_avf_buffer_xvmc(struct AVCodecContext *c, AVFrame *pic);
    friend void render_slice_xvmc(struct AVCodecContext *c, const AVFrame *src,
                                  int offset[4], int y, int type, int height);

    friend int open_avf(URLContext *h, const char *filename, int flags);
    friend int read_avf(URLContext *h, uint8_t *buf, int buf_size);
    friend int write_avf(URLContext *h, uint8_t *buf, int buf_size);
    friend int64_t seek_avf(URLContext *h, int64_t offset, int whence);
    friend int close_avf(URLContext *h);

    void DecodeDTVCC(const uint8_t *buf, uint buf_size);
    void InitByteContext(void);
    void InitVideoCodec(AVStream *stream, AVCodecContext *enc,
                        bool selectedStream = false);

    /// Preprocess a packet, setting the video parms if necessary.
    void MpegPreProcessPkt(AVStream *stream, AVPacket *pkt);
    bool H264PreProcessPkt(AVStream *stream, AVPacket *pkt);
    bool PreProcessVideoPacket(AVStream *stream, AVPacket *pkt);
    bool ProcessVideoPacket(AVStream *stream, AVPacket *pkt);
    bool ProcessVideoFrame(AVStream *stream, AVFrame *mpa_pic);
    bool ProcessAudioPacket(AVStream *stream, AVPacket *pkt,
                            DecodeType decodetype);
    bool ProcessSubtitlePacket(AVStream *stream, AVPacket *pkt);
    bool ProcessRawTextPacket(AVPacket *pkt);
    bool ProcessDataPacket(AVStream *curstream, AVPacket *pkt,
                           DecodeType decodetype);

    void ProcessVBIDataPacket(const AVStream *stream, const AVPacket *pkt);
    void ProcessDVBDataPacket(const AVStream *stream, const AVPacket *pkt);
    void ProcessDSMCCPacket(const AVStream *stream, const AVPacket *pkt);

    float GetMpegAspect(AVCodecContext *context, int aspect_ratio_info,
                        int width, int height);

    void SeekReset(long long, uint skipFrames, bool doFlush, bool discardFrames);

    inline bool DecoderWillDownmix(const AVCodecContext *ctx);
    bool DoPassThrough(const AVCodecContext *ctx);
    bool SetupAudioStream(void);
    void SetupAudioStreamSubIndexes(int streamIndex);
    void RemoveAudioStreams();

    /// Update our position map, keyframe distance, and the like.
    /// Called for key frame packets.
    void HandleGopStart(AVPacket *pkt, bool can_reliably_parse_keyframes);

    bool GenerateDummyVideoFrame(void);
    bool HasVideo(const AVFormatContext *ic);
    float normalized_fps(AVStream *stream, AVCodecContext *enc);
    void av_update_stream_timings_video(AVFormatContext *ic);

  private:
    PrivateDecoder *private_dec;

    bool is_db_ignored;

    H264Parser *m_h264_parser;

    AVFormatContext *ic;
    AVFormatParameters params;

    URLContext readcontext;

    int frame_decoded;
    VideoFrame *decoded_video_frame;
    AVFRingBuffer *avfRingBuffer;

    struct SwsContext *sws_ctx;
    bool directrendering;

    bool no_dts_hack;
    bool dorewind;

    bool gopset;
    /// A flag to indicate that we've seen a GOP frame.  Used in junction with seq_count.
    bool seen_gop;
    int seq_count; ///< A counter used to determine if we need to force a call to HandleGopStart

    QList<AVPacket*> storedPackets;

    int prevgoppos;

    bool gotvideo;

    // from full GetFrame implementation
    bool skipaudio;
    bool allowedquit;

    uint32_t  start_code_state;

    long long lastvpts;
    long long lastapts;
    long long lastccptsu;

    int64_t faulty_pts;
    int64_t faulty_dts;
    int64_t last_pts_for_fault_detection;
    int64_t last_dts_for_fault_detection;
    bool pts_detected;
    bool reordered_pts_detected;
    bool pts_selected;

    bool using_null_videoout;
    MythCodecID video_codec_id;
    bool no_hardware_decoders;
    bool allow_private_decoders;
    AVSpecialDecode special_decode;

    int maxkeyframedist;

    // Caption/Subtitle/Teletext decoders
    CC608Decoder     *ccd608;
    CC708Decoder     *ccd708;
    TeletextDecoder  *ttd;
    SubtitleReader   *subReader;
    int               cc608_parity_table[256];
    /// Lookup table for whether a stream was seen in the PMT
    /// entries 0-3 correspond to CEA-608 CC1 through CC4, while
    /// entries 4-67 corresport to CEA-708 streams 0 through 64
    bool              ccX08_in_pmt[64+4];
    /// Lookup table for whether a stream is represented in the UI
    /// entries 0-3 correspond to CEA-608 CC1 through CC4, while
    /// entries 4-67 corresport to CEA-708 streams 0 through 64
    bool              ccX08_in_tracks[64+4];
    /// StreamInfo for 608 and 708 Captions seen in the PMT descriptor
    QList<StreamInfo> pmt_tracks;
    /// TrackType (608 or 708) for Captions seen in the PMT descriptor
    QList<TrackType>  pmt_track_types;
    /// StreamInfo for 608 and 708 Captions seen in the caption stream itself
    /// but not seen in the PMT
    QList<StreamInfo> stream_tracks;
    /// TrackType (608 or 708) for Captions seen in the caption stream itself
    /// but not seen in the PMT
    QList<TrackType>  stream_track_types;

    // MHEG
    InteractiveTV    *itv;                ///< MHEG/MHP decoder

    // Audio
    short int        *audioSamples;
    bool              internal_vol;
    bool              disable_passthru;

    VideoFrame       *dummy_frame;

    AudioInfo         audioIn;
    AudioInfo         audioOut;

    // DVD
    bool dvd_xvmc_enabled;
    bool dvd_video_codec_changed;

    float m_fps;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
