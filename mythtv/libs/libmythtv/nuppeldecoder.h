#ifndef NUPPELDECODER_H_
#define NUPPELDECODER_H_

#include <qmap.h>
#include <qptrlist.h>

#include "programinfo.h"
#include "decoderbase.h"
#include "format.h"

#ifdef MMX
#undef MMX
#define MMXBLAH
#endif
#include <lame/lame.h>
#ifdef MMXBLAH
#define MMX
#endif

#include "RTjpegN.h"
#include "format.h"

extern "C" {
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
    NuppelDecoder(NuppelVideoPlayer *parent, QSqlDatabase *db,
                  ProgramInfo *pginfo);
   ~NuppelDecoder();

    void Reset(void);

    static bool CanHandle(char testbuf[2048]);

    int OpenFile(RingBuffer *rbuffer, bool novideo, char testbuf[2048]);
    void GetFrame(int onlyvideo);

    bool DoRewind(long long desiredFrame);
    bool DoFastForward(long long desiredFrame);

    bool isLastFrameKey(void) { return (lastKey == framesPlayed); }
    void WriteStoredData(RingBuffer *rb);
    void SetRawFrameState(bool state) { getrawframes = state; }
    bool GetRawFrameState(void) { return getrawframes; }

    void SetPositionMap(void);

  private:
    bool DecodeFrame(struct rtframeheader *frameheader,
                     unsigned char *lstrm, unsigned char *outbuf);
    bool isValidFrametype(char type);

    bool InitAVCodec(int codec);
    void CloseAVCodec(void);
    void StoreRawData(unsigned char *strm);

    friend int get_nuppel_buffer(struct AVCodecContext *c, AVFrame *pic);
    friend void release_nuppel_buffer(struct AVCodecContext *c, AVFrame *pic);

    struct rtfileheader fileheader;
    struct rtframeheader frameheader;

    lame_global_flags *gf;
    RTjpeg *rtjd;

    int video_width, video_height, video_size, keyframedist;
    double video_frame_rate;
    int audio_samplerate;

    int ffmpeg_extradatasize;
    char *ffmpeg_extradata;

    struct extendeddata extradata;
    bool usingextradata;

    bool disablevideo;

    bool hasFullPositionMap;
    QMap<long long, long long> *positionMap;

    int totalLength;
    long long totalFrames;

    int effdsp;

    unsigned char *directbuf;

    AVCodec *mpa_codec;
    AVCodecContext *mpa_ctx;
    AVFrame *mpa_pic;
    AVPicture tmppicture;
    AVPicture mpa_pic_tmp;

    bool directrendering;

    char lastct;

    unsigned char *strm;
    unsigned char *buf;
    unsigned char *buf2;
    unsigned char *planes[3];

    long long lastKey;
    long long framesPlayed;

    QSqlDatabase *m_db;
    ProgramInfo *m_playbackinfo;

    QPtrList <RawDataList> StoredData;
    bool getrawframes;
};

#endif
