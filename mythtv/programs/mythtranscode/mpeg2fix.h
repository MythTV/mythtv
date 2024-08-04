// POSIX
#include <pthread.h>

// C++
#include <cstdlib>

#include "libmythbase/mythconfig.h"

extern "C"
{
//AVFormat/AVCodec
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"

//libmpeg2
#if CONFIG_LIBMPEG2EXTERNAL
#include <mpeg2dec/mpeg2.h>
#else
#include "libmythmpeg2/mpeg2.h"
#endif
}

//replex
#include "external/replex/multiplex.h"
#include "external/replex/ringbuffer.h"

// Qt
#include <QMap>
#include <QList>
#include <QQueue>
#include <QStringList>
#include <QDateTime>

// MythTV
#include "libmythbase/programtypes.h"
#include "libmythtv/mythavutil.h"

// MythTranscode
#include "transcodedefs.h"

enum MPFListType : std::uint8_t {
    MPF_TYPE_CUTLIST = 0,
    MPF_TYPE_SAVELIST,
};

class MPEG2frame
{
  public:
    explicit MPEG2frame(int size);
    ~MPEG2frame();
    void ensure_size(int size) const;
    void set_pkt(AVPacket *newpkt) const;

    AVPacket         *m_pkt        {nullptr};
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
    int m_vidId;
};

//container for all multiplex related variables
using RingbufferArray = std::array<ringbuffer,N_AUDIO>;
using ExtTypeIntArray = std::array<int,N_AUDIO>;
using AudioFrameArray = std::array<audio_frame_t,N_AUDIO>;

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
    ringbuffer      m_vrBuf                   {};
    RingbufferArray m_extrbuf                 {};
    ringbuffer      m_indexVrbuf              {};
    RingbufferArray m_indexExtrbuf            {};
    int             m_extCount                {0};
    ExtTypeIntArray m_exttype                 {0};
    ExtTypeIntArray m_exttypcnt               {0};

    pthread_mutex_t m_mutex                   {};
    pthread_cond_t  m_cond                    {};
    AudioFrameArray m_extframe                {};
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
               frm_dir_map_t *deleteMap, const char *fmt, bool norp,
               bool fixPTS, int maxf, bool showprog, int otype,
               void (*update_func)(float) = nullptr, int (*check_func)() = nullptr);
    ~MPEG2fixup();
    int Start();
    void AddRangeList(const QStringList& rangelist, int type);
    static void ShowRangeMap(frm_dir_map_t *mapPtr, QString msg);
    int BuildKeyframeIndex(const QString &file, frm_pos_map_t &posMap, frm_pos_map_t &durMap);

    void SetAllAudio(bool keep) { m_allAudio = keep; }

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
                         int64_t &PTSdiscrep, int numframes, bool fix) const;
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
        if (type == 1)
            return 'I';
        if (type == 2)
            return 'P';
        if (type == 3)
            return 'B';
        return 'X';
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
        return m_codecMap.GetCodecContext(m_inputFC->streams[id]);
    }
    AVCodecParserContext *getCodecParserContext(uint id)
    {
        if (id >= m_inputFC->nb_streams)
            return nullptr;
        return av_stream_get_parser(m_inputFC->streams[id]);
    }

    static void dumpList(FrameList *list);

    int (*m_checkAbort)()                      {nullptr};
    void (*m_updateStatus)(float percent_done) {nullptr};

    FrameList     m_vSecondary;
    bool          m_useSecondary  {false};

    FrameList     m_vFrame;
    FrameMap      m_aFrame;
    FrameQueue    m_framePool;
    FrameQueue    m_unreadFrames;
    int           m_displayFrame    {0};
    mpeg2dec_t   *m_headerDecoder   {nullptr};
    mpeg2dec_t   *m_imgDecoder      {nullptr};

    frm_dir_map_t m_delMap;
    frm_dir_map_t m_saveMap;

    pthread_t     m_thread          {};

    MythCodecMap     m_codecMap;
    AVFormatContext *m_inputFC      {nullptr};
    AVFrame         *m_picture      {nullptr};

    int             m_vidId         {-1};
    int             m_extCount      {0};
    QMap <int, int> m_audMap;
    int64_t         m_ptsIncrement  {0};
    bool            m_mkvFile       {false};

    bool            m_discard       {false};
    //control options
    bool            m_noRepeat;
    bool            m_fixPts;
    int             m_maxFrames;
    QString         m_infile;
    const char     *m_format        {nullptr};
    bool            m_allAudio      {false};

    //complete?
    bool            m_fileEnd       {false};
    bool            m_realFileEnd   {false};

    //progress indicators
    QDateTime       m_statusTime;
    bool            m_showProgress          {false};
    uint64_t        m_fileSize              {0};
    int             m_frameNum              {0};
    int             m_statusUpdateTime      {5};
    uint64_t        m_lastWrittenPos        {0};
};

#ifdef NO_MYTH
    #include <QDateTime>
    #include <iostream>

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
   #include "libmythbase/exitcodes.h"
   #include "libmyth/mythcontext.h"
#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
