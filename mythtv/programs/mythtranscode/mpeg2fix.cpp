//To Do
//support missing audio frames
//support analyze-only mode

// C++ headers
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <getopt.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

// Qt headers
#include <QFileInfo>
#include <QList>
#include <QMap>
#include <QQueue>

// MythTV headers
#include "libmythbase/mythconfig.h"

#include "libmythbase/mthread.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythlogging.h"

extern "C" {
#include "libavutil/cpu.h"
#include "libmythmpeg2/attributes.h"     // for ATTR_ALIGN() in mpeg2_internal.h
#include "libmythmpeg2/mpeg2.h"          // for mpeg2_decoder_t, mpeg2_fbuf_t, et c
#include "libmythmpeg2/mpeg2_internal.h"
}

// MythTranscode
#include "mpeg2fix.h"

#ifdef _WIN32
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif

#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif

static void my_av_print([[maybe_unused]] void *ptr,
                        int level, const char* fmt, va_list vl)
{
    static QString s_fullLine("");

    if (level > AV_LOG_INFO)
        return;

    s_fullLine += QString::vasprintf(fmt, vl);
    if (s_fullLine.endsWith("\n"))
    {
        s_fullLine.chop(1);
        LOG(VB_GENERAL, LOG_INFO, s_fullLine);
        s_fullLine = QString("");
    }
}

static QString PtsTime(int64_t pts)
{
    bool is_neg = false;
    if (pts < 0)
    {
        pts = -pts;
        is_neg = true;
    }
    return QString("%1%2:%3:%4.%5")
        .arg(is_neg ? "-" : "")
        .arg((uint)(pts / 90000.) / 3600, 2, 10, QChar('0'))
        .arg(((uint)(pts / 90000.) % 3600) / 60, 2, 10, QChar('0'))
        .arg(((uint)(pts / 90000.) % 3600) % 60, 2, 10, QChar('0'))
        .arg(((((uint)(pts / 90.) % 3600000) % 60000) % 1000), 3, 10, QChar('0'));
}

MPEG2frame::MPEG2frame(int size) :
    m_pkt(av_packet_alloc()),
    m_mpeg2_seq(), m_mpeg2_gop(), m_mpeg2_pic()
{
    av_new_packet(m_pkt, size);
}

MPEG2frame::~MPEG2frame()
{
    av_packet_free(&m_pkt);
}

void MPEG2frame::ensure_size(int size) const
{
    if (m_pkt->size < size)
    {
        int oldSize = m_pkt->size;
        if ((av_grow_packet(m_pkt, size - m_pkt->size) < 0) || m_pkt->size < size)
        {
            LOG(VB_GENERAL, LOG_CRIT, QString("MPEG2frame::ensure_size(): "
                                              "Failed to grow packet size "
                                              "from %1 to %2, result was %3")
                                                .arg(oldSize).arg(size)
                                                .arg(m_pkt->size));
        }
    }
}

void MPEG2frame::set_pkt(AVPacket *newpkt) const
{
    // TODO: Don't free + copy, attempt to re-use existing buffer
    av_packet_unref(m_pkt);
    av_packet_ref(m_pkt, newpkt);
}

PTSOffsetQueue::PTSOffsetQueue(int vidid, QList<int> keys, int64_t initPTS)
  : m_keyList(std::move(keys)),
    m_vidId(vidid)
{
    poq_idx_t idx {};
    m_keyList.append(m_vidId);

    idx.newPTS = initPTS;
    idx.pos_pts = 0;
    idx.framenum = 0;
    idx.type = false;

    for (const int key : std::as_const(m_keyList))
        m_offset[key].push_back(idx);
}

int64_t PTSOffsetQueue::Get(int idx, AVPacket *pkt)
{
    QList<poq_idx_t>::iterator it;
    int64_t value = m_offset[idx].first().newPTS;
    bool done = false;

    if (!pkt)
        return value;

    //Be aware: the key for offset can be either a file position OR a PTS
    //The type is defined by type (0==PTS, 1==Pos)
    while (m_offset[idx].count() > 1 && !done)
    {
        it = ++m_offset[idx].begin();
        if (((static_cast<int>((*it).type) == 0) &&
             (pkt->pts >= (*it).pos_pts) /* PTS type */) ||
            (((*it).type) /* Pos type */ &&
             ((pkt->pos >= (*it).pos_pts) || (pkt->duration > (*it).framenum))))
        {
            m_offset[idx].pop_front();
            value = m_offset[idx].first().newPTS;
        }
        else
        {
            done = true;
        }
    }
    return value;
}

void PTSOffsetQueue::SetNextPTS(int64_t newPTS, int64_t atPTS)
{
    poq_idx_t idx {};

    idx.newPTS = newPTS;
    idx.pos_pts = atPTS;
    idx.type = false;
    idx.framenum = 0;

    for (const int key : std::as_const(m_keyList))
        m_offset[key].push_back(idx);
}

void PTSOffsetQueue::SetNextPos(int64_t newPTS, AVPacket *pkt)
{
    int64_t delta = MPEG2fixup::diff2x33(newPTS, m_offset[m_vidId].last().newPTS);
    poq_idx_t idx {};

    idx.pos_pts = pkt->pos;
    idx.framenum = pkt->duration;
    idx.type = true;

    LOG(VB_FRAME, LOG_INFO, QString("Offset %1 -> %2 (%3) at %4")
            .arg(PtsTime(m_offset[m_vidId].last().newPTS),
                 PtsTime(newPTS),
                 PtsTime(delta), QString::number(pkt->pos)));
    for (const int key : std::as_const(m_keyList))
    {
        idx.newPTS = newPTS;
        m_offset[key].push_back(idx);
        idx.newPTS = delta;
        m_orig[key].push_back(idx);
    }
}

int64_t PTSOffsetQueue::UpdateOrigPTS(int idx, int64_t &origPTS, AVPacket *pkt)
{
    int64_t delta = 0;
    QList<poq_idx_t> *dltaList = &m_orig[idx];
    while (!dltaList->isEmpty() &&
           (pkt->pos     >= dltaList->first().pos_pts ||
            pkt->duration > dltaList->first().framenum))
    {
        if (dltaList->first().newPTS >= 0)
            ptsinc((uint64_t *)&origPTS, 300 * dltaList->first().newPTS);
        else
            ptsdec((uint64_t *)&origPTS, -300 * dltaList->first().newPTS);
        delta += dltaList->first().newPTS;
        dltaList->pop_front();
        LOG(VB_PROCESS, LOG_INFO,
            QString("Moving PTS offset of stream %1 by %2")
                .arg(idx).arg(PtsTime(delta)));
    }
    return (delta);
}

MPEG2fixup::MPEG2fixup(const QString &inf, const QString &outf,
                       frm_dir_map_t *deleteMap,
                       const char *fmt, bool norp, bool fixPTS, int maxf,
                       bool showprog, int otype, void (*update_func)(float),
                       int (*check_func)())
    : m_noRepeat(norp), m_fixPts(fixPTS), m_maxFrames(maxf),
      m_infile(inf), m_format(fmt)
{
    m_rx.m_outfile = outf;
    m_rx.m_done = 0;
    m_rx.m_otype = otype;
    if (deleteMap && !deleteMap->isEmpty())
    {
        /* convert MythTV cutlist to mpeg2fix cutlist */
        frm_dir_map_t::iterator it = deleteMap->begin();
        for (; it != deleteMap->end(); ++it)
        {
            uint64_t mark = it.key();
            if (mark > 0)
            {
                if (it.value() == MARK_CUT_START) // NOLINT(bugprone-branch-clone)
                    mark += 1; // +2 looks good, but keyframes are hit with +1
                else
                    mark += 1;
            }
            m_delMap.insert (mark, it.value());
        }

        if (m_delMap.contains(0))
        {
            m_discard = true;
            m_delMap.remove(0);
        }
        if (m_delMap.begin().value() == MARK_CUT_END)
            m_discard = true;
        m_useSecondary = true;
    }

    m_headerDecoder = mpeg2_init();
    m_imgDecoder = mpeg2_init();

    av_log_set_callback(my_av_print);

    pthread_mutex_init(&m_rx.m_mutex, nullptr);
    pthread_cond_init(&m_rx.m_cond, nullptr);

    //await multiplexer initialization (prevent a deadlock race)
    pthread_mutex_lock(&m_rx.m_mutex);
    pthread_create(&m_thread, nullptr, ReplexStart, this);
    pthread_cond_wait(&m_rx.m_cond, &m_rx.m_mutex);
    pthread_mutex_unlock(&m_rx.m_mutex);

    //initialize progress stats
    m_showProgress = showprog;
    m_updateStatus = update_func;
    m_checkAbort = check_func;
    if (m_showProgress || m_updateStatus)
    {
        if (m_updateStatus)
        {
            m_statusUpdateTime = 20;
            m_updateStatus(0);
        }
        else
        {
            m_statusUpdateTime = 5;
        }
        m_statusTime = MythDate::current();
        m_statusTime = m_statusTime.addSecs(m_statusUpdateTime);

        const QFileInfo finfo(inf);
        m_fileSize = finfo.size();
    }
}

MPEG2fixup::~MPEG2fixup()
{
    mpeg2_close(m_headerDecoder);
    mpeg2_close(m_imgDecoder);

    if (m_inputFC)
        avformat_close_input(&m_inputFC);
    av_frame_free(&m_picture);

    while (!m_vFrame.isEmpty())
    {
        MPEG2frame *tmpFrame = m_vFrame.takeFirst();
        delete tmpFrame;
    }

    while (!m_vSecondary.isEmpty())
    {
        MPEG2frame *tmpFrame = m_vSecondary.takeFirst();
        delete tmpFrame;
    }

    for (auto *af : std::as_const(m_aFrame))
    {
        while (!af->isEmpty())
        {
            MPEG2frame *tmpFrame = af->takeFirst();
            delete tmpFrame;
        }
        delete af;
    }

    while (!m_framePool.isEmpty())
        delete m_framePool.dequeue();
}

//#define MPEG2trans_DEBUG
static constexpr bool MATCH_HEADER(const uint8_t *ptr)
    { return (ptr[0] == 0x00) && (ptr[1] == 0x00) && (ptr[2] == 0x01); };

static void SETBITS(unsigned char *ptr, long value, int num)
{
    static int s_sbPos = 0;
    static unsigned char *s_sbPtr = nullptr;

    if (ptr != nullptr)
    {
        s_sbPtr = ptr;
        s_sbPos = 0;
    }

    if (s_sbPtr == nullptr)
        return;

    int offset = s_sbPos >> 3;
    int offset_r = s_sbPos & 0x07;
    int offset_b = 32 - offset_r;
    uint32_t mask = ~(((1 << num) - 1) << (offset_b - num));
    uint32_t sb_long = ntohl(*((uint32_t *) (s_sbPtr + offset)));
    value = value << (offset_b - num);
    sb_long = (sb_long & mask) + value;
    *((uint32_t *)(s_sbPtr + offset)) = htonl(sb_long);
}

void MPEG2fixup::dec2x33(int64_t *pts1, int64_t pts2)
{
    *pts1 = udiff2x33(*pts1, pts2);
}

void MPEG2fixup::inc2x33(int64_t *pts1, int64_t pts2)
{
    *pts1 = (*pts1 + pts2) % MAX_PTS;
}

int64_t MPEG2fixup::udiff2x33(int64_t pts1, int64_t pts2)
{
    int64_t diff = pts1 - pts2;

    if (diff < 0)
    {
        diff = MAX_PTS + diff;
    }
    return (diff % MAX_PTS);
}

int64_t MPEG2fixup::diff2x33(int64_t pts1, int64_t pts2)
{
    switch (cmp2x33(pts1, pts2))
    {
        case 0:
            return 0;
            break;

        case 1:
        case -2:
            return (pts1 - pts2);
            break;

        case 2:
            return (pts1 + MAX_PTS - pts2);
            break;

        case -1:
            return (pts1 - (pts2 + MAX_PTS));
            break;
    }

    return 0;
}

int64_t MPEG2fixup::add2x33(int64_t pts1, int64_t pts2)
{
    int64_t tmp = pts1 + pts2;
    if (tmp >= 0)
        return (pts1 + pts2) % MAX_PTS;
    return (tmp + MAX_PTS);
}

