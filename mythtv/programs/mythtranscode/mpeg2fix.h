
#include <pthread.h>
//Qt
#include <qmap.h>
#include <qptrlist.h>
#include <qptrqueue.h>
#include <qvaluelist.h>
#include <qstringlist.h>

extern "C"
{
//AVFormat/AVCodec
#include "avformat.h"
#include "avcodec.h"

//replex
#include "ringbuffer.h"
#include "multiplex.h"

//libmpeg2
#include "mpeg2.h"
}

enum MPFDisplayMask {
    MPF_IMPORTANT = 0x0001,
    MPF_GENERAL   = 0x0002,
    MPF_PROCESS   = 0x0004,
    MPF_FRAME     = 0x0008,
    MPF_RPLXQUEUE = 0x0010,
    MPF_DECODE    = 0x0020,
};

class MPEG2frame
{
  public:
    MPEG2frame(int size)
    {
        pkt.data = (uint8_t *)malloc(size);
        pkt_memsize = size;
    }
    void ensure_size(int size)
    {
        if (pkt_memsize < size)
        {
            pkt.data = (uint8_t *)realloc(pkt.data, size);
            pkt_memsize = size;
        }
    }
    void set_pkt(AVPacket *newpkt)
    {
        ensure_size(newpkt->size);
        uint8_t *data = pkt.data;
        pkt = *newpkt;
        pkt.data = data;
        memcpy(pkt.data, newpkt->data, newpkt->size);
    }

    AVPacket pkt;
    int pkt_memsize;
    bool isSequence;
    bool isGop;
    uint8_t *framePos;
    uint8_t *gopPos;
    mpeg2_sequence_t mpeg2_seq;
    mpeg2_gop_t mpeg2_gop;
    mpeg2_picture_t mpeg2_pic;
};

class MPEG2ptsdelta
{
  public:
    MPEG2ptsdelta() {}
    MPEG2ptsdelta(int64_t a, int64_t b, int64_t c): prev(a), pos(b), delta(c)
    {}

    int64_t prev;
    int64_t pos;
    int64_t delta;
};
typedef QValueList<MPEG2ptsdelta> MPEG2ptsDeltaList;

//container for all multiplex related variables
class MPEG2replex
{
  public:
    void start();
    int wait_buffers();
    int done;
    QString outfile;
    ringbuffer vrbuf;
    ringbuffer arbuf[N_AUDIO];
    ringbuffer ac3rbuf[N_AUDIO];
    ringbuffer index_vrbuf;
    ringbuffer index_arbuf[N_AUDIO];
    ringbuffer index_ac3rbuf[N_AUDIO];
    int ac3_count;
    int mp2_count;

    pthread_mutex_t mutex;
    pthread_cond_t cond;
    audio_frame_t aframe[N_AUDIO];
    audio_frame_t ac3frame[N_AUDIO];
    sequence_t seq_head;

  private:
    multiplex_t *mplex;
};

class MPEG2fixup
{
  public:
    MPEG2fixup(const char *inf, const char *outf,
               QMap<long long, int> *deleteMap, const char *fmt, int norp,
               int fixPTS, int maxf);
    int start();
    void add_cutlist(QStringList cutlist);
    int BuildKeyframeIndex(QString &file, QMap<long long, long long> &posMap);

  protected:
    static void *replex_start(void *data);
    MPEG2replex rx;

