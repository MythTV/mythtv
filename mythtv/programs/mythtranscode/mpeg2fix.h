#include <stdlib.h> 
#include <pthread.h>
//Qt
#include <qmap.h>
#include <qptrlist.h>
#include <qptrqueue.h>
#include <qvaluelist.h>
#include <qstringlist.h>
#include <qdatetime.h>

extern "C"
{
//AVFormat/AVCodec
#include "../libavcodec/avcodec.h"
#include "../libavformat/avformat.h"

//replex
#include "replex/ringbuffer.h"
#include "replex/multiplex.h"

//libmpeg2
#include "../libmythmpeg2/mpeg2.h"
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
    ~MPEG2frame()
    {
        free(pkt.data);
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

typedef struct {
    int64_t newPTS;
    int64_t pos_pts;
    int framenum;
    bool type;
} poq_idx_t;

class PTSOffsetQueue
{
  public:
    PTSOffsetQueue(QValueList<int> keys, int64_t initPTS);
    void SetNextPTS(int64_t newPTS, int64_t atPTS);
    void SetNextPos(int64_t newPTS, AVPacket &pkt);
    int64_t Get(int idx, AVPacket *pkt);
    int64_t UpdateOrigPTS(int idx, int64_t &origPTS, AVPacket &pkt);
  private:
    QMap<int, QValueList<poq_idx_t> > offset;
    QMap<int, QValueList<poq_idx_t> > orig;
    QValueList<int> keyList;
};

//container for all multiplex related variables
class MPEG2replex
{
  public:
    MPEG2replex();
    ~MPEG2replex();
    void Start();
    int WaitBuffers();
    int done;
    QString outfile;
    ringbuffer vrbuf;
    ringbuffer extrbuf[N_AUDIO];
    ringbuffer index_vrbuf;
    ringbuffer index_extrbuf[N_AUDIO];
    int ext_count;
    int exttype[N_AUDIO];
    int exttypcnt[N_AUDIO];

    pthread_mutex_t mutex;
    pthread_cond_t cond;
    audio_frame_t extframe[N_AUDIO];
    sequence_t seq_head;

  private:
    multiplex_t *mplex;
};

class MPEG2fixup
{
  public:
    MPEG2fixup(const char *inf, const char *outf,
               QMap<long long, int> *deleteMap, const char *fmt, int norp,
               int fixPTS, int maxf, bool showprog);
    ~MPEG2fixup();
    int Start();
    void AddCutlist(QStringList cutlist);
    int BuildKeyframeIndex(QString &file, QMap<long long, long long> &posMap);


    static void dec2x33(int64_t *pts1, int64_t pts2);
    static void inc2x33(int64_t *pts1, int64_t pts2);
    static int64_t udiff2x33(int64_t pts1, int64_t pts2);
    static int64_t diff2x33(int64_t pts1, int64_t pts2);
    static int64_t add2x33(int64_t pts1, int64_t pts2);
    static int cmp2x33(int64_t pts1, int64_t pts2);

  protected:
    static void *ReplexStart(void *data);
    MPEG2replex rx;

  private:
    int FindMPEG2Header(uint8_t *buf, int size, uint8_t code);
    void InitReplex();
    void FrameInfo(MPEG2frame *f);
    int AddFrame(MPEG2frame *f);
    int InitAV(const char *inputfile, const char *type, int64_t offset);
    void ScanAudio();
    bool ProcessVideo(MPEG2frame *vf, mpeg2dec_t *dec);
    void WriteFrame(const char *filename, AVPacket *pkt);
    void WriteYUV(const char *filename, const mpeg2_info_t *info);
    void WriteData(const char *filename, uint8_t *data, int size);
    int BuildFrame(AVPacket *pkt, QString fname);
    MPEG2frame *GetPoolFrame(AVPacket *pkt);
    int GetFrame(AVPacket *pkt);
    bool FindStart();
    void SetRepeat(MPEG2frame *vf, int nb_fields, bool topff);
    void SetRepeat(uint8_t *ptr, int size, int fields, bool topff);
    MPEG2frame *FindFrameNum(int frameNum);
    void RenumberFrames(int frame_pos, int delta);
    void StoreSecondary();
    int  PlaybackSecondary();
    MPEG2frame *DecodeToFrame(int frameNum, int skip_reset);
    int ConvertToI(int frameNum, int numFrames, int headPos);
    int InsertFrame(int frameNum, int64_t deltaPTS,
                     int64_t ptsIncrement, int64_t initPTS);
    void AddSequence(MPEG2frame *frame1, MPEG2frame *frame2);
    QPtrList<MPEG2frame> ReorderDTStoPTS(QPtrList<MPEG2frame> *dtsOrder);
    void InitialPTSFixup(MPEG2frame *curFrame, int64_t &origvPTS,
                         int64_t &PTSdiscrep, int numframes, bool fix);
    void SetFrameNum(uint8_t *ptr, int num);
    int GetFrameNum(MPEG2frame *frame)
    {
        return frame->mpeg2_pic.temporal_reference;
    }
    int GetFrameTypeN(MPEG2frame *frame)
    {
        return frame->mpeg2_pic.flags & PIC_MASK_CODING_TYPE;
    }
    char GetFrameTypeT(MPEG2frame *frame)
    {
        int type = GetFrameTypeN(frame);
        return (type == 1 ? 'I' :
                (type == 2 ? 'P' : (type == 3 ? 'B' : 'X')));
    }
    int GetNbFields(MPEG2frame *frame)
    {
        return frame->mpeg2_pic.nb_fields;
    }
    int GetStreamType(int id)
    {
        return (inputFC->streams[id]->codec->codec_id == CODEC_ID_AC3) ?
               CODEC_ID_AC3 : CODEC_ID_MP2;
    }
    AVCodecContext *getCodecContext(int id)
    {
        return inputFC->streams[id]->codec;
    }

    QPtrList<MPEG2frame> vSecondary;
    bool use_secondary;

    QPtrList<MPEG2frame> vFrame;
    QMap<int, QPtrList<MPEG2frame> > aFrame;
    QPtrQueue<MPEG2frame> framePool;
    QPtrQueue<MPEG2frame> unreadFrames;
    QPtrListIterator<MPEG2frame> *displayFrame;
    mpeg2dec_t *header_decoder;
    mpeg2dec_t *img_decoder;

    MPEG2ptsDeltaList *ptsDelta;

    QMap<long long, int> delMap;

    pthread_t thread;

    AVFormatContext *inputFC;
    int vid_id;
    int ext_count;
    QMap <int, int> aud_map;
    int aud_stream_count;
    int64_t ptsIncrement;
    int64_t ptsOffset;  //was initPTS

    int discard;
    //control options
    int no_repeat, fix_PTS, maxframes;
    QString infile;
    const char *format;

    //complete?
    bool file_end;
    bool real_file_end;

    //progress indicators
    QDateTime statustime;
    bool showprogress;
    uint64_t filesize;
    int framenum;
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