int MPEG2fixup::cmp2x33(int64_t pts1, int64_t pts2)
{
    int ret = 0;

    if (pts1 > pts2)
    {
        if ((uint64_t)(pts1 - pts2) > MAX_PTS/2ULL)
            ret = -1;
        else 
            ret = 1;
    }
    else if (pts1 == pts2)
    {
        ret = 0; 
    }
    else
    {
        if ((uint64_t)(pts2 - pts1) > MAX_PTS/2ULL)
            ret = 2;
        else  
            ret = -2;
    }
    return ret;
}

int MPEG2fixup::FindMPEG2Header(const uint8_t *buf, int size, uint8_t code)
{
    for (int i = 0; i < size; i++)
    {
        if (MATCH_HEADER(buf + i) && buf[i + 3] == code)
            return i;
    }

    return 0;
}

//fill_buffers will signal the main thread to start decoding again as soon
//as it runs out of buffers.  It will then wait for the buffer to completely
//fill before returning.  In this way, the 2 threads are never running
// concurrently
static int fill_buffers(void *r, int finish)
{
    auto *rx = (MPEG2replex *)r;

    if (finish)
        return 0;

    return (rx->WaitBuffers());
}

MPEG2replex::~MPEG2replex()
{
    if (m_vrBuf.size)
        ring_destroy(&m_vrBuf);
    if (m_indexVrbuf.size)
        ring_destroy(&m_indexVrbuf);
    
    for (int i = 0; i < m_extCount; i++)
    {
        if (m_extrbuf[i].size)
            ring_destroy(&m_extrbuf[i]);
        if (m_indexExtrbuf[i].size)
            ring_destroy(&m_indexExtrbuf[i]);
    }
}

int MPEG2replex::WaitBuffers()
{
    pthread_mutex_lock( &m_mutex );
    while (true)
    {
        int ok = 1;

        if (ring_avail(&m_indexVrbuf) < sizeof(index_unit))
            ok = 0;

        for (int i = 0; i < m_extCount; i++)
            if (ring_avail(&m_indexExtrbuf[i]) < sizeof(index_unit))
                ok = 0;

        if (ok || m_done)
            break;

        pthread_cond_signal(&m_cond);
        pthread_cond_wait(&m_cond, &m_mutex);
    }
    pthread_mutex_unlock(&m_mutex);

    if (m_done)
    {
        finish_mpg(m_mplex);
	// mythtv#244: thread exit must return static, not stack
	static int errorcount = 0;
	errorcount = m_mplex->error;
	if (m_mplex->error) {
	  LOG(VB_GENERAL, LOG_ERR,
	      QString("thread finished with %1 write errors")
	      .arg(m_mplex->error));
	}
	pthread_exit(&errorcount);
    }

    return 0;
}

void *MPEG2fixup::ReplexStart(void *data)
{
    MThread::ThreadSetup("MPEG2Replex");
    auto *m2f = static_cast<MPEG2fixup *>(data);
    if (!m2f)
        return nullptr;
    m2f->m_rx.Start();
    MThread::ThreadCleanup();
    return nullptr;
}

void MPEG2replex::Start()
{
    int start = 1;
    multiplex_t mx {};

    //array defines number of allowed audio streams
    // note that although only 1 stream is currently supported, multiplex.c
    // expects the size to by N_AUDIO
    aok_arr ext_ok {};
    int video_ok = 0;

    //seq_head should be set only for the 1st sequence header.  If a new
    // seq header comes which is different, we are screwed.


    int video_delay = 0;
    int audio_delay = 0;

    mx.priv = (void *)this;

    int fd_out = open(m_outfile.toLocal8Bit().constData(),
                      O_WRONLY | O_CREAT | O_TRUNC | O_LARGEFILE, 0644);

    //await buffer fill
    pthread_mutex_lock(&m_mutex);
    pthread_cond_signal(&m_cond);
    pthread_cond_wait(&m_cond, &m_mutex);
    pthread_mutex_unlock(&m_mutex);

    m_mplex = &mx;

    init_multiplex(&mx, &m_seq_head, m_extframe.data(), m_exttype.data(), m_exttypcnt.data(),
                   video_delay, audio_delay, fd_out, fill_buffers,
                   &m_vrBuf, &m_indexVrbuf, m_extrbuf.data(), m_indexExtrbuf.data(), m_otype);
    setup_multiplex(&mx);

    while (true)
    {
        check_times( &mx, &video_ok, ext_ok, &start);
        if (write_out_packs( &mx, video_ok, ext_ok)) {
	  // mythtv#244: exiting here blocks the reading thread indefinitely;
	  // maybe there is a way to fail it also?
	  // LOG(VB_GENERAL, LOG_ERR, // or comment all this to fail until close
	  //     QString("exiting thread after %1 write errors")
	  //     .arg(m_mplex->error));
	  // pthread_exit(&m_mplex->error);
	}
    }
}

#define INDEX_BUF (sizeof(index_unit) * 200)
void MPEG2fixup::InitReplex()
{
    // index_vrbuf contains index_units which describe a video frame
    //   it also contains the start pos of the next frame
    // index_arbuf only uses, pts, framesize, length, start, (active, err)

    if (m_vFrame.first()->m_mpeg2_seq.height >= 720)
    {
        LOG(VB_GENERAL, LOG_NOTICE, "MPEG2fixup::InitReplex(): High Definition input, increasing replex buffers");
        if (m_rx.m_otype == REPLEX_MPEG2)
            m_rx.m_otype = REPLEX_HDTV;
        else if (m_rx.m_otype == REPLEX_TS_SD)
            m_rx.m_otype = REPLEX_TS_HD;
        else
        {
            LOG(VB_GENERAL, LOG_WARNING, "MPEG2fixup::InitReplex(): Using '--ostream=dvd' with HD video is an invalid combination");
        }
    }

    //this should support > 100 frames
    uint32_t memsize = m_vFrame.first()->m_mpeg2_seq.width *
                       m_vFrame.first()->m_mpeg2_seq.height * 10;
    ring_init(&m_rx.m_vrBuf, memsize);
    ring_init(&m_rx.m_indexVrbuf, INDEX_BUF);

    m_rx.m_exttype.fill(0);
    m_rx.m_exttypcnt.fill(0);
    int mp2_count = 0;
    int ac3_count = 0;
    for (auto it = m_aFrame.begin(); it != m_aFrame.end(); it++)
    {
        if (it.key() < 0)
            continue;   // will never happen in practice
        uint index = it.key();
        if (index > m_inputFC->nb_streams)
            continue;   // will never happen in practice
        AVCodecContext  *avctx = getCodecContext(index);
        if (avctx == nullptr)
            continue;
        int i = m_audMap[index];
        AVDictionaryEntry *metatag =
            av_dict_get(m_inputFC->streams[index]->metadata,
                        "language", nullptr, 0);
        char *lang = metatag ? metatag->value : (char *)"";
        ring_init(&m_rx.m_extrbuf[i], memsize / 5);
        ring_init(&m_rx.m_indexExtrbuf[i], INDEX_BUF);
        m_rx.m_extframe[i].set = 1;
        m_rx.m_extframe[i].bit_rate = avctx->bit_rate;
        m_rx.m_extframe[i].framesize = (*it)->first()->m_pkt->size;
        strncpy(m_rx.m_extframe[i].language, lang, 4);
        switch(GetStreamType(index))
        {
            case AV_CODEC_ID_MP2:
            case AV_CODEC_ID_MP3:
                m_rx.m_exttype[i] = 2;
                m_rx.m_exttypcnt[i] = mp2_count++;
                break;
            case AV_CODEC_ID_AC3:
                m_rx.m_exttype[i] = 1;
                m_rx.m_exttypcnt[i] = ac3_count++;
                break;
        }
    }

    //bit_rate/400
    m_rx.m_seq_head.bit_rate = m_vFrame.first()->m_mpeg2_seq.byte_rate / 50;
    m_rx.m_seq_head.frame_rate = (m_vFrame.first()->m_mpeg2_seq.frame_period +
                         26999999ULL) / m_vFrame.first()->m_mpeg2_seq.frame_period;

    m_rx.m_extCount = m_extCount;
}

void MPEG2fixup::FrameInfo(MPEG2frame *f)
{
    QString msg = QString("Id:%1 %2 V:%3").arg(f->m_pkt->stream_index)
                    .arg(PtsTime(f->m_pkt->pts))
                    .arg(ring_free(&m_rx.m_indexVrbuf) / sizeof(index_unit));

    if (m_extCount)
    {
        msg += " EXT:";
        for (int i = 0; i < m_extCount; i++)
            msg += QString(" %2")
                   .arg(ring_free(&m_rx.m_indexExtrbuf[i]) / sizeof(index_unit));
    }
    LOG(VB_RPLXQUEUE, LOG_INFO, msg);
}

int MPEG2fixup::AddFrame(MPEG2frame *f)
{
    index_unit iu {};
    ringbuffer *rb = nullptr;
    ringbuffer *rbi = nullptr;
    int id = f->m_pkt->stream_index;

    memset(&iu, 0, sizeof(index_unit));
    iu.frame_start = 1;

    if (id == m_vidId)
    {
        rb = &m_rx.m_vrBuf;
        rbi = &m_rx.m_indexVrbuf;
        iu.frame = GetFrameTypeN(f);
        iu.seq_header = static_cast<uint8_t>(f->m_isSequence);
        iu.gop = static_cast<uint8_t>(f->m_isGop);

        iu.gop_off = f->m_gopPos - f->m_pkt->data;
        iu.frame_off = f->m_framePos - f->m_pkt->data;
        iu.dts = f->m_pkt->dts * 300;
    }
    else if (GetStreamType(id) == AV_CODEC_ID_MP2 ||
             GetStreamType(id) == AV_CODEC_ID_MP3 ||
             GetStreamType(id) == AV_CODEC_ID_AC3)
    {
        rb = &m_rx.m_extrbuf[m_audMap[id]];
        rbi = &m_rx.m_indexExtrbuf[m_audMap[id]];
        iu.framesize = f->m_pkt->size;
    }

    if (!rb || !rbi)
    {
        LOG(VB_GENERAL, LOG_ERR, "Ringbuffer pointers empty. No stream found");
        return 1;
    }

    iu.active = 1;
    iu.length = f->m_pkt->size;
    iu.pts = f->m_pkt->pts * 300;
    pthread_mutex_lock( &m_rx.m_mutex );

    FrameInfo(f);
    while (ring_free(rb) < (unsigned int)f->m_pkt->size ||
            ring_free(rbi) < sizeof(index_unit))
    {
        int ok = 1;

        if (rbi != &m_rx.m_indexVrbuf &&
                ring_avail(&m_rx.m_indexVrbuf) < sizeof(index_unit))
            ok = 0;

        for (int i = 0; i < m_extCount; i++)
        {
            if (rbi != &m_rx.m_indexExtrbuf[i] &&
                    ring_avail(&m_rx.m_indexExtrbuf[i]) < sizeof(index_unit))
                ok = 0;
        }

        if (!ok && ring_free(rb) < (unsigned int)f->m_pkt->size &&
                    ring_free(rbi) >= sizeof(index_unit))
        {
            // increase memory to avoid deadlock
            unsigned int inc_size = 10 * (unsigned int)f->m_pkt->size;
            LOG(VB_GENERAL, LOG_NOTICE,
                QString("Increasing ringbuffer size by %1 to avoid deadlock")
                    .arg(inc_size));

            if (!ring_reinit(rb, rb->size + inc_size))
                ok = 1;
        }
        if (!ok)
        {
            pthread_mutex_unlock( &m_rx.m_mutex );
            //deadlock
            LOG(VB_GENERAL, LOG_ERR,
                "Deadlock detected.  One buffer is full when "
                "the other is empty!  Aborting");
            return 1;
        }

        pthread_cond_signal(&m_rx.m_cond);
        pthread_cond_wait(&m_rx.m_cond, &m_rx.m_mutex);

        FrameInfo(f);
    }

    if (ring_write(rb, f->m_pkt->data, f->m_pkt->size)<0)
    {
        pthread_mutex_unlock( &m_rx.m_mutex );
        LOG(VB_GENERAL, LOG_ERR,
            QString("Ring buffer overflow %1").arg(rb->size));
        return 1;
    }

    if (ring_write(rbi, (uint8_t *)&iu, sizeof(index_unit))<0)
    {
        pthread_mutex_unlock( &m_rx.m_mutex );
        LOG(VB_GENERAL, LOG_ERR,
            QString("Ring buffer overflow %1").arg(rbi->size));
        return 1;
    }
    pthread_mutex_unlock(&m_rx.m_mutex);
    m_lastWrittenPos = f->m_pkt->pos;
    return 0;
}

