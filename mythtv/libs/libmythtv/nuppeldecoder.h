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
    NuppelDecoder(NuppelVideoPlayer *parent, MythSqlDatabase *db,
                  ProgramInfo *pginfo);
   ~NuppelDecoder();

    void Reset(void);

    static bool CanHandle(char testbuf[2048]);

    int OpenFile(RingBuffer *rbuffer, bool novideo, char testbuf[2048]);
    void GetFrame(int onlyvideo);

    bool DoRewind(long long desiredFrame);
    bool DoFastForward(long long desiredFrame);

    bool isLastFrameKey(void) { return (lastKey == framesPlayed); }
    void WriteStoredData(RingBuffer *rb, bool writevid);
    void SetRawAudioState(bool state) { getrawframes = state; }
    bool GetRawAudioState(void) { return getrawframes; }
    void SetRawVideoState(bool state) { getrawvideo = state; }
    bool GetRawVideoState(void) { return getrawvideo; }

    long UpdateStoredFrameNum(long framenumber);

    void SetPositionMap(void);

    QString GetEncodingType(void);

  private:
    bool DecodeFrame(struct rtframeheader *frameheader,
                     unsigned char *lstrm, VideoFrame *frame);
    bool isValidFrametype(char type);

    bool InitAVCodec(int codec);
    void CloseAVCodec(void);
    void StoreRawData(unsigned char *strm);

    int GetKeyIndex(int KeyFrame);

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

    bool hasKeyFrameAdjustMap;
    QMap<long long, int> *keyFrameAdjustMap;

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

    QPtrList <RawDataList> StoredData;
    bool getrawframes;
    bool getrawvideo;

    int videosizetotal;
    int videoframesread;
    bool setreadahead;
};

#endif