  private:
    int find_mpeg2_header(uint8_t *buf, int size, uint8_t code);
    void init_replex();
    QString pts_time(int64_t pts);
    void frame_info(MPEG2frame *f);
    int add_frame(MPEG2frame *f);
    int init_av(const char *inputfile, const char *type, int64_t offset);
    void scan_audio();
    bool process_video(MPEG2frame *vf, mpeg2dec_t *dec);
    void write_frame(const char *filename, AVPacket *pkt);
    void write_yuv(const char *filename, const mpeg2_info_t *info);
    void write_data(const char *filename, uint8_t *data, int size);
    int build_frame(AVPacket *pkt, QString fname);
    MPEG2frame *get_pool_frame(AVPacket *pkt);
    int get_frame(AVPacket *pkt);
    bool find_start();
    void set_repeat(MPEG2frame *vf, int nb_fields, bool topff);
    void set_repeat(uint8_t *ptr, int size, int fields, bool topff);
    MPEG2frame *find_frame_num(int frameNum);
    void renumber_frames(int frame_pos, int delta);
    void store_secondary();
    int  playback_secondary();
    MPEG2frame *decode_to_frame(int frameNum, int skip_reset);
    int convert_to_i(int frameNum, int numFrames, int headPos);
    int insert_frame(int frameNum, int64_t deltaPTS,
                     int64_t ptsIncrement, int64_t initPTS);
    void add_sequence(MPEG2frame *frame1, MPEG2frame *frame2);
    void set_frame_num(uint8_t *ptr, int num);
    int get_frame_num(MPEG2frame *frame)
    {
        return frame->mpeg2_pic.temporal_reference;
    }
    int get_frame_type_n(MPEG2frame *frame)
    {
        return frame->mpeg2_pic.flags & PIC_MASK_CODING_TYPE;
    }
    char get_frame_type_t(MPEG2frame *frame)
    {
        int type = get_frame_type_n(frame);
        return (type == 1 ? 'I' :
                (type == 2 ? 'P' : (type == 3 ? 'B' : 'X')));
    }
    int get_nb_fields(MPEG2frame *frame)
    {
        return frame->mpeg2_pic.nb_fields;
    }
    int get_stream_type(int id)
    {
        return (inputFC->streams[id]->codec->codec_id == CODEC_ID_AC3) ?
               CODEC_ID_AC3 : CODEC_ID_MP2;
    }
    AVCodecContext *getCodecContext(int id)
    {
        return inputFC->streams[id]->codec;
    }

    void dec2x33(int64_t *pts1, int64_t pts2);
    void inc2x33(int64_t *pts1, int64_t pts2);
    int64_t udiff2x33(int64_t pts1, int64_t pts2);
    int64_t diff2x33(int64_t pts1, int64_t pts2);
    int64_t add2x33(int64_t pts1, int64_t pts2);
    int cmp2x33(int64_t pts1, int64_t pts2);
    QPtrList<MPEG2frame> vSecondary;
    bool use_secondary;

    QPtrList<MPEG2frame> vFrame;
    QMap<int, QPtrList<MPEG2frame> > aFrame;
    QPtrQueue<MPEG2frame> framePool;
    QPtrListIterator<MPEG2frame> *displayFrame;
    mpeg2dec_t *header_decoder;
    mpeg2dec_t *img_decoder;

    MPEG2ptsDeltaList *ptsDelta;

    QMap<long long, int> delMap;

    pthread_t thread;

    AVFormatContext *inputFC;
    int vid_id;
    int ac3_count, mp2_count;
    QMap <int, int> aud_map;
    int aud_stream_count;
    int64_t ptsIncrement;
    int64_t ptsOffset;  //was initPTS

    int discard;
    //control options
    int no_repeat, fix_PTS, maxframes;
    QString infile;
    const char *format;
};

#ifdef NO_MYTH
    #include <qdatetime.h>
    #include <iostream>

    using namespace std;

    extern int print_verbose_messages;
    #define VERBOSE(mask,args...) \
    do { \
        if ((print_verbose_messages & mask) != 0) \
        { \
            cout << args << endl; \
        } \
    } while (0)
    #define TRANSCODE_EXIT_OK                         0
    #define TRANSCODE_EXIT_UNKNOWN_ERROR              -1
    #define TRANSCODE_BUGGY_EXIT_WRITE_FRAME_ERROR    -2
    #define TRANSCODE_BUGGY_EXIT_DEADLOCK             -3
#else
   #include "mythcontext.h"
   #include "exitcodes.h"
#endif
