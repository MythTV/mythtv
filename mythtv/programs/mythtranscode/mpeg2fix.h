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
#include "external/replex/ringbuffer.h"
#include "external/replex/multiplex.h"

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
#include "mythavutil.h"

enum MPFListType {
    MPF_TYPE_CUTLIST = 0,
    MPF_TYPE_SAVELIST,
};

class MPEG2frame
{
  public:
    explicit MPEG2frame(int size);
    ~MPEG2frame();
    void ensure_size(int size);
    void set_pkt(AVPacket *newpkt);

    AVPacket          m_pkt;
    bool              m_isSequence {false};
    bool              m_isGop      {false};
    uint8_t          *m_framePos   {nullptr};
    uint8_t          *m_gopPos     {nullptr};
    mpeg2_sequence_t  m_mpeg2_seq;
    mpeg2_gop_t       m_mpeg2_gop;
    mpeg2_picture_t   m_mpeg2_pic;
};

struct poq_idx_t {
    int64_t newPTS;
    int64_t pos_pts;
    int framenum;
    bool type;
};

class PTSOffsetQueue
{
  public:
    PTSOffsetQueue(int vidid, QList<int> keys, int64_t initPTS);
    void SetNextPTS(int64_t newPTS, int64_t atPTS);
    void SetNextPos(int64_t newPTS, AVPacket *pkt);
    int64_t Get(int idx, AVPacket *pkt);
    int64_t UpdateOrigPTS(int idx, int64_t &origPTS, AVPacket *pkt);
  private:
    QMap<int, QList<poq_idx_t> > m_offset;
    QMap<int, QList<poq_idx_t> > m_orig;
    QList<int> m_keyList;
    int m_vid_id;
};

//container for all multiplex related variables
class MPEG2replex
{
  public:
    MPEG2replex() = default;
    ~MPEG2replex();
    void Start();
    int WaitBuffers();
    int             m_done                    {0};
    QString         m_outfile;
    int             m_otype                   {0};
    ringbuffer      m_vrbuf                   {};
    ringbuffer      m_extrbuf[N_AUDIO]        {};
    ringbuffer      m_index_vrbuf             {};
    ringbuffer      m_index_extrbuf[N_AUDIO]  {};
    int             m_ext_count               {0};
    int             m_exttype[N_AUDIO]        {0};
    int             m_exttypcnt[N_AUDIO]      {0};

    pthread_mutex_t m_mutex                   {};
    pthread_cond_t  m_cond                    {};
    audio_frame_t   m_extframe[N_AUDIO]       {};
    sequence_t      m_seq_head                {};

  private:
    multiplex_t    *m_mplex                   {nullptr};
};

using FrameList  = QList<MPEG2frame *>;
using FrameQueue = QQueue<MPEG2frame *>;
using FrameMap   = QMap<int, FrameList *>;

class MPEG2fixup
{
  public:
    MPEG2fixup(const QString &inf, const QString &outf,
               frm_dir_map_t *deleteMap, const char *fmt, int norp,
               int fixPTS, int maxf, bool showprog, int otype,
               void (*update_func)(float) = nullptr, int (*check_func)() = nullptr);
    ~MPEG2fixup();
    int Start();
    void AddRangeList(QStringList rangelist, int type);
    static void ShowRangeMap(frm_dir_map_t *mapPtr, QString msg);
    int BuildKeyframeIndex(QString &file, frm_pos_map_t &posMap, frm_pos_map_t &durMap);

    void SetAllAudio(bool keep) { m_allaudio = keep; }

    static void dec2x33(int64_t *pts1, int64_t pts2);
    static void inc2x33(int64_t *pts1, int64_t pts2);
    static int64_t udiff2x33(int64_t pts1, int64_t pts2);
    static int64_t diff2x33(int64_t pts1, int64_t pts2);
    static int64_t add2x33(int64_t pts1, int64_t pts2);
    static int cmp2x33(int64_t pts1, int64_t pts2);

  protected:
    static void *ReplexStart(void *data);
    MPEG2replex m_rx;

