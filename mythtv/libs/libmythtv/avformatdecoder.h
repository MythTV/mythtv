#ifndef AVFORMATDECODER_H_
#define AVFORMATDECODER_H_

#include <qstring.h>
#include <qmap.h>
#include <qsqldatabase.h>

#include "programinfo.h"
#include "format.h"
#include "decoderbase.h"
#include "frame.h"

extern "C" {
#include "../libavcodec/avcodec.h"
#include "../libavformat/avformat.h"
}

class ProgramInfo;

class AvFormatDecoder : public DecoderBase
{
  public:
    AvFormatDecoder(NuppelVideoPlayer *parent, QSqlDatabase *db,
                    ProgramInfo *pginfo);
   ~AvFormatDecoder();

    void Reset(void);

    static bool CanHandle(char testbuf[2048], const QString &filename);

    int OpenFile(RingBuffer *rbuffer, bool novideo, char testbuf[2048]);
    void GetFrame(int onlyvideo);

    bool DoRewind(long long desiredFrame);
    bool DoFastForward(long long desiredFrame);

    bool isLastFrameKey(void) { return false; }
    void WriteStoredData(RingBuffer *rb, bool storevid)
                           { (void)rb; (void)storevid; }
    void SetRawAudioState(bool state) { (void)state; }
    bool GetRawAudioState(void) { return false; }
    void SetRawVideoState(bool state) { (void)state; }
    bool GetRawVideoState(void) { return false; }

    void UpdateFrameNumber(long frame) { (void)frame;}

    void SetPositionMap(void);

    QString GetEncodingType(void) { return QString("MPEG-2"); }

  protected:
    RingBuffer *getRingBuf(void) { return ringBuffer; }

  private:
    friend int get_avf_buffer(struct AVCodecContext *c, AVFrame *pic);
    friend void release_avf_buffer(struct AVCodecContext *c, AVFrame *pic);

    friend int get_avf_buffer_xvmc(struct AVCodecContext *c, AVFrame *pic);
    friend void release_avf_buffer_xvmc(struct AVCodecContext *c, AVFrame *pic);
    friend void render_slice_xvmc(struct AVCodecContext *c, AVFrame *src, 
                                  int offset[4], int y, int type, int height);

    friend int open_avf(URLContext *h, const char *filename, int flags);
    friend int read_avf(URLContext *h, uint8_t *buf, int buf_size);
    friend int write_avf(URLContext *h, uint8_t *buf, int buf_size);
    friend offset_t seek_avf(URLContext *h, offset_t offset, int whence);
    friend int close_avf(URLContext *h);

    void InitByteContext(void);
    int PacketHasHeader(unsigned char *buf, int len, unsigned int startcode);
    float GetMpegAspect(AVCodecContext *context, int aspect_ratio_info,
                        int width, int height);

    void SeekReset(void);

    bool CheckVideoParams(int width, int height);
    bool CheckAudioParams(int freq, int channels);

    RingBuffer *ringBuffer;

    AVFormatContext *ic;
    AVFormatParameters params;

    URLContext readcontext;

    int frame_decoded;

    bool directrendering;
    bool drawband;
    long long framesPlayed;
    long long framesRead;

    int audio_sample_size;
    int audio_sampling_rate;
    int audio_channels;

    long long lastapts;
    long long lastvpts;

    bool hasbframes;

    bool hasFullPositionMap;
    QMap<long long, long long> positionMap;

    long long lastKey;

    int keyframedist;

    bool exitafterdecoded;

    int bitrate;
    bool ateof;

    bool gopset;

    QSqlDatabase *m_db;
    ProgramInfo *m_playbackinfo;

    double fps;
    bool validvpts;

    double ptsmultiplier;

    QPtrList<VideoFrame> inUseBuffers;

    int current_width;
    int current_height;
    float current_aspect;

    QPtrList<AVPacket> storedPackets;

    int firstgoppos;
};

#endif
