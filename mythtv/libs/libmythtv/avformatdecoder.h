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

class AvFormatDecoder : public DecoderBase
{
  public:
    AvFormatDecoder(NuppelVideoPlayer *parent, MythSqlDatabase *db,
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

    void setWatchingRecording(bool mode);

    long UpdateStoredFrameNum(long frame) { (void)frame; return 0;}

    void SetPositionMap(void);
    
    bool SyncPositionMap();
    bool PosMapFromDb();
    bool PosMapFromEnc();

    QString GetEncodingType(void) { return QString("MPEG-2"); }

    void SetPixelFormat(const int);

  protected:
    RingBuffer *getRingBuf(void) { return ringBuffer; }

  private:
    typedef struct posmapentry 
    {
        long long index; // frame or keyframe number
        long long pos; // position in stream
    } PosMapEntry;

    friend int get_avf_buffer(struct AVCodecContext *c, AVFrame *pic);
    friend void release_avf_buffer(struct AVCodecContext *c, AVFrame *pic);

    friend int get_avf_buffer_xvmc(struct AVCodecContext *c, AVFrame *pic);
    friend void release_avf_buffer_xvmc(struct AVCodecContext *c, AVFrame *pic);
    friend void render_slice_xvmc(struct AVCodecContext *c, const AVFrame *src, 
                                  int offset[4], int y, int type, int height);

    friend int get_avf_buffer_via(struct AVCodecContext *c, AVFrame *pic);
    friend void release_avf_buffer_via(struct AVCodecContext *c, AVFrame *pic);
    friend void render_slice_via(struct AVCodecContext *c, const AVFrame *src,
                                 int offset[4], int y, int type, int height);
        
    friend int open_avf(URLContext *h, const char *filename, int flags);
    friend int read_avf(URLContext *h, uint8_t *buf, int buf_size);
    friend int write_avf(URLContext *h, uint8_t *buf, int buf_size);
    friend offset_t seek_avf(URLContext *h, offset_t offset, int whence);
    friend int close_avf(URLContext *h);

    void InitByteContext(void);
    void MpegPreProcessPkt(AVStream *stream, AVPacket *pkt);
    float GetMpegAspect(AVCodecContext *context, int aspect_ratio_info,
                        int width, int height);

    void SeekReset(void);

    bool CheckVideoParams(int width, int height);
    bool CheckAudioParams(int freq, int channels);

    int EncodeAC3Frame(unsigned char* data, int len, short *samples,
                       int &samples_size);
    bool FindPosition(long long desired_value, bool search_pos,
                      int &lower_bound, int &upper_bound);

    void HandleGopStart(AVPacket *pkt);

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

    int audio_check_1st;
    int audio_sampling_rate_2nd;
    int audio_channels_2nd;

    bool hasbframes;

    bool hasFullPositionMap;
    bool recordingHasPositionMap;
    bool positionMapByFrame;
    bool posmapStarted;
    
    QValueVector<PosMapEntry> m_positionMap;

    long long lastKey;

    int keyframedist;

    bool exitafterdecoded;

    int bitrate;
    bool ateof;

    bool gopset;
    bool seen_gop;
    int seq_count;

    double fps;

    QPtrList<VideoFrame> inUseBuffers;

    int current_width;
    int current_height;
    float current_aspect;

    QPtrList<AVPacket> storedPackets;

    int firstgoppos;
    int prevgoppos;

    bool gotvideo;

    unsigned char prvpkt[3];

    long long video_last_P_pts;
    long long lastvpts;
    long long lastapts; 

    bool do_ac3_passthru;
};

#endif