  private:
    static int FindMPEG2Header(const uint8_t *buf, int size, uint8_t code);
    void InitReplex();
    void FrameInfo(MPEG2frame *f);
    int AddFrame(MPEG2frame *f);
    bool InitAV(const QString& inputfile, const char *type, int64_t offset);
    void ScanAudio();
    int ProcessVideo(MPEG2frame *vf, mpeg2dec_t *dec);
    void WriteFrame(const QString& filename, MPEG2frame *f);
    void WriteFrame(const QString& filename, AVPacket *pkt);
    static void WriteYUV(const QString& filename, const mpeg2_info_t *info);
    static void WriteData(const QString& filename, uint8_t *data, int size);
    bool BuildFrame(AVPacket *pkt, const QString& fname);
    MPEG2frame *GetPoolFrame(AVPacket *pkt);
    MPEG2frame *GetPoolFrame(MPEG2frame *f);
    int GetFrame(AVPacket *pkt);
    bool FindStart();
    static void SetRepeat(MPEG2frame *vf, int nb_fields, bool topff);
    static void SetRepeat(uint8_t *ptr, int size, int fields, bool topff);
    MPEG2frame *FindFrameNum(int frameNum);
    void RenumberFrames(int start_pos, int delta);
    void StoreSecondary();
    int  PlaybackSecondary();
    MPEG2frame *DecodeToFrame(int frameNum, int skip_reset);
    int ConvertToI(FrameList *orderedFrames, int headPos);
    int InsertFrame(int frameNum, int64_t deltaPTS,
                     int64_t ptsIncrement, int64_t initPTS);
    void AddSequence(MPEG2frame *frame1, MPEG2frame *frame2);
    static FrameList ReorderDTStoPTS(FrameList *dtsOrder, int pos);
    void InitialPTSFixup(MPEG2frame *curFrame, int64_t &origvPTS,
                         int64_t &PTSdiscrep, int numframes, bool fix);
    static void SetFrameNum(uint8_t *ptr, int num);
    static int GetFrameNum(const MPEG2frame *frame)
    {
        return frame->m_mpeg2_pic.temporal_reference;
    }
    static int GetFrameTypeN(const MPEG2frame *frame)
    {
        return frame->m_mpeg2_pic.flags & PIC_MASK_CODING_TYPE;
    }
    static char GetFrameTypeT(const MPEG2frame *frame)
    {
        int type = GetFrameTypeN(frame);
        return (type == 1 ? 'I' :
                (type == 2 ? 'P' : (type == 3 ? 'B' : 'X')));
    }
    static int GetNbFields(const MPEG2frame *frame)
    {
        return frame->m_mpeg2_pic.nb_fields;
    }
    int GetStreamType(int id) const
    {
        return (m_inputFC->streams[id]->codecpar->codec_id == AV_CODEC_ID_AC3) ?
               AV_CODEC_ID_AC3 : AV_CODEC_ID_MP2;
    }
    AVCodecContext *getCodecContext(uint id)
    {
        if (id >= m_inputFC->nb_streams)
            return nullptr;
        return gCodecMap->getCodecContext(m_inputFC->streams[id]);
    }
    AVCodecParserContext *getCodecParserContext(uint id)
    {
        if (id >= m_inputFC->nb_streams)
            return nullptr;
        return m_inputFC->streams[id]->parser;
    }

    static void dumpList(FrameList *list);

    int (*check_abort)()                      {nullptr};
    void (*update_status)(float percent_done) {nullptr};

    FrameList     m_vSecondary;
    bool          m_use_secondary  {false};

    FrameList     m_vFrame;
    FrameMap      m_aFrame;
    FrameQueue    m_framePool;
    FrameQueue    m_unreadFrames;
    int           m_displayFrame    {0};
    mpeg2dec_t   *m_header_decoder  {nullptr};
    mpeg2dec_t   *m_img_decoder     {nullptr};

    frm_dir_map_t m_delMap;
    frm_dir_map_t m_saveMap;

    pthread_t     m_thread          {};

    AVFormatContext *m_inputFC      {nullptr};
    AVFrame         *m_picture      {nullptr};

    int             m_vid_id        {-1};
    int             m_ext_count     {0};
    QMap <int, int> m_aud_map;
    int64_t         m_ptsIncrement  {0};
    bool            m_mkvfile       {false};

    bool            m_discard       {false};
    //control options
    int             m_no_repeat, m_fix_PTS, m_maxframes;
    QString         m_infile;
    const char     *m_format        {nullptr};
    bool            m_allaudio      {false};

    //complete?
    bool            m_file_end      {false};
    bool            m_real_file_end {false};

    //progress indicators
    QDateTime       m_statustime;
    bool            m_showprogress  {false};
    uint64_t        m_filesize      {0};
    int             m_framenum      {0};
    int             m_status_update_time      {5};
    uint64_t        m_last_written_pos        {0};
    uint16_t        m_inv_zigzag_direct16[64] {};
    bool            m_zigzag_init   {false};
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