bool MPEG2fixup::InitAV(const QString& inputfile, const char *type, int64_t offset)
{
    QByteArray ifarray = inputfile.toLocal8Bit();
    const char *ifname = ifarray.constData();

    const AVInputFormat *fmt = nullptr;

    if (type)
        fmt = av_find_input_format(type);

    // Open recording
    LOG(VB_GENERAL, LOG_INFO, QString("Opening %1").arg(inputfile));

    if (m_inputFC)
    {
        avformat_close_input(&m_inputFC);
        m_inputFC = nullptr;
    }

    int ret = avformat_open_input(&m_inputFC, ifname, fmt, nullptr);
    if (ret)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Couldn't open input file, error #%1").arg(ret));
        return false;
    }

    if (m_inputFC->iformat && strcmp(m_inputFC->iformat->name, "mpegts") == 0 &&
        gCoreContext->GetBoolSetting("FFMPEGTS", false))
    {
        fmt = av_find_input_format("mpegts-ffmpeg");
        if (fmt)
        {
            LOG(VB_PLAYBACK, LOG_INFO, "Using FFmpeg MPEG-TS demuxer (forced)");
            avformat_close_input(&m_inputFC);
            ret = avformat_open_input(&m_inputFC, ifname, fmt, nullptr);
            if (ret)
            {
                LOG(VB_GENERAL, LOG_ERR,
                    QString("Couldn't open input file, error #%1").arg(ret));
                return false;
            }
        }
    }

    m_mkvFile = m_inputFC->iformat && strcmp(m_inputFC->iformat->name, "mkv") == 0;

    if (offset)
        av_seek_frame(m_inputFC, m_vidId, offset, AVSEEK_FLAG_BYTE);

    // Getting stream information
    ret = avformat_find_stream_info(m_inputFC, nullptr);
    if (ret < 0)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Couldn't get stream info, error #%1").arg(ret));
        avformat_close_input(&m_inputFC);
        m_inputFC = nullptr;
        return false;
    }

    // Dump stream information
    if (VERBOSE_LEVEL_CHECK(VB_GENERAL, LOG_INFO))
        av_dump_format(m_inputFC, 0, ifname, 0);

    for (unsigned int i = 0; i < m_inputFC->nb_streams; i++)
    {
        switch (m_inputFC->streams[i]->codecpar->codec_type)
        {
            case AVMEDIA_TYPE_VIDEO:
                if (m_vidId == -1)
                    m_vidId = i;
                break;

            case AVMEDIA_TYPE_AUDIO:
                if (!m_allAudio && m_extCount > 0 &&
                    m_inputFC->streams[i]->codecpar->ch_layout.nb_channels < 2 &&
                    m_inputFC->streams[i]->codecpar->sample_rate < 100000)
                {
                    LOG(VB_GENERAL, LOG_ERR,
                        QString("Skipping audio stream: %1").arg(i));
                    break;
                }
                if (m_inputFC->streams[i]->codecpar->codec_id == AV_CODEC_ID_AC3 ||
                    m_inputFC->streams[i]->codecpar->codec_id == AV_CODEC_ID_MP3 ||
                    m_inputFC->streams[i]->codecpar->codec_id == AV_CODEC_ID_MP2)
                {
                    m_audMap[i] = m_extCount++;
                    m_aFrame[i] = new FrameList();
                }
                else
                {
                    LOG(VB_GENERAL, LOG_ERR,
                        QString("Skipping unsupported audio stream: %1")
                            .arg(m_inputFC->streams[i]->codecpar->codec_id));
                }
                break;
            default:
                LOG(VB_GENERAL, LOG_ERR,
                    QString("Skipping unsupported codec %1 on stream %2")
                        .arg(m_inputFC->streams[i]->codecpar->codec_type).arg(i));
                break;
        }
    }

    return true;
}

void MPEG2fixup::SetFrameNum(uint8_t *ptr, int num)
{
    SETBITS(ptr + 4, num, 10);
}

void MPEG2fixup::AddSequence(MPEG2frame *frame1, MPEG2frame *frame2)
{
    if (frame1->m_isSequence || !frame2->m_isSequence)
        return;

    int head_size = (frame2->m_framePos - frame2->m_pkt->data);

    int oldPktSize = frame1->m_pkt->size;
    frame1->ensure_size(frame1->m_pkt->size + head_size); // Changes pkt.size
    memmove(frame1->m_pkt->data + head_size, frame1->m_pkt->data, oldPktSize);
    memcpy(frame1->m_pkt->data, frame2->m_pkt->data, head_size);
    ProcessVideo(frame1, m_headerDecoder);
#if 0
    if (VERBOSE_LEVEL_CHECK(VB_PROCESS, LOG_ANY))
    {
        static int count = 0;
        QString filename = QString("hdr%1.yuv").arg(count++);
        WriteFrame(filename, &frame1->m_pkt);
    }
#endif
}

int MPEG2fixup::ProcessVideo(MPEG2frame *vf, mpeg2dec_t *dec)
{
    int state = -1;
    int last_pos = 0;

    if (dec == m_headerDecoder)
    {
        mpeg2_reset(dec, 0);
        vf->m_isSequence = false;
        vf->m_isGop = false;
    }

    auto *info = (mpeg2_info_t *)mpeg2_info(dec);

    mpeg2_buffer(dec, vf->m_pkt->data, vf->m_pkt->data + vf->m_pkt->size);

    while (state != STATE_PICTURE)
    {
        state = mpeg2_parse(dec);

        if (dec == m_headerDecoder)
        {
            switch (state)
            {

                case STATE_SEQUENCE:
                case STATE_SEQUENCE_MODIFIED:
                case STATE_SEQUENCE_REPEATED:
                    memcpy(&vf->m_mpeg2_seq, info->sequence,
                           sizeof(mpeg2_sequence_t));
                    vf->m_isSequence = true;
                    break;

                case STATE_GOP:
                    memcpy(&vf->m_mpeg2_gop, info->gop, sizeof(mpeg2_gop_t));
                    vf->m_isGop = true;
                    vf->m_gopPos = vf->m_pkt->data + last_pos;
                    //pd->adjustFrameCount=0;
                    break;

                case STATE_PICTURE:
                    memcpy(&vf->m_mpeg2_pic, info->current_picture,
                           sizeof(mpeg2_picture_t));
                    vf->m_framePos = vf->m_pkt->data + last_pos;
                    break;

                case STATE_BUFFER:
                    LOG(VB_GENERAL, LOG_WARNING,
                        "Warning: partial frame found!");
                    return 1;
            }
        }
        else if (state == STATE_BUFFER)
        {
            WriteData("abort.dat", vf->m_pkt->data, vf->m_pkt->size);
            LOG(VB_GENERAL, LOG_ERR,
                QString("Failed to decode frame.  Position was: %1")
                    .arg(last_pos));
            return -1;
        } 
        last_pos = (vf->m_pkt->size - mpeg2_getpos(dec)) - 4;
    }

    if (dec != m_headerDecoder)
    {
        while (state != STATE_BUFFER)
            state = mpeg2_parse(dec);
        if (info->display_picture)
        {
            // This is a hack to force libmpeg2 to finish writing out the slice
            // without it, the final row doesn't get put into the disp_pic
            // (for B-frames only).
            // 0xb2 is 'user data' and is actually illegal between pic
            // headers, but it is just discarded by libmpeg2
            std::array<uint8_t,8> tmp {0x00, 0x00, 0x01, 0xb2, 0xff, 0xff, 0xff, 0xff};
            mpeg2_buffer(dec, tmp.data(), tmp.data() + 8);
            mpeg2_parse(dec);
        }   
    }

    if (VERBOSE_LEVEL_CHECK(VB_DECODE, LOG_INFO))
    {
        QString msg = QString("");
#if 0
        msg += QString("unused:%1 ") .arg(vf->m_pkt->size - mpeg2_getpos(dec));
#endif

        if (vf->m_isSequence)
            msg += QString("%1x%2 P:%3 ").arg(info->sequence->width)
                .arg(info->sequence->height).arg(info->sequence->frame_period);

        if (info->gop)
        {
            QString gop = QString("%1:%2:%3:%4 ")
                .arg(info->gop->hours, 2, 10, QChar('0')).arg(info->gop->minutes, 2, 10, QChar('0'))
                .arg(info->gop->seconds, 2, 10, QChar('0')).arg(info->gop->pictures, 3, 10, QChar('0'));
            msg += gop;
        }
        if (info->current_picture)
        {
            int ct = info->current_picture->flags & PIC_MASK_CODING_TYPE;
            char coding_type { 'X' };
            if (ct == PIC_FLAG_CODING_TYPE_I)
                coding_type = 'I';
            else if (ct == PIC_FLAG_CODING_TYPE_P)
                coding_type = 'P';
            else if (ct == PIC_FLAG_CODING_TYPE_B)
                coding_type = 'B';
            else if (ct == PIC_FLAG_CODING_TYPE_D)
                coding_type = 'D';
            char top_bottom = (info->current_picture->flags & 
                               PIC_FLAG_TOP_FIELD_FIRST) ? 'T' : 'B';
            char progressive = (info->current_picture->flags &
                                PIC_FLAG_PROGRESSIVE_FRAME) ? 'P' : '_';
            msg += QString("#%1 fl:%2%3%4%5%6 ")
                           .arg(info->current_picture->temporal_reference)
                           .arg(info->current_picture->nb_fields)
                           .arg(coding_type)
                           .arg(top_bottom)
                           .arg(progressive)
                           .arg(info->current_picture->flags >> 4, 0, 16);
        }
        msg += QString("pos: %1").arg(vf->m_pkt->pos);
        LOG(VB_DECODE, LOG_INFO, msg);
    }

    return 0;
}

void MPEG2fixup::WriteFrame(const QString& filename, MPEG2frame *f)
{
    MPEG2frame *tmpFrame = GetPoolFrame(f);
    if (tmpFrame == nullptr)
        return;
    if (!tmpFrame->m_isSequence)
    {
        for (const auto & vf : std::as_const(m_vFrame))
        {
            if (vf->m_isSequence)
            {
                AddSequence(tmpFrame, vf);
                break;
            }
        }
    }
    WriteFrame(filename, tmpFrame->m_pkt);
    m_framePool.enqueue(tmpFrame);
}
   
void MPEG2fixup::WriteFrame(const QString& filename, AVPacket *pkt)
{
    MPEG2frame *tmpFrame = GetPoolFrame(pkt);
    if (tmpFrame == nullptr)
        return;

    QString fname = filename + ".enc";
    WriteData(fname, pkt->data, pkt->size);

    mpeg2dec_t *tmp_decoder = mpeg2_init();
    auto *info = (mpeg2_info_t *)mpeg2_info(tmp_decoder);

    while (!info->display_picture)
    {
        if (ProcessVideo(tmpFrame, tmp_decoder))
        {
            delete tmpFrame;
            return;
        }
    }

    WriteYUV(filename, info);
    m_framePool.enqueue(tmpFrame);
    mpeg2_close(tmp_decoder);
}

void MPEG2fixup::WriteYUV(const QString& filename, const mpeg2_info_t *info)
{
    int fh = open(filename.toLocal8Bit().constData(),
                  O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
    if (fh == -1)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Couldn't open file %1: ").arg(filename) + ENO);
        return;
    }

    // Automatically close file at function exit
    auto close_fh = [](const int *fh2) { close(*fh2); };
    std::unique_ptr<int,decltype(close_fh)> cleanup { &fh, close_fh };

    ssize_t ret = write(fh, info->display_fbuf->buf[0],
		       static_cast<size_t>(info->sequence->width) *
		       static_cast<size_t>(info->sequence->height));
    if (ret < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("write failed %1: ").arg(filename) +
                ENO);
        return;
    }
    ret = write(fh, info->display_fbuf->buf[1],
                static_cast<size_t>(info->sequence->chroma_width) *
                static_cast<size_t>(info->sequence->chroma_height));
    if (ret < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("write failed %1: ").arg(filename) +
                ENO);
        return;
    }
    ret = write(fh, info->display_fbuf->buf[2],
                static_cast<size_t>(info->sequence->chroma_width) *
                static_cast<size_t>(info->sequence->chroma_height));
    if (ret < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("write failed %1: ").arg(filename) +
                ENO);
        return;
    }
}

