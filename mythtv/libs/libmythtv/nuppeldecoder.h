#ifndef NUPPELDECODER_H_
#define NUPPELDECODER_H_

#include <qmap.h>
#include <qptrlist.h>

#include "programinfo.h"
#include "format.h"
#include "decoderbase.h"

#ifdef MMX
#undef MMX
#define MMXBLAH
#endif
#include <lame/lame.h>
#ifdef MMXBLAH
#define MMX
#endif

#include "RTjpegN.h"

extern "C" {
#include "frame.h"
#include "../libavcodec/avcodec.h"
}

class RawDataList
{
  public:
    RawDataList(struct rtframeheader frameh, unsigned char *data)
    { frameheader = frameh; packet = data; }
   ~RawDataList() { if (packet) delete [] packet; }
  
    struct rtframeheader frameheader;
    unsigned char *packet;
};

class NuppelDecoder : public DecoderBase
{
  public:
    NuppelDecoder(NuppelVideoPlayer *parent, ProgramInfo *pginfo);
   ~NuppelDecoder();

    static bool CanHandle(char testbuf[2048]);

    int OpenFile(RingBuffer *rbuffer, bool novideo, char testbuf[2048]);
    bool GetFrame(int onlyvideo);

    // lastFrame is really (framesPlayed - 1) since we increment after getting
    bool isLastFrameKey(void) { return (lastKey == (framesPlayed - 1)); }
    void WriteStoredData(RingBuffer *rb, bool writevid, long timecodeOffset);
    void ClearStoredData(void);

    long UpdateStoredFrameNum(long framenumber);

    QString GetEncodingType(void) const;

  private:
    inline bool NuppelDecoder::ReadFileheader(struct rtfileheader *fileheader);
    inline bool NuppelDecoder::ReadFrameheader(struct rtframeheader *frameheader);

    bool DecodeFrame(struct rtframeheader *frameheader,
                     unsigned char *lstrm, VideoFrame *frame);
    bool isValidFrametype(char type);

    bool InitAVCodec(int codec);
    void CloseAVCodec(void);
    void StoreRawData(unsigned char *strm);

    void SeekReset(long long newKey = 0, int skipFrames = 0,
                   bool needFlush = false);

    friend int get_nuppel_buffer(struct AVCodecContext *c, AVFrame *pic);
    friend void release_nuppel_buffer(struct AVCodecContext *c, AVFrame *pic);

    struct rtfileheader fileheader;
    struct rtframeheader frameheader;

    lame_global_flags *gf;
    RTjpeg *rtjd;

    int video_width, video_height, video_size;
    double video_frame_rate;
    int audio_samplerate;
#ifdef WORDS_BIGENDIAN
    int audio_bits_per_sample;
#endif

    int ffmpeg_extradatasize;
    char *ffmpeg_extradata;

    struct extendeddata extradata;
    bool usingextradata;

    bool disablevideo;

    int totalLength;
    long long totalFrames;

    int effdsp;

    unsigned char *directbuf;

    AVCodec *mpa_codec;
    AVCodecContext *mpa_ctx;
    AVPicture tmppicture;

    bool directrendering;

    char lastct;

    unsigned char *strm;
    unsigned char *buf;
    unsigned char *buf2;
    unsigned char *planes[3];

    QPtrList <RawDataList> StoredData;

    int videosizetotal;
    int videoframesread;
    bool setreadahead;
};

#endif
