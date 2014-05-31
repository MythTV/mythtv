// POSIX
#include <pthread.h>

// C
#include <cstdlib>

#include "mythconfig.h"

extern "C"
{
//AVFormat/AVCodec
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"

//replex
#include "replex/ringbuffer.h"
#include "replex/multiplex.h"

//libmpeg2
#include "config.h"
#if CONFIG_LIBMPEG2EXTERNAL
#include <mpeg2dec/mpeg2.h>
#else
#include "mpeg2.h"
#endif
}

//Qt
#include <QMap>
#include <QList>
#include <QQueue>
#include <QStringList>
#include <QDateTime>

// MythTV
#include "transcodedefs.h"
#include "programtypes.h"

enum MPFListType {
    MPF_TYPE_CUTLIST = 0,
    MPF_TYPE_SAVELIST,
};

class MPEG2frame
{
  public:
    MPEG2frame(int size) :
        pkt_memsize(size), isSequence(false), isGop(false),
        framePos(NULL), gopPos(NULL)
    {
        pkt.data = (uint8_t *)malloc(size);
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

typedef struct {
    int64_t newPTS;
    int64_t pos_pts;
    int framenum;
    bool type;
} poq_idx_t;

class PTSOffsetQueue
{
  public:
    PTSOffsetQueue(int vidid, QList<int> keys, int64_t initPTS);
    void SetNextPTS(int64_t newPTS, int64_t atPTS);
    void SetNextPos(int64_t newPTS, AVPacket &pkt);
    int64_t Get(int idx, AVPacket *pkt);
    int64_t UpdateOrigPTS(int idx, int64_t &origPTS, AVPacket &pkt);
  private:
    QMap<int, QList<poq_idx_t> > offset;
    QMap<int, QList<poq_idx_t> > orig;
    QList<int> keyList;
    int vid_id;
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
    int otype;
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

typedef QList<MPEG2frame *> FrameList;
typedef QQueue<MPEG2frame *> FrameQueue;
typedef QMap<int, FrameList *> FrameMap;

class MPEG2fixup
{
  public:
    MPEG2fixup(const QString &inf, const QString &outf,
               frm_dir_map_t *deleteMap, const char *fmt, int norp,
               int fixPTS, int maxf, bool showprog, int otype,
               void (*update_func)(float) = NULL, int (*check_func)() = NULL);
    ~MPEG2fixup();
    int Start();
    void AddRangeList(QStringList cutlist, int type);
    void ShowRangeMap(frm_dir_map_t *mapPtr, QString msg);
    int BuildKeyframeIndex(QString &file, frm_pos_map_t &posMap, frm_pos_map_t &durMap);


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
    bool InitAV(QString inputfile, const char *type, int64_t offset);
    void ScanAudio();
    int ProcessVideo(MPEG2frame *vf, mpeg2dec_t *dec);
    void WriteFrame(QString filename, MPEG2frame *f);
    void WriteFrame(QString filename, AVPacket *pkt);
    void WriteYUV(QString filename, const mpeg2_info_t *info);
    void WriteData(QString filename, uint8_t *data, int size);
    bool BuildFrame(AVPacket *pkt, QString fname);
    MPEG2frame *GetPoolFrame(AVPacket *pkt);
    MPEG2frame *GetPoolFrame(MPEG2frame *f);
    int GetFrame(AVPacket *pkt);
    bool FindStart();
    void SetRepeat(MPEG2frame *vf, int nb_fields, bool topff);
    void SetRepeat(uint8_t *ptr, int size, int fields, bool topff);
    MPEG2frame *FindFrameNum(int frameNum);
    void RenumberFrames(int frame_pos, int delta);
    void StoreSecondary();
    int  PlaybackSecondary();
    MPEG2frame *DecodeToFrame(int frameNum, int skip_reset);
    int ConvertToI(FrameList *orderedFrames, int headPos);
    int InsertFrame(int frameNum, int64_t deltaPTS,
                     int64_t ptsIncrement, int64_t initPTS);
    void AddSequence(MPEG2frame *frame1, MPEG2frame *frame2);
    FrameList ReorderDTStoPTS(FrameList *dtsOrder, int pos);
    void InitialPTSFixup(MPEG2frame *curFrame, int64_t &origvPTS,
                         int64_t &PTSdiscrep, int numframes, bool fix);
    void SetFrameNum(uint8_t *ptr, int num);
    static int GetFrameNum(const MPEG2frame *frame)
    {
        return frame->mpeg2_pic.temporal_reference;
    }
    static int GetFrameTypeN(const MPEG2frame *frame)
    {
        return frame->mpeg2_pic.flags & PIC_MASK_CODING_TYPE;
    }
    static char GetFrameTypeT(const MPEG2frame *frame)
    {
        int type = GetFrameTypeN(frame);
        return (type == 1 ? 'I' :
                (type == 2 ? 'P' : (type == 3 ? 'B' : 'X')));
    }
    static int GetNbFields(const MPEG2frame *frame)
    {
        return frame->mpeg2_pic.nb_fields;
    }
    int GetStreamType(int id) const
    {
        return (inputFC->streams[id]->codec->codec_id == AV_CODEC_ID_AC3) ?
               AV_CODEC_ID_AC3 : AV_CODEC_ID_MP2;
    }
    AVCodecContext *getCodecContext(int id)
    {
        return inputFC->streams[id]->codec;
    }
    AVCodecParserContext *getCodecParserContext(int id)
    {
        return inputFC->streams[id]->parser;
    }

    void dumpList(FrameList *list);

    int (*check_abort)();
    void (*update_status)(float percent_done);

    FrameList vSecondary;
    bool use_secondary;

    FrameList vFrame;
    FrameMap  aFrame;
    FrameQueue framePool;
    FrameQueue unreadFrames;
    int displayFrame;
    mpeg2dec_t *header_decoder;
    mpeg2dec_t *img_decoder;

    frm_dir_map_t delMap;
    frm_dir_map_t saveMap;

    pthread_t thread;

    AVFormatContext *inputFC;
    AVFrame *picture;

    int vid_id;
    int ext_count;
    QMap <int, int> aud_map;
    int aud_stream_count;
    int64_t ptsIncrement;
    int64_t ptsOffset;  //was initPTS
    bool mkvfile;

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
    int status_update_time;
    uint64_t last_written_pos;
    uint16_t inv_zigzag_direct16[64];
    bool zigzag_init;
};

#ifdef NO_MYTH
    #include <QDateTime>
    #include <iostream>

    using namespace std;

    extern int verboseMask;
    #undef LOG
    #define LOG(mask,level,args...) \
    do { \
        if ((verboseMask & mask) != 0) \
        { \
            cout << args << endl; \
        } \
    } while (0)

    // Be sure to keep these the same as in libmythbase/exitcodes.h or expect
    // odd behavior eventually
    #define GENERIC_EXIT_OK                     0
    #define GENERIC_EXIT_NOT_OK               128
    #define GENERIC_EXIT_WRITE_FRAME_ERROR    149
    #define GENERIC_EXIT_DEADLOCK             150
#else
   #include "exitcodes.h"
   #include "mythcontext.h"
#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