void MPEG2fixup::WriteData(const QString& filename, uint8_t *data, int size)
{
    int fh = open(filename.toLocal8Bit().constData(),
                  O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
    if (fh == -1)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Couldn't open file %1: ").arg(filename) + ENO);
        return;
    }

    int ret = write(fh, data, size);
    if (ret < 0)
        LOG(VB_GENERAL, LOG_ERR, QString("write failed %1").arg(filename) +
                ENO);
    close(fh);
}

bool MPEG2fixup::BuildFrame(AVPacket *pkt, const QString& fname)
{
    alignas(16) std::array<uint16_t,64> intra_matrix {};
    int64_t savedPts = pkt->pts; // save the original pts

    const mpeg2_info_t *info = mpeg2_info(m_imgDecoder);
    if (!info->display_fbuf)
        return true;

    int outbuf_size = info->sequence->width * info->sequence->height * 2;

    if (!fname.isEmpty())
    {
        QString tmpstr = fname + ".yuv";
        WriteYUV(tmpstr, info);
    }

    if (!m_picture)
    {
        m_picture = av_frame_alloc();
        if (m_picture == nullptr)
        {
            return true;
        }
    }
    else
    {
        av_frame_unref(m_picture);
    }

    //pkt->data = (uint8_t *)av_malloc(outbuf_size);
    if (pkt->size < outbuf_size)
        av_grow_packet(pkt, (outbuf_size - pkt->size));

    m_picture->data[0] = info->display_fbuf->buf[0];
    m_picture->data[1] = info->display_fbuf->buf[1];
    m_picture->data[2] = info->display_fbuf->buf[2];

    m_picture->linesize[0] = info->sequence->width;
    m_picture->linesize[1] = info->sequence->chroma_width;
    m_picture->linesize[2] = info->sequence->chroma_width;

    m_picture->opaque = info->display_fbuf->id;

#if 0 //RUN_ONCE
    static constexpr std::array<uint8_t, 64> k_zigzag_scan = {
        0,   1,  8, 16,  9,  2,  3, 10,
        17, 24, 32, 25, 18, 11,  4,  5,
        12, 19, 26, 33, 40, 48, 41, 34,
        27, 20, 13,  6,  7, 14, 21, 28,
        35, 42, 49, 56, 57, 50, 43, 36,
        29, 22, 15, 23, 30, 37, 44, 51,
        58, 59, 52, 45, 38, 31, 39, 46,
        53, 60, 61, 54, 47, 55, 62, 63
    };

    static std::array<uint16_t, 64> k_invZigzagDirect16 = {};
    for (int i = 0; i < 64; i++)
    {
        k_invZigzagDirect16[k_zigzag_scan[i]] = i;
    }
#endif
    static constexpr std::array<uint16_t, 64> k_invZigzagDirect16 = {
          0,  1,  5,  6, 14, 15, 27, 28,
          2,  4,  7, 13, 16, 26, 29, 42,
          3,  8, 12, 17, 25, 30, 41, 43,
          9, 11, 18, 24, 31, 40, 44, 53,
         10, 19, 23, 32, 39, 45, 52, 54,
         20, 22, 33, 38, 46, 51, 55, 60,
         21, 34, 37, 47, 50, 56, 59, 61,
         35, 36, 48, 49, 57, 58, 62, 63,
    };

    //copy_quant_matrix(m_imgDecoder, intra_matrix);
    for (int i = 0; i < 64; i++)
    {
        intra_matrix[k_invZigzagDirect16[i]] = m_imgDecoder->quantizer_matrix[0][i];
    }

    if (info->display_picture->nb_fields % 2)
    {
        if ((info->display_picture->flags & PIC_FLAG_TOP_FIELD_FIRST) != 0)
        {
            m_picture->flags &= ~AV_FRAME_FLAG_TOP_FIELD_FIRST;
        }
        else
        {
            m_picture->flags |= AV_FRAME_FLAG_TOP_FIELD_FIRST;
        }
    }
    else
    {
        if ((info->display_picture->flags & PIC_FLAG_TOP_FIELD_FIRST) != 0)
        {
            m_picture->flags |= AV_FRAME_FLAG_TOP_FIELD_FIRST;
        }
        else
        {
            m_picture->flags &= ~AV_FRAME_FLAG_TOP_FIELD_FIRST;
        }
    }

    if ((info->display_picture->flags & PIC_FLAG_PROGRESSIVE_FRAME) != 0)
    {
        m_picture->flags &= ~AV_FRAME_FLAG_INTERLACED;
    }
    else
    {
        m_picture->flags |= AV_FRAME_FLAG_INTERLACED;
    }

    const AVCodec *out_codec = avcodec_find_encoder(AV_CODEC_ID_MPEG2VIDEO);
    if (!out_codec)
    {
        LOG(VB_GENERAL, LOG_ERR, "Couldn't find MPEG2 encoder");
        return true;
    }

    AVCodecContext *c = avcodec_alloc_context3(nullptr);

    //NOTE: The following may seem wrong, but avcodec requires
    //sequence->progressive == frame->progressive
    //We fix the discrepancy by discarding avcodec's sequence header, and
    //replace it with the original
    if ((m_picture->flags & AV_FRAME_FLAG_INTERLACED) != 0)
        c->flags |= AV_CODEC_FLAG_INTERLACED_DCT;

    c->bit_rate = info->sequence->byte_rate << 3; //not used
    c->bit_rate_tolerance = c->bit_rate >> 2; //not used
    c->width = info->sequence->width;
    c->height = info->sequence->height;
    av_reduce(&c->time_base.num, &c->time_base.den,
              info->sequence->frame_period, 27000000LL, 100000);
    c->pix_fmt = AV_PIX_FMT_YUV420P;
    c->max_b_frames = 0;
    c->has_b_frames = 0;
    // c->rc_buffer_aggressivity = 1;
    // rc_buf_aggressivity is now "currently useless"

    //  c->profile = vidCC->profile;
    //  c->level = vidCC->level;
    c->rc_buffer_size = 0;
    c->gop_size = 0; // this should force all i-frames
    //  c->flags=CODEC_FLAG_LOW_DELAY;

    if (intra_matrix[0] == 0x08)
        c->intra_matrix = intra_matrix.data();

    c->qmin = c->qmax = 2;

    m_picture->width = info->sequence->width;
    m_picture->height = info->sequence->height;
    m_picture->format = AV_PIX_FMT_YUV420P;
    m_picture->pts = AV_NOPTS_VALUE;
    m_picture->flags |= AV_FRAME_FLAG_KEY;
    m_picture->pict_type = AV_PICTURE_TYPE_NONE;
    m_picture->quality = 0;

    if (avcodec_open2(c, out_codec, nullptr) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, "could not open codec");
        return true;
    }

    int got_packet = 0;
    int ret = avcodec_send_frame(c, m_picture);

    bool flushed = false;
    while (ret >= 0)
    {
        // ret = avcodec_encode_video2(c, pkt, m_picture, &got_packet);
        ret = avcodec_receive_packet(c, pkt);
        if (ret == 0)
            got_packet = 1;
        if (ret == AVERROR(EAGAIN))
            ret = 0;
        if (ret < 0)
            break;
        if (flushed)
            break;
        // flush
        ret = avcodec_send_frame(c, nullptr);
        flushed = true;
    }

    if (ret < 0 || !got_packet)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("avcodec_encode_video2 failed (%1)").arg(ret));
        return true;
    }

    if (!fname.isEmpty())
    {
        QString ename = fname + ".enc";
        WriteData(ename, pkt->data, pkt->size);

        QString yname = fname + ".enc.yuv";
        WriteFrame(yname, pkt);
    }
    int delta = FindMPEG2Header(pkt->data, pkt->size, 0x00);
    //  out_size=avcodec_encode_video(c, outbuf, outbuf_size, m_picture);
    // HACK: a hack to get to the picture frame
    //pkt->size -= delta; // a hack to get to the picture frame
    int newSize = pkt->size - delta;
    pkt->pts = savedPts; // restore the original pts
    memmove(pkt->data, pkt->data + delta, newSize);
    av_shrink_packet(pkt, newSize); // Shrink packet to it's new size
    // End HACK

    SetRepeat(pkt->data, pkt->size, info->display_picture->nb_fields,
              ((info->display_picture->flags & PIC_FLAG_TOP_FIELD_FIRST) != 0U));

    avcodec_free_context(&c);

    return false;
}

static constexpr int MAX_FRAMES { 20000 };
MPEG2frame *MPEG2fixup::GetPoolFrame(AVPacket *pkt)
{
    MPEG2frame *f = nullptr;

    if (m_framePool.isEmpty())
    {
        static int s_frameCount = 0;
        if (s_frameCount >= MAX_FRAMES)
        {
            LOG(VB_GENERAL, LOG_ERR, "No more queue slots!");
            return nullptr;
        }
        f = new MPEG2frame(pkt->size);
        s_frameCount++;
    }
    else
    {
        f = m_framePool.dequeue();
    }

    f->set_pkt(pkt);

    return f;
}

MPEG2frame *MPEG2fixup::GetPoolFrame(MPEG2frame *f)
{
    MPEG2frame *tmpFrame = GetPoolFrame(f->m_pkt);
    if (!tmpFrame)
        return tmpFrame;

    tmpFrame->m_isSequence = f->m_isSequence;
    tmpFrame->m_isGop      = f->m_isGop;
    tmpFrame->m_mpeg2_seq  = f->m_mpeg2_seq;
    tmpFrame->m_mpeg2_gop  = f->m_mpeg2_gop;
    tmpFrame->m_mpeg2_pic  = f->m_mpeg2_pic;
    return tmpFrame;
}

int MPEG2fixup::GetFrame(AVPacket *pkt)
{
    while (true)
    {
        bool done = false;
        if (!m_unreadFrames.isEmpty())
        {
            m_vFrame.append(m_unreadFrames.dequeue());
            if (m_realFileEnd && m_unreadFrames.isEmpty())
                m_fileEnd = true;
            return static_cast<int>(m_fileEnd);
        }

        while (!done)
        {
            pkt->pts = AV_NOPTS_VALUE;
            pkt->dts = AV_NOPTS_VALUE;
            int ret = av_read_frame(m_inputFC, pkt);

            if (ret < 0)
            {
                // If it is EAGAIN, obey it, dangit!
                if (ret == -EAGAIN)
                    continue;

                //insert a bogus frame (this won't be written out)
                if (m_vFrame.isEmpty())
                {
                    LOG(VB_GENERAL, LOG_ERR,
                        "Found end of file without finding any frames");
                    av_packet_unref(pkt);
                    return 1;
                }

                MPEG2frame *tmpFrame = GetPoolFrame(m_vFrame.last()->m_pkt);
                if (tmpFrame == nullptr)
                {
                    av_packet_unref(pkt);
                    return 1;
                }

                m_vFrame.append(tmpFrame);
                m_realFileEnd = true;
                m_fileEnd = true;
                return 1;
            }

            if (pkt->stream_index == m_vidId ||
                  m_aFrame.contains(pkt->stream_index))
                done = true;
            else 
                av_packet_unref(pkt);
        }
        pkt->duration = m_frameNum++;
        if ((m_showProgress || m_updateStatus) &&
            MythDate::current() > m_statusTime)
        {
            float percent_done = 100.0F * pkt->pos / m_fileSize;
            if (m_updateStatus)
                m_updateStatus(percent_done);
            if (m_showProgress)
                LOG(VB_GENERAL, LOG_INFO, QString("%1% complete")
                        .arg(percent_done, 0, 'f', 1));
            if (m_checkAbort && m_checkAbort())
                return REENCODE_STOPPED;
            m_statusTime = MythDate::current();
            m_statusTime = m_statusTime.addSecs(m_statusUpdateTime);
        }

#ifdef DEBUG_AUDIO
        LOG(VB_DECODE, LOG_INFO, QString("Stream: %1 PTS: %2 DTS: %3 pos: %4")
              .arg(pkt->stream_index)
              .arg((pkt->pts == AV_NOPTS_VALUE) ? "NONE" : PtsTime(pkt->pts))
              .arg((pkt->dts == AV_NOPTS_VALUE) ? "NONE" : PtsTime(pkt->dts))
              .arg(pkt->pos));
#endif

        MPEG2frame *tmpFrame = GetPoolFrame(pkt);
        if (tmpFrame == nullptr)
        {
            av_packet_unref(pkt);
            return 1;
        }

        switch (m_inputFC->streams[pkt->stream_index]->codecpar->codec_type)
        {
            case AVMEDIA_TYPE_VIDEO:
                m_vFrame.append(tmpFrame);
                av_packet_unref(pkt);

                if (!ProcessVideo(m_vFrame.last(), m_headerDecoder))
                    return 0;
                m_framePool.enqueue(m_vFrame.takeLast());
                break;

            case AVMEDIA_TYPE_AUDIO:
                if (m_aFrame.contains(pkt->stream_index))
                {
                    m_aFrame[pkt->stream_index]->append(tmpFrame);
                }
                else
                {
                    LOG(VB_GENERAL, LOG_DEBUG,
                        QString("Invalid stream ID %1, ignoring").arg(pkt->stream_index));
                    m_framePool.enqueue(tmpFrame);
                }
                av_packet_unref(pkt);
                return 0;

            default:
                m_framePool.enqueue(tmpFrame);
                av_packet_unref(pkt);
                return 1;
        }
    }
}

