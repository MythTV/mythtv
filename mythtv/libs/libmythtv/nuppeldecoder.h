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

    // lastFrame is really (m_framesPlayed - 1) since we increment after getting
    bool IsLastFrameKey(void) const override // DecoderBase
        { return (m_lastKey == m_framesPlayed); }
    void WriteStoredData(RingBuffer *rb, bool storevid,
                         long timecodeOffset) override; // DecoderBase
    void ClearStoredData(void) override; // DecoderBase

    long UpdateStoredFrameNum(long framenumber) override; // DecoderBase

    QString GetCodecDecoderName(void) const override // DecoderBase
         { return "nuppel"; }
    QString GetRawEncodingType(void) override; // DecoderBase
    MythCodecID GetVideoCodecID(void) const override; // DecoderBase

  private:
    NuppelDecoder(const NuppelDecoder &) = delete;            // not copyable
    NuppelDecoder &operator=(const NuppelDecoder &) = delete; // not copyable

    inline bool ReadFileheader(struct rtfileheader *fh);
    inline bool ReadFrameheader(struct rtframeheader *fh);

    bool DecodeFrame(struct rtframeheader *frameheader,
                     unsigned char *lstrm, VideoFrame *frame);
    bool isValidFrametype(char type);

    bool InitAVCodecVideo(int codec);
    void CloseAVCodecVideo(void);
    bool InitAVCodecAudio(int codec);
    void CloseAVCodecAudio(void);
    void StoreRawData(unsigned char *strm);

    void SeekReset(long long newKey = 0, uint skipFrames = 0,
                   bool doFlush = false, bool discardFrames = false) override; // DecoderBase

    friend int get_nuppel_buffer(struct AVCodecContext *c, AVFrame *pic, int flags);
    friend void release_nuppel_buffer(void *opaque, uint8_t *data);

    struct rtfileheader   m_fileheader;
    struct rtframeheader  m_frameheader;

    RTjpeg               *m_rtjd                  {nullptr};

    int                   m_video_width           {0};
    int                   m_video_height          {0};
    int                   m_video_size            {0};
    double                m_video_frame_rate      {0.0};
    int                   m_audio_samplerate      {44100};
#if HAVE_BIGENDIAN
    int                   m_audio_bits_per_sample {0};
#endif

    int                   m_ffmpeg_extradatasize  {0};
    uint8_t              *m_ffmpeg_extradata      {nullptr};

    struct extendeddata   m_extradata;
    bool                  m_usingextradata        {false};

    bool                  m_disablevideo          {false};

    int                   m_totalLength           {0};
    long long             m_totalFrames           {0};

    int                   m_effdsp                {0};

    VideoFrame           *m_directframe           {nullptr};
    VideoFrame           *m_decoded_video_frame   {nullptr};

    AVCodec              *m_mpa_vidcodec          {nullptr};
    AVCodecContext       *m_mpa_vidctx            {nullptr};
    AVCodec              *m_mpa_audcodec          {nullptr};
    AVCodecContext       *m_mpa_audctx            {nullptr};
    uint8_t              *m_audioSamples          {nullptr};

    bool                  m_directrendering       {false};

    char                  m_lastct                {'1'};

    unsigned char        *m_strm                  {nullptr};
    unsigned char        *m_buf                   {nullptr};
    unsigned char        *m_buf2                  {nullptr};
    unsigned char        *m_planes[3];

    list<RawDataList*>    m_storedData;

    int                   m_videosizetotal        {0};
    int                   m_videoframesread       {0};
    bool                  m_setreadahead          {false};
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
