#ifndef NUPPELDECODER_H_
#define NUPPELDECODER_H_

#include "config.h"

#include <list>
using namespace std;

#include <QString>

#include "format.h"
#include "decoderbase.h"
#include "mythframe.h"

#include "RTjpegN.h"

extern "C" {
#include "libavcodec/avcodec.h"
}

class ProgramInfo;
class RawDataList
{
  public:
    RawDataList(struct rtframeheader frameh, unsigned char *data) :
        frameheader(frameh), packet(data) {}
   ~RawDataList() { if (packet) delete [] packet; }
  
    struct rtframeheader frameheader;
    unsigned char *packet;
};

class NuppelDecoder : public DecoderBase
{
  public:
    NuppelDecoder(MythPlayer *parent, const ProgramInfo &pginfo);
   ~NuppelDecoder();

    static bool CanHandle(char testbuf[kDecoderProbeBufferSize], 
                          int testbufsize = kDecoderProbeBufferSize);

    int OpenFile(RingBuffer *rbuffer, bool novideo, 
                 char testbuf[kDecoderProbeBufferSize], 
                 int testbufsize = kDecoderProbeBufferSize) override; // DecoderBase

    bool GetFrame(DecodeType) override; // DecoderBase

    // lastFrame is really (framesPlayed - 1) since we increment after getting
    bool IsLastFrameKey(void) const override // DecoderBase
        { return (lastKey == framesPlayed); }
    void WriteStoredData(RingBuffer *rb, bool writevid,
                         long timecodeOffset) override; // DecoderBase
    void ClearStoredData(void) override; // DecoderBase

    long UpdateStoredFrameNum(long framenumber) override; // DecoderBase

    QString GetCodecDecoderName(void) const override // DecoderBase
         { return "nuppel"; }
    QString GetRawEncodingType(void) override; // DecoderBase
    MythCodecID GetVideoCodecID(void) const override; // DecoderBase

  private:
    inline bool ReadFileheader(struct rtfileheader *fileheader);
    inline bool ReadFrameheader(struct rtframeheader *frameheader);

    bool DecodeFrame(struct rtframeheader *frameheader,
                     unsigned char *lstrm, VideoFrame *frame);
    bool isValidFrametype(char type);

    bool InitAVCodecVideo(int codec);
    void CloseAVCodecVideo(void);
    bool InitAVCodecAudio(int codec);
    void CloseAVCodecAudio(void);
    void StoreRawData(unsigned char *strm);

    void SeekReset(long long newKey = 0, uint skipFrames = 0,
                   bool needFlush = false, bool discardFrames = false) override; // DecoderBase

    friend int get_nuppel_buffer(struct AVCodecContext *c, AVFrame *pic, int flags);
    friend void release_nuppel_buffer(void *opaque, uint8_t *data);

    struct rtfileheader fileheader;
    struct rtframeheader frameheader;

    RTjpeg *rtjd;

    int video_width, video_height, video_size;
    double video_frame_rate;
    int audio_samplerate;
#if HAVE_BIGENDIAN
    int audio_bits_per_sample;
#endif

    int ffmpeg_extradatasize;
    uint8_t *ffmpeg_extradata;

    struct extendeddata extradata;
    bool usingextradata;

    bool disablevideo;

    int totalLength;
    long long totalFrames;

    int effdsp;

    VideoFrame *directframe;
    VideoFrame *decoded_video_frame;

    AVCodec *mpa_vidcodec;
    AVCodecContext *mpa_vidctx;
    AVCodec *mpa_audcodec;
    AVCodecContext *mpa_audctx;
    uint8_t *m_audioSamples;

    bool directrendering;

    char lastct;

    unsigned char *strm;
    unsigned char *buf;
    unsigned char *buf2;
    unsigned char *planes[3];

    list<RawDataList*> StoredData;

    int videosizetotal;
    int videoframesread;
    bool setreadahead;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