bool MPEG2fixup::FindStart()
{
    QMap <int, bool> found;
    AVPacket *pkt = av_packet_alloc();
    if (pkt == nullptr)
    {
        LOG(VB_PROCESS, LOG_ERR, "packet allocation failed");
        return false;
    }

    do
    {
        if (GetFrame(pkt))
        {
            av_packet_free(&pkt);
            return false;
        }

        if (m_vidId == pkt->stream_index)
        {
            while (!m_vFrame.isEmpty())
            {
                if (m_vFrame.first()->m_isSequence)
                {
                    if (pkt->pos != m_vFrame.first()->m_pkt->pos)
                        break;

                    if (pkt->pts != AV_NOPTS_VALUE ||
                        pkt->dts != AV_NOPTS_VALUE)
                    {
                        if (pkt->pts == AV_NOPTS_VALUE)
                            m_vFrame.first()->m_pkt->pts = pkt->dts;

                        LOG(VB_PROCESS, LOG_INFO,
                            "Found 1st valid video frame");
                        break;
                    }
                }

                LOG(VB_PROCESS, LOG_INFO, "Dropping V packet");

                m_framePool.enqueue(m_vFrame.takeFirst());
            }
        }

        if (m_vFrame.isEmpty())
            continue;

        for (auto it = m_aFrame.begin(); it != m_aFrame.end(); it++)
        {
            if (found.contains(it.key()))
                continue;

            FrameList *af = (*it);

            while (!af->isEmpty())
            {
                int64_t delta = diff2x33(af->first()->m_pkt->pts,
                                         m_vFrame.first()->m_pkt->pts);
                if (delta < -180000 || delta > 180000) //2 seconds
                {
                    //Check all video sequence packets against current
                    //audio packet
                    MPEG2frame *foundframe = nullptr;
                    for (auto *currFrame : std::as_const(m_vFrame))
                    {
                        if (currFrame->m_isSequence)
                        {
                            int64_t dlta1 = diff2x33(af->first()->m_pkt->pts,
                                                     currFrame->m_pkt->pts);
                            if (dlta1 >= -180000 && dlta1 <= 180000)
                            {
                                foundframe = currFrame;
                                delta = dlta1;
                                break;
                            }
                        }
                    }

                    while (foundframe && m_vFrame.first() != foundframe)
                    {
                        m_framePool.enqueue(m_vFrame.takeFirst());
                    }
                }

                if (delta < -180000 || delta > 180000) //2 seconds
                {
                    LOG(VB_PROCESS, LOG_INFO,
                        QString("Dropping A packet from stream %1")
                                   .arg(it.key()));
                    LOG(VB_PROCESS, LOG_INFO, QString("     A:%1 V:%2")
                            .arg(PtsTime(af->first()->m_pkt->pts),
                                 PtsTime(m_vFrame.first()->m_pkt->pts)));
                    m_framePool.enqueue(af->takeFirst());
                    continue;
                }

                if (delta < 0 && af->count() > 1)
                {
                    if (cmp2x33(af->at(1)->m_pkt->pts,
                                m_vFrame.first()->m_pkt->pts) > 0)
                    {
                        LOG(VB_PROCESS, LOG_INFO,
                            QString("Found useful audio frame from stream %1")
                                .arg(it.key()));
                        found[it.key()] = true;
                        break;
                    }
                    LOG(VB_PROCESS, LOG_INFO,
                        QString("Dropping A packet from stream %1")
                        .arg(it.key()));
                    m_framePool.enqueue(af->takeFirst());
                    continue;
                }
                if (delta >= 0)
                {
                    LOG(VB_PROCESS, LOG_INFO,
                        QString("Found useful audio frame from stream %1")
                            .arg(it.key()));
                    found[it.key()] = true;
                    break;
                }

                if (af->count() == 1)
                    break;
            }
        }
    } while (found.count() != m_aFrame.count());

    av_packet_free(&pkt);
    return true;
}

void MPEG2fixup::SetRepeat(MPEG2frame *vf, int fields, bool topff)
{
    vf->m_mpeg2_pic.nb_fields = 2;
    SetRepeat(vf->m_framePos, vf->m_pkt->data + vf->m_pkt->size - vf->m_framePos,
               fields, topff);
}

void MPEG2fixup::SetRepeat(uint8_t *ptr, int size, int fields, bool topff)
{
    uint8_t *end = ptr + size;
    uint8_t setmask = 0x00;
    uint8_t clrmask = 0xff;
    if (topff)
        setmask |= 0x80;
    else
        clrmask &= 0x7f;

    if (fields == 2)
        clrmask &= 0xfd;
    else
        setmask |= 0x02;

    while (ptr < end)
    {
        if (MATCH_HEADER(ptr) && ptr[3] == 0xB5 && (ptr[4] & 0xF0) == 0x80)
        {
            //unset repeat_first_field
            //set top_field_first
            ptr[7] |= setmask;
            ptr[7] &= clrmask;
            return;
        }

        ptr++;
    }
}

MPEG2frame *MPEG2fixup::FindFrameNum(int frameNum)
{
    for (const auto & vf : std::as_const(m_vFrame))
    {
        if (GetFrameNum(vf) == frameNum)
            return vf;
    }

    return nullptr;
}

void MPEG2fixup::RenumberFrames(int start_pos, int delta)
{
    int maxPos = m_vFrame.count() - 1;
    
    for (int pos = start_pos; pos < maxPos; pos++)
    {
        MPEG2frame *frame = m_vFrame.at(pos);
        SetFrameNum(frame->m_framePos, GetFrameNum(frame) + delta);
        frame->m_mpeg2_pic.temporal_reference += delta;
    }
}

void MPEG2fixup::StoreSecondary()
{
    while (!m_vSecondary.isEmpty())
    {
        m_framePool.enqueue(m_vSecondary.takeFirst());
    }

    while (m_vFrame.count() > 1)
    {
        if (m_useSecondary && GetFrameTypeT(m_vFrame.first()) != 'B')
            m_vSecondary.append(m_vFrame.takeFirst());
        else
            m_framePool.enqueue(m_vFrame.takeFirst());
    }
}

int MPEG2fixup::PlaybackSecondary()
{
    int frame_num = 0;
    mpeg2_reset(m_imgDecoder, 1);
    for (const auto & vs : std::as_const(m_vSecondary))
    {
        SetFrameNum(vs->m_framePos, frame_num++);
        if (ProcessVideo(vs, m_imgDecoder) < 0)
            return 1;
    }
    return 0;
}

MPEG2frame *MPEG2fixup::DecodeToFrame(int frameNum, int skip_reset)
{
    MPEG2frame *spare = nullptr;
    int found = 0;
    bool skip_first = false;
    const mpeg2_info_t * info = mpeg2_info(m_imgDecoder);
    int maxPos = m_vFrame.count() - 1;

    if (m_vFrame.at(m_displayFrame)->m_isSequence)
    {
        skip_first = true;
        if (!skip_reset && (m_displayFrame != maxPos || m_displayFrame == 0))
            mpeg2_reset(m_imgDecoder, 1);
    }

    spare = FindFrameNum(frameNum);
    if (!spare)
        return nullptr;

    int framePos = m_vFrame.indexOf(spare);

    for (int curPos = m_displayFrame; m_displayFrame != maxPos;
         curPos++, m_displayFrame++)
    {
        if (ProcessVideo(m_vFrame.at(m_displayFrame), m_imgDecoder) < 0)
            return nullptr;

        if (!skip_first && curPos >= framePos && info->display_picture &&
            (int)info->display_picture->temporal_reference >= frameNum)
        {
            found = 1;
            m_displayFrame++;
            break;
        }

        skip_first = false;
    }

    if (!found)
    {
        int tmpFrameNum = frameNum;
        MPEG2frame *tmpFrame = GetPoolFrame(spare->m_pkt);
        if (tmpFrame == nullptr)
            return nullptr;

        tmpFrame->m_framePos = tmpFrame->m_pkt->data +
                               (spare->m_framePos - spare->m_pkt->data);

        while (!info->display_picture ||
               (int)info->display_picture->temporal_reference < frameNum)
        {
            SetFrameNum(tmpFrame->m_framePos, ++tmpFrameNum);
            if (ProcessVideo(tmpFrame, m_imgDecoder) < 0)
            {
                delete tmpFrame;
                return nullptr;
            }
        }

        m_framePool.enqueue(tmpFrame);
    }

    if ((int)info->display_picture->temporal_reference > frameNum)
    {
        // the frame in question doesn't exist.  We have no idea where we are.
        // reset the displayFrame so we start searching from the beginning next
        // time
        m_displayFrame = 0;
        LOG(VB_GENERAL, LOG_NOTICE,
            QString("Frame %1 > %2.  Corruption likely at pos: %3")
               .arg(info->display_picture->temporal_reference)
               .arg(frameNum).arg(spare->m_pkt->pos));
    }

    return spare;
}

int MPEG2fixup::ConvertToI(FrameList *orderedFrames, int headPos)
{
    MPEG2frame *spare = nullptr;
    AVPacket *pkt = av_packet_alloc();
    if (pkt == nullptr)
    {
        LOG(VB_PROCESS, LOG_ERR, "packet allocation failed");
        return 0;
    }
#ifdef SPEW_FILES
    static int ins_count = 0;
#endif

    //head_pos == 0 means that we are decoding B frames after a seq_header
    if (headPos == 0)
    {
        if (PlaybackSecondary())
        {
            av_packet_free(&pkt);
            return 1;
        }
    }

    for (const auto & of : std::as_const(*orderedFrames))
    {
        int i = GetFrameNum(of);
        spare = DecodeToFrame(i, static_cast<int>(headPos == 0));
        if (spare == nullptr)
        {
            LOG(VB_GENERAL, LOG_WARNING,
                QString("ConvertToI skipping undecoded frame #%1").arg(i));
            continue;
        }

        if (GetFrameTypeT(spare) == 'I')
            continue;
        
        //pkt = spare->m_pkt;
        av_packet_ref(pkt, spare->m_pkt);
        //pkt->data is a newly malloced area

        QString fname;

#ifdef SPEW_FILES
        if (VERBOSE_LEVEL_CHECK(VB_PROCESS, LOG_ANY))
            fname = QString("cnv%1").arg(ins_count++);
#endif

        if (BuildFrame(pkt, fname))
        {
            av_packet_free(&pkt);
            return 1;
        }

        LOG(VB_GENERAL, LOG_INFO,
            QString("Converting frame #%1 from %2 to I %3")
                .arg(i).arg(GetFrameTypeT(spare)).arg(fname));

        spare->set_pkt(pkt);
        av_packet_unref(pkt);
        SetFrameNum(spare->m_pkt->data, GetFrameNum(spare));
        ProcessVideo(spare, m_headerDecoder); //process this new frame
    }

    //reorder frames
    m_vFrame.move(headPos, headPos + orderedFrames->count() - 1);
    av_packet_free(&pkt);
    return 0;
}

