#ifndef AVFORMATDECODER_H_
#define AVFORMATDECODER_H_

#include <qstring.h>
#include <qmap.h>

#include "programinfo.h"
#include "format.h"
#include "decoderbase.h"

extern "C" {
#include "frame.h"
#include "../libavcodec/avcodec.h"
#include "../libavformat/avformat.h"
}

class ProgramInfo;
class MythSqlDatabase;

extern "C" void HandleStreamChange(void*);

/// A decoder for video files.

/// The AvFormatDecoder is used to decode non-NuppleVideo files.
/// It's used a a decoder of last resort after trying the NuppelDecoder
/// and IvtvDecoder (if "USING_IVTV" is defined).
class AvFormatDecoder : public DecoderBase
{
    friend void HandleStreamChange(void*);
  public:
    AvFormatDecoder(NuppelVideoPlayer *parent, ProgramInfo *pginfo,
                    bool use_null_video_out);
   ~AvFormatDecoder();

    void CloseContext();
    void Reset(void);
    void Reset(bool reset_video_data = true, bool seek_reset = true);

    /// Perform an av_probe_input_format on the passed data to see if we
    /// can decode it with this class.
    static bool CanHandle(char testbuf[2048], const QString &filename);

    /// Open our file and set up or audio and video parameters.
    int OpenFile(RingBuffer *rbuffer, bool novideo, char testbuf[2048]);

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
    bool GetRawAudioState(void) { return false; }

    /// This is a No-op for this class.
    void SetRawVideoState(bool state) { (void)state; }

    /// This is a No-op for this class.
    bool GetRawVideoState(void) { return false; }

    /// This is a No-op for this class.
    long UpdateStoredFrameNum(long frame) { (void)frame; return 0;}

    QString GetEncodingType(void) { return QString("MPEG-2"); }

    MythCodecID GetVideoCodecID() { return video_codec_id; }

    virtual void incCurrentAudioTrack();
    virtual void decCurrentAudioTrack();
    virtual bool setCurrentAudioTrack(int trackNo);

    int ScanStreams(bool novideo);

    virtual bool DoRewind(long long desiredFrame, bool doflush = true);
    virtual bool DoFastForward(long long desiredFrame, bool doflush = true);

  protected:
    /// Attempt to find the optimal audio stream to use based on the number of channels,
    /// and if we're doing AC3 passthrough.  This will select the highest stream number
    /// that matches our criteria.
    bool autoSelectAudioTrack();

    RingBuffer *getRingBuf(void) { return ringBuffer; }

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
    friend offset_t seek_avf(URLContext *h, offset_t offset, int whence);
    friend int close_avf(URLContext *h);

    void InitByteContext(void);
    void InitVideoCodec(AVCodecContext *enc);

    /// Preprocess a packet, setting the video parms if nessesary.
    /// Also feeds HandleGopStart for MPEG2 files.
    void MpegPreProcessPkt(AVStream *stream, AVPacket *pkt);

    float GetMpegAspect(AVCodecContext *context, int aspect_ratio_info,
                        int width, int height);

    void SeekReset(long long newKey = 0, int skipFrames = 0,
                   bool needFlush = false);

    /// See if the video parameters have changed, return true if so.
    bool CheckVideoParams(int width, int height);

    /// See if the audio parameters have changed, return true if so.
    void CheckAudioParams(int freq, int channels, bool safe);
    void SetupAudioStream(void);

    int EncodeAC3Frame(unsigned char* data, int len, short *samples,
                       int &samples_size);

    // Update our position map, keyframe distance, and the like.  Called for key frame packets.
    void HandleGopStart(AVPacket *pkt);

    bool IsWantedSubtitleLanguage(const QString &language);    
    class AvFormatDecoderPrivate *d;

    AVFormatContext *ic;
    AVFormatParameters params;

    URLContext readcontext;

    int frame_decoded;

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

    bool using_null_videoout;
    MythCodecID video_codec_id;

    int maxkeyframedist;

    QStringList subtitleLanguagePreferences;

    // Audio
    short int        *audioSamples;
    int               audio_sample_size;
    int               audio_sampling_rate;
    int               audio_channels;
    bool              do_ac3_passthru;
    int               wantedAudioStream;
    QValueVector<int> audioStreams;
    int               audio_check_1st;         ///< Used by CheckAudioParams
    int               audio_sampling_rate_2nd; ///< Used by CheckAudioParams
    int               audio_channels_2nd;      ///< Used by CheckAudioParams
};

#endif
