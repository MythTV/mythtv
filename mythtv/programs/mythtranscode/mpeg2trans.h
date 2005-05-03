#include <qmap.h>
#include <qptrqueue.h>
#include <qptrstack.h>
#include "programinfo.h"
#include "exitcodes.h"
#include "mythcontext.h"

extern "C" {
#include "avformat.h"
}

typedef struct {
  uint8_t drop;
  uint8_t hour;
  uint8_t min;
  uint8_t sec;
  uint8_t pic;
} gop_timestamp_t;

class FrameBuffer
{
  public:
    FrameBuffer(void) { size = 0; pkt.size = 0; pkt.data = NULL; delta = 0;}
    ~FrameBuffer(void) { if (pkt.data) free (pkt.data); }
    void setPkt(AVPacket *newpkt, int64_t del = 0);
    void setPkt(uint8_t *data, uint32_t newsize, int64_t newpts, int64_t del=0);
    AVPacket *getPkt(void) { return &pkt; }
    uint8_t *getData(void) { return pkt.data; }
    uint32_t getSize(void) { return pkt.size; }
    int64_t  getPTS(void)  { return pkt.pts; }
    int64_t  getDelta(void)  { return delta; }
    void Reset(void) { pkt.size = 0; }
  private:
    AVPacket pkt;
    int32_t size;
    int64_t delta;
};

class MPEG2trans
{
  public:
    MPEG2trans(QMap<long long, int> *map);
    ~MPEG2trans(void);
    int DoTranscode(QString inputFilename, QString outputFilename);
    int BuildKeyframeIndex(QString filename, QMap <long long, long long> &posMap);
  private:
    uint32_t parse_seq_header(uint8_t *PES_data, bool store);
    uint32_t get_gop_data(uint8_t *PES_data, gop_timestamp_t *gopts, 
                          bool *closed, bool *store);
    void set_gop_data(uint8_t *PES_data, gop_timestamp_t *gopts,
                      bool closed, bool store);
    void update_timestamp(gop_timestamp_t *fixed_gopts, gop_timestamp_t *ts);
    void update_pic_frame_num(uint8_t *PES_data, int num);
    uint32_t get_ext_frame(uint8_t *PES_data);

    void init_audioout_stream(void);
    void init_videoout_stream(void);

    void write_muxed_frame(AVStream *stream, AVPacket *pkt, int advance, int64_t delta = 0);

    bool check_ac3_header(uint8_t *buf);
    bool check_mp2_header(uint8_t *buf);
    bool check_video_header(uint8_t *buf);

    uint32_t process_ac3_audio(AVPacket *pkt);
    uint32_t process_mp2_audio(AVPacket *pkt);
    void process_audio(AVPacket *pkt);
    bool process_video(AVPacket *pkt, bool gopsearch = false);


    int video_width;
    int video_height;
    int video_aspect;
    uint32_t video_frame_rate;
    uint32_t video_bitrate;
    int pic_frame_count;

    bool use_ac3;
    uint32_t audio_freq;
    uint32_t audio_bitrate;

    int64_t goppts_delta;

    gop_timestamp_t *fixed_gop_ts;

    bool chopping;
    bool chop_end;
    int skipped_frames;

    bool got_audio;
    bool got_video;
    bool init_av;

    AVFormatContext *outputContext;
    AVFormatContext *inputContext;

    AVStream *audioout_st;
    AVStream *videoout_st;

    int audioin_index;
    int videoin_index;

    uint32_t last_frame_number;
    int64_t last_gop_pts;
    int64_t chop_start_pts;
    uint8_t video_frame_state;

    FrameBuffer audioFrame;
    FrameBuffer videoFrame;
    FrameBuffer seqFrame;

    QMap<long long, long long > cutlistMap;
    QMap<long long, long long>::Iterator cutlistIter;

    QPtrQueue<FrameBuffer> audioQueue; 
    QPtrQueue<FrameBuffer> videoQueue; 
    QPtrStack<FrameBuffer> audioPool; 
    QPtrStack<FrameBuffer> videoPool; 

    QPtrQueue<AVPacket> audskipQueue;
};