int MPEG2fixup::InsertFrame(int frameNum, int64_t deltaPTS,
                            int64_t ptsIncrement, int64_t initPTS)
{
    MPEG2frame *spare = nullptr;
    int increment = 0;
    int index = 0;

    AVPacket *pkt = av_packet_alloc();
    if (pkt == nullptr)
    {
        LOG(VB_PROCESS, LOG_ERR, "packet allocation failed");
        return 0;
    }

    spare = DecodeToFrame(frameNum, 0);
    if (spare == nullptr)
    {
        av_packet_free(&pkt);
        return -1;
    }

    av_packet_ref(pkt, spare->m_pkt);
    //pkt->data is a newly malloced area

    {
        QString fname;
#ifdef SPEW_FILES
        static int ins_count = 0;
        fname = (VERBOSE_LEVEL_CHECK(VB_PROCESS, LOG_ANY) ?
                (QString("ins%1").arg(ins_count++)) : QString());
#endif

        if (BuildFrame(pkt, fname))
        {
            av_packet_free(&pkt);
            return -1;
        }

        LOG(VB_GENERAL, LOG_INFO,
            QString("Inserting %1 I-Frames after #%2 %3")
                .arg((int)(deltaPTS / ptsIncrement))
                .arg(GetFrameNum(spare)).arg(fname));
    }
    
    inc2x33(&pkt->pts, (ptsIncrement * GetNbFields(spare) / 2) + initPTS);

    index = m_vFrame.indexOf(spare) + 1;
    while (index < m_vFrame.count() &&
           GetFrameTypeT(m_vFrame.at(index)) == 'B')
        spare = m_vFrame.at(index++);

    index = m_vFrame.indexOf(spare);

    while (deltaPTS > 0)
    {
        index++;
        increment++;
        pkt->dts = pkt->pts;
        SetFrameNum(pkt->data, ++frameNum);
        MPEG2frame *tmpFrame = GetPoolFrame(pkt);
        if (tmpFrame == nullptr)
            return -1;
        m_vFrame.insert(index, tmpFrame);
        ProcessVideo(tmpFrame, m_headerDecoder); //process new frame

        inc2x33(&pkt->pts, ptsIncrement);
        deltaPTS -= ptsIncrement;
    }

    av_packet_free(&pkt);
    // update frame # for all later frames in this group
    index++;
    RenumberFrames(index, increment);

    return increment;
}

void MPEG2fixup::AddRangeList(const QStringList& rangelist, int type)
{
    frm_dir_map_t *mapPtr = nullptr;

    if (type == MPF_TYPE_CUTLIST)
    {
        mapPtr = &m_delMap;
        m_discard = false;
    }
    else
    {
        mapPtr = &m_saveMap;
    }

    mapPtr->clear();

    for (const auto & range : std::as_const(rangelist))
    {
        QStringList tmp = range.split(" - ");
        if (tmp.size() < 2)
            continue;

        std::array<bool,2> ok { false, false };

        long long start = tmp[0].toLongLong(ok.data());
        long long end   = tmp[1].toLongLong(&ok[1]);
    
        if (ok[0] && ok[1])
        {
            if (start == 0)
            {
                if (type == MPF_TYPE_CUTLIST)
                    m_discard = true;
            }
            else
            {
                mapPtr->insert(start - 1, MARK_CUT_START);
            }

            mapPtr->insert(end, MARK_CUT_END);
        }
    }

    if (!rangelist.isEmpty())
        m_useSecondary = true;
}

void MPEG2fixup::ShowRangeMap(frm_dir_map_t *mapPtr, QString msg)
{
    if (!mapPtr->isEmpty())
    {
        int64_t start = 0;
        frm_dir_map_t::iterator it = mapPtr->begin();
        for (; it != mapPtr->end(); ++it)
        {
            if (*it == MARK_CUT_END)
                msg += QString("\n\t\t%1 - %2").arg(start).arg(it.key());
            else
                start = it.key();
        }
        if (*(--it) == MARK_CUT_START)
            msg += QString("\n\t\t%1 - end").arg(start);
        LOG(VB_PROCESS, LOG_INFO, msg);
    }
}

FrameList MPEG2fixup::ReorderDTStoPTS(FrameList *dtsOrder, int pos)
{
    FrameList Lreorder;
    int maxPos = dtsOrder->count() - 1;

    if (pos >= maxPos)
        return Lreorder;

    MPEG2frame *frame = dtsOrder->at(pos);

    for (pos++; pos < maxPos && GetFrameTypeT(dtsOrder->at(pos)) == 'B'; pos++)
        Lreorder.append(dtsOrder->at(pos));

    Lreorder.append(frame);
    return Lreorder;
}

void MPEG2fixup::InitialPTSFixup(MPEG2frame *curFrame, int64_t &origvPTS,
                                 int64_t &PTSdiscrep, int numframes, bool fix) const
{
    int64_t tmpPTS = diff2x33(curFrame->m_pkt->pts,
                              origvPTS / 300);

    if (curFrame->m_pkt->pts == AV_NOPTS_VALUE)
    {
        LOG(VB_PROCESS, LOG_INFO,
            QString("Found frame %1 with missing PTS at %2")
                .arg(GetFrameNum(curFrame))
                .arg(PtsTime(origvPTS / 300)));
        if (fix)
            curFrame->m_pkt->pts = origvPTS / 300;
        else
            PTSdiscrep = AV_NOPTS_VALUE;
    }
    else if (tmpPTS < -m_ptsIncrement ||
             tmpPTS > m_ptsIncrement*numframes)
    {
        if (tmpPTS != PTSdiscrep)
        {
            PTSdiscrep = tmpPTS;
            LOG(VB_PROCESS, LOG_INFO,
                QString("Found invalid PTS (off by %1) at %2")
                       .arg(PtsTime(tmpPTS),
                            PtsTime(origvPTS / 300)));
        }
        if (fix)
            curFrame->m_pkt->pts = origvPTS / 300;
    }
    else
    {
        origvPTS = curFrame->m_pkt->pts * 300;
    }
    ptsinc((uint64_t *)&origvPTS,
            (uint64_t)(150 * m_ptsIncrement * GetNbFields(curFrame)));
}

void MPEG2fixup::dumpList(FrameList *list)
{
    LOG(VB_GENERAL, LOG_INFO, "=========================================");
    LOG(VB_GENERAL, LOG_INFO, QString("List contains %1 items")
            .arg(list->count()));

    for (auto *curFrame : std::as_const(*list))
    {
        LOG(VB_GENERAL, LOG_INFO,
            QString("VID: %1 #:%2 nb: %3 pts: %4 dts: %5 pos: %6")
                .arg(GetFrameTypeT(curFrame))
                .arg(GetFrameNum(curFrame))
                .arg(GetNbFields(curFrame))
                .arg(PtsTime(curFrame->m_pkt->pts),
                     PtsTime(curFrame->m_pkt->dts),
                     QString::number(curFrame->m_pkt->pos)));
    }
    LOG(VB_GENERAL, LOG_INFO, "=========================================");
}

