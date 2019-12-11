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

    bool GetFrame(DecodeType, bool&) override; // DecoderBase

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
    static bool isValidFrametype(char type);

    bool InitAVCodecVideo(int codec);
    void CloseAVCodecVideo(void);
    bool InitAVCodecAudio(int codec);
    void CloseAVCodecAudio(void);
    void StoreRawData(unsigned char *strm);

    void SeekReset(long long newKey = 0, uint skipFrames = 0,
                   bool doFlush = false, bool discardFrames = false) override; // DecoderBase

    friend int get_nuppel_buffer(struct AVCodecContext *c, AVFrame *pic, int flags);
    friend void release_nuppel_buffer(void *opaque, uint8_t *data);

    struct rtfileheader   m_fileHeader;
    struct rtframeheader  m_frameHeader;

    RTjpeg               *m_rtjd                  {nullptr};

    int                   m_videoWidth           {0};
    int                   m_videoHeight          {0};
    int                   m_videoSize            {0};
    double                m_videoFrameRate       {0.0};
    int                   m_audioSamplerate      {44100};
#if HAVE_BIGENDIAN
    int                   m_audioBitsPerSample   {0};
#endif

    int                   m_ffmpegExtraDataSize   {0};
    uint8_t              *m_ffmpegExtraData       {nullptr};

    struct extendeddata   m_extraData;
    bool                  m_usingExtraData        {false};

    bool                  m_disableVideo          {false};

    int                   m_totalLength           {0};
    long long             m_totalFrames           {0};

    int                   m_effDsp                {0};

    VideoFrame           *m_directFrame           {nullptr};
    VideoFrame           *m_decodedVideoFrame     {nullptr};

    AVCodec              *m_mpaVidCodec           {nullptr};
    AVCodecContext       *m_mpaVidCtx             {nullptr};
    AVCodec              *m_mpaAudCodec           {nullptr};
    AVCodecContext       *m_mpaAudCtx             {nullptr};
    uint8_t              *m_audioSamples          {nullptr};

    bool                  m_directRendering       {false};

    char                  m_lastCt                {'1'};

    unsigned char        *m_strm                  {nullptr};
    unsigned char        *m_buf                   {nullptr};
    unsigned char        *m_buf2                  {nullptr};
    unsigned char        *m_planes[3];

    list<RawDataList*>    m_storedData;

    int                   m_videoSizeTotal        {0};
    int                   m_videoFramesRead       {0};
    bool                  m_setReadAhead          {false};
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
