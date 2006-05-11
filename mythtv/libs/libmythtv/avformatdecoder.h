#ifndef AVFORMATDECODER_H_
#define AVFORMATDECODER_H_

#include <qstring.h>
#include <qmap.h>

#include "programinfo.h"
#include "format.h"
#include "decoderbase.h"
#include "vbilut.h"
#include "h264utils.h"

extern "C" {
#include "frame.h"
#include "../libavcodec/avcodec.h"
#include "../libavformat/avformat.h"
}

class TeletextDecoder;
class CC608Decoder;
class CC708Decoder;
class InteractiveTV;
class ProgramInfo;
class MythSqlDatabase;

extern "C" void HandleStreamChange(void*);

class AudioInfo
{
  public:
    AudioInfo() :
        codec_id(CODEC_ID_NONE), sample_size(-2),   sample_rate(-1),
        channels(-1), do_passthru(false)
    {;}

    AudioInfo(CodecID id, int sr, int ch, bool passthru) :
        codec_id(id), sample_size(ch*2),   sample_rate(sr),
        channels(ch), do_passthru(passthru)
    {;}

    CodecID codec_id;
    int sample_size, sample_rate, channels;
    bool do_passthru;

    /// \brief Bits per sample.
    int bps(void) const
    {
        uint chan = (channels) ? channels : 2;
        return (8 * sample_size) / chan;
    }
    bool operator==(const AudioInfo &o) const
    {
        return (codec_id==o.codec_id        && channels==o.channels       &&
                sample_size==o.sample_size  && sample_rate==o.sample_rate &&
                do_passthru==o.do_passthru);
    }
    QString toString() const
    {
        return QString("id(%1) %2Hz %3ch %4bps%5")
            .arg(codec_id_string(codec_id),4).arg(sample_rate,5)
            .arg(channels,2).arg(bps(),3)
            .arg((do_passthru) ? "pt":"",3);
    }
};

/// A decoder for video files.

/// The AvFormatDecoder is used to decode non-NuppleVideo files.
/// It's used a a decoder of last resort after trying the NuppelDecoder
/// and IvtvDecoder (if "USING_IVTV" is defined).
class AvFormatDecoder : public DecoderBase
{
    friend void HandleStreamChange(void*);
  public:
    AvFormatDecoder(NuppelVideoPlayer *parent, ProgramInfo *pginfo,
                    bool use_null_video_out, bool allow_libmpeg2 = true);
   ~AvFormatDecoder();

    void CloseContext();
    void Reset(void);
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

    /// Decode a frame of video/audio. If onlyvideo is set, 
    /// just decode the video portion.
    bool GetFrame(int onlyvideo);

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

    QString GetEncodingType(void) const;
    MythCodecID GetVideoCodecID() const { return video_codec_id; }

    virtual void SetDisablePassThrough(bool disable);
    void AddTextData(unsigned char *buf, int len, long long timecode, char type);

    virtual QString GetTrackDesc(uint type, uint trackNo) const;
    virtual int SetTrack(uint type, int trackNo);

    int ScanStreams(bool novideo);

    virtual bool DoRewind(long long desiredFrame, bool doflush = true);
    virtual bool DoFastForward(long long desiredFrame, bool doflush = true);

    virtual int  GetTeletextDecoderType(void) const;
    virtual void SetTeletextDecoderViewer(TeletextViewer*);

    virtual QString GetXDS(const QString&) const;

    // MHEG stuff
    virtual bool ITVUpdate(bool itvVisible);
    virtual bool ITVHandleAction(const QString&);
    virtual void ITVRestart(uint chanid, uint cardid, bool livetv);

    virtual bool SetAudioByComponentTag(int tag);
    virtual bool SetVideoByComponentTag(int tag);

  protected:
    RingBuffer *getRingBuf(void) { return ringBuffer; }

    virtual int AutoSelectTrack(uint type);

    void ScanATSCCaptionStreams(int av_stream_index);
    void ScanTeletextCaptions(int av_stream_index);
    int AutoSelectAudioTrack(void);

  private:
    friend int get_avf_buffer(struct AVCodecContext *c, AVFrame *pic);
    friend void release_avf_buffer(struct AVCodecContext *c, AVFrame *pic);

    friend int get_avf_buffer_xvmc(struct AVCodecContext *c, AVFrame *pic);
    friend void release_avf_buffer_xvmc(struct AVCodecContext *c, AVFrame *pic);
    friend void render_slice_xvmc(struct AVCodecContext *c, const AVFrame *src,
                                  int offset[4], int y, int type, int height);

    friend void decode_cc_dvd(struct AVCodecContext *c, const uint8_t *buf, int buf_size);

    friend int open_avf(URLContext *h, const char *filename, int flags);
    friend int read_avf(URLContext *h, uint8_t *buf, int buf_size);
    friend int write_avf(URLContext *h, uint8_t *buf, int buf_size);
    friend offset_t seek_avf(URLContext *h, offset_t offset, int whence);
    friend int close_avf(URLContext *h);

    void DecodeDTVCC(const uint8_t *buf);
    void InitByteContext(void);
    void InitVideoCodec(AVStream *stream, AVCodecContext *enc);

    /// Preprocess a packet, setting the video parms if nessesary.
    void MpegPreProcessPkt(AVStream *stream, AVPacket *pkt);
    void H264PreProcessPkt(AVStream *stream, AVPacket *pkt);

    void ProcessVBIDataPacket(const AVStream *stream, const AVPacket *pkt);
    void ProcessDVBDataPacket(const AVStream *stream, const AVPacket *pkt);
    void ProcessDSMCCPacket(const AVStream *stream, const AVPacket *pkt);

    float GetMpegAspect(AVCodecContext *context, int aspect_ratio_info,
                        int width, int height);

    void SeekReset(long long, uint skipFrames, bool doFlush, bool discardFrames);

    bool SetupAudioStream(void);

    /// Update our position map, keyframe distance, and the like.
    /// Called for key frame packets.
    void HandleGopStart(AVPacket *pkt);

    class AvFormatDecoderPrivate *d;
    H264::KeyframeSequencer *h264_kf_seq;

    AVFormatContext *ic;
    AVFormatParameters params;

    URLContext readcontext;

    int frame_decoded;
    VideoFrame *decoded_video_frame;

    bool directrendering;
    bool drawband;

    int bitrate;

    bool gopset;
    /// A flag to indicate that we've seen a GOP frame.  Used in junction with seq_count.
    bool seen_gop;
    int seq_count; ///< A counter used to determine if we need to force a call to HandleGopStart

    QPtrList<AVPacket> storedPackets;

    int firstgoppos;
    int prevgoppos;

    bool gotvideo;

    unsigned char prvpkt[3];

    long long lastvpts;
    long long lastapts;
    long long lastccptsu;

    bool using_null_videoout;
    MythCodecID video_codec_id;

    int maxkeyframedist;

    // Caption/Subtitle/Teletext decoders
    CC608Decoder     *ccd608;
    CC708Decoder     *ccd708;
    TeletextDecoder  *ttd;

    // MHEG
    InteractiveTV    *itv;                ///< MHEG/MHP decoder
    int               selectedVideoIndex; ///< MHEG/MHP video stream to use.
    QMutex            itvLock;

    // Audio
    short int        *audioSamples;
    bool              allow_ac3_passthru;
    bool              allow_dts_passthru;
    bool              disable_passthru;

    AudioInfo         audioIn;
    AudioInfo         audioOut;

    // DVD
    int  lastdvdtitle;
    uint lastcellstart;
    bool dvdmenupktseen;
    bool dvdvideopause;

    int lastrepeat;
};

#endif