int MPEG2fixup::Start()
{
    // NOTE: expectedvPTS/DTS are in units of SCR (300*PTS) to allow for better
    // accounting of rounding errors (still won't be right, but better)
    int64_t lastPTS = 0;
    int64_t deltaPTS = 0;
    std::array<int64_t,N_AUDIO> origaPTS {};
    int64_t cutStartPTS = 0;
    int64_t cutEndPTS = 0;
    uint64_t frame_count = 0;
    int new_discard_state = 0;
    QMap<int, int> af_dlta_cnt;
    QMap<int, int> cutState;

    AVPacket *pkt = av_packet_alloc();
    AVPacket *lastRealvPkt = av_packet_alloc();
    if ((pkt == nullptr) || (lastRealvPkt == nullptr))
    {
        LOG(VB_GENERAL, LOG_ERR, "packet allocation failed");
        return GENERIC_EXIT_NOT_OK;
    }

    if (!InitAV(m_infile, m_format, 0))
    {
        av_packet_free(&pkt);
        av_packet_free(&lastRealvPkt);
        return GENERIC_EXIT_NOT_OK;
    }

    if (!FindStart())
    {
        av_packet_free(&pkt);
        av_packet_free(&lastRealvPkt);
        return GENERIC_EXIT_NOT_OK;
    }

    m_ptsIncrement = m_vFrame.first()->m_mpeg2_seq.frame_period / 300;

    int64_t initPTS = m_vFrame.first()->m_pkt->pts;

    LOG(VB_GENERAL, LOG_INFO, QString("#%1 PTS:%2 Delta: 0.0ms queue: %3")
            .arg(m_vidId).arg(PtsTime(m_vFrame.first()->m_pkt->pts))
            .arg(m_vFrame.count()));

    for (auto it = m_aFrame.begin(); it != m_aFrame.end(); it++)
    {
        FrameList *af = (*it);
        deltaPTS = diff2x33(m_vFrame.first()->m_pkt->pts, af->first()->m_pkt->pts);
        LOG(VB_GENERAL, LOG_INFO,
            QString("#%1 PTS:%2 Delta: %3ms queue: %4")
                .arg(it.key()) .arg(PtsTime(af->first()->m_pkt->pts))
                .arg(1000.0*deltaPTS / 90000.0).arg(af->count()));

        if (cmp2x33(af->first()->m_pkt->pts, initPTS) < 0)
            initPTS = af->first()->m_pkt->pts;
    }

    initPTS -= 16200; //0.18 seconds back to prevent underflow

    PTSOffsetQueue poq(m_vidId, m_aFrame.keys(), initPTS);

    LOG(VB_PROCESS, LOG_INFO,
        QString("ptsIncrement: %1 Frame #: %2 PTS-adjust: %3")
            .arg(m_ptsIncrement).arg(GetFrameNum(m_vFrame.first()))
            .arg(PtsTime(initPTS)));


    int64_t origvPTS = 300 * udiff2x33(m_vFrame.first()->m_pkt->pts,
                     m_ptsIncrement * GetFrameNum(m_vFrame.first()));
    int64_t expectedvPTS = 300 * (udiff2x33(m_vFrame.first()->m_pkt->pts, initPTS) -
                         (m_ptsIncrement * GetFrameNum(m_vFrame.first())));
    int64_t expectedDTS = expectedvPTS - (300 * m_ptsIncrement);

    if (m_discard)
    {
        cutStartPTS = origvPTS / 300;
    }

    for (auto it = m_aFrame.begin(); it != m_aFrame.end(); it++)
    {
        FrameList *af = (*it);
        origaPTS[it.key()] = af->first()->m_pkt->pts * 300;
        //expectedPTS[it.key()] = udiff2x33(af->first()->m_pkt->pts, initPTS);
        af_dlta_cnt[it.key()] = 0;
        cutState[it.key()] = static_cast<int>(m_discard);
    }

    ShowRangeMap(&m_delMap, "Cutlist:");
    ShowRangeMap(&m_saveMap, "Same Range:");

    InitReplex();

    while (!m_fileEnd)
    {
        /* read packet */
        int ret = GetFrame(pkt);
        if (ret < 0)
        {
            av_packet_free(&pkt);
            av_packet_free(&lastRealvPkt);
            return ret;
        }

        if (!m_vFrame.isEmpty() && (m_fileEnd || m_vFrame.last()->m_isSequence))
        {
            m_displayFrame = 0;

            // since we might reorder the frames when coming out of a cutpoint
            // me need to save the first frame here, as it is guaranteed to
            // have a sequence header.
            MPEG2frame *seqFrame = m_vFrame.first();

            if (!seqFrame->m_isSequence)
            {
                LOG(VB_GENERAL, LOG_WARNING, 
                    QString("Problem: Frame %1 (type %2) doesn't contain "
                            "sequence header!")
                       .arg(frame_count) .arg(GetFrameTypeT(seqFrame)));
            }

            if (m_ptsIncrement != seqFrame->m_mpeg2_seq.frame_period / 300)
            {
                LOG(VB_GENERAL, LOG_WARNING,
                    QString("WARNING - Unsupported FPS change from %1 to %2")
                       .arg(90000.0 / m_ptsIncrement, 0, 'f', 2)
                       .arg(27000000.0 / seqFrame->m_mpeg2_seq.frame_period,
                            0, 'f', 2));
            }

            for (int frame_pos = 0; frame_pos < m_vFrame.count() - 1;)
            {
                bool ptsorder_eq_dtsorder = false;
                int64_t PTSdiscrep = 0;
                FrameList Lreorder;
                MPEG2frame *markedFrame = nullptr;
                MPEG2frame *markedFrameP = nullptr;

                if (expectedvPTS != expectedDTS + (m_ptsIncrement * 300))
                {
                    LOG(VB_GENERAL, LOG_ERR,
                        QString("expectedPTS != expectedDTS + ptsIncrement"));
                    LOG(VB_GENERAL, LOG_ERR, QString("%1 != %2 + %3")
                            .arg(PtsTime(expectedvPTS / 300),
                                 PtsTime(expectedDTS / 300),
                                 PtsTime(m_ptsIncrement)));
                    LOG(VB_GENERAL, LOG_ERR, QString("%1 != %2 + %3")
                            .arg(expectedvPTS)
                            .arg(expectedDTS)
                            .arg(m_ptsIncrement));
                    av_packet_free(&pkt);
                    av_packet_free(&lastRealvPkt);
                    return GENERIC_EXIT_NOT_OK;
                }

                //reorder frames in presentation order (to the next I/P frame)
                Lreorder = ReorderDTStoPTS(&m_vFrame, frame_pos);

                //First pass at fixing PTS values (fixes gross errors only)
                for (auto *curFrame : std::as_const(Lreorder))
                {
                    poq.UpdateOrigPTS(m_vidId, origvPTS, curFrame->m_pkt);
                    InitialPTSFixup(curFrame, origvPTS, PTSdiscrep, 
                                    m_maxFrames, true);
                }

                // if there was a PTS jump, find the largest change
                // in the next x frames
                // At the end of this, vFrame should look just like it did
                // beforehand
                if (PTSdiscrep && !m_fileEnd)
                {
                    int pos = m_vFrame.count();
                    int count = Lreorder.count();
                    while (m_vFrame.count() - frame_pos - count < 20 && !m_fileEnd)
                    {
                        ret = GetFrame(pkt);
                        if (ret < 0)
                        {
                            av_packet_free(&pkt);
                            av_packet_free(&lastRealvPkt);
                            return ret;
                        }
                    }

                    if (!m_fileEnd)
                    {
                        int64_t tmp_origvPTS = origvPTS;
                        int numframes = (m_maxFrames > 1) ? m_maxFrames - 1 : 1;
                        bool done = false;
                        while (!done &&
                               (frame_pos + count + 1) < m_vFrame.count())
                        {
                            FrameList tmpReorder;
                            tmpReorder = ReorderDTStoPTS(&m_vFrame,
                                                         frame_pos + count);
                            for (auto *curFrame : std::as_const(tmpReorder))
                            {
                                int64_t tmpPTSdiscrep = 0;
                                InitialPTSFixup(curFrame, tmp_origvPTS,
                                           tmpPTSdiscrep, numframes, false);
                                if (!tmpPTSdiscrep)
                                {
                                    //discrepancy was short-lived, continue on
                                    done = true;
                                    PTSdiscrep = 0;
                                    break;
                                }
                                if (tmpPTSdiscrep != AV_NOPTS_VALUE &&
                                    tmpPTSdiscrep != PTSdiscrep)
                                    PTSdiscrep = tmpPTSdiscrep;
                            }
                            count += tmpReorder.count();
                        }
                    }

                    // push extra read frames onto 'unreadFrames' queue
                    while (m_vFrame.count() > pos)
                    {
                        m_unreadFrames.enqueue(m_vFrame.takeAt(pos));
                    }
                    m_fileEnd = false;
                }
  
                //check for cutpoints and convert to I-frames if needed 
                for (int curIndex = 0; curIndex < Lreorder.count(); curIndex++)
                {
                    MPEG2frame *curFrame = Lreorder.at(curIndex);
                    if (!m_saveMap.isEmpty())
                    {
                        if (m_saveMap.begin().key() <= frame_count)
                           m_saveMap.remove(m_saveMap.begin().key());
                        if (!m_saveMap.empty() && m_saveMap.begin().value() == 0)
                        {
                            LOG(VB_GENERAL, LOG_INFO,
                                QString("Saving frame #%1") .arg(frame_count));

                            if (GetFrameTypeT(curFrame) != 'I' &&
                                ConvertToI(&Lreorder, frame_pos))
                            {
                                av_packet_free(&pkt);
                                av_packet_free(&lastRealvPkt);
                                return GENERIC_EXIT_WRITE_FRAME_ERROR;
                            }

                            WriteFrame(QString("save%1.yuv").arg(frame_count),
                                       curFrame);
                        }
                    }

                    if (!m_delMap.empty() && m_delMap.begin().key() <= frame_count)
                    {
                        new_discard_state = m_delMap.begin().value();
                        LOG(VB_GENERAL, LOG_INFO,
                            QString("Del map found %1 at %2 (%3)")
                                .arg(new_discard_state) .arg(frame_count)
                                .arg(m_delMap.begin().key()));

                        m_delMap.remove(m_delMap.begin().key());
                        markedFrameP = curFrame;

                        if (!new_discard_state)
                        {
                            cutEndPTS = markedFrameP->m_pkt->pts;
                            poq.SetNextPTS(
                                      diff2x33(cutEndPTS, expectedvPTS / 300),
                                      cutEndPTS);
                        }
                        else
                        {
                            cutStartPTS =
                                  add2x33(markedFrameP->m_pkt->pts,
                                          m_ptsIncrement * 
                                          GetNbFields(markedFrameP) / 2);
                            for (auto it3 = m_aFrame.begin();
                                 it3 != m_aFrame.end(); it3++)
                            {
                                cutState[it3.key()] = 1;
                            }
                        }

                        // Rebuild when 'B' frame, or completing a cut, and the
                        // marked frame is a 'P' frame.
                        // After conversion, frames will be in linear order.
                        if ((GetFrameTypeT(curFrame) == 'B') ||
                            (!new_discard_state &&
                             (GetFrameTypeT(curFrame) == 'P')))
                        {
                            if (ConvertToI(&Lreorder, frame_pos))
                            {
                                av_packet_free(&pkt);
                                av_packet_free(&lastRealvPkt);
                                return GENERIC_EXIT_WRITE_FRAME_ERROR;
                            }
                            ptsorder_eq_dtsorder = true;
                        }
                        else if (!new_discard_state &&
                                 GetFrameTypeT(curFrame) == 'I')
                        {
                            m_vFrame.move(frame_pos, frame_pos + curIndex);
                            ptsorder_eq_dtsorder = true;
                        }

                        //convert from presentation-order to decode-order
                        markedFrame = m_vFrame.at(frame_pos + curIndex);

                        if (!new_discard_state)
                        {
                            AddSequence(markedFrame, seqFrame);
                            RenumberFrames(frame_pos + curIndex,
                                           - GetFrameNum(markedFrame));
                        }
                    }

                    frame_count++;
                }

                if (!Lreorder.isEmpty())
                {
                    av_packet_unref(lastRealvPkt);
                    av_packet_ref(lastRealvPkt, Lreorder.last()->m_pkt);
                }

                if (markedFrame || !m_discard)
                {
                    int64_t dtsExtra = 0;
                    //check for PTS discontinuity
                    for (auto *curFrame : std::as_const(Lreorder))
                    {
                        if (markedFrameP && m_discard)
                        {
                            if (curFrame != markedFrameP)
                                continue;

                            markedFrameP = nullptr;
                        }

                        dec2x33(&curFrame->m_pkt->pts,
                                poq.Get(m_vidId, curFrame->m_pkt));
                        deltaPTS = diff2x33(curFrame->m_pkt->pts,
                                            expectedvPTS / 300);

                        if (deltaPTS < -2 || deltaPTS > 2)
                        {
                            LOG(VB_PROCESS, LOG_INFO,
                                QString("PTS discrepancy: %1 != %2 on "
                                        "%3-Type (%4)")
                                    .arg(curFrame->m_pkt->pts)
                                    .arg(expectedvPTS / 300)
                                    .arg(GetFrameTypeT(curFrame))
                                    .arg(GetFrameNum(curFrame)));
                        }

                        //remove repeat_first_field if necessary
                        if (m_noRepeat)
                            SetRepeat(curFrame, 2, false);

                        //force PTS to stay in sync (this could be a bad idea!)
                        if (m_fixPts)
                            curFrame->m_pkt->pts = expectedvPTS / 300;

                        if (deltaPTS > m_ptsIncrement*m_maxFrames)
                        {
                            LOG(VB_GENERAL, LOG_NOTICE,
                                QString("Need to insert %1 frames > max "
                                        "allowed: %2.  Assuming bad PTS")
                                    .arg((int)(deltaPTS / m_ptsIncrement))
                                    .arg(m_maxFrames));
                            curFrame->m_pkt->pts = expectedvPTS / 300;
                            deltaPTS = 0;
                        }

                        lastPTS = expectedvPTS;
                        expectedvPTS += 150 * m_ptsIncrement *
                                        GetNbFields(curFrame);

                        if (curFrame == markedFrameP && new_discard_state)
                            break;
                    }

                    // dtsExtra is applied at the end of this block if the
                    // current tail has repeat_first_field set
                    if (ptsorder_eq_dtsorder)
                        dtsExtra = 0;
                    else
                        dtsExtra = 150 * m_ptsIncrement *
                                   (GetNbFields(m_vFrame.at(frame_pos)) - 2);

                    if (!markedFrame && deltaPTS > (4 * m_ptsIncrement / 5))
                    {
                        // if we are off by more than 1/2 frame, it is time to
                        // add a frame
                        // The frame(s) will be added right after lVpkt_tail,
                        // and lVpkt_head will be adjusted accordingly

                        m_vFrame.at(frame_pos)->m_pkt->pts = lastPTS / 300;
                        ret = InsertFrame(GetFrameNum(m_vFrame.at(frame_pos)),
                                              deltaPTS, m_ptsIncrement, 0);
                        
                        if (ret < 0)
                        {
                            av_packet_free(&pkt);
                            av_packet_free(&lastRealvPkt);
                            return GENERIC_EXIT_WRITE_FRAME_ERROR;
                        }

                        for (int index = frame_pos + Lreorder.count();
                             ret && index < m_vFrame.count(); index++, --ret)
                        {
                            lastPTS = expectedvPTS;
                            expectedvPTS += 150 * m_ptsIncrement *
                                            GetNbFields(m_vFrame.at(index));
                            Lreorder.append(m_vFrame.at(index));
                        }
                    }

                    // Set DTS (ignore any current values), and send frame to
                    // multiplexer

                    for (int i = 0; i < Lreorder.count(); i++, frame_pos++)
                    {
                        MPEG2frame *curFrame = m_vFrame.at(frame_pos);
                        if (m_discard)
                        {
                            if (curFrame != markedFrame)
                                continue;

                            m_discard = false;
                            markedFrame = nullptr;
                        }

                        // Make clang-tidy null dereference checker happy.
                        if (curFrame == nullptr)
                            continue;
                        curFrame->m_pkt->dts = (expectedDTS / 300);
#if 0
                        if (GetFrameTypeT(curFrame) == 'B')
                            curFrame->m_pkt->pts = (expectedDTS / 300);
#endif
                        expectedDTS += 150 * m_ptsIncrement *
                                       ((!ptsorder_eq_dtsorder && i == 0) ?  2 :
                                        GetNbFields(curFrame));
                        LOG(VB_FRAME, LOG_INFO,
                            QString("VID: %1 #:%2 nb: %3 pts: %4 dts: %5 "
                                    "pos: %6")
                                .arg(GetFrameTypeT(curFrame))
                                .arg(GetFrameNum(curFrame))
                                .arg(GetNbFields(curFrame))
                                .arg(PtsTime(curFrame->m_pkt->pts),
                                     PtsTime(curFrame->m_pkt->dts),
                                     QString::number(curFrame->m_pkt->pos)));
                        if (AddFrame(curFrame))
                        {
                            av_packet_free(&pkt);
                            av_packet_free(&lastRealvPkt);
                            return GENERIC_EXIT_DEADLOCK;
                        }

                        if (curFrame == markedFrame)
                        {
                            markedFrame = nullptr;
                            m_discard = true;
                        }
                    }
                        
                    expectedDTS += dtsExtra;
                }
                else
                {
                    frame_pos += Lreorder.count();
                }
                if (PTSdiscrep)
                    poq.SetNextPos(add2x33(poq.Get(m_vidId, lastRealvPkt),
                                                   PTSdiscrep), lastRealvPkt);
            }

            if (m_discard)
                cutEndPTS = lastRealvPkt->pts;

            if (m_fileEnd)
                m_useSecondary = false;
            if (m_vFrame.count() > 1 || m_fileEnd)
                StoreSecondary();
        }

        for (auto it = m_aFrame.begin(); it != m_aFrame.end(); it++)
        {
            FrameList *af = (*it);
            AVCodecContext *CC = getCodecContext(it.key());
            AVCodecParserContext *CPC = getCodecParserContext(it.key());
            bool backwardsPTS = false;

            while (!af->isEmpty())
            {
                if (!CC || !CPC)
                {
                    m_framePool.enqueue(af->takeFirst());
                    continue;
                }
                // What to do if the CC is corrupt?
                // Just wait and hope it repairs itself
                if (CC->sample_rate == 0 || !CPC || CPC->duration == 0)
                    break;

                // The order of processing frames is critical to making
                // everything work.  Backwards PTS discrepancies complicate
                // the processing significantly
                // Processing works as follows:
                //   detect whether there is a discontinuous PTS (tmpPTS != 0)
                //     in the audio stream only.
                //   next check if a cutpoint is active, and discard frames
                //     as needed
                //   next check that the current PTS < last video PTS
                //   if we get this far, update the expected PTS, and write out
                //     the audio frame
                int64_t incPTS =
                         90000LL * (int64_t)CPC->duration / CC->sample_rate;

                if (poq.UpdateOrigPTS(it.key(), origaPTS[it.key()],
                                                  af->first()->m_pkt) < 0)
                {
                    backwardsPTS = true;
                    af_dlta_cnt[it.key()] = 0;
                }

                int64_t tmpPTS = diff2x33(af->first()->m_pkt->pts,
                                  origaPTS[it.key()] / 300);

                if (tmpPTS < -incPTS)
                {
#ifdef DEBUG_AUDIO
                    LOG(VB_PROCESS, LOG_INFO,
                        QString("Aud discard: PTS %1 < %2")
                            .arg(PtsTime(af->first()->m_pkt->pts))
                            .arg(PtsTime(origaPTS[it.key()] / 300)));
#endif
                    m_framePool.enqueue(af->takeFirst());
                    af_dlta_cnt[it.key()] = 0;
                    continue;
                }

                if (tmpPTS > incPTS * m_maxFrames)
                {
                    LOG(VB_PROCESS, LOG_INFO,
                        QString("Found invalid audio PTS (off by %1) at %2")
                            .arg(PtsTime(tmpPTS),
                                 PtsTime(origaPTS[it.key()] / 300)));
                    if (backwardsPTS && tmpPTS < 90000LL)
                    {
                        //there are missing audio frames
                        LOG(VB_PROCESS, LOG_INFO,
                            "Fixing missing audio frames");
                        ptsinc((uint64_t *)&origaPTS[it.key()], 300 * tmpPTS);
                        backwardsPTS = false;
                    }
                    else if (tmpPTS < 90000LL * 4) // 4 seconds
                    {
                        if (af_dlta_cnt[it.key()] >= 20)
                        {
                            //If there are 20 consecutive frames with an
                            //offset < 4sec, assume a mismatch and correct.
                            //Note: if we allow too much discrepancy,
                            //we could overrun the video queue
                            ptsinc((uint64_t *)&origaPTS[it.key()],
                                   300 * tmpPTS);
                            af_dlta_cnt[it.key()] = 0;
                        }
                        else
                        {
                            af_dlta_cnt[it.key()]++;
                        }
                    }
                    af->first()->m_pkt->pts = origaPTS[it.key()] / 300;
                }
                else if (tmpPTS > incPTS) //correct for small discrepancies
                {
                    incPTS += incPTS;
                    backwardsPTS = false;
                    af_dlta_cnt[it.key()] = 0;
                }
                else
                {
                    backwardsPTS = false;
                    af_dlta_cnt[it.key()] = 0;
                }

                int64_t nextPTS = add2x33(af->first()->m_pkt->pts,
                           90000LL * (int64_t)CPC->duration / CC->sample_rate);

                if ((cutState[it.key()] == 1 &&
                     cmp2x33(nextPTS, cutStartPTS) > 0) ||
                    (cutState[it.key()] == 2 && 
                     cmp2x33(af->first()->m_pkt->pts, cutEndPTS) < 0))
                {
#ifdef DEBUG_AUDIO
                    LOG(VB_PROCESS, LOG_INFO,
                        QString("Aud in cutpoint: %1 > %2 && %3 < %4")
                            .arg(PtsTime(nextPTS)).arg(PtsTime(cutStartPTS))
                            .arg(PtsTime(af->first()->m_pkt->pts))
                            .arg(PtsTime(cutEndPTS)));
#endif
                    m_framePool.enqueue(af->takeFirst());
                    cutState[it.key()] = 2;
                    ptsinc((uint64_t *)&origaPTS[it.key()], incPTS * 300);
                    continue;
                }

                int64_t deltaPTS2 = poq.Get(it.key(), af->first()->m_pkt);

                if (udiff2x33(nextPTS, deltaPTS2) * 300 > expectedDTS &&
                    cutState[it.key()] != 1)
                {
#ifdef DEBUG_AUDIO
                    LOG(VB_PROCESS, LOG_INFO, QString("Aud not ready: %1 > %2")
                            .arg(PtsTime(udiff2x33(nextPTS, deltaPTS2)))
                            .arg(PtsTime(expectedDTS / 300)));
#endif
                    break;
                }

                if (cutState[it.key()] == 2)
                    cutState[it.key()] = 0;

                ptsinc((uint64_t *)&origaPTS[it.key()], incPTS * 300);

                dec2x33(&af->first()->m_pkt->pts, deltaPTS2);

#if 0
                expectedPTS[it.key()] = udiff2x33(nextPTS, initPTS);
                write_audio(lApkt_tail->m_pkt, initPTS);
#endif
                LOG(VB_FRAME, LOG_INFO, QString("AUD #%1: pts: %2 pos: %3")
                        .arg(it.key()) 
                        .arg(PtsTime(af->first()->m_pkt->pts))
                        .arg(af->first()->m_pkt->pos));
                if (AddFrame(af->first()))
                {
                    av_packet_free(&pkt);
                    av_packet_free(&lastRealvPkt);
                    return GENERIC_EXIT_DEADLOCK;
                }
                m_framePool.enqueue(af->takeFirst());
            }
        }
    }

    m_rx.m_done = 1;
    pthread_mutex_lock( &m_rx.m_mutex );
    pthread_cond_signal(&m_rx.m_cond);
    pthread_mutex_unlock( &m_rx.m_mutex );
    int ex = REENCODE_OK;
    void *errors = nullptr; // mythtv#244: return error if any write or close failures
    pthread_join(m_thread, &errors);
    if (*(int *)errors) {
      LOG(VB_GENERAL, LOG_ERR,
	  QString("joined thread failed with %1 write errors")
	  .arg(*(int *)errors));
      ex = REENCODE_ERROR;
    }

    av_packet_free(&pkt);
    av_packet_free(&lastRealvPkt);
    avformat_close_input(&m_inputFC);
    m_inputFC = nullptr;
    return ex;
}

#ifdef NO_MYTH
int verboseMask = VB_GENERAL;

void usage(char *s)
{
    fprintf(stderr, "%s usage:\n", s);
    fprintf(stderr, "\t--infile <file>    -i <file> : Input mpg file\n");
    fprintf(stderr, "\t--outfile <file>   -o <file> : Output mpg file\n");
    fprintf(stderr, "\t--dbg_lvl #        -d #      : Debug level\n");
    fprintf(stderr, "\t--maxframes #      -m #      : Max frames to insert at once (default=10)\n");
    fprintf(stderr, "\t--cutlist \"start - end\" -c : Apply a cutlist.  Specify on e'-c' per cut\n");
    fprintf(stderr, "\t--no3to2           -t        : Remove 3:2 pullup\n");
    fprintf(stderr, "\t--fixup            -f        : make PTS continuous\n");
    fprintf(stderr, "\t--ostream <dvd|ps> -e        : Output stream type (defaults to ps)\n");
    fprintf(stderr, "\t--showprogress     -p        : show progress\n");
    fprintf(stderr, "\t--help             -h        : This screen\n");
    exit(0);
}

int main(int argc, char **argv)
{
    QStringList cutlist;
    QStringList savelist;
    char *infile = nullptr, *outfile = nullptr, *format = nullptr;
    int no_repeat = 0, fix_PTS = 0, max_frames = 20, otype = REPLEX_MPEG2;
    bool showprogress = 0;
    const struct option long_options[] =
        {
            {"infile", required_argument, nullptr, 'i'},
            {"outfile", required_argument, nullptr, 'o'},
            {"format", required_argument, nullptr, 'r'},
            {"dbg_lvl", required_argument, nullptr, 'd'},
            {"cutlist", required_argument, nullptr, 'c'},
            {"saveframe", required_argument, nullptr, 's'},
            {"ostream", required_argument, nullptr, 'e'},
            {"no3to2", no_argument, nullptr, 't'},
            {"fixup", no_argument, nullptr, 'f'},
            {"showprogress", no_argument, nullptr, 'p'},
            {"help", no_argument , nullptr, 'h'},
            {0, 0, 0, 0}
        };

    while (1)
    {
        int option_index = 0;
        char c;
        c = getopt_long (argc, argv, "i:o:d:r:m:c:s:e:tfph",
                         long_options, &option_index);

        if (c == -1)
            break;

        switch (c)
        {

            case 'i':
                infile = optarg;
                break;

            case 'o':
                outfile = optarg;
                break;

            case 'r':
                format = optarg;
                break;

            case 'e':
                if (strlen(optarg) == 3 && strncmp(optarg, "dvd", 3) == 0)
                    otype = REPLEX_DVD;
                break;

            case 'd':
                verboseMask = atoi(optarg);
                break;

            case 'm':
                max_frames = atoi(optarg);
                break;

            case 'c':
                cutlist.append(optarg);
                break;

            case 't':
                no_repeat = 1;

            case 'f':
                fix_PTS = 1;
                break;

            case 's':
                savelist.append(optarg);
                break;

            case 'p':
                showprogress = true;
                break;

            case 'h':

            case '?':

            default:
                usage(argv[0]);
        }
    }

    if (infile == nullptr || outfile == nullptr)
        usage(argv[0]);

    MPEG2fixup m2f(infile, outfile, nullptr, format,
                   no_repeat, fix_PTS, max_frames,
                   showprogress, otype);

    if (cutlist.count())
        m2f.AddRangeList(cutlist, MPF_TYPE_CUTLIST);
    if (savelist.count())
        m2f.AddRangeList(savelist, MPF_TYPE_SAVELIST);
    return m2f.Start();
}
#endif

int MPEG2fixup::BuildKeyframeIndex(const QString &file,
                                   frm_pos_map_t &posMap,
                                   frm_pos_map_t &durMap)
{
    LOG(VB_GENERAL, LOG_INFO, "Generating Keyframe Index");

    int count = 0;

    /*============ initialise AV ===============*/
    m_vidId = -1;
    if (!InitAV(file, nullptr, 0))
        return GENERIC_EXIT_NOT_OK;

    if (m_mkvFile)
    {
        LOG(VB_GENERAL, LOG_INFO, "Seek tables are not required for MKV");
        return GENERIC_EXIT_NOT_OK;
    }

    AVPacket *pkt = av_packet_alloc();
    if (pkt == nullptr)
    {
        LOG(VB_GENERAL, LOG_ERR, "packet allocation failed");
        return GENERIC_EXIT_NOT_OK;
    }

    uint64_t totalDuration = 0;
    while (av_read_frame(m_inputFC, pkt) >= 0)
    {
        if (pkt->stream_index == m_vidId)
        {
            if (pkt->flags & AV_PKT_FLAG_KEY)
            {
                posMap[count] = pkt->pos;
                durMap[count] = totalDuration;
            }

            // XXX totalDuration untested.  Results should be the same
            // as from mythcommflag --rebuild.

            // totalDuration calculation based on
            // AvFormatDecoder::PreProcessVideoPacket()
            totalDuration +=
                av_q2d(m_inputFC->streams[pkt->stream_index]->time_base) *
                pkt->duration * 1000; // msec
            count++;
        }
        av_packet_unref(pkt);
    }

    // Close input file
    av_packet_free(&pkt);
    avformat_close_input(&m_inputFC);
    m_inputFC = nullptr;

    LOG(VB_GENERAL, LOG_NOTICE, "Transcode Completed");

    return REENCODE_OK;
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
